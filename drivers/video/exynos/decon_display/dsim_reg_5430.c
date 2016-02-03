/* linux/drivers/video/decon_display/dsim_reg_5430.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Jiun Yu <jiun.yu@samsung.com>
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "dsim_reg.h"

#define MHZ				(1000 * 1000)

/* These definitions are need to guide from AP team */
#define DSIM_STOP_STATE_CNT		0xa
#define DSIM_BTA_TIMEOUT		0xff
#define DSIM_LP_RX_TIMEOUT		0xffff
#define DSIM_MULTI_PACKET_CNT		0xffff
#define DSIM_PLL_STABLE_TIME		22200

/* If below values depend on panel. These values wil be move to panel file.
 * And these values are valid in case of video mode only. */
#define DSIM_CMD_ALLOW_VALUE		4
#define DSIM_STABLE_VFP_VALUE		2

/* DPHY timing table */
const u32 dphy_timing[][10] = {
	/* bps, clk_prepare, clk_zero, clk_post, clk_trail, hs_prepare, hs_zero, hs_trail, lpx */
	{1500, 13, 65, 17, 13, 16, 24, 16, 11, 18},
	{1490, 13, 65, 17, 13, 16, 24, 16, 11, 18},
	{1480, 13, 64, 17, 13, 16, 24, 16, 11, 18},
	{1470, 13, 64, 17, 13, 16, 24, 16, 11, 18},
	{1460, 13, 63, 17, 13, 16, 24, 16, 10, 18},
	{1450, 13, 63, 17, 13, 16, 23, 16, 10, 18},
	{1440, 13, 63, 17, 13, 15, 23, 16, 10, 18},
	{1430, 12, 62, 17, 13, 15, 23, 16, 10, 17},
	{1420, 12, 62, 17, 13, 15, 23, 16, 10, 17},
	{1410, 12, 61, 16, 13, 15, 23, 16, 10, 17},
	{1400, 12, 61, 16, 13, 15, 23, 16, 10, 17},
	{1390, 12, 60, 16, 12, 15, 22, 15, 10, 17},
	{1380, 12, 60, 16, 12, 15, 22, 15, 10, 17},
	{1370, 12, 59, 16, 12, 15, 22, 15, 10, 17},
	{1360, 12, 59, 16, 12, 15, 22, 15, 10, 17},
	{1350, 12, 59, 16, 12, 14, 22, 15, 10, 16},
	{1340, 12, 58, 16, 12, 14, 21, 15, 10, 16},
	{1330, 11, 58, 16, 12, 14, 21, 15, 9, 16},
	{1320, 11, 57, 16, 12, 14, 21, 15, 9, 16},
	{1310, 11, 57, 16, 12, 14, 21, 15, 9, 16},
	{1300, 11, 56, 16, 12, 14, 21, 15, 9, 16},
	{1290, 11, 56, 16, 12, 14, 21, 15, 9, 16},
	{1280, 11, 56, 15, 11, 14, 20, 14, 9, 16},
	{1270, 11, 55, 15, 11, 14, 20, 14, 9, 15},
	{1260, 11, 55, 15, 11, 13, 20, 14, 9, 15},
	{1250, 11, 54, 15, 11, 13, 20, 14, 9, 15},
	{1240, 11, 54, 15, 11, 13, 20, 14, 9, 15},
	{1230, 11, 53, 15, 11, 13, 19, 14, 9, 15},
	{1220, 10, 53, 15, 11, 13, 19, 14, 9, 15},
	{1210, 10, 52, 15, 11, 13, 19, 14, 9, 15},
	{1200, 10, 52, 15, 11, 13, 19, 14, 9, 15},
	{1190, 10, 52, 15, 11, 13, 19, 14, 8, 14},
	{1180, 10, 51, 15, 11, 13, 19, 13, 8, 14},
	{1170, 10, 51, 15, 10, 12, 18, 13, 8, 14},
	{1160, 10, 50, 15, 10, 12, 18, 13, 8, 14},
	{1150, 10, 50, 15, 10, 12, 18, 13, 8, 14},
	{1140, 10, 49, 14, 10, 12, 18, 13, 8, 14},
	{1130, 10, 49, 14, 10, 12, 18, 13, 8, 14},
	{1120, 10, 49, 14, 10, 12, 17, 13, 8, 14},
	{1110, 9, 48, 14, 10, 12, 17, 13, 8, 13},
	{1100, 9, 48, 14, 10, 12, 17, 13, 8, 13},
	{1090, 9, 47, 14, 10, 12, 17, 13, 8, 13},
	{1080, 9, 47, 14, 10, 11, 17, 13, 8, 13},
	{1070, 9, 46, 14, 10, 11, 17, 12, 8, 13},
	{1060, 9, 46, 14, 10, 11, 16, 12, 7, 13},
	{1050, 9, 45, 14, 9, 11, 16, 12, 7, 13},
	{1040, 9, 45, 14, 9, 11, 16, 12, 7, 13},
	{1030, 9, 45, 14, 9, 11, 16, 12, 7, 12},
	{1020, 9, 44, 14, 9, 11, 16, 12, 7, 12},
	{1010, 8, 44, 13, 9, 11, 15, 12, 7, 12},
	{1000, 8, 43, 13, 9, 11, 15, 12, 7, 12},
	{990, 8, 43, 13, 9, 10, 15, 12, 7, 12},
	{980, 8, 42, 13, 9, 10, 15, 12, 7, 12},
	{970, 8, 42, 13, 9, 10, 15, 12, 7, 12},
	{960, 8, 42, 13, 9, 10, 15, 11, 7, 12},
	{950, 8, 41, 13, 9, 10, 14, 11, 7, 11},
	{940, 8, 41, 13, 8, 10, 14, 11, 7, 11},
	{930, 8, 40, 13, 8, 10, 14, 11, 6, 11},
	{920, 8, 40, 13, 8, 10, 14, 11, 6, 11},
	{910, 8, 39, 13, 8, 9, 14, 11, 6, 11},
	{900, 7, 39, 13, 8, 9, 13, 11, 6, 11},
	{890, 7, 38, 13, 8, 9, 13, 11, 6, 11},
	{880, 7, 38, 12, 8, 9, 13, 11, 6, 11},
	{870, 7, 38, 12, 8, 9, 13, 11, 6, 10},
	{860, 7, 37, 12, 8, 9, 13, 11, 6, 10},
	{850, 7, 37, 12, 8, 9, 13, 10, 6, 10},
	{840, 7, 36, 12, 8, 9, 12, 10, 6, 10},
	{830, 7, 36, 12, 8, 9, 12, 10, 6, 10},
	{820, 7, 35, 12, 7, 8, 12, 10, 6, 10},
	{810, 7, 35, 12, 7, 8, 12, 10, 6, 10},
	{800, 7, 35, 12, 7, 8, 12, 10, 6, 10},
	{790, 6, 34, 12, 7, 8, 11, 10, 5, 9},
	{780, 6, 34, 12, 7, 8, 11, 10, 5, 9},
	{770, 6, 33, 12, 7, 8, 11, 10, 5, 9},
	{760, 6, 33, 12, 7, 8, 11, 10, 5, 9},
	{750, 6, 32, 12, 7, 8, 11, 9, 5, 9},
	{740, 6, 32, 11, 7, 8, 11, 9, 5, 9},
	{730, 6, 31, 11, 7, 7, 10, 9, 5, 9},
	{720, 6, 31, 11, 7, 7, 10, 9, 5, 9},
	{710, 6, 31, 11, 6, 7, 10, 9, 5, 8},
	{700, 6, 30, 11, 6, 7, 10, 9, 5, 8},
	{690, 5, 30, 11, 6, 7, 10, 9, 5, 8},
	{680, 5, 29, 11, 6, 7, 9, 9, 5, 8},
	{670, 5, 29, 11, 6, 7, 9, 9, 5, 8},
	{660, 5, 28, 11, 6, 7, 9, 9, 4, 8},
	{650, 5, 28, 11, 6, 7, 9, 9, 4, 8},
	{640, 5, 28, 11, 6, 6, 9, 8, 4, 8},
	{630, 5, 27, 11, 6, 6, 9, 8, 4, 7},
	{620, 5, 27, 11, 6, 6, 8, 8, 4, 7},
	{610, 5, 26, 10, 6, 6, 8, 8, 4, 7},
	{600, 5, 26, 10, 6, 6, 8, 8, 4, 7},
	{590, 5, 25, 10, 5, 6, 8, 8, 4, 7},
	{580, 4, 25, 10, 5, 6, 8, 8, 4, 7},
	{570, 4, 24, 10, 5, 6, 7, 8, 4, 7},
	{560, 4, 24, 10, 5, 6, 7, 8, 4, 7},
	{550, 4, 24, 10, 5, 5, 7, 8, 4, 6},
	{540, 4, 23, 10, 5, 5, 7, 8, 4, 6},
	{530, 4, 23, 10, 5, 5, 7, 7, 3, 6},
	{520, 4, 22, 10, 5, 5, 7, 7, 3, 6},
	{510, 4, 22, 10, 5, 5, 6, 7, 3, 6},
	{500, 4, 21, 10, 5, 5, 6, 7, 3, 6},
	{490, 4, 21, 10, 5, 5, 6, 7, 3, 6},
	{480, 4, 21, 9, 4, 5, 6, 7, 3, 6},
	{450, 3, 19, 9, 4, 4, 5, 7, 3, 5},
};

