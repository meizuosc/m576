/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Taikyung yu(taikyung.yu@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/devfreq.h>
#include <mach/tmu.h>
#include <mach/asv-exynos.h>
#include <mach/regs-clock-exynos5433.h>
#include <mach/asv-exynos_cal.h>

#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_EXYNOS_THERMAL
#include <mach/tmu.h>
#endif
#include "devfreq_exynos.h"
#include "governor.h"
#include "dram_timing_parameter.h"

/* ========== 0. DISP related function */

enum devfreq_disp_idx {
	DISP_LV0,
	DISP_LV1,
	DISP_LV2,
	DISP_LV3,
	DISP_LV_COUNT,
};

enum devfreq_disp_clk {
	DOUT_ACLK_DISP_333,
	DOUT_SCLK_DSD,
	DISP_CLK_COUNT,
};

struct devfreq_clk_list devfreq_disp_clk[DISP_CLK_COUNT] = {
	{"dout_aclk_disp_333",},
	{"dout_sclk_dsd",},
};

struct devfreq_opp_table devfreq_disp_opp_list[] = {
	{DISP_LV0,	334000,	0},
	{DISP_LV1,	222000,	0},
	{DISP_LV2,	167000,	0},
	{DISP_LV3,	134000,	0},
};

struct devfreq_clk_info aclk_disp_333[] = {
	{DISP_LV0,	334000000,	0,	NULL},
	{DISP_LV1,	222000000,	0,	NULL},
	{DISP_LV2,	167000000,	0,	NULL},
	{DISP_LV3,	134000000,	0,	NULL},
};

struct devfreq_clk_info sclk_dsd[] = {
	{DISP_LV0,	334000000,	0,	NULL},
	{DISP_LV1,	334000000,	0,	NULL},
	{DISP_LV2,	334000000,	0,	NULL},
	{DISP_LV3,	334000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_disp_info_list[] = {
	aclk_disp_333,
	sclk_dsd,
};

enum devfreq_disp_clk devfreq_clk_disp_info_idx[] = {
	DOUT_ACLK_DISP_333,
	DOUT_SCLK_DSD,
};

static int g_mif_level;
int exynos5_devfreq_get_mif_level(void)
{
	return g_mif_level;
}

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_disp_pm_domain[] = {
	{"pd-disp",},
	{"pd-disp",},
};

static int exynos5_devfreq_disp_init_pm_domain(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	int i;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;

		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd = platform_get_drvdata(pdev);

		for (i = 0; i < ARRAY_SIZE(devfreq_disp_pm_domain); ++i) {
			if (devfreq_disp_pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(devfreq_disp_pm_domain[i].pm_domain_name, pd->genpd.name))
				devfreq_disp_pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}
#endif

static int exynos5_devfreq_disp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_clk); ++i) {
		devfreq_disp_clk[i].clk = clk_get(NULL, devfreq_disp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_disp_clk[i].clk)) {
			pr_err("DEVFREQ(DISP) : %s can't get clock\n", devfreq_disp_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("DISP clk name: %s, rate: %lu\n", devfreq_disp_clk[i].clk_name, clk_get_rate(devfreq_disp_clk[i].clk));
	}

	return 0;
}

static int exynos5_devfreq_disp_set_freq(struct devfreq_data_disp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;
#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_disp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
				pr_debug("DISP clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk));
			}
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;
#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_disp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
				pr_debug("DISP clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk));
			}
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}

	return 0;
}

static struct devfreq_data_disp *data_disp;

#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_disp_set_clk(struct devfreq_data_disp *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info[target_idx].states;

	if (clk_get_rate(clk) < clk_info[target_idx].freq) {
		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_disp_clk[clk_states->state[i].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	} else {
		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);

		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_disp_clk[clk_states->state[i].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	}

	return 0;
}

void exynos5_disp_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_disp->use_dvfs)
		return;

	mutex_lock(&data_disp->lock);
	cur_freq_idx = exynos5_devfreq_get_idx(devfreq_disp_opp_list,
			ARRAY_SIZE(devfreq_disp_opp_list),
			data_disp->devfreq->previous_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_disp->lock);
		pr_err("DEVFREQ(INT) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_pm_domain); ++i) {
		if (devfreq_disp_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_disp_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos5_devfreq_disp_set_clk(data_disp,
				cur_freq_idx,
				devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk,
				devfreq_clk_disp_info_list[i]);
	}
	mutex_unlock(&data_disp->lock);
}
#endif

int exynos5433_devfreq_disp_init(struct devfreq_data_disp *data)
{
	int ret = 0;
	data->max_state = DISP_LV_COUNT;
	data_disp = data;
	if (exynos5_devfreq_disp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos5_devfreq_disp_init_pm_domain()) {
		ret = -EINVAL;
		goto err_data;
	}
#endif

	data->disp_set_freq = exynos5_devfreq_disp_set_freq;
err_data:
	return ret;
}
/* end of DISP related function */

/* ========== 1. INT related function */
static struct devfreq_data_int *data_int;

extern struct pm_qos_request min_int_thermal_qos;

enum devfreq_int_idx {
	INT_LV0,
	INT_LV1,
	INT_LV2,
	INT_LV3,
	INT_LV4,
	INT_LV5,
	INT_LV6,
	INT_LV_COUNT,
};

enum devfreq_int_clk {
	DOUT_ACLK_BUS0_400,
	DOUT_ACLK_BUS1_400,
	DOUT_MIF_PRE_4_INT,
	DOUT_ACLK_BUS2_400,
	MOUT_BUS_PLL_USER_4_INT,
	MOUT_MFC_PLL_USER_4_INT,
	MOUT_ISP_PLL_4_INT,
	MOUT_MPHY_PLL_USER,
	MOUT_ACLK_G2D_400_A,
	DOUT_ACLK_G2D_400,
	DOUT_ACLK_G2D_266,
	DOUT_ACLK_GSCL_333,
	MOUT_ACLK_MSCL_400_A,
	DOUT_ACLK_MSCL_400,
	MOUT_SCLK_JPEG_A,
	MOUT_SCLK_JPEG_B,
	MOUT_SCLK_JPEG_C,
	DOUT_SCLK_JPEG,
	MOUT_ACLK_MFC_400_A,
	MOUT_ACLK_MFC_400_B,
	MOUT_ACLK_MFC_400_C,
	DOUT_ACLK_MFC_400,
	DOUT_ACLK_HEVC_400,
	INT_CLK_COUNT,
};

struct devfreq_clk_list devfreq_int_clk[INT_CLK_COUNT] = {
	{"dout_aclk_bus0_400",},
	{"dout_aclk_bus1_400",},
	{"dout_mif_pre",},
	{"dout_aclk_bus2_400",},
	{"mout_bus_pll_user",},
	{"mout_mfc_pll_user",},
	{"mout_isp_pll",},
	{"mout_mphy_pll_user",},
	{"mout_aclk_g2d_400_a",},
	{"dout_aclk_g2d_400",},
	{"dout_aclk_g2d_266",},
	{"dout_aclk_gscl_333",},
	{"mout_aclk_mscl_400_a",},
	{"dout_aclk_mscl_400", },
	{"mout_sclk_jpeg_a",},
	{"mout_sclk_jpeg_b",},
	{"mout_sclk_jpeg_c",},
	{"dout_sclk_jpeg",},
	{"mout_aclk_mfc_400_a",},
	{"mout_aclk_mfc_400_b",},
	{"mout_aclk_mfc_400_c",},
	{"dout_aclk_mfc_400",},
	{"dout_aclk_hevc_400",},
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{INT_LV0,	400000,	1075000},
	{INT_LV1,	334000,	1025000},
	{INT_LV2,	267000,	1000000},
	{INT_LV3,	200000,	 975000},
	{INT_LV4,	160000,	 962500},
	{INT_LV5,	133000,	 950000},
	{INT_LV6,	100000,	 937500},
};

struct devfreq_clk_state aclk_g2d_mfc_pll[] = {
	{MOUT_ACLK_G2D_400_A,	MOUT_MFC_PLL_USER_4_INT},
};

struct devfreq_clk_state aclk_g2d_bus_pll[] = {
	{MOUT_ACLK_G2D_400_A,	MOUT_BUS_PLL_USER_4_INT},
};

struct devfreq_clk_state aclk_mscl_mfc_pll[] = {
	{MOUT_ACLK_MSCL_400_A,	MOUT_MFC_PLL_USER_4_INT},
};

struct devfreq_clk_state aclk_mscl_bus_pll[] = {
	{MOUT_ACLK_MSCL_400_A,	MOUT_BUS_PLL_USER_4_INT},
};

struct devfreq_clk_state sclk_jpeg_mfc_pll[] = {
	{MOUT_SCLK_JPEG_B,	MOUT_MFC_PLL_USER_4_INT},
	{MOUT_SCLK_JPEG_C,	MOUT_SCLK_JPEG_B},
};

struct devfreq_clk_state sclk_jpeg_bus_pll[] = {
	{MOUT_SCLK_JPEG_A,	MOUT_BUS_PLL_USER_4_INT},
	{MOUT_SCLK_JPEG_B,	MOUT_SCLK_JPEG_A},
	{MOUT_SCLK_JPEG_C,	MOUT_SCLK_JPEG_B},
};

struct devfreq_clk_state aclk_mfc_400_bus_pll[] = {
	{MOUT_ACLK_MFC_400_B,	MOUT_BUS_PLL_USER_4_INT},
	{MOUT_ACLK_MFC_400_C,	MOUT_ACLK_MFC_400_B},
};

struct devfreq_clk_state aclk_mfc_400_mfc_pll[] = {
	{MOUT_ACLK_MFC_400_A,	MOUT_MFC_PLL_USER_4_INT},
	{MOUT_ACLK_MFC_400_B,	MOUT_ACLK_MFC_400_A},
	{MOUT_ACLK_MFC_400_C,	MOUT_ACLK_MFC_400_B},
};

struct devfreq_clk_states aclk_g2d_mfc_pll_list = {
	.state = aclk_g2d_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_g2d_mfc_pll),
};

struct devfreq_clk_states aclk_g2d_bus_pll_list = {
	.state = aclk_g2d_bus_pll,
	.state_count = ARRAY_SIZE(aclk_g2d_bus_pll),
};

struct devfreq_clk_states aclk_mscl_mfc_pll_list = {
	.state = aclk_mscl_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_mfc_pll),
};

struct devfreq_clk_states aclk_mscl_bus_pll_list = {
	.state = aclk_mscl_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_bus_pll),
};

struct devfreq_clk_states sclk_jpeg_mfc_pll_list = {
	.state = sclk_jpeg_mfc_pll,
	.state_count = ARRAY_SIZE(sclk_jpeg_mfc_pll),
};

struct devfreq_clk_states sclk_jpeg_bus_pll_list = {
	.state = sclk_jpeg_bus_pll,
	.state_count = ARRAY_SIZE(sclk_jpeg_bus_pll),
};

struct devfreq_clk_states aclk_mfc_400_bus_pll_list = {
	.state = aclk_mfc_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mfc_400_bus_pll),
};

struct devfreq_clk_states aclk_mfc_400_mfc_pll_list = {
	.state = aclk_mfc_400_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mfc_400_bus_pll),
};

