#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/clk-private.h>
#include <linux/pm_domain.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/kmemleak.h>

#include <asm/cacheflush.h>
#include <asm/pgtable.h>

#if defined(CONFIG_SOC_EXYNOS5430)
#include <mach/regs-clock.h>
#endif

#include "exynos-iommu.h"

#define MAX_NUM_PPC	4

const char *ppc_event_name[] = {
	"TOTAL",
	"L1TLB MISS",
	"L2TLB MISS",
	"FLPD CACHE MISS",
	"PB LOOK-UP",
	"PB MISS",
	"BLOCK NUM BY PREFETCHING",
	"BLOCK CYCLE BY PREFETCHING",
	"TLB MISS",
	"FLPD MISS ON PREFETCHING",
};

static int iova_from_sent(sysmmu_pte_t *base, sysmmu_pte_t *sent)
{
	return ((unsigned long)sent - (unsigned long)base) *
					(SECT_SIZE / sizeof(sysmmu_pte_t));
}

struct sysmmu_list_data {
	struct device *sysmmu;
	struct list_head node; /* entry of exynos_iommu_owner.mmu_list */
};

#define has_sysmmu(dev)		(dev->archdata.iommu != NULL)
#define for_each_sysmmu_list(dev, sysmmu_list)			\
	list_for_each_entry(sysmmu_list,				\
		&((struct exynos_iommu_owner *)dev->archdata.iommu)->mmu_list,\
		node)

static struct exynos_iommu_owner *sysmmu_owner_list = NULL;
static struct sysmmu_drvdata *sysmmu_drvdata_list = NULL;

static struct kmem_cache *lv2table_kmem_cache;
static phys_addr_t fault_page;
sysmmu_pte_t *zero_lv2_table;
static struct dentry *exynos_sysmmu_debugfs_root;

#ifdef CONFIG_ARM
static inline void pgtable_flush(void *vastart, void *vaend)
{
	dmac_flush_range(vastart, vaend);
	outer_flush_range(virt_to_phys(vastart),
				virt_to_phys(vaend));
}
#else /* ARM64 */
static inline void pgtable_flush(void *vastart, void *vaend)
{
	dma_sync_single_for_device(NULL,
			virt_to_phys(vastart),
			(size_t)(virt_to_phys(vaend) - virt_to_phys(vastart)),
			DMA_TO_DEVICE);
}
#endif



void sysmmu_tlb_invalidate_flpdcache(struct device *dev, dma_addr_t iova)
{
	struct sysmmu_list_data *list;

	for_each_sysmmu_list(dev, list) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			TRACE_LOG_DEV(drvdata->sysmmu,
				"FLPD invalidation @ %#x\n", iova);
			__master_clk_enable(drvdata);
			__sysmmu_tlb_invalidate_flpdcache(
					drvdata->sfrbase, iova);
			SYSMMU_EVENT_LOG_FLPD_FLUSH(
					SYSMMU_DRVDATA_TO_LOG(drvdata), iova);
			__master_clk_disable(drvdata);
		} else {
			TRACE_LOG_DEV(drvdata->sysmmu,
				"Skip FLPD invalidation @ %#x\n", iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static void sysmmu_tlb_invalidate_entry(struct device *dev, dma_addr_t iova,
					bool force)
{
	struct sysmmu_list_data *list;

	for_each_sysmmu_list(dev, list) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		if (!force && !(drvdata->prop & SYSMMU_PROP_NONBLOCK_TLBINV))
			continue;

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			TRACE_LOG_DEV(drvdata->sysmmu,
				"TLB invalidation @ %#x\n", iova);
			__master_clk_enable(drvdata);
			__sysmmu_tlb_invalidate_entry(drvdata->sfrbase, iova);
			SYSMMU_EVENT_LOG_TLB_INV_VPN(
					SYSMMU_DRVDATA_TO_LOG(drvdata), iova);
			__master_clk_disable(drvdata);
		} else {
			TRACE_LOG_DEV(drvdata->sysmmu,
				"Skip TLB invalidation @ %#x\n", iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

void exynos_sysmmu_tlb_invalidate(struct iommu_domain *domain, dma_addr_t start,
				  size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner;
	struct sysmmu_list_data *list;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	list_for_each_entry(owner, &priv->clients, client) {
		for_each_sysmmu_list(owner->dev, list) {
			struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

			if (!!(drvdata->prop & SYSMMU_PROP_NONBLOCK_TLBINV))
				continue;

			spin_lock(&drvdata->lock);
			if (!is_sysmmu_active(drvdata) ||
					!is_sysmmu_runtime_active(drvdata)) {
				spin_unlock(&drvdata->lock);
				TRACE_LOG_DEV(drvdata->sysmmu,
					"Skip TLB invalidation %#x@%#x\n", size, start);
				continue;
			}

			TRACE_LOG_DEV(drvdata->sysmmu,
				"TLB invalidation %#x@%#x\n", size, start);

			__master_clk_enable(drvdata);

			__sysmmu_tlb_invalidate(drvdata, start, size);

			__master_clk_disable(drvdata);

			spin_unlock(&drvdata->lock);
		}
	}
	spin_unlock_irqrestore(&priv->lock, flags);
}

static inline void __sysmmu_disable_nocount(struct sysmmu_drvdata *drvdata)
{
	int disable = (drvdata->prop & SYSMMU_PROP_STOP_BLOCK) ?
					CTRL_BLOCK_DISABLE : CTRL_DISABLE;

#if defined(CONFIG_SOC_EXYNOS5430)
	if (!strcmp(dev_name(drvdata->sysmmu), "15200000.sysmmu")) {
		if (!(__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC0_SECURE_SMMU_MFC) & 0x1) ||
			!(__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC0_SECURE_SMMU_MFC) & 0x1)) {
			pr_err("MFC0_0 SYSMMU clock is disabled ACLK: [%#x], PCLK[%#x]\n",
				__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC0_SECURE_SMMU_MFC),
				__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC0_SECURE_SMMU_MFC));
			BUG();
		}
	} else if (!strcmp(dev_name(drvdata->sysmmu), "15210000.sysmmu")) {
		if (!(__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC0_SECURE_SMMU_MFC) & 0x2) ||
			!(__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC0_SECURE_SMMU_MFC) & 0x2)) {
			pr_err("MFC0_1 SYSMMU clock is disabled ACLK: [%#x], PCLK[%#x]\n",
				__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC0_SECURE_SMMU_MFC),
				__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC0_SECURE_SMMU_MFC));
			BUG();
		}
	} else if (!strcmp(dev_name(drvdata->sysmmu), "15300000.sysmmu")) {
		if (!(__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC1_SECURE_SMMU_MFC) & 0x1) ||
			!(__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC1_SECURE_SMMU_MFC) & 0x1)) {
			pr_err("MFC1_0 SYSMMU clock is disabled ACLK: [%#x], PCLK[%#x]\n",
				__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC1_SECURE_SMMU_MFC),
				__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC1_SECURE_SMMU_MFC));
			BUG();
		}
	} else if (!strcmp(dev_name(drvdata->sysmmu), "15310000.sysmmu")) {
		if (!(__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC1_SECURE_SMMU_MFC) & 0x2) ||
			!(__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC1_SECURE_SMMU_MFC) & 0x2)) {
			pr_err("MFC1_1 SYSMMU clock is disabled ACLK: [%#x], PCLK[%#x]\n",
				__raw_readl(EXYNOS5430_ENABLE_ACLK_MFC1_SECURE_SMMU_MFC),
				__raw_readl(EXYNOS5430_ENABLE_PCLK_MFC1_SECURE_SMMU_MFC));
			BUG();
		}
	}
#endif

	__raw_sysmmu_disable(drvdata->sfrbase, disable);

	__sysmmu_clk_disable(drvdata);
	if (IS_ENABLED(CONFIG_EXYNOS_IOMMU_NO_MASTER_CLKGATE))
		__master_clk_disable(drvdata);

	SYSMMU_EVENT_LOG_DISABLE(SYSMMU_DRVDATA_TO_LOG(drvdata));

	TRACE_LOG("%s(%s)\n", __func__, dev_name(drvdata->sysmmu));
}

static bool __sysmmu_disable(struct sysmmu_drvdata *drvdata)
{
	bool disabled;
	unsigned long flags;

	spin_lock_irqsave(&drvdata->lock, flags);

	disabled = set_sysmmu_inactive(drvdata);

	if (disabled) {
		drvdata->pgtable = 0;
		drvdata->domain = NULL;

		if (drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			__sysmmu_disable_nocount(drvdata);
			__master_clk_disable(drvdata);
		}

		TRACE_LOG_DEV(drvdata->sysmmu, "Disabled\n");
	} else  {
		TRACE_LOG_DEV(drvdata->sysmmu, "%d times left to disable\n",
					drvdata->activations);
	}

	spin_unlock_irqrestore(&drvdata->lock, flags);

	return disabled;
}

