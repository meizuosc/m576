/* linux/arch/arm/mach-exynos/setup-fimc-is.c
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
#include <mach/exynos-fimc-is.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_SOC_EXYNOS5422)
#include <mach/regs-clock-exynos5422.h>
#elif defined(CONFIG_SOC_EXYNOS5430)
#include <mach/regs-clock-exynos5430.h>
#elif defined(CONFIG_SOC_EXYNOS5433)
#include <mach/regs-clock-exynos5433.h>
#endif

struct platform_device; /* don't need the contents */

/*------------------------------------------------------*/
/*		Common control				*/
/*------------------------------------------------------*/

#define PRINT_CLK(c, n) pr_info("%s : 0x%08X\n", n, readl(c));

int exynos_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* utility function to set rate with DT */
int fimc_is_set_rate(struct platform_device *pdev,
	const char *conid, unsigned int rate)
{
	int ret = 0;
	int id;
	struct clk *target;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_set_rate(target, rate);
	if (ret) {
		pr_err("%s: clk_set_rate is fail(%s)\n", __func__, conid);
		return ret;
	}

	/* fimc_is_get_rate_dt(pdev, conid); */

	return 0;
}

/* utility function to get rate with DT */
int  fimc_is_get_rate(struct platform_device *pdev,
	const char *conid)
{
	int id;
	struct clk *target;
	unsigned int rate_target;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	pr_info("%s(), %s : %dMhz\n", __func__, conid, rate_target/1000000);

	return rate_target;
}

/* utility function to eable with DT */
int  fimc_is_enable(struct platform_device *pdev,
	const char *conid)
{
	int ret;
	int id;
	struct clk *target;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_prepare(target);
	if (ret) {
		pr_err("%s: clk_prepare is fail(%s)\n", __func__, conid);
		return ret;
	}

	ret = clk_enable(target);
	if (ret) {
		pr_err("%s: clk_enable is fail(%s)\n", __func__, conid);
		return ret;
	}

	return 0;
}

/* utility function to disable with DT */
int fimc_is_disable(struct platform_device *pdev,
	const char *conid)
{
	int id;
	struct clk *target;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_disable(target);
	clk_unprepare(target);

	return 0;
}

