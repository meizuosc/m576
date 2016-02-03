/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Seunghyun Song <sh78.song@samsung.com>
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * Chip Abstraction Layer for USB3.0 PHY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "phy-samsung-usb-cal.h"
#include "phy-samsung-usb3-cal.h"

void samsung_exynos5_cal_usb3phy_enable(void __iomem *regs_base,
					u32 refclkfreq)
{
	u32 phyutmi;
	u32 phytest;
	u32 phyclkrst;
	u32 phyparam0;

	phytest = readl(regs_base + EXYNOS5_DRD_PHYTEST);
	phytest &= ~PHYTEST_POWERDOWN_HSP;
	phytest &= ~PHYTEST_POWERDOWN_SSP;
	writel(phytest, regs_base + EXYNOS5_DRD_PHYTEST);
	udelay(500);

	/* release force_sleep & force_suspend */
	phyutmi = readl(regs_base + EXYNOS5_DRD_PHYUTMI);
	phyutmi &= ~PHYUTMI_FORCESLEEP;
	phyutmi &= ~PHYUTMI_FORCESUSPEND;

	/* DP/DM Pull Down Disable */
	phyutmi &= ~PHYUTMI_DMPULLDOWN;
	phyutmi &= ~PHYUTMI_DPPULLDOWN;

	phyutmi &= ~PHYUTMI_DRVVBUS;
	phyutmi &= ~PHYUTMI_OTGDISABLE;
	writel(phyutmi, regs_base + EXYNOS5_DRD_PHYUTMI);

	/* set phy clock & control HS phy */
	phyclkrst = readl(regs_base + EXYNOS5_DRD_PHYCLKRST);

	/* Enable Common Block in suspend/sleep mode */
	phyclkrst |= PHYCLKRST_COMMONONN;

	/* Digital Supply Mode : normal operating mode */
	phyclkrst |= PHYCLKRST_RETENABLEN;

	/* assert port_reset */
	phyclkrst |= PHYCLKRST_PORTRESET;

	/* ref. clock enable for ss function */
	phyclkrst |= PHYCLKRST_REF_SSP_EN;

	/* shared reference clock for HS & SS */
	phyclkrst &= ~PHYCLKRST_REFCLKSEL_MASK;
	phyclkrst |= PHYCLKRST_REFCLKSEL_PAD_REFCLK;
	phyparam0 = readl(regs_base + EXYNOS5_DRD_PHYPARAM0);
	phyparam0 &= ~PHYPARAM0_REF_USE_PAD;
	writel(phyparam0, regs_base + EXYNOS5_DRD_PHYPARAM0);

	phyclkrst &= ~PHYCLKRST_FSEL_MASK;
	phyclkrst |= PHYCLKRST_FSEL(refclkfreq & 0x3f);

	switch (refclkfreq) {
	case FSEL_CLKSEL_DIFF_100M:
		phyclkrst &= ~PHYCLKRST_MPLL_MULTIPLIER_MASK;
		phyclkrst |= PHYCLKRST_MPLL_MULTIPLIER_100MHZ_REF;
		phyclkrst &= ~PHYCLKRST_REF_CLKDIV2;
		phyclkrst &= ~PHYCLKRST_SSC_REFCLKSEL_MASK;
		phyclkrst |= PHYCLKRST_SSC_REFCLKSEL(0x00);
		break;
	case FSEL_CLKSEL_DIFF_24M:
	case FSEL_CLKSEL_24M:
		phyclkrst &= ~PHYCLKRST_MPLL_MULTIPLIER_MASK;
		phyclkrst |= PHYCLKRST_MPLL_MULTIPLIER_24MHZ_REF;
		phyclkrst &= ~PHYCLKRST_REF_CLKDIV2;
		phyclkrst &= ~PHYCLKRST_SSC_REFCLKSEL_MASK;
		phyclkrst |= PHYCLKRST_SSC_REFCLKSEL(0x88);
		break;
	case FSEL_CLKSEL_DIFF_20M:
	case FSEL_CLKSEL_20M:
		phyclkrst &= ~PHYCLKRST_MPLL_MULTIPLIER_MASK;
		phyclkrst |= PHYCLKRST_MPLL_MULTIPLIER_20MHZ_REF;
		phyclkrst &= ~PHYCLKRST_REF_CLKDIV2;
		phyclkrst &= ~PHYCLKRST_SSC_REFCLKSEL_MASK;
		phyclkrst |= PHYCLKRST_SSC_REFCLKSEL(0x00);
		break;
	case FSEL_CLKSEL_DIFF_19200K:
	case FSEL_CLKSEL_19200K:
		phyclkrst &= ~PHYCLKRST_MPLL_MULTIPLIER_MASK;
		phyclkrst |= PHYCLKRST_MPLL_MULTIPLIER_19200KHZ_REF;
		phyclkrst &= ~PHYCLKRST_REF_CLKDIV2;
		phyclkrst &= ~PHYCLKRST_SSC_REFCLKSEL_MASK;
		phyclkrst |= PHYCLKRST_SSC_REFCLKSEL(0x88);
		break;
	default:
		break;
	}

	/* Spread Spectrum Control */
	phyclkrst |= PHYCLKRST_SSC_EN;
	phyclkrst &= ~PHYCLKRST_SSC_RANGE_MASK;
	phyclkrst |= PHYCLKRST_SSC_RANGE(0x0);
	phyclkrst &= ~PHYCLKRST_EN_UTMISUSPEND;
	writel(phyclkrst, regs_base + EXYNOS5_DRD_PHYCLKRST);
	udelay(10);
	phyclkrst &= ~PHYCLKRST_PORTRESET;
	writel(phyclkrst, regs_base + EXYNOS5_DRD_PHYCLKRST);

}

