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
#include <mach/regs-clock-exynos7420.h>

#include "exynos-mipi-lli.h"
#include "exynos-mipi-lli-mphy.h"

static const void __iomem *lli_debug_clk_info[] = {
	EXYNOS7420_MUX_SEL_FSYS00,
	EXYNOS7420_MUX_ENABLE_FSYS00,
	EXYNOS7420_DIV_FSYS0,
};

static int exynos_lli_get_clk_info(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	/* To gate/ungate clocks */
	clks->aclk_xiu_llisfrx = devm_clk_get(lli->dev, "aclk_xiu_llisfrx");
	clks->aclk_axius_lli_be = devm_clk_get(lli->dev, "aclk_axius_lli_be");
	clks->aclk_axius_lli_ll = devm_clk_get(lli->dev, "aclk_axius_lli_ll");
	clks->aclk_axius_llisfrx_llill = devm_clk_get(lli->dev,
						"aclk_axius_llisfrx_llill");
	clks->aclk_axius_llisfrx_llibe = devm_clk_get(lli->dev,
						"aclk_axius_llisfrx_llibe");

	clks->aclk_lli_svc_loc = devm_clk_get(lli->dev, "aclk_lli_svc_loc");
	clks->aclk_lli_svc_rem = devm_clk_get(lli->dev, "aclk_lli_svc_rem");
	clks->aclk_lli_ll_init = devm_clk_get(lli->dev, "aclk_lli_ll_init");
	clks->aclk_lli_ll_targ = devm_clk_get(lli->dev, "aclk_lli_ll_targ");
	clks->aclk_lli_be_init = devm_clk_get(lli->dev, "aclk_lli_be_init");
	clks->aclk_lli_be_targ = devm_clk_get(lli->dev, "aclk_lli_be_targ");
	clks->user_phyclk_lli_tx0_symbol = devm_clk_get(lli->dev,
							"user_phyclk_lli_tx0_symbol");
	clks->user_phyclk_lli_rx0_symbol = devm_clk_get(lli->dev,
							"user_phyclk_lli_rx0_symbol");

	clks->aclk_xiu_modemx = devm_clk_get(lli->dev, "aclk_xiu_modemx");
	clks->aclk_combo_phy_modem_pcs_pclk = devm_clk_get(lli->dev,
							"aclk_combo_phy_modem_pcs_pclk");
	clks->sclk_phy_fsys0 = devm_clk_get(lli->dev, "sclk_phy_fsys0");
	clks->sclk_combo_phy_modem_26m = devm_clk_get(lli->dev,
							"sclk_combo_phy_modem_26m");
	clks->pclk_async_combo_phy_modem = devm_clk_get(lli->dev,
							"pclk_async_combo_phy_modem");

	if (IS_ERR(clks->aclk_xiu_llisfrx) ||
		IS_ERR(clks->aclk_axius_lli_be) ||
		IS_ERR(clks->aclk_axius_lli_ll) ||
		IS_ERR(clks->aclk_axius_llisfrx_llill) ||
		IS_ERR(clks->aclk_axius_llisfrx_llibe) ||
		IS_ERR(clks->aclk_lli_svc_loc) ||
		IS_ERR(clks->aclk_lli_svc_rem) ||
		IS_ERR(clks->aclk_lli_ll_init) ||
		IS_ERR(clks->aclk_lli_be_init) ||
		IS_ERR(clks->aclk_lli_ll_targ) ||
		IS_ERR(clks->aclk_lli_be_targ) ||
		IS_ERR(clks->user_phyclk_lli_tx0_symbol) ||
		IS_ERR(clks->user_phyclk_lli_rx0_symbol) ||
		IS_ERR(clks->aclk_xiu_modemx) ||
		IS_ERR(clks->aclk_combo_phy_modem_pcs_pclk) ||
		IS_ERR(clks->sclk_phy_fsys0) ||
		IS_ERR(clks->sclk_combo_phy_modem_26m) ||
		IS_ERR(clks->pclk_async_combo_phy_modem)) {
		dev_err(lli->dev, "exynos_lli_get_clks - failed \
			%lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx %lx \
			%lx %lx %lx %lx %lx %lx\n",
			IS_ERR(clks->aclk_xiu_llisfrx),
			IS_ERR(clks->aclk_axius_lli_be),
			IS_ERR(clks->aclk_axius_lli_ll),
			IS_ERR(clks->aclk_axius_llisfrx_llill),
			IS_ERR(clks->aclk_axius_llisfrx_llibe),
			IS_ERR(clks->aclk_lli_svc_loc),
			IS_ERR(clks->aclk_lli_svc_rem),
			IS_ERR(clks->aclk_lli_ll_init),
			IS_ERR(clks->aclk_lli_be_init),
			IS_ERR(clks->aclk_lli_ll_targ),
			IS_ERR(clks->aclk_lli_be_targ),
			IS_ERR(clks->user_phyclk_lli_tx0_symbol),
			IS_ERR(clks->user_phyclk_lli_rx0_symbol),
			IS_ERR(clks->aclk_xiu_modemx),
			IS_ERR(clks->aclk_combo_phy_modem_pcs_pclk),
			IS_ERR(clks->sclk_phy_fsys0),
			IS_ERR(clks->sclk_combo_phy_modem_26m),
			IS_ERR(clks->pclk_async_combo_phy_modem));
		return -ENODEV;
	}
	return 0;
}

