/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5433 - ATLAS Core frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/clk-private.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-clock-exynos5433.h>
#include <mach/regs-pmu.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>
#include <mach/asv-exynos_cal.h>
#include <linux/of.h>

#define CPUFREQ_LEVEL_END_CA57	(L23 + 1)
#define L2_LOCAL_PWR_EN		0x7

#undef PRINT_DIV_VAL
#undef ENABLE_CLKOUT

static int max_support_idx_CA57;
static int min_support_idx_CA57 = (CPUFREQ_LEVEL_END_CA57 - 1);

static struct clk *mout_atlas;
static struct clk *mout_atlas_pll;
static struct clk *mout_bus_pll_div2;
static struct clk *mout_bus_pll_user;
static struct clk *fout_atlas_pll;

static unsigned int exynos5433_volt_table_CA57[CPUFREQ_LEVEL_END_CA57];
static unsigned int exynos5433_abb_table_CA57[CPUFREQ_LEVEL_END_CA57];

static int en_smpl_warn = 0;
static BLOCKING_NOTIFIER_HEAD(exynos_cpufreq_smpl_warn_notifier_list);
int exynos_cpufreq_smpl_warn_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&exynos_cpufreq_smpl_warn_notifier_list, nb);
}

int exynos_cpufreq_smpl_warn_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&exynos_cpufreq_smpl_warn_notifier_list, nb);
}

int exynos_cpufreq_smpl_warn_notify_call_chain(void)
{
	int ret = blocking_notifier_call_chain(&exynos_cpufreq_smpl_warn_notifier_list, 0, NULL);
	return notifier_to_errno(ret);
}
EXPORT_SYMBOL(exynos_cpufreq_smpl_warn_notify_call_chain);

