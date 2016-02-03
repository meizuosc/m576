/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Taikyung yu(taikyung.yu@samsung.com)
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
#include <mach/asv-exynos.h>
#include <mach/apm-exynos.h>

#include "devfreq_exynos.h"
#include "governor.h"

//#define DMC_DQS_MODE0 /* DQS unbalanced */
//#define DMC_DQS_MODE1	/* DQS balanced mode */
#define DMC_DQS_MODE2	/* DQS balanced mode - ODT OFF <= @ 416MHz */

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

int get_volt_of_avail_max_freq(struct device *dev)
{
	unsigned long freq = 0;
	unsigned long last_freq = 0;
	struct opp *opp;
	struct opp *last_opp = NULL;
	int volt_of_avail_max_freq = 0;


	rcu_read_lock();
	do {
		opp = opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp))
			break;

		last_opp = opp;
		last_freq = freq;
		freq++;
	} while (1);

	if (last_opp && !IS_ERR(last_opp))
		volt_of_avail_max_freq = opp_get_voltage(last_opp);

	rcu_read_unlock();

	return volt_of_avail_max_freq;
}


unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset, unsigned int max_volt)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (max_volt && (voltage + volt_offset > max_volt)) {
		pr_info("voltage(%d) + volt_offset(%d) is bigger than max_volt(%d)\n",
				voltage, volt_offset, max_volt);
		return max_volt;
	}

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
static struct devfreq_data_disp *data_disp;

/* ========== 0. DISP related function */
enum devfreq_disp_idx {
	DISP_LV0,
	DISP_LV1,
	DISP_LV2,
	DISP_LV3,
	DISP_LV4,
	DISP_LV5,
	DISP_LV_COUNT,
};

enum devfreq_disp_clk {
	USERMUX_BUS0_PLL_TOP0,
	USERMUX_BUS1_PLL_TOP0,
	USERMUX_CCI_PLL_TOP0,
	USERMUX_MFC_PLL_TOP0,
	FFAC_TOP0_BUS0_PLL_DIV2,
	FFAC_TOP0_BUS1_PLL_DIV2,
	FFAC_TOP0_CCI_PLL_DIV2,
	FFAC_TOP0_MFC_PLL_DIV2,
	MOUT_ACLK_DISP_400,
	DOUT_ACLK_DISP_400,
	DISP_CLK_COUNT,
};

struct devfreq_clk_list devfreq_disp_clk[DISP_CLK_COUNT] = {
	{"usermux_bus0_pll_top0",},
	{"usermux_bus1_pll_top0",},
	{"usermux_cci_pll_top0",},
	{"usermux_mfc_pll_top0",},
	{"ffac_top0_bus0_pll_div2",},
	{"ffac_top0_bus1_pll_div2",},
	{"ffac_top0_cci_pll_div2",},
	{"ffac_top0_mfc_pll_div2"},
	{"mout_aclk_disp_400",},
	{"dout_aclk_disp_400",},
};

struct devfreq_opp_table devfreq_disp_opp_list[] = {
	{DISP_LV0,	400000,	900000},
	{DISP_LV1,	334000,	900000},
	{DISP_LV2,	267000,	900000},
	{DISP_LV3,	200000,	900000},
	{DISP_LV4,	167000,	900000},
	{DISP_LV5,	100000,	900000},
};

struct devfreq_clk_state aclk_disp_400_bus0_pll[] = {
	{MOUT_ACLK_DISP_400,	USERMUX_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_disp_400_bus0_pll_list = {
	.state = aclk_disp_400_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_disp_400_bus0_pll),
};

struct devfreq_clk_state aclk_disp_400_bus1_pll[] = {
	{MOUT_ACLK_DISP_400,	USERMUX_BUS1_PLL_TOP0},
};

struct devfreq_clk_states aclk_disp_400_bus1_pll_list = {
	.state = aclk_disp_400_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_disp_400_bus1_pll),
};

struct devfreq_clk_state aclk_disp_400_cci_pll[] = {
	{MOUT_ACLK_DISP_400,	USERMUX_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_disp_400_cci_pll_list = {
	.state = aclk_disp_400_cci_pll,
	.state_count = ARRAY_SIZE(aclk_disp_400_cci_pll),
};

struct devfreq_clk_state aclk_disp_400_mfc_pll[] = {
	{MOUT_ACLK_DISP_400,	USERMUX_MFC_PLL_TOP0},
};

struct devfreq_clk_states aclk_disp_400_mfc_pll_list = {
	.state = aclk_disp_400_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_disp_400_mfc_pll),
};

struct devfreq_clk_state aclk_disp_400_bus0_pll_div2[] = {
	{MOUT_ACLK_DISP_400,	FFAC_TOP0_BUS0_PLL_DIV2},
};

struct devfreq_clk_states aclk_disp_400_bus0_pll_div2_list = {
	.state = aclk_disp_400_bus0_pll_div2,
	.state_count = ARRAY_SIZE(aclk_disp_400_bus0_pll_div2),
};

struct devfreq_clk_state aclk_disp_400_bus1_pll_div2[] = {
	{MOUT_ACLK_DISP_400,	FFAC_TOP0_BUS1_PLL_DIV2},
};

struct devfreq_clk_states aclk_disp_400_bus1_pll_div2_list = {
	.state = aclk_disp_400_bus1_pll_div2,
	.state_count = ARRAY_SIZE(aclk_disp_400_bus1_pll_div2),
};

struct devfreq_clk_state aclk_disp_400_cci_pll_div2[] = {
	{MOUT_ACLK_DISP_400,	FFAC_TOP0_CCI_PLL_DIV2},
};

struct devfreq_clk_states aclk_disp_400_cci_pll_div2_list = {
	.state = aclk_disp_400_cci_pll_div2,
	.state_count = ARRAY_SIZE(aclk_disp_400_cci_pll_div2),
};

struct devfreq_clk_state aclk_disp_400_mfc_pll_div2[] = {
	{MOUT_ACLK_DISP_400,	FFAC_TOP0_MFC_PLL_DIV2},
};

struct devfreq_clk_states aclk_disp_400_mfc_pll_div2_list = {
	.state = aclk_disp_400_mfc_pll_div2,
	.state_count = ARRAY_SIZE(aclk_disp_400_mfc_pll_div2),
};

struct devfreq_clk_info aclk_disp_400[] = {
	{DISP_LV0,	400 * MHZ,	0,	&aclk_disp_400_bus0_pll_list},
	{DISP_LV1,	334 * MHZ,	0,	&aclk_disp_400_bus1_pll_list},
	{DISP_LV2,	267 * MHZ,	0,	&aclk_disp_400_bus0_pll_list},
	{DISP_LV3,	200 * MHZ,	0,	&aclk_disp_400_bus0_pll_list},
	{DISP_LV4,	167 * MHZ,	0,	&aclk_disp_400_bus1_pll_list},
	{DISP_LV5,	100 * MHZ,	0,	&aclk_disp_400_bus0_pll_div2_list},
};

struct devfreq_clk_info *devfreq_clk_disp_info_list[] = {
	aclk_disp_400,
};

enum devfreq_disp_clk devfreq_clk_disp_info_idx[] = {
	DOUT_ACLK_DISP_400,
};

static int exynos7_devfreq_disp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_clk); ++i) {
		devfreq_disp_clk[i].clk = clk_get(NULL, devfreq_disp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_disp_clk[i].clk)) {
			pr_err("DEVFREQ(DISP) : %s can't get clock\n", devfreq_disp_clk[i].clk_name);
			return -EINVAL;
		}
		pr_debug("DISP clk name: %s, rate: %luMHz\n",
				devfreq_disp_clk[i].clk_name, (clk_get_rate(devfreq_disp_clk[i].clk) + (MHZ-1))/MHZ);
	}
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_disp_pm_domain[] = {
	{"pd-disp",},
};
#endif

static int exynos7_devfreq_disp_set_freq(struct devfreq_data_disp *data,
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
#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_disp_clk, DISP_CLK_COUNT, "DISP");
#endif
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int exynos7_devfreq_disp_set_clk(struct devfreq_data_disp *data,
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

void exynos7_disp_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_disp->use_dvfs)
		return;

	mutex_lock(&data_disp->lock);
	cur_freq_idx = devfreq_get_opp_idx(devfreq_disp_opp_list,
			ARRAY_SIZE(devfreq_disp_opp_list),
			data_disp->cur_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_disp->lock);
		pr_err("DEVFREQ(DISP) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_pm_domain); ++i) {
		if (devfreq_disp_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_disp_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos7_devfreq_disp_set_clk(data_disp,
				cur_freq_idx,
				devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk,
				devfreq_clk_disp_info_list[i]);
	}
	mutex_unlock(&data_disp->lock);
}
#endif

static int exynos7_devfreq_disp_set_volt(struct devfreq_data_disp *data,
		unsigned long volt,
		unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_disp_cam0, volt, volt_range);
	pr_debug("DISP: set_volt(%lu), get_volt(%d)\n", volt, regulator_get_voltage(data->vdd_disp_cam0));
	data->old_volt = volt;
out:
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos7_devfreq_disp_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_disp *data = container_of(nb, struct devfreq_data_disp, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;
	struct opp *target_opp;
	unsigned long freq = 0;

	if (event == TMU_COLD) {
		if (*on) {
			mutex_lock(&data->lock);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			prev_volt = (uint32_t)data->old_volt;

			set_volt = get_limit_voltage(prev_volt, data->volt_offset, data_isp->volt_of_avail_max_freq);
			regulator_set_voltage(data->vdd_disp_cam0, set_volt, REGULATOR_MAX_MICROVOLT);

			freq = data->cur_freq;
			mutex_unlock(&data->lock);
			pr_info("DISP(%lu): set TMU_COLD: %d => (set_volt: %d, get_volt: %d)\n",
					freq, prev_volt, set_volt, regulator_get_voltage(data->vdd_disp_cam0));
		} else {
			mutex_lock(&data->lock);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			rcu_read_lock();
			freq = data->cur_freq;
			target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
			set_volt = (uint32_t)opp_get_voltage(target_opp);
			rcu_read_unlock();

			regulator_set_voltage(data->vdd_disp_cam0, set_volt, REGULATOR_MAX_MICROVOLT);
			data->old_volt = (unsigned long)set_volt;

			mutex_unlock(&data->lock);
			pr_info("DISP(%lu): unset TMU_COLD: (set_volt: %d, get_volt: %d)\n",
					freq, set_volt, regulator_get_voltage(data->vdd_disp_cam0));
		}
	}
	return NOTIFY_OK;
}
#endif

int exynos7420_devfreq_disp_init(struct devfreq_data_disp *data)
{
	int ret = 0;

	data_disp = data;
	data->max_state = DISP_LV_COUNT;
	if (exynos7_devfreq_disp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}
#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_disp_pm_domain, ARRAY_SIZE(devfreq_disp_pm_domain))) {
		ret = -EINVAL;
		goto err_data;
	}
#endif
	data->disp_set_freq = exynos7_devfreq_disp_set_freq;
	data->disp_set_volt = exynos7_devfreq_disp_set_volt;
#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos7_devfreq_disp_tmu_notifier;
#endif
err_data:
	return ret;
}
/* end of DISP related function */

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
	INT_LV8,
	INT_LV9,
	INT_LV10,
	INT_LV11,
	INT_LV_COUNT,
};

enum devfreq_int_clk {
	DOUT_ACLK_BUS0_532,
	DOUT_ACLK_BUS1_532,
	DOUT_ACLK_BUS1_200,
	DOUT_PCLK_BUS01_133,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	DOUT_ACLK_IMEM_100,
	DOUT_ACLK_MFC_532,
	DOUT_ACLK_MSCL_532,
	DOUT_ACLK_PERIS_66,
	DOUT_ACLK_FSYS0_200,
	DOUT_ACLK_FSYS1_200,
	DOUT_ACLK_VPP0_400,
	DOUT_ACLK_VPP1_400,
	DOUT_ACLK_PERIC0_66,
	DOUT_ACLK_PERIC1_66,
	DOUT_ACLK_ISP0_TREX_532,
	DOUT_ACLK_ISP0_ISP0_590,
	DOUT_ACLK_ISP0_TPU_590,
	DOUT_ACLK_ISP1_ISP1_468,
	DOUT_ACLK_ISP1_AHB_117,
	DOUT_ACLK_CAM1_TREX_532,
	DOUT_ACLK_CAM1_SCLVRA_491,
	DOUT_ACLK_CAM1_BNSCSIS_133,
	DOUT_ACLK_CAM1_ARM_668,
	DOUT_ACLK_CAM1_BUSPERI_334,
	DOUT_ACLK_CAM1_NOCP_133,
	MOUT_ACLK_BUS0_532,

	_MOUT_BUS0_PLL_CMUC,
	_BUS1_PLL,
	_CCI_PLL,
	_MFC_PLL,
	_FFAC_MOUT_BUS0_PLL_CMUC_DIV2,
	_FFAC_TOPC_BUS1_PLL_DIV2,
	_FFAC_TOPC_CCI_PLL_DIV2,
	_FFAC_TOPC_MFC_PLL_DIV2,
	_FFAC_MOUT_BUS0_PLL_CMUC_DIV4,
	_FFAC_TOPC_BUS1_PLL_DIV4,
	_FFAC_TOPC_CCI_PLL_DIV4,
	_FFAC_TOPC_MFC_PLL_DIV4,

	_MOUT_BUS1_PLL_CMUC,
	_MOUT_CCI_PLL_CMUC,
	_MOUT_MFC_PLL_CMUC,

	MOUT_ACLK_BUS1_532,
	MOUT_ACLK_BUS1_200,
	MOUT_PCLK_BUS01_133,
	MOUT_ACLK_IMEM_266,
	MOUT_ACLK_IMEM_200,
	MOUT_ACLK_IMEM_100,
	MOUT_ACLK_MFC_532,
	MOUT_ACLK_MSCL_532,
	MOUT_ACLK_PERIS_66,
	MOUT_ACLK_FSYS0_200,
	MOUT_BUS0_PLL_TOP1,
	MOUT_ACLK_FSYS1_200,
	MOUT_ACLK_VPP0_400,
	MOUT_BUS1_PLL_TOP0,
	MOUT_MFC_PLL_TOP0,
	MOUT_BUS0_PLL_TOP0,
	MOUT_CCI_PLL_TOP0,
	MOUT_ACLK_VPP1_400,
	MOUT_ACLK_PERIC0_66,
	MOUT_ACLK_PERIC1_66,
	MOUT_ACLK_ISP0_TREX_532,
	ISP_PLL,
	MOUT_ACLK_ISP0_ISP0_590,
	_CAM_PLL,
	MOUT_ACLK_ISP0_TPU_590,
	MOUT_ACLK_ISP1_ISP1_468,
	MOUT_ACLK_ISP1_AHB_117,
	MOUT_ACLK_CAM1_TREX_532,
	MOUT_ACLK_CAM1_SCLVRA_491,
	MOUT_ACLK_CAM1_BNSCSIS_133,
	MOUT_ACLK_CAM1_ARM_668,
	MOUT_ACLK_CAM1_BUSPERI_334,
	MOUT_ACLK_CAM1_NOCP_133,

	INT_CLK_COUNT,
};

struct devfreq_clk_list devfreq_int_clk[INT_CLK_COUNT] = {
	{"dout_aclk_bus0_532",},
	{"dout_aclk_bus1_532",},
	{"dout_aclk_bus1_200",},
	{"dout_pclk_bus01_133",},
	{"dout_aclk_imem_266",},
	{"dout_aclk_imem_200",},
	{"dout_aclk_imem_100",},
	{"dout_aclk_mfc_532",},
	{"dout_aclk_mscl_532",},
	{"dout_aclk_peris_66",},
	{"dout_aclk_fsys0_200",},
	{"dout_aclk_fsys1_200",},
	{"dout_aclk_vpp0_400",},
	{"dout_aclk_vpp1_400",},
	{"dout_aclk_peric0_66",},
	{"dout_aclk_peric1_66",},
	{"dout_aclk_isp0_trex_532",},
	{"dout_aclk_isp0_isp0_590",},
	{"dout_aclk_isp0_tpu_590",},
	{"dout_aclk_isp1_isp1_468",},
	{"dout_aclk_isp1_ahb_117",},
	{"dout_aclk_cam1_trex_532",},
	{"dout_aclk_cam1_sclvra_491",},
	{"dout_aclk_cam1_bnscsis_133",},
	{"dout_aclk_cam1_arm_668",},
	{"dout_aclk_cam1_busperi_334",},
	{"dout_aclk_cam1_nocp_133",},
	{"mout_aclk_bus0_532",},
	{"mout_bus0_pll_cmuc"},
	{"bus1_pll"},
	{"cci_pll"},
	{"mfc_pll"},
	{"ffac_mout_bus0_pll_cmuc_div2"},
	{"ffac_topc_bus1_pll_div2"},
	{"ffac_topc_cci_pll_div2"},
	{"ffac_topc_mfc_pll_div2"},
	{"ffac_mout_bus0_pll_cmuc_div4"},
	{"ffac_topc_bus1_pll_div4"},
	{"ffac_topc_cci_pll_div4"},
	{"ffac_topc_mfc_pll_div4"},
	{"mout_bus1_pll_cmuc"},
	{"mout_cci_pll_cmuc"},
	{"mout_mfc_pll_cmuc"},
	{"mout_aclk_bus1_532",},
	{"mout_aclk_bus1_200",},
	{"mout_pclk_bus01_133",},
	{"mout_aclk_imem_266",},
	{"mout_aclk_imem_200",},
	{"mout_aclk_imem_100",},
	{"mout_aclk_mfc_532",},
	{"mout_aclk_mscl_532",},
	{"mout_aclk_peris_66",},
	{"mout_aclk_fsys0_200",},
	{"mout_bus0_pll_top1",},
	{"mout_aclk_fsys1_200",},
	{"mout_aclk_vpp0_400",},
	{"mout_bus1_pll_top0",},
	{"mout_mfc_pll_top0",},
	{"mout_bus0_pll_top0",},
	{"mout_cci_pll_top0",},
	{"mout_aclk_vpp1_400",},
	{"mout_aclk_peric0_66",},
	{"mout_aclk_peric1_66",},
	{"mout_aclk_isp0_trex_532",},
	{"isp_pll",},
	{"mout_aclk_isp0_isp0_590",},
	{"cam_pll",},
	{"mout_aclk_isp0_tpu_590",},
	{"mout_aclk_isp1_isp1_468",},
	{"mout_aclk_isp1_ahb_117",},
	{"mout_aclk_cam1_trex_532",},
	{"mout_aclk_cam1_sclvra_491",},
	{"mout_aclk_cam1_bnscsis_133",},
	{"mout_aclk_cam1_arm_668",},
	{"mout_aclk_cam1_busperi_334",},
	{"mout_aclk_cam1_nocp_133",},
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{INT_LV0,	560000,	1000000},
	{INT_LV1,	550000,	1000000},
	{INT_LV2,	540000,	1000000},
	{INT_LV3,	530000,	1000000},
	{INT_LV4,	520000,	1000000},
	{INT_LV5,	510000,	1000000},
	{INT_LV6,	500000,	1000000},
	{INT_LV7,	400000,	1000000},
	{INT_LV8,	334000,	1000000},
	{INT_LV9,	266000,	1000000},
	{INT_LV10,	200000,	1000000},
	{INT_LV11,	100000,	1000000},
};

struct devfreq_clk_state aclk_bus0_532_bus0_pll[] = {
	{MOUT_ACLK_BUS0_532,	_MOUT_BUS0_PLL_CMUC},
};
struct devfreq_clk_state aclk_bus0_532_bus1_pll[] = {
	{MOUT_ACLK_BUS0_532,	_BUS1_PLL},
};
struct devfreq_clk_state aclk_bus0_532_cci_pll[] = {
	{MOUT_ACLK_BUS0_532,	_CCI_PLL},
};
struct devfreq_clk_state aclk_bus0_532_mfc_pll[] = {
	{MOUT_ACLK_BUS0_532,	_MFC_PLL},
};
struct devfreq_clk_state aclk_bus0_532_bus0_pll_cmuc_div2[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_MOUT_BUS0_PLL_CMUC_DIV2},
};
struct devfreq_clk_state aclk_bus0_532_bus1_pll_div2[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_TOPC_BUS1_PLL_DIV2},
};
struct devfreq_clk_state aclk_bus0_532_cci_pll_div2[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_TOPC_CCI_PLL_DIV2},
};
struct devfreq_clk_state aclk_bus0_532_mfc_pll_div2[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_TOPC_MFC_PLL_DIV2},
};
struct devfreq_clk_state aclk_bus0_532_bus0_pll_cmuc_div4[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_MOUT_BUS0_PLL_CMUC_DIV4},
};
struct devfreq_clk_state aclk_bus0_532_bus1_pll_div4[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_TOPC_BUS1_PLL_DIV4},
};
struct devfreq_clk_state aclk_bus0_532_cci_pll_div4[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_TOPC_CCI_PLL_DIV4},
};
struct devfreq_clk_state aclk_bus0_532_mfc_pll_div4[] = {
	{MOUT_ACLK_BUS0_532,	_FFAC_TOPC_MFC_PLL_DIV4},
};

struct devfreq_clk_state aclk_bus1_532_bus0_pll[] = {
	{MOUT_ACLK_BUS1_532,	_MOUT_BUS0_PLL_CMUC},
};
struct devfreq_clk_state aclk_bus1_532_bus1_pll[] = {
	{MOUT_ACLK_BUS1_532,	_BUS1_PLL},
};
struct devfreq_clk_state aclk_bus1_532_cci_pll[] = {
	{MOUT_ACLK_BUS1_532,	_CCI_PLL},
};
struct devfreq_clk_state aclk_bus1_532_mfc_pll[] = {
	{MOUT_ACLK_BUS1_532,	_MFC_PLL},
};
struct devfreq_clk_state aclk_bus1_532_bus0_pll_cmuc_div2[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_MOUT_BUS0_PLL_CMUC_DIV2},
};
struct devfreq_clk_state aclk_bus1_532_bus1_pll_div2[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_TOPC_BUS1_PLL_DIV2},
};
struct devfreq_clk_state aclk_bus1_532_cci_pll_div2[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_TOPC_CCI_PLL_DIV2},
};
struct devfreq_clk_state aclk_bus1_532_mfc_pll_div2[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_TOPC_MFC_PLL_DIV2},
};
struct devfreq_clk_state aclk_bus1_532_bus0_pll_cmuc_div4[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_MOUT_BUS0_PLL_CMUC_DIV4},
};
struct devfreq_clk_state aclk_bus1_532_bus1_pll_div4[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_TOPC_BUS1_PLL_DIV4},
};
struct devfreq_clk_state aclk_bus1_532_cci_pll_div4[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_TOPC_CCI_PLL_DIV4},
};
struct devfreq_clk_state aclk_bus1_532_mfc_pll_div4[] = {
	{MOUT_ACLK_BUS1_532,	_FFAC_TOPC_MFC_PLL_DIV4},
};

struct devfreq_clk_state aclk_bus1_200_bus0_pll[] = {
	{MOUT_ACLK_BUS1_200,	_MOUT_BUS0_PLL_CMUC},
};
struct devfreq_clk_state aclk_bus1_200_bus1_pll[] = {
	{MOUT_ACLK_BUS1_200,	_MOUT_BUS1_PLL_CMUC},
};
struct devfreq_clk_state aclk_bus1_200_cci_pll[] = {
	{MOUT_ACLK_BUS1_200,	_MOUT_CCI_PLL_CMUC},
};

struct devfreq_clk_state pclk_bus01_133_bus0_pll[] = {
	{MOUT_PCLK_BUS01_133,	_MOUT_BUS0_PLL_CMUC},
};

struct devfreq_clk_state aclk_imem_266_bus0_pll[] = {
	{MOUT_ACLK_IMEM_266,	_MOUT_BUS0_PLL_CMUC},
};
struct devfreq_clk_state aclk_imem_266_cci_pll[] = {
	{MOUT_ACLK_IMEM_266,	_MOUT_CCI_PLL_CMUC},
};
struct devfreq_clk_state aclk_imem_266_mfc_pll[] = {
	{MOUT_ACLK_IMEM_266,	_MOUT_MFC_PLL_CMUC},
};
struct devfreq_clk_state aclk_imem_266_bus1_pll[] = {
	{MOUT_ACLK_IMEM_266,	_MOUT_BUS1_PLL_CMUC},
};

struct devfreq_clk_state aclk_imem_200_cci_pll[] = {
	{MOUT_ACLK_IMEM_200,	_MOUT_CCI_PLL_CMUC},
};
struct devfreq_clk_state aclk_imem_200_bus1_pll[] = {
	{MOUT_ACLK_IMEM_200,	_MOUT_BUS1_PLL_CMUC},
};
struct devfreq_clk_state aclk_imem_200_bus0_pll[] = {
	{MOUT_ACLK_IMEM_200,	_MOUT_BUS0_PLL_CMUC},
};

