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

/* use this definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "dsim_fw.h"
#else
#include "dsim.h"
#endif

/* These definitions are need to guide from AP team */
#define DSIM_STOP_STATE_CNT		0x7ff
#define DSIM_BTA_TIMEOUT		0xff
#define DSIM_LP_RX_TIMEOUT		0xffff
#define DSIM_MULTI_PACKET_CNT		0xffff
#define DSIM_PLL_STABLE_TIME		22200

/* If below values depend on panel. These values wil be move to panel file.
 * And these values are valid in case of video mode only. */
/* Why VFP should so small or first few rows will not display for video mode lcd? */
#define DSIM_CMD_ALLOW_VALUE	1
#define DSIM_STABLE_VFP_VALUE	0

/* DPHY timing table */
const u32 dphy_timing[][10] = {
	/* bps, clk_prepare, clk_zero, clk_post, clk_trail, hs_prepare, hs_zero, hs_trail, lpx, hs_exit */
	{1500, 13, 65, 17, 13, 14, 24, 16, 11, 18},
	{1490, 13, 65, 17, 13, 14, 24, 16, 11, 18},
	{1480, 13, 64, 17, 13, 14, 24, 16, 11, 18},
	{1470, 13, 64, 17, 13, 14, 24, 16, 11, 18},
	{1460, 13, 63, 17, 13, 13, 24, 16, 10, 18},
	{1450, 13, 63, 17, 13, 13, 23, 16, 10, 18},
	{1440, 13, 63, 17, 13, 13, 23, 16, 10, 18},
	{1430, 12, 62, 17, 13, 13, 23, 16, 10, 17},
	{1420, 12, 62, 17, 13, 13, 23, 16, 10, 17},
	{1410, 12, 61, 16, 13, 13, 23, 16, 10, 17},
	{1400, 12, 61, 16, 13, 13, 23, 16, 10, 17},
	{1390, 12, 60, 16, 12, 13, 22, 15, 10, 17},
	{1380, 12, 60, 16, 12, 13, 22, 15, 10, 17},
	{1370, 12, 59, 16, 12, 13, 22, 15, 10, 17},
	{1360, 12, 59, 16, 12, 13, 22, 15, 10, 17},
	{1350, 12, 59, 16, 12, 12, 22, 15, 10, 16},
	{1340, 12, 58, 16, 12, 12, 21, 15, 10, 16},
	{1330, 11, 58, 16, 12, 12, 21, 15, 9, 16},
	{1320, 11, 57, 16, 12, 12, 21, 15, 9, 16},
	{1310, 11, 57, 16, 12, 12, 21, 15, 9, 16},
	{1300, 11, 56, 16, 12, 12, 21, 15, 9, 16},
	{1290, 11, 56, 16, 12, 12, 21, 15, 9, 16},
	{1280, 11, 56, 15, 11, 12, 20, 14, 9, 16},
	{1270, 11, 55, 15, 11, 12, 20, 14, 9, 15},
	{1260, 11, 55, 15, 11, 12, 20, 14, 9, 15},
	{1250, 11, 54, 15, 11, 11, 20, 14, 9, 15},
	{1240, 11, 54, 15, 11, 11, 20, 14, 9, 15},
	{1230, 11, 53, 15, 11, 11, 19, 14, 9, 15},
	{1220, 10, 53, 15, 11, 11, 19, 14, 9, 15},
	{1210, 10, 52, 15, 11, 11, 19, 14, 9, 15},
	{1200, 10, 52, 15, 11, 11, 19, 14, 9, 15},
	{1190, 10, 52, 15, 11, 11, 19, 14, 8, 14},
	{1180, 10, 51, 15, 11, 11, 19, 13, 8, 14},
	{1170, 10, 51, 15, 10, 11, 18, 13, 8, 14},
	{1160, 10, 50, 15, 10, 11, 18, 13, 8, 14},
	{1150, 10, 50, 15, 10, 11, 18, 13, 8, 14},
	{1140, 10, 49, 14, 10, 10, 18, 13, 8, 14},
	{1130, 10, 49, 14, 10, 10, 18, 13, 8, 14},
	{1120, 10, 49, 14, 10, 10, 17, 13, 8, 14},
	{1110, 9, 48, 14, 10, 10, 17, 13, 8, 13},
	{1100, 9, 48, 14, 10, 10, 17, 13, 8, 13},
	{1090, 9, 47, 14, 10, 10, 17, 13, 8, 13},
	{1080, 9, 47, 14, 10, 10, 17, 13, 8, 13},
	{1070, 9, 46, 14, 10, 10, 17, 12, 8, 13},
	{1060, 9, 46, 14, 10, 10, 16, 12, 7, 13},
	{1050, 9, 45, 14, 9, 10, 16, 12, 7, 13},
	{1040, 9, 45, 14, 9, 10, 16, 12, 7, 13},
	{1030, 9, 45, 14, 9, 9, 16, 12, 7, 12},
	{1020, 9, 44, 14, 9, 9, 16, 12, 7, 12},
	{1010, 8, 44, 13, 9, 9, 15, 12, 7, 12},
	{1000, 8, 43, 13, 9, 9, 15, 12, 7, 12},
	{990, 8, 43, 13, 9, 9, 15, 12, 7, 12},
	{980, 8, 42, 13, 9, 9, 15, 12, 7, 12},
	{970, 8, 42, 13, 9, 9, 15, 12, 7, 12},
	{960, 8, 42, 13, 9, 9, 15, 11, 7, 12},
	{950, 8, 41, 13, 9, 9, 14, 11, 7, 11},
	{940, 8, 41, 13, 8, 9, 14, 11, 7, 11},
	{930, 8, 40, 13, 8, 8, 14, 11, 6, 11},
	{920, 8, 40, 13, 8, 8, 14, 11, 6, 11},
	{910, 8, 39, 13, 8, 8, 14, 11, 6, 11},
	{900, 7, 39, 13, 8, 8, 13, 11, 6, 11},
	{890, 7, 38, 13, 8, 8, 13, 11, 6, 11},
	{880, 7, 38, 12, 8, 8, 13, 11, 6, 11},
	{870, 7, 38, 12, 8, 8, 13, 11, 6, 10},
	{860, 7, 37, 12, 8, 8, 13, 11, 6, 10},
	{850, 7, 37, 12, 8, 8, 13, 10, 6, 10},
	{840, 7, 36, 12, 8, 8, 12, 10, 6, 10},
	{830, 7, 36, 12, 8, 8, 12, 10, 6, 10},
	{820, 7, 35, 12, 7, 7, 12, 10, 6, 10},
	{810, 7, 35, 12, 7, 7, 12, 10, 6, 10},
	{800, 7, 35, 12, 7, 7, 12, 10, 6, 10},
	{790, 6, 34, 12, 7, 7, 11, 10, 5, 9},
	{780, 6, 34, 12, 7, 7, 11, 10, 5, 9},
	{770, 6, 33, 12, 7, 7, 11, 10, 5, 9},
	{760, 6, 33, 12, 7, 7, 11, 10, 5, 9},
	{750, 6, 32, 12, 7, 7, 11, 9, 5, 9},
	{740, 6, 32, 11, 7, 7, 11, 9, 5, 9},
	{730, 6, 31, 11, 7, 7, 10, 9, 5, 9},
	{720, 6, 31, 11, 7, 6, 10, 9, 5, 9},
	{710, 6, 31, 11, 6, 6, 10, 9, 5, 8},
	{700, 6, 30, 11, 6, 6, 10, 9, 5, 8},
	{690, 5, 30, 11, 6, 6, 10, 9, 5, 8},
	{680, 5, 29, 11, 6, 6, 9, 9, 5, 8},
	{670, 5, 29, 11, 6, 6, 9, 9, 5, 8},
	{660, 5, 28, 11, 6, 6, 9, 9, 4, 8},
	{650, 5, 28, 11, 6, 6, 9, 9, 4, 8},
	{640, 5, 28, 11, 6, 6, 9, 8, 4, 8},
	{630, 5, 27, 11, 6, 6, 9, 8, 4, 7},
	{620, 5, 27, 11, 6, 6, 8, 8, 4, 7},
	{610, 5, 26, 10, 6, 5, 8, 8, 4, 7},
	{600, 5, 26, 10, 6, 5, 8, 8, 4, 7},
	{590, 5, 25, 10, 5, 5, 8, 8, 4, 7},
	{580, 4, 25, 10, 5, 5, 8, 8, 4, 7},
	{570, 4, 24, 10, 5, 5, 7, 8, 4, 7},
	{560, 4, 24, 10, 5, 5, 7, 8, 4, 7},
	{550, 4, 24, 10, 5, 5, 7, 8, 4, 6},
	{540, 4, 23, 10, 5, 5, 7, 8, 4, 6},
	{530, 4, 23, 10, 5, 5, 7, 7, 3, 6},
	{520, 4, 22, 10, 5, 5, 7, 7, 3, 6},
	{510, 4, 22, 10, 5, 5, 6, 7, 3, 6},
	{500, 4, 21, 10, 5, 4, 6, 7, 3, 6},
	{490, 4, 21, 10, 5, 4, 6, 7, 3, 6},
	{480, 4, 21, 9, 4, 4, 6, 7, 3, 6},
};

