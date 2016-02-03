/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */

#ifndef _REGS_DSIM_H
#define _REGS_DSIM_H

#define DSIM_STATUS				(0x4)
#define DSIM_STATUS_PLL_STABLE			(1 << 31)
#define DSIM_STATUS_STOP_STATE_DAT(_x)		(((_x) & 0xf) << 0)
#define DSIM_STATUS_ULPS_DAT(_x)		(((_x) & 0xf) << 4)
#define DSIM_STATUS_STOP_STATE_CLK		(1 << 8)
#define DSIM_STATUS_ULPS_CLK			(1 << 9)
#define DSIM_STATUS_TX_READY_HS_CLK		(1 << 10)
#define DSIM_STATUS_ULPS_DATA_LANE_GET(x)	(((x) >> 4) & 0xf)

#define DSIM_SWRST				(0xc)
#define DSIM_SWRST_FUNCRST			(1 << 16)
#define DSIM_SWRST_RESET			(1 << 0)

#define DSIM_CLKCTRL				(0x10)
#define DSIM_CLKCTRL_TX_REQUEST_HSCLK		(1 << 31)
#define DSIM_CLKCTRL_ESCCLK_EN			(1 << 28)
#define DSIM_CLKCTRL_BYTE_CLK_SRC_MASK		(0x3 << 25)
#define DSIM_CLKCTRL_BYTECLK_EN			(1 << 24)
#define DSIM_CLKCTRL_LANE_ESCCLK_EN_MASK	(0x1f << 19)
#define DSIM_CLKCTRL_LANE_ESCCLK_EN(_x)		((_x) << 19)
#define DSIM_CLKCTRL_ESC_PRESCALER(_x)		((_x) << 0)
#define DSIM_CLKCTRL_ESC_PRESCALER_MASK		(0xffff << 0)

/* Time out register */
#define DSIM_TIMEOUT				(0x14)
#define DSIM_TIMEOUT_BTA_TOUT(_x)		((_x) << 16)
#define DSIM_TIMEOUT_BTA_TOUT_MASK		(0xff << 16)
#define DSIM_TIMEOUT_LPDR_TOUT(_x)		((_x) << 0)
#define DSIM_TIMEOUT_LPDR_TOUT_MASK		(0xffff << 0)

/* Configuration register */
#define DSIM_CONFIG				(0x18)
#define DSIM_CONFIG_NONCONTINUOUS_CLOCK_LANE	(1 << 31)
#define DSIM_CONFIG_CLKLANE_STOP_START		(1 << 30)
#define DSIM_CONFIG_EOT_R03_DISABLE		(1 << 28)	/* disable EoT packet generation for V1.01r03 */
#define DSIM_CONFIG_BURST_MODE			(1 << 26)
#define DSIM_CONFIG_VIDEO_MODE			(1 << 25)
#define DSIM_CONFIG_AUTO_MODE			(1 << 24)
#define DSIM_CONFIG_HSE_DISABLE			(1 << 23)
#define DSIM_CONFIG_HFP_DISABLE			(1 << 22)
#define DSIM_CONFIG_HBP_DISABLE			(1 << 21)
#define DSIM_CONFIG_HSA_DISABLE			(1 << 20)
#define DSIM_CONFIG_PIXEL_FORMAT(_x)		((_x) << 12)
#define DSIM_CONFIG_PIXEL_FORMAT_MASK		(0x7 << 12)
#define DSIM_CONFIG_NUM_OF_DATA_LANE(x)		((x) << 5)
#define DSIM_CONFIG_NUM_OF_DATA_LANE_MASK	(0x3 << 5)
#define DSIM_CONFIG_LANE_ENx(_x)		(((_x) & 0x1f) << 0)

/* Escape mode register */
#define DSIM_ESCMODE				(0x1c)
#define DSIM_ESCMODE_STOP_STATE_CNT(_x)		((_x) << 21)
#define DSIM_ESCMODE_STOP_STATE_CNT_MASK	(0x7ff << 21)
#define DSIM_ESCMODE_FORCE_STOP_STATE		(1 << 20)
#define DSIM_ESCMODE_CMD_LPDT			(1 << 7)
#define DSIM_ESCMODE_TX_LPDT			(1 << 6)
#define DSIM_ESCMODE_TX_ULPS_DATA		(1 << 3)
#define DSIM_ESCMODE_TX_ULPS_DATA_EXIT		(1 << 2)
#define DSIM_ESCMODE_TX_ULPS_CLK		(1 << 1)
#define DSIM_ESCMODE_TX_ULPS_CLK_EXIT		(1 << 0)

