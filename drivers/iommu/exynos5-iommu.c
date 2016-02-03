/* linux/drivers/iommu/exynos_iommu.c
 *
 * Copyright (c) 2013-2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_EXYNOS_IOMMU_DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/device.h>

#include <asm/pgtable.h>

#include "exynos-iommu.h"

#define CFG_LRU		0x1
#define CFG_MASK	0x0150FFFF /* Selecting bit 0-15, 20, 22 and 24 */
#define CFG_SYSSEL	(1 << 22) /* System MMU 3.2 only */

#define PB_INFO_NUM(reg)	((reg) & 0xFF) /* System MMU 3.3 only */

#define REG_MMU_FLUSH		0x00C
#define REG_MMU_FLUSH_ENTRY	0x010
#define REG_PT_BASE_ADDR	0x014
#define REG_INT_STATUS		0x018
#define REG_INT_CLEAR		0x01C
#define REG_PB_INFO		0x400
#define REG_PB_LMM		0x404
#define REG_PB_INDICATE		0x408
#define REG_PB_CFG		0x40C
#define REG_PB_START_ADDR	0x410
#define REG_PB_END_ADDR		0x414
#define REG_SPB_BASE_VPN	0x418

#define REG_PAGE_FAULT_ADDR	0x024
#define REG_AW_FAULT_ADDR	0x028
#define REG_AR_FAULT_ADDR	0x02C
#define REG_DEFAULT_SLAVE_ADDR	0x030
#define REG_FAULT_TRANS_INFO	0x04C
#define REG_L1TLB_READ_ENTRY	0x040
#define REG_L1TLB_ENTRY_PPN	0x044
#define REG_L1TLB_ENTRY_VPN	0x048

#define MAX_NUM_PBUF		6
#define SINGLE_PB_SIZE		16

#define NUM_MINOR_OF_SYSMMU_V3	4

#define MMU_TLB_ENT_NUM(val)	((val) & 0x7F)

enum exynos_sysmmu_inttype {
	SYSMMU_PAGEFAULT,
	SYSMMU_AR_MULTIHIT,
	SYSMMU_AW_MULTIHIT,
	SYSMMU_BUSERROR,
	SYSMMU_AR_SECURITY,
	SYSMMU_AR_ACCESS,
	SYSMMU_AW_SECURITY,
	SYSMMU_AW_PROTECTION, /* 7 */
	SYSMMU_FAULT_UNDEF,
	SYSMMU_FAULTS_NUM
};

static unsigned short fault_reg_offset[9] = {
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_AW_FAULT_ADDR
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNDEFINED FAULT"
};

static bool has_sysmmu_capable_pbuf(void __iomem *sfrbase, int *min)
{
	unsigned int ver;

	ver = __raw_sysmmu_version(sfrbase);
	if (min)
		*min = MMU_MIN_VER(ver);
	return MMU_MAJ_VER(ver) == 3;
}

void __sysmmu_tlb_invalidate(struct sysmmu_drvdata *drvdata,
				dma_addr_t iova, size_t size)
{
	void * __iomem sfrbase = drvdata->sfrbase;

	if (!WARN_ON(!sysmmu_block(sfrbase))) {
		__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
		SYSMMU_EVENT_LOG_TLB_INV_ALL(SYSMMU_DRVDATA_TO_LOG(drvdata));
	}
	sysmmu_unblock(sfrbase);
}