const u32 b_dphyctl[14] = {
	0x0af, 0x0c8, 0x0e1, 0x0fa,		/* esc 7 ~ 10 */
	0x113, 0x12c, 0x145, 0x15e, 0x177,	/* esc 11 ~ 15 */
	0x190, 0x1a9, 0x1c2, 0x1db, 0x1f4};	/* esc 16 ~ 20 */


/******************* CAL raw functions implementation *************************/
void dsim_reg_sw_reset(u32 id)
{
	dsim_write_mask(id, DSIM_SWRST, ~0, DSIM_SWRST_RESET);
}

/* add */
void dsim_reg_funtion_reset(u32 id)
{
	dsim_write_mask(id, DSIM_SWRST, ~0, DSIM_SWRST_FUNCRST);
}

void dsim_reg_dp_dn_swap(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;
	u32 mask = DSIM_PLLCTRL_DPDN_SWAP_DATA | DSIM_PLLCTRL_DPDN_SWAP_CLK;

	dsim_write_mask(id, DSIM_PLLCTRL, val, mask);
}

static void dsim_reg_enable_lane(u32 id, u32 lane, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_LANE_ENx(lane));
}

void dsim_reg_set_byte_clk_src_is_pll(u32 id)
{
	u32 val = 0;

	dsim_write_mask(id, DSIM_CLKCTRL, val, DSIM_CLKCTRL_BYTE_CLK_SRC_MASK);
}