const u32 b_dphyctl[][2] = {
	{20, 0x1F4},
	{19, 0x1DB},
	{18, 0x1C2},
	{17, 0x1A9},
	{16, 0x190},
	{15, 0x177},
	{14, 0x15E},
	{13, 0x145},
	{12, 0x12C},
	{11, 0x113},
	{10, 0x0FA},
	{9, 0x0E1},
	{8, 0x0C8},
	{7, 0x0AF},
	{6, 0x096},
	{5, 0x07D},
	{4, 0x064},
	{3, 0x04B},
	{2, 0x032},
	{1, 0x019}
};


/******************* CAL raw functions implementation *************************/
void dsim_reg_sw_reset(void)
{
	dsim_write_mask(DSIM_SWRST, ~0, DSIM_SWRST_RESET);
}

void dsim_reg_funtion_reset(void)
{
	dsim_write_mask(DSIM_SWRST, ~0, DSIM_SWRST_FUNCRST);
}

void dsim_reg_dp_dn_swap(u32 en)
{
	u32 val = en ? ~0 : 0;
	u32 mask = DSIM_PLLCTRL_DPDN_SWAP_DATA | DSIM_PLLCTRL_DPDN_SWAP_CLK;

	dsim_write_mask(DSIM_PLLCTRL, val, mask);
}

