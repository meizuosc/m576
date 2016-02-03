/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Seunghyun Song <sh78.song@samsung.com>
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * Chip Abstraction Layer for USB2.0 PHY
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

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/samsung_usb_phy.h>

#include "phy-samsung-usb.h"
#include "phy-samsung-dwc-usb2.h"

void samsung_exynos5_cal_usb2phy_enable(void __iomem *regs_base)
{
	u32 phyctrl0;
	u32 phyctrl1;
	u32 phyehcictrl;
	u32 phytune1;

	phyctrl0 = readl(regs_base + EXYNOS5_PHY_HOST_CTRL0);
	phyctrl0 &= ~HOST_CTRL0_PHYSWRSTALL;
	writel(phyctrl0, regs_base + EXYNOS5_PHY_HOST_CTRL0);

	phyctrl1 = readl(regs_base + EXYNOS5_PHY_HSIC_CTRL1);
	phyctrl1 &= ~HSIC_CTRL1_REFCLKSEL_MASK;
	phyctrl1 |= HSIC_CTRL1_REFCLKSEL(0x2);
	phyctrl1 &= ~HSIC_CTRL1_REFCLKDIV_MASK;
	phyctrl1 |= HSIC_CTRL1_REFCLKDIV(HSIC_CTRL1_REFCLKDIV_12M);
	phyctrl1 &= ~HSIC_CTRL1_SIDDQ;
	phyctrl1 &= ~HSIC_CTRL1_FORCESLEEP;
	phyctrl1 &= ~HSIC_CTRL1_FORCESUSPEND;
	phyctrl1 &= ~HSIC_CTRL1_PHYSWRST;
	writel(phyctrl1, regs_base + EXYNOS5_PHY_HSIC_CTRL1);

	/* set AHB master interface type */
	phyehcictrl = readl(regs_base + EXYNOS5_PHY_HOST_EHCICTRL);
	phyehcictrl |= HOST_EHCICTRL_ENAINCRXALIGN;
	phyehcictrl |= HOST_EHCICTRL_ENAINCR4;
	phyehcictrl |= HOST_EHCICTRL_ENAINCR8;
	phyehcictrl |= HOST_EHCICTRL_ENAINCR16;
	writel(phyehcictrl, regs_base + EXYNOS5_PHY_HOST_EHCICTRL);

	/* phy tuning (default value) */
	phytune1 = readl(regs_base + EXYNOS5_PHY_HSIC_TUNE1);
	phytune1 &= ~HSIC_TUNE1_TXSRTUNE_MASK;
	phytune1 |= HSIC_TUNE1_TXSRTUNE(0x3);
	phytune1 &= ~HSIC_TUNE1_TXPRDTUNE_MASK;
	phytune1 |= HSIC_TUNE1_TXPRDTUNE(0x2);
	phytune1 &= ~HSIC_TUNE1_TXRPUTUNE_MASK;
	phytune1 |= HSIC_TUNE1_TXRPUTUNE(0x2);
	writel(phytune1, regs_base + EXYNOS5_PHY_HSIC_TUNE1);
}

void samsung_exynos5_cal_usb2phy_disable(void __iomem *regs_base)
{
	u32 phyctrl0;
	u32 phyctrl1;

	phyctrl0 = readl(regs_base + EXYNOS5_PHY_HOST_CTRL0);
	phyctrl0 |= HOST_CTRL0_PHYSWRSTALL;
	writel(phyctrl0, regs_base + EXYNOS5_PHY_HOST_CTRL0);

	phyctrl1 = readl(regs_base + EXYNOS5_PHY_HSIC_CTRL1);
	phyctrl1 |= HSIC_CTRL1_SIDDQ;
	phyctrl1 |= HSIC_CTRL1_FORCESLEEP;
	phyctrl1 |= HSIC_CTRL1_FORCESUSPEND;
	writel(phyctrl1, regs_base + EXYNOS5_PHY_HSIC_CTRL1);
}