struct devfreq_clk_info aclk_bus0_400[] = {
	{INT_LV0,	400000000,	0,	NULL},
	{INT_LV1,	267000000,	0,	NULL},
	{INT_LV2,	267000000,	0,	NULL},
	{INT_LV3,	200000000,	0,	NULL},
	{INT_LV4,	160000000,	0,	NULL},
	{INT_LV5,	134000000,	0,	NULL},
	{INT_LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus1_400[] = {
	{INT_LV0,	400000000,	0,	NULL},
	{INT_LV1,	267000000,	0,	NULL},
	{INT_LV2,	267000000,	0,	NULL},
	{INT_LV3,	200000000,	0,	NULL},
	{INT_LV4,	160000000,	0,	NULL},
	{INT_LV5,	134000000,	0,	NULL},
	{INT_LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info dout_mif_pre[] = {
	{INT_LV0,	400000000,	0,	NULL},
	{INT_LV1,	400000000,	0,	NULL},
	{INT_LV2,	400000000,	0,	NULL},
	{INT_LV3,	400000000,	0,	NULL},
	{INT_LV4,	400000000,	0,	NULL},
	{INT_LV5,	400000000,	0,	NULL},
	{INT_LV6,	400000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus2_400[] = {
	{INT_LV0,	400000000,	0,	NULL},
	{INT_LV1,	200000000,	0,	NULL},
	{INT_LV2,	200000000,	0,	NULL},
	{INT_LV3,	200000000,	0,	NULL},
	{INT_LV4,	200000000,	0,	NULL},
	{INT_LV5,	134000000,	0,	NULL},
	{INT_LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_g2d_400[] = {
	{INT_LV0,	400000000,	0,	&aclk_g2d_bus_pll_list},
	{INT_LV1,	334000000,	0,	&aclk_g2d_mfc_pll_list},
	{INT_LV2,	267000000,	0,	&aclk_g2d_bus_pll_list},
	{INT_LV3,	200000000,	0,	&aclk_g2d_bus_pll_list},
	{INT_LV4,	160000000,	0,	&aclk_g2d_bus_pll_list},
	{INT_LV5,	134000000,	0,	&aclk_g2d_bus_pll_list},
	{INT_LV6,	100000000,	0,	&aclk_g2d_bus_pll_list},
};

struct devfreq_clk_info aclk_g2d_266[] = {
	{INT_LV0,	267000000,	0,	NULL},
	{INT_LV1,	267000000,	0,	NULL},
	{INT_LV2,	200000000,	0,	NULL},
	{INT_LV3,	160000000,	0,	NULL},
	{INT_LV4,	134000000,	0,	NULL},
	{INT_LV5,	100000000,	0,	NULL},
	{INT_LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_gscl_333[] = {
	{INT_LV0,	334000000,	0,	NULL},
	{INT_LV1,	334000000,	0,	NULL},
	{INT_LV2,	334000000,	0,	NULL},
	{INT_LV3,	222000000,	0,	NULL},
	{INT_LV4,	222000000,	0,	NULL},
	{INT_LV5,	167000000,	0,	NULL},
	{INT_LV6,	167000000,	0,	NULL},
};

struct devfreq_clk_info aclk_mscl[] = {
	{INT_LV0,	400000000,	0,	&aclk_mscl_bus_pll_list},
	{INT_LV1,	334000000,	0,	&aclk_mscl_mfc_pll_list},
	{INT_LV2,	267000000,	0,	&aclk_mscl_bus_pll_list},
	{INT_LV3,	200000000,	0,	&aclk_mscl_bus_pll_list},
	{INT_LV4,	160000000,	0,	&aclk_mscl_bus_pll_list},
	{INT_LV5,	134000000,	0,	&aclk_mscl_bus_pll_list},
	{INT_LV6,	100000000,	0,	&aclk_mscl_bus_pll_list},
};

struct devfreq_clk_info sclk_jpeg[] = {
	{INT_LV0,	400000000,	0,	&sclk_jpeg_bus_pll_list},
	{INT_LV1,	334000000,	0,	&sclk_jpeg_mfc_pll_list},
	{INT_LV2,	267000000,	0,	&sclk_jpeg_bus_pll_list},
	{INT_LV3,	200000000,	0,	&sclk_jpeg_bus_pll_list},
	{INT_LV4,	160000000,	0,	&sclk_jpeg_bus_pll_list},
	{INT_LV5,	134000000,	0,	&sclk_jpeg_bus_pll_list},
	{INT_LV6,	100000000,	0,	&sclk_jpeg_bus_pll_list},
};

struct devfreq_clk_info aclk_mfc_400[] = {
	{INT_LV0,	400000000,	0,	&aclk_mfc_400_bus_pll_list},
	{INT_LV1,	334000000,	0,	&aclk_mfc_400_mfc_pll_list},
	{INT_LV2,	267000000,	0,	&aclk_mfc_400_bus_pll_list},
	{INT_LV3,	200000000,	0,	&aclk_mfc_400_bus_pll_list},
	{INT_LV4,	200000000,	0,	&aclk_mfc_400_bus_pll_list},
	{INT_LV5,	160000000,	0,	&aclk_mfc_400_bus_pll_list},
	{INT_LV6,	100000000,	0,	&aclk_mfc_400_bus_pll_list},
};

struct devfreq_clk_info aclk_hevc_400[] = {
	{INT_LV0,	400000000,	0,	NULL},
	{INT_LV1,	267000000,	0,	NULL},
	{INT_LV2,	267000000,	0,	NULL},
	{INT_LV3,	200000000,	0,	NULL},
	{INT_LV4,	160000000,	0,	NULL},
	{INT_LV5,	134000000,	0,	NULL},
	{INT_LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	aclk_bus0_400,
	aclk_bus1_400,
	dout_mif_pre,
	aclk_bus2_400,
	aclk_g2d_400,
	aclk_g2d_266,
	aclk_gscl_333,
	aclk_mscl,
	sclk_jpeg,
	aclk_mfc_400,
	aclk_hevc_400,
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	DOUT_ACLK_BUS0_400,
	DOUT_ACLK_BUS1_400,
	DOUT_MIF_PRE_4_INT,
	DOUT_ACLK_BUS2_400,
	DOUT_ACLK_G2D_400,
	DOUT_ACLK_G2D_266,
	DOUT_ACLK_GSCL_333,
	DOUT_ACLK_MSCL_400,
	DOUT_SCLK_JPEG,
	DOUT_ACLK_MFC_400,
	DOUT_ACLK_HEVC_400,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_int_pm_domain[] = {
	{"pd-bus0",},
	{"pd-bus1",},
	{"pd-bus2",},
	{"pd-bus2",},
	{"pd-g2d",},
	{"pd-g2d",},
	{"pd-gscl",},
	{"pd-mscl",},
	{"pd-mscl",},
	{"pd-mfc",},
	{"pd-hevc",},
};

static int exynos5_devfreq_int_init_pm_domain(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	int i;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;

		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd = platform_get_drvdata(pdev);

		for (i = 0; i < ARRAY_SIZE(devfreq_int_pm_domain); ++i) {
			if (devfreq_int_pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(devfreq_int_pm_domain[i].pm_domain_name, pd->genpd.name))
				devfreq_int_pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}
#endif

int district_level_by_disp_333[] = {
	INT_LV2,
	INT_LV4,
	INT_LV6,
	INT_LV6,
};

extern struct pm_qos_request exynos5_int_bts_qos;

void exynos5_update_district_int_level(int aclk_disp_333_idx)
{
	int int_qos = INT_LV6;

	if (aclk_disp_333_idx < 0 ||
		ARRAY_SIZE(district_level_by_disp_333) <= aclk_disp_333_idx) {
		pr_err("DEVFREQ(INT) : can't update distriction of int level by aclk_disp_333\n");
		return;
	}

	int_qos = district_level_by_disp_333[aclk_disp_333_idx];

	if (pm_qos_request_active(&exynos5_int_bts_qos))
		pm_qos_update_request(&exynos5_int_bts_qos, devfreq_int_opp_list[int_qos].freq);
}

static int exynos5_devfreq_int_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_clk); ++i) {
		devfreq_int_clk[i].clk = clk_get(NULL, devfreq_int_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_int_clk[i].clk)) {
			pr_err("DEVFREQ(INT) : %s can't get clock\n", devfreq_int_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("INT clk name: %s, rate: %lu\n", devfreq_int_clk[i].clk_name, clk_get_rate(devfreq_int_clk[i].clk));
	}

	return 0;
}

#define ISP_CONSTRAINT_VOLT_BASE	(900000)

static int exynos5_devfreq_int_set_volt(struct devfreq_data_int *data,
					unsigned long volt,
					unsigned long volt_range)
{
	unsigned long volt_m = data->volt_constraint_isp;

	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_int, volt, volt_range);
	data->old_volt = volt;
out:
	if (volt_m < data->target_volt)
		volt_m = data->target_volt;
	if (volt_m < ISP_CONSTRAINT_VOLT_BASE)
		volt_m = ISP_CONSTRAINT_VOLT_BASE;
	volt_m += 50000;
	regulator_set_voltage(data->vdd_int_m, volt_m, volt_m + VOLT_STEP);

	return 0;
}

static int exynos5_devfreq_int_set_freq(struct devfreq_data_int *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_int_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);
				pr_debug("INT clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk));
			}
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_int_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);
				pr_debug("INT clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk));
			}
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}
	return 0;
}

int exynos5_devfreq_get_idx(struct devfreq_opp_table *table,
				unsigned int size,
				unsigned long freq)
{
	int i;

	for (i = 0; i < size; ++i) {
		if (table[i].freq == freq)
			return i;
	}

	return -1;
}

#ifdef INT_ISP_CONSTRAINT
int exynos5_int_check_voltage_constraint(unsigned long isp_voltage)
{
	unsigned long max_voltage;

	mutex_lock(&data_int->lock);
	max_voltage = data_int->volt_constraint_isp;
	if (max_voltage < data_int->target_volt)
		max_voltage = data_int->target_volt;
	if (max_voltage < ISP_CONSTRAINT_VOLT_BASE)
		max_voltage = ISP_CONSTRAINT_VOLT_BASE;
	max_voltage += 50000;
	regulator_set_voltage(data_int->vdd_int_m, max_voltage, max_voltage + VOLT_STEP);
	mutex_unlock(&data_int->lock);

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_THERMAL
int exynos5_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_int *data = container_of(nb, struct devfreq_data_int, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos, data->initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos, data->default_qos);
	}

	return NOTIFY_OK;
}
#endif

static struct devfreq_data_int *data_int;

#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_int_set_clk(struct devfreq_data_int *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info[target_idx].states;

	if (clk_get_rate(clk) < clk_info[target_idx].freq) {
		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_int_clk[clk_states->state[i].clk_idx].clk,
					devfreq_int_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	} else {
		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);

		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_int_clk[clk_states->state[i].clk_idx].clk,
					devfreq_int_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	}

	return 0;
}

void exynos5_int_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_int->use_dvfs)
		return;

	mutex_lock(&data_int->lock);
	cur_freq_idx = exynos5_devfreq_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						data_int->devfreq->previous_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_int->lock);
		pr_err("DEVFREQ(INT) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_int_pm_domain); ++i) {
		if (devfreq_int_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_int_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos5_devfreq_int_set_clk(data_int,
						cur_freq_idx,
						devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk,
						devfreq_clk_int_info_list[i]);
	}
	mutex_unlock(&data_int->lock);
}
#endif

int exynos5433_devfreq_int_init(struct devfreq_data_int *data)
{
	int ret = 0;

	data_int = data;
	data->max_state = INT_LV_COUNT;

	if (exynos5_devfreq_int_init_clock()) {
		ret = -EINVAL;
		return ret;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos5_devfreq_int_init_pm_domain()) {
		ret = -EINVAL;
		return ret;
	}
#endif
	data->int_set_volt = exynos5_devfreq_int_set_volt;
	data->int_set_freq = exynos5_devfreq_int_set_freq;

	return ret;
}
/* end of INT related function */

/* ========== 2. ISP related function */
extern struct pm_qos_request min_isp_thermal_qos;

enum devfreq_isp_idx {
	ISP_LV0,
	ISP_LV1,
	ISP_LV2,
	ISP_LV3,
	ISP_LV4,
	ISP_LV5,
	ISP_LV6,
	ISP_LV7,
	ISP_LV8,
	ISP_LV9,
	ISP_LV10,
	ISP_LV_COUNT,
};

enum devfreq_isp_clk {
	ISP_PLL,
	DOUT_ACLK_CAM0_552,
	DOUT_ACLK_CAM0_400,
	DOUT_ACLK_CAM0_333,
	DOUT_ACLK_CAM0_BUS_400,
	DOUT_ACLK_CSIS0,
	DOUT_ACLK_LITE_A,
	DOUT_ACLK_3AA0,
	DOUT_ACLK_CSIS1,
	DOUT_ACLK_LITE_B,
	DOUT_ACLK_3AA1,
	DOUT_ACLK_LITE_D,
	DOUT_SCLK_PIXEL_INIT_552,
	DOUT_SCLK_PIXEL_333,
	DOUT_ACLK_CAM1_552,
	DOUT_ACLK_CAM1_400,
	DOUT_ACLK_CAM1_333,
	DOUT_ACLK_FD_400,
	DOUT_ACLK_CSIS2_333,
	DOUT_ACLK_LITE_C,
	DOUT_ACLK_ISP_400,
	DOUT_ACLK_ISP_DIS_400,
	MOUT_ACLK_CAM1_552_A,
	MOUT_ACLK_CAM1_552_B,
	MOUT_ISP_PLL_4_ISP,
	MOUT_BUS_PLL_USER_4_ISP,
	MOUT_MFC_PLL_USER_ISP,
	MOUT_ACLK_FD_A,
	MOUT_ACLK_FD_B,
	MOUT_ACLK_ISP_400,
	MOUT_ACLK_CAM1_333_USER,
	MOUT_ACLK_CAM1_400_USER,
	MOUT_ACLK_CAM1_552_USER,
	MOUT_SCLK_PIXELASYNC_LITE_C_B,
	MOUT_ACLK_CAM0_333_USER,
	MOUT_ACLK_CAM0_400_USER,
	MOUT_ACLK_CAM0_552_USER,
	MOUT_ACLK_LITE_A_A,
	MOUT_ACLK_3AA0_A,
	MOUT_ACLK_CSIS0_B,
	MOUT_ACLK_CSIS0_A,
	MOUT_ACLK_CSIS1_B,
	MOUT_ACLK_CSIS1_A,
	MOUT_ACLK_LITE_B_A,
	MOUT_ACLK_3AA1_A,
	ACLK_CAM0_552,
	ACLK_CAM0_400,
	ACLK_CAM0_333,
	MOUT_ACLK_LITE_C_B,
	MOUT_ACLK_CSIS2_B,
	MOUT_ACLK_ISP_DIS_400,
	MOUT_SCLK_LITE_FREECNT_C,
	MOUT_SCLK_LITE_FREECNT_B,
	DOUT_PCLK_PIXELASYNC_LITE_C,
	DOUT_PCLK_LITE_D,
	DOUT_PCLK_CAM1_83,
	CLK_COUNT,
};

struct devfreq_clk_list devfreq_isp_clk[CLK_COUNT] = {
	{"fout_isp_pll",},
	{"dout_aclk_cam0_552",},
	{"dout_aclk_cam0_400",},
	{"dout_aclk_cam0_333",},
	{"dout_aclk_cam0_bus_400",},
	{"dout_aclk_csis0",},
	{"dout_aclk_lite_a",},
	{"dout_aclk_3aa0",},
	{"dout_aclk_csis1",},
	{"dout_aclk_lite_b",},
	{"dout_aclk_3aa1",},
	{"dout_aclk_lite_d",},
	{"dout_sclk_pixelasync_lite_c_init",},
	{"dout_sclk_pixelasync_lite_c",},
	{"dout_aclk_cam1_552",},
	{"dout_aclk_cam1_400",},
	{"dout_aclk_cam1_333",},
	{"dout_aclk_fd",},
	{"dout_aclk_csis2",},
	{"dout_aclk_lite_c",},
	{"dout_aclk_isp_400",},
	{"dout_aclk_isp_dis_400",},
	{"mout_aclk_cam1_552_a",},
	{"mout_aclk_cam1_552_b",},
	{"mout_isp_pll",},
	{"mout_bus_pll_user",},
	{"mout_mfc_pll_user",},
	{"mout_aclk_fd_a",},
	{"mout_aclk_fd_b",},
	{"mout_aclk_isp_400",},
	{"mout_aclk_cam1_333_user",},
	{"mout_aclk_cam1_400_user",},
	{"mout_aclk_cam1_552_user",},
	{"mout_sclk_pixelasync_lite_c_b",},
	{"mout_aclk_cam0_333_user",},
	{"mout_aclk_cam0_400_user",},
	{"mout_aclk_cam0_552_user",},
	{"mout_aclk_lite_a_a",},
	{"mout_aclk_3aa0_a",},
	{"mout_aclk_csis0_b",},
	{"mout_aclk_csis0_a",},
	{"mout_aclk_csis1_b",},
	{"mout_aclk_csis1_a",},
	{"mout_aclk_lite_b_a",},
	{"mout_aclk_3aa1_a",},
	{"aclk_cam0_552",},
	{"aclk_cam0_400",},
	{"aclk_cam0_333",},
	{"mout_aclk_lite_c_b",},
	{"mout_aclk_csis2_b",},
	{"mout_aclk_isp_dis_400",},
	{"mout_sclk_lite_freecnt_c",},
	{"mout_sclk_lite_freecnt_b",},
	{"dout_pclk_pixelasync_lite_c",},
	{"dout_pclk_lite_d",},
	{"dout_pclk_cam1_83",},
};

struct devfreq_opp_table devfreq_isp_opp_list[] = {
	{ISP_LV0,	777000,	950000},		/* CAM0(L0), ISP&CAM1(L0) */
	{ISP_LV1,	666000,	950000},		/* CAM0(L0), ISP&CAM1(L1) */
	{ISP_LV2,	600000,	950000},		/* CAM0(L0), ISP&CAM1(L3) */
	{ISP_LV3,	580000,	950000},		/* CAM0(L0), ISP&CAM1(L4) */
	{ISP_LV4,	555000,	950000},		/* CAM0(L1), ISP&CAM1(L4) */
	{ISP_LV5,	444000,	950000},		/* CAM0(L2), ISP&CAM1(L2) */
	{ISP_LV6,	333000,	950000},		/* CAM0(L2), ISP&CAM1(L3) */
	{ISP_LV7,	222000,	950000},		/* CAM0(L3), ISP&CAM1(L2) */
	{ISP_LV8,	200000,	950000},		/* CAM0(L3), ISP&CAM1(L3) */
	{ISP_LV9,	111000,	925000},		/* CAM0(L4), ISP&CAM1(L3) */
	{ISP_LV10,	 66000,	925000},		/* CAM0(L5), ISP&CAM1(L5) */
};

struct devfreq_clk_state mux_sclk_pixelasync_lite_c[] = {
	{MOUT_SCLK_PIXELASYNC_LITE_C_B,	MOUT_ACLK_CAM0_333_USER},
};

struct devfreq_clk_state mux_sclk_lite_freecnt_c[] = {
	{MOUT_SCLK_LITE_FREECNT_B,	DOUT_PCLK_PIXELASYNC_LITE_C},
};

struct devfreq_clk_state aclk_cam1_552_isp_pll[] = {
	{MOUT_ACLK_CAM1_552_A,	MOUT_ISP_PLL_4_ISP},
	{MOUT_ACLK_CAM1_552_B,	MOUT_ACLK_CAM1_552_A},
};

struct devfreq_clk_state aclk_cam1_552_bus_pll[] = {
	{MOUT_ACLK_CAM1_552_A,	MOUT_BUS_PLL_USER_4_ISP},
	{MOUT_ACLK_CAM1_552_B,	MOUT_ACLK_CAM1_552_A},
};

/* for debugging */
struct devfreq_clk_state mux_aclk_cam0_user[] = {
	{MOUT_ACLK_CAM0_333_USER, ACLK_CAM0_333},
	{MOUT_ACLK_CAM0_400_USER, ACLK_CAM0_400},
	{MOUT_ACLK_CAM0_552_USER, ACLK_CAM0_552},
};

struct devfreq_clk_states aclk_mux_aclk_cam0_user_list = {
	.state = mux_aclk_cam0_user,
	.state_count = ARRAY_SIZE(mux_aclk_cam0_user),
};

struct devfreq_clk_state mux_aclk_csis2[] = {
	{MOUT_ACLK_CSIS2_B,	MOUT_ACLK_CAM1_333_USER},
};

struct devfreq_clk_state mux_aclk_lite_c[] = {
	{MOUT_ACLK_LITE_C_B,	MOUT_ACLK_CAM1_333_USER},
};

struct devfreq_clk_state aclk_fd_400_bus_pll[] = {
	{MOUT_ACLK_FD_A,	MOUT_ACLK_CAM1_400_USER},
	{MOUT_ACLK_FD_B,	MOUT_ACLK_FD_A},
};

struct devfreq_clk_state aclk_fd_400_mfc_pll[] = {
	{MOUT_ACLK_FD_B,	MOUT_ACLK_CAM1_333_USER},
};

struct devfreq_clk_state aclk_isp_400_bus_pll[] = {
	{MOUT_ACLK_ISP_400,	MOUT_BUS_PLL_USER_4_ISP},
};

struct devfreq_clk_state aclk_isp_400_mfc_pll[] = {
	{MOUT_ACLK_ISP_400,	MOUT_MFC_PLL_USER_ISP},
};

struct devfreq_clk_state mux_aclk_isp_dis_400_bus_pll[] = {
	{MOUT_ACLK_ISP_DIS_400,	MOUT_BUS_PLL_USER_4_ISP},
};

struct devfreq_clk_state mux_aclk_isp_dis_400_mfc_pll[] = {
	{MOUT_ACLK_ISP_DIS_400,	MOUT_MFC_PLL_USER_ISP},
};

struct devfreq_clk_states sclk_lite_freecnt_c_list = {
	.state = mux_sclk_lite_freecnt_c,
	.state_count = ARRAY_SIZE(mux_sclk_lite_freecnt_c),
};

struct devfreq_clk_states sclk_pixelasync_lite_c_list = {
	.state = mux_sclk_pixelasync_lite_c,
	.state_count = ARRAY_SIZE(mux_sclk_pixelasync_lite_c),
};

struct devfreq_clk_states aclk_cam1_552_isp_pll_list = {
	.state = aclk_cam1_552_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_552_isp_pll),
};

struct devfreq_clk_states aclk_cam1_552_bus_pll_list = {
	.state = aclk_cam1_552_bus_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_552_bus_pll),
};

