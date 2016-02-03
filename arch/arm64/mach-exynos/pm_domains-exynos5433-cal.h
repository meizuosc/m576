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

struct exynos5430_pd_state {
	void __iomem *reg;
	u8 bit_offset;
	struct clk *clock;
};

/* BLK_MFC clocks */
static struct exynos5430_pd_state gateclks_mfc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,		.bit_offset = 1, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    	.bit_offset = 3, },
};

/* BLK_HEVC clocks */
static struct exynos5430_pd_state gateclks_hevc[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,		.bit_offset = 3, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    	.bit_offset = 5, },
};

/* BLK_GSCL clocks */
static struct exynos5430_pd_state gateclks_gscl[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,		.bit_offset = 7, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    	.bit_offset = 14, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    	.bit_offset = 15, },
};

/* BLK_MSCL clocks */
static struct exynos5430_pd_state gateclks_mscl[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 	.bit_offset = 10, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 19, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_MSCL,       .bit_offset = 0, },
};

/* BLK_G2D clocks */
static struct exynos5430_pd_state gateclks_g2d[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,		.bit_offset = 0, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    	.bit_offset = 0, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,    	.bit_offset = 2, },
};

/* BLK_ISP clocks */
static struct exynos5430_pd_state gateclks_isp[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 	.bit_offset = 4, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 6, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 7, },
};

/* BLK_CAM0 clocks */
static struct exynos5430_pd_state gateclks_cam0[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 	.bit_offset = 5, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 8, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 9, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 10, },
};

/* BLK_CAM1 clocks */
static struct exynos5430_pd_state gateclks_cam1[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,	 	.bit_offset = 6, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 11, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 12, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 13, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,	.bit_offset = 0, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,	.bit_offset = 1, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,	.bit_offset = 2, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,	.bit_offset = 4, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,	.bit_offset = 5, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,	.bit_offset = 6, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_TOP_CAM1,	.bit_offset = 7, },
};

/* BLK_G3D clocks */
static struct exynos5430_pd_state gateclks_g3d[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_TOP,       	.bit_offset = 18, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,     	.bit_offset = 30, },
};

/* BLK_DISP clocks */
static struct exynos5430_pd_state gateclks_disp[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      	.bit_offset = 1, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      	.bit_offset = 5, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      	.bit_offset = 6, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      	.bit_offset = 7, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      	.bit_offset = 8, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,      	.bit_offset = 9, },
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,		.bit_offset = 10,},
	{ .reg = EXYNOS5430_ENABLE_IP_MIF3,		.bit_offset = 11,},
	{ .reg = EXYNOS5430_ENABLE_ACLK_MIF3,   	.bit_offset = 1, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     	.bit_offset = 5, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     	.bit_offset = 6, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     	.bit_offset = 7, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     	.bit_offset = 8, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,     	.bit_offset = 9, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,            .bit_offset = 14, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_MIF,            .bit_offset = 15, },
};
