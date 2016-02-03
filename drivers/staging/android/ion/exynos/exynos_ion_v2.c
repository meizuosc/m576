#ifdef CONFIG_OF_RESERVED_MEM

#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/smc.h>
#include <linux/genalloc.h>
#include <linux/exynos_ion.h>

#include "../ion.h"
#include "../ion_priv.h"

struct ion_device *ion_exynos;
static DEFINE_SPINLOCK(smc_lock);

/* starting from index=1 regarding default index=0 for system heap */
static int nr_heaps = 1;

struct exynos_ion_platform_heap {
	struct ion_platform_heap heap_data;
	struct reserved_mem *rmem;
	unsigned int id;
	unsigned int compat_ids;
	bool secure;
	bool reusable;
	bool protected;
	struct kref secure_ref;
	struct device dev;
	struct ion_heap *heap;
	struct mutex cma_lock;
};

static struct ion_platform_heap ion_noncontig_heap = {
	.name = "ion_noncontig_heap",
	.type = ION_HEAP_TYPE_SYSTEM,
	.id = EXYNOS_ION_HEAP_SYSTEM_ID,
};

static struct exynos_ion_platform_heap plat_heaps[ION_NUM_HEAPS];

static int __find_platform_heap_id(unsigned int heap_id)
{
	int i;

	for (i = 0; i < nr_heaps; i++) {
		if (heap_id == plat_heaps[i].id)
			break;
	}

	if (i == nr_heaps)
		return -EINVAL;

	return i;
}

static void __ion_secure_protect(struct exynos_ion_platform_heap *pdata)
{
	pr_info("%s: enter\n", __func__);

	pdata->protected = true;

	spin_lock(&smc_lock);

	/* passing region info */
	BUG_ON(exynos_smc(SMC_DRM_SECMEM_REGION_INFO, pdata->id - 1,
			pdata->rmem->base, pdata->rmem->size) != 0);

	/* protection */
	BUG_ON(exynos_smc(SMC_DRM_SECMEM_REGION_PROT, pdata->id - 1,
				SMC_PROTECTION_ENABLE, 0) != 0);

	spin_unlock(&smc_lock);

	pr_info("%s: protection enabled for heap %s\n", __func__,
						pdata->heap->name);
}

int ion_secure_protect(struct ion_heap *heap)
{
	struct exynos_ion_platform_heap *pdata;
	int id;

	id = __find_platform_heap_id(heap->id);
	if (id < 0) {
		pr_err("%s: invalid heap id(%d) for %s\n", __func__,
						heap->id, heap->name);
		return -EINVAL;
	}

	pdata = &plat_heaps[id];
	if (!pdata->secure) {
		pr_err("%s: heap %s is not secure heap\n", __func__, heap->name);
		return -EPERM;
	}

	if (unlikely(atomic_read(&pdata->secure_ref.refcount) == 0)) {
		kref_init(&pdata->secure_ref);
		__ion_secure_protect(pdata);
	} else {
		kref_get(&pdata->secure_ref);
	}

	return 0;
}

static void __ion_secure_unprotect(struct kref *kref)
{
	struct exynos_ion_platform_heap *pdata = container_of(kref,
				struct exynos_ion_platform_heap, secure_ref);

	pr_info("%s: enter\n", __func__);

	spin_lock(&smc_lock);

	/* unprotection */
	BUG_ON(exynos_smc(SMC_DRM_SECMEM_REGION_PROT,
			pdata->id - 1, SMC_PROTECTION_DISABLE, 0) != 0);

	spin_unlock(&smc_lock);
	pdata->protected = false;

	pr_info("%s: protection disabled for heap %s\n", __func__,
						pdata->heap->name);
}