struct devfreq_clk_states mux_aclk_csis2_list = {
	.state = mux_aclk_csis2,
	.state_count = ARRAY_SIZE(mux_aclk_csis2),
};

struct devfreq_clk_states mux_aclk_lite_c_list = {
	.state = mux_aclk_lite_c,
	.state_count = ARRAY_SIZE(mux_aclk_lite_c),
};

struct devfreq_clk_states aclk_fd_400_bus_pll_list = {
	.state = aclk_fd_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_fd_400_bus_pll),
};

struct devfreq_clk_states aclk_fd_400_mfc_pll_list = {
	.state = aclk_fd_400_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_fd_400_mfc_pll),
};

struct devfreq_clk_states aclk_isp_400_bus_pll_list = {
	.state = aclk_isp_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_isp_400_bus_pll),
};

struct devfreq_clk_states aclk_isp_400_mfc_pll_list = {
	.state = aclk_isp_400_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_isp_400_mfc_pll),
};

struct devfreq_clk_states aclk_isp_dis_400_bus_pll_list = {
	.state = mux_aclk_isp_dis_400_bus_pll,
	.state_count = ARRAY_SIZE(mux_aclk_isp_dis_400_bus_pll),
};

struct devfreq_clk_states aclk_isp_dis_400_mfc_pll_list = {
	.state = mux_aclk_isp_dis_400_mfc_pll,
	.state_count = ARRAY_SIZE(mux_aclk_isp_dis_400_mfc_pll),
};

/* for ACLK_CSIS0 */
struct devfreq_clk_state mux_aclk_csis0_isp_pll[] = {
	{MOUT_ACLK_CSIS0_A,	MOUT_ACLK_CAM0_552_USER},
};

struct devfreq_clk_states aclk_csis0_isp_pll_list = {
	.state = mux_aclk_csis0_isp_pll,
	.state_count = ARRAY_SIZE(mux_aclk_csis0_isp_pll),
};

struct devfreq_clk_state mux_aclk_csis0_bus_pll[] = {
	{MOUT_ACLK_CSIS0_A,	MOUT_ACLK_CAM0_400_USER},
};

struct devfreq_clk_states aclk_csis0_bus_pll_list = {
	.state = mux_aclk_csis0_bus_pll,
	.state_count = ARRAY_SIZE(mux_aclk_csis0_bus_pll),
};

/* for ACLK_LITE_A  */
struct devfreq_clk_state mux_aclk_lite_a_isp_pll[] = {
	{MOUT_ACLK_LITE_A_A,	MOUT_ACLK_CAM0_552_USER},
};

struct devfreq_clk_states aclk_lite_a_isp_pll_list = {
	.state = mux_aclk_lite_a_isp_pll,
	.state_count = ARRAY_SIZE(mux_aclk_lite_a_isp_pll),
};

struct devfreq_clk_state mux_aclk_lite_a_bus_pll[] = {
	{MOUT_ACLK_LITE_A_A,	MOUT_ACLK_CAM0_400_USER},
};

struct devfreq_clk_states aclk_lite_a_bus_pll_list = {
	.state = mux_aclk_lite_a_bus_pll,
	.state_count = ARRAY_SIZE(mux_aclk_lite_a_bus_pll),
};

/* for ACLK_3AA0 */
struct devfreq_clk_state mux_aclk_3aa0_isp_pll[] = {
	{MOUT_ACLK_3AA0_A,	MOUT_ACLK_CAM0_552_USER},
};

struct devfreq_clk_states aclk_3aa0_isp_pll_list = {
	.state = mux_aclk_3aa0_isp_pll,
	.state_count = ARRAY_SIZE(mux_aclk_3aa0_isp_pll),
};

struct devfreq_clk_state mux_aclk_3aa0_bus_pll[] = {
	{MOUT_ACLK_3AA0_A,	MOUT_ACLK_CAM0_400_USER},
};

struct devfreq_clk_states aclk_3aa0_bus_pll_list = {
	.state = mux_aclk_3aa0_bus_pll,
	.state_count = ARRAY_SIZE(mux_aclk_3aa0_bus_pll),
};

/* for ACLK_CSIS1 */
struct devfreq_clk_state mux_aclk_csis1_bus_pll[] = {
	{MOUT_ACLK_CSIS1_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_CSIS1_B,	MOUT_ACLK_CSIS1_A},
};

struct devfreq_clk_states aclk_csis1_bus_pll_list = {
	.state = mux_aclk_csis1_bus_pll,
	.state_count = ARRAY_SIZE(mux_aclk_csis1_bus_pll),
};

struct devfreq_clk_state mux_aclk_csis1_isp_pll[] = {
	{MOUT_ACLK_CSIS1_A,	MOUT_ACLK_CAM0_552_USER},
	{MOUT_ACLK_CSIS1_B,	MOUT_ACLK_CSIS1_A},
};

struct devfreq_clk_states aclk_csis1_isp_pll_list = {
	.state = mux_aclk_csis1_isp_pll,
	.state_count = ARRAY_SIZE(mux_aclk_csis1_isp_pll),
};

/* for ACLK_LITE_B */
struct devfreq_clk_state mux_aclk_lite_b_bus_pll[] = {
	{MOUT_ACLK_LITE_B_A,	MOUT_ACLK_CAM0_400_USER},
};

struct devfreq_clk_states aclk_lite_b_bus_pll_list = {
	.state = mux_aclk_lite_b_bus_pll,
	.state_count = ARRAY_SIZE(mux_aclk_lite_b_bus_pll),
};

struct devfreq_clk_state mux_aclk_lite_b_isp_pll[] = {
	{MOUT_ACLK_LITE_B_A,	MOUT_ACLK_CAM0_552_USER},
};

struct devfreq_clk_states aclk_lite_b_isp_pll_list = {
	.state = mux_aclk_lite_b_isp_pll,
	.state_count = ARRAY_SIZE(mux_aclk_lite_b_isp_pll),
};

/* for ACLK_3AA1 */
struct devfreq_clk_state mux_aclk_3aa1_bus_pll[] = {
	{MOUT_ACLK_3AA1_A,	MOUT_ACLK_CAM0_400_USER},
};

struct devfreq_clk_states aclk_3aa1_bus_pll_list = {
	.state = mux_aclk_3aa1_bus_pll,
	.state_count = ARRAY_SIZE(mux_aclk_3aa1_bus_pll),
};

struct devfreq_clk_state mux_aclk_3aa1_isp_pll[] = {
	{MOUT_ACLK_3AA1_A,	MOUT_ACLK_CAM0_552_USER},
};

struct devfreq_clk_states aclk_3aa1_isp_pll_list = {
	.state = mux_aclk_3aa1_isp_pll,
	.state_count = ARRAY_SIZE(mux_aclk_3aa1_isp_pll),
};

/* CAM0 */
struct devfreq_clk_info aclk_cam0_552[] = {
	{ISP_LV0,	552000000,	0,	NULL},
	{ISP_LV1,	552000000,	0,	NULL},
	{ISP_LV2,	552000000,	0,	NULL},
	{ISP_LV3,	552000000,	0,	NULL},
	{ISP_LV4,	552000000,	0,	NULL},
	{ISP_LV5,	552000000,	0,	NULL},
	{ISP_LV6,	552000000,	0,	NULL},
	{ISP_LV7,	552000000,	0,	NULL},
	{ISP_LV8,	552000000,	0,	NULL},
	{ISP_LV9,	138000000,	0,	NULL},
	{ISP_LV10,	 69000000,	0,	NULL},
};