static void dsim_reg_enable_lane(u32 lane, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(DSIM_CONFIG, val, DSIM_CONFIG_LANE_ENx(lane));
}

void dsim_reg_set_byte_clk_src_is_pll(void)
{
	u32 val = 0;

	dsim_write_mask(DSIM_CLKCTRL, val, DSIM_CLKCTRL_BYTE_CLK_SRC_MASK);
}

void dsim_reg_set_pll_freq(u32 p, u32 m, u32 s)
{
	u32 val = (p & 0x3f) << 13 | (m & 0x1ff) << 4 | (s & 0x7) << 1;

	dsim_write_mask(DSIM_PLLCTRL, val, DSIM_PLLCTRL_PMS_MASK);
}

void dsim_reg_pll_stable_time(void)
{
	dsim_write(DSIM_PLLTMR, DSIM_PLL_STABLE_TIME);
}

void dsim_reg_set_dphy_timing_values(struct dphy_timing_value *t)
{
	u32 val, mask;

	val = DSIM_PHYTIMING_M_TLPXCTL(t->lpx) |
		DSIM_PHYTIMING_M_THSEXITCTL(t->hs_exit);
	dsim_write(DSIM_PHYTIMING, val);

	val = DSIM_PHYTIMING1_M_TCLKPRPRCTL(t->clk_prepare) |
		DSIM_PHYTIMING1_M_TCLKZEROCTL(t->clk_zero) |
		DSIM_PHYTIMING1_M_TCLKPOSTCTL(t->clk_post) |
		DSIM_PHYTIMING1_M_TCLKTRAILCTL(t->clk_trail);
	dsim_write(DSIM_PHYTIMING1, val);

	val = DSIM_PHYTIMING2_M_THSPRPRCTL(t->hs_prepare) |
		DSIM_PHYTIMING2_M_THSZEROCTL(t->hs_zero) |
		DSIM_PHYTIMING2_M_THSTRAILCTL(t->hs_trail);
	dsim_write(DSIM_PHYTIMING2, val);

	val = DSIM_PHYCTRL_B_DPHYCTL(t->b_dphyctl) |
		DSIM_PHYCTRL_B_DPHYCTL_VREG_LP |
		DSIM_PHYCTRL_B_DPHYCTL_SLEW_UP;
	mask = DSIM_PHYCTRL_B_DPHYCTL_MASK |
		DSIM_PHYCTRL_B_DPHYCTL_VREG_LP |
		DSIM_PHYCTRL_B_DPHYCTL_SLEW_UP;
	dsim_write_mask(DSIM_B_DPHYCTRL, val, mask);

	val = DSIM_PHYCTRL_M_DPHYCTL_VREG_HS;
	mask = DSIM_PHYCTRL_M_DPHYCTL_VREG_HS;
	dsim_write_mask(DSIM_M_DPHYCTRL, val, mask);
}

void dsim_reg_pll_bypass(u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(DSIM_CLKCTRL, val, DSIM_CLKCTRL_PLL_BYPASS);
}

void dsim_reg_clear_int(u32 int_src)
{
	dsim_write(DSIM_INTSRC, int_src);
}

