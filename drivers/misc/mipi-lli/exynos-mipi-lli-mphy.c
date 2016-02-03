/*
 * Exynos MIPI-LLI-MPHY driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/mipi-lli.h>
#include <linux/platform_device.h>

#include "exynos-mipi-lli.h"
#include "exynos-mipi-lli-mphy.h"

static u32 exynos_mphy_get_tx_fsmstate(void);
static u32 exynos_mphy_get_rx_fsmstate(void);
struct device *lli_mphy;

#include "exynos-mipi-lli-mphy-cal.c"

int exynos_mphy_init(struct exynos_mphy *phy)
{
	int gear = phy->default_mode & 0x7;

	writel(0x45, phy->loc_regs + PHY_TX_HS_SYNC_LENGTH(0));
	/* if TX_LCC is disable, M-TX should enter SLEEP or STALL state
	based on the current value of the TX_MODE upon getting a TOB REQ */
	writel(0x0, phy->loc_regs + PHY_TX_LCC_ENABLE(0));

	if (phy->default_mode & OPMODE_HS) {
		writel(0x2, phy->loc_regs + PHY_TX_MODE(0));
		writel(0x2, phy->loc_regs + PHY_RX_MODE(0));
	}

	if (gear > 0x1) {
		writel(gear, phy->loc_regs + PHY_TX_HSGEAR(0));
		writel(gear, phy->loc_regs + PHY_RX_HSGEAR(0));
	}

	if (phy->default_mode & HS_RATE_B) {
		writel(0x2, phy->loc_regs + PHY_TX_HSRATE_SERIES(0));
		writel(0x2, phy->loc_regs + PHY_RX_HSRATE_SERIES(0));
	}

	return 0;
}

static int exynos_mphy_cmn_init(struct exynos_mphy *phy)
{
	static bool is_first = true;

	if (phy->is_shared_clk)
		writel(0x00, phy->loc_regs + (0x4f*4));
	else
		writel(0xF9, phy->loc_regs + (0x4f*4));

	/* Basic tune for series-A */
	writel(0x05, phy->loc_regs + (0x0A*4));
	writel(0x03, phy->loc_regs + (0x11*4));
	writel(0x03, phy->loc_regs + (0x12*4));
	writel(0x03, phy->loc_regs + (0x13*4));
	writel(0x02, phy->loc_regs + (0x14*4));
	writel(0x00, phy->loc_regs + (0x16*4));
	writel(0x01, phy->loc_regs + (0x17*4));
	writel(0xD6, phy->loc_regs + (0x19*4));
	writel(0x00, phy->loc_regs + (0x44*4));
	writel(0x01, phy->loc_regs + (0x4D*4));
	writel(0x03, phy->loc_regs + (0x4E*4));

	/* afc on only booting time */
	if (is_first) {
		writel(0x00, phy->loc_regs + (0x44*4));
		writel(0x00, phy->loc_regs + (0x31*4));
		is_first = false;
	}
	else {
		writel(phy->afc_val, phy->loc_regs + (0x44*4));
		writel(0x01, phy->loc_regs + (0x31*4));
	}

	/* afc tune */
	writel(0x2c, phy->loc_regs + (0x46*4));

	return 0;
}

static int exynos_mphy_ovtm_init(struct exynos_mphy *phy)
{
	if (!phy->is_shared_clk)
		writel(0, phy->loc_regs + (0x20*4));
	else
		writel(1, phy->loc_regs + (0x20*4));

	/* SYNC PATTERN change enable as 0b10101010 */
	writel(0x1, phy->loc_regs + (0x8A*4));
	writel(0xA, phy->loc_regs + (0x87*4));
	writel(0xAA, phy->loc_regs + (0x88*4));
	writel(0xAA, phy->loc_regs + (0x89*4));

	/* Basic tune for series-A */
	/* for TX */
	writel(0x02, phy->loc_regs + (0x76*4));
	writel(0x01, phy->loc_regs + (0x84*4));
	writel(0xAA, phy->loc_regs + (0x85*4));
	/* for RX */
	writel(0xBC, phy->loc_regs + (0x0A*4));
	writel(0x02, phy->loc_regs + (0x16*4));
#ifdef CONFIG_SOC_EXYNOS5430
	writel(0x00, phy->loc_regs + (0x1A*4));
#endif
	/* It should be changed by CP. (0x2E) */
	writel(0xC0, phy->loc_regs + (0x2E*4));
	writel(0xAB, phy->loc_regs + (0x2F*4));
	writel(0x08, phy->loc_regs + (0x40*4));
	writel(0x8F, phy->loc_regs + (0x45*4));

	/* setting values for each mphy cfg clk */
	{
		/* for tx */
		{
			/* tx_line_reset_nvalue when cfg clk is 100Mhz */
			writel(0x0E, phy->loc_regs + (0x77*4));
			/* tx_line_reset_pvalue when cfg clk is 100Mhz */
			writel(0x49, phy->loc_regs + (0x7D*4));
		}
		/* for rx */
		{
			/* rx_line_reset_value when cfg clk is 100Mhz */
			writel(0x19, phy->loc_regs + (0x17*4));
			/* h8_wait_value when cfg clk is 100Mhz */
			writel(0x16, phy->loc_regs + (0x31*4));
			writel(0xE3, phy->loc_regs + (0x32*4));
			writel(0x60, phy->loc_regs + (0x33*4));
		}
	}

	/* PLL power off */
	writel(0x0, phy->loc_regs + (0x1A*4));

	return 0;
}