int ion_secure_unprotect(struct ion_heap *heap)
{
	struct exynos_ion_platform_heap *pdata;
	int id;

	id = __find_platform_heap_id(heap->id);
	if (id < 0) {
		pr_err("%s: invalid heap id(%d) for %s\n", __func__,
						heap->id, heap->name);
		return -EINVAL;
	}

	pdata = &plat_heaps[id];
	if (!pdata->secure) {
		pr_err("%s: heap %s is not secure heap\n", __func__,
						pdata->heap->name);
		return -EPERM;
	}

	kref_put(&pdata->secure_ref, __ion_secure_unprotect);

	return 0;
}

bool ion_is_heap_available(struct ion_heap *heap,
				unsigned long flags, void *data)
{
	struct exynos_ion_platform_heap *pdata;
	bool protected = !!(flags & ION_FLAG_PROTECTED);
	struct cma_info info;
	size_t free_size;
	int id;

	id = __find_platform_heap_id(heap->id);
	if (id < 0) {
		pr_err("%s: invalid heap id(%d) for %s\n", __func__,
						heap->id, heap->name);
		return false;
	}

	pdata = &plat_heaps[id];

	if (protected == pdata->protected)
		return true;

	if (pdata->reusable) {
		if (dma_contiguous_info(&pdata->dev, &info)) {
			dev_err(&pdata->dev, "failed to retrieve "
					"region information\n");
			return false;
		}

		free_size = info.free;
	} else {
		if (!data) {
			pr_err("%s: gen_pool required\n", __func__);
			return false;
		}

		free_size = gen_pool_avail((struct gen_pool *) data);
	}

	if (free_size != pdata->rmem->size) {
		pr_err("%s: heap %s is now in-use, total=%zd, free=%zd\n",
				__func__, pdata->heap->name,
				(size_t) pdata->rmem->size, free_size);
		return false;
	}

	return true;
}

int ion_parse_heap_id(unsigned int heap_id_mask, unsigned int flags)
{
	unsigned int heap_id = 1;
	int i;

	pr_debug("%s: heap_id_mask=%#x, flags=%#x\n",
			__func__, heap_id_mask, flags);

	if (heap_id_mask != EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK) {
		if (heap_id_mask < (1 << ION_NUM_HEAPS)) {
			return heap_id_mask;
		} else {
			pr_err("%s: bad heap id %#x\n", __func__,
							heap_id_mask);
			return -EINVAL;
		}
	}

	if (flags & EXYNOS_ION_CONTIG_ID_MASK)
		heap_id += BITS_PER_INT - __fls(flags & EXYNOS_ION_CONTIG_ID_MASK);

	for (i = 1; i < nr_heaps; i++) {
		if ((plat_heaps[i].id == heap_id) ||
			(plat_heaps[i].compat_ids & (1 << heap_id)))
			break;
	}

	if (i == nr_heaps) {
		pr_err("%s: bad heap flags %#x\n", __func__, flags);
		return -EINVAL;
	}

	pr_debug("%s: found new heap id %d for %s\n", __func__,
			plat_heaps[i].id - 1, plat_heaps[i].heap_data.name);

	return (1 << plat_heaps[i].id);
}

static void exynos_ion_rmem_device_init(struct reserved_mem *rmem,
						struct device *dev)
{
	/* Nothing to do */
}

static void exynos_ion_rmem_device_release(struct reserved_mem *rmem,
						struct device *dev)
{
	/* Nothing to do */
}

static const struct reserved_mem_ops exynos_ion_rmem_ops = {
	.device_init	= exynos_ion_rmem_device_init,
	.device_release	= exynos_ion_rmem_device_release,
};