void dsim_reg_clear_int_all(void)
{
	dsim_write(DSIM_INTSRC, 0xffffffff);
}

void dsim_reg_set_pll(u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(DSIM_PLLCTRL, val, DSIM_PLLCTRL_PLL_EN);
}

u32 dsim_reg_is_pll_stable(void)
{
	u32 val;

	val = dsim_read(DSIM_STATUS);
	if (val & DSIM_STATUS_PLL_STABLE)
		return 1;

	return 0;
}

int dsim_reg_enable_pll(u32 en)
{
	u32 cnt = 1000;

	if (en) {
		dsim_reg_clear_int(DSIM_INTSRC_PLL_STABLE);

		dsim_reg_set_pll(1);
		udelay(200); /* should be wait 200us to stable PLL */

		while (1) {
			cnt--;
			if (dsim_reg_is_pll_stable())
				return 0;
			if (cnt == 0)
				return -EBUSY;
		}
	} else {
		dsim_reg_set_pll(0);
	}

	return 0;
}

void dsim_reg_set_byte_clock(u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(DSIM_CLKCTRL, val, DSIM_CLKCTRL_BYTECLK_EN);
}

void dsim_reg_set_esc_clk_prescaler(u32 en, u32 p)
{
	u32 val = en ? DSIM_CLKCTRL_ESCCLK_EN : 0;
	u32 mask = DSIM_CLKCTRL_ESCCLK_EN | DSIM_CLKCTRL_ESC_PRESCALER_MASK;

	val |= DSIM_CLKCTRL_ESC_PRESCALER(p);
	dsim_write_mask(DSIM_CLKCTRL, val, mask);
}

void dsim_reg_set_esc_clk_on_lane(u32 en, u32 lane)
{
	u32 val = en ? DSIM_CLKCTRL_LANE_ESCCLK_EN(lane) : 0;

	dsim_write_mask(DSIM_CLKCTRL, val, DSIM_CLKCTRL_LANE_ESCCLK_EN_MASK);
}

static u32 dsim_reg_is_lane_stop_state(u32 lanes)
{
	u32 val = dsim_read(DSIM_STATUS);
	u32 data_lane = lanes >> DSIM_LANE_CLOCK;
	/**
	 * check clock and data lane states.
	 * if MIPI-DSI controller was enabled at bootloader then
	 * TX_READY_HS_CLK is enabled otherwise STOP_STATE_CLK.
	 * so it should be checked for two case.
	 */
	if ((DSIM_STATUS_STOP_STATE_DATA_LANE_GET(val) == data_lane) &&
		((val & DSIM_STATUS_STOP_STATE_CLK) || (val & DSIM_STATUS_TX_READY_HS_CLK)))
		return 1;

	return 0;
}

u32 dsim_reg_wait_lane_stop_state(u32 lanes)
{
	u32 state;
	u32 cnt = 100;

	do {
		state = dsim_reg_is_lane_stop_state(lanes);
		cnt--;
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("wait timeout DSI Master is not stop state.\n");
		dsim_err("check initialization process.\n");
		return -EBUSY;
	}

	return 0;
}

void dsim_reg_set_stop_state_cnt(void)
{
	u32 val = DSIM_ESCMODE_STOP_STATE_CNT(DSIM_STOP_STATE_CNT);

	dsim_write_mask(DSIM_ESCMODE, val, DSIM_ESCMODE_STOP_STATE_CNT_MASK);
}

void dsim_reg_set_bta_timeout(void)
{
	u32 val = DSIM_TIMEOUT_BTA_TOUT(DSIM_BTA_TIMEOUT);

	dsim_write_mask(DSIM_TIMEOUT, val, DSIM_TIMEOUT_BTA_TOUT_MASK);
}

void dsim_reg_set_lpdr_timeout(void)
{
	u32 val = DSIM_TIMEOUT_LPDR_TOUT(DSIM_LP_RX_TIMEOUT);

	dsim_write_mask(DSIM_TIMEOUT, val, DSIM_TIMEOUT_LPDR_TOUT_MASK);
}

void dsim_reg_set_packet_ctrl(void)
{
	u32 val = DSIM_MULTI_PKT_CNT(DSIM_MULTI_PACKET_CNT);
	dsim_write_mask(DSIM_MULTI_PKT, val, DSIM_MULTI_PKT_CNT_MASK);
}