static struct cpufreq_frequency_table exynos5433_freq_table_CA57[] = {
	{L0,  2500 * 1000},
	{L1,  2400 * 1000},
	{L2,  2300 * 1000},
	{L3,  2200 * 1000},
	{L4,  2100 * 1000},
	{L5,  2000 * 1000},
	{L6,  1900 * 1000},
	{L7,  1800 * 1000},
	{L8,  1700 * 1000},
	{L9,  1600 * 1000},
	{L10, 1500 * 1000},
	{L11, 1400 * 1000},
	{L12, 1300 * 1000},
	{L13, 1200 * 1000},
	{L14, 1100 * 1000},
	{L15, 1000 * 1000},
	{L16,  900 * 1000},
	{L17,  800 * 1000},
	{L18,  700 * 1000},
	{L19,  600 * 1000},
	{L20,  500 * 1000},
	{L21,  400 * 1000},
	{L22,  300 * 1000},
	{L23,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct apll_freq exynos5433_apll_freq_CA57[] = {
	/*
	 * values:
	 * freq
	 * clock divider for ATLAS1, ATLAS2, ACLK_ATLAS, PCLK_ATLAS, ATCLK, PCLK_DBG_ATLAS, SCLK_CNTCLK, RESERVED
	 * clock divider for SCLK_ATLAS_PLL, SCLK_HPM_ATLAS, RESERVED
	 * PLL M, P, S
	 */
	APLL_FREQ(2500, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 625, 6, 0),    /* ARM L0: 2.5GHz  */
	APLL_FREQ(2400, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 500, 5, 0),    /* ARM L1: 2.4GMHz */
	APLL_FREQ(2300, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 575, 6, 0),    /* ARM L2: 2.3GMHz */
	APLL_FREQ(2200, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 550, 6, 0),    /* ARM L3: 2.2GHz  */
	APLL_FREQ(2100, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 350, 4, 0),    /* ARM L4: 2.1GHz  */
	APLL_FREQ(2000, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 500, 6, 0),    /* ARM L5: 2.0GHz  */
	APLL_FREQ(1900, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 475, 6, 0),    /* ARM L6: 1.9GHz  */
	APLL_FREQ(1800, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 375, 5, 0),    /* ARM L7: 1.8GHz  */
	APLL_FREQ(1700, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 425, 6, 0),    /* ARM L8: 1.7GHz  */
	APLL_FREQ(1600, 0, 0, 4, 7, 7, 7, 7, 0, 1, 7, 0, 400, 6, 0),    /* ARM L9: 1.6GHz  */
	APLL_FREQ(1500, 0, 0, 3, 7, 7, 7, 7, 0, 1, 7, 0, 250, 4, 0),    /* ARM L10: 1.5GHz */
	APLL_FREQ(1400, 0, 0, 3, 7, 7, 7, 7, 0, 1, 7, 0, 350, 6, 0),    /* ARM L11: 1.4GHz */
	APLL_FREQ(1300, 0, 0, 3, 7, 7, 7, 7, 0, 1, 7, 0, 325, 6, 0),    /* ARM L12: 1.3GHz */
	APLL_FREQ(1200, 0, 0, 3, 7, 7, 7, 7, 0, 1, 7, 0, 500, 5, 1),    /* ARM L13: 1.2GHz */
	APLL_FREQ(1100, 0, 0, 3, 7, 7, 7, 7, 0, 1, 7, 0, 550, 6, 1),    /* ARM L14: 1.1GHz */
	APLL_FREQ(1000, 0, 0, 3, 7, 7, 7, 7, 0, 1, 7, 0, 500, 6, 1),    /* ARM L15: 1.0GHz */
	APLL_FREQ( 900, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 375, 5, 1),    /* ARM L16: 900MHz */
	APLL_FREQ( 800, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 400, 6, 1),    /* ARM L17: 800MHz */
	APLL_FREQ( 700, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 350, 6, 1),    /* ARM L18: 700MHz */
	APLL_FREQ( 600, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 500, 5, 2),    /* ARM L19: 600MHz */
	APLL_FREQ( 500, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 500, 6, 2),    /* ARM L20: 500MHz */
	APLL_FREQ( 400, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 400, 6, 2),    /* ARM L21: 400MHz */
	APLL_FREQ( 300, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 500, 5, 3),    /* ARM L22: 300MHz */
	APLL_FREQ( 200, 0, 0, 2, 7, 7, 7, 7, 0, 1, 7, 0, 400, 6, 3),    /* ARM L23: 200MHz */
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5433_CA57[CPUFREQ_LEVEL_END_CA57] = {
	1350000,	/* L0  2500 */
	1350000,	/* L1  2400 */
	1350000,	/* L2  2300 */
	1350000,	/* L3  2200 */
	1350000,	/* L4  2100 */
	1312500,	/* L5  2000 */
	1262500,	/* L6  1900 */
	1212500,	/* L7  1800 */
	1175000,	/* L8  1700 */
	1137500,	/* L9  1600 */
	1112500,	/* L10 1500 */
	1087500,	/* L11 1400 */
	1062500,	/* L12 1300 */
	1037500,	/* L13 1200 */
	1012500,	/* L14 1100 */
	 975000,	/* L15 1000 */
	 937500,	/* L16  900 */
	 912500,	/* L17  800 */
	 912500,	/* L18  700 */
	 900000,	/* L19  600 */
	 900000,	/* L20  500 */
	 900000,	/* L21  400 */
	 900000,	/* L22  300 */
	 900000,	/* L23  200 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos5433_bus_table_CA57[CPUFREQ_LEVEL_END_CA57] = {
	825000,		/* 2.5 GHz */
	825000,		/* 2.4 GHz */
	825000,		/* 2.3 GHz */
	825000,		/* 2.2 GHz */
	825000,		/* 2.1 GHz */
	825000,		/* 2.0 GHz */
	825000,		/* 1.9 GHz */
	825000,		/* 1.8 GHz */
	667000,		/* 1.7 MHz */
	667000,		/* 1.6 GHz */
	667000,		/* 1.5 GHz */
	667000,		/* 1.4 GHz */
	543000,		/* 1.3 GHz */
	543000,		/* 1.2 GHz */
	413000,		/* 1.1 GHz */
	413000,		/* 1.0 GHz */
	0,		/* 900 MHz */
	0,		/* 800 MHz */
	0,		/* 700 MHz */
	0,		/* 600 MHz */
	0,		/* 500 MHz */
	0,		/* 400 MHz */
	0,		/* 300 MHz */
	0,		/* 200 MHz */
};

static void exynos5433_set_ema_CA57(unsigned int target_volt)
{
	cal_set_ema(SYSC_DVFS_BIG, target_volt);
}

static int exynos5433_cpufreq_smpl_warn_notifier_call(
					struct notifier_block *notifer,
					unsigned long event, void *v)
{
	unsigned int state;

	state = __raw_readl(EXYNOS5430_DIV_EGL_PLL_FREQ_DET);
	state &= ~0x2;
	__raw_writel(state, EXYNOS5430_DIV_EGL_PLL_FREQ_DET);
	pr_info("%s: SMPL_WARN: EXYNOS5430_DIV_EGL_PLL_FREQ_DET is cleared\n",__func__);

	return NOTIFY_OK;
}

static int exynos5433_check_smpl_CA57(void)
{
	int tmp;

	tmp = __raw_readl(EXYNOS5430_DIV_EGL_PLL_FREQ_DET);

	if (tmp &= 0x2) {
		pr_info("%s: SMPL HAPPENED!", __func__);
		return 1;
	}

	return 0;
};

static struct notifier_block exynos5433_cpufreq_smpl_warn_notifier = {
	.notifier_call = exynos5433_cpufreq_smpl_warn_notifier_call,
};

static void exynos5433_set_clkdiv_CA57(unsigned int div_index)
{
	unsigned int tmp, tmp1;

	/* Change Divider - ATLAS0 */
	tmp = exynos5433_apll_freq_CA57[div_index].clk_div_cpu0;

	__raw_writel(tmp, EXYNOS5430_DIV_EGL0);

	while (__raw_readl(EXYNOS5430_DIV_STAT_EGL0) & 0x1111111)
		cpu_relax();

	/* Change Divider - ATLAS1 */
	tmp1 = exynos5433_apll_freq_CA57[div_index].clk_div_cpu1;

	__raw_writel(tmp1, EXYNOS5430_DIV_EGL1);

	while (__raw_readl(EXYNOS5430_DIV_STAT_EGL1) & 0x11)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5430_DIV_EGL0);
	tmp1 = __raw_readl(EXYNOS5430_DIV_EGL1);

	pr_info("%s: DIV_ATLAS0[0x%08x], DIV_ATLAS1[0x%08x]\n",
					__func__, tmp, tmp1);
#endif
}

static void exynos5433_set_atlas_pll_CA57(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

	/* 1. before change to BUS_PLL, set div for BUS_PLL output */
	if ((new_index > L17) && (old_index > L17))
		exynos5433_set_clkdiv_CA57(L17); /* pll_safe_idx of CA57 */

	/* 2. CLKMUX_SEL_ATLAS = MOUT_BUS_PLL_USER, ATLASCLK uses BUS_PLL_USER for lock time */
	if (clk_set_parent(mout_atlas, mout_bus_pll_user))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bus_pll_user->name, mout_atlas->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_EGL2);
		tmp &= EXYNOS5430_SRC_STAT_EGL2_EGL_MASK;
		tmp >>= EXYNOS5430_SRC_STAT_EGL2_EGL_SHIFT;
	} while (tmp != 0x2);