struct devfreq_clk_info aclk_cam0_400[] = {
	{ISP_LV0,	400000000,	0,	NULL},
	{ISP_LV1,	400000000,	0,	NULL},
	{ISP_LV2,	400000000,	0,	NULL},
	{ISP_LV3,	400000000,	0,	NULL},
	{ISP_LV4,	400000000,	0,	NULL},
	{ISP_LV5,	400000000,	0,	NULL},
	{ISP_LV6,	400000000,	0,	NULL},
	{ISP_LV7,	400000000,	0,	NULL},
	{ISP_LV8,	400000000,	0,	NULL},
	{ISP_LV9,	160000000,	0,	NULL},
	{ISP_LV10,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_cam0_333[] = {
	{ISP_LV0,	334000000,	0,	NULL},
	{ISP_LV1,	334000000,	0,	NULL},
	{ISP_LV2,	334000000,	0,	NULL},
	{ISP_LV3,	334000000,	0,	NULL},
	{ISP_LV4,	334000000,	0,	NULL},
	{ISP_LV5,	334000000,	0,	NULL},
	{ISP_LV6,	334000000,	0,	NULL},
	{ISP_LV7,	334000000,	0,	NULL},
	{ISP_LV8,	334000000,	0,	NULL},
	{ISP_LV9,	167000000,	0,	NULL},
	{ISP_LV10,	 84000000,	0,	NULL},
};

struct devfreq_clk_info aclk_cam0_bus_400[] = {
	{ISP_LV0,	400000000,	0,	NULL},
	{ISP_LV1,	400000000,	0,	NULL},
	{ISP_LV2,	400000000,	0,	NULL},
	{ISP_LV3,	400000000,	0,	NULL},
	{ISP_LV4,	400000000,	0,	NULL},
	{ISP_LV5,	400000000,	0,	NULL},
	{ISP_LV6,	400000000,	0,	NULL},
	{ISP_LV7,	400000000,	0,	NULL},
	{ISP_LV8,	400000000,	0,	NULL},
	{ISP_LV9,	160000000,	0,	NULL},
	{ISP_LV10,	 13000000,	0,	NULL},
};

struct devfreq_clk_info aclk_csis0[] = {
	{ISP_LV0,	552000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV1,	552000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV2,	552000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV3,	552000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV4,	552000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV5,	552000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV6,	552000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV7,	276000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV8,	276000000,	0,	&aclk_csis0_isp_pll_list},
	{ISP_LV9,	160000000,	0,	&aclk_csis0_bus_pll_list},
	{ISP_LV10,	 13000000,	0,	&aclk_csis0_bus_pll_list},
};

struct devfreq_clk_info aclk_lite_a[] = {
	{ISP_LV0,	552000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV1,	552000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV2,	552000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV3,	552000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV4,	552000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV5,	552000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV6,	552000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV7,	276000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV8,	276000000,	0,	&aclk_lite_a_isp_pll_list},
	{ISP_LV9,	160000000,	0,	&aclk_lite_a_bus_pll_list},
	{ISP_LV10,	 13000000,	0,	&aclk_lite_a_bus_pll_list},
};

struct devfreq_clk_info aclk_3aa0[] = {
	{ISP_LV0,	552000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV1,	552000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV2,	552000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV3,	552000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV4,	400000000,	0,	&aclk_3aa0_bus_pll_list},
	{ISP_LV5,	276000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV6,	276000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV7,	276000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV8,	276000000,	0,	&aclk_3aa0_isp_pll_list},
	{ISP_LV9,	160000000,	0,	&aclk_3aa0_bus_pll_list},
	{ISP_LV10,	 13000000,	0,	&aclk_3aa0_bus_pll_list},
};

struct devfreq_clk_info aclk_csis1[] = {
	{ISP_LV0,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV1,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV2,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV3,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV4,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV5,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV6,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV7,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV8,	184000000,	0,	&aclk_csis1_isp_pll_list},
	{ISP_LV9,	160000000,	0,	&aclk_csis1_bus_pll_list},
	{ISP_LV10,	 13000000,	0,	&aclk_csis1_bus_pll_list},
};

struct devfreq_clk_info aclk_lite_b[] = {
	{ISP_LV0,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV1,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV2,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV3,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV4,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV5,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV6,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV7,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV8,	184000000,	0,	&aclk_lite_b_isp_pll_list},
	{ISP_LV9,	160000000,	0,	&aclk_lite_b_bus_pll_list},
	{ISP_LV10,	 13000000,	0,	&aclk_lite_b_bus_pll_list},
};

struct devfreq_clk_info aclk_3aa1[] = {
	{ISP_LV0,	400000000,	0,	&aclk_3aa1_bus_pll_list},
	{ISP_LV1,	400000000,	0,	&aclk_3aa1_bus_pll_list},
	{ISP_LV2,	400000000,	0,	&aclk_3aa1_bus_pll_list},
	{ISP_LV3,	400000000,	0,	&aclk_3aa1_bus_pll_list},
	{ISP_LV4,	184000000,	0,	&aclk_3aa1_isp_pll_list},
	{ISP_LV5,	184000000,	0,	&aclk_3aa1_isp_pll_list},
	{ISP_LV6,	184000000,	0,	&aclk_3aa1_isp_pll_list},
	{ISP_LV7,	184000000,	0,	&aclk_3aa1_isp_pll_list},
	{ISP_LV8,	184000000,	0,	&aclk_3aa1_isp_pll_list},
	{ISP_LV9,	160000000,	0,	&aclk_3aa1_bus_pll_list},
	{ISP_LV10,	 13000000,	0,	&aclk_3aa1_bus_pll_list},
};

struct devfreq_clk_info aclk_lite_d[] = {
	{ISP_LV0,	138000000,	0,	NULL},
	{ISP_LV1,	138000000,	0,	NULL},
	{ISP_LV2,	138000000,	0,	NULL},
	{ISP_LV3,	138000000,	0,	NULL},
	{ISP_LV4,	138000000,	0,	NULL},
	{ISP_LV5,	138000000,	0,	NULL},
	{ISP_LV6,	138000000,	0,	NULL},
	{ISP_LV7,	138000000,	0,	NULL},
	{ISP_LV8,	138000000,	0,	NULL},
	{ISP_LV9,	138000000,	0,	NULL},
	{ISP_LV10,	  9000000,	0,	NULL},
};

struct devfreq_clk_info sclk_pixel_init_552[] = {
	{ISP_LV0,	69000000,	0,	NULL},
	{ISP_LV1,	69000000,	0,	NULL},
	{ISP_LV2,	69000000,	0,	NULL},
	{ISP_LV3,	69000000,	0,	NULL},
	{ISP_LV4,	69000000,	0,	NULL},
	{ISP_LV5,	69000000,	0,	NULL},
	{ISP_LV6,	69000000,	0,	NULL},
	{ISP_LV7,	69000000,	0,	NULL},
	{ISP_LV8,	69000000,	0,	NULL},
	{ISP_LV9,	18000000,	0,	NULL},
	{ISP_LV10,	 9000000,	0,	NULL},
};

struct devfreq_clk_info sclk_pixel_333[] = {
	{ISP_LV0,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV1,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV2,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV3,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV4,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV5,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV6,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV7,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV8,	42000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV9,	21000000,	0,	&sclk_pixelasync_lite_c_list},
	{ISP_LV10,	10000000,	0,	&sclk_pixelasync_lite_c_list},
};

struct devfreq_clk_info sclk_lite_freecnt_c[] = {
	{ISP_LV0,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV1,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV2,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV3,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV4,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV5,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV6,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV7,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV8,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV9,	0,	0,	&sclk_lite_freecnt_c_list},
	{ISP_LV10,	0,	0,	&sclk_lite_freecnt_c_list},
};

/* ISP & CAM1 */
struct devfreq_clk_info aclk_cam1_552[] = {
	{ISP_LV0,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV1,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV2,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV3,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
	{ISP_LV4,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
	{ISP_LV5,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV6,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV7,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV8,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV9,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{ISP_LV10,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
};

struct devfreq_clk_info aclk_cam1_400[] = {
	{ISP_LV0,	400000000,	0,	NULL},
	{ISP_LV1,	400000000,	0,	NULL},
	{ISP_LV2,	400000000,	0,	NULL},
	{ISP_LV3,	400000000,	0,	NULL},
	{ISP_LV4,	400000000,	0,	NULL},
	{ISP_LV5,	400000000,	0,	NULL},
	{ISP_LV6,	400000000,	0,	NULL},
	{ISP_LV7,	400000000,	0,	NULL},
	{ISP_LV8,	400000000,	0,	NULL},
	{ISP_LV9,	400000000,	0,	NULL},
	{ISP_LV10,	267000000,	0,	NULL},
};

struct devfreq_clk_info aclk_cam1_333[] = {
	{ISP_LV0,	334000000,	0,	NULL},
	{ISP_LV1,	334000000,	0,	NULL},
	{ISP_LV2,	334000000,	0,	NULL},
	{ISP_LV3,	334000000,	0,	NULL},
	{ISP_LV4,	334000000,	0,	NULL},
	{ISP_LV5,	334000000,	0,	NULL},
	{ISP_LV6,	334000000,	0,	NULL},
	{ISP_LV7,	334000000,	0,	NULL},
	{ISP_LV8,	334000000,	0,	NULL},
	{ISP_LV9,	334000000,	0,	NULL},
	{ISP_LV10,	167000000,	0,	NULL},
};

struct devfreq_clk_info pclk_cam1_83[] = {
	{ISP_LV0,	84000000,	0,	NULL},
	{ISP_LV1,	84000000,	0,	NULL},
	{ISP_LV2,	84000000,	0,	NULL},
	{ISP_LV3,	84000000,	0,	NULL},
	{ISP_LV4,	84000000,	0,	NULL},
	{ISP_LV5,	84000000,	0,	NULL},
	{ISP_LV6,	84000000,	0,	NULL},
	{ISP_LV7,	84000000,	0,	NULL},
	{ISP_LV8,	84000000,	0,	NULL},
	{ISP_LV9,	84000000,	0,	NULL},
	{ISP_LV10,	84000000,	0,	NULL},
};

struct devfreq_clk_info aclk_fd_400[] = {
	{ISP_LV0,	400000000,	0,	&aclk_fd_400_bus_pll_list},
	{ISP_LV1,	334000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV2,	167000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV3,	112000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV4,	112000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV5,	334000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV6,	167000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV7,	334000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV8,	167000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV9,	167000000,	0,	&aclk_fd_400_mfc_pll_list},
	{ISP_LV10,	 84000000,	0,	&aclk_fd_400_mfc_pll_list},
};

struct devfreq_clk_info aclk_csis2_333[] = {
	{ISP_LV0,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV1,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV2,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV3,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV4,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV5,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV6,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV7,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV8,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV9,	 42000000,	0,	&mux_aclk_csis2_list},
	{ISP_LV10,	 21000000,	0,	&mux_aclk_csis2_list},
};

struct devfreq_clk_info aclk_lite_c[] = {
	{ISP_LV0,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV1,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV2,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV3,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV4,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV5,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV6,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV7,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV8,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV9,	 42000000,	0,	&mux_aclk_lite_c_list},
	{ISP_LV10,	 21000000,	0,	&mux_aclk_lite_c_list},
};

struct devfreq_clk_info aclk_isp_400[] = {
	{ISP_LV0,	400000000,	0,	&aclk_isp_400_bus_pll_list},
	{ISP_LV1,	334000000,	0,	&aclk_isp_400_mfc_pll_list},
	{ISP_LV2,	167000000,	0,	&aclk_isp_400_mfc_pll_list},
	{ISP_LV3,	112000000,	0,	&aclk_isp_400_mfc_pll_list},
	{ISP_LV4,	112000000,	0,	&aclk_isp_400_mfc_pll_list},
	{ISP_LV5,	267000000,	0,	&aclk_isp_400_bus_pll_list},
	{ISP_LV6,	167000000,	0,	&aclk_isp_400_mfc_pll_list},
	{ISP_LV7,	267000000,	0,	&aclk_isp_400_bus_pll_list},
	{ISP_LV8,	167000000,	0,	&aclk_isp_400_mfc_pll_list},
	{ISP_LV9,	167000000,	0,	&aclk_isp_400_mfc_pll_list},
	{ISP_LV10,	 84000000,	0,	&aclk_isp_400_mfc_pll_list},
};

struct devfreq_clk_info aclk_isp_dis_400[] = {
	{ISP_LV0,	400000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{ISP_LV1,	334000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{ISP_LV2,	167000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{ISP_LV3,	112000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{ISP_LV4,	112000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{ISP_LV5,	267000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{ISP_LV6,	167000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{ISP_LV7,	267000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{ISP_LV8,	167000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{ISP_LV9,	167000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{ISP_LV10,	 84000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_isp_pm_domain[] = {
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam0",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-isp",},
	{"pd-isp",},
};
#endif

struct devfreq_clk_info *devfreq_clk_isp_info_list[] = {
	aclk_cam0_552,
	aclk_cam0_400,
	aclk_cam0_333,
	aclk_cam0_bus_400,
	aclk_csis0,
	aclk_lite_a,
	aclk_3aa0,
	aclk_csis1,
	aclk_lite_b,
	aclk_3aa1,
	aclk_lite_d,
	sclk_pixel_init_552,
	sclk_pixel_333,
	sclk_lite_freecnt_c,
	aclk_cam1_552,
	aclk_cam1_400,
	aclk_cam1_333,
	pclk_cam1_83,
	aclk_fd_400,
	aclk_csis2_333,
	aclk_lite_c,
	aclk_isp_400,
	aclk_isp_dis_400,
};

enum devfreq_isp_clk devfreq_clk_isp_info_idx[] = {
	DOUT_ACLK_CAM0_552,
	DOUT_ACLK_CAM0_400,
	DOUT_ACLK_CAM0_333,
	DOUT_ACLK_CAM0_BUS_400,
	DOUT_ACLK_CSIS0,
	DOUT_ACLK_LITE_A,
	DOUT_ACLK_3AA0,
	DOUT_ACLK_CSIS1,
	DOUT_ACLK_LITE_B,
	DOUT_ACLK_3AA1,
	DOUT_ACLK_LITE_D,
	DOUT_SCLK_PIXEL_INIT_552,
	DOUT_SCLK_PIXEL_333,
	MOUT_SCLK_LITE_FREECNT_C,
	DOUT_ACLK_CAM1_552,
	DOUT_ACLK_CAM1_400,
	DOUT_ACLK_CAM1_333,
	DOUT_PCLK_CAM1_83,
	DOUT_ACLK_FD_400,
	DOUT_ACLK_CSIS2_333,
	DOUT_ACLK_LITE_C,
	DOUT_ACLK_ISP_400,
	DOUT_ACLK_ISP_DIS_400,
};

static struct devfreq_data_isp *data_isp;
#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_isp_set_clk(struct devfreq_data_isp *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info[target_idx].states;

	if (clk_get_rate(clk) < clk_info[target_idx].freq) {
		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_isp_clk[clk_states->state[i].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	} else {
		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);

		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_isp_clk[clk_states->state[i].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	}

	return 0;
}

void exynos5_isp_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on ||
		!data_isp->use_dvfs)
		return;

	mutex_lock(&data_isp->lock);
	cur_freq_idx = exynos5_devfreq_get_idx(devfreq_isp_opp_list,
			ARRAY_SIZE(devfreq_isp_opp_list),
			data_isp->devfreq->previous_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_isp->lock);
		pr_err("DEVFREQ(INT) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_pm_domain); ++i) {
		if (devfreq_isp_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_isp_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos5_devfreq_isp_set_clk(data_isp,
				cur_freq_idx,
				devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk,
				devfreq_clk_isp_info_list[i]);
	}
	mutex_unlock(&data_isp->lock);
}
#endif

static int exynos5_devfreq_isp_set_freq(struct devfreq_data_isp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_info *old_clk_info;
	struct devfreq_clk_states *clk_states;
	struct devfreq_clk_states *old_clk_states;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_isp_info_list); ++i) {
			clk_info = &devfreq_clk_isp_info_list[i][target_idx];
			clk_states = clk_info->states;
			old_clk_info = &devfreq_clk_isp_info_list[i][old_idx];

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_isp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_info->freq >= old_clk_info->freq) {
				if (clk_states) {
					for (j = 0; j < clk_states->state_count; ++j) {
						clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
								devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
					}
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
				pr_debug("ISP clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk));
			}

			if (clk_info->freq < old_clk_info->freq) {
				if (clk_states) {
					for (j = 0; j < clk_states->state_count; ++j) {
						clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
								devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
					}
				}

				if (clk_info->freq != 0) {
					clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
					pr_debug("ISP clk name: %s, set_rate: %lu, get_rate: %lu\n",
							devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk_name,
							clk_info->freq, clk_get_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk));
				}
			}

#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_isp_info_list); ++i) {
			clk_info = &devfreq_clk_isp_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_isp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_info->freq != 0) {
				/* it is just code to prevent 3aa0 overflow, when trasition ISP_LV3(552) to ISP_LV4(400),
				   it apply mux value after divider. so it assert 276M for a while because of clk_set_rate().
				   it caused 3aa0 overflow.
				   */
				if (!strcmp(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk_name, "dout_aclk_3aa0")) {
					old_clk_info = &devfreq_clk_isp_info_list[i][old_idx];
					old_clk_states = old_clk_info->states;
					if ((clk_get_rate(devfreq_isp_clk[clk_states->state[0].parent_clk_idx].clk) >=
								clk_get_rate(devfreq_isp_clk[old_clk_states->state[0].parent_clk_idx].clk)))
						clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
				} else {
					clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
				}
			}

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
				pr_debug("ISP clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk));
			}
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}

	return 0;
}

#define CONSTRAINT_VOLT		900000
extern int exynos5_int_check_voltage_constraint(unsigned long isp_voltage);

static int exynos5_devfreq_isp_set_volt(struct devfreq_data_isp *data,
		unsigned long volt,
		unsigned long volt_range,
		bool tolower)
{
	if (data->old_volt == volt)
		goto out;
#ifdef INT_ISP_CONSTRAINT
	if (!tolower && (volt >= CONSTRAINT_VOLT))
		exynos5_int_check_voltage_constraint(volt);
#endif
	regulator_set_voltage(data->vdd_isp, volt, volt_range);
	data->old_volt = volt;
#ifdef INT_ISP_CONSTRAINT
	if (tolower && (volt >= CONSTRAINT_VOLT))
		exynos5_int_check_voltage_constraint(volt);
#endif
out:
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_isp_init_pm_domain(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	int i;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;

		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd = platform_get_drvdata(pdev);

		for (i = 0; i < ARRAY_SIZE(devfreq_isp_pm_domain); ++i) {
			if (devfreq_isp_pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(devfreq_isp_pm_domain[i].pm_domain_name, pd->genpd.name))
				devfreq_isp_pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}
#endif

static int exynos5_devfreq_isp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_clk); ++i) {
		devfreq_isp_clk[i].clk = clk_get(NULL, devfreq_isp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_isp_clk[i].clk)) {
			pr_err("DEVFREQ(ISP) : %s can't get clock\n", devfreq_isp_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("ISP clk name: %s, rate: %lu\n", devfreq_isp_clk[i].clk_name, clk_get_rate(devfreq_isp_clk[i].clk));
	}

	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos5_devfreq_isp_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_isp *data = container_of(nb, struct devfreq_data_isp, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_isp_thermal_qos))
			pm_qos_update_request(&min_isp_thermal_qos, data->initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_isp);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_isp, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_isp);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_isp, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_isp_thermal_qos))
			pm_qos_update_request(&min_isp_thermal_qos, data->default_qos);
	}

	return NOTIFY_OK;
}
#endif

int exynos5433_devfreq_isp_init(struct devfreq_data_isp *data)
{
	int ret = 0;
	data_isp = data;
	data->max_state = ISP_LV_COUNT;
	if (exynos5_devfreq_isp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos5_devfreq_isp_init_pm_domain()) {
		ret = -EINVAL;
		goto err_data;
	}
#endif
	data->isp_set_freq = exynos5_devfreq_isp_set_freq;
	data->isp_set_volt = exynos5_devfreq_isp_set_volt;
err_data:
	return ret;
}
int exynos5433_devfreq_isp_deinit(struct devfreq_data_isp *data)
{
	return 0;
}
/* end of ISP related function */

/* ========== 3. MIF related function */
#define TRAFFIC_BYTES_HD_32BIT_60FPS		(1280*720*4*60)
#define TRAFFIC_BYTES_FHD_32BIT_60FPS		(1920*1080*4*60)
#define TRAFFIC_BYTES_WQHD_32BIT_60FPS		(2560*1440*4*60)
#define TRAFFIC_BYTES_WQXGA_32BIT_60FPS		(2560*1600*4*60)

extern struct pm_qos_request exynos5_mif_bts_qos;

enum devfreq_mif_idx {
	MIF_LV0,
	MIF_LV1,
	MIF_LV2,
	MIF_LV3,
	MIF_LV4,
	MIF_LV5,
	MIF_LV6,
	MIF_LV7,
	MIF_LV8,
	MIF_LV9,
	MIF_LV_COUNT,
};

enum devfreq_mif_clk {
	FOUT_MEM0_PLL,
	FOUT_MEM1_PLL,
	FOUT_MFC_PLL,
	FOUT_BUS_PLL,
	MOUT_MEM0_PLL,
	MOUT_MEM0_PLL_DIV2,
	MOUT_MEM1_PLL,
	MOUT_MEM1_PLL_DIV2,
	MOUT_MFC_PLL,
	MOUT_MFC_PLL_DIV2,
	MOUT_BUS_PLL,
	MOUT_BUS_PLL_DIV2,
	MOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_266,
	DOUT_ACLK_MIF_200,
	DOUT_MIF_PRE,
	MOUT_ACLK_MIFNM_200,
	DOUT_ACLK_MIFNM_200,
	DOUT_ACLK_MIFND_133,
	DOUT_ACLK_MIF_133,
	DOUT_ACLK_CPIF_200,
	DOUT_CLK2X_PHY,
	DOUT_ACLK_DREX0,
	DOUT_ACLK_DREX1,
	DOUT_SCLK_HPM_MIF,
	MIF_CLK_COUNT,
};

struct devfreq_clk_list devfreq_mif_clk[MIF_CLK_COUNT] = {
	{"fout_mem0_pll",},
	{"fout_mem1_pll",},
	{"fout_mfc_pll",},
	{"fout_bus_pll",},
	{"mout_mem0_pll",},
	{"mout_mem0_pll_div2",},
	{"mout_mem1_pll",},
	{"mout_mem1_pll_div2",},
	{"mout_mfc_pll",},
	{"mout_mfc_pll_div2",},
	{"mout_bus_pll",},
	{"mout_bus_pll_div2",},
	{"mout_aclk_mif_400",},
	{"dout_aclk_mif_400",},
	{"dout_aclk_mif_266",},
	{"dout_aclk_mif_200",},
	{"dout_mif_pre",},
	{"mout_aclk_mifnm_200",},
	{"dout_aclk_mifnm_200",},
	{"dout_aclk_mifnd_133",},
	{"dout_aclk_mif_133",},
	{"dout_aclk_cpif_200",},
	{"dout_clk2x_phy",},
	{"dout_aclk_drex0",},
	{"dout_aclk_drex1",},
	{"dout_sclk_hpm_mif",},
};

struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{MIF_LV0,	825000,	1050000},
	{MIF_LV1,	667000,	1000000},
	{MIF_LV2,	543000,	 975000},
	{MIF_LV3,	413000,	 950000},
	{MIF_LV4,	272000,	 950000},
	{MIF_LV5,	222000,	 950000},
	{MIF_LV6,	167000,	 925000},
	{MIF_LV7,	136000,	 900000},
	{MIF_LV8,	109000,	 875000},
	{MIF_LV9,	 78000,	 875000},
};

struct devfreq_clk_value aclk_clk2x_825[] = {
	{0x1000, (0x1 << 20), ((0xF << 28) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_633[] = {
	{0x1000, ((0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_543[] = {
	{0x1000, 0, ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_413[] = {
	{0x1000, ((0x1 << 28) | (0x1 << 20)), ((0xF << 28) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_275[] = {
	{0x1000, (0x1 << 28), ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_206[] = {
	{0x1000, ((0x2 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_165[] = {
	{0x1000, ((0x3 << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12)), (0xF << 28)},
};

struct devfreq_clk_value aclk_clk2x_138[] = {
	{0x1000, (0x3 << 28), ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_103[] = {
	{0x1000, (0x4 << 28), ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value aclk_clk2x_78[] = {
	{0x1000, (0x6 << 28), ((0xF << 28) | (0x1 << 20) | (0x1 << 16) | (0x1 << 12))},
};

struct devfreq_clk_value *aclk_clk2x_list[] = {
	aclk_clk2x_825,
	aclk_clk2x_633,
	aclk_clk2x_543,
	aclk_clk2x_413,
	aclk_clk2x_275,
	aclk_clk2x_206,
	aclk_clk2x_165,
	aclk_clk2x_138,
	aclk_clk2x_103,
	aclk_clk2x_78,
};

struct devfreq_clk_state aclk_mif_400_mem1_pll[] = {
	{MOUT_ACLK_MIF_400,     MOUT_MEM1_PLL_DIV2},
};

struct devfreq_clk_state aclk_mifnm_200_bus_pll[] = {
	{DOUT_MIF_PRE,		MOUT_ACLK_MIFNM_200},
};

struct devfreq_clk_states aclk_mif_400_mem1_pll_list = {
	.state = aclk_mif_400_mem1_pll,
	.state_count = ARRAY_SIZE(aclk_mif_400_mem1_pll),
};

struct devfreq_clk_states aclk_mifnm_200_bus_pll_list = {
	.state = aclk_mifnm_200_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mifnm_200_bus_pll),
};

struct devfreq_clk_info aclk_mif_400[] = {
	{MIF_LV0,	413000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV1,	275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV2,	275000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV3,	207000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV4,	207000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV5,	207000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV6,	165000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV7,	165000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV8,	138000000,      0,      &aclk_mif_400_mem1_pll_list},
	{MIF_LV9,	138000000,      0,      &aclk_mif_400_mem1_pll_list},
};

struct devfreq_clk_info aclk_mif_266[] = {
	{MIF_LV0,	267000000,      0,      NULL},
	{MIF_LV1,	200000000,      0,      NULL},
	{MIF_LV2,	200000000,      0,      NULL},
	{MIF_LV3,	160000000,      0,      NULL},
	{MIF_LV4,	134000000,      0,      NULL},
	{MIF_LV5,	134000000,      0,      NULL},
	{MIF_LV6,	100000000,      0,      NULL},
	{MIF_LV7,	100000000,      0,      NULL},
	{MIF_LV8,	100000000,      0,      NULL},
	{MIF_LV9,	100000000,      0,      NULL},
};

struct devfreq_clk_info aclk_mif_200[] = {
	{MIF_LV0,	207000000,      0,      NULL},
	{MIF_LV1,	138000000,      0,      NULL},
	{MIF_LV2,	138000000,      0,      NULL},
	{MIF_LV3,	104000000,      0,      NULL},
	{MIF_LV4,	104000000,      0,      NULL},
	{MIF_LV5,	104000000,      0,      NULL},
	{MIF_LV6,	 83000000,      0,      NULL},
	{MIF_LV7,	 83000000,      0,      NULL},
	{MIF_LV8,	 69000000,      0,      NULL},
	{MIF_LV9,	 69000000,      0,      NULL},
};

struct devfreq_clk_info mif_pre[] = {
	{MIF_LV0,	400000000,      0,      NULL},
	{MIF_LV1,	400000000,      0,      NULL},
	{MIF_LV2,	400000000,      0,      NULL},
	{MIF_LV3,	400000000,      0,      NULL},
	{MIF_LV4,	400000000,      0,      NULL},
	{MIF_LV5,	400000000,      0,      NULL},
	{MIF_LV6,	400000000,      0,      NULL},
	{MIF_LV7,	400000000,      0,      NULL},
	{MIF_LV8,	400000000,      0,      NULL},
	{MIF_LV9,	400000000,      0,      NULL},
};

struct devfreq_clk_info aclk_mifnm[] = {
	{MIF_LV0,	134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV1,	134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV2,	134000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV3,	100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV4,	100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV5,	100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV6,	100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV7,	100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV8,	100000000,      0,      &aclk_mifnm_200_bus_pll_list},
	{MIF_LV9,	100000000,      0,      &aclk_mifnm_200_bus_pll_list},
};

struct devfreq_clk_info aclk_mifnd[] = {
	{MIF_LV0,	 80000000,      0,      NULL},
	{MIF_LV1,	 80000000,      0,      NULL},
	{MIF_LV2,	 80000000,      0,      NULL},
	{MIF_LV3,	 67000000,      0,      NULL},
	{MIF_LV4,	 67000000,      0,      NULL},
	{MIF_LV5,	 67000000,      0,      NULL},
	{MIF_LV6,	 67000000,      0,      NULL},
	{MIF_LV7,	 67000000,      0,      NULL},
	{MIF_LV8,	 67000000,      0,      NULL},
	{MIF_LV9,	 67000000,      0,      NULL},
};

struct devfreq_clk_info aclk_mif_133[] = {
	{MIF_LV0,	 80000000,      0,      NULL},
	{MIF_LV1,	 67000000,      0,      NULL},
	{MIF_LV2,	 67000000,      0,      NULL},
	{MIF_LV3,	 67000000,      0,      NULL},
	{MIF_LV4,	 67000000,      0,      NULL},
	{MIF_LV5,	 50000000,      0,      NULL},
	{MIF_LV6,	 50000000,      0,      NULL},
	{MIF_LV7,	 50000000,      0,      NULL},
	{MIF_LV8,	 50000000,      0,      NULL},
	{MIF_LV9,	 50000000,      0,      NULL},
};

struct devfreq_clk_info aclk_cpif_200[] = {
	{MIF_LV0,	100000000,      0,      NULL},
	{MIF_LV1,	100000000,      0,      NULL},
	{MIF_LV2,	100000000,      0,      NULL},
	{MIF_LV3,	100000000,      0,      NULL},
	{MIF_LV4,	100000000,      0,      NULL},
	{MIF_LV5,	100000000,      0,      NULL},
	{MIF_LV6,	100000000,      0,      NULL},
	{MIF_LV7,	100000000,      0,      NULL},
	{MIF_LV8,	100000000,      0,      NULL},
	{MIF_LV9,	100000000,      0,      NULL},
};

struct devfreq_clk_info sclk_hpm_mif[] = {
	{MIF_LV0,	207000000,      0,      NULL},
	{MIF_LV1,	167000000,      0,      NULL},
	{MIF_LV2,	136000000,      0,      NULL},
	{MIF_LV3,	104000000,      0,      NULL},
	{MIF_LV4,	 69000000,      0,      NULL},
	{MIF_LV5,	 56000000,      0,      NULL},
	{MIF_LV6,	 42000000,      0,      NULL},
	{MIF_LV7,	 35000000,      0,      NULL},
	{MIF_LV8,	 27000000,      0,      NULL},
	{MIF_LV9,	 19000000,      0,      NULL},
};

struct devfreq_clk_info *devfreq_clk_mif_info_list[] = {
	aclk_mif_400,
	aclk_mif_266,
	aclk_mif_200,
	mif_pre,
	aclk_mifnm,
	aclk_mifnd,
	aclk_mif_133,
	aclk_cpif_200,
	sclk_hpm_mif,
};

enum devfreq_mif_clk devfreq_clk_mif_info_idx[] = {
	DOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_266,
	DOUT_ACLK_MIF_200,
	DOUT_MIF_PRE,
	DOUT_ACLK_MIFNM_200,
	DOUT_ACLK_MIFND_133,
	DOUT_ACLK_MIF_133,
	DOUT_ACLK_CPIF_200,
	DOUT_SCLK_HPM_MIF,
};

struct devfreq_mif_timing_parameter {
	unsigned int timing_row;
	unsigned int timing_data;
	unsigned int timing_power;
	unsigned int rd_fetch;
	unsigned int timing_rfcpb;
	unsigned int dvfs_con1;
	unsigned int mif_drex_mr_data[4];
	unsigned int dvfs_offset;
};

struct devfreq_mif_timing_parameter dmc_timing_parameter_2gb[] = {
	{	/* 825Mhz */
		.timing_row	= 0x365A9713,
		.timing_data	= 0x4740085E,
		.timing_power	= 0x543A0446,
		.rd_fetch	= 0x00000003,
		.timing_rfcpb	= 0x00001919,
		.dvfs_con1	= 0x0C0C2121,
		.mif_drex_mr_data = {
			[0]	= 0x00000870,
			[1]	= 0x00100870,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 0,
	}, {	/* 633Mhz */
		.timing_row	= 0x2A48758F,
		.timing_data	= 0x3530064A,
		.timing_power	= 0x402D0335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001313,
		.dvfs_con1	= 0x0A0A2121,
		.mif_drex_mr_data = {
			[0]	= 0x00000860,
			[1]	= 0x00100860,
			[2]	= 0x0000040C,
			[3]	= 0x0010040C,
		},
		.dvfs_offset	= 0,
	}, {	/* 543Mhz */
		.timing_row	= 0x244764CD,
		.timing_data	= 0x35300549,
		.timing_power	= 0x38270335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001111,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000078C,
			[3]	= 0x0010078C,
		},
		.dvfs_offset	= 0,
	}, {	/* 413Mhz */
		.timing_row	= 0x1B35538A,
		.timing_data	= 0x24200539,
		.timing_power	= 0x2C1D0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000D0D,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 15,
	}, {	/* 275Mhz */
		.timing_row	= 0x12244287,
		.timing_data	= 0x23200529,
		.timing_power	= 0x1C140225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000909,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 30,
	}, {	/* 206Mhz */
		.timing_row	= 0x112331C6,
		.timing_data	= 0x23200529,
		.timing_power	= 0x140E0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000707,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 30,
	}, {	/* 165Mhz */
		.timing_row	= 0x11223185,
		.timing_data	= 0x23200529,
		.timing_power	= 0x140C0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000505,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 40,
	}, {	/* 138Mhz */
		.timing_row	= 0x11222144,
		.timing_data	= 0x23200529,
		.timing_power	= 0x100A0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000505,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 40,
	}, {	/* 103Mhz */
		.timing_row	= 0x11222103,
		.timing_data	= 0x23200529,
		.timing_power	= 0x10080225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000303,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 40,
	},
};

enum FW_MIF_DVFS_LEVELS {
	FW_L0,	/* 825 */
	FW_L1,	/* 633 */
	FW_L2,	/* 543 */
	FW_L3,	/* 413 */
	FW_L4,	/* 272 */
	FW_L5,	/* 211 */
	FW_L6,	/* 158 */
	FW_L7,	/* 136 */
	FW_L8,	/* 109 */
	FW_L9,	/*  78 */

	CNT_FW_MIF_LEVELS,
};

struct devfreq_mif_timing_parameter dmc_timing_parameter_3gb[CNT_FW_MIF_LEVELS];

static DEFINE_MUTEX(media_mutex);
static unsigned int media_enabled_fimc_lite;
static unsigned int media_enabled_gscl_local;
static unsigned int media_enabled_tv;
static unsigned int media_num_mixer_layer;
static unsigned int media_num_decon_layer;
static unsigned int enabled_ud_encode;
static unsigned int enabled_ud_decode;
static enum devfreq_media_resolution media_resolution;
static unsigned int media_resolution_bandwidth;

static unsigned int (*timeout_table)[2];
static unsigned int wqhd_tv_window5;

struct devfreq_distriction_level {
	int mif_level;
	int disp_level;
	int int_level;
};

struct devfreq_distriction_level distriction_fullhd[] = {
	{MIF_LV9,       DISP_LV3},                      /*  78000 or  83000 */
	{MIF_LV8,       DISP_LV3},                      /* 103000 or 111000 */
	{MIF_LV7,       DISP_LV3},                      /* 138000 or 133000 */
	{MIF_LV6,       DISP_LV2},                      /* 165000 or 167000 */
	{MIF_LV5,       DISP_LV2},                      /* 206000 or 222000 */
	{MIF_LV4,       DISP_LV2},                      /* 275000 or 334000 */
	{MIF_LV0,       DISP_LV0},                      /* 825000 or 921000 */
};

unsigned int timeout_fullhd[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_fullhd_gscl[] = {
	{MIF_LV6,	DISP_LV2},
	{MIF_LV5,	DISP_LV2},
	{MIF_LV5,	DISP_LV2},
	{MIF_LV4,	DISP_LV2},
	{MIF_LV4,	DISP_LV2},
	{MIF_LV0,	DISP_LV3},
	{MIF_LV0,	DISP_LV0},
};

unsigned int timeout_fullhd_gscl[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_fullhd_tv[] = {
	{MIF_LV4,	DISP_LV1},
	{MIF_LV4,	DISP_LV1},
	{MIF_LV4,	DISP_LV1},
	{MIF_LV4,	DISP_LV1},
	{MIF_LV4,	DISP_LV1},
	{MIF_LV3,	DISP_LV1},
	{MIF_LV0,	DISP_LV0},
};

unsigned int timeout_fullhd_tv[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};


struct devfreq_distriction_level distriction_fullhd_camera[] = {
	{MIF_LV3,	DISP_LV3},
	{MIF_LV3,	DISP_LV3},
	{MIF_LV3,	DISP_LV3},
	{MIF_LV3,	DISP_LV2},
	{MIF_LV3,	DISP_LV2},
	{MIF_LV3,	DISP_LV2},
	{MIF_LV0,	DISP_LV0},
};

unsigned int timeout_fullhd_camera[][2] = {
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd[] = {
	{MIF_LV9,   	DISP_LV3,	INT_LV6},
	{MIF_LV7,	DISP_LV2,	INT_LV6},
	{MIF_LV4,	DISP_LV1,	INT_LV4},
	{MIF_LV3,	DISP_LV0,	INT_LV2},
	{MIF_LV3,   	DISP_LV0,	INT_LV0},
	{MIF_LV2,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
};

unsigned int timeout_wqhd[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd_gscl[] = {
	{MIF_LV7,	DISP_LV3,	INT_LV6},
	{MIF_LV7,	DISP_LV2,	INT_LV4},
	{MIF_LV4,	DISP_LV1,	INT_LV2},
	{MIF_LV3,   	DISP_LV0,	INT_LV0},
	{MIF_LV3,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
};

unsigned int timeout_wqhd_gscl[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd_tv[] = {
	{MIF_LV4,	DISP_LV1,	INT_LV4},
	{MIF_LV4,	DISP_LV1,	INT_LV4},
	{MIF_LV3,	DISP_LV0,	INT_LV2},
	{MIF_LV3,	DISP_LV0,	INT_LV0},
	{MIF_LV2,	DISP_LV0,	INT_LV0},
	{MIF_LV1,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
};

unsigned int timeout_wqhd_tv[][2] = {
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00400040,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
};

struct devfreq_distriction_level distriction_wqhd_camera[] = {
	{MIF_LV2,	DISP_LV2,	INT_LV6},
	{MIF_LV2,	DISP_LV2,	INT_LV6},
	{MIF_LV2,	DISP_LV1,	INT_LV4},
	{MIF_LV1,	DISP_LV1,	INT_LV2},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
};

struct devfreq_distriction_level distriction_wqxga_camera[] = {
	{MIF_LV2,	DISP_LV2,	INT_LV6},
	{MIF_LV2,	DISP_LV2,	INT_LV6},
	{MIF_LV2,	DISP_LV1,	INT_LV4},
	{MIF_LV1,	DISP_LV1,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
	{MIF_LV0,	DISP_LV0,	INT_LV0},
};

unsigned int timeout_wqhd_camera[][2] = {
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
	{0x00000000,	0x000000FF},
};

unsigned int timeout_wqhd_ud_encode[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
};

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x000B005D,
	RATE_HALF = 0x0005002E,
	RATE_QUARTER = 0x00030017,
};

enum devfreq_mif_thermal_channel {
	THERMAL_CHANNEL0,
	THERMAL_CHANNEL1,
};

static struct workqueue_struct *devfreq_mif_thermal_wq_ch0;
static struct workqueue_struct *devfreq_mif_thermal_wq_ch1;
struct devfreq_thermal_work devfreq_mif_ch0_work = {
	.channel = THERMAL_CHANNEL0,
	.polling_period = 1000,
};
struct devfreq_thermal_work devfreq_mif_ch1_work = {
	.channel = THERMAL_CHANNEL1,
	.polling_period = 1000,
};
struct devfreq_data_mif *data_mif;

static void exynos5_devfreq_waiting_pause(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	while ((__raw_readl(data->base_mif + 0x1008) & 0x00070000) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait pause completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static void exynos5_devfreq_waiting_mux(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	while ((__raw_readl(data->base_mif + 0x0404) & 0x04440000) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait mux completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
	timeout = 1000;
	while ((__raw_readl(data->base_mif + 0x0704) & 0x00000010) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait divider completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static int exynos5_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
	unsigned int tmp;

	if (target_idx < old_idx) {
		tmp = __raw_readl(data->base_mif + aclk_clk2x_list[target_idx]->reg);
		tmp &= ~(aclk_clk2x_list[target_idx]->clr_value);
		tmp |= aclk_clk2x_list[target_idx]->set_value;
		__raw_writel(tmp, data->base_mif + aclk_clk2x_list[target_idx]->reg);

		exynos5_devfreq_waiting_pause(data);
		exynos5_devfreq_waiting_mux(data);

		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
			clk_info = &devfreq_clk_mif_info_list[i][target_idx];
			clk_states = clk_info->states;
			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
				pr_debug("MIF clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk));
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
			clk_info = &devfreq_clk_mif_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
				pr_debug("MIF clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk));
			}
		}

		tmp = __raw_readl(data->base_mif + aclk_clk2x_list[target_idx]->reg);
		tmp &= ~(aclk_clk2x_list[target_idx]->clr_value);
		tmp |= aclk_clk2x_list[target_idx]->set_value;
		__raw_writel(tmp, data->base_mif + aclk_clk2x_list[target_idx]->reg);

		exynos5_devfreq_waiting_pause(data);
		exynos5_devfreq_waiting_mux(data);
	}

	g_mif_level = target_idx;
	return 0;
}

struct devfreq_mif_timing_parameter dmc_timing_parameter_default[] = {
	{	/* 825Mhz */
		.timing_row	= 0x575A9713,
		.timing_data	= 0x4740085E,
		.timing_power	= 0x545B0446,
		.rd_fetch	= 0x00000003,
		.timing_rfcpb	= 0x00002626,
		.dvfs_con1	= 0x0E0E2121,
		.mif_drex_mr_data = {
			[0]	= 0x00000870,
			[1]	= 0x00100870,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 0,
	}, {	/* 633Mhz */
		.timing_row	= 0x4348758F,
		.timing_data	= 0x3530064A,
		.timing_power	= 0x40460335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001D1D,
		.dvfs_con1	= 0x0A0A2121,
		.mif_drex_mr_data = {
			[0]	= 0x00000860,
			[1]	= 0x00100860,
			[2]	= 0x0000040C,
			[3]	= 0x0010040C,
		},
		.dvfs_offset	= 0,
	}, {	/* 543Mhz */
		.timing_row	= 0x3A4764CD,
		.timing_data	= 0x35300549,
		.timing_power	= 0x383C0335,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001919,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000078C,
			[3]	= 0x0010078C,
		},
		.dvfs_offset	= 0,
	}, {	/* 413Mhz */
		.timing_row	= 0x2C35538A,
		.timing_data	= 0x24200539,
		.timing_power	= 0x2C2E0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00001313,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 15,
	}, {	/* 275Mhz */
		.timing_row	= 0x1D244287,
		.timing_data	= 0x23200529,
		.timing_power	= 0x1C1F0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000D0D,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 30,
	}, {	/* 206Mhz */
		.timing_row	= 0x162331C6,
		.timing_data	= 0x23200529,
		.timing_power	= 0x18170225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000A0A,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 30,
	}, {	/* 165Mhz */
		.timing_row	= 0x12223185,
		.timing_data	= 0x23200529,
		.timing_power	= 0x14130225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000808,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 40,
	}, {	/* 138Mhz */
		.timing_row	= 0x11222144,
		.timing_data	= 0x23200529,
		.timing_power	= 0x10100225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000707,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 40,
	}, {	/* 103Mhz */
		.timing_row	= 0x11222103,
		.timing_data	= 0x23200529,
		.timing_power	= 0x100C0225,
		.rd_fetch	= 0x00000002,
		.timing_rfcpb	= 0x00000505,
		.dvfs_con1	= 0x09092121,
		.mif_drex_mr_data = {
			[0]	= 0x0000081C,
			[1]	= 0x0010081C,
			[2]	= 0x0000060C,
			[3]	= 0x0010060C,
		},
		.dvfs_offset	= 40,
	},
};

static bool use_mif_timing_set_0;

int exynos5_devfreq_mif_update_timingset(struct devfreq_data_mif *data)
{
	use_mif_timing_set_0 = ((__raw_readl(data->base_mif + 0x1004) & 0x1) == 0);

	return 0;
}

static int exynos5_devfreq_mif_change_timing_set(struct devfreq_data_mif *data)
{
	unsigned int tmp;

	if (use_mif_timing_set_0) {
		tmp = __raw_readl(data->base_mif + 0x1004);
		tmp |= 0x1;
		__raw_writel(tmp, data->base_mif + 0x1004);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x2 << 24);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x2 << 24);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);
	} else {
		tmp = __raw_readl(data->base_mif + 0x1004);
		tmp &= ~0x1;
		__raw_writel(tmp, data->base_mif + 0x1004);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x1 << 24);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0x3 << 24);
		tmp |= (0x1 << 24);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);
	}

	exynos5_devfreq_mif_update_timingset(data);

	return 0;
}

static int exynos5_devfreq_mif_set_phy(struct devfreq_data_mif *data,
		int target_idx)
{
	struct devfreq_mif_timing_parameter *cur_parameter;
	unsigned int tmp;

	cur_parameter = &dmc_timing_parameter_3gb[target_idx];

	if (use_mif_timing_set_0) {
		tmp = __raw_readl(data->base_lpddr_phy0 + 0xBC);
		tmp &= ~(0x1F << 24);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 24));
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xBC);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xBC);
		tmp &= ~(0x1F << 24);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 24));
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xBC);
	} else {
		tmp = __raw_readl(data->base_lpddr_phy0 + 0xBC);
		tmp &= ~(0x1F << 16);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 16));
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xBC);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xBC);
		tmp &= ~(0x1F << 16);
		tmp |= (cur_parameter->dvfs_con1 & (0x1F << 16));
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xBC);
	}

	return 0;
}

#define TIMING_RFCPB_MASK	(0x3F)
static int exynos5_devfreq_mif_set_and_change_timing_set(struct devfreq_data_mif *data,
							int target_idx)
{
	struct devfreq_mif_timing_parameter *cur_parameter;
	unsigned int tmp;

	cur_parameter = &dmc_timing_parameter_3gb[target_idx];

	if (use_mif_timing_set_0) {
		__raw_writel(cur_parameter->timing_row, data->base_drex0 + 0xE4);
		__raw_writel(cur_parameter->timing_data, data->base_drex0 + 0xE8);
		__raw_writel(cur_parameter->timing_power, data->base_drex0 + 0xEC);
		tmp = __raw_readl(data->base_drex0 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK << 8);
		tmp |= (cur_parameter->timing_rfcpb & (TIMING_RFCPB_MASK << 8));
		__raw_writel(tmp, data->base_drex0 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex0 + 0x50);

		__raw_writel(cur_parameter->timing_row, data->base_drex1 + 0xE4);
		__raw_writel(cur_parameter->timing_data, data->base_drex1 + 0xE8);
		__raw_writel(cur_parameter->timing_power, data->base_drex1 + 0xEC);
		tmp = __raw_readl(data->base_drex1 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK << 8);
		tmp |= (cur_parameter->timing_rfcpb & (TIMING_RFCPB_MASK << 8));
		__raw_writel(tmp, data->base_drex1 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex1 + 0x50);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0xFF << 8);
		tmp |= (cur_parameter->dvfs_offset << 8);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0xFF << 8);
		tmp |= (cur_parameter->dvfs_offset << 8);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);
	} else {
		__raw_writel(cur_parameter->timing_row, data->base_drex0 + 0x34);
		__raw_writel(cur_parameter->timing_data, data->base_drex0 + 0x38);
		__raw_writel(cur_parameter->timing_power, data->base_drex0 + 0x3C);
		tmp = __raw_readl(data->base_drex0 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK);
		tmp |= (cur_parameter->timing_rfcpb & TIMING_RFCPB_MASK);
		__raw_writel(tmp, data->base_drex0 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex0 + 0x4C);

		__raw_writel(cur_parameter->timing_row, data->base_drex1 + 0x34);
		__raw_writel(cur_parameter->timing_data, data->base_drex1 + 0x38);
		__raw_writel(cur_parameter->timing_power, data->base_drex1 + 0x3C);
		tmp = __raw_readl(data->base_drex1 + 0x20);
		tmp &= ~(TIMING_RFCPB_MASK);
		tmp |= (cur_parameter->timing_rfcpb & TIMING_RFCPB_MASK);
		__raw_writel(tmp, data->base_drex1 + 0x20);
		__raw_writel(cur_parameter->rd_fetch, data->base_drex1 + 0x4C);

		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB8);
		tmp &= ~(0xFF);
		tmp |= (cur_parameter->dvfs_offset);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB8);

		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB8);
		tmp &= ~(0xFF);
		tmp |= (cur_parameter->dvfs_offset);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB8);
	}
	exynos5_devfreq_mif_set_phy(data, target_idx);
	exynos5_devfreq_mif_change_timing_set(data);

	return 0;
}

static int exynos5_devfreq_mif_set_timeout(struct devfreq_data_mif *data,
					int target_idx)
{
	if (timeout_table == NULL) {
		pr_err("DEVFREQ(MIF) : can't setting timeout value\n");
		return -EINVAL;
	}

	if (enabled_ud_encode) {
		__raw_writel(0x00800080, data->base_drex0 + 0xA0);
		__raw_writel(0x00800080, data->base_drex1 + 0xA0);
	} else {
		__raw_writel(0x0fff0fff, data->base_drex0 + 0xA0);
		__raw_writel(0x0fff0fff, data->base_drex1 + 0xA0);
	}
	__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xD0);
	__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xC0);
	__raw_writel(timeout_table[target_idx][0], data->base_drex0 + 0xC8);
	__raw_writel(timeout_table[target_idx][1], data->base_drex0 + 0x100);

	__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xD0);
	__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xC0);
	__raw_writel(timeout_table[target_idx][0], data->base_drex1 + 0xC8);
	__raw_writel(timeout_table[target_idx][1], data->base_drex1 + 0x100);

	return 0;
}

extern void exynos5_update_district_disp_level(unsigned int idx);
extern struct pm_qos_request exynos5_int_bts_qos;
void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value)
{
	unsigned int total_layer_count = 0;
	int disp_qos = DISP_LV3;
	int mif_qos = MIF_LV9;
	int int_qos = INT_LV6;
	int tv_layer_value;

	mutex_lock(&media_mutex);

	switch (media_type) {
	case TYPE_FIMC_LITE:
		media_enabled_fimc_lite = value;
		break;
	case TYPE_MIXER:
		media_num_mixer_layer = value;
		break;
	case TYPE_DECON:
		media_num_decon_layer = value;
		break;
	case TYPE_GSCL_LOCAL:
		media_enabled_gscl_local = value;
		break;
	case TYPE_TV:
		media_enabled_tv = !!value;
		tv_layer_value = value;
		switch (media_resolution) {
			case RESOLUTION_HD:
				tv_layer_value = (value - (TRAFFIC_BYTES_FHD_32BIT_60FPS * 2)
					  + (TRAFFIC_BYTES_FHD_32BIT_60FPS - 1));
				if (tv_layer_value < 0)
					tv_layer_value = 0;
				media_num_mixer_layer = tv_layer_value / TRAFFIC_BYTES_FHD_32BIT_60FPS;
				break;
			case RESOLUTION_FULLHD:
			case RESOLUTION_WQHD:
			case RESOLUTION_WQXGA:
				tv_layer_value = (value - (TRAFFIC_BYTES_FHD_32BIT_60FPS * 2)
					  + (media_resolution_bandwidth - 1));
				if (tv_layer_value < 0)
					tv_layer_value = 0;
				media_num_mixer_layer = tv_layer_value / media_resolution_bandwidth;
				break;
			default:
				pr_err("DEVFREQ(MIF) : can't calculate mixer layer by traffic(%u)\n", media_resolution);
				break;
		}
		break;
	case TYPE_UD_DECODING:
		enabled_ud_decode = value;
		break;
	case TYPE_UD_ENCODING:
		enabled_ud_encode = value;
		break;
	case TYPE_RESOLUTION:
		switch (value) {
			case TRAFFIC_BYTES_HD_32BIT_60FPS:
				media_resolution = RESOLUTION_HD;
				break;
			case TRAFFIC_BYTES_FHD_32BIT_60FPS:
				media_resolution = RESOLUTION_FULLHD;
				break;
			case TRAFFIC_BYTES_WQHD_32BIT_60FPS:
				media_resolution = RESOLUTION_WQHD;
				break;
			case TRAFFIC_BYTES_WQXGA_32BIT_60FPS:
				media_resolution = RESOLUTION_WQXGA;
				break;
			default:
				pr_err("DEVFREQ(MIF) : can't decide resolution type by traffic bytes(%u)\n", value);
				break;
		}
		media_resolution_bandwidth = value;
		mutex_unlock(&media_mutex);
		return;
	}

	total_layer_count = media_num_mixer_layer + media_num_decon_layer;

	if (total_layer_count > 6)
		total_layer_count = 6;

	if (media_resolution == RESOLUTION_FULLHD) {
		if (media_enabled_fimc_lite) {
			if (mif_qos > distriction_fullhd_camera[total_layer_count].mif_level)
				mif_qos = distriction_fullhd_camera[total_layer_count].mif_level;
			timeout_table = timeout_fullhd_camera;
		}
		if (media_enabled_gscl_local) {
			total_layer_count = media_num_decon_layer - 1;
			if (total_layer_count < 0)
				total_layer_count = 0;
			total_layer_count += media_num_mixer_layer;

			if (total_layer_count == NUM_LAYER_5) {
				pr_err("DEVFREQ(MIF) : can't support mif and disp distriction. using gscl local with 5 windows.\n");
				goto out;
			}
			if (mif_qos > distriction_fullhd_gscl[total_layer_count].mif_level)
				mif_qos = distriction_fullhd_gscl[total_layer_count].mif_level;
			if (disp_qos > distriction_fullhd_gscl[total_layer_count].disp_level)
				disp_qos = distriction_fullhd_gscl[total_layer_count].disp_level;
			timeout_table = timeout_fullhd_gscl;
		}
		if (media_enabled_tv) {
			if (mif_qos > distriction_fullhd_tv[total_layer_count].mif_level)
				mif_qos = distriction_fullhd_tv[total_layer_count].mif_level;
			if (disp_qos > distriction_fullhd_tv[total_layer_count].disp_level)
				disp_qos = distriction_fullhd_tv[total_layer_count].disp_level;
			timeout_table = timeout_fullhd_tv;
		}
		if (!media_enabled_fimc_lite && !media_enabled_gscl_local && !media_enabled_tv)
			timeout_table = timeout_fullhd;
		if (mif_qos > distriction_fullhd[total_layer_count].mif_level)
			mif_qos = distriction_fullhd[total_layer_count].mif_level;
		if (disp_qos > distriction_fullhd[total_layer_count].disp_level)
			disp_qos = distriction_fullhd[total_layer_count].disp_level;
	} else if (media_resolution == RESOLUTION_WQHD || media_resolution == RESOLUTION_WQXGA) {
		if (media_enabled_fimc_lite) {
			if (mif_qos > distriction_wqhd_camera[total_layer_count].mif_level)
				mif_qos = distriction_wqhd_camera[total_layer_count].mif_level;
			if (media_resolution == RESOLUTION_WQHD) {
				if (int_qos > distriction_wqhd_camera[total_layer_count].int_level)
					int_qos = distriction_wqhd_camera[total_layer_count].int_level;
			} else if (media_resolution == RESOLUTION_WQXGA) {
				if (int_qos > distriction_wqxga_camera[total_layer_count].int_level)
					int_qos = distriction_wqxga_camera[total_layer_count].int_level;
			}
			if (disp_qos > distriction_wqhd_camera[total_layer_count].disp_level)
				disp_qos = distriction_wqhd_camera[total_layer_count].disp_level;
			timeout_table = timeout_wqhd_camera;
		}
		if (media_enabled_tv) {
			if (mif_qos > distriction_wqhd_tv[total_layer_count].mif_level)
				mif_qos = distriction_wqhd_tv[total_layer_count].mif_level;
			if (int_qos > distriction_wqhd_tv[total_layer_count].int_level)
				int_qos = distriction_wqhd_tv[total_layer_count].int_level;
			if (disp_qos > distriction_wqhd_tv[total_layer_count].disp_level)
				disp_qos = distriction_wqhd_tv[total_layer_count].disp_level;
			timeout_table = timeout_wqhd_tv;

			wqhd_tv_window5 = (total_layer_count == NUM_LAYER_5);
		} else {
			wqhd_tv_window5 = false;
		}
		if (media_enabled_gscl_local) {
			total_layer_count = media_num_decon_layer - 1;
			if (total_layer_count < 0)
				total_layer_count = 0;
			total_layer_count += media_num_mixer_layer;

			if (total_layer_count == NUM_LAYER_5) {
				pr_err("DEVFREQ(MIF) : can't support mif and disp distriction. using gscl local with 5 windows.\n");
				goto out;
			}
			if (mif_qos > distriction_wqhd_gscl[total_layer_count].mif_level)
				mif_qos = distriction_wqhd_gscl[total_layer_count].mif_level;
			if (disp_qos > distriction_wqhd_gscl[total_layer_count].disp_level)
				disp_qos = distriction_wqhd_gscl[total_layer_count].disp_level;
			if (int_qos > distriction_wqhd_gscl[total_layer_count].int_level)
				int_qos = distriction_wqhd_gscl[total_layer_count].int_level;
			timeout_table = timeout_wqhd_gscl;
		}
		if (!media_enabled_fimc_lite && !media_enabled_gscl_local && !media_enabled_tv)
			timeout_table = timeout_wqhd;
		if (mif_qos > distriction_wqhd[total_layer_count].mif_level)
			mif_qos = distriction_wqhd[total_layer_count].mif_level;
		if (int_qos > distriction_wqhd[total_layer_count].int_level)
			int_qos = distriction_wqhd[total_layer_count].int_level;
		if (disp_qos > distriction_wqhd[total_layer_count].disp_level)
			disp_qos = distriction_wqhd[total_layer_count].disp_level;
		if (enabled_ud_encode)
			timeout_table = timeout_wqhd_ud_encode;
	}

	if (pm_qos_request_active(&exynos5_mif_bts_qos)) {
		pm_qos_update_request(&exynos5_mif_bts_qos, devfreq_mif_opp_list[mif_qos].freq);
	}

	exynos5_update_district_disp_level(disp_qos);

	if (pm_qos_request_active(&exynos5_int_bts_qos))
		pm_qos_update_request(&exynos5_int_bts_qos, devfreq_int_opp_list[int_qos].freq);
out:
	mutex_unlock(&media_mutex);
}

#define DLL_ON_BASE_VOLT	(900000)
static int exynos5_devfreq_calculate_dll_lock_value(struct devfreq_data_mif *data,
							long vdd_mif_l0)
{
	return  ((vdd_mif_l0 - DLL_ON_BASE_VOLT + 9999) / 10000) * 2;
}

void exynos5_devfreq_set_dll_lock_value(struct devfreq_data_mif *data,
							long vdd_mif_l0)
{
	/* 9999 make ceiling result */
	int lock_value_offset = exynos5_devfreq_calculate_dll_lock_value(data, vdd_mif_l0);
	int ctrl_force, ctrl_force_value;

	ctrl_force = __raw_readl(data->base_lpddr_phy0 + 0xB0);
	ctrl_force_value = (ctrl_force >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
	ctrl_force_value += lock_value_offset;
	ctrl_force &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
	ctrl_force |= (ctrl_force_value << CTRL_FORCE_SHIFT);
	__raw_writel(ctrl_force, data->base_lpddr_phy0 + 0xB0);

	ctrl_force = __raw_readl(data->base_lpddr_phy1 + 0xB0);
	ctrl_force_value = (ctrl_force >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
	ctrl_force_value += lock_value_offset;
	ctrl_force &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
	ctrl_force |= (ctrl_force_value << CTRL_FORCE_SHIFT);
	__raw_writel(ctrl_force, data->base_lpddr_phy1 + 0xB0);
}

static void exynos5_devfreq_mif_dynamic_setting(struct devfreq_data_mif *data,
						bool flag)
{
	unsigned int tmp;

	if (flag) {
		/* DREX MEMCONTROL
		   [5]: dsref_en: Dynamic self-refresh
		   [1]: dpwrdn_en: Dynamic power down
		   */
		tmp = __raw_readl(data->base_drex0 + 0x0004);
		tmp |= ((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex0 + 0x0004);
		tmp = __raw_readl(data->base_drex1 + 0x0004);
		tmp |= ((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex1 + 0x0004);

		/* DREX CGCONTROL
		   [3]: memif_cg_en: Memory Controller Internal Clock Gating - Memory I/F
		   [2]: scg_cg_en: Memory Controller Internal Clock Gating - Scheduler
		   [1]: busif_wr_cg_en: Memory Controller Internal Clock Gating - BUS I/F Write
		   [0]: busif_rd_cg_en: Memory Controller Internal Clock Gating - BUS I/F Read
		   */
		tmp = __raw_readl(data->base_drex0 + 0x0008);
		tmp |= (0x3F);
		__raw_writel(tmp, data->base_drex0 + 0x0008);
		tmp = __raw_readl(data->base_drex1 + 0x0008);
		tmp |= (0x3F);
		__raw_writel(tmp, data->base_drex1 + 0x0008);
	} else {
		tmp = __raw_readl(data->base_drex0 + 0x0004);
		tmp &= ~((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex0 + 0x0004);
		tmp = __raw_readl(data->base_drex1 + 0x0004);
		tmp &= ~((0x1 << 5) | (0x1 << 1));
		__raw_writel(tmp, data->base_drex1 + 0x0004);

		tmp = __raw_readl(data->base_drex0 + 0x0008);
		tmp &= ~(0x3F);
		__raw_writel(tmp, data->base_drex0 + 0x0008);
		tmp = __raw_readl(data->base_drex1 + 0x0008);
		tmp &= ~(0x3F);
		__raw_writel(tmp, data->base_drex1 + 0x0008);
	}
}

static int exynos5_devfreq_mif_set_dll(struct devfreq_data_mif *data,
					unsigned long target_volt,
					int target_idx)
{
	unsigned int tmp;
	unsigned int lock_value;
	unsigned int timeout;

	if (target_idx == MIF_LV0) {
		/* only MIF_LV0 use DLL tacing mode(CLKM_PHY_C_ENABLE mux gating 1(enable)/0(disable)). */
		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
		tmp |= (0x1 << 5);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);
		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
		tmp |= (0x1 << 5);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

		timeout = 1000;
		while ((__raw_readl(data->base_lpddr_phy0 + 0xB4) & 0x5) != 0x5) {
			if (timeout-- == 0) {
				pr_err("DEVFREQ(MIF) : Timeout to wait dll on(lpddrphy0)\n");
				return -EINVAL;
			}
			udelay(1);
		}
		timeout = 1000;
		while ((__raw_readl(data->base_lpddr_phy1 + 0xB4) & 0x5) != 0x5) {
			if (timeout-- == 0) {
				pr_err("DEVFREQ(MIF) : Timeout to wait dll on(lpddrphy1)\n");
				return -EINVAL;
			}
			udelay(1);
		}
	} else {
		/* 1. CH0, CH1 DLL Tracing off mode */
		tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
		tmp &= ~(0x1 << 5);
		__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);
		tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
		tmp &= ~(0x1 << 5);
		__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

		if (data->dll_status) {
			/* 2. CH0: read Current ctrl_lock_value */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB4);
			lock_value = (tmp >> CTRL_LOCK_VALUE_SHIFT) & CTRL_LOCK_VALUE_MASK;

			/* 3. CH0: Enter DLL Tracing ON mode by setting "ctrl_dll_on" to one */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp |= (0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			/* 4. CH0: Write the read "ctrl_lock_value" to "ctrl_force" */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (lock_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			/* 5. CH0: Enter DLL Tracing OFF mode by clearing "ctrl_dll_on" to zero */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			tmp &= ~(0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			/* CH1 is same to CH0 */
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB4);
			lock_value = (tmp >> CTRL_LOCK_VALUE_SHIFT) & CTRL_LOCK_VALUE_MASK;
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp |= (0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (lock_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);
			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			tmp &= ~(0x1 << 5);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);
			data->dll_status = false;
		}
	}

	return 0;
}

static int exynos5_devfreq_mif_set_volt(struct devfreq_data_mif *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_mif, volt, volt_range);
	data->old_volt = volt;
out:
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	return voltage + volt_offset;
}
#endif

#ifdef CONFIG_EXYNOS_THERMAL
extern struct pm_qos_request min_mif_thermal_qos;
int exynos5_devfreq_mif_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_mif *data = container_of(nb, struct devfreq_data_mif,
							tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;
	unsigned int tmp;
	unsigned int ctrl_force_value;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_mif_thermal_qos))
			pm_qos_update_request(&min_mif_thermal_qos,
					data->initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);

			/* Update CTRL FORCE */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value += CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value += CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);

			/* Update CTRL FORCE */
			tmp = __raw_readl(data->base_lpddr_phy0 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value -= CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy0 + 0xB0);

			tmp = __raw_readl(data->base_lpddr_phy1 + 0xB0);
			ctrl_force_value = (tmp >> CTRL_FORCE_SHIFT) & CTRL_FORCE_MASK;
			ctrl_force_value -= CTRL_FORCE_OFFSET;
			tmp &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
			tmp |= (ctrl_force_value << CTRL_FORCE_SHIFT);
			__raw_writel(tmp, data->base_lpddr_phy1 + 0xB0);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_mif_thermal_qos))
			pm_qos_update_request(&min_mif_thermal_qos,
					data->default_qos);
	}

	return NOTIFY_OK;
}
#endif

static void exynos5_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

#ifdef CONFIG_SCHED_HMP
	mod_delayed_work_on(0,
			work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
#else
	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
#endif
}

static unsigned int use_mif_throttling = 1;

static ssize_t mif_show_templvl_ch0_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch0_work.thermal_level_cs0);
}
static ssize_t mif_show_templvl_ch0_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch0_work.thermal_level_cs1);
}
static ssize_t mif_show_templvl_ch1_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch1_work.thermal_level_cs0);
}
static ssize_t mif_show_templvl_ch1_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch1_work.thermal_level_cs1);
}
static ssize_t mif_show_use_throttling(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", use_mif_throttling);
}
static ssize_t mif_store_use_throttling(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int use_throttling;

	if (kstrtoint(buf, count, &use_throttling))
		return -EINVAL;

	use_mif_throttling = use_throttling;
	return count;
}