void dsim_reg_set_porch(struct decon_lcd *lcd)
{
	u32 val, mask, width, height;

	if (lcd->mode == VIDEO_MODE) {
		val = DSIM_MVPORCH_CMD_ALLOW(DSIM_CMD_ALLOW_VALUE) |
			DSIM_MVPORCH_STABLE_VFP(DSIM_STABLE_VFP_VALUE) |
			DSIM_MVPORCH_VBP(lcd->vbp);
		mask = DSIM_MVPORCH_CMD_ALLOW_MASK | DSIM_MVPORCH_VBP_MASK |
			DSIM_MVPORCH_STABLE_VFP_MASK;
		dsim_write_mask(DSIM_MVPORCH, val, mask);

		val = DSIM_MHPORCH_HFP(lcd->hfp) | DSIM_MHPORCH_HBP(lcd->hbp);
		dsim_write(DSIM_MHPORCH, val);

		val = DSIM_MSYNC_VSA(lcd->vsa) | DSIM_MSYNC_HSA(lcd->hsa);
		mask = DSIM_MSYNC_VSA_MASK | DSIM_MSYNC_HSA_MASK;
		dsim_write_mask(DSIM_MSYNC, val, mask);
	}

	width = lcd->xres << lcd->hsync_2h_cycle;
	height = (lcd->mic || lcd->hsync_2h_cycle) ? lcd->yres >> 1 : lcd->yres;

	val = DSIM_MDRESOL_VRESOL(height) | DSIM_MDRESOL_HRESOL(width);
	mask = DSIM_MDRESOL_VRESOL_MASK | DSIM_MDRESOL_HRESOL_MASK;
	dsim_write_mask(DSIM_MDRESOL, val, mask);
}

void dsim_reg_set_config(u32 mode, u32 data_lane_cnt)
{
	u32 val;
	u32 mask;

	if (mode == VIDEO_MODE) {
		val = DSIM_CONFIG_VIDEO_MODE | DSIM_CONFIG_BURST_MODE |
		DSIM_CONFIG_HFP_DISABLE | DSIM_CONFIG_MFLUSH_VS;
	} else if (mode == COMMAND_MODE) {
		val = DSIM_CONFIG_CLKLANE_STOP_START;   /* In Command mode Clk lane stop disable */
	} else {
		dsim_err("This DDI is not MIPI interface.\n");
		return;
	}

	val |= DSIM_CONFIG_EOT_R03_DISABLE |
		DSIM_CONFIG_NUM_OF_DATA_LANE(data_lane_cnt - 1) |
		DSIM_CONFIG_PIXEL_FORMAT(DSIM_PIXEL_FORMAT_RGB24);

	mask = DSIM_CONFIG_CLKLANE_STOP_START | DSIM_CONFIG_MFLUSH_VS | DSIM_CONFIG_EOT_R03_DISABLE |
		DSIM_CONFIG_BURST_MODE | DSIM_CONFIG_VIDEO_MODE | DSIM_CONFIG_HFP_DISABLE |
		DSIM_CONFIG_PIXEL_FORMAT_MASK | DSIM_CONFIG_NUM_OF_DATA_LANE_MASK;

	dsim_write_mask(DSIM_CONFIG, val, mask);
}

void dsim_reg_set_cmd_transfer_mode(u32 lp)
{
	u32 val = lp ? ~0 : 0;

	dsim_write_mask(DSIM_ESCMODE, val, DSIM_ESCMODE_CMD_LPDT);
}

void dsim_reg_set_data_transfer_mode(u32 lp)
{
	u32 val = lp ? ~0 : 0;

	dsim_write_mask(DSIM_ESCMODE, val, DSIM_ESCMODE_TX_LPDT);
}

void dsim_reg_enable_hs_clock(u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(DSIM_CLKCTRL, val, DSIM_CLKCTRL_TX_REQUEST_HSCLK);
}

static u32 dsim_reg_is_hs_clk_ready(void)
{
	if (dsim_read(DSIM_STATUS) & DSIM_STATUS_TX_READY_HS_CLK)
		return 1;

	return 0;
}

int dsim_reg_wait_hs_clk_ready(void)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_hs_clk_ready();
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not HS state.\n");
		return -EBUSY;
	}

	return 0;
}

void dsim_reg_set_fifo_ctrl(u32 cfg)
{
	/* first clear the bit and then set high */
	dsim_write_mask(DSIM_FIFOCTRL, ~cfg, cfg);
	dsim_write_mask(DSIM_FIFOCTRL, ~0, cfg);
}

void dsim_reg_force_dphy_stop_state(u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(DSIM_ESCMODE, val, DSIM_ESCMODE_FORCE_STOP_STATE);
}

void dsim_reg_wr_tx_header(u32 id, unsigned long data0, u32 data1)
{
	u32 val = DSIM_PKTHDR_ID(id) | DSIM_PKTHDR_DATA0(data0) |
		DSIM_PKTHDR_DATA1(data1);

	dsim_write(DSIM_PKTHDR, val);
}

