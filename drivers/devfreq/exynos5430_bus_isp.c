/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include <mach/tmu.h>
#include <mach/asv-exynos.h>

#include "exynos5430_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(111000)
#define DEVFREQ_POLLING_PERIOD	(0)

#define DISP_VOLT_STEP		12500
#define COLD_VOLT_OFFSET	37500
#define LIMIT_COLD_VOLTAGE	1250000

#define CONSTRAINT_VOLT		900000

enum devfreq_isp_idx {
	LV0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV5,
	LV6,
#if defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
	LV7,
	LV8,
#endif
	LV_COUNT,
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
	MOUT_ISP_PLL,
	MOUT_BUS_PLL_USER,
	MOUT_MFC_PLL_USER,
	MOUT_ACLK_FD_A,
	MOUT_ACLK_FD_B,
	MOUT_ACLK_ISP_400,
	MOUT_ACLK_CAM1_333_USER,
	MOUT_ACLK_CAM1_400_USER,
	MOUT_ACLK_CAM1_552_USER,
	MOUT_SCLK_PIXELASYNC_LITE_C_B,
	MOUT_SCLK_PIXELASYNC_LITE_C_INIT_A,
	MOUT_SCLK_PIXELASYNC_LITE_C_INIT_B,
	MOUT_ACLK_CAM0_333_USER,
	ACLK_CAM0_333,
	MOUT_ACLK_LITE_C_B,
	MOUT_ACLK_CSIS2_B,
	MOUT_ACLK_ISP_DIS_400,
	MOUT_SCLK_LITE_FREECNT_C,
	DOUT_PCLK_LITE_D,
	DOUT_PCLK_CAM1_83,
	MOUT_ACLK_CSIS0_A,
	MOUT_ACLK_CSIS0_B,
	MOUT_ACLK_CSIS1_A,
	MOUT_ACLK_CSIS1_B,
	MOUT_ACLK_LITE_A_A,
	MOUT_ACLK_LITE_A_B,
	MOUT_ACLK_LITE_B_A,
	MOUT_ACLK_LITE_B_B,
	MOUT_ACLK_3AA0_A,
	MOUT_ACLK_3AA0_B,
	MOUT_ACLK_3AA1_A,
	MOUT_ACLK_3AA1_B,
	MOUT_ACLK_LITE_D_A,
	MOUT_ACLK_LITE_D_B,
	MOUT_ACLK_CAM1_333,
	MOUT_ACLK_CSIS2_A,
	MOUT_ACLK_LITE_C_A,
	MOUT_ACLK_CAM0_552_USER,
	MOUT_ACLK_CAM0_400_USER,
	MOUT_ACLK_CAM0_400,
	CLK_COUNT,
};

struct devfreq_data_isp {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_isp;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	unsigned int use_dvfs;