static int exynos_combo_mphy_init(struct exynos_mphy *phy)
{
	exynos_mphy_cal_init(phy->loc_regs, phy->default_mode);

	return 0;
}

static int exynos_combo_mphy_cmn_init(struct exynos_mphy *phy)
{
	struct refclk_info refclk_path;

	refclk_path.freq = LLI_MPHY_REFERENCE_CLK_IN_26MHZ;
	refclk_path.is_shared = phy->is_shared_clk;

	exynos_mphy_cal_cmn_init(phy->loc_regs, &refclk_path);

	return 0;
}

static int exynos_combo_mphy_ovtm_init(struct exynos_mphy *phy)
{
	exynos_mphy_cal_ovtm_init(phy->loc_regs);

	return 0;
}

static int exynos_combo_mphy_pma_init(struct exynos_mphy *phy)
{
	struct refclk_info refclk_path;
	int ret;

	refclk_path.freq = LLI_MPHY_REFERENCE_CLK_IN_26MHZ;
	refclk_path.is_shared = phy->is_shared_clk;

	ret = exynos_mphy_cal_pma_init(phy->pma_regs,
				 &refclk_path, phy->default_mode);
	if (ret)
		dev_err(phy->dev, "pma init ret = %x\n", ret);

	return 0;
}

static int exynos_mphy_shutdown(struct exynos_mphy *phy)
{
	return 0;
}

static u32 exynos_mphy_get_tx_fsmstate(void)
{
	struct exynos_mphy *phy = dev_get_drvdata(lli_mphy);

	return readl(phy->loc_regs + 0x41*4);
}

static u32 exynos_mphy_get_rx_fsmstate(void)
{
	struct exynos_mphy *phy = dev_get_drvdata(lli_mphy);

	return readl(phy->loc_regs + 0xC1*4);
}

#if defined(CONFIG_SOC_EXYNOS7420)
int exynos_mphy_block_powerdown(struct device *lli_mphy, int enable)
{
	struct exynos_mphy *phy = dev_get_drvdata(lli_mphy);
	u32 phy_cmn_value;

	phy_cmn_value = readl(phy->pma_regs + PHY_CMN_POWER_DOWN);

	if(!enable)
	{
		/* Disable MPHY_TRSV_POWER_DOWN (Turn on TRSV block)  */
		writel(0x80, phy->pma_regs + PHY_TRSV_POWER_DOWN_LOW);
		writel(0x00, phy->pma_regs + PHY_TRSV_POWER_DOWN_HIGH);

		phy_cmn_value &= 0xFFFFFF87; /* 6,5,4,3 bit is to be 0 */

		/* Disable MPHY_CMN_POWER_DOWN (Turn on CMN block) */
		writel(phy_cmn_value, phy->pma_regs + PHY_CMN_POWER_DOWN);
	}
	else
	{
		/* Enable MPHY_TRSV_POWER_DOWN (Turn off TRSV block) */
		writel(0xFF, phy->pma_regs + PHY_TRSV_POWER_DOWN_LOW);
		writel(0x3F, phy->pma_regs + PHY_TRSV_POWER_DOWN_HIGH);

		phy_cmn_value |= 0x78; /* 6,5,4,3 bit is to be 1 */

		/* Enable MPHY_CMN_POWER_DOWN (Turn off CMN block) */
		writel(phy_cmn_value, phy->pma_regs + PHY_CMN_POWER_DOWN);
	}

	return 0;
}
#endif

static struct exynos_mphy_driver exynos_dedicated_mphy_driver = {
	.init = exynos_mphy_init,
	.cmn_init = exynos_mphy_cmn_init,
	.ovtm_init = exynos_mphy_ovtm_init,
	.pma_init = NULL,
	.shutdown = exynos_mphy_shutdown,
};

static struct exynos_mphy_driver exynos_combo_mphy_driver = {
	.init = exynos_combo_mphy_init,
	.cmn_init = exynos_combo_mphy_cmn_init,
	.ovtm_init = exynos_combo_mphy_ovtm_init,
	.pma_init = exynos_combo_mphy_pma_init,
	.shutdown = exynos_mphy_shutdown,
};

