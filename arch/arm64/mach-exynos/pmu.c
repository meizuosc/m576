/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/threads.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/io.h>

#include <mach/pmu.h>
#ifndef CONFIG_SOC_EXYNOS7580
#include <mach/exynos-powermode.h>
#else
#include <mach/exynos-powermode-smp.h>
#endif
#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>

#define CPU_RESET_UP_CONFIG	0x7
static void exynos_cpu_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int core_config, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_PMU_CPU_CONFIGURATION(core + (4 * cluster));
	core_config = __raw_readl(addr);

	if ((core_config & LOCAL_PWR_CFG) != LOCAL_PWR_CFG) {
#ifdef CONFIG_SOC_EXYNOS7420
		if ((core_config & LOCAL_PWR_CFG) == CPU_RESET_UP_CONFIG) {
			unsigned int tmp = __raw_readl(EXYNOS_PMU_CPU_STATUS(core + (4 * cluster)));
			if ((tmp & LOCAL_PWR_CFG) != LOCAL_PWR_CFG)
				panic("%s: Abnormal core status\n", __func__);
		}
#endif

		core_config |= LOCAL_PWR_CFG;
		__raw_writel(core_config, addr);
	}
}

static void exynos_cpu_down(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core, cluster;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	addr = EXYNOS_PMU_CPU_CONFIGURATION(core + (4 * cluster));

	tmp = __raw_readl(addr);
	tmp &= ~(LOCAL_PWR_CFG);
	__raw_writel(tmp, addr);
}

static int exynos_cpu_state(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int core, cluster, val;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	cluster = MPIDR_AFFINITY_LEVEL(phys_cpu, 1);

	val = __raw_readl(EXYNOS_PMU_CPU_STATUS(core + (4 * cluster)))
						& LOCAL_PWR_CFG;

	return val == LOCAL_PWR_CFG;
}

struct exynos_cpu_power_ops exynos_cpu = {
	.power_up = exynos_cpu_up,
	.power_down = exynos_cpu_down,
	.power_state = exynos_cpu_state,
};

void exynos_set_wakeupmask(enum sys_powerdown mode)
{
	u64 eintmask = exynos_get_eint_wake_mask();
	u32 intmask = 0;

	/* Set external interrupt mask */
	__raw_writel((u32)eintmask, EXYNOS_PMU_EINT_WAKEUP_MASK);
#if defined(CONFIG_SOC_EXYNOS5433)
	__raw_writel((u32)(eintmask >> 32), EXYNOS_PMU_EINT_WAKEUP_MASK1);
#endif

	switch (mode) {
	case SYS_AFTR:
	case SYS_LPA:
	case SYS_ALPA:
		intmask = 0x40001000;
		break;
	case SYS_SLEEP:
		/* BIT(31): deactivate wakeup event monitoring circuit */
		intmask = 0x7FFFFFFF;
		break;
	default:
		break;
	}

	__raw_writel(intmask, EXYNOS_PMU_WAKEUP_MASK);

	__raw_writel(0xFFFF0000, EXYNOS_PMU_WAKEUP_MASK2);
	__raw_writel(0xFFFE0000, EXYNOS_PMU_WAKEUP_MASK3);
}

void exynos_clear_wakeupmask(void)
{
	__raw_writel(0, EXYNOS_PMU_EINT_WAKEUP_MASK);
	__raw_writel(0, EXYNOS_PMU_WAKEUP_MASK);
	__raw_writel(0, EXYNOS_PMU_WAKEUP_MASK2);
	__raw_writel(0, EXYNOS_PMU_WAKEUP_MASK3);
}


void exynos_cpu_sequencer_ctrl(bool enable)
{
	unsigned int tmp;

#if !defined(CONFIG_SOC_EXYNOS7580)
	tmp = __raw_readl(EXYNOS_PMU_ATLAS_CPUSEQUENCER_OPTION);
#else
	tmp = __raw_readl(EXYNOS_PMU_APL_CPUSEQUENCER_OPTION);
#endif
	if (enable)
		tmp |= USE_AUTOMATIC_PWRCTRL;
	else
		tmp &= ~USE_AUTOMATIC_PWRCTRL;
#if !defined(CONFIG_SOC_EXYNOS7580)
	__raw_writel(tmp, EXYNOS_PMU_ATLAS_CPUSEQUENCER_OPTION);
#else
	__raw_writel(tmp, EXYNOS_PMU_APL_CPUSEQUENCER_OPTION);
#endif
}