void samsung_exynos_cal_dwc_usb2phy_enable(void __iomem *regs_base,
		u32 refclkfreq, enum samsung_usb_phy_type phy_type)
{
	u32 phyutmi;
	u32 phyclkrst;
	u32 hostplltune;
	u32 stdphyctrl;
	u32 otglinkctrl;
	u32 hostlinkctrl;
	u32 resume;

	stdphyctrl = readl(regs_base + SAMSUNG_PHY_STDPHYCTRL);
	stdphyctrl &= ~STDPHYCTRL_SIDDQ;
	writel(stdphyctrl, regs_base + SAMSUNG_PHY_STDPHYCTRL);
	udelay(500);

	/* USB Bypass cell clear */
	resume = readl(regs_base + SAMSUNG_RESUME);
	resume &= ~USB_BYPASSSEL;
	resume &= ~USB_BYPASSDPEN;
	resume &= ~USB_BYPASSDMEN;
	writel(resume, regs_base + SAMSUNG_RESUME);

	/* release force_sleep & force_suspend */
	phyutmi = readl(regs_base + SAMSUNG_PHYUTMI);
	phyutmi &= ~PHYUTMI_FORCESLEEP;
	phyutmi &= ~PHYUTMI_FORCESUSPEND;

	/* DP/DM Pull Down Disable */
	phyutmi &= ~PHYUTMI_DMPULLDOWN;
	phyutmi &= ~PHYUTMI_DPPULLDOWN;

	phyutmi |= PHYUTMI_DRVVBUS;
	phyutmi &= ~PHYUTMI_OTGDISABLE;
	writel(phyutmi, regs_base + SAMSUNG_PHYUTMI);

	/* Enable Common Block in suspend/sleep mode */
	phyclkrst = readl(regs_base + SAMSUNG_PHYCLKRST);
	phyclkrst &= ~PHYCLKRST_COMMONONN;

	/* shared reference clock for HS & SS */
	phyclkrst &= ~PHYCLKRST_FSEL_MASK;
	switch (refclkfreq) {
	case PHYCLKRST_FSEL_50M:
		phyclkrst |= PHYCLKRST_FSEL(0x7);
		break;
	case PHYCLKRST_FSEL_24M:
		phyclkrst |= PHYCLKRST_FSEL(0x2);
		hostplltune = readl(regs_base + SAMSUNG_PHY_HOSTPLLTUNE);
		hostplltune |= HOSTPLLTUNE_PLLBTUNE;
		writel(hostplltune, regs_base + SAMSUNG_PHY_HOSTPLLTUNE);
		break;
	default:
		break;
	}
	writel(phyclkrst, regs_base + SAMSUNG_PHYCLKRST);

	/* reset both PHY and Link of Host*/
	stdphyctrl |= STDPHYCTRL_SWRST_PHY;
	stdphyctrl |= STDPHYCTRL_SWRST_ALL;
	writel(stdphyctrl, regs_base + SAMSUNG_PHY_STDPHYCTRL);
	switch (phy_type) {
	case USB_PHY_TYPE_DEVICE:
		otglinkctrl = readl(regs_base + SAMSUNG_LINK_OTGLINKCTL);
		otglinkctrl |= OTGLINKCTRL_SW_RESET_OTG_LINK;
		otglinkctrl |= OTGLINKCTRL_OTG_LINK_PRST;
		writel(otglinkctrl, regs_base + SAMSUNG_LINK_OTGLINKCTL);
		break;
	case USB_PHY_TYPE_HOST:
		hostlinkctrl = readl(regs_base + SAMSUNG_LINK_HOSTLINKCTL);
		hostlinkctrl |= HOSTLINKCTL_LINKSWRST;
		hostlinkctrl |= HOSTLINKCTL_SW_RESET_PORT0;
		hostlinkctrl |= HOSTLINKCTL_SW_RESET_PORT1;
		hostlinkctrl |= HOSTLINKCTL_SW_RESET_PORT2;
		writel(hostlinkctrl, regs_base + SAMSUNG_LINK_HOSTLINKCTL);
		break;
	}

	udelay(10);

	stdphyctrl &= ~STDPHYCTRL_SWRST_ALL;
	stdphyctrl &= ~STDPHYCTRL_SWRST_PHY;
	writel(stdphyctrl, regs_base + SAMSUNG_PHY_STDPHYCTRL);

	switch (phy_type) {
	case USB_PHY_TYPE_DEVICE:
		otglinkctrl &= ~OTGLINKCTRL_SW_RESET_OTG_LINK;
		otglinkctrl &= ~OTGLINKCTRL_OTG_LINK_PRST;
		writel(otglinkctrl, regs_base + SAMSUNG_LINK_OTGLINKCTL);
		break;
	case USB_PHY_TYPE_HOST:
		hostlinkctrl &= ~HOSTLINKCTL_LINKSWRST;
		hostlinkctrl &= ~HOSTLINKCTL_SW_RESET_PORT0;
		hostlinkctrl &= ~HOSTLINKCTL_SW_RESET_PORT1;
		hostlinkctrl &= ~HOSTLINKCTL_SW_RESET_PORT2;
		writel(hostlinkctrl, regs_base + SAMSUNG_LINK_HOSTLINKCTL);
		break;
	}
	udelay(50);
}
void samsung_exynos_cal_dwc_usb2phy_disable(void __iomem *regs_base)
{
	u32 phyclkrst;
	u32 phyutmi;
	u32 stdphyctrl;
	u32 otglinkctrl;
	u32 hostlinkctrl;

	/* disable Common Block */
	phyclkrst = readl(regs_base + SAMSUNG_PHYCLKRST);
	phyclkrst |= PHYCLKRST_COMMONONN;
	writel(phyclkrst, regs_base + SAMSUNG_PHYCLKRST);

	/* disable power block */
	phyutmi = readl(regs_base + SAMSUNG_PHYUTMI);
	phyutmi |= PHYUTMI_FORCESLEEP;
	phyutmi |= PHYUTMI_FORCESUSPEND;
	phyutmi &= ~PHYUTMI_DRVVBUS;
	phyutmi |= PHYUTMI_OTGDISABLE;
	writel(phyutmi, regs_base + SAMSUNG_PHYUTMI);

	/* reset both PHY and Link of Host*/
	stdphyctrl = readl(regs_base + SAMSUNG_PHY_STDPHYCTRL);
	stdphyctrl |= STDPHYCTRL_SWRST_PHY;
	stdphyctrl |= STDPHYCTRL_SWRST_ALL;
	writel(stdphyctrl, regs_base + SAMSUNG_PHY_STDPHYCTRL);

	otglinkctrl = readl(regs_base + SAMSUNG_LINK_OTGLINKCTL);
	otglinkctrl |= OTGLINKCTRL_SW_RESET_OTG_LINK;
	otglinkctrl |= OTGLINKCTRL_OTG_LINK_PRST;
	writel(otglinkctrl, regs_base + SAMSUNG_LINK_OTGLINKCTL);

	hostlinkctrl = readl(regs_base + SAMSUNG_LINK_HOSTLINKCTL);
	hostlinkctrl |= HOSTLINKCTL_LINKSWRST;
	hostlinkctrl |= HOSTLINKCTL_SW_RESET_PORT0;
	hostlinkctrl |= HOSTLINKCTL_SW_RESET_PORT1;
	hostlinkctrl |= HOSTLINKCTL_SW_RESET_PORT2;
	writel(hostlinkctrl, regs_base + SAMSUNG_LINK_HOSTLINKCTL);

	/* phy power off */
	stdphyctrl = readl(regs_base + SAMSUNG_PHY_STDPHYCTRL);
	stdphyctrl |= STDPHYCTRL_SIDDQ;
	writel(stdphyctrl, regs_base + SAMSUNG_PHY_STDPHYCTRL);
}