struct devfreq_clk_state aclk_imem_100_bus0_pll[] = {
	{MOUT_ACLK_IMEM_100,	_MOUT_BUS0_PLL_CMUC},
};

struct devfreq_clk_state aclk_mfc_532_bus0_pll[] = {
	{MOUT_ACLK_MFC_532,	_MOUT_BUS0_PLL_CMUC},
};
struct devfreq_clk_state aclk_mfc_532_cci_pll[] = {
	{MOUT_ACLK_MFC_532,	_MOUT_CCI_PLL_CMUC},
};
struct devfreq_clk_state aclk_mfc_532_mfc_pll[] = {
	{MOUT_ACLK_MFC_532,	_MOUT_MFC_PLL_CMUC},
};
struct devfreq_clk_state aclk_mfc_532_bus1_pll[] = {
	{MOUT_ACLK_MFC_532,	_MOUT_BUS1_PLL_CMUC},
};

struct devfreq_clk_state aclk_mscl_532_bus0_pll[] = {
	{MOUT_ACLK_MSCL_532,	_MOUT_BUS0_PLL_CMUC},
};
struct devfreq_clk_state aclk_mscl_532_cci_pll[] = {
	{MOUT_ACLK_MSCL_532,	_MOUT_CCI_PLL_CMUC},
};
struct devfreq_clk_state aclk_mscl_532_mfc_pll[] = {
	{MOUT_ACLK_MSCL_532,	_MOUT_MFC_PLL_CMUC},
};
struct devfreq_clk_state aclk_mscl_532_bus1_pll[] = {
	{MOUT_ACLK_MSCL_532,	_MOUT_BUS1_PLL_CMUC},
};

struct devfreq_clk_state aclk_peris_66_bus0_pll[] = {
	{MOUT_ACLK_PERIS_66,	_MOUT_BUS0_PLL_CMUC},
};

struct devfreq_clk_state aclk_vpp0_400_bus1_pll[] = {
	{MOUT_ACLK_VPP0_400,	MOUT_BUS1_PLL_TOP0},
};
struct devfreq_clk_state aclk_vpp0_400_mfc_pll[] = {
	{MOUT_ACLK_VPP0_400,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_vpp0_400_bus0_pll[] = {
	{MOUT_ACLK_VPP0_400,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_vpp0_400_cci_pll[] = {
	{MOUT_ACLK_VPP0_400,	MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_state aclk_vpp1_400_bus1_pll[] = {
	{MOUT_ACLK_VPP1_400,	MOUT_BUS1_PLL_TOP0},
};
struct devfreq_clk_state aclk_vpp1_400_mfc_pll[] = {
	{MOUT_ACLK_VPP1_400,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_vpp1_400_bus0_pll[] = {
	{MOUT_ACLK_VPP1_400,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_vpp1_400_cci_pll[] = {
	{MOUT_ACLK_VPP1_400,	MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_state aclk_isp0_trex_532_bus0_pll[] = {
	{MOUT_ACLK_ISP0_TREX_532,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_trex_532_cci_pll[] = {
	{MOUT_ACLK_ISP0_TREX_532,	MOUT_CCI_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_trex_532_isp_pll[] = {
	{MOUT_ACLK_ISP0_TREX_532,	ISP_PLL},
};
struct devfreq_clk_state aclk_isp0_trex_532_cam_pll[] = {
	{MOUT_ACLK_ISP0_TREX_532,	_CAM_PLL},
};

struct devfreq_clk_state aclk_isp0_isp0_590_mfc_pll[] = {
	{MOUT_ACLK_ISP0_ISP0_590,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_isp0_590_isp_pll[] = {
	{MOUT_ACLK_ISP0_ISP0_590,	ISP_PLL},
};
struct devfreq_clk_state aclk_isp0_isp0_590_cci_pll[] = {
	{MOUT_ACLK_ISP0_ISP0_590,	MOUT_CCI_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_isp0_590_bus0_pll[] = {
	{MOUT_ACLK_ISP0_ISP0_590,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_isp0_590_cam_pll[] = {
	{MOUT_ACLK_ISP0_ISP0_590,	_CAM_PLL},
};

struct devfreq_clk_state aclk_isp0_tpu_590_mfc_pll[] = {
	{MOUT_ACLK_ISP0_TPU_590,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_tpu_590_isp_pll[] = {
	{MOUT_ACLK_ISP0_TPU_590,	ISP_PLL},
};
struct devfreq_clk_state aclk_isp0_tpu_590_cci_pll[] = {
	{MOUT_ACLK_ISP0_TPU_590,	MOUT_CCI_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_tpu_590_bus0_pll[] = {
	{MOUT_ACLK_ISP0_TPU_590,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp0_tpu_590_cam_pll[] = {
	{MOUT_ACLK_ISP0_TPU_590,	_CAM_PLL},
};

struct devfreq_clk_state aclk_isp1_isp1_468_cci_pll[] = {
	{MOUT_ACLK_ISP1_ISP1_468,	MOUT_CCI_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp1_isp1_468_cam_pll[] = {
	{MOUT_ACLK_ISP1_ISP1_468,	_CAM_PLL},
};
struct devfreq_clk_state aclk_isp1_isp1_468_mfc_pll[] = {
	{MOUT_ACLK_ISP1_ISP1_468,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp1_isp1_468_bus1_pll[] = {
	{MOUT_ACLK_ISP1_ISP1_468,	MOUT_BUS1_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp1_isp1_468_isp_pll[] = {
	{MOUT_ACLK_ISP1_ISP1_468,	ISP_PLL},
};
struct devfreq_clk_state aclk_isp1_isp1_468_bus0_pll[] = {
	{MOUT_ACLK_ISP1_ISP1_468,	MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_state aclk_isp1_ahb_117_mfc_pll[] = {
	{MOUT_ACLK_ISP1_AHB_117,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_isp1_ahb_117_isp_pll[] = {
	{MOUT_ACLK_ISP1_AHB_117,	ISP_PLL},
};
struct devfreq_clk_state aclk_isp1_ahb_117_bus0_pll[] = {
	{MOUT_ACLK_ISP1_AHB_117,	MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_state aclk_cam1_trex_532_bus1_pll[] = {
	{MOUT_ACLK_CAM1_TREX_532,	MOUT_BUS1_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_trex_532_bus0_pll[] = {
	{MOUT_ACLK_CAM1_TREX_532,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_trex_532_cci_pll[] = {
	{MOUT_ACLK_CAM1_TREX_532,	MOUT_CCI_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_trex_532_mfc_pll[] = {
	{MOUT_ACLK_CAM1_TREX_532,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_trex_532_isp_pll[] = {
	{MOUT_ACLK_CAM1_TREX_532,	ISP_PLL},
};
struct devfreq_clk_state aclk_cam1_trex_532_cam_pll[] = {
	{MOUT_ACLK_CAM1_TREX_532,	_CAM_PLL},
};

struct devfreq_clk_state aclk_cam1_sclvra_491_mfc_pll[] = {
	{MOUT_ACLK_CAM1_SCLVRA_491,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_sclvra_491_bus1_pll[] = {
	{MOUT_ACLK_CAM1_SCLVRA_491,	MOUT_BUS1_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_sclvra_491_cam_pll[] = {
	{MOUT_ACLK_CAM1_SCLVRA_491,	_CAM_PLL},
};
struct devfreq_clk_state aclk_cam1_sclvra_491_bus0_pll[] = {
	{MOUT_ACLK_CAM1_SCLVRA_491,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_sclvra_491_isp_pll[] = {
	{MOUT_ACLK_CAM1_SCLVRA_491,	ISP_PLL},
};
struct devfreq_clk_state aclk_cam1_sclvra_491_cci_pll[] = {
	{MOUT_ACLK_CAM1_SCLVRA_491,	MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_state aclk_cam1_bnscsis_133_cci_pll[] = {
	{MOUT_ACLK_CAM1_BNSCSIS_133,	MOUT_CCI_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_bnscsis_133_mfc_pll[] = {
	{MOUT_ACLK_CAM1_BNSCSIS_133,	MOUT_MFC_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_bnscsis_133_cam_pll[] = {
	{MOUT_ACLK_CAM1_BNSCSIS_133,	_CAM_PLL},
};
struct devfreq_clk_state aclk_cam1_bnscsis_133_isp_pll[] = {
	{MOUT_ACLK_CAM1_BNSCSIS_133,	ISP_PLL},
};

struct devfreq_clk_state aclk_cam1_arm_668_bus1_pll[] = {
	{MOUT_ACLK_CAM1_ARM_668,	MOUT_BUS1_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_arm_668_bus0_pll[] = {
	{MOUT_ACLK_CAM1_ARM_668,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_arm_668_cam_pll[] = {
	{MOUT_ACLK_CAM1_ARM_668,	_CAM_PLL},
};
struct devfreq_clk_state aclk_cam1_arm_668_isp_pll[] = {
	{MOUT_ACLK_CAM1_ARM_668,	ISP_PLL},
};

struct devfreq_clk_state aclk_cam1_busperi_334_bus1_pll[] = {
	{MOUT_ACLK_CAM1_BUSPERI_334,	MOUT_BUS1_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_busperi_334_bus0_pll[] = {
	{MOUT_ACLK_CAM1_BUSPERI_334,	MOUT_BUS0_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_busperi_334_cam_pll[] = {
	{MOUT_ACLK_CAM1_BUSPERI_334,	_CAM_PLL},
};
struct devfreq_clk_state aclk_cam1_busperi_334_isp_pll[] = {
	{MOUT_ACLK_CAM1_BUSPERI_334,	ISP_PLL},
};

struct devfreq_clk_state aclk_cam1_nocp_133_cci_pll[] = {
	{MOUT_ACLK_CAM1_NOCP_133,	MOUT_CCI_PLL_TOP0},
};
struct devfreq_clk_state aclk_cam1_nocp_133_isp_pll[] = {
	{MOUT_ACLK_CAM1_NOCP_133,	ISP_PLL},
};

struct devfreq_clk_states aclk_bus0_532_bus0_pll_list = {
	.state = aclk_bus0_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_532_bus0_pll),
};

struct devfreq_clk_states aclk_bus0_532_cci_pll_list = {
	.state = aclk_bus0_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_532_cci_pll),
};

struct devfreq_clk_states aclk_bus0_532_mfc_pll_list = {
	.state = aclk_bus0_532_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_532_mfc_pll),
};

struct devfreq_clk_states aclk_bus0_532_bus1_pll_list = {
	.state = aclk_bus0_532_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_532_bus1_pll),
};

struct devfreq_clk_states aclk_bus0_532_bus0_pll_cmuc_div2_list = {
	.state = aclk_bus0_532_bus0_pll_cmuc_div2,
	.state_count = ARRAY_SIZE(aclk_bus0_532_bus0_pll_cmuc_div2),
};

struct devfreq_clk_states aclk_bus0_532_bus1_pll_div2_list = {
	.state = aclk_bus0_532_bus1_pll_div2,
	.state_count = ARRAY_SIZE(aclk_bus0_532_bus1_pll_div2),
};

struct devfreq_clk_states aclk_bus0_532_cci_pll_div2_list = {
	.state = aclk_bus0_532_cci_pll_div2,
	.state_count = ARRAY_SIZE(aclk_bus0_532_cci_pll_div2),
};

struct devfreq_clk_states aclk_bus0_532_mfc_pll_div2_list = {
	.state = aclk_bus0_532_mfc_pll_div2,
	.state_count = ARRAY_SIZE(aclk_bus0_532_mfc_pll_div2),
};

struct devfreq_clk_states aclk_bus0_532_bus0_pll_cmuc_div4_list = {
	.state = aclk_bus0_532_bus0_pll_cmuc_div4,
	.state_count = ARRAY_SIZE(aclk_bus0_532_bus0_pll_cmuc_div4),
};

struct devfreq_clk_states aclk_bus0_532_bus1_pll_div4_list = {
	.state = aclk_bus0_532_bus1_pll_div4,
	.state_count = ARRAY_SIZE(aclk_bus0_532_bus1_pll_div4),
};

struct devfreq_clk_states aclk_bus0_532_cci_pll_div4_list = {
	.state = aclk_bus0_532_cci_pll_div4,
	.state_count = ARRAY_SIZE(aclk_bus0_532_cci_pll_div4),
};

struct devfreq_clk_states aclk_bus0_532_mfc_pll_div4_list = {
	.state = aclk_bus0_532_mfc_pll_div4,
	.state_count = ARRAY_SIZE(aclk_bus0_532_mfc_pll_div4),
};

struct devfreq_clk_states aclk_bus1_532_bus0_pll_list = {
	.state = aclk_bus1_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_532_bus0_pll),
};

struct devfreq_clk_states aclk_bus1_532_cci_pll_list = {
	.state = aclk_bus1_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_532_cci_pll),
};

struct devfreq_clk_states aclk_bus1_532_mfc_pll_list = {
	.state = aclk_bus1_532_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_532_mfc_pll),
};

struct devfreq_clk_states aclk_bus1_532_bus1_pll_list = {
	.state = aclk_bus1_532_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_532_bus1_pll),
};

struct devfreq_clk_states aclk_bus1_532_bus0_pll_cmuc_div2_list = {
	.state = aclk_bus1_532_bus0_pll_cmuc_div2,
	.state_count = ARRAY_SIZE(aclk_bus1_532_bus0_pll_cmuc_div2),
};

struct devfreq_clk_states aclk_bus1_532_bus1_pll_div2_list = {
	.state = aclk_bus1_532_bus1_pll_div2,
	.state_count = ARRAY_SIZE(aclk_bus1_532_bus1_pll_div2),
};

struct devfreq_clk_states aclk_bus1_532_cci_pll_div2_list = {
	.state = aclk_bus1_532_cci_pll_div2,
	.state_count = ARRAY_SIZE(aclk_bus1_532_cci_pll_div2),
};

struct devfreq_clk_states aclk_bus1_532_mfc_pll_div2_list = {
	.state = aclk_bus1_532_mfc_pll_div2,
	.state_count = ARRAY_SIZE(aclk_bus1_532_mfc_pll_div2),
};

struct devfreq_clk_states aclk_bus1_532_bus0_pll_cmuc_div4_list = {
	.state = aclk_bus1_532_bus0_pll_cmuc_div4,
	.state_count = ARRAY_SIZE(aclk_bus1_532_bus0_pll_cmuc_div4),
};

struct devfreq_clk_states aclk_bus1_532_bus1_pll_div4_list = {
	.state = aclk_bus1_532_bus1_pll_div4,
	.state_count = ARRAY_SIZE(aclk_bus1_532_bus1_pll_div4),
};

struct devfreq_clk_states aclk_bus1_532_cci_pll_div4_list = {
	.state = aclk_bus1_532_cci_pll_div4,
	.state_count = ARRAY_SIZE(aclk_bus1_532_cci_pll_div4),
};

struct devfreq_clk_states aclk_bus1_532_mfc_pll_div4_list = {
	.state = aclk_bus1_532_mfc_pll_div4,
	.state_count = ARRAY_SIZE(aclk_bus1_532_mfc_pll_div4),
};

struct devfreq_clk_states aclk_bus1_200_bus0_pll_list = {
	.state = aclk_bus1_200_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_200_bus0_pll),
};

struct devfreq_clk_states aclk_bus1_200_bus1_pll_list = {
	.state = aclk_bus1_200_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_200_bus1_pll),
};

struct devfreq_clk_states aclk_bus1_200_cci_pll_list = {
	.state = aclk_bus1_200_cci_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_200_cci_pll),
};

struct devfreq_clk_states aclk_imem_266_bus0_pll_list = {
	.state = aclk_imem_266_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_imem_266_bus0_pll),
};

struct devfreq_clk_states aclk_imem_266_cci_pll_list = {
	.state = aclk_imem_266_cci_pll,
	.state_count = ARRAY_SIZE(aclk_imem_266_cci_pll),
};

struct devfreq_clk_states aclk_imem_266_mfc_pll_list = {
	.state = aclk_imem_266_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_imem_266_mfc_pll),
};

struct devfreq_clk_states aclk_imem_266_bus1_pll_list = {
	.state = aclk_imem_266_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_imem_266_bus1_pll),
};

struct devfreq_clk_states aclk_imem_200_cci_pll_list = {
	.state = aclk_imem_200_cci_pll,
	.state_count = ARRAY_SIZE(aclk_imem_200_cci_pll),
};

struct devfreq_clk_states aclk_imem_200_bus1_pll_list = {
	.state = aclk_imem_200_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_imem_200_bus1_pll),
};

struct devfreq_clk_states aclk_imem_200_bus0_pll_list = {
	.state = aclk_imem_200_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_imem_200_bus0_pll),
};

struct devfreq_clk_states aclk_mfc_532_bus0_pll_list = {
	.state = aclk_mfc_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_mfc_532_bus0_pll),
};

struct devfreq_clk_states aclk_mfc_532_cci_pll_list = {
	.state = aclk_mfc_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_mfc_532_cci_pll),
};

struct devfreq_clk_states aclk_mfc_532_mfc_pll_list = {
	.state = aclk_mfc_532_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mfc_532_mfc_pll),
};

struct devfreq_clk_states aclk_mfc_532_bus1_pll_list = {
	.state = aclk_mfc_532_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_mfc_532_bus1_pll),
};

struct devfreq_clk_states aclk_mscl_532_bus0_pll_list = {
	.state = aclk_mscl_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_532_bus0_pll),
};

struct devfreq_clk_states aclk_mscl_532_cci_pll_list = {
	.state = aclk_mscl_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_532_cci_pll),
};

struct devfreq_clk_states aclk_mscl_532_mfc_pll_list = {
	.state = aclk_mscl_532_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_532_mfc_pll),
};

struct devfreq_clk_states aclk_mscl_532_bus1_pll_list = {
	.state = aclk_mscl_532_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_532_bus1_pll),
};

struct devfreq_clk_states aclk_vpp0_400_bus0_pll_list = {
	.state = aclk_vpp0_400_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_vpp0_400_bus0_pll),
};

struct devfreq_clk_states aclk_vpp0_400_bus1_pll_list = {
	.state = aclk_vpp0_400_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_vpp0_400_bus1_pll),
};

struct devfreq_clk_states aclk_vpp0_400_mfc_pll_list = {
	.state = aclk_vpp0_400_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_vpp0_400_mfc_pll),
};

struct devfreq_clk_states aclk_vpp0_400_cci_pll_list = {
	.state = aclk_vpp0_400_cci_pll,
	.state_count = ARRAY_SIZE(aclk_vpp0_400_cci_pll),
};

struct devfreq_clk_states aclk_vpp1_400_bus1_pll_list = {
	.state = aclk_vpp1_400_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_vpp1_400_bus1_pll),
};

struct devfreq_clk_states aclk_vpp1_400_mfc_pll_list = {
	.state = aclk_vpp1_400_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_vpp1_400_mfc_pll),
};

struct devfreq_clk_states aclk_vpp1_400_bus0_pll_list = {
	.state = aclk_vpp1_400_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_vpp1_400_bus0_pll),
};

struct devfreq_clk_states aclk_vpp1_400_cci_pll_list = {
	.state = aclk_vpp1_400_cci_pll,
	.state_count = ARRAY_SIZE(aclk_vpp1_400_cci_pll),
};

struct devfreq_clk_states aclk_isp0_trex_532_bus0_pll_list = {
	.state = aclk_isp0_trex_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_trex_532_bus0_pll),
};

struct devfreq_clk_states aclk_isp0_trex_532_cci_pll_list = {
	.state = aclk_isp0_trex_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_trex_532_cci_pll),
};

struct devfreq_clk_states aclk_isp0_trex_532_isp_pll_list = {
	.state = aclk_isp0_trex_532_isp_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_trex_532_isp_pll),
};

struct devfreq_clk_states aclk_isp0_trex_532_cam_pll_list = {
	.state = aclk_isp0_trex_532_cam_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_trex_532_cam_pll),
};

struct devfreq_clk_states aclk_isp0_isp0_590_mfc_pll_list = {
	.state = aclk_isp0_isp0_590_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_isp0_590_mfc_pll),
};

struct devfreq_clk_states aclk_isp0_isp0_590_isp_pll_list = {
	.state = aclk_isp0_isp0_590_isp_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_isp0_590_isp_pll),
};

struct devfreq_clk_states aclk_isp0_isp0_590_cci_pll_list = {
	.state = aclk_isp0_isp0_590_cci_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_isp0_590_cci_pll),
};

struct devfreq_clk_states aclk_isp0_isp0_590_bus0_pll_list = {
	.state = aclk_isp0_isp0_590_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_isp0_590_bus0_pll),
};

struct devfreq_clk_states aclk_isp0_isp0_590_cam_pll_list = {
	.state = aclk_isp0_isp0_590_cam_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_isp0_590_cam_pll),
};

struct devfreq_clk_states aclk_isp0_tpu_590_mfc_pll_list = {
	.state = aclk_isp0_tpu_590_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_tpu_590_mfc_pll),
};

struct devfreq_clk_states aclk_isp0_tpu_590_isp_pll_list = {
	.state = aclk_isp0_tpu_590_isp_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_tpu_590_isp_pll),
};

struct devfreq_clk_states aclk_isp0_tpu_590_cci_pll_list = {
	.state = aclk_isp0_tpu_590_cci_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_tpu_590_cci_pll),
};

struct devfreq_clk_states aclk_isp0_tpu_590_bus0_pll_list = {
	.state = aclk_isp0_tpu_590_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_tpu_590_bus0_pll),
};

struct devfreq_clk_states aclk_isp0_tpu_590_cam_pll_list = {
	.state = aclk_isp0_tpu_590_cam_pll,
	.state_count = ARRAY_SIZE(aclk_isp0_tpu_590_cam_pll),
};

struct devfreq_clk_states aclk_isp1_isp1_468_mfc_pll_list = {
	.state = aclk_isp1_isp1_468_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_isp1_468_mfc_pll),
};

struct devfreq_clk_states aclk_isp1_isp1_468_cam_pll_list = {
	.state = aclk_isp1_isp1_468_cam_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_isp1_468_cam_pll),
};

struct devfreq_clk_states aclk_isp1_isp1_468_cci_pll_list = {
	.state = aclk_isp1_isp1_468_cci_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_isp1_468_cci_pll),
};

struct devfreq_clk_states aclk_isp1_isp1_468_bus1_pll_list = {
	.state = aclk_isp1_isp1_468_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_isp1_468_bus1_pll),
};

struct devfreq_clk_states aclk_isp1_isp1_468_isp_pll_list = {
	.state = aclk_isp1_isp1_468_isp_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_isp1_468_isp_pll),
};

struct devfreq_clk_states aclk_isp1_isp1_468_bus0_pll_list = {
	.state = aclk_isp1_isp1_468_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_isp1_468_bus0_pll),
};

struct devfreq_clk_states aclk_isp1_ahb_117_bus0_pll_list = {
	.state = aclk_isp1_ahb_117_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_ahb_117_bus0_pll),
};

struct devfreq_clk_states aclk_isp1_ahb_117_mfc_pll_list = {
	.state = aclk_isp1_ahb_117_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_ahb_117_mfc_pll),
};

struct devfreq_clk_states aclk_isp1_ahb_117_isp_pll_list = {
	.state = aclk_isp1_ahb_117_isp_pll,
	.state_count = ARRAY_SIZE(aclk_isp1_ahb_117_isp_pll),
};

struct devfreq_clk_states aclk_cam1_trex_532_bus1_pll_list = {
	.state = aclk_cam1_trex_532_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_trex_532_bus1_pll),
};

struct devfreq_clk_states aclk_cam1_trex_532_cci_pll_list = {
	.state = aclk_cam1_trex_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_trex_532_cci_pll),
};

struct devfreq_clk_states aclk_cam1_trex_532_bus0_pll_list = {
	.state = aclk_cam1_trex_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_trex_532_bus0_pll),
};

struct devfreq_clk_states aclk_cam1_trex_532_mfc_pll_list = {
	.state = aclk_cam1_trex_532_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_trex_532_mfc_pll),
};

struct devfreq_clk_states aclk_cam1_trex_532_isp_pll_list = {
	.state = aclk_cam1_trex_532_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_trex_532_isp_pll),
};

struct devfreq_clk_states aclk_cam1_trex_532_cam_pll_list = {
	.state = aclk_cam1_trex_532_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_trex_532_cam_pll),
};

struct devfreq_clk_states aclk_cam1_sclvra_491_mfc_pll_list = {
	.state = aclk_cam1_sclvra_491_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_sclvra_491_mfc_pll),
};

struct devfreq_clk_states aclk_cam1_sclvra_491_bus1_pll_list = {
	.state = aclk_cam1_sclvra_491_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_sclvra_491_bus1_pll),
};

struct devfreq_clk_states aclk_cam1_sclvra_491_cam_pll_list = {
	.state = aclk_cam1_sclvra_491_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_sclvra_491_cam_pll),
};

struct devfreq_clk_states aclk_cam1_sclvra_491_bus0_pll_list = {
	.state = aclk_cam1_sclvra_491_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_sclvra_491_bus0_pll),
};

struct devfreq_clk_states aclk_cam1_sclvra_491_isp_pll_list = {
	.state = aclk_cam1_sclvra_491_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_sclvra_491_isp_pll),
};

struct devfreq_clk_states aclk_cam1_sclvra_491_cci_pll_list = {
	.state = aclk_cam1_sclvra_491_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_sclvra_491_cci_pll),
};

struct devfreq_clk_states aclk_cam1_bnscsis_133_cci_pll_list = {
	.state = aclk_cam1_bnscsis_133_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_bnscsis_133_cci_pll),
};

struct devfreq_clk_states aclk_cam1_bnscsis_133_mfc_pll_list = {
	.state = aclk_cam1_bnscsis_133_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_bnscsis_133_mfc_pll),
};

struct devfreq_clk_states aclk_cam1_bnscsis_133_cam_pll_list = {
	.state = aclk_cam1_bnscsis_133_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_bnscsis_133_cam_pll),
};

struct devfreq_clk_states aclk_cam1_bnscsis_133_isp_pll_list = {
	.state = aclk_cam1_bnscsis_133_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_bnscsis_133_isp_pll),
};

struct devfreq_clk_states aclk_cam1_arm_668_bus1_pll_list = {
	.state = aclk_cam1_arm_668_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_arm_668_bus1_pll),
};