void dsim_reg_set_pll_freq(u32 id, u32 p, u32 m, u32 s)
{
	u32 val = (p & 0x3f) << 14 | (m & 0x1ff) << 4 | (s & 0x7) << 1;

	dsim_write_mask(id, DSIM_PLLCTRL, val, DSIM_PLLCTRL_PMS_MASK);
}

void dsim_reg_pll_stable_time(u32 id)
{
	dsim_write(id, DSIM_PLLTMR, DSIM_PLL_STABLE_TIME);
}

void dsim_reg_set_dphy_timing_values(u32 id, struct dphy_timing_value *t)
{
	u32 val;

	val = DSIM_PHYTIMING_M_TLPXCTL(t->lpx) |
		DSIM_PHYTIMING_M_THSEXITCTL(t->hs_exit);
	dsim_write(id, DSIM_PHYTIMING, val);

	val = DSIM_PHYTIMING1_M_TCLKPRPRCTL(t->clk_prepare) |
		DSIM_PHYTIMING1_M_TCLKZEROCTL(t->clk_zero) |
		DSIM_PHYTIMING1_M_TCLKPOSTCTL(t->clk_post) |
		DSIM_PHYTIMING1_M_TCLKTRAILCTL(t->clk_trail);
	dsim_write(id, DSIM_PHYTIMING1, val);

	val = DSIM_PHYTIMING2_M_THSPRPRCTL(t->hs_prepare) |
		DSIM_PHYTIMING2_M_THSZEROCTL(t->hs_zero) |
		DSIM_PHYTIMING2_M_THSTRAILCTL(t->hs_trail);
	dsim_write(id, DSIM_PHYTIMING2, val);

	val = DSIM_PHYCTRL_B_DPHYCTL0(t->b_dphyctl);
	dsim_write_mask(id, DSIM_PHYCTRL, val, DSIM_PHYCTRL_B_DPHYCTL0_MASK);
}

void dsim_reg_clear_int(u32 id, u32 int_src)
{
	dsim_write(id, DSIM_INTSRC, int_src);
}

void dsim_reg_clear_int_all(u32 id)
{
	dsim_write(id, DSIM_INTSRC, 0xffffffff);
}

void dsim_reg_set_pll(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_PLLCTRL, val, DSIM_PLLCTRL_PLL_EN);
}

u32 dsim_reg_is_pll_stable(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_STATUS);
	if (val & DSIM_STATUS_PLL_STABLE)
		return 1;

	return 0;
}

int dsim_reg_enable_pll(u32 id, u32 en)
{
	u32 cnt;

	if (en) {
		cnt = 1000;
		dsim_reg_clear_int(id, DSIM_INTSRC_PLL_STABLE);

		dsim_reg_set_pll(id, 1);
		while (1) {
			cnt--;
			if (dsim_reg_is_pll_stable(id))
				return 0;
			if (cnt == 0)
				return -EBUSY;
		}
	} else {
		dsim_reg_set_pll(id, 0);
	}

	return 0;
}

void dsim_reg_set_byte_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLKCTRL, val, DSIM_CLKCTRL_BYTECLK_EN);
}

void dsim_reg_set_esc_clk_prescaler(u32 id, u32 en, u32 p)
{
	u32 val = en ? DSIM_CLKCTRL_ESCCLK_EN : 0;
	u32 mask = DSIM_CLKCTRL_ESCCLK_EN | DSIM_CLKCTRL_ESC_PRESCALER_MASK;

	val |= DSIM_CLKCTRL_ESC_PRESCALER(p);
	dsim_write_mask(id, DSIM_CLKCTRL, val, mask);
}

void dsim_reg_set_esc_clk_on_lane(u32 id, u32 en, u32 lane)
{
	u32 val = en ? DSIM_CLKCTRL_LANE_ESCCLK_EN(lane) : 0;

	dsim_write_mask(id, DSIM_CLKCTRL, val, DSIM_CLKCTRL_LANE_ESCCLK_EN_MASK);
}

static u32 dsim_reg_is_lane_stop_state(u32 id)
{
	u32 val = dsim_read(id, DSIM_STATUS);
	/**
	 * check clock and data lane states.
	 * if MIPI-DSI controller was enabled at bootloader then
	 * TX_READY_HS_CLK is enabled otherwise STOP_STATE_CLK.
	 * so it should be checked for two case.
	 */
	if ((val & DSIM_STATUS_STOP_STATE_DAT(0xf)) &&
		((val & DSIM_STATUS_STOP_STATE_CLK) || (val & DSIM_STATUS_TX_READY_HS_CLK)))
		return 1;

	return 0;
}

u32 dsim_reg_wait_lane_stop_state(u32 id)
{
	u32 state;
	u32 cnt = 100;

	do {
		state = dsim_reg_is_lane_stop_state(id);
		cnt--;
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("wait timeout DSI Master is not stop state.\n");
		dsim_err("check initialization process.\n");
		return -EBUSY;
	}

	return 0;
}

