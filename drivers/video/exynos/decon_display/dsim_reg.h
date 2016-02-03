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

#ifndef _DSIM_REG_H
#define _DSIM_REG_H

#include <linux/io.h>
#include <linux/delay.h>

#include "decon_mipi_dsi.h"
#include "regs-dsim.h"
#include "decon_display_driver.h"

#define DSIM_PIXEL_FORMAT_RGB24		0x7
#define DSIM_PIXEL_FORMAT_RGB18		0x6
#define DSIM_PIXEL_FORMAT_RGB18_PACKED	0x5

struct dsim_pll_param {
	u32 p;
	u32 m;
	u32 s;
	unsigned long pll_freq; /* in/out parameter: Mhz */
};

struct dsim_clks {
	unsigned long hs_clk;
	unsigned long esc_clk;
	unsigned long byte_clk;
};

static inline struct mipi_dsim_device *get_dsim_drvdata(void)
{
	struct display_driver *dispdrv = get_display_driver();
	return dispdrv->dsi_driver.dsim;
}

static inline int dsim_wr_data(u32 id, unsigned long d0, u32 d1)
{
	int ret;
	struct mipi_dsim_device *dsim = get_dsim_drvdata();

	ret = s5p_mipi_dsi_wr_data(dsim, id, d0, d1);
	if (ret)
		return ret;

	return 0;
}

/* register access subroutines */
static inline u32 dsim_read(u32 reg_id)
{
	struct mipi_dsim_device *dsim = get_dsim_drvdata();
	return readl(dsim->reg_base + reg_id);
}

static inline u32 dsim_read_mask(u32 reg_id, u32 mask)
{
	u32 val = dsim_read(reg_id);
	val &= (~mask);
	return val;
}

static inline void dsim_write(u32 reg_id, u32 val)
{
	struct mipi_dsim_device *dsim = get_dsim_drvdata();
	writel(val, dsim->reg_base + reg_id);
}

static inline void dsim_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct mipi_dsim_device *dsim = get_dsim_drvdata();
	u32 old = dsim_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->reg_base + reg_id);
}

#define dsim_err(fmt, ...)							\
	do {									\
		dev_err(get_dsim_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define dsim_info(fmt, ...)							\
	do {									\
		dev_dbg(get_dsim_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define dsim_dbg(fmt, ...)							\
	do {									\
		dev_dbg(get_dsim_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

/* CAL APIs list */
void dsim_reg_init(struct decon_lcd *lcd_info, u32 data_lane_cnt);
void dsim_reg_init_probe(struct decon_lcd *lcd_info, u32 data_lane_cnt);
int dsim_reg_set_clocks(struct dsim_clks *clks, u32 lane, u32 en);
int dsim_reg_set_lanes(u32 lanes, u32 en);
int dsim_reg_set_hs_clock(u32 en);
void dsim_reg_set_int(u32 en);
int dsim_reg_set_ulps(u32 en, u32 lanes);
int dsim_reg_set_smddi_ulps(u32 en, u32 lanes);

/* CAL raw functions list */
void dsim_reg_sw_reset(void);
void dsim_reg_dp_dn_swap(u32 en);
void dsim_reg_set_byte_clk_src_is_pll(void);
void dsim_reg_set_pll_freq(u32 p, u32 m, u32 s);
void dsim_reg_pll_stable_time(void);
void dsim_reg_set_dphy_timing_values(struct dphy_timing_value *t);
void dsim_reg_pll_bypass(u32 en);
void dsim_reg_clear_int(u32 int_src);
void dsim_reg_clear_int_all(void);
void dsim_reg_set_pll(u32 en);
u32 dsim_reg_is_pll_stable(void);
int dsim_reg_enable_pll(u32 en);
void dsim_reg_set_byte_clock(u32 en);
void dsim_reg_set_esc_clk_prescaler(u32 en, u32 p);
void dsim_reg_set_esc_clk_on_lane(u32 en, u32 lane);
u32 dsim_reg_wait_lane_stop_state(u32 lanes);
void dsim_reg_set_stop_state_cnt(void);
void dsim_reg_set_bta_timeout(void);
void dsim_reg_set_lpdr_timeout(void);
void dsim_reg_set_packet_ctrl(void);
void dsim_reg_set_porch(struct decon_lcd *lcd);
void dsim_reg_set_config(u32 mode, u32 data_lane_cnt);
void dsim_reg_set_cmd_transfer_mode(u32 lp);
void dsim_reg_set_data_transfer_mode(u32 lp);
void dsim_reg_enable_hs_clock(u32 en);
int dsim_reg_wait_hs_clk_ready(void);
void dsim_reg_set_fifo_ctrl(u32 cfg);
void dsim_reg_force_dphy_stop_state(u32 en);
void dsim_reg_wr_tx_header(u32 id, unsigned long data0, u32 data1);
void dsim_reg_wr_tx_payload(u32 payload);
void dsim_reg_enter_ulps(u32 enter);
void dsim_reg_exit_ulps(u32 exit);
int dsim_reg_wait_enter_ulps_state(u32 lanes);
int dsim_reg_wait_exit_ulps_state(void);
void dsim_reg_set_standby(u32 en);
void dsim_reg_set_clear_rx_fifo(void);
void dsim_reg_set_pkt_go_enable(bool en);
void dsim_reg_set_pkt_go_ready(void);
void dsim_reg_set_pkt_go_cnt(unsigned int count);
void dsim_reg_set_win_update_conf(int w, int h, bool mic_on);

#endif /* _DSIM_REG_H */