static int exynos_mipi_lli_mphy_get_setting(struct exynos_mphy *phy)
{
	struct device_node *modem_node;
	struct device_node *mphy_node = phy->dev->of_node;
	const char *modem_name;
	const __be32 *prop;

	modem_name = (char *)of_get_property(mphy_node, "modem-name", NULL);
	if (!modem_name) {
		dev_err(phy->dev, "parsing err : modem-name\n");
		goto parsing_err;
	}
	modem_node = of_get_child_by_name(mphy_node, "modems");
	if (!modem_node) {
		dev_err(phy->dev, "parsing err : modems node\n");
		goto parsing_err;
	}
	modem_node = of_get_child_by_name(modem_node, modem_name);
	if (!modem_node) {
		dev_err(phy->dev, "parsing err : modem node\n");
		goto parsing_err;
	}

	prop = of_get_property(modem_node, "init-gear", NULL);
	if (prop) {
		int value = 0;

		value = be32_to_cpu(prop[0]);
		if (value == 0x2)
			phy->default_mode = OPMODE_HS;
		else
			phy->default_mode = OPMODE_PWM;

		value = be32_to_cpu(prop[1]);
		if (value > 0x0 && value < 0x8)
			phy->default_mode |= value;
		else
			phy->default_mode |= GEAR_1;

		value = be32_to_cpu(prop[2]);
		if (value == 0x2)
			phy->default_mode |= HS_RATE_B;
		else
			phy->default_mode |= HS_RATE_A;
	}
	else {
		phy->default_mode = OPMODE_PWM | GEAR_1 | HS_RATE_A;
	}

	prop = of_get_property(modem_node, "shd-refclk", NULL);
	if (prop)
		phy->is_shared_clk = be32_to_cpup(prop) ? true : false;
	else
		phy->is_shared_clk = true;

parsing_err:
	dev_err(phy->dev, "modem_name:%s, gear:%s-G%d%s, shdclk:%d\n",
			modem_name,
			phy->default_mode & OPMODE_HS ? "HS" : "PWM",
			phy->default_mode & 0x7,
			phy->default_mode & HS_RATE_B ? "B" : "A",
			phy->is_shared_clk);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_mphy_dt_match[] = {
	{
		.compatible = "samsung,exynos-mipi-lli-mphy",
		.data = &exynos_dedicated_mphy_driver,
	},
	{
		.compatible = "samsung,exynos-mipi-lli-combo-mphy",
		.data = &exynos_combo_mphy_driver,
	},
};
MODULE_DEVICE_TABLE(of, exynos_mphy_dt_match);
#endif

static int exynos_mipi_lli_mphy_probe(struct platform_device *pdev)
{
	struct exynos_mphy *mphy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	const struct of_device_id *of_id;
	void __iomem *regs, *pma_regs;
	bool has_pma;
	int ret = 0;

	mphy = devm_kzalloc(dev, sizeof(struct exynos_mphy), GFP_KERNEL);
	if (!mphy) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot find register resource 0\n");
		return -ENXIO;
	}

	regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	has_pma = of_property_read_bool(dev->of_node, "has_pma");
	if (has_pma) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res) {
			dev_err(dev, "cannot find register resource 1\n");
			return -ENXIO;
		}

		pma_regs = devm_request_and_ioremap(dev, res);
		if (!pma_regs) {
			dev_err(dev, "cannot request_and_map registers\n");
			return -EADDRNOTAVAIL;
		}
	} else {
		pma_regs = NULL;
	}

	of_id = of_match_node(exynos_mphy_dt_match, dev->of_node);

	spin_lock_init(&mphy->lock);
	mphy->driver = (struct exynos_mphy_driver *)of_id->data;
	mphy->dev = dev;
	mphy->loc_regs = regs;
	mphy->pma_regs = pma_regs;
	mphy->lane = 1;
	mphy->default_mode = OPMODE_PWM | GEAR_1 | HS_RATE_A;
	mphy->is_shared_clk= true;
	exynos_mipi_lli_mphy_get_setting(mphy);

	platform_set_drvdata(pdev, mphy);
	lli_mphy = dev;
	dev_info(dev, "%p driver : %p, mphy->pma_regs :%p, mphy->loc_regs : %p\n",
				mphy, mphy->driver, mphy->pma_regs, mphy->loc_regs);
	return ret;
}

static int exynos_mipi_lli_mphy_remove(struct platform_device *pdev)
{
	return 0;
}

struct device *exynos_get_mphy(void)
{
	return lli_mphy;
}
EXPORT_SYMBOL(exynos_get_mphy);

static struct platform_driver exynos_mipi_lli_mphy_driver = {
	.probe		= exynos_mipi_lli_mphy_probe,
	.remove		= exynos_mipi_lli_mphy_remove,
	.driver		= {
		.name	= "exynos-mipi-lli-mphy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_mphy_dt_match),
	},
};

module_platform_driver(exynos_mipi_lli_mphy_driver);

MODULE_DESCRIPTION("Exynos MIPI-LLI MPHY driver");
MODULE_AUTHOR("Yulgon Kim <yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
