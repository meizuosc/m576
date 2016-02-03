/*
 * Cal header file for Exynos Generic power domain.
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

/* ########################
   ##### BLK_G3D info #####
   ######################## */

static struct exynos_pd_clk top_clks_g3d[] = {
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOPC1,		.bit_offset = 9, },
};

static struct exynos_pd_clk local_clks_g3d[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_G3D,		.bit_offset = 1, },
};

static struct exynos_pd_reg sys_pwr_regs_g3d[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_G3D_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_G3D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_g3d[] = {
	{ .reg = EXYNOS7420_DIV_G3D, },
	{ .reg = G3D_LOCK },
	{ .reg = G3D_CON },
	{ .reg = G3D_CON1 },
	{ .reg = G3D_CON2 },
	{ .reg = EXYNOS7420_MUX_SEL_G3D },
	{ .reg = EXYNOS7420_ENABLE_ACLK_G3D },
	{ .reg = EXYNOS7420_ENABLE_PCLK_G3D },
	{ .reg = EXYNOS7420_ENABLE_SCLK_G3D },
};

/* #########################
   ##### BLK_CAM0 info #####
   ######################### */

static struct exynos_pd_clk top_clks_cam0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 12, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP05,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP06,		.bit_offset = 16, },
};

static struct exynos_pd_clk local_clks_cam0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 18, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 17, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 10, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM01,		.bit_offset = 17, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM01,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 6, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 5, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 6, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 5, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 4, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM0_LOCAL,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_cam0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 23,	.domain_name = "pd-cam1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 12,	.domain_name = "pd-isp0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL0,	.bit_offset = 12,	.domain_name = "pd-isp0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 4,	.domain_name = "pd-isp1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1_LOCAL,	.bit_offset = 4,	.domain_name = "pd-isp1", },
};

static struct exynos_pd_reg sys_pwr_regs_cam0[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_CAM0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM0_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_CAM0_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_cam0[] = {
	{ .reg = EXYNOS7420_DIV_CAM0, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM00, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM01, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM02, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM00, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM01, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_CAM0, },
};

/* #########################
   ##### BLK_CAM1 info #####
   ######################### */

static struct exynos_pd_clk top_clks_cam1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 12, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP07,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,	.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,	.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_CAM10,	.bit_offset = 4, },
};

static struct exynos_pd_clk local_clks_cam1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 26, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 25, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 23, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 22, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 11, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 10, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM13,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM13,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM10,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM10,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM10,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM1_LOCAL,	.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM1_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM1_LOCAL,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_cam1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 17,	.domain_name = "pd-isp0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 15,	.domain_name = "pd-isp0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 12,	.domain_name = "pd-isp1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 9,	.domain_name = "pd-isp1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 8,	.domain_name = "pd-isp1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM01,		.bit_offset = 16,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 1,	.domain_name = "pd-isp0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 0,	.domain_name = "pd-isp0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 1,	.domain_name = "pd-isp0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 0,	.domain_name = "pd-isp0", },
};

static struct exynos_pd_reg sys_pwr_regs_cam1[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_CAM1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_CAM1_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_CAM1_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_cam1[] = {
	{ .reg = EXYNOS7420_DIV_CAM1, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM10, },
	{ .reg = EXYNOS7420_MUX_SEL_CAM11, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM12, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM13, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM10, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM11, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_CAM12, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_CAM10, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_CAM12, },
};

/* #########################
   ##### BLK_ISP0 info #####
   ######################### */

static struct exynos_pd_clk top_clks_isp0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 20, },
};

static struct exynos_pd_clk local_clks_isp0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 17, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 15, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00,		.bit_offset = 13, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL0,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL0,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP0_LOCAL1,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_isp0[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 25,	.domain_name = "pd-cam1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 10,	.domain_name = "pd-cam1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 1,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 0,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 1,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 0,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM10,		.bit_offset = 19,	.domain_name = "pd-cam1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM1_LOCAL,	.bit_offset = 4,	.domain_name = "pd-cam1", },
};

static struct exynos_pd_reg sys_pwr_regs_isp0[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_ISP0_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS_PMU_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS_PMU_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_ISP0_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_ISP0_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_isp0[] = {
	{ .reg = EXYNOS7420_DIV_ISP0, },
	{ .reg = EXYNOS7420_MUX_SEL_ISP0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP00, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP01, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_ISP0, },
};

/* #########################
   ##### BLK_ISP1 info #####
   ######################### */

static struct exynos_pd_clk top_clks_isp1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP04,		.bit_offset = 12, },
};

static struct exynos_pd_clk local_clks_isp1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 9, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1,		.bit_offset = 12, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_ISP1,		.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1_LOCAL,	.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1_LOCAL,	.bit_offset = 0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_ISP1_LOCAL,	.bit_offset = 0, },
};

static struct exynos_pd_clk asyncbridge_clks_isp1[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 26,	.domain_name = "pd-cam1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 11,	.domain_name = "pd-cam1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM11,		.bit_offset = 22,	.domain_name = "pd-cam1", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 1,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM00,		.bit_offset = 0,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 1,	.domain_name = "pd-cam0", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_CAM0_LOCAL,	.bit_offset = 0,	.domain_name = "pd-cam0", },
};

