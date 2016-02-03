/*
 * Exynos MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Byeongjo Park <bjo.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mipi-lli.h>
#ifdef CONFIG_SOC_EXYNOS5430
#include <mach/regs-clock-exynos5430.h>
#else
#include <mach/regs-clock-exynos5433.h>
#endif

#include "exynos-mipi-lli.h"
#include "exynos-mipi-lli-mphy.h"

static const void __iomem *lli_debug_clk_info[] = {
	EXYNOS5430_DIV_MIF3,
	EXYNOS5430_DIV_STAT_MIF3,
	EXYNOS5430_ENABLE_ACLK_MIF3,
	EXYNOS5430_ENABLE_IP_CPIF0,
	EXYNOS5430_ENABLE_IP_CPIF1,
	EXYNOS5430_ENABLE_IP_MIF3,
	EXYNOS5430_SRC_SEL_CPIF0,
	EXYNOS5430_SRC_STAT_CPIF1,
};

static int exynos_lli_get_clk_info(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	clks->aclk_cpif_200 = devm_clk_get(lli->dev, "aclk_cpif_200");
	/* To gate/ungate clocks */
	clks->gate_cpifnm_200 = devm_clk_get(lli->dev, "gate_cpifnm_200");
	clks->gate_lli_svc_loc = devm_clk_get(lli->dev, "gate_lli_svc_loc");
	clks->gate_lli_svc_rem = devm_clk_get(lli->dev, "gate_lli_svc_rem");
	clks->gate_lli_ll_init = devm_clk_get(lli->dev, "gate_lli_ll_init");
	clks->gate_lli_be_init = devm_clk_get(lli->dev, "gate_lli_be_init");
	clks->gate_lli_cmn_cfg = devm_clk_get(lli->dev, "gate_lli_cmn_cfg");
	clks->gate_lli_tx0_cfg = devm_clk_get(lli->dev, "gate_lli_tx0_cfg");
	clks->gate_lli_rx0_cfg = devm_clk_get(lli->dev, "gate_lli_rx0_cfg");
	clks->gate_lli_tx0_symbol = devm_clk_get(lli->dev, "gate_lli_tx0_symbol");
	clks->gate_lli_rx0_symbol = devm_clk_get(lli->dev, "gate_lli_rx0_symbol");

	/* For mux selection of clocks */
	clks->mout_phyclk_lli_tx0_symbol_user = devm_clk_get(lli->dev,
			"mout_phyclk_lli_tx0_symbol_user");
	clks->phyclk_lli_tx0_symbol = devm_clk_get(lli->dev,
			"phyclk_lli_tx0_symbol");
	clks->mout_phyclk_lli_rx0_symbol_user = devm_clk_get(lli->dev,
			"mout_phyclk_lli_rx0_symbol_user");
	clks->phyclk_lli_rx0_symbol = devm_clk_get(lli->dev,
			"phyclk_lli_rx0_symbol");

	/* To clock set_rate */
	clks->dout_aclk_cpif_200 = devm_clk_get(lli->dev, "dout_aclk_cpif_200");
	clks->dout_mif_pre = devm_clk_get(lli->dev, "dout_mif_pre");

	if (IS_ERR(clks->aclk_cpif_200) ||
		IS_ERR(clks->gate_cpifnm_200) ||
		IS_ERR(clks->gate_lli_svc_loc) ||
		IS_ERR(clks->gate_lli_svc_rem) ||
		IS_ERR(clks->gate_lli_ll_init) ||
		IS_ERR(clks->gate_lli_be_init) ||
		IS_ERR(clks->gate_lli_cmn_cfg) ||
		IS_ERR(clks->gate_lli_tx0_cfg) ||
		IS_ERR(clks->gate_lli_rx0_cfg) ||
		IS_ERR(clks->gate_lli_tx0_symbol) ||
		IS_ERR(clks->gate_lli_rx0_symbol) ||
		IS_ERR(clks->mout_phyclk_lli_tx0_symbol_user) ||
		IS_ERR(clks->phyclk_lli_tx0_symbol) ||
		IS_ERR(clks->mout_phyclk_lli_rx0_symbol_user) ||
		IS_ERR(clks->phyclk_lli_rx0_symbol) ||
		IS_ERR(clks->dout_aclk_cpif_200) ||
		IS_ERR(clks->dout_mif_pre)
	) {
		dev_err(lli->dev, "exynos_lli_get_clks - failed\n");
		return -ENODEV;
	}

	return 0;
}

static int exynos_lli_clock_init(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if(!clks)
		return -EINVAL;

	clk_set_parent(clks->mout_phyclk_lli_tx0_symbol_user,
			clks->phyclk_lli_tx0_symbol);
	clk_set_parent(clks->mout_phyclk_lli_rx0_symbol_user,
			clks->phyclk_lli_rx0_symbol);

	return 0;
}

static int exynos_lli_clock_div(struct mipi_lli *lli)
{
	int ret = clk_set_rate(lli->clks.dout_aclk_cpif_200, 100000000);

	dev_err(lli->dev, "dout_aclk_cpif_200 = %ld\n",
			lli->clks.dout_aclk_cpif_200->rate);
	dev_err(lli->dev, "dout_mif_pre= %ld\n",
			lli->clks.dout_mif_pre->rate);

	return ret;
}