void samsung_exynos5_cal_usb3phy_disable(void __iomem *regs_base)
{
	u32 phyutmi;
	u32 phyclkrst;
	u32 phytest;

	phyutmi = readl(regs_base + EXYNOS5_DRD_PHYCLKRST);
	phyutmi &= ~PHYCLKRST_COMMONONN;
	phyutmi |= PHYCLKRST_RETENABLEN;
	phyutmi &= ~PHYCLKRST_REF_SSP_EN;
	phyutmi &= ~PHYCLKRST_SSC_EN;
	writel(phyutmi, regs_base + EXYNOS5_DRD_PHYCLKRST);

	phyclkrst = readl(regs_base + EXYNOS5_DRD_PHYUTMI);
	phyclkrst &= ~PHYUTMI_IDPULLUP;
	phyclkrst &= ~PHYUTMI_DRVVBUS;
	phyclkrst |= PHYUTMI_FORCESUSPEND;
	phyclkrst |= PHYUTMI_FORCESLEEP;
	writel(phyclkrst, regs_base + EXYNOS5_DRD_PHYUTMI);

	phytest = readl(regs_base + EXYNOS5_DRD_PHYTEST);
	phytest |= PHYTEST_POWERDOWN_SSP;
	/* if cpu_type != Exynos5430 */
	phytest |= PHYTEST_POWERDOWN_HSP;
	writel(phytest, regs_base + EXYNOS5_DRD_PHYTEST);
}

void samsung_exynos5_cal_usb3phy_crport_handshake(
					void __iomem *regs_base,
					u32 val, u32 cmd)
{
	u32 usec = 100;
	u32 result;

	writel(val | cmd, regs_base + EXYNOS5_DRD_PHYREG0);

	do {
		result = readl(regs_base + EXYNOS5_DRD_PHYREG1);
		if (result & EXYNOS5_DRD_PHYREG1_CR_ACK)
			break;

		udelay(1);
	} while (usec-- > 0);

	if (!usec)
		pr_err("CRPORT handshake timeout1 (0x%08x)\n", val);

	usec = 100;

	writel(val, regs_base + EXYNOS5_DRD_PHYREG0);

	do {
		result = readl(regs_base + EXYNOS5_DRD_PHYREG1);
		if (!(result & EXYNOS5_DRD_PHYREG1_CR_ACK))
			break;

		udelay(1);
	} while (usec-- > 0);

	if (!usec)
		pr_err("CRPORT handshake timeout2 (0x%08x)\n", val);
}

void samsung_exynos5_cal_usb3phy_crport_ctrl(void *regs_base,
						u16 addr, u32 data)
{
	/* Write Address */
	writel(EXYNOS5_DRD_PHYREG0_CR_DATA_IN(addr),
			regs_base + EXYNOS5_DRD_PHYREG0);
	samsung_exynos5_cal_usb3phy_crport_handshake(regs_base,
			EXYNOS5_DRD_PHYREG0_CR_DATA_IN(addr),
			EXYNOS5_DRD_PHYREG0_CR_CR_CAP_ADDR);

	/* Write Data */
	writel(EXYNOS5_DRD_PHYREG0_CR_DATA_IN(data),
			regs_base + EXYNOS5_DRD_PHYREG0);
	samsung_exynos5_cal_usb3phy_crport_handshake(regs_base,
			EXYNOS5_DRD_PHYREG0_CR_DATA_IN(data),
			EXYNOS5_DRD_PHYREG0_CR_CR_CAP_DATA);
	samsung_exynos5_cal_usb3phy_crport_handshake(regs_base,
			EXYNOS5_DRD_PHYREG0_CR_DATA_IN(data),
			EXYNOS5_DRD_PHYREG0_CR_WRITE);
}

