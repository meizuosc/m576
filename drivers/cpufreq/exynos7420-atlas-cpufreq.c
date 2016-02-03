/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS7420 - ATLAS Core frequency scaling support
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
#include <mach/regs-clock-exynos7420.h>
#include <mach/regs-pmu.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>
#include <linux/of.h>

#define CPUFREQ_LEVEL_END_CA57	(L23 + 1)
#undef PRINT_DIV_VAL

#define ATLAS_EMA_CON (EXYNOS7420_VA_SYSREG + 0x0138)
#define CPU_EMA_REG1 (EXYNOS7420_VA_SYSREG + 0x2908)

static int max_support_idx_CA57;
static int min_support_idx_CA57 = (CPUFREQ_LEVEL_END_CA57 - 1);

static struct clk *mout_atlas;
static struct clk *mout_atlas_pll;
static struct clk *mout_bus0_pll_atlas;

static unsigned int exynos7420_volt_table_CA57[CPUFREQ_LEVEL_END_CA57];
static unsigned int exynos7420_abb_table_CA57[CPUFREQ_LEVEL_END_CA57];

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


static struct cpufreq_frequency_table exynos7420_freq_table_CA57[] = {
	{L0,  2496 * 1000},
	{L1,  2400 * 1000},
	{L2,  2304 * 1000},
	{L3,  2200 * 1000},
	{L4,  2100 * 1000},
	{L5,  2000 * 1000},
	{L6,  1896 * 1000},
	{L7,  1800 * 1000},
	{L8,  1704 * 1000},
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

static struct apll_freq exynos7420_apll_freq_CA57[] = {
	/*
	 * values:
	 * freq
	 * clock divider for ATLAS1, ATLAS2, ACLK_ATLAS, PCLK_ATLAS, ATCLK, PCLK_DBG_ATLAS,
	 * clock divider for SCLK_ATLAS_PLL, SCLK_HPM_ATLAS, SCLK_CNTCLK
	 * PLL M, P, S values are NOT used, Instead CCF clk_set_rate is used
	 */
	APLL_ATLAS_FREQ(2496, 0, 0, 2, 6, 6, 6, 1, 5, 6, 208, 2, 0),    /* ARM L0: 2.5GHz  */
	APLL_ATLAS_FREQ(2400, 0, 0, 2, 6, 6, 6, 1, 5, 6, 200, 2, 0),    /* ARM L1: 2.4GMHz */
	APLL_ATLAS_FREQ(2304, 0, 0, 2, 6, 6, 6, 1, 5, 6, 192, 2, 0),    /* ARM L2: 2.3GMHz */
	APLL_ATLAS_FREQ(2200, 0, 0, 2, 6, 6, 6, 1, 5, 6, 275, 3, 0),    /* ARM L3: 2.2GHz  */
	APLL_ATLAS_FREQ(2100, 0, 0, 2, 6, 6, 6, 1, 5, 6, 175, 2, 0),    /* ARM L4: 2.1GHz  */
	APLL_ATLAS_FREQ(2000, 0, 0, 2, 6, 6, 6, 1, 5, 6, 250, 3, 0),    /* ARM L5: 2.0GHz  */
	APLL_ATLAS_FREQ(1896, 0, 0, 2, 6, 6, 6, 1, 4, 6, 158, 2, 0),    /* ARM L6: 1.9GHz  */
	APLL_ATLAS_FREQ(1800, 0, 0, 2, 6, 6, 6, 1, 4, 6, 150, 2, 0),    /* ARM L7: 1.8GHz  */
	APLL_ATLAS_FREQ(1704, 0, 0, 2, 6, 6, 6, 1, 4, 6, 142, 2, 0),    /* ARM L8: 1.7GHz  */
	APLL_ATLAS_FREQ(1600, 0, 0, 2, 6, 6, 6, 1, 4, 6, 200, 3, 0),    /* ARM L9: 1.6GHz  */
	APLL_ATLAS_FREQ(1500, 0, 0, 2, 6, 6, 6, 1, 4, 6, 250, 2, 1),    /* ARM L10: 1.5GHz */
	APLL_ATLAS_FREQ(1400, 0, 0, 2, 6, 6, 6, 1, 4, 6, 350, 3, 1),    /* ARM L11: 1.4GHz */
	APLL_ATLAS_FREQ(1300, 0, 0, 2, 6, 6, 6, 1, 4, 6, 325, 3, 1),    /* ARM L12: 1.3GHz */
	APLL_ATLAS_FREQ(1200, 0, 0, 1, 6, 6, 6, 1, 3, 6, 200, 2, 1),    /* ARM L13: 1.2GHz */
	APLL_ATLAS_FREQ(1100, 0, 0, 1, 6, 6, 6, 1, 3, 6, 275, 3, 1),    /* ARM L14: 1.1GHz */
	APLL_ATLAS_FREQ(1000, 0, 0, 1, 6, 6, 6, 1, 3, 6, 250, 3, 1),    /* ARM L15: 1.0GHz */
	APLL_ATLAS_FREQ( 900, 0, 0, 1, 6, 6, 6, 1, 3, 6, 150, 2, 1),    /* ARM L16: 900MHz */
	APLL_ATLAS_FREQ( 800, 0, 0, 1, 5, 5, 5, 1, 3, 5, 200, 3, 1),    /* ARM L17: 800MHz */
	APLL_ATLAS_FREQ( 700, 0, 0, 1, 5, 5, 5, 1, 3, 5, 350, 3, 2),    /* ARM L18: 700MHz */
	APLL_ATLAS_FREQ( 600, 0, 0, 1, 4, 4, 4, 1, 3, 4, 200, 2, 2),    /* ARM L19: 600MHz */
	APLL_ATLAS_FREQ( 500, 0, 0, 1, 3, 3, 3, 1, 2, 3, 250, 3, 2),    /* ARM L20: 500MHz */
	APLL_ATLAS_FREQ( 400, 0, 0, 1, 3, 3, 3, 1, 2, 3, 200, 3, 2),    /* ARM L21: 400MHz */
	APLL_ATLAS_FREQ( 300, 0, 0, 1, 3, 3, 3, 1, 2, 3, 200, 2, 3),    /* ARM L22: 300MHz */
	APLL_ATLAS_FREQ( 200, 0, 0, 1, 3, 3, 3, 1, 1, 3, 200, 3, 3),    /* ARM L23: 200MHz */
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_7420_CA57[CPUFREQ_LEVEL_END_CA57] = {
	1250000,	/* L0  2500 */
	1250000,	/* L1  2400 */
	1250000,	/* L2  2300 */
	1250000,	/* L3  2200 */
	1250000,	/* L4  2100 */
	1200000,	/* L5  2000 */
	1156250,	/* L6  1900 */
	1118750,	/* L7  1800 */
	1081250,	/* L8  1700 */
	1043750,	/* L9  1600 */
	1012500,	/* L10 1500 */
	 981250,	/* L11 1400 */
	 950000,	/* L12 1300 */
	 925000,	/* L13 1200 */
	 900000,	/* L14 1100 */
	 875000,	/* L15 1000 */
	 850000,	/* L16  900 */
	 825000,	/* L17  800 */
	 800000,	/* L18  700 */
	 775000,	/* L19  600 */
	 750000,	/* L20  500 */
	 725000,	/* L21  400 */
	 700000,	/* L22  300 */
	 675000,	/* L23  200 */
};

/* minimum memory throughput in megabytes per second */
static int exynos7420_bus_table_CA57[CPUFREQ_LEVEL_END_CA57] = {
	1552000,		/* 2.5 GHz */
	1552000,		/* 2.4 GHz */
	1552000,		/* 2.3 GHz */
	1552000,		/* 2.2 GHz */
	1552000,		/* 2.1 GHz */
	1456000,		/* 2.0 GHz */
	1264000,		/* 1.9 GHz */
	1026000,		/* 1.8 GHz */
	 828000,		/* 1.7 MHz */
	 828000,		/* 1.6 GHz */
	 828000,		/* 1.5 GHz */
	 828000,		/* 1.4 GHz */
	 828000,		/* 1.3 GHz */
	 828000,		/* 1.2 GHz */
	 632000,		/* 1.1 GHz */
	 543000,		/* 1.0 GHz */
	 543000,		/* 900 MHz */
	 416000,		/* 800 MHz */
	      0,		/* 700 MHz */
	      0,		/* 600 MHz */
	      0,		/* 500 MHz */
	      0,		/* 400 MHz */
	      0,		/* 300 MHz */
	      0,		/* 200 MHz */
};

static int exynos7420_cpufreq_smpl_warn_notifier_call(
					struct notifier_block *notifer,
					unsigned long event, void *v)
{
	unsigned int state;

	state = __raw_readl(EXYNOS7420_ATLAS_SMPL_CTRL0);
	state &= ~0x40;
	__raw_writel(state, EXYNOS7420_ATLAS_SMPL_CTRL0);
	state |= 0x40;
	__raw_writel(state, EXYNOS7420_ATLAS_SMPL_CTRL0);

	pr_info("%s: SMPL_WARN: SMPL_WARN is cleared\n",__func__);

	return NOTIFY_OK;
}

static int exynos7420_check_smpl_CA57(void)
{
	int tmp;

	tmp = __raw_readl(EXYNOS7420_ATLAS_SMPL_CTRL1);

	if (tmp & 0x80) {
		pr_info("%s: SMPL_WARN HAPPENED!\n", __func__);
		return 1;
	}

	return 0;
};

static struct notifier_block exynos7420_cpufreq_smpl_warn_notifier = {
	.notifier_call = exynos7420_cpufreq_smpl_warn_notifier_call,
};

static void exynos7420_set_clkdiv_CA57(unsigned int div_index)
{
	unsigned int tmp, tmp1;

	/* Change Divider - ATLAS0 */
	tmp = exynos7420_apll_freq_CA57[div_index].clk_div_cpu0;

	__raw_writel(tmp, EXYNOS7420_DIV_ATLAS0);

	while (__raw_readl(EXYNOS7420_DIV_STAT_ATLAS0) & 0x4101111)
		cpu_relax();

	/* Change Divider - ATLAS1 */
	tmp1 = exynos7420_apll_freq_CA57[div_index].clk_div_cpu1;

	__raw_writel(tmp1, EXYNOS7420_DIV_ATLAS1);

	while (__raw_readl(EXYNOS7420_DIV_STAT_ATLAS1) & 0x111)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS7420_DIV_ATLAS0);
	tmp1 = __raw_readl(EXYNOS7420_DIV_ATLAS1);