static int exynos_lli_clock_gating(struct mipi_lli *lli, int is_gating)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if (!clks)
		return -EINVAL;

	if (is_gating) {
		clk_disable_unprepare(clks->gate_lli_rx0_symbol);
		clk_disable_unprepare(clks->gate_lli_tx0_symbol);
		clk_disable_unprepare(clks->gate_lli_rx0_cfg);
		clk_disable_unprepare(clks->gate_lli_tx0_cfg);
		clk_disable_unprepare(clks->gate_lli_cmn_cfg);
		clk_disable_unprepare(clks->gate_lli_be_init);
		clk_disable_unprepare(clks->gate_lli_ll_init);
		clk_disable_unprepare(clks->gate_lli_svc_rem);
		clk_disable_unprepare(clks->gate_lli_svc_loc);
		clk_disable_unprepare(clks->gate_cpifnm_200);
		/* it doesn't gate/ungate aclk_cpif_200
		   clk_disable_unprepare(clks->aclk_cpif_200);
		 */
	} else {
		/* it doesn't gate/ungate aclk_cpif_200
		   clk_prepare_enable(clks->aclk_cpif_200);
		 */
		if (clks->gate_cpifnm_200->enable_count < 1)
			clk_prepare_enable(clks->gate_cpifnm_200);
		if (clks->gate_lli_svc_loc->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_svc_loc);
		if (clks->gate_lli_svc_rem->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_svc_rem);
		if (clks->gate_lli_ll_init->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_ll_init);
		if (clks->gate_lli_be_init->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_be_init);
		if (clks->gate_lli_cmn_cfg->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_cmn_cfg);
		if (clks->gate_lli_tx0_cfg->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_tx0_cfg);
		if (clks->gate_lli_rx0_cfg->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_rx0_cfg);
		if (clks->gate_lli_tx0_symbol->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_tx0_symbol);
		if (clks->gate_lli_rx0_symbol->enable_count < 1)
			clk_prepare_enable(clks->gate_lli_rx0_symbol);
	}

	return 0;
}

static int exynos_lli_loopback_test(struct mipi_lli *lli)
{
	int uval = 0;
	struct exynos_mphy *phy = dev_get_drvdata(lli->mphy);

	/* enable LLI_PHY_CONTROL */
	writel(1, lli->pmu_regs);

	/* software reset */
	writel(1, lli->regs + EXYNOS_DME_LLI_RESET);

	/* to set TxH8 as H8_EXIT : 0x2b -> 0 */
	writel(0x0, phy->loc_regs + PHY_TX_HIBERN8_CONTROL(0));
	/* to set RxH8 as H8_EXIT : 0xa7 -> 0 */
	writel(0x0, phy->loc_regs + PHY_RX_ENTER_HIBERN8(0));

	writel(0x1, phy->loc_regs + PHY_TX_MODE(0));
	writel(0x1, phy->loc_regs + PHY_RX_MODE(0));

	writel(0x0, phy->loc_regs + PHY_TX_LCC_ENABLE(0));
	/* 0x10F24128 sets 0xFFFFFFFF to
	   LLI_Set_Config_update_All(LLI_SVC_BASE_LOCAL); */
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);

	writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	/* RX setting */
	writel(0x75, phy->loc_regs + (0x0a*4)); /* Internal Serial loopback */
	writel(0x01, phy->loc_regs + (0x16*4)); /* Align Enable */

	writel(0x01, phy->loc_regs + (0x0b*4)); /* user pattern ALL 0 */
	writel(0x0F, phy->loc_regs + (0x0C*4));
	writel(0xFF, phy->loc_regs + (0x0D*4));
	writel(0xFF, phy->loc_regs + (0x0E*4));

	writel(0x2b, phy->loc_regs + (0x2f*4));
	writel(0x60, phy->loc_regs + (0x1a*4));
	writel(0x14, phy->loc_regs + (0x29*4)); /* failed LB test on ATE */
	writel(0xC0, phy->loc_regs + (0x2E*4));

	/* TX setting */
	writel(0x03, phy->loc_regs + (0x32*4));
	writel(0x25, phy->loc_regs + (0x78*4)); /* Internal serial loopback */
	writel(0x01, phy->loc_regs + (0x79*4)); /* user ALL 0 */

	writel(0x0F, phy->loc_regs + (0x7A*4));
	writel(0xFF, phy->loc_regs + (0x7B*4));
	writel(0xFF, phy->loc_regs + (0x7C*4));

	writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);

	mdelay(100);
	/* LLI_Set_RxBYpass as BY_PASS_ENTER */
	writel(0x1, phy->loc_regs + (0x2e*4));
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);
	/* LLI_Set_TxBYpass as BY_PASS_ENTER */
	writel(0x1, phy->loc_regs + (0xa8*4));
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);

	mdelay(200);

	writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	uval = readl(phy->loc_regs + (0x23*4));
	dev_err(lli->dev, "LB error : 0x%x\n", uval);
	if (uval == 0x24)
		dev_err(lli->dev, "PWM loopbacktest is Passed!!");
	else
		dev_err(lli->dev, "PWM loopbacktest is Failed!!");

	writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);

	writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	uval = readl(phy->loc_regs + (0x26*4)); /* o_pll_lock[7] */
	writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);

	dev_err(lli->dev, "uval : %d\n", uval);

	return 0;
}

static void exynos_lli_system_config(struct mipi_lli *lli)
{
	if (lli->sys_regs) {
		writel(SIG_INT_MASK0, lli->sys_regs + CPIF_LLI_SIG_INT_MASK0);
		writel(SIG_INT_MASK1, lli->sys_regs + CPIF_LLI_SIG_INT_MASK1);
	}
}