/* Display image resolution register */
#define DSIM_MDRESOL				(0x20)
#define DSIM_MDRESOL_STAND_BY			(1 << 31)
#define DSIM_MDRESOL_SHADOW_EN			(1 << 30)
#define DSIM_MDRESOL_VRESOL(x)			(((x) & 0xfff) << 16)
#define DSIM_MDRESOL_VRESOL_MASK		(0xfff << 16)
#define DSIM_MDRESOL_HRESOL(x)			(((x) & 0Xfff) << 0)
#define DSIM_MDRESOL_HRESOL_MASK		(0xfff << 0)
#define DSIM_MDRESOL_LINEVAL_GET(_v)		(((_v) >> 16) & 0xfff)
#define DSIM_MDRESOL_HOZVAL_GET(_v)		(((_v) >> 0) & 0xfff)

/* Main display Vporch register */
#define DSIM_MVPORCH				(0x24)
#define DSIM_MVPORCH_CMD_ALLOW(_x)		((_x) << 28)
#define DSIM_MVPORCH_CMD_ALLOW_MASK		(0xf << 28)
#define DSIM_MVPORCH_STABLE_VFP(_x)		((_x) << 16)
#define DSIM_MVPORCH_STABLE_VFP_MASK		(0x7ff << 16)
#define DSIM_MVPORCH_VBP(_x)			((_x) << 0)
#define DSIM_MVPORCH_VBP_MASK			(0x7ff << 0)

/* Main display Hporch register */
#define DSIM_MHPORCH				(0x28)
#define DSIM_MHPORCH_HFP(_x)			((_x) << 16)
#define DSIM_MHPORCH_HFP_MASK			(0xffff << 16)
#define DSIM_MHPORCH_HBP(_x)			((_x) << 0)
#define DSIM_MHPORCH_HBP_MASK			(0xffff << 0)

/* Main display sync area register */
#define DSIM_MSYNC				(0x2C)
#define DSIM_MSYNC_VSA(_x)			((_x) << 22)
#define DSIM_MSYNC_VSA_MASK			(0x3ff << 22)
#define DSIM_MSYNC_HSA(_x)			((_x) << 0)
#define DSIM_MSYNC_HSA_MASK			(0xffff << 0)

/* Interrupt source register */
#define DSIM_INTSRC				(0x34)
#define DSIM_INTSRC_PLL_STABLE			(1 << 31)
#define DSIM_INTSRC_SFR_PL_FIFO_EMPTY		(1 << 29)
#define DSIM_INTSRC_SFR_PH_FIFO_EMPTY		(1 << 28)
#define DSIM_INTSRC_SFR_PH_FIFO_OVERFLOW	(1 << 26)
#define DSIM_INTSRC_FRAME_DONE			(1 << 24)
#define DSIM_INTSRC_RX_DAT_DONE			(1 << 18)
#define DSIM_INTSRC_ERR_RX_ECC			(1 << 15)

/* Interrupt mask register */
#define DSIM_INTMSK				(0x38)
#define DSIM_INTMSK_PLL_STABLE			(1 << 31)
#define DSIM_INTMSK_SW_RST_RELEASE		(1 << 30)
#define DSIM_INTMSK_SFR_PL_FIFO_EMPTY		(1 << 29)
#define DSIM_INTMSK_SFR_PH_FIFO_EMPTY		(1 << 28)
#define DSIM_INTMSK_SFR_PH_FIFO_OVERFLOW	(1 << 26)
#define DSIM_INTMSK_FRAME_DONE			(1 << 24)
#define DSIM_INTMSK_RX_DATA_DONE		(1 << 18)
#define DSIM_INTMSK_RX_ECC			(1 << 15)

/* Packet Header FIFO register */
#define DSIM_PKTHDR				(0x3c)
#define DSIM_PKTHDR_ID(_x)			((_x) << 0)
#define DSIM_PKTHDR_DATA0(_x)			((_x) << 8)
#define DSIM_PKTHDR_DATA1(_x)			((_x) << 16)

/* Payload FIFO register */
#define DSIM_PAYLOAD				(0x40)

/* Read FIFO register */
#define DSIM_RXFIFO				(0x44)

/* FIFO status and control register */
#define DSIM_FIFOCTRL				(0x4C)
#define DSIM_FIFOCTRL_FULL_PH_SFR		(1 << 23)
#define DSIM_FIFOCTRL_FULL_PL_SFR		(1 << 21)
#define DSIM_FIFOCTRL_INIT_SFR			(1 << 3)

/* Multi packet configuration */
#define DSIM_MULTI_PKT				(0x78)
#define DSIM_MULTI_PKT_EN			(1 << 30)
#define DSIM_MULTI_PKT_GO_EN			(1 << 29)
#define DSIM_MULTI_PKT_GO_RDY			(1 << 28)
#define DSIM_MULTI_PKT_SEND_CNT(_x)		((_x) << 16)
#define DSIM_MULTI_PKT_SEND_CNT_MASK		(0xfff << 16)
#define DSIM_MULTI_PKT_CNT(_x)			((_x) << 0)
#define DSIM_MULTI_PKT_CNT_MASK			(0xffff << 0)