	pr_info("%s: DIV_ATLAS0[0x%08x], DIV_ATLAS1[0x%08x]\n",
					__func__, tmp, tmp1);
#endif
}

static bool exynos7420_pms_change_CA57(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos7420_apll_freq_CA57[old_index].mps >>
				EXYNOS7420_PLL_PDIV_SHIFT);
	unsigned int new_pm = (exynos7420_apll_freq_CA57[new_index].mps >>
				EXYNOS7420_PLL_PDIV_SHIFT);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos7420_set_atlas_pll_CA57(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp;

	if (!exynos7420_pms_change_CA57(old_index, new_index)) {
		/* Change MOUT_ATLAS_PLL frequency
		  When PM value are matching with previous frequency
		  then only S value should be change in this case
		  don't need to change ATLAS_PLL to BUS0_PLL_ATLASO */
		clk_set_rate(mout_atlas_pll, exynos7420_freq_table_CA57[new_index].frequency * 1000);
	} else {
		/* 1. before change to BUS0_PLL, set div for BUS0_PLL output */
		if ((new_index > L17) && (old_index > L17))
			exynos7420_set_clkdiv_CA57(L17); /* pll_safe_idx of CA57 */

		/* 2. MUX_SEL_ATLAS = MOUT_BUS0_PLL_ALTAS, ATLASCLK uses BUS0_PLL_ATLAS for lock time */
		if (clk_set_parent(mout_atlas, mout_bus0_pll_atlas))
			pr_err("Unable to set parent %s of clock %s.\n",
					mout_bus0_pll_atlas->name, mout_atlas->name);
		do {
			cpu_relax();
			tmp = __raw_readl(EXYNOS7420_MUX_STAT_ATLAS2);
			tmp &= EXYNOS7420_MUX_STAT_CORE2_MASK;
			tmp >>= EXYNOS7420_MUX_STAT_CORE2_SHIFT;
		} while (tmp != 0x1);

		/* 3. Change MOUT_ATLAS_PLL Frequency */
		clk_set_rate(mout_atlas_pll, exynos7420_freq_table_CA57[new_index].frequency * 1000);

		/* 4. MUX_SEL_ATLAS = MOUT_ATLAS_PLL */
		if (clk_set_parent(mout_atlas, mout_atlas_pll))
			pr_err("Unable to set parent %s of clock %s.\n",
					mout_atlas_pll->name, mout_atlas->name);
		do {
			cpu_relax();
			tmp = __raw_readl(EXYNOS7420_MUX_STAT_ATLAS2);
			tmp &= EXYNOS7420_MUX_STAT_CORE2_MASK;
			tmp >>= EXYNOS7420_MUX_STAT_CORE2_SHIFT;
		} while (tmp != 0x0);

		/* 5. restore original div value */
		if ((new_index > L17) && (old_index > L17))
			exynos7420_set_clkdiv_CA57(new_index);
	}
}

static void exynos7420_set_frequency_CA57(unsigned int old_index,
					 unsigned int new_index)
{
	if (old_index > new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the system clock divider values */
		exynos7420_set_clkdiv_CA57(new_index);
		/* 2. Change the atlas_pll m,p,s value */
		exynos7420_set_atlas_pll_CA57(new_index, old_index);
	} else if (old_index < new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the atlas_pll m,p,s value */
		exynos7420_set_atlas_pll_CA57(new_index, old_index);
		/* 2. Change the system clock divider values */
		exynos7420_set_clkdiv_CA57(new_index);
	}

	pr_debug("[%s] Atlas NewFreq: %d OldFreq: %d\n", __func__, exynos7420_freq_table_CA57[new_index].frequency,
										exynos7420_freq_table_CA57[old_index].frequency);
}

static void __init set_volt_table_CA57(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA57; i++) {
		asv_volt = get_match_volt(ID_CL1, exynos7420_freq_table_CA57[i].frequency);
		if (!asv_volt)
			exynos7420_volt_table_CA57[i] = asv_voltage_7420_CA57[i];
		else
			exynos7420_volt_table_CA57[i] = asv_volt;

		pr_info("CPUFREQ of CA57  L%d : %d uV\n", i,
				exynos7420_volt_table_CA57[i]);

		exynos7420_abb_table_CA57[i] =
			get_match_abb(ID_CL1, exynos7420_freq_table_CA57[i].frequency);

		pr_info("CPUFREQ of CA57  L%d : ABB %d\n", i,
				exynos7420_abb_table_CA57[i]);
	}

#if defined(CONFIG_CPU_THERMAL) && (defined(CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG) || defined(CONFIG_EXYNOS_MARCH_DYNAMIC_CPU_HOTPLUG))
	switch (exynos_get_table_ver()) {
	case 0 :
	case 1 :
	case 4 :
		max_support_idx_CA57 = L7; break;	/* 1.8GHz */
	case 5 :
		max_support_idx_CA57 = L10; break;	/* 1.5GHz */
	default :
		max_support_idx_CA57 = L4;		/* 2.1GHz */
	}
#else
	max_support_idx_CA57 = L13;	/* 1.2 GHz */
#endif

