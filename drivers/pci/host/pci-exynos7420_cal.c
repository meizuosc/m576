/*
 * PCIe phy driver for Samsung EXYNOS7420
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Kyoungil Kim <ki0351.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

void exynos_pcie_phy_config(void *phy_base_regs, void *phy_pcs_base_regs, void *sysreg_base_regs, void *elbi_bsae_regs)
{
	/* 24MHz gen1 */
	u32 cmn_config_val[26] = {0x01, 0x0F, 0xa6, 0x71, 0x90, 0x62, 0x20, 0x00, 0x00, 0xa7, 0x0a,
				  0x37, 0x20, 0x08, 0xEF, 0xfc, 0x96, 0x14, 0x00, 0x10, 0x68, 0x01,
				  0x00, 0x00, 0x04, 0x20};
	u32 trsv_config_val[41] = {0x31, 0xF4, 0xF4, 0x80, 0x25, 0x40, 0xC0, 0x03, 0x35, 0x55, 0x4c,
				   0xc3, 0x10, 0x54, 0x70, 0xc5, 0x00, 0x2f, 0x44, 0xa4, 0x00, 0x3b,
				   0x34, 0xa7, 0x64, 0x00, 0x1f, 0x83, 0x1b, 0x01, 0xE0, 0x00, 0x00,
				   0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x00};
	int i;

	writel(readl(sysreg_base_regs) & ~(0x3 << 4), sysreg_base_regs);
	writel((readl(sysreg_base_regs) & ~(0x3 << 2)) | (0x1<<2), sysreg_base_regs);

	/* pcs_g_rst */
	writel(0x1, elbi_bsae_regs + 0x130);
	udelay(10);
	writel(0x0, elbi_bsae_regs + 0x130);
	udelay(10);

	/* pma_init_rst */
	writel(0x1, elbi_bsae_regs + 0x134);
	udelay(10);
	writel(0x0, elbi_bsae_regs + 0x134);
	udelay(10);

	/* PHY Common block Setting */
	for (i = 0; i < 26; i++)
		writel(cmn_config_val[i], phy_base_regs + (i * 4));

	/* PHY Tranceiver/Receiver block Setting */
	for (i = 0; i < 41; i++)
		writel(trsv_config_val[i], phy_base_regs + ((0x30 + i) * 4));

	writel(0x2, phy_pcs_base_regs);

	/* PCIE_PHY CMN_RST */
	writel(readl(sysreg_base_regs) & ~(0x1 << 7), sysreg_base_regs);
	writel(readl(sysreg_base_regs) | (0x1 << 7), sysreg_base_regs);
	udelay(20);
	writel(readl(sysreg_base_regs) & ~(0x1 << 7), sysreg_base_regs);
}
