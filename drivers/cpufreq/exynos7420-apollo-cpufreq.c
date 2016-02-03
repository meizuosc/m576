/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS7420 - APOLLO Core frequency scaling support
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

#define CPUFREQ_LEVEL_END_CA53	(L18 + 1)

#undef PRINT_DIV_VAL

#define APOLLO_EMA_CON (EXYNOS7420_VA_SYSREG + 0x0038)

static int max_support_idx_CA53;
static int min_support_idx_CA53 = (CPUFREQ_LEVEL_END_CA53 - 1);

static struct clk *mout_apollo;
static struct clk *mout_apollo_pll;
static struct clk *mout_bus0_pll_apollo;

static unsigned int exynos7420_volt_table_CA53[CPUFREQ_LEVEL_END_CA53];
static unsigned int exynos7420_abb_table_CA53[CPUFREQ_LEVEL_END_CA53];

static struct cpufreq_frequency_table exynos7420_freq_table_CA53[] = {
	{L0,  2000 * 1000},
	{L1,  1900 * 1000},
	{L2,  1800 * 1000},
	{L3,  1704 * 1000},
	{L4,  1600 * 1000},
	{L5,  1500 * 1000},
	{L6,  1400 * 1000},
	{L7,  1296 * 1000},
	{L8,  1200 * 1000},
	{L9,  1104 * 1000},
	{L10, 1000 * 1000},
	{L11,  900 * 1000},
	{L12,  800 * 1000},
	{L13,  700 * 1000},
	{L14,  600 * 1000},
	{L15,  500 * 1000},
	{L16,  400 * 1000},
	{L17,  300 * 1000},
	{L18,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct apll_freq exynos7420_apll_freq_CA53[] = {
	/*
	 * values:
	 * freq
	 * clock divider for APOLLO1, APOLLO2, ACLK_APOLLO, PCLK_APOLLO, ATCLK, PCLK_DBG, SCLK_CNTCLK, RESERVED
	 * clock divider for APOLLO_PLL, SCLK_HPM_APOLLO, RESERVED
	 * PLL M, P, S values are NOT used, Instead CCF clk_set_rate is used
	 */
	APLL_FREQ(2000, 0, 0, 2, 5, 5, 5, 5, 0, 1, 4, 0,   0, 0, 0),  /* ARM L0: 2.0GHz   */
	APLL_FREQ(1900, 0, 0, 2, 5, 5, 5, 5, 0, 1, 4, 0,   0, 0, 0),  /* ARM L1: 1.9GMHz  */
	APLL_FREQ(1800, 0, 0, 2, 5, 5, 5, 5, 0, 1, 4, 0, 150, 2, 0),  /* ARM L2: 1.8GMHz  */
	APLL_FREQ(1704, 0, 0, 2, 5, 5, 5, 5, 0, 1, 4, 0, 142, 2, 0),  /* ARM L3: 1.7GHz   */
	APLL_FREQ(1600, 0, 0, 2, 5, 5, 5, 5, 0, 1, 3, 0, 200, 3, 0),  /* ARM L4: 1.6GHz   */
	APLL_FREQ(1500, 0, 0, 2, 5, 5, 5, 5, 0, 1, 3, 0, 250, 4, 0),  /* ARM L5: 1.5GMHz  */
	APLL_FREQ(1400, 0, 0, 2, 5, 5, 5, 5, 0, 1, 3, 0, 175, 3, 0),  /* ARM L6: 1.4GMHz  */
	APLL_FREQ(1296, 0, 0, 2, 5, 5, 5, 5, 0, 1, 3, 0, 108, 2, 0),  /* ARM L7: 1.3GHz   */
	APLL_FREQ(1200, 0, 0, 2, 5, 5, 5, 5, 0, 1, 3, 0, 100, 2, 0),  /* ARM L8: 1.2GHz   */
	APLL_FREQ(1104, 0, 0, 2, 5, 5, 5, 5, 0, 1, 3, 0,  92, 2, 0),  /* ARM L9: 1.1GHz   */
	APLL_FREQ(1000, 0, 0, 2, 5, 5, 5, 5, 0, 1, 3, 0, 125, 3, 0),  /* ARM L10: 1000MHz */
	APLL_FREQ( 900, 0, 0, 1, 5, 5, 5, 5, 0, 1, 2, 0, 150, 2, 1),  /* ARM L11: 900MHz  */
	APLL_FREQ( 800, 0, 0, 1, 5, 5, 5, 5, 0, 1, 2, 0, 200, 3, 1),  /* ARM L12: 800MHz  */
	APLL_FREQ( 700, 0, 0, 1, 4, 4, 4, 4, 0, 1, 2, 0, 175, 3, 1),  /* ARM L13: 700MHz  */
	APLL_FREQ( 600, 0, 0, 1, 4, 4, 4, 4, 0, 1, 2, 0, 100, 2, 1),  /* ARM L14: 600MHz  */
	APLL_FREQ( 500, 0, 0, 1, 4, 4, 4, 4, 0, 1, 2, 0, 125, 3, 1),  /* ARM L15: 500MHz  */
	APLL_FREQ( 400, 0, 0, 1, 3, 3, 3, 3, 0, 1, 2, 0, 200, 3, 2),  /* ARM L16: 400MHz  */
	APLL_FREQ( 300, 0, 0, 1, 3, 3, 3, 3, 0, 1, 2, 0, 100, 2, 2),  /* ARM L17: 300MHz  */
	APLL_FREQ( 200, 0, 0, 1, 3, 3, 3, 3, 0, 1, 1, 0, 200, 3, 3),  /* ARM L18: 200MHz  */
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_7420_CA53[CPUFREQ_LEVEL_END_CA53] = {
	1168750,	/* L0  2000 */
	1168750,	/* L1  1900 */
	1168750,	/* L2  1800 */
	1168750,	/* L3  1700 */
	1168750,	/* L4  1600 */
	1118750,	/* L5  1500 */
	1068750,	/* L6  1400 */
	1018750,	/* L7  1300 */
	 981250,	/* L8  1200 */
	 943750,	/* L9  1100 */
	 900000,	/* L10 1000 */
	 862500,	/* L11  900 */
	 825000,	/* L12  800 */
	 787500,	/* L13  700 */
	 750000,	/* L14  600 */
	 712500,	/* L15  500 */
	 675000,	/* L16  400 */
	 650000,	/* L17  300 */
	 625000,	/* L18  200 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos7420_bus_table_CA53[CPUFREQ_LEVEL_END_CA53] = {
	1026000,		/* 2.0 GHz */
	1026000,		/* 1.9 GHz */
	1026000,		/* 1.8 GHz */
	1026000,		/* 1.7 GHz */
	1026000,		/* 1.6 GHz */
	1026000,		/* 1.5 GHz */
	 828000,		/* 1.4 GHz */
	 828000,		/* 1.3 GHz */
	 828000,		/* 1.2 GHz */
	 828000,		/* 1.1 GHz */
	 828000,		/* 1.0 GHz */
	 828000,		/* 900 MHz */
	 632000,		/* 800 MHz */
	 543000,		/* 700 MHz */
	 416000,		/* 600 MHz */
	 416000,		/* 500 MHz */
	 348000,		/* 400 MHz */
	      0,		/* 300 MHz */
	      0,		/* 200 MHz */
};

static void exynos7420_set_clkdiv_CA53(unsigned int div_index)
{
	unsigned int tmp, tmp1;

	/* Change Divider - APOLLO0 */
	tmp = exynos7420_apll_freq_CA53[div_index].clk_div_cpu0;

	__raw_writel(tmp, EXYNOS7420_DIV_APOLLO0);

	while (__raw_readl(EXYNOS7420_DIV_STAT_APOLLO0) & 0x1111111)
		cpu_relax();

	/* Change Divider - APOLLO1 */
	tmp1 = exynos7420_apll_freq_CA53[div_index].clk_div_cpu1;

	__raw_writel(tmp1, EXYNOS7420_DIV_APOLLO1);

	while (__raw_readl(EXYNOS7420_DIV_STAT_APOLLO1) & 0x11)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS7420_DIV_APOLLO0);
	tmp1 = __raw_readl(EXYNOS7420_DIV_APOLLO1);
	pr_info("%s: DIV_APOLLO0[0x%08x], DIV_APOLLO1[0x%08x]\n",
					__func__, tmp, tmp1);
#endif
}

static bool exynos7420_pms_change_CA53(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos7420_apll_freq_CA53[old_index].mps >>
				EXYNOS7420_PLL_PDIV_SHIFT);
	unsigned int new_pm = (exynos7420_apll_freq_CA53[new_index].mps >>
				EXYNOS7420_PLL_PDIV_SHIFT);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos7420_set_apollo_pll_CA53(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp;

	if (!exynos7420_pms_change_CA53(old_index, new_index)) {
		/* Change MOUT_APOLLO_PLL frequency
		  When PM value are matching with previous frequency
		  then only S value should be change, in this case
		  don't need to change APOLLO_PLL to BUS0_PLL_APOLLO */
		clk_set_rate(mout_apollo_pll, exynos7420_freq_table_CA53[new_index].frequency * 1000);
	} else {
		/* 1. before change to BUS_PLL, set div for BUS_PLL output */
		if ((new_index > L12) && (old_index > L12))
			exynos7420_set_clkdiv_CA53(L12); /* pll_safe_idx of CA53 */

		/* 2. MUX_SEL_APOLLO2 = MOUT_BUS0_PLL_APOLLO, APOLLOCLK uses BUS0_PLL_APOLLO for lock time */
		if (clk_set_parent(mout_apollo, mout_bus0_pll_apollo))
			pr_err("Unable to set parent %s of clock %s.\n",
					mout_bus0_pll_apollo->name, mout_apollo->name);
		do {
			cpu_relax();
			tmp = __raw_readl(EXYNOS7420_MUX_STAT_APOLLO2);
			tmp &= EXYNOS7420_MUX_STAT_CORE2_MASK;
			tmp >>= EXYNOS7420_MUX_STAT_CORE2_SHIFT;
		} while (tmp != 0x1);

		/* 3.  Change MOUT_APOLLO_PLL frequency */
		clk_set_rate(mout_apollo_pll, exynos7420_freq_table_CA53[new_index].frequency * 1000);

		/* 4. CLKMUX_SEL_APOLLO2 = MOUT_APOLLO_PLL */
		if (clk_set_parent(mout_apollo, mout_apollo_pll))
			pr_err("Unable to set parent %s of clock %s.\n",
					mout_apollo_pll->name, mout_apollo->name);
		do {
			cpu_relax();
			tmp = __raw_readl(EXYNOS7420_MUX_STAT_APOLLO2);
			tmp &= EXYNOS7420_MUX_STAT_CORE2_MASK;
			tmp >>= EXYNOS7420_MUX_STAT_CORE2_SHIFT;
		} while (tmp != 0x0);

		/* 5. restore original div value */
		if ((new_index > L12) && (old_index > L12))
			exynos7420_set_clkdiv_CA53(new_index);
	}
}

static void exynos7420_set_frequency_CA53(unsigned int old_index,
					 unsigned int new_index)
{
	if (old_index > new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the system clock divider values */
		exynos7420_set_clkdiv_CA53(new_index);
		/* 2. Change the apollo_pll m,p,s value */
		exynos7420_set_apollo_pll_CA53(new_index, old_index);
	} else if (old_index < new_index) {
		/* Clock Configuration Procedure */
		/* 1. Change the apollo_pll m,p,s value */
		exynos7420_set_apollo_pll_CA53(new_index, old_index);
		/* 2. Change the system clock divider values */
		exynos7420_set_clkdiv_CA53(new_index);
	}

	pr_debug("[%s] Apollo NewFreq: %d OldFreq: %d\n", __func__, exynos7420_freq_table_CA53[new_index].frequency,
										exynos7420_freq_table_CA53[old_index].frequency);
}

static void __init set_volt_table_CA53(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA53; i++) {
		asv_volt = get_match_volt(ID_CL0, exynos7420_freq_table_CA53[i].frequency);
		if (!asv_volt)
			exynos7420_volt_table_CA53[i] = asv_voltage_7420_CA53[i];
		else
			exynos7420_volt_table_CA53[i] = asv_volt;

		pr_info("CPUFREQ of CA53  L%d : %d uV\n", i,
				exynos7420_volt_table_CA53[i]);

		exynos7420_abb_table_CA53[i] =
			get_match_abb(ID_CL0, exynos7420_freq_table_CA53[i].frequency);

		pr_info("CPUFREQ of CA53  L%d : ABB %d\n", i,
				exynos7420_abb_table_CA53[i]);
	}
	switch (exynos_get_table_ver()) {
	case 0 :
	case 1 :
	case 4 :
		max_support_idx_CA53 = L6; break;	/* 1.4GHz */
	case 5 :
		max_support_idx_CA53 = L8; break;	/* 1.2GHz */
	default :
		max_support_idx_CA53 = L5;	/* 1.5GHz */
	}

	min_support_idx_CA53 = L16;	/* 400MHz */
	pr_info("CPUFREQ of CA53 max_freq : L%d %u khz\n", max_support_idx_CA53,
		exynos7420_freq_table_CA53[max_support_idx_CA53].frequency);
	pr_info("CPUFREQ of CA53 min_freq : L%d %u khz\n", min_support_idx_CA53,
		exynos7420_freq_table_CA53[min_support_idx_CA53].frequency);
}

static bool exynos7420_is_alive_CA53(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS7420_APOLLO_PLL_CON1);
	tmp &= EXYNOS7420_PLL_BYPASS_MASK;
	tmp >>= EXYNOS7420_PLL_BYPASS_SHIFT;