static void __sysmmu_enable_nocount(struct sysmmu_drvdata *drvdata)
{
	if (IS_ENABLED(CONFIG_EXYNOS_IOMMU_NO_MASTER_CLKGATE))
		__master_clk_enable(drvdata);

	__sysmmu_clk_enable(drvdata);

	__sysmmu_init_config(drvdata);

	__sysmmu_set_ptbase(drvdata->sfrbase, drvdata->pgtable / PAGE_SIZE);

	__raw_sysmmu_enable(drvdata->sfrbase);

	SYSMMU_EVENT_LOG_ENABLE(SYSMMU_DRVDATA_TO_LOG(drvdata));

	TRACE_LOG_DEV(drvdata->sysmmu, "Really enabled\n");
}

static int __sysmmu_enable(struct sysmmu_drvdata *drvdata,
			phys_addr_t pgtable, struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&drvdata->lock, flags);
	if (set_sysmmu_active(drvdata)) {
		drvdata->pgtable = pgtable;
		drvdata->domain = domain;

		if (drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			__sysmmu_enable_nocount(drvdata);
			__master_clk_disable(drvdata);
		}

		TRACE_LOG_DEV(drvdata->sysmmu, "Enabled\n");
	} else {
		ret = (pgtable == drvdata->pgtable) ? 1 : -EBUSY;

		TRACE_LOG_DEV(drvdata->sysmmu, "Already enabled (%d)\n", ret);
	}

	if (WARN_ON(ret < 0))
		set_sysmmu_inactive(drvdata); /* decrement count */

	spin_unlock_irqrestore(&drvdata->lock, flags);

	return ret;
}

/* __exynos_sysmmu_enable: Enables System MMU
 *
 * returns -error if an error occurred and System MMU is not enabled,
 * 0 if the System MMU has been just enabled and 1 if System MMU was already
 * enabled before.
 */
static int __exynos_sysmmu_enable(struct device *dev, phys_addr_t pgtable,
				struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);
		drvdata->master = dev;
		ret = __sysmmu_enable(drvdata, pgtable, domain);
		if (ret < 0) {
			struct sysmmu_list_data *iter;
			for_each_sysmmu_list(dev, iter) {
				if (iter == list)
					break;
				__sysmmu_disable(dev_get_drvdata(iter->sysmmu));
				drvdata->master = NULL;
			}
			break;
		}
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return ret;
}

int exynos_sysmmu_enable(struct device *dev, unsigned long pgtable)
{
	int ret;

	BUG_ON(!memblock_is_memory(pgtable));

	ret = __exynos_sysmmu_enable(dev, pgtable, NULL);

	return ret;
}

bool exynos_sysmmu_disable(struct device *dev)
{
	unsigned long flags;
	bool disabled = true;
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	/* Every call to __sysmmu_disable() must return same result */
	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);
		disabled = __sysmmu_disable(drvdata);
		if (disabled)
			drvdata->master = NULL;
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return disabled;
}

#ifdef CONFIG_EXYNOS_IOMMU_RECOVER_FAULT_HANDLER
int recover_fault_handler (struct iommu_domain *domain,
				struct device *dev, unsigned long fault_addr,
				int itype, void *reserved)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner;
	unsigned long flags;

	itype %= 16;

	if (itype == SYSMMU_PAGEFAULT) {
		struct exynos_iovmm *vmm_data;
		sysmmu_pte_t *sent;
		sysmmu_pte_t *pent;

		BUG_ON(priv->pgtable == NULL);

		spin_lock_irqsave(&priv->pgtablelock, flags);

		sent = section_entry(priv->pgtable, fault_addr);
		if (!lv1ent_page(sent)) {
			pent = kmem_cache_zalloc(lv2table_kmem_cache,
						 GFP_ATOMIC);
			if (!pent)
				return -ENOMEM;

			*sent = mk_lv1ent_page(virt_to_phys(pent));
			pgtable_flush(sent, sent + 1);
		}
		pent = page_entry(sent, fault_addr);
		if (lv2ent_fault(pent)) {
			*pent = mk_lv2ent_spage(fault_page);
			pgtable_flush(pent, pent + 1);
		} else {
			pr_err("[%s] 0x%lx by '%s' is already mapped\n",
				sysmmu_fault_name[itype], fault_addr,
				dev_name(dev));
		}

		spin_unlock_irqrestore(&priv->pgtablelock, flags);

		owner = dev->archdata.iommu;
		vmm_data = (struct exynos_iovmm *)owner->vmm_data;
		if (find_iovm_region(vmm_data, fault_addr)) {
			pr_err("[%s] 0x%lx by '%s' is remapped\n",
				sysmmu_fault_name[itype],
				fault_addr, dev_name(dev));
		} else {
			pr_err("[%s] '%s' accessed unmapped address(0x%lx)\n",
				sysmmu_fault_name[itype], dev_name(dev),
				fault_addr);
		}
	} else if (itype == SYSMMU_L1TLB_MULTIHIT) {
		spin_lock_irqsave(&priv->lock, flags);
		list_for_each_entry(owner, &priv->clients, client)
			sysmmu_tlb_invalidate_entry(owner->dev,
						(dma_addr_t)fault_addr, true);
		spin_unlock_irqrestore(&priv->lock, flags);

		pr_err("[%s] occured at 0x%lx by '%s'\n",
			sysmmu_fault_name[itype], fault_addr, dev_name(dev));
	} else {
		return -ENOSYS;
	}

	return 0;
}
#else
int recover_fault_handler (struct iommu_domain *domain,
				struct device *dev, unsigned long fault_addr,
				int itype, void *reserved)
{
	return -ENOSYS;
}
#endif

/* called by exynos5-iommu.c and exynos7-iommu.c */
#define PB_CFG_MASK	0x11111;
int __prepare_prefetch_buffers_by_plane(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_pb,
				int inplanes, int onplanes,
				int ipoption, int opoption)
{
	int ret_num_pb = 0;
	int i = 0;
	struct exynos_iovmm *vmm;

	if (!drvdata->master || !drvdata->master->archdata.iommu) {
		dev_err(drvdata->sysmmu, "%s: No master device is specified\n",
					__func__);
		return 0;
	}

	vmm = ((struct exynos_iommu_owner *)
			(drvdata->master->archdata.iommu))->vmm_data;
	if (!vmm)
		return 0; /* No VMM information to set prefetch buffers */

	if (!inplanes && !onplanes) {
		inplanes = vmm->inplanes;
		onplanes = vmm->onplanes;
	}

	ipoption &= PB_CFG_MASK;
	opoption &= PB_CFG_MASK;

	if (drvdata->prop & SYSMMU_PROP_READ) {
		ret_num_pb = min(inplanes, num_pb);
		for (i = 0; i < ret_num_pb; i++) {
			prefbuf[i].base = vmm->iova_start[i];
			prefbuf[i].size = vmm->iovm_size[i];
			prefbuf[i].config = ipoption;
		}
	}

	if ((drvdata->prop & SYSMMU_PROP_WRITE) &&
				(ret_num_pb < num_pb) && (onplanes > 0)) {
		for (i = 0; i < min(num_pb - ret_num_pb, onplanes); i++) {
			prefbuf[ret_num_pb + i].base =
					vmm->iova_start[vmm->inplanes + i];
			prefbuf[ret_num_pb + i].size =
					vmm->iovm_size[vmm->inplanes + i];
			prefbuf[ret_num_pb + i].config = opoption;
		}

		ret_num_pb += i;
	}

	if (drvdata->prop & SYSMMU_PROP_WINDOW_MASK) {
		unsigned long prop = (drvdata->prop & SYSMMU_PROP_WINDOW_MASK)
						>> SYSMMU_PROP_WINDOW_SHIFT;
		BUG_ON(ret_num_pb != 0);
		for (i = 0; (i < (vmm->inplanes + vmm->onplanes)) &&
						(ret_num_pb < num_pb); i++) {
			if (prop & 1) {
				prefbuf[ret_num_pb].base = vmm->iova_start[i];
				prefbuf[ret_num_pb].size = vmm->iovm_size[i];
				prefbuf[ret_num_pb].config = ipoption;
				ret_num_pb++;
			}
			prop >>= 1;
			if (prop == 0)
				break;
		}
	}

	return ret_num_pb;
}

void sysmmu_set_prefetch_buffer_by_region(struct device *dev,
			struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;
	unsigned long flags;

	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: No System MMU is configured\n", __func__);
		return;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock(&drvdata->lock);

		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			spin_unlock(&drvdata->lock);
			continue;
		}

		__master_clk_enable(drvdata);

		if (sysmmu_block(drvdata->sfrbase)) {
			__exynos_sysmmu_set_prefbuf_by_region(drvdata, pb_reg, num_reg);
			sysmmu_unblock(drvdata->sfrbase);
		}

		__master_clk_disable(drvdata);

		spin_unlock(&drvdata->lock);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