struct devfreq_clk_states aclk_cam1_arm_668_bus0_pll_list = {
	.state = aclk_cam1_arm_668_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_arm_668_bus0_pll),
};

struct devfreq_clk_states aclk_cam1_arm_668_cam_pll_list = {
	.state = aclk_cam1_arm_668_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_arm_668_cam_pll),
};

struct devfreq_clk_states aclk_cam1_arm_668_isp_pll_list = {
	.state = aclk_cam1_arm_668_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_arm_668_isp_pll),
};

struct devfreq_clk_states aclk_cam1_busperi_334_bus1_pll_list = {
	.state = aclk_cam1_busperi_334_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_busperi_334_bus1_pll),
};

struct devfreq_clk_states aclk_cam1_busperi_334_bus0_pll_list = {
	.state = aclk_cam1_busperi_334_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_busperi_334_bus0_pll),
};

struct devfreq_clk_states aclk_cam1_busperi_334_cam_pll_list = {
	.state = aclk_cam1_busperi_334_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_busperi_334_cam_pll),
};

struct devfreq_clk_states aclk_cam1_busperi_334_isp_pll_list = {
	.state = aclk_cam1_busperi_334_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_busperi_334_isp_pll),
};

struct devfreq_clk_states aclk_cam1_nocp_133_cci_pll_list = {
	.state = aclk_cam1_nocp_133_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_nocp_133_cci_pll),
};

struct devfreq_clk_states aclk_cam1_nocp_133_isp_pll_list = {
	.state = aclk_cam1_nocp_133_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_nocp_133_isp_pll),
};

struct devfreq_clk_info aclk_bus0_532[] = {
	{INT_LV0,	532 * MHZ,	0,	&aclk_bus0_532_cci_pll_list},
	{INT_LV1,	532 * MHZ,	0,	&aclk_bus0_532_cci_pll_list},
	{INT_LV2,	400 * MHZ,	0,	&aclk_bus0_532_bus0_pll_list},
	{INT_LV3,	400 * MHZ,	0,	&aclk_bus0_532_bus0_pll_list},
	{INT_LV4,	400 * MHZ,	0,	&aclk_bus0_532_bus0_pll_list},
	{INT_LV5,	400 * MHZ,	0,	&aclk_bus0_532_bus0_pll_list},
	{INT_LV6,	400 * MHZ,	0,	&aclk_bus0_532_bus0_pll_list},
	{INT_LV7,	400 * MHZ,	0,	&aclk_bus0_532_bus0_pll_list},
	{INT_LV8,	334 * MHZ,	0,	&aclk_bus0_532_bus1_pll_div2_list},
	{INT_LV9,	266 * MHZ,	0,	&aclk_bus0_532_cci_pll_div2_list},
	{INT_LV10,	200 * MHZ,	0,	&aclk_bus0_532_bus0_pll_cmuc_div2_list},
	{INT_LV11,	100 * MHZ,	0,	&aclk_bus0_532_bus0_pll_cmuc_div4_list},
};

struct devfreq_clk_info aclk_bus1_532[] = {
	{INT_LV0,	532 * MHZ,	0,	&aclk_bus1_532_cci_pll_list},
	{INT_LV1,	532 * MHZ,	0,	&aclk_bus1_532_cci_pll_list},
	{INT_LV2,	400 * MHZ,	0,	&aclk_bus1_532_bus0_pll_list},
	{INT_LV3,	400 * MHZ,	0,	&aclk_bus1_532_bus0_pll_list},
	{INT_LV4,	400 * MHZ,	0,	&aclk_bus1_532_bus0_pll_list},
	{INT_LV5,	400 * MHZ,	0,	&aclk_bus1_532_bus0_pll_list},
	{INT_LV6,	400 * MHZ,	0,	&aclk_bus1_532_bus0_pll_list},
	{INT_LV7,	400 * MHZ,	0,	&aclk_bus1_532_bus0_pll_list},
	{INT_LV8,	334 * MHZ,	0,	&aclk_bus1_532_bus1_pll_div2_list},
	{INT_LV9,	266 * MHZ,	0,	&aclk_bus1_532_cci_pll_div2_list},
	{INT_LV10,	200 * MHZ,	0,	&aclk_bus1_532_bus0_pll_cmuc_div2_list},
	{INT_LV11,	100 * MHZ,	0,	&aclk_bus1_532_bus0_pll_cmuc_div4_list},
};