static int __init exynos_ion_reserved_mem_setup(struct reserved_mem *rmem)
{
	struct exynos_ion_platform_heap *pdata;
	struct ion_platform_heap *heap_data;
	unsigned long len = 0;
	__be32 *prop;

	BUG_ON(nr_heaps >= ION_NUM_HEAPS);

	pdata = &plat_heaps[nr_heaps];
	pdata->secure = !!of_get_flat_dt_prop(rmem->fdt_node, "secure", NULL);
	pdata->reusable = !!of_get_flat_dt_prop(rmem->fdt_node, "reusable", NULL);

	prop = of_get_flat_dt_prop(rmem->fdt_node, "id", &len);
	if (!prop) {
		pr_err("%s: no <id> found\n", __func__);
		return -EINVAL;
	}

	len /= sizeof(int);
	if (len != 1) {
		pr_err("%s: wrong <id> field definition\n", __func__);
		return -EINVAL;
	}

	/*
	 * id=0: system heap
	 * id=1 ~: contig heaps
	 */
	pdata->id = be32_to_cpu(prop[0]) + 1;
	if (pdata->id >= ION_NUM_HEAPS) {
		pr_err("%s: bad <id> number\n", __func__);
		return -EINVAL;
	}

	prop = of_get_flat_dt_prop(rmem->fdt_node, "compat-id", &len);
	if (prop) {
		len /= sizeof(int);
		while (len > 0) {
			pdata->compat_ids |=
				(1 << (be32_to_cpu(prop[--len]) + 1));
		}
	}

	rmem->ops = &exynos_ion_rmem_ops;
	pdata->rmem = rmem;

	heap_data = &pdata->heap_data;
	heap_data->id = pdata->id;
	heap_data->name = rmem->name;
	heap_data->base = rmem->base;
	heap_data->size = rmem->size;

	prop = of_get_flat_dt_prop(rmem->fdt_node, "alignment", &len);
	if (!prop)
		heap_data->align = PAGE_SIZE;
	else
		heap_data->align = be32_to_cpu(prop[0]);

	if (pdata->reusable) {
		heap_data->type = ION_HEAP_TYPE_DMA;
		heap_data->priv = &pdata->dev;

		if (dma_declare_contiguous(&pdata->dev, heap_data->size,
				heap_data->base, MEMBLOCK_ALLOC_ANYWHERE)) {
			pr_err("%s: failed to declare cma region %s\n",
						__func__, heap_data->name);
			return -EFAULT;
		}

		pr_info("CMA memory[%d]: %s:%#lx\n", heap_data->id,
				heap_data->name, (unsigned long)rmem->size);
	} else {
		heap_data->type = ION_HEAP_TYPE_CARVEOUT;
		heap_data->priv = rmem;
		pr_info("Reserved memory[%d]: %s:%#lx\n", heap_data->id,
				heap_data->name, (unsigned long)rmem->size);
	}

	atomic_set(&pdata->secure_ref.refcount, 0);
	nr_heaps++;

	return 0;
}

#define DECLARE_EXYNOS_ION_RESERVED_REGION(name) \
RESERVEDMEM_OF_DECLARE(name, "exynos5433-ion,"#name, exynos_ion_reserved_mem_setup)

DECLARE_EXYNOS_ION_RESERVED_REGION(common);
DECLARE_EXYNOS_ION_RESERVED_REGION(mfc_sh);
DECLARE_EXYNOS_ION_RESERVED_REGION(g2d_wfd);
DECLARE_EXYNOS_ION_RESERVED_REGION(video);
DECLARE_EXYNOS_ION_RESERVED_REGION(video_ext);
DECLARE_EXYNOS_ION_RESERVED_REGION(sectbl);
DECLARE_EXYNOS_ION_RESERVED_REGION(mfc_fw);
DECLARE_EXYNOS_ION_RESERVED_REGION(mfc_nfw);
DECLARE_EXYNOS_ION_RESERVED_REGION(secdma);
DECLARE_EXYNOS_ION_RESERVED_REGION(seclog);
DECLARE_EXYNOS_ION_RESERVED_REGION(reset_reason);
DECLARE_EXYNOS_ION_RESERVED_REGION(ramoops);

int ion_exynos_contig_heap_info(int region_id, phys_addr_t *phys, size_t *size)
{
	int i;

	for (i = 0; i < nr_heaps; i++) {
		if (plat_heaps[i].id == region_id + 1) {
			if (phys)
				*phys = plat_heaps[i].rmem->base;
			if (size)
				*size = plat_heaps[i].rmem->size;
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ion_exynos_contig_heap_info);

#ifdef CONFIG_DMA_CMA
static struct class *ion_cma_class;

static int __init exynos_ion_create_cma_class(void)
{
	ion_cma_class = class_create(THIS_MODULE, "ion_cma");
	if (IS_ERR(ion_cma_class)) {
		pr_err("%s: failed to create 'ion_cma' class - %ld\n",
			__func__, PTR_ERR(ion_cma_class));
		return PTR_ERR(ion_cma_class);
	}

	return 0;
}
static ssize_t region_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_ion_platform_heap *pdata = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n", pdata->heap_data.name);
}

static ssize_t region_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_ion_platform_heap *pdata = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pdata->id);
}

