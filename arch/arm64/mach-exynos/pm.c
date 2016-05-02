/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <asm/suspend.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#ifndef CONFIG_SOC_EXYNOS7580
#include <mach/exynos-powermode.h>
#else
#include <mach/exynos-powermode-smp.h>
#endif
#include <mach/pmu_cal_sys.h>

#define PSCI_INDEX_SLEEP		0x8

#define REG_INFORM0	(S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1	(S5P_VA_SYSRAM_NS + 0xC)

#define WAKEUP_STAT_EINT		(1 << 0)
#define WAKEUP_STAT_RTC_ALARM		(1 << 1)

void __iomem *wake_ext_int_pend;
static int resume_wakeup_flag = 0;
extern u32 exynos_eint_to_pin_num(int eint);

static void init_resume_wakeup_flag(void)
{
	resume_wakeup_flag = 0;
}

static int is_speedup_irq(struct irq_desc *desc, char *irq_name)
{
	return strstr(desc->action->name, irq_name) != NULL;
}

static void set_resume_wakeup_flag(int irq)
{
	struct irq_desc *desc;
	desc = irq_to_desc(irq);

	if (desc && desc->action && desc->action->name) {
		if (is_speedup_irq(desc, "KEY_HOME")
			|| is_speedup_irq(desc, "KEY_LEFTMETA")
			|| is_speedup_irq(desc, "KEY_POWER")
			|| is_speedup_irq(desc, "fts")
			|| is_speedup_irq(desc, "synaptics_dsx")) {
			resume_wakeup_flag = 1;
		}
	}
}

int get_resume_wakeup_flag(void)
{
	int flag = resume_wakeup_flag;

	pr_debug("%s: flag = %d\n", __func__, flag);
	/* Clear it for next calling */
	init_resume_wakeup_flag();

	return flag;
}

static void exynos_show_wakeup_reason_eint(void)
{
	int bit, i;
	long unsigned int ext_int_pend;
	unsigned long eint_wakeup_mask;
	bool found = 0;

	eint_wakeup_mask = __raw_readl(EXYNOS_PMU_EINT_WAKEUP_MASK);
	init_resume_wakeup_flag();

	for (i = 0; i < 4; i++) {
		ext_int_pend = __raw_readl(wake_ext_int_pend + i*0x4);
		for_each_set_bit(bit, &ext_int_pend, 8) {
			u32 gpio;
			int irq;
			if (eint_wakeup_mask & (1 << (i * 8 + bit)))
				continue;
			gpio = exynos_eint_to_pin_num((i * 8) + bit);
			irq = gpio_to_irq(gpio);
			set_resume_wakeup_flag(irq);
			log_wakeup_reason(irq);
			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

static void exynos_show_wakeup_reason(void)
{
	unsigned int wakeup_stat;

	wakeup_stat = __raw_readl(EXYNOS_PMU_WAKEUP_STAT);

	if (wakeup_stat & WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else
		pr_info("Resume caused by wakeup_stat 0x%08x\n",
			wakeup_stat);
}

static int exynos_pm_suspend(void)
{
	exynos_prepare_sys_powerdown(SYS_SLEEP);

	if (!(__raw_readl(EXYNOS_PMU_PMU_DEBUG) & CLKOUT_DISABLE))
		__raw_writel(0x1, EXYNOS_PMU_XXTI_SYS_PWR_REG);

	return 0;
}

static void exynos_cpu_prepare(void)
{
	/* inform0: return address, inform1: sleep mode */
	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);
	__raw_writel(virt_to_phys(cpu_resume), REG_INFORM0);
}

static int __maybe_unused exynos_cpu_suspend(unsigned long arg)
{
	/* This rule is moved to PSCI */
	return 1; /* abort suspend */
}

static void exynos_pm_resume(void)
{
	bool sleep_abort = exynos_sys_powerdown_enabled();

	exynos_wakeup_sys_powerdown(SYS_SLEEP, sleep_abort);

	exynos_show_wakeup_reason();
	return;
}

#if defined(CONFIG_SOC_EXYNOS7420)
/* List for Root clock gating */
static struct sfr_save save_rcg[] = {
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0018),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0118),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x016C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0218),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x021C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x023C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0240),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0318),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0320),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0418),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0420),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0518),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x051C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0524),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0540),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0544),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0618),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0624),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x064C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0650),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0714),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x071C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0724),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x0818),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x081C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x08D0),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x08D4),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x08D8),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x08DC),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1018),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x101C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1020),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1068),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x106C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1074),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1078),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1134),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1138),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1218),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1240),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1244),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x124C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1418),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x141C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1420),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1434),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1444),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1448),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x151C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1524),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1538),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x153C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1618),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x162C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1630),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1634),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1724),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1738),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1818),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1824),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1828),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1918),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1924),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x1928),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2018),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2024),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2028),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2118),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2124),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2128),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2218),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x221C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x222C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2230),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2234),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2318),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2418),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2440),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2518),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x251C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2550),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2554),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2558),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2618),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x261C),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2620),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2624),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2640),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2648),
	SFR_SAVE(EXYNOS7420_VA_SYSREG + 0x2818),

	SFR_SAVE(EXYNOS7420_VA_CMU_TOPC + 0x0D08),

	SFR_SAVE(EXYNOS7420_VA_CMU_TOP0 + 0x0D04),
	SFR_SAVE(EXYNOS7420_VA_CMU_TOP0 + 0x0D08),

	SFR_SAVE(EXYNOS7420_VA_CMU_TOP1 + 0x0D04),
};
#endif

static int exynos_pm_enter(suspend_state_t state)
{
	int ret;

	pr_debug("%s: state %d\n", __func__, state);

	exynos_cpu_prepare();

#if defined(CONFIG_SOC_EXYNOS7420)
	/* Save SFR list for RCG */
	exynos_save_sfr(save_rcg, ARRAY_SIZE(save_rcg));
#endif

	/* This will also act as our return point when
	 * we resume as it saves its own register state and restores it
	 * during the resume. */
	ret = cpu_suspend(PSCI_INDEX_SLEEP);
	if (ret) {
		pr_info("%s: return to originator\n", __func__);
		return ret;
	}

#if defined(CONFIG_SOC_EXYNOS7420)
	/* Restore SFR list for RCG */
	exynos_restore_sfr(save_rcg, ARRAY_SIZE(save_rcg));
#endif
	pr_debug("%s: post sleep, preparing to return\n", __func__);
	return 0;
}

static int exynos_pm_prepare(void)
{
	/* TODO */
	return 0;
}

static void exynos_pm_finish(void)
{
	/* TODO */
}

static const struct platform_suspend_ops exynos_pm_ops = {
	.enter		= exynos_pm_enter,
	.prepare	= exynos_pm_prepare,
	.finish		= exynos_pm_finish,
	.valid		= suspend_valid_only_mem,
};

static __init int exynos_pm_drvinit(void)
{
	pr_info("Exynos PM intialize\n");

	suspend_set_ops(&exynos_pm_ops);
	return 0;
}
arch_initcall(exynos_pm_drvinit);

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

static __init int exynos_pm_syscore_init(void)
{
	pr_info("Exynos PM syscore intialize\n");

	register_syscore_ops(&exynos_pm_syscore_ops);
	return 0;
}
arch_initcall(exynos_pm_syscore_init);