void dsim_reg_set_stop_state_cnt(u32 id)
{
	u32 val = DSIM_ESCMODE_STOP_STATE_CNT(DSIM_STOP_STATE_CNT);

	dsim_write_mask(id, DSIM_ESCMODE, val, DSIM_ESCMODE_STOP_STATE_CNT_MASK);
}

void dsim_reg_set_bta_timeout(u32 id)
{
	u32 val = DSIM_TIMEOUT_BTA_TOUT(DSIM_BTA_TIMEOUT);

	dsim_write_mask(id, DSIM_TIMEOUT, val, DSIM_TIMEOUT_BTA_TOUT_MASK);
}

void dsim_reg_set_lpdr_timeout(u32 id)
{
	u32 val = DSIM_TIMEOUT_LPDR_TOUT(DSIM_LP_RX_TIMEOUT);

	dsim_write_mask(id, DSIM_TIMEOUT, val, DSIM_TIMEOUT_LPDR_TOUT_MASK);
}

void dsim_reg_set_packet_ctrl(u32 id)
{
	u32 val = DSIM_MULTI_PKT_CNT(DSIM_MULTI_PACKET_CNT);
	dsim_write_mask(id, DSIM_MULTI_PKT, val, DSIM_MULTI_PKT_CNT_MASK);
}

void dsim_reg_set_shadow(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_MDRESOL, val, DSIM_MDRESOL_SHADOW_EN);
}

void dsim_reg_set_porch(u32 id, struct decon_lcd *lcd)
{
	u32 val, mask, width;

	if (lcd->mode == DECON_VIDEO_MODE) {
		val = DSIM_MVPORCH_CMD_ALLOW(DSIM_CMD_ALLOW_VALUE) |
			DSIM_MVPORCH_STABLE_VFP(DSIM_STABLE_VFP_VALUE) |
			DSIM_MVPORCH_VBP(lcd->vbp);
		mask = DSIM_MVPORCH_CMD_ALLOW_MASK | DSIM_MVPORCH_VBP_MASK |
			DSIM_MVPORCH_STABLE_VFP_MASK;
		dsim_write_mask(id, DSIM_MVPORCH, val, mask);

		val = DSIM_MHPORCH_HFP(lcd->hfp) | DSIM_MHPORCH_HBP(lcd->hbp);
		dsim_write(id, DSIM_MHPORCH, val);

		val = DSIM_MSYNC_VSA(lcd->vsa) | DSIM_MSYNC_HSA(lcd->hsa);
		mask = DSIM_MSYNC_VSA_MASK | DSIM_MSYNC_HSA_MASK;
		dsim_write_mask(id, DSIM_MSYNC, val, mask);
	}

	if (lcd->mic_enabled)
		width = (lcd->xres/3) + (lcd->xres % 4);
	else
		width = lcd->xres;

	/* TODO: will be added SHADOW_EN in EVT1 */
	val = DSIM_MDRESOL_VRESOL(lcd->yres) | DSIM_MDRESOL_HRESOL(width);
	mask = DSIM_MDRESOL_VRESOL_MASK | DSIM_MDRESOL_HRESOL_MASK;
	dsim_write_mask(id, DSIM_MDRESOL, val, mask);
}

void dsim_reg_set_config(u32 id, u32 mode, u32 data_lane_cnt)
{
	u32 val = 0;
	u32 mask;

	if (mode == DECON_VIDEO_MODE) {
		val = DSIM_CONFIG_VIDEO_MODE;
	} else if (mode == DECON_MIPI_COMMAND_MODE) {
		val &= ~(DSIM_CONFIG_CLKLANE_STOP_START);   /* In Command mode, Clklane_Stop/Start must be disabled */
	} else {
		dsim_err("This DDI is not MIPI interface.\n");
		return;
	}

	val |= DSIM_CONFIG_BURST_MODE | DSIM_CONFIG_EOT_R03_DISABLE | DSIM_CONFIG_HFP_DISABLE |
		DSIM_CONFIG_NUM_OF_DATA_LANE(data_lane_cnt - 1) |
		DSIM_CONFIG_PIXEL_FORMAT(DSIM_PIXEL_FORMAT_RGB24);

	mask = DSIM_CONFIG_NONCONTINUOUS_CLOCK_LANE | DSIM_CONFIG_CLKLANE_STOP_START | DSIM_CONFIG_EOT_R03_DISABLE |
		DSIM_CONFIG_BURST_MODE | DSIM_CONFIG_VIDEO_MODE | DSIM_CONFIG_HFP_DISABLE |
		DSIM_CONFIG_PIXEL_FORMAT_MASK | DSIM_CONFIG_NUM_OF_DATA_LANE_MASK;

	dsim_write_mask(id, DSIM_CONFIG, val, mask);
}

void dsim_reg_set_noncontinuous_clock_mode(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_NONCONTINUOUS_CLOCK_LANE);
}

void dsim_reg_set_cmd_transfer_mode(u32 id, u32 lp)
{
	u32 val = lp ? ~0 : 0;

	dsim_write_mask(id, DSIM_ESCMODE, val, DSIM_ESCMODE_CMD_LPDT);
}