void samsung_exynos5_cal_usb3phy_set_cr_port(void __iomem *regs_base)
{
	/* Set los_bias to 0x5 and los_level to 0x9 */
	samsung_exynos5_cal_usb3phy_crport_ctrl(regs_base, 0x15, 0xA409);

	/* Set TX_VBOOST_LEVLE to default Value (0x4) */
	samsung_exynos5_cal_usb3phy_crport_ctrl(regs_base, 0x12, 0x8000);

	/* to set the charge pump proportional current */
	samsung_exynos5_cal_usb3phy_crport_ctrl(regs_base, 0x30, 0xC0);

	/* Set RXDET_MEAS_TIME[11:4] to 24MHz clock value: 0x80 */
	samsung_exynos5_cal_usb3phy_crport_ctrl(regs_base, 0x1010, 0x80);
}

void samsung_exynos5_cal_usb3phy_tune_dev(void __iomem *regs_base)
{
	u32 linksystem;
	u32 phypipe;
	u32 phyparam0;
	u32 phyparam1;
	u32 phyparam2;
	u32 phypcsval;

	/* Set the LINK Version Control and Frame Adjust Value */
	linksystem = readl(regs_base + EXYNOS5_DRD_LINKSYSTEM);
	linksystem &= ~LINKSYSTEM_FLADJ_MASK;
	linksystem |= LINKSYSTEM_FLADJ(0x20);
	linksystem |= LINKSYSTEM_XHCI_VERSION_CONTROL;
	writel(linksystem, regs_base + EXYNOS5_DRD_LINKSYSTEM);

	/* Select UTMI CLOCK 0 : PHY CLOCK, 1 : FREE CLOCK */
	phypipe = readl(regs_base + EXYNOS5_DRD_PHYPIPE);
	phypipe |= PHY_CLOCK_SEL;
	writel(phypipe, regs_base + EXYNOS5_DRD_PHYPIPE);

	/* Tuning the USB3 HS Pre-empasis and TXVREFTUNE */
	phyparam0 = readl(regs_base + EXYNOS5_DRD_PHYPARAM0);
	/* compdistune[2:0] 3'b100 : default value */
	phyparam0 &= ~PHYPARAM0_COMPDISTUNE_MASK;
	phyparam0 |= PHYPARAM0_COMPDISTUNE(0x4);
	/* sqrxtune[8:6] 3'b111 : -20% */
	phyparam0 &= ~PHYPARAM0_SQRXTUNE_MASK;
	phyparam0 |= PHYPARAM0_SQRXTUNE(0x3 /* 0x7 */);
	/* txpreempamptune[16:15] 2'b11 : 3X  */
	phyparam0 &= ~PHYPARAM0_TXPREEMPAMPTUNE_MASK;
	phyparam0 |= PHYPARAM0_TXPREEMPAMPTUNE(0x3);
	/* txresitune */
	phyparam0 &= ~PHYPARAM0_TXRISETUNE_MASK;
	phyparam0 |= PHYPARAM0_TXRISETUNE(0x3);
	/* txvreftune[25:22] 4'b1111 : i5% */
	phyparam0 &= ~PHYPARAM0_TXVREFTUNE_MASK;
	phyparam0 |= PHYPARAM0_TXVREFTUNE(0xf);
	writel(phyparam0, regs_base + EXYNOS5_DRD_PHYPARAM0);

	/* Set the PHY Signal Quality Tuning Value */
	phyparam1 = readl(regs_base + EXYNOS5_DRD_PHYPARAM1);
	phyparam1 &= ~PHYPARAM1_PCS_TXSWING_FULL_MASK;
	phyparam1 |= PHYPARAM1_PCS_TXSWING_FULL(0x7f);
	phyparam1 &= ~PHYPARAM1_PCS_TXDEEMPH_3P5DB_MASK;
	phyparam1 |= PHYPARAM1_PCS_TXDEEMPH_3P5DB(0x18);
	writel(phyparam1, regs_base + EXYNOS5_DRD_PHYPARAM1);

	/* Set vboost value for eye diagram */
	phyparam2 = readl(regs_base + EXYNOS5_DRD_PHYPARAM2);
	phyparam2 &= ~PHYPARAM2_TX_VBOOST_LVL_MASK;
	phyparam2 |= PHYPARAM2_TX_VBOOST_LVL(0x5);
	writel(phyparam2, regs_base + EXYNOS5_DRD_PHYPARAM2);

	/*
	 * Set pcs_rx_los_mask_val for 14nm PHY to mask the abnormal
	 * LFPS and glitches
	 */
	phypcsval = readl(regs_base + EXYNOS5_DRD_PHYPCSVAL);
	phypcsval &= ~PHYPCSVAL_PCS_RX_LOS_MASK_VAL_MASK;
	phypcsval |= PHYPCSVAL_PCS_RX_LOS_MASK_VAL(0x18);
	writel(phypcsval, regs_base + EXYNOS5_DRD_PHYPCSVAL);
}

