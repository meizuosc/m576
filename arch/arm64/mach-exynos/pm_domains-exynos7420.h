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
#include "pm_domains-exynos7420-cal.h"

void __iomem *gpu_dvs_ctrl;
void __iomem *lpi_mask_cam1_busmaster;
void __iomem *lpi_mask_cam0_busmaster;
void __iomem *lpi_mask_isp1_busmaster;
void __iomem *lpi_mask_isp0_busmaster;

struct exynos7420_pd_data {
	const char *name;
	struct exynos_pd_clk *top_clks;
	struct exynos_pd_clk *local_clks;
	struct exynos_pd_clk *asyncbridge_clks;
	struct exynos_pd_reg *sys_pwr_regs;
	struct sfr_save *save_list;
	unsigned int num_top_clks;
	unsigned int num_local_clks;
	unsigned int num_asyncbridge_clks;
	unsigned int num_sys_pwr_regs;
	unsigned int num_save_list;
};

static struct exynos7420_pd_data pd_data_list[] = {
	{
		.name = "pd-g3d",
		.top_clks = top_clks_g3d,
		.local_clks = local_clks_g3d,
		.sys_pwr_regs = sys_pwr_regs_g3d,
		.save_list = save_list_g3d,
		.num_top_clks = ARRAY_SIZE(top_clks_g3d),
		.num_local_clks = ARRAY_SIZE(local_clks_g3d),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_g3d),
		.num_save_list = ARRAY_SIZE(save_list_g3d),
	}, {
		.name = "pd-cam0",
		.top_clks = top_clks_cam0,
		.local_clks = local_clks_cam0,
		.asyncbridge_clks = asyncbridge_clks_cam0,
		.sys_pwr_regs = sys_pwr_regs_cam0,
		.save_list = save_list_cam0,
		.num_top_clks = ARRAY_SIZE(top_clks_cam0),
		.num_local_clks = ARRAY_SIZE(local_clks_cam0),
		.num_asyncbridge_clks = ARRAY_SIZE(asyncbridge_clks_cam0),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_cam0),
		.num_save_list = ARRAY_SIZE(save_list_cam0),
	}, {
		.name = "pd-cam1",
		.top_clks = top_clks_cam1,
		.local_clks = local_clks_cam1,
		.asyncbridge_clks = asyncbridge_clks_cam1,
		.sys_pwr_regs = sys_pwr_regs_cam1,
		.save_list = save_list_cam1,
		.num_top_clks = ARRAY_SIZE(top_clks_cam1),
		.num_local_clks = ARRAY_SIZE(local_clks_cam1),
		.num_asyncbridge_clks = ARRAY_SIZE(asyncbridge_clks_cam1),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_cam1),
		.num_save_list = ARRAY_SIZE(save_list_cam1),
	}, {
		.name = "pd-isp0",
		.top_clks = top_clks_isp0,
		.local_clks = local_clks_isp0,
		.asyncbridge_clks = asyncbridge_clks_isp0,
		.sys_pwr_regs = sys_pwr_regs_isp0,
		.save_list = save_list_isp0,
		.num_top_clks = ARRAY_SIZE(top_clks_isp0),
		.num_local_clks = ARRAY_SIZE(local_clks_isp0),
		.num_asyncbridge_clks = ARRAY_SIZE(asyncbridge_clks_isp0),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_isp0),
		.num_save_list = ARRAY_SIZE(save_list_isp0),
	}, {
		.name = "pd-isp1",
		.top_clks = top_clks_isp1,
		.local_clks = local_clks_isp1,
		.asyncbridge_clks = asyncbridge_clks_isp1,
		.sys_pwr_regs = sys_pwr_regs_isp1,
		.save_list = save_list_isp1,
		.num_top_clks = ARRAY_SIZE(top_clks_isp1),
		.num_local_clks = ARRAY_SIZE(local_clks_isp1),
		.num_asyncbridge_clks = ARRAY_SIZE(asyncbridge_clks_isp1),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_isp1),
		.num_save_list = ARRAY_SIZE(save_list_isp1),
	}, {
		.name = "pd-vpp",
		.top_clks = top_clks_vpp,
		.local_clks = local_clks_vpp,
		.sys_pwr_regs = sys_pwr_regs_vpp,
		.save_list = save_list_vpp,
		.num_top_clks = ARRAY_SIZE(top_clks_vpp),
		.num_local_clks = ARRAY_SIZE(local_clks_vpp),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_vpp),
		.num_save_list = ARRAY_SIZE(save_list_vpp),
	}, {
		.name = "pd-disp",
		.top_clks = top_clks_disp,
		.local_clks = local_clks_disp,
		.asyncbridge_clks = asyncbridge_clks_disp,
		.sys_pwr_regs = sys_pwr_regs_disp,
		.save_list = save_list_disp,
		.num_top_clks = ARRAY_SIZE(top_clks_disp),
		.num_local_clks = ARRAY_SIZE(local_clks_disp),
		.num_asyncbridge_clks = ARRAY_SIZE(asyncbridge_clks_disp),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_disp),
		.num_save_list = ARRAY_SIZE(save_list_disp),
	}, {
		.name = "pd-aud",
		.local_clks = local_clks_aud,
		.sys_pwr_regs = sys_pwr_regs_aud,
		.save_list = save_list_aud,
		.num_local_clks = ARRAY_SIZE(local_clks_aud),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_aud),
		.num_save_list = ARRAY_SIZE(save_list_aud),
	}, {
		.name = "pd-mscl",
		.top_clks = top_clks_mscl,
		.local_clks = local_clks_mscl,
		.sys_pwr_regs = sys_pwr_regs_mscl,
		.save_list = save_list_mscl,
		.num_top_clks = ARRAY_SIZE(top_clks_mscl),
		.num_local_clks = ARRAY_SIZE(local_clks_mscl),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_mscl),
		.num_save_list = ARRAY_SIZE(save_list_mscl),
	}, {
		.name = "pd-mfc",
		.top_clks = top_clks_mfc,
		.local_clks = local_clks_mfc,
		.sys_pwr_regs = sys_pwr_regs_mfc,
		.save_list = save_list_mfc,
		.num_top_clks = ARRAY_SIZE(top_clks_mfc),
		.num_local_clks = ARRAY_SIZE(local_clks_mfc),
		.num_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_mfc),
		.num_save_list = ARRAY_SIZE(save_list_mfc),
	},
};