void dsim_reg_wr_tx_payload(u32 payload)
{
	dsim_write(DSIM_PAYLOAD, payload);
}

void dsim_reg_enter_ulps(u32 enter)
{
	u32 val = enter ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK | DSIM_ESCMODE_TX_ULPS_DATA;

	dsim_write_mask(DSIM_ESCMODE, val, mask);
}

void dsim_reg_exit_ulps(u32 exit)
{
	u32 val = exit ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK_EXIT | DSIM_ESCMODE_TX_ULPS_DATA_EXIT;

	dsim_write_mask(DSIM_ESCMODE, val, mask);
}

static u32 dsim_reg_is_ulps_state(u32 lanes)
{
	u32 val = dsim_read(DSIM_STATUS);
	u32 data_lane = lanes >> DSIM_LANE_CLOCK;

	if ((DSIM_STATUS_ULPS_DATA_LANE_GET(val) == data_lane)
			&& (val & DSIM_STATUS_ULPS_CLK))
		return 1;

	return 0;
}

int dsim_reg_wait_enter_ulps_state(u32 lanes)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_ulps_state(lanes);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not ULPS state.\n");
		return -EBUSY;
	}

	return 0;
}

static u32 dsim_reg_is_not_ulps_state(void)
{
	u32 val = dsim_read(DSIM_STATUS);

	if (!(DSIM_STATUS_ULPS_DATA_LANE_GET(val))
			&& !(val & DSIM_STATUS_ULPS_CLK))
		return 1;

	return 0;
}

int dsim_reg_wait_exit_ulps_state(void)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_not_ulps_state();
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not stop state.\n");
		return -EBUSY;
	}

	return 0;
}

