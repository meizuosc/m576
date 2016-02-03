/* linux/drivers/video/decon_display/dsim_reg.h
 *
 * Header file for Samsung MIPI-DSI lowlevel driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _DSIM_COMMON_H_
#define _DSIM_COMMON_H_

#include "./panels/decon_lcd.h"

#define DSIM_PIXEL_FORMAT_RGB24		0x7
#define DSIM_PIXEL_FORMAT_RGB18		0x6
#define DSIM_PIXEL_FORMAT_RGB18_PACKED	0x5

/* define DSI lane types. */
enum {
	DSIM_LANE_CLOCK	= (1 << 0),
	DSIM_LANE_DATA0	= (1 << 1),
	DSIM_LANE_DATA1	= (1 << 2),
	DSIM_LANE_DATA2	= (1 << 3),
	DSIM_LANE_DATA3	= (1 << 4),
};

struct dsim_pll_param {
	u32 p;
	u32 m;
	u32 s;
	u32 pll_freq; /* in/out parameter: Mhz */
};

struct dsim_clks {
	u32 hs_clk;
	u32 esc_clk;
	u32 byte_clk;
};

struct dphy_timing_value {
	u32 bps;
	u32 clk_prepare;
	u32 clk_zero;
	u32 clk_post;
	u32 clk_trail;
	u32 hs_prepare;
	u32 hs_zero;
	u32 hs_trail;
	u32 lpx;
	u32 hs_exit;
	u32 b_dphyctl;
};

/* CAL APIs list */
void dsim_reg_init(u32 id, struct decon_lcd *lcd_info, u32 data_lane_cnt);
void dsim_reg_init_probe(u32 id, struct decon_lcd *lcd_info, u32 data_lane_cnt);
int dsim_reg_set_clocks(u32 id, struct dsim_clks *clks, u32 lane, u32 en);
int dsim_reg_set_lanes(u32 id, u32 lanes, u32 en);
int dsim_reg_set_hs_clock(u32 id, u32 en);
void dsim_reg_set_int(u32 id, u32 en);
int dsim_reg_set_ulps(u32 id, u32 en, u32 lanes);
int dsim_reg_set_smddi_ulps(u32 id, u32 en, u32 lanes);

/* CAL raw functions list */
void dsim_reg_sw_reset(u32 id);
void dsim_reg_dp_dn_swap(u32 id, u32 en);
void dsim_reg_set_byte_clk_src_is_pll(u32 id);
void dsim_reg_set_pll_freq(u32 id, u32 p, u32 m, u32 s);
void dsim_reg_pll_stable_time(u32 id);
void dsim_reg_set_dphy_timing_values(u32 id, struct dphy_timing_value *t);
void dsim_reg_clear_int(u32 id, u32 int_src);
void dsim_reg_clear_int_all(u32 id);
void dsim_reg_set_pll(u32 id, u32 en);
u32 dsim_reg_is_pll_stable(u32 id);
int dsim_reg_enable_pll(u32 id, u32 en);
void dsim_reg_set_byte_clock(u32 id, u32 en);
void dsim_reg_set_esc_clk_prescaler(u32 id, u32 p, u32 en);
void dsim_reg_set_esc_clk_on_lane(u32 id, u32 lane, u32 en);
u32 dsim_reg_wait_lane_stop_state(u32 id);
void dsim_reg_set_stop_state_cnt(u32 id);
void dsim_reg_set_bta_timeout(u32 id);
void dsim_reg_set_lpdr_timeout(u32 id);
void dsim_reg_set_packet_ctrl(u32 id);
void dsim_reg_set_porch(u32 id, struct decon_lcd *lcd);
void dsim_reg_set_config(u32 id, u32 mode, u32 data_lane_cnt);
void dsim_reg_set_cmd_transfer_mode(u32 id, u32 lp);
void dsim_reg_set_data_transfer_mode(u32 id, u32 lp);
void dsim_reg_enable_hs_clock(u32 id, u32 en);
u32 dsim_reg_is_hs_clk_ready(u32 id);
int dsim_reg_wait_hs_clk_ready(u32 id);
void dsim_reg_set_fifo_ctrl(u32 id, u32 cfg);
void dsim_reg_force_dphy_stop_state(u32 id, u32 en);
void dsim_reg_wr_tx_header(u32 id, u32 data_id, unsigned long data0, u32 data1);
void dsim_reg_wr_tx_payload(u32 id, u32 payload);
void dsim_reg_enter_ulps(u32 id, u32 enter);
void dsim_reg_exit_ulps(u32 id, u32 exit);
int dsim_reg_wait_enter_ulps_state(u32 id, u32 lanes);
int dsim_reg_wait_exit_ulps_state(u32 id);
void dsim_reg_set_standby(u32 id, u32 en);
void dsim_reg_set_pkt_go_enable(u32 id, bool en);
void dsim_reg_set_pkt_go_ready(u32 id);
void dsim_reg_set_pkt_go_cnt(u32 id, unsigned int count);

#endif /* _DSIM_COMMON_H_ */
