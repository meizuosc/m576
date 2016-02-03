/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file is for exynos7580 clocks.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <dt-bindings/clock/exynos7580.h>
#include <mach/regs-clock-exynos7580.h>
#include "composite.h"


/* please define clocks only used in device tree */
enum exynos7580_clks {
	none,

	oscclk = 1, usb_pll,
	g3d_pll = 10, disp_pll,
	mout_top0_mfc_pll, mout_top0_cci_pll, mout_top0_bus1_pll,
	mout_top0_bus0_pll, cpu_pll, apl_pll, aud_pll,
	mem0_pll = 19,

	bus_pll = 35,
	sclk_spi0 = 80, sclk_spi1, sclk_spi2,
	pclk_spi0 = 90, pclk_spi1, pclk_spi2,

	/* number for uart driver starts from 100 */
	baud0 = 100, baud1, baud2, puart0, suart0,
	puart1, suart1, puart2, suart2, mct,
	puart3 = 110, suart3, baud3,

	/*number for i2c driver starts from 120 */
	hsi2c0 = 120, hsi2c1, hsi2c2, hsi2c3,
	i2c0 = 130, i2c1, i2c2, i2c3,

	/*number for audio driver starts from 140 */
	lpass_dmac = 140, mout_aud_pll_user, mout_sclk_mi2s, mout_sclk_pcm,
	lpass_mem, dout_aclk_133, dout_sclk_mi2s, dout_sclk_pcm,
	dout_sclk_aud_uart, dout_sclk_audmixer,
	pclk_mi2s = 150, sclk_mi2s, mi2s_aud_bclk, sclk_pcm,
	audmixer_sysclk = 154, audmixer_bclk0, audmixer_bclk1, audmixer_bclk2,

	/* number for g3d starts from 160 */
	g3d = 160, mout_g3d, dout_aclk_g3d, aclk_g3d_400,

	/* number for FSYS block clocks starts from 200 */
	pdma0 = 200, pdma1,
	aclk_mmc0, aclk_mmc1, aclk_mmc2,	/* biu for mmc */
	sclk_mmc0, sclk_mmc1, sclk_mmc2,	/* gate_ciu for mmc */
	sclk_fsys_mmc0, sclk_fsys_mmc1, sclk_fsys_mmc2,
	dout_mmc0_a, dout_mmc0_b,
	dout_mmc1_a, dout_mmc1_b,
	dout_mmc2_a, dout_mmc2_b,

	otg_aclk = 300, otg_hclk, upsizer_otg, xiu_d_fsys1,
	ahb_usbhs, ahb2axi_usbhs, upsizer_fsys1, upsizer_ahb_usbhs,

	/* number for FSYS SCLK clocks */
	freeclk = 400, phyclk, clk48mohci, phy_otg,

	gate_rtc = 1020,
	pclk_wdt = 1030, pclk_pwm, pclk_adcif,

	/*
	 * number for MACROS defined in dt-bindings/clock/exynos7580.h file
	 * ranges from 3000 to nr_clks-1
	 */
	nr_clks = 5000,
};

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
static __initdata void *exynos7580_clk_regs[] = {
	/* local gate */
	EXYNOS7580_EN_SCLK_FSYS,
	EXYNOS7580_EN_PCLK_FSYS,
	EXYNOS7580_EN_ACLK_FSYS0,
	EXYNOS7580_EN_ACLK_FSYS1,
	EXYNOS7580_EN_ACLK_FSYS2,
	EXYNOS7580_EN_SCLK_PERIC,
	EXYNOS7580_EN_SCLK_PERIS,
	EXYNOS7580_EN_PCLK_PERIS,
	EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC,
	EXYNOS7580_EN_PCLK_PERIS_SEC_RTC_TOP,
	EXYNOS7580_EN_PCLK_PERIS_SEC_CHIPID,
	EXYNOS7580_EN_PCLK_PERIS_SEC_SECKEY,
	EXYNOS7580_EN_PCLK_PERIS_SEC_ANTIRBK,
	EXYNOS7580_EN_PCLK_PERIS_SEC_MONOTONIC,
	EXYNOS7580_EN_PCLK_PERIS_SEC_RTC_APBIF,
	EXYNOS7580_EN_PCLK_PERIC,
	EXYNOS7580_EN_ACLK_PERI,

	EXYNOS7580_EN_PCLK_IMEM,
	EXYNOS7580_EN_PCLK_IMEM_SEC_SSS,
	EXYNOS7580_EN_PCLK_IMEM_SEC_RTIC,
	EXYNOS7580_EN_ACLK_IMEM,
	EXYNOS7580_EN_ACLK_IMEM_SEC_IRAMC_TOP,
	EXYNOS7580_EN_ACLK_IMEM_SEC_SSS,
	EXYNOS7580_EN_ACLK_IMEM_SEC_RTIC,

	/* local div */
	EXYNOS7580_DIV_BUS0,
	EXYNOS7580_DIV_BUS1,
	EXYNOS7580_DIV_BUS2,

	EXYNOS7580_DIV_MIF0,
	EXYNOS7580_DIV_MIF1,

	/* local mux */
	EXYNOS7580_MUX_SEL_MIF3,
	EXYNOS7580_MUX_SEL_MIF4,
	EXYNOS7580_MUX_SEL_MIF5,

	/* top gate */
	EXYNOS7580_EN_SCLK_TOP_FSYS,

	EXYNOS7580_EN_SCLK_TOP_PERI,

	/* top div */
	EXYNOS7580_DIV_TOP_FSYS0,
	EXYNOS7580_DIV_TOP_FSYS1,
	EXYNOS7580_DIV_TOP_FSYS2,
	EXYNOS7580_DIV_TOP_PERI0,
	EXYNOS7580_DIV_TOP_PERI1,
	EXYNOS7580_DIV_TOP_PERI2,
	EXYNOS7580_DIV_TOP_PERI3,

	EXYNOS7580_DIV_TOP_DISP,

	/* top mux */
	EXYNOS7580_MUX_SEL_TOP_FSYS0,
	EXYNOS7580_MUX_SEL_TOP_FSYS1,
	EXYNOS7580_MUX_SEL_TOP_FSYS2,
	EXYNOS7580_MUX_SEL_TOP_PERI,

	EXYNOS7580_MUX_SEL_TOP_DISP,

	EXYNOS7580_EN_SCLK_TOP,
	EXYNOS7580_EN_ACLK_TOP,

	/* top0 div */
	EXYNOS7580_DIV_TOP0,

	/* top1, top2 mux */
	EXYNOS7580_MUX_SEL_TOP1,
	EXYNOS7580_MUX_SEL_TOP2,

	/* pll */
	EXYNOS7580_MEDIA_PLL_CON0,
	EXYNOS7580_BUS_PLL_CON0,
	EXYNOS7580_USB_PLL_CON0,
	EXYNOS7580_AUD_PLL_CON1,
	EXYNOS7580_AUD_PLL_CON0,
	EXYNOS7580_MUX_SEL_TOP0,
};

/*
 * table for pll clocks
 */
