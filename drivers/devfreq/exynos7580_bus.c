/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Seungook yang(swy.yang@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>

#include <plat/cpu.h>

#include <mach/devfreq.h>
#include <mach/tmu.h>
#include <mach/regs-pmu-exynos7580.h>

#include "devfreq_exynos.h"
#include "governor.h"

/* =========== common function */
int devfreq_get_opp_idx(struct devfreq_opp_table *table,
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

int exynos_devfreq_init_pm_domain(struct devfreq_pm_domain_link pm_domain[], int num_of_pd)
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

		for (i = 0; i < num_of_pd; ++i) {
			if (pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(pm_domain[i].pm_domain_name, pd->genpd.name))
				pm_domain[i].pm_domain = pd;
		}
	}
	return 0;
}

unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	return voltage + volt_offset;
}

#ifdef DUMP_DVFS_CLKS
static void exynos7_devfreq_dump_all_clks(struct devfreq_clk_list devfreq_clks[], int clk_count, char *str)
{
	int i;
	for (i = 0; i < clk_count; ++i) {
		if (devfreq_clks[i].clk)
			pr_info("%s %2d. clk name: %25s, rate: %4luMHz\n", str,
					i, devfreq_clks[i].clk_name, (clk_get_rate(devfreq_clks[i].clk) + (MHZ-1))/MHZ);
	}
	return;
}
#endif

static struct devfreq_data_int *data_int;
static struct devfreq_data_mif *data_mif;
static struct devfreq_data_isp *data_isp;

/* ========== 1. INT related function */
extern struct pm_qos_request min_int_thermal_qos;

enum devfreq_int_idx {
	INT_LV0,
	INT_LV1,
	INT_LV2,
	INT_LV3,
	INT_LV4,
	INT_LV5,
	INT_LV6,
	INT_LV7,
	INT_LV_COUNT,
};

enum devfreq_int_clk {
	_BUS_PLL,
	MEDIA_PLL,
	DOUT_ACLK_BUS0_400,
	DOUT_ACLK_BUS1_400,
	DOUT_ACLK_BUS2_400,
	DOUT_ACLK_PERI_66,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	DOUT_ACLK_MFCMSCL_400,
	DOUT_ACLK_MFCMSCL_266,
	DOUT_ACLK_FSYS_200,
	DOUT_ACLK_ISP_400,
	DOUT_ACLK_ISP_333,
	_DOUT_ACLK_ISP_266,
	DOUT_ACLK_G3D_400,
	DOUT_PCLK_BUS0_100,
	DOUT_PCLK_BUS1_100,
	DOUT_PCLK_BUS2_100,
	INT_CLK_COUNT,
};

struct devfreq_clk_list devfreq_int_clk[INT_CLK_COUNT] = {
	{"bus_pll",},
	{"media_pll",},
	{"dout_aclk_bus0_400",},
	{"dout_aclk_bus1_400",},
	{"dout_aclk_bus2_400",},
	{"dout_aclk_peri_66",},
	{"dout_aclk_imem_266",},
	{"dout_aclk_imem_200",},
	{"dout_aclk_mfcmscl_400",},
	{"dout_aclk_mfcmscl_266",},
	{"dout_aclk_fsys_200",},
	{"dout_aclk_isp_400",},
	{"dout_aclk_isp_333",},
	{"dout_aclk_isp_266",},
	{"dout_aclk_g3d_400",},
	{"dout_pclk_bus0_100",},
	{"dout_pclk_bus1_100",},
	{"dout_pclk_bus2_100",},
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{INT_LV0,	400000,	1200000},
	{INT_LV1,	334000,	1200000},
	{INT_LV2,	267000,	1200000},
	{INT_LV3,	200000,	1200000},
	{INT_LV4,	160000,	1200000},
	{INT_LV5,	134000,	1200000},
	{INT_LV6,	111000,	1200000},
	{INT_LV7,	100000,	1200000},
};

struct devfreq_clk_state aclk_mfcmscl_400_bus_pll[] = {
	{DOUT_ACLK_MFCMSCL_400,	_BUS_PLL},
};

struct devfreq_clk_states aclk_mfcmscl_400_bus_pll_list= {
	.state = aclk_mfcmscl_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mfcmscl_400_bus_pll),
};

struct devfreq_clk_state aclk_mfcmscl_400_media_pll[] = {
	{DOUT_ACLK_MFCMSCL_400,	MEDIA_PLL},
};

struct devfreq_clk_states aclk_mfcmscl_400_media_pll_list= {
	.state = aclk_mfcmscl_400_media_pll,
	.state_count = ARRAY_SIZE(aclk_mfcmscl_400_media_pll),
};