/* utility function to set parent with DT */
int fimc_is_set_parent_dt(struct platform_device *pdev,
	const char *child, const char *parent)
{
	int ret = 0;
	struct clk *p;
	struct clk *c;

	p = clk_get(&pdev->dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c = clk_get(&pdev->dev, child);
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	ret = clk_set_parent(c, p);
	if (ret) {
		pr_err("%s: clk_set_parent is fail(%s -> %s)\n", __func__, child, parent);
		return ret;
	}

	return 0;
}

/* utility function to set rate with DT */
int fimc_is_set_rate_dt(struct platform_device *pdev,
	const char *conid, unsigned int rate)
{
	int ret = 0;
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_set_rate(target, rate);
	if (ret) {
		pr_err("%s: clk_set_rate is fail(%s)\n", __func__, conid);
		return ret;
	}

	/* fimc_is_get_rate_dt(pdev, conid); */

	return 0;
}

/* utility function to get rate with DT */
int  fimc_is_get_rate_dt(struct platform_device *pdev,
	const char *conid)
{
	struct clk *target;
	unsigned int rate_target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	pr_info("%s(), %s : %dMhz\n", __func__, conid, rate_target/1000000);

	return rate_target;
}

/* utility function to eable with DT */
int  fimc_is_enable_dt(struct platform_device *pdev,
	const char *conid)
{
	int ret;
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_prepare(target);
	if (ret) {
		pr_err("%s: clk_prepare is fail(%s)\n", __func__, conid);
		return ret;
	}

	ret = clk_enable(target);
	if (ret) {
		pr_err("%s: clk_enable is fail(%s)\n", __func__, conid);
		return ret;
	}

	return 0;
}

/* utility function to disable with DT */
int fimc_is_disable_dt(struct platform_device *pdev,
	const char *conid)
{
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_disable(target);
	clk_unprepare(target);

	return 0;
}

#if defined(CONFIG_SOC_EXYNOS5422)
int exynos5422_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	int cfg = 0;
	u32 value = 0;

	if (clk_gate_id == 0)
		return 0;

	/* CAM block */
	/* 3AA 0*/
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA0_IP)) {
		value |= ((1 << 31) | (1 << 27));
		cfg = readl(EXYNOS5_CLK_GATE_IP_CAM);
		if (is_on)
			writel(cfg | value, EXYNOS5_CLK_GATE_IP_CAM);
		else
			writel(cfg & ~(value), EXYNOS5_CLK_GATE_IP_CAM);
		pr_debug("%s :1 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* 3AA 1*/
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA1_IP)) {
		value |= ((1 << 9) | (1 << 4));
		cfg = readl(EXYNOS5_CLK_GATE_IP_GSCL0);
		if (is_on)
			writel(cfg | value, EXYNOS5_CLK_GATE_IP_GSCL0);
		else
			writel(cfg & ~(value), EXYNOS5_CLK_GATE_IP_GSCL0);
		pr_debug("%s :1 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* ISP block */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_ISP_IP))
		value |= (1 << 0);
	if (clk_gate_id & (1 << FIMC_IS_GATE_DRC_IP))
		value |= (1 << 1);
	if (clk_gate_id & (1 << FIMC_IS_GATE_FD_IP))
		value |= (1 << 2);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCC_IP))
		value |= (1 << 3);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCP_IP))
		value |= (1 << 4);
	if (value > 0) {
		cfg = readl(EXYNOS5_CLK_GATE_IP_ISP0);
		if (is_on)
			writel(cfg | value, EXYNOS5_CLK_GATE_IP_ISP0);
		else
			writel(cfg & ~(value), EXYNOS5_CLK_GATE_IP_ISP0);
		pr_debug("%s :1 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_3DNR_IP))
		value |= (1 << 2);
	if (value > 0) {
		cfg = readl(EXYNOS5_CLK_GATE_IP_ISP1);
		if (is_on)
			writel(cfg | value, EXYNOS5_CLK_GATE_IP_ISP1);
		else
			writel(cfg & ~(value), EXYNOS5_CLK_GATE_IP_ISP1);
		pr_debug("%s :1 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

/*
	pr_info("%s : [%s] gate(%d) (0x%x)\n", __func__,
			is_on ? "ON" : "OFF",
			clk_gate_id,
			cfg);
*/
	return 0;
}

int exynos5422_cfg_clk_div_max(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	/* CMU TOP */
	/* 333_432_ISP0 */
	fimc_is_set_rate_dt(pdev, "dout_aclk_333_432_isp0", 1);
	/* 333_432_ISP */
	fimc_is_set_rate_dt(pdev, "dout_aclk_333_432_isp", 1);
	/* 400_ISP */
	fimc_is_set_rate_dt(pdev, "dout_aclk_400_isp", 1);
	/* 266_ISP */
	fimc_is_set_rate_dt(pdev, "dout_aclk_266_isp", 1);

	/* 333_432_GSCL */
	fimc_is_set_rate_dt(pdev, "dout_aclk_333_432_gscl", 1);
	/* 432_CAM */
	fimc_is_set_rate_dt(pdev, "dout_aclk_432_cam", 1);
	/* FL1_550_CAM */
	fimc_is_set_rate_dt(pdev, "dout_aclk_fl1_550_cam", 1);
	/* 550_CAM */
	fimc_is_set_rate_dt(pdev, "dout_aclk_550_cam", 1);

	return 0;
}

int exynos5422_cfg_clk_sclk(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
#ifndef CONFIG_COMPANION_USE
	/* SCLK_SPI0_ISP */
	fimc_is_set_parent_dt(pdev, "mout_spi0_isp", "fin_pll");
	fimc_is_set_rate_dt(pdev, "dout_spi0_isp", 24 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_spi0_isp_pre", 24 * 1000000);
	/* SCLK_SPI1_ISP */
	fimc_is_set_parent_dt(pdev, "mout_spi1_isp", "fin_pll");
	fimc_is_set_rate_dt(pdev, "dout_spi1_isp", 24 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_spi1_isp_pre", 24 * 1000000);
#endif
	/* SCLK_UART_ISP */
	fimc_is_set_parent_dt(pdev, "mout_uart_isp", "fin_pll");
	fimc_is_set_rate_dt(pdev, "dout_uart_isp", (24* 1000000));
	/* SCLK_PWM_ISP */
	fimc_is_set_parent_dt(pdev, "mout_pwm_isp", "fin_pll");
	fimc_is_set_rate_dt(pdev, "dout_pwm_isp", (2 * 1000000));

	return 0;
}

int exynos5422_cfg_clk_cam(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	/* CMU TOP */
	/* 333_432_GSCL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_gscl", "mout_ipll_ctrl");
	fimc_is_set_rate_dt(pdev, "dout_aclk_333_432_gscl", (432 * 1000000));
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_gscl_sw", "dout_aclk_333_432_gscl");
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_gscl_user", "mout_aclk_333_432_gscl_sw");
	/* 432_CAM */
	fimc_is_set_parent_dt(pdev, "mout_aclk_432_cam", "mout_ipll_ctrl");
	fimc_is_set_rate_dt(pdev, "dout_aclk_432_cam", (432 * 1000000));
	fimc_is_set_parent_dt(pdev, "mout_aclk_432_cam_sw", "dout_aclk_432_cam");
	fimc_is_set_parent_dt(pdev, "mout_aclk_432_cam_user", "mout_aclk_432_cam_sw");
	/* 550_CAM */
	fimc_is_set_parent_dt(pdev, "mout_aclk_550_cam", "mout_mpll_ctrl");
	fimc_is_set_rate_dt(pdev, "dout_aclk_550_cam", (532 * 1000000));
	fimc_is_set_parent_dt(pdev, "mout_aclk_550_cam_sw", "dout_aclk_550_cam");
	fimc_is_set_parent_dt(pdev, "mout_aclk_550_cam_user", "mout_aclk_550_cam_sw");

	/* CMU CAM */
	/* CLKDIV2_GSCL_BLK_333 */
	fimc_is_set_rate_dt(pdev, "dout2_gscl_blk_333", (217 * 1000000));
	/* CLKDIV2_CAM_BLK_432 */
	fimc_is_set_rate_dt(pdev, "dout2_cam_blk_432", (217 * 1000000));

	return 0;
}

int exynos5422_cfg_clk_isp(struct platform_device *pdev)
{

	/* CMU TOP */
	/* 333_432_ISP0 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_isp0", "mout_ipll_ctrl");
	fimc_is_set_rate_dt(pdev, "dout_aclk_333_432_isp0", (432 * 1000000));
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_isp0_sw", "dout_aclk_333_432_isp0");
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_isp0_user", "mout_aclk_333_432_isp0_sw");
	/* 333_432_ISP */
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_isp", "mout_ipll_ctrl");
	fimc_is_set_rate_dt(pdev, "dout_aclk_333_432_isp", (432 * 1000000));
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_isp_sw", "dout_aclk_333_432_isp");
	fimc_is_set_parent_dt(pdev, "mout_aclk_333_432_isp_user", "mout_aclk_333_432_isp_sw");
	/* 400_ISP */
	fimc_is_set_parent_dt(pdev, "mout_aclk_400_isp", "mout_mpll_ctrl");
	fimc_is_set_rate_dt(pdev, "dout_aclk_400_isp", (532 * 1000000));
	fimc_is_set_parent_dt(pdev, "mout_aclk_400_isp_sw", "dout_aclk_400_isp");
	fimc_is_set_parent_dt(pdev, "mout_aclk_400_isp_user", "mout_aclk_400_isp_sw");
	/* 266_ISP */
	fimc_is_set_parent_dt(pdev, "mout_aclk_266_isp", "mout_ipll_ctrl");
	fimc_is_set_rate_dt(pdev, "dout_aclk_266_isp", (432 * 1000000));
	fimc_is_set_parent_dt(pdev, "mout_aclk_266_isp_sw", "dout_aclk_266_isp");
	fimc_is_set_parent_dt(pdev, "mout_aclk_266_isp_user", "mout_aclk_266_isp_sw");

	/* CMU ISP */
	/* ACLK_MCUISP_DIV0 */
	fimc_is_set_rate_dt(pdev, "dout_mcuispdiv0", (267 * 1000000));
	/* ACLK_MCUISP_DIV1 */
	fimc_is_set_rate_dt(pdev, "dout_mcuispdiv1", (134 * 1000000));
	/* ACLK_DIV0 */
	fimc_is_set_rate_dt(pdev, "dout_ispdiv0", (216 * 1000000));
	/* ACLK_DIV1 */
	fimc_is_set_rate_dt(pdev, "dout_ispdiv1", (108 * 1000000));
	/* ACLK_DIV2 */
	fimc_is_set_rate_dt(pdev, "dout_ispdiv2", (54 * 1000000));

	return 0;
}

int exynos5422_fimc_is_print_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	fimc_is_get_rate_dt(pdev, "mout_aclk_550_cam_user");
	fimc_is_get_rate_dt(pdev, "mout_aclk_fl1_550_cam_user");
	fimc_is_get_rate_dt(pdev, "mout_aclk_432_cam_user");
	fimc_is_get_rate_dt(pdev, "mout_aclk_333_432_gscl_user");
	fimc_is_get_rate_dt(pdev, "mout_aclk_333_432_isp0_user");
	fimc_is_get_rate_dt(pdev, "mout_aclk_333_432_isp_user");
	fimc_is_get_rate_dt(pdev, "mout_aclk_400_isp_user");
	fimc_is_get_rate_dt(pdev, "mout_aclk_266_isp_user");

	fimc_is_get_rate_dt(pdev, "dout_mcuispdiv0");
	fimc_is_get_rate_dt(pdev, "dout_mcuispdiv1");
	fimc_is_get_rate_dt(pdev, "dout_ispdiv0");
	fimc_is_get_rate_dt(pdev, "dout_ispdiv1");
	fimc_is_get_rate_dt(pdev, "dout_ispdiv2");

	fimc_is_get_rate_dt(pdev, "dout2_gscl_blk_333");
	fimc_is_get_rate_dt(pdev, "dout2_cam_blk_432");
	fimc_is_get_rate_dt(pdev, "dout2_cam_blk_550");

	fimc_is_get_rate_dt(pdev, "dout_pwm_isp");
	fimc_is_get_rate_dt(pdev, "dout_uart_isp");
	fimc_is_get_rate_dt(pdev, "dout_spi0_isp_pre");
	fimc_is_get_rate_dt(pdev, "dout_spi1_isp_pre");

	/* CMU_TOP_DUMP */
	pr_info("EXYNOS5_CLK_SRC_TOP0(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP0));
	pr_info("EXYNOS5_CLK_SRC_TOP1(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP1));
	pr_info("EXYNOS5_CLK_SRC_TOP3(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP3));
	pr_info("EXYNOS5_CLK_SRC_TOP4(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP4));
	pr_info("EXYNOS5_CLK_SRC_TOP8(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP8));
	pr_info("EXYNOS5_CLK_SRC_TOP9(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP9));
	pr_info("EXYNOS5_CLK_SRC_TOP11(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP11));
	pr_info("EXYNOS5_CLK_SRC_TOP13(0x%08X)\n", readl(EXYNOS5_CLK_SRC_TOP13));

	pr_info("EXYNOS5_CLK_DIV_TOP0(0x%08X)\n", readl(EXYNOS5_CLK_DIV_TOP0));
	pr_info("EXYNOS5_CLK_DIV_TOP1(0x%08X)\n", readl(EXYNOS5_CLK_DIV_TOP1));
	pr_info("EXYNOS5_CLK_DIV_TOP8(0x%08X)\n", readl(EXYNOS5_CLK_DIV_TOP8));

	return 0;
}

int exynos5422_fimc_is_cfg_clk(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	/* initialize Clocks */
	exynos5422_cfg_clk_sclk(pdev);
	exynos5422_cfg_clk_cam(pdev);
	exynos5422_cfg_clk_isp(pdev);

	return 0;
}

int exynos5422_fimc_is_clk_on(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	fimc_is_enable_dt(pdev, "sclk_uart_isp");
	fimc_is_enable_dt(pdev, "sclk_pwm_isp");
	fimc_is_enable_dt(pdev, "sclk_spi0_isp");

	fimc_is_enable_dt(pdev, "clk_3aa");
	fimc_is_enable_dt(pdev, "clk_camif_top_3aa");
	fimc_is_enable_dt(pdev, "clk_3aa_2");
	fimc_is_enable_dt(pdev, "clk_camif_top_3aa0");

	return 0;
}

int exynos5422_fimc_is_clk_off(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	exynos5422_cfg_clk_div_max(pdev);
	fimc_is_disable_dt(pdev, "sclk_uart_isp");
	fimc_is_disable_dt(pdev, "sclk_pwm_isp");
	fimc_is_disable_dt(pdev, "sclk_spi0_isp");

	fimc_is_disable_dt(pdev, "clk_3aa");
	fimc_is_disable_dt(pdev, "clk_camif_top_3aa");
	fimc_is_disable_dt(pdev, "clk_3aa_2");
	fimc_is_disable_dt(pdev, "clk_camif_top_3aa0");

	return 0;
}

/* sequence is important, don't change order */
int exynos5422_fimc_is_sensor_power_on(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* sequence is important, don't change order */
int exynos5422_fimc_is_sensor_power_off(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5422_fimc_is_print_pwr(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pr_info("ISP power state(0x%08x)\n", readl(EXYNOS5422_ISP_STATUS));
	pr_info("CAM power state(0x%08x)\n", readl(EXYNOS5422_CAM_STATUS));
	pr_info("CA5 power state(0x%08x)\n", readl(EXYNOS5422_ISP_ARM_STATUS));

	return 0;
}

int exynos5422_fimc_is_set_user_clk_gate(u32 group_id,
		bool is_on,
		u32 user_scenario_id,
		unsigned long msk_state,
		struct exynos_fimc_is_clk_gate_info *gate_info) {
	/* if you want to skip clock on/off, let this func return -1 */
	int ret = -1;

	switch (user_scenario_id) {
	case CLK_GATE_FULL_BYPASS_SN:
		if (is_on == true)
			gate_info->groups[group_id].mask_clk_on_mod &=
				~((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		else
			gate_info->groups[group_id].mask_clk_off_self_mod |=
				((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		ret = 0;
		break;
	case CLK_GATE_DIS_SN:
		if (is_on == true)
			gate_info->groups[group_id].mask_clk_on_mod |=
				((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		else
			gate_info->groups[group_id].mask_clk_off_self_mod |=
				((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
int exynos5430_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	int cfg = 0;
	u32 value = 0;

	if (clk_gate_id == 0)
		return 0;

	/* CAM00 */
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA1_IP))
		value |= (1 << 4);
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA0_IP))
		value |= (1 << 3);

	if (value > 0) {
		cfg = readl(EXYNOS5430_ENABLE_IP_CAM00);
		if (is_on)
			writel(cfg | value, EXYNOS5430_ENABLE_IP_CAM00);
		else
			writel(cfg & ~(value), EXYNOS5430_ENABLE_IP_CAM00);
		pr_debug("%s :1 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}


	/* ISP 0 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_ISP_IP))
		value |= (1 << 0);
	if (clk_gate_id & (1 << FIMC_IS_GATE_DRC_IP))
		value |= (1 << 1);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCC_IP))
		value |= (1 << 2);
	if (clk_gate_id & (1 << FIMC_IS_GATE_DIS_IP))
		value |= (1 << 3);
	if (clk_gate_id & (1 << FIMC_IS_GATE_3DNR_IP))
		value |= (1 << 4);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCP_IP))
		value |= (1 << 5);

	if (value > 0) {
		cfg = readl(EXYNOS5430_ENABLE_IP_ISP0);
		if (is_on)
			writel(cfg | value, EXYNOS5430_ENABLE_IP_ISP0);
		else
			writel(cfg & ~(value), EXYNOS5430_ENABLE_IP_ISP0);
		pr_debug("%s :2 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* CAM 10 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_FD_IP))
		value |= (1 << 3);

	if (value > 0) {
		cfg = readl(EXYNOS5430_ENABLE_IP_CAM10);
		if (is_on)
			writel(cfg | value, EXYNOS5430_ENABLE_IP_CAM10);
		else
			writel(cfg & ~(value), EXYNOS5430_ENABLE_IP_CAM10);
		pr_debug("%s :3 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}
/*
	pr_info("%s : [%s] gate(%d) (0x%x)\n", __func__,
			is_on ? "ON" : "OFF",
			clk_gate_id,
			cfg);
*/
	return 0;
}

static int exynos5430_cfg_clk_isp_pll_on(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	fimc_is_enable_dt(pdev, "fout_isp_pll");
	fimc_is_set_parent_dt(pdev, "mout_isp_pll", "fout_isp_pll");

	return 0;
}

static int exynos5430_cfg_clk_isp_pll_off(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	fimc_is_set_parent_dt(pdev, "mout_isp_pll", "fin_pll");
	fimc_is_disable_dt(pdev, "fout_isp_pll");

	return 0;
}

int exynos5430_cfg_clk_div_max(struct platform_device *pdev)
{
	/* SCLK */
#ifndef CONFIG_COMPANION_USE
	/* SCLK_SPI0 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_a", 1);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0_user", "oscclk");

	/* SCLK_SPI1 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi1_a", 1);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1_user", "oscclk");
#endif

	/* SCLK_UART */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_uart", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_uart", 1);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_uart_user", "oscclk");

	/* CAM1 */
	/* C-A5 */
	fimc_is_set_rate_dt(pdev, "dout_atclk_cam1", 1);
	fimc_is_set_rate_dt(pdev, "dout_pclk_dbg_cam1", 1);

	return 0;
}

int exynos5430_cfg_clk_sclk(struct platform_device *pdev)
{
#ifndef CONFIG_COMPANION_USE
	/* SCLK_SPI0 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0", "mout_bus_pll_user");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_a", 275 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_b", 46 * 1000000);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0_user", "sclk_isp_spi0_top");

#endif
	/* SCLK_SPI1 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1", "mout_bus_pll_user");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi1_a", 275 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi1_b", 46 * 1000000);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi1_user", "sclk_isp_spi1_top");

	return 0;
}

int exynos5430_cfg_clk_cam0(struct platform_device *pdev)
{
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "aclk_cam0_552");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "aclk_cam0_400");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "aclk_cam0_333");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s4", "phyclk_rxbyteclkhs0_s4");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2a", "phyclk_rxbyteclkhs0_s2a");
	fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2b", "phyclk_rxbyteclkhs0_s2b");

	/* LITE A */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_b", "mout_aclk_lite_a_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_a", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_a", 276 * 1000000);

	/* LITE B */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_b", "mout_aclk_lite_b_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_b", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_b", 276 * 1000000);

	/* LITE D */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_b", "mout_aclk_lite_d_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_d", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_d", 276 * 1000000);

	/* LITE C PIXELASYNC */
	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_b", "mout_sclk_pixelasync_lite_c_init_a");
	fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_pixelasync_lite_c", 276 * 1000000);

	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_b", "mout_aclk_cam0_333_user");
	fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c", 333 * 1000000);

	/* 3AA 0 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa0_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa0_b", "mout_aclk_3aa0_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_3aa0", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_3aa0", 276 * 1000000);

	/* 3AA 0 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa1_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_3aa1_b", "mout_aclk_3aa1_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_3aa1", 552 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_3aa1", 276 * 1000000);

	/* CSI 0 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_b", "mout_aclk_csis0_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis0", 552 * 1000000);

	/* CSI 1 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_b", "mout_aclk_csis1_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis1", 552 * 1000000);

	/* CAM0 400 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400", "mout_aclk_cam0_400_user");
	fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_400", 413 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_200", 207 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam0_50", 52 * 1000000);

	return 0;
}

int exynos5430_cfg_clk_cam1(struct platform_device *pdev)
{
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_552_user", "aclk_cam1_552");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_400_user", "aclk_cam1_400");
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "aclk_cam1_333");

	/* C-A5 */
	fimc_is_set_rate_dt(pdev, "dout_atclk_cam1", 276 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_dbg_cam1", 138 * 1000000);

	/* LITE A */
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_c_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_lite_c_b", "mout_aclk_cam1_333_user");
	fimc_is_set_rate_dt(pdev, "dout_aclk_lite_c", 333 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_lite_c", 166 * 1000000);

	/* FD */
	fimc_is_set_parent_dt(pdev, "mout_aclk_fd_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_fd_b", "mout_aclk_fd_a");
	fimc_is_set_rate_dt(pdev, "dout_aclk_fd", 413 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_fd", 207 * 1000000);

	/* CSI 2 */
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_b", "mout_aclk_cam1_333_user");
	fimc_is_set_rate_dt(pdev, "dout_aclk_csis2_a", 333 * 1000000);

	/* MPWM */
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam1_166", 167 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam1_83", 84 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_mpwm", 84 * 1000000);

	/* CAM1 QE CLK GATE */
	fimc_is_enable_dt(pdev, "gate_bts_fd");
	fimc_is_disable_dt(pdev, "gate_bts_fd");

	return 0;
}

int exynos5430_cfg_clk_cam1_spi(struct platform_device *pdev)
{
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "aclk_cam1_333");

	/* SPI */
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam1_83", 84 * 1000000);

	return 0;
}

int exynos5430_cfg_clk_isp(struct platform_device *pdev)
{
	/* CMU_ISP */
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_isp_400_user", "aclk_isp_400");
	fimc_is_set_parent_dt(pdev, "mout_aclk_isp_dis_400_user", "aclk_isp_dis_400");
	/* ISP */
	fimc_is_set_rate_dt(pdev, "dout_aclk_isp_c_200", 207 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_aclk_isp_d_200", 207 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp", 83 * 1000000);
	/* DIS */
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp_dis", 207 * 1000000);

	/* ISP QE CLK GATE */
	fimc_is_enable_dt(pdev, "gate_bts_3dnr");
	fimc_is_enable_dt(pdev, "gate_bts_dis1");
	fimc_is_enable_dt(pdev, "gate_bts_dis0");
	fimc_is_enable_dt(pdev, "gate_bts_scalerc");
	fimc_is_enable_dt(pdev, "gate_bts_drc");
	fimc_is_disable_dt(pdev, "gate_bts_3dnr");
	fimc_is_disable_dt(pdev, "gate_bts_dis1");
	fimc_is_disable_dt(pdev, "gate_bts_dis0");
	fimc_is_disable_dt(pdev, "gate_bts_scalerc");
	fimc_is_disable_dt(pdev, "gate_bts_drc");

	return 0;
}

int exynos5430_fimc_is_cfg_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	exynos5430_cfg_clk_isp_pll_on(pdev);
	exynos5430_cfg_clk_div_max(pdev);

	/* initialize Clocks */
	exynos5430_cfg_clk_sclk(pdev);
	exynos5430_cfg_clk_cam0(pdev);
	exynos5430_cfg_clk_cam1(pdev);
	exynos5430_cfg_clk_isp(pdev);

	return 0;
}

static int exynos_fimc_is_sensor_iclk_init(struct platform_device *pdev)
{
	fimc_is_enable_dt(pdev, "aclk_csis0");
	fimc_is_enable_dt(pdev, "pclk_csis0");
	fimc_is_enable_dt(pdev, "aclk_csis1");
	fimc_is_enable_dt(pdev, "pclk_csis1");
	fimc_is_enable_dt(pdev, "gate_csis2");
	fimc_is_enable_dt(pdev, "gate_lite_a");
	fimc_is_enable_dt(pdev, "gate_lite_b");
	fimc_is_enable_dt(pdev, "gate_lite_d");
	fimc_is_enable_dt(pdev, "gate_lite_c");
	fimc_is_enable_dt(pdev, "gate_lite_freecnt");

	return 0;
}

static int exynos_fimc_is_sensor_iclk_deinit(struct platform_device *pdev)
{
	fimc_is_disable_dt(pdev, "aclk_csis0");
	fimc_is_disable_dt(pdev, "pclk_csis0");
	fimc_is_disable_dt(pdev, "aclk_csis1");
	fimc_is_disable_dt(pdev, "pclk_csis1");
	fimc_is_disable_dt(pdev, "gate_csis2");
	fimc_is_disable_dt(pdev, "gate_lite_a");
	fimc_is_disable_dt(pdev, "gate_lite_b");
	fimc_is_disable_dt(pdev, "gate_lite_d");
	fimc_is_disable_dt(pdev, "gate_lite_c");
	fimc_is_disable_dt(pdev, "gate_lite_freecnt");

	return 0;
}

int exynos5430_fimc_is_clk_on(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	exynos_fimc_is_sensor_iclk_init(pdev);
	exynos_fimc_is_sensor_iclk_deinit(pdev);

	return 0;
}

int exynos5430_fimc_is_clk_off(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	exynos5430_cfg_clk_div_max(pdev);
	exynos5430_cfg_clk_isp_pll_off(pdev);

	return 0;
}

int exynos5430_fimc_is_print_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/* SCLK */
	/* SCLK_SPI0 */
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi0_top");
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi0");
	/* SCLK_SPI1 */
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi1_top");
	fimc_is_get_rate_dt(pdev, "sclk_isp_spi1");
	/* SCLK_UART */
	fimc_is_get_rate_dt(pdev, "sclk_isp_uart_top");
	fimc_is_get_rate_dt(pdev, "sclk_isp_uart");

	/* CAM0 */
	/* CMU_TOP */
	fimc_is_get_rate_dt(pdev, "aclk_cam0_552");
	fimc_is_get_rate_dt(pdev, "aclk_cam0_400");
	fimc_is_get_rate_dt(pdev, "aclk_cam0_333");
	/* LITE A */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_a");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_a");
	/* LITE B */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_b");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_b");
	/* LITE D */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_d");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_d");
	/* LITE C PIXELASYNC */
	fimc_is_get_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init");
	fimc_is_get_rate_dt(pdev, "dout_pclk_pixelasync_lite_c");
	fimc_is_get_rate_dt(pdev, "dout_sclk_pixelasync_lite_c");
	/* 3AA 0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_3aa0");
	fimc_is_get_rate_dt(pdev, "dout_pclk_3aa0");
	/* 3AA 0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_3aa1");
	fimc_is_get_rate_dt(pdev, "dout_pclk_3aa1");
	/* CSI 0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_csis0");
	/* CSI 1 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_csis1");
	/* CAM0 400 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_400");
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_200");
	fimc_is_get_rate_dt(pdev, "dout_pclk_cam0_50");

	/* CAM1 */
	/* CMU_TOP */
	fimc_is_get_rate_dt(pdev, "aclk_cam1_552");
	fimc_is_get_rate_dt(pdev, "aclk_cam1_400");
	fimc_is_get_rate_dt(pdev, "aclk_cam1_333");
	/* C-A5 */
	fimc_is_get_rate_dt(pdev, "dout_atclk_cam1");
	fimc_is_get_rate_dt(pdev, "dout_pclk_dbg_cam1");
	/* LITE A */
	fimc_is_get_rate_dt(pdev, "dout_aclk_lite_c");
	fimc_is_get_rate_dt(pdev, "dout_pclk_lite_c");
	/* FD */
	fimc_is_get_rate_dt(pdev, "dout_aclk_fd");
	fimc_is_get_rate_dt(pdev, "dout_pclk_fd");
	/* CSI 2 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_csis2_a");
	/* MPWM */
	fimc_is_get_rate_dt(pdev, "dout_pclk_cam1_166");
	fimc_is_get_rate_dt(pdev, "dout_pclk_cam1_83");
	fimc_is_get_rate_dt(pdev, "dout_sclk_isp_mpwm");

	/* ISP */
	/* CMU_TOP */
	fimc_is_get_rate_dt(pdev, "aclk_isp_400");
	fimc_is_get_rate_dt(pdev, "aclk_isp_dis_400");
	/* ISP */
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp_c_200");
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp_d_200");
	fimc_is_get_rate_dt(pdev, "dout_pclk_isp");
	/* DIS */
	fimc_is_get_rate_dt(pdev, "dout_pclk_isp_dis");

	/* CMU_TOP_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_TOP1(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_TOP1));
	pr_info("EXYNOS5430_SRC_SEL_TOP2(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_TOP2));
	pr_info("EXYNOS5430_SRC_SEL_TOP_CAM1(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_TOP_CAM1));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP0(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP0));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP1(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP1));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP2(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP2));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP3(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP3));
	pr_info("EXYNOS5430_SRC_ENABLE_TOP_CAM1(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_TOP_CAM1));
	pr_info("EXYNOS5430_DIV_TOP0(0x%08X)\n", readl(EXYNOS5430_DIV_TOP0));
	pr_info("EXYNOS5430_DIV_TOP_CAM10(0x%08X)\n", readl(EXYNOS5430_DIV_TOP_CAM10));
	pr_info("EXYNOS5430_DIV_TOP_CAM11(0x%08X)\n", readl(EXYNOS5430_DIV_TOP_CAM11));
	pr_info("EXYNOS5430_ENABLE_SCLK_TOP_CAM1(0x%08X)\n", readl(EXYNOS5430_ENABLE_SCLK_TOP_CAM1));
	pr_info("EXYNOS5430_ENABLE_IP_TOP(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_TOP));
	/* CMU_CAM0_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_CAM00(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM00));
	pr_info("EXYNOS5430_SRC_SEL_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM01));
	pr_info("EXYNOS5430_SRC_SEL_CAM02(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM02));
	pr_info("EXYNOS5430_SRC_SEL_CAM03(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM03));
	pr_info("EXYNOS5430_SRC_SEL_CAM04(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM04));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM00(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM00));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM01));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM02(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM02));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM03(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM03));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM04(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM04));
	pr_info("EXYNOS5430_SRC_STAT_CAM00(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM00));
	pr_info("EXYNOS5430_SRC_STAT_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM01));
	pr_info("EXYNOS5430_SRC_STAT_CAM02(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM02));
	pr_info("EXYNOS5430_SRC_STAT_CAM03(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM03));
	pr_info("EXYNOS5430_SRC_STAT_CAM04(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM04));
	pr_info("EXYNOS5430_SRC_IGNORE_CAM01(0x%08X)\n", readl(EXYNOS5430_SRC_IGNORE_CAM01));
	pr_info("EXYNOS5430_DIV_CAM00(0x%08X)\n", readl(EXYNOS5430_DIV_CAM00));
	pr_info("EXYNOS5430_DIV_CAM01(0x%08X)\n", readl(EXYNOS5430_DIV_CAM01));
	pr_info("EXYNOS5430_DIV_CAM02(0x%08X)\n", readl(EXYNOS5430_DIV_CAM02));
	pr_info("EXYNOS5430_DIV_CAM03(0x%08X)\n", readl(EXYNOS5430_DIV_CAM03));
	pr_info("EXYNOS5430_DIV_STAT_CAM00(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM00));
	pr_info("EXYNOS5430_DIV_STAT_CAM01(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM01));
	pr_info("EXYNOS5430_DIV_STAT_CAM02(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM02));
	pr_info("EXYNOS5430_DIV_STAT_CAM03(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM03));
	pr_info("EXYNOS5430_ENABLE_IP_CAM00(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM00));
	pr_info("EXYNOS5430_ENABLE_IP_CAM01(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM01));
	pr_info("EXYNOS5430_ENABLE_IP_CAM02(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM02));
	pr_info("EXYNOS5430_ENABLE_IP_CAM03(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM03));
	/* CMU_CAM1_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_CAM10(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM10));
	pr_info("EXYNOS5430_SRC_SEL_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM11));
	pr_info("EXYNOS5430_SRC_SEL_CAM12(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_CAM12));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM10(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM10));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM11));
	pr_info("EXYNOS5430_SRC_ENABLE_CAM12(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_CAM12));
	pr_info("EXYNOS5430_SRC_STAT_CAM10(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM10));
	pr_info("EXYNOS5430_SRC_STAT_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM11));
	pr_info("EXYNOS5430_SRC_STAT_CAM12(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_CAM12));
	pr_info("EXYNOS5430_SRC_IGNORE_CAM11(0x%08X)\n", readl(EXYNOS5430_SRC_IGNORE_CAM11));
	pr_info("EXYNOS5430_DIV_CAM10(0x%08X)\n", readl(EXYNOS5430_DIV_CAM10));
	pr_info("EXYNOS5430_DIV_CAM11(0x%08X)\n", readl(EXYNOS5430_DIV_CAM11));
	pr_info("EXYNOS5430_DIV_STAT_CAM10(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM10));
	pr_info("EXYNOS5430_DIV_STAT_CAM11(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_CAM11));
	pr_info("EXYNOS5430_ENABLE_SCLK_CAM1(0x%08X)\n", readl(EXYNOS5430_ENABLE_SCLK_CAM1));
	pr_info("EXYNOS5430_ENABLE_IP_CAM10(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM10));
	pr_info("EXYNOS5430_ENABLE_IP_CAM11(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM11));
	pr_info("EXYNOS5430_ENABLE_IP_CAM12(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM12));
	/* CMU_ISP_DUMP */
	pr_info("EXYNOS5430_SRC_SEL_ISP(0x%08X)\n", readl(EXYNOS5430_SRC_SEL_ISP));
	pr_info("EXYNOS5430_SRC_ENABLE_ISP(0x%08X)\n", readl(EXYNOS5430_SRC_ENABLE_ISP));
	pr_info("EXYNOS5430_SRC_STAT_ISP(0x%08X)\n", readl(EXYNOS5430_SRC_STAT_ISP));
	pr_info("EXYNOS5430_DIV_ISP(0x%08X)\n", readl(EXYNOS5430_DIV_ISP));
	pr_info("EXYNOS5430_DIV_STAT_ISP(0x%08X)\n", readl(EXYNOS5430_DIV_STAT_ISP));
	pr_info("EXYNOS5430_ENABLE_ACLK_ISP0(0x%08X)\n", readl(EXYNOS5430_ENABLE_ACLK_ISP0));
	pr_info("EXYNOS5430_ENABLE_ACLK_ISP1(0x%08X)\n", readl(EXYNOS5430_ENABLE_ACLK_ISP1));
	pr_info("EXYNOS5430_ENABLE_ACLK_ISP2(0x%08X)\n", readl(EXYNOS5430_ENABLE_ACLK_ISP2));
	pr_info("EXYNOS5430_ENABLE_PCLK_ISP(0x%08X)\n", readl(EXYNOS5430_ENABLE_PCLK_ISP));
	pr_info("EXYNOS5430_ENABLE_SCLK_ISP(0x%08X)\n", readl(EXYNOS5430_ENABLE_SCLK_ISP));
	pr_info("EXYNOS5430_ENABLE_IP_ISP0(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP0));
	pr_info("EXYNOS5430_ENABLE_IP_ISP1(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP1));
	pr_info("EXYNOS5430_ENABLE_IP_ISP2(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP2));
	pr_info("EXYNOS5430_ENABLE_IP_ISP3(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP3));
	/* CMU_ENABLE_DUMP */
	pr_info("EXYNOS5430_ENABLE_SCLK_TOP_CAM1(0x%08X)\n", readl(EXYNOS5430_ENABLE_SCLK_TOP_CAM1));
	pr_info("EXYNOS5430_ENABLE_IP_TOP(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_TOP));
	pr_info("EXYNOS5430_ENABLE_IP_CAM00(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM00));
	pr_info("EXYNOS5430_ENABLE_IP_CAM01(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM01));
	pr_info("EXYNOS5430_ENABLE_IP_CAM02(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM02));
	pr_info("EXYNOS5430_ENABLE_IP_CAM03(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM03));
	pr_info("EXYNOS5430_ENABLE_IP_CAM10(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM10));
	pr_info("EXYNOS5430_ENABLE_IP_CAM11(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM11));
	pr_info("EXYNOS5430_ENABLE_IP_CAM12(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_CAM12));
	pr_info("EXYNOS5430_ENABLE_IP_ISP0(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP0));
	pr_info("EXYNOS5430_ENABLE_IP_ISP1(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP1));
	pr_info("EXYNOS5430_ENABLE_IP_ISP2(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP2));
	pr_info("EXYNOS5430_ENABLE_IP_ISP3(0x%08X)\n", readl(EXYNOS5430_ENABLE_IP_ISP3));

	return 0;
}

/* sequence is important, don't change order */
int exynos5430_fimc_is_sensor_power_on(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* sequence is important, don't change order */
int exynos5430_fimc_is_sensor_power_off(struct platform_device *pdev, int sensor_id)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_print_pwr(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pr_info("ISP power state(0x%08x)\n", readl(EXYNOS_PMU_ISP_STATUS));
	pr_info("CAM0 power state(0x%08x)\n", readl(EXYNOS_PMU_CAM0_STATUS));
	pr_info("CAM1 power state(0x%08x)\n", readl(EXYNOS_PMU_CAM1_STATUS));
	pr_info("CA5 power state(0x%08x)\n", readl(EXYNOS_PMU_A5IS_STATUS));

	return 0;
}

int exynos5430_fimc_is_set_user_clk_gate(u32 group_id,
		bool is_on,
		u32 user_scenario_id,
		unsigned long msk_state,
		struct exynos_fimc_is_clk_gate_info *gate_info) {
	/* if you want to skip clock on/off, let this func return -1 */
	int ret = -1;

	switch (user_scenario_id) {
	case CLK_GATE_NOT_FULL_BYPASS_SN:
		if (is_on == true)
			gate_info->groups[group_id].mask_clk_on_mod &=
				~((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		else
			gate_info->groups[group_id].mask_clk_on_mod |=
				((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		ret = 0;
		break;
	case CLK_GATE_DIS_SN:
		if (is_on == true)
			gate_info->groups[group_id].mask_clk_on_mod |=
				((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		else
			gate_info->groups[group_id].mask_clk_on_mod &=
				~((1 << FIMC_IS_GATE_DIS_IP) |
				(1 << FIMC_IS_GATE_3DNR_IP));
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

#elif defined(CONFIG_SOC_EXYNOS7420)
int exynos7420_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	int cfg = 0;
	u32 value = 0;

	if (clk_gate_id == 0)
		return 0;

	/* CAM0 */
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA1_IP))
		value |= (1 << 1);
	if (clk_gate_id & (1 << FIMC_IS_GATE_3AA0_IP))
		value |= (1 << 0);

	if (value > 0) {
		cfg = readl(EXYNOS7420_ENABLE_IP_CAM00);
		if (is_on)
			writel(cfg | value, EXYNOS7420_ENABLE_IP_CAM00);
		else
			writel(cfg & ~(value), EXYNOS7420_ENABLE_IP_CAM00);
		pr_debug("%s :1 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* ISP 0 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_ISP_IP))
		value |= (1 << 0);
	if (clk_gate_id & (1 << FIMC_IS_GATE_TPU_IP))
		value |= (1 << 1);

	if (value > 0) {
		cfg = readl(EXYNOS7420_ENABLE_IP_ISP0);
		if (is_on)
			writel(cfg | value, EXYNOS7420_ENABLE_IP_ISP0);
		else
			writel(cfg & ~(value), EXYNOS7420_ENABLE_IP_ISP0);
		pr_debug("%s :2 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* ISP 1 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_ISP1_IP))
		value |= (1 << 0);

	if (value > 0) {
		cfg = readl(EXYNOS7420_ENABLE_IP_ISP1);
		if (is_on)
			writel(cfg | value, EXYNOS7420_ENABLE_IP_ISP1);
		else
			writel(cfg & ~(value), EXYNOS7420_ENABLE_IP_ISP1);
		pr_debug("%s :3 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* CAM 10 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCP_IP))
		value |= (1 << 2);
	if (clk_gate_id & (1 << FIMC_IS_GATE_VRA_IP))
		value |= (1 << 3);

	if (value > 0) {
		cfg = readl(EXYNOS7420_ENABLE_IP_CAM10);
		if (is_on)
			writel(cfg | value, EXYNOS7420_ENABLE_IP_CAM10);
		else
			writel(cfg & ~(value), EXYNOS7420_ENABLE_IP_CAM10);
		pr_debug("%s :3 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	return 0;
}

int exynos7420_fimc_is_cfg_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/*
	 * CAUTION
	 * source clock is top0 source not OSC although USERMUX is disbled
	 * anyway, div config for max can be completed whatever source clock is
	 */

	/* CAM0 */
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsa_345", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsb_345", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsd_345", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_3aa0_345", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_3aa1_234", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_trex_266", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_trex_133", 1);

	/* CAM1 */
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_sclvra_246", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_arm_167", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_busperi_167", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_busperi_84", 1);

	/* ISP0 */
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_isp0_295", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_tpu_295", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_trex_266", 1);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_trex_133", 1);

	/* ISP1 */
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp1_isp1_234", 1);

	return 0;
}

int exynos7420_fimc_is_clk_on(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is *pdata;

	pdata = dev_get_platdata(&pdev->dev);
	if (pdata->clock_on) {
		ret = pdata->clk_off(pdev);
		if (ret) {
			pr_err("clk_off is fail(%d)\n", ret);
			goto p_err;
		}
	}

	/* BUS0 */
	fimc_is_enable(pdev, "gate_aclk_lh_cam0");
	fimc_is_enable(pdev, "gate_aclk_lh_cam1");
	fimc_is_enable(pdev, "gate_aclk_lh_isp");
	fimc_is_enable(pdev, "gate_aclk_noc_bus0_nrt");

	/* CAM0 */
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_csis0_690");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_bnsa_690");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_bnsb_690");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_bnsd_690");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_csis1_174");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_3aa0_690");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_3aa1_468");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_trex_532");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam0_nocp_133");
	fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s2a");
	fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s4");
	fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs1_s4");
	fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs2_s4");
	fimc_is_enable(pdev, "mout_user_mux_phyclk_rxbyteclkhs3_s4");

	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsa_345", 330 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsb_345", 330 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_bnsd_345", 330 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_3aa0_345", 330 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_3aa1_234", 234 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_trex_266", 266 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam0_trex_133", 133 * 1000000);

	/* CAM1 */
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_sclvra_491");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_arm_668");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_busperi_334");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_bnscsis_133");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_nocp_133");
	fimc_is_enable(pdev, "mout_user_mux_aclk_cam1_trex_532");
	fimc_is_enable(pdev, "mout_user_mux_sclk_isp_spi0");
	fimc_is_enable(pdev, "mout_user_mux_sclk_isp_spi1");
	fimc_is_enable(pdev, "mout_user_mux_sclk_isp_uart");
	fimc_is_enable(pdev, "mout_user_mux_phyclk_hs0_csis2_rx_byte");

	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_sclvra_246", 246 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_arm_167", 167 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_busperi_167", 167 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_cam1_busperi_84", 84 * 1000000);

	/* ISP0 */
	fimc_is_enable(pdev, "mout_user_mux_aclk_isp0_isp0_590");
	fimc_is_enable(pdev, "mout_user_mux_aclk_isp0_tpu_590");
	fimc_is_enable(pdev, "mout_user_mux_aclk_isp0_trex_532");

	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_isp0_295", 276 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_tpu_295", 276 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_trex_266", 266 * 1000000);
	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp0_trex_133", 133 * 1000000);

	/* ISP1 */
	fimc_is_enable(pdev, "mout_user_mux_aclk_isp1_isp1_468");
	fimc_is_enable(pdev, "mout_user_mux_aclk_isp1_ahb_117");

	fimc_is_set_rate(pdev, "dout_clkdiv_pclk_isp1_isp1_234", 234 * 1000000);

	/* POWER PRO ENABLE */
	writel(0x0, EXYNOS7420_VA_SYSREG + 0x544); /* CAM0 */
	writel(0x0, EXYNOS7420_VA_SYSREG + 0x650); /* CAM1 */
	writel(0x0, EXYNOS7420_VA_SYSREG + 0x1538); /* ISP0 */
	writel(0x0, EXYNOS7420_VA_SYSREG + 0x1738); /* ISP1 */

	/* RCG(Root Clock Gating) */
	/* BLK_CAM0 */
	writel(0x1, EXYNOS7420_VA_SYSREG + 0x0518);
	writel(0x2, EXYNOS7420_VA_SYSREG + 0x051c);
	writel(0xf00, EXYNOS7420_VA_SYSREG + 0x0524);
	writel(0x1, EXYNOS7420_VA_SYSREG + 0x0540);
	writel(0x1ff, EXYNOS7420_VA_SYSREG + 0x0544);

	/* BLK_CAM1 */
	writel(0x1, EXYNOS7420_VA_SYSREG + 0x0618);
	writel(0x10fd, EXYNOS7420_VA_SYSREG + 0x0624);
	writel(0x1, EXYNOS7420_VA_SYSREG + 0x064c);
	writel(0x1ff, EXYNOS7420_VA_SYSREG + 0x0650);

	/* BLK_ISP0 */
	writel(0x1, EXYNOS7420_VA_SYSREG + 0x151c);
	writel(0x1, EXYNOS7420_VA_SYSREG + 0x1524);
	writel(0x1f, EXYNOS7420_VA_SYSREG + 0x1538);
	writel(0x1, EXYNOS7420_VA_SYSREG + 0x153c);

	/* BLK_ISP1 */
	writel(0x3, EXYNOS7420_VA_SYSREG + 0x1724);

	/* for debugging */
#if 0
	writel(0x55233000, EXYNOS7420_MUX_SEL_TOP04);
	writel(0x66666000, EXYNOS7420_MUX_SEL_TOP05);
	writel(0x63220000, EXYNOS7420_MUX_SEL_TOP06);
	writel(0x31122200, EXYNOS7420_MUX_SEL_TOP07);
	writel(0x11111000, EXYNOS7420_MUX_ENABLE_TOP04);
	writel(0x11111000, EXYNOS7420_MUX_ENABLE_TOP05);
	writel(0x11110000, EXYNOS7420_MUX_ENABLE_TOP06);
	writel(0x11111100, EXYNOS7420_MUX_ENABLE_TOP07);
	writel(0x00114000, EXYNOS7420_DIV_TOP04);
	writel(0x00003000, EXYNOS7420_DIV_TOP05);
	writel(0x00030000, EXYNOS7420_DIV_TOP06);
	writel(0x10133100, EXYNOS7420_DIV_TOP07);

	writel(0x00000001, EXYNOS7420_MUX_SEL_BUS0);
	writel(0x0000019F, EXYNOS7420_ENABLE_ACLK_BUS0);
	writel(0x00003E00, EXYNOS7420_ENABLE_PCLK_BUS0);

	writel(0x11111111, EXYNOS7420_MUX_SEL_CAM00);
	writel(0x00000001, EXYNOS7420_MUX_SEL_CAM01);
	writel(0x00011111, EXYNOS7420_MUX_SEL_CAM02);
	writel(0x11111111, EXYNOS7420_MUX_ENABLE_CAM00);
	writel(0x00000001, EXYNOS7420_MUX_ENABLE_CAM01);
	writel(0x00011111, EXYNOS7420_MUX_ENABLE_CAM02);
	writel(0x01111111, EXYNOS7420_DIV_CAM0);
	writel(0x03F31F1F, EXYNOS7420_ENABLE_IP_CAM00);
	writel(0xD1FF1F01, EXYNOS7420_ENABLE_IP_CAM01);
	writel(0x000000F1, EXYNOS7420_ENABLE_IP_CAM02);

	writel(0x00111111, EXYNOS7420_MUX_SEL_CAM10);
	writel(0x10000111, EXYNOS7420_MUX_SEL_CAM11);
	writel(0x00111111, EXYNOS7420_MUX_ENABLE_CAM10);
	writel(0x10110111, EXYNOS7420_MUX_ENABLE_CAM11);
	writel(0x00000313, EXYNOS7420_DIV_CAM1);
	writel(0x337F3FFF, EXYNOS7420_ENABLE_IP_CAM10);
	writel(0x17F7FF1F, EXYNOS7420_ENABLE_IP_CAM11);
	writel(0x0100FF72, EXYNOS7420_ENABLE_IP_CAM12);

	writel(0x00000111, EXYNOS7420_MUX_SEL_ISP0);
	writel(0x00000111, EXYNOS7420_MUX_ENABLE_ISP0);
	writel(0x00003111, EXYNOS7420_DIV_ISP0);
	writel(0xFFFFFFFF, EXYNOS7420_ENABLE_IP_ISP0);

	writel(0x00000011, EXYNOS7420_MUX_SEL_ISP1);
	writel(0x00000011, EXYNOS7420_MUX_ENABLE_ISP1);
	writel(0x00000001, EXYNOS7420_DIV_ISP1);
	writel(0x00011F11, EXYNOS7420_ENABLE_IP_ISP1);
#endif
#if 0
	printk(KERN_DEBUG "TOP0\n");
	PRINT_CLK(EXYNOS7420_MUX_SEL_TOP04, "MUX4");
	PRINT_CLK(EXYNOS7420_MUX_SEL_TOP05, "MUX5");
	PRINT_CLK(EXYNOS7420_MUX_SEL_TOP06, "MUX6");
	PRINT_CLK(EXYNOS7420_MUX_SEL_TOP07, "MUX7");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_TOP04, "MXE4");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_TOP05, "MXE5");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_TOP06, "MXE6");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_TOP07, "MXE7");
	PRINT_CLK(EXYNOS7420_DIV_TOP04, "DIV4");
	PRINT_CLK(EXYNOS7420_DIV_TOP05, "DIV5");
	PRINT_CLK(EXYNOS7420_DIV_TOP06, "DIV6");
	PRINT_CLK(EXYNOS7420_DIV_TOP07, "DIV7");
	PRINT_CLK(EXYNOS7420_ENABLE_ACLK_TOP04, "ENA4");
	PRINT_CLK(EXYNOS7420_ENABLE_ACLK_TOP05, "ENA5");
	PRINT_CLK(EXYNOS7420_ENABLE_ACLK_TOP06, "ENA6");
	PRINT_CLK(EXYNOS7420_ENABLE_ACLK_TOP07, "ENA7");

	printk(KERN_DEBUG "BUS0\n");
	PRINT_CLK(EXYNOS7420_MUX_SEL_BUS0, "MUX0");
	PRINT_CLK(EXYNOS7420_ENABLE_ACLK_BUS0, "ENAA");
	PRINT_CLK(EXYNOS7420_ENABLE_PCLK_BUS0, "ENAP");

	printk(KERN_DEBUG "CAM0\n");
	PRINT_CLK(EXYNOS7420_MUX_SEL_CAM00, "MUX0");
	PRINT_CLK(EXYNOS7420_MUX_SEL_CAM01, "MUX1");
	PRINT_CLK(EXYNOS7420_MUX_SEL_CAM02, "MUX2");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_CAM00, "MXE0");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_CAM01, "MXE1");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_CAM02, "MXE2");
	/* PRINT_CLK(EXYNOS7420_MUX_IGNORE_CAM0); */
	PRINT_CLK(EXYNOS7420_DIV_CAM0, "DIV0");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_CAM00, "ENA0");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_CAM01, "ENA1");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_CAM02, "ENA2");

	printk(KERN_DEBUG "CAM1\n");
	PRINT_CLK(EXYNOS7420_MUX_SEL_CAM10, "MUX0");
	PRINT_CLK(EXYNOS7420_MUX_SEL_CAM11, "MUX1");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_CAM10, "MXE0");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_CAM11, "MXE1");
	/* PRINT_CLK(EXYNOS7420_MUX_IGNORE_CAM1); */
	PRINT_CLK(EXYNOS7420_DIV_CAM1, "DIV0");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_CAM10, "ENA0");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_CAM11, "ENA1");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_CAM12, "ENA2");

	printk(KERN_DEBUG "ISP0\n");
	PRINT_CLK(EXYNOS7420_MUX_SEL_ISP0, "MUX0");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_ISP0, "MXE0");
	PRINT_CLK(EXYNOS7420_DIV_ISP0, "DIV0");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_ISP0, "ENA0");

	printk(KERN_DEBUG "ISP1\n");
	PRINT_CLK(EXYNOS7420_MUX_SEL_ISP1, "MUX0");
	PRINT_CLK(EXYNOS7420_MUX_ENABLE_ISP1, "MXE0");
	PRINT_CLK(EXYNOS7420_DIV_ISP1, "DIV0");
	PRINT_CLK(EXYNOS7420_ENABLE_IP_ISP1, "ENA0");
#endif

	fimc_is_set_rate(pdev, "cam_pll", 660000000);
	fimc_is_set_rate(pdev, "isp_pll", 552000000);
	pdata->clock_on = true;

p_err:
	return 0;
}

int exynos7420_fimc_is_clk_off(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is *pdata;

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata->clock_on) {
		pr_err("clk_off is fail(already off)\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* BUS0 */
	fimc_is_disable(pdev, "gate_aclk_lh_cam0");
	fimc_is_disable(pdev, "gate_aclk_lh_cam1");
	fimc_is_disable(pdev, "gate_aclk_lh_isp");
	fimc_is_disable(pdev, "gate_aclk_noc_bus0_nrt");

	/* CAM0 */
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_csis0_690");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_bnsa_690");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_bnsb_690");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_bnsd_690");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_csis1_174");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_3aa0_690");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_3aa1_468");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_trex_532");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam0_nocp_133");
	fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s2a");
	fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs0_s4");
	fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs1_s4");
	fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs2_s4");
	fimc_is_disable(pdev, "mout_user_mux_phyclk_rxbyteclkhs3_s4");

	/* CAM1 */
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_sclvra_491");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_arm_668");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_busperi_334");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_bnscsis_133");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_nocp_133");
	fimc_is_disable(pdev, "mout_user_mux_aclk_cam1_trex_532");
	fimc_is_disable(pdev, "mout_user_mux_sclk_isp_spi0");
	fimc_is_disable(pdev, "mout_user_mux_sclk_isp_spi1");
	fimc_is_disable(pdev, "mout_user_mux_sclk_isp_uart");
	fimc_is_disable(pdev, "mout_user_mux_phyclk_hs0_csis2_rx_byte");

	/* ISP0 */
	fimc_is_disable(pdev, "mout_user_mux_aclk_isp0_isp0_590");
	fimc_is_disable(pdev, "mout_user_mux_aclk_isp0_tpu_590");
	fimc_is_disable(pdev, "mout_user_mux_aclk_isp0_trex_532");

	/* ISP1 */
	fimc_is_disable(pdev, "mout_user_mux_aclk_isp1_isp1_468");
	fimc_is_disable(pdev, "mout_user_mux_aclk_isp1_ahb_117");

	pdata->clock_on = false;

p_err:
	return ret;
}

int exynos7420_fimc_is_print_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/* INPUT CLOCK */
	fimc_is_get_rate_dt(pdev, "isp_pll");
	fimc_is_get_rate_dt(pdev, "cam_pll");
	fimc_is_get_rate_dt(pdev, "bus0_pll");
	fimc_is_get_rate_dt(pdev, "bus1_pll");
	fimc_is_get_rate_dt(pdev, "cci_pll");
	fimc_is_get_rate_dt(pdev, "mfc_pll");
	fimc_is_get_rate_dt(pdev, "aud_pll");

	printk(KERN_DEBUG "#################### CAM0 clock ####################\n");
	/* CAM0 */
	/* CSIS0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_csis0_690");
	/* BNS A */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_bnsa_690");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam0_bnsa_345");
	/* BNS B */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_bnsb_690");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam0_bnsb_345");
	/* BNS D */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_bnsd_690");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam0_bnsd_345");
	/* CSIS1 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_csis1_174");
	/* 3AA0 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_3aa0_690");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam0_3aa0_345");
	/* 3AA1 */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_3aa1_468");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam0_3aa1_234");
	/* TREX */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_trex_532");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam0_trex_266");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam0_trex_133");
	/* NOCP */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam0_nocp_133");
	/* PHY .. */
	fimc_is_get_rate_dt(pdev, "phyclk_rxbyteclkhs0_s2a");
	fimc_is_get_rate_dt(pdev, "phyclk_rxbyteclkhs0_s4");
	fimc_is_get_rate_dt(pdev, "phyclk_rxbyteclkhs1_s4");
	fimc_is_get_rate_dt(pdev, "phyclk_rxbyteclkhs2_s4");
	fimc_is_get_rate_dt(pdev, "phyclk_rxbyteclkhs3_s4");

	printk(KERN_DEBUG "#################### CAM1 clock ####################\n");
	/* CAM1 */
	/* VRA */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam1_sclvra_491");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam1_sclvra_246");
	/* CORTEX */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam1_arm_668");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam1_arm_167");
	/* BUSPERI */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam1_busperi_334");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam1_busperi_167");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_cam1_busperi_84");
	/* BNS C */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam1_bnscsis_133");
	/* PHY .. */
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam1_nocp_133");
	fimc_is_get_rate_dt(pdev, "dout_aclk_cam1_trex_532");
	/* sclk */
	fimc_is_get_rate_dt(pdev, "dout_sclk_isp_spi0");
	fimc_is_get_rate_dt(pdev, "dout_sclk_isp_spi1");
	fimc_is_get_rate_dt(pdev, "dout_sclk_isp_uart");

	printk(KERN_DEBUG "#################### ISP0 clock ####################\n");
	/* ISP0 */
	/* ISP */
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp0_isp0_590");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_isp0_isp0_295");
	/* TPU */
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp0_tpu_590");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_isp0_tpu_295");
	/* TREX */
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp0_trex_532");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_isp0_trex_266");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_isp0_trex_133");

	printk(KERN_DEBUG "#################### ISP1 clock ####################\n");
	/* ISP1 */
	/* ISP */
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp1_isp1_468");
	fimc_is_get_rate_dt(pdev, "dout_clkdiv_pclk_isp1_isp1_234");
	/* ETC */
	fimc_is_get_rate_dt(pdev, "dout_aclk_isp1_ahb_117");

	return 0;
}
#endif