static struct exynos_pd_reg sys_pwr_regs_isp1[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_ISP1_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_ISP1_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_ISP1_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_ISP1_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_isp1[] = {
	{ .reg = EXYNOS7420_DIV_ISP1, },
	{ .reg = EXYNOS7420_MUX_SEL_ISP1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_ISP1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_ISP1, },
};

/* ########################
   ##### BLK_VPP info #####
   ######################## */

static struct exynos_pd_clk top_clks_vpp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP03,		.bit_offset = 8, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP03,		.bit_offset = 4, },
};

static struct exynos_pd_clk local_clks_vpp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 15, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 14, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 3, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 2, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 0, },
};

static struct exynos_pd_reg sys_pwr_regs_vpp[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_VPP_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_VPP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_VPP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_VPP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 9, },
	{ .reg = EXYNOS_PMU_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 8, },
	{ .reg = EXYNOS_PMU_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS_PMU_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS_PMU_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_VPP_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_VPP_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_vpp[] = {
	{ .reg = EXYNOS7420_DIV_VPP, },
	{ .reg = EXYNOS7420_MUX_SEL_VPP, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_VPP, },
};

/* #########################
   ##### BLK_DISP info #####
   ######################### */

static struct exynos_pd_clk top_clks_disp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOP03,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 24, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 20, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 16, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_TOP0_DISP,	.bit_offset = 12, },
};

static struct exynos_pd_clk local_clks_disp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 30, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 29, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP,		.bit_offset = 28, },
};

static struct exynos_pd_clk asyncbridge_clks_disp[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 3,	.domain_name = "pd-vpp", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 2,	.domain_name = "pd-vpp", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 1,	.domain_name = "pd-vpp", },
	{ .reg = EXYNOS7420_ENABLE_ACLK_VPP,		.bit_offset = 0,	.domain_name = "pd-vpp", },
};

static struct exynos_pd_reg sys_pwr_regs_disp[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 9, },
	{ .reg = EXYNOS_PMU_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 8, },
	{ .reg = EXYNOS_PMU_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS_PMU_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS_PMU_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_DISP_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_DISP_SYS_PWR_REG	,	.bit_offset = 0, },
};

static struct sfr_save save_list_disp[] = {
	{ .reg = EXYNOS7420_DIV_DISP, },
	{ .reg = DISP_LOCK, },
	{ .reg = DISP_CON, },
	{ .reg = DISP_CON1, },
	{ .reg = DISP_CON2, },
	{ .reg = DPHY_LOCK, },
	{ .reg = DPHY_CON, },
	{ .reg = DPHY_CON1, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP0, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP1, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP2, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP3, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP4, },
	{ .reg = EXYNOS7420_MUX_SEL_DISP5, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP_RO_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP_RW_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP_RO_SFW, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_DISP_RW_SFW, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_DISP, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_DISP_RO_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_DISP_RW_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_DISP_RO_SFW, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_DISP_RW_SFW, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_DISP1, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_DISP2, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_DISP4, },
};

/* ########################
   ##### BLK_AUD info #####
   ######################## */

static struct exynos_pd_clk local_clks_aud[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_AUD,		.bit_offset = 28, },
};

static struct exynos_pd_reg sys_pwr_regs_aud[] = {
	{ .reg = EXYNOS_PMU_PAD_RETENTION_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_GPIO_MODE_AUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_AUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_AUD_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_AUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_AUD_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_aud[] = {
	{ .reg = EXYNOS7420_DIV_AUD0, },
	{ .reg = EXYNOS7420_DIV_AUD1, },
	{ .reg = EXYNOS7420_MUX_SEL_AUD, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_AUD, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_AUD, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_AUD, },
};

/* #########################
   ##### BLK_MSCL info #####
   ######################### */

static struct exynos_pd_clk top_clks_mscl[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 20, },
};

static struct exynos_pd_clk local_clks_mscl[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 30, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 29, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 28, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 27, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL,		.bit_offset = 26, },
};

static struct exynos_pd_reg sys_pwr_regs_mscl[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_MSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 9, },
	{ .reg = EXYNOS_PMU_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 8, },
	{ .reg = EXYNOS_PMU_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS_PMU_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS_PMU_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_MSCL_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_MSCL_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_mscl[] = {
	{ .reg = EXYNOS7420_DIV_MSCL, },
	{ .reg = EXYNOS7420_MUX_SEL_MSCL, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL_SMMU0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL_SFW0, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL_SMMU1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MSCL_SFW1, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_G2D_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MSCL, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MSCL_SMMU0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MSCL_SFW0, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MSCL_SMMU1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MSCL_SFW1, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_G2D_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_SCLK_MSCL, },
};

/* ########################
   ##### BLK_MFC info #####
   ######################## */

static struct exynos_pd_clk top_clks_mfc[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_TOPC1,		.bit_offset = 8, },
};

static struct exynos_pd_clk local_clks_mfc[] = {
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 31, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 30, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC,		.bit_offset = 19, },
};

static struct exynos_pd_reg sys_pwr_regs_mfc[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_MFC_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_MFC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_MFC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_MFC_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 5, },
	{ .reg = EXYNOS_PMU_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 4, },
	{ .reg = EXYNOS_PMU_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 1, },
	{ .reg = EXYNOS_PMU_MEMORY_MFC_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_MFC_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_mfc[] = {
	{ .reg = EXYNOS7420_DIV_MFC, },
	{ .reg = EXYNOS7420_MUX_SEL_MFC, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC_0_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC_0_SFW, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC_1_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_ACLK_MFC_1_SFW, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MFC, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MFC_0_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MFC_0_SFW, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MFC_1_SMMU, },
	{ .reg = EXYNOS7420_ENABLE_PCLK_MFC_1_SFW, },
};