	min_support_idx_CA57 = L17;	/* 800 MHz */

	pr_info("CPUFREQ of CA57 max_freq : L%d %u khz\n", max_support_idx_CA57,
		exynos7420_freq_table_CA57[max_support_idx_CA57].frequency);
	pr_info("CPUFREQ of CA57 min_freq : L%d %u khz\n", min_support_idx_CA57,
		exynos7420_freq_table_CA57[min_support_idx_CA57].frequency);
}
static bool exynos7420_is_alive_CA57(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS_PMU_ATLAS_L2_STATUS) &
	      __raw_readl(EXYNOS_PMU_ATLAS_NONCPU_STATUS) &
	      POWER_ON_STATUS;

	return tmp ? true : false;
}

static void exynos7420_set_ema_CA57(unsigned int volt)
{
	exynos_set_ema(ID_CL1, volt);
}

int __init exynos_cpufreq_cluster1_init(struct exynos_dvfs_info *info)
{
	unsigned long rate;
	struct device_node *pmic_node;
	int ret, tmp;

	set_volt_table_CA57();

	mout_atlas_pll = clk_get(NULL, "mout_atlas_pll");
	if (IS_ERR(mout_atlas_pll)) {
		pr_err("failed get mout_atlas_pll clk\n");
		goto err_mout_atlas_pll;
	}

	mout_atlas = clk_get(NULL, "mout_atlas");
	if (IS_ERR(mout_atlas)) {
		pr_err("failed get mout_atlas clk\n");
		goto err_mout_atlas;
	}

	if (clk_set_parent(mout_atlas, mout_atlas_pll)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_atlas_pll->name, mout_atlas->name);
		goto err_clk_set_parent_atlas;
	}

	mout_bus0_pll_atlas = clk_get(NULL, "mout_bus0_pll_atlas");
	if (IS_ERR(mout_bus0_pll_atlas)) {
		pr_err("failed get mout_bus0_pll_atlas clk\n");
		goto err_mout_bus0_pll_atlas;
	}

	if (clk_prepare_enable(mout_atlas_pll) || clk_prepare_enable(mout_atlas)) {
		pr_err("Unable to enable Atlas clocks \n");
		goto err_clk_prepare_enable;
	}

	rate = clk_get_rate(mout_bus0_pll_atlas) / 1000;

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L17;
	info->max_support_idx = max_support_idx_CA57;
	info->min_support_idx = min_support_idx_CA57;

	/* booting frequency is 1.7GHz */
	info->boot_cpu_min_qos = exynos7420_freq_table_CA57[L8].frequency;
	info->boot_cpu_max_qos = exynos7420_freq_table_CA57[L8].frequency;
	info->bus_table = exynos7420_bus_table_CA57;
	info->cpu_clk = mout_atlas_pll;

	/* reboot limit frequency is 800MHz */
	info->reboot_limit_freq = exynos7420_freq_table_CA57[L17].frequency;

	info->volt_table = exynos7420_volt_table_CA57;
	info->abb_table = NULL; //exynos7420_abb_table_CA57;
	info->freq_table = exynos7420_freq_table_CA57;
	info->set_freq = exynos7420_set_frequency_CA57;
	info->need_apll_change = exynos7420_pms_change_CA57;
	info->is_alive = exynos7420_is_alive_CA57;
	info->set_ema = exynos7420_set_ema_CA57;

	pmic_node = of_find_compatible_node(NULL, NULL, "samsung,s2mps15-pmic");

	if (!pmic_node) {
		pr_err("%s: faile to get pmic dt_node\n", __func__);
	} else {
		ret = of_property_read_u32(pmic_node, "smpl_warn_en", &en_smpl_warn);
		if (ret)
			pr_err("%s: faile to get Property of smpl_warn_en\n", __func__);
	}

	if (en_smpl_warn) {
		info->check_smpl = exynos7420_check_smpl_CA57;

		/* ATLAS_RATIO_SMPL */
		tmp = __raw_readl(EXYNOS7420_ATLAS_SMPL_CTRL0);
		tmp &= 0x7F;
		tmp |= 0x44;
		__raw_writel(tmp, EXYNOS7420_ATLAS_SMPL_CTRL0);
		pr_info("%s SMPL_WARN ENABLE (DIV:%d) ", __func__, tmp&0x3F);

		exynos_cpufreq_smpl_warn_register_notifier(&exynos7420_cpufreq_smpl_warn_notifier);
	}

	return 0;

err_clk_prepare_enable:
err_mout_bus0_pll_atlas:
err_clk_set_parent_atlas:
	clk_put(mout_atlas);
err_mout_atlas:
	clk_put(mout_atlas_pll);
err_mout_atlas_pll:

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
