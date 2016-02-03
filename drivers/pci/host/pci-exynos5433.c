/*
 * PCIe clock control driver for Samsung EXYNOS5433
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kyoungil Kim <ki0351.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

static int exynos_pcie_clock_get(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;

	clks->clk = devm_clk_get(pp->dev, "gate_pcie");
	clks->phy_clk = devm_clk_get(pp->dev, "gate_pcie_phy");

	if (IS_ERR(clks->clk) || IS_ERR(clks->phy_clk)) {
		dev_err(pp->dev, "Failed to get pcie clock\n");
		return -ENODEV;
	}
	return 0;
}

static int exynos_pcie_clock_enable(struct pcie_port *pp, int enable)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;

	if (enable) {
		clk_prepare_enable(clks->clk);
		clk_prepare_enable(clks->phy_clk);
	} else {
		clk_disable_unprepare(clks->clk);
		clk_disable_unprepare(clks->phy_clk);
	}
	return 0;
}