static ssize_t isolated_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cma_info info;
	/*
	 * lock is not required
	 * because this just shows a snapshot of the information
	 */
	if (dma_contiguous_info(dev, &info)) {
		dev_err(dev, "Failed to retrieve region information\n");
		info.isolated = false;
	}
	return scnprintf(buf, PAGE_SIZE, "%d\n", info.isolated ? 1 : 0);
}

static int exynos_ion_isolate_thread(void *p)
{
	if (dma_contiguous_isolate(p) != 0)
		dev_err(p, "Failed to isolate\n");

	flush_all_cpu_caches();

	if (!signal_pending(current))
		do_exit(0);

	return 0;
}

static ssize_t isolated_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct exynos_ion_platform_heap *pdata = dev_get_drvdata(dev);
	struct cma_info info;
	long new;
	int ret;

	if (!dev) {
		pr_err("%s: Invalid device info\n", __func__);
		return -ENODEV;
	}

	if (IS_ERR_OR_NULL(pdata)) {
		dev_err(dev, "Failed to get ion platform data\n");
		return -ENODEV;
	}

	/**
	 * 0 : deisolation
	 * 1 : synchronous isolation
	 * 2 : asynchronous isolation
	 * others: error
	 */

	ret = kstrtol(buf, 0, &new);
	if (ret)
		return ret;

	if ((new > 2) || (new < 0)) /* only 0, 1 and 2 are allowed */
		return -EINVAL;

	if (dma_contiguous_info(dev, &info)) {
		dev_err(dev, "Failed to retrieve region information\n");
		return -ENODEV;
	}
	mutex_lock(&pdata->cma_lock);
	if (info.isolated != (new != 0)) {
		if (!new) {
			dma_contiguous_deisolate(dev);
		} else if (new == 1) { /* synchronous isolation */
			if (dma_contiguous_isolate(dev) != 0)
				dev_err(dev, "Failed to isolate\n");
			else
				flush_all_cpu_caches();

		} else { /* new == 2. asynchronous isolation */
			struct task_struct *thr;
			struct sched_param param = { .sched_priority = 0 };
			thr = kthread_run(exynos_ion_isolate_thread,
				dev, "cma_isolation:%s", dev_name(dev));
			if (IS_ERR(thr))
				dev_err(dev,
					"Failed to create isolation thread\n");
			else
				sched_setscheduler(thr, SCHED_NORMAL, &param);
		}
	}

	mutex_unlock(&pdata->cma_lock);

	return count;
}

static struct device_attribute cma_regname_attr = __ATTR_RO(region_name);
static struct device_attribute cma_regid_attr = __ATTR_RO(region_id);
static DEVICE_ATTR(isolated, S_IRUSR | S_IWUSR, isolated_show, isolated_store);

static int __init exynos_ion_create_cma_devices(
				struct exynos_ion_platform_heap *pdata)
{
	struct device *dev;
	int ret;

	if (!pdata) {
		pr_err("%s: heap_data must be given\n", __func__);
		return -EINVAL;
	}