#if defined(CONFIG_ARM_EXYNOS_BUS_DEVFREQ_SYSFS_MAX_TEMP)
static ssize_t mif_show_max_temp_level(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned int thermal_level;

	thermal_level = max(
		max(devfreq_mif_ch0_work.thermal_level_cs0,
			devfreq_mif_ch0_work.thermal_level_cs1),
		max(devfreq_mif_ch1_work.thermal_level_cs0,
			devfreq_mif_ch1_work.thermal_level_cs1));

	return snprintf(buf, PAGE_SIZE, "%u\n", thermal_level);
}
#endif

static ssize_t mif_show_media_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "[DEVFREQ media information]\n"
			"media_resolution: %d(0:HD, 1:FHD, 2: WQHD, 3: WQXGA)\n"
			"media_resolution_bandwidth: %d\n"
			"media_enabled_fimc_lite: %d\n"
			"media_num_mixer_layer: %d\n"
			"media_num_decon_layer: %d\n"
			"media_enabled_gscl_local: %d\n"
			"media_enabled_tv: %d\n"
			"enabled_ud_decode: %d\n"
			"enabled_ud_encode: %d\n",
			media_resolution, media_resolution_bandwidth,
			media_enabled_fimc_lite, media_num_mixer_layer,
			media_num_decon_layer, media_enabled_gscl_local,
			media_enabled_tv, enabled_ud_decode,
			enabled_ud_encode);
}