int sysmmu_set_prefetch_buffer_by_plane(struct device *dev,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm;
	struct sysmmu_list_data *list;
	unsigned long flags;

	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: No System MMU is configured\n", __func__);
		return -EINVAL;
	}

	vmm = exynos_get_iovmm(dev);
	if (!vmm) {
		dev_err(dev, "%s: IOVMM is not configured\n", __func__);
		return -EINVAL;
	}

	if ((inplanes > vmm->inplanes) || (onplanes > vmm->onplanes)) {
		dev_err(dev, "%s: Given planes [%d, %d] exceeds [%d, %d]\n",
				__func__, inplanes, onplanes,
				vmm->inplanes, vmm->onplanes);
		return -EINVAL;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock(&drvdata->lock);

		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			spin_unlock(&drvdata->lock);
			continue;
		}

		__master_clk_enable(drvdata);

		if (sysmmu_block(drvdata->sfrbase)) {
			__exynos_sysmmu_set_prefbuf_by_plane(drvdata,
					inplanes, onplanes, ipoption, opoption);
			sysmmu_unblock(drvdata->sfrbase);
		}

		__master_clk_disable(drvdata);

		spin_unlock(&drvdata->lock);
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return 0;
}

static void __sysmmu_set_ptwqos(struct sysmmu_drvdata *data)
{
	u32 cfg;

	if (!sysmmu_block(data->sfrbase))
		return;

	cfg = __raw_readl(data->sfrbase + REG_MMU_CFG);
	cfg &= ~CFG_QOS(15); /* clearing PTW_QOS field */

	/*
	 * PTW_QOS of System MMU 1.x ~ 3.x are all overridable
	 * in __sysmmu_init_config()
	 */
	if (__raw_sysmmu_version(data->sfrbase) < MAKE_MMU_VER(5, 0))
		cfg |= CFG_QOS(data->qos);
	else if (!(data->qos < 0))
		cfg |= CFG_QOS_OVRRIDE | CFG_QOS(data->qos);
	else
		cfg &= ~CFG_QOS_OVRRIDE;

	__raw_writel(cfg, data->sfrbase + REG_MMU_CFG);
	sysmmu_unblock(data->sfrbase);
}

static void __sysmmu_set_qos(struct device *dev, unsigned int qosval)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;
	unsigned long flags;

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *data;
		data = dev_get_drvdata(list->sysmmu);
		spin_lock(&data->lock);
		data->qos = qosval;
		if (is_sysmmu_really_enabled(data)) {
			__master_clk_enable(data);
			__sysmmu_set_ptwqos(data);
			__master_clk_disable(data);
		}
		spin_unlock(&data->lock);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

void sysmmu_set_qos(struct device *dev, unsigned int qos)
{
	__sysmmu_set_qos(dev, (qos > 15) ? 15 : qos);
}

void sysmmu_reset_qos(struct device *dev)
{
	__sysmmu_set_qos(dev, DEFAULT_QOS_VALUE);
}

void exynos_sysmmu_set_df(struct device *dev, dma_addr_t iova)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;
	unsigned long flags;
	struct exynos_iovmm *vmm;
	int plane;

	BUG_ON(!has_sysmmu(dev));

	vmm = exynos_get_iovmm(dev);
	if (!vmm) {
		dev_err(dev, "%s: IOVMM not found\n", __func__);
		return;
	}

	plane = find_iovmm_plane(vmm, iova);
	if (plane < 0) {
		dev_err(dev, "%s: IOVA %pa is out of IOVMM\n", __func__, &iova);
		return;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock(&drvdata->lock);

		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			if (drvdata->prop & SYSMMU_PROP_WINDOW_MASK) {
				unsigned long prop;
				prop = drvdata->prop & SYSMMU_PROP_WINDOW_MASK;
				prop >>= SYSMMU_PROP_WINDOW_SHIFT;
				if (prop & (1 << plane))
					__exynos_sysmmu_set_df(drvdata, iova);
			} else {
				__exynos_sysmmu_set_df(drvdata, iova);
			}
			__master_clk_disable(drvdata);
		}
		spin_unlock(&drvdata->lock);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

void exynos_sysmmu_release_df(struct device *dev)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list;
	unsigned long flags;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock(&drvdata->lock);
		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			__exynos_sysmmu_release_df(drvdata);
			__master_clk_disable(drvdata);
		}
		spin_unlock(&drvdata->lock);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

static int __init __sysmmu_init_clock(struct device *sysmmu,
					struct sysmmu_drvdata *drvdata)
{
	int ret;

	drvdata->clocks[SYSMMU_PCLK] = ERR_PTR(-ENOENT);
	drvdata->clocks[SYSMMU_MASTER] = ERR_PTR(-ENOENT);

	drvdata->clocks[SYSMMU_ACLK] = devm_clk_get(sysmmu, "sysmmu");
	if (IS_ERR(drvdata->clocks[SYSMMU_ACLK])) {
		if (PTR_ERR(drvdata->clocks[SYSMMU_ACLK]) == -ENOENT) {
			dev_info(sysmmu, "No gating clock found.\n");
			return 0;
		}

		dev_err(sysmmu, "Failed get sysmmu clock\n");
		return PTR_ERR(drvdata->clocks[SYSMMU_ACLK]);
	}

	ret = clk_prepare(drvdata->clocks[SYSMMU_ACLK]);
	if (ret) {
		dev_err(sysmmu, "Failed to prepare sysmmu clock\n");
		return ret;
	}

	drvdata->clocks[SYSMMU_MASTER]= devm_clk_get(sysmmu, "master");
	if (PTR_ERR(drvdata->clocks[SYSMMU_MASTER]) == -ENOENT) {
		return 0;
	} else if (IS_ERR(drvdata->clocks[SYSMMU_MASTER])) {
		dev_err(sysmmu, "Failed to get master clock\n");
		clk_unprepare(drvdata->clocks[SYSMMU_ACLK]);
		return PTR_ERR(drvdata->clocks[SYSMMU_MASTER]);
	}

	ret = clk_prepare(drvdata->clocks[SYSMMU_MASTER]);
	if (ret) {
		clk_unprepare(drvdata->clocks[SYSMMU_ACLK]);
		dev_err(sysmmu, "Failed to prepare master clock\n");
		return ret;
	}

	return 0;
}

