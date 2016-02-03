/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5433 - APOLLO Core frequency scaling support
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

#define CPUFREQ_LEVEL_END_CA53	(L18 + 1)
#define L2_LOCAL_PWR_EN		0x7

#undef PRINT_DIV_VAL
#undef ENABLE_CLKOUT

static int max_support_idx_CA53;
static int min_support_idx_CA53 = (CPUFREQ_LEVEL_END_CA53 - 1);

static struct clk *mout_apollo;
static struct clk *mout_apollo_pll;
static struct clk *mout_bus_pll_div2;
static struct clk *mout_bus_pll_user;
static struct clk *fout_apollo_pll;

static unsigned int exynos5433_volt_table_CA53[CPUFREQ_LEVEL_END_CA53];
static unsigned int exynos5433_abb_table_CA53[CPUFREQ_LEVEL_END_CA53];

static struct cpufreq_frequency_table exynos5433_freq_table_CA53[] = {
	{L0,  2000 * 1000},
	{L1,  1900 * 1000},
	{L2,  1800 * 1000},
	{L3,  1700 * 1000},
	{L4,  1600 * 1000},
	{L5,  1500 * 1000},
	{L6,  1400 * 1000},
	{L7,  1300 * 1000},
	{L8,  1200 * 1000},
	{L9,  1100 * 1000},
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

static struct apll_freq exynos5433_apll_freq_CA53[] = {
	/*
	 * values:
	 * freq
	 * clock divider for APOLLO1, APOLLO2 ACLK_APOLLO, PCLK_APOLLO, ATCLK, PCLK_DBG, SCLK_CNTCLK, RESERVED
	 * clock divider for APOLLO_PLL, SCLK_HPM_APOLLO, RESERVED
	 * PLL M, P, S
	 */
	APLL_FREQ(2000, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 500, 6, 0),  /* ARM L0: 2.0GHz   */
	APLL_FREQ(1900, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 475, 6, 0),  /* ARM L1: 1.9GMHz  */
	APLL_FREQ(1800, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 375, 5, 0),  /* ARM L2: 1.8GMHz  */
	APLL_FREQ(1700, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 425, 6, 0),  /* ARM L3: 1.7GHz   */
	APLL_FREQ(1600, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 400, 6, 0),  /* ARM L4: 1.6GHz   */
	APLL_FREQ(1500, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 250, 4, 0),  /* ARM L5: 1.5GMHz  */
	APLL_FREQ(1400, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 350, 6, 0),  /* ARM L6: 1.4GMHz  */
	APLL_FREQ(1300, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 325, 6, 0),  /* ARM L7: 1.3GHz   */
	APLL_FREQ(1200, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 500, 5, 1),  /* ARM L8: 1.2GHz   */
	APLL_FREQ(1100, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 550, 6, 1),  /* ARM L9: 1.1GHz   */
	APLL_FREQ(1000, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 500, 6, 1),  /* ARM L10: 1000MHz */
	APLL_FREQ( 900, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 375, 5, 1),  /* ARM L11: 900MHz  */
	APLL_FREQ( 800, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 400, 6, 1),  /* ARM L12: 800MHz  */
	APLL_FREQ( 700, 0, 0, 2, 7, 7, 7, 3, 0, 1, 7, 0, 350, 6, 1),  /* ARM L13: 700MHz  */
	APLL_FREQ( 600, 0, 0, 1, 7, 7, 7, 3, 0, 1, 7, 0, 500, 5, 2),  /* ARM L14: 600MHz  */
	APLL_FREQ( 500, 0, 0, 1, 7, 7, 7, 3, 0, 1, 7, 0, 500, 6, 2),  /* ARM L15: 500MHz  */
	APLL_FREQ( 400, 0, 0, 1, 7, 7, 7, 3, 0, 1, 7, 0, 400, 6, 2),  /* ARM L16: 400MHz  */
	APLL_FREQ( 300, 0, 0, 1, 7, 7, 7, 3, 0, 1, 7, 0, 500, 5, 3),  /* ARM L17: 300MHz  */
	APLL_FREQ( 200, 0, 0, 1, 7, 7, 7, 3, 0, 1, 7, 0, 400, 6, 3),  /* ARM L18: 200MHz  */
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5433_CA53[CPUFREQ_LEVEL_END_CA53] = {
	1375000,	/* L0  2000 */
	1375000,	/* L1  1900 */
	1375000,	/* L2  1800 */
	1325000,	/* L3  1700 */
	1275000,	/* L4  1600 */
	1250000,	/* L5  1500 */
	1200000,	/* L6  1400 */
	1150000,	/* L7  1300 */
	1112500,	/* L8  1200 */
	1075000,	/* L9  1100 */
	1050000,	/* L10 1000 */
	1025000,	/* L11  900 */
	1000000,	/* L12  800 */
	 975000,	/* L13  700 */
	 950000,	/* L14  600 */
	 925000,	/* L15  500 */
	 900000,	/* L16  400 */
	 875000,	/* L17  300 */
	 850000,	/* L18  200 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos5433_bus_table_CA53[CPUFREQ_LEVEL_END_CA53] = {
	667000,		/* 2.0 GHz */
	667000,		/* 1.9 GHz */
	667000,		/* 1.8 GHz */
	667000,		/* 1.7 GHz */
	667000,		/* 1.6 GHz */
	667000,		/* 1.5 GHz */
	667000,		/* 1.4 GHz */
	667000,		/* 1.3 GHz */
	667000,		/* 1.2 GHz */
	667000,		/* 1.1 GHz */
	543000,		/* 1.0 GHz */
	413000,		/* 900 MHz */
	413000,		/* 800 MHz */
	413000,		/* 700 MHz */
	413000,		/* 600 MHz */
	413000,		/* 500 MHz */
	0,		/* 400 MHz */
	0,		/* 300 MHz */
	0,		/* 200 MHz */
};

static void exynos5433_set_clkdiv_CA53(unsigned int div_index)
{
	unsigned int tmp, tmp1;

	/* Change Divider - APOLLO0 */
	tmp = exynos5433_apll_freq_CA53[div_index].clk_div_cpu0;

	__raw_writel(tmp, EXYNOS5430_DIV_KFC0);

	while (__raw_readl(EXYNOS5430_DIV_STAT_KFC0) & 0x1111111)
		cpu_relax();

	/* Change Divider - APOLLO1 */
	tmp1 = exynos5433_apll_freq_CA53[div_index].clk_div_cpu1;

	__raw_writel(tmp1, EXYNOS5430_DIV_KFC1);

	while (__raw_readl(EXYNOS5430_DIV_STAT_KFC1) & 0x11)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5430_DIV_KFC0);
	tmp1 = __raw_readl(EXYNOS5430_DIV_KFC1);
	pr_info("%s: DIV_APOLLO0[0x%08x], DIV_APOLLO1[0x%08x]\n",
					__func__, tmp, tmp1);
#endif
}

static void exynos5433_set_apollo_pll_CA53(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

	/* 1. before change to BUS_PLL, set div for BUS_PLL output */
	if ((new_index > L12) && (old_index > L12))
		exynos5433_set_clkdiv_CA53(L12); /* pll_safe_idx of CA53 */

	/* 2. CLKMUX_SEL_APOLLO2 = MOUT_BUS_PLL_USER, APOLLOCLK uses BUS_PLL_USER for lock time */
	if (clk_set_parent(mout_apollo, mout_bus_pll_user))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bus_pll_user->name, mout_apollo->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_KFC2);
		tmp &= EXYNOS5430_SRC_STAT_KFC2_KFC_MASK;
		tmp >>= EXYNOS5430_SRC_STAT_KFC2_KFC_SHIFT;
	} while (tmp != 0x2);

	/* 3. Set APOLLO_PLL Lock time */
	pdiv = ((exynos5433_apll_freq_CA53[new_index].mps &
				 EXYNOS5430_PLL_PDIV_MASK) >> EXYNOS5430_PLL_PDIV_SHIFT);

	__raw_writel((pdiv * 150), EXYNOS5430_KFC_PLL_LOCK);

	/* 4. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
	tmp &= ~(EXYNOS5430_PLL_MDIV_MASK |
		EXYNOS5430_PLL_PDIV_MASK |
		EXYNOS5430_PLL_SDIV_MASK);
	tmp |= exynos5433_apll_freq_CA53[new_index].mps;
	__raw_writel(tmp, EXYNOS5430_KFC_PLL_CON0);

	/* 5. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5430_KFC_PLL_CON0_LOCKED_SHIFT)));

	/* 6. CLKMUX_SEL_APOLLO2 = MOUT_APOLLO_PLL */
	if (clk_set_parent(mout_apollo, mout_apollo_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_apollo_pll->name, mout_apollo->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_KFC2);
		tmp &= EXYNOS5430_SRC_STAT_KFC2_KFC_MASK;
		tmp >>= EXYNOS5430_SRC_STAT_KFC2_KFC_SHIFT;
	} while (tmp != 0x1);

	/* 7. restore original div value */
	if ((new_index > L12) && (old_index > L12))
		exynos5433_set_clkdiv_CA53(new_index);
}

