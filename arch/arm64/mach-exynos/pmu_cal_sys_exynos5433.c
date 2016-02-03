/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *		http://www.samsung.com
 *
 * Chip Abstraction Layer for System power down support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_CAL_SYS_PWRDOWN
/* for OS */
#include <mach/pmu_cal_sys.h>
#else
/* for firmware */
#include "util.h"
#include "pmu_cal_sys_exynos5433.h"
#endif

#define	writebits(addr, base, mask, val) \
	__raw_writel((__raw_readl(addr) & ~((mask) << (base))) | \
		(((mask) & (val)) << (base)), addr)

#define readbits(addr, base, mask) \
	((__raw_readl(addr) >> (base)) & (mask))

struct pmu_table {
	void __iomem *reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};

static struct pmu_table exynos5433_pmu_config[] = {
	/* { .addr = address, .val = { AFTR, STOP, DSTOP, DSTOP_PSR, LPD, LPA, ALPA, SLEEP } } */
	{ ATLAS_CPU0_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ DIS_IRQ_ATLAS_CPU0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ ATLAS_CPU1_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0,	0x0, 0x8 } },
	{ DIS_IRQ_ATLAS_CPU1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,	0x0, 0x0 } },
	{ ATLAS_CPU2_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0,	0x0, 0x8 } },
	{ DIS_IRQ_ATLAS_CPU2_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,	0x0, 0x0 } },
	{ ATLAS_CPU3_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0,	0x0, 0x8 } },
	{ DIS_IRQ_ATLAS_CPU3_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,	0x0, 0x0 } },
	{ APOLLO_CPU0_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0,	0x0, 0x8 } },
	{ DIS_IRQ_APOLLO_CPU0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ APOLLO_CPU1_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ DIS_IRQ_APOLLO_CPU1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ APOLLO_CPU2_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ DIS_IRQ_APOLLO_CPU2_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ APOLLO_CPU3_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ DIS_IRQ_APOLLO_CPU3_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ ATLAS_NONCPU_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ APOLLO_NONCPU_SYS_PWR_REG,			{ 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ A5IS_SYS_PWR_REG,				{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ DIS_IRQ_A5IS_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ DIS_IRQ_A5IS_CENTRAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ ATLAS_L2_SYS_PWR_REG,				{ 0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7 } },
	{ APOLLO_L2_SYS_PWR_REG,			{ 0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7 } },
	{ CLKSTOP_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ CLKRUN_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ RESET_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ RESET_CPUCLKSTOP_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ CLKSTOP_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ CLKRUN_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ RESET_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ DDRPHY_DLLLOCK_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1 } },
	{ DISABLE_PLL_CMU_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ DISABLE_PLL_AUD_PLL_SYS_PWR_REG,		{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x0 } },
	{ DISABLE_PLL_CMU_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ TOP_BUS_SYS_PWR_REG,				{ 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ TOP_RETENTION_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 } },
	{ TOP_PWR_SYS_PWR_REG,				{ 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3 } },
	{ TOP_BUS_MIF_SYS_PWR_REG,			{ 0x7, 0x0, 0x0, 0x0, 0x7, 0x0, 0x0, 0x0 } },
	{ TOP_RETENTION_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1 } },
	{ TOP_PWR_MIF_SYS_PWR_REG,			{ 0x3, 0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x3 } },
	{ LOGIC_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ OSCCLK_GATE_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 } },
	{ SLEEP_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ LOGIC_RESET_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ OSCCLK_GATE_MIF_SYS_PWR_REG,			{ 0x1, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1 } },
	{ SLEEP_RESET_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ MEMORY_TOP_SYS_PWR_REG,			{ 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_LPDDR3_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_JTAG_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_TOP_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_UART_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_EBIA_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_EBIB_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_SPI_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ PAD_ISOLATION_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 } },
	{ PAD_RETENTION_USBXTI_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_RETENTION_BOOTLDO_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_ISOLATION_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1 } },
	{ PAD_RETENTION_FSYSGENIO_SYS_PWR_REG,		{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ PAD_ALV_SEL_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ XXTI_SYS_PWR_REG,				{ 0x1, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ XXTI26_SYS_PWR_REG,				{ 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ EXT_REGULATOR_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ GPIO_MODE_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ GPIO_MODE_FSYS_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ GPIO_MODE_MIF_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ GPIO_MODE_AUD_SYS_PWR_REG,			{ 0x1, 0x1, 0x0, 0x0, 0x0, 0x1, 0x1, 0x0 } },
	{ GSCL_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ CAM0_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ MSCL_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ G3D_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ DISP_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0xF, 0xF, 0x0, 0x0, 0x0 } },
	{ CAM1_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ AUD_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0xF, 0xF, 0x0 } },
	{ FSYS_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ BUS2_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0xF, 0x0, 0x0, 0x0 } },
	{ G2D_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ ISP_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ MFC_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ HEVC_SYS_PWR_REG,				{ 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ RESET_SLEEP_FSYS_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ RESET_SLEEP_BUS2_SYS_PWR_REG,			{ 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ NULL, },
};

static void set_pmu_sys_pwr_reg(enum sys_powerdown mode)
{
	int i;

	for (i = 0; exynos5433_pmu_config[i].reg != NULL; i++)
		__raw_writel(exynos5433_pmu_config[i].val[mode],
				exynos5433_pmu_config[i].reg);
}

static void set_pmu_central_seq(bool enable)
{
	unsigned int tmp;

#define	CENTRALSEQ_PWR_CFG	0x10000
	/* central sequencer */
	tmp = __raw_readl(CENTRAL_SEQ_CONFIGURATION);
	if (enable)
		tmp &= ~CENTRALSEQ_PWR_CFG;
	else
		tmp |= CENTRALSEQ_PWR_CFG;
	__raw_writel(tmp, CENTRAL_SEQ_CONFIGURATION);

	/* central sequencer MIF */
	tmp = __raw_readl(CENTRAL_SEQ_MIF_CONFIGURATION);
	if (enable)
		tmp &= ~CENTRALSEQ_PWR_CFG;
	else
		tmp |= CENTRALSEQ_PWR_CFG;
	__raw_writel(tmp, CENTRAL_SEQ_MIF_CONFIGURATION);
}

void set_pmu_prepare_sys_powerdown(enum sys_powerdown mode)
{
	if (mode == SYS_SLEEP) {
		writebits(ATLAS_NONCPU_OPTION, 2, 1, 1);
		writebits(ATLAS_NONCPU_OPTION, 3, 1, 0);
		writebits(ATLAS_L2_OPTION, 4, 1, 0);
		writebits(APOLLO_NONCPU_OPTION, 2, 1, 1);
		writebits(APOLLO_NONCPU_OPTION, 3, 1, 0);
		writebits(APOLLO_L2_OPTION, 4, 1, 0);
	} else {
		writebits(ATLAS_NONCPU_OPTION, 2, 1, 1);
		writebits(ATLAS_NONCPU_OPTION, 3, 1, 1);
		writebits(ATLAS_L2_OPTION, 4, 1, 1);
		writebits(APOLLO_NONCPU_OPTION, 2, 1, 1);
		writebits(APOLLO_NONCPU_OPTION, 3, 1, 1);
		writebits(APOLLO_L2_OPTION, 4, 1, 1);
	}

#ifdef EVT1	/* FIXME: use vma */
	Outp32(0x105c2608, 0x10);
	Outp32(0x105c2628, 0x10);

	Outp32(0x105c2408, 0x01);
	Outp32(0x105c2428, 0x01);
#endif
}

static void set_pmu_pad_retention_release(void)
{
#define PAD_INITIATE_WAKEUP	(0x1 << 28)
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_LPDDR3_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_AUD_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_JTAG_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MMC2_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_TOP_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_UART_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MMC0_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MMC1_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_EBIA_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_EBIB_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_SPI_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_MIF_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_USBXTI_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_BOOTLDO_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_UFS_OPTION);
	__raw_writel(PAD_INITIATE_WAKEUP, PAD_RETENTION_FSYSGENIO_OPTION);
}

