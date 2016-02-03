/* linux/arch/arm/mach-exynos/setup-fimc-sensor.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_SOC_EXYNOS5422)
#include <mach/regs-clock-exynos5422.h>
#elif defined(CONFIG_SOC_EXYNOS5430)
#include <mach/regs-clock-exynos5430.h>
#elif defined(CONFIG_SOC_EXYNOS5433)
#include <mach/regs-clock-exynos5433.h>
#elif defined(CONFIG_SOC_EXYNOS7420)
#include <mach/regs-clock-exynos7420.h>
#endif

#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>
#include <mach/exynos-fimc-is-module.h>

char *clk_g_list[CLK_NUM] = {
	"cam_pll",
	"isp_pll",
	"dout_aclk_cam0_3aa0_690",
	"dout_aclk_cam0_3aa1_468",
	"dout_aclk_cam0_bnsa_690",
	"dout_aclk_cam0_bnsb_690",
	"dout_aclk_cam0_bnsd_690",
	"dout_aclk_cam0_csis0_690",
	"dout_aclk_cam0_csis1_174",
	"dout_aclk_cam0_nocp_133",
	"dout_aclk_cam0_trex_532",
	"dout_aclk_cam1_arm_668",
	"dout_aclk_cam1_bnscsis_133",
	"dout_aclk_cam1_busperi_334",
	"dout_aclk_cam1_nocp_133",
	"dout_aclk_cam1_sclvra_491",
	"dout_aclk_cam1_trex_532",
	"dout_aclk_isp0_isp0_590",
	"dout_aclk_isp0_tpu_590",
	"dout_aclk_isp0_trex_532",
	"dout_aclk_isp1_ahb_117",
	"dout_aclk_isp1_isp1_468",
	"dout_clkdiv_pclk_cam0_3aa0_345",
	"dout_clkdiv_pclk_cam0_3aa1_234",
	"dout_clkdiv_pclk_cam0_bnsa_345",
	"dout_clkdiv_pclk_cam0_bnsb_345",
	"dout_clkdiv_pclk_cam0_bnsd_345",
	"dout_clkdiv_pclk_cam0_trex_133",
	"dout_clkdiv_pclk_cam0_trex_266",
	"dout_clkdiv_pclk_cam1_arm_167",
	"dout_clkdiv_pclk_cam1_busperi_167",
	"dout_clkdiv_pclk_cam1_busperi_84",
	"dout_clkdiv_pclk_cam1_sclvra_246",
	"dout_clkdiv_pclk_isp0_isp0_295",
	"dout_clkdiv_pclk_isp0_tpu_295",
	"dout_clkdiv_pclk_isp0_trex_133",
	"dout_clkdiv_pclk_isp0_trex_266",
	"dout_clkdiv_pclk_isp1_isp1_234",
	"dout_sclk_isp_spi0",
	"dout_sclk_isp_spi1",
	"dout_sclk_isp_uart",
	"gate_aclk_csis0_i_wrap",
	"gate_aclk_csis1_i_wrap",
	"gate_aclk_csis3_i_wrap",
	"gate_aclk_fimc_bns_a",
	"gate_aclk_fimc_bns_b",
	"gate_aclk_fimc_bns_c",
	"gate_aclk_fimc_bns_d",
	"gate_aclk_lh_cam0",
	"gate_aclk_lh_cam1",
	"gate_aclk_lh_isp",
	"gate_aclk_noc_bus0_nrt",
	"gate_aclk_wrap_csis2",
	"gate_cclk_asyncapb_socp_fimc_bns_a",
	"gate_cclk_asyncapb_socp_fimc_bns_b",
	"gate_cclk_asyncapb_socp_fimc_bns_c",
	"gate_cclk_asyncapb_socp_fimc_bns_d",
	"gate_pclk_asyncapb_socp_fimc_bns_a",
	"gate_pclk_asyncapb_socp_fimc_bns_b",
	"gate_pclk_asyncapb_socp_fimc_bns_c",
	"gate_pclk_asyncapb_socp_fimc_bns_d",
	"gate_pclk_csis0",
	"gate_pclk_csis1",
	"gate_pclk_csis2",
	"gate_pclk_csis3",
	"gate_pclk_fimc_bns_a",
	"gate_pclk_fimc_bns_b",
	"gate_pclk_fimc_bns_c",
	"gate_pclk_fimc_bns_d",
	"mout_user_mux_aclk_cam0_3aa0_690",
	"mout_user_mux_aclk_cam0_3aa1_468",
	"mout_user_mux_aclk_cam0_bnsa_690",
	"mout_user_mux_aclk_cam0_bnsb_690",
	"mout_user_mux_aclk_cam0_bnsd_690",
	"mout_user_mux_aclk_cam0_csis0_690",
	"mout_user_mux_aclk_cam0_csis1_174",
	"mout_user_mux_aclk_cam0_nocp_133",
	"mout_user_mux_aclk_cam0_trex_532",
	"mout_user_mux_aclk_cam1_arm_668",
	"mout_user_mux_aclk_cam1_bnscsis_133",
	"mout_user_mux_aclk_cam1_busperi_334",
	"mout_user_mux_aclk_cam1_nocp_133",
	"mout_user_mux_aclk_cam1_sclvra_491",
	"mout_user_mux_aclk_cam1_trex_532",
	"mout_user_mux_aclk_isp0_isp0_590",
	"mout_user_mux_aclk_isp0_tpu_590",
	"mout_user_mux_aclk_isp0_trex_532",
	"mout_user_mux_aclk_isp1_ahb_117",
	"mout_user_mux_aclk_isp1_isp1_468",
	"mout_user_mux_phyclk_hs0_csis2_rx_byte",
	"mout_user_mux_phyclk_rxbyteclkhs0_s2a",
	"mout_user_mux_phyclk_rxbyteclkhs0_s4",
	"mout_user_mux_phyclk_rxbyteclkhs1_s4",
	"mout_user_mux_phyclk_rxbyteclkhs2_s4",
	"mout_user_mux_phyclk_rxbyteclkhs3_s4",
	"mout_user_mux_sclk_isp_spi0",
	"mout_user_mux_sclk_isp_spi1",
	"mout_user_mux_sclk_isp_uart",
	"phyclk_hs0_csis2_rx_byte",
	"phyclk_rxbyteclkhs0_s2a",
	"phyclk_rxbyteclkhs0_s4",
	"phyclk_rxbyteclkhs1_s4",
	"phyclk_rxbyteclkhs2_s4",
	"phyclk_rxbyteclkhs3_s4",
};

struct clk *clk_target_list[CLK_NUM];

#if defined(CONFIG_SOC_EXYNOS5422)
int exynos5422_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	pr_info("clk_cfg:(ch%d),scenario(%d)\n", channel, scenario);

	switch (channel) {
	case 0:
		/* MIPI-CSIS0 */
		fimc_is_set_parent_dt(pdev, "mout_gscl_wrap_a", "mout_mpll_ctrl");
		fimc_is_set_rate_dt(pdev, "dout_gscl_wrap_a", (532 * 1000000));
		fimc_is_get_rate_dt(pdev, "dout_gscl_wrap_a");
		break;
	case 1:
		/* FL1_550_CAM */
		fimc_is_set_parent_dt(pdev, "mout_aclk_fl1_550_cam", "mout_mpll_ctrl");
		fimc_is_set_rate_dt(pdev, "dout_aclk_fl1_550_cam", (76 * 1000000));
		fimc_is_set_parent_dt(pdev, "mout_aclk_fl1_550_cam_sw", "dout_aclk_fl1_550_cam");
		fimc_is_set_parent_dt(pdev, "mout_aclk_fl1_550_cam_user", "mout_aclk_fl1_550_cam_sw");
		fimc_is_set_rate_dt(pdev, "dout2_cam_blk_550", (38 * 1000000));

		/* MIPI-CSIS1 */
		fimc_is_set_parent_dt(pdev, "mout_gscl_wrap_b", "mout_mpll_ctrl");
		fimc_is_set_rate_dt(pdev, "dout_gscl_wrap_b", (76 * 1000000));
		fimc_is_get_rate_dt(pdev, "dout_gscl_wrap_b");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return ret;
}