static int __init __sysmmu_init_master(struct device *dev)
{
	int ret;
	int i = 0;
	struct device_node *node;

	while ((node = of_parse_phandle(dev->of_node, "mmu-masters", i++))) {
		struct platform_device *master = of_find_device_by_node(node);
		struct exynos_iommu_owner *owner;
		struct sysmmu_list_data *list_data;

		if (!master) {
			dev_err(dev, "%s: mmu-master '%s' not found\n",
				__func__, node->name);
			ret = -EINVAL;
			goto err;
		}

		owner = master->dev.archdata.iommu;
		if (!owner) {
			owner = devm_kzalloc(dev, sizeof(*owner), GFP_KERNEL);
			if (!owner) {
				dev_err(dev,
				"%s: Failed to allocate owner structure\n",
				__func__);
				ret = -ENOMEM;
				goto err;
			}

			INIT_LIST_HEAD(&owner->mmu_list);
			INIT_LIST_HEAD(&owner->client);
			owner->dev = &master->dev;
			spin_lock_init(&owner->lock);

			master->dev.archdata.iommu = owner;
			if (!sysmmu_owner_list) {
				sysmmu_owner_list = owner;
			} else {
				owner->next = sysmmu_owner_list->next;
				sysmmu_owner_list->next = owner;
			}
		}

		list_data = devm_kzalloc(dev, sizeof(*list_data), GFP_KERNEL);
		if (!list_data) {
			dev_err(dev,
				"%s: Failed to allocate sysmmu_list_data\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}

		INIT_LIST_HEAD(&list_data->node);
		list_data->sysmmu = dev;

		/*
		 * System MMUs are attached in the order of the presence
		 * in device tree
		 */
		list_add_tail(&list_data->node, &owner->mmu_list);
		dev_info(dev, "--> %s\n", dev_name(&master->dev));
	}

	return 0;
err:
	while ((node = of_parse_phandle(dev->of_node, "mmu-masters", i++))) {
		struct platform_device *master = of_find_device_by_node(node);
		struct exynos_iommu_owner *owner;
		struct sysmmu_list_data *list_data;

		if (!master)
			continue;

		owner = master->dev.archdata.iommu;
		if (!owner)
			continue;

		list_for_each_entry(list_data, &owner->mmu_list, node) {
			if (list_data->sysmmu == dev) {
				list_del(&list_data->node);
				kfree(list_data);
				break;
			}
		}
	}

	return ret;
}

static const char * const sysmmu_prop_opts[] = {
	[SYSMMU_PROP_RESERVED]		= "Reserved",
	[SYSMMU_PROP_READ]		= "r",
	[SYSMMU_PROP_WRITE]		= "w",
	[SYSMMU_PROP_READWRITE]		= "rw",	/* default */
};

static int __init __sysmmu_init_prop(struct device *sysmmu,
				     struct sysmmu_drvdata *drvdata)
{
	struct device_node *prop_node;
	const char *s;
	int winmap = 0;
	unsigned int qos = DEFAULT_QOS_VALUE;
	int ret;

	drvdata->prop = SYSMMU_PROP_READWRITE;

	ret = of_property_read_u32_index(sysmmu->of_node, "qos", 0, &qos);

	if ((ret == 0) && (qos > 15)) {
		dev_err(sysmmu, "%s: Invalid QoS value %d specified\n",
				__func__, qos);
		qos = DEFAULT_QOS_VALUE;
	}

	drvdata->qos = (short)qos;

	/**
	 * Deprecate 'prop-map' child node of System MMU device nodes in FDT.
	 * It is not required to introduce new child node for boolean
	 * properties like 'block-stop' and 'tlbinv-nonblock'.
	 * 'tlbinv-nonblock' is H/W W/A to accellerates master H/W performance
	 * for 5.x and the earlier versions of System MMU.x.
	 * 'sysmmu,tlbinv-nonblock' is introduced, instead for those earlier
	 * versions.
	 * Instead of 'block-stop' in 'prop-map' childe node,
	 * 'sysmmu,block-when-stop' without a value is introduced to simplify
	 * the FDT node definitions.
	 * Likewise, prop-map.iomap and prop-map.winmap are replaced with
	 * sysmmu,pb-iomap and sysmmu,pb-winmap, respectively.
	 * For the compatibility with the existing FDT files, the 'prop-map'
	 * child node parsing is still kept.
	 */
	prop_node = of_get_child_by_name(sysmmu->of_node, "prop-map");
	if (prop_node) {
		if (!of_property_read_string(prop_node, "iomap", &s)) {
			int val;
			for (val = 1; val < ARRAY_SIZE(sysmmu_prop_opts);
									val++) {
				if (!strcasecmp(s, sysmmu_prop_opts[val])) {
					drvdata->prop &= ~SYSMMU_PROP_RW_MASK;
					drvdata->prop |= val;
					break;
				}
			}
		} else if (!of_property_read_u32_index(
					prop_node, "winmap", 0, &winmap)) {
			if (winmap) {
				drvdata->prop &= ~SYSMMU_PROP_RW_MASK;
				drvdata->prop |=
					winmap << SYSMMU_PROP_WINDOW_SHIFT;
			}
		}

		if (!of_property_read_string(prop_node, "tlbinv-nonblock", &s))
			if (strnicmp(s, "yes", 3) == 0)
				drvdata->prop |= SYSMMU_PROP_NONBLOCK_TLBINV;

		if (!of_property_read_string(prop_node, "block-stop", &s))
			if (strnicmp(s, "yes", 3) == 0)
				drvdata->prop |= SYSMMU_PROP_STOP_BLOCK;

		of_node_put(prop_node);
	}

	if (!of_property_read_string(sysmmu->of_node, "sysmmu,pb-iomap", &s)) {
		int val;
		for (val = 1; val < ARRAY_SIZE(sysmmu_prop_opts); val++) {
			if (!strcasecmp(s, sysmmu_prop_opts[val])) {
				drvdata->prop &= ~SYSMMU_PROP_RW_MASK;
				drvdata->prop |= val;
				break;
			}
		}
	} else if (!of_property_read_u32_index(
			sysmmu->of_node, "sysmmu,pb-winmap", 0, &winmap)) {
		if (winmap) {
			drvdata->prop &= ~SYSMMU_PROP_RW_MASK;
			drvdata->prop |= winmap << SYSMMU_PROP_WINDOW_SHIFT;
		}
	}

	if (of_find_property(sysmmu->of_node, "sysmmu,block-when-stop", NULL))
		drvdata->prop |= SYSMMU_PROP_STOP_BLOCK;

	if (of_find_property(sysmmu->of_node, "sysmmu,tlbinv-nonblock", NULL))
		drvdata->prop |= SYSMMU_PROP_NONBLOCK_TLBINV;

	return 0;
}

static int __init __sysmmu_setup(struct device *sysmmu,
				struct sysmmu_drvdata *drvdata)
{
	int ret;

	ret = __sysmmu_init_prop(sysmmu, drvdata);
	if (ret) {
		dev_err(sysmmu, "Failed to initialize sysmmu properties\n");
		return ret;
	}

	ret = __sysmmu_init_clock(sysmmu, drvdata);
	if (ret) {
		dev_err(sysmmu, "Failed to initialize gating clocks\n");
		return ret;
	}

	ret = __sysmmu_init_master(sysmmu);
	if (ret) {
		if (!IS_ERR(drvdata->clocks[SYSMMU_ACLK]))
			clk_unprepare(drvdata->clocks[SYSMMU_ACLK]);
		if (!IS_ERR(drvdata->clocks[SYSMMU_MASTER]))
			clk_unprepare(drvdata->clocks[SYSMMU_MASTER]);
		dev_err(sysmmu, "Failed to initialize master device.\n");
	}

	return ret;
}

static int __init exynos_sysmmu_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct sysmmu_drvdata *data;
	struct resource *res;

	data = devm_kzalloc(dev, sizeof(*data) , GFP_KERNEL);
	if (!data) {
		dev_err(dev, "Not enough memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Unable to find IOMEM region\n");
		return -ENOENT;
	}

	data->sfrbase = devm_request_and_ioremap(dev, res);
	if (!data->sfrbase) {
		dev_err(dev, "Unable to map IOMEM @ PA:%pa\n", &res->start);
		return -EBUSY;
	}

	ret = platform_get_irq(pdev, 0);
	if (ret <= 0) {
		dev_err(dev, "Unable to find IRQ resource\n");
		return ret;
	}

	ret = devm_request_irq(dev, ret, exynos_sysmmu_irq, 0,
				dev_name(dev), data);
	if (ret) {
		dev_err(dev, "Unabled to register interrupt handler\n");
		return ret;
	}

	pm_runtime_enable(dev);

	ret = exynos_iommu_init_event_log(SYSMMU_DRVDATA_TO_LOG(data),
					SYSMMU_LOG_LEN);
	if (!ret)
		sysmmu_add_log_to_debugfs(exynos_sysmmu_debugfs_root,
				SYSMMU_DRVDATA_TO_LOG(data), dev_name(dev));
	else
		return ret;

	ret = __sysmmu_setup(dev, data);
	if (!ret) {
		data->runtime_active = !pm_runtime_enabled(dev);
		data->sysmmu = dev;
		spin_lock_init(&data->lock);
		if (!sysmmu_drvdata_list) {
			sysmmu_drvdata_list = data;
		} else {
			data->next = sysmmu_drvdata_list->next;
			sysmmu_drvdata_list->next = data;
		}

		platform_set_drvdata(pdev, data);

		dev_info(dev, "[OK]\n");
	}

	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id sysmmu_of_match[] __initconst = {
	{ .compatible = SYSMMU_OF_COMPAT_STRING, },
	{ },
};
#endif

static struct platform_driver exynos_sysmmu_driver __refdata = {
	.probe		= exynos_sysmmu_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= MODULE_NAME,
		.of_match_table = of_match_ptr(sysmmu_of_match),
	}
};

static int exynos_iommu_domain_init(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv;
	int i;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgtable = (sysmmu_pte_t *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 2);
	if (!priv->pgtable)
		goto err_pgtable;

	priv->lv2entcnt = (short *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 1);
	if (!priv->lv2entcnt)
		goto err_counter;

	if (exynos_iommu_init_event_log(IOMMU_PRIV_TO_LOG(priv), IOMMU_LOG_LEN))
		goto err_init_event_log;

	for (i = 0; i < NUM_LV1ENTRIES; i += 8) {
		priv->pgtable[i + 0] = ZERO_LV2LINK;
		priv->pgtable[i + 1] = ZERO_LV2LINK;
		priv->pgtable[i + 2] = ZERO_LV2LINK;
		priv->pgtable[i + 3] = ZERO_LV2LINK;
		priv->pgtable[i + 4] = ZERO_LV2LINK;
		priv->pgtable[i + 5] = ZERO_LV2LINK;
		priv->pgtable[i + 6] = ZERO_LV2LINK;
		priv->pgtable[i + 7] = ZERO_LV2LINK;
	}

	pgtable_flush(priv->pgtable, priv->pgtable + NUM_LV1ENTRIES);

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->pgtablelock);
	INIT_LIST_HEAD(&priv->clients);

	domain->priv = priv;
	domain->handler = recover_fault_handler;

	return 0;

err_init_event_log:
	free_pages((unsigned long)priv->lv2entcnt, 1);
err_counter:
	free_pages((unsigned long)priv->pgtable, 2);
err_pgtable:
	kfree(priv);
	return -ENOMEM;
}

