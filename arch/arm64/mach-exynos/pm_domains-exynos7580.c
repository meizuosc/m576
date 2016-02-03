/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #include <mach/tmu.h> */
#include <linux/delay.h>
#include "pm_domains-exynos7580.h"

struct exynos7580_pd_data *exynos_pd_find_data(struct exynos_pm_domain *pd)
{
	int i;
	struct exynos7580_pd_data *pd_data;

	/* find pd_data for each power domain */
	for (i = 0, pd_data = &pd_data_list[0]; i < ARRAY_SIZE(pd_data_list); i++, pd_data++) {
		if (strcmp(pd_data->name, pd->name))
			continue;

		DEBUG_PRINT_INFO("%s: found pd_data\n", pd->name);
		break;
	}

	return pd_data;
}

int exynos_pd_clk_get(struct exynos_pm_domain *pd)
{
	int i = 0;
	struct exynos7580_pd_data *pd_data = pd->pd_data;
	struct exynos_pd_clk *top_clks = NULL;

	DEBUG_PRINT_INFO("%s: fetch pd top clkd\n", pd->name);

	if (!pd_data) {
		pr_err(PM_DOMAIN_PREFIX "pd_data is null\n");
		return -ENODEV;
	}

	if (!pd_data->top_clks) {
		DEBUG_PRINT_INFO("%s: pd clk not defined \n", pd->name);
		return 0;
	}

	top_clks = pd_data->top_clks;
	for (i = 0; i < pd_data->num_top_clks; i++) {
		top_clks[i].clock = samsung_clk_get_by_reg((unsigned long)top_clks[i].reg, top_clks[i].bit_offset);
		if (!top_clks[i].clock) {
			pr_err(PM_DOMAIN_PREFIX "clk_get of %s's top clock has been failed  i = %d \n", pd->name, i);
			return -ENODEV;
		}
	}
	return 0;
}


static int exynos7_pd_enable_ccf_clks(struct exynos_pd_clk *ptr, int nr_regs)
{
	unsigned int ret;

	for (; nr_regs > 0; nr_regs--, ptr++) {
		ret = clk_prepare_enable(ptr->clock);
		if (ret)
			return ret;

		DEBUG_PRINT_INFO("clock name : %s, usage_count : %d, SFR : 0x%x\n",
			ptr->clock->name, ptr->clock->enable_count, __raw_readl(ptr->reg));
	}
	return 0;
}

static void exynos7_pd_disable_ccf_clks(struct exynos_pd_clk *ptr, int nr_regs)
{
	for (; nr_regs > 0; nr_regs--, ptr++) {
		if (ptr->clock->enable_count > 0) {
			clk_disable_unprepare(ptr->clock);
			DEBUG_PRINT_INFO("clock name : %s, usage_count : %d, SFR : 0x%x\n",
				ptr->clock->name, ptr->clock->enable_count, __raw_readl(ptr->reg));
		}
	}
}

static void exynos7_pd_set_sys_pwr_regs(struct exynos_pd_reg *ptr, int nr_regs)
{
	unsigned int reg;

	if (!ptr)
		return;

	for (; nr_regs > 0; nr_regs--, ptr++) {
		reg = __raw_readl(ptr->reg);
		reg &= ~(1 << ptr->bit_offset);
		__raw_writel(reg, ptr->reg);
	}
}

static void exynos_pd_notify_power_state(struct exynos_pm_domain *pd, unsigned int turn_on)
{
#if defined(CONFIG_ARM_EXYNOS7580_BUS_DEVFREQ) && defined(CONFIG_PM_RUNTIME)
/*	will be enabled once it is integrated with devfreq */
/* 	exynos7_int_notify_power_status(pd->genpd.name, true);	*/
/*	exynos7_isp_notify_power_status(pd->genpd.name, true);	*/
/*	exynos7_disp_notify_power_status(pd->genpd.name, true);	*/
#endif
}

static int exynos_pd_power_init(struct exynos_pm_domain *pd)
{
	struct exynos7580_pd_data *pd_data = pd->pd_data;

	DEBUG_PRINT_INFO("%s's init callback start\n", pd->name);

	/* enable the top clocks, disabled by unused clock gating */
	if (!pd_data->top_clks)
		return 0;

	exynos7_pd_enable_ccf_clks(pd_data->top_clks, pd_data->num_top_clks);
	DEBUG_PRINT_INFO("%s's init callback start\n", pd->name);

	return 0;
}