static bool exynos5433_pms_change_CA53(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos5433_apll_freq_CA53[old_index].mps >>
				EXYNOS5430_PLL_PDIV_SHIFT);
	unsigned int new_pm = (exynos5433_apll_freq_CA53[new_index].mps >>
				EXYNOS5430_PLL_PDIV_SHIFT);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5433_set_frequency_CA53(unsigned int old_index,
					 unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5433_pms_change_CA53(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5433_set_clkdiv_CA53(new_index);
			/* 2. Change just s value in apollo_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5433_apll_freq_CA53[new_index].mps & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_KFC_PLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5433_set_clkdiv_CA53(new_index);
			/* 2. Change the apollo_pll m,p,s value */
			exynos5433_set_apollo_pll_CA53(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5433_pms_change_CA53(old_index, new_index)) {
			/* 1. Change just s value in apollo_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5433_apll_freq_CA53[new_index].mps & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_KFC_PLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5433_set_clkdiv_CA53(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the apollo_pll m,p,s value */
			exynos5433_set_apollo_pll_CA53(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5433_set_clkdiv_CA53(new_index);
		}
	}

	clk_set_rate(fout_apollo_pll, exynos5433_freq_table_CA53[new_index].frequency * 1000);
	pr_debug("[%s] APOLLO NewFreq: %d OldFreq: %d\n", __func__, exynos5433_freq_table_CA53[new_index].frequency,
										exynos5433_freq_table_CA53[old_index].frequency);
}

static void __init set_volt_table_CA53(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA53; i++) {
		asv_volt = get_match_volt(ID_CL0, exynos5433_freq_table_CA53[i].frequency);
		if (!asv_volt)
			exynos5433_volt_table_CA53[i] = asv_voltage_5433_CA53[i];
		else
			exynos5433_volt_table_CA53[i] = asv_volt;

		pr_info("CPUFREQ of CA53  L%d : %d uV\n", i,
				exynos5433_volt_table_CA53[i]);

		exynos5433_abb_table_CA53[i] =
			get_match_abb(ID_CL0, exynos5433_freq_table_CA53[i].frequency);

		pr_info("CPUFREQ of CA53  L%d : ABB %d\n", i,
				exynos5433_abb_table_CA53[i]);
	}

	max_support_idx_CA53 = L7;	/* 1.3GHz */
	min_support_idx_CA53 = L16;	/* 400MHz */
	pr_info("CPUFREQ of CA53 max_freq : L%d %u khz\n", max_support_idx_CA53,
		exynos5433_freq_table_CA53[max_support_idx_CA53].frequency);
	pr_info("CPUFREQ of CA53 min_freq : L%d %u khz\n", min_support_idx_CA53,
		exynos5433_freq_table_CA53[min_support_idx_CA53].frequency);
}