void __sysmmu_tlb_invalidate_flpdcache(void __iomem *sfrbase, dma_addr_t iova)
{
	if (__raw_sysmmu_version(sfrbase) == MAKE_MMU_VER(3, 3))
		__raw_writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase, dma_addr_t iova)
{
	__raw_writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

void __sysmmu_set_ptbase(void __iomem *sfrbase, phys_addr_t pfn_pgtable)
{
	__raw_writel(pfn_pgtable * PAGE_SIZE, sfrbase + REG_PT_BASE_ADDR);

	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
}

static void __sysmmu_set_prefbuf(void __iomem *pbufbase, unsigned long base,
					unsigned long size, int idx)
{
	__raw_writel(base, pbufbase + idx * 8);
	__raw_writel(size - 1 + base,  pbufbase + 4 + idx * 8);
}

/*
 * Offset of prefetch buffer setting registers are different
 * between SysMMU 3.1 and 3.2. 3.3 has a single prefetch buffer setting.
 */
static unsigned short
	pbuf_offset[NUM_MINOR_OF_SYSMMU_V3] = {0x04C, 0x04C, 0x070, 0x410};


static void __sysmmu_set_pbuf_ver31(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_bufs)
{
	unsigned long cfg =
		__raw_readl(drvdata->sfrbase + REG_MMU_CFG) & CFG_MASK;

	/* Only the first 2 buffers are set to PB */
	if (num_bufs >= 2) {
		/* Separate PB mode */
		cfg |= 2 << 28;

		if (prefbuf[1].size == 0)
			prefbuf[1].size = 1;
		__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[1],
					prefbuf[1].base, prefbuf[1].size, 1);
		SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata),
					cfg, num_bufs);
		SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata),
					1, prefbuf[1].base,
					prefbuf[1].base +  prefbuf[1].size - 1);
	} else {
		/* Combined PB mode */
		cfg |= 3 << 28;
		drvdata->num_pbufs = 1;
		drvdata->pbufs[0] = prefbuf[0];
		SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata),
					cfg, num_bufs);
	}


	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);

	if (prefbuf[0].size == 0)
		prefbuf[0].size = 1;
	__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[1],
				prefbuf[0].base, prefbuf[0].size, 0);
	SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata),
				0, prefbuf[0].base,
				prefbuf[0].base +  prefbuf[0].size - 1);
}

static void __sysmmu_set_pbuf_ver32(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_bufs)
{
	static char pbidx[3][3] = { /* [numbufs][PB index] */
		/* Index of Prefetch buffer entries */
		{1, 0, 0}, {3, 1, 0}, {3, 2, 1},
	};
	int i;
	unsigned long cfg =
		__raw_readl(drvdata->sfrbase + REG_MMU_CFG) & CFG_MASK;

	__raw_writel(0x1, drvdata->sfrbase + REG_MMU_FLUSH);

	cfg |= 7 << 16; /* enabling PB0 ~ PB2 */

	switch (num_bufs) {
	case 1:
		/* Combined PB mode (0 ~ 2) */
		cfg |= 1 << 19;
		break;
	case 2:
		/* Combined PB mode (0 ~ 1) */
		cfg |= 1 << 21;
		break;
	case 3:
		break;
	default:
		num_bufs = 3; /* Only the first 3 buffers are set to PB */
	}

	SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata), cfg, num_bufs);

	for (i = 0; i < 3; i++) {
		if (pbidx[num_bufs - 1][i]) {
			if (prefbuf[i].size == 0) {
				dev_err(drvdata->sysmmu,
					"%s: Trying to init PB[%d/%d]with zero-size\n",
					__func__, i, num_bufs);
				prefbuf[i].size = 1;
			}
			__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[2],
					prefbuf[i].base, prefbuf[i].size,
					pbidx[num_bufs - 1][i] - 1);

			SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata),
					pbidx[num_bufs - 1][i] - 1,
					prefbuf[i].base,
					prefbuf[i].base + prefbuf[i].size - 1);
		}
	}

	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);
}

static unsigned int find_lmm_preset(unsigned int num_pb, unsigned int num_bufs)
{
	static char lmm_preset[4][6] = {  /* [num of PB][num of buffers] */
	/*	  1,  2,  3,  4,  5,  6 */
		{ 1,  1,  0, -1, -1, -1}, /* num of pb: 3 */
		{ 3,  2,  1,  0, -1, -1}, /* num of pb: 4 */
		{-1, -1, -1, -1, -1, -1},
		{ 5,  5,  4,  2,  1,  0}, /* num of pb: 6 */
		};
	unsigned int lmm;

	BUG_ON(num_bufs > 6);
	lmm = lmm_preset[num_pb - 3][num_bufs - 1];
	BUG_ON(lmm == -1);
	return lmm;
}