int exynos5422_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos5422_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos5422_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	u32 frequency;
	char div_name[30];
	char sclk_name[30];

	pr_info("%s:ch(%d)\n", __func__, channel);

	snprintf(div_name, sizeof(div_name), "dout_isp_sensor%d", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_parent_dt(pdev, "mout_isp_sensor", "fin_pll");
	fimc_is_set_rate_dt(pdev, div_name, (24 * 1000000));
	fimc_is_enable_dt(pdev, sclk_name);
	frequency = fimc_is_get_rate_dt(pdev, div_name);

	switch (channel) {
	case SENSOR_CONTROL_I2C0:
		fimc_is_enable_dt(pdev, "sclk_gscl_wrap_a");
		fimc_is_enable_dt(pdev, "clk_camif_top_fimcl0");
		fimc_is_enable_dt(pdev, "clk_camif_top_fimcl3");
		fimc_is_enable_dt(pdev, "gscl_fimc_lite0");
		fimc_is_enable_dt(pdev, "gscl_fimc_lite3");
		fimc_is_enable_dt(pdev, "clk_gscl_wrap_a");
		break;
	case SENSOR_CONTROL_I2C1:
	case SENSOR_CONTROL_I2C2:
		fimc_is_enable_dt(pdev, "sclk_gscl_wrap_b");
		fimc_is_enable_dt(pdev, "clk_camif_top_fimcl1");
		fimc_is_enable_dt(pdev, "gscl_fimc_lite1");
		fimc_is_enable_dt(pdev, "clk_gscl_wrap_b");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	fimc_is_enable_dt(pdev, "clk_camif_top_csis0");
	fimc_is_enable_dt(pdev, "clk_xiu_si_gscl_cam");
	fimc_is_enable_dt(pdev, "clk_noc_p_rstop_fimcl");

	pr_info("%s(%d, mclk : %d)\n", __func__, channel, frequency);

	return 0;
}

int exynos5422_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_disable_dt(pdev, sclk_name);

	switch (channel) {
	case SENSOR_CONTROL_I2C0:
		fimc_is_disable_dt(pdev, "sclk_gscl_wrap_a");
		fimc_is_disable_dt(pdev, "clk_camif_top_fimcl0");
		fimc_is_disable_dt(pdev, "clk_camif_top_fimcl3");
		fimc_is_disable_dt(pdev, "gscl_fimc_lite0");
		fimc_is_disable_dt(pdev, "gscl_fimc_lite3");
		fimc_is_disable_dt(pdev, "clk_gscl_wrap_a");
		break;
	case SENSOR_CONTROL_I2C2:
		fimc_is_disable_dt(pdev, "sclk_gscl_wrap_b");
		fimc_is_disable_dt(pdev, "clk_camif_top_fimcl1");
		fimc_is_disable_dt(pdev, "gscl_fimc_lite1");
		fimc_is_disable_dt(pdev, "clk_gscl_wrap_b");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return 0;
}
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
int exynos5430_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	if (scenario != SENSOR_SCENARIO_VISION)
		return ret;

	pr_info("clk_cfg(ch%d)\n", channel);

	switch (channel) {
	case 0:
		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "oscclk");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s4", "oscclk");

		/* MIPI-CSIS0 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis0", 1);

		/* FIMC-LITE0 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_a", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_a", 1);

		/* FIMC-LITE3 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_d", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_d", 1);

		/* ASYNC, FLITE, 3AA, SMMU, QE ... */
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_400", 1);
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_200", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_cam0_50", 1);

		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "aclk_cam0_552");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "aclk_cam0_400");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "aclk_cam0_333");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s4", "phyclk_rxbyteclkhs0_s4");

		/* MIPI-CSIS0 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_b", "mout_aclk_csis0_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis0", 552 * 1000000);

		/* FIMC-LITE0 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_b", "mout_aclk_lite_a_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_a", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_a", 276 * 1000000);

		/* FIMC-LITE3 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_b", "mout_aclk_lite_d_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_d", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_d", 276 * 1000000);

		/* ASYNC, FLITE, 3AA, SMMU, QE ... */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400", "mout_aclk_cam0_400_user");
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_400", 400 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_200", 200 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_cam0_50", 50 * 1000000);

		/* FIMC-LITE2 PIXELASYNC */
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_pixelasync_lite_c", 1);
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c", 1);

		/* FIMC-LITE2 PIXELASYNC */
		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_b", "mout_sclk_pixelasync_lite_c_init_a");
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_pixelasync_lite_c", 276 * 1000000);

		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_b", "mout_aclk_cam0_333_user");
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c", 333 * 1000000);

		break;
	case 1:
		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "oscclk");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2a", "oscclk");

		/* MIPI-CSIS1 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis1", 1);

		/* FIMC-LITE1 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_b", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_b", 1);

		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "aclk_cam0_552");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "aclk_cam0_400");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "aclk_cam0_333");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2a", "phyclk_rxbyteclkhs0_s2a");

		/* MIPI-CSIS1 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_b", "mout_aclk_csis1_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis1", 552 * 1000000);

		/* FIMC-LITE1 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_b", "mout_aclk_lite_b_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_b", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_b", 276 * 1000000);
		break;
	case 2:
		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_552_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_400_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "oscclk");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2b", "oscclk");

		/*  MIPI-CSIS2 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis2_a", 1);

		/* FIMC-LITE2 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_c", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_c", 1);

		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_552_user", "aclk_cam1_552");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_400_user", "aclk_cam1_400");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "aclk_cam1_333");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2b", "phyclk_rxbyteclkhs0_s2b");

		/*  MIPI-CSIS2 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_a", "mout_aclk_cam1_400_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_b", "mout_aclk_cam1_333_user");
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis2_a", 333 * 1000000);
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return ret;
}

int exynos5430_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	switch (channel) {
	case 0:
		fimc_is_enable_dt(pdev, "aclk_csis0");
		fimc_is_enable_dt(pdev, "pclk_csis0");
		fimc_is_enable_dt(pdev, "gate_lite_a");
		fimc_is_enable_dt(pdev, "gate_lite_d");
		break;
	case 1:
		fimc_is_enable_dt(pdev, "aclk_csis1");
		fimc_is_enable_dt(pdev, "pclk_csis1");
		fimc_is_enable_dt(pdev, "gate_lite_b");
		break;
	case 2:
		fimc_is_enable_dt(pdev, "gate_csis2");
		fimc_is_enable_dt(pdev, "gate_lite_c");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return 0;
}

int exynos5430_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	switch (channel) {
	case 0:
		fimc_is_disable_dt(pdev, "aclk_csis0");
		fimc_is_disable_dt(pdev, "pclk_csis0");
		fimc_is_disable_dt(pdev, "gate_lite_a");
		fimc_is_disable_dt(pdev, "gate_lite_d");
		break;
	case 1:
		fimc_is_disable_dt(pdev, "aclk_csis1");
		fimc_is_disable_dt(pdev, "pclk_csis1");
		fimc_is_disable_dt(pdev, "gate_lite_b");
		break;
	case 2:
		fimc_is_disable_dt(pdev, "gate_csis2");
		fimc_is_disable_dt(pdev, "gate_lite_c");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return 0;
}

int exynos5430_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	u32 frequency;
	char mux_name[30];
	char div_a_name[30];
	char div_b_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(mux_name, sizeof(mux_name), "mout_sclk_isp_sensor%d", channel);
	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", channel);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_parent_dt(pdev, mux_name, "oscclk");
	fimc_is_set_rate_dt(pdev, div_a_name, 24 * 1000000);
	fimc_is_set_rate_dt(pdev, div_b_name, 24 * 1000000);
	frequency = fimc_is_get_rate_dt(pdev, sclk_name);

	pr_info("%s(%d, mclk : %d)\n", __func__, channel, frequency);

	return 0;
}

int exynos5430_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	char mux_name[30];
	char div_a_name[30];
	char div_b_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(mux_name, sizeof(mux_name), "mout_sclk_isp_sensor%d", channel);
	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", channel);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_parent_dt(pdev, mux_name, "oscclk");
	fimc_is_set_rate_dt(pdev, div_a_name, 1);
	fimc_is_set_rate_dt(pdev, div_b_name, 1);
	fimc_is_get_rate_dt(pdev, sclk_name);

	return 0;
}

#elif defined(CONFIG_SOC_EXYNOS7420)
static int exynos7420_fimc_is_csi0_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_csis0");
		fimc_is_disable(pdev, "gate_aclk_csis0_i_wrap");
		fimc_is_disable(pdev, "phyclk_rxbyteclkhs0_s4");
		fimc_is_disable(pdev, "phyclk_rxbyteclkhs1_s4");
		fimc_is_disable(pdev, "phyclk_rxbyteclkhs2_s4");
		fimc_is_disable(pdev, "phyclk_rxbyteclkhs3_s4");
	} else {
		fimc_is_enable(pdev, "gate_pclk_csis0");
		fimc_is_enable(pdev, "gate_aclk_csis0_i_wrap");
		fimc_is_enable(pdev, "phyclk_rxbyteclkhs0_s4");
		fimc_is_enable(pdev, "phyclk_rxbyteclkhs1_s4");
		fimc_is_enable(pdev, "phyclk_rxbyteclkhs2_s4");
		fimc_is_enable(pdev, "phyclk_rxbyteclkhs3_s4");
	}

	return ret;
}

static int exynos7420_fimc_is_csi1_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_csis1");
		fimc_is_disable(pdev, "gate_aclk_csis1_i_wrap");
		fimc_is_disable(pdev, "phyclk_rxbyteclkhs0_s2a");
	} else {
		fimc_is_enable(pdev, "gate_pclk_csis1");
		fimc_is_enable(pdev, "gate_aclk_csis1_i_wrap");
		fimc_is_enable(pdev, "phyclk_rxbyteclkhs0_s2a");
	}

	return ret;
}

static int exynos7420_fimc_is_csi2_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_csis2");
		fimc_is_disable(pdev, "gate_aclk_wrap_csis2");
		fimc_is_disable(pdev, "phyclk_hs0_csis2_rx_byte");
	} else {
		fimc_is_enable(pdev, "gate_pclk_csis2");
		fimc_is_enable(pdev, "gate_aclk_wrap_csis2");
		fimc_is_enable(pdev, "phyclk_hs0_csis2_rx_byte");
	}

	return ret;
}

static int exynos7420_fimc_is_csi3_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_csis3");
		fimc_is_disable(pdev, "gate_aclk_csis3_i_wrap");
	} else {
		fimc_is_enable(pdev, "gate_pclk_csis3");
		fimc_is_enable(pdev, "gate_aclk_csis3_i_wrap");
	}

	return ret;
}

static int exynos7420_fimc_is_bns0_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_a");
		fimc_is_disable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_a");
		fimc_is_disable(pdev, "gate_pclk_fimc_bns_a");
		fimc_is_disable(pdev, "gate_aclk_fimc_bns_a");
	} else {
		fimc_is_enable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_a");
		fimc_is_enable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_a");
		fimc_is_enable(pdev, "gate_pclk_fimc_bns_a");
		fimc_is_enable(pdev, "gate_aclk_fimc_bns_a");
	}

	return ret;
}

static int exynos7420_fimc_is_bns1_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_b");
		fimc_is_disable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_b");
		fimc_is_disable(pdev, "gate_pclk_fimc_bns_b");
		fimc_is_disable(pdev, "gate_aclk_fimc_bns_b");
	} else {
		fimc_is_enable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_b");
		fimc_is_enable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_b");
		fimc_is_enable(pdev, "gate_pclk_fimc_bns_b");
		fimc_is_enable(pdev, "gate_aclk_fimc_bns_b");
	}

	return ret;
}

static int exynos7420_fimc_is_bns2_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_c");
		fimc_is_disable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_c");
		fimc_is_disable(pdev, "gate_pclk_fimc_bns_c");
		fimc_is_disable(pdev, "gate_aclk_fimc_bns_c");
	} else {
		fimc_is_enable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_c");
		fimc_is_enable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_c");
		fimc_is_enable(pdev, "gate_pclk_fimc_bns_c");
		fimc_is_enable(pdev, "gate_aclk_fimc_bns_c");
	}

	return ret;
}

static int exynos7420_fimc_is_bns3_gate(struct platform_device *pdev, bool mask)
{
	int ret = 0;

	if (mask) {
		fimc_is_disable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_d");
		fimc_is_disable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_d");
		fimc_is_disable(pdev, "gate_pclk_fimc_bns_d");
		fimc_is_disable(pdev, "gate_aclk_fimc_bns_d");
	} else {
		fimc_is_enable(pdev, "gate_pclk_asyncapb_socp_fimc_bns_d");
		fimc_is_enable(pdev, "gate_cclk_asyncapb_socp_fimc_bns_d");
		fimc_is_enable(pdev, "gate_pclk_fimc_bns_d");
		fimc_is_enable(pdev, "gate_aclk_fimc_bns_d");
	}

	return ret;
}

int exynos7420_fimc_is_sensor_iclk_get(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	char *conid;
	static int cnt = 0;
	int id = 0;
	struct clk *target;

	if ( cnt >= 1 )
		return 0;

	for ( id =0; id < CLK_NUM; id++ ) {
		conid = clk_g_list[id];
		target = clk_get(&pdev->dev, conid);

		if (IS_ERR_OR_NULL(target)) {
			pr_err("%s: could not lookup clock : %s\n", __func__, conid);
			return -EINVAL;
		}
		clk_target_list[id] = target;
	}
	cnt++;

	return 0;
}

int exynos7420_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	/* this dummy enable make refcount to be 1 for clock off */
	exynos7420_fimc_is_csi0_gate(pdev, false);
	exynos7420_fimc_is_csi1_gate(pdev, false);
	exynos7420_fimc_is_csi2_gate(pdev, false);
	exynos7420_fimc_is_csi3_gate(pdev, false);

	exynos7420_fimc_is_bns0_gate(pdev, false);
	exynos7420_fimc_is_bns1_gate(pdev, false);
	exynos7420_fimc_is_bns2_gate(pdev, false);
	exynos7420_fimc_is_bns3_gate(pdev, false);

	return 0;
}