static void exynos_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner;
	unsigned long flags;
	int i;

	WARN_ON(!list_empty(&priv->clients));

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client)
		while (!exynos_sysmmu_disable(owner->dev))
			; /* until System MMU is actually disabled */

	while (!list_empty(&priv->clients))
		list_del_init(priv->clients.next);

	spin_unlock_irqrestore(&priv->lock, flags);

	for (i = 0; i < NUM_LV1ENTRIES; i++)
		if (lv1ent_page(priv->pgtable + i))
			kmem_cache_free(lv2table_kmem_cache,
				phys_to_virt(lv2table_base(priv->pgtable + i)));

	exynos_iommu_free_event_log(IOMMU_PRIV_TO_LOG(priv), IOMMU_LOG_LEN);

	free_pages((unsigned long)priv->pgtable, 2);
	free_pages((unsigned long)priv->lv2entcnt, 1);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int exynos_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iommu_domain *priv = domain->priv;
	phys_addr_t pgtable = virt_to_phys(priv->pgtable);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&priv->lock, flags);

	ret = __exynos_sysmmu_enable(dev, virt_to_phys(priv->pgtable), domain);

	if (ret == 0)
		list_add_tail(&owner->client, &priv->clients);

	spin_unlock_irqrestore(&priv->lock, flags);

	if (ret < 0) {
		dev_err(dev, "%s: Failed to attach IOMMU with pgtable %pa\n",
				__func__, &pgtable);
	} else {
		SYSMMU_EVENT_LOG_IOMMU_ATTACH(IOMMU_PRIV_TO_LOG(priv), dev);
		TRACE_LOG_DEV(dev,
			"%s: Attached new IOMMU with pgtable %pa %s\n",
			__func__, &pgtable, (ret == 0) ? "" : ", again");
	}

	return ret;
}

static void exynos_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct exynos_iommu_owner *owner;
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client) {
		if (owner == dev->archdata.iommu) {
			if (exynos_sysmmu_disable(dev))
				list_del_init(&owner->client);
			break;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (owner == dev->archdata.iommu) {
		SYSMMU_EVENT_LOG_IOMMU_DETACH(IOMMU_PRIV_TO_LOG(priv), dev);
		TRACE_LOG_DEV(dev, "%s: Detached IOMMU with pgtable %#lx\n",
					__func__, virt_to_phys(priv->pgtable));
	} else {
		dev_err(dev, "%s: No IOMMU is attached\n", __func__);
	}
}

static sysmmu_pte_t *alloc_lv2entry(struct exynos_iommu_domain *priv,
		sysmmu_pte_t *sent, unsigned long iova, short *pgcounter)
{
	if (lv1ent_fault(sent)) {
		sysmmu_pte_t *pent;
		struct exynos_iommu_owner *owner;
		unsigned long flags;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return ERR_PTR(-ENOMEM);

		*sent = mk_lv1ent_page(virt_to_phys(pent));
		kmemleak_ignore(pent);
		*pgcounter = NUM_LV2ENTRIES;
		pgtable_flush(pent, pent + NUM_LV2ENTRIES);
		pgtable_flush(sent, sent + 1);
		SYSMMU_EVENT_LOG_IOMMU_ALLOCSLPD(IOMMU_PRIV_TO_LOG(priv),
						iova & SECT_MASK);

		/*
		 * If pretched SLPD is a fault SLPD in zero_l2_table, FLPD cache
		 * caches the address of zero_l2_table. This function replaces
		 * the zero_l2_table with new L2 page table to write valid
		 * mappings.
		 * Accessing the valid area may cause page fault since FLPD
		 * cache may still caches zero_l2_table for the valid area
		 * instead of new L2 page table that have the mapping
		 * information of the valid area
		 * Thus any replacement of zero_l2_table with other valid L2
		 * page table must involve FLPD cache invalidation if the System
		 * MMU have prefetch feature and FLPD cache (version 3.3).
		 * FLPD cache invalidation is performed with TLB invalidation
		 * by VPN without blocking. It is safe to invalidate TLB without
		 * blocking because the target address of TLB invalidation is
		 * not currently mapped.
		 */
		spin_lock_irqsave(&priv->lock, flags);
		list_for_each_entry(owner, &priv->clients, client)
			sysmmu_tlb_invalidate_flpdcache(owner->dev, iova);
		spin_unlock_irqrestore(&priv->lock, flags);
	} else if (!lv1ent_page(sent)) {
		BUG();
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

static int lv1ent_check_page(struct exynos_iommu_domain *priv,
				sysmmu_pte_t *sent, short *pgcnt)
{
	if (lv1ent_page(sent)) {
		if (WARN_ON(*pgcnt != NUM_LV2ENTRIES))
			return -EADDRINUSE;

		kmem_cache_free(lv2table_kmem_cache, page_entry(sent, 0));

		*pgcnt = 0;

		SYSMMU_EVENT_LOG_IOMMU_FREESLPD(IOMMU_PRIV_TO_LOG(priv),
				iova_from_sent(priv->pgtable, sent));
	}

	return 0;
}

static void clear_lv1_page_table(sysmmu_pte_t *ent, int n)
{
	int i;
	for (i = 0; i < n; i++)
		ent[i] = ZERO_LV2LINK;
}

static void clear_lv2_page_table(sysmmu_pte_t *ent, int n)
{
	if (n > 0)
		memset(ent, 0, sizeof(*ent) * n);
}

static int lv1set_section(struct exynos_iommu_domain *priv,
			sysmmu_pte_t *sent, phys_addr_t paddr,
			  size_t size,  short *pgcnt)
{
	int ret;

	if (WARN_ON(!lv1ent_fault(sent) && !lv1ent_page(sent)))
		return -EADDRINUSE;

	if (size == SECT_SIZE) {
		ret = lv1ent_check_page(priv, sent, pgcnt);
		if (ret)
			return ret;
		*sent = mk_lv1ent_sect(paddr);
		pgtable_flush(sent, sent + 1);
	} else if (size == DSECT_SIZE) {
		int i;
		for (i = 0; i < SECT_PER_DSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(priv, sent, pgcnt);
			if (ret) {
				clear_lv1_page_table(sent - i, i);
				return ret;
			}
			*sent = mk_lv1ent_dsect(paddr);
		}
		pgtable_flush(sent - SECT_PER_DSECT, sent);
	} else {
		int i;
		for (i = 0; i < SECT_PER_SPSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(priv, sent, pgcnt);
			if (ret) {
				clear_lv1_page_table(sent - i, i);
				return ret;
			}
			*sent = mk_lv1ent_spsect(paddr);
		}
		pgtable_flush(sent - SECT_PER_SPSECT, sent);
	}

	return 0;
}

static int lv2set_page(sysmmu_pte_t *pent, phys_addr_t paddr,
		       size_t size, short *pgcnt)
{
	if (size == SPAGE_SIZE) {
		if (WARN_ON(!lv2ent_fault(pent)))
			return -EADDRINUSE;

		*pent = mk_lv2ent_spage(paddr);
		pgtable_flush(pent, pent + 1);
		*pgcnt -= 1;
	} else { /* size == LPAGE_SIZE */
		int i;
		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (WARN_ON(!lv2ent_fault(pent))) {
				clear_lv2_page_table(pent - i, i);
				return -EADDRINUSE;
			}

			*pent = mk_lv2ent_lpage(paddr);
		}
		pgtable_flush(pent - SPAGES_PER_LPAGE, pent);
		*pgcnt -= SPAGES_PER_LPAGE;
	}

	return 0;
}

static int exynos_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct exynos_iommu_domain *priv = domain->priv;
	sysmmu_pte_t *entry;
	unsigned long flags;
	int ret = -ENOMEM;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (size >= SECT_SIZE) {
		int num_entry = size / SECT_SIZE;
		struct exynos_iommu_owner *owner;

		ret = lv1set_section(priv, entry, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);

		spin_lock(&priv->lock);
		list_for_each_entry(owner, &priv->clients, client) {
			int i;
			for (i = 0; i < num_entry; i++)
				sysmmu_tlb_invalidate_flpdcache(owner->dev,
						iova + i * SECT_SIZE);
		}
		spin_unlock(&priv->lock);

		SYSMMU_EVENT_LOG_IOMMU_MAP(IOMMU_PRIV_TO_LOG(priv),
				iova, iova + size, paddr / SPAGE_SIZE);
	} else {
		sysmmu_pte_t *pent;

		pent = alloc_lv2entry(priv, entry, iova,
					&priv->lv2entcnt[lv1ent_offset(iova)]);

		if (IS_ERR(pent)) {
			ret = PTR_ERR(pent);
		} else {
			ret = lv2set_page(pent, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);

			SYSMMU_EVENT_LOG_IOMMU_MAP(IOMMU_PRIV_TO_LOG(priv),
					iova, iova + size, paddr / SPAGE_SIZE);
		}
	}

	if (ret)
		pr_err("%s: Failed(%d) to map %#zx bytes @ %pa\n",
			__func__, ret, size, &iova);

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return ret;
}

static void exynos_iommu_tlb_invalidate_entry(struct exynos_iommu_domain *priv,
					unsigned long iova)
{
	struct exynos_iommu_owner *owner;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client)
		sysmmu_tlb_invalidate_entry(owner->dev, iova, false);

	spin_unlock_irqrestore(&priv->lock, flags);
}