static int exynos_pd_power_on_pre(struct exynos_pm_domain *pd)
{
	struct exynos7580_pd_data *pd_data = pd->pd_data;

	DEBUG_PRINT_INFO("%s's on_pre callback start\n", pd->name);


	/* 1. Enabling Top-to-Local clocks */
	if (!pd_data->top_clks)
		DEBUG_PRINT_INFO("Nothing to do for %s :\n", pd->name);
	else
		exynos7_pd_enable_ccf_clks(pd_data->top_clks, pd_data->num_top_clks);

	/* 2. Setting sys pwr regs */
	exynos7_pd_set_sys_pwr_regs(pd_data->sys_pwr_regs, pd_data->num_sys_pwr_regs);

	DEBUG_PRINT_INFO("%s's on_pre callback end\n", pd->name);

	return 0;
}

static int exynos_pd_power_on_post(struct exynos_pm_domain *pd)
{
	struct exynos7580_pd_data *pd_data = pd->pd_data;

	DEBUG_PRINT_INFO("%s's on_post callback start\n", pd->name);

	/* 1. Restoring resetted SFRs belonged to this block */
	exynos_restore_sfr(pd_data->save_list, pd_data->num_save_list);

	exynos_pd_notify_power_state(pd, true);

	DEBUG_PRINT_INFO("%s's on_post callback end\n", pd->name);

	return 0;
}

static int exynos_pd_power_off_pre(struct exynos_pm_domain *pd)
{
	struct exynos7580_pd_data *pd_data = pd->pd_data;

	DEBUG_PRINT_INFO("%s's off_pre callback start\n", pd->name);
	/* 1. Save SFRs belonged to this block */
	exynos_save_sfr(pd_data->save_list, pd_data->num_save_list);

	/* 2. Setting sys pwr  */
	exynos7_pd_set_sys_pwr_regs(pd_data->sys_pwr_regs, pd_data->num_sys_pwr_regs);

	DEBUG_PRINT_INFO("%s's off_pre callback end\n", pd->name);

	return 0;
}

static int exynos_pd_power_off_post(struct exynos_pm_domain *pd)
{
	struct exynos7580_pd_data *pd_data = pd->pd_data;

	DEBUG_PRINT_INFO("%s's off_post callback start\n", pd->name);

	/* Disable the CMU top clocks */
	if (!pd_data->top_clks)
		DEBUG_PRINT_INFO("Nothing to do for %s :\n", pd->name);
	else
		exynos7_pd_disable_ccf_clks(pd_data->top_clks, pd_data->num_top_clks);

	DEBUG_PRINT_INFO("%s's off_post callback end\n", pd->name);

	return 0;
}

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.name = "pd-aud",
		.init = exynos_pd_power_init,
		.on_pre = exynos_pd_power_on_pre,
		.on_post = exynos_pd_power_on_post,
		.off_pre = exynos_pd_power_off_pre,
	}, {
		.name = "pd-mfcmscl",
		.init = exynos_pd_power_init,
		.on_pre = exynos_pd_power_on_pre,
		.on_post = exynos_pd_power_on_post,
		.off_pre = exynos_pd_power_off_pre,
		.off_post = exynos_pd_power_off_post,
	}, {
		.name = "pd-disp",
		.init = exynos_pd_power_init,
		.on_pre = exynos_pd_power_on_pre,
		.on_post = exynos_pd_power_on_post,
		.off_pre = exynos_pd_power_off_pre,
		.off_post = exynos_pd_power_off_post,
	}, {
		.name = "pd-g3d",
		.init = exynos_pd_power_init,
		.on_pre = exynos_pd_power_on_pre,
		.on_post = exynos_pd_power_on_post,
		.off_pre = exynos_pd_power_off_pre,
		.off_post = exynos_pd_power_off_post,
	}, {
		.name = "pd-isp",
		.init = exynos_pd_power_init,
		.on_pre = exynos_pd_power_on_pre,
		.on_post = exynos_pd_power_on_post,
		.off_pre = exynos_pd_power_off_pre,
		.off_post = exynos_pd_power_off_post,
	},
};

struct exynos_pd_callback *exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

	DEBUG_PRINT_INFO("%s: find callback function for\n", pd->name);

	/* find callback function for power domain */
	for (i = 0, cb = &pd_callback_list[0]; i < ARRAY_SIZE(pd_callback_list); i++, cb++) {
		if (strcmp(cb->name, pd->name))
			continue;

		DEBUG_PRINT_INFO("%s: found callback function\n", pd->name);
		break;
	}

	pd->cb = cb;
	return cb;
}
