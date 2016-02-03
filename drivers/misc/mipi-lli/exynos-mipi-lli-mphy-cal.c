/*
 * Exynos MIPI-LLI-MPHY-CAL driver
 *
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include "exynos-mipi-lli-mphy-cal.h"

static u32 exynos_mphy_cal_init(void *mphy_addr,
				u32 info)
{
	u32 power_mode;
	u32 gear = info & 0x7;
	u32 series;

	/* PWM: 0x10, HS: 0x20 */
	if (info & 0x10)
		power_mode = LLI_MPHY_PWM_MODE;
	else
		power_mode = LLI_MPHY_HS_MODE;

	/* HS_RATE_A: 0x100, HS_RATE_B: 0x200 */
	if (info & 0x100)
		series = LLI_MPHY_RATEA;
	else
		series = LLI_MPHY_RATEB;

	/* HS Prepare Length - should be modified in comparison with
	   CP's Rx HS Prepare Length Cap. PHY_TX_HS_PREPARE_LENGTH(0)
	   In case of Istor-SS300, set to 0x02 or greater than 0x02,
	   because Rx_hs_prepare_legnth_cap in SS300 is 0x01
	 */
	writel(0x45, mphy_addr + 0x28*4);

	writel(0xF, mphy_addr + 0x29*4);
	writel(0x0, mphy_addr + 0x2C*4);
	/* set Rx PWM cap same to Tx */

	writel(power_mode, mphy_addr + 0x21*4);
	writel(power_mode, mphy_addr + 0xA1*4);

	if (power_mode == LLI_MPHY_PWM_MODE) {
		/* PHY_TX_PWMGEAR(0)) */
		writel(gear, mphy_addr + 0x24*4);
		/* PHY_RX_PWMGEAR(0)) */
		writel(gear, mphy_addr + 0xA4*4);
	} else { /* HS mode */
		/*PHY_TX_HSGEAR(0)) */
		writel(gear, mphy_addr + 0x23*4);
		/* PHY_RX_HSGEAR(0)) */
		writel(gear, mphy_addr + 0xA3*4);
		/* PHY_TX_HSRATE_SERIES(0)) */
		writel(series, mphy_addr + 0x22*4);
		/* PHY_RX_HSRATE_SERIES(0)) */
		writel(series, mphy_addr + 0xA2*4);
	}

	return 0;
}

static u32 exynos_mphy_cal_cmn_init(void *mphy_addr,
				    struct refclk_info *refclk_path)
{
	/* INT_PWM_CMN_CTRL */
	writel(0x00, mphy_addr + 0x02*4);
	/* Deep Stall Disable */
	writel(0x00, mphy_addr + 0x60*4);

	if (refclk_path->freq == LLI_MPHY_REFERENCE_CLK_IN_26MHZ)
		writel(0x01, mphy_addr + 0x01*4);

	return 0;
}

static u32 exynos_mphy_cal_ovtm_init(void *mphy_addr)
{
	/* Sync Pattern Masking Period */
	writel(0x20, mphy_addr + 0x21*4);
	/* Increase PWM Gear Capability from PWM G2 to PWM G4 */
	writel(0x05, mphy_addr + 0x84*4);
	/* Enable Fas H8 Enterance */
	writel(0x82, mphy_addr + 0x8E*4);
	/* Masking DIF-P when enter H8 */
	writel(0x21, mphy_addr + 0x37*4);

	return 0;
}

static u32 exynos_mphy_cal_pma_PWM_G1_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	if (refclk_path->freq == LLI_MPHY_REFERENCE_CLK_IN_26MHZ)
		writel(0x08, pma_addr + 0x12*4);
	else
		writel(0x00, pma_addr + 0x12*4);

	if (refclk_path->freq == LLI_MPHY_REFERENCE_CLK_IN_26MHZ)
		writel(0x05, pma_addr + 0x15*4);
	else /* rer_clk_in == 24MHz */
		writel(0x01, pma_addr + 0x15*4);
	/* get Ref_clk from CP */
	if (refclk_path->is_shared)
		writel(0xF0, pma_addr + 0x17*4);
	else /* get Ref_Clk from internal OSC or PLL */
		writel(0x80, pma_addr + 0x17*4);

	writel(0x10, pma_addr + 0x19*4);
	writel(0x18, pma_addr + 0x4E*4);
	writel(0x14, pma_addr + 0x57*4);

	return 0;
}