void dsim_reg_set_standby(u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(DSIM_MDRESOL, val, DSIM_MDRESOL_STAND_BY);
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

	/*
	 * One clk lane consists of 2 lines.
	 * HS clk freq = line rate X 2
	 * Here, we calculate the freq of ONE line(fout_cal is the freq of ONE line).
	 * Thus, target_freq = dsim->lcd_info->hs_clk/2.
	 */
	dsim_info("requested HS clock is %ld\n", pll->pll_freq);
	target_freq = pll->pll_freq / 2;
	target_freq /= MHZ;

	for (p_div = 1; p_div <= 33; p_div++) {
		for (m_div = 25; m_div <= 125; m_div++) {
			for (s_div = 0; s_div <= 3; s_div++) {
				fin_pll = fin / p_div;
				voc_out = (m_div * fin) / p_div;
				fout_cal = (m_div * fin) / (p_div * (1 << s_div));

				if ((fin_pll < 6) || (fin_pll > 12))
					continue;
				if ((voc_out < 300) || (voc_out > 750))
					continue;
				if (fout_cal < target_freq)
					continue;
				if ((target_freq == fout_cal) && (fout_cal <= 750))
					goto calculation_success;
			}
		}
	}

	for (p_div = 1; p_div <= 33; p_div++) {
		for (m_div = 25; m_div <= 125; m_div++) {
			for (s_div = 0; s_div <= 3; s_div++) {
				fin_pll = fin / p_div;
				voc_out = (m_div * fin) / p_div;
				fout_cal = (m_div * fin) / (p_div * (1 << s_div));

				if ((fin_pll < 6) || (fin_pll > 12))
					continue;
				if ((voc_out < 300) || (voc_out > 750))
					continue;
				if (fout_cal < target_freq)
					continue;
				/* target_freq < fout_cal, here is different from the above */
				if ((target_freq < fout_cal) && (fout_cal <= 750))
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
	pll->pll_freq = fout_cal * 2 * MHZ;
	dsim_info("calculated HS clock is %ld. p(%u), m(%u), s(%u)\n",
			pll->pll_freq, pll->p, pll->m, pll->s);

	return 0;
}

static int dsim_reg_get_dphy_timing(unsigned long hs_clk, unsigned long esc_clk, struct dphy_timing_value *t)
{
	int i = sizeof(dphy_timing) / sizeof(dphy_timing[0]) - 1;

	while (i) {
		if ((dphy_timing[i][0] * MHZ) < hs_clk) {
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
		dsim_err("%ld hs clock can't find proper dphy timing values\n", hs_clk);
		return -EINVAL;
	}

	if ((esc_clk > 20 * MHZ) || (esc_clk < 7 * MHZ)) {
		dsim_err("%ld can't be used as escape clock\n", esc_clk);
		return -EINVAL;
	}

	i = sizeof(b_dphyctl) / sizeof(b_dphyctl[0]);
	while (i) {
		if ((b_dphyctl[i - 1][0] * MHZ) < esc_clk) {
			i--;
			continue;
		} else {
			t->b_dphyctl = b_dphyctl[i - 1][1];
			break;
		}
	}

	if (!i) {
		dsim_err("%ld esc clock can't find proper dphy timing values\n", esc_clk);
		return -EINVAL;
	}

	return 0;
}



/***************** CAL APIs implementation *******************/

void dsim_reg_init(struct decon_lcd *lcd_info, u32 data_lane_cnt)
{
	/* phy reset is called in stead of s/w reset*/
	dsim_reg_funtion_reset();
	/* dsim_reg_dp_dn_swap(0); discard in Helsinki-Prime */

	/* set counter */
	dsim_reg_set_stop_state_cnt();
	dsim_reg_set_bta_timeout();
	dsim_reg_set_lpdr_timeout();
	dsim_reg_set_packet_ctrl();

	/* set DSIM configuration */
	dsim_reg_set_porch(lcd_info);
	dsim_reg_set_config(lcd_info->mode, data_lane_cnt);
}

void dsim_reg_init_probe(struct decon_lcd *lcd_info, u32 data_lane_cnt)
{
	/* set counter */
	dsim_reg_set_stop_state_cnt();
	dsim_reg_set_bta_timeout();
	dsim_reg_set_lpdr_timeout();
	dsim_reg_set_packet_ctrl();

	/* set DSIM configuration */
	dsim_reg_set_porch(lcd_info);
	dsim_reg_set_config(lcd_info->mode, data_lane_cnt);
}

/*
 * configure and set DPHY PLL, byte clock, escape clock and hs clock
*
 * Parameters
 *	- hs_clk : in/out parameter.
 *		in :  requested hs clock. out : calculated hs clock
 *	- esc_clk : in/out paramter.
 *		in : requested escape clock. out : calculated escape clock
 *	- byte_clk : out parameter. byte clock = hs clock / 8
 */
int dsim_reg_set_clocks(struct dsim_clks *clks, u32 lane, u32 en)
{
	unsigned int esc_div;
	struct dsim_pll_param pll;
	struct dphy_timing_value t;
	int ret;

	if (en) {
		/* byte clock source must be DPHY PLL */
		dsim_reg_set_byte_clk_src_is_pll();

		/* requested DPHY PLL frequency(HS clock) */
		pll.pll_freq = clks->hs_clk;
		/* calculate p, m, s for setting DPHY PLL and hs clock */
		ret = dsim_reg_calculate_pms(&pll);
		if (ret)
			return ret;
		/* store calculated hs clock */
		clks->hs_clk = pll.pll_freq;
		/* set p, m, s to DPHY PLL */
		dsim_reg_set_pll_freq(pll.p, pll.m, pll.s);

		/* set PLL's lock time */
		dsim_reg_pll_stable_time();

		/* get byte clock */
		clks->byte_clk = clks->hs_clk / 8;
		dsim_info("byte clock is %ld\n", clks->byte_clk);

		/* requeseted escape clock */
		dsim_info("requested escape clock %ld\n", clks->esc_clk);
		/* escape clock divider */
		esc_div = clks->byte_clk / clks->esc_clk;

		/* adjust escape clock */
		if ((clks->byte_clk / esc_div) > clks->esc_clk)
			esc_div += 1;
		/* adjusted escape clock */
		clks->esc_clk = clks->byte_clk / esc_div;
		dsim_info("escape clock divider is 0x%x\n", esc_div);
		dsim_info("escape clock is %ld\n", clks->esc_clk);

		/* get DPHY timing values using hs clock and escape clock */
		dsim_reg_get_dphy_timing(clks->hs_clk, clks->esc_clk, &t);
		dsim_reg_set_dphy_timing_values(&t);

		/* DPHY uses PLL output */
		/* dsim_reg_pll_bypass(0); discard in Helsinki-Prime */

		/* enable PLL */
		dsim_reg_enable_pll(1);

		/* enable byte clock. */
		dsim_reg_set_byte_clock(1);

		/* enable escape clock */
		dsim_reg_set_esc_clk_prescaler(1, esc_div);

		/* escape clock on lane */
		dsim_reg_set_esc_clk_on_lane(1, lane);

	} else {
		dsim_reg_set_esc_clk_on_lane(0, lane);
		dsim_reg_set_esc_clk_prescaler(0, 0);

		dsim_reg_set_byte_clock(0);
		dsim_reg_enable_pll(0);
	}

	return 0;
}

int dsim_reg_set_lanes(u32 lanes, u32 en)
{
	int ret;

	dsim_reg_enable_lane(lanes, en);
	udelay(300);
	if (en) {
		ret = dsim_reg_wait_lane_stop_state(lanes);
		if (ret)
			return ret;
	}

	return 0;
}

void dsim_reg_set_mdclk(u32 enbyteclk)
{
	/* 0:External CLK, 1:BYTECLK */
	u32 val = enbyteclk ? ~0 : 0;
	dsim_write_mask(DSIM_CONFIG, val, DSIM_CONFIG_MDCLK);
}

void dsim_reg_set_link_standby(void)
{
	dsim_reg_set_mdclk(1);
	/* reset ip state_machine */
	dsim_reg_funtion_reset();
	dsim_reg_set_standby(1);
	dsim_reg_set_mdclk(0);
}

int dsim_reg_set_hs_clock(u32 en)
{
	int ret;

	if (en) {
		dsim_reg_set_cmd_transfer_mode(0);
		dsim_reg_set_data_transfer_mode(0);

		dsim_reg_enable_hs_clock(1);

		ret = dsim_reg_wait_hs_clk_ready();
		if (ret)
			return ret;

		dsim_reg_set_link_standby();
	} else {
		/* MD stand_by off */
		dsim_reg_set_standby(0);
		dsim_reg_enable_hs_clock(0);
	}

	return 0;
}

void dsim_reg_set_int(u32 en)
{
	u32 val = en ? 0 : ~0;
	u32 mask;

	mask = DSIM_INTMSK_SFR_PL_FIFO_EMPTY | DSIM_INTMSK_SFR_PH_FIFO_EMPTY |
		DSIM_INTMSK_FRAME_DONE | DSIM_INTMSK_RX_DATA_DONE |
		DSIM_INTMSK_RX_ECC;

	dsim_write_mask(DSIM_INTMSK, val, mask);
}

/*
 * enter or exit ulps mode
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : exit ULPS mode
 */
int dsim_reg_set_ulps(u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(lanes);
		if (ret)
			return ret;

	} else {
		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(1);

		ret = dsim_reg_wait_exit_ulps_state();
		if (ret)
			return ret;
		udelay(1000);   /* wait at least 1ms : Twakeup time for MARK1 state  */

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(0);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(0);
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
int dsim_reg_set_smddi_ulps(u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(lanes);
		if (ret)
			return ret;
	} else {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(lanes);
		if (ret)
			return ret;

		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(1);

		ret = dsim_reg_wait_exit_ulps_state();
		if (ret)
			return ret;

		udelay(1000);   /* wait at least 1ms : Twakeup time for MARK1 state */

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(0);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(0);
	}

	return ret;
}

void dsim_reg_set_clear_rx_fifo(void)
{
	int i;
	unsigned int rx_fifo;

	for (i = 0; i < DSIM_MAX_RX_FIFO; i++)
		rx_fifo = dsim_read(DSIM_RXFIFO);

	dsim_info("%s: rx_fifo 0x%08X\n", __func__, rx_fifo);
}

int dsim_reg_get_pkt_go_status(void)
{
	return (dsim_read(DSIM_MULTI_PKT) & DSIM_PKT_GO_EN) ? 1 : 0;
}

void dsim_reg_set_pkt_go_enable(bool en)
{
	u32 val = en ? ~0 : 0;
	dsim_write_mask(DSIM_MULTI_PKT, val, DSIM_PKT_GO_EN);
}

void dsim_reg_set_pkt_go_ready(void)
{
	dsim_write_mask(DSIM_MULTI_PKT, ~0, DSIM_PKT_GO_RDY);
}

void dsim_reg_set_pkt_go_cnt(unsigned int count)
{
	u32 val = DSIM_PKT_SEND_CNT(count);
	dsim_write_mask(DSIM_MULTI_PKT, val, DSIM_PKT_SEND_CNT_MASK);
}

/*
 * dsim main display configuration for window partial update
 *	- w : width for partial update
 *	- h : height for partial update
 *	- mic_on : MIC_ENABLE (1) / MIC_DISABLE (0)
 */
void dsim_reg_set_win_update_conf(int w, int h, bool mic_on)
{
	u32 val;
	u32 mask;

	if (mic_on)
		w = ((w >> 2) << 1) + (w & 0x3);
	/* Before setting config. disable standby */
	mask = DSIM_MDRESOL_STAND_BY;
	val = 0;
	dsim_write_mask(DSIM_MDRESOL, val, mask);

	val = DSIM_MDRESOL_VRESOL(h) | DSIM_MDRESOL_HRESOL(w) |
		DSIM_MDRESOL_STAND_BY;
	mask = DSIM_MDRESOL_STAND_BY | DSIM_MDRESOL_VRESOL_MASK |
		DSIM_MDRESOL_HRESOL_MASK;
	dsim_write_mask(DSIM_MDRESOL, val, mask);
}