static bool exynos5433_is_alive_CA53(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON1);
	tmp &= EXYNOS5430_PLL_BYPASS_MASK;
	tmp >>= EXYNOS5430_PLL_BYPASS_SHIFT;

	return !tmp ? true : false;
}

int __init exynos_cpufreq_cluster0_init(struct exynos_dvfs_info *info)
{
	unsigned long rate;

	set_volt_table_CA53();

	mout_apollo = clk_get(NULL, "mout_kfc");
	if (IS_ERR(mout_apollo)) {
		pr_err("failed get mout_apollo clk\n");
		return -EINVAL;
	}

	mout_apollo_pll = clk_get(NULL, "mout_kfc_pll");
	if (IS_ERR(mout_apollo_pll)) {
		pr_err("failed get mout_apollo_pll clk\n");
		goto err_mout_apollo_pll;
	}

	mout_bus_pll_div2 = clk_get(NULL, "mout_bus_pll_div2");
	if (IS_ERR(mout_bus_pll_div2)) {
		pr_err("failed get mout_bus_pll_div2 clk\n");
		goto err_sclk_bus_pll;
	}

	mout_bus_pll_user = clk_get(NULL, "mout_bus_pll_kfc_user");
	if (IS_ERR(mout_bus_pll_user)) {
		pr_err("failed get mout_bus_pll_user clk\n");
		goto err_mout_bus_pll_user;
	}

	if (clk_set_parent(mout_bus_pll_user, mout_bus_pll_div2)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bus_pll_div2->name, mout_bus_pll_user->name);
		goto err_clk_set_parent;
	}

	rate = clk_get_rate(mout_bus_pll_user) / 1000;

	fout_apollo_pll = clk_get(NULL, "fout_kfc_pll");
	if (IS_ERR(fout_apollo_pll)) {
		pr_err("failed get fout_apollo_pll clk\n");
		goto err_fout_apollo_pll;
	}

	clk_put(mout_bus_pll_div2);

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L12;
	info->max_support_idx = max_support_idx_CA53;
	info->min_support_idx = min_support_idx_CA53;
	info->boost_freq = exynos5433_freq_table_CA53[L10].frequency;
	/* booting frequency is 1.3GHz */
	info->boot_cpu_min_qos = exynos5433_freq_table_CA53[L7].frequency;
	info->boot_cpu_max_qos = exynos5433_freq_table_CA53[L7].frequency;
	info->bus_table = exynos5433_bus_table_CA53;
	info->cpu_clk = fout_apollo_pll;

	info->volt_table = exynos5433_volt_table_CA53;
	info->abb_table = exynos5433_abb_table_CA53;
	info->freq_table = exynos5433_freq_table_CA53;
	info->set_freq = exynos5433_set_frequency_CA53;
	info->need_apll_change = exynos5433_pms_change_CA53;
	info->is_alive = exynos5433_is_alive_CA53;

#ifdef ENABLE_CLKOUT
	/* dividing APOLLO_CLK to 1/16 */
	tmp = __raw_readl(EXYNOS5430_CLKOUT_CMU_KFC);
	tmp &= ~0xfff;
	tmp |= 0xf01;
	__raw_writel(tmp, EXYNOS5430_CLKOUT_CMU_KFC);
#endif

	return 0;

err_fout_apollo_pll:
err_clk_set_parent:
	clk_put(mout_bus_pll_user);
err_mout_bus_pll_user:
	clk_put(mout_bus_pll_div2);
err_sclk_bus_pll:
	clk_put(mout_apollo_pll);
err_mout_apollo_pll:
	clk_put(mout_apollo);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