	dev = device_create(ion_cma_class, NULL, 0, pdata, "ion_%s",
						pdata->heap_data.name);
	if (IS_ERR(dev)) {
		pr_err("%s: failed to create device of %s\n", __func__,
						pdata->heap_data.name);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: Registered (region %d)\n", __func__, pdata->id);

	dev->cma_area = pdata->dev.cma_area;

	ret = device_create_file(dev, &cma_regid_attr);
	if (ret)
		dev_err(dev, "%s: failed to create %s file (%d)\n",
				__func__, cma_regid_attr.attr.name, ret);

	ret = device_create_file(dev, &cma_regname_attr);
	if (ret)
		dev_err(dev, "%s: failed to create %s file (%d)\n",
				__func__, cma_regname_attr.attr.name, ret);

	ret = device_create_file(dev, &dev_attr_isolated);
	if (ret)
		dev_err(dev, "%s: failed to create %s file (%d)\n",
				__func__, dev_attr_isolated.attr.name, ret);

	mutex_init(&pdata->cma_lock);

	return 0;
}
#else
static int __init exynos_ion_create_cma_class(void)
{
	return 0;
}

static int __init exynos_ion_create_cma_devices(
			struct exynos_ion_platform_heap *pdata)
{
	pr_err("%s: CMA should be configured for '%s'\n",
		__func__, pdata->heap_data.name);

	return 0;
}
#endif


static int exynos_ion_populate_heaps(struct platform_device *pdev,
				     struct ion_device *ion_dev)
{
	int i, ret;

	plat_heaps[0].reusable = false;
	memcpy(&plat_heaps[0].heap_data, &ion_noncontig_heap,
				sizeof(struct ion_platform_heap));

	for (i = 0; i < nr_heaps; i++) {
		plat_heaps[i].heap = ion_heap_create(&plat_heaps[i].heap_data);
		if (IS_ERR(plat_heaps[i].heap)) {
			pr_err("%s: failed to create heap %s[%d]\n", __func__,
					plat_heaps[i].heap_data.name,
					plat_heaps[i].id);
			ret = PTR_ERR(plat_heaps[i].heap);
			goto err;
		}

		ion_device_add_heap(ion_exynos, plat_heaps[i].heap);

		if (plat_heaps[i].reusable)
			exynos_ion_create_cma_devices(&plat_heaps[i]);
	}

	return 0;

err:
	while (i-- > 0)
		ion_heap_destroy(plat_heaps[i].heap);

	return ret;
}

static int __init exynos_ion_probe(struct platform_device *pdev)
{
	int ret;

	ion_exynos = ion_device_create(NULL);
	if (IS_ERR_OR_NULL(ion_exynos)) {
		pr_err("%s: failed to create ion device\n", __func__);
		return PTR_ERR(ion_exynos);
	}

	platform_set_drvdata(pdev, ion_exynos);

	ret = exynos_ion_create_cma_class();
	if (ret)
		return ret;

	return exynos_ion_populate_heaps(pdev, ion_exynos);
}

static int __devexit exynos_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < nr_heaps; i++)
		ion_heap_destroy(plat_heaps[i].heap);

	return 0;
}

static struct of_device_id exynos_ion_of_match[] __initconst = {
	{ .compatible	= "samsung,exynos5430-ion", },
	{ },
};

static struct platform_driver exynos_ion_driver __refdata = {
	.probe	= exynos_ion_probe,
	.remove	= exynos_ion_remove,
	.driver	= {
		.owner		= THIS_MODULE,
		.name		= "ion-exynos",
		.of_match_table	= of_match_ptr(exynos_ion_of_match),
	}
};

static int __init exynos_ion_init(void)
{
	return platform_driver_register(&exynos_ion_driver);
}

subsys_initcall(exynos_ion_init);

