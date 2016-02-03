#ifndef __PHY_SAMSUNG_USB3_FW_CAL_H__
#define __PHY_SAMSUNG_USB3_FW_CAL_H__

#include "phy-samsung-usb-cal.h"

#define EXYNOS5_DRD_LINKSYSTEM			(0x04)
#define LINKSYSTEM_FLADJ_MASK			(0x3f << 1)
#define LINKSYSTEM_FLADJ(_x)			((_x) << 1)
#define LINKSYSTEM_XHCI_VERSION_CONTROL		(0x1 << 27)

#define EXYNOS5_DRD_PHYUTMI			(0x08)
#define PHYUTMI_OTGDISABLE			(0x1 << 6)
#define PHYUTMI_IDPULLUP			(0x1 << 5)
#define PHYUTMI_DRVVBUS				(0x1 << 4)
#define PHYUTMI_DPPULLDOWN			(0x1 << 3)
#define PHYUTMI_DMPULLDOWN			(0x1 << 2)
#define PHYUTMI_FORCESUSPEND			(0x1 << 1)
#define PHYUTMI_FORCESLEEP			(0x1 << 0)

#define EXYNOS5_DRD_PHYCLKRST			(0x10)
#define PHYCLKRST_EN_UTMISUSPEND		(0x1 << 31)
#define PHYCLKRST_SSC_REFCLKSEL_MASK		(0xff << 23)
#define PHYCLKRST_SSC_REFCLKSEL(_x)		((_x) << 23)
#define PHYCLKRST_SSC_RANGE_MASK		(0x03 << 21)
#define PHYCLKRST_SSC_RANGE(_x)			((_x) << 21)
#define PHYCLKRST_SSC_EN			(0x1 << 20)
#define PHYCLKRST_REF_SSP_EN			(0x1 << 19)
#define PHYCLKRST_REF_CLKDIV2			(0x1 << 18)
#define PHYCLKRST_MPLL_MULTIPLIER_MASK		(0x7f << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_100MHZ_REF	(0x19 << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_50M_REF	(0x02 << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_24MHZ_REF	(0x68 << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_20MHZ_REF	(0x7d << 11)
#define PHYCLKRST_MPLL_MULTIPLIER_19200KHZ_REF	(0x02 << 11)
#define PHYCLKRST_FSEL_MASK			(0x3f << 5)
#define PHYCLKRST_FSEL(_x)			((_x) << 5)
#define PHYCLKRST_FSEL_PAD_100MHZ		(0x27 << 5)
#define PHYCLKRST_FSEL_PAD_24MHZ		(0x2a << 5)
#define PHYCLKRST_FSEL_PAD_20MHZ		(0x31 << 5)
#define PHYCLKRST_FSEL_PAD_19_2MHZ		(0x38 << 5)
#define PHYCLKRST_RETENABLEN			(0x1 << 4)
#define PHYCLKRST_REFCLKSEL_MASK		(0x03 << 2)
#define PHYCLKRST_REFCLKSEL_PAD_REFCLK		(0x2 << 2)
#define PHYCLKRST_REFCLKSEL_EXT_REFCLK		(0x3 << 2)
#define PHYCLKRST_PORTRESET			(0x1 << 1)
#define PHYCLKRST_COMMONONN			(0x1 << 0)

#define EXYNOS5_DRD_PHYPIPE			(0x0c)
#define PHY_CLOCK_SEL				(0x1 << 4)

#define EXYNOS5_DRD_PHYREG0                     (0x14)
#define EXYNOS5_DRD_PHYREG0_CR_WRITE            (1 << 19)
#define EXYNOS5_DRD_PHYREG0_CR_READ             (1 << 18)
#define EXYNOS5_DRD_PHYREG0_CR_DATA_IN(_x)      ((_x) << 2)
#define EXYNOS5_DRD_PHYREG0_CR_CR_CAP_DATA      (1 << 1)
#define EXYNOS5_DRD_PHYREG0_CR_CR_CAP_ADDR      (1 << 0)