	return !tmp ? true : false;
}

static void exynos7420_set_ema_CA53(unsigned int volt)
{
	exynos_set_ema(ID_CL0, volt);
}

int __init exynos_cpufreq_cluster0_init(struct exynos_dvfs_info *info)
{
	unsigned long rate;

	set_volt_table_CA53();

	mout_apollo_pll = clk_get(NULL, "mout_apollo_pll");
	if (IS_ERR(mout_apollo_pll)) {
		pr_err("failed get mout_apollo_pll clk\n");
		goto err_mout_apollo_pll;
	}

	mout_apollo = clk_get(NULL, "mout_apollo");
	if (IS_ERR(mout_apollo)) {
		pr_err("failed get mout_apollo clk\n");
		goto err_mout_apollo;
	}

	if (clk_set_parent(mout_apollo, mout_apollo_pll)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_apollo_pll->name, mout_apollo->name);
		goto err_clk_set_parent_apollo;
	}

	mout_bus0_pll_apollo = clk_get(NULL, "mout_bus0_pll_apollo");
	if (IS_ERR(mout_bus0_pll_apollo)) {
		pr_err("failed get mout_bus0_pll_apollo clk\n");
		goto err_mout_bus0_pll_apollo;
	}

	if (clk_prepare_enable(mout_apollo_pll) || clk_prepare_enable(mout_apollo)) {
		pr_err("Unable to enable Apollo clocks \n");
		goto err_clk_prepare_enable;
	}

	rate = clk_get_rate(mout_bus0_pll_apollo) / 1000;

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L12;
	info->max_support_idx = max_support_idx_CA53;
	info->min_support_idx = min_support_idx_CA53;
	info->boost_freq = exynos7420_freq_table_CA53[L10].frequency;
	/* booting frequency is 1.4GHz */
	info->boot_cpu_min_qos = exynos7420_freq_table_CA53[L6].frequency;
	info->boot_cpu_max_qos = exynos7420_freq_table_CA53[L6].frequency;
	info->bus_table = exynos7420_bus_table_CA53;
	info->cpu_clk = mout_apollo_pll;

	info->volt_table = exynos7420_volt_table_CA53;
	info->abb_table = NULL; //exynos7420_abb_table_CA53;
	info->freq_table = exynos7420_freq_table_CA53;
	info->set_freq = exynos7420_set_frequency_CA53;
	info->need_apll_change = exynos7420_pms_change_CA53;
	info->is_alive = exynos7420_is_alive_CA53;
	info->set_ema = exynos7420_set_ema_CA53;

	return 0;

err_clk_prepare_enable:
err_mout_bus0_pll_apollo:
err_clk_set_parent_apollo:
	clk_put(mout_apollo);
err_mout_apollo:
	clk_put(mout_apollo_pll);
err_mout_apollo_pll:

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
