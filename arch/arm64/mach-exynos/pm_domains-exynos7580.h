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

#include <mach/exynos-powermode.h>
#include <mach/pm_domains.h>
#include <mach/devfreq.h>
#include "pm_domains-exynos7580-cal.h"

struct exynos7580_pd_data {
	const char *name;
	struct exynos_pd_clk *top_clks;
	struct exynos_pd_reg *sys_pwr_regs;
	struct sfr_save *save_list;
	unsigned int num_top_clks;
	unsigned int num_sys_pwr_regs;
	unsigned int num_save_list;
};

static struct exynos7580_pd_data pd_data_list[] = {
	{
		.name = "pd-g3d",
		.top_clks = top_clks_g3d,
		.sys_pwr_regs = sys_pwr_regs_g3d,
		.save_list = save_list_g3d,
		.num_top_clks = ARRAY_SIZE(top_clks_g3d),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_g3d),
		.num_save_list = ARRAY_SIZE(save_list_g3d),
	}, {
		.name = "pd-isp",
		.top_clks = top_clks_isp,
		.sys_pwr_regs = sys_pwr_regs_isp,
		.save_list = save_list_isp,
		.num_top_clks = ARRAY_SIZE(top_clks_isp),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_isp),
		.num_save_list = ARRAY_SIZE(save_list_isp),
	}, {
		.name = "pd-disp",
		.top_clks = top_clks_disp,
		.sys_pwr_regs = sys_pwr_regs_disp,
		.save_list = save_list_disp,
		.num_top_clks = ARRAY_SIZE(top_clks_disp),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_disp),
		.num_save_list = ARRAY_SIZE(save_list_disp),
	}, {
		.name = "pd-aud",
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_aud),
		.save_list = save_list_aud,
		.num_save_list = ARRAY_SIZE(save_list_aud),
	}, {
		.name = "pd-mfcmscl",
		.top_clks = top_clks_mscl,
		.sys_pwr_regs = sys_pwr_regs_mscl,
		.save_list = save_list_mscl,
		.num_top_clks = ARRAY_SIZE(top_clks_mscl),
		.num_save_list = ARRAY_SIZE(save_list_mscl),
	},
};