/* Wrapper functions */
int exynos_fimc_is_cfg_clk(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_cfg_clk(pdev);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_cfg_clk(pdev);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_cfg_clk(pdev);
#endif
	return 0;
}

int exynos_fimc_is_clk_on(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_clk_on(pdev);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_clk_on(pdev);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_clk_on(pdev);
#endif
	return 0;
}

int exynos_fimc_is_clk_off(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_clk_off(pdev);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_clk_off(pdev);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_clk_off(pdev);
#endif
	return 0;
}

int exynos_fimc_is_print_clk(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_print_clk(pdev);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_print_clk(pdev);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_print_clk(pdev);
#endif
	return 0;
}

int exynos_fimc_is_set_user_clk_gate(u32 group_id, bool is_on,
	u32 user_scenario_id,
	unsigned long msk_state,
	struct exynos_fimc_is_clk_gate_info *gate_info)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_set_user_clk_gate(group_id, is_on, user_scenario_id, msk_state, gate_info);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_set_user_clk_gate(group_id, is_on, user_scenario_id, msk_state, gate_info);
#endif
	return 0;
}

int exynos_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_clk_gate(clk_gate_id, is_on);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_clk_gate(clk_gate_id, is_on);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos7420_fimc_is_clk_gate(clk_gate_id, is_on);
#endif
	return 0;
}

#if !defined(CONFIG_SOC_EXYNOS7420)
int exynos_fimc_is_print_pwr(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_print_pwr(pdev);
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	exynos5430_fimc_is_print_pwr(pdev);
#endif
	return 0;
}
#endif