void dsim_reg_set_data_transfer_mode(u32 id, u32 lp)
{
	u32 val = lp ? ~0 : 0;

	dsim_write_mask(id, DSIM_ESCMODE, val, DSIM_ESCMODE_TX_LPDT);
}

void dsim_reg_set_pllctrl_value(u32 id, u32 en)
{
	u32 val = 0;
	u32 mask;

	if (en) {
		val = DSIM_PLLCTRL1_AFC_INITIAL_DEALY_PIN |
			DSIM_PLLCTRL1_LOCK_DETECTOR_INPUT_MARGIN(0x3) |
			DSIM_PLLCTRL1_LOCK_DETECTOR_OUTPUT_MARGIN(0x3) |
			DSIM_PLLCTRL1_LOCK_DETECTOR_DETECT_RESOL(0x3) |
			DSIM_PLLCTRL1_CHARGE_PUMP_CURRENT(0x1) |
			DSIM_PLLCTRL1_AFC_OPERATION_MODE_SELECT;
	}

	mask = DSIM_PLLCTRL1_AFC_INITIAL_DEALY_PIN |
		DSIM_PLLCTRL1_LOCK_DETECTOR_INPUT_MARGIN_MASK |
		DSIM_PLLCTRL1_LOCK_DETECTOR_OUTPUT_MARGIN_MASK |
		DSIM_PLLCTRL1_LOCK_DETECTOR_DETECT_RESOL_MASK |
		DSIM_PLLCTRL1_CHARGE_PUMP_CURRENT_MASK |
		DSIM_PLLCTRL1_AFC_OPERATION_MODE_SELECT;

	dsim_write_mask(id, DSIM_PLLCTRL1, val, mask);
}

void dsim_reg_enable_hs_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLKCTRL, val, DSIM_CLKCTRL_TX_REQUEST_HSCLK);
}

u32 dsim_reg_is_hs_clk_ready(u32 id)
{
	if (dsim_read(id, DSIM_STATUS) & DSIM_STATUS_TX_READY_HS_CLK)
		return 1;

	return 0;
}

int dsim_reg_wait_hs_clk_ready(u32 id)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_hs_clk_ready(id);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not HS state.\n");
		return -EBUSY;
	}

	return 0;
}

void dsim_reg_set_fifo_ctrl(u32 id, u32 cfg)
{
	dsim_write_mask(id, DSIM_FIFOCTRL, ~cfg, cfg);
	dsim_write_mask(id, DSIM_FIFOCTRL, ~0, cfg);
}

void dsim_reg_force_dphy_stop_state(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_ESCMODE, val, DSIM_ESCMODE_FORCE_STOP_STATE);
}

void dsim_reg_wr_tx_header(u32 id, u32 data_id, unsigned long data0, u32 data1)
{
	u32 val = DSIM_PKTHDR_ID(data_id) | DSIM_PKTHDR_DATA0(data0) |
		DSIM_PKTHDR_DATA1(data1);

	dsim_write(id, DSIM_PKTHDR, val);
}

void dsim_reg_wr_tx_payload(u32 id, u32 payload)
{
	dsim_write(id, DSIM_PAYLOAD, payload);
}

void dsim_reg_enter_ulps(u32 id, u32 enter)
{
	u32 val = enter ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK | DSIM_ESCMODE_TX_ULPS_DATA;

	dsim_write_mask(id, DSIM_ESCMODE, val, mask);
}

void dsim_reg_exit_ulps(u32 id, u32 exit)
{
	u32 val = exit ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK_EXIT | DSIM_ESCMODE_TX_ULPS_DATA_EXIT;

	dsim_write_mask(id, DSIM_ESCMODE, val, mask);
}

static u32 dsim_reg_is_ulps_state(u32 id, u32 lanes)
{
	u32 val = dsim_read(id, DSIM_STATUS);
	u32 data_lane = lanes >> DSIM_LANE_CLOCK;

	if ((DSIM_STATUS_ULPS_DATA_LANE_GET(val) == data_lane)
			&& (val & DSIM_STATUS_ULPS_CLK))
		return 1;

	return 0;
}

int dsim_reg_wait_enter_ulps_state(u32 id, u32 lanes)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_ulps_state(id, lanes);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not ULPS state.\n");
		return -EBUSY;
	}

	return 0;
}

static u32 dsim_reg_is_not_ulps_state(u32 id)
{
	u32 val = dsim_read(id, DSIM_STATUS);

	if (!(DSIM_STATUS_ULPS_DATA_LANE_GET(val))
			&& !(val & DSIM_STATUS_ULPS_CLK))
		return 1;

	return 0;
}

int dsim_reg_wait_exit_ulps_state(u32 id)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_not_ulps_state(id);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not stop state.\n");
		return -EBUSY;
	}

	return 0;
}