/* PLL control register */
#define DSIM_PLLCTRL				(0x94)
#define DSIM_PLLCTRL_PLL_EN			(1 << 23)
#define DSIM_PLLCTRL_DPDN_SWAP_DATA		(1 << 24)
#define DSIM_PLLCTRL_DPDN_SWAP_CLK		(1 << 25)
#define DSIM_PLLCTRL_PMS_MASK			(0x7ffff << 1)

/* PLL control register1 */
#define DSIM_PLLCTRL1					(0x98)
#define DSIM_PLLCTRL1_AFC_INITIAL_DEALY_PIN		(0x1 << 6)	/* default value is 1 */
#define DSIM_PLLCTRL1_LOCK_DETECTOR_INPUT_MARGIN(_x)	((_x) << 7)	/* default value is 11 */
#define DSIM_PLLCTRL1_LOCK_DETECTOR_INPUT_MARGIN_MASK	(0x3 << 7)
#define DSIM_PLLCTRL1_LOCK_DETECTOR_OUTPUT_MARGIN(_x)	((_x) << 9)	/* default value is 11 */
#define DSIM_PLLCTRL1_LOCK_DETECTOR_OUTPUT_MARGIN_MASK	(0x3 << 9)
#define DSIM_PLLCTRL1_LOCK_DETECTOR_DETECT_RESOL(_x)	((_x) << 11)	/* default value is 11 */
#define DSIM_PLLCTRL1_LOCK_DETECTOR_DETECT_RESOL_MASK	(0x3 << 11)
#define DSIM_PLLCTRL1_CHARGE_PUMP_CURRENT(_x)		((_x) << 20)	/* default value is 1 */
#define DSIM_PLLCTRL1_CHARGE_PUMP_CURRENT_MASK		(0x3 << 20)
#define DSIM_PLLCTRL1_AFC_OPERATION_MODE_SELECT		(0x1 << 24)	/* default value is 1 */

/* PLL timer register */
#define DSIM_PLLTMR				(0xa0)

/* D-PHY Master & Slave Analog block characteristics control register */
#define DSIM_PHYCTRL				(0xa4)
#define DSIM_PHYCTRL_B_DPHYCTL0(_x)		((_x) << 0)
#define DSIM_PHYCTRL_B_DPHYCTL0_MASK		(0xffff << 0)

/* D-PHY Master global operating timing register */
#define DSIM_PHYTIMING				(0xb4)
#define DSIM_PHYTIMING_M_TLPXCTL(_x)		((_x) << 8)
#define DSIM_PHYTIMING_M_TLPXCTL_MASK		(0xff << 8)
#define DSIM_PHYTIMING_M_THSEXITCTL(_x)		((_x) << 0)
#define DSIM_PHYTIMING_M_THSEXITCTL_MASK	(0xff << 0)

#define DSIM_PHYTIMING1				(0xb8)
#define DSIM_PHYTIMING1_M_TCLKPRPRCTL(_x)	((_x) << 24)
#define DSIM_PHYTIMING1_M_TCLKPRPRCTL_MASK	(0xff << 24)
#define DSIM_PHYTIMING1_M_TCLKZEROCTL(_x)	((_x) << 16)
#define DSIM_PHYTIMING1_M_TCLKZEROCTL_MASK	(0xff << 16)
#define DSIM_PHYTIMING1_M_TCLKPOSTCTL(_x)	((_x) << 8)
#define DSIM_PHYTIMING1_M_TCLKPOSTCTL_MASK	(0xff << 8)
#define DSIM_PHYTIMING1_M_TCLKTRAILCTL(_x)	((_x) << 0)
#define DSIM_PHYTIMING1_M_TCLKTRAILCTL_MASK	(0xff << 0)

#define DSIM_PHYTIMING2				(0xbc)
#define DSIM_PHYTIMING2_M_THSPRPRCTL(_x)	((_x) << 16)
#define DSIM_PHYTIMING2_M_THSPRPRCTL_MASK	(0xff << 16)
#define DSIM_PHYTIMING2_M_THSZEROCTL(_x)	((_x) << 8)
#define DSIM_PHYTIMING2_M_THSZEROCTL_MASK	(0xff << 8)
#define DSIM_PHYTIMING2_M_THSTRAILCTL(_x)	((_x) << 0)
#define DSIM_PHYTIMING2_M_THSTRAILCTL_MASK	(0xff << 0)

#endif /* _REGS_DSIM_H */