void samsung_exynos5_cal_usb3phy_tune_host(void __iomem *regs_base)
{
	u32 linksystem;
	u32 phypipe;
	u32 phyparam0;
	u32 phyparam1;
	u32 phyparam2;
	u32 phypcsval;

	/* Set the LINK Version Control and Frame Adjust Value */
	linksystem = readl(regs_base + EXYNOS5_DRD_LINKSYSTEM);
	linksystem &= ~LINKSYSTEM_FLADJ_MASK;
	linksystem |= LINKSYSTEM_FLADJ(0x20);
	linksystem |= LINKSYSTEM_XHCI_VERSION_CONTROL;
	writel(linksystem, regs_base + EXYNOS5_DRD_LINKSYSTEM);

	/* Select UTMI CLOCK 0 : PHY CLOCK, 1 : FREE CLOCK */
	phypipe = readl(regs_base + EXYNOS5_DRD_PHYPIPE);
	phypipe |= PHY_CLOCK_SEL;
	writel(phypipe, regs_base + EXYNOS5_DRD_PHYPIPE);

	phyparam0 = readl(regs_base + EXYNOS5_DRD_PHYPARAM0);
	/* compdistune[2:0] 3'b111 : +4.5% */
	phyparam0 &= ~PHYPARAM0_COMPDISTUNE_MASK;
	phyparam0 |= PHYPARAM0_COMPDISTUNE(0x7);
	/* sqrxtune[8:6] 3'b011 : default value */
	phyparam0 &= ~PHYPARAM0_SQRXTUNE_MASK;
	phyparam0 |= PHYPARAM0_SQRXTUNE(0x3);
	/* txpreempamptune[16:15] 2'b00 : default value */
	phyparam0 &= ~PHYPARAM0_TXPREEMPAMPTUNE_MASK;
	phyparam0 |= PHYPARAM0_TXPREEMPAMPTUNE(0x0);
	/* txvreftune[25:22] 4'b0001 : -2.5% */
	phyparam0 &= ~PHYPARAM0_TXVREFTUNE_MASK;
	phyparam0 |= PHYPARAM0_TXVREFTUNE(0x1);
	writel(phyparam0, regs_base + EXYNOS5_DRD_PHYPARAM0);

	/* Set the PHY Signal Quality Tuning Value */
	phyparam1 = readl(regs_base + EXYNOS5_DRD_PHYPARAM1);
	phyparam1 &= ~PHYPARAM1_PCS_TXSWING_FULL_MASK;
	phyparam1 |= PHYPARAM1_PCS_TXSWING_FULL(0x7f);
	phyparam1 &= ~PHYPARAM1_PCS_TXDEEMPH_3P5DB_MASK;
	phyparam1 |= PHYPARAM1_PCS_TXDEEMPH_3P5DB(0x18);
	writel(phyparam1, regs_base + EXYNOS5_DRD_PHYPARAM1);

	/* Set vboost value for eye diagram */
	phyparam2 = readl(regs_base + EXYNOS5_DRD_PHYPARAM2);
	phyparam2 &= ~PHYPARAM2_TX_VBOOST_LVL_MASK;
	phyparam2 |= PHYPARAM2_TX_VBOOST_LVL(0x5);
	writel(phyparam2, regs_base + EXYNOS5_DRD_PHYPARAM2);

	/*
	 * Set pcs_rx_los_mask_val for 14nm PHY to mask the abnormal
	 * LFPS and glitches
	 */
	phypcsval = readl(regs_base + EXYNOS5_DRD_PHYPCSVAL);
	phypcsval &= ~PHYPCSVAL_PCS_RX_LOS_MASK_VAL_MASK;
	phypcsval |= PHYPCSVAL_PCS_RX_LOS_MASK_VAL(0x18);
	writel(phypcsval, regs_base + EXYNOS5_DRD_PHYPCSVAL);
}
