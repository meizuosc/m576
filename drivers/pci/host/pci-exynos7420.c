/*
 * PCIe clock control driver for Samsung EXYNOS7420
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
	int i;

	if (exynos_pcie->ch_num == 0) {
		clks->pcie_clks[0] = devm_clk_get(pp->dev, "aclk_xiu_modemx");
		clks->pcie_clks[1] = devm_clk_get(pp->dev, "aclk_combo_phy_modem_pcs_pclk");
		clks->pcie_clks[2] = devm_clk_get(pp->dev, "sclk_combo_phy_modem_26m");
		clks->pcie_clks[3] = devm_clk_get(pp->dev, "pclk_async_combo_phy_modem");
		clks->pcie_clks[4] = devm_clk_get(pp->dev, "aclk_pcie_modem_mstr_aclk");
		clks->pcie_clks[5] = devm_clk_get(pp->dev, "aclk_pcie_modem_slv_aclk");
		clks->pcie_clks[6] = devm_clk_get(pp->dev, "aclk_pcie_modem_dbi_aclk");
		clks->pcie_clks[7] = devm_clk_get(pp->dev, "sclk_pcie_modem_gated");
		clks->pcie_clks[8] = devm_clk_get(pp->dev, "sclk_phy_fsys0");
		clks->phy_clks[0] = devm_clk_get(pp->dev, "phyclk_pcie_tx0_gated");
		clks->phy_clks[1] = devm_clk_get(pp->dev, "phyclk_pcie_rx0_gated");
	} else if (exynos_pcie->ch_num == 1) {
		clks->pcie_clks[0] = devm_clk_get(pp->dev, "aclk_xiu_wifi1x");
		clks->pcie_clks[1] = devm_clk_get(pp->dev, "aclk_ahb2axi_pcie_wifi1");
		clks->pcie_clks[2] = devm_clk_get(pp->dev, "aclk_pcie_wifi1_mstr_aclk");
		clks->pcie_clks[3] = devm_clk_get(pp->dev, "aclk_pcie_wifi1_slv_aclk");
		clks->pcie_clks[4] = devm_clk_get(pp->dev, "aclk_pcie_wifi1_dbi_aclk");
		clks->pcie_clks[5] = devm_clk_get(pp->dev, "aclk_combo_phy_pcs_pclk_wifi1");
		clks->pcie_clks[6] = devm_clk_get(pp->dev, "pclk_async_combo_phy_wifi1");
		clks->pcie_clks[7] = devm_clk_get(pp->dev, "sclk_pcie_link_wifi1_gated");
		clks->pcie_clks[8] = devm_clk_get(pp->dev, "sclk_combo_phy_wifi1_26m_gated");
		clks->pcie_clks[9] = devm_clk_get(pp->dev, "sclk_phy_fsys1_gated");
		clks->phy_clks[0] = devm_clk_get(pp->dev, "phyclk_pcie_wifi1_tx0_gated");
		clks->phy_clks[1] = devm_clk_get(pp->dev, "phyclk_pcie_wifi1_rx0_gated");
	}

	for (i = 0; i < exynos_pcie->pcie_clk_num; i++) {
		if (IS_ERR(clks->pcie_clks[i])) {
			dev_err(pp->dev, "Failed to get pcie clock\n");
			return -ENODEV;
		}
	}
	for (i = 0; i < exynos_pcie->phy_clk_num; i++) {
		if (IS_ERR(clks->phy_clks[i])) {
			dev_err(pp->dev, "Failed to get pcie clock\n");
			return -ENODEV;
		}
	}
	return 0;
}

static int exynos_pcie_clock_enable(struct pcie_port *pp, int enable)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i;

	if (enable) {
		for (i = 0; i < exynos_pcie->pcie_clk_num - 1; i++)
			clk_prepare_enable(clks->pcie_clks[i]);
	} else {
		for (i = 0; i < exynos_pcie->pcie_clk_num - 1; i++)
			clk_disable_unprepare(clks->pcie_clks[i]);
	}
	return 0;
}

static int exynos_pcie_ref_clock_enable(struct pcie_port *pp, int enable)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i = exynos_pcie->pcie_clk_num - 1;
	void __iomem *block_base = exynos_pcie->block_base;

	if (enable) {
		clk_prepare_enable(clks->pcie_clks[i]);
		if (exynos_pcie->ch_num == 0)
			writel(readl(block_base) & ~(0x1 << 1), block_base);
	} else {
		if (exynos_pcie->ch_num == 0)
			writel(readl(block_base) | (0x1 << 1), block_base);
		clk_disable_unprepare(clks->pcie_clks[i]);
	}

	return 0;
}

static int exynos_pcie_phy_clock_enable(struct pcie_port *pp, int enable)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	struct exynos_pcie_clks	*clks = &exynos_pcie->clks;
	int i;

	if (enable) {
		for (i = 0; i < exynos_pcie->phy_clk_num; i++)
			clk_prepare_enable(clks->phy_clks[i]);
	} else {
		for (i = 0; i < exynos_pcie->phy_clk_num; i++)
			clk_disable_unprepare(clks->phy_clks[i]);
	}
	return 0;
}