	/* 3. Set ATLAS_PLL Lock time */
	pdiv = ((exynos5433_apll_freq_CA57[new_index].mps &
		EXYNOS5430_PLL_PDIV_MASK) >> EXYNOS5430_PLL_PDIV_SHIFT);

	__raw_writel((pdiv * 150), EXYNOS5430_EGL_PLL_LOCK);

	/* 4. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
	tmp &= ~(EXYNOS5430_PLL_MDIV_MASK |
		EXYNOS5430_PLL_PDIV_MASK |
		EXYNOS5430_PLL_SDIV_MASK);
	tmp |= exynos5433_apll_freq_CA57[new_index].mps;
	__raw_writel(tmp, EXYNOS5430_EGL_PLL_CON0);

	/* 5. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5430_EGL_PLL_CON0_LOCKED_SHIFT)));

	/* 6. CLKMUX_SEL_ATLAS = MOUT_ATLAS_PLL */
	if (clk_set_parent(mout_atlas, mout_atlas_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_atlas_pll->name, mout_atlas->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_EGL2);
		tmp &= EXYNOS5430_SRC_STAT_EGL2_EGL_MASK;
		tmp >>= EXYNOS5430_SRC_STAT_EGL2_EGL_SHIFT;
	} while (tmp != 0x1);

	/* 7. restore original div value */
	if ((new_index > L17) && (old_index > L17))
		exynos5433_set_clkdiv_CA57(new_index);
}

