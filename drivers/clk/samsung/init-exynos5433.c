/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5433 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock-exynos5433.h>
#include <mach/regs-pmu.h>
#include <mach/map.h>

#include "clk.h"
#include "clk-pll.h"

static void usb_init_clock(void)
{
	exynos_set_parent("mout_sclk_usbdrd30_user", "oscclk");

	exynos_set_parent("mout_phyclk_usbdrd30_udrd30_phyclock",
			"phyclk_usbdrd30_udrd30_phyclock_phy");
	exynos_set_parent("mout_phyclk_usbdrd30_udrd30_pipe_pclk",
			"phyclk_usbdrd30_udrd30_pipe_pclk_phy");

	exynos_set_parent("mout_sclk_usbhost30_user", "oscclk");

	exynos_set_parent("mout_phyclk_usbhost30_uhost30_phyclock",
			"phyclk_usbhost30_uhost30_phyclock_phy");
	exynos_set_parent("mout_phyclk_usbhost30_uhost30_pipe_pclk",
			"phyclk_usbhost30_uhost30_pipe_pclk_phy");

	exynos_set_parent("mout_phyclk_usbhost20_phy_freeclk",
			"phyclk_usbhost20_phy_freeclk_phy");
	exynos_set_parent("mout_phyclk_usbhost20_phy_phyclock",
			"phyclk_usbhost20_phy_phyclock_phy");
	exynos_set_parent("mout_phyclk_usbhost20_phy_clk48mohci",
			"phyclk_usbhost20_phy_clk48mohci_phy");
	exynos_set_parent("mout_phyclk_usbhost20_phy_hsic1",
			"phyclk_usbhost20_phy_hsic1_phy");
}

void crypto_init_clock(void)
{
	exynos_set_rate("dout_aclk_imem_sssx_266", 160 * 1000000);
	exynos_set_rate("dout_aclk_imem_200", 160 * 1000000);
}

static void pcie_init_clock(void)
{
	exynos_set_parent("mout_sclk_pcie_100", "mout_bus_pll_user");
	exynos_set_parent("dout_sclk_pcie_100", "mout_sclk_pcie_100");
	exynos_set_parent("sclk_pcie_100_fsys", "dout_sclk_pcie_100");
	exynos_set_parent("mout_sclk_pcie_100_user", "sclk_pcie_100_fsys");
	exynos_set_rate("dout_sclk_pcie_100", 100000000);
}