int exynos7420_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	switch (channel) {
	case 0:
		/* CSI */
		exynos7420_fimc_is_csi1_gate(pdev, true);
		exynos7420_fimc_is_csi2_gate(pdev, true);
		exynos7420_fimc_is_csi3_gate(pdev, true);
		/* BNS */
#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
		exynos7420_fimc_is_bns2_gate(pdev, true);
#else
		exynos7420_fimc_is_bns1_gate(pdev, true);
		exynos7420_fimc_is_bns2_gate(pdev, true);
#endif
		break;
	case 1:
		/* CSI */
		exynos7420_fimc_is_csi0_gate(pdev, true);
		exynos7420_fimc_is_csi2_gate(pdev, true);
		exynos7420_fimc_is_csi3_gate(pdev, true);
		/* BNS */
		exynos7420_fimc_is_bns0_gate(pdev, true);
		exynos7420_fimc_is_bns2_gate(pdev, true);
		exynos7420_fimc_is_bns3_gate(pdev, true);
		break;
	case 2:
		/* CSI */
		exynos7420_fimc_is_csi0_gate(pdev, true);
		exynos7420_fimc_is_csi1_gate(pdev, true);
		exynos7420_fimc_is_csi3_gate(pdev, true);
		/* BNS */
		exynos7420_fimc_is_bns0_gate(pdev, true);
		exynos7420_fimc_is_bns1_gate(pdev, true);
		exynos7420_fimc_is_bns3_gate(pdev, true);
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	if (scenario == SENSOR_SCENARIO_NORMAL)
		goto p_err;

	switch (channel) {
	case 0:
		/* BUS0 */
		fimc_is_enable(pdev, "gate_aclk_lh_cam0");

		/* CAM0 */
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_csis0_690");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_bnsa_690");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_bnsd_690");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_trex_532");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_nocp_133");
		fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s4");
		fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs1_s4");
		fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs2_s4");
		fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs3_s4");

		fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsa_345", 330 * 1000000);
		fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsd_345", 330 * 1000000);
		fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_trex_266", 266 * 1000000);
		fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_trex_133", 133 * 1000000);
		break;
	case 1:
		/* BUS0 */
		fimc_is_enable(pdev, "gate_aclk_lh_cam0");

		/* CAM0 */
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_csis1_174");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_bnsb_690");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_trex_532");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_nocp_133");
		fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s2a");

		fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsb_345", 330 * 1000000);
		break;
	case 2:
		/* BUS0 */
		fimc_is_enable(pdev, "gate_aclk_lh_cam1");

		/* CAM1 */
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_bnscsis_133");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_trex_532");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_nocp_133");
		fimc_is_enable(pdev, "mout_user_mux_phyclk_hs0_csis2_rx_byte");
		fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_busperi_334");

		fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_busperi_167", 167 * 1000000);
		fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_busperi_84", 84 * 1000000);
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		ret = -EINVAL;
		goto p_err;
		break;
	}

