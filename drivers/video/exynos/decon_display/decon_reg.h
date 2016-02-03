/* linux/drivers/video/decon_display/decon_reg.h
 *
 * Header file for Samsung DECON lowlevel driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 * Sewoon Park <seuni.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _DECON_FB_REGISTER_H
#define _DECON_FB_REGISTER_H

#include <linux/io.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/bitops.h>
#include <linux/v4l2-subdev.h>
#include <linux/delay.h>
#include <media/exynos_mc.h>

#include "decon_fb.h"
#include "regs-decon.h"
#include "decon_display_driver.h"
#include "decon_mipi_dsi.h"

enum decon_hold_scheme {
	DECON_VCLK_HOLD = 0x00,
	DECON_VCLK_RUNNING = 0x01,
	DECON_VCLK_RUN_VDEN_DISABLE = 0x3,
};

enum decon_rgb_order {
	DECON_RGB = 0x0,
	DECON_GBR = 0x1,
	DECON_BRG = 0x2,
	DECON_BGR = 0x4,
	DECON_RBG = 0x5,
	DECON_GRB = 0x6,
};

enum decon_set_trig {
	DECON_TRIG_DISABLE = 0,
	DECON_TRIG_ENABLE
};

struct decon_psr_info {
	enum s3c_fb_psr_mode psr_mode;
	enum decon_trig_mode trig_mode;
};

struct decon_init_param {
	struct decon_psr_info psr;
	struct decon_lcd *lcd_info;
	u32 nr_windows;
};

struct decon_regs_data {
	u32 wincon;
	u32 winmap;
	u32 vidosd_a;
	u32 vidosd_b;
	u32 vidosd_c;
	u32 vidosd_d;
	u32 vidw_buf_start;
	u32 vidw_buf_end;
	u32 vidw_buf_size;
	u32 blendeq;
};

static inline struct s3c_fb *get_decon_drvdata(void)
{
	struct display_driver *dispdrv = get_display_driver();

	return dispdrv->decon_driver.sfb;
}

/* register access subroutines */
static inline u32 decon_read(u32 reg_id)
{
	struct s3c_fb *sfb = get_decon_drvdata();
	return readl(sfb->regs + reg_id);
}

static inline u32 decon_read_mask(u32 reg_id, u32 mask)
{
	u32 val = decon_read(reg_id);
	val &= (~mask);
	return val;
}

static inline void decon_write(u32 reg_id, u32 val)
{
	struct s3c_fb *sfb = get_decon_drvdata();
	writel(val, sfb->regs + reg_id);
}

static inline void decon_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct s3c_fb *sfb = get_decon_drvdata();
	u32 old = decon_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, sfb->regs + reg_id);
}

#define decon_err(fmt, ...)							\
	do {									\
		dev_err(get_decon_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define decon_info(fmt, ...)							\
	do {									\
		dev_info(get_decon_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define decon_dbg(fmt, ...)							\
	do {									\
		dev_dbg(get_decon_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

/* CAL APIs list */
void decon_reg_init(struct decon_init_param *p);
void decon_reg_init_probe(struct decon_init_param *p);
void decon_reg_start(struct decon_psr_info *psr);
int decon_reg_stop(struct decon_psr_info *psr);
void decon_reg_set_regs_data(int idx, struct decon_regs_data *regs);
void decon_reg_set_int(struct decon_psr_info *psr, u32 en);
void decon_reg_set_trigger(enum decon_trig_mode trig, enum decon_set_trig en);
int decon_reg_wait_for_update_timeout(unsigned long timeout);
void decon_reg_shadow_protect_win(u32 idx, u32 protect);
void decon_reg_activate_window(u32 index);

/* CAL raw functions list */
int decon_reg_reset(void);
void decon_reg_set_clkgate_mode(u32 en);
void decon_reg_blend_alpha_bits(u32 alpha_bits);
void decon_reg_set_vidout(enum s3c_fb_psr_mode mode, u32 en);
void decon_reg_set_crc(u32 en);
void decon_reg_set_fixvclk(enum decon_hold_scheme mode);
void decon_reg_clear_win(int win);
void decon_reg_set_rgb_order(enum decon_rgb_order order);
void decon_reg_set_porch(struct decon_lcd *info);
void decon_reg_set_linecnt_op_threshold(u32 th);
void decon_reg_set_clkval(u32 clkdiv);
void decon_reg_direct_on_off(u32 en);
void decon_reg_per_frame_off(void);
void decon_reg_set_freerun_mode(u32 en);
void decon_reg_update_standalone(void);
void decon_reg_configure_lcd(struct decon_lcd *lcd_info);
void decon_reg_configure_trigger(enum decon_trig_mode mode);
void decon_reg_set_winmap(u32 idx, u32 color, u32 en);
u32 decon_reg_get_linecnt(void);
int decon_reg_wait_linecnt_is_zero_timeout(unsigned long timeout);
u32 decon_reg_get_stop_status(void);
int decon_reg_wait_stop_status_timeout(unsigned long timeout);
int decon_reg_is_win_enabled(int idx);
void decon_reg_set_sys_reg(void);
int dsim_reg_get_pkt_go_status(void);

#endif /* _DECON_FB_REGISTER_H */