static void set_pmu_blk_fsys_on(void)
{
	if (__raw_readl(FSYS_STATUS) == 0x0) {
		__raw_writel(0xf, FSYS_CONFIGURATION);
		do {
			if ((__raw_readl(FSYS_STATUS) == 0xf))
				break;
		} while (1);
	}
}

static void set_pmu_lpi_mask(void)
{
	unsigned int tmp;

#define ATB_AUD_CSSYS		(1 << 7)
	tmp = __raw_readl(LPI_MASK_ATLAS_ASYNCBRIDGE);
	tmp |= ATB_AUD_CSSYS;
	__raw_writel(tmp, LPI_MASK_ATLAS_ASYNCBRIDGE);
}

static void init_pmu_spare(void)
{
	unsigned int tmp;

#define	EN_NONRET_RESET		0x01
	/* enable non retention flip-flop reset for wakeup */
	tmp = __raw_readl(PMU_SPARE0) | EN_NONRET_RESET;
	__raw_writel(tmp, PMU_SPARE0);
}

static void init_pmu_l2_option(void)
{
	int cluster;
	unsigned int tmp;

#define L2_OPTION(_nr)			(ATLAS_L2_OPTION + (_nr) * 0x20)
#define USE_DEACTIVATE_ACE		(0x1 << 19)
#define USE_DEACTIVATE_ACP		(0x1 << 18)
#define USE_AUTOMATIC_L2FLUSHREQ	(0x1 << 17)
#define USE_STANDBYWFIL2		(0x1 << 16)
#define USE_RETENTION			(0x1 << 4)
	/* disable automatic L2 flush */
	/* disable L2 retention */
	/* eanble STANDBYWFIL2, ACE/ACP for ATLAS only */
	for (cluster = 0; cluster < 2; cluster++) {
		tmp = __raw_readl(L2_OPTION(cluster));
		tmp &= ~(USE_AUTOMATIC_L2FLUSHREQ | USE_RETENTION);
		if (cluster == 0) /* ATLAS */
			tmp |= (USE_STANDBYWFIL2 | USE_DEACTIVATE_ACE | USE_DEACTIVATE_ACP);
		__raw_writel(tmp, L2_OPTION(cluster));
	}
}