void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type)
{
	unsigned int value;
	unsigned int wdt_auto_reset_dis;
	unsigned int wdt_reset_mask;

	/*
	 * When SYS_WDTRESET is set, watchdog timer reset request is ignored
	 * by power management unit.
	 */
	if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE0) {
#if !defined(CONFIG_SOC_EXYNOS7580)
		wdt_auto_reset_dis = ATLAS_WDTRESET |
			APOLLO_WDTRESET;
		wdt_reset_mask = ATLAS_WDTRESET;
#else
		wdt_auto_reset_dis = CPU_WDTRESET | APL_WDTRESET;
		wdt_reset_mask = CPU_WDTRESET;
#endif
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE1) {
#if !defined(CONFIG_SOC_EXYNOS7580)
		wdt_auto_reset_dis = ATLAS_WDTRESET |
			APOLLO_WDTRESET;
		wdt_reset_mask = APOLLO_WDTRESET;
#else
		wdt_auto_reset_dis = CPU_WDTRESET | APL_WDTRESET;
		wdt_reset_mask = APL_WDTRESET;
#endif
	} else {
		pr_err("Failed to %s pmu wdt reset\n",
				on ? "enable" : "disable");
		return;
	}

	value = __raw_readl(EXYNOS_PMU_AUTOMATIC_DISABLE_WDT);
	if (on)
		value &= ~wdt_auto_reset_dis;
	else
		value |= wdt_auto_reset_dis;
	__raw_writel(value, EXYNOS_PMU_AUTOMATIC_DISABLE_WDT);
	value = __raw_readl(EXYNOS_PMU_MASK_WDT_RESET_REQUEST);
	if (on)
		value &= ~wdt_reset_mask;
	else
		value |= wdt_reset_mask;
	__raw_writel(value, EXYNOS_PMU_MASK_WDT_RESET_REQUEST);

	return;
}

static void exynos_enable_hw_trip(void)
{
	unsigned int tmp;

	/* Set output high at PSHOLD port and enable H/W trip */
	tmp = __raw_readl(EXYNOS_PMU_PS_HOLD_CONTROL);
	tmp |= (PS_HOLD_OUTPUT_HIGH | ENABLE_HW_TRIP);
	__raw_writel(tmp, EXYNOS_PMU_PS_HOLD_CONTROL);
}

#if defined(CONFIG_SOC_EXYNOS5433)
static void exynos5433_pmu_init(void)
{
	/* Set clock freeze cycle before and after ARM clamp to 0 */
	__raw_writel(0x0, EXYNOS5430_EGL_STOPCTRL);
	__raw_writel(0x0, EXYNOS5430_KFC_STOPCTRL);
}
#endif

#if defined(CONFIG_SOC_EXYNOS7420)
static void exynos7420_pmu_init(void)
{
	/* Set clock freeze cycle before and after ARM clamp to 0 */
	__raw_writel(0x0, EXYNOS7420_ATLAS_ARMCLK_STOPCTRL);
	__raw_writel(0x0, EXYNOS7420_APOLLO_ARMCLK_STOPCTRL);

	/* Set Apollo SRAM control sysreg to Dual rail mode*/
	__raw_writel(0x0, EXYNOS7420_VA_SYSREG + 0x0054);
}

static int __init exynos7420_atlas_dbg_pmu_init(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS_PMU_ATLAS_DBG_CONFIGURATION);
	tmp |= DBG_INITIATE_WAKEUP;
	__raw_writel(tmp, EXYNOS_PMU_ATLAS_DBG_CONFIGURATION);
	return 0;
}
early_initcall(exynos7420_atlas_dbg_pmu_init);
#endif

#if defined(CONFIG_SOC_EXYNOS7580)
static void exynos7580_pmu_init(void)
{
}
#endif

int __init exynos_pmu_init(void)
{
	unsigned int tmp;

	exynos_enable_hw_trip();

#if defined(CONFIG_SOC_EXYNOS5433)
	exynos5433_pmu_init();
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_pmu_init();
#elif defined(CONFIG_SOC_EXYNOS7580)
	exynos7580_pmu_init();
#endif

	/* select XXTI or TCXO to clkout */
	tmp = __raw_readl(EXYNOS_PMU_PMU_DEBUG);
	tmp &= ~CLKOUT_SEL_MASK;
#if !defined(CONFIG_SOC_EXYNOS7580)
	tmp |= CLKOUT_SEL_XXTI;
#else
	tmp |= CLKOUT_SEL_TCXO;
#endif
	__raw_writel(tmp, EXYNOS_PMU_PMU_DEBUG);

#if defined(CONFIG_SOC_EXYNOS7580)
	exynos_pmu_cp_init();
#endif
	pr_info("EXYNOS PMU Initialize\n");

	return 0;
}