#ifdef CONFIG_HIGHMEM
#define exynos_sync_single_for_device(addr, size, dir)	dmac_map_area(addr, size, dir)
#define exynos_sync_single_for_cpu(addr, size, dir)	dmac_unmap_area(addr, size, dir)
#define exynos_sync_sg_for_device(dev, size, sg, nents, dir)	\
	ion_device_sync(ion_exynos, sgl, nents, dir, dmac_map_area, false)
#define exynos_sync_sg_for_cpu(dev, size, sg, nents, dir)	\
	ion_device_sync(ion_exynos, sgl, nents, dir, dmac_unmap_area, false)
#define exynos_sync_all					flush_all_cpu_caches
#else
static void __exynos_sync_sg_for_device(struct device *dev, size_t size,
					 struct scatterlist *sgl, int nelems,
					 enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nelems, i) {
		size_t sg_len = min(size, (size_t)sg->length);

		__dma_map_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
			       sg_len, dir);
		if (size > sg->length)
			size -= sg->length;
		else
			break;
	}
}

static void __exynos_sync_sg_for_cpu(struct device *dev, size_t size,
				      struct scatterlist *sgl, int nelems,
				      enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nelems, i) {
		size_t sg_len = min(size, (size_t)sg->length);

		__dma_unmap_area(phys_to_virt(dma_to_phys(dev, sg->dma_address)),
				 sg_len, dir);
		if (size > sg->length)
			size -= sg->length;
		else
			break;
	}
}

#define exynos_sync_single_for_device(addr, size, dir)	__dma_map_area(addr, size, dir)
#define exynos_sync_single_for_cpu(addr, size, dir)	__dma_unmap_area(addr, size, dir)
#define exynos_sync_sg_for_device(dev, size, sg, nents, dir)	\
	__exynos_sync_sg_for_device(dev, size, sg, nents, dir)
#define exynos_sync_sg_for_cpu(dev, size, sg, nents, dir)	\
	__exynos_sync_sg_for_cpu(dev, size, sg, nents, dir)
#define exynos_sync_all					flush_all_cpu_caches
#endif

void exynos_ion_sync_dmabuf_for_device(struct device *dev,
					struct dma_buf *dmabuf,
					size_t size,
					enum dma_data_direction dir)
{
	struct ion_buffer *buffer = (struct ion_buffer *) dmabuf->priv;

	if (IS_ERR_OR_NULL(buffer))
		BUG();

	if (!ion_buffer_cached(buffer) ||
			ion_buffer_fault_user_mappings(buffer))
		return;

	mutex_lock(&buffer->lock);

	pr_debug("%s: syncing for device %s, buffer: %p, size: %zd\n",
			__func__, dev ? dev_name(dev) : "null", buffer, size);

	trace_ion_sync_start(_RET_IP_, dev, dir, size,
			buffer->vaddr, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		exynos_sync_all();
	else if (!IS_ERR_OR_NULL(buffer->vaddr))
		exynos_sync_single_for_device(buffer->vaddr, size, dir);
	else
		exynos_sync_sg_for_device(dev, size, buffer->sg_table->sgl,
						buffer->sg_table->nents, dir);

	trace_ion_sync_end(_RET_IP_, dev, dir, size,
			buffer->vaddr, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);

	mutex_unlock(&buffer->lock);
}
EXPORT_SYMBOL(exynos_ion_sync_dmabuf_for_device);

void exynos_ion_sync_vaddr_for_device(struct device *dev,
					void *vaddr,
					size_t size,
					off_t offset,
					enum dma_data_direction dir)
{
	pr_debug("%s: syncing for device %s, vaddr: %p, size: %zd, offset: %ld\n",
			__func__, dev ? dev_name(dev) : "null",
			vaddr, size, offset);

	trace_ion_sync_start(_RET_IP_, dev, dir, size,
			vaddr, offset, size >= ION_FLUSH_ALL_HIGHLIMIT);

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		exynos_sync_all();
	else if (!IS_ERR_OR_NULL(vaddr))
		exynos_sync_single_for_device(vaddr + offset, size, dir);
	else
		BUG();