static void init_pmu_cpu_option(void)
{
	int cpu;
	unsigned int tmp;

#define CPU_OPTION(_nr)		(ATLAS_CPU0_OPTION + (_nr) * 0x80)
#define USE_SMPEN		(0x1 << 28)
#define USE_STANDBYWFE		(0x1 << 24)
#define USE_STANDBYWFI		(0x1 << 16)
#define USE_SC_FEEDBACK		(0x1 << 1)
#define USE_SC_COUNTER		(0x1 << 0)
	/* use both sc_counter and sc_feedback */
	/* enable to wait for low SMP-bit at sys power down */
	for (cpu = 0; cpu < 8; cpu++) {
		tmp = __raw_readl(CPU_OPTION(cpu));
		tmp |= (USE_SC_FEEDBACK | USE_SC_COUNTER);
		tmp |= USE_SMPEN;
		tmp |= USE_STANDBYWFI;
		tmp &= ~USE_STANDBYWFE;
		__raw_writel(tmp, CPU_OPTION(cpu));
	}

#define CPU_DURATION(_nr)	(ATLAS_CPU0_DURATION0 + (_nr) * 0x80)
#define DUR_WAIT_RESET		(0xF << 20)
#define DUR_SCALL		(0xF << 4)
#define DUR_SCALL_VALUE		(0x1 << 4)
	for (cpu = 0; cpu < 8; cpu++) {
		tmp = __raw_readl(CPU_DURATION(cpu));
		tmp |= DUR_WAIT_RESET;
		tmp &= ~DUR_SCALL;
		tmp |= DUR_SCALL_VALUE;
		__raw_writel(tmp, CPU_DURATION(cpu));
	}
}

static void init_pmu_up_scheduler(void)
{
	unsigned int tmp;

#define ENABLE_ATLAS_CPU	(0x1 << 0)
	/* limit in-rush current for ATLAS local power up */
	tmp = __raw_readl(UP_SCHEDULER);
	tmp |= ENABLE_ATLAS_CPU;
	__raw_writel(tmp, UP_SCHEDULER);
}

static void __iomem *feed_list[] = {
	ATLAS_NONCPU_OPTION,
	APOLLO_NONCPU_OPTION,
	TOP_PWR_OPTION,
	TOP_PWR_MIF_OPTION,
	AUD_OPTION,
	CAM0_OPTION,
	DISP_OPTION,
	G2D_OPTION,
	G3D_OPTION,
	HEVC_OPTION,
	MSCL_OPTION,
	MFC_OPTION,
	GSCL_OPTION,
	FSYS_OPTION,
	ISP_OPTION,
	BUS2_OPTION,
};

static void init_pmu_feedback(void)
{
	int i;
	unsigned int tmp;

#define ARR_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
	for (i = 0; i < ARR_SIZE(feed_list); i++) {
		tmp = __raw_readl(feed_list[i]);
		tmp &= ~USE_SC_COUNTER;
		tmp |= USE_SC_FEEDBACK;
		__raw_writel(tmp, feed_list[i]);
	}
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_init
 * description: default settings at boot time
 */
void pmu_cal_sys_init(void)
{
	init_pmu_spare();
	init_pmu_feedback();
	init_pmu_l2_option();
	init_pmu_cpu_option();
	init_pmu_up_scheduler();
	set_pmu_lpi_mask();
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_prepare
 * description: settings before going to system power down
 */
void pmu_cal_sys_prepare(enum sys_powerdown mode)
{
	set_pmu_sys_pwr_reg(mode);
	set_pmu_central_seq(true);

	switch (mode) {
	case SYS_SLEEP:
		set_pmu_lpi_mask();
		set_pmu_blk_fsys_on();
		break;
	default:
		break;
	}
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_post
 * description: settings after normal wakeup
 */
void pmu_cal_sys_post(enum sys_powerdown mode)
{
	set_pmu_pad_retention_release();

	switch (mode) {
	case SYS_SLEEP:
		/* restore lpi mask after sleep */
		set_pmu_lpi_mask();
		break;
	default:
		break;
	}
}

/*
 * CAL 1.0 API
 * function: pmu_cal_sys_post
 * description: settings after early wakeup
 */
void pmu_cal_sys_earlywake(enum sys_powerdown mode)
{
	set_pmu_central_seq(false);
}

#ifdef CONFIG_CAL_SYS_PWRDOWN
static const struct pmu_cal_sys_ops sys_ops = {
	.sys_init	= pmu_cal_sys_init,
	.sys_prepare	= pmu_cal_sys_prepare,
	.sys_post	= pmu_cal_sys_post,
	.sys_earlywake	= pmu_cal_sys_earlywake,
};

void register_pmu_cal_sys_ops(const struct pmu_cal_sys_ops **cal)
{
	*cal = &sys_ops;
}
#endif