static u32 exynos_mphy_cal_pma_PWM_G2_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	exynos_mphy_cal_pma_PWM_G1_init(pma_addr, refclk_path);
	writel(0x83, pma_addr + 0x4D*4);
	return 0;
}

static u32 exynos_mphy_cal_pma_PWM_G3_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	exynos_mphy_cal_pma_PWM_G1_init(pma_addr, refclk_path);
	writel(0x87, pma_addr + 0x4D*4);
	return 0;
}

static u32 exynos_mphy_cal_pma_PWM_G4_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	exynos_mphy_cal_pma_PWM_G1_init(pma_addr, refclk_path);
	writel(0x8F, pma_addr + 0x4D*4);
	return 0;
}

static u32 exynos_mphy_cal_pma_PWM_G5_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	exynos_mphy_cal_pma_PWM_G1_init(pma_addr, refclk_path);
	writel(0x9F, pma_addr + 0x4D*4);
	return 0;
}

static u32 exynos_mphy_cal_pma_HS_G1A_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	writel(0x00, pma_addr + 0x12*4);
	/* get Ref_clk from CP */
	if (refclk_path->is_shared)
		writel(0xF0, pma_addr + 0x17*4);
	else /* get Ref_Clk from internal OSC or PLL */
		writel(0x80, pma_addr + 0x17*4);
	/* DIPD tuning 2014-09-18 for CTS test */
	writel(0xE4, pma_addr + 0x31*4);
	writel(0xE4, pma_addr + 0x32*4);
	/* ------------- DIPD tuning 2014-02-21 ----start ---------------- */
	/* modify VCO initial control voltage */
	writel(0xFA, pma_addr + 0x0F*4);
	/* modify Charge Pump Current */
	writel(0x82, pma_addr + 0x10*4);
	/* modity Loop Filter Register */
	writel(0x1E, pma_addr + 0x11*4);

	/* Equalizer Enable */
	writel(0x35, pma_addr + 0x34*4);
	/* modify Equalizer Control, CDR Data Mode Timing */
	writel(0x5B, pma_addr + 0x35*4);
	/* Increase Integrator PWM current */
	writel(0x02, pma_addr + 0x36*4);
	/* Enable only 1 Equalizer stage. Squelch Voltage Control */
	writel(0x43, pma_addr + 0x37*4);
	/* AFC Restart from coarse tuning */
	writel(0x83, pma_addr + 0x3B*4);

	/* modify CDR Bandwidth */
	writel(0x88, pma_addr + 0x42*4);
	/* modify CDR Charge pump current */
	writel(0xA6, pma_addr + 0x43*4);
	/* modify CDR Frequency range */
	writel(0x74, pma_addr + 0x48*4);
	/* Enable DIF-Z always */
	writel(0x5B, pma_addr + 0x4C*4);
	/* Enable DIF-Z always */
	writel(0x81, pma_addr + 0x4D*4);
	/* ------------- DIPD tuning 2014-02-21 ---- end  ---------------- */

	return 0;
}

static u32 exynos_mphy_cal_pma_HS_G1B_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	writel(0x00, pma_addr + 0x12*4);
	/* get Ref_clk from CP */
	if (refclk_path->is_shared)
		writel(0xF0, pma_addr + 0x17*4);
	else /* get Ref_Clk from internal OSC or PLL */
		writel(0x80, pma_addr + 0x17*4);

	/* -------------- DIPD tuning 2014-02-21 ----start -------------*/
	/* modify VCO initial control voltage */
	writel(0xFA, pma_addr + 0x0F*4);
	/* modify Charge Pump Current */
	writel(0x82, pma_addr + 0x10*4);
	/* modity Loop Filter Register */
	writel(0x1E, pma_addr + 0x11*4);

	/* Equalizer Enable */
	writel(0x35, pma_addr + 0x34*4);
	/* modify Equalizer Control, CDR Data Mode Timing */
	writel(0x5B, pma_addr + 0x35*4);
	/* Increase Integrator PWM current */
	writel(0x02, pma_addr + 0x36*4);
	/* Enable only 1 Equalizer stage. Squelch Voltage Control */
	writel(0x43, pma_addr + 0x37*4);
	/* AFC Restart from coarse tuning */
	writel(0x83, pma_addr + 0x3B*4);

	/* modify CDR Bandwidth */
	writel(0x88, pma_addr + 0x42*4);
	/* modify CDR Charge pump current */
	writel(0xA6, pma_addr + 0x43*4);
	/* modify CDR Frequency range */
	writel(0x74, pma_addr + 0x48*4);
	/* Enable DIF-Z always */
	writel(0x5B, pma_addr + 0x4C*4);
	/* Enable DIF-Z always */
	writel(0x81, pma_addr + 0x4D*4);
	/* --------------- DIPD tuning 2014-02-21 ---- end  --------------*/

	return 0;
}

