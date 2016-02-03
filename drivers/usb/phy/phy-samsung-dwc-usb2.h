/* linux/drivers/usb/phy/phy-samsung-dwc-usb2.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Samsung USB-PHY transceiver; talks to S3C HS OTG controller, EHCI-S5P and
 * OHCI-EXYNOS controllers.
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

#include <linux/usb/phy.h>

/* Register definitions */

/* For exynos7580 */
/* DWC_PHY_UTMI */
#define SAMSUNG_PHYUTMI				(0x08)
#define PHYUTMI_OTGDISABLE			(0x1 << 6)
#define PHYUTMI_IDPULLUP			(0x1 << 5)
#define PHYUTMI_DRVVBUS				(0x1 << 4)
#define PHYUTMI_DPPULLDOWN			(0x1 << 3)
#define PHYUTMI_DMPULLDOWN			(0x1 << 2)
#define PHYUTMI_FORCESUSPEND			(0x1 << 1)
#define PHYUTMI_FORCESLEEP			(0x1 << 0)

/* DWC_PHY_CLKPWR */
#define SAMSUNG_PHYCLKRST			(0x10)
#define STDPHYCLKRST_FSEL_MASK			(0x7 << 5)
#define PHYCLKRST_FSEL(_x)			((_x) << 5)
#define PHYCLKRST_FSEL_50M			(0x7)
#define PHYCLKRST_FSEL_24M			(0x5)
#define PHYCLKRST_FSEL_20M			(0x4)
#define PHYCLKRST_FSEL_19200K			(0x3)
#define PHYCLKRST_FSEL_12M			(0x2)
#define PHYCLKRST_FSEL_10M			(0x1)
#define PHYCLKRST_FSEL_9600K			(0x0)
#define PHYCLKRST_COMMONONN			(0x1 << 0)

/* DWC_PHY_PARAM0 */
#define SAMSUNG_PHYPARAM0			(0x1c)
#define PHYPARAM0_SQRXTUNE_MASK			(0x7 << 6)
#define PHYPARAM0_SQRXTUNE(_x)			((_x) << 6)
#define PHYPARAM0_COMPDISTUNE_MASK		(0x7 << 0)
#define PHYPARAM0_COMPDISTUNE(_x)		((_x) << 0)

/* USB3_PHY_RESUME */
#define SAMSUNG_RESUME				(0x34)
#define USB_BYPASSDPEN				(0x1 << 2)
#define USB_BYPASSDMEN				(0x1 << 3)
#define USB_BYPASSSEL				(0x1 << 4)

/* USB_PHY_STDPHYCTRL */
#define SAMSUNG_PHY_STDPHYCTRL			(0x54)
#define STDPHYCTRL_SWRST_PHY			(0x1 << 0)
#define STDPHYCTRL_SIDDQ			(0x1 << 6)
#define STDPHYCTRL_SWRST_ALL			(0x1 << 31)

/* SAMSUNG_HOSTPLLTUNE */
#define SAMSUNG_PHY_HOSTPLLTUNE			(0x70)
#define HOSTPLLTUNE_PLLBTUNE			(0x1 << 6)
#define HOSTPLLTUNE_PLLITUNE_MASK		(0x3 << 4)
#define HOSTPLLTUNE_PLLITUNE(_x)		((_x) << 4)
#define HOSTPLLTUNE_PLLPTUNE_MASK		(0xf << 0)
#define HOSTPLLTUNE_PLLPTUNE(_x)		((_x) << 0)

/* SAMSUNG_LINK_HOSTLINKCTL */
#define SAMSUNG_LINK_HOSTLINKCTL		(0x88)
#define HOSTLINKCTL_EN_SLEEP_HOST_P2		(0x1 << 31)
#define HOSTLINKCTL_EN_SLEEP_HOST_P1		(0x1 << 30)
#define HOSTLINKCTL_EN_SLEEP_HOST_P0		(0x1 << 29)
#define HOSTLINKCTL_EN_SLEEP_OTG		(0x1 << 28)
#define HOSTLINKCTL_EN_SUSPEND_HOST_P2		(0x1 << 27)
#define HOSTLINKCTL_EN_SUSPEND_HOST_P1		(0x1 << 26)
#define HOSTLINKCTL_EN_SUSPEND_HOST_P0		(0x1 << 25)
#define HOSTLINKCTL_EN_SUSPEND_OTG		(0x1 << 24)
#define HOSTLINKCTL_FORCE_HOST_OVERCUR_P2	(0x1 << 17)
#define HOSTLINKCTL_FORCE_HOST_OVERCUR		(0x1 << 16)
#define HOSTLINKCTL_SW_RESET_PORT2		(0x1 << 3)
#define HOSTLINKCTL_SW_RESET_PORT1		(0x1 << 2)
#define HOSTLINKCTL_SW_RESET_PORT0		(0x1 << 1)
#define HOSTLINKCTL_LINKSWRST			(0x1 << 0)

/* SAMSUNG_LINK_OTGLINKCTL */
#define SAMSUNG_LINK_OTGLINKCTL			(0x8c)
#define OTGLINKCTRL_AVALID			(0x1 << 14)
#define OTGLINKCTRL_BVALID			(0x1 << 13)
#define OTGLINKCTRL_IDDIG			(0x1 << 12)
#define OTGLINKCTRL_VBUSDETECT			(0x1 << 11)
#define OTGLINKCTRL_VBUSVLDSEL_MASK		(0x3 << 9)
#define OTGLINKCTRL_VBUSVLDSEL(_x)		((_x) << 9)
#define OTGLINKCTRL_SCALEDOWN_MODE_MASK		(0x3 << 7)
#define OTGLINKCTRL_SCALEDOWN_MODE(_x)		((_x) << 7)
#define OTGLINKCTRL_ENDIAN_SELM			(0x1 << 6)
#define OTGLINKCTRL_ENDIAN_SELS			(0x1 << 5)
#define OTGLINKCTRL_OTG_LINK_PRST		(0x1 << 4)
#define OTGLINKCTRL_SW_RESET_OTG_LINK		(0x1 << 3)

#define EXYNOS_USB_PHY_CTRL_OFFSET		(0x0)

void samsung_exynos_cal_dwc_usb2phy_enable(void __iomem *regs_base,
		u32 refclkfreq, enum samsung_usb_phy_type phy_type);
void samsung_exynos_cal_dwc_usb2phy_disable(void __iomem *regs_base);