static DEVICE_ATTR(mif_media_info, 0644, mif_show_media_info, NULL);
static DEVICE_ATTR(mif_templvl_ch0_0, 0644, mif_show_templvl_ch0_0, NULL);
static DEVICE_ATTR(mif_templvl_ch0_1, 0644, mif_show_templvl_ch0_1, NULL);
static DEVICE_ATTR(mif_templvl_ch1_0, 0644, mif_show_templvl_ch1_0, NULL);
static DEVICE_ATTR(mif_templvl_ch1_1, 0644, mif_show_templvl_ch1_1, NULL);
static DEVICE_ATTR(mif_use_throttling, 0644, mif_show_use_throttling, mif_store_use_throttling);
#if defined(CONFIG_ARM_EXYNOS_BUS_DEVFREQ_SYSFS_MAX_TEMP)
static DEVICE_ATTR(mif_max_temp_level, 0644, mif_show_max_temp_level, NULL);
#endif

static struct attribute *devfreq_mif_sysfs_entries[] = {
	&dev_attr_mif_media_info.attr,
	&dev_attr_mif_templvl_ch0_0.attr,
	&dev_attr_mif_templvl_ch0_1.attr,
	&dev_attr_mif_templvl_ch1_0.attr,
	&dev_attr_mif_templvl_ch1_1.attr,
	&dev_attr_mif_use_throttling.attr,
#if defined(CONFIG_ARM_EXYNOS_BUS_DEVFREQ_SYSFS_MAX_TEMP)
	&dev_attr_mif_max_temp_level.attr,
#endif
	NULL,
};