static bool exynos5433_pms_change_CA57(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos5433_apll_freq_CA57[old_index].mps >>
				EXYNOS5430_PLL_PDIV_SHIFT);
	unsigned int new_pm = (exynos5433_apll_freq_CA57[new_index].mps >>
				EXYNOS5430_PLL_PDIV_SHIFT);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5433_set_frequency_CA57(unsigned int old_index,
					 unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5433_pms_change_CA57(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5433_set_clkdiv_CA57(new_index);
			/* 2. Change just s value in atlas_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5433_apll_freq_CA57[new_index].mps & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_EGL_PLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5433_set_clkdiv_CA57(new_index);
			/* 2. Change the atlas_pll m,p,s value */
			exynos5433_set_atlas_pll_CA57(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5433_pms_change_CA57(old_index, new_index)) {
			/* 1. Change just s value in atlas_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5433_apll_freq_CA57[new_index].mps & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_EGL_PLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5433_set_clkdiv_CA57(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the atlas_pll m,p,s value */
			exynos5433_set_atlas_pll_CA57(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5433_set_clkdiv_CA57(new_index);
		}
	}

	clk_set_rate(fout_atlas_pll, exynos5433_freq_table_CA57[new_index].frequency * 1000);
	pr_debug("[%s] Atlas NewFreq: %d OldFreq: %d\n", __func__, exynos5433_freq_table_CA57[new_index].frequency,
										exynos5433_freq_table_CA57[old_index].frequency);
}

static void __init set_volt_table_CA57(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA57; i++) {
		asv_volt = get_match_volt(ID_CL1, exynos5433_freq_table_CA57[i].frequency);
		if (!asv_volt)
			exynos5433_volt_table_CA57[i] = asv_voltage_5433_CA57[i];
		else
			exynos5433_volt_table_CA57[i] = asv_volt;

		pr_info("CPUFREQ of CA57  L%d : %d uV\n", i,
				exynos5433_volt_table_CA57[i]);

		exynos5433_abb_table_CA57[i] =
			get_match_abb(ID_CL1, exynos5433_freq_table_CA57[i].frequency);

		pr_info("CPUFREQ of CA57  L%d : ABB %d\n", i,
				exynos5433_abb_table_CA57[i]);
	}

#if defined(CONFIG_CPU_THERMAL) && defined(CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG)
	switch (exynos_get_table_ver()) {
	case 0 :
		max_support_idx_CA57 = L13;	/* 1.2 GHz */
		break;
	case 1 :
	case 4 :
		max_support_idx_CA57 = L8;	/* 1.7 GHz */
		break;
	case 2 :
		max_support_idx_CA57 = L7;	/* 1.8 GHz */
		break;
	default :
		max_support_idx_CA57 = L6;	/* 1.9 GHz */
	}

	if (is_max_limit_sample() == 1)
		max_support_idx_CA57 = L8;      /* 1.7 GHz */
#else
	max_support_idx_CA57 = L13;	/* 1.2 GHz */
#endif

	min_support_idx_CA57 = L18;	/* 700 MHz */

	pr_info("CPUFREQ of CA57 max_freq : L%d %u khz\n", max_support_idx_CA57,
		exynos5433_freq_table_CA57[max_support_idx_CA57].frequency);
	pr_info("CPUFREQ of CA57 min_freq : L%d %u khz\n", min_support_idx_CA57,
		exynos5433_freq_table_CA57[min_support_idx_CA57].frequency);
}

static bool exynos5433_is_alive_CA57(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5430_EGL_PLL_CON1);
	tmp &= EXYNOS5430_PLL_BYPASS_MASK;
	tmp >>= EXYNOS5430_PLL_BYPASS_SHIFT;

	return !tmp ? true : false;
}