static u32 exynos_mphy_cal_pma_HS_G2A_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	writel(0x00, pma_addr + 0x12*4);
	/* get Ref_clk from CP */
	if (refclk_path->is_shared)
		writel(0xF0, pma_addr + 0x17*4);
	else /* get Ref_Clk from internal OSC or PLL */
		writel(0x80, pma_addr + 0x17*4);

	/* DIPD tuning 2014-09-18 for CTS test */
	writel(0xF4, pma_addr + 0x31*4);
	writel(0xF4, pma_addr + 0x32*4);
	/* ------------- DIPD tuning 2014-02-21 ----start ---------------- */
	/* modify VCO initial control voltage */
	writel(0xFA, pma_addr + 0x0F*4);
	/* modify Charge Pump Current */
	writel(0x82, pma_addr + 0x10*4);
	/* modity Loop Filter Register */
	writel(0x1E, pma_addr + 0x11*4);

	/* Equalizer Enable */
	writel(0x36, pma_addr + 0x34*4);
	/* modify Equalizer Control, CDR Data Mode Timing */
	writel(0x5c, pma_addr + 0x35*4);
	/* Increase Integrator PWM current */
	writel(0x02, pma_addr + 0x36*4);
	/* Enable only 1 Equalizer stage. Squelch Voltage Control */
	writel(0x43, pma_addr + 0x37*4);
	/* RX irext bias current control, RX irmres bias current control */
	writel(0x3F, pma_addr + 0x38*4);
	/* AFC Restart from coarse tuning */
	writel(0x83, pma_addr + 0x3B*4);

	/* modify CDR Bandwidth */
	writel(0x88, pma_addr + 0x42*4);
	/* modify CDR Charge pump current */
	writel(0xA6, pma_addr + 0x43*4);
	/* modify CDR Frequency range */
	writel(0x74, pma_addr + 0x48*4);
	/* Enable DIF-Z always */
	writel(0x5B, pma_addr + 0x4C*4);
	/* Enable DIF-Z always */
	writel(0x81, pma_addr + 0x4D*4);
	/* ------------- DIPD tuning 2014-02-21 ---- end  ---------------- */

	return 0;
}

static u32 exynos_mphy_cal_pma_HS_G2B_init(void *pma_addr,
					   struct refclk_info *refclk_path)
{
	writel(0x00, pma_addr + 0x12*4);
	/* get Ref_clk from CP */
	if (refclk_path->is_shared)
		writel(0xF0, pma_addr + 0x17*4);
	else /* get Ref_Clk from internal OSC or PLL */
		writel(0x80, pma_addr + 0x17*4);

	/* DIPD tuning 2014-09-18 for CTS test */
	writel(0xE4, pma_addr + 0x31*4);
	writel(0xE4, pma_addr + 0x32*4);
	/* ------------- DIPD tuning 2014-02-21 ----start ---------------- */
	/* modify VCO initial control voltage */
	writel(0xFA, pma_addr + 0x0F*4);
	/* modify Charge Pump Current */
	writel(0x82, pma_addr + 0x10*4);
	/* modity Loop Filter Register */
	writel(0x1E, pma_addr + 0x11*4);

	/* Equalizer Enable */
	writel(0x36, pma_addr + 0x34*4);
	/* modify Equalizer Control, CDR Data Mode Timing */
	writel(0x5c, pma_addr + 0x35*4);
	/* Increase Integrator PWM current */
	writel(0x02, pma_addr + 0x36*4);
	/* Enable only 1 Equalizer stage. Squelch Voltage Control */
	writel(0x43, pma_addr + 0x37*4);
	/* RX irext bias current control, RX irmres bias current control */
	writel(0x3F, pma_addr + 0x38*4);
	/* AFC Restart from coarse tuning */
	writel(0x83, pma_addr + 0x3B*4);