void g2d_init_clock(void)
{
	int clk_rate1;
	int clk_rate2;

	if (exynos_set_parent("mout_aclk_g2d_400_a", "mout_bus_pll_user"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_a", "mout_bus_pll_user");

	if (exynos_set_parent("mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a");

	if (exynos_set_parent("mout_aclk_g2d_400_user", "aclk_g2d_400"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_user", "aclk_g2d_400");

	if (exynos_set_parent("mout_aclk_g2d_266_user", "aclk_g2d_266"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_266_user", "aclk_g2d_266");

	if (exynos_set_rate("dout_aclk_g2d_400", 413 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_400");

	if (exynos_set_rate("dout_aclk_g2d_266", 276 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_266");

	clk_rate1 = exynos_get_rate("aclk_g2d_400");
	clk_rate2 = exynos_get_rate("aclk_g2d_266");

	pr_info("[%s:%d] aclk_g2d_400:%d, aclk_g2d_266:%d\n"
			, __func__, __LINE__, clk_rate1, clk_rate2);
}

void jpeg_init_clock(void)
{
	if (exynos_set_parent("mout_sclk_jpeg_a", "mout_bus_pll_user"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_bus_pll_user", "mout_sclk_jpeg_a");

	if (exynos_set_parent("mout_sclk_jpeg_b", "mout_sclk_jpeg_a"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpog_a", "mout_sclk_jpeg_b");

	if (exynos_set_parent("mout_sclk_jpeg_c", "mout_sclk_jpeg_b"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_b", "mout_sclk_jpeg_c");

	if (exynos_set_parent("dout_sclk_jpeg", "mout_sclk_jpeg_c"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_c", "dout_sclk_jpeg");

	if (exynos_set_parent("sclk_jpeg_mscl", "dout_sclk_jpeg"))
		pr_err("Unable to set parent %s of clock %s\n",
				"dout_sclk_jpeg", "sclk_jpeg_mscl");

	if (exynos_set_parent("mout_sclk_jpeg_user", "sclk_jpeg_mscl"))
		pr_err("Unable to set parent %s of clock %s\n",
				"sclk_jpeg_mscl", "mout_sclk_jpeg_user");

	if (exynos_set_rate("dout_sclk_jpeg", 413 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_sclk_jpeg");

	pr_debug("jpeg: sclk_jpeg %d\n", exynos_get_rate("dout_sclk_jpeg"));
}

static void clkout_init_clock(void)
{
	writel(0x1000, EXYNOS_PMU_PMU_DEBUG);
}

static void aud_init_clock(void)
{
	/* AUD0 */
	exynos_set_parent("mout_aud_pll_user", "fin_pll");

	/* AUD1 */
	exynos_set_parent("mout_sclk_aud_i2s", "mout_aud_pll_user");
	exynos_set_parent("mout_sclk_aud_pcm", "mout_aud_pll_user");

	exynos_set_rate("fout_aud_pll", 196608010);
	exynos_set_rate("dout_aud_ca5", 196608010);
	exynos_set_rate("dout_aclk_aud", 65536010);
	exynos_set_rate("dout_pclk_dbg_aud", 32768010);

	exynos_set_rate("dout_sclk_aud_i2s", 49152004);
	exynos_set_rate("dout_sclk_aud_pcm", 2048002);
	exynos_set_rate("dout_sclk_aud_slimbus", 24576002);
	exynos_set_rate("dout_sclk_aud_uart", 196608010);

	/* TOP1 */
	exynos_set_parent("mout_aud_pll", "fin_pll");
	exynos_set_parent("mout_aud_pll_user_top", "mout_aud_pll");

	/* TOP_PERIC1 */
	exynos_set_parent("mout_sclk_audio0", "mout_aud_pll_user_top");
	exynos_set_parent("mout_sclk_audio1", "mout_aud_pll_user_top");
	exynos_set_parent("mout_sclk_spdif", "dout_sclk_audio0");
	exynos_set_rate("dout_sclk_audio0", 24576002);
	exynos_set_rate("dout_sclk_audio1", 49152004 * 2);
	exynos_set_rate("dout_sclk_pcm1", 2048002);
	exynos_set_rate("dout_sclk_i2s1", 49152004);
}

static void spi_init_clock(void)
{
	exynos_set_parent("mout_sclk_spi0", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi1", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi2", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi3", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi4", "mout_bus_pll_user");

	/* dout_sclk_spi_a should be 100Mhz */
	exynos_set_rate("dout_sclk_spi0_a", 100000000);
	exynos_set_rate("dout_sclk_spi1_a", 100000000);
	exynos_set_rate("dout_sclk_spi2_a", 100000000);
	exynos_set_rate("dout_sclk_spi3_a", 100000000);
	exynos_set_rate("dout_sclk_spi4_a", 100000000);
}

static void uart_init_clock(void)
{
	/* Set dout_sclk_uart to 200Mhz */
	if (CONFIG_S3C_LOWLEVEL_UART_PORT != 0) {
		exynos_set_parent("mout_sclk_uart0", "mout_bus_pll_user");
		exynos_set_rate("dout_sclk_uart0", 200000000);
	}
	if (CONFIG_S3C_LOWLEVEL_UART_PORT != 1) {
		exynos_set_parent("mout_sclk_uart1", "mout_bus_pll_user");
		exynos_set_rate("dout_sclk_uart1", 200000000);
	}
	if (CONFIG_S3C_LOWLEVEL_UART_PORT != 2) {
		exynos_set_parent("mout_sclk_uart2", "mout_bus_pll_user");
		exynos_set_rate("dout_sclk_uart2", 200000000);
	}

}

static void isp_init_clock(void)
{
	/* Turn Off ISP PLL on Exynos5433 Init Time */
	exynos_set_parent("mout_isp_pll", "fin_pll");
}

void pwm_init_clock(void)
{
	clk_register_fixed_factor(NULL, "pwm-clock",
			"pclk_pwm",CLK_SET_RATE_PARENT, 1, 1);
}

void __init exynos5433_clock_init(void)
{
	clkout_init_clock();
	aud_init_clock();
	usb_init_clock();
	crypto_init_clock();
	pcie_init_clock();
	g2d_init_clock();
	jpeg_init_clock();
	uart_init_clock();
	spi_init_clock();
	isp_init_clock();
	pwm_init_clock();

}
