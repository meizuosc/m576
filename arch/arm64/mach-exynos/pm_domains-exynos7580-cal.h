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
	{ .reg = EXYNOS7580_EN_ACLK_TOP,		.bit_offset = 14, },
};

static struct exynos_pd_reg sys_pwr_regs_g3d[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_g3d[] = {
	{ .reg = EXYNOS7580_DIV_G3D, },
	{ .reg = EXYNOS7580_G3D_PLL_LOCK },
	{ .reg = EXYNOS7580_G3D_PLL_CON0 },
	{ .reg = EXYNOS7580_MUX_SEL_G3D0 },
};

/* #########################
   ##### BLK_ISP info #####
   ######################### */

static struct exynos_pd_clk top_clks_isp[] = {
	{ .reg = EXYNOS7580_EN_ACLK_TOP,		.bit_offset = 11, },
	{ .reg = EXYNOS7580_EN_ACLK_TOP,		.bit_offset = 10, },
	{ .reg = EXYNOS7580_EN_ACLK_TOP,		.bit_offset = 9, },
};

static struct exynos_pd_reg sys_pwr_regs_isp[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_ISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_ISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_ISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_ISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_ISP_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_isp[] = {
	{ .reg = EXYNOS7580_DIV_TOP_ISP0, },
	{ .reg = EXYNOS7580_DIV_TOP_ISP1, },
	{ .reg = EXYNOS7580_DIV_ISP0, },
	{ .reg = EXYNOS7580_DIV_ISP1, },
	{ .reg = EXYNOS7580_DIV_ISP2, },
	{ .reg = EXYNOS7580_DIV_ISP3, },
	{ .reg = EXYNOS7580_DIV_ISP4, },
	{ .reg = EXYNOS7580_DIV_ISP5, },
	{ .reg = EXYNOS7580_MUX_SEL_ISP1, },
	{ .reg = EXYNOS7580_MUX_SEL_ISP2, },
	{ .reg = EXYNOS7580_MUX_SEL_ISP3, },
	{ .reg = EXYNOS7580_MUX_SEL_ISP4, },
	{ .reg = EXYNOS7580_MUX_SEL_ISP5, },
};

/* #########################
   ##### BLK_DISP info #####
   ######################### */

static struct exynos_pd_clk top_clks_disp[] = {
	{ .reg = EXYNOS7580_EN_ACLK_TOP_DISP,		.bit_offset = 0, },
	{ .reg = EXYNOS7580_EN_SCLK_TOP_DISP,	.bit_offset = 1, },
	{ .reg = EXYNOS7580_EN_SCLK_TOP_DISP,	.bit_offset = 0, },
};

static struct exynos_pd_reg sys_pwr_regs_disp[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_DISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_DISP_SYS_PWR_REG	,	.bit_offset = 0, },
};

static struct sfr_save save_list_disp[] = {
	{ .reg = EXYNOS7580_DISP_PLL_LOCK,},
	{ .reg = EXYNOS7580_DISP_PLL_CON0,},
	{ .reg = EXYNOS7580_DISP_PLL_CON1,},
	{ .reg = EXYNOS7580_DISP_PLL_FREQ_DET,},
	{ .reg = EXYNOS7580_MUX_SEL_DISP0,},
	{ .reg = EXYNOS7580_MUX_SEL_DISP1,},
	{ .reg = EXYNOS7580_MUX_SEL_DISP2,},
	{ .reg = EXYNOS7580_MUX_SEL_DISP4,},
	{ .reg = EXYNOS7580_MUX_EN_DISP0,},
	{ .reg = EXYNOS7580_MUX_EN_DISP1,},
	{ .reg = EXYNOS7580_MUX_EN_DISP2,},
	{ .reg = EXYNOS7580_MUX_EN_DISP4,},
	{ .reg = EXYNOS7580_MUX_IGNORE_DISP2,},
	{ .reg = EXYNOS7580_DIV_DISP,},
	{ .reg = EXYNOS7580_DIV_STAT_DISP,},
	{ .reg = EXYNOS7580_EN_ACLK_DISP0,},
	{ .reg = EXYNOS7580_EN_ACLK_DISP1,},
	{ .reg = EXYNOS7580_EN_PCLK_DISP,},
	{ .reg = EXYNOS7580_EN_SCLK_DISP,},
	{ .reg = EXYNOS7580_CLKOUT_CMU_DISP,},
	{ .reg = EXYNOS7580_CLKOUT_CMU_DISP_DIV_STAT,},
};

/* ########################
   ##### BLK_AUD info #####
   ######################## */

static struct exynos_pd_reg sys_pwr_regs_aud[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_AUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_AUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_AUD_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_aud[] = {
	{ .reg = EXYNOS7580_MUX_SEL_AUD0, },
};

/* ############################
   ##### BLK_MFCMSCL info #####
   ############################ */

static struct exynos_pd_clk top_clks_mscl[] = {
	{ .reg = EXYNOS7580_EN_ACLK_TOP,		.bit_offset = 13, },
	{ .reg = EXYNOS7580_EN_ACLK_TOP,		.bit_offset = 12, },
};

static struct exynos_pd_reg sys_pwr_regs_mscl[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_MFCMSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_MFCMSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_MFCMSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_MFCMSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_MFCMSCL_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_mscl[] = {
	{ .reg = EXYNOS7580_DIV_MFCMSCL, },
	{ .reg = EXYNOS7580_MUX_SEL_MFCMSCL, },
};