	trace_ion_sync_end(_RET_IP_, dev, dir, size,
			vaddr, offset, size >= ION_FLUSH_ALL_HIGHLIMIT);
}
EXPORT_SYMBOL(exynos_ion_sync_vaddr_for_device);

void exynos_ion_sync_sg_for_device(struct device *dev, size_t size,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	trace_ion_sync_start(_RET_IP_, dev, dir, size,
				0, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		exynos_sync_all();
	else
		exynos_sync_sg_for_device(dev, size, sgt->sgl, sgt->nents, dir);

	trace_ion_sync_end(_RET_IP_, dev, dir, size,
				0, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);
}
EXPORT_SYMBOL(exynos_ion_sync_sg_for_device);

void exynos_ion_sync_dmabuf_for_cpu(struct device *dev,
					struct dma_buf *dmabuf,
					size_t size,
					enum dma_data_direction dir)
{
	struct ion_buffer *buffer = (struct ion_buffer *) dmabuf->priv;

	if (dir == DMA_TO_DEVICE)
		return;

	if (IS_ERR_OR_NULL(buffer))
		BUG();

	if (!ion_buffer_cached(buffer) ||
			ion_buffer_fault_user_mappings(buffer))
		return;

	mutex_lock(&buffer->lock);

	pr_debug("%s: syncing for cpu %s, buffer: %p, size: %zd\n",
			__func__, dev ? dev_name(dev) : "null", buffer, size);

	trace_ion_sync_start(_RET_IP_, dev, dir, size,
			buffer->vaddr, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		exynos_sync_all();
	else if (!IS_ERR_OR_NULL(buffer->vaddr))
		exynos_sync_single_for_cpu(buffer->vaddr, size, dir);
	else
		exynos_sync_sg_for_cpu(dev, size, buffer->sg_table->sgl,
						buffer->sg_table->nents, dir);

	trace_ion_sync_end(_RET_IP_, dev, dir, size,
			buffer->vaddr, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);

	mutex_unlock(&buffer->lock);
}
EXPORT_SYMBOL(exynos_ion_sync_dmabuf_for_cpu);

void exynos_ion_sync_vaddr_for_cpu(struct device *dev,
					void *vaddr,
					size_t size,
					off_t offset,
					enum dma_data_direction dir)
{
	if (dir == DMA_TO_DEVICE)
		return;

	pr_debug("%s: syncing for cpu %s, vaddr: %p, size: %zd, offset: %ld\n",
			__func__, dev ? dev_name(dev) : "null",
			vaddr, size, offset);

	trace_ion_sync_start(_RET_IP_, dev, dir, size,
			vaddr, offset, size >= ION_FLUSH_ALL_HIGHLIMIT);

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		exynos_sync_all();
	else if (!IS_ERR_OR_NULL(vaddr))
		exynos_sync_single_for_cpu(vaddr + offset, size, dir);
	else
		BUG();

	trace_ion_sync_end(_RET_IP_, dev, dir, size,
			vaddr, offset, size >= ION_FLUSH_ALL_HIGHLIMIT);
}
EXPORT_SYMBOL(exynos_ion_sync_vaddr_for_cpu);

void exynos_ion_sync_sg_for_cpu(struct device *dev, size_t size,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	trace_ion_sync_start(_RET_IP_, dev, dir, size,
				0, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);

	if (dir == DMA_TO_DEVICE)
		return;

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		exynos_sync_all();
	else
		exynos_sync_sg_for_cpu(dev, size, sgt->sgl, sgt->nents, dir);

	trace_ion_sync_end(_RET_IP_, dev, dir, size,
				0, 0, size >= ION_FLUSH_ALL_HIGHLIMIT);
}
EXPORT_SYMBOL(exynos_ion_sync_sg_for_cpu);

#endif