int __init exynos_cpufreq_cluster1_init(struct exynos_dvfs_info *info)
{
	unsigned long rate;
	int tmp, ret;
	struct device_node *pmic_node;

	set_volt_table_CA57();

	mout_atlas = clk_get(NULL, "mout_egl");
	if (IS_ERR(mout_atlas)) {
		pr_err("failed get mout_atlas clk\n");
		return -EINVAL;
	}

	mout_atlas_pll = clk_get(NULL, "mout_egl_pll");
	if (IS_ERR(mout_atlas_pll)) {
		pr_err("failed get mout_atlas_pll clk\n");
		goto err_mout_atlas_pll;
	}

	if (clk_set_parent(mout_atlas, mout_atlas_pll)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_atlas_pll->name, mout_atlas->name);
		goto err_clk_set_parent_atlas;
	}

	mout_bus_pll_div2 = clk_get(NULL, "mout_bus_pll_div2");
	if (IS_ERR(mout_bus_pll_div2)) {
		pr_err("failed get mout_bus_pll_div2 clk\n");
		goto err_sclk_bus_pll;
	}

	mout_bus_pll_user = clk_get(NULL, "mout_bus_pll_egl_user");
	if (IS_ERR(mout_bus_pll_user)) {
		pr_err("failed get mout_bus_pll_user clk\n");
		goto err_mout_bus_pll_user;
	}

	if (clk_set_parent(mout_bus_pll_user, mout_bus_pll_div2)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bus_pll_div2->name, mout_bus_pll_user->name);
		goto err_clk_set_parent_bus_pll;
	}

	rate = clk_get_rate(mout_bus_pll_user) / 1000;

	fout_atlas_pll = clk_get(NULL, "fout_egl_pll");
	if (IS_ERR(fout_atlas_pll)) {
		pr_err("failed get fout_atlas_pll clk\n");
		goto err_fout_atlas_pll;
	}

	clk_put(mout_bus_pll_div2);

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L17;
	info->max_support_idx = max_support_idx_CA57;
	info->min_support_idx = min_support_idx_CA57;

	/* booting frequency is 1.2GHz */
	info->boot_cpu_min_qos = exynos5433_freq_table_CA57[L13].frequency;
	info->boot_cpu_max_qos = exynos5433_freq_table_CA57[L13].frequency;
	/* reboot limit frequency is 800MHz */

	info->reboot_limit_freq = exynos5433_freq_table_CA57[L17].frequency;
	info->bus_table = exynos5433_bus_table_CA57;
	info->cpu_clk = fout_atlas_pll;

	info->volt_table = exynos5433_volt_table_CA57;
	info->abb_table = exynos5433_abb_table_CA57;
	info->freq_table = exynos5433_freq_table_CA57;
	info->set_freq = exynos5433_set_frequency_CA57;
	info->need_apll_change = exynos5433_pms_change_CA57;
	info->is_alive = exynos5433_is_alive_CA57;
	info->set_ema = exynos5433_set_ema_CA57;

	/* get smpl_enable value */
	pmic_node = of_find_compatible_node(NULL, NULL, "samsung,s2mps13-pmic");

	if (!pmic_node) {
		pr_err("%s: faile to get pmic dt_node\n", __func__);
	} else {
		ret = of_property_read_u32(pmic_node, "smpl_warn_en", &en_smpl_warn);
		if (ret)
			pr_err("%s: faile to get Property of smpl_warn_en\n", __func__);
	}

	if (en_smpl_warn) {
		info->check_smpl = exynos5433_check_smpl_CA57;

		/* Enable SMPL */
		tmp = __raw_readl(EXYNOS5430_DIV_EGL_PLL_FREQ_DET);
		pr_info("%s SMPL ENABLE %d ", __func__, tmp);
		tmp &= ~0x1;
		tmp |= 0x1;
		pr_info("%s -- SMPL ENABLE %d ", __func__, tmp);
		__raw_writel(tmp, EXYNOS5430_DIV_EGL_PLL_FREQ_DET);

		/* Add smpl_notifer */
		exynos_cpufreq_smpl_warn_register_notifier(&exynos5433_cpufreq_smpl_warn_notifier);
	}

#ifdef ENABLE_CLKOUT
	/* dividing ATLAS_CLK to 1/16 */
	tmp = __raw_readl(EXYNOS5430_CLKOUT_CMU_EGL);
	tmp &= ~0xfff;
	tmp |= 0xf02;
	__raw_writel(tmp, EXYNOS5430_CLKOUT_CMU_EGL);
#endif

	return 0;

err_fout_atlas_pll:
err_clk_set_parent_bus_pll:
	clk_put(mout_bus_pll_user);
err_mout_bus_pll_user:
	clk_put(mout_bus_pll_div2);
err_sclk_bus_pll:
err_clk_set_parent_atlas:
	clk_put(mout_atlas_pll);
err_mout_atlas_pll:
	clk_put(mout_atlas);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