static int exynos_lli_clock_init(struct mipi_lli *lli)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if(!clks)
		return -EINVAL;

	return 0;
}

static int exynos_lli_clock_div(struct mipi_lli *lli)
{
	return 0;
}

static int exynos_lli_clock_gating(struct mipi_lli *lli, int is_gating)
{
	struct mipi_lli_clks *clks = &lli->clks;

	if (!clks)
		return -EINVAL;

	if (is_gating) {
		clk_disable_unprepare(clks->user_phyclk_lli_tx0_symbol);
		clk_disable_unprepare(clks->user_phyclk_lli_rx0_symbol);
		clk_disable_unprepare(clks->aclk_xiu_llisfrx);
		clk_disable_unprepare(clks->aclk_axius_lli_be);
		clk_disable_unprepare(clks->aclk_axius_lli_ll);
		clk_disable_unprepare(clks->aclk_axius_llisfrx_llill);
		clk_disable_unprepare(clks->aclk_axius_llisfrx_llibe);
		clk_disable_unprepare(clks->aclk_lli_svc_loc);
		clk_disable_unprepare(clks->aclk_lli_svc_rem);
		clk_disable_unprepare(clks->aclk_lli_ll_init);
		clk_disable_unprepare(clks->aclk_lli_be_init);
		clk_disable_unprepare(clks->aclk_lli_ll_targ);
		clk_disable_unprepare(clks->aclk_lli_be_targ);
		clk_disable_unprepare(clks->aclk_xiu_modemx);
		clk_disable_unprepare(clks->pclk_async_combo_phy_modem);
		clk_disable_unprepare(clks->aclk_combo_phy_modem_pcs_pclk);
		clk_disable_unprepare(clks->sclk_phy_fsys0);
		clk_disable_unprepare(clks->sclk_combo_phy_modem_26m);
	} else {
		clk_prepare_enable(clks->aclk_xiu_modemx);
		clk_prepare_enable(clks->aclk_combo_phy_modem_pcs_pclk);
		clk_prepare_enable(clks->sclk_phy_fsys0);
		clk_prepare_enable(clks->sclk_combo_phy_modem_26m);
		clk_prepare_enable(clks->pclk_async_combo_phy_modem);
		clk_prepare_enable(clks->aclk_xiu_llisfrx);
		clk_prepare_enable(clks->aclk_axius_lli_be);
		clk_prepare_enable(clks->aclk_axius_lli_ll);
		clk_prepare_enable(clks->aclk_axius_llisfrx_llill);
		clk_prepare_enable(clks->aclk_axius_llisfrx_llibe);
		clk_prepare_enable(clks->aclk_lli_svc_loc);
		clk_prepare_enable(clks->aclk_lli_svc_rem);
		clk_prepare_enable(clks->aclk_lli_ll_init);
		clk_prepare_enable(clks->aclk_lli_be_init);
		clk_prepare_enable(clks->aclk_lli_ll_targ);
		clk_prepare_enable(clks->aclk_lli_be_targ);
		clk_prepare_enable(clks->user_phyclk_lli_tx0_symbol);
		clk_prepare_enable(clks->user_phyclk_lli_rx0_symbol);
	}

	return 0;
}

static int exynos_lli_loopback_test(struct mipi_lli *lli)
{
	return 0;
}

static void exynos_lli_system_config(struct mipi_lli *lli)
{
	if (lli->sys_regs) {
		/* MODEM, 0:PCIe, 1:LLI */
		writel(1, lli->sys_regs + FSYS0_AXI_SEL_DEMUX);

		writel(SIG_INT_MASK0, lli->sys_regs + FSYS0_LLI_SIG_INT_MASK0_1);
		writel(SIG_INT_MASK1, lli->sys_regs + FSYS0_LLI_SIG_INT_MASK2_3);
	}
}