static size_t exynos_iommu_unmap(struct iommu_domain *domain,
					unsigned long iova, size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	size_t err_pgsize;
	sysmmu_pte_t *ent;
	unsigned long flags;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	ent = section_entry(priv->pgtable, iova);

	if (lv1ent_spsection(ent)) {
		if (WARN_ON(size < SPSECT_SIZE)) {
			err_pgsize = SPSECT_SIZE;
			goto err;
		}

		clear_lv1_page_table(ent, SECT_PER_SPSECT);

		pgtable_flush(ent, ent + SECT_PER_SPSECT);
		size = SPSECT_SIZE;
		goto done;
	}

	if (lv1ent_dsection(ent)) {
		if (WARN_ON(size < DSECT_SIZE)) {
			err_pgsize = DSECT_SIZE;
			goto err;
		}

		*ent = ZERO_LV2LINK;
		*(++ent) = ZERO_LV2LINK;
		pgtable_flush(ent, ent + 2);
		size = DSECT_SIZE;
		goto done;
	}

	if (lv1ent_section(ent)) {
		if (WARN_ON(size < SECT_SIZE)) {
			err_pgsize = SECT_SIZE;
			goto err;
		}

		*ent = ZERO_LV2LINK;
		pgtable_flush(ent, ent + 1);
		size = SECT_SIZE;
		goto done;
	}

	if (unlikely(lv1ent_fault(ent))) {
		if (size > SECT_SIZE)
			size = SECT_SIZE;
		goto done;
	}

	/* lv1ent_page(sent) == true here */

	ent = page_entry(ent, iova);

	if (unlikely(lv2ent_fault(ent))) {
		size = SPAGE_SIZE;
		goto done;
	}

	if (lv2ent_small(ent)) {
		*ent = 0;
		size = SPAGE_SIZE;
		pgtable_flush(ent, ent + 1);
		priv->lv2entcnt[lv1ent_offset(iova)] += 1;
		goto done;
	}

	/* lv1ent_large(ent) == true here */
	if (WARN_ON(size < LPAGE_SIZE)) {
		err_pgsize = LPAGE_SIZE;
		goto err;
	}

	clear_lv2_page_table(ent, SPAGES_PER_LPAGE);
	pgtable_flush(ent, ent + SPAGES_PER_LPAGE);

	size = LPAGE_SIZE;
	priv->lv2entcnt[lv1ent_offset(iova)] += SPAGES_PER_LPAGE;
done:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	SYSMMU_EVENT_LOG_IOMMU_UNMAP(IOMMU_PRIV_TO_LOG(priv),
						iova, iova + size);

	exynos_iommu_tlb_invalidate_entry(priv, iova);

	/* TLB invalidation is performed by IOVMM */
	return size;
err:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	pr_err("%s: Failed: size(%#zx) @ %pa is smaller than page size %#zx\n",
		__func__, size, &iova, err_pgsize);

	return 0;
}

static phys_addr_t exynos_iommu_iova_to_phys(struct iommu_domain *domain,
					     dma_addr_t iova)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long flags;
	sysmmu_pte_t *entry;
	phys_addr_t phys = 0;

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (lv1ent_spsection(entry)) {
		phys = spsection_phys(entry) + spsection_offs(iova);
	} else if (lv1ent_dsection(entry)) {
		phys = dsection_phys(entry) + dsection_offs(iova);
	} else if (lv1ent_section(entry)) {
		phys = section_phys(entry) + section_offs(iova);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, iova);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry) + lpage_offs(iova);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry) + spage_offs(iova);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return phys;
}

static struct iommu_ops exynos_iommu_ops = {
	.domain_init = &exynos_iommu_domain_init,
	.domain_destroy = &exynos_iommu_domain_destroy,
	.attach_dev = &exynos_iommu_attach_device,
	.detach_dev = &exynos_iommu_detach_device,
	.map = &exynos_iommu_map,
	.unmap = &exynos_iommu_unmap,
	.iova_to_phys = &exynos_iommu_iova_to_phys,
	.pgsize_bitmap = PGSIZE_BITMAP,
};

static int __sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr,
					exynos_iova_t iova,
					size_t size)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm = owner->vmm_data;
	struct iommu_domain *domain = vmm->domain;
	struct exynos_iommu_domain *priv = domain->priv;
	struct vm_area_struct *vma;
	unsigned long start = vaddr & PAGE_MASK;
	unsigned long end = PAGE_ALIGN(vaddr + size);
	bool is_pfnmap;
	sysmmu_pte_t *sent, *pent;
	int ret = 0;

	down_read(&mm->mmap_sem);

	BUG_ON((vaddr + size) < vaddr);
	/*
	 * Assumes that the VMA is safe.
	 * The caller must check the range of address space before calling this.
	 */
	vma = find_vma(mm, vaddr);
	if (!vma) {
		pr_err("%s: vma is null\n", __func__);
		ret = -EINVAL;
		goto out_unmap;
	}

	if (vma->vm_end < (vaddr + size)) {
		pr_err("%s: vma overflow: %#lx--%#lx, vaddr: %#lx, size: %zd\n",
			__func__, vma->vm_start, vma->vm_end, vaddr, size);
		ret = -EINVAL;
		goto out_unmap;
	}

	is_pfnmap = vma->vm_flags & VM_PFNMAP;

	TRACE_LOG_DEV(dev, "%s: unmap starts @ %#zx@%#lx\n",
			__func__, size, start);

	do {
		sysmmu_pte_t *pent_first;

		sent = section_entry(priv->pgtable, iova);
		if (lv1ent_fault(sent)) {
			ret = -EFAULT;
			goto out_unmap;
		}

		pent = page_entry(sent, iova);
		if (lv2ent_fault(pent)) {
			ret = -EFAULT;
			goto out_unmap;
		}

		pent_first = pent;

		do {
			if (!lv2ent_fault(pent) && !is_pfnmap)
				put_page(phys_to_page(spage_phys(pent)));

			*pent = 0;
			if (lv2ent_offset(iova) == NUM_LV2ENTRIES - 1) {
				pgtable_flush(pent_first, pent);
				iova += PAGE_SIZE;
				sent = section_entry(priv->pgtable, iova);
				if (lv1ent_fault(sent)) {
					ret = -EFAULT;
					goto out_unmap;
				}

				pent = page_entry(sent, iova);
				if (lv2ent_fault(pent)) {
					ret = -EFAULT;
					goto out_unmap;
				}

				pent_first = pent;
			} else {
				iova += PAGE_SIZE;
				pent++;
			}
		} while (start += PAGE_SIZE, start != end);

		if (pent_first != pent)
			pgtable_flush(pent_first, pent);
	} while (start != end);

	TRACE_LOG_DEV(dev, "%s: unmap done @ %#lx\n", __func__, start);

out_unmap:
	up_read(&mm->mmap_sem);

	if (ret) {
		pr_debug("%s: Ignoring unmapping for %#lx ~ %#lx\n",
					__func__, start, end);
	}

	return ret;
}