	struct notifier_block tmu_notifier;
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
	{"mout_sclk_pixelasync_lite_c_init_a",},
	{"mout_sclk_pixelasync_lite_c_init_b",},
	{"mout_aclk_cam0_333_user",},
	{"aclk_cam0_333",},
	{"mout_aclk_lite_c_b",},
	{"mout_aclk_csis2_b",},
	{"mout_aclk_isp_dis_400",},
	{"mout_sclk_lite_freecnt_c",},
	{"dout_pclk_lite_d",},
	{"dout_pclk_cam1_83",},
	{"mout_aclk_csis0_a",},
	{"mout_aclk_csis0_b",},
	{"mout_aclk_csis1_a",},
	{"mout_aclk_csis1_b",},
	{"mout_aclk_lite_a_a",},
	{"mout_aclk_lite_a_b",},
	{"mout_aclk_lite_b_a",},
	{"mout_aclk_lite_b_b",},
	{"mout_aclk_3aa0_a",},
	{"mout_aclk_3aa0_b",},
	{"mout_aclk_3aa1_a",},
	{"mout_aclk_3aa1_b",},
	{"mout_aclk_lite_d_a",},
	{"mout_aclk_lite_d_b",},
	{"mout_aclk_cam1_333",},
	{"mout_aclk_csis2_a",},
	{"mout_aclk_lite_c_a",},
	{"mout_aclk_cam0_552_user",},
	{"mout_aclk_cam0_400_user",},
	{"mout_aclk_cam0_400",},
};

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_opp_table devfreq_isp_opp_list[] = {
	{LV0,	466000, 950000},
	{LV1,	450000, 950000},
	{LV2,	447000,	950000},
	{LV3,	444000,	950000},
	{LV4,	333000,	950000},
	{LV5,	222000,	950000},
	{LV6,	111000,	950000},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_opp_table devfreq_isp_opp_list[] = {
	{LV0,	777000, 950000},
	{LV1,	666000,	950000},
	{LV2,	555000,	950000},
	{LV3,	466000,	950000},
	{LV4,	455000,	950000},
	{LV5,	444000,	950000},
	{LV6,	333000, 950000},
	{LV7,	222000,	925000},
	{LV8,	111000,	925000},
};
#endif

struct devfreq_clk_state mux_sclk_pixelasync_lite_c[] = {
	{MOUT_SCLK_PIXELASYNC_LITE_C_B,	MOUT_ACLK_CAM0_333_USER},
};

struct devfreq_clk_state mux_sclk_lite_freecnt_c[] = {
	{MOUT_SCLK_LITE_FREECNT_C,	DOUT_PCLK_LITE_D},
};

struct devfreq_clk_state aclk_cam1_552_isp_pll[] = {
	{MOUT_ACLK_CAM1_552_A,	MOUT_ISP_PLL},
	{MOUT_ACLK_CAM1_552_B,	MOUT_ACLK_CAM1_552_A},
};

struct devfreq_clk_state aclk_cam1_552_bus_pll[] = {
	{MOUT_ACLK_CAM1_552_A,	MOUT_BUS_PLL_USER},
	{MOUT_ACLK_CAM1_552_B,	MOUT_ACLK_CAM1_552_A},
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
	{MOUT_ACLK_ISP_400,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_state aclk_isp_400_mfc_pll[] = {
	{MOUT_ACLK_ISP_400,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state mux_aclk_isp_dis_400_bus_pll[] = {
	{MOUT_ACLK_ISP_DIS_400,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_state mux_aclk_isp_dis_400_mfc_pll[] = {
	{MOUT_ACLK_ISP_DIS_400,	MOUT_MFC_PLL_USER},
};

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_state aclk_cam0_bus_400_bus_pll[] = {
	{MOUT_ACLK_CAM0_400, MOUT_ACLK_CAM0_400_USER},
};

struct devfreq_clk_state aclk_cam0_bus_400_mfc_pll[] = {
	{MOUT_ACLK_CAM0_400, MOUT_ACLK_CAM0_333_USER},
};

struct devfreq_clk_state aclk_csis0_bus_pll[] = {
	{MOUT_ACLK_CSIS0_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_CSIS0_B,	MOUT_ACLK_CSIS0_A},
};

struct devfreq_clk_state aclk_lite_a_bus_pll[] = {
	{MOUT_ACLK_LITE_A_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_LITE_A_B,	MOUT_ACLK_LITE_A_A},
};

struct devfreq_clk_state aclk_3aa0_bus_pll[] = {
	{MOUT_ACLK_3AA0_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_3AA0_B,	MOUT_ACLK_3AA0_A},
};

struct devfreq_clk_state aclk_csis1_bus_pll[] = {
	{MOUT_ACLK_CSIS1_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_CSIS1_B,	MOUT_ACLK_CSIS1_A},
};

struct devfreq_clk_state aclk_lite_b_bus_pll[] = {
	{MOUT_ACLK_LITE_B_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_LITE_B_B,	MOUT_ACLK_LITE_B_A},
};

struct devfreq_clk_state aclk_3aa1_bus_pll[] = {
	{MOUT_ACLK_3AA1_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_3AA1_B,	MOUT_ACLK_3AA1_A},
};

struct devfreq_clk_state aclk_lite_d_bus_pll[] = {
	{MOUT_ACLK_LITE_D_A,	MOUT_ACLK_CAM0_400_USER},
	{MOUT_ACLK_LITE_D_B,	MOUT_ACLK_LITE_D_A},
};

struct devfreq_clk_state sclk_pixel_init_bus_pll[] = {
	{MOUT_SCLK_PIXELASYNC_LITE_C_INIT_A, MOUT_ACLK_CAM0_400_USER},
	{MOUT_SCLK_PIXELASYNC_LITE_C_INIT_B, MOUT_SCLK_PIXELASYNC_LITE_C_INIT_A},
};

struct devfreq_clk_state aclk_cam1_333_mfc_pll[] = {
	{MOUT_ACLK_CAM1_333,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state aclk_cam1_333_bus_pll[] = {
	{MOUT_ACLK_CAM1_333,	MOUT_BUS_PLL_USER},
};
#endif

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

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_states aclk_cam0_bus_400_bus_pll_list = {
	.state = aclk_cam0_bus_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bus_400_bus_pll),
};

struct devfreq_clk_states aclk_cam0_bus_400_mfc_pll_list = {
	.state = aclk_cam0_bus_400_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam0_bus_400_mfc_pll),
};

struct devfreq_clk_states aclk_csis0_bus_pll_list = {
	.state = aclk_csis0_bus_pll,
	.state_count = ARRAY_SIZE(aclk_csis0_bus_pll),
};

struct devfreq_clk_states aclk_lite_a_bus_pll_list = {
	.state = aclk_lite_a_bus_pll,
	.state_count = ARRAY_SIZE(aclk_lite_a_bus_pll),
};

struct devfreq_clk_states aclk_3aa0_bus_pll_list = {
	.state = aclk_3aa0_bus_pll,
	.state_count = ARRAY_SIZE(aclk_3aa0_bus_pll),
};

struct devfreq_clk_states aclk_csis1_bus_pll_list = {
	.state = aclk_csis1_bus_pll,
	.state_count = ARRAY_SIZE(aclk_csis1_bus_pll),
};

struct devfreq_clk_states aclk_lite_b_bus_pll_list = {
	.state = aclk_lite_b_bus_pll,
	.state_count = ARRAY_SIZE(aclk_lite_b_bus_pll),
};

struct devfreq_clk_states aclk_3aa1_bus_pll_list = {
	.state = aclk_3aa1_bus_pll,
	.state_count = ARRAY_SIZE(aclk_3aa1_bus_pll),
};

struct devfreq_clk_states aclk_lite_d_bus_pll_list = {
	.state = aclk_lite_d_bus_pll,
	.state_count = ARRAY_SIZE(aclk_lite_d_bus_pll),
};

struct devfreq_clk_states sclk_pixel_init_bus_pll_list = {
	.state = sclk_pixel_init_bus_pll,
	.state_count = ARRAY_SIZE(sclk_pixel_init_bus_pll),
};

struct devfreq_clk_states aclk_cam1_333_mfc_pll_list = {
	.state = aclk_cam1_333_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_333_mfc_pll),
};

struct devfreq_clk_states aclk_cam1_333_bus_pll_list = {
	.state = aclk_cam1_333_bus_pll,
	.state_count = ARRAY_SIZE(aclk_cam1_333_bus_pll),
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info isp_pll[] = {
	{LV0,	480000000,	0,	NULL},
	{LV1,	480000000,	0,	NULL},
	{LV2,	480000000,	0,	NULL},
	{LV3,	480000000,	0,	NULL},
	{LV4,	480000000,	0,	NULL},
	{LV5,	480000000,	0,	NULL},
	{LV6,	480000000,	0,	NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info isp_pll[] = {
	{LV0,	552000000,	0,	NULL},
	{LV1,	552000000,	0,	NULL},
	{LV2,	552000000,	0,	NULL},
	{LV3,	552000000,	0,	NULL},
	{LV4,	552000000,	0,	NULL},
	{LV5,	552000000,	0,	NULL},
	{LV6,	552000000,	0,	NULL},
	{LV7,	552000000,	0,	NULL},
	{LV8,	552000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_cam0_552[] = {
	{LV0,	 60000000,	0,	NULL},
	{LV1,	 60000000,	0,	NULL},
	{LV2,	 60000000,	0,	NULL},
	{LV3,	 60000000,	0,	NULL},
	{LV4,	 60000000,	0,	NULL},
	{LV5,	 60000000,	0,	NULL},
	{LV6,	 60000000,	0,	NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_cam0_552[] = {
	{LV0,	552000000,	0,	NULL},
	{LV1,	552000000,	0,	NULL},
	{LV2,	552000000,	0,	NULL},
	{LV3,	552000000,	0,	NULL},
	{LV4,	552000000,	0,	NULL},
	{LV5,	552000000,	0,	NULL},
	{LV6,	552000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_cam0_400[] = {
	{LV0,	400000000,	0,	NULL},
	{LV1,	400000000,	0,	NULL},
	{LV2,	400000000,	0,	NULL},
	{LV3,	400000000,	0,	NULL},
	{LV4,	400000000,	0,	NULL},
	{LV5,	134000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_cam0_400[] = {
	{LV0,	400000000,	0,	NULL},
	{LV1,	400000000,	0,	NULL},
	{LV2,	400000000,	0,	NULL},
	{LV3,	400000000,	0,	NULL},
	{LV4,	400000000,	0,	NULL},
	{LV5,	400000000,	0,	NULL},
	{LV6,	400000000,	0,	NULL},
	{LV7,	134000000,	0,	NULL},
	{LV8,	134000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_cam0_333[] = {
	{LV0,	317000000,	0,	NULL},
	{LV1,	317000000,	0,	NULL},
	{LV2,	317000000,	0,	NULL},
	{LV3,	317000000,	0,	NULL},
	{LV4,	317000000,	0,	NULL},
	{LV5,	 80000000,	0,	NULL},
	{LV6,	 80000000,	0,	NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_cam0_333[] = {
	{LV0,	317000000,	0,	NULL},
	{LV1,	317000000,	0,	NULL},
	{LV2,	317000000,	0,	NULL},
	{LV3,	317000000,	0,	NULL},
	{LV4,	317000000,	0,	NULL},
	{LV5,	317000000,	0,	NULL},
	{LV6,	317000000,	0,	NULL},
	{LV7,	 80000000,	0,	NULL},
	{LV8,	 80000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_cam0_bus_400[] = {
	{LV0,	400000000,	0,	&aclk_cam0_bus_400_bus_pll_list},
	{LV1,	317000000,	0,	&aclk_cam0_bus_400_mfc_pll_list},
	{LV2,	317000000,	0,	&aclk_cam0_bus_400_mfc_pll_list},
	{LV3,	317000000,	0,	&aclk_cam0_bus_400_mfc_pll_list},
	{LV4,	317000000,	0,	&aclk_cam0_bus_400_mfc_pll_list},
	{LV5,	134000000,	0,	&aclk_cam0_bus_400_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_cam0_bus_400_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_cam0_bus_400[] = {
	{LV0,	400000000,	0,	NULL},
	{LV1,	400000000,	0,	NULL},
	{LV2,	400000000,	0,	NULL},
	{LV3,	400000000,	0,	NULL},
	{LV4,	400000000,	0,	NULL},
	{LV5,	400000000,	0,	NULL},
	{LV6,	400000000,	0,	NULL},
	{LV7,	134000000,	0,	NULL},
	{LV8,	134000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_csis0[] = {
	{LV0,	400000000,	0,	&aclk_csis0_bus_pll_list},
	{LV1,	400000000,	0,	&aclk_csis0_bus_pll_list},
	{LV2,	400000000,	0,	&aclk_csis0_bus_pll_list},
	{LV3,	400000000,	0,	&aclk_csis0_bus_pll_list},
	{LV4,	400000000,	0,	&aclk_csis0_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_csis0_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_csis0_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_csis0[] = {
	{LV0,	552000000,	0,	NULL},
	{LV1,	552000000,	0,	NULL},
	{LV2,	552000000,	0,	NULL},
	{LV3,	552000000,	0,	NULL},
	{LV4,	552000000,	0,	NULL},
	{LV5,	552000000,	0,	NULL},
	{LV6,	552000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_lite_a[] = {
	{LV0,	400000000,	0,	&aclk_lite_a_bus_pll_list},
	{LV1,	400000000,	0,	&aclk_lite_a_bus_pll_list},
	{LV2,	400000000,	0,	&aclk_lite_a_bus_pll_list},
	{LV3,	400000000,	0,	&aclk_lite_a_bus_pll_list},
	{LV4,	400000000,	0,	&aclk_lite_a_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_lite_a_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_lite_a_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_lite_a[] = {
	{LV0,	552000000,	0,	NULL},
	{LV1,	552000000,	0,	NULL},
	{LV2,	552000000,	0,	NULL},
	{LV3,	552000000,	0,	NULL},
	{LV4,	552000000,	0,	NULL},
	{LV5,	552000000,	0,	NULL},
	{LV6,	552000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_3aa0[] = {
	{LV0,	400000000,	0,	&aclk_3aa0_bus_pll_list},
	{LV1,	400000000,	0,	&aclk_3aa0_bus_pll_list},
	{LV2,	400000000,	0,	&aclk_3aa0_bus_pll_list},
	{LV3,	400000000,	0,	&aclk_3aa0_bus_pll_list},
	{LV4,	400000000,	0,	&aclk_3aa0_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_3aa0_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_3aa0_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_3aa0[] = {
	{LV0,	552000000,	0,	NULL},
	{LV1,	552000000,	0,	NULL},
	{LV2,	552000000,	0,	NULL},
	{LV3,	552000000,	0,	NULL},
	{LV4,	552000000,	0,	NULL},
	{LV5,	276000000,	0,	NULL},
	{LV6,	276000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_csis1[] = {
	{LV0,	 50000000,	0,	&aclk_csis1_bus_pll_list},
	{LV1,	 50000000,	0,	&aclk_csis1_bus_pll_list},
	{LV2,	 50000000,	0,	&aclk_csis1_bus_pll_list},
	{LV3,	 50000000,	0,	&aclk_csis1_bus_pll_list},
	{LV4,	 50000000,	0,	&aclk_csis1_bus_pll_list},
	{LV5,	 17000000,	0,	&aclk_csis1_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_csis1_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_csis1[] = {
	{LV0,	 69000000,	0,	NULL},
	{LV1,	 69000000,	0,	NULL},
	{LV2,	 69000000,	0,	NULL},
	{LV3,	 69000000,	0,	NULL},
	{LV4,	 69000000,	0,	NULL},
	{LV5,	 69000000,	0,	NULL},
	{LV6,	 69000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_lite_b[] = {
	{LV0,	 50000000,	0,	&aclk_lite_b_bus_pll_list},
	{LV1,	 50000000,	0,	&aclk_lite_b_bus_pll_list},
	{LV2,	 50000000,	0,	&aclk_lite_b_bus_pll_list},
	{LV3,	 50000000,	0,	&aclk_lite_b_bus_pll_list},
	{LV4,	 50000000,	0,	&aclk_lite_b_bus_pll_list},
	{LV5,	 17000000,	0,	&aclk_lite_b_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_lite_b_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_lite_b[] = {
	{LV0,	 69000000,	0,	NULL},
	{LV1,	 69000000,	0,	NULL},
	{LV2,	 69000000,	0,	NULL},
	{LV3,	 69000000,	0,	NULL},
	{LV4,	 69000000,	0,	NULL},
	{LV5,	 69000000,	0,	NULL},
	{LV6,	 69000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_3aa1[] = {
	{LV0,	400000000,	0,	&aclk_3aa1_bus_pll_list},
	{LV1,	200000000,	0,	&aclk_3aa1_bus_pll_list},
	{LV2,	200000000,	0,	&aclk_3aa1_bus_pll_list},
	{LV3,	200000000,	0,	&aclk_3aa1_bus_pll_list},
	{LV4,	200000000,	0,	&aclk_3aa1_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_3aa1_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_3aa1_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_3aa1[] = {
	{LV0,	552000000,	0,	NULL},
	{LV1,	552000000,	0,	NULL},
	{LV2,	552000000,	0,	NULL},
	{LV3,	552000000,	0,	NULL},
	{LV4,	552000000,	0,	NULL},
	{LV5,	 92000000,	0,	NULL},
	{LV6,	 92000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_lite_d[] = {
	{LV0,	134000000,	0,	&aclk_lite_d_bus_pll_list},
	{LV1,	134000000,	0,	&aclk_lite_d_bus_pll_list},
	{LV2,	134000000,	0,	&aclk_lite_d_bus_pll_list},
	{LV3,	134000000,	0,	&aclk_lite_d_bus_pll_list},
	{LV4,	134000000,	0,	&aclk_lite_d_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_lite_d_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_lite_d_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_lite_d[] = {
	{LV0,	138000000,	0,	NULL},
	{LV1,	138000000,	0,	NULL},
	{LV2,	138000000,	0,	NULL},
	{LV3,	138000000,	0,	NULL},
	{LV4,	138000000,	0,	NULL},
	{LV5,	138000000,	0,	NULL},
	{LV6,	138000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info sclk_pixel_init_552[] = {
	{LV0,	 50000000,	0,	&sclk_pixel_init_bus_pll_list},
	{LV1,	 50000000,	0,	&sclk_pixel_init_bus_pll_list},
	{LV2,	 50000000,	0,	&sclk_pixel_init_bus_pll_list},
	{LV3,	 50000000,	0,	&sclk_pixel_init_bus_pll_list},
	{LV4,	 50000000,	0,	&sclk_pixel_init_bus_pll_list},
	{LV5,	 17000000,	0,	&sclk_pixel_init_bus_pll_list},
	{LV6,	 13000000,	0,	&sclk_pixel_init_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info sclk_pixel_init_552[] = {
	{LV0,	 69000000,	0,	NULL},
	{LV1,	 69000000,	0,	NULL},
	{LV2,	 69000000,	0,	NULL},
	{LV3,	 69000000,	0,	NULL},
	{LV4,	 69000000,	0,	NULL},
	{LV5,	 69000000,	0,	NULL},
	{LV6,	 69000000,	0,	NULL},
	{LV7,	 92000000,	0,	NULL},
	{LV8,	 79000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info sclk_pixel_333[] = {
	{LV0,	 40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV1,	 40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV2,	 40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV3,	 40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV4,	 40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV5,	 10000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV6,	 10000000,	0,	&sclk_pixelasync_lite_c_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info sclk_pixel_333[] = {
	{LV0,	  40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV1,	  40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV2,	  40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV3,	  40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV4,	  40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV5,	  40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV6,	  40000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV7,	  79000000,	0,	&sclk_pixelasync_lite_c_list},
	{LV8,	  79000000,	0,	&sclk_pixelasync_lite_c_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_cam1_552[] = {
	{LV0,	480000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV1,	480000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV2,	480000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV3,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
	{LV4,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
	{LV5,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_cam1_552_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_cam1_552[] = {
	{LV0,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV1,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV2,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV3,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV4,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV5,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV6,	552000000,	0,	&aclk_cam1_552_isp_pll_list},
	{LV7,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
	{LV8,	400000000,	0,	&aclk_cam1_552_bus_pll_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_cam1_400[] = {
	{LV0,	400000000,	0,	NULL},
	{LV1,	400000000,	0,	NULL},
	{LV2,	267000000,	0,	NULL},
	{LV3,	160000000,	0,	NULL},
	{LV4,	160000000,	0,	NULL},
	{LV5,	160000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_cam1_400[] = {
	{LV0,	400000000,	0,	NULL},
	{LV1,	400000000,	0,	NULL},
	{LV2,	400000000,	0,	NULL},
	{LV3,	400000000,	0,	NULL},
	{LV4,	400000000,	0,	NULL},
	{LV5,	400000000,	0,	NULL},
	{LV6,	400000000,	0,	NULL},
	{LV7,	267000000,	0,	NULL},
	{LV8,	267000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_cam1_333[] = {
	{LV0,	267000000,	0,	&aclk_cam1_333_bus_pll_list},
	{LV1,	317000000,	0,	&aclk_cam1_333_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_cam1_333_bus_pll_list},
	{LV3,	160000000,	0,	&aclk_cam1_333_bus_pll_list},
	{LV4,	160000000,	0,	&aclk_cam1_333_bus_pll_list},
	{LV5,	160000000,	0,	&aclk_cam1_333_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_cam1_333_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_cam1_333[] = {
	{LV0,	317000000,	0,	NULL},
	{LV1,	317000000,	0,	NULL},
	{LV2,	317000000,	0,	NULL},
	{LV3,	317000000,	0,	NULL},
	{LV4,	317000000,	0,	NULL},
	{LV5,	317000000,	0,	NULL},
	{LV6,	317000000,	0,	NULL},
	{LV7,	159000000,	0,	NULL},
	{LV8,	159000000,	0,	NULL},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_fd_400[] = {
	{LV0,	267000000,	0,	&aclk_fd_400_bus_pll_list},
	{LV1,	317000000,	0,	&aclk_fd_400_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_fd_400_bus_pll_list},
	{LV3,	160000000,	0,	&aclk_fd_400_bus_pll_list},
	{LV4,	 80000000,	0,	&aclk_fd_400_bus_pll_list},
	{LV5,	160000000,	0,	&aclk_fd_400_bus_pll_list},
	{LV6,	 13000000,	0,	&aclk_fd_400_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_fd_400[] = {
	{LV0,	400000000,	0,	&aclk_fd_400_bus_pll_list},
	{LV1,	400000000,	0,	&aclk_fd_400_bus_pll_list},
	{LV2,	317000000,	0,	&aclk_fd_400_mfc_pll_list},
	{LV3,	317000000,	0,	&aclk_fd_400_mfc_pll_list},
	{LV4,	159000000,	0,	&aclk_fd_400_mfc_pll_list},
	{LV5,	317000000,	0,	&aclk_fd_400_mfc_pll_list},
	{LV6,	159000000,	0,	&aclk_fd_400_mfc_pll_list},
	{LV7,	159000000,	0,	&aclk_fd_400_mfc_pll_list},
	{LV8,	 80000000,	0,	&aclk_fd_400_mfc_pll_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_csis2_333[] = {
	{LV0,	134000000,	0,	&mux_aclk_csis2_list},
	{LV1,	159000000,	0,	&mux_aclk_csis2_list},
	{LV2,	134000000,	0,	&mux_aclk_csis2_list},
	{LV3,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV4,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV5,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV6,	 13000000,	0,	&mux_aclk_csis2_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_csis2_333[] = {
	{LV0,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV1,	 40000000,	0,	&mux_aclk_csis2_list},
	{LV2,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV3,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV4,	 40000000,	0,	&mux_aclk_csis2_list},
	{LV5,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV6,	 40000000,	0,	&mux_aclk_csis2_list},
	{LV7,	 80000000,	0,	&mux_aclk_csis2_list},
	{LV8,	 40000000,	0,	&mux_aclk_csis2_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_lite_c[] = {
	{LV0,	134000000,	0,	&mux_aclk_lite_c_list},
	{LV1,	159000000,	0,	&mux_aclk_lite_c_list},
	{LV2,	134000000,	0,	&mux_aclk_lite_c_list},
	{LV3,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV4,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV5,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV6,	 13000000,	0,	&mux_aclk_lite_c_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_lite_c[] = {
	{LV0,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV1,	 40000000,	0,	&mux_aclk_lite_c_list},
	{LV2,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV3,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV4,	 40000000,	0,	&mux_aclk_lite_c_list},
	{LV5,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV6,	 40000000,	0,	&mux_aclk_lite_c_list},
	{LV7,	 80000000,	0,	&mux_aclk_lite_c_list},
	{LV8,	 40000000,	0,	&mux_aclk_lite_c_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_isp_400[] = {
	{LV0,	317000000,	0,	&aclk_isp_400_mfc_pll_list},
	{LV1,	317000000,	0,	&aclk_isp_400_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV3,	160000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV4,	 80000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV5,	160000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_isp_400_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_isp_400[] = {
	{LV0,	400000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV1,	400000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV2,	317000000,	0,	&aclk_isp_400_mfc_pll_list},
	{LV3,	267000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV4,	159000000,	0,	&aclk_isp_400_mfc_pll_list},
	{LV5,	267000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV6,	159000000,	0,	&aclk_isp_400_mfc_pll_list},
	{LV7,	159000000,	0,	&aclk_isp_400_mfc_pll_list},
	{LV8,	159000000,	0,	&aclk_isp_400_mfc_pll_list},
};
#endif

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info aclk_isp_dis_400[] = {
	{LV0,	317000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{LV1,	317000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV3,	160000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV4,	 80000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV5,	160000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_isp_dis_400_bus_pll_list},
};
#elif defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
struct devfreq_clk_info aclk_isp_dis_400[] = {
	{LV0,	400000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV1,	400000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV2,	317000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{LV3,	267000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV4,	159000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{LV5,	267000000,	0,	&aclk_isp_dis_400_bus_pll_list},
	{LV6,	159000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{LV7,	159000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
	{LV8,	159000000,	0,	&aclk_isp_dis_400_mfc_pll_list},
};
#endif

struct devfreq_clk_info sclk_lite_freecnt_c[] = {
	{LV0,	 0,	0,	&sclk_lite_freecnt_c_list},
	{LV1,	 0,	0,	&sclk_lite_freecnt_c_list},
	{LV2,	 0,	0,	&sclk_lite_freecnt_c_list},
	{LV3,	 0,	0,	&sclk_lite_freecnt_c_list},
	{LV4,	 0,	0,	&sclk_lite_freecnt_c_list},
	{LV5,	 0,	0,	&sclk_lite_freecnt_c_list},
	{LV6,	 0,	0,	&sclk_lite_freecnt_c_list},
#if defined(CONFIG_EXYNOS5430_FHD) || defined(CONFIG_EXYNOS5430_WQHD)
	{LV7,	 0,	0,	&sclk_lite_freecnt_c_list},
	{LV8,	 0,	0,	&sclk_lite_freecnt_c_list},
#endif
};

#if defined(CONFIG_EXYNOS5430_HD)
struct devfreq_clk_info pclk_cam1_83[] = {
	{LV0,	 6700000,	0,	NULL},
	{LV1,	 8000000,	0,	NULL},
	{LV2,	 6700000,	0,	NULL},
	{LV3,	 8000000,	0,	NULL},
	{LV4,	 8000000,	0,	NULL},
	{LV5,	 8000000,	0,	NULL},
	{LV6,	 5000000,	0,	NULL},
};
#endif

struct devfreq_clk_info *devfreq_clk_isp_info_list[] = {
	isp_pll,
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
	aclk_cam1_552,
	aclk_cam1_400,
	aclk_cam1_333,
	aclk_fd_400,
	aclk_csis2_333,
	aclk_lite_c,
	aclk_isp_400,
	aclk_isp_dis_400,
	sclk_lite_freecnt_c,
#if defined(CONFIG_EXYNOS5430_HD)
	pclk_cam1_83,
#endif
};

enum devfreq_isp_clk devfreq_clk_isp_info_idx[] = {
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
	MOUT_SCLK_LITE_FREECNT_C,
#if defined(CONFIG_EXYNOS5430_HD)
	DOUT_PCLK_CAM1_83,
#endif
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_isp_pm_domain[] = {
	{NULL,},
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
	{"pd-isp",},
	{"pd-isp",},
	{"pd-cam0",},
#if defined(CONFIG_EXYNOS5430_HD)
	{"pd-cam1"},
#endif
};
#endif

static struct devfreq_simple_ondemand_data exynos5_devfreq_isp_governor_data = {
	.pm_qos_class		= PM_QOS_CAM_THROUGHPUT,
	.upthreshold		= 95,
#if defined(CONFIG_EXYNOS5430_HD)
	.cal_qos_max		= 466000,
#else
	.cal_qos_max		= 777000,
#endif
};

static struct exynos_devfreq_platdata exynos5430_qos_isp = {
	.default_qos		= 111000,
};

static struct pm_qos_request exynos5_isp_qos;
static struct pm_qos_request boot_isp_qos;
static struct pm_qos_request min_isp_thermal_qos;
static struct devfreq_data_isp *data_isp;

extern int exynos5_int_check_voltage_constraint(unsigned long isp_voltage);

static inline int exynos5_devfreq_isp_get_idx(struct devfreq_opp_table *table,
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
	cur_freq_idx = exynos5_devfreq_isp_get_idx(devfreq_isp_opp_list,
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

static int exynos5_devfreq_isp_set_freq(struct devfreq_data_isp *data,
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
#if defined(CONFIG_EXYNOS5430_HD)
		if (target_idx != LV0 && old_idx == LV0)
			clk_prepare_enable(devfreq_isp_clk[ACLK_CAM0_333].clk);
#endif

		for (i = 0; i < ARRAY_SIZE(devfreq_clk_isp_info_list); ++i) {
			clk_info = &devfreq_clk_isp_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_isp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
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
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}

#if defined(CONFIG_EXYNOS5430_HD)
		if (target_idx == LV0 && old_idx != LV0)
			clk_disable_unprepare(devfreq_isp_clk[ACLK_CAM0_333].clk);
#endif
	}

	return 0;
}

static int exynos5_devfreq_isp_set_volt(struct devfreq_data_isp *data,
		unsigned long volt,
		unsigned long volt_range,
		bool tolower)
{
	if (data->old_volt == volt)
		goto out;

	if (!tolower && (volt >= CONSTRAINT_VOLT))
		exynos5_int_check_voltage_constraint(volt);

	regulator_set_voltage(data->vdd_isp, volt, volt_range);
	data->old_volt = volt;

	if (volt < CONSTRAINT_VOLT)
		exynos5_int_check_voltage_constraint(0);
	else if (tolower && (volt >= CONSTRAINT_VOLT))
		exynos5_int_check_voltage_constraint(volt);
out:
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
static unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	return voltage + volt_offset;
}
#endif

static int exynos5_devfreq_isp_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_isp *isp_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_isp = isp_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&isp_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		mutex_unlock(&isp_data->lock);
		dev_err(dev, "DEVFREQ(ISP) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, isp_data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_isp_get_idx(devfreq_isp_opp_list,
						ARRAY_SIZE(devfreq_isp_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_isp_get_idx(devfreq_isp_opp_list,
						ARRAY_SIZE(devfreq_isp_opp_list),
						devfreq_isp->previous_freq);
	old_freq = devfreq_isp->previous_freq;

	if (target_idx < 0 ||
		old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_devfreq_isp_set_volt(isp_data, target_volt, target_volt + VOLT_STEP, false);
		exynos5_devfreq_isp_set_freq(isp_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_isp_set_freq(isp_data, target_idx, old_idx);
		exynos5_devfreq_isp_set_volt(isp_data, target_volt, target_volt + VOLT_STEP, true);
	}
out:
	mutex_unlock(&isp_data->lock);

	return ret;
}

static int exynos5_devfreq_isp_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_isp *data = dev_get_drvdata(dev);

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = 0;
	stat->total_time = 1;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_isp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_isp_target,
	.get_dev_status	= exynos5_devfreq_isp_get_dev_status,
	.max_state	= LV_COUNT,
};

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
		devfreq_isp_clk[i].clk = __clk_lookup(devfreq_isp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_isp_clk[i].clk)) {
			pr_err("DEVFREQ(ISP) : %s can't get clock\n", devfreq_isp_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int exynos5_init_isp_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_opp_list); ++i) {
		freq = devfreq_isp_opp_list[i].freq;
		volt = get_match_volt(ID_ISP, freq);

		if (!volt)
			volt = devfreq_isp_opp_list[i].volt;

		exynos5_devfreq_isp_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(ISP) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(ISP) : %uKhz, %uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos5_devfreq_isp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_isp_qos, exynos5_devfreq_isp_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_isp_reboot_notifier = {
	.notifier_call = exynos5_devfreq_isp_reboot_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_isp_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_isp *data = container_of(nb, struct devfreq_data_isp, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_isp_thermal_qos))
			pm_qos_update_request(&min_isp_thermal_qos,
					exynos5_devfreq_isp_profile.initial_freq);

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
			pm_qos_update_request(&min_isp_thermal_qos,
					exynos5430_qos_isp.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static int exynos5_devfreq_isp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_isp *data;
	struct exynos_devfreq_platdata *plat_data;
	struct opp *target_opp;
	unsigned long freq;
	unsigned long volt;

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

	data = kzalloc(sizeof(struct devfreq_data_isp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5_devfreq_isp_profile.freq_table = kzalloc(sizeof(int) * LV_COUNT, GFP_KERNEL);
	if (exynos5_devfreq_isp_profile.freq_table == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_isp_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	data_isp = data;
	mutex_init(&data->lock);

	data->volt_offset = 0;
	data->dev = &pdev->dev;
	data->vdd_isp = regulator_get(NULL, "vdd_disp_cam0");
	freq = DEVFREQ_INITIAL_FREQ;
	rcu_read_lock();
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(ISP) : Invalid OPP to set voltage");
		ret = PTR_ERR(target_opp);
		goto err_inittable;
	}
	volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	volt = get_limit_voltage(volt, data->volt_offset);
#endif
	rcu_read_unlock();
	exynos5_devfreq_isp_set_volt(data, volt, volt + VOLT_STEP, false);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_isp_profile,
						"simple_ondemand",
						&exynos5_devfreq_isp_governor_data);
	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_isp_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_isp_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_isp_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_inittable:
	devfreq_remove_device(data->devfreq);
	kfree(exynos5_devfreq_isp_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_isp_remove(struct platform_device *pdev)
{
	struct devfreq_data_isp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_isp_thermal_qos);
	pm_qos_remove_request(&exynos5_isp_qos);
	pm_qos_remove_request(&boot_isp_qos);

	regulator_put(data->vdd_isp);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_isp_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_isp_qos))
		pm_qos_update_request(&exynos5_isp_qos, exynos5_devfreq_isp_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_isp_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_isp_qos))
		pm_qos_update_request(&exynos5_isp_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_isp_pm = {
	.suspend	= exynos5_devfreq_isp_suspend,
	.resume		= exynos5_devfreq_isp_resume,
};

static struct platform_driver exynos5_devfreq_isp_driver = {
	.probe	= exynos5_devfreq_isp_probe,
	.remove	= exynos5_devfreq_isp_remove,
	.driver	= {
		.name	= "exynos5-devfreq-isp",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_isp_pm,
	},
};

static struct platform_device exynos5_devfreq_isp_device = {
	.name	= "exynos5-devfreq-isp",
	.id	= -1,
};

static int exynos5_devfreq_isp_qos_init(void)
{
	pm_qos_add_request(&exynos5_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos5430_qos_isp.default_qos);
	pm_qos_add_request(&min_isp_thermal_qos, PM_QOS_CAM_THROUGHPUT, exynos5430_qos_isp.default_qos);
	pm_qos_add_request(&boot_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos5430_qos_isp.default_qos);

	return 0;
}
device_initcall(exynos5_devfreq_isp_qos_init);

static int __init exynos5_devfreq_isp_init(void)
{
	int ret;

	exynos5_devfreq_isp_device.dev.platform_data = &exynos5430_qos_isp;

	ret = platform_device_register(&exynos5_devfreq_isp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_isp_driver);
}
late_initcall(exynos5_devfreq_isp_init);

static void __exit exynos5_devfreq_isp_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_isp_driver);
	platform_device_unregister(&exynos5_devfreq_isp_device);
}
module_exit(exynos5_devfreq_isp_exit);