struct devfreq_clk_info aclk_bus1_200[] = {
	{INT_LV0,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV1,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV2,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV3,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV4,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV5,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV6,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV7,	200 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV8,	167 * MHZ,	0,	&aclk_bus1_200_bus1_pll_list},
	{INT_LV9,	133 * MHZ,	0,	&aclk_bus1_200_cci_pll_list},
	{INT_LV10,	100 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
	{INT_LV11,	80 * MHZ,	0,	&aclk_bus1_200_bus0_pll_list},
};

struct devfreq_clk_info pclk_bus01_133[] = {
	{INT_LV0,	134 * MHZ,	0,	NULL},
	{INT_LV1,	134 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	100 * MHZ,	0,	NULL},
	{INT_LV4,	100 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
	{INT_LV8,	80 * MHZ,	0,	NULL},
	{INT_LV9,	67 * MHZ,	0,	NULL},
	{INT_LV10,	50 * MHZ,	0,	NULL},
	{INT_LV11,	25 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_imem_266[] = {
	{INT_LV0,	266 * MHZ,	0,	&aclk_imem_266_cci_pll_list},
	{INT_LV1,	266 * MHZ,	0,	&aclk_imem_266_cci_pll_list},
	{INT_LV2,	200 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
	{INT_LV3,	200 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
	{INT_LV4,	200 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
	{INT_LV5,	200 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
	{INT_LV6,	200 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
	{INT_LV7,	200 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
	{INT_LV8,	167 * MHZ,	0,	&aclk_imem_266_bus1_pll_list},
	{INT_LV9,	133 * MHZ,	0,	&aclk_imem_266_cci_pll_list},
	{INT_LV10,	100 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
	{INT_LV11,	80 * MHZ,	0,	&aclk_imem_266_bus0_pll_list},
};

struct devfreq_clk_info aclk_imem_200[] = {
	{INT_LV0,	167 * MHZ,	0,	&aclk_imem_200_bus1_pll_list},
	{INT_LV1,	133 * MHZ,	0,	&aclk_imem_200_cci_pll_list},
	{INT_LV2,	133 * MHZ,	0,	&aclk_imem_200_cci_pll_list},
	{INT_LV3,	133 * MHZ,	0,	&aclk_imem_200_cci_pll_list},
	{INT_LV4,	133 * MHZ,	0,	&aclk_imem_200_cci_pll_list},
	{INT_LV5,	133 * MHZ,	0,	&aclk_imem_200_cci_pll_list},
	{INT_LV6,	133 * MHZ,	0,	&aclk_imem_200_cci_pll_list},
	{INT_LV7,	133 * MHZ,	0,	&aclk_imem_200_cci_pll_list},
	{INT_LV8,	100 * MHZ,	0,	&aclk_imem_200_bus0_pll_list},
	{INT_LV9,	84 * MHZ,	0,	&aclk_imem_200_bus1_pll_list},
	{INT_LV10,	50 * MHZ,	0,	&aclk_imem_200_bus0_pll_list},
	{INT_LV11,	50 * MHZ,	0,	&aclk_imem_200_bus0_pll_list},
};

struct devfreq_clk_info aclk_imem_100[] = {
	{INT_LV0,	100 * MHZ,	0,	NULL},
	{INT_LV1,	100 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	100 * MHZ,	0,	NULL},
	{INT_LV4,	100 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
	{INT_LV8,	100 * MHZ,	0,	NULL},
	{INT_LV9,	100 * MHZ,	0,	NULL},
	{INT_LV10,	100 * MHZ,	0,	NULL},
	{INT_LV11,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_mfc_532[] = {
	{INT_LV0,	468 * MHZ,	0,	&aclk_mfc_532_mfc_pll_list},
	{INT_LV1,	400 * MHZ,	0,	&aclk_mfc_532_bus0_pll_list},
	{INT_LV2,	334 * MHZ,	0,	&aclk_mfc_532_bus1_pll_list},
	{INT_LV3,	334 * MHZ,	0,	&aclk_mfc_532_bus1_pll_list},
	{INT_LV4,	334 * MHZ,	0,	&aclk_mfc_532_bus1_pll_list},
	{INT_LV5,	334 * MHZ,	0,	&aclk_mfc_532_bus1_pll_list},
	{INT_LV6,	334 * MHZ,	0,	&aclk_mfc_532_bus1_pll_list},
	{INT_LV7,	334 * MHZ,	0,	&aclk_mfc_532_bus1_pll_list},
	{INT_LV8,	266 * MHZ,	0,	&aclk_mfc_532_cci_pll_list},
	{INT_LV9,	200 * MHZ,	0,	&aclk_mfc_532_bus0_pll_list},
	{INT_LV10,	200 * MHZ,	0,	&aclk_mfc_532_bus0_pll_list},
	{INT_LV11,	100 * MHZ,	0,	&aclk_mfc_532_bus0_pll_list},
};

struct devfreq_clk_info aclk_mscl_532[] = {
	{INT_LV0,	532 * MHZ,	0,	&aclk_mscl_532_cci_pll_list},
	{INT_LV1,	468 * MHZ,	0,	&aclk_mscl_532_mfc_pll_list},
	{INT_LV2,	400 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
	{INT_LV3,	400 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
	{INT_LV4,	400 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
	{INT_LV5,	400 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
	{INT_LV6,	400 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
	{INT_LV7,	400 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
	{INT_LV8,	334 * MHZ,	0,	&aclk_mscl_532_bus1_pll_list},
	{INT_LV9,	266 * MHZ,	0,	&aclk_mscl_532_cci_pll_list},
	{INT_LV10,	200 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
	{INT_LV11,	100 * MHZ,	0,	&aclk_mscl_532_bus0_pll_list},
};

struct devfreq_clk_info aclk_peris_66[] = {
	{INT_LV0,	67 * MHZ,	0,	NULL},
	{INT_LV1,	67 * MHZ,	0,	NULL},
	{INT_LV2,	67 * MHZ,	0,	NULL},
	{INT_LV3,	67 * MHZ,	0,	NULL},
	{INT_LV4,	67 * MHZ,	0,	NULL},
	{INT_LV5,	67 * MHZ,	0,	NULL},
	{INT_LV6,	67 * MHZ,	0,	NULL},
	{INT_LV7,	67 * MHZ,	0,	NULL},
	{INT_LV8,	67 * MHZ,	0,	NULL},
	{INT_LV9,	67 * MHZ,	0,	NULL},
	{INT_LV10,	67 * MHZ,	0,	NULL},
	{INT_LV11,	67 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_fsys0_200[] = {
	{INT_LV0,	100 * MHZ,	0,	NULL},
	{INT_LV1,	100 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	100 * MHZ,	0,	NULL},
	{INT_LV4,	100 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
	{INT_LV8,	100 * MHZ,	0,	NULL},
	{INT_LV9,	100 * MHZ,	0,	NULL},
	{INT_LV10,	100 * MHZ,	0,	NULL},
	{INT_LV11,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_fsys1_200[] = {
	{INT_LV0,	100 * MHZ,	0,	NULL},
	{INT_LV1,	100 * MHZ,	0,	NULL},
	{INT_LV2,	100 * MHZ,	0,	NULL},
	{INT_LV3,	100 * MHZ,	0,	NULL},
	{INT_LV4,	100 * MHZ,	0,	NULL},
	{INT_LV5,	100 * MHZ,	0,	NULL},
	{INT_LV6,	100 * MHZ,	0,	NULL},
	{INT_LV7,	100 * MHZ,	0,	NULL},
	{INT_LV8,	100 * MHZ,	0,	NULL},
	{INT_LV9,	100 * MHZ,	0,	NULL},
	{INT_LV10,	100 * MHZ,	0,	NULL},
	{INT_LV11,	100 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_vpp0_400[] = {
	{INT_LV0,	400 * MHZ,	0,	&aclk_vpp0_400_bus0_pll_list},
	{INT_LV1,	400 * MHZ,	0,	&aclk_vpp0_400_bus0_pll_list},
	{INT_LV2,	400 * MHZ,	0,	&aclk_vpp0_400_bus0_pll_list},
	{INT_LV3,	400 * MHZ,	0,	&aclk_vpp0_400_bus0_pll_list},
	{INT_LV4,	400 * MHZ,	0,	&aclk_vpp0_400_bus0_pll_list},
	{INT_LV5,	400 * MHZ,	0,	&aclk_vpp0_400_bus0_pll_list},
	{INT_LV6,	400 * MHZ,	0,	&aclk_vpp0_400_bus0_pll_list},
	{INT_LV7,	334 * MHZ,	0,	&aclk_vpp0_400_bus1_pll_list},
	{INT_LV8,	266 * MHZ,	0,	&aclk_vpp0_400_cci_pll_list},
	{INT_LV9,	234 * MHZ,	0,	&aclk_vpp0_400_mfc_pll_list},
	{INT_LV10,	156 * MHZ,	0,	&aclk_vpp0_400_mfc_pll_list},
	{INT_LV11,	117 * MHZ,	0,	&aclk_vpp0_400_mfc_pll_list},
};

struct devfreq_clk_info aclk_vpp1_400[] = {
	{INT_LV0,	400 * MHZ,	0,	&aclk_vpp1_400_bus0_pll_list},
	{INT_LV1,	400 * MHZ,	0,	&aclk_vpp1_400_bus0_pll_list},
	{INT_LV2,	400 * MHZ,	0,	&aclk_vpp1_400_bus0_pll_list},
	{INT_LV3,	400 * MHZ,	0,	&aclk_vpp1_400_bus0_pll_list},
	{INT_LV4,	400 * MHZ,	0,	&aclk_vpp1_400_bus0_pll_list},
	{INT_LV5,	400 * MHZ,	0,	&aclk_vpp1_400_bus0_pll_list},
	{INT_LV6,	400 * MHZ,	0,	&aclk_vpp1_400_bus0_pll_list},
	{INT_LV7,	334 * MHZ,	0,	&aclk_vpp1_400_bus1_pll_list},
	{INT_LV8,	266 * MHZ,	0,	&aclk_vpp1_400_cci_pll_list},
	{INT_LV9,	234 * MHZ,	0,	&aclk_vpp1_400_mfc_pll_list},
	{INT_LV10,	156 * MHZ,	0,	&aclk_vpp1_400_mfc_pll_list},
	{INT_LV11,	117 * MHZ,	0,	&aclk_vpp1_400_mfc_pll_list},
};

unsigned long vpp_get_int_freq(unsigned long freq)
{
	int i;

	freq *= 1000;

	for (i = INT_LV_COUNT - 1; i >= INT_LV7; i--) {
		if (aclk_vpp1_400[i].freq >= freq)
			return devfreq_int_opp_list[i].freq;
	}

	return devfreq_int_opp_list[INT_LV0].freq;
}

struct devfreq_clk_info aclk_peric0_66[] = {
	{INT_LV0,	67 * MHZ,	0,	NULL},
	{INT_LV1,	67 * MHZ,	0,	NULL},
	{INT_LV2,	67 * MHZ,	0,	NULL},
	{INT_LV3,	67 * MHZ,	0,	NULL},
	{INT_LV4,	67 * MHZ,	0,	NULL},
	{INT_LV5,	67 * MHZ,	0,	NULL},
	{INT_LV6,	67 * MHZ,	0,	NULL},
	{INT_LV7,	67 * MHZ,	0,	NULL},
	{INT_LV8,	67 * MHZ,	0,	NULL},
	{INT_LV9,	67 * MHZ,	0,	NULL},
	{INT_LV10,	67 * MHZ,	0,	NULL},
	{INT_LV11,	67 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_peric1_66[] = {
	{INT_LV0,	67 * MHZ,	0,	NULL},
	{INT_LV1,	67 * MHZ,	0,	NULL},
	{INT_LV2,	67 * MHZ,	0,	NULL},
	{INT_LV3,	67 * MHZ,	0,	NULL},
	{INT_LV4,	67 * MHZ,	0,	NULL},
	{INT_LV5,	67 * MHZ,	0,	NULL},
	{INT_LV6,	67 * MHZ,	0,	NULL},
	{INT_LV7,	67 * MHZ,	0,	NULL},
	{INT_LV8,	67 * MHZ,	0,	NULL},
	{INT_LV9,	67 * MHZ,	0,	NULL},
	{INT_LV10,	67 * MHZ,	0,	NULL},
	{INT_LV11,	67 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_isp0_trex_532[] = {
	{INT_LV0,	532 * MHZ,	0,	&aclk_isp0_trex_532_cci_pll_list},
	{INT_LV1,	532 * MHZ,	0,	&aclk_isp0_trex_532_cci_pll_list},
	{INT_LV2,	100 * MHZ,	0,	&aclk_isp0_trex_532_bus0_pll_list},
	{INT_LV3,	133 * MHZ,	0,	&aclk_isp0_trex_532_cci_pll_list},
	{INT_LV4,	100 * MHZ,	0,	&aclk_isp0_trex_532_bus0_pll_list},
	{INT_LV5,	133 * MHZ,	0,	&aclk_isp0_trex_532_cci_pll_list},
	{INT_LV6,	100 * MHZ,	0,	&aclk_isp0_trex_532_bus0_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_isp0_trex_532_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_isp0_trex_532_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_isp0_trex_532_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_isp0_trex_532_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_isp0_trex_532_isp_pll_list},
};

struct devfreq_clk_info aclk_isp0_isp0_590[] = {
	{INT_LV0,	400 * MHZ,	0,	&aclk_isp0_isp0_590_bus0_pll_list},
	{INT_LV1,	400 * MHZ,	0,	&aclk_isp0_isp0_590_bus0_pll_list},
	{INT_LV2,	220 * MHZ,	0,	&aclk_isp0_isp0_590_cam_pll_list},
	{INT_LV3,	178 * MHZ,	0,	&aclk_isp0_isp0_590_cci_pll_list},
	{INT_LV4,	165 * MHZ,	0,	&aclk_isp0_isp0_590_cam_pll_list},
	{INT_LV5,	156 * MHZ,	0,	&aclk_isp0_isp0_590_mfc_pll_list},
	{INT_LV6,	83 * MHZ,	0,	&aclk_isp0_isp0_590_cam_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_isp0_isp0_590_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_isp0_isp0_590_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_isp0_isp0_590_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_isp0_isp0_590_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_isp0_isp0_590_isp_pll_list},
};

struct devfreq_clk_info aclk_isp0_tpu_590[] = {
	{INT_LV0,	400 * MHZ,	0,	&aclk_isp0_tpu_590_bus0_pll_list},
	{INT_LV1,	400 * MHZ,	0,	&aclk_isp0_tpu_590_bus0_pll_list},
	{INT_LV2,	35 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
	{INT_LV3,	178 * MHZ,	0,	&aclk_isp0_tpu_590_cci_pll_list},
	{INT_LV4,	35 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
	{INT_LV5,	156 * MHZ,	0,	&aclk_isp0_tpu_590_mfc_pll_list},
	{INT_LV6,	35 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_isp0_tpu_590_isp_pll_list},
};

struct devfreq_clk_info aclk_isp1_isp1_468[] = {
	{INT_LV0,	400 * MHZ,	0,	&aclk_isp1_isp1_468_bus0_pll_list},
	{INT_LV1,	400 * MHZ,	0,	&aclk_isp1_isp1_468_bus0_pll_list},
	{INT_LV2,	330 * MHZ,	0,	&aclk_isp1_isp1_468_cam_pll_list},
	{INT_LV3,	330 * MHZ,	0,	&aclk_isp1_isp1_468_cam_pll_list},
	{INT_LV4,	330 * MHZ,	0,	&aclk_isp1_isp1_468_cam_pll_list},
	{INT_LV5,	330 * MHZ,	0,	&aclk_isp1_isp1_468_cam_pll_list},
	{INT_LV6,	330 * MHZ,	0,	&aclk_isp1_isp1_468_cam_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_isp1_isp1_468_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_isp1_isp1_468_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_isp1_isp1_468_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_isp1_isp1_468_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_isp1_isp1_468_isp_pll_list},
};

struct devfreq_clk_info aclk_isp1_ahb_117[] = {
	{INT_LV0,	200 * MHZ,	0,	&aclk_isp1_ahb_117_bus0_pll_list},
	{INT_LV1,	50 * MHZ,	0,	&aclk_isp1_ahb_117_bus0_pll_list},
	{INT_LV2,	50 * MHZ,	0,	&aclk_isp1_ahb_117_bus0_pll_list},
	{INT_LV3,	50 * MHZ,	0,	&aclk_isp1_ahb_117_bus0_pll_list},
	{INT_LV4,	50 * MHZ,	0,	&aclk_isp1_ahb_117_bus0_pll_list},
	{INT_LV5,	50 * MHZ,	0,	&aclk_isp1_ahb_117_bus0_pll_list},
	{INT_LV6,	50 * MHZ,	0,	&aclk_isp1_ahb_117_bus0_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_isp1_ahb_117_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_isp1_ahb_117_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_isp1_ahb_117_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_isp1_ahb_117_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_isp1_ahb_117_isp_pll_list},
};

struct devfreq_clk_info aclk_cam1_trex_532[] = {
	{INT_LV0,	266 * MHZ,	0,	&aclk_cam1_trex_532_cci_pll_list},
	{INT_LV1,	156 * MHZ,	0,	&aclk_cam1_trex_532_mfc_pll_list},
	{INT_LV2,	132 * MHZ,	0,	&aclk_cam1_trex_532_cam_pll_list},
	{INT_LV3,	133 * MHZ,	0,	&aclk_cam1_trex_532_cci_pll_list},
	{INT_LV4,	266 * MHZ,	0,	&aclk_cam1_trex_532_cci_pll_list},
	{INT_LV5,	133 * MHZ,	0,	&aclk_cam1_trex_532_cci_pll_list},
	{INT_LV6,	100 * MHZ,	0,	&aclk_cam1_trex_532_bus0_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_cam1_trex_532_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_cam1_trex_532_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_cam1_trex_532_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_cam1_trex_532_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_cam1_trex_532_isp_pll_list},
};

struct devfreq_clk_info aclk_cam1_sclvra_491[] = {
	{INT_LV0,	400 * MHZ,	0,	&aclk_cam1_sclvra_491_bus0_pll_list},
	{INT_LV1,	400 * MHZ,	0,	&aclk_cam1_sclvra_491_bus0_pll_list},
	{INT_LV2,	234 * MHZ,	0,	&aclk_cam1_sclvra_491_mfc_pll_list},
	{INT_LV3,	178 * MHZ,	0,	&aclk_cam1_sclvra_491_cci_pll_list},
	{INT_LV4,	165 * MHZ,	0,	&aclk_cam1_sclvra_491_cam_pll_list},
	{INT_LV5,	156 * MHZ,	0,	&aclk_cam1_sclvra_491_mfc_pll_list},
	{INT_LV6,	83 * MHZ,	0,	&aclk_cam1_sclvra_491_cam_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_cam1_sclvra_491_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_cam1_sclvra_491_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_cam1_sclvra_491_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_cam1_sclvra_491_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_cam1_sclvra_491_isp_pll_list},
};

struct devfreq_clk_info aclk_cam1_bnscsis_133[] = {
	{INT_LV0,	133 * MHZ,	0,	&aclk_cam1_bnscsis_133_cci_pll_list},
	{INT_LV1,	34 * MHZ,	0,	&aclk_cam1_bnscsis_133_cci_pll_list},
	{INT_LV2,	34 * MHZ,	0,	&aclk_cam1_bnscsis_133_cci_pll_list},
	{INT_LV3,	34 * MHZ,	0,	&aclk_cam1_bnscsis_133_cci_pll_list},
	{INT_LV4,	34 * MHZ,	0,	&aclk_cam1_bnscsis_133_cci_pll_list},
	{INT_LV5,	34 * MHZ,	0,	&aclk_cam1_bnscsis_133_cci_pll_list},
	{INT_LV6,	34 * MHZ,	0,	&aclk_cam1_bnscsis_133_cci_pll_list},
	{INT_LV7,	3 * MHZ,	0,	&aclk_cam1_bnscsis_133_isp_pll_list},
	{INT_LV8,	3 * MHZ,	0,	&aclk_cam1_bnscsis_133_isp_pll_list},
	{INT_LV9,	3 * MHZ,	0,	&aclk_cam1_bnscsis_133_isp_pll_list},
	{INT_LV10,	3 * MHZ,	0,	&aclk_cam1_bnscsis_133_isp_pll_list},
	{INT_LV11,	3 * MHZ,	0,	&aclk_cam1_bnscsis_133_isp_pll_list},
};

struct devfreq_clk_info aclk_cam1_arm_668[] = {
	{INT_LV0,	552 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV1,	552 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV2,	552 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV3,	552 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV4,	552 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV5,	552 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV6,	552 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV7,	24 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV8,	24 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV9,	24 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV10,	24 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
	{INT_LV11,	24 * MHZ,	0,	&aclk_cam1_arm_668_isp_pll_list},
};

struct devfreq_clk_info aclk_cam1_busperi_334[] = {
	{INT_LV0,	276 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV1,	276 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV2,	276 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV3,	276 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV4,	276 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV5,	276 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV6,	276 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV7,	12 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV8,	12 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV9,	12 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV10,	12 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
	{INT_LV11,	12 * MHZ,	0,	&aclk_cam1_busperi_334_isp_pll_list},
};

struct devfreq_clk_info aclk_cam1_nocp_133[] = {
	{INT_LV0,	133 * MHZ,	0,	&aclk_cam1_nocp_133_cci_pll_list},
	{INT_LV1,	67 * MHZ,	0,	&aclk_cam1_nocp_133_cci_pll_list},
	{INT_LV2,	67 * MHZ,	0,	&aclk_cam1_nocp_133_cci_pll_list},
	{INT_LV3,	67 * MHZ,	0,	&aclk_cam1_nocp_133_cci_pll_list},
	{INT_LV4,	67 * MHZ,	0,	&aclk_cam1_nocp_133_cci_pll_list},
	{INT_LV5,	67 * MHZ,	0,	&aclk_cam1_nocp_133_cci_pll_list},
	{INT_LV6,	67 * MHZ,	0,	&aclk_cam1_nocp_133_cci_pll_list},
	{INT_LV7,	6 * MHZ,	0,	&aclk_cam1_nocp_133_isp_pll_list},
	{INT_LV8,	6 * MHZ,	0,	&aclk_cam1_nocp_133_isp_pll_list},
	{INT_LV9,	6 * MHZ,	0,	&aclk_cam1_nocp_133_isp_pll_list},
	{INT_LV10,	6 * MHZ,	0,	&aclk_cam1_nocp_133_isp_pll_list},
	{INT_LV11,	6 * MHZ,	0,	&aclk_cam1_nocp_133_isp_pll_list},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	aclk_bus0_532,
	aclk_bus1_532,
	aclk_bus1_200,
	pclk_bus01_133,
	aclk_imem_266,
	aclk_imem_200,
	//aclk_imem_100,
	aclk_mfc_532,
	aclk_mscl_532,
	//aclk_peris_66,
	//aclk_fsys0_200,
	//aclk_fsys1_200,
	aclk_vpp0_400,
	aclk_vpp1_400,
	//aclk_peric0_66,
	//aclk_peric1_66,
	aclk_isp0_trex_532,
	aclk_isp0_isp0_590,
	aclk_isp0_tpu_590,
	aclk_isp1_isp1_468,
	aclk_isp1_ahb_117,
	aclk_cam1_trex_532,
	aclk_cam1_sclvra_491,
	aclk_cam1_bnscsis_133,
	aclk_cam1_arm_668,
	aclk_cam1_busperi_334,
	aclk_cam1_nocp_133,
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	DOUT_ACLK_BUS0_532,
	DOUT_ACLK_BUS1_532,
	DOUT_ACLK_BUS1_200,
	DOUT_PCLK_BUS01_133,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	//DOUT_ACLK_IMEM_100,
	DOUT_ACLK_MFC_532,
	DOUT_ACLK_MSCL_532,
	//DOUT_ACLK_PERIS_66,
	//DOUT_ACLK_FSYS0_200,
	//DOUT_ACLK_FSYS1_200,
	DOUT_ACLK_VPP0_400,
	DOUT_ACLK_VPP1_400,
	//DOUT_ACLK_PERIC0_66,
	//DOUT_ACLK_PERIC1_66,
	DOUT_ACLK_ISP0_TREX_532,
	DOUT_ACLK_ISP0_ISP0_590,
	DOUT_ACLK_ISP0_TPU_590,
	DOUT_ACLK_ISP1_ISP1_468,
	DOUT_ACLK_ISP1_AHB_117,
	DOUT_ACLK_CAM1_TREX_532,
	DOUT_ACLK_CAM1_SCLVRA_491,
	DOUT_ACLK_CAM1_BNSCSIS_133,
	DOUT_ACLK_CAM1_ARM_668,
	DOUT_ACLK_CAM1_BUSPERI_334,
	DOUT_ACLK_CAM1_NOCP_133,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_int_pm_domain[] = {
	{NULL,},	/* NULL means always_on */
	{NULL,},
	{NULL,},
	{NULL,},
	{NULL,},
	{NULL,},
	//{NULL,},
	{"pd-mfc",},
	{"pd-mscl",},
	//{NULL,},
	//{NULL,},
	//{NULL,},
	{"pd-vpp",},
	{"pd-vpp",},
	//{NULL,},
	//{NULL,},
	{"pd-isp0",},
	{"pd-isp0",},
	{"pd-isp0",},
	{"pd-isp1",},
	{"pd-isp1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
	{"pd-cam1",},
};
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

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
		if (strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_mfc_532") &&
				strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_mscl_532") &&
				strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_vpp0_400") &&
				strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_vpp1_400"))
			clk_prepare_enable(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk);
	}

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

static int exynos7_devfreq_int_get_volt(struct devfreq_data_int *data)
{
	return regulator_get_voltage(data->vdd_int);
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

			/* In order to prevent undershooting */
			if ((clk_info->freq != 0) &&
				(strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_vpp0_400") &&
				 strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_vpp1_400") &&
				 strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_bus0_532") &&
				 strcmp(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name, "dout_aclk_bus1_532")))
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
	struct opp *target_opp;
	unsigned long freq = 0;

	if (event == TMU_COLD) {
		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset, data->volt_of_avail_max_freq);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			freq = data->cur_freq;
			mutex_unlock(&data->lock);
			pr_info("INT(%lu): set TMU_COLD: %d => %d\n", freq, prev_volt, regulator_get_voltage(data->vdd_int));
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			rcu_read_lock();
			freq = data->cur_freq;
			target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
			set_volt = (uint32_t)opp_get_voltage(target_opp);
			rcu_read_unlock();

			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
			pr_info("INT(%lu): unset TMU_COLD: %d => %d\n", freq, prev_volt, regulator_get_voltage(data->vdd_int));
		}
	}
	return NOTIFY_OK;
}
#endif

#ifdef CONFIG_PM_RUNTIME
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
		if ((clk_info[target_idx].freq != 0) &&
				(strcmp(__clk_get_name(clk), "dout_aclk_vpp0_400") &&
				 strcmp(__clk_get_name(clk), "dout_aclk_vpp1_400")))
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
                                                data_int->cur_freq);
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

int exynos7420_devfreq_int_init(struct devfreq_data_int *data)
{
	int ret = 0;

	data_int = data;
	data->max_state = INT_LV_COUNT;

	if (exynos7_devfreq_int_init_clock()) {
		ret = -EINVAL;
		return ret;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_int_pm_domain, ARRAY_SIZE(devfreq_int_pm_domain))) {
		ret = -EINVAL;
		return ret;
	}
#endif
	data->int_set_volt = exynos7_devfreq_int_set_volt;
	data->int_get_volt = exynos7_devfreq_int_get_volt;
	data->int_set_freq = exynos7_devfreq_int_set_freq;
#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos7_devfreq_int_tmu_notifier;
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
	CAM_PLL,
	_ISP_PLL,
	MOUT_ACLK_CAM0_TREX_532,
	MOUT_ACLK_CAM0_CSIS0_690,
	MOUT_ACLK_CAM0_BNSA_690,
	_MOUT_BUS0_PLL_TOP0,
	_MOUT_BUS1_PLL_TOP0,
	MOUT_ACLK_CAM0_3AA0_690,
	_MOUT_MFC_PLL_TOP0,
	_MOUT_CCI_PLL_TOP0,
	MOUT_ACLK_CAM0_3AA1_468,
	MOUT_ACLK_CAM0_CSIS1_174,
	MOUT_ACLK_CAM0_BNSB_690,
	MOUT_ACLK_CAM0_BNSD_690,
	DOUT_ACLK_CAM0_TREX_532,
	DOUT_ACLK_CAM0_CSIS0_690,
	DOUT_ACLK_CAM0_BNSA_690,
	DOUT_ACLK_CAM0_3AA0_690,
	DOUT_ACLK_CAM0_3AA1_468,
	DOUT_ACLK_CAM0_CSIS1_174,
	DOUT_ACLK_CAM0_BNSB_690,
	DOUT_ACLK_CAM0_CSIS3_133,
	DOUT_ACLK_CAM0_BNSD_690,
	DOUT_ACLK_CAM0_NOCP_133,
	ISP_CLK_COUNT,
};

struct devfreq_clk_list devfreq_isp_clk[ISP_CLK_COUNT] = {
	{"cam_pll",},
	{"isp_pll",},
	{"mout_aclk_cam0_trex_532",},
	{"mout_aclk_cam0_csis0_690",},
	{"mout_aclk_cam0_bnsa_690",},
	{"mout_bus0_pll_top0",},
	{"mout_bus1_pll_top0",},
	{"mout_aclk_cam0_3aa0_690",},
	{"mout_mfc_pll_top0",},
	{"mout_cci_pll_top0",},
	{"mout_aclk_cam0_3aa1_468",},
	{"mout_aclk_cam0_csis1_174",},
	{"mout_aclk_cam0_bnsb_690",},
	{"mout_aclk_cam0_bnsd_690",},
	{"dout_aclk_cam0_trex_532",},
	{"dout_aclk_cam0_csis0_690",},
	{"dout_aclk_cam0_bnsa_690",},
	{"dout_aclk_cam0_3aa0_690",},
	{"dout_aclk_cam0_3aa1_468",},
	{"dout_aclk_cam0_csis1_174",},
	{"dout_aclk_cam0_bnsb_690",},
	{"dout_aclk_cam0_csis3_133"},
	{"dout_aclk_cam0_bnsd_690",},
	{"dout_aclk_cam0_nocp_133",},
};

struct devfreq_opp_table devfreq_isp_opp_list[] = {
	{ISP_LV0,	540000,	900000},
	{ISP_LV1,	530000,	900000},
	{ISP_LV2,	520000,	900000},
	{ISP_LV3,	510000,	900000},
	{ISP_LV4,	500000,	900000},
};

struct devfreq_clk_state aclk_cam0_trex_532_mfc_pll[] = {
	{MOUT_ACLK_CAM0_TREX_532,	_MOUT_MFC_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_trex_532_mfc_pll_list = {
	.state = aclk_cam0_trex_532_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_trex_532_mfc_pll),
};

struct devfreq_clk_state aclk_cam0_trex_532_isp_pll[] = {
	{MOUT_ACLK_CAM0_TREX_532,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_trex_532_isp_pll_list = {
	.state = aclk_cam0_trex_532_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_trex_532_isp_pll),
};

struct devfreq_clk_state aclk_cam0_trex_532_cci_pll[] = {
	{MOUT_ACLK_CAM0_TREX_532,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_trex_532_cci_pll_list = {
	.state = aclk_cam0_trex_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_trex_532_cci_pll),
};

struct devfreq_clk_state aclk_cam0_trex_532_bus0_pll[] = {
	{MOUT_ACLK_CAM0_TREX_532,	_MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_trex_532_bus0_pll_list = {
	.state = aclk_cam0_trex_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_trex_532_bus0_pll),
};

struct devfreq_clk_state aclk_cam0_csis0_690_isp_pll[] = {
	{MOUT_ACLK_CAM0_CSIS0_690,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_csis0_690_isp_pll_list = {
	.state = aclk_cam0_csis0_690_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis0_690_isp_pll),
};

struct devfreq_clk_state aclk_cam0_csis0_690_cam_pll[] = {
	{MOUT_ACLK_CAM0_CSIS0_690,	CAM_PLL},
};

struct devfreq_clk_states aclk_cam0_csis0_690_cam_pll_list = {
	.state = aclk_cam0_csis0_690_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis0_690_cam_pll),
};

struct devfreq_clk_state aclk_cam0_csis0_690_mfc_pll[] = {
	{MOUT_ACLK_CAM0_CSIS0_690,	_MOUT_MFC_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_csis0_690_mfc_pll_list = {
	.state = aclk_cam0_csis0_690_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis0_690_mfc_pll),
};

struct devfreq_clk_state aclk_cam0_csis0_690_cci_pll[] = {
	{MOUT_ACLK_CAM0_CSIS0_690,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_csis0_690_cci_pll_list = {
	.state = aclk_cam0_csis0_690_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis0_690_cci_pll),
};

struct devfreq_clk_state aclk_cam0_csis0_690_bus0_pll[] = {
	{MOUT_ACLK_CAM0_CSIS0_690,	_MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_csis0_690_bus0_pll_list = {
	.state = aclk_cam0_csis0_690_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis0_690_bus0_pll),
};


struct devfreq_clk_state aclk_cam0_bnsa_690_isp_pll[] = {
	{MOUT_ACLK_CAM0_BNSA_690,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_bnsa_690_isp_pll_list = {
	.state = aclk_cam0_bnsa_690_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsa_690_isp_pll),
};

struct devfreq_clk_state aclk_cam0_bnsa_690_mfc_pll[] = {
	{MOUT_ACLK_CAM0_BNSA_690,	_MOUT_MFC_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsa_690_mfc_pll_list = {
	.state = aclk_cam0_bnsa_690_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsa_690_mfc_pll),
};

struct devfreq_clk_state aclk_cam0_bnsa_690_cci_pll[] = {
	{MOUT_ACLK_CAM0_BNSA_690,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsa_690_cci_pll_list = {
	.state = aclk_cam0_bnsa_690_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsa_690_cci_pll),
};

struct devfreq_clk_state aclk_cam0_bnsa_690_bus0_pll[] = {
	{MOUT_ACLK_CAM0_BNSA_690,	_MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsa_690_bus0_pll_list = {
	.state = aclk_cam0_bnsa_690_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsa_690_bus0_pll),
};

struct devfreq_clk_state aclk_cam0_bnsa_690_cam_pll[] = {
	{MOUT_ACLK_CAM0_BNSA_690,	CAM_PLL},
};

struct devfreq_clk_states aclk_cam0_bnsa_690_cam_pll_list = {
	.state = aclk_cam0_bnsa_690_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsa_690_cam_pll),
};

struct devfreq_clk_state aclk_cam0_3aa0_690_bus1_pll[] = {
	{MOUT_ACLK_CAM0_3AA0_690,	_MOUT_BUS1_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_3aa0_690_bus1_pll_list = {
	.state = aclk_cam0_3aa0_690_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa0_690_bus1_pll),
};

struct devfreq_clk_state aclk_cam0_3aa0_690_isp_pll[] = {
	{MOUT_ACLK_CAM0_3AA0_690,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_3aa0_690_isp_pll_list = {
	.state = aclk_cam0_3aa0_690_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa0_690_isp_pll),
};

struct devfreq_clk_state aclk_cam0_3aa0_690_cam_pll[] = {
	{MOUT_ACLK_CAM0_3AA0_690,	CAM_PLL},
};

struct devfreq_clk_states aclk_cam0_3aa0_690_cam_pll_list = {
	.state = aclk_cam0_3aa0_690_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa0_690_cam_pll),
};

struct devfreq_clk_state aclk_cam0_3aa0_690_mfc_pll[] = {
	{MOUT_ACLK_CAM0_3AA0_690,	_MOUT_MFC_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_3aa0_690_mfc_pll_list = {
	.state = aclk_cam0_3aa0_690_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa0_690_mfc_pll),
};

struct devfreq_clk_state aclk_cam0_3aa0_690_cci_pll[] = {
	{MOUT_ACLK_CAM0_3AA0_690,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_3aa0_690_cci_pll_list = {
	.state = aclk_cam0_3aa0_690_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa0_690_cci_pll),
};

struct devfreq_clk_state aclk_cam0_3aa0_690_bus0_pll[] = {
	{MOUT_ACLK_CAM0_3AA0_690,	_MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_3aa0_690_bus0_pll_list = {
	.state = aclk_cam0_3aa0_690_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa0_690_bus0_pll),
};

struct devfreq_clk_state aclk_cam0_3aa1_468_cam_pll[] = {
	{MOUT_ACLK_CAM0_3AA1_468,	CAM_PLL},
};

struct devfreq_clk_states aclk_cam0_3aa1_468_cam_pll_list = {
	.state = aclk_cam0_3aa1_468_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa1_468_cam_pll),
};

struct devfreq_clk_state aclk_cam0_3aa1_468_isp_pll[] = {
	{MOUT_ACLK_CAM0_3AA1_468,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_3aa1_468_isp_pll_list = {
	.state = aclk_cam0_3aa1_468_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa1_468_isp_pll),
};

struct devfreq_clk_state aclk_cam0_3aa1_468_mfc_pll[] = {
	{MOUT_ACLK_CAM0_3AA1_468,	_MOUT_MFC_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_3aa1_468_mfc_pll_list = {
	.state = aclk_cam0_3aa1_468_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa1_468_mfc_pll),
};

struct devfreq_clk_state aclk_cam0_3aa1_468_cci_pll[] = {
	{MOUT_ACLK_CAM0_3AA1_468,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_3aa1_468_cci_pll_list = {
	.state = aclk_cam0_3aa1_468_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_3aa1_468_cci_pll),
};

struct devfreq_clk_state aclk_cam0_csis1_174_isp_pll[] = {
	{MOUT_ACLK_CAM0_CSIS1_174,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_csis1_174_isp_pll_list = {
	.state = aclk_cam0_csis1_174_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis1_174_isp_pll),
};

struct devfreq_clk_state aclk_cam0_csis1_174_cci_pll[] = {
	{MOUT_ACLK_CAM0_CSIS1_174,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_csis1_174_cci_pll_list = {
	.state = aclk_cam0_csis1_174_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis1_174_cci_pll),
};

struct devfreq_clk_state aclk_cam0_csis1_174_bus1_pll[] = {
	{MOUT_ACLK_CAM0_CSIS1_174,	_MOUT_BUS1_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_csis1_174_bus1_pll_list = {
	.state = aclk_cam0_csis1_174_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis1_174_bus1_pll),
};

struct devfreq_clk_state aclk_cam0_csis1_174_bus0_pll[] = {
	{MOUT_ACLK_CAM0_CSIS1_174,	_MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_csis1_174_bus0_pll_list = {
	.state = aclk_cam0_csis1_174_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_csis1_174_bus0_pll),
};

struct devfreq_clk_state aclk_cam0_bnsb_690_bus0_pll[] = {
	{MOUT_ACLK_CAM0_BNSB_690,	_MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsb_690_bus0_pll_list = {
	.state = aclk_cam0_bnsb_690_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsb_690_bus0_pll),
};

struct devfreq_clk_state aclk_cam0_bnsb_690_bus1_pll[] = {
	{MOUT_ACLK_CAM0_BNSB_690,	_MOUT_BUS1_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsb_690_bus1_pll_list = {
	.state = aclk_cam0_bnsb_690_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsb_690_bus1_pll),
};

struct devfreq_clk_state aclk_cam0_bnsb_690_isp_pll[] = {
	{MOUT_ACLK_CAM0_BNSB_690,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_bnsb_690_isp_pll_list = {
	.state = aclk_cam0_bnsb_690_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsb_690_isp_pll),
};

struct devfreq_clk_state aclk_cam0_bnsb_690_cam_pll[] = {
	{MOUT_ACLK_CAM0_BNSB_690,	CAM_PLL},
};

struct devfreq_clk_states aclk_cam0_bnsb_690_cam_pll_list = {
	.state = aclk_cam0_bnsb_690_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsb_690_cam_pll),
};

struct devfreq_clk_state aclk_cam0_bnsb_690_cci_pll[] = {
	{MOUT_ACLK_CAM0_BNSB_690,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsb_690_cci_pll_list = {
	.state = aclk_cam0_bnsb_690_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsb_690_cci_pll),
};

struct devfreq_clk_state aclk_cam0_bnsd_690_cci_pll[] = {
	{MOUT_ACLK_CAM0_BNSD_690,	_MOUT_CCI_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsd_690_cci_pll_list = {
	.state = aclk_cam0_bnsd_690_cci_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsd_690_cci_pll),
};

struct devfreq_clk_state aclk_cam0_bnsd_690_cam_pll[] = {
	{MOUT_ACLK_CAM0_BNSD_690,	CAM_PLL},
};

struct devfreq_clk_states aclk_cam0_bnsd_690_cam_pll_list = {
	.state = aclk_cam0_bnsd_690_cam_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsd_690_cam_pll),
};

struct devfreq_clk_state aclk_cam0_bnsd_690_isp_pll[] = {
	{MOUT_ACLK_CAM0_BNSD_690,	_ISP_PLL},
};

struct devfreq_clk_states aclk_cam0_bnsd_690_isp_pll_list = {
	.state = aclk_cam0_bnsd_690_isp_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsd_690_isp_pll),
};

struct devfreq_clk_state aclk_cam0_bnsd_690_mfc_pll[] = {
	{MOUT_ACLK_CAM0_BNSD_690,	_MOUT_MFC_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsd_690_mfc_pll_list = {
	.state = aclk_cam0_bnsd_690_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsd_690_mfc_pll),
};

struct devfreq_clk_state aclk_cam0_bnsd_690_bus0_pll[] = {
	{MOUT_ACLK_CAM0_BNSD_690,	_MOUT_BUS0_PLL_TOP0},
};

struct devfreq_clk_states aclk_cam0_bnsd_690_bus0_pll_list = {
	.state = aclk_cam0_bnsd_690_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bnsd_690_bus0_pll),
};

struct devfreq_clk_info aclk_cam0_trex_532[] = {
	{ISP_LV0,	532 * MHZ,	0,	&aclk_cam0_trex_532_cci_pll_list},
	{ISP_LV1,	266 * MHZ,	0,	&aclk_cam0_trex_532_cci_pll_list},
	{ISP_LV2,	266 * MHZ,	0,	&aclk_cam0_trex_532_cci_pll_list},
	{ISP_LV3,	178 * MHZ,	0,	&aclk_cam0_trex_532_cci_pll_list},
	{ISP_LV4,	133 * MHZ,	0,	&aclk_cam0_trex_532_cci_pll_list},
};

struct devfreq_clk_info aclk_cam0_csis0_690[] = {
	{ISP_LV0,	660 * MHZ,	0,	&aclk_cam0_csis0_690_cam_pll_list},
	{ISP_LV1,	552 * MHZ,	0,	&aclk_cam0_csis0_690_isp_pll_list},
	{ISP_LV2,	468 * MHZ,	0,	&aclk_cam0_csis0_690_mfc_pll_list},
	{ISP_LV3,	266 * MHZ,	0,	&aclk_cam0_csis0_690_cci_pll_list},
	{ISP_LV4,	200 * MHZ,	0,	&aclk_cam0_csis0_690_bus0_pll_list},
};

struct devfreq_clk_info aclk_cam0_bnsa_690[] = {
	{ISP_LV0,	660 * MHZ,	0,	&aclk_cam0_bnsa_690_cam_pll_list},
	{ISP_LV1,	552 * MHZ,	0,	&aclk_cam0_bnsa_690_isp_pll_list},
	{ISP_LV2,	468 * MHZ,	0,	&aclk_cam0_bnsa_690_mfc_pll_list},
	{ISP_LV3,	266 * MHZ,	0,	&aclk_cam0_bnsa_690_cci_pll_list},
	{ISP_LV4,	200 * MHZ,	0,	&aclk_cam0_bnsa_690_bus0_pll_list},
};

struct devfreq_clk_info aclk_cam0_3aa0_690[] = {
	{ISP_LV0,	660 * MHZ,	0,	&aclk_cam0_3aa0_690_cam_pll_list},
	{ISP_LV1,	552 * MHZ,	0,	&aclk_cam0_3aa0_690_isp_pll_list},
	{ISP_LV2,	220 * MHZ,	0,	&aclk_cam0_3aa0_690_cam_pll_list},
	{ISP_LV3,	266 * MHZ,	0,	&aclk_cam0_3aa0_690_cci_pll_list},
	{ISP_LV4,	200 * MHZ,	0,	&aclk_cam0_3aa0_690_bus0_pll_list},
};

struct devfreq_clk_info aclk_cam0_3aa1_468[] = {
	{ISP_LV0,	468 * MHZ,	0,	&aclk_cam0_3aa1_468_mfc_pll_list},
	{ISP_LV1,	42 * MHZ,	0,	&aclk_cam0_3aa1_468_cam_pll_list},
	{ISP_LV2,	234 * MHZ,	0,	&aclk_cam0_3aa1_468_mfc_pll_list},
	{ISP_LV3,	42 * MHZ,	0,	&aclk_cam0_3aa1_468_cam_pll_list},
	{ISP_LV4,	42 * MHZ,	0,	&aclk_cam0_3aa1_468_cam_pll_list},
};

struct devfreq_clk_info aclk_cam0_csis1_174[] = {
	{ISP_LV0,	276 * MHZ,	0,	&aclk_cam0_csis1_174_isp_pll_list},
	{ISP_LV1,	35 * MHZ,	0,	&aclk_cam0_csis1_174_isp_pll_list},
	{ISP_LV2,	200 * MHZ,	0,	&aclk_cam0_csis1_174_bus0_pll_list},
	{ISP_LV3,	35 * MHZ,	0,	&aclk_cam0_csis1_174_isp_pll_list},
	{ISP_LV4,	200 * MHZ,	0,	&aclk_cam0_csis1_174_bus0_pll_list},
};

struct devfreq_clk_info aclk_cam0_bnsb_690[] = {
	{ISP_LV0,	276 * MHZ,	0,	&aclk_cam0_bnsb_690_isp_pll_list},
	{ISP_LV1,	42 * MHZ,	0,	&aclk_cam0_bnsb_690_cam_pll_list},
	{ISP_LV2,	200 * MHZ,	0,	&aclk_cam0_bnsb_690_bus0_pll_list},
	{ISP_LV3,	42 * MHZ,	0,	&aclk_cam0_bnsb_690_cam_pll_list},
	{ISP_LV4,	200 * MHZ,	0,	&aclk_cam0_bnsb_690_bus0_pll_list},
};

struct devfreq_clk_info aclk_cam0_csis3_133[] = {
	{ISP_LV0,	133 * MHZ,	0,	NULL},
	{ISP_LV1,	34 * MHZ,	0,	NULL},
	{ISP_LV2,	34 * MHZ,	0,	NULL},
	{ISP_LV3,	34 * MHZ,	0,	NULL},
	{ISP_LV4,	34 * MHZ,	0,	NULL},
};

struct devfreq_clk_info aclk_cam0_bnsd_690[] = {
	{ISP_LV0,	660 * MHZ,	0,	&aclk_cam0_bnsd_690_cam_pll_list},
	{ISP_LV1,	552 * MHZ,	0,	&aclk_cam0_bnsd_690_isp_pll_list},
	{ISP_LV2,	468 * MHZ,	0,	&aclk_cam0_bnsd_690_mfc_pll_list},
	{ISP_LV3,	266 * MHZ,	0,	&aclk_cam0_bnsd_690_cci_pll_list},
	{ISP_LV4,	200 * MHZ,	0,	&aclk_cam0_bnsd_690_bus0_pll_list},
};

struct devfreq_clk_info aclk_cam0_nocp_133[] = {
	{ISP_LV0,	133 * MHZ,	0,	NULL},
	{ISP_LV1,	34 * MHZ,	0,	NULL},
	{ISP_LV2,	34 * MHZ,	0,	NULL},
	{ISP_LV3,	34 * MHZ,	0,	NULL},
	{ISP_LV4,	34 * MHZ,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_isp_info_list[] = {
	aclk_cam0_trex_532,
	aclk_cam0_csis0_690,
	aclk_cam0_bnsa_690,
	aclk_cam0_3aa0_690,
	aclk_cam0_3aa1_468,
	aclk_cam0_csis1_174,
	aclk_cam0_bnsb_690,
	aclk_cam0_csis3_133,
	aclk_cam0_bnsd_690,
	aclk_cam0_nocp_133,
};

enum devfreq_isp_clk devfreq_clk_isp_info_idx[] = {
	DOUT_ACLK_CAM0_TREX_532,
	DOUT_ACLK_CAM0_CSIS0_690,
	DOUT_ACLK_CAM0_BNSA_690,
	DOUT_ACLK_CAM0_3AA0_690,
	DOUT_ACLK_CAM0_3AA1_468,
	DOUT_ACLK_CAM0_CSIS1_174,
	DOUT_ACLK_CAM0_BNSB_690,
	DOUT_ACLK_CAM0_CSIS3_133,
	DOUT_ACLK_CAM0_BNSD_690,
	DOUT_ACLK_CAM0_NOCP_133,
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

	if (!data_isp->use_dvfs)
		return;

	mutex_lock(&data_isp->lock);

	if (!strcmp(pd_name, "pd-cam0")) {
		if (turn_on) {
			if (!data_isp->vdd_disp_cam0)
				data_isp->vdd_disp_cam0 = regulator_get(NULL, "vdd_disp_cam0");
		} else {
			if (data_isp->vdd_disp_cam0) {
				regulator_put(data_isp->vdd_disp_cam0);
				data_isp->vdd_disp_cam0 = NULL;
				data_isp->old_volt = 0;
			}
			if (data_disp->vdd_disp_cam0)
				regulator_sync_voltage(data_disp->vdd_disp_cam0);
		}
	}

	if (!turn_on) {
		mutex_unlock(&data_isp->lock);
		return;
	}

	cur_freq_idx = devfreq_get_opp_idx(devfreq_isp_opp_list,
			ARRAY_SIZE(devfreq_isp_opp_list),
			data_isp->cur_freq);
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
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif
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

static int exynos7_devfreq_isp_set_volt(struct devfreq_data_isp *data,
		unsigned long volt,
		unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_disp_cam0, volt, volt_range);
	pr_debug("ISP: set_volt(%lu), get_volt(%d)\n", volt, regulator_get_voltage(data->vdd_disp_cam0));
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
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos7_devfreq_isp_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_isp *data = container_of(nb, struct devfreq_data_isp, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;
	struct opp *target_opp;
	unsigned long freq = 0;

	if (event == TMU_COLD) {
		if (*on) {
			mutex_lock(&data->lock);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			/* if pd-cam0 is off */
			if (!data->vdd_disp_cam0) {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			prev_volt = (uint32_t)data->old_volt;

			set_volt = get_limit_voltage(prev_volt, data->volt_offset, data->volt_of_avail_max_freq);
			regulator_set_voltage(data->vdd_disp_cam0, set_volt, REGULATOR_MAX_MICROVOLT);

			freq = data->cur_freq;
			mutex_unlock(&data->lock);
			pr_info("ISP(%lu): set TMU_COLD: %d => %d\n", freq, prev_volt, regulator_get_voltage(data->vdd_disp_cam0));
		} else {
			mutex_lock(&data->lock);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			/* if pd-cam0 is off */
			if (!data->vdd_disp_cam0) {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			rcu_read_lock();
			freq = data->cur_freq;
			target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
			set_volt = (uint32_t)opp_get_voltage(target_opp);
			rcu_read_unlock();

			regulator_set_voltage(data->vdd_disp_cam0, set_volt, REGULATOR_MAX_MICROVOLT);
			data->old_volt = (unsigned long)set_volt;

			mutex_unlock(&data->lock);
			pr_info("ISP(%lu): unset TMU_COLD: %d\n", freq, regulator_get_voltage(data->vdd_disp_cam0));
		}
	}
	return NOTIFY_OK;
}
#endif

int exynos7420_devfreq_isp_init(struct devfreq_data_isp *data)
{
	int ret = 0;
	data_isp = data;
	data->max_state = ISP_LV_COUNT;
	if (exynos7_devfreq_isp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_isp_pm_domain, ARRAY_SIZE(devfreq_isp_pm_domain))) {
		ret = -EINVAL;
		goto err_data;
	}
#endif
	data->isp_set_freq = exynos7_devfreq_isp_set_freq;
	data->isp_set_volt = exynos7_devfreq_isp_set_volt;
#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos7_devfreq_isp_tmu_notifier;
#endif
err_data:
	return ret;
}
int exynos7420_devfreq_isp_deinit(struct devfreq_data_isp *data)
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
	MIF_LV9,
	MIF_LV10,
	MIF_LV11,
	MIF_LV12,
	MIF_LV_COUNT,
};

enum devfreq_mif_clk {
	MIF_PLL,
	SCLK_BUS0_PLL_MIF,
	DOUT_SCLK_BUS0_PLL_MIF,
	BUS0_PLL,
	FFAC_TOPC_BUS0_PLL_DIV2,
	MOUT_SCLK_BUS0_PLL_MIF,
	MOUT_ACLK_MIF_PLL,

	MOUT_BUS0_PLL_CMUC,
	BUS1_PLL,
	CCI_PLL,
	MFC_PLL,
	FFAC_MOUT_BUS0_PLL_CMUC_DIV2,
	FFAC_TOPC_BUS1_PLL_DIV2,
	FFAC_TOPC_CCI_PLL_DIV2,
	FFAC_TOPC_MFC_PLL_DIV2,
	FFAC_MOUT_BUS0_PLL_CMUC_DIV4,
	FFAC_TOPC_BUS1_PLL_DIV4,
	FFAC_TOPC_CCI_PLL_DIV4,
	FFAC_TOPC_MFC_PLL_DIV4,

	MOUT_ACLK_CCORE_133,
	DOUT_ACLK_CCORE_133,
	DOUT_ACLK_CCORE_266,
	MOUT_ACLK_CCORE_532,
	DOUT_ACLK_CCORE_532,
	MOUT_CELL_CLKSEL,
	DOUT_PCLK_MIF0,
	DOUT_PCLK_MIF1,
	DOUT_PCLK_MIF2,
	DOUT_PCLK_MIF3,
	MIF_CLK_COUNT,
};

struct devfreq_clk_list devfreq_mif_clk[MIF_CLK_COUNT] = {
	{"mif_pll"},
	{"sclk_bus0_pll_mif"},
	{"dout_sclk_bus0_pll_mif"},
	{"bus0_pll"},
	{"ffac_topc_bus0_pll_div2"},
	{"mout_sclk_bus0_pll_mif"},
	{"mout_aclk_mif_pll"},

	{"mout_bus0_pll_cmuc"},
	{"bus1_pll"},
	{"cci_pll"},
	{"mfc_pll"},
	{"ffac_mout_bus0_pll_cmuc_div2"},
	{"ffac_topc_bus1_pll_div2"},
	{"ffac_topc_cci_pll_div2"},
	{"ffac_topc_mfc_pll_div2"},
	{"ffac_mout_bus0_pll_cmuc_div4"},
	{"ffac_topc_bus1_pll_div4"},
	{"ffac_topc_cci_pll_div4"},
	{"ffac_topc_mfc_pll_div4"},

	{"mout_aclk_ccore_133"},
	{"dout_aclk_ccore_133"},
	{"dout_aclk_ccore_266"},
	{"mout_aclk_ccore_532"},
	{"dout_aclk_ccore_532"},
	{"mout_cell_clksel"},
	{"dout_pclk_mif0"},
	{"dout_pclk_mif1"},
	{"dout_pclk_mif2"},
	{"dout_pclk_mif3"},
};

/*  DDR_PHY use half frequency clock with internal dividing */
struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{MIF_LV0,	3104000/2,	900000},
	{MIF_LV1,	2912000/2,	900000},
	{MIF_LV2,	2528000/2,	900000},
	{MIF_LV3,	2052000/2,	900000},
	{MIF_LV4,	1656000/2,	900000},
	{MIF_LV5,	1264000/2,	900000},
	{MIF_LV6,	1086000/2,	900000},
	{MIF_LV7,	832000/2,	900000},
	{MIF_LV8,	696000/2,	900000},
	{MIF_LV9,	552000/2,	900000},
	{MIF_LV10,	334000/2,	900000},
	{MIF_LV11,	266000/2,	900000},
	{MIF_LV12,	200000/2,	900000},
};

struct devfreq_clk_state aclk_ccore_532_bus0_pll[] = {
	{MOUT_ACLK_CCORE_532,	MOUT_BUS0_PLL_CMUC},
};

struct devfreq_clk_states aclk_ccore_532_bus0_pll_list = {
	.state = aclk_ccore_532_bus0_pll,
	.state_count = ARRAY_SIZE(aclk_ccore_532_bus0_pll),
};

struct devfreq_clk_state aclk_ccore_532_bus1_pll[] = {
	{MOUT_ACLK_CCORE_532,	BUS1_PLL},
};

struct devfreq_clk_states aclk_ccore_532_bus1_pll_list = {
	.state = aclk_ccore_532_bus1_pll,
	.state_count = ARRAY_SIZE(aclk_ccore_532_bus1_pll),
};

struct devfreq_clk_state aclk_ccore_532_cci_pll[] = {
	{MOUT_ACLK_CCORE_532,	CCI_PLL},
};

struct devfreq_clk_states aclk_ccore_532_cci_pll_list = {
	.state = aclk_ccore_532_cci_pll,
	.state_count = ARRAY_SIZE(aclk_ccore_532_cci_pll),
};

struct devfreq_clk_state aclk_ccore_532_mfc_pll[] = {
	{MOUT_ACLK_CCORE_532,	MFC_PLL},
};

struct devfreq_clk_states aclk_ccore_532_mfc_pll_list = {
	.state = aclk_ccore_532_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_ccore_532_mfc_pll),
};

struct devfreq_clk_state aclk_ccore_532_bus0_pll_cmuc_div2[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_MOUT_BUS0_PLL_CMUC_DIV2},
};

struct devfreq_clk_states aclk_ccore_532_bus0_pll_cmuc_div2_list = {
	.state = aclk_ccore_532_bus0_pll_cmuc_div2,
	.state_count = ARRAY_SIZE(aclk_ccore_532_bus0_pll_cmuc_div2),
};

struct devfreq_clk_state aclk_ccore_532_bus1_pll_div2[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_TOPC_BUS1_PLL_DIV2},
};

struct devfreq_clk_states aclk_ccore_532_bus1_pll_div2_list = {
	.state = aclk_ccore_532_bus1_pll_div2,
	.state_count = ARRAY_SIZE(aclk_ccore_532_bus1_pll_div2),
};

struct devfreq_clk_state aclk_ccore_532_cci_pll_div2[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_TOPC_CCI_PLL_DIV2},
};

struct devfreq_clk_states aclk_ccore_532_cci_pll_div2_list = {
	.state = aclk_ccore_532_cci_pll_div2,
	.state_count = ARRAY_SIZE(aclk_ccore_532_cci_pll_div2),
};

struct devfreq_clk_state aclk_ccore_532_mfc_pll_div2[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_TOPC_MFC_PLL_DIV2},
};

struct devfreq_clk_states aclk_ccore_532_mfc_pll_div2_list = {
	.state = aclk_ccore_532_mfc_pll_div2,
	.state_count = ARRAY_SIZE(aclk_ccore_532_mfc_pll_div2),
};

struct devfreq_clk_state aclk_ccore_532_bus0_pll_cmuc_div4[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_MOUT_BUS0_PLL_CMUC_DIV4},
};

struct devfreq_clk_states aclk_ccore_532_bus0_pll_cmuc_div4_list = {
	.state = aclk_ccore_532_bus0_pll_cmuc_div4,
	.state_count = ARRAY_SIZE(aclk_ccore_532_bus0_pll_cmuc_div4),
};

struct devfreq_clk_state aclk_ccore_532_bus1_pll_div4[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_TOPC_BUS1_PLL_DIV4},
};

struct devfreq_clk_states aclk_ccore_532_bus1_pll_div4_list = {
	.state = aclk_ccore_532_bus1_pll_div4,
	.state_count = ARRAY_SIZE(aclk_ccore_532_bus1_pll_div4),
};

struct devfreq_clk_state aclk_ccore_532_cci_pll_div4[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_TOPC_CCI_PLL_DIV4},
};

struct devfreq_clk_states aclk_ccore_532_cci_pll_div4_list = {
	.state = aclk_ccore_532_cci_pll_div4,
	.state_count = ARRAY_SIZE(aclk_ccore_532_cci_pll_div4),
};

struct devfreq_clk_state aclk_ccore_532_mfc_pll_div4[] = {
	{MOUT_ACLK_CCORE_532,	FFAC_TOPC_MFC_PLL_DIV4},
};

struct devfreq_clk_states aclk_ccore_532_mfc_pll_div4_list = {
	.state = aclk_ccore_532_mfc_pll_div4,
	.state_count = ARRAY_SIZE(aclk_ccore_532_mfc_pll_div4),
};

struct devfreq_clk_state sclk_bus0_pll_mif_bus0_pll[] = {
	{MOUT_SCLK_BUS0_PLL_MIF,	BUS0_PLL},
};

struct devfreq_clk_states sclk_bus0_pll_mif_bus0_pll_list= {
	.state = sclk_bus0_pll_mif_bus0_pll,
	.state_count = ARRAY_SIZE(sclk_bus0_pll_mif_bus0_pll),
};

struct devfreq_clk_state sclk_bus0_pll_mif_bus0_pll_div2[] = {
	{MOUT_SCLK_BUS0_PLL_MIF,	FFAC_TOPC_BUS0_PLL_DIV2},
};

struct devfreq_clk_states sclk_bus0_pll_mif_bus0_pll_div2_list= {
	.state = sclk_bus0_pll_mif_bus0_pll_div2,
	.state_count = ARRAY_SIZE(sclk_bus0_pll_mif_bus0_pll_div2),
};

struct devfreq_clk_state sclk_bus0_pll_mif_bus1_pll[] = {
	{MOUT_SCLK_BUS0_PLL_MIF,	BUS1_PLL},
};

struct devfreq_clk_states sclk_bus0_pll_mif_bus1_pll_list= {
	.state = sclk_bus0_pll_mif_bus1_pll,
	.state_count = ARRAY_SIZE(sclk_bus0_pll_mif_bus1_pll),
};

struct devfreq_clk_state sclk_bus0_pll_mif_cci_pll[] = {
	{MOUT_SCLK_BUS0_PLL_MIF,	CCI_PLL},
};

struct devfreq_clk_states sclk_bus0_pll_mif_cci_pll_list= {
	.state = sclk_bus0_pll_mif_cci_pll,
	.state_count = ARRAY_SIZE(sclk_bus0_pll_mif_cci_pll),
};

struct devfreq_clk_info aclk_ccore_532[] = {
	{MIF_LV0,	532 * MHZ,	0,      &aclk_ccore_532_cci_pll_list},
	{MIF_LV1,	532 * MHZ,	0,      &aclk_ccore_532_cci_pll_list},
	{MIF_LV2,	468 * MHZ,	0,      &aclk_ccore_532_mfc_pll_list},
	{MIF_LV3,	400 * MHZ,	0,      &aclk_ccore_532_bus0_pll_list},
	{MIF_LV4,	334 * MHZ,	0,      &aclk_ccore_532_bus1_pll_div2_list},
	{MIF_LV5,	334 * MHZ,	0,      &aclk_ccore_532_bus1_pll_div2_list},
	{MIF_LV6,	266 * MHZ,	0,      &aclk_ccore_532_cci_pll_div2_list},
	{MIF_LV7,	200 * MHZ,	0,      &aclk_ccore_532_bus0_pll_cmuc_div2_list},
	{MIF_LV8,	167 * MHZ,	0,      &aclk_ccore_532_bus1_pll_div4_list},
	{MIF_LV9,	167 * MHZ,	0,      &aclk_ccore_532_bus1_pll_div4_list},
	{MIF_LV10,	133 * MHZ,	0,      &aclk_ccore_532_cci_pll_div4_list},
	{MIF_LV11,	100 * MHZ,	0,      &aclk_ccore_532_bus0_pll_cmuc_div4_list},
	{MIF_LV12,	100 * MHZ,	0,      &aclk_ccore_532_bus0_pll_cmuc_div4_list},
};

struct devfreq_clk_info aclk_ccore_133[] = {
	{MIF_LV0,	80 * MHZ,	0,      NULL},
	{MIF_LV1,	80 * MHZ,	0,      NULL},
	{MIF_LV2,	80 * MHZ,	0,      NULL},
	{MIF_LV3,	80 * MHZ,	0,      NULL},
	{MIF_LV4,	80 * MHZ,	0,      NULL},
	{MIF_LV5,	67 * MHZ,	0,      NULL},
	{MIF_LV6,	50 * MHZ,	0,      NULL},
	{MIF_LV7,	50 * MHZ,	0,      NULL},
	{MIF_LV8,	50 * MHZ,	0,      NULL},
	{MIF_LV9,	50 * MHZ,	0,      NULL},
	{MIF_LV10,	50 * MHZ,	0,      NULL},
	{MIF_LV11,	50 * MHZ,	0,      NULL},
	{MIF_LV12,	50 * MHZ,	0,      NULL},
};

struct devfreq_clk_info *devfreq_clk_ccore_info_list[] = {
	aclk_ccore_532,
	aclk_ccore_133,
};

enum devfreq_mif_clk devfreq_clk_ccore_info_idx[] = {
	DOUT_ACLK_CCORE_532,
	DOUT_ACLK_CCORE_133,
};

struct dmc_drex_dfs_mif_table
{
	uint32_t drex_TimingRfcPb;
	uint32_t drex_TimingRow;
	uint32_t drex_TimingData;
	uint32_t drex_TimingPower;
	uint32_t drex_RdFetch;
	uint32_t drex_EtcControl;
	uint32_t drex_Train_Timing;
	uint32_t drex_Hw_Ptrain_Period;
	uint32_t drex_Hwpr_Train_Config;
	uint32_t drex_Hwpr_Train_Control;
	uint32_t drex_DirectCmd_mr13;
	uint32_t drex_DirectCmd_mr2;
	uint32_t drex_DirectCmd_mr22;
	uint32_t drex_DirectCmd_mr11;
	uint32_t drex_DirectCmd_mr14;
	uint32_t vtmon_Drex_Timing_Set_Sw;
};

struct dmc_phy_dfs_mif_table {
	uint32_t phy_Dvfs_Con0_set1;
	uint32_t phy_Dvfs_Con0_set0;
	uint32_t phy_Dvfs_Con0_set1_mask;
	uint32_t phy_Dvfs_Con0_set0_mask;
	uint32_t phy_Dvfs_Con1_set1;
	uint32_t phy_Dvfs_Con1_set0;
	uint32_t phy_Dvfs_Con1_set1_mask;
	uint32_t phy_Dvfs_Con1_set0_mask;
	uint32_t phy_Dvfs_Con2_set1;
	uint32_t phy_Dvfs_Con2_set0;
	uint32_t phy_Dvfs_Con2_set1_mask;
	uint32_t phy_Dvfs_Con2_set0_mask;
	uint32_t phy_Dvfs_Con3_set1;
	uint32_t phy_Dvfs_Con3_set0;
	uint32_t phy_Dvfs_Con3_set1_mask;
	uint32_t phy_Dvfs_Con3_set0_mask;
	uint32_t phy_Dvfs_Con4_set1;
	uint32_t phy_Dvfs_Con4_set0;
	uint32_t phy_Dvfs_Con4_set1_mask;
	uint32_t phy_Dvfs_Con4_set0_mask;
	uint32_t phy_Dvfs_Con5_set1;
	uint32_t phy_Dvfs_Con5_set0;
	uint32_t phy_Dvfs_Con5_set1_mask;
	uint32_t phy_Dvfs_Con5_set0_mask;
};

struct dmc_vtmon_dfs_mif_table
{
	uint32_t vtmon_Cnt_Limit_set0;
	uint32_t vtmon_Cnt_Limit_set1;
};

#define DREX_TIMING_PARA(rfcpb, row, data, power, fetch, etc_con, train_timing, hw_ptrain_period, \
		hwpr_train_config, hwpr_train_control, mr13, mr2, mr22, mr11, mr14, vtmon_drex_timing_set_sw) \
{ \
	.drex_TimingRfcPb	= rfcpb, \
	.drex_TimingRow		= row, \
	.drex_TimingData	= data, \
	.drex_TimingPower	= power, \
	.drex_RdFetch		= fetch, \
	.drex_EtcControl	= etc_con, \
	.drex_Train_Timing	= train_timing, \
	.drex_Hw_Ptrain_Period	= hw_ptrain_period, \
	.drex_Hwpr_Train_Config	= hwpr_train_config, \
	.drex_Hwpr_Train_Control= hwpr_train_control, \
	.drex_DirectCmd_mr13	= mr13, \
	.drex_DirectCmd_mr2	= mr2, \
	.drex_DirectCmd_mr22	= mr22, \
	.drex_DirectCmd_mr11	= mr11, \
	.drex_DirectCmd_mr14	= mr14, \
	.vtmon_Drex_Timing_Set_Sw = vtmon_drex_timing_set_sw, \
}

#define PHY_DVFS_CON(con0_set1, con0_set0, con0_set1_mask, con0_set0_mask, con1_set1, con1_set0, con1_set1_mask, con1_set0_mask, \
		con2_set1, con2_set0, con2_set1_mask, con2_set0_mask, con3_set1, con3_set0, con3_set1_mask, con3_set0_mask, \
		con4_set1, con4_set0, con4_set1_mask, con4_set0_mask, con5_set1, con5_set0, con5_set1_mask, con5_set0_mask) \
{ \
	.phy_Dvfs_Con0_set1		= con0_set1, \
	.phy_Dvfs_Con0_set0		= con0_set0, \
	.phy_Dvfs_Con0_set1_mask	= con0_set1_mask, \
	.phy_Dvfs_Con0_set0_mask	= con0_set0_mask, \
	.phy_Dvfs_Con1_set1		= con1_set1, \
	.phy_Dvfs_Con1_set0		= con1_set0, \
	.phy_Dvfs_Con1_set1_mask	= con1_set1_mask, \
	.phy_Dvfs_Con1_set0_mask	= con1_set0_mask, \
	.phy_Dvfs_Con2_set1		= con2_set1, \
	.phy_Dvfs_Con2_set0		= con2_set0, \
	.phy_Dvfs_Con2_set1_mask	= con2_set1_mask, \
	.phy_Dvfs_Con2_set0_mask	= con2_set0_mask, \
	.phy_Dvfs_Con3_set1		= con3_set1, \
	.phy_Dvfs_Con3_set0		= con3_set0, \
	.phy_Dvfs_Con3_set1_mask	= con3_set1_mask, \
	.phy_Dvfs_Con3_set0_mask	= con3_set0_mask, \
	.phy_Dvfs_Con4_set1		= con4_set1, \
	.phy_Dvfs_Con4_set0		= con4_set0, \
	.phy_Dvfs_Con4_set1_mask	= con4_set1_mask, \
	.phy_Dvfs_Con4_set0_mask	= con4_set0_mask, \
	.phy_Dvfs_Con5_set1		= con5_set1, \
	.phy_Dvfs_Con5_set0		= con5_set0, \
	.phy_Dvfs_Con5_set1_mask	= con5_set1_mask, \
	.phy_Dvfs_Con5_set0_mask	= con5_set0_mask, \
}

#define LP4_RL 12 //16 //12
#define LP4_WL 6 //8 //6
#define LP4_RL_L10 6 //16 //12
#define LP4_WL_L10 4 //8 //6
#define DRAM_RLWL 0x09
#define DRAM_RLWL_L10 0x00

#define MIF_L4_DVFS_OFFSET	0x20
#define MIF_L5_DVFS_OFFSET	0x20
#define MIF_L6_DVFS_OFFSET	0x20
#define MIF_L7_DVFS_OFFSET	0x20
#define MIF_L8_DVFS_OFFSET	0x20
#define MIF_L9_DVFS_OFFSET	0x20
#define MIF_L10_DVFS_OFFSET	0x20
#define MIF_L11_DVFS_OFFSET	0x20
#define MIF_L12_DVFS_OFFSET	0x20

#define DQS_OSC_UPDATE_EN 0
#define PERIODIC_WR_TRAIN 0

#if DQS_OSC_UPDATE_EN
#define dvfs_dqs_osc_en 1
#else
#define dvfs_dqs_osc_en 0
#endif

#if PERIODIC_WR_TRAIN
#define dvfs_offset 16
#else
#define dvfs_offset 0
#endif

/* DRAM latency adjust option for power saving */
#define L6_RL_WL_DECREASE
#define L9_RL_WL_DECREASE

// PHY DVFS CON SFR BIT DEFINITION
#define PHY_DVFS_CON0_SET1_MASK	(0x0)|(1<<31)|(1<<29)|(0x3<<24)|(1<<23)|(1<<21)|(1<<19)|(1<<17)|(0xFF<<8)
#define PHY_DVFS_CON0_SET0_MASK	(0x0)|(1<<30)|(1<<28)|(0x3<<24)|(1<<22)|(1<<20)|(1<<18)|(1<<16)|(0xFF<<0)
#define PHY_DVFS_CON0_DVFS_MODE_MASK (0x0)|(0x3<<24)
#define PHY_DVFS_CON0_DVFS_MODE_POSITION	24

#define PHY_DVFS_CON1_SET1_MASK	(0x0)|(1<<31)|(0x3F<<24)|(0xF<<12)|(0xF<<8)
#define PHY_DVFS_CON1_SET0_MASK	(0x0)|(1<<30)|(0x3F<<16)|(0xF<<4)|(0xF<<0)

#define PHY_DVFS_CON2_SET1_MASK	(0x0)|(0x7F<<25) //|(0xFFF<<0)
#define PHY_DVFS_CON2_SET0_MASK	(0x0)|(0x7F<<18) //|(0xFFF<<0)
#define PHY_DVFS_CON2_FREQ_TRAIN_MASK (0x0)|(0xFFF<<0)
#define PHY_DVFS_CON2_FREQ_TRAIN_POSITION  0

#define PHY_DVFS_CON3_SET1_MASK	(0x0)|(0x1<<31)|(0x1<<29)|(0x1<<27)|(0xFFF<<12)
#define PHY_DVFS_CON3_SET0_MASK	(0x0)|(0x1<<30)|(0x1<<28)|(0x1<<26)|(0xFFF<<0)

#define PHY_DVFS_CON4_SET1_MASK	(0x0)|(0x3F<<18)|(0x3F<<12)
#define PHY_DVFS_CON4_SET0_MASK	(0x0)|(0x3F<<6)|(0x3F<<0)
#define PHY_DVFS_CON4_DVFS_FSBST_EN_MASK	(0x0)|(0x1<<31)
#define PHY_DVFS_CON4_DVFS_FSBST_EN_POSITION	31

#define PHY_DVFS_CON5_SET1_MASK	(0x0)|(1<<31)|(0xF<<20)|(0xF<<12)|(0xF<<4)
#define PHY_DVFS_CON5_SET0_MASK	(0x0)|(1<<30)|(0xF<<16)|(0xF<<8)|(0xF<<0)

#if defined(DMC_DQS_MODE2)
static struct dmc_drex_dfs_mif_table dfs_drex_mif_table[] =
{
	DREX_TIMING_PARA(0x00002323, 0x46588652, 0x4732BBE0, 0x404A0336, 0x00004E02, 0x00000000, 0x319BA14A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x2D<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00002121, 0x42588611, 0x4732BBE0, 0x3C460336, 0x00004902, 0x00000000, 0x319BA14A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x2D<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001D1D, 0x3947754F, 0x4632B3DC, 0x343D0335, 0x00004002, 0x00000000, 0x319BA13A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x24<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001919, 0x3146648C, 0x3532B39C, 0x2C340335, 0x00003602, 0x00000000, 0x319BA13A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x24<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001313, 0x2635538A, 0x3422AAD6, 0x24280234, 0x00002A02, 0x00000000, 0x219BA12A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x1B<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000F0F, 0x1D2442C8, 0x2322A310, 0x1C1F0233, 0x00002003, 0x00000000, 0x019BA10B, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x12<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#ifdef L6_RL_WL_DECREASE
	DREX_TIMING_PARA(0x00000D0D, 0x19233247, 0x23229ACC, 0x181F0233, 0x00001C02, 0x00000000, 0x019BA10A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#else
	DREX_TIMING_PARA(0x00000D0D, 0x19233247, 0x2322A2D0, 0x181F0233, 0x00001C02, 0x00000000, 0x019BA10A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x12<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#endif
	DREX_TIMING_PARA(0x00000A0A, 0x192331C5, 0x22229ACC, 0x141F0233, 0x00001502, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x24<<2), 0x00210c00|(0x00<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000808, 0x19232185, 0x22229ACC, 0x101F0233, 0x00001202, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x24<<2), 0x00210c00|(0x00<<2), 0x00211800|(0x0F<<2), 0x00000000),
#ifdef L9_RL_WL_DECREASE
	DREX_TIMING_PARA(0x00000707, 0x19232144, 0x22229286, 0x0C1F0233, 0x00000E02, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x24<<2), 0x00210c00|(0x00<<2), 0x00211800|(0x0F<<2), 0x00000000),
#else
	DREX_TIMING_PARA(0x00000707, 0x19232144, 0x22229A8C, 0x0C1F0233, 0x00000E02, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x24<<2), 0x00210c00|(0x00<<2), 0x00211800|(0x0F<<2), 0x00000000),
#endif
	DREX_TIMING_PARA(0x00000404, 0x192320C3, 0x22229286, 0x081F0233, 0x00000902, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x24<<2), 0x00210c00|(0x00<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000303, 0x192320C2, 0x22229286, 0x081F0233, 0x00000702, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x24<<2), 0x00210c00|(0x00<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000303, 0x192320C2, 0x22229286, 0x081F0233, 0x00000502, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x24<<2), 0x00210c00|(0x00<<2), 0x00211800|(0x0F<<2), 0x00000000),
};

static struct dmc_phy_dfs_mif_table dfs_phy_mif_table[] =
{
	PHY_DVFS_CON(0x80200000|(dvfs_dqs_osc_en<<17), 0x40100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x2000B000, 0x002000B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0860E000, 0x0400060E, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x2000B000, 0x002000B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x085B0000, 0x040005B0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x1C00B000, 0x001C00B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x084F0000, 0x040004F0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x1C00B000, 0x001C00B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0842C000, 0x0400042C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x16004000, 0x00160040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0033C000, 0x0000033C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x10004000, 0x00100040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00278000, 0x00000278, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#ifdef L6_RL_WL_DECREASE
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0021F000, 0x0000021F, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#else
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x10004000, 0x00100040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0021F000, 0x0000021F, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#endif
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17), 0x41500000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x400C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x001A0000, 0x000001A0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x0005010F, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82A80000|(dvfs_dqs_osc_en<<17), 0x41540000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x8C004000, 0x400C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0015C000, 0x0000015C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x005010F0, 0x0005010F, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#ifdef L9_RL_WL_DECREASE
	PHY_DVFS_CON(0x82A80000|(dvfs_dqs_osc_en<<17), 0x41540000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x86004000, 0x40060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00114000, 0x00000114, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x005010F0, 0x0005010F, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#else
	PHY_DVFS_CON(0x82A80000|(dvfs_dqs_osc_en<<17), 0x41540000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x8C004000, 0x400C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00114000, 0x00000114, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x005010F0, 0x0005010F, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#endif
	PHY_DVFS_CON(0x82803000|(dvfs_dqs_osc_en<<17), 0x41400030|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x86004000, 0x40060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x000A7000, 0x000000A7, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x005010F0, 0x0005010F, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82803000|(dvfs_dqs_osc_en<<17), 0x41400030|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x86004000, 0x40060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00085000, 0x00000085, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x005010F0, 0x0005010F, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82803000|(dvfs_dqs_osc_en<<17), 0x41400030|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x86004000, 0x40060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00064000, 0x00000064, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x005010F0, 0x0005010F, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
};
#elif defined(DMC_DQS_MODE1)
static struct dmc_drex_dfs_mif_table dfs_drex_mif_table[] =
{
	DREX_TIMING_PARA(0x00002323, 0x46588652, 0x4732BBE0, 0x404A0336, 0x00004E02, 0x00000000, 0x319BA14A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x2D<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00002121, 0x42588611, 0x4732BBE0, 0x3C460336, 0x00004902, 0x00000000, 0x319BA14A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x2D<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001D1D, 0x3947754F, 0x4632B3DC, 0x343D0335, 0x00004002, 0x00000000, 0x319BA13A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x24<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001919, 0x3146648C, 0x3532B39C, 0x2C340335, 0x00003602, 0x00000000, 0x319BA13A, 0x00005DC0, 0x01010101, 0x00000003,
			0x00211400, 0x00200800|(0x24<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001313, 0x2635538A, 0x3422AAD6, 0x24280234, 0x00002A02, 0x00000000, 0x219BA12A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x1B<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000F0F, 0x1D2442C8, 0x2322A310, 0x1C1F0233, 0x00002003, 0x00000000, 0x019BA10B, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x12<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#ifdef L6_RL_WL_DECREASE
	DREX_TIMING_PARA(0x00000D0D, 0x19233247, 0x23229ACC, 0x181F0233, 0x00001C02, 0x00000000, 0x019BA10A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#else
	DREX_TIMING_PARA(0x00000D0D, 0x19233247, 0x2322A2D0, 0x181F0233, 0x00001C02, 0x00000000, 0x019BA10A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x12<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#endif
	DREX_TIMING_PARA(0x00000A0A, 0x192331C5, 0x22229ACC, 0x141F0233, 0x00001502, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000808, 0x19232185, 0x22229ACC, 0x101F0233, 0x00001202, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#ifdef L9_RL_WL_DECREASE
	DREX_TIMING_PARA(0x00000707, 0x19232144, 0x22229286, 0x0C1F0233, 0x00000E02, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#else
	DREX_TIMING_PARA(0x00000707, 0x19232144, 0x22229A8C, 0x0C1F0233, 0x00000E02, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#endif
	DREX_TIMING_PARA(0x00000404, 0x192320C3, 0x22229286, 0x081F0233, 0x00000902, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000303, 0x192320C2, 0x22229286, 0x081F0233, 0x00000702, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000303, 0x192320C2, 0x22229286, 0x081F0233, 0x00000502, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00221800 | (0x26<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
};

static struct dmc_phy_dfs_mif_table dfs_phy_mif_table[] =
{
	PHY_DVFS_CON(0x80200000|(dvfs_dqs_osc_en<<17), 0x40100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x2000B000, 0x002000B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0860E000, 0x0400060E, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x2000B000, 0x002000B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x085B0000, 0x040005B0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x1C00B000, 0x001C00B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x084F0000, 0x040004F0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x1C00B000, 0x001C00B0, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0842C000, 0x0400042C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x80700000, 0x40070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x16004000, 0x00160040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0033C000, 0x0000033C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x10004000, 0x00100040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00278000, 0x00000278, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#ifdef L6_RL_WL_DECREASE
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0021F000, 0x0000021F, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#else
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x10004000, 0x00100040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0021F000, 0x0000021F, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#endif
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x001A0000, 0x000001A0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0015C000, 0x0000015C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#ifdef L9_RL_WL_DECREASE
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00114000, 0x00000114, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#else
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00114000, 0x00000114, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#endif
	PHY_DVFS_CON(0x82003000|(dvfs_dqs_osc_en<<17), 0x41000030|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x000A7000, 0x000000A7, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82003000|(dvfs_dqs_osc_en<<17), 0x41000030|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00085000, 0x00000085, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82003000|(dvfs_dqs_osc_en<<17), 0x41000030|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00064000, 0x00000064, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
};
#elif defined(DMC_DQS_MODE0)
static struct dmc_drex_dfs_mif_table dfs_drex_mif_table[] =
{
	DREX_TIMING_PARA(0x00002323, 0x46588652, 0x4732BBE0, 0x404A0336, 0x00004E02, 0x00000000, 0x319BA14A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x2D<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00002121, 0x42588611, 0x4732BBE0, 0x3C460336, 0x00004902, 0x00000000, 0x319BA14A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x2D<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001D1D, 0x3947754F, 0x4632B3DC, 0x343D0335, 0x00004002, 0x00000000, 0x319BA13A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x24<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001919, 0x3146648C, 0x3532B39C, 0x2C340335, 0x00003602, 0x00000000, 0x319BA13A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x24<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00001313, 0x2635538A, 0x3422AAD6, 0x24280234, 0x00002A02, 0x00000000, 0x219BA12A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x1B<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000F0F, 0x1D2442C8, 0x2322A310, 0x1C1F0233, 0x00002003, 0x00000000, 0x019BA10B, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x12<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000D0D, 0x19233247, 0x2322A2D0, 0x181F0233, 0x00001C02, 0x00000000, 0x019BA10A, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x12<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000A0A, 0x192331C5, 0x22229ACC, 0x141F0233, 0x00001502, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000808, 0x19232185, 0x22229ACC, 0x101F0233, 0x00001202, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#ifdef L9_RL_WL_DECREASE
	DREX_TIMING_PARA(0x00000707, 0x19232144, 0x22229286, 0x0C1F0233, 0x00000E02, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#else
	DREX_TIMING_PARA(0x00000707, 0x19232144, 0x22229A8C, 0x0C1F0233, 0x00000E02, 0x00000000, 0x019BA0FA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x09<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
#endif
	DREX_TIMING_PARA(0x00000404, 0x192320C3, 0x22229286, 0x081F0233, 0x00000902, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000303, 0x192320C2, 0x22229286, 0x081F0233, 0x00000702, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
	DREX_TIMING_PARA(0x00000303, 0x192320C2, 0x22229286, 0x081F0233, 0x00000502, 0x00000000, 0x019BA0EA, 0x00FFFFFF, 0x00000000, 0x00000000,
			0x00211400, 0x00200800|(0x00<<2), 0x00210c00|(0x04<<2), 0x00211800|(0x0F<<2), 0x00000000),
};

static struct dmc_phy_dfs_mif_table dfs_phy_mif_table[] =
{
	PHY_DVFS_CON(0x80200000|(dvfs_dqs_osc_en<<17), 0x40100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x20004000, 0x00200040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0060E000, 0x0000060E, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00700000, 0x00070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x20004000, 0x00200040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x005B0000, 0x000005B0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00700000, 0x00070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x1C004000, 0x001C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x004F0000, 0x000004F0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00700000, 0x00070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x1C004000, 0x001C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0042C000, 0x0000042C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00700000, 0x00070000, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x16004000, 0x00160040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0033C000, 0x0000033C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x10004000, 0x00100040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00278000, 0x00000278, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17),	0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x10004000, 0x00100040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0021F000, 0x0000021F, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82200000|(dvfs_offset<<8)|(dvfs_dqs_osc_en<<17), 0x41100000|(dvfs_offset<<0)|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x001A0000, 0x000001A0, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82001000|(dvfs_dqs_osc_en<<17), 0x41000010|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x0015C000, 0x0000015C, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#ifdef L9_RL_WL_DECREASE
	PHY_DVFS_CON(0x82001000|(dvfs_dqs_osc_en<<17), 0x41000010|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00114000, 0x00000114, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#else
	PHY_DVFS_CON(0x82001000|(dvfs_dqs_osc_en<<17), 0x41000010|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0C004000, 0x000C0040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00114000, 0x00000114, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
#endif
	PHY_DVFS_CON(0x82001000|(dvfs_dqs_osc_en<<17), 0x41000010|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x000A7000, 0x000000A7, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82001000|(dvfs_dqs_osc_en<<17), 0x41000010|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00085000, 0x00000085, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
	PHY_DVFS_CON(0x82001000|(dvfs_dqs_osc_en<<17), 0x41000010|(dvfs_dqs_osc_en<<16), PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x06004000, 0x00060040, PHY_DVFS_CON1_SET1_MASK, PHY_DVFS_CON1_SET0_MASK,
			0x20000000, 0x00400000, PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x00064000, 0x00000064, PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK,
			0x00000000, 0x00000000, PHY_DVFS_CON4_SET1_MASK, PHY_DVFS_CON4_SET0_MASK,
			0x00501000, 0x00050100, PHY_DVFS_CON5_SET1_MASK, PHY_DVFS_CON5_SET0_MASK),
};
#else
#endif

static struct dmc_vtmon_dfs_mif_table dfs_vtmon_mif_table[] =
{
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
	{0x00480060, 0x000003e8},
};

enum devfreq_mif_thermal_autorate {
	RATE_TWO = 0x001800BA,
	RATE_ONE = 0x000C005D,
	RATE_HALF = 0x0006002E,
	RATE_QUARTER = 0x00030017,
};

enum devfreq_mif_thermal_channel {
	THERMAL_CHANNEL0,
	THERMAL_CHANNEL1,
	THERMAL_CHANNEL2,
	THERMAL_CHANNEL3,
};

static struct workqueue_struct *devfreq_mif_thermal_wq;
struct devfreq_thermal_work devfreq_mif_thermal_work = {
	.polling_period = 500,
};

#define PAUSE		0x1008
#define PAUSE_MIF	0x1000

static void exynos7_devfreq_waiting_pause(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;
	int i;

	while ((__raw_readl(data->base_cmu_topc + PAUSE) & (0x07 << 16)) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait pause completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}

	for (i = 0; i<MIF_BLK_NUM; i++)
	{
		timeout = 1000;
		while ((__raw_readl(data->base_cmu_mif[i] + PAUSE_MIF) & (0x07 << 16)) != 0) {
			if (timeout == 0) {
				pr_err("DEVFREQ(MIF) : timeout to wait pause completion\n");
				return;
			}
			udelay(1);
			timeout--;
		}
	}
}

#define MIF_MUX_CTRL_BY_CMUC  BIT(20)

static void exynos7_devfreq_check_mif_mux_ctrl_by_cmuc(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;
	int i;

	for (i = 0; i<MIF_BLK_NUM; i++)
	{
		timeout = 1000;
		while ((__raw_readl(data->base_cmu_mif[i] + PAUSE_MIF) & MIF_MUX_CTRL_BY_CMUC) == 0) {
			if (timeout == 0) {
				pr_err("DEVFREQ(MIF) : timeout to wait mif_mux_ctrl_by_cmuc completion\n");
				return;
			}
			udelay(1);
			timeout--;
		}
	}
}

static void exynos7_devfreq_waiting_mux(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	/* check whether ACLK_MIF_PLL_CLK_STAT0/1/2/3 is 0 */
	while ((__raw_readl(data->base_cmu_topc + 0x0414) & (0x0f << 24)) != 0) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait mux completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

#define DREX_TimingRfcPb		0x20
#define DREX_TimingRow_0		0x34
#define DREX_TimingData_0		0x38
#define DREX_TimingPower_0		0x3C
#define DREX_RdFetch_0			0x4C
#define DREX_EtcControl_0		0x58
#define DREX_Train_Timing_0		0x410
#define DREX_Hw_Ptrain_Period_0		0x420
#define DREX_Hwpr_Train_Config_0	0x440
#define DREX_Hwpr_Train_Control_0	0x450
#define DREX_TimingRow_1		0xE4
#define DREX_TimingData_1		0xE8
#define DREX_TimingPower_1		0xEC
#define DREX_RdFetch_1			0x50
#define DREX_EtcControl_1		0x5C
#define DREX_Train_Timing_1		0x414
#define DREX_Hw_Ptrain_Period_1		0x424
#define DREX_Hwpr_Train_Config_1	0x444
#define DREX_Hwpr_Train_Control_1	0x454
#define DREX_PhyStatus			0x40
#define DREX_DIRECTCMD			0x10

#define PHY_ZQ_CON9			0x3EC
#define PHY_DVFS_CON0			0xB8
#define PHY_DVFS_CON1			0xBC
#define PHY_DVFS_CON2			0xC0
#define PHY_DVFS_CON3			0xC4
#define PHY_DVFS_CON4			0xC8
#define PHY_DVFS_CON5			0xCC

#define VTMON_DREX_TIMING_SW_SET	0x3C
#define VTMON_MIF_DREX_WDATA0		0x1C
#define VTMON_CNT_LIMIT_SET0_SW0	0x80
#define VTMON_CNT_LIMIT_SET1_SW0	0x84
#define VTMON_CNT_LIMIT_SET0_SW1	0x88
#define VTMON_CNT_LIMIT_SET1_SW1	0x8C

#define DREX_TimingRfcPb_Set1_MASK	0x0000FF00
#define DREX_TimingRfcPb_Set0_MASK	0x000000FF

#define NUM_OF_SLICE                    2
static int exynos7_devfreq_mif_preset(struct devfreq_data_mif *data,
							int target_idx, int timing_set_num)
{
	struct dmc_drex_dfs_mif_table *cur_drex_param;
	struct dmc_phy_dfs_mif_table *cur_phy_param;
	struct dmc_vtmon_dfs_mif_table *cur_vtmon_param;
	uint32_t tmp;
	int i;
        uint32_t soc_vref[MIF_BLK_NUM][NUM_OF_SLICE];

	cur_drex_param = &dfs_drex_mif_table[target_idx];
	cur_phy_param = &dfs_phy_mif_table[target_idx];
	cur_vtmon_param = &dfs_vtmon_mif_table[target_idx];

        for (i = 0; i< MIF_BLK_NUM; i++) {
            soc_vref[i][1] = (__raw_readl(data->base_lp4_phy[i] + PHY_ZQ_CON9) & 0x3f00) >> 8;
            soc_vref[i][0] = (__raw_readl(data->base_lp4_phy[i] + PHY_ZQ_CON9) & 0x003f) >> 0;
        }

	pr_debug(" ===> pre-setting mif_timing_set_#%d\n", timing_set_num);

	if (timing_set_num) {
		for (i = 0; i< MIF_BLK_NUM; i++) {
			/* DREX */
			tmp = __raw_readl(data->base_drex[i] + DREX_TimingRfcPb);
			tmp &= ~(DREX_TimingRfcPb_Set1_MASK);
			tmp |= (cur_drex_param->drex_TimingRfcPb & DREX_TimingRfcPb_Set1_MASK);
			__raw_writel(tmp, data->base_drex[i] + DREX_TimingRfcPb);
			__raw_writel(cur_drex_param->drex_TimingRow, data->base_drex[i] + DREX_TimingRow_1);
			__raw_writel(cur_drex_param->drex_TimingData,data-> base_drex[i] + DREX_TimingData_1);
			__raw_writel(cur_drex_param->drex_TimingPower, data->base_drex[i] + DREX_TimingPower_1);
			__raw_writel(cur_drex_param->drex_RdFetch, data->base_drex[i] + DREX_RdFetch_1);
			__raw_writel((0x1<<7)|cur_drex_param->drex_EtcControl, data->base_drex[i] + DREX_EtcControl_1);
			__raw_writel(cur_drex_param->drex_Train_Timing, data->base_drex[i] + DREX_Train_Timing_1);
			__raw_writel(cur_drex_param->drex_Hw_Ptrain_Period, data->base_drex[i] + DREX_Hw_Ptrain_Period_1);
			__raw_writel(cur_drex_param->drex_Hwpr_Train_Config, data->base_drex[i] + DREX_Hwpr_Train_Config_1);
			__raw_writel(cur_drex_param->drex_Hwpr_Train_Control, data->base_drex[i] + DREX_Hwpr_Train_Control_1);

			/* LP4_PHY */
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON0);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con0_set1_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con0_set1;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON0);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON1);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con1_set1_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con1_set1;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON1);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON2);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con2_set1_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con2_set1;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON2);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON3);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con3_set1_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con3_set1;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON3);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON4);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con4_set1_mask);
			tmp |= (cur_phy_param->phy_Dvfs_Con4_set1 | (soc_vref[i][1] << 18)| (soc_vref[i][0] << 12));
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON4);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON5);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con5_set1_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con5_set1;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON5);

			__raw_writel(cur_vtmon_param->vtmon_Cnt_Limit_set0, data->base_vt_mon_mif[i] + VTMON_CNT_LIMIT_SET0_SW1);
			__raw_writel(cur_vtmon_param->vtmon_Cnt_Limit_set1, data->base_vt_mon_mif[i] + VTMON_CNT_LIMIT_SET1_SW1);

			/* DIRECTCMD */
			__raw_writel((0x40<<2)|cur_drex_param->drex_DirectCmd_mr13, data->base_drex[i] + DREX_DIRECTCMD);
			__raw_writel(cur_drex_param->drex_DirectCmd_mr2, data->base_drex[i] + DREX_DIRECTCMD);
			__raw_writel(cur_drex_param->drex_DirectCmd_mr22, data->base_drex[i] + DREX_DIRECTCMD);
			__raw_writel(cur_drex_param->drex_DirectCmd_mr11, data->base_drex[i] + DREX_DIRECTCMD);
			//__raw_writel(cur_drex_param->drex_DirectCmd_mr14, data->base_drex[i] + DREX_DIRECTCMD);

			/* VTMON */
			__raw_writel((0x1<<0)| cur_drex_param->vtmon_Drex_Timing_Set_Sw, data->base_vt_mon_mif[i] + VTMON_DREX_TIMING_SW_SET);
			__raw_writel((0xC0<<2)|cur_drex_param->drex_DirectCmd_mr13, data->base_vt_mon_mif[i] + VTMON_MIF_DREX_WDATA0);
		}
	} else {
		for (i = 0; i< MIF_BLK_NUM; i++) {
			/* DREX */
			tmp = __raw_readl(data->base_drex[i] + DREX_TimingRfcPb);
			tmp &= ~(DREX_TimingRfcPb_Set0_MASK);
			tmp |= (cur_drex_param->drex_TimingRfcPb & DREX_TimingRfcPb_Set0_MASK);
			__raw_writel(tmp, data->base_drex[i] + DREX_TimingRfcPb);
			__raw_writel(cur_drex_param->drex_TimingRow, data->base_drex[i] + DREX_TimingRow_0);
			__raw_writel(cur_drex_param->drex_TimingData, data->base_drex[i] + DREX_TimingData_0);
			__raw_writel(cur_drex_param->drex_TimingPower, data->base_drex[i] + DREX_TimingPower_0);
			__raw_writel(cur_drex_param->drex_RdFetch, data->base_drex[i] + DREX_RdFetch_0);
			__raw_writel((0x0<<7)|cur_drex_param->drex_EtcControl, data->base_drex[i] + DREX_EtcControl_0);
			__raw_writel(cur_drex_param->drex_Train_Timing, data->base_drex[i] + DREX_Train_Timing_0);
			__raw_writel(cur_drex_param->drex_Hw_Ptrain_Period, data->base_drex[i] + DREX_Hw_Ptrain_Period_0);
			__raw_writel(cur_drex_param->drex_Hwpr_Train_Config, data->base_drex[i] + DREX_Hwpr_Train_Config_0);
			__raw_writel(cur_drex_param->drex_Hwpr_Train_Control, data->base_drex[i] + DREX_Hwpr_Train_Control_0);

			/* LP4_PHY */
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON0);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con0_set0_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con0_set0;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON0);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON1);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con1_set0_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con1_set0;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON1);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON2);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con2_set0_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con2_set0;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON2);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON3);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con3_set0_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con3_set0;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON3);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON4);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con4_set0_mask);
			tmp |= (cur_phy_param->phy_Dvfs_Con4_set0 | (soc_vref[i][1] << 6)| (soc_vref[i][0] << 0));
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON4);
			tmp = __raw_readl(data->base_lp4_phy[i] + PHY_DVFS_CON5);
			tmp &= ~(cur_phy_param->phy_Dvfs_Con5_set0_mask);
			tmp |= cur_phy_param->phy_Dvfs_Con5_set0;
			__raw_writel(tmp, data->base_lp4_phy[i] + PHY_DVFS_CON5);

			__raw_writel(cur_vtmon_param->vtmon_Cnt_Limit_set0, data->base_vt_mon_mif[i] + VTMON_CNT_LIMIT_SET0_SW0);
			__raw_writel(cur_vtmon_param->vtmon_Cnt_Limit_set1, data->base_vt_mon_mif[i] + VTMON_CNT_LIMIT_SET1_SW0);

			/* DIRECTCMD */
			__raw_writel((0x80<<2)|cur_drex_param->drex_DirectCmd_mr13, data->base_drex[i] + DREX_DIRECTCMD);
			__raw_writel(cur_drex_param->drex_DirectCmd_mr2, data->base_drex[i] + DREX_DIRECTCMD);
			__raw_writel(cur_drex_param->drex_DirectCmd_mr22, data->base_drex[i] + DREX_DIRECTCMD);
			__raw_writel(cur_drex_param->drex_DirectCmd_mr11, data->base_drex[i] + DREX_DIRECTCMD);
			//__raw_writel(cur_drex_param->drex_DirectCmd_mr14, data->base_drex[i] + DREX_DIRECTCMD);

			/* VTMON */
			__raw_writel((0x0<<0)|cur_drex_param->vtmon_Drex_Timing_Set_Sw, data->base_vt_mon_mif[i] + VTMON_DREX_TIMING_SW_SET);
			__raw_writel((0x0<<2)|cur_drex_param->drex_DirectCmd_mr13, data->base_vt_mon_mif[i] + VTMON_MIF_DREX_WDATA0);
		}
	}
	return 0;
}

void DisablePhyCgEn(struct devfreq_data_mif *data);
void RestorePhyCgEn(struct devfreq_data_mif *data);
int exynos7_devfreq_mif_set_volt(struct devfreq_data_mif *data,
		unsigned long volt,
		unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	DisablePhyCgEn(data);
	regulator_set_voltage(data->vdd_mif, volt, volt_range);
	RestorePhyCgEn(data);
	pr_debug("MIF: set_volt(%lu), get_volt(%d)\n", volt, regulator_get_voltage(data->vdd_mif));
	data->old_volt = volt;
out:
	return 0;
}

#define EXYNOS7420_VT_MON_0		0x10890000
#define EXYNOS7420_VT_MON_1		0x10990000
#define EXYNOS7420_VT_MON_2		0x10A90000
#define EXYNOS7420_VT_MON_3		0x10B90000
#define VT_MON_CON_OFFSET		0x0014
#define PER_UPDATE_STATUS_OFFSET	0x0064
#define CAL_CON0			0x04
#define AREF_UPDATE_EN			BIT(28)
#define PER_MRS_EN			BIT(17)

static void disable_VT_MON_compensation(struct devfreq_data_mif *data)
{
	uint32_t temp;
	unsigned int timeout = 1000;
	int i;

	/* Disable VT MON DQS OSC compensation and wait for its completion */
	data->per_mrs_en = __raw_readl(data->base_vt_mon_mif[0] + VT_MON_CON_OFFSET) & (AREF_UPDATE_EN | PER_MRS_EN);

	for (i = 0; i< MIF_BLK_NUM; i++) {
		temp = __raw_readl(data->base_vt_mon_mif[i] + VT_MON_CON_OFFSET);
		temp &= ~(AREF_UPDATE_EN | PER_MRS_EN);
		__raw_writel(temp, data->base_vt_mon_mif[i] + VT_MON_CON_OFFSET);
		timeout = 1000;
		while((__raw_readl(data->base_vt_mon_mif[i] + PER_UPDATE_STATUS_OFFSET) & 0x1F) != 0x0) {
			if (timeout == 0) {
				pr_err("DEVFREQ(MIF) : disable : timeout to wait per_update completion\n");
				return;
			}
			udelay(1);
			timeout--;
		}
	}
}

static void restore_VT_MON_compensation(struct devfreq_data_mif *data)
{
	uint32_t temp;
	unsigned int timeout = 1000;
	int i;

	/* Restore VT MON DQS OSC compensation and wait for its completion */
	for (i = 0; i< MIF_BLK_NUM; i++) {
		temp = __raw_readl(data->base_vt_mon_mif[i] + VT_MON_CON_OFFSET);
		temp |= data->per_mrs_en;
		__raw_writel(temp, data->base_vt_mon_mif[i] + VT_MON_CON_OFFSET);
		timeout = 1000;
		while((__raw_readl(data->base_vt_mon_mif[i] + PER_UPDATE_STATUS_OFFSET) & 0x1F) != 0x0) {
			if (timeout == 0) {
				pr_err("DEVFREQ(MIF) : restore : timeout to wait per_update completion\n");
				return;
			}
			udelay(1);
			timeout--;
		}
	}
}

static int clkdiv_sclk_hpm[] = {
	2,
	2,
	1,
	1,
	1,
	1,
	1,
	0,
	0,
	0,
	0,
	0,
	0,
};

#define SCLK_HPM_MIF0_STAT 	BIT(4)

static void exynos7420_set_clkdiv_sclk_hpm(unsigned int target_idx)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS7420_DIV_MIF0);

	tmp = (tmp & ~(0x07 << 4)) | ((clkdiv_sclk_hpm[target_idx] & 0x07) << 4);

	__raw_writel(tmp, EXYNOS7420_DIV_MIF0);

	while (__raw_readl(EXYNOS7420_DIV_STAT_MIF0) & SCLK_HPM_MIF0_STAT)
		cpu_relax();

	tmp = __raw_readl(EXYNOS7420_DIV_MIF1);

	tmp = (tmp & ~(0x07 << 4)) | ((clkdiv_sclk_hpm[target_idx] & 0x07) << 4);

	__raw_writel(tmp, EXYNOS7420_DIV_MIF1);

	while (__raw_readl(EXYNOS7420_DIV_STAT_MIF1) & SCLK_HPM_MIF0_STAT)
		cpu_relax();

	tmp = __raw_readl(EXYNOS7420_DIV_MIF2);

	tmp = (tmp & ~(0x07 << 4)) | ((clkdiv_sclk_hpm[target_idx] & 0x07) << 4);

	__raw_writel(tmp, EXYNOS7420_DIV_MIF2);

	while (__raw_readl(EXYNOS7420_DIV_STAT_MIF2) & SCLK_HPM_MIF0_STAT)
		cpu_relax();

	tmp = __raw_readl(EXYNOS7420_DIV_MIF3);

	tmp = (tmp & ~(0x07 << 4)) | ((clkdiv_sclk_hpm[target_idx] & 0x07) << 4);

	__raw_writel(tmp, EXYNOS7420_DIV_MIF3);

	while (__raw_readl(EXYNOS7420_DIV_STAT_MIF3) & SCLK_HPM_MIF0_STAT)
		cpu_relax();
}

static int exynos7_devfreq_ccore_set_freq(struct devfreq_data_mif *data,
					int old_idx,
					int target_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;

	/* Change ccore clock */
	if (target_idx < old_idx) {
		/* mux , divider */
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_ccore_info_list); ++i) {
			clk_info = &devfreq_clk_ccore_info_list[i][target_idx];
			clk_states = clk_info->states;
			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_mif_clk[devfreq_clk_ccore_info_idx[i]].clk, clk_info->freq);
				pr_debug("MIF clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_mif_clk[devfreq_clk_ccore_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_mif_clk[devfreq_clk_ccore_info_idx[i]].clk));
			}
		}
	} else {
		/* divider, mux */
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_ccore_info_list); ++i) {
			clk_info = &devfreq_clk_ccore_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_mif_clk[devfreq_clk_ccore_info_idx[i]].clk, clk_info->freq);
				pr_debug("MIF clk name: %s, set_rate: %lu, get_rate: %lu\n",
						devfreq_mif_clk[devfreq_clk_ccore_info_idx[i]].clk_name,
						clk_info->freq, clk_get_rate(devfreq_mif_clk[devfreq_clk_ccore_info_idx[i]].clk));
			}
		}
	}
	return 0;
}

#define MEMCONTROL	0x04
#define PWRDNCONFIG 	0x28
#define PB_REF_EN 	BIT(27)
#define DSREF_EN	BIT(5)

#define DRAM_POWER_DYNAMIC_SAVE
#ifdef DRAM_POWER_DYNAMIC_SAVE
static void exynos7_devfreq_mif_set_dynamic_sref_cycle(struct devfreq_data_mif *data, int target_idx)
{
	int i;
	uint32_t tmp;
	uint32_t dsref_cyc;

	if (target_idx <= MIF_LV4)
		dsref_cyc = 0x3ff;
	else if (target_idx <= MIF_LV7)
		dsref_cyc = 0x1ff;
	else
		dsref_cyc = 0x90;

	/* dsref disable */
	for (i = 0; i< MIF_BLK_NUM; i++) {
		tmp = __raw_readl(data->base_drex[i] + MEMCONTROL);
		tmp &= ~DSREF_EN;
		__raw_writel(tmp, data->base_drex[i] + MEMCONTROL);
	}

	/* dsref_cyc[31:16]: Number of Cycles for dynamic self refresh entry
	 * dpwrdn_cyc[7:0]: Number of Cycles for dynamic power down entry
	 */
	for (i = 0; i< MIF_BLK_NUM; i++) {
		tmp = __raw_readl(data->base_drex[i] + PWRDNCONFIG);
		tmp = ((tmp & ~(0xffff<<16)) | (dsref_cyc << 16));
		__raw_writel(tmp, data->base_drex[i] + PWRDNCONFIG);
	}

	/* dsref enable */
	for (i = 0; i< MIF_BLK_NUM; i++) {
		tmp = __raw_readl(data->base_drex[i] + MEMCONTROL);
		tmp |= DSREF_EN;
		__raw_writel(tmp, data->base_drex[i] + MEMCONTROL);
	}
}

static void exynos7_devfreq_mif_set_PBR_en(struct devfreq_data_mif *data, bool en)
{
	int i;
	uint32_t tmp;
	for (i = 0; i< MIF_BLK_NUM; i++) {
		/* DREX */
		tmp = __raw_readl(data->base_drex[i] + MEMCONTROL);
		tmp = ((tmp & ~PB_REF_EN) | (en << 27));
		__raw_writel(tmp, data->base_drex[i] + MEMCONTROL);
	}
	return;
}

static void exynos7_devfreq_mif_set_refresh_method_pre_dvfs(struct devfreq_data_mif *data, int target_idx, int old_idx)
{
	if ((old_idx < MIF_LV4) && (target_idx >= MIF_LV4))
		exynos7_devfreq_mif_set_PBR_en(data, 0);
}

static void exynos7_devfreq_mif_set_refresh_method_post_dvfs(struct devfreq_data_mif *data, int target_idx, int old_idx)
{
	if ((old_idx >= MIF_LV4) && (target_idx < MIF_LV4))
		exynos7_devfreq_mif_set_PBR_en(data, 1);
}
#endif

static uint32_t gPhy_cg_en;

#define CGCONTROL      0x0008
#define PHY_CG_EN      BIT(4)
void DisablePhyCgEn(struct devfreq_data_mif *data)
{
	int i;
	uint32_t tmp;

	for (i=0; i<MIF_BLK_NUM; i++)
	{
		tmp = __raw_readl(data->base_drex[i] + CGCONTROL);
		/* save */
		if (i == 0)
			gPhy_cg_en = tmp & PHY_CG_EN;
		/* clear */
		tmp = tmp & ~PHY_CG_EN;
		__raw_writel(tmp, data->base_drex[i] + CGCONTROL);
	}
}

void RestorePhyCgEn(struct devfreq_data_mif *data)
{
	int i;
	uint32_t tmp;

	for (i=0; i<MIF_BLK_NUM; i++)
	{
		tmp = __raw_readl(data->base_drex[i] + CGCONTROL);
		/* restore */
		tmp = tmp | gPhy_cg_en;
		__raw_writel(tmp, data->base_drex[i] + CGCONTROL);
	}
}

#ifdef DMC_DQS_MODE2
#define CMOS_SE_OFFSET 56250
#else
#define CMOS_SE_OFFSET 0
#endif

static int exynos7_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	int pll_safe_idx = -1;
	unsigned int voltage = 0;
	struct clk *mif_pll, *sclk_bus0_pll_mif, *mout_aclk_mif_pll;
	struct clk *mout_sclk_bus0_pll_mif;

	mif_pll = devfreq_mif_clk[MIF_PLL].clk;
	sclk_bus0_pll_mif = devfreq_mif_clk[SCLK_BUS0_PLL_MIF].clk;
	mout_aclk_mif_pll = devfreq_mif_clk[MOUT_ACLK_MIF_PLL].clk;
	mout_sclk_bus0_pll_mif = devfreq_mif_clk[MOUT_SCLK_BUS0_PLL_MIF].clk;

	exynos7_devfreq_check_mif_mux_ctrl_by_cmuc(data);

#ifdef DRAM_POWER_DYNAMIC_SAVE
	exynos7_devfreq_mif_set_refresh_method_pre_dvfs(data, target_idx, old_idx);
#endif
	if ((old_idx <= MIF_LV4) && (target_idx <= MIF_LV4)) {
		clk_set_rate(devfreq_mif_clk[DOUT_SCLK_BUS0_PLL_MIF].clk, 1600000000);
		data->pll_safe_idx = MIF_LV4;
	} else {
		clk_set_rate(devfreq_mif_clk[DOUT_SCLK_BUS0_PLL_MIF].clk, 800000000);
		data->pll_safe_idx = MIF_LV7;
	}

	pll_safe_idx = data->pll_safe_idx;

	/* Change volt for swiching PLL if it needed */
#ifndef DMC_DQS_MODE0
	if ((old_idx <= MIF_LV4) && (target_idx <= MIF_LV4)) {
		voltage = 0;
	} else {
		if (data->pll_safe_idx <= old_idx)
			voltage = devfreq_mif_opp_list[pll_safe_idx].volt + CMOS_SE_OFFSET;
		else if ((target_idx < old_idx) && (MIF_LV4 <= old_idx))
			voltage = devfreq_mif_opp_list[target_idx].volt;
		else
			voltage = 0;
	}
#else
	if ((data->pll_safe_idx <= old_idx) && (data->pll_safe_idx <= target_idx))
		voltage = devfreq_mif_opp_list[pll_safe_idx].volt;
	else if (old_idx >= target_idx)
		voltage = devfreq_mif_opp_list[target_idx].volt;
	else
		voltage = 0;
#endif

	if (voltage) {
#ifdef CONFIG_EXYNOS_THERMAL
		voltage = get_limit_voltage(voltage, data->volt_offset, data->volt_of_avail_max_freq);
#endif
		exynos7_devfreq_mif_set_volt(data, voltage, voltage + VOLT_STEP);
	} else {
		pr_debug("1st: skip voltage setting from %lu to %lu\n",
				devfreq_mif_opp_list[old_idx].freq, devfreq_mif_opp_list[data->pll_safe_idx].freq);
	}

	/* Disable VT MON DQS OSC compensation and wait for its completion */
	disable_VT_MON_compensation(data);

	/* Change CCORE clock for switching pll
	   CCORE clock level always  must be same or slower than DREX clock level.
	   - When decreasing MIF DVFS clock
	   : Decreasing CCORE clock and then decreasing DREX clock
	   - When increasing MIF DVFS clock
	   : Increasing DREX clock and then increasing CCORE clock
	*/
	if ((data->pll_safe_idx >= old_idx) && (data->pll_safe_idx >= target_idx))
		exynos7_devfreq_ccore_set_freq(data, old_idx, data->pll_safe_idx);
	else if (old_idx <= target_idx)
		exynos7_devfreq_ccore_set_freq(data, old_idx, target_idx);

	/* Apply timing set 1 for switching PLL */
	exynos7_devfreq_mif_preset(data, pll_safe_idx, 1);

	/* clk enable for switching PLL */
	clk_prepare_enable(sclk_bus0_pll_mif);

	/* sclk_hpm_mif dfs for cl-dvfs */
	exynos7420_set_clkdiv_sclk_hpm(pll_safe_idx);

	/* Change to switching PLL(dout_sclk_bus0_pll_mif), and then wait pause completion */
	clk_set_parent(mout_aclk_mif_pll, sclk_bus0_pll_mif);
	exynos7_devfreq_waiting_pause(data);
	exynos7_devfreq_waiting_mux(data);

#ifndef DMC_DQS_MODE0
	/* For Differential DQS mode,
	 * it set the target voltage prior to target freq setting for gate re-training performing at right voltage */
	if ((old_idx <= MIF_LV4) && (target_idx <= MIF_LV4)) {
		voltage = devfreq_mif_opp_list[target_idx].volt;
	} else {
		if ((data->pll_safe_idx <= old_idx) && (target_idx < data->pll_safe_idx))
			voltage = devfreq_mif_opp_list[target_idx].volt;
		else if ((old_idx <= MIF_LV3) && (target_idx <= MIF_LV3))
			voltage = devfreq_mif_opp_list[target_idx].volt;
		else if ((old_idx < data->pll_safe_idx) && (data->pll_safe_idx <= target_idx))
			voltage = devfreq_mif_opp_list[pll_safe_idx].volt + CMOS_SE_OFFSET;
		else
			voltage = 0;
	}

	if (voltage) {
#ifdef CONFIG_EXYNOS_THERMAL
		voltage = get_limit_voltage(voltage, data->volt_offset, data->volt_of_avail_max_freq);
#endif
		exynos7_devfreq_mif_set_volt(data, voltage, voltage + VOLT_STEP);
	}
#endif

	/* Change mif_pll rate for target freq*/
	clk_set_rate(mif_pll, devfreq_mif_opp_list[target_idx].freq * 2 * KHZ);

	/* Apply timing set 0 for target freq */
	exynos7_devfreq_mif_preset(data, target_idx, 0);

	/* sclk_hpm_mif dfs for cl-dvfs */
	exynos7420_set_clkdiv_sclk_hpm(target_idx);

	/* Change to target PLL(fout_mif_pll), and then wait pause completion * */
	clk_set_parent(mout_aclk_mif_pll, mif_pll);
	exynos7_devfreq_waiting_pause(data);
	exynos7_devfreq_waiting_mux(data);

	/* clk disable for switching PLL */
	clk_disable_unprepare(sclk_bus0_pll_mif);

	/* Change ccore clock for target */
	if ((data->pll_safe_idx >= old_idx) && (data->pll_safe_idx >= target_idx))
		exynos7_devfreq_ccore_set_freq(data, data->pll_safe_idx, target_idx);
	else if (old_idx <= target_idx)
		;
	else
		exynos7_devfreq_ccore_set_freq(data, old_idx, target_idx);

	/* Restore VT MON DQS OSC compensation and wait for its completion */
	restore_VT_MON_compensation(data);

	/* Change volt for target if it needed */
#ifndef DMC_DQS_MODE0
	if ((old_idx <= MIF_LV4) && (target_idx <= MIF_LV4)) {
		voltage = 0;
	} else {
		if ((data->pll_safe_idx <= old_idx) && (data->pll_safe_idx < target_idx))
			voltage = devfreq_mif_opp_list[target_idx].volt;
		else if ((old_idx < target_idx) && (old_idx < data->pll_safe_idx) && (MIF_LV4 <= target_idx))
			voltage = devfreq_mif_opp_list[target_idx].volt;
		else
			voltage = 0;
	}
#else
	if ((data->pll_safe_idx <= old_idx) && (data->pll_safe_idx <= target_idx)) {
		voltage = devfreq_mif_opp_list[target_idx].volt;
	} else if (old_idx < target_idx) {
		voltage = devfreq_mif_opp_list[target_idx].volt;
	} else
		voltage = 0;
#endif

	if (voltage) {
#ifdef CONFIG_EXYNOS_THERMAL
		voltage = get_limit_voltage(voltage, data->volt_offset, data->volt_of_avail_max_freq);
#endif
		exynos7_devfreq_mif_set_volt(data, voltage, voltage + VOLT_STEP);
	} else {
		pr_debug("2nd: skip voltage setting from %lu to %lu\n",
				devfreq_mif_opp_list[data->pll_safe_idx].freq, devfreq_mif_opp_list[target_idx].freq);
	}

#ifdef DRAM_POWER_DYNAMIC_SAVE
	exynos7_devfreq_mif_set_refresh_method_post_dvfs(data, target_idx, old_idx);
	exynos7_devfreq_mif_set_dynamic_sref_cycle(data, target_idx);
#endif

#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_mif_clk, MIF_CLK_COUNT, "MIF");
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
	unsigned int *on = v, cl_idx;
	int i;
	struct opp *target_opp;
	unsigned long freq = 0;

	if (event == MIF_TH_LV2) {
		/* it means that tmu_temp is Greater than or equal to 65 degrees */
		mutex_lock(&data->lock);

		/* DRAM Thermal offset 10 */
		data->tmu_temp = 1;
		for (i = 0; i< MIF_BLK_NUM; i++) {
			/* cs0 */
			__raw_writel((0x00001000 | (0 << 20) | 0x40 << 2), data->base_drex[i] + DREX_DIRECTCMD);
			/* cs1 */
			__raw_writel((0x00001000 | (1 << 20) | 0x40 << 2), data->base_drex[i] + DREX_DIRECTCMD);
		}

		mutex_unlock(&data->lock);
		pr_info("MIF_TH_LV2 from TMU, %d\n", data->tmu_temp);
		return NOTIFY_OK;
	} else if (event == MIF_TH_LV1) {
		/* it means that tmu_temp is under 65 degrees */
		mutex_lock(&data->lock);

		data->tmu_temp = 0;

		mutex_unlock(&data->lock);
		pr_info("MIF_TH_LV1 from TMU, %d\n", data->tmu_temp);
		return NOTIFY_OK;
	} else if (event == TMU_COLD) {
		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = (uint32_t)data->old_volt;

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			rcu_read_lock();
			cl_idx = devfreq_get_opp_idx(devfreq_mif_opp_list, data->max_state, data->cur_freq);
			rcu_read_unlock();
			set_volt = get_limit_voltage(prev_volt, data->volt_offset, data->volt_of_avail_max_freq);
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
			exynos7420_cl_dvfs_stop(ID_MIF, cl_idx);
#endif
			DisablePhyCgEn(data);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);
			RestorePhyCgEn(data);

			data->old_volt = set_volt;

			freq = data->cur_freq;
			mutex_unlock(&data->lock);

			pr_info("MIF(%lu): set TMU_COLD: %d => (set_volt: %d, get_volt: %d)\n",
					freq, prev_volt, set_volt, regulator_get_voltage(data->vdd_mif));
		} else {
			mutex_lock(&data->lock);

			prev_volt = (uint32_t)data->old_volt;

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			rcu_read_lock();
			freq = data->cur_freq;
			target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
			set_volt = (uint32_t)opp_get_voltage(target_opp);
			cl_idx = devfreq_get_opp_idx(devfreq_mif_opp_list, data->max_state, freq);
			rcu_read_unlock();

#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
			exynos7420_cl_dvfs_stop(ID_MIF, cl_idx);
#endif
			DisablePhyCgEn(data);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);
			RestorePhyCgEn(data);
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
			exynos7420_cl_dvfs_start(ID_MIF);
#endif
			data->old_volt = set_volt;

			mutex_unlock(&data->lock);

			pr_info("MIF(%lu): unset TMU_COLD: %d => (set_volt: %d, get_volt: %d)\n",
					freq, prev_volt, set_volt, regulator_get_voltage(data->vdd_mif));
		}
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

static ssize_t mif_show_templvl_ch0_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch0_cs0);
}
static ssize_t mif_show_templvl_ch0_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch0_cs1);
}
static ssize_t mif_show_templvl_ch1_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch1_cs0);
}
static ssize_t mif_show_templvl_ch1_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch1_cs1);
}
static ssize_t mif_show_templvl_ch2_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch2_cs0);
}
static ssize_t mif_show_templvl_ch2_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch2_cs1);
}
static ssize_t mif_show_templvl_ch3_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch3_cs0);
}
static ssize_t mif_show_templvl_ch3_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_thermal_work.thermal_level_ch3_cs1);
}

static DEVICE_ATTR(mif_templvl_ch0_0, 0644, mif_show_templvl_ch0_0, NULL);
static DEVICE_ATTR(mif_templvl_ch0_1, 0644, mif_show_templvl_ch0_1, NULL);
static DEVICE_ATTR(mif_templvl_ch1_0, 0644, mif_show_templvl_ch1_0, NULL);
static DEVICE_ATTR(mif_templvl_ch1_1, 0644, mif_show_templvl_ch1_1, NULL);
static DEVICE_ATTR(mif_templvl_ch2_0, 0644, mif_show_templvl_ch2_0, NULL);
static DEVICE_ATTR(mif_templvl_ch2_1, 0644, mif_show_templvl_ch2_1, NULL);
static DEVICE_ATTR(mif_templvl_ch3_0, 0644, mif_show_templvl_ch3_0, NULL);
static DEVICE_ATTR(mif_templvl_ch3_1, 0644, mif_show_templvl_ch3_1, NULL);

static struct attribute *devfreq_mif_sysfs_entries[] = {
	&dev_attr_mif_templvl_ch0_0.attr,
	&dev_attr_mif_templvl_ch0_1.attr,
	&dev_attr_mif_templvl_ch1_0.attr,
	&dev_attr_mif_templvl_ch1_1.attr,
	&dev_attr_mif_templvl_ch2_0.attr,
	&dev_attr_mif_templvl_ch2_1.attr,
	&dev_attr_mif_templvl_ch3_0.attr,
	&dev_attr_mif_templvl_ch3_1.attr,
	NULL,
};

struct attribute_group devfreq_mif_attr_group = {
	.name   = "debug",
	.attrs  = devfreq_mif_sysfs_entries,
};

static void exynos7_devfreq_swtrip(void)
{
	pr_err("DEVFREQ(MIF) : SW trip by MR4\n");
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
#define MRSTATUS			0x54
#define TIMINGAREF			0x30

enum devfreq_mif_thermal_autorate exynos7_devfreq_get_autorate(unsigned int thermal_level)
{
	enum devfreq_mif_thermal_autorate timingaref_value = RATE_ONE;
	switch (thermal_level) {
	case 0:
	case 1:
	case 2:
		timingaref_value = RATE_TWO;
		break;
	case 3:
		timingaref_value = RATE_ONE;
		break;
	case 4:
		timingaref_value = RATE_HALF;
		break;
	case 5:
	case 6:
	case 7:
		timingaref_value = RATE_QUARTER;
		break;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return -1;
	}
	return timingaref_value;
}

static void exynos7_devfreq_mr4_set(void __iomem *base_drex, uint32_t cs_num, uint32_t prev_mr4, uint32_t next_mr4, uint32_t tmu_temp)
{
	uint32_t cmd_chip;
	cmd_chip = !!cs_num;

	if (!tmu_temp) {
		if (next_mr4 >=4) {
			/* DRAM Thermal offset 10 */
			__raw_writel((0x00001000 | (cmd_chip << 20) | 0x40 << 2), base_drex + DREX_DIRECTCMD);
		} else {
			/* DRAM Thermal offset 0 */
			__raw_writel((0x00001000 | (cmd_chip << 20) | 0 << 2), base_drex + DREX_DIRECTCMD);
		}
	}
}

static void exynos7_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int max_thermal_level = 0;
	unsigned int ch0_timingaref_value = RATE_ONE;
	unsigned int ch1_timingaref_value = RATE_ONE;
	unsigned int ch2_timingaref_value = RATE_ONE;
	unsigned int ch3_timingaref_value = RATE_ONE;
	unsigned int ch0_cs0_thermal_level = 0;
	unsigned int ch0_cs1_thermal_level = 0;
	unsigned int ch1_cs0_thermal_level = 0;
	unsigned int ch1_cs1_thermal_level = 0;
	unsigned int ch2_cs0_thermal_level = 0;
	unsigned int ch2_cs1_thermal_level = 0;
	unsigned int ch3_cs0_thermal_level = 0;
	unsigned int ch3_cs1_thermal_level = 0;
	unsigned int ch0_max_thermal_level = 0;
	unsigned int ch1_max_thermal_level = 0;
	unsigned int ch2_max_thermal_level = 0;
	unsigned int ch3_max_thermal_level = 0;
	unsigned int mrstatus0 = 0;
	unsigned int mrstatus1 = 0;
	unsigned int mrstatus2 = 0;
	unsigned int mrstatus3 = 0;
	void __iomem *base_drex0 = NULL;
	void __iomem *base_drex1 = NULL;
	void __iomem *base_drex2 = NULL;
	void __iomem *base_drex3 = NULL;
	bool throttling = false;
	unsigned long max_freq = devfreq_mif_opp_list[MIF_LV0].freq;

	base_drex0 = data_mif->base_drex[0];
	base_drex1 = data_mif->base_drex[1];
	base_drex2 = data_mif->base_drex[2];
	base_drex3 = data_mif->base_drex[3];

	mutex_lock(&data_mif->lock);

	__raw_writel(0x09001000, base_drex0 + DREX_DIRECTCMD);
	mrstatus0 = __raw_readl(base_drex0 + MRSTATUS);
	ch0_cs0_thermal_level = (mrstatus0 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex0, 0, thermal_work->thermal_level_ch0_cs0, ch0_cs0_thermal_level, data_mif->tmu_temp);
	if (ch0_cs0_thermal_level > max_thermal_level)
		max_thermal_level = ch0_cs0_thermal_level;
	thermal_work->thermal_level_ch0_cs0 = ch0_cs0_thermal_level;

	__raw_writel(0x09101000, base_drex0 + DREX_DIRECTCMD);
	mrstatus0 = __raw_readl(base_drex0 + MRSTATUS);
	ch0_cs1_thermal_level = (mrstatus0 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex0, 1, thermal_work->thermal_level_ch0_cs1, ch0_cs1_thermal_level, data_mif->tmu_temp);
	if (ch0_cs1_thermal_level > max_thermal_level)
		max_thermal_level = ch0_cs1_thermal_level;
	thermal_work->thermal_level_ch0_cs1 = ch0_cs1_thermal_level;

	ch0_max_thermal_level = max(ch0_cs0_thermal_level, ch0_cs1_thermal_level);

	__raw_writel(0x09001000, base_drex1 + DREX_DIRECTCMD);
	mrstatus1 = __raw_readl(base_drex1 + MRSTATUS);
	ch1_cs0_thermal_level = (mrstatus1 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex1, 0, thermal_work->thermal_level_ch1_cs0, ch1_cs0_thermal_level, data_mif->tmu_temp);
	if (ch1_cs0_thermal_level > max_thermal_level)
		max_thermal_level = ch1_cs0_thermal_level;
	thermal_work->thermal_level_ch1_cs0 = ch1_cs0_thermal_level;

	__raw_writel(0x09101000, base_drex1 + DREX_DIRECTCMD);
	mrstatus1 = __raw_readl(base_drex1 + MRSTATUS);
	ch1_cs1_thermal_level = (mrstatus1 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex1, 1, thermal_work->thermal_level_ch1_cs1, ch1_cs1_thermal_level, data_mif->tmu_temp);
	if (ch1_cs1_thermal_level > max_thermal_level)
		max_thermal_level = ch1_cs1_thermal_level;
	thermal_work->thermal_level_ch1_cs1 = ch1_cs1_thermal_level;

	ch1_max_thermal_level = max(ch1_cs0_thermal_level, ch1_cs1_thermal_level);

	__raw_writel(0x09001000, base_drex2 + DREX_DIRECTCMD);
	mrstatus2 = __raw_readl(base_drex2 + MRSTATUS);
	ch2_cs0_thermal_level = (mrstatus2 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex2, 0, thermal_work->thermal_level_ch2_cs0, ch2_cs0_thermal_level, data_mif->tmu_temp);
	if (ch2_cs0_thermal_level > max_thermal_level)
		max_thermal_level = ch2_cs0_thermal_level;
	thermal_work->thermal_level_ch2_cs0 = ch2_cs0_thermal_level;

	__raw_writel(0x09101000, base_drex2 + DREX_DIRECTCMD);
	mrstatus2 = __raw_readl(base_drex2 + MRSTATUS);
	ch2_cs1_thermal_level = (mrstatus2 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex2, 1, thermal_work->thermal_level_ch2_cs1, ch2_cs1_thermal_level, data_mif->tmu_temp);
	if (ch2_cs1_thermal_level > max_thermal_level)
		max_thermal_level = ch2_cs1_thermal_level;
	thermal_work->thermal_level_ch2_cs1 = ch2_cs1_thermal_level;

	ch2_max_thermal_level = max(ch2_cs0_thermal_level, ch2_cs1_thermal_level);

	__raw_writel(0x09001000, base_drex3 + DREX_DIRECTCMD);
	mrstatus3 = __raw_readl(base_drex3 + MRSTATUS);
	ch3_cs0_thermal_level = (mrstatus0 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex3, 0, thermal_work->thermal_level_ch3_cs0, ch3_cs0_thermal_level, data_mif->tmu_temp);
	if (ch3_cs0_thermal_level > max_thermal_level)
		max_thermal_level = ch3_cs0_thermal_level;
	thermal_work->thermal_level_ch3_cs0 = ch3_cs0_thermal_level;

	__raw_writel(0x09101000, base_drex3 + DREX_DIRECTCMD);
	mrstatus3 = __raw_readl(base_drex3 + MRSTATUS);
	ch3_cs1_thermal_level = (mrstatus3 & MRSTATUS_THERMAL_LV_MASK);
	exynos7_devfreq_mr4_set(base_drex3, 1, thermal_work->thermal_level_ch3_cs1, ch3_cs1_thermal_level, data_mif->tmu_temp);
	if (ch3_cs1_thermal_level > max_thermal_level)
		max_thermal_level = ch3_cs1_thermal_level;
	thermal_work->thermal_level_ch3_cs1 = ch3_cs1_thermal_level;

	ch3_max_thermal_level = max(ch3_cs0_thermal_level, ch3_cs1_thermal_level);

	mutex_unlock(&data_mif->lock);

	switch (max_thermal_level) {
	case 0:
	case 1:
	case 2:
	case 3:
		thermal_work->polling_period = 500;
		break;
	case 4:
		throttling = true;
		thermal_work->polling_period = 300;
		break;
	case 5:
	case 6:
		throttling = true;
		thermal_work->polling_period = 100;
		break;
	case 7:
		throttling = true;
		thermal_work->polling_period = 100;
		exynos7_devfreq_swtrip();
		break;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return;
	}

	if (throttling)
		max_freq = devfreq_mif_opp_list[MIF_LV1].freq;
	else
		max_freq = devfreq_mif_opp_list[MIF_LV0].freq;

	if (thermal_work->max_freq != max_freq) {
		thermal_work->max_freq = max_freq;
		mutex_lock(&data_mif->devfreq->lock);
		update_devfreq(data_mif->devfreq);
		mutex_unlock(&data_mif->devfreq->lock);
		if (throttling)
			pr_info("DEVFREQ(MIF): THROTTLING: max_thermal_level: %d => max_freq: %lu\n", max_thermal_level, max_freq);
	}

	/* set timming auto refresh rate */
	mutex_lock(&data_mif->lock);
	ch0_timingaref_value = exynos7_devfreq_get_autorate(ch0_max_thermal_level);
	if (ch0_timingaref_value > 0)
		__raw_writel(ch0_timingaref_value, base_drex0 + TIMINGAREF);
	ch1_timingaref_value = exynos7_devfreq_get_autorate(ch1_max_thermal_level);
	if (ch1_timingaref_value > 0)
		__raw_writel(ch1_timingaref_value, base_drex1 + TIMINGAREF);
	ch2_timingaref_value = exynos7_devfreq_get_autorate(ch2_max_thermal_level);
	if (ch2_timingaref_value > 0)
		__raw_writel(ch2_timingaref_value, base_drex2 + TIMINGAREF);
	ch3_timingaref_value = exynos7_devfreq_get_autorate(ch3_max_thermal_level);
	if (ch3_timingaref_value > 0)
		__raw_writel(ch3_timingaref_value, base_drex3 + TIMINGAREF);
	mutex_unlock(&data_mif->lock);

	exynos_ss_printk("[MIF]%d,%d(tREFI:0x%08x);%d,%d(0x%08x);%d,%d(0x%08x);%d,%d(0x%08x)",
			ch0_cs0_thermal_level, ch0_cs1_thermal_level, ch0_timingaref_value,
			ch1_cs0_thermal_level, ch1_cs1_thermal_level, ch1_timingaref_value,
			ch2_cs0_thermal_level, ch2_cs1_thermal_level, ch2_timingaref_value,
			ch3_cs0_thermal_level, ch3_cs1_thermal_level, ch3_timingaref_value);
	exynos7_devfreq_thermal_event(thermal_work);
}

void exynos7_devfreq_set_tREFI_1x(struct devfreq_data_mif *data)
{
	uint32_t timingaref_value = 0;
	int i;

	mutex_lock(&data->lock);
	for (i = 0; i< MIF_BLK_NUM; i++) {
		timingaref_value = __raw_readl(data->base_drex[i] + TIMINGAREF);
		if (timingaref_value == RATE_TWO)
			__raw_writel(RATE_ONE, data->base_drex[i] + TIMINGAREF);
	}
	mutex_unlock(&data->lock);
}

void exynos7_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq = alloc_workqueue("devfreq_mif_thermal_wq", WQ_FREEZABLE | WQ_MEM_RECLAIM, 0);

#if defined(CONFIG_DEFERRABLE_INVOKING) || defined(CONFIG_HYBRID_INVOKING)
	INIT_DEFERRABLE_WORK(&devfreq_mif_thermal_work.devfreq_mif_thermal_work,
			exynos7_devfreq_thermal_monitor);
#else
	INIT_DELAYED_WORK(&devfreq_mif_thermal_work.devfreq_mif_thermal_work,
			exynos7_devfreq_thermal_monitor);
#endif
	devfreq_mif_thermal_work.work_queue = devfreq_mif_thermal_wq;
	exynos7_devfreq_thermal_event(&devfreq_mif_thermal_work);
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

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_ccore_info_list); ++i)
		clk_prepare_enable(devfreq_mif_clk[devfreq_clk_ccore_info_idx[i]].clk);

	return 0;
}

#define DREX0_BASE      0x10800000
#define DREX1_BASE      0x10900000
#define DREX2_BASE      0x10a00000
#define DREX3_BASE      0x10b00000
#define CMU_TOPC_BASE	0x10570000
#define CMU_MIF0_BASE	0x10850000
#define CMU_MIF1_BASE	0x10950000
#define CMU_MIF2_BASE	0x10A50000
#define CMU_MIF3_BASE	0x10B50000
#define NSP_BASE		0x10552000
#define PHY0_BASE	0x10820000
#define PHY1_BASE	0x10920000
#define PHY2_BASE	0x10a20000
#define PHY3_BASE	0x10b20000

#define CAL_CON0		0x04
#define CTRL_DQS_OSC_EN		BIT(19)

int exynos7_devfreq_mif_init_parameter(struct devfreq_data_mif *data)
{
	int i;
	uint32_t temp;
	data->base_drex[0] = devm_ioremap(data->dev, DREX0_BASE, SZ_64K);
	data->base_drex[1] = devm_ioremap(data->dev, DREX1_BASE, SZ_64K);
	data->base_drex[2] = devm_ioremap(data->dev, DREX2_BASE, SZ_64K);
	data->base_drex[3] = devm_ioremap(data->dev, DREX3_BASE, SZ_64K);

	data->base_lp4_phy[0] = devm_ioremap(data->dev, PHY0_BASE, SZ_64K);
	data->base_lp4_phy[1] = devm_ioremap(data->dev, PHY1_BASE, SZ_64K);
	data->base_lp4_phy[2] = devm_ioremap(data->dev, PHY2_BASE, SZ_64K);
	data->base_lp4_phy[3] = devm_ioremap(data->dev, PHY3_BASE, SZ_64K);

	data->base_cmu_topc = devm_ioremap(data->dev, CMU_TOPC_BASE, SZ_64K);
	data->base_nsp = devm_ioremap(data->dev, NSP_BASE, SZ_4K);
	data->base_cmu_mif[0] = devm_ioremap(data->dev, CMU_MIF0_BASE, SZ_64K);
	data->base_cmu_mif[1] = devm_ioremap(data->dev, CMU_MIF1_BASE, SZ_64K);
	data->base_cmu_mif[2] = devm_ioremap(data->dev, CMU_MIF2_BASE, SZ_64K);
	data->base_cmu_mif[3] = devm_ioremap(data->dev, CMU_MIF3_BASE, SZ_64K);

	data->base_vt_mon_mif[0] = devm_ioremap(data->dev, EXYNOS7420_VT_MON_0, SZ_64K);
	data->base_vt_mon_mif[1] = devm_ioremap(data->dev, EXYNOS7420_VT_MON_1, SZ_64K);
	data->base_vt_mon_mif[2] = devm_ioremap(data->dev, EXYNOS7420_VT_MON_2, SZ_64K);
	data->base_vt_mon_mif[3] = devm_ioremap(data->dev, EXYNOS7420_VT_MON_3, SZ_64K);

	/* it shoul be clear ctrl_dqs_osc_en, per_mrs_en in bl 2*/
	for (i = 0; i< MIF_BLK_NUM; i++) {
		temp = __raw_readl(data->base_vt_mon_mif[i] + VT_MON_CON_OFFSET);
		pr_info("VT_MON%d: PER_MRS_EN: 0x%08x\n", i, (uint32_t)(temp & PER_MRS_EN));
		//temp &= ~(PER_MRS_EN);
		//__raw_writel(temp, data->base_vt_mon_mif[i] + VT_MON_CON_OFFSET);
	}

	for (i = 0; i< MIF_BLK_NUM; i++) {
		temp = __raw_readl(data->base_lp4_phy[i] + CAL_CON0);
		pr_info("LP4_PHY%d: CTRL_DQS_OSC_EN: 0x%08x\n", i, (uint32_t)(temp & CTRL_DQS_OSC_EN));
		//temp &= ~(CTRL_DQS_OSC_EN);
		//__raw_writel(temp, data->base_lp4_phy[i] + CAL_CON0);
	}
	return 0;
}

int exynos7420_devfreq_mif_init(struct devfreq_data_mif *data)
{
	int ret = 0;
	data_mif = data;

	data->max_state = MIF_LV_COUNT;

	data->mif_set_freq = exynos7_devfreq_mif_set_freq;
	data->mif_set_volt = NULL;

	exynos7_devfreq_mif_init_clock();
	exynos7_devfreq_mif_init_parameter(data);
#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos7_devfreq_mif_tmu_notifier;
#endif
	data->pll_safe_idx = MIF_LV7;
	clk_set_rate(devfreq_mif_clk[DOUT_SCLK_BUS0_PLL_MIF].clk, 800000000);
	pr_info("DEVFREQ(MIF): MIF SWITCHING PLL rate: (%lu/2)\n",
			clk_get_rate(devfreq_mif_clk[DOUT_SCLK_BUS0_PLL_MIF].clk));
	pr_info("DEVFREQ(MIF): pll_safe_idx: MIF_LV%d\n", data->pll_safe_idx);

	pr_info("DEVFREQ(MIF): dvfs_dqs_osc_en: %d\n", dvfs_dqs_osc_en);
	pr_info("DEVFREQ(MIF): dvfs_offset: %d\n", dvfs_offset);

	return ret;
}

int exynos7420_devfreq_mif_deinit(struct devfreq_data_mif *data)
{
	flush_workqueue(devfreq_mif_thermal_wq);
	destroy_workqueue(devfreq_mif_thermal_wq);

	return 0;
}
/* end of MIF related function */