struct samsung_pll_rate_table table_cpu[] = {
	/* rate		p,  m,  s,  k */
	{1900000000U,	4, 292, 0, 0},
	{1800000000U,	4, 276, 0, 0},
	{1700000000U,	4, 262, 0, 0},
	{1600000000U,	4, 246, 0, 0},
	{1500000000U,	4, 230, 0, 0},
	{1400000000U,	4, 216, 0, 0},
	{1300000000U,	4, 200, 0, 0},
	{1200000000U,	4, 368, 1, 0},
	{1100000000U,	4, 340, 1, 0},
	{1000000000U,	4, 308, 1, 0},
	{900000000U,	4, 276, 1, 0},
	{800000000U,	4, 248, 1, 0},
	{700000000U,	4, 216, 1, 0},
	{600000000U,	4, 368, 2, 0},
	{500000000U,	4, 312, 2, 0},
	{400000000U,	4, 248, 2, 0},
	{300000000U,	4, 368, 3, 0},
	{200000000U,	4, 240, 3, 0},
	{100000000U,	4, 256, 4, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_apl[] = {
	/* rate		p,  m,  s,  k */
	{1900000000U,	4, 292, 0, 0},
	{1800000000U,	4, 276, 0, 0},
	{1700000000U,	4, 262, 0, 0},
	{1600000000U,	4, 246, 0, 0},
	{1500000000U,	4, 230, 0, 0},
	{1400000000U,	4, 216, 0, 0},
	{1300000000U,	4, 200, 0, 0},
	{1200000000U,	4, 368, 1, 0},
	{1100000000U,	4, 340, 1, 0},
	{1000000000U,	4, 308, 1, 0},
	{900000000U,	4, 276, 1, 0},
	{800000000U,	4, 248, 1, 0},
	{700000000U,	4, 216, 1, 0},
	{600000000U,	4, 368, 2, 0},
	{500000000U,	4, 312, 2, 0},
	{400000000U,	4, 248, 2, 0},
	{300000000U,	4, 368, 3, 0},
	{200000000U,	4, 240, 3, 0},
	{100000000U,	4, 256, 4, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_mem0[] = {
	/* rate		p,  m,  s,  k */
	{1650000000U,	13, 825, 0, 0},
	{1334000000U,	13, 667, 0, 0},
	{910000000U,	4, 280, 1, 0},
	{832000000U,	4, 256, 1, 0},
	{825000000U,	13, 825, 1, 0},
	{741000000U,	4, 228, 1, 0},
	{728000000U,	4, 224, 1, 0},
	{715000000U,	4, 220, 1, 0},
	{689000000U,	4, 212, 1, 0},
	{676000000U,	4, 208, 1, 0},
	{667000000U,	13, 667, 1, 0},
	{663000000U,	4, 204, 1, 0},
	{559000000U,	4, 344, 2, 0},
	{546000000U,	4, 336, 2, 0},
	{416000000U,	4, 256, 2, 0},
	{338000000U,	4, 208, 2, 0},
	{325000000U,	4, 200, 2, 0},
	{273000000U,	4, 336, 3, 0},
	{247000000U,	4, 304, 3, 0},
	{200000000U,	13, 800, 3, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_media[] = {
	/* rate		p,  m,  s,  k */
	{1650000000U,	13, 825, 0, 0},
	{1334000000U,	13, 667, 0, 0},
	{910000000U,	4, 280, 1, 0},
	{832000000U,	4, 256, 1, 0},
	{825000000U,	13, 825, 1, 0},
	{741000000U,	4, 228, 1, 0},
	{728000000U,	4, 224, 1, 0},
	{715000000U,	4, 220, 1, 0},
	{689000000U,	4, 212, 1, 0},
	{676000000U,	4, 208, 1, 0},
	{667000000U,	13, 667, 1, 0},
	{663000000U,	4, 204, 1, 0},
	{559000000U,	4, 344, 2, 0},
	{546000000U,	4, 336, 2, 0},
	{416000000U,	4, 256, 2, 0},
	{338000000U,	4, 208, 2, 0},
	{325000000U,	4, 200, 2, 0},
	{273000000U,	4, 336, 3, 0},
	{247000000U,	4, 304, 3, 0},
	{200000000U,	13, 800, 3, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_bus[] = {
	/* rate		p,  m,  s,  k */
	{800000000U,	13, 400, 0, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_g3d[] = {
	/* rate		p,  m,  s,  k */
	{800000000U,    13, 400, 0, 0},
	{734000000U,    13, 367, 0, 0},
	{668000000U,	13, 334, 0, 0},
	{534000000U,	13, 267, 0, 0},
	{440000000U,	13, 440, 1, 0},
	{350000000U,    13, 350, 1, 0},
	{266000000U,	13, 266, 1, 0},
	{160000000U,	13, 320, 2, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_disp[] = {
	/* rate		p,  m,  s,  k */
	{333000000U,	13, 333, 1, 0},
	{284000000U,	13, 284, 1, 0},
	{276000000U,	13, 276, 1, 0},
	{166000000U,	13, 332, 2, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_isp[] = {
	/* rate		p,  m,  s,  k */
	{1060000000U,	13, 530, 0, 0},
	{860000000U,	13, 430, 0, 0},
	{430000000U,	13, 430, 1, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_usb[] = {
	/* rate		p,  m,  s,  k */
	{50000000U,	13, 400, 4, 0},
	{24000000U,	13, 384, 5, 0},
	{0,		0, 0, 0, 0},
};

struct samsung_pll_rate_table table_aud[] = {
	/* rate		p,  m,  s,  k */
	{393216000U,	3, 181, 2, 31740},
	{294912000U,	9, 408, 2, 22262},
	{196608000U,	5, 302, 3, 31054},
	{147456000U,	9, 408, 3, 22262},
	{98304000U,	5, 302, 4, 31054},
	{73728000U,	9, 408, 4, 22262},
	{67737600U,	3, 250, 5, 7082},
	{65536000U,	5, 403, 5, 19560},
	{49152000U,	5, 302, 5, 31054},
	{45158400U,	3, 167, 5, -17124},
	{0,		0, 0, 0, 0},
};

/*
 * parent names are defined as array like below.
 * it is for mux clocks.
 */
/* TOP Block */
PNAME(mout_bus_media_pll_top_user_p) = {"mout_bus_pll_top_user",
					"mout_media_pll_top_user"};
PNAME(mout_media_bus_pll_top_user_p) = {"mout_media_pll_top_user",
					"mout_bus_pll_top_user"};
PNAME(mout_bus_pll_top_user_p) = {"fin_pll", "mout_bus_pll_top_user"};
/* CPU Block */
PNAME(mout_cpu_p) = {"cpu_pll", "mout_bus_pll_cpu_user"};
/* APL Block */
PNAME(mout_apl_p) = {"apl_pll", "mout_bus_pll_apl_user"};
/* G3D Block */
PNAME(mout_g3d_p) = {"g3d_pll", "aclk_g3d_400"};
/* MIF Block */
PNAME(mout_bus_media_pll_p) = {"bus_pll", "mout_media_pll_div2"};
PNAME(mout_mem0_pll_div2_p) = {"mem0_pll", "ffac_mif_mem0_pll_div2"};
PNAME(mout_media_pll_div2_p) = {"media_pll", "ffac_mif_media_pll_div2"};
PNAME(mout_mem0_media_pll_div2_p) = {"mout_mem0_pll_div2", "mout_media_pll_div2"};
PNAME(mout_sclk_disp_decon_int_eclk_a_p) = {"fin_pll", "mout_bus_pll_top_user"};
PNAME(mout_sclk_disp_decon_int_eclk_b_p) = {"mout_sclk_disp_decon_int_eclk_a",
					    "mout_media_pll_div2"};
PNAME(mout_sclk_disp_decon_int_vclk_a_p) = {"fin_pll", "mout_media_pll_div2"};
PNAME(mout_sclk_disp_decon_int_vclk_b_p) = {"mout_sclk_disp_decon_int_vclk_a",
					    "mout_bus_pll_top_user"};
/* ISP Block */
PNAME(mout_sclk_cpu_isp_clkin_a_p) = {"dout_isp_pll_div2", "mout_aclk_isp_400_user"};
PNAME(mout_sclk_cpu_isp_clkin_b_p) = {"mout_sclk_cpu_isp_clkin_a", "mout_aclk_isp_333_user"};
PNAME(mout_aclk_link_data_a_p) = {"dout_isp_pll_div2", "mout_aclk_isp_400_user"};
PNAME(mout_aclk_link_data_b_p) = {"mout_aclk_link_data_a", "mout_aclk_isp_333_user"};
PNAME(mout_aclk_link_data_c_p) = {"mout_aclk_link_data_b", "mout_aclk_isp_266_user"};
PNAME(mout_aclk_csi_link1_75_p) = {"dout_isp_pll_div3", "mout_aclk_isp_333_user"};
PNAME(mout_aclk_csi_link1_75_b_p) = {"mout_aclk_csi_link1_75", "mout_aclk_isp_400_user"};
PNAME(mout_aclk_fimc_isp_450_a_p) = {"dout_isp_pll_div2", "mout_aclk_isp_400_user"};
PNAME(mout_aclk_fimc_isp_450_b_p) = {"mout_aclk_fimc_isp_450_a", "mout_aclk_isp_333_user"};
PNAME(mout_aclk_fimc_isp_450_c_p) = {"mout_aclk_fimc_isp_450_b", "dout_isp_pll_div3"};
PNAME(mout_aclk_fimc_isp_450_d_p) = {"mout_aclk_fimc_isp_450_c", "mout_aclk_isp_266_user"};
PNAME(mout_aclk_fimc_fd_300_p) = {"dout_isp_pll_div3", "mout_aclk_isp_266_user"};
/* DISP Block */
PNAME(mout_sclk_decon_int_eclk_p) = {"disp_pll", "mout_sclk_decon_int_eclk_user"};
PNAME(mout_sclk_decon_int_vclk_p) = {"disp_pll", "mout_sclk_decon_int_vclk_user"};
/* FSYS Block */
PNAME(mout_sclk_fsys_mmc0_b_p) = {"mout_sclk_fsys_mmc0_a", "mout_media_pll_top_user"};
PNAME(mout_sclk_fsys_mmc1_b_p) = {"mout_sclk_fsys_mmc1_a", "mout_media_pll_top_user"};
PNAME(mout_sclk_fsys_mmc2_b_p) = {"mout_sclk_fsys_mmc2_a", "mout_media_pll_top_user"};
/* AUD Block */
PNAME(mout_sclk_mi2s_pcm_aud_p) = {"mout_aud_pll_user", "ioclk_audi2s0cdclk"};
/* PERI Block */
PNAME(mout_sclk_i2s_i2scodclki_p) = {"ioclk_audiocdclk1", "fin_pll",
				     "sclk_peri_i2s_i2scodclki"};

static struct samsung_fixed_rate exynos7580_fixed_rate_ext_clks[] __initdata = {
	FRATE(oscclk, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "samsung,exynos7580-oscclk", .data = (void *)0, },
};

static struct samsung_composite_pll exynos7580_pll_clks[] = {
	PLL(cpu_pll, "cpu_pll", pll_2555x, EXYNOS7580_CPU_PLL_LOCK,
			EXYNOS7580_CPU_PLL_CON0, EXYNOS7580_CPU_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_CPU_0, 0, \
			EXYNOS7580_MUX_STAT_CPU_0, 0, \
			table_cpu, 0, CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE, "cpu-cluster.0"),
	PLL(apl_pll, "apl_pll", pll_2555x, EXYNOS7580_APL_PLL_LOCK,
			EXYNOS7580_APL_PLL_CON0, EXYNOS7580_APL_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_APL_0, 0, \
			EXYNOS7580_MUX_STAT_APL_0, 0, \
			table_apl, 0, CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE, "cpu-cluster.1"),
	PLL(mem0_pll, "mem0_pll", pll_2555x, EXYNOS7580_MEM0_PLL_LOCK,
			EXYNOS7580_MEM0_PLL_CON0, EXYNOS7580_MEM0_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_MIF0, 0, \
			EXYNOS7580_MUX_STAT_MIF0, 0, \
			table_mem0, 0, CLK_IGNORE_UNUSED, "mem0_pll"),
	PLL(none, "media_pll", pll_2555x, EXYNOS7580_MEDIA_PLL_LOCK,
			EXYNOS7580_MEDIA_PLL_CON0, EXYNOS7580_MEDIA_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_MIF1, 8, \
			EXYNOS7580_MUX_STAT_MIF1, 8, \
			table_media, 0, CLK_IGNORE_UNUSED, "media_pll"),
	PLL(none, "bus_pll", pll_2551x, EXYNOS7580_BUS_PLL_LOCK,
			EXYNOS7580_BUS_PLL_CON0, EXYNOS7580_BUS_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_MIF2, 12, \
			EXYNOS7580_MUX_STAT_MIF2, 12, \
			table_bus, 0, CLK_IGNORE_UNUSED, "bus_pll"),
	PLL(g3d_pll, "g3d_pll", pll_2551x, EXYNOS7580_G3D_PLL_LOCK,
			EXYNOS7580_G3D_PLL_CON0, EXYNOS7580_G3D_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_G3D0, 0, \
			EXYNOS7580_MUX_STAT_G3D0, 0, \
			table_g3d, 0, CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE, "g3d_pll"),
	PLL(disp_pll, "disp_pll", pll_2551x, EXYNOS7580_DISP_PLL_LOCK,
			EXYNOS7580_DISP_PLL_CON0, EXYNOS7580_DISP_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_DISP0, 0, \
			EXYNOS7580_MUX_STAT_DISP0, 0, \
			table_disp, 0, CLK_IGNORE_UNUSED, "disp_pll"),
	PLL(none, "isp_pll", pll_2551x, EXYNOS7580_ISP_PLL_LOCK,
			EXYNOS7580_ISP_PLL_CON0, EXYNOS7580_ISP_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_ISP0, 0, \
			EXYNOS7580_MUX_STAT_ISP0, 0, \
			table_isp, 0, CLK_IGNORE_UNUSED, "isp_pll"),
	PLL(none, "usb_pll", pll_2551x, EXYNOS7580_USB_PLL_LOCK,
			EXYNOS7580_USB_PLL_CON0, EXYNOS7580_USB_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_FSYS0, 0, \
			EXYNOS7580_MUX_STAT_FSYS0, 0, \
			table_usb, 0, CLK_IGNORE_UNUSED, "usb_pll"),
	PLL(aud_pll, "aud_pll", pll_2650x, EXYNOS7580_AUD_PLL_LOCK,
			EXYNOS7580_AUD_PLL_CON0, EXYNOS7580_AUD_PLL_CON0, 31, \
			EXYNOS7580_MUX_SEL_TOP0, 0, \
			EXYNOS7580_MUX_STAT_TOP0, 0, \
			table_aud, 0, CLK_IGNORE_UNUSED, "aud_pll"),
};

static struct samsung_fixed_rate exynos7580_fixed_rate_clks[] __initdata = {
	FRATE(none, "ioclk_audi2s0cdclk", NULL, CLK_IS_ROOT, 100000000),
	FRATE(none, "ioclk_audi2s0sclk", NULL, CLK_IS_ROOT, 50000000),
	FRATE(none, "ioclk_audiocdclk1", NULL, CLK_IS_ROOT, 100000000),
	FRATE(none, "ioclk_peri_i2sbclki", NULL, CLK_IS_ROOT, 12288000),
	FRATE(none, "ioclk_audmix_bclk", NULL, CLK_IS_ROOT, 12288000),
	FRATE(none, "ioclk_peri_spi", NULL, CLK_IS_ROOT, 50000000),
	FRATE(none, "phyclk_usbhost20_freeclk", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbhost20_phyclock", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbhost20_clk48mohcl", NULL, CLK_IS_ROOT, 48000000),
	FRATE(none, "phyclk_usbotg20", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_csi_phy0_rxbyteclkhs0", NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_csi_phy1_rxbyteclkhs0", NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_txbyteclkhs_m4s4", NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_rxclkesc0_m4s4", NULL, CLK_IS_ROOT, 20000000),

	/* This is the special clock generated by DECON for dsi link0 */
	FRATE(none, "sclk_decon_int_rgb_vclk", NULL, CLK_IS_ROOT, 166000000),
};

static struct samsung_fixed_factor exynos7580_fixed_factor_clks[] __initdata = {
	/* CMU_MIF */
	FFACTOR(none, "ffac_mif_mem0_pll_div2", "mem0_pll", 1, 2, 0),
	FFACTOR(none, "ffac_mif_media_pll_div2", "media_pll", 1, 2, 0),
};

static struct samsung_composite_mux exynos7580_mux_clks[] = {
	/* TOP */
	MUX(none, "mout_aclk_bus1_400", mout_bus_media_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP2, 16, 1, \
			EXYNOS7580_MUX_STAT_TOP2, 16, 3, CLK_ON_CHANGING, "mout_aclk_bus1_400"),
	MUX(none, "mout_aclk_bus0_400", mout_bus_media_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP2, 12, 1, \
			EXYNOS7580_MUX_STAT_TOP2, 12, 3, CLK_ON_CHANGING, "mout_aclk_bus0_400"),
	MUX(none, "mout_aclk_bus2_400", mout_media_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP2, 20, 1, \
			EXYNOS7580_MUX_STAT_TOP2, 20, 3, CLK_ON_CHANGING, "mout_aclk_bus2_400"),
	MUX(none, "mout_aclk_isp_266", mout_bus_media_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP2, 0, 1, \
			EXYNOS7580_MUX_STAT_TOP2, 0, 3, CLK_ON_CHANGING, "mout_aclk_isp_266"),
	MUX(none, "mout_aclk_mfcmscl_266", mout_bus_media_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP2, 4, 1, \
			EXYNOS7580_MUX_STAT_TOP2, 4, 3, CLK_ON_CHANGING, "mout_aclk_mfcmscl_266"),
	MUX(none, "mout_aclk_mfcmscl_400", mout_bus_media_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP2, 8, 1, \
			EXYNOS7580_MUX_STAT_TOP2, 8, 3, CLK_ON_CHANGING, "mout_aclk_mfcmscl_400"),
	/* CPU */
	MUX(none, "mout_cpu", mout_cpu_p, \
			EXYNOS7580_MUX_SEL_CPU_2, 0, 1, \
			EXYNOS7580_MUX_STAT_CPU_2, 0, 3, CLK_ON_CHANGING, "mout_cpu"),
	/* APL */
	MUX(none, "mout_apl", mout_apl_p, \
			EXYNOS7580_MUX_SEL_APL_2, 0, 1, \
			EXYNOS7580_MUX_STAT_APL_2, 0, 3, CLK_ON_CHANGING, "mout_apl"),
	/* G3D */
	MUX(mout_g3d, "mout_g3d", mout_g3d_p, \
			EXYNOS7580_MUX_SEL_G3D2, 0, 1, \
			EXYNOS7580_MUX_STAT_G3D2, 0, 3, CLK_ON_CHANGING, NULL),
	/* MIF */
	MUX(none, "mout_mem0_pll_div2", mout_mem0_pll_div2_p, \
			EXYNOS7580_MUX_SEL_MIF3, 16, 1, \
			EXYNOS7580_MUX_STAT_MIF3, 16, 3, CLK_ON_CHANGING, "mout_mem0_pll_div2"),
	MUX(none, "mout_media_pll_div2", mout_media_pll_div2_p, \
			EXYNOS7580_MUX_SEL_MIF3, 24, 1, \
			EXYNOS7580_MUX_STAT_MIF3, 24, 3, CLK_ON_CHANGING, "mout_media_pll_div2"),
	MUX(none, "mout_clkm_phy_b", mout_mem0_media_pll_div2_p, \
			EXYNOS7580_MUX_SEL_MIF4, 4, 1, \
			EXYNOS7580_MUX_STAT_MIF4, 4, 3, CLK_ON_CHANGING, "mout_clkm_phy_b"),
	MUX(none, "mout_clk2x_phy_b", mout_mem0_media_pll_div2_p, \
			EXYNOS7580_MUX_SEL_MIF4, 20, 1, \
			EXYNOS7580_MUX_STAT_MIF4, 20, 3, CLK_ON_CHANGING, "mout_clk2x_phy_b"),
	MUX(none, "mout_aclk_mif_400", mout_bus_media_pll_p, \
			EXYNOS7580_MUX_SEL_MIF5, 0, 1, \
			EXYNOS7580_MUX_STAT_MIF5, 0, 3, CLK_ON_CHANGING, "mout_aclk_mif_400"),
	MUX(none, "mout_aclk_mif_100", mout_bus_media_pll_p, \
			EXYNOS7580_MUX_SEL_MIF5, 4, 1, \
			EXYNOS7580_MUX_STAT_MIF5, 4, 3, CLK_ON_CHANGING, "mout_aclk_mif_100"),
	MUX(none, "mout_aclk_mif_fix_100", mout_bus_media_pll_p, \
			EXYNOS7580_MUX_SEL_MIF5, 8, 1, \
			EXYNOS7580_MUX_STAT_MIF5, 8, 3, CLK_ON_CHANGING, "mout_aclk_mif_fix_100"),
	MUX(CLK_MUX_ACLK_DISP_200, "mout_aclk_disp_200", mout_bus_media_pll_p,
			EXYNOS7580_MUX_SEL_TOP_DISP, 0, 1, \
			EXYNOS7580_MUX_STAT_TOP_DISP, 0, 3, CLK_ON_CHANGING, "mout_aclk_disp_200"),
	MUX(CLK_MUX_SCLK_DISP_DECON_INT_ECLK_A, "mout_sclk_disp_decon_int_eclk_a", mout_sclk_disp_decon_int_eclk_a_p, \
			EXYNOS7580_MUX_SEL_TOP_DISP, 8, 1, \
			EXYNOS7580_MUX_STAT_TOP_DISP, 8, 3, CLK_ON_CHANGING, NULL),
	MUX(CLK_MUX_SCLK_DISP_DECON_INT_ECLK_B, "mout_sclk_disp_decon_int_eclk_b", mout_sclk_disp_decon_int_eclk_b_p, \
			EXYNOS7580_MUX_SEL_TOP_DISP, 12, 1, \
			EXYNOS7580_MUX_STAT_TOP_DISP, 12, 3, CLK_ON_CHANGING, NULL),
	MUX(CLK_MUX_SCLK_DISP_DECON_INT_VCLK_A, "mout_sclk_disp_decon_int_vclk_a", mout_sclk_disp_decon_int_vclk_a_p, \
			EXYNOS7580_MUX_SEL_TOP_DISP, 16, 1, \
			EXYNOS7580_MUX_STAT_TOP_DISP, 16, 3, CLK_ON_CHANGING, NULL),
	MUX(CLK_MUX_SCLK_DISP_DECON_INT_VCLK_B, "mout_sclk_disp_decon_int_vclk_b", mout_sclk_disp_decon_int_vclk_b_p, \
			EXYNOS7580_MUX_SEL_TOP_DISP, 20, 1, \
			EXYNOS7580_MUX_STAT_TOP_DISP, 20, 3, CLK_ON_CHANGING, NULL),
	/* ISP */
	MUX(none, "mout_sclk_cpu_isp_clkin_a", mout_sclk_cpu_isp_clkin_a_p, \
			EXYNOS7580_MUX_SEL_ISP3, 0, 1, \
			EXYNOS7580_MUX_STAT_ISP3, 0, 3, CLK_ON_CHANGING, "mout_sclk_cpu_isp_clkin_a"),
	MUX(none, "mout_sclk_cpu_isp_clkin_b", mout_sclk_cpu_isp_clkin_b_p, \
			EXYNOS7580_MUX_SEL_ISP3, 4, 1, \
			EXYNOS7580_MUX_STAT_ISP3, 4, 3, CLK_ON_CHANGING, "mout_sclk_cpu_isp_clkin_b"),
	MUX(none, "mout_aclk_link_data_a", mout_aclk_link_data_a_p, \
			EXYNOS7580_MUX_SEL_ISP5, 0, 1, \
			EXYNOS7580_MUX_STAT_ISP5, 0, 3, CLK_ON_CHANGING, "mout_aclk_link_data_a"),
	MUX(none, "mout_aclk_link_data_b", mout_aclk_link_data_b_p, \
			EXYNOS7580_MUX_SEL_ISP5, 4, 1, \
			EXYNOS7580_MUX_STAT_ISP5, 4, 3, CLK_ON_CHANGING, "mout_aclk_link_data_b"),
	MUX(none, "mout_aclk_link_data_c", mout_aclk_link_data_c_p, \
			EXYNOS7580_MUX_SEL_ISP5, 8, 1, \
			EXYNOS7580_MUX_STAT_ISP5, 8, 3, CLK_ON_CHANGING, "mout_aclk_link_data_c"),
	MUX(none, "mout_aclk_csi_link1_75", mout_aclk_csi_link1_75_p, \
			EXYNOS7580_MUX_SEL_ISP3, 8, 1, \
			EXYNOS7580_MUX_STAT_ISP3, 8, 3, CLK_ON_CHANGING, "mout_aclk_csi_link1_75"),
	MUX(none, "mout_aclk_csi_link1_75_b", mout_aclk_csi_link1_75_b_p, \
			EXYNOS7580_MUX_SEL_ISP3, 12, 1, \
			EXYNOS7580_MUX_STAT_ISP3, 12, 3, CLK_ON_CHANGING, "mout_aclk_csi_link1_75_b"),
	MUX(none, "mout_aclk_fimc_isp_450_a", mout_aclk_fimc_isp_450_a_p, \
			EXYNOS7580_MUX_SEL_ISP4, 0, 1, \
			EXYNOS7580_MUX_STAT_ISP4, 0, 3, CLK_ON_CHANGING, "mout_aclk_fimc_isp_450_a"),
	MUX(none, "mout_aclk_fimc_isp_450_b", mout_aclk_fimc_isp_450_b_p, \
			EXYNOS7580_MUX_SEL_ISP4, 4, 1, \
			EXYNOS7580_MUX_STAT_ISP4, 4, 3, CLK_ON_CHANGING, "mout_aclk_fimc_isp_450_b"),
	MUX(none, "mout_aclk_fimc_isp_450_c", mout_aclk_fimc_isp_450_c_p, \
			EXYNOS7580_MUX_SEL_ISP4, 8, 1, \
			EXYNOS7580_MUX_STAT_ISP4, 8, 3, CLK_ON_CHANGING, "mout_aclk_fimc_isp_450_c"),
	MUX(none, "mout_aclk_fimc_isp_450_d", mout_aclk_fimc_isp_450_d_p, \
			EXYNOS7580_MUX_SEL_ISP4, 12, 1, \
			EXYNOS7580_MUX_STAT_ISP4, 12, 3, CLK_ON_CHANGING, "mout_aclk_fimc_isp_450_d"),
	MUX(none, "mout_aclk_fimc_fd_300", mout_aclk_fimc_fd_300_p, \
			EXYNOS7580_MUX_SEL_ISP4, 16, 1, \
			EXYNOS7580_MUX_STAT_ISP4, 16, 3, CLK_ON_CHANGING, "mout_aclk_fimc_fd_300"),
	MUX(none, "mout_sclk_isp_spi0_ext_clk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_ISP, 0, 1, \
			EXYNOS7580_MUX_STAT_TOP_ISP, 0, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_isp_spi1_ext_clk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_ISP, 4, 1, \
			EXYNOS7580_MUX_STAT_TOP_ISP, 4, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_isp_uart_ext_uclk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_ISP, 16, 1, \
			EXYNOS7580_MUX_STAT_TOP_ISP, 16, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_isp_sensor0", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_ISP, 20, 1, \
			EXYNOS7580_MUX_STAT_TOP_ISP, 20, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_isp_sensor1", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_ISP, 24, 1, \
			EXYNOS7580_MUX_STAT_TOP_ISP, 24, 3, CLK_ON_CHANGING, NULL),
	/* DISP */
	MUX(CLK_MUX_SCLK_DECON_INT_ECLK, "mout_sclk_decon_int_eclk", mout_sclk_decon_int_eclk_p, \
			EXYNOS7580_MUX_SEL_DISP4, 0, 1, \
			EXYNOS7580_MUX_STAT_DISP4, 0, 3, CLK_ON_CHANGING, NULL),
	MUX(CLK_MUX_SCLK_DECON_INT_VCLK, "mout_sclk_decon_int_vclk", mout_sclk_decon_int_vclk_p, \
			EXYNOS7580_MUX_SEL_DISP4, 4, 1, \
			EXYNOS7580_MUX_STAT_DISP4, 4, 3, CLK_ON_CHANGING, NULL),
	/* FSYS */
	MUX(none, "mout_sclk_fsys_mmc0_a", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_FSYS0, 0, 1, \
			EXYNOS7580_MUX_STAT_TOP_FSYS0, 0, 3, CLK_ON_CHANGING | \
			CLK_IGNORE_UNUSED, NULL),
	MUX(none, "mout_sclk_fsys_mmc1_a", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_FSYS1, 0, 1, \
			EXYNOS7580_MUX_STAT_TOP_FSYS1, 0, 3, CLK_ON_CHANGING | \
			CLK_IGNORE_UNUSED, NULL),
	MUX(none, "mout_sclk_fsys_mmc2_a", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_FSYS2, 0, 1, \
			EXYNOS7580_MUX_STAT_TOP_FSYS2, 0, 3, CLK_ON_CHANGING | \
			CLK_IGNORE_UNUSED, NULL),
	MUX(none, "mout_sclk_fsys_mmc0_b", mout_sclk_fsys_mmc0_b_p, \
			EXYNOS7580_MUX_SEL_TOP_FSYS0, 4, 1, \
			EXYNOS7580_MUX_STAT_TOP_FSYS0, 4, 3, CLK_ON_CHANGING | \
			CLK_IGNORE_UNUSED, NULL),
	MUX(none, "mout_sclk_fsys_mmc1_b", mout_sclk_fsys_mmc1_b_p, \
			EXYNOS7580_MUX_SEL_TOP_FSYS1, 4, 1, \
			EXYNOS7580_MUX_STAT_TOP_FSYS1, 4, 3, CLK_ON_CHANGING | \
			CLK_IGNORE_UNUSED, NULL),
	MUX(none, "mout_sclk_fsys_mmc2_b", mout_sclk_fsys_mmc2_b_p, \
			EXYNOS7580_MUX_SEL_TOP_FSYS2, 4, 1, \
			EXYNOS7580_MUX_STAT_TOP_FSYS2, 4, 3, CLK_ON_CHANGING | \
			CLK_IGNORE_UNUSED, NULL),
	/* AUD */
	MUX(mout_sclk_mi2s, "mout_sclk_mi2s_aud", mout_sclk_mi2s_pcm_aud_p, \
			EXYNOS7580_MUX_SEL_AUD1, 0, 1, \
			0, 0, 0, 0, "mout_sclk_mi2s_aud"),
	MUX(mout_sclk_pcm, "mout_sclk_pcm_aud", mout_sclk_mi2s_pcm_aud_p, \
			EXYNOS7580_MUX_SEL_AUD1, 8, 1, \
			0, 0, 0, 0, "mout_sclk_pcm_aud"),
	/* PERI */
	MUX(none, "mout_sclk_peri_spi0_ext_clk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_PERI, 0, 1, \
			EXYNOS7580_MUX_STAT_TOP_PERI, 0, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_peri_spi1_ext_clk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_PERI, 4, 1, \
			EXYNOS7580_MUX_STAT_TOP_PERI, 4, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_peri_spi2_ext_clk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_PERI, 8, 1, \
			EXYNOS7580_MUX_STAT_TOP_PERI, 8, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_peri_uart0_ext_uclk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_PERI, 20, 1, \
			EXYNOS7580_MUX_STAT_TOP_PERI, 20, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_peri_uart1_ext_uclk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_PERI, 24, 1, \
			EXYNOS7580_MUX_STAT_TOP_PERI, 24, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_peri_uart2_ext_uclk", mout_bus_pll_top_user_p, \
			EXYNOS7580_MUX_SEL_TOP_PERI, 28, 1, \
			EXYNOS7580_MUX_STAT_TOP_PERI, 28, 3, CLK_ON_CHANGING, NULL),
	MUX(none, "mout_sclk_i2s_i2scodclki", mout_sclk_i2s_i2scodclki_p, \
			EXYNOS7580_MUX_SEL_PERI, 0, 2, \
			0, 0, 0, 0, NULL),
};

static struct samsung_composite_divider exynos7580_div_clks[] = {
	/* TOP */
	DIV(none, "dout_aclk_fsys_200", "mout_bus_pll_top_user", \
			EXYNOS7580_DIV_TOP0, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP0, 0, 1, 0, "dout_aclk_fsys_200"),
	DIV(none, "dout_aclk_imem_266", "mout_bus_pll_top_user", \
			EXYNOS7580_DIV_TOP0, 4, 4, \
			EXYNOS7580_DIV_STAT_TOP0, 4, 1, 0, "dout_aclk_imem_266"),
	DIV(none, "dout_aclk_imem_200", "mout_bus_pll_top_user", \
			EXYNOS7580_DIV_TOP0, 8, 4, \
			EXYNOS7580_DIV_STAT_TOP0, 8, 1, 0, "dout_aclk_imem_200"),
	DIV(none, "dout_aclk_bus1_400", "mout_aclk_bus1_400", \
			EXYNOS7580_DIV_TOP0, 16, 4, \
			EXYNOS7580_DIV_STAT_TOP0, 16, 1, 0, "dout_aclk_bus1_400"),
	DIV(none, "dout_aclk_bus0_400", "mout_aclk_bus0_400", \
			EXYNOS7580_DIV_TOP0, 12, 4, \
			EXYNOS7580_DIV_STAT_TOP0, 12, 1, 0, "dout_aclk_bus0_400"),
	DIV(none, "dout_aclk_bus2_400", "mout_aclk_bus2_400", \
			EXYNOS7580_DIV_TOP0, 24, 4, \
			EXYNOS7580_DIV_STAT_TOP0, 24, 1, 0, "dout_aclk_bus2_400"),
	DIV(none, "dout_aclk_peri_66", "mout_bus_pll_top_user", \
			EXYNOS7580_DIV_TOP0, 28, 4, \
			EXYNOS7580_DIV_STAT_TOP0, 28, 1, 0, "dout_aclk_peri_66"),
	DIV(none, "dout_aclk_g3d_400", "mout_bus_pll_top_user", \
			EXYNOS7580_DIV_TOP1, 28, 4, \
			EXYNOS7580_DIV_STAT_TOP1, 28, 1, 0, "dout_aclk_g3d_400"),
	DIV(none, "dout_aclk_isp_400", "mout_bus_pll_top_user", \
			EXYNOS7580_DIV_TOP1, 8, 4, \
			EXYNOS7580_DIV_STAT_TOP1, 8, 1, 0, "dout_aclk_isp_400"),
	DIV(none, "dout_aclk_isp_333", "mout_media_pll_top_user", \
			EXYNOS7580_DIV_TOP1, 12, 4, \
			EXYNOS7580_DIV_STAT_TOP1, 12, 1, 0, "dout_aclk_isp_333"),
	DIV(none, "dout_aclk_isp_266_top", "mout_aclk_isp_266", \
			EXYNOS7580_DIV_TOP1, 16, 4, \
			EXYNOS7580_DIV_STAT_TOP1, 16, 1, 0, "dout_aclk_isp_266_top"),
	DIV(CLK_DOUT_ACLK_MFCMSCL_266, "dout_aclk_mfcmscl_266", "mout_aclk_mfcmscl_266", \
			EXYNOS7580_DIV_TOP1, 20, 4, \
			EXYNOS7580_DIV_STAT_TOP1, 20, 1, 0, "dout_aclk_mfcmscl_266"),
	DIV(none, "dout_aclk_mfcmscl_400", "mout_aclk_mfcmscl_400", \
			EXYNOS7580_DIV_TOP1, 24, 4, \
			EXYNOS7580_DIV_STAT_TOP1, 24, 1, 0, "dout_aclk_mfcmscl_400"),
	/* G3D */
	DIV(dout_aclk_g3d, "dout_aclk_g3d_600", "mout_g3d", \
			EXYNOS7580_DIV_G3D, 0, 3, \
			EXYNOS7580_DIV_STAT_G3D, 0, 1, 0, NULL),
	DIV(none, "dout_pclk_g3d_150", "dout_aclk_g3d_600", \
			EXYNOS7580_DIV_G3D, 4, 3, \
			EXYNOS7580_DIV_STAT_G3D, 4, 1, 0, NULL),
	/* MIF */
	DIV(none, "dout_clkm_phy", "mout_clkm_phy_b", \
			EXYNOS7580_DIV_MIF0, 0, 4, \
			EXYNOS7580_DIV_STAT_MIF0, 0, 1, 0, "dout_clkm_phy"),
	DIV(none, "dout_clk2x_phy", "mout_clk2x_phy_b", \
			EXYNOS7580_DIV_MIF0, 4, 4, \
			EXYNOS7580_DIV_STAT_MIF0, 4, 1, 0, "dout_clk2x_phy"),
	DIV(none, "dout_aclk_mif_400", "mout_aclk_mif_400", \
			EXYNOS7580_DIV_MIF1, 0, 3, \
			EXYNOS7580_DIV_STAT_MIF1, 0, 1, 0, "dout_aclk_mif_400"),
	DIV(none, "dout_aclk_mif_200", "dout_aclk_mif_400", \
			EXYNOS7580_DIV_MIF1, 4, 2, \
			EXYNOS7580_DIV_STAT_MIF1, 4, 1, 0, "dout_aclk_mif_200"),
	DIV(none, "dout_aclk_mif_100", "mout_aclk_mif_100", \
			EXYNOS7580_DIV_MIF1, 8, 4, \
			EXYNOS7580_DIV_STAT_MIF1, 8, 1, 0, "dout_aclk_mif_100"),
	DIV(none, "dout_aclk_mif_fix_100", "mout_aclk_mif_fix_100", \
			EXYNOS7580_DIV_MIF1, 12, 3, \
			EXYNOS7580_DIV_STAT_MIF1, 12, 1, 0, "dout_aclk_mif_fix_100"),
	DIV(CLK_DIV_ACLK_DISP_200, "dout_aclk_disp_200", "mout_aclk_disp_200", \
			EXYNOS7580_DIV_TOP_DISP, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_DISP, 0, 1, 0, "dout_aclk_disp_200"),
	DIV(CLK_DIV_SCLK_DISP_DECON_INT_ECLK, "dout_sclk_disp_decon_int_eclk", "mout_sclk_disp_decon_int_eclk_b", \
			EXYNOS7580_DIV_TOP_DISP, 16, 4, \
			EXYNOS7580_DIV_STAT_TOP_DISP, 16, 1, 0, NULL),
	DIV(CLK_DIV_SCLK_DISP_DECON_INT_VCLK, "dout_sclk_disp_decon_int_vclk", "mout_sclk_disp_decon_int_vclk_b", \
			EXYNOS7580_DIV_TOP_DISP, 20, 4, \
			EXYNOS7580_DIV_STAT_TOP_DISP, 20, 1, 0, NULL),
	/* ISP */
	DIV(none, "dout_isp_pll_div2", "isp_pll", \
			EXYNOS7580_DIV_ISP0, 0, 2, \
			EXYNOS7580_DIV_STAT_ISP0, 0, 1, 0, "dout_isp_pll_div2"),
	DIV(none, "dout_isp_pll_div3", "isp_pll", \
			EXYNOS7580_DIV_ISP0, 4, 2, \
			EXYNOS7580_DIV_STAT_ISP0, 4, 1, 0, "dout_isp_pll_div3"),
	DIV(none, "dout_sclk_cpu_isp_clkin", "mout_sclk_cpu_isp_clkin_b", \
			EXYNOS7580_DIV_ISP1, 0, 3, \
			EXYNOS7580_DIV_STAT_ISP1, 0, 1, 0, "dout_sclk_cpu_isp_clkin"),
	DIV(none, "dout_sclk_cpu_isp_atclkin", "dout_sclk_cpu_isp_clkin", \
			EXYNOS7580_DIV_ISP1, 4, 3, \
			EXYNOS7580_DIV_STAT_ISP1, 4, 1, 0, "dout_sclk_cpu_isp_atclkin"),
	DIV(none, "dout_sclk_cpu_isp_pclkdbg", "dout_sclk_cpu_isp_clkin", \
			EXYNOS7580_DIV_ISP1, 8, 3, \
			EXYNOS7580_DIV_STAT_ISP1, 8, 1, 0, "dout_sclk_cpu_isp_pclkdbg"),
	DIV(none, "dout_pclk_csi_link0_225", "dout_isp_pll_div2", \
			EXYNOS7580_DIV_ISP2, 0, 2, \
			EXYNOS7580_DIV_STAT_ISP2, 0, 1, 0, "dout_pclk_csi_link0_225"),
	DIV(none, "dout_aclk_link_data", "mout_aclk_link_data_c", \
			EXYNOS7580_DIV_ISP5, 0, 3, \
			EXYNOS7580_DIV_STAT_ISP5, 0, 1, 0, "dout_aclk_link_data"),
	DIV(none, "dout_aclk_csi_link1_75", "mout_aclk_csi_link1_75_b", \
			EXYNOS7580_DIV_ISP2, 4, 3, \
			EXYNOS7580_DIV_STAT_ISP2, 4, 1, 0, "dout_aclk_csi_link1_75"),
	DIV(none, "dout_pclk_csi_link1_37", "dout_aclk_csi_link1_75", \
			EXYNOS7580_DIV_ISP2, 8, 2, \
			EXYNOS7580_DIV_STAT_ISP2, 8, 1, 0, "dout_pclk_csi_link1_37"),
	DIV(none, "dout_aclk_fimc_isp_450", "mout_aclk_fimc_isp_450_d", \
			EXYNOS7580_DIV_ISP3, 0, 3, \
			EXYNOS7580_DIV_STAT_ISP3, 0, 1, 0, "dout_aclk_fimc_isp_450"),
	DIV(none, "dout_pclk_fimc_isp_225", "dout_aclk_fimc_isp_450", \
			EXYNOS7580_DIV_ISP3, 4, 3, \
			EXYNOS7580_DIV_STAT_ISP3, 4, 1, 0, "dout_pclk_fimc_isp_225"),
	DIV(none, "dout_aclk_fimc_fd_300", "mout_aclk_fimc_fd_300", \
			EXYNOS7580_DIV_ISP3, 8, 3, \
			EXYNOS7580_DIV_STAT_ISP3, 8, 1, 0, "dout_aclk_fimc_fd_300"),
	DIV(none, "dout_pclk_fimc_fd_150", "dout_aclk_fimc_fd_300", \
			EXYNOS7580_DIV_ISP3, 12, 2, \
			EXYNOS7580_DIV_STAT_ISP3, 12, 1, 0, "dout_pclk_fimc_fd_150"),
	DIV(none, "dout_aclk_isp_266", "mout_aclk_isp_266_user", \
			EXYNOS7580_DIV_ISP4, 0, 3, \
			EXYNOS7580_DIV_STAT_ISP4, 0, 1, 0, "dout_aclk_isp_266"),
	DIV(none, "dout_aclk_isp_133", "dout_aclk_isp_266", \
			EXYNOS7580_DIV_ISP4, 4, 2, \
			EXYNOS7580_DIV_STAT_ISP4, 4, 1, 0, "dout_aclk_isp_133"),
	DIV(none, "dout_aclk_isp_67", "dout_aclk_isp_133", \
			EXYNOS7580_DIV_ISP4, 8, 2, \
			EXYNOS7580_DIV_STAT_ISP4, 8, 1, 0, "dout_aclk_isp_67"),
	DIV(none, "dout_sclk_isp_spi0_ext_clk_a", "mout_sclk_isp_spi0_ext_clk", \
			EXYNOS7580_DIV_TOP_ISP0, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_ISP0, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_spi0_ext_clk_b", "dout_sclk_isp_spi0_ext_clk_a", \
			EXYNOS7580_DIV_TOP_ISP0, 4, 8, \
			EXYNOS7580_DIV_STAT_TOP_ISP0, 4, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_spi1_ext_clk_a", "mout_sclk_isp_spi1_ext_clk", \
			EXYNOS7580_DIV_TOP_ISP0, 12, 4, \
			EXYNOS7580_DIV_STAT_TOP_ISP0, 12, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_spi1_ext_clk_b", "dout_sclk_isp_spi1_ext_clk_a", \
			EXYNOS7580_DIV_TOP_ISP0, 16, 8, \
			EXYNOS7580_DIV_STAT_TOP_ISP0, 16, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_uart_ext_uclk", "mout_sclk_isp_uart_ext_uclk", \
			EXYNOS7580_DIV_TOP_ISP0, 24, 4, \
			EXYNOS7580_DIV_STAT_TOP_ISP0, 24, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_sensor0_a", "mout_sclk_isp_sensor0", \
			EXYNOS7580_DIV_TOP_ISP1, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_ISP1, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_sensor0_b", "dout_sclk_isp_sensor0_a", \
			EXYNOS7580_DIV_TOP_ISP1, 4, 4, \
			EXYNOS7580_DIV_STAT_TOP_ISP1, 4, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_sensor1_a", "mout_sclk_isp_sensor1", \
			EXYNOS7580_DIV_TOP_ISP1, 8, 4, \
			EXYNOS7580_DIV_STAT_TOP_ISP1, 8, 1, 0, NULL),
	DIV(none, "dout_sclk_isp_sensor1_b", "dout_sclk_isp_sensor1_a", \
			EXYNOS7580_DIV_TOP_ISP1, 12, 4, \
			EXYNOS7580_DIV_STAT_TOP_ISP1, 12, 1, 0, NULL),
	/* MFCMSCL */
	DIV(none, "dout_pclk_mfcmscl_100", "mout_aclk_mscl_400_user", \
			EXYNOS7580_DIV_MFCMSCL, 0, 2, \
			EXYNOS7580_DIV_STAT_MFCMSCL, 0, 1, 0, "dout_pclk_mfcmscl_100"),
	/* DISP */
	DIV(CLK_DIV_PCLK_DISP_100, "dout_pclk_disp_100", "mout_aclk_disp_200_user", \
			EXYNOS7580_DIV_DISP, 0, 2, \
			EXYNOS7580_DIV_STAT_DISP, 0, 1, 0, "dout_pclk_disp_100"),
	DIV(CLK_DIV_SCLK_DECON_INT_ECLK, "dout_sclk_decon_int_eclk", "mout_sclk_decon_int_eclk", \
			EXYNOS7580_DIV_DISP, 4, 3, \
			EXYNOS7580_DIV_STAT_DISP, 4, 1, 0, NULL),
	DIV(CLK_DIV_SCLK_DECON_INT_VCLK, "dout_sclk_decon_int_vclk", "mout_sclk_decon_int_vclk", \
			EXYNOS7580_DIV_DISP, 8, 3, \
			EXYNOS7580_DIV_STAT_DISP, 8, 1, 0, NULL),
	/* FSYS */
	DIV(dout_mmc0_a, "dout_sclk_fsys_mmc0_a", "mout_sclk_fsys_mmc0_b",
			EXYNOS7580_DIV_TOP_FSYS0, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_FSYS0, 0, 1, \
			CLK_IGNORE_UNUSED, NULL),
	DIV(dout_mmc1_a, "dout_sclk_fsys_mmc1_a", "mout_sclk_fsys_mmc1_b",
			EXYNOS7580_DIV_TOP_FSYS1, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_FSYS1, 0, 1, \
			CLK_IGNORE_UNUSED, NULL),
	DIV(dout_mmc2_a, "dout_sclk_fsys_mmc2_a", "mout_sclk_fsys_mmc2_b",
			EXYNOS7580_DIV_TOP_FSYS2, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_FSYS2, 0, 1, \
			CLK_IGNORE_UNUSED, NULL),
	DIV(dout_mmc0_b, "dout_sclk_fsys_mmc0_b", "dout_sclk_fsys_mmc0_a",
			EXYNOS7580_DIV_TOP_FSYS0, 4, 8, \
			EXYNOS7580_DIV_STAT_TOP_FSYS0, 4, 1, \
			CLK_IGNORE_UNUSED, NULL),
	DIV(dout_mmc1_b, "dout_sclk_fsys_mmc1_b", "dout_sclk_fsys_mmc1_a",
			EXYNOS7580_DIV_TOP_FSYS1, 4, 8, \
			EXYNOS7580_DIV_STAT_TOP_FSYS1, 4, 1, \
			CLK_IGNORE_UNUSED, NULL),
	DIV(dout_mmc2_b, "dout_sclk_fsys_mmc2_b", "dout_sclk_fsys_mmc2_a",
			EXYNOS7580_DIV_TOP_FSYS2, 4, 8, \
			EXYNOS7580_DIV_STAT_TOP_FSYS2, 4, 1, \
			CLK_IGNORE_UNUSED, NULL),
	/* AUD */
	DIV(dout_aclk_133, "dout_aclk_aud_133", "mout_aud_pll_user", \
			EXYNOS7580_DIV_AUD0, 0, 4, \
			EXYNOS7580_DIV_STAT_AUD0, 0, 1, 0, NULL),
	DIV(dout_sclk_mi2s, "dout_sclk_mi2s_aud", "mout_sclk_mi2s_aud", \
			EXYNOS7580_DIV_AUD1, 0, 4, \
			EXYNOS7580_DIV_STAT_AUD1, 0, 1, 0, NULL),
	DIV(dout_sclk_pcm, "dout_sclk_pcm_aud", "mout_sclk_pcm_aud", \
			EXYNOS7580_DIV_AUD1, 4, 8, \
			EXYNOS7580_DIV_STAT_AUD1, 4, 1, 0, NULL),
	DIV(dout_sclk_aud_uart, "dout_sclk_uart_aud", "mout_aud_pll_user", \
			EXYNOS7580_DIV_AUD1, 12, 4, \
			EXYNOS7580_DIV_STAT_AUD1, 12, 1, 0, NULL),
	DIV(dout_sclk_audmixer, "dout_sclk_audmixer_aud", "mout_aud_pll_user", \
			EXYNOS7580_DIV_AUD1, 20, 4, \
			EXYNOS7580_DIV_STAT_AUD1, 20, 1, 0, NULL),
	/* PERI */
	DIV(none, "dout_sclk_peri_aud", "mout_aud_pll_user", \
			EXYNOS7580_DIV_TOP_PERI0, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_PERI0, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_i2s", "dout_sclk_peri_aud", \
			EXYNOS7580_DIV_TOP_PERI0, 4, 6, \
			EXYNOS7580_DIV_STAT_TOP_PERI0, 4, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi0_a", "mout_sclk_peri_spi0_ext_clk", \
			EXYNOS7580_DIV_TOP_PERI1, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_PERI1, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi1_a", "mout_sclk_peri_spi1_ext_clk", \
			EXYNOS7580_DIV_TOP_PERI1, 16, 4, \
			EXYNOS7580_DIV_STAT_TOP_PERI1, 16, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi2_a", "mout_sclk_peri_spi2_ext_clk", \
			EXYNOS7580_DIV_TOP_PERI2, 0, 4, \
			EXYNOS7580_DIV_STAT_TOP_PERI1, 0, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi0_b", "dout_sclk_peri_spi0_a", \
			EXYNOS7580_DIV_TOP_PERI1, 4, 8, \
			EXYNOS7580_DIV_STAT_TOP_PERI1, 4, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi1_b", "dout_sclk_peri_spi1_a", \
			EXYNOS7580_DIV_TOP_PERI1, 20, 8, \
			EXYNOS7580_DIV_STAT_TOP_PERI1, 20, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi2_b", "dout_sclk_peri_spi2_a", \
			EXYNOS7580_DIV_TOP_PERI2, 4, 8, \
			EXYNOS7580_DIV_STAT_TOP_PERI1, 4, 1, 0, NULL),
	DIV(baud0, "dout_sclk_peri_uart0", "mout_sclk_peri_uart0_ext_uclk", \
			EXYNOS7580_DIV_TOP_PERI3, 16, 4, \
			EXYNOS7580_DIV_STAT_TOP_PERI3, 16, 1, 0, "dout_sclk_peri_uart0"),
	DIV(baud1, "dout_sclk_peri_uart1", "mout_sclk_peri_uart1_ext_uclk", \
			EXYNOS7580_DIV_TOP_PERI3, 20, 4, \
			EXYNOS7580_DIV_STAT_TOP_PERI3, 20, 1, 0, "dout_sclk_peri_uart1"),
	DIV(baud2, "dout_sclk_peri_uart2", "mout_sclk_peri_uart2_ext_uclk", \
			EXYNOS7580_DIV_TOP_PERI3, 24, 4, \
			EXYNOS7580_DIV_STAT_TOP_PERI3, 24, 1, 0, "dout_sclk_peri_uart2"),
	/* BUS */
	DIV(none, "dout_pclk_bus0_100", "dout_aclk_bus0_400", \
			EXYNOS7580_DIV_BUS0, 0, 3, \
			EXYNOS7580_DIV_STAT_BUS0, 0, 1, 0, "dout_pclk_bus0_100"),
	DIV(none, "dout_pclk_bus1_100", "dout_aclk_bus1_400", \
			EXYNOS7580_DIV_BUS1, 0, 3, \
			EXYNOS7580_DIV_STAT_BUS1, 0, 1, 0, "dout_pclk_bus1_100"),
	DIV(none, "dout_pclk_bus2_100", "dout_aclk_bus2_400", \
			EXYNOS7580_DIV_BUS2, 0, 3, \
			EXYNOS7580_DIV_STAT_BUS2, 0, 1, 0, "dout_pclk_bus2_100"),
};

static struct samsung_usermux exynos7580_usermux_clks[] __initdata = {
	/* TOP */
	USERMUX(CLK_MUX_BUS_PLL_TOP_USER, "mout_bus_pll_top_user", "bus_pll", \
			EXYNOS7580_MUX_SEL_TOP1, 4, \
			EXYNOS7580_MUX_STAT_TOP1, 4, 0, "mout_bus_pll_top_user"),
	USERMUX(none, "mout_media_pll_top_user", "sclk_media_pll_top", \
			EXYNOS7580_MUX_SEL_TOP1, 8, \
			EXYNOS7580_MUX_STAT_TOP1, 8, 0, "mout_media_pll_top_user"),
	/* CPU */
	USERMUX(none, "mout_bus_pll_cpu_user", "bus_pll", \
			EXYNOS7580_MUX_SEL_CPU_1, 0, \
			EXYNOS7580_MUX_STAT_CPU_1, 0, 0, "mout_bus_pll_cpu_user"),
	/* APL */
	USERMUX(none, "mout_bus_pll_apl_user", "bus_pll", \
			EXYNOS7580_MUX_SEL_APL_1, 0, \
			EXYNOS7580_MUX_STAT_APL_1, 0, 0, "mout_bus_pll_apl_user"),
	/* ISP */
	USERMUX(none, "mout_aclk_isp_400_user", "aclk_isp_400", \
			EXYNOS7580_MUX_SEL_ISP1, 0, \
			EXYNOS7580_MUX_STAT_ISP1, 0, 0, "mout_aclk_isp_400_user"),
	USERMUX(none, "mout_aclk_isp_333_user", "aclk_isp_333", \
			EXYNOS7580_MUX_SEL_ISP1, 4, \
			EXYNOS7580_MUX_STAT_ISP1, 4, 0, "mout_aclk_isp_333_user"),
	USERMUX(none, "mout_aclk_isp_266_user", "aclk_isp_266", \
			EXYNOS7580_MUX_SEL_ISP1, 8, \
			EXYNOS7580_MUX_STAT_ISP1, 8, 0, "mout_aclk_isp_266_user"),
	USERMUX(none, "mout_sclk_spi0_isp_ext_clk_user", "sclk_isp_spi0_ext_clk", \
			EXYNOS7580_MUX_SEL_ISP1, 12, \
			EXYNOS7580_MUX_STAT_ISP1, 12, 0, "mout_sclk_spi0_isp_ext_clk_user"),
	USERMUX(none, "mout_sclk_spi1_isp_ext_clk_user", "sclk_isp_spi1_ext_clk", \
			EXYNOS7580_MUX_SEL_ISP1, 16, \
			EXYNOS7580_MUX_STAT_ISP1, 16, 0, "mout_sclk_spi1_isp_ext_clk_user"),
	USERMUX(none, "mout_sclk_uart_isp_ext_clk_user", "sclk_isp_uart_ext_clk", \
			EXYNOS7580_MUX_SEL_ISP1, 20, \
			EXYNOS7580_MUX_STAT_ISP1, 20, 0, "mout_sclk_uart_isp_ext_clk_user"),
	USERMUX(none, "mout_phyclk_csi_link0_rx_user", "phyclk_csi_phy0_rxbyteclkhs0", \
			EXYNOS7580_MUX_SEL_ISP2, 0, \
			EXYNOS7580_MUX_STAT_ISP2, 0, 0, "mout_phyclk_csi_link0_rx_user"),
	USERMUX(none, "mout_phyclk_csi_link1_rx_user", "phyclk_csi_phy1_rxbyteclkhs0", \
			EXYNOS7580_MUX_SEL_ISP2, 4, \
			EXYNOS7580_MUX_STAT_ISP2, 4, 0, "mout_phyclk_csi_link1_rx_user"),
	/* MFCMSCL */
	USERMUX(CLK_MOUT_ACLK_MSCL_400_USER, "mout_aclk_mscl_400_user", "aclk_mfcmscl_400", \
			EXYNOS7580_MUX_SEL_MFCMSCL, 4, \
			EXYNOS7580_MUX_STAT_MFCMSCL, 4, 0, "mout_aclk_mscl_400_user"),
	USERMUX(CLK_MOUT_ACLK_MFC_266_USER, "mout_aclk_mfc_266_user", "aclk_mfcmscl_266", \
			EXYNOS7580_MUX_SEL_MFCMSCL, 0, \
			EXYNOS7580_MUX_STAT_MFCMSCL, 4, 0, "mout_aclk_mfc_266_user"),
	/* DISP */
	USERMUX(CLK_MUX_ACLK_DISP_200_USER, "mout_aclk_disp_200_user", "aclk_disp_200", \
			EXYNOS7580_MUX_SEL_DISP1, 0, \
			EXYNOS7580_MUX_STAT_DISP1, 0, 0, "mout_aclk_disp_200_user"),
	USERMUX(CLK_MUX_SCLK_DECON_INT_ECLK_USER, "mout_sclk_decon_int_eclk_user", "sclk_disp_decon_int_eclk", \
			EXYNOS7580_MUX_SEL_DISP1, 8, \
			EXYNOS7580_MUX_STAT_DISP1, 8, 0, "mout_sclk_decon_int_eclk_user"),
	USERMUX(CLK_MUX_SCLK_DECON_INT_VCLK_USER, "mout_sclk_decon_int_vclk_user", "sclk_disp_decon_int_vclk", \
			EXYNOS7580_MUX_SEL_DISP1, 12, \
			EXYNOS7580_MUX_STAT_DISP1, 12, 0, "mout_sclk_decon_int_vclk_user"),
	USERMUX(CLK_MUX_PHYCLK_BITCLKDIV8_USER, "mout_phyclk_bitclkdiv8_user", "phyclk_txbyteclkhs_m4s4", \
			EXYNOS7580_MUX_SEL_DISP2, 16, \
			0, 0, 0, "mout_phyclk_bitclkdiv8_user"),
	USERMUX(CLK_MUX_PHYCLK_RXCLKESC0_USER, "mout_phyclk_rxclkesc0_user", "phyclk_rxclkesc0_m4s4", \
			EXYNOS7580_MUX_SEL_DISP2, 20, \
			0, 0, 0, "mout_phyclk_rxclkesc0_user"),
	/* FSYS */
	USERMUX(none, "mout_phyclk_usbhost20_freeclk_user", "phyclk_usbhost20_freeclk", \
			EXYNOS7580_MUX_SEL_FSYS1, 8, \
			EXYNOS7580_MUX_STAT_FSYS1, 8, 0, "mout_phyclk_usb20_freeclk_user"),
	USERMUX(none, "mout_phyclk_usbhost20_phyclock_user", "phyclk_usbhost20_phyclock", \
			EXYNOS7580_MUX_SEL_FSYS1, 12, \
			EXYNOS7580_MUX_STAT_FSYS1, 12, 0, "mout_phyclk_usb20_phyclock_user"),
	USERMUX(none, "mout_phyclk_usbhost20_clk48mohcl_user", "phyclk_usbhost20_clk48mohcl", \
			EXYNOS7580_MUX_SEL_FSYS1, 16, \
			EXYNOS7580_MUX_STAT_FSYS1, 16, 0, "mout_phyclk_usb20_clk48mohcl_user"),
	USERMUX(phy_otg, "mout_phyclk_usbotg20", "phyclk_usbotg20", \
			EXYNOS7580_MUX_SEL_FSYS2, 12, \
			EXYNOS7580_MUX_STAT_FSYS2, 12, 0, "mout_phyclk_uhost20_phyclock_1_user"),
	/* AUD */
	USERMUX(mout_aud_pll_user, "mout_aud_pll_user", "aud_pll", \
			EXYNOS7580_MUX_SEL_AUD0, 0, \
			EXYNOS7580_MUX_STAT_AUD0, 0, 0, NULL),
};

static struct samsung_gate exynos7580_gate_clks[] __initdata = {
	/* TOP */
	GATE(none, "sclk_media_pll_top", "mout_media_pll_div2", \
			EXYNOS7580_EN_SCLK_TOP, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_fsys_200", "dout_aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_266", "dout_aclk_imem_266", \
			EXYNOS7580_EN_ACLK_TOP, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200", "dout_aclk_imem_200", \
			EXYNOS7580_EN_ACLK_TOP, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_g3d_400, "aclk_g3d_400", "dout_aclk_g3d_400", \
			EXYNOS7580_EN_ACLK_TOP, 14, 0, NULL),
	GATE(none, "aclk_isp_400", "dout_aclk_isp_400", \
			EXYNOS7580_EN_ACLK_TOP, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_isp_333", "dout_aclk_isp_333", \
			EXYNOS7580_EN_ACLK_TOP, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_isp_266", "dout_aclk_isp_266_top", \
			EXYNOS7580_EN_ACLK_TOP, 11, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_ACLK_MFCMSCL_266, "aclk_mfcmscl_266", "dout_aclk_mfcmscl_266", \
			EXYNOS7580_EN_ACLK_TOP, 12, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_ACLK_MFCMSCL_400, "aclk_mfcmscl_400", "dout_aclk_mfcmscl_400", \
			EXYNOS7580_EN_ACLK_TOP, 13, CLK_IGNORE_UNUSED, NULL),
	/* G3D */
	GATE(g3d, "aclk_g3d", "dout_aclk_g3d_600", \
			EXYNOS7580_EN_ACLK_G3D, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_bus_d_g3d", "dout_aclk_g3d_600", \
			EXYNOS7580_EN_ACLK_G3D, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_async_ahb2apb_g3d", "dout_aclk_g3d_600", \
			EXYNOS7580_EN_ACLK_G3D, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_qe_g3d", "dout_aclk_g3d_600", \
			EXYNOS7580_EN_ACLK_G3D, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_ppmu_g3d", "dout_aclk_g3d_600", \
			EXYNOS7580_EN_ACLK_G3D, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_async_ahb2apb_g3d", "dout_pclk_g3d_150", \
			EXYNOS7580_EN_PCLK_G3D, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_pmu_g3d", "dout_pclk_g3d_150", \
			EXYNOS7580_EN_PCLK_G3D, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_sysreg_g3d", "dout_pclk_g3d_150", \
			EXYNOS7580_EN_PCLK_G3D, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_ge_g3d", "dout_pclk_g3d_150", \
			EXYNOS7580_EN_PCLK_G3D, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_ppmu_g3d", "dout_pclk_g3d_150", \
			EXYNOS7580_EN_PCLK_G3D, 7, CLK_IGNORE_UNUSED, NULL),
	/* DISP */
	GATE(CLK_ACLK_DECON0, "aclk_decon_int", "mout_aclk_disp_200_user", \
			EXYNOS7580_EN_ACLK_DISP0, 0, 0, NULL),
	GATE(CLK_ACLK_XIU_DISP1, "aclk_xiu_disp1", "mout_aclk_disp_200_user", \
			EXYNOS7580_EN_ACLK_DISP1, 0, 0, NULL),
	GATE(CLK_ACLK_SMMU_DISP_MMU, "aclk_smmu_disp_mmu", "mout_aclk_disp_200_user", \
			EXYNOS7580_EN_ACLK_DISP1, 9, 0, NULL),
	GATE(CLK_PCLK_DECON_INT, "pclk_decon_int", "dout_pclk_disp_100", \
			EXYNOS7580_EN_PCLK_DISP, 0, 0, NULL),
	GATE(CLK_PCLK_DSI_LINK0, "pclk_dsi_link0", "dout_pclk_disp_100", \
			EXYNOS7580_EN_PCLK_DISP, 4, 0, NULL),
	GATE(CLK_PCLK_SMMU_DISP_MMU, "pclk_smmu_disp_mmu", "dout_pclk_disp_100", \
			EXYNOS7580_EN_PCLK_DISP, 11, 0, NULL),
	GATE(CLK_DIV_ACLK_DISP_200, "aclk_disp_200", "dout_aclk_disp_200", \
			EXYNOS7580_EN_ACLK_TOP_DISP, 0, 0, NULL),
	GATE(CLK_SCLK_DISP_DECON_INT_ECLK, "sclk_disp_decon_int_eclk", "dout_sclk_disp_decon_int_eclk", \
			EXYNOS7580_EN_SCLK_TOP_DISP, 0, 0, NULL),
	GATE(CLK_SCLK_DISP_DECON_INT_VCLK, "sclk_disp_decon_int_vclk", "dout_sclk_disp_decon_int_vclk", \
			EXYNOS7580_EN_SCLK_TOP_DISP, 1, 0, NULL),
	GATE(CLK_SCLK_DECON_INT_ECLK, "sclk_decon_int_eclk", "dout_sclk_decon_int_eclk", \
			EXYNOS7580_EN_SCLK_DISP, 0, 0, NULL),
	GATE(CLK_SCLK_DECON_INT_VCLK, "sclk_decon_int_vclk", "dout_sclk_decon_int_vclk", \
			EXYNOS7580_EN_SCLK_DISP, 1, 0, NULL),
	GATE(CLK_SCLK_DSI_LINK0_I_RGB_VCLK, "sclk_dsi_link0_i_rgb_vclk", "sclk_decon_int_rgb_vclk", \
			EXYNOS7580_EN_SCLK_DISP, 5, 0, NULL),
	GATE(CLK_PHYCLK_BITCLKDIV8, "phyclk_bitclkdiv8", "mout_phyclk_bitclkdiv8_user", \
			EXYNOS7580_EN_SCLK_DISP, 9, 0, NULL),
	GATE(CLK_PHYCLK_RXCLKESC0, "phyclk_rxclkesc0", "mout_phyclk_rxclkesc0_user", \
			EXYNOS7580_EN_SCLK_DISP, 10, 0, NULL),
	/* FSYS */
	GATE(pdma0, "aclk_dma_fsys0", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS0, 0, 0, NULL),
	GATE(pdma1, "aclk_dma_fsys1", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS0, 1, 0, NULL),
	GATE(none, "aclk_usbhost20", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS0, 3, 0, NULL),
	GATE(aclk_mmc0, "aclk_mmc0", "aclk_fsys_200",
			EXYNOS7580_EN_ACLK_FSYS0, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_mmc1, "aclk_mmc1", "aclk_fsys_200",
			EXYNOS7580_EN_ACLK_FSYS0, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_mmc2, "aclk_mmc2", "aclk_fsys_200",
			EXYNOS7580_EN_ACLK_FSYS0, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(otg_aclk, "aclk_usbotg20", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS0, 16, CLK_IGNORE_UNUSED, NULL),
	GATE(otg_hclk, "aclk_ahb2axi_usbotg", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS0, 17, CLK_IGNORE_UNUSED, NULL),
	GATE(upsizer_otg, "aclk_upsizer_usbotg", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS0, 18, CLK_IGNORE_UNUSED, NULL),
	GATE(xiu_d_fsys1, "aclk_xiu_d_fsys1", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS1, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(ahb_usbhs, "aclk_ahb_usbhs", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS1, 8, 0, NULL),
	GATE(ahb2axi_usbhs, "aclk_ahb2axi_usbhs", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS1, 17, 0, NULL),
	GATE(upsizer_fsys1, "aclk_upsizer_fsys1_to_fsys0", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS2, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_upsizer_dma_fsys0", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS2, 6, 0, NULL),
	GATE(upsizer_ahb_usbhs, "aclk_upsizer_ahb_usbhs", "aclk_fsys_200", \
			EXYNOS7580_EN_ACLK_FSYS2, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_mmc0, "sclk_mmc0_sdclkin", "sclk_fsys_mmc0_sdclkin",
			EXYNOS7580_EN_SCLK_FSYS, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_mmc1, "sclk_mmc1_sdclkin", "sclk_fsys_mmc1_sdclkin",
			EXYNOS7580_EN_SCLK_FSYS, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_mmc2, "sclk_mmc2_sdclkin", "sclk_fsys_mmc2_sdclkin",
			EXYNOS7580_EN_SCLK_FSYS, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_fsys_mmc0, "sclk_fsys_mmc0_sdclkin", "dout_sclk_fsys_mmc0_b",
			EXYNOS7580_EN_SCLK_TOP_FSYS, 0,
			CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_fsys_mmc1, "sclk_fsys_mmc1_sdclkin", "dout_sclk_fsys_mmc1_b",
			EXYNOS7580_EN_SCLK_TOP_FSYS, 1,
			CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_fsys_mmc2, "sclk_fsys_mmc2_sdclkin", "dout_sclk_fsys_mmc2_b",
			EXYNOS7580_EN_SCLK_TOP_FSYS, 2,
			CLK_IGNORE_UNUSED, NULL),
	GATE(usb_pll, "sclk_usb20_phy_clkcore", "usb_pll", \
			EXYNOS7580_EN_SCLK_FSYS, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(freeclk, "phyclk_usbhot20_freeclk", "mout_phyclk_usbhost20_freeclk_user", \
			EXYNOS7580_EN_SCLK_FSYS, 14, 0, NULL),
	GATE(phyclk, "phyclk_usbhost20_usb20_phyclock", "mout_phyclk_usbhost20_phyclock_user", \
			EXYNOS7580_EN_SCLK_FSYS, 15, 0, NULL),
	GATE(clk48mohci, "phyclk_usbhost20_clk48mohci", "mout_phyclk_usbhost20_clk48mohcl_user", \
			EXYNOS7580_EN_SCLK_FSYS, 16, 0, NULL),
	GATE(none, "phyclk_usbotg20_otg20_phyclock", "mout_phyclk_usbotg20", \
			EXYNOS7580_EN_SCLK_FSYS, 22, CLK_IGNORE_UNUSED, NULL),
	/* IMEM */
	GATE(none, "aclk_intc_cpu", "aclk_imem_200", \
			EXYNOS7580_EN_ACLK_IMEM, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_iramc_top", "aclk_imem_200", \
			EXYNOS7580_EN_ACLK_IMEM_SEC_IRAMC_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_downsizer_gic", "aclk_imem_200", \
			EXYNOS7580_EN_ACLK_IMEM, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_pmu_imem", "aclk_imem_200", \
			EXYNOS7580_EN_PCLK_IMEM, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_sysreg_imem", "aclk_imem_200", \
			EXYNOS7580_EN_PCLK_IMEM, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_sss", "aclk_imem_200", \
			EXYNOS7580_EN_PCLK_IMEM_SEC_SSS, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_rtic", "aclk_imem_200", \
			EXYNOS7580_EN_PCLK_IMEM_SEC_RTIC, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_ppmu_imem", "aclk_imem_200", \
			EXYNOS7580_EN_PCLK_IMEM, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_masync_xiu_d_to_p_imem", "aclk_imem_200", \
			EXYNOS7580_EN_ACLK_IMEM, 12, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_sss", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM_SEC_SSS, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_rtic", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM_SEC_RTIC, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_xiu_d_imem", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_bus_d_imem", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_asyncahbm_sss_atlas", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_ppmu_imem", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_sasync_xiu_d_to_p_imem", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM, 11, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_downsizer_async", "aclk_imem_266", \
			EXYNOS7580_EN_ACLK_IMEM, 13, CLK_IGNORE_UNUSED, NULL),
	/* PERI */
	GATE(none, "aclk_ahb2apb_peris0", "dout_aclk_peri_66", \
			EXYNOS7580_EN_ACLK_PERI, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_ahb2apb_peris1", "dout_aclk_peri_66", \
			EXYNOS7580_EN_ACLK_PERI, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc0", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc1", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc2", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc3", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc4", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc5", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc6", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc7", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc8", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc9", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tzpc10", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_TZPC, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(mct, "pclk_mct", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(pclk_wdt, "pclk_wdt_cpu", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_rtc_top", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_RTC_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_chipid_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_CHIPID, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_seckey_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_SECKEY, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cmu_top_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_abb_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_antirbk_cnt_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_ANTIRBK, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_efuse_writer_sc_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_custom_efuse_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_monotonic_cnt_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_MONOTONIC, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(gate_rtc, "pclk_rtc_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_RTC_APBIF, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_ahb2apb_peric0", "dout_aclk_peri_66", \
			EXYNOS7580_EN_ACLK_PERI, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(i2c1, "pclk_i2c1", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(i2c2, "pclk_i2c2", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(i2c3, "pclk_i2c3", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(puart0, "pclk_uart0", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 15, CLK_IGNORE_UNUSED, "console-pclk0"),
	GATE(puart1, "pclk_uart1", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 16, CLK_IGNORE_UNUSED, "console-pclk1"),
	GATE(puart2, "pclk_uart2", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 17, CLK_IGNORE_UNUSED, "console-pclk2"),
	GATE(pclk_adcif, "pclk_adcif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 18, CLK_IGNORE_UNUSED, NULL),
	GATE(pclk_spi0, "pclk_spi0", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 19, CLK_IGNORE_UNUSED, NULL),
	GATE(pclk_spi1, "pclk_spi1", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 20, CLK_IGNORE_UNUSED, NULL),
	GATE(pclk_spi2, "pclk_spi2", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 21, CLK_IGNORE_UNUSED, NULL),
	GATE(pclk_pwm, "pwm-clock", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 26, CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT, NULL),
	GATE(none, "pclk_tmu0_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_tmu1_apbif", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIS, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_gpio_peri", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_gpio_nfc", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_gpio_touch", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_gpio_alive", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_peri_i2s_i2scodclki", "dout_sclk_peri_i2s", \
			EXYNOS7580_EN_SCLK_TOP_PERI, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_spi0, "sclk_spi0_ext_clk", "sclk_peri_spi0_ext_clk", \
			EXYNOS7580_EN_SCLK_PERIC, 3, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_spi1, "sclk_spi1_ext_clk", "sclk_peri_spi1_ext_clk", \
			EXYNOS7580_EN_SCLK_PERIC, 4, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_spi2, "sclk_spi2_ext_clk", "sclk_peri_spi2_ext_clk", \
			EXYNOS7580_EN_SCLK_PERIC, 5, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "sclk_peri_spi0_ext_clk", "dout_sclk_peri_spi0_b", \
			EXYNOS7580_EN_SCLK_TOP_PERI, 2, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "sclk_peri_spi1_ext_clk", "dout_sclk_peri_spi1_b", \
			EXYNOS7580_EN_SCLK_TOP_PERI, 3, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "sclk_peri_spi2_ext_clk", "dout_sclk_peri_spi2_b", \
			EXYNOS7580_EN_SCLK_TOP_PERI, 4, CLK_SET_RATE_PARENT, NULL),
	GATE(suart0, "sclk_uart0", "sclk_peri_uart0_ext_uclk", \
			EXYNOS7580_EN_SCLK_PERIC, 0, CLK_IGNORE_UNUSED, "console-sclk0"),
	GATE(suart1, "sclk_uart1", "sclk_peri_uart1_ext_uclk", \
			EXYNOS7580_EN_SCLK_PERIC, 1, CLK_IGNORE_UNUSED, "console-sclk1"),
	GATE(suart2, "sclk_uart2", "sclk_peri_uart2_ext_uclk", \
			EXYNOS7580_EN_SCLK_PERIC, 2, CLK_IGNORE_UNUSED, "console-sclk2"),
	GATE(none, "sclk_peri_uart0_ext_uclk", "dout_sclk_peri_uart0", \
			EXYNOS7580_EN_SCLK_TOP_PERI, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_peri_uart1_ext_uclk", "dout_sclk_peri_uart1", \
			EXYNOS7580_EN_SCLK_TOP_PERI, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_peri_uart2_ext_uclk", "dout_sclk_peri_uart2", \
			EXYNOS7580_EN_SCLK_TOP_PERI, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_spi0", "ioclk_peri_spi", \
			EXYNOS7580_EN_SCLK_PERIC, 12, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_spi1", "ioclk_peri_spi", \
			EXYNOS7580_EN_SCLK_PERIC, 13, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_spi2", "ioclk_peri_spi", \
			EXYNOS7580_EN_SCLK_PERIC, 14, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_adcif_i_osc_sys", "fin_pll", \
			EXYNOS7580_EN_SCLK_PERIC, 17, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_pwm_tclk0", "fin_pll", \
			EXYNOS7580_EN_SCLK_PERIC, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_abb", "fin_pll", \
			EXYNOS7580_EN_PCLK_PERIS, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_tmu0", "fin_pll", \
			EXYNOS7580_EN_PCLK_PERIS, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_tmu1", "fin_pll", \
			EXYNOS7580_EN_PCLK_PERIS, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_chipid", "fin_pll", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_CHIPID, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_seckey", "fin_pll", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_SECKEY, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_antirbk_cnt", "fin_pll", \
			EXYNOS7580_EN_PCLK_PERIS_SEC_ANTIRBK, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_efuse_writer", "fin_pll", \
			EXYNOS7580_EN_SCLK_PERIS, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_custom_efuse", "fin_pll", \
			EXYNOS7580_EN_SCLK_PERIS, 4, CLK_IGNORE_UNUSED, NULL),
	/* AUD */
	GATE(lpass_dmac, "aclk_lpass_dmac", "dout_aclk_aud_133", \
			EXYNOS7580_EN_ACLK_AUD, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(lpass_mem, "aclk_lpass_mem", "dout_aclk_aud_133", \
			EXYNOS7580_EN_ACLK_AUD, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_sasync_xiu_aud_to_mif", "dout_aclk_aud_133", \
			EXYNOS7580_EN_ACLK_AUD, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_upsizer_aud_64to128", "dout_aclk_aud_133", \
			EXYNOS7580_EN_ACLK_AUD, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(pclk_mi2s, "pclk_mi2s_aud", "dout_aclk_aud_133", \
			EXYNOS7580_EN_PCLK_AUD, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_pcm_aud", "dout_aclk_aud_133", \
			EXYNOS7580_EN_PCLK_AUD, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_uart_aud", "dout_aclk_aud_133", \
			EXYNOS7580_EN_PCLK_AUD, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_lpass_sfr", "dout_aclk_aud_133", \
			EXYNOS7580_EN_PCLK_AUD, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_gpio_aud", "dout_aclk_aud_133", \
			EXYNOS7580_EN_PCLK_AUD, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_mi2s, "sclk_mi2s_aud_i2scodclki", "dout_sclk_mi2s_aud", \
			EXYNOS7580_EN_SCLK_AUD, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_pcm, "sclk_pcm_aud", "dout_sclk_pcm_aud", \
			EXYNOS7580_EN_SCLK_AUD, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_uart_aud", "dout_sclk_uart_aud", \
			EXYNOS7580_EN_SCLK_AUD, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(audmixer_sysclk, "sclk_audmixer", "dout_sclk_audmixer_aud", \
			EXYNOS7580_EN_SCLK_AUD, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(audmixer_bclk0, "sclk_audmixer_bclk0", "ioclk_audmix_bclk", \
			EXYNOS7580_EN_SCLK_AUD, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(audmixer_bclk1, "sclk_audmixer_bclk1", "ioclk_audmix_bclk", \
			EXYNOS7580_EN_SCLK_AUD, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(audmixer_bclk2, "sclk_audmixer_bclk2", "ioclk_audmix_bclk", \
			EXYNOS7580_EN_SCLK_AUD, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(mi2s_aud_bclk, "sclk_mi2s_aud_i2sbclki", "ioclk_audi2s0sclk", \
			EXYNOS7580_EN_SCLK_AUD, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(hsi2c0, "pclk_hsi2c0", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 11, CLK_IGNORE_UNUSED, NULL),
	GATE(hsi2c1, "pclk_hsi2c1", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 12, CLK_IGNORE_UNUSED, NULL),
	GATE(hsi2c2, "pclk_hsi2c2", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 13, CLK_IGNORE_UNUSED, NULL),
	GATE(hsi2c3, "pclk_hsi2c3", "dout_aclk_mif_fix_100", \
			EXYNOS7580_EN_PCLK_MIF, 27, CLK_IGNORE_UNUSED, NULL),
	GATE(i2c0, "pclk_i2c0", "dout_aclk_peri_66", \
			EXYNOS7580_EN_PCLK_PERIC, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_isp_spi0_ext_clk", "dout_sclk_isp_spi0_ext_clk_b", \
			EXYNOS7580_EN_SCLK_TOP_ISP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_isp_spi1_ext_clk", "dout_sclk_isp_spi1_ext_clk_b", \
			EXYNOS7580_EN_SCLK_TOP_ISP, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_isp_uart_ext_clk", "dout_sclk_isp_uart_ext_uclk", \
			EXYNOS7580_EN_SCLK_TOP_ISP, 2, CLK_IGNORE_UNUSED, NULL),
	/* MFCMSCL */
	GATE(GATE_ACLK_M2M_SCALER0, "aclk_m2m_scaler0", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(GATE_ACLK_M2M_SCALER1, "aclk_m2m_scaler1", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_ACLK_JPEG, "aclk_jpeg", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_xiu_d_mscl", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_bus_d_mscl", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_masync_ahb_jpeg", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_upsizer_xiu_d_mscl", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_ACLK_SMMU_MSCL_MMU, "aclk_smmu_mscl_mmu", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_ppmu_mscl", "mout_aclk_mscl_400_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_ahb2apb_mfc", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_sasync_ahb_jpeg", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 12, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_ahb_jpeg", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(GATE_PCLK_M2M_SCALER0, "pclk_m2m_scaler0", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(GATE_PCLK_M2M_SCALER1, "pclk_m2m_scaler1", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_PCLK_SMMU_MSCL_MMU, "pclk_smmu_mscl_mmu", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_ppmu_mscl", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_PCLK_SMMU_MFC_MMU, "pclk_smmu_mfc_mmu", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 17, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_ppmu_mfc", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 18, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_sasync_ahb2apb_mfc", "dout_pclk_mfcmscl_100", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 19, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_ACLK_MFC, "aclk_mfc", "mout_aclk_mfc_266_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 16, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_mfc", "mout_aclk_mfc_266_user", \
			EXYNOS7580_EN_PCLK_MFCMSCL, 16, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_bus_d_mfc", "mout_aclk_mfc_266_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 17, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_upsizer_mfc", "mout_aclk_mfc_266_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 18, CLK_IGNORE_UNUSED, NULL),
	GATE(CLK_ACLK_SMMU_MFC_MMU, "aclk_smmu_mfc_mmu", "mout_aclk_mfc_266_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 19, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_ppmu_mfc", "mout_aclk_mfc_266_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 20, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_sasync_ahb2apb_mfc", "mout_aclk_mfc_266_user", \
			EXYNOS7580_EN_ACLK_MFCMSCL, 13, CLK_IGNORE_UNUSED, NULL),
};

static void __init exynos7580_clk_init(struct device_node *np)
{
	if (!np)
		panic("%s: unable to determine SoC\n", __func__);
	/*
	 * Register clocks for exynos7580 series.
	 * Gate clocks should be registered at last because of some gate clocks.
	 * Some gate clocks should be enabled at initial time.
	 */
	samsung_clk_init(np, 0, nr_clks, (unsigned long *)exynos7580_clk_regs,
			ARRAY_SIZE(exynos7580_clk_regs), NULL, 0);
	samsung_register_of_fixed_ext(exynos7580_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos7580_fixed_rate_ext_clks),
			ext_clk_match);
	samsung_register_comp_pll(exynos7580_pll_clks,
			ARRAY_SIZE(exynos7580_pll_clks));
	samsung_register_fixed_rate(exynos7580_fixed_rate_clks,
			ARRAY_SIZE(exynos7580_fixed_rate_clks));
	samsung_register_fixed_factor(exynos7580_fixed_factor_clks,
			ARRAY_SIZE(exynos7580_fixed_factor_clks));
	samsung_register_comp_mux(exynos7580_mux_clks,
			ARRAY_SIZE(exynos7580_mux_clks));
	samsung_register_comp_divider(exynos7580_div_clks,
			ARRAY_SIZE(exynos7580_div_clks));
	samsung_register_usermux(exynos7580_usermux_clks,
			ARRAY_SIZE(exynos7580_usermux_clks));
	samsung_register_gate(exynos7580_gate_clks,
			ARRAY_SIZE(exynos7580_gate_clks));
	pr_info("EXYNOS7580: Clock setup completed\n");
}
CLK_OF_DECLARE(exynos7580_clks, "samsung,exynos7580-clock", exynos7580_clk_init);