void dsim_reg_set_standby(u32 id, struct decon_lcd *lcd, u32 en)
{
	u32 val, mask;
	u32 width;

	if (lcd->mic_enabled)
		width = (lcd->xres/3) + (lcd->xres % 4);
	else
		width = lcd->xres;

	if (en) {
		val = DSIM_MDRESOL_STAND_BY | DSIM_MDRESOL_VRESOL(lcd->yres) |
			DSIM_MDRESOL_HRESOL(width);
		mask = DSIM_MDRESOL_STAND_BY | DSIM_MDRESOL_VRESOL_MASK |
			DSIM_MDRESOL_HRESOL_MASK;
	} else {
		val = DSIM_MDRESOL_VRESOL(lcd->yres) | DSIM_MDRESOL_HRESOL(width);
		mask = DSIM_MDRESOL_STAND_BY | DSIM_MDRESOL_VRESOL_MASK |
			DSIM_MDRESOL_HRESOL_MASK;
	}

	dsim_write_mask(id, DSIM_MDRESOL, val, mask);
}

/*
 * input parameter
 *	- pll_freq : requested pll frequency
 *
 * output parameters
 *	- p, m, s : calculated p, m, s values
 *	- pll_freq : adjusted pll frequency
 */
static int dsim_reg_calculate_pms(struct dsim_pll_param *pll)
{
	u32 p_div, m_div, s_div;
	u32 target_freq, fin_pll, voc_out, fout_cal;
	u32 fin = 24;

	dsim_dbg("requested HS clock is %u\n", pll->pll_freq);
	target_freq = pll->pll_freq;

	for (p_div = 1; p_div <= 63; p_div++) {
		for (m_div = 64; m_div <= 1023; m_div++) {
			for (s_div = 0; s_div <= 6; s_div++) {
				fin_pll = fin / p_div;
				voc_out = (m_div * fin) / p_div;
				fout_cal = (m_div * fin) / (p_div * (1 << s_div));

				if ((fin_pll < 4) || (fin_pll > 12))
					continue;
				if ((voc_out < 1600) || (voc_out > 3200))
					continue;
				if ((fout_cal < 25) || (fout_cal > 3200))
					continue;
				if (fout_cal < target_freq)
					continue;
				if ((target_freq <= fout_cal) && (fout_cal <= 1500))
					goto calculation_success;
			}
		}
	}

	dsim_err("failed to calculate PMS values for DPHY\n");
	return -EINVAL;

calculation_success:
	pll->p = p_div;
	pll->m = m_div;
	pll->s = s_div;
	pll->pll_freq = fout_cal;
	dsim_dbg("calculated HS clock is %u Mhz. p(%u), m(%u), s(%u)\n",
			pll->pll_freq, pll->p, pll->m, pll->s);

	return 0;
}

static int dsim_reg_get_dphy_timing(u32 hs_clk, u32 esc_clk, struct dphy_timing_value *t)
{
	int i = sizeof(dphy_timing) / sizeof(dphy_timing[0]) - 1;

	while (i) {
		if (dphy_timing[i][0] < hs_clk) {
			i--;
			continue;
		} else {
			t->bps = hs_clk;
			t->clk_prepare = dphy_timing[i][1];
			t->clk_zero = dphy_timing[i][2];
			t->clk_post = dphy_timing[i][3];
			t->clk_trail = dphy_timing[i][4];
			t->hs_prepare = dphy_timing[i][5];
			t->hs_zero = dphy_timing[i][6];
			t->hs_trail = dphy_timing[i][7];
			t->lpx = dphy_timing[i][8];
			t->hs_exit = dphy_timing[i][9];
			break;
		}
	}

	if (!i) {
		dsim_err("%u Mhz hs clock can't find proper dphy timing values\n", hs_clk);
		return -EINVAL;
	}

	if ((esc_clk > 20) || (esc_clk < 7)) {
		dsim_err("%u Mhz cann't be used as escape clock\n", esc_clk);
		return -EINVAL;
	}

	t->b_dphyctl = b_dphyctl[esc_clk - 7];

	return 0;
}



/***************** CAL APIs implementation *******************/

/*
 * dsim basic configuration
 *	- sw reset
 *	- set interval between transmitting rx packet and BTA request
 *	- set BTA timeout
 *	- LP Rx mode timeout
 *	- set multi packet count
 *	- set porch values
 *	- set burst mode, data lane count, pixel format and etc
 */
void dsim_reg_init(u32 id, struct decon_lcd *lcd_info, u32 data_lane_cnt)
{
	/* set counter */
	dsim_reg_set_stop_state_cnt(id);
	dsim_reg_set_bta_timeout(id);
	dsim_reg_set_lpdr_timeout(id);
	dsim_reg_set_packet_ctrl(id);

	/* set DSIM configuration */
	dsim_reg_set_shadow(id, 1);
	dsim_reg_set_porch(id, lcd_info);
	dsim_reg_set_config(id, lcd_info->mode, data_lane_cnt);
	dsim_reg_set_pllctrl_value(id, 1);

	/* set non-continous clock mode */
	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE)
		dsim_reg_set_noncontinuous_clock_mode(id, 1);
}

void dsim_reg_init_probe(u32 id, struct decon_lcd *lcd_info, u32 data_lane_cnt)
{
	/* set counter */
	dsim_reg_set_stop_state_cnt(id);
	dsim_reg_set_bta_timeout(id);
	dsim_reg_set_lpdr_timeout(id);
	dsim_reg_set_packet_ctrl(id);

	/* set DSIM configuration */
	dsim_reg_set_shadow(id, 1);
	dsim_reg_set_porch(id, lcd_info);
	dsim_reg_set_config(id, lcd_info->mode, data_lane_cnt);
	dsim_reg_set_pllctrl_value(id, 1);

	/* set non-continous clock mode */
	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE)
		dsim_reg_set_noncontinuous_clock_mode(id, 1);
}