static sysmmu_pte_t *alloc_lv2entry_fast(struct exynos_iommu_domain *priv,
		sysmmu_pte_t *sent, unsigned long iova)
{
	if (lv1ent_fault(sent)) {
		sysmmu_pte_t *pent;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return ERR_PTR(-ENOMEM);

		*sent = mk_lv1ent_page(virt_to_phys(pent));
		kmemleak_ignore(pent);
		pgtable_flush(sent, sent + 1);
	} else if (WARN_ON(!lv1ent_page(sent))) {
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

int exynos_sysmmu_map_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr,
					exynos_iova_t iova,
					size_t size, bool write,
					bool shareable)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm = owner->vmm_data;
	struct iommu_domain *domain = vmm->domain;
	struct exynos_iommu_domain *priv = domain->priv;
	exynos_iova_t iova_start = iova;
	struct vm_area_struct *vma;
	unsigned long start, end;
	unsigned long pgd_next;
	int ret = -EINVAL;
	bool is_pfnmap;
	pgd_t *pgd;

	if (WARN_ON(size == 0))
		return 0;

	down_read(&mm->mmap_sem);

	/*
	 * Assumes that the VMA is safe.
	 * The caller must check the range of address space before calling this.
	 */
	vma = find_vma(mm, vaddr);
	if (!vma) {
		pr_err("%s: vma is null\n", __func__);
		up_read(&mm->mmap_sem);
		return -EINVAL;
	}

	if (vma->vm_end < (vaddr + size)) {
		pr_err("%s: vma overflow: %#lx--%#lx, vaddr: %#lx, size: %zd\n",
			__func__, vma->vm_start, vma->vm_end, vaddr, size);
		up_read(&mm->mmap_sem);
		return -EINVAL;
	}

	is_pfnmap = vma->vm_flags & VM_PFNMAP;

	start = vaddr & PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);

	TRACE_LOG_DEV(dev, "%s: map @ %#lx--%#lx, %zd bytes, vm_flags: %#lx\n",
			__func__, start, end, size, vma->vm_flags);

	pgd = pgd_offset(mm, start);
	do {
		unsigned long pmd_next;
		pmd_t *pmd;

		if (pgd_none_or_clear_bad(pgd)) {
			ret = -EBADR;
			goto out_unmap;
		}

		pgd_next = pgd_addr_end(start, end);
		pmd = pmd_offset((pud_t *)pgd, start);

		do {
			pte_t *pte;
			sysmmu_pte_t *pent, *pent_first;
			sysmmu_pte_t *sent;
			spinlock_t *ptl;

			if (pmd_none(*pmd)) {
				pmd = pmd_alloc(mm, (pud_t *)pgd, start);
				if (!pmd) {
					pr_err("%s: failed to alloc pmd\n",
								__func__);
					ret = -ENOMEM;
					goto out_unmap;
				}

				if (__pte_alloc(mm, vma, pmd, start)) {
					pr_err("%s: failed to alloc pte\n",
								__func__);
					ret = -ENOMEM;
					goto out_unmap;
				}
			} else if (pmd_bad(*pmd)) {
				pr_err("%s: bad pmd value %#lx\n", __func__,
						(unsigned long)pmd_val(*pmd));
				pmd_clear_bad(pmd);
				ret = -EBADR;
				goto out_unmap;
			}

			pmd_next = pmd_addr_end(start, pgd_next);
			pte = pte_offset_map(pmd, start);

			sent = section_entry(priv->pgtable, iova);
			pent = alloc_lv2entry_fast(priv, sent, iova);
			if (IS_ERR(pent)) {
				ret = PTR_ERR(pent); /* ENOMEM or EADDRINUSE */
				goto out_unmap;
			}

			pent_first = pent;
			ptl = pte_lockptr(mm, pmd);

			spin_lock(ptl);
			do {
				WARN_ON(!lv2ent_fault(pent));

				if (!pte_present(*pte) ||
					(write && !pte_write(*pte))) {
					if (pte_present(*pte) || pte_none(*pte)) {
						spin_unlock(ptl);
						ret = handle_pte_fault(mm,
							vma, start, pte, pmd,
							write ? FAULT_FLAG_WRITE : 0);
						if (IS_ERR_VALUE(ret)) {
							ret = -EIO;
							goto out_unmap;
						}
						spin_lock(ptl);
					}
				}

				if (!pte_present(*pte) ||
					(write && !pte_write(*pte))) {
					ret = -EPERM;
					spin_unlock(ptl);
					goto out_unmap;
				}

				if (!is_pfnmap)
					get_page(pte_page(*pte));
				*pent = mk_lv2ent_spage(__pfn_to_phys(
							pte_pfn(*pte)));
				if (shareable)
					set_lv2ent_shareable(pent);

				if (lv2ent_offset(iova) == (NUM_LV2ENTRIES - 1)) {
					pgtable_flush(pent_first, pent);
					iova += PAGE_SIZE;
					sent = section_entry(priv->pgtable, iova);
					pent = alloc_lv2entry_fast(priv, sent, iova);
					if (IS_ERR(pent)) {
						ret = PTR_ERR(pent);
						spin_unlock(ptl);
						goto out_unmap;
					}
					pent_first = pent;
				} else {
					iova += PAGE_SIZE;
					pent++;
				}
			} while (pte++, start += PAGE_SIZE, start < pmd_next);

			if (pent_first != pent)
				pgtable_flush(pent_first, pent);
			spin_unlock(ptl);
		} while (pmd++, start = pmd_next, start != pgd_next);

	} while (pgd++, start = pgd_next, start != end);

	ret = 0;
out_unmap:
	up_read(&mm->mmap_sem);

	if (ret) {
		pr_debug("%s: Ignoring mapping for %#lx ~ %#lx\n",
					__func__, start, end);
		__sysmmu_unmap_user_pages(dev, mm, vaddr, iova_start,
					start - (vaddr & PAGE_MASK));
	}

	return ret;
}

int exynos_sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr,
					exynos_iova_t iova,
					size_t size)
{
	if (WARN_ON(size == 0))
		return 0;

	return __sysmmu_unmap_user_pages(dev, mm, vaddr, iova, size);
}

static int __init exynos_iommu_init(void)
{
	struct page *page;
	int ret = -ENOMEM;

	lv2table_kmem_cache = kmem_cache_create("exynos-iommu-lv2table",
		LV2TABLE_SIZE, LV2TABLE_SIZE, 0, NULL);
	if (!lv2table_kmem_cache) {
		pr_err("%s: failed to create kmem cache\n", __func__);
		return -ENOMEM;
	}

	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!page) {
		pr_err("%s: failed to allocate fault page\n", __func__);
		goto err_fault_page;
	}
	fault_page = page_to_phys(page);

	ret = bus_set_iommu(&platform_bus_type, &exynos_iommu_ops);
	if (ret) {
		pr_err("%s: Failed to register IOMMU ops\n", __func__);
		goto err_set_iommu;
	}

	zero_lv2_table = kmem_cache_zalloc(lv2table_kmem_cache, GFP_KERNEL);
	if (zero_lv2_table == NULL) {
		pr_err("%s: Failed to allocate zero level2 page table\n",
			__func__);
		ret = -ENOMEM;
		goto err_zero_lv2;
	}

	exynos_sysmmu_debugfs_root = debugfs_create_dir("sysmmu", NULL);
	if (!exynos_sysmmu_debugfs_root)
		pr_err("%s: Failed to create debugfs entry\n", __func__);

	ret = platform_driver_register(&exynos_sysmmu_driver);
	if (ret) {
		pr_err("%s: Failed to register System MMU driver.\n", __func__);
		goto err_driver_register;
	}

	return 0;
err_driver_register:
	kmem_cache_free(lv2table_kmem_cache, zero_lv2_table);
err_zero_lv2:
	bus_set_iommu(&platform_bus_type, NULL);
err_set_iommu:
	__free_page(page);
err_fault_page:
	kmem_cache_destroy(lv2table_kmem_cache);
	return ret;
}
arch_initcall_sync(exynos_iommu_init);

#ifdef CONFIG_PM_SLEEP
static int sysmmu_pm_genpd_suspend(struct device *dev)
{
	struct sysmmu_list_data *list;
	int ret;

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	ret = pm_generic_suspend(dev);
	if (ret) {
		TRACE_LOG("<----- %s(%s) Failed\n", __func__, dev_name(dev));
		return ret;
	}

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);
		unsigned long flags;
		TRACE_LOG("Suspending %s...\n", dev_name(drvdata->sysmmu));
		spin_lock_irqsave(&drvdata->lock, flags);
		if (!drvdata->suspended && is_sysmmu_active(drvdata) &&
			(!pm_runtime_enabled(dev) || drvdata->runtime_active))
			__sysmmu_disable_nocount(drvdata);
		drvdata->suspended = true;
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	TRACE_LOG("<----- %s(%s)\n", __func__, dev_name(dev));

	return 0;
}

static int sysmmu_pm_genpd_resume(struct device *dev)
{
	struct sysmmu_list_data *list;
	int ret;

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);
		unsigned long flags;
		spin_lock_irqsave(&drvdata->lock, flags);
		if (drvdata->suspended && is_sysmmu_active(drvdata) &&
			(!pm_runtime_enabled(dev) || drvdata->runtime_active))
			__sysmmu_enable_nocount(drvdata);
		drvdata->suspended = false;
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	ret = pm_generic_resume(dev);

	TRACE_LOG("<----- %s(%s) OK\n", __func__, dev_name(dev));

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static void sysmmu_restore_state(struct device *dev)
{
	struct sysmmu_list_data *list;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *data = dev_get_drvdata(list->sysmmu);
		unsigned long flags;

		TRACE_LOG("%s(%s)\n", __func__, dev_name(data->sysmmu));

		SYSMMU_EVENT_LOG_POWERON(SYSMMU_DRVDATA_TO_LOG(data));

		spin_lock_irqsave(&data->lock, flags);
		if (!data->runtime_active && is_sysmmu_active(data))
			__sysmmu_enable_nocount(data);
		data->runtime_active = true;
		spin_unlock_irqrestore(&data->lock, flags);
	}
}

static void sysmmu_save_state(struct device *dev)
{
	struct sysmmu_list_data *list;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *data = dev_get_drvdata(list->sysmmu);
		unsigned long flags;

		TRACE_LOG("%s(%s)\n", __func__, dev_name(data->sysmmu));

		SYSMMU_EVENT_LOG_POWEROFF(SYSMMU_DRVDATA_TO_LOG(data));

		spin_lock_irqsave(&data->lock, flags);
		if (data->runtime_active && is_sysmmu_active(data))
			__sysmmu_disable_nocount(data);
		data->runtime_active = false;
		spin_unlock_irqrestore(&data->lock, flags);
	}
}