	/* modify CDR Bandwidth */
	writel(0x88, pma_addr + 0x42*4);
	/* modify CDR Charge pump current */
	writel(0xA6, pma_addr + 0x43*4);
	/* modify CDR Frequency range */
	writel(0x74, pma_addr + 0x48*4);
	/* Enable DIF-Z always */
	writel(0x5B, pma_addr + 0x4C*4);
	/* Enable DIF-Z always */
	writel(0x81, pma_addr + 0x4D*4);
	/* ------------- DIPD tuning 2014-02-21 ---- end  ---------------- */

	return 0;
}

static int exynos_mphy_cal_pma_init(void *pma_addr,
				    struct refclk_info *refclk_path,
				    u32 info)
{
	u32 power_mode, gear, series;
	u32 txfsmstate;
	u32 rxfsmstate;
	u32 pll_locked;
	u32 cdr_locked;
	u32 count = 0;

	gear = info & 0x7;
	/* PWM: 0x10, HS: 0x20 */
	if (info & 0x10)
		power_mode = LLI_MPHY_PWM_MODE;
	else
		power_mode = LLI_MPHY_HS_MODE;

	/* HS_RATE_A: 0x100, HS_RATE_B: 0x200 */
	if (info & 0x100)
		series = LLI_MPHY_RATEA;
	else
		series = LLI_MPHY_RATEB;

	if (power_mode == LLI_MPHY_PWM_MODE) {
		switch (gear) {
		case LLI_MPHY_G1:
		default:
			exynos_mphy_cal_pma_PWM_G1_init(pma_addr,
							refclk_path);
			break;
		case LLI_MPHY_G2:
			exynos_mphy_cal_pma_PWM_G2_init(pma_addr,
							refclk_path);
			break;
		case LLI_MPHY_G3:
			exynos_mphy_cal_pma_PWM_G3_init(pma_addr,
							refclk_path);
			break;
		case LLI_MPHY_G4:
			exynos_mphy_cal_pma_PWM_G4_init(pma_addr,
							refclk_path);
			break;
		case LLI_MPHY_G5:
			exynos_mphy_cal_pma_PWM_G5_init(pma_addr,
							refclk_path);
			break;
		}
	} else {
		if (gear == LLI_MPHY_G1) {
			if (series == LLI_MPHY_RATEA)
				exynos_mphy_cal_pma_HS_G1A_init(pma_addr,
								refclk_path);
			else if (series == LLI_MPHY_RATEB)
				exynos_mphy_cal_pma_HS_G1B_init(pma_addr,
								refclk_path);
		} else if (gear == LLI_MPHY_G2) {
			if (series == LLI_MPHY_RATEA)
				exynos_mphy_cal_pma_HS_G2A_init(pma_addr,
								refclk_path);
			else if (series == LLI_MPHY_RATEB)
				exynos_mphy_cal_pma_HS_G2B_init(pma_addr,
								refclk_path);
		}
	}

	txfsmstate = exynos_mphy_get_tx_fsmstate();
	rxfsmstate = exynos_mphy_get_rx_fsmstate();

	/* in case of H8 Exit */
	if ((txfsmstate != 1) && (rxfsmstate != 1)) {
		/* check PLL locked */
		for (count = 0; count < 0xfffff; count++) {
			pll_locked = readl(pma_addr + 0x1E*4);
			/* MPHY_PMA_O_PLL_LOCK_DONE */
			if (pll_locked & (1<<5)) {
				count = 0;
				break;
			}
		}

		/* pll is not locked */
		if (count > 0)
			return -16;

		/* check CDR locked */
		if (power_mode == LLI_MPHY_HS_MODE) {
			for (count = 0; count < 0xfffff; count++) {
				cdr_locked = readl(pma_addr + 0x5E*4);
				/* MPHY_PMA_CDR_LOCK */
				if (cdr_locked & (1<<4)) {
					count = 0;
					break;
				}
			}

			/* cdr is not locked */
			if (count > 0)
				return -16;

		}
	}

	return 0;
}