static unsigned int find_num_pb(unsigned int num_pb, unsigned int lmm)
{
	static char lmm_preset[6][6] = { /* [pb_num - 1][pb_lmm] */
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{3, 2, 0, 0, 0, 0},
		{4, 3, 2, 1, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{6, 5, 4, 3, 3, 2},
	};

	num_pb = lmm_preset[num_pb - 1][lmm];
	BUG_ON(num_pb == 0);
	return num_pb;
}

static void __sysmmu_init_pb(void __iomem *sfrbase, unsigned int num_pb)
{
	unsigned int i = 0;

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, sfrbase + REG_PB_INDICATE);
		__raw_writel(0, sfrbase + REG_PB_CFG);
	}

	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
}

static void __sysmmu_set_pbuf_ver33(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_bufs)
{
	unsigned int i, num_pb, lmm;

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));

	__sysmmu_init_pb(drvdata->sfrbase,
			find_num_pb(num_pb,
				__raw_readl(drvdata->sfrbase + REG_PB_LMM)));

	lmm = find_lmm_preset(num_pb, (unsigned int)num_bufs);
	num_pb = find_num_pb(num_pb, lmm);

	__raw_writel(lmm, drvdata->sfrbase + REG_PB_LMM);

	SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata), lmm, num_bufs);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, drvdata->sfrbase + REG_PB_INDICATE);
		if ((prefbuf[i].size > 0) && (i < num_bufs)) {
			__sysmmu_set_prefbuf(drvdata->sfrbase + pbuf_offset[3],
					prefbuf[i].base, prefbuf[i].size, 0);
			__raw_writel(prefbuf[i].config | 1,
						drvdata->sfrbase + REG_PB_CFG);
			SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata),
				prefbuf[i].config | 1, prefbuf[i].base,
				prefbuf[i].size - 1 + prefbuf[i].base);
		} else {
			if (prefbuf[i].size == 0) {
				dev_err(drvdata->sysmmu,
				"%s: Trying to init PB[%d/%d]with zero-size\n",
				__func__, i, num_bufs);
			}

			SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata),
						0, 0, 0);
		}
	}
}

static void (*func_set_pbuf[NUM_MINOR_OF_SYSMMU_V3])
		(struct sysmmu_drvdata *, struct sysmmu_prefbuf *, int) = {
		__sysmmu_set_pbuf_ver31,
		__sysmmu_set_pbuf_ver31,
		__sysmmu_set_pbuf_ver32,
		__sysmmu_set_pbuf_ver33,
};


static void __sysmmu_disable_pbuf_ver31(struct sysmmu_drvdata *drvdata)
{
	unsigned int cfg = __raw_readl(drvdata->sfrbase + REG_MMU_CFG);

	cfg &= CFG_MASK;
	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);

	SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata), cfg, 0);
}

#define __sysmmu_disable_pbuf_ver32 __sysmmu_disable_pbuf_ver31

static void __sysmmu_disable_pbuf_ver33(struct sysmmu_drvdata *drvdata)
{
	unsigned int i, num_pb;

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));

	__sysmmu_init_pb(drvdata->sfrbase,
			find_num_pb(num_pb,
				__raw_readl(drvdata->sfrbase + REG_PB_LMM)));

	__raw_writel(0, drvdata->sfrbase + REG_PB_LMM);

	SYSMMU_EVENT_LOG_PBLMM(SYSMMU_DRVDATA_TO_LOG(drvdata), 0, 0);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, drvdata->sfrbase + REG_PB_INDICATE);
		__raw_writel(0, drvdata->sfrbase + REG_PB_CFG);
		SYSMMU_EVENT_LOG_PBSET(SYSMMU_DRVDATA_TO_LOG(drvdata), 0, 0, 0);
	}
}

static void (*func_disable_pbuf[NUM_MINOR_OF_SYSMMU_V3])
					(struct sysmmu_drvdata *) = {
		__sysmmu_disable_pbuf_ver31,
		__sysmmu_disable_pbuf_ver31,
		__sysmmu_disable_pbuf_ver32,
		__sysmmu_disable_pbuf_ver33,
};

static unsigned int __sysmmu_get_num_pb(struct sysmmu_drvdata *drvdata,
					int *min)
{
	if (!has_sysmmu_capable_pbuf(drvdata->sfrbase, min))
		return 0;

	switch (*min) {
	case 0:
	case 1:
		return 2;
	case 2:
		return 3;
	case 3:
		return PB_INFO_NUM(__raw_readl(drvdata->sfrbase + REG_PB_INFO));
	default:
		BUG();
	}

	return 0;
}