struct devfreq_clk_info aclk_fsys_200[] = {
	{INT_LV0,	100 * MHZ,	0,	NULL},
	{INT_LV1,	100 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	100 * MHZ,	0,	NULL},
	{INT_LV4,	100 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_imem_266[] = {
	{INT_LV0,	267 * MHZ,	0,	NULL},
	{INT_LV1,	267 * MHZ,	0,	NULL},
	{INT_LV2,	200 * MHZ,	0,	NULL},
	{INT_LV3,	200 * MHZ,	0,	NULL},
	{INT_LV4,	160 * MHZ,	0,	NULL},
	{INT_LV5,	134 * MHZ,	0,	NULL},
	{INT_LV6,	115 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_imem_200[] = {
	{INT_LV0,	200 * MHZ,	0,	NULL},
	{INT_LV1,	200 * MHZ,	0,	NULL},
	{INT_LV2,	160 * MHZ,	0,	NULL},
	{INT_LV3,	160 * MHZ,	0,	NULL},
	{INT_LV4,	134 * MHZ,	0,	NULL},
	{INT_LV5,	115 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_bus0_400[] = {
	{INT_LV0,	400 * MHZ,	0,	NULL},
	{INT_LV1,	400 * MHZ,	0,	NULL},
	{INT_LV2,	400 * MHZ,	0,	NULL},
	{INT_LV3,	267 * MHZ,	0,	NULL},
	{INT_LV4,	267 * MHZ,	0,	NULL},
	{INT_LV5,	200 * MHZ,	0,	NULL},
	{INT_LV6,	160 * MHZ,	0,	NULL},
	{INT_LV7,	134 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_bus1_400[] = {
	{INT_LV0,	400 * MHZ,	0,	NULL},
	{INT_LV1,	400 * MHZ,	0,	NULL},
	{INT_LV2,	400 * MHZ,	0,	NULL},
	{INT_LV3,	400 * MHZ,	0,	NULL},
	{INT_LV4,	267 * MHZ,	0,	NULL},
	{INT_LV5,	200 * MHZ,	0,	NULL},
	{INT_LV6,	160 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_bus2_400[] = {
	{INT_LV0,	100 * MHZ,	0,	NULL},
	{INT_LV1,	100 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	100 * MHZ,	0,	NULL},
	{INT_LV4,	100 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_peri_66[] = {
	{INT_LV0,	67 * MHZ,	0,	NULL},
	{INT_LV1,	67 * MHZ,	0,	NULL},
	{INT_LV2,	67 * MHZ,	0,	NULL},
	{INT_LV3,	67 * MHZ,	0,	NULL},
	{INT_LV4,	67 * MHZ,	0,	NULL},
	{INT_LV5,	67 * MHZ,	0,	NULL},
	{INT_LV6,	67 * MHZ,	0,	NULL},
	{INT_LV7,	67 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_g3d_400[] = {
	{INT_LV0,	400 * MHZ,	0,	NULL},
	{INT_LV1,	400 * MHZ,	0,	NULL},
	{INT_LV2,	400 * MHZ,	0,	NULL},
	{INT_LV3,	400 * MHZ,	0,	NULL},
	{INT_LV4,	267 * MHZ,	0,	NULL},
	{INT_LV5,	200 * MHZ,	0,	NULL},
	{INT_LV6,	160 * MHZ,	0,	NULL},
	{INT_LV7,	134 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_isp_400[] = {
	{INT_LV0,	400 * MHZ,	0,	NULL},
	{INT_LV1,	400 * MHZ,	0,	NULL},
	{INT_LV2,	400 * MHZ,	0,	NULL},
	{INT_LV3,	267 * MHZ,	0,	NULL},
	{INT_LV4,	200 * MHZ,	0,	NULL},
	{INT_LV5,	134 * MHZ,	0,	NULL},
	{INT_LV6,	115 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_isp_333[] = {
	{INT_LV0,	334 * MHZ,	0,	NULL},
	{INT_LV1,	334 * MHZ,	0,	NULL},
	{INT_LV2,	334 * MHZ,	0,	NULL},
	{INT_LV3,	223 * MHZ,	0,	NULL},
	{INT_LV4,	167 * MHZ,	0,	NULL},
	{INT_LV5,	112 * MHZ,	0,	NULL},
	{INT_LV6,	96 * MHZ,	0,	NULL},
	{INT_LV7,	84 * MHZ,	0,	NULL},
};

struct devfreq_clk_info _aclk_isp_266[] = {
	{INT_LV0,	267 * MHZ,	0,	NULL},
	{INT_LV1,	267 * MHZ,	0,	NULL},
	{INT_LV2,	267 * MHZ,	0,	NULL},
	{INT_LV3,	200 * MHZ,	0,	NULL},
	{INT_LV4,	160 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	89 * MHZ,	0,	NULL},
	{INT_LV7,	80 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_mfcmscl_400[] = {
	{INT_LV0,	334 * MHZ,	0,	&aclk_mfcmscl_400_bus_pll_list},
	{INT_LV1,	334 * MHZ,	0,	&aclk_mfcmscl_400_bus_pll_list},
	{INT_LV2,	267 * MHZ,	0,	&aclk_mfcmscl_400_media_pll_list},
	{INT_LV3,	200 * MHZ,	0,	&aclk_mfcmscl_400_media_pll_list},
	{INT_LV4,	160 * MHZ,	0,	&aclk_mfcmscl_400_media_pll_list},
	{INT_LV5,	100 * MHZ,	0,	&aclk_mfcmscl_400_media_pll_list},
	{INT_LV6,	100 * MHZ,	0,	&aclk_mfcmscl_400_media_pll_list},
	{INT_LV7,	100 * MHZ,	0,	&aclk_mfcmscl_400_media_pll_list},
};

struct devfreq_clk_info aclk_mfcmscl_266[] = {
	{INT_LV0,	267 * MHZ,	0,	NULL},
	{INT_LV1,	267 * MHZ,	0,	NULL},
	{INT_LV2,	267 * MHZ,	0,	NULL},
	{INT_LV3,	200 * MHZ,	0,	NULL},
	{INT_LV4,	160 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info pclk_bus0_100[] = {
	{INT_LV0,	100 * MHZ,	0,	NULL},
	{INT_LV1,	100 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	100 * MHZ,	0,	NULL},
	{INT_LV4,	54 * MHZ,	0,	NULL},
	{INT_LV5,	40 * MHZ,	0,	NULL},
	{INT_LV6,	32 * MHZ,	0,	NULL},
	{INT_LV7,	20 * MHZ,	0,	NULL},
};

struct devfreq_clk_info pclk_bus1_100[] = {
	{INT_LV0,	100 * MHZ,	0,	NULL},
	{INT_LV1,	100 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	67 * MHZ,	0,	NULL},
	{INT_LV4,	54 * MHZ,	0,	NULL},
	{INT_LV5,	40 * MHZ,	0,	NULL},
	{INT_LV6,	32 * MHZ,	0,	NULL},
	{INT_LV7,	20 * MHZ,	0,	NULL},
};

struct devfreq_clk_info pclk_bus2_100[] = {
	{INT_LV0,	50 * MHZ,	0,	NULL},
	{INT_LV1,	50 * MHZ,	0,	NULL},
	{INT_LV2,	50 * MHZ,	0,	NULL},
	{INT_LV3,	50 * MHZ,	0,	NULL},
	{INT_LV4,	50 * MHZ,	0,	NULL},
	{INT_LV5,	50 * MHZ,	0,	NULL},
	{INT_LV6,	50 * MHZ,	0,	NULL},
	{INT_LV7,	50 * MHZ,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	aclk_bus0_400,
	aclk_bus1_400,
	aclk_bus2_400,
	aclk_peri_66,
	aclk_imem_266,
	aclk_imem_200,
	aclk_mfcmscl_400,
	aclk_mfcmscl_266,
	aclk_fsys_200,
	aclk_isp_400,
	aclk_isp_333,
	_aclk_isp_266,
	aclk_g3d_400,
	pclk_bus0_100,
	pclk_bus1_100,
	pclk_bus2_100,
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	DOUT_ACLK_BUS0_400,
	DOUT_ACLK_BUS1_400,
	DOUT_ACLK_BUS2_400,
	DOUT_ACLK_PERI_66,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	DOUT_ACLK_MFCMSCL_400,
	DOUT_ACLK_MFCMSCL_266,
	DOUT_ACLK_FSYS_200,
	DOUT_ACLK_ISP_400,
	DOUT_ACLK_ISP_333,
	_DOUT_ACLK_ISP_266,
	DOUT_ACLK_G3D_400,
	DOUT_PCLK_BUS0_100,
	DOUT_PCLK_BUS1_100,
	DOUT_PCLK_BUS2_100,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_int_pm_domain[] = {
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"pd-mfcmscl"},
	{"pd-mfcmscl"},
	{NULL},
	{"pd-isp"},
	{"pd-isp"},
	{"pd-isp"},
	{"pd-g3d"},
	{NULL},
	{NULL},
	{NULL},
};

static int exynos7_devfreq_int_set_clk(struct devfreq_data_int *data,
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

void exynos7_int_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_int->use_dvfs)
		return;

	mutex_lock(&data_int->lock);
	cur_freq_idx = devfreq_get_opp_idx(devfreq_int_opp_list,
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

		exynos7_devfreq_int_set_clk(data_int,
						cur_freq_idx,
						devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk,
						devfreq_clk_int_info_list[i]);
	}
	mutex_unlock(&data_int->lock);
}
#endif

static int exynos7_devfreq_int_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_clk); ++i) {
		devfreq_int_clk[i].clk = clk_get(NULL, devfreq_int_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_int_clk[i].clk)) {
			pr_err("DEVFREQ(INT) : %s can't get clock\n", devfreq_int_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("INT clk name: %s, rate: %luMHz\n",
				devfreq_int_clk[i].clk_name, (clk_get_rate(devfreq_int_clk[i].clk) + (MHZ-1))/MHZ);
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i)
		clk_prepare_enable(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk);

	return 0;
}

static int exynos7_devfreq_int_set_volt(struct devfreq_data_int *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_int, volt, volt_range);
	pr_debug("INT: set_volt(%lu), get_volt(%d)\n", volt, regulator_get_voltage(data->vdd_int));
	data->old_volt = volt;
out:
	return 0;
}

static int exynos7_devfreq_int_set_freq(struct devfreq_data_int *data,
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
#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_int_clk, INT_CLK_COUNT, "INT");
#endif
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos7_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
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
			regulator_set_voltage(data->vdd_int, set_volt, REGULATOR_MAX_MICROVOLT);

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
			regulator_set_voltage(data->vdd_int, set_volt, REGULATOR_MAX_MICROVOLT);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos, data->default_qos);
	}
	return NOTIFY_OK;
}
#endif

int exynos7580_devfreq_int_init(void *data)
{
	int ret = 0;

	data_int = (struct devfreq_data_int *)data;
	data_int->max_state = INT_LV_COUNT;

	if (exynos7_devfreq_int_init_clock())
		return -EINVAL;

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_int_pm_domain, ARRAY_SIZE(devfreq_int_pm_domain)))
		return -EINVAL;
#endif
	data_int->int_set_volt = exynos7_devfreq_int_set_volt;
	data_int->int_set_freq = exynos7_devfreq_int_set_freq;
#ifdef CONFIG_EXYNOS_THERMAL
	data_int->tmu_notifier.notifier_call = exynos7_devfreq_int_tmu_notifier;
#endif

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
	ISP_LV_COUNT,
};

enum devfreq_isp_clk {
	ISP_PLL,
	DOUT_SCLK_CPU_ISP_CLKIN,
	DOUT_SCLK_CPU_ISP_ATCLKIN,
	DOUT_SCLK_CPU_ISP_PCLKDBG,
	/* TODO: this gate clk has not been defined in CCF yet */
	//DOUT_ACLK_CSI_LINK0_I_WRAP_CLK,
	DOUT_PCLK_CSI_LINK0_225,
	DOUT_ACLK_LINK_DATA,
	DOUT_ACLK_CSI_LINK1_75,
	DOUT_PCLK_CSI_LINK1_37,
	DOUT_ACLK_FIMC_ISP_450,
	MOUT_ACLK_FIMC_ISP_450_D,
	MOUT_ACLK_FIMC_ISP_450_C,
	MOUT_ACLK_FIMC_ISP_450_B,
	MOUT_ACLK_FIMC_ISP_450_A,
	MOUT_ACLK_ISP_266_USER,
	DOUT_ISP_PLL_DIV2,
	MOUT_BUS_PLL_TOP_USER,
	DOUT_PCLK_FIMC_ISP_225,
	DOUT_ACLK_FIMC_FD_300,
	DOUT_PCLK_FIMC_FD_150,
	DOUT_ACLK_ISP_266,
	DOUT_ACLK_ISP_133,
	DOUT_ACLK_ISP_67,
	ISP_CLK_COUNT,
};

struct devfreq_clk_list devfreq_isp_clk[ISP_CLK_COUNT] = {
	{"isp_pll",},
	{"dout_sclk_cpu_isp_clkin",},
	{"dout_sclk_cpu_isp_atclkin",},
	{"dout_sclk_cpu_isp_pclkdbg",},
	/* TODO: this gate clk has not been defined in CCF yet */
	//{"aclk_csi_link0_i_wrap_clk",},
	{"dout_pclk_csi_link0_225",},
	{"dout_aclk_link_data",},
	{"dout_aclk_csi_link1_75",},
	{"dout_pclk_csi_link1_37",},
	{"dout_aclk_fimc_isp_450",},
	{"mout_aclk_fimc_isp_450_d"},
	{"mout_aclk_fimc_isp_450_c"},
	{"mout_aclk_fimc_isp_450_b"},
	{"mout_aclk_fimc_isp_450_a"},
	{"mout_aclk_isp_266_user"},
	{"dout_isp_pll_div2"},
	{"mout_bus_pll_top_user"},
	{"dout_pclk_fimc_isp_225",},
	{"dout_aclk_fimc_fd_300",},
	{"dout_pclk_fimc_fd_150",},
	{"dout_aclk_isp_266",},
	{"dout_aclk_isp_133",},
	{"dout_aclk_isp_67",},
};

/* aclk_fimc_isp_450*/
struct devfreq_clk_state aclk_mif_fimc_bus_pll[] = {
	{MOUT_ACLK_ISP_266_USER,	MOUT_BUS_PLL_TOP_USER},
	{MOUT_ACLK_FIMC_ISP_450_D,	MOUT_ACLK_ISP_266_USER},
};

struct devfreq_clk_states aclk_mif_fimc_bus_pll_list= {
	.state = aclk_mif_fimc_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_fimc_bus_pll),
};

/* aclk_fimc_isp_450*/
struct devfreq_clk_state aclk_mif_fimc_isp_pll[] = {
	{MOUT_ACLK_FIMC_ISP_450_A,	DOUT_ISP_PLL_DIV2},
	{MOUT_ACLK_FIMC_ISP_450_B,	MOUT_ACLK_FIMC_ISP_450_A},
	{MOUT_ACLK_FIMC_ISP_450_C,	MOUT_ACLK_FIMC_ISP_450_B},
	{MOUT_ACLK_FIMC_ISP_450_D,	MOUT_ACLK_FIMC_ISP_450_C},
};

struct devfreq_clk_states aclk_mif_fimc_isp_pll_list= {
	.state = aclk_mif_fimc_isp_pll,
	.state_count = ARRAY_SIZE(aclk_mif_fimc_isp_pll),
};

struct devfreq_opp_table devfreq_isp_opp_list[] = {
	{ISP_LV0,	530000,	1200000},
	{ISP_LV1,	480000,	1200000},
	{ISP_LV2,	430000,	1200000},
	{ISP_LV3,	400000,	1200000},
	{ISP_LV4,	200000,	1200000},
};

struct devfreq_clk_info sclk_cpu_isp_clkin[] = {
	{ISP_LV0,	530 * MHZ,	0,	NULL},
	{ISP_LV1,	530 * MHZ,	0,	NULL},
	{ISP_LV2,	430 * MHZ,	0,	NULL},
	{ISP_LV3,	430 * MHZ,	0,	NULL},
	{ISP_LV4,	200 * MHZ,	0,	NULL},
};

struct devfreq_clk_info sclk_cpu_isp_atclkin[] = {
	{ISP_LV0,	265 * MHZ,	0,	NULL},
	{ISP_LV1,	265 * MHZ,	0,	NULL},
	{ISP_LV2,	215 * MHZ,	0,	NULL},
	{ISP_LV3,	200 * MHZ,	0,	NULL},
	{ISP_LV4,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info sclk_cpu_isp_pclkdbg[] = {
	{ISP_LV0,	67 * MHZ,	0,	NULL},
	{ISP_LV1,	67 * MHZ,	0,	NULL},
	{ISP_LV2,	54 * MHZ,	0,	NULL},
	{ISP_LV3,	50 * MHZ,	0,	NULL},
	{ISP_LV4,	25 * MHZ,	0,	NULL},
};
/* TODO: this gate clk has not been defined in CCF yet */
/*
struct devfreq_clk_info aclk_fimc_bns_aclk[] = {
	{ISP_LV0,	530 * MHZ,	0,	NULL},
	{ISP_LV1,	530 * MHZ,	0,	NULL},
	{ISP_LV2,	430 * MHZ,	0,	NULL},
	{ISP_LV3,	400 * MHZ,	0,	NULL},
	{ISP_LV4,	200 * MHZ,	0,	NULL},
};
*/
struct devfreq_clk_info pclk_csi_link0_225[] = {
	{ISP_LV0,	265 * MHZ,	0,	NULL},
	{ISP_LV1,	265 * MHZ,	0,	NULL},
	{ISP_LV2,	215 * MHZ,	0,	NULL},
	{ISP_LV3,	200 * MHZ,	0,	NULL},
	{ISP_LV4,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_link_data[] = {
	{ISP_LV0,	400 * MHZ,	0,	NULL},
	{ISP_LV1,	400 * MHZ,	0,	NULL},
	{ISP_LV2,	400 * MHZ,	0,	NULL},
	{ISP_LV3,	267 * MHZ,	0,	NULL},
	{ISP_LV4,	200 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_csi_link1_75[] = {
	{ISP_LV0,	200 * MHZ,	0,	NULL},
	{ISP_LV1,	200 * MHZ,	0,	NULL},
	{ISP_LV2,	200 * MHZ,	0,	NULL},
	{ISP_LV3,	200 * MHZ,	0,	NULL},
	{ISP_LV4,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info pclk_csi_link1_37[] = {
	{ISP_LV0,	100 * MHZ,	0,	NULL},
	{ISP_LV1,	67 * MHZ,	0,	NULL},
	{ISP_LV2,	67 * MHZ,	0,	NULL},
	{ISP_LV3,	67 * MHZ,	0,	NULL},
	{ISP_LV4,	34 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_fimc_isp_450[] = {
	{ISP_LV0,	400 * MHZ,	0,	NULL},
	{ISP_LV1,	267 * MHZ,	0,	NULL},
	{ISP_LV2,	430 * MHZ,	0,	NULL},
	{ISP_LV3,	200 * MHZ,	0,	NULL},
	{ISP_LV4,	200 * MHZ,	0,	NULL},
};

struct devfreq_clk_info pclk_fimc_isp_225[] = {
	{ISP_LV0,	200 * MHZ,	0,	NULL},
	{ISP_LV1,	134 * MHZ,	0,	NULL},
	{ISP_LV2,	215 * MHZ,	0,	NULL},
	{ISP_LV3,	100 * MHZ,	0,	NULL},
	{ISP_LV4,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_fimc_fd_300[] = {
	{ISP_LV0,	267 * MHZ,	0,	NULL},
	{ISP_LV1,	267 * MHZ,	0,	NULL},
	{ISP_LV2,	267 * MHZ,	0,	NULL},
	{ISP_LV3,	200 * MHZ,	0,	NULL},
	{ISP_LV4,	160 * MHZ,	0,	NULL},
};

struct devfreq_clk_info pclk_fimc_fd_150[] = {
	{ISP_LV0,	134 * MHZ,	0,	NULL},
	{ISP_LV1,	134 * MHZ,	0,	NULL},
	{ISP_LV2,	134 * MHZ,	0,	NULL},
	{ISP_LV3,	100 * MHZ,	0,	NULL},
	{ISP_LV4,	80 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_isp_266[] = {
	{ISP_LV0,	267 * MHZ,	0,	NULL},
	{ISP_LV1,	267 * MHZ,	0,	NULL},
	{ISP_LV2,	267 * MHZ,	0,	NULL},
	{ISP_LV3,	200 * MHZ,	0,	NULL},
	{ISP_LV4,	160 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_isp_133[] = {
	{ISP_LV0,	134 * MHZ,	0,	NULL},
	{ISP_LV1,	134 * MHZ,	0,	NULL},
	{ISP_LV2,	134 * MHZ,	0,	NULL},
	{ISP_LV3,	100 * MHZ,	0,	NULL},
	{ISP_LV4,	80 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_isp_67[] = {
	{ISP_LV0,	67 * MHZ,	0,	NULL},
	{ISP_LV1,	67 * MHZ,	0,	NULL},
	{ISP_LV2,	67 * MHZ,	0,	NULL},
	{ISP_LV3,	50 * MHZ,	0,	NULL},
	{ISP_LV4,	40 * MHZ,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_isp_info_list[] = {
	sclk_cpu_isp_clkin,
	sclk_cpu_isp_atclkin,
	sclk_cpu_isp_pclkdbg,
	/* TODO: this gate clk has not been defined in CCF yet */
	//aclk_fimc_bns_aclk,
	pclk_csi_link0_225,
	aclk_link_data,
	aclk_csi_link1_75,
	pclk_csi_link1_37,
	aclk_fimc_isp_450,
	pclk_fimc_isp_225,
	aclk_fimc_fd_300,
	pclk_fimc_fd_150,
	aclk_isp_266,
	aclk_isp_133,
	aclk_isp_67,
};

enum devfreq_isp_clk devfreq_clk_isp_info_idx[] = {
	DOUT_SCLK_CPU_ISP_CLKIN,
	DOUT_SCLK_CPU_ISP_ATCLKIN,
	DOUT_SCLK_CPU_ISP_PCLKDBG,
	/* TODO: this gate clk has not been defined in CCF yet */
	//DOUT_FIMC_BNS_ACLK,
	DOUT_PCLK_CSI_LINK0_225,
	DOUT_ACLK_LINK_DATA,
	DOUT_ACLK_CSI_LINK1_75,
	DOUT_PCLK_CSI_LINK1_37,
	DOUT_ACLK_FIMC_ISP_450,
	DOUT_PCLK_FIMC_ISP_225,
	DOUT_ACLK_FIMC_FD_300,
	DOUT_PCLK_FIMC_FD_150,
	DOUT_ACLK_ISP_266,
	DOUT_ACLK_ISP_133,
	DOUT_ACLK_ISP_67,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_isp_pm_domain[] = {
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	//{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
};

static int exynos7_devfreq_isp_set_clk(struct devfreq_data_isp *data,
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

void exynos7_isp_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on ||!data_isp->use_dvfs)
		return;

	mutex_lock(&data_isp->lock);
	cur_freq_idx = devfreq_get_opp_idx(devfreq_isp_opp_list,
			ARRAY_SIZE(devfreq_isp_opp_list),
			data_isp->devfreq->previous_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_isp->lock);
		pr_err("DEVFREQ(ISP) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_pm_domain); ++i) {
		if (devfreq_isp_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_isp_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos7_devfreq_isp_set_clk(data_isp,
				cur_freq_idx,
				devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk,
				devfreq_clk_isp_info_list[i]);
	}
	mutex_unlock(&data_isp->lock);
}
#endif

static int exynos7_devfreq_isp_set_freq(struct devfreq_data_isp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
	struct clk *isp_pll;

#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif
	//uint32_t tmp;

	/* If both old_idx and target_idx is lower than ISP_LV1,
	 * p, m, s value for ISP_PLL don't need to be changed.
	 */
	if (old_idx <= 1 && target_idx <= 1)
		goto no_change_pms;

	/* Seqeunce to use s/w pll for ISP_PLL
	 * 1. Change OSCCLK as source pll
	 * 2. Set p, m, s value for ISP_PLL
	 * 3. Change ISP_PLL as source pll
	 */
	isp_pll = devfreq_isp_clk[ISP_PLL].clk;

	/* Change OSCCLK as source pll */
	clk_disable(isp_pll);

	/* Set p, m, s value for ISP_PLL
	 * ISP_LV0, ISP_LV1 = 1060MHz
	 * ISP_LV2 = 860MHz
	 * ISP_LV3 = 800MHz
	 * ISP_LV4 = 400MHz
	 */
	if (target_idx <= 1)
		clk_set_rate(isp_pll, 1060000000);
	else if (target_idx == 2)
		clk_set_rate(isp_pll, 860000000);
	else if (target_idx == 3)
		clk_set_rate(isp_pll, 800000000);
	else
		clk_set_rate(isp_pll, 430000000);

	/* Change ISP_PLL as source pll */
	clk_enable(isp_pll);

no_change_pms:

	if (target_idx < old_idx) {
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
			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j)
					clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
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
			if (clk_info->freq != 0)
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j)
					clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
							devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
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
#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_isp_clk, ISP_CLK_COUNT, "ISP");
#endif
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos7_devfreq_isp_tmu_notifier(struct notifier_block *nb, unsigned long event,
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

			prev_volt = regulator_get_voltage(data->vdd_isp_cam0);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_isp_cam0, set_volt, REGULATOR_MAX_MICROVOLT);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_isp_cam0);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_isp_cam0, set_volt, REGULATOR_MAX_MICROVOLT);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_isp_thermal_qos))
			pm_qos_update_request(&min_isp_thermal_qos, data->default_qos);
	}
	return NOTIFY_OK;
}
#endif

static int exynos7_devfreq_isp_set_volt(struct devfreq_data_isp *data,
		unsigned long volt,
		unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_isp_cam0, volt, volt_range);
	pr_debug("ISP: set_volt(%lu), get_volt(%d)\n", volt, regulator_get_voltage(data->vdd_isp_cam0));
	data->old_volt = volt;
out:
	return 0;
}

static int exynos7_devfreq_isp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_clk); ++i) {
		devfreq_isp_clk[i].clk = __clk_lookup(devfreq_isp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_isp_clk[i].clk)) {
			pr_err("DEVFREQ(ISP) : %s can't get clock\n", devfreq_isp_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("ISP clk name: %s, rate: %luMhz\n",
				devfreq_isp_clk[i].clk_name, (clk_get_rate(devfreq_isp_clk[i].clk) + (MHZ-1))/MHZ);
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_isp_info_list); ++i)
		clk_prepare_enable(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk);
	return 0;
}

int exynos7580_devfreq_isp_init(void *data)
{
	int ret = 0;
	data_isp = (struct devfreq_data_isp *)data;
	data_isp->max_state = ISP_LV_COUNT;
	if (exynos7_devfreq_isp_init_clock())
		return -EINVAL;

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_isp_pm_domain,
								ARRAY_SIZE(devfreq_isp_pm_domain)))
		return -EINVAL;
#endif
	data_isp->isp_set_freq = exynos7_devfreq_isp_set_freq;
	data_isp->isp_set_volt = exynos7_devfreq_isp_set_volt;
#ifdef CONFIG_EXYNOS_THERMAL
	data_isp->tmu_notifier.notifier_call = exynos7_devfreq_isp_tmu_notifier;
#endif

	return ret;
}
int exynos7580_devfreq_isp_deinit(void *data)
{
	return 0;
}
/* end of ISP related function */

/* ========== 3. MIF related function */

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
	MIF_LV_COUNT,
};

enum devfreq_mif_clk {
	MEM0_PLL,
	MOUT_MEDIA_PLL_DIV2,
	DOUT_ACLK_MIF_400,
	MOUT_ACLK_MIF_400,
	BUS_PLL,
	DOUT_ACLK_MIF_200,
	DOUT_ACLK_MIF_100,
	MOUT_ACLK_MIF_100,
	DOUT_ACLK_MIF_FIX_100,
	DOUT_ACLK_DISP_200,
	MIF_CLK_COUNT,
};

struct devfreq_clk_list devfreq_mif_clk[MIF_CLK_COUNT] = {
	{"mem0_pll"},
	{"mout_media_pll_div2"},
	{"dout_aclk_mif_400"},
	{"mout_aclk_mif_400"},
	{"bus_pll"},
	{"dout_aclk_mif_200"},
	{"dout_aclk_mif_100"},
	{"mout_aclk_mif_100"},
	{"dout_aclk_mif_fix_100"},
	{"dout_aclk_disp_200"},
};

struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{MIF_LV0,	825000,	1200000},
	{MIF_LV1,	741000,	1200000},
	{MIF_LV2,	728000,	1200000},
	{MIF_LV3,	667000,	1200000},
	{MIF_LV4,	559000,	1200000},
	{MIF_LV5,	416000,	1200000},
	{MIF_LV6,	338000,	1200000},
	{MIF_LV7,	273000,	1200000},
	{MIF_LV8,	200000,	1200000},
};

struct devfreq_clk_state aclk_mif_400_bus_pll[] = {
	{MOUT_ACLK_MIF_400,	BUS_PLL},
};

struct devfreq_clk_states aclk_mif_400_bus_pll_list= {
	.state = aclk_mif_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_400_bus_pll),
};

struct devfreq_clk_state aclk_mif_400_media_pll[] = {
	{MOUT_ACLK_MIF_400,	MOUT_MEDIA_PLL_DIV2},
};

struct devfreq_clk_states aclk_mif_400_media_pll_list= {
	.state = aclk_mif_400_media_pll,
	.state_count = ARRAY_SIZE(aclk_mif_400_media_pll),
};

struct devfreq_clk_state aclk_mif_100_bus_pll[] = {
	{MOUT_ACLK_MIF_100,	BUS_PLL},
};

struct devfreq_clk_states aclk_mif_100_bus_pll_list= {
	.state = aclk_mif_100_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_100_bus_pll),
};

struct devfreq_clk_state aclk_mif_100_media_pll[] = {
	{MOUT_ACLK_MIF_100,	MOUT_MEDIA_PLL_DIV2},
};

struct devfreq_clk_states aclk_mif_100_media_pll_list= {
	.state = aclk_mif_100_media_pll,
	.state_count = ARRAY_SIZE(aclk_mif_100_media_pll),
};

struct devfreq_clk_info sclk_clk_phy[] = {
	{MIF_LV0,	825 * MHZ,	0,	NULL},
	{MIF_LV1,	825 * MHZ,	0,	NULL},
	{MIF_LV2,	728 * MHZ,	0,	NULL},
	{MIF_LV3,	667 * MHZ,	0,	NULL},
	{MIF_LV4,	559 * MHZ,	0,	NULL},
	{MIF_LV5,	416 * MHZ,	0,	NULL},
	{MIF_LV6,	338 * MHZ,	0,	NULL},
	/* FIXME: MIF_LV7 is orginally set as 273MHz. But it seems like not working with 273MHz now */
	{MIF_LV7,	273 * MHZ,	0,	NULL},
	/* FIXME: MIF_LV8 is orginally set as 200MHz. But it seems like not working with 200MHz now */
	{MIF_LV8,	200 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_mif_400[] = {
	{MIF_LV0,	400 * MHZ,	0,	&aclk_mif_400_bus_pll_list},
	{MIF_LV1,	400 * MHZ,	0,	&aclk_mif_400_bus_pll_list},
	{MIF_LV2,	334 * MHZ,	0,	&aclk_mif_400_media_pll_list},
	{MIF_LV3,	267 * MHZ,	0,	&aclk_mif_400_bus_pll_list},
	{MIF_LV4,	223 * MHZ,	0,	&aclk_mif_400_media_pll_list},
	{MIF_LV5,	167 * MHZ,	0,	&aclk_mif_400_media_pll_list},
	{MIF_LV6,	160 * MHZ,	0,	&aclk_mif_400_bus_pll_list},
	{MIF_LV7,	112 * MHZ,	0,	&aclk_mif_400_media_pll_list},
	{MIF_LV8,	96 * MHZ,	0,	&aclk_mif_400_media_pll_list},
};
struct devfreq_clk_info aclk_mif_200[] = {
	{MIF_LV0,	200 * MHZ,	0,	NULL},
	{MIF_LV1,	200 * MHZ,	0,	NULL},
	{MIF_LV2,	167 * MHZ,	0,	NULL},
	{MIF_LV3,	134 * MHZ,	0,	NULL},
	{MIF_LV4,	112 * MHZ,	0,	NULL},
	{MIF_LV5,	84 * MHZ,	0,	NULL},
	{MIF_LV6,	80 * MHZ,	0,	NULL},
	{MIF_LV7,	56 * MHZ,	0,	NULL},
	{MIF_LV8,	48 * MHZ,	0,	NULL},
};
struct devfreq_clk_info aclk_mif_100[] = {
	{MIF_LV0,	100 * MHZ,	0,	&aclk_mif_100_bus_pll_list},
	{MIF_LV1,	100 * MHZ,	0,	&aclk_mif_100_bus_pll_list},
	{MIF_LV2,	84 * MHZ,	0,	&aclk_mif_100_media_pll_list},
	{MIF_LV3,	84 * MHZ,	0,	&aclk_mif_100_media_pll_list},
	{MIF_LV4,	89 * MHZ,	0,	&aclk_mif_100_bus_pll_list},
	{MIF_LV5,	89 * MHZ,	0,	&aclk_mif_100_bus_pll_list},
	{MIF_LV6,	67 * MHZ,	0,	&aclk_mif_100_media_pll_list},
	{MIF_LV7,	67 * MHZ,	0,	&aclk_mif_100_media_pll_list},
	{MIF_LV8,	61 * MHZ,	0,	&aclk_mif_100_media_pll_list},
};
struct devfreq_clk_info aclk_mif_fix_100[] = {
	{MIF_LV0,	100 * MHZ,	0,	NULL},
	{MIF_LV1,	100 * MHZ,	0,	NULL},
	{MIF_LV2,	100 * MHZ,	0,	NULL},
	{MIF_LV3,	100 * MHZ,	0,	NULL},
	{MIF_LV4,	100 * MHZ,	0,	NULL},
	{MIF_LV5,	100 * MHZ,	0,	NULL},
	{MIF_LV6,	100 * MHZ,	0,	NULL},
	{MIF_LV7,	100 * MHZ,	0,	NULL},
	{MIF_LV8,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_disp_200[] = {
	{MIF_LV0,	267 * MHZ,	0,	NULL},
	{MIF_LV1,	267 * MHZ,	0,	NULL},
	{MIF_LV2,	267 * MHZ,	0,	NULL},
	{MIF_LV3,	267 * MHZ,	0,	NULL},
	{MIF_LV4,	200 * MHZ,	0,	NULL},
	{MIF_LV5,	200 * MHZ,	0,	NULL},
	{MIF_LV6,	160 * MHZ,	0,	NULL},
	{MIF_LV7,	160 * MHZ,	0,	NULL},
	{MIF_LV8,	160 * MHZ,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_mif_info_list[] = {
	aclk_mif_400,
	aclk_mif_200,
	aclk_mif_100,
	aclk_mif_fix_100,
	aclk_disp_200,
};

enum devfreq_mif_clk devfreq_clk_mif_info_idx[] = {
	DOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_200,
	DOUT_ACLK_MIF_100,
	DOUT_ACLK_MIF_FIX_100,
	DOUT_ACLK_DISP_200,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_mif_pm_domain[] = {
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"pd-disp"},
};

static int exynos7_devfreq_mif_set_clk(struct devfreq_data_mif *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info[target_idx].states;

	if (clk_get_rate(clk) < clk_info[target_idx].freq) {
		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_mif_clk[clk_states->state[i].clk_idx].clk,
					devfreq_mif_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	} else {
		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);

		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_mif_clk[clk_states->state[i].clk_idx].clk,
					devfreq_mif_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info[target_idx].freq != 0)
			clk_set_rate(clk, clk_info[target_idx].freq);
	}
	return 0;
}

void exynos7_mif_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_mif->use_dvfs)
		return;

	mutex_lock(&data_mif->lock);
	cur_freq_idx = devfreq_get_opp_idx(devfreq_mif_opp_list,
                                                ARRAY_SIZE(devfreq_mif_opp_list),
                                                data_mif->devfreq->previous_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_mif->lock);
		pr_err("DEVFREQ(MIF) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_pm_domain); ++i) {
		if (devfreq_mif_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_mif_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos7_devfreq_mif_set_clk(data_mif,
						cur_freq_idx,
						devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk,
						devfreq_clk_mif_info_list[i]);
	}
	mutex_unlock(&data_mif->lock);
}
#endif
/* DIV and MUX Setting for area of DREX_PAUSE */
struct devfreq_clk_value aclk_clk_phy_825[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_741[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_728[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_667[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_559[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_416[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_338[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_273[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value aclk_clk_phy_200[] = {
	/* Using MEM0_PLL for source pll, both divides are set as 1
	 * CLK2X_PHY_B_SEL: 0, CLKM_PHY_B_SEL: 0
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, 0,
	((0xF << 20) | (0xF << 16) | (0x1 << 12) | (0x1 << 4))},

	/* Using MEDIA_PLL for source pll, both dividers are set as 1
	 * CLK2X_PHY_B_SEL: 1, CLKM_PHY_B_SEL: 1
	 * CLK2X_PHY_RATIO: 0, CLKM_PHY_RATIO: 0
	 */
	{0x1000, ((0x1 << 12) | (0x1 << 4)),
	((0xF << 20) | (0xF << 16))},
};

struct devfreq_clk_value *aclk_clk_phy_list[] = {
	aclk_clk_phy_825,
	aclk_clk_phy_741,
	aclk_clk_phy_728,
	aclk_clk_phy_667,
	aclk_clk_phy_559,
	aclk_clk_phy_416,
	aclk_clk_phy_338,
	aclk_clk_phy_273,
	aclk_clk_phy_200,
};

struct dmc_drex_dfs_mif_table {
	uint32_t drex_timingrfcpb;
	uint32_t drex_timingrow;
	uint32_t drex_timingdata;
	uint32_t drex_timingpower;
	uint32_t drex_rd_fetch;
};

struct dmc_phy_dfs_mif_table {
	uint32_t phy_dvfs_con0_set1;
	uint32_t phy_dvfs_con0_set0;
	uint32_t phy_dvfs_con0_set1_mask;
	uint32_t phy_dvfs_con0_set0_mask;
	uint32_t phy_dvfs_con2_set1;
	uint32_t phy_dvfs_con2_set0;
	uint32_t phy_dvfs_con2_set1_mask;
	uint32_t phy_dvfs_con2_set0_mask;
	uint32_t phy_dvfs_con3_set1;
	uint32_t phy_dvfs_con3_set0;
	uint32_t phy_dvfs_con3_set1_mask;
	uint32_t phy_dvfs_con3_set0_mask;
};

#define DREX_TIMING_PARA(rfcpb, row, data, power, rdfetch) \
{ \
	.drex_timingrfcpb	= rfcpb, \
	.drex_timingrow	= row, \
	.drex_timingdata	= data, \
	.drex_timingpower	= power, \
	.drex_rd_fetch	= rdfetch, \
}

#define PHY_DVFS_CON(con0_set1, con0_set0, con0_set1_mask, con0_set0_mask, \
		con2_set1, con2_set0, con2_set1_mask, con2_set0_mask, \
		con3_set1, con3_set0, con3_set1_mask, con3_set0_mask) \
{ \
	.phy_dvfs_con0_set1	= con0_set1, \
	.phy_dvfs_con0_set0	= con0_set0, \
	.phy_dvfs_con0_set1_mask	= con0_set1_mask, \
	.phy_dvfs_con0_set0_mask	= con0_set0_mask, \
	.phy_dvfs_con2_set1	= con2_set1, \
	.phy_dvfs_con2_set0	= con2_set0, \
	.phy_dvfs_con2_set1_mask	= con2_set1_mask, \
	.phy_dvfs_con2_set0_mask	= con2_set0_mask, \
	.phy_dvfs_con3_set1	= con3_set1, \
	.phy_dvfs_con3_set0	= con3_set0, \
	.phy_dvfs_con3_set1_mask	= con3_set1_mask, \
	.phy_dvfs_con3_set0_mask	= con3_set0_mask, \
}

/* PHY DVFS CON SFR BIT DEFINITION */

/* (0x0)|(1<<31)|(1<<27)|(0x3<<24) */
#define PHY_DVFS_CON0_SET1_MASK	0x8B000000

/* (0x0)|(1<<30)|(1<<27)|(0x3<<24) */
#define PHY_DVFS_CON0_SET0_MASK	0x4B000000

/* (0x0)|(0x1F<<24)|(0xFF<<8) */
#define PHY_DVFS_CON2_SET1_MASK	0x1F00FF00

/* (0x0)|(0x1F<<16)|(0xFF<<0) */
#define PHY_DVFS_CON2_SET0_MASK	0x001F00FF

/* (0x0)|(1<<30)|(1<<29)|(0x7<<23) */
#define PHY_DVFS_CON3_SET1_MASK	0x63800000

/* (0x0)|(1<<31)|(1<<28)|(0x7<<20) */
#define PHY_DVFS_CON3_SET0_MASK	0x90700000

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x19, 0x36588652, 0x4740185E, 0x4C3A4746, 0x3),
	DREX_TIMING_PARA(0x19, 0x36588652, 0x4740185E, 0x4C3A4746, 0x3),
	DREX_TIMING_PARA(0x16, 0x30478590, 0x3630185E, 0x44333636, 0x2),
	DREX_TIMING_PARA(0x15, 0x2C47754F, 0x3630184E, 0x402F3635, 0x2),
	DREX_TIMING_PARA(0x11, 0x2536644D, 0x3630184E, 0x34283535, 0x2),
	DREX_TIMING_PARA(0xD, 0x1C34534A, 0x2620183E, 0x281F2425, 0x2),
	DREX_TIMING_PARA(0xB, 0x192442C8, 0x2620182E, 0x201F2325, 0x2),
	DREX_TIMING_PARA(0x9, 0x19233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x6, 0x19223185, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_phy_dfs_mif_table dfs_phy_mif_table[] = {
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E002100, 0x000E0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E002100, 0x000E0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E002100, 0x000E0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E001100, 0x000E0011,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E001100, 0x000E0011,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E000100, 0x000E0001,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E009100, 0x000E0091,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E009100, 0x000E0091,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E009100, 0x000E0091,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
};

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x000C0065,
	RATE_HALF = 0x00060032,
	RATE_QUARTER = 0x00030019,
};

static struct workqueue_struct *devfreq_mif_thermal_wq_ch;
static struct workqueue_struct *devfreq_mif_thermal_wq_logging;
struct devfreq_thermal_work devfreq_mif_ch_work = {
	.polling_period = 1000,
};

struct devfreq_thermal_work devfreq_mif_thermal_logging_work = {
	.polling_period = 100,
};

static int exynos7580_devfreq_mif_set_dll(struct devfreq_data_mif *data,
						int target_idx)
{
	uint32_t reg;

	if (target_idx <= DLL_LOCK_LV) {
		/* DLL ON */
		reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
		reg |= (0x1 << CTRL_DLL_ON_SHIFT);
		__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);
	} else {
		/* Write stored lock value to ctrl_force register */
		reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
		reg &= ~(PHY_CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
		reg |= (data->dll_lock_value << CTRL_FORCE_SHIFT);
		__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

		/* DLL OFF */
		reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
		reg &= ~(0x1 << CTRL_DLL_ON_SHIFT);
		__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);
	}

	return 0;
}

static bool timing_set_num;

static int exynos7580_devfreq_mif_timing_set(struct devfreq_data_mif *data,
		int target_idx)
{
	struct dmc_drex_dfs_mif_table *cur_drex_param;
	struct dmc_phy_dfs_mif_table *cur_phy_param;
	uint32_t reg;

	cur_drex_param = &dfs_drex_mif_table[target_idx];
	cur_phy_param = &dfs_phy_mif_table[target_idx];

	/* Check what timing_set_num needs to be used */
	timing_set_num = (((__raw_readl(data->base_drex + DREX_PHYSTATUS)
						>> TIMING_SET_SW_SHIFT) & 0x1) == 0);

	if (timing_set_num) {
		/* DREX */
		reg = __raw_readl(data->base_drex + DREX_TIMINGRFCPB);
		reg &= ~(DREX_TIMINGRFCPB_SET1_MASK);
		reg |= ((cur_drex_param->drex_timingrfcpb << 8)
					& DREX_TIMINGRFCPB_SET1_MASK);
		__raw_writel(reg, data->base_drex + DREX_TIMINGRFCPB);
		__raw_writel(cur_drex_param->drex_timingrow,
				data->base_drex + DREX_TIMINGROW_1);
		__raw_writel(cur_drex_param->drex_timingdata,
				data->base_drex + DREX_TIMINGDATA_1);
		__raw_writel(cur_drex_param->drex_timingpower,
				data->base_drex + DREX_TIMINGPOWER_1);
		__raw_writel(cur_drex_param->drex_rd_fetch,
				data->base_drex + DREX_RDFETCH_1);
		reg = __raw_readl(data->base_mif + 0x1004);
		reg |= 0x1;
		__raw_writel(reg, data->base_mif + 0x1004);

		/* PHY */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON0);
		reg &= ~(cur_phy_param->phy_dvfs_con0_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con0_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON0);

		/* Check whether DLL needs to be turned on or off */
		exynos7580_devfreq_mif_set_dll(data, target_idx);

		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON2);
		reg &= ~(cur_phy_param->phy_dvfs_con2_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con2_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON2);
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON3);
		reg &= ~(cur_phy_param->phy_dvfs_con3_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con3_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON3);
	} else {
		/* DREX */
		reg = __raw_readl(data->base_drex + DREX_TIMINGRFCPB);
		reg &= ~(DREX_TIMINGRFCPB_SET0_MASK);
		reg |= (cur_drex_param->drex_timingrfcpb
					& DREX_TIMINGRFCPB_SET0_MASK);
		__raw_writel(reg, data->base_drex + DREX_TIMINGRFCPB);
		__raw_writel(cur_drex_param->drex_timingrow,
				data->base_drex + DREX_TIMINGROW_0);
		__raw_writel(cur_drex_param->drex_timingdata,
				data->base_drex + DREX_TIMINGDATA_0);
		__raw_writel(cur_drex_param->drex_timingpower,
				data->base_drex + DREX_TIMINGPOWER_0);
		__raw_writel(cur_drex_param->drex_rd_fetch,
				data->base_drex + DREX_RDFETCH_0);
		reg = __raw_readl(data->base_mif + 0x1004);
		reg &= ~0x1;
		__raw_writel(reg, data->base_mif + 0x1004);

		/* PHY */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON0);
		reg &= ~(cur_phy_param->phy_dvfs_con0_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con0_set0;

		/* Check whether DLL needs to be turned on or off */
		exynos7580_devfreq_mif_set_dll(data, target_idx);

		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON0);
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON2);
		reg &= ~(cur_phy_param->phy_dvfs_con2_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con2_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON2);
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON3);
		reg &= ~(cur_phy_param->phy_dvfs_con3_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con3_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON3);
	}
	return 0;
}

int exynos7_devfreq_mif_init_parameter(struct devfreq_data_mif *data)
{
	data->base_drex = devm_ioremap(data->dev, DREX_BASE, SZ_64K);
	data->base_lpddr_phy = devm_ioremap(data->dev, PHY_BASE, SZ_64K);
	data->base_mif = devm_ioremap(data->dev, CMU_MIF_BASE, SZ_64K);

	return 0;
}

static int exynos7_devfreq_mif_set_volt(struct devfreq_data_mif *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_mif, volt, volt_range);
	pr_debug("MIF: set_volt(%lu), get_volt(%d)\n", volt, regulator_get_voltage(data->vdd_mif));
	data->old_volt = volt;
out:
	return 0;
}

static void exynos7580_devfreq_waiting_pause(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	while ((__raw_readl(data->base_mif + CLK_PAUSE) & CLK_PAUSE_MASK) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait pause completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static void exynos7580_devfreq_waiting_mux(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	while ((__raw_readl(data->base_mif + CLK_MUX_STAT_MIF4)
				& CLK_MUX_STAT_MIF4_MASK) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait mux completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
	timeout = 1000;
	while ((__raw_readl(data->base_mif + CLK_DIV_STAT_MIF0)
				& CLK_DIV_STAT_MIF0_MASK) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait divider completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static int exynos7_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	int pll_safe_idx = -1;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
	unsigned int voltage = 0;
	struct clk *mem0_pll;
	uint32_t reg;

#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif

	/* Check whether voltage and freq need to be change or not */
	if (old_idx <= 1 && target_idx <= 1)
		goto no_change_freq;

	/* Enable PAUSE */
	__raw_writel(0x1, data->base_mif + CLK_PAUSE);

	/* Disable Clock Gating */
	__raw_writel(0x0, data->base_drex + DREX_CGCONTROL);

	/* Set PHY ctrl_ref as 0xE */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg &= ~(0xF << 1);
	reg |= (0xE << 1);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	mem0_pll = devfreq_mif_clk[MEM0_PLL].clk;
	pll_safe_idx = data->pll_safe_idx;

	/* Find the proper voltage to be set */
	if ((data->pll_safe_idx <= old_idx) && (data->pll_safe_idx <= target_idx))
		voltage = devfreq_mif_opp_list[pll_safe_idx].volt;
	else if (old_idx > target_idx)
		voltage = devfreq_mif_opp_list[target_idx].volt;
	else
		voltage = 0;

	/* Set voltage */
	if (voltage)
		exynos7_devfreq_mif_set_volt(data, voltage, REGULATOR_MAX_MICROVOLT);

	/* If frequency needs to be higher,
	 * 1. Set PHY Clock
	 * 2. Set BUS Clock
	 */
	if (target_idx < old_idx) {
		/* Set DREX and PHY value for MEDIA_PLL which is always 667MHz */
		exynos7580_devfreq_mif_timing_set(data, data->pll_safe_idx);

		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_C as 1 to use MEDIA_PLL */
		reg = __raw_readl(data->base_mif +
					aclk_clk_phy_list[target_idx][1].reg);
		reg &= ~(aclk_clk_phy_list[target_idx][1].clr_value);
		reg |= aclk_clk_phy_list[target_idx][1].set_value;
		__raw_writel(reg, data->base_mif +
					aclk_clk_phy_list[target_idx][1].reg);

		exynos7580_devfreq_waiting_pause(data);
		exynos7580_devfreq_waiting_mux(data);

		/* Change p, m, s value for mem0_pll */
		clk_set_rate(mem0_pll, sclk_clk_phy[target_idx].freq);

		/* Set DREX and PHY value for MEM0_PLL */
		exynos7580_devfreq_mif_timing_set(data, target_idx);

		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_C as 0 to use MEM0_PLL */
		reg = __raw_readl(data->base_mif +
					aclk_clk_phy_list[target_idx][0].reg);
		reg &= ~(aclk_clk_phy_list[target_idx][0].clr_value);
		reg |= aclk_clk_phy_list[target_idx][0].set_value;
		__raw_writel(reg, data->base_mif +
					aclk_clk_phy_list[target_idx][0].reg);

		exynos7580_devfreq_waiting_pause(data);
		exynos7580_devfreq_waiting_mux(data);

		/* Set frequency */
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
			clk_info = &devfreq_clk_mif_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_mif_pm_domain[i].pm_domain;
			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			/* If source of MUX_ACLK_MIF is bus_pll, setting DIV and MUX orderly */
			if ((__raw_readl(data->base_drex + CLK_MUX_STAT_MIF5) | 0x1) == 1) {
				if (clk_info->freq != 0) {
					clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
					pr_debug("MIF clk name: %s, set_rate: %lu, get_rate: %lu\n",
							devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk_name,
							clk_info->freq, clk_get_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk));
				}
				if (clk_states) {
					for (j = 0; j < clk_states->state_count; ++j) {
						clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
							devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
					}
				}
			}
			/* If source of MUX_ACLK_MIF is media_pll, setting MUX and DIV orderly */
			else if ((__raw_readl(data->base_drex + CLK_MUX_STAT_MIF5) | 0x10) == 1) {
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

#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}
	/* If frequency needs to be lower,
	 * 1. Set BUS Clock
	 * 2. Set PHY Clock
	 */
	else {
		/* Set DREX and PHY value for MEDIA_PLL which is always 667MHz */
		exynos7580_devfreq_mif_timing_set(data, data->pll_safe_idx);

		/* Set frequency */
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
				clk_info = &devfreq_clk_mif_info_list[i][target_idx];
				clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_mif_pm_domain[i].pm_domain;
			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			/* If source of MUX_ACLK_MIF is bus_pll, setting DIV and MUX orderly */
			if ((__raw_readl(data->base_drex + CLK_MUX_STAT_MIF5) | 0x1) == 1) {
					if (clk_info->freq != 0) {
							clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
							pr_debug("MIF clk name: %s, set_rate: %lu, get_rate: %lu\n",
											devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk_name,
											clk_info->freq, clk_get_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk));
					}
					if (clk_states) {
							for (j = 0; j < clk_states->state_count; ++j) {
									clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
													devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
							}
					}
			}
			/* If source of MUX_ACLK_MIF is media_pll, setting MUX and DIV orderly */
			else if ((__raw_readl(data->base_drex + CLK_MUX_STAT_MIF5) | 0x10) == 1) {
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
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}

		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_C as 0 to use MEDIA_PLL */
		reg = __raw_readl(data->base_mif +
					aclk_clk_phy_list[target_idx][1].reg);
		reg &= ~(aclk_clk_phy_list[target_idx][1].clr_value);
		reg |= aclk_clk_phy_list[target_idx][1].set_value;
		__raw_writel(reg, data->base_mif +
					aclk_clk_phy_list[target_idx][1].reg);

		exynos7580_devfreq_waiting_pause(data);
		exynos7580_devfreq_waiting_mux(data);

		/* Change p, m, s value for mem0_pll */
		clk_set_rate(mem0_pll, sclk_clk_phy[target_idx].freq);

		/* Set DREX and PHY value for MEM0_PLL */
		exynos7580_devfreq_mif_timing_set(data, target_idx);

		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_C as 0 to use MEM0_PLL */
		reg = __raw_readl(data->base_mif +
					aclk_clk_phy_list[target_idx][0].reg);
		reg &= ~(aclk_clk_phy_list[target_idx][0].clr_value);
		reg |= aclk_clk_phy_list[target_idx][0].set_value;
		__raw_writel(reg, data->base_mif +
					aclk_clk_phy_list[target_idx][0].reg);

		exynos7580_devfreq_waiting_pause(data);
		exynos7580_devfreq_waiting_mux(data);
	}

	/* Find the proper voltage to be set */
	if ((data->pll_safe_idx <= old_idx) && (data->pll_safe_idx <= target_idx))
		voltage = devfreq_mif_opp_list[target_idx].volt;
	else if (old_idx < target_idx)
		voltage = devfreq_mif_opp_list[target_idx].volt;
	else
		voltage = 0;

	/* Set voltage */
	if (voltage)
		exynos7_devfreq_mif_set_volt(data, voltage, REGULATOR_MAX_MICROVOLT);

	/* Enable Clock Gating */
	__raw_writel(0x3FF, data->base_drex + DREX_CGCONTROL);

	/* Set PHY ctrl_ref as 0xF */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg |= (0xF << 1);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

no_change_freq:
#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_mif_clk, MIF_CLK_COUNT);
#endif
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
extern struct pm_qos_request min_mif_thermal_qos;
int exynos7_devfreq_mif_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_mif *data = container_of(nb, struct devfreq_data_mif,
							tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

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
			regulator_set_voltage(data->vdd_mif, set_volt, REGULATOR_MAX_MICROVOLT);

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
			regulator_set_voltage(data->vdd_mif, set_volt, REGULATOR_MAX_MICROVOLT);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_mif_thermal_qos))
			pm_qos_update_request(&min_mif_thermal_qos,
					data->default_qos);
	}
	return NOTIFY_OK;
}
#endif

static void exynos7_devfreq_thermal_event(struct devfreq_thermal_work *work)
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

static void exynos7_devfreq_swtrip(void)
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

static void exynos7_devfreq_thermal_logging(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int timingaref_value;
	unsigned int cs0_thermal_level = 0;
	unsigned int cs1_thermal_level = 0;
	unsigned int mrstatus = 0;

	void __iomem *base_drex = NULL;

	base_drex = data_mif->base_drex;

	mutex_lock(&data_mif->lock);

	__raw_writel(0x09001000, base_drex + DREX_DIRECTCMD);
	mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
	cs0_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);

	__raw_writel(0x09101000, base_drex + DREX_DIRECTCMD);
	mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
	cs1_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);

	timingaref_value = __raw_readl(base_drex + DREX_TIMINGAREF);

	mutex_unlock(&data_mif->lock);

	exynos_ss_printk("[MIF]%d,%d(0x%08x);0x%08x\n",
			cs0_thermal_level, cs1_thermal_level, mrstatus,
			timingaref_value);

	exynos7_devfreq_thermal_event(thermal_work);
}

static void exynos7_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0;
	unsigned int timingaref_value = RATE_ONE;
	void __iomem *base_drex = NULL;

	base_drex = data_mif->base_drex;

	mutex_lock(&data_mif->lock);

	__raw_writel(0x09001000, base_drex + DREX_DIRECTCMD);
	mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
	tmp_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs0 = tmp_thermal_level;

	__raw_writel(0x09101000, base_drex + DREX_DIRECTCMD);
	mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
	tmp_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs1 = tmp_thermal_level;

	mutex_unlock(&data_mif->lock);

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
		break;
	case 5:
		timingaref_value = RATE_QUARTER;
		thermal_work->polling_period = 100;
		break;

	case 6:
		exynos7_devfreq_swtrip();
		return;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return;
	}

	exynos_ss_printk("[MIF]%d,%d;VT_MON_CON:0x%08x;TIMINGAREF:0x%08x",
			thermal_work->thermal_level_cs0, thermal_work->thermal_level_cs1,
			__raw_readl(base_drex + DREX_TIMINGAREF));

	exynos7_devfreq_thermal_event(thermal_work);
}

void exynos7_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq_ch = create_freezable_workqueue("devfreq_thermal_wq_ch");
	devfreq_mif_thermal_wq_logging = create_freezable_workqueue("devfreq_thermal_wq_logging");

	INIT_DELAYED_WORK(&devfreq_mif_ch_work.devfreq_mif_thermal_work,
			exynos7_devfreq_thermal_monitor);
	INIT_DELAYED_WORK(&devfreq_mif_thermal_logging_work.devfreq_mif_thermal_work,
			exynos7_devfreq_thermal_logging);

	devfreq_mif_ch_work.work_queue = devfreq_mif_thermal_wq_ch;
	devfreq_mif_thermal_logging_work.work_queue = devfreq_mif_thermal_wq_logging;

	exynos7_devfreq_thermal_event(&devfreq_mif_ch_work);
	exynos7_devfreq_thermal_event(&devfreq_mif_thermal_logging_work);
}

int exynos7_devfreq_mif_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_clk); ++i) {
		devfreq_mif_clk[i].clk = clk_get(NULL, devfreq_mif_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_mif_clk[i].clk)) {
			pr_err("DEVFREQ(MIF) : %s can't get clock\n", devfreq_mif_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("MIF clk name: %s, rate: %luMHz\n",
				devfreq_mif_clk[i].clk_name, (clk_get_rate(devfreq_mif_clk[i].clk) + (MHZ-1))/MHZ);
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i)
		clk_prepare_enable(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk);

	return 0;
}

void exynos7580_devfreq_set_dll_lock_value(struct devfreq_data_mif *data,
						int target_idx)
{
	uint32_t reg;
	uint32_t lock_value;
	int pll_safe_idx;
	struct clk *mem0_pll;

	mem0_pll = devfreq_mif_clk[MEM0_PLL].clk;
	pll_safe_idx = data->pll_safe_idx;

	/* FIXME: Not sure voltage needs to be controlled or not */
	//unsigned int voltage = 0;

	/* PHY and DREX timing_set for 667MHz */
	exynos7580_devfreq_mif_timing_set(data, pll_safe_idx);

	/* Set SCLK_CLKM_PHY and SCLK_CLK2X_PHY as 667MHz */
	reg = __raw_readl(data->base_mif +
					aclk_clk_phy_list[pll_safe_idx][1].reg);
	reg &= ~(aclk_clk_phy_list[pll_safe_idx][1].clr_value);
	reg |= aclk_clk_phy_list[pll_safe_idx][1].set_value;
	__raw_writel(reg, data->base_mif +
					aclk_clk_phy_list[pll_safe_idx][1].reg);

	exynos7580_devfreq_waiting_pause(data);
	exynos7580_devfreq_waiting_mux(data);

	/* Change p, m, s value for mem0_pll */
	clk_set_rate(mem0_pll, sclk_clk_phy[target_idx].freq);

	/* Set DREX and PHY value for 416MHz */
	exynos7580_devfreq_mif_timing_set(data, target_idx);

	/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_C as 0 to use MEM0_PLL */
	reg = __raw_readl(data->base_mif +
				aclk_clk_phy_list[target_idx][0].reg);
	reg &= ~(aclk_clk_phy_list[target_idx][0].clr_value);
	reg |= aclk_clk_phy_list[target_idx][0].set_value;
	__raw_writel(reg, data->base_mif +
				aclk_clk_phy_list[target_idx][0].reg);

	exynos7580_devfreq_waiting_pause(data);
	exynos7580_devfreq_waiting_mux(data);

	/* FIXME: Not sure voltage needs to be controlled or not */
	#if 0
	/* Set volt for 416MHz */
	voltage = devfreq_mif_opp_list[target_idx].volt;
	exynos7_devfreq_mif_set_volt(data, voltage,
					REGULATOR_MAX_MICROVOLT);
	#endif

	/* Sequence to get DLL Lock value
		1. Disable phy_cg_en (PHY clock gating)
		2. Enable ctrl_lock_rden
		3. Disable ctrl_lock_rden
		4. Read ctrl_lock_new
		5. Write ctrl_lock_new to ctrl_force and PMU_SPARE3
		6. Enable phy_cg_en (PHY clock gating)
	*/

	/* 1. Disable PHY Clock Gating */
	reg = __raw_readl(data->base_drex + DREX_CGCONTROL);
	reg &= ~(0x1 << CG_EN_SHIFT);
	__raw_writel(reg, data->base_drex + DREX_CGCONTROL);

	/* 2. Enable ctrl_lock_rden */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg |= (0x1 << CTRL_LOCK_RDEN_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* 3. Disable ctrl_lock_rden */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg &= ~(0x1 << CTRL_LOCK_RDEN_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* 4. Read ctrl_lock_new */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON1);
	lock_value = (reg >> CTRL_LOCK_NEW_SHIFT) & PHY_CTRL_LOCK_NEW_MASK;
	data->dll_lock_value = lock_value;

	/* 5. Write ctrl_lock_new to ctrl_force and PMU_SPARE3*/
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg &= ~(PHY_CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
	reg |= (lock_value << CTRL_FORCE_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);
	__raw_writel(lock_value, EXYNOS_PMU_PMU_SPARE3);

	/* 6. Enable PHY Clock Gating */
	reg = __raw_readl(data->base_drex + DREX_CGCONTROL);
	reg |= (0x1 << CG_EN_SHIFT);
	__raw_writel(reg, data->base_drex + DREX_CGCONTROL);

	/* FIXME: Not sure voltage needs to be controlled or not */
	#if 0
	/* Set volt for 825MHz */
	voltage = devfreq_mif_opp_list[target_idx].volt;
	exynos7_devfreq_mif_set_volt(data, voltage,
					REGULATOR_MAX_MICROVOLT);
	#endif

	/* PHY and DREX timing_set for 667MHz */
	exynos7580_devfreq_mif_timing_set(data, pll_safe_idx);

	/* Set SCLK_CLKM_PHY and SCLK_CLK2X_PHY as 667MHz */
	reg = __raw_readl(data->base_mif +
					aclk_clk_phy_list[pll_safe_idx][1].reg);
	reg &= ~(aclk_clk_phy_list[pll_safe_idx][1].clr_value);
	reg |= aclk_clk_phy_list[pll_safe_idx][1].set_value;
	__raw_writel(reg, data->base_mif +
					aclk_clk_phy_list[pll_safe_idx][1].reg);

	exynos7580_devfreq_waiting_pause(data);
	exynos7580_devfreq_waiting_mux(data);

	/* Change p, m, s value for mem0_pll */
	clk_set_rate(mem0_pll, sclk_clk_phy[0].freq);

	/* PHY and DREX timing_set for 825MHz */
	exynos7580_devfreq_mif_timing_set(data, 0);

	/* Set SCLK_CLKM_PHY and SCLK_CLK2X_PHY as 825MHz */
	reg = __raw_readl(data->base_mif +
					aclk_clk_phy_list[0][0].reg);
	reg &= ~(aclk_clk_phy_list[0][0].clr_value);
	reg |= aclk_clk_phy_list[0][0].set_value;
	__raw_writel(reg, data->base_mif +
					aclk_clk_phy_list[0][0].reg);

	exynos7580_devfreq_waiting_pause(data);
	exynos7580_devfreq_waiting_mux(data);
}

int exynos7580_devfreq_mif_init(void *data)
{
	int ret = 0;

	data_mif = (struct devfreq_data_mif *)data;

	data_mif->max_state = MIF_LV_COUNT;

	data_mif->mif_set_freq = exynos7_devfreq_mif_set_freq;
	data_mif->mif_set_volt = NULL;

	exynos7_devfreq_mif_init_clock();
	exynos7_devfreq_mif_init_parameter(data_mif);
#ifdef CONFIG_EXYNOS_THERMAL
	data_mif->tmu_notifier.notifier_call = exynos7_devfreq_mif_tmu_notifier;
#endif
	data_mif->pll_safe_idx = MIF_LV3;

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_mif_pm_domain,
								ARRAY_SIZE(devfreq_mif_pm_domain)))
		return -EINVAL;
#endif

	return ret;
}

int exynos7580_devfreq_mif_deinit(void *data)
{
	flush_workqueue(devfreq_mif_thermal_wq_ch);
	destroy_workqueue(devfreq_mif_thermal_wq_ch);
	return 0;
}
/* end of MIF related function */
DEVFREQ_INIT_OF_DECLARE(exynos7580_devfreq_int_init, "samsung,exynos7-devfreq-int", exynos7580_devfreq_int_init);
DEVFREQ_INIT_OF_DECLARE(exynos7580_devfreq_mif_init, "samsung,exynos7-devfreq-mif", exynos7580_devfreq_mif_init);
DEVFREQ_INIT_OF_DECLARE(exynos7580_devfreq_isp_init, "samsung,exynos7-devfreq-isp", exynos7580_devfreq_isp_init);
DEVFREQ_DEINIT_OF_DECLARE(exynos7580_devfreq_deinit, "samsung,exynos7-devfreq-mif", exynos7580_devfreq_mif_deinit);