#define EXYNOS5_DRD_PHYREG1                     (0x18)
#define EXYNOS5_DRD_PHYREG1_CR_DATA_OUT(_x)     ((_x) << 1)
#define EXYNOS5_DRD_PHYREG1_CR_ACK              (1 << 0)

#define EXYNOS5_DRD_PHYPARAM0			(0x1c)
#define PHYPARAM0_REF_USE_PAD			(0x1 << 31)
#define PHYPARAM0_REF_LOSLEVEL_MASK		(0x1f << 26)
#define PHYPARAM0_REF_LOSLEVEL			(0x9 << 26)
#define PHYPARAM0_TXVREFTUNE_MASK		(0xf << 22)
#define PHYPARAM0_TXVREFTUNE(_x)		((_x) << 22)
#define PHYPARAM0_TXRISETUNE_MASK		(0x3 << 20)
#define PHYPARAM0_TXRISETUNE(_x)		((_x) << 20)
#define PHYPARAM0_TXPREEMPAMPTUNE_MASK		(0x3 << 15)
#define PHYPARAM0_TXPREEMPAMPTUNE(_x)		((_x) << 15)
#define PHYPARAM0_SQRXTUNE_MASK			(0x7 << 6)
#define PHYPARAM0_SQRXTUNE(_x)			((_x) << 6)
#define PHYPARAM0_COMPDISTUNE_MASK		(0x7 << 0)
#define PHYPARAM0_COMPDISTUNE(_x)		((_x) << 0)

#define EXYNOS5_DRD_PHYPARAM1			(0x20)
#define PHYPARAM1_TX0_TERM_OFFSET_MASK		(0x1f << 26)
#define PHYPARAM1_TX0_TERM_OFFSET(_x)		((_x) << 26)
#define PHYPARAM1_PCS_TXSWING_FULL_MASK		(0x7f << 12)
#define PHYPARAM1_PCS_TXSWING_FULL(_x)		((_x) << 12)
#define PHYPARAM1_PCS_TXDEEMPH_3P5DB_MASK	(0x3f << 0)
#define PHYPARAM1_PCS_TXDEEMPH_3P5DB(_x)	((_x) << 0)

#define EXYNOS5_DRD_PHYTEST			(0x28)
#define PHYTEST_POWERDOWN_SSP			(0x1 << 3)
#define PHYTEST_POWERDOWN_HSP			(0x1 << 2)

#define EXYNOS5_DRD_PHYPCSVAL			(0x3C)
#define PHYPCSVAL_PCS_RX_LOS_MASK_VAL_MASK	(0x3FF << 0)
#define PHYPCSVAL_PCS_RX_LOS_MASK_VAL(_x)	((_x) << 0)

#define EXYNOS5_DRD_PHYPARAM2			(0x50)
#define PHYPARAM2_LOS_BIAS_MASK			(0x7 << 0)
#define PHYPARAM2_LOS_BIAS(_x)			((_x) << 0)
#define PHYPARAM2_TX_VBOOST_LVL_MASK		(0x7 << 4)
#define PHYPARAM2_TX_VBOOST_LVL(_x)		((_x) << 4)

void samsung_exynos5_cal_usb3phy_enable(void __iomem *, u32);
void samsung_exynos5_cal_usb3phy_disable(void __iomem *);
void samsung_exynos5_cal_usb3phy_crport_handshake(void __iomem *, u32, u32);
void samsung_exynos5_cal_usb3phy_crport_ctrl(void __iomem *, u16, u32);
void samsung_exynos5_cal_usb3phy_set_cr_port(void __iomem *);
void samsung_cal_usb3phy_tune(void __iomem *);

#define FSEL_CLKSEL_DIFF_100M		0x27
#define FSEL_CLKSEL_DIFF_24M		0x2a
#define FSEL_CLKSEL_24M			0x5
#define FSEL_CLKSEL_DIFF_20M		0x31
#define FSEL_CLKSEL_20M			0x4
#define FSEL_CLKSEL_DIFF_19200K		0x38
#define FSEL_CLKSEL_19200K		0x3

#endif