struct attribute_group devfreq_mif_attr_group = {
	.name   = "temp_level",
	.attrs  = devfreq_mif_sysfs_entries,
};

static BLOCKING_NOTIFIER_HEAD(mif_max_thermal_level_notifier);

/* FIMC-IS should call this function to register client */
int exynos_mif_add_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&mif_max_thermal_level_notifier, n);
}

void exynos_mif_call_notifier(int val)
{
	void *v = NULL;
	blocking_notifier_call_chain(&mif_max_thermal_level_notifier, val, v);
}

/* notifier for DTM driver to handle MR4 throttling */
static BLOCKING_NOTIFIER_HEAD(mif_thermal_level_notifier);

int exynos5_mif_thermal_add_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&mif_thermal_level_notifier, n);
}

void exynos5_mif_thermal_call_notifier(int val, enum devfreq_mif_thermal_channel ch)
{
	blocking_notifier_call_chain(&mif_thermal_level_notifier, val, &ch);
}

static void exynos5_devfreq_swtrip(void)
{
#ifdef CONFIG_EXYNOS_SWTRIP
	char tmustate_string[20];
	char *envp[2];

	snprintf(tmustate_string, sizeof(tmustate_string), "TMUSTATE=%d", 3);
	envp[0] = tmustate_string;
	envp[1] = NULL;
	pr_err("DEVFREQ(MIF) : SW trip by MR4\n");
	kobject_uevent_env(&data_mif->dev->kobj, KOBJ_CHANGE, envp);
#endif
}