void __exynos_sysmmu_set_prefbuf_by_region(struct sysmmu_drvdata *drvdata,
			struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	unsigned int i;
	int num_bufs = 0;
	struct sysmmu_prefbuf prefbuf[6];
	unsigned int version;

	version = __raw_sysmmu_version(drvdata->sfrbase);
	if (version < MAKE_MMU_VER(3, 0))
		return;

	if ((num_reg == 0) || (pb_reg == NULL)) {
		/* Disabling prefetch buffers */
		func_disable_pbuf[MMU_MIN_VER(version)](drvdata);
		return;
	}

	for (i = 0; i < num_reg; i++) {
		if (((pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
					(drvdata->prop & SYSMMU_PROP_WRITE)) ||
			(!(pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
				 (drvdata->prop & SYSMMU_PROP_READ)))
			prefbuf[num_bufs++] = pb_reg[i];
	}

	func_set_pbuf[MMU_MIN_VER(version)](drvdata, prefbuf, num_bufs);
}

void __exynos_sysmmu_set_prefbuf_by_plane(struct sysmmu_drvdata *drvdata,
			unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption)
{
	unsigned int num_pb;
	int num_bufs, min;
	struct sysmmu_prefbuf prefbuf[6];

	num_pb = __sysmmu_get_num_pb(drvdata, &min);
	if (num_pb == 0) /* No Prefetch buffers */
		return;

	num_bufs = __prepare_prefetch_buffers_by_plane(drvdata,
			prefbuf, num_pb, inplanes, onplanes,
			ipoption, opoption);

	if (num_bufs == 0)
		func_disable_pbuf[min](drvdata);
	else
		func_set_pbuf[min](drvdata, prefbuf, num_bufs);
}

static void dump_sysmmu_pb_v31(void __iomem *sfrbase)
{
	unsigned int cfg, i;
	cfg = __raw_readl(sfrbase + REG_MMU_CFG);

	switch ((cfg >> 28) & 0x3) {
	case 2:
		pr_crit("PB[1] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x54),
				__raw_readl(sfrbase + 0x58),
				__raw_readl(sfrbase + 0x60));
		for (i = 0; i < 16; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0x70);
			pr_crit("PB[1][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0x74));
		}
		/* fall trhough */
	case 1:
		pr_crit("PB[0] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x4C),
				__raw_readl(sfrbase + 0x50),
				__raw_readl(sfrbase + 0x5C));
		for (i = 0; i < 16; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0x68);
			pr_crit("PB[0][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0x6C));
		}
		break;
	case 3:
		pr_crit("PB[0] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x4C),
				__raw_readl(sfrbase + 0x50),
				__raw_readl(sfrbase + 0x5C));
		for (i = 0; i < 32; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0x68);
			pr_crit("PB[0][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0x6C));
		}
	case 0:
		break;
	}
}

static void dump_sysmmu_pb_v32(void __iomem *sfrbase)
{
	unsigned int cfg, i;
	cfg = __raw_readl(sfrbase + REG_MMU_CFG);

	if (cfg & (1 << 19)) {
		pr_crit("PB[0] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x70),
				__raw_readl(sfrbase + 0x74),
				__raw_readl(sfrbase + 0x88));
		for (i = 0; i < 64; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0x98);
			pr_crit("PB[0][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0x9C));
		}
		return;
	} else if (cfg & (1 << 21)) {
		pr_crit("PB[0] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x70),
				__raw_readl(sfrbase + 0x74),
				__raw_readl(sfrbase + 0x88));
		for (i = 0; i < 32; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0x98);
			pr_crit("PB[0][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0x9C));
		}

		if ((cfg & (1 << 18)) == 0)
			return;

		pr_crit("PB[2] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x80),
				__raw_readl(sfrbase + 0x84),
				__raw_readl(sfrbase + 0x90));
		for (i = 0; i < 32; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0xA8);
			pr_crit("PB[2][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0xAC));
		}

		return;
	}

	if (cfg & (1 << 16)) {
		pr_crit("PB[0] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x70),
				__raw_readl(sfrbase + 0x74),
				__raw_readl(sfrbase + 0x88));
		for (i = 0; i < 16; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0x98);
			pr_crit("PB[0][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0x9C));
		}
	}

	if (cfg & (1 << 17)) {
		pr_crit("PB[1] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x78),
				__raw_readl(sfrbase + 0x7C),
				__raw_readl(sfrbase + 0x8C));
		for (i = 0; i < 16; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0xA0);
			pr_crit("PB[1][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0xA4));
		}
	}

	if (cfg & (1 << 18)) {
		pr_crit("PB[2] [%#010x, %#010x] Cached VA: %08x\n",
				__raw_readl(sfrbase + 0x80),
				__raw_readl(sfrbase + 0x84),
				__raw_readl(sfrbase + 0x90));
		for (i = 0; i < 32; i++) {
			__raw_writel((i << 4) | 1, sfrbase + 0xA8);
			pr_crit("PB[2][%2d] %08x\n", i,
				__raw_readl(sfrbase + 0xAC));
		}
	}
}

void dump_sysmmu_tlb_pb(void __iomem *sfrbase)
{
	unsigned int i, capa, lmm, tlb_ent_num, ver;

	ver = MMU_RAW_VER(__raw_readl(sfrbase + REG_MMU_VERSION));

	pr_crit("---------- System MMU Status -----------------------------\n");
	pr_crit("VERSION %d.%d, MMU_CFG: %#010x, MMU_STATUS: %#010x\n",
		MMU_MAJ_VER(ver), MMU_MIN_VER(ver),
		__raw_readl(sfrbase + REG_MMU_CFG),
		__raw_readl(sfrbase + REG_MMU_STATUS));

	/* TODO: dump tlb with vpn for sysmmu v1 */
	if (MMU_MAJ_VER(ver) < 2)
		return;

	pr_crit("---------- Level 1 TLB -----------------------------------\n");

	tlb_ent_num = MMU_TLB_ENT_NUM(__raw_readl(sfrbase + REG_MMU_VERSION));
	for (i = 0; i < tlb_ent_num; i++) {
		__raw_writel((i << 4) | 1, sfrbase + REG_L1TLB_READ_ENTRY);
		pr_crit("[%02d] VPN: %#010x, PPN: %#010x\n",
			i, __raw_readl(sfrbase + REG_L1TLB_ENTRY_VPN),
			__raw_readl(sfrbase + REG_L1TLB_ENTRY_PPN));
	}

	if (MMU_MAJ_VER(ver) < 3)
		return;

	pr_crit("---------- Prefetch Buffers ------------------------------\n");

	if (MMU_MIN_VER(ver) < 2) {
		dump_sysmmu_pb_v31(sfrbase);
		return;
	}

	if (MMU_MIN_VER(ver) < 3) {
		dump_sysmmu_pb_v32(sfrbase);
		return;
	}

	capa = __raw_readl(sfrbase + REG_PB_INFO);
	lmm = __raw_readl(sfrbase + REG_PB_LMM);

	pr_crit("PB_INFO: %#010x, PB_LMM: %#010x\n", capa, lmm);

	capa = find_num_pb(capa & 0xFF, lmm);

	for (i = 0; i < capa; i++) {
		__raw_writel(i, sfrbase + REG_PB_INDICATE);
		pr_crit("PB[%d] = CFG: %#010x, START: %#010x, END: %#010x\n", i,
			__raw_readl(sfrbase + REG_PB_CFG),
			__raw_readl(sfrbase + REG_PB_START_ADDR),
			__raw_readl(sfrbase + REG_PB_END_ADDR));

		pr_crit("PB[%d]_SUB0 BASE_VPN = %#010x\n", i,
			__raw_readl(sfrbase + REG_SPB_BASE_VPN));
		__raw_writel(i | 0x100, sfrbase + REG_PB_INDICATE);
		pr_crit("PB[%d]_SUB1 BASE_VPN = %#010x\n", i,
			__raw_readl(sfrbase + REG_SPB_BASE_VPN));
	}

	/* Reading L2TLB is not provided by H/W */
}

static void show_fault_information(struct sysmmu_drvdata *drvdata,
					enum exynos_sysmmu_inttype itype,
					unsigned long fault_addr)
{
	unsigned int info;
	phys_addr_t pgtable;
	unsigned int version;

	pgtable = __raw_readl(drvdata->sfrbase + REG_PT_BASE_ADDR);

	pr_crit("----------------------------------------------------------\n");
	pr_crit("%s %s at %#010lx by %s (page table @ %#010x)\n",
		dev_name(drvdata->sysmmu),
		sysmmu_fault_name[itype], fault_addr,
		dev_name(drvdata->master), pgtable);

	if (itype== SYSMMU_FAULT_UNDEF) {
		pr_crit("The fault is not caused by this System MMU.\n");
		pr_crit("Please check IRQ and SFR base address.\n");
		goto finish;
	}

	version = __raw_sysmmu_version(drvdata->sfrbase);
	if (version == MAKE_MMU_VER(3, 3)) {
		info = __raw_readl(drvdata->sfrbase +
				REG_FAULT_TRANS_INFO);
		pr_crit("AxID: %#x, AxLEN: %#x RW: %s\n",
			info & 0xFFFF, (info >> 16) & 0xF,
			(info >> 20) ? "WRITE" : "READ");
	}

	if (pgtable != drvdata->pgtable)
		pr_crit("Page table base of driver: %#010x\n",
			drvdata->pgtable);

	if (itype == SYSMMU_BUSERROR) {
		pr_crit("System MMU has failed to access page table\n");
		goto finish;
	}

	if (!pfn_valid(pgtable >> PAGE_SHIFT)) {
		pr_crit("Page table base is not in a valid memory region\n");
	} else {
		sysmmu_pte_t *ent;
		ent = section_entry(phys_to_virt(pgtable), fault_addr);
		pr_crit("Lv1 entry: %#010x\n", *ent);

		if (lv1ent_page(ent)) {
			ent = page_entry(ent, fault_addr);
			pr_crit("Lv2 entry: %#010x\n", *ent);
		}
	}

	dump_sysmmu_tlb_pb(drvdata->sfrbase);

finish:
	pr_crit("----------------------------------------------------------\n");
}

irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *drvdata = dev_id;
	unsigned int itype;
	unsigned long addr = -1;
	int ret = -ENOSYS;
	int flags = 0;

	WARN(!is_sysmmu_active(drvdata),
		"Fault occurred while System MMU %s is not enabled!\n",
		dev_name(drvdata->sysmmu));

	itype =  __ffs(__raw_readl(drvdata->sfrbase + REG_INT_STATUS));

	if (WARN_ON(!((itype >= 0) && (itype < SYSMMU_FAULT_UNDEF))))
		itype = SYSMMU_FAULT_UNDEF;
	else
		addr = __raw_readl(drvdata->sfrbase + fault_reg_offset[itype]);

	show_fault_information(drvdata, itype, addr);

	if (drvdata->domain) /* master is set if drvdata->domain exists */
		ret = report_iommu_fault(drvdata->domain,
					drvdata->master, addr, flags);

	panic("Unrecoverable System MMU Fault!!");

	return IRQ_HANDLED;
}

void __sysmmu_init_config(struct sysmmu_drvdata *drvdata)
{
	unsigned long cfg = CFG_LRU | CFG_QOS(drvdata->qos);
	unsigned int version;

	__raw_writel(0, drvdata->sfrbase + REG_MMU_CTRL);

	version = __raw_sysmmu_version(drvdata->sfrbase);
	if (version < MAKE_MMU_VER(3, 0))
		goto set_cfg;

	if (MMU_MAJ_VER(version) != 3)
		panic("%s: Failed to read version (%d.%d), master: %s\n",
			dev_name(drvdata->sysmmu), MMU_MAJ_VER(version),
			MMU_MIN_VER(version), dev_name(drvdata->master));


	if (MMU_MIN_VER(version) < 2)
		goto set_pb;

	BUG_ON(MMU_MIN_VER(version) > 3);

	cfg |= CFG_FLPDCACHE;
	cfg |= (MMU_MIN_VER(version) == 2) ? CFG_SYSSEL : CFG_ACGEN;
	cfg |= CFG_QOS_OVRRIDE;

set_pb:
	__exynos_sysmmu_set_prefbuf_by_plane(drvdata, 0, 0,
				SYSMMU_PBUFCFG_DEFAULT_INPUT,
				SYSMMU_PBUFCFG_DEFAULT_OUTPUT);
set_cfg:
	cfg |= __raw_readl(drvdata->sfrbase + REG_MMU_CFG) & ~CFG_MASK;
	__raw_writel(cfg, drvdata->sfrbase + REG_MMU_CFG);
}

void dump_sysmmu_ppc_cnt(struct sysmmu_drvdata *drvdata)
{
	static char ppc_event_name_preset[4][8] = { /* [type][event_name] */
		{8, 8, -1, -1, 0, 0, -1, -1}, /*SYSMMU v1.2 */
		{1, 1, 2, 2, 0, 0, -1, -1}, /* SYSMMU v2.1 */
		{8, 8, 5, 5, 0, 0, 4, 4}, /* SYSMMU v3.1/2 */
		{0, 8, 4, 5, 3, 9, 6, 7}, /* SYSMMU v3.3 */
	};

	unsigned int ver, type, offset;
	int i, maj;
	u32 cfg;

	ver = __raw_sysmmu_version(drvdata->sfrbase);
	maj = MMU_MAJ_VER(ver);

	offset = (maj < 2) ? 0x40 : 0x58;

	pr_crit("------------- System MMU PPC Status --------------\n");
	for (i = 0; i < drvdata->event_cnt; i++) {
		int event, write = 0;
		unsigned char preset;
		cfg = __raw_readl(drvdata->sfrbase +
				REG_PPC_EVENT_SEL(offset, i));
		event = cfg & 0xf;
		if (ver == MAKE_MMU_VER(3, 3)) {
			type = maj;
			if (event > 0x7) {
				write = 1;
				event -= 0x8;
			};
		} else {
			type = maj - 1;
			write = !(event % 2);
		}

		if (ppc_event_name_preset[type][event] < 0) {
			pr_err("PPC event value is unknown");
			continue;
		}

		preset = ppc_event_name_preset[type][event];
		pr_crit("%s %s %s CNT : %d", dev_name(drvdata->sysmmu),
			write ? "WRITE" : "READ", ppc_event_name[preset],
			__raw_readl(drvdata->sfrbase + REG_PPC_PMCNT(i)));
	}
	pr_crit("--------------------------------------------------\n");
}

int sysmmu_set_ppc_event(struct sysmmu_drvdata *drvdata, int event)
{
	static char ppc_event_preset[4][2][10] = { /* [type][write][event] */
		{ /* SYSMMU v1.2 */
		{5, -1, -1, -1, -1, -1, -1, -1, 1, -1},
		{4, -1, -1, -1, -1, -1, -1, -1, 0, -1},
		},
		{ /* SYSMMU v2.x */
		{5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
		{4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
		},
		{ /* SYSMMU v3.1/2 */
		{5, -1, -1, -1, 7, 3, -1, -1, 1, -1},
		{4, -1, -1, -1, 6, 2, -1, -1, 0, -1},
		},
		{ /* SYSMMU v3.3 */
		{0, -1, -1, 4, 2, 3, 6, 7, 1, 5},
		{8, -1, -1, 12, 10, 11, 14, 15, 9, 13},
		},
	};
	unsigned int ver, type, offset;
	u32 cfg, write = 0;
	char event_sel;

	if (event < 0 || event > TOTAL_ID_NUM)
		return -EINVAL;

	if (event > READ_FLPD_MISS_PREFETCH) {
		write = 1;
		event -= 0x10;
	}

	ver = __raw_sysmmu_version(drvdata->sfrbase);

	offset = (MMU_MAJ_VER(ver) < 2) ? 0x40 : 0x58;

	if (!(ver == MAKE_MMU_VER(3, 3)))
		type = MMU_MAJ_VER(ver) - 1;
	else
		type = MMU_MAJ_VER(ver);


	event_sel = ppc_event_preset[type][write][event];
	if (event_sel < 0)
		return -EINVAL;

	if (!drvdata->event_cnt)
		__raw_writel(0x1, drvdata->sfrbase + REG_PPC_PMNC);

	__raw_writel(event_sel, drvdata->sfrbase +
			REG_PPC_EVENT_SEL(offset, drvdata->event_cnt));
	cfg = __raw_readl(drvdata->sfrbase +
			REG_PPC_CNTENS);
	__raw_writel(cfg | 0x1 << drvdata->event_cnt,
			drvdata->sfrbase + REG_PPC_CNTENS);

	return 0;
}