/*
 * configure and set DPHY PLL, byte clock, escape clock and hs clock
 *	- calculate PMS using requested HS clock from driver
 *	- PLL out is source clock of HS clock
 *	- byte clock = HS clock / 8
 *	- calculate divider of escape clock using requested escape clock
 *	  from driver
 *	- DPHY PLL, byte clock, escape clock are enabled.
 *	- HS clock will be enabled another function.
 *
 * Parameters
 *	- hs_clk : in/out parameter.
 *		in :  requested hs clock. out : calculated hs clock
 *	- esc_clk : in/out paramter.
 *		in : requested escape clock. out : calculated escape clock
 *	- byte_clk : out parameter. byte clock = hs clock / 8
 */
int dsim_reg_set_clocks(u32 id, struct dsim_clks *clks, u32 lane, u32 en)
{
	unsigned int esc_div;
	struct dsim_pll_param pll;
	struct dphy_timing_value t;
	int ret;

	if (en) {
		/* byte clock source must be DPHY PLL */
		dsim_reg_set_byte_clk_src_is_pll(id);

		/* requested DPHY PLL frequency(HS clock) */
		pll.pll_freq = clks->hs_clk;
		/* calculate p, m, s for setting DPHY PLL and hs clock */
		ret = dsim_reg_calculate_pms(&pll);
		if (ret)
			return ret;
		/* store calculated hs clock */
		clks->hs_clk = pll.pll_freq;
		/* set p, m, s to DPHY PLL */
		dsim_reg_set_pll_freq(id, pll.p, pll.m, pll.s);

		/* set PLL's lock time */
		dsim_reg_pll_stable_time(id);

		/* get byte clock */
		clks->byte_clk = clks->hs_clk / 8;
		dsim_dbg("byte clock is %u MHz\n", clks->byte_clk);

		/* requeseted escape clock */
		dsim_dbg("requested escape clock %u MHz\n", clks->esc_clk);
		/* escape clock divider */
		esc_div = clks->byte_clk / clks->esc_clk;

		/* adjust escape clock */
		if ((clks->byte_clk / esc_div) > clks->esc_clk)
			esc_div += 1;
		/* adjusted escape clock */
		clks->esc_clk = clks->byte_clk / esc_div;
		dsim_dbg("escape clock divider is 0x%x\n", esc_div);
		dsim_dbg("escape clock is %u MHz\n", clks->esc_clk);

		/* get DPHY timing values using hs clock and escape clock */
		dsim_reg_get_dphy_timing(clks->hs_clk, clks->esc_clk, &t);
		dsim_reg_set_dphy_timing_values(id, &t);

		/* enable PLL */
		dsim_reg_enable_pll(id, 1);

		/* enable byte clock. */
		dsim_reg_set_byte_clock(id, 1);

		/* enable escape clock */
		dsim_reg_set_esc_clk_prescaler(id, 1, esc_div);

		/* escape clock on lane */
		dsim_reg_set_esc_clk_on_lane(id, 1, lane);

	} else {
		dsim_reg_set_esc_clk_on_lane(id, 0, lane);
		dsim_reg_set_esc_clk_prescaler(id, 0, 0);

		dsim_reg_set_byte_clock(id, 0);
		dsim_reg_enable_pll(id, 0);
	}

	return 0;
}

int dsim_reg_prepare_clocks(struct dsim_clks_param *clks_param)
{
	struct dsim_pll_param pll;
	int ret;
	u32 esc_div;

	/* calculate P,M,S for HS clock */
	/* requested DPHY PLL frequency(HS clock) */
	pll.pll_freq = clks_param->clks.hs_clk;
	/* calculate p, m, s for setting DPHY PLL and hs clock */
	ret = dsim_reg_calculate_pms(&pll);
	if (ret)
		return ret;

	clks_param->pll.p = pll.p;
	clks_param->pll.m = pll.m;
	clks_param->pll.s = pll.s;
	clks_param->pll.pll_freq = pll.pll_freq;
	dsim_info("calculated P(%d),M(%d),S(%d) and HS clock(%d)\n",
			pll.p, pll.m, pll.s, pll.pll_freq);

	clks_param->clks.hs_clk = pll.pll_freq;
	clks_param->clks.byte_clk = pll.pll_freq / 8;

	/* calculate escape clock using calculated HS clock */
	/* requeseted escape clock */
	dsim_info("requested escape clock %u MHz\n", clks_param->clks.esc_clk);

	/* escape clock divider */
	esc_div = clks_param->clks.byte_clk / clks_param->clks.esc_clk;

	/* adjust escape clock */
	if ((clks_param->clks.byte_clk / esc_div) > clks_param->clks.esc_clk)
		esc_div += 1;
	/* adjusted escape clock */
	clks_param->clks.esc_clk = clks_param->clks.byte_clk / esc_div;
	clks_param->esc_div = esc_div;
	dsim_info("escape clock divider is 0x%x\n", esc_div);
	dsim_info("escape clock is %u MHz\n", clks_param->clks.esc_clk);

	/* get DPHY timing values using hs clock and escape clock */
	dsim_reg_get_dphy_timing(clks_param->clks.hs_clk,
			clks_param->clks.esc_clk, &clks_param->t);

	return 0;
}