#define MRSTATUS_THERMAL_BIT_SHIFT	(7)
#define MRSTATUS_THERMAL_BIT_MASK	(1)
#define MRSTATUS_THERMAL_LV_MASK	(0x7)
static void exynos5_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0, tmp;
	unsigned int timingaref_value = RATE_ONE;
	unsigned long max_freq = data_mif->cal_qos_max;
	bool throttling = false;
	void __iomem *base_drex = NULL;

	if (thermal_work->channel == THERMAL_CHANNEL0)
		base_drex = data_mif->base_drex0;
	else /* thermal_work->channel == THERMAL_CHANNEL1 */
		base_drex = data_mif->base_drex1;

	mutex_lock(&data_mif->lock);

	__raw_writel(0x09001000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs0 = tmp_thermal_level;

	__raw_writel(0x09101000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs1 = tmp_thermal_level;

	mutex_unlock(&data_mif->lock);

	exynos5_mif_thermal_call_notifier(thermal_work->thermal_level_cs1, thermal_work->channel);

	switch (max_thermal_level) {
	case 0:
	case 1:
	case 2:
	case 3:
		timingaref_value = RATE_ONE;
		thermal_work->polling_period = 1000;
		break;
	case 4:
		timingaref_value = RATE_HALF;
		thermal_work->polling_period = 300;
		pr_info("MIF: max_thermal_level is %d\n", max_thermal_level);
		break;
	case 5:
		throttling = true;
		timingaref_value = RATE_QUARTER;
		thermal_work->polling_period = 100;

		pr_info("MIF: max_thermal_level is %d\n", max_thermal_level);
		if (throttling) {
			pr_info("so it need MIF throttling!\n");
			/* signal to FIMC-IS */
			exynos_mif_call_notifier(devfreq_mif_opp_list[MIF_LV5].freq);
		}
		break;
	case 6:
		exynos5_devfreq_swtrip();
		return;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return;
	}

	if (throttling && use_mif_throttling) {
		max_freq = devfreq_mif_opp_list[MIF_LV5].freq;
	} else {
		max_freq = data_mif->cal_qos_max;
	}

	if (thermal_work->max_freq != max_freq) {
		thermal_work->max_freq = max_freq;
		mutex_lock(&data_mif->devfreq->lock);
		update_devfreq(data_mif->devfreq);
		mutex_unlock(&data_mif->devfreq->lock);
	}

	if (max_freq != data_mif->cal_qos_max) {
		tmp = __raw_readl(base_drex + 0x4);
		tmp &= ~(0x1 << 27);
		__raw_writel(tmp, base_drex + 0x4);
	}

	__raw_writel(timingaref_value, base_drex + 0x30);

	if (max_freq == data_mif->cal_qos_max) {
		tmp = __raw_readl(base_drex + 0x4);
		tmp |= (0x1 << 27);
		__raw_writel(tmp, base_drex + 0x4);
	}

	exynos5_devfreq_thermal_event(thermal_work);
}

void exynos5_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq_ch0 = create_freezable_workqueue("devfreq_thermal_wq_ch0");
	devfreq_mif_thermal_wq_ch1 = create_freezable_workqueue("devfreq_thermal_wq_ch1");

	INIT_DELAYED_WORK(&devfreq_mif_ch0_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);
	INIT_DELAYED_WORK(&devfreq_mif_ch1_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);

	devfreq_mif_ch0_work.work_queue = devfreq_mif_thermal_wq_ch0;
	devfreq_mif_ch1_work.work_queue = devfreq_mif_thermal_wq_ch1;

	exynos5_devfreq_thermal_event(&devfreq_mif_ch0_work);
	exynos5_devfreq_thermal_event(&devfreq_mif_ch1_work);
}

int exynos5_devfreq_mif_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_clk); ++i) {
		devfreq_mif_clk[i].clk = clk_get(NULL, devfreq_mif_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_mif_clk[i].clk)) {
			pr_err("DEVFREQ(MIF) : %s can't get clock\n", devfreq_mif_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("MIF clk name: %s, rate: %lu\n", devfreq_mif_clk[i].clk_name, clk_get_rate(devfreq_mif_clk[i].clk));
	}

	return 0;
}

int exynos5_devfreq_mif_init_parameter(struct devfreq_data_mif *data)
{
	data->base_mif = ioremap(0x105B0000, SZ_64K);
	data->base_sysreg_mif = ioremap(0x105E0000, SZ_64K);
	data->base_drex0 = ioremap(0x10400000, SZ_64K);
	data->base_drex1 = ioremap(0x10440000, SZ_64K);
	data->base_lpddr_phy0 = ioremap(0x10420000, SZ_64K);
	data->base_lpddr_phy1 = ioremap(0x10460000, SZ_64K);

	exynos5_devfreq_mif_update_timingset(data);

	return 0;
}

int exynos5433_devfreq_mif_init(struct devfreq_data_mif *data)
{
	int i;
	int ret = 0;

	timeout_table = timeout_fullhd;
	media_enabled_fimc_lite = false;
	media_enabled_gscl_local = false;
	media_enabled_tv = false;
	media_num_mixer_layer = false;
	media_num_decon_layer = false;
	wqhd_tv_window5 = false;
	data_mif = data;

	data->mif_set_freq = exynos5_devfreq_mif_set_freq;
	data->mif_set_and_change_timing_set = exynos5_devfreq_mif_set_and_change_timing_set;
	data->mif_set_timeout = exynos5_devfreq_mif_set_timeout;
	data->mif_set_dll = exynos5_devfreq_mif_set_dll;
	data->mif_dynamic_setting = exynos5_devfreq_mif_dynamic_setting;
	data->mif_set_volt = exynos5_devfreq_mif_set_volt;

	for (i = 0; i < CNT_FW_MIF_LEVELS; i++) {
		int RL = g_aDvfsMifParam[i].uDvfsRdLat;

		dmc_timing_parameter_3gb[i].timing_row = g_aDvfsMifParam[i].uTimingRow;
		dmc_timing_parameter_3gb[i].timing_data = g_aDvfsMifParam[i].uTimingData;
		dmc_timing_parameter_3gb[i].timing_power = g_aDvfsMifParam[i].uTimingPower;
		dmc_timing_parameter_3gb[i].rd_fetch = g_aDvfsMifParam[i].uRdFetch;
		dmc_timing_parameter_3gb[i].timing_rfcpb = g_aDvfsMifParam[i].uRFCpb;
		dmc_timing_parameter_3gb[i].dvfs_con1 = ((RL<<24)|(RL<<16)|0x2121);
		dmc_timing_parameter_3gb[i].mif_drex_mr_data[0] = g_aDvfsMifParam[i].uMRW0;
		dmc_timing_parameter_3gb[i].mif_drex_mr_data[1] = g_aDvfsMifParam[i].uMRW1;
		dmc_timing_parameter_3gb[i].mif_drex_mr_data[2] = g_aDvfsMifParam[i].uMRW2;
		dmc_timing_parameter_3gb[i].mif_drex_mr_data[3] = g_aDvfsMifParam[i].uMRW3;
		dmc_timing_parameter_3gb[i].dvfs_offset = g_aDvfsMifParam[i].uDvfsOffset;
	}

	data->max_state = MIF_LV_COUNT;

	data->mif_asv_abb_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (data->mif_asv_abb_table == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate abb table\n");
		ret = -ENOMEM;
		return ret;
	}

	exynos5_devfreq_mif_init_clock();
	exynos5_devfreq_mif_init_parameter(data);
	data->dll_status = ((__raw_readl(data->base_lpddr_phy0 + 0xB0) & (0x1 << 5)) != 0);
	pr_info("DEVFREQ(MIF) : default dll satus : %s\n", (data->dll_status ? "on" : "off"));

	return ret;
}

int exynos5433_devfreq_mif_deinit(struct devfreq_data_mif *data)
{
	flush_workqueue(devfreq_mif_thermal_wq_ch0);
	destroy_workqueue(devfreq_mif_thermal_wq_ch0);
	flush_workqueue(devfreq_mif_thermal_wq_ch1);
	destroy_workqueue(devfreq_mif_thermal_wq_ch1);

	return 0;
}

/* end of MIF related function */