static int sysmmu_pm_genpd_save_state(struct device *dev)
{
	int (*cb)(struct device *__dev);
	int ret = 0;

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_suspend;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_suspend;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_suspend;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_suspend;

	if (cb)
		ret = cb(dev);

	if (ret == 0)
		sysmmu_save_state(dev);

	TRACE_LOG("<----- %s(%s) (cb = %pS) %s\n", __func__, dev_name(dev),
			cb, ret ? "Failed" : "OK");

	return ret;
}

static int sysmmu_pm_genpd_restore_state(struct device *dev)
{
	int (*cb)(struct device *__dev);
	int ret = 0;

	TRACE_LOG("%s(%s) ----->\n", __func__, dev_name(dev));

	if (dev->type && dev->type->pm)
		cb = dev->type->pm->runtime_resume;
	else if (dev->class && dev->class->pm)
		cb = dev->class->pm->runtime_resume;
	else if (dev->bus && dev->bus->pm)
		cb = dev->bus->pm->runtime_resume;
	else
		cb = NULL;

	if (!cb && dev->driver && dev->driver->pm)
		cb = dev->driver->pm->runtime_resume;

	sysmmu_restore_state(dev);

	if (cb)
		ret = cb(dev);

	if (ret)
		sysmmu_save_state(dev);

	TRACE_LOG("<----- %s(%s) (cb = %pS) %s\n", __func__, dev_name(dev),
			cb, ret ? "Failed" : "OK");

	return ret;
}
#endif

#ifdef CONFIG_PM_GENERIC_DOMAINS
static struct gpd_dev_ops sysmmu_devpm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.save_state = &sysmmu_pm_genpd_save_state,
	.restore_state = &sysmmu_pm_genpd_restore_state,
#endif
#ifdef CONFIG_PM_SLEEP
	.suspend = &sysmmu_pm_genpd_suspend,
	.resume = &sysmmu_pm_genpd_resume,
#endif
};
#endif /* CONFIG_PM_GENERIC_DOMAINS */

#ifdef CONFIG_PM_GENERIC_DOMAINS
static int sysmmu_hook_driver_register(struct notifier_block *nb,
					unsigned long val,
					void *p)
{
	struct device *dev = p;

	/*
	 * No System MMU assigned. See exynos_sysmmu_probe().
	 */
	if (dev->archdata.iommu == NULL)
		return 0;

	switch (val) {
	case BUS_NOTIFY_BIND_DRIVER:
	{
		if (dev->pm_domain) {
			int ret = pm_genpd_add_callbacks(
					dev, &sysmmu_devpm_ops, NULL);
			if (ret && (ret != -ENOSYS)) {
				dev_err(dev,
				"Failed to register 'dev_pm_ops' for iommu\n");
				return ret;
			}

			dev_info(dev, "exynos-iommu gpd_dev_ops inserted!\n");
		}

		break;
	}
	case BUS_NOTIFY_BOUND_DRIVER:
	{
		struct sysmmu_list_data *list;

		if (pm_runtime_enabled(dev) && dev->pm_domain)
			break;

		for_each_sysmmu_list(dev, list) {
			struct sysmmu_drvdata *data =
						dev_get_drvdata(list->sysmmu);
			unsigned long flags;
			spin_lock_irqsave(&data->lock, flags);
			if (is_sysmmu_active(data) && !data->runtime_active)
				__sysmmu_enable_nocount(data);
			data->runtime_active = true;
			pm_runtime_disable(data->sysmmu);
			spin_unlock_irqrestore(&data->lock, flags);
		}

		break;
	}
	case BUS_NOTIFY_UNBOUND_DRIVER:
	{
		struct exynos_iommu_owner *owner = dev->archdata.iommu;
		WARN_ON(!list_empty(&owner->client));
		__pm_genpd_remove_callbacks(dev, false);
		dev_info(dev, "exynos-iommu gpd_dev_ops removed!\n");
		break;
	}
	} /* switch (val) */

	return 0;
}

static struct notifier_block sysmmu_notifier = {
	.notifier_call = &sysmmu_hook_driver_register,
};

static int __init exynos_iommu_prepare(void)
{
	return bus_register_notifier(&platform_bus_type, &sysmmu_notifier);
}
subsys_initcall_sync(exynos_iommu_prepare);
#endif

static void sysmmu_dump_lv2_page_table(unsigned int lv1idx, sysmmu_pte_t *base)
{
	unsigned int i;
	for (i = 0; i < NUM_LV2ENTRIES; i += 4) {
		if (!base[i] && !base[i + 1] && !base[i + 2] && !base[i + 3])
			continue;
		pr_info("    LV2[%04d][%03d] %08x %08x %08x %08x\n",
			lv1idx, i,
			base[i], base[i + 1], base[i + 2], base[i + 3]);
	}
}

static void sysmmu_dump_page_table(sysmmu_pte_t *base)
{
	unsigned int i;
	phys_addr_t phys_base = virt_to_phys(base);

	pr_info("---- System MMU Page Table @ %pa (ZeroLv2Desc: %#x) ----\n",
		&phys_base, ZERO_LV2LINK);

	for (i = 0; i < NUM_LV1ENTRIES; i += 4) {
		unsigned int j;
		if ((base[i] == ZERO_LV2LINK) &&
			(base[i + 1] == ZERO_LV2LINK) &&
			(base[i + 2] == ZERO_LV2LINK) &&
			(base[i + 3] == ZERO_LV2LINK))
			continue;
		pr_info("LV1[%04d] %08x %08x %08x %08x\n",
			i, base[i], base[i + 1], base[i + 2], base[i + 3]);

		for (j = 0; j < 4; j++)
			if (lv1ent_page(&base[i + j]))
				sysmmu_dump_lv2_page_table(i + j,
						page_entry(&base[i + j], 0));
	}
}

void exynos_sysmmu_show_status(struct device *dev)
{
	struct sysmmu_list_data *list;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			continue;
		}

		pr_info("DUMPING SYSTEM MMU: %s\n", dev_name(drvdata->sysmmu));

		__master_clk_enable(drvdata);
		if (sysmmu_block(drvdata->sfrbase))
			dump_sysmmu_tlb_pb(drvdata->sfrbase);
		else
			pr_err("!!Failed to block Sytem MMU!\n");
		sysmmu_unblock(drvdata->sfrbase);

		__master_clk_disable(drvdata);
	}
}

void exynos_sysmmu_dump_pgtable(struct device *dev)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct sysmmu_list_data *list =
		list_entry(&owner->mmu_list, struct sysmmu_list_data, node);
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

	sysmmu_dump_page_table(phys_to_virt(drvdata->pgtable));
}

void exynos_sysmmu_show_ppc_event(struct device *dev)
{
	struct sysmmu_list_data *list;
	unsigned long flags;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		__master_clk_enable(drvdata);
		if (sysmmu_block(drvdata->sfrbase))
			dump_sysmmu_ppc_cnt(drvdata);
		else
			pr_err("!!Failed to block Sytem MMU!\n");
		sysmmu_unblock(drvdata->sfrbase);
		__master_clk_disable(drvdata);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

void exynos_sysmmu_clear_ppc_event(struct device *dev)
{
	struct sysmmu_list_data *list;
	unsigned long flags;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		__master_clk_enable(drvdata);
		if (sysmmu_block(drvdata->sfrbase)) {
			dump_sysmmu_ppc_cnt(drvdata);
			__raw_writel(0x2, drvdata->sfrbase + REG_PPC_PMNC);
			__raw_writel(0, drvdata->sfrbase + REG_PPC_CNTENS);
			__raw_writel(0, drvdata->sfrbase + REG_PPC_INTENS);
			drvdata->event_cnt = 0;
		} else
			pr_err("!!Failed to block Sytem MMU!\n");
		sysmmu_unblock(drvdata->sfrbase);
		__master_clk_disable(drvdata);

		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

int exynos_sysmmu_set_ppc_event(struct device *dev, int event)
{
	struct sysmmu_list_data *list;
	unsigned long flags;
	int ret = 0;

	for_each_sysmmu_list(dev, list) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(list->sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(drvdata->sysmmu,
				"%s: System MMU is not active\n", __func__);
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		__master_clk_enable(drvdata);
		if (sysmmu_block(drvdata->sfrbase)) {
			if (drvdata->event_cnt < MAX_NUM_PPC) {
				ret = sysmmu_set_ppc_event(drvdata, event);
				if (ret)
					pr_err("Not supported Event ID (%d)",
						event);
				else
					drvdata->event_cnt++;
			}
		} else
			pr_err("!!Failed to block Sytem MMU!\n");
		sysmmu_unblock(drvdata->sfrbase);
		__master_clk_disable(drvdata);

		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	return ret;
}