void dsim_reg_enable_clocks(u32 id, struct dsim_clks_param *p, u32 lane)
{
	/* byte clock source must be DPHY PLL */
	dsim_reg_set_byte_clk_src_is_pll(id);

	/* set p, m, s to DPHY PLL */
	dsim_reg_set_pll_freq(id, p->pll.p, p->pll.m, p->pll.s);

	/* set PLL's lock time */
	dsim_reg_pll_stable_time(id);

	dsim_reg_set_dphy_timing_values(id, &p->t);

	/* enable PLL */
	dsim_reg_enable_pll(id, 1);

	/* enable byte clock. */
	dsim_reg_set_byte_clock(id, 1);

	/* enable escape clock */
	dsim_reg_set_esc_clk_prescaler(id, 1, p->esc_div);

	/* escape clock on lane */
	dsim_reg_set_esc_clk_on_lane(id, 1, lane);
}

int dsim_reg_set_lanes(u32 id, u32 lanes, u32 en)
{
	int ret;

	dsim_reg_enable_lane(id, lanes, en);
	udelay(300);    /* CAL add */
	if (en) {
		ret = dsim_reg_wait_lane_stop_state(id);
		if (ret)
			return ret;
	}

	return 0;
}

u32 dsim_reg_is_noncontinuous_clk_enabled(u32 id)
{
	if (dsim_read(id, DSIM_CONFIG) & DSIM_CONFIG_NONCONTINUOUS_CLOCK_LANE)
		return 1;

	return 0;
}

int dsim_reg_set_hs_clock(u32 id, struct decon_lcd *lcd, u32 en)
{
	int ret;
	int is_non = dsim_reg_is_noncontinuous_clk_enabled(id);

	if (en) {
		dsim_reg_set_cmd_transfer_mode(id, 0);
		dsim_reg_set_data_transfer_mode(id, 0);

		dsim_reg_enable_hs_clock(id, 1);

		if (!is_non) {
			ret = dsim_reg_wait_hs_clk_ready(id);
			if (ret)
				return ret;
		}

		dsim_reg_set_standby(id, lcd, 1);

		/* for preventing 2C miss issue */
		dsim_reg_funtion_reset(id);
	} else {
		dsim_reg_set_standby(id, lcd, 0);
		dsim_reg_enable_hs_clock(id, 0);
	}

	return 0;
}

void dsim_reg_set_int(u32 id, u32 en)
{
	u32 val = en ? 0 : ~0;
	u32 mask;

	mask = DSIM_INTMSK_SFR_PL_FIFO_EMPTY | DSIM_INTMSK_SFR_PH_FIFO_EMPTY |
		DSIM_INTMSK_FRAME_DONE | DSIM_INTMSK_RX_DATA_DONE |
		DSIM_INTMSK_RX_ECC;

	dsim_write_mask(id, DSIM_INTMSK, val, mask);
}

/*
 * enter or exit ulps mode
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : exit ULPS mode
 */
int dsim_reg_set_ulps(u32 id, u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	} else {
		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(id, 1);

		ret = dsim_reg_wait_exit_ulps_state(id);
		if (ret)
			return ret;
		udelay(1000);   /* wait at least 1ms : Twakeup time for MARK1 state  */

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(id, 0);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	}

	return ret;
}

/*
 * enter or exit ulps mode for LSI DDI
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : exit ULPS mode
 * assume that disp block power is off after ulps mode enter
 */
int dsim_reg_set_smddi_ulps(u32 id, u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;
		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	} else {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;

		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(id, 1);

		ret = dsim_reg_wait_exit_ulps_state(id);
		if (ret)
			return ret;

		udelay(1000);   /* wait at least 1ms : Twakeup time for MARK1 state */

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(id, 0);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	}

	return ret;
}

#ifdef CONFIG_DECON_MIPI_DSI_PKTGO
void dsim_reg_set_pkt_go_enable(u32 id, bool en)
{
	u32 val = en ? ~0 : 0;
	dsim_write_mask(id, DSIM_MULTI_PKT, val, DSIM_MULTI_PKT_GO_EN);
}

void dsim_reg_set_pkt_go_ready(u32 id)
{
	dsim_write_mask(id, DSIM_MULTI_PKT, ~0, DSIM_MULTI_PKT_GO_RDY);
}

void dsim_reg_set_pkt_go_cnt(u32 id, unsigned int count)
{
	u32 val = DSIM_MULTI_PKT_SEND_CNT(count);
	dsim_write_mask(id, DSIM_MULTI_PKT, val, DSIM_MULTI_PKT_SEND_CNT_MASK);
}
#endif

u32 dsim_reg_get_lineval(u32 id)
{
	u32 val = dsim_read(id, DSIM_MDRESOL);
	return DSIM_MDRESOL_LINEVAL_GET(val);
}

u32 dsim_reg_get_hozval(u32 id)
{
	u32 val = dsim_read(id, DSIM_MDRESOL);
	return DSIM_MDRESOL_HOZVAL_GET(val);
}