p_err:
	return ret;
}

int exynos7420_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	switch (channel) {
	case 0:
		/* CSI */
		exynos7420_fimc_is_csi0_gate(pdev, true);
		/* BNS */
		exynos7420_fimc_is_bns0_gate(pdev, true);
#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
		exynos7420_fimc_is_bns1_gate(pdev, true);
#endif
		exynos7420_fimc_is_bns3_gate(pdev, true);
		break;
	case 1:
		/* CSI */
		exynos7420_fimc_is_csi1_gate(pdev, true);
		/* BNS */
		exynos7420_fimc_is_bns1_gate(pdev, true);
		break;
	case 2:
		/* CSI */
		exynos7420_fimc_is_csi2_gate(pdev, true);
		/* BNS */
		exynos7420_fimc_is_bns2_gate(pdev, true);
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	if (scenario == SENSOR_SCENARIO_NORMAL)
		goto p_err;

	switch (channel) {
	case 0:
		/* BUS0 */
		fimc_is_disable(pdev, "gate_aclk_lh_cam0");

		/* CAM0 */
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_csis0_690");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_bnsa_690");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_bnsd_690");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_trex_532");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_nocp_133");
		fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s4");
		fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs1_s4");
		fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs2_s4");
		fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs3_s4");
		break;
	case 1:
		/* BUS0 */
		fimc_is_disable(pdev, "gate_aclk_lh_cam0");

		/* CAM0 */
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_csis1_174");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_bnsb_690");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_trex_532");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_nocp_133");
		fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s2a");
		break;
	case 2:
		/* BUS0 */
		fimc_is_enable(pdev, "gate_aclk_lh_cam1");

		/* CAM1 */
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_bnscsis_133");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_trex_532");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_nocp_133");
		fimc_is_disable(pdev, "mout_user_mux_phyclk_hs0_csis2_rx_byte");
		fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_busperi_334");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		ret = -EINVAL;
		goto p_err;
		break;
	}

p_err:
	return ret;
}

int exynos7420_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	char div_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(div_name, sizeof(div_name), "dout_sclk_isp_sensor%d", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_rate_dt(pdev, div_name, 24 * 1000000);
	fimc_is_enable_dt(pdev, sclk_name);

	return 0;
}

int exynos7420_fimc_is_sensor_mclk_off(struct platform_device *pdev,
		u32 scenario,
		u32 channel)
{
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_disable_dt(pdev, sclk_name);

	return 0;
}
#endif

/* Wrapper functions */
int exynos_fimc_is_sensor_iclk_get(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS7420)
       exynos7420_fimc_is_sensor_iclk_get(pdev, scenario, channel);
#endif
       return 0;
}
int exynos_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#endif
	return 0;
}
