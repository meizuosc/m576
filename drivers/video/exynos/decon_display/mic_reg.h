/* linux/drivers/video/decon_display/mic_reg.h
 *
 * Header file for Samsung MIC lowlevel driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _MIC_REG_H
#define _MIC_REG_H

/* header files */
#include <linux/io.h>
#include <linux/delay.h>

#include "decon_mic.h"
#include "regs-mic.h"
#include "decon_display_driver.h"

/* enum and structure */

static inline struct decon_mic *get_mic_drvdata(void)
{
	struct display_driver *dispdrv = get_display_driver();

	return dispdrv->mic_driver.mic;
}

/* register access subroutines */
static inline u32 mic_read(u32 reg_id)
{
	struct decon_mic *mic = get_mic_drvdata();
	return readl(mic->reg_base + reg_id);
}

static inline u32 mic_read_mask(u32 reg_id, u32 mask)
{
	u32 val = mic_read(reg_id);
	val &= (~mask);
	return val;
}

static inline void mic_write(u32 reg_id, u32 val)
{
	struct decon_mic *mic = get_mic_drvdata();
	writel(val, mic->reg_base + reg_id);
}

static inline void mic_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct decon_mic *mic = get_mic_drvdata();
	u32 old = mic_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, mic->reg_base + reg_id);
}

#define mic_err(fmt, ...)					\
	do {							\
		dev_err(get_mic_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define mic_info(fmt, ...)					\
	do {							\
		dev_info(get_mic_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

#define mic_dbg(fmt, ...)					\
	do {							\
		dev_dbg(get_mic_drvdata()->dev, pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)

/* CAL APIs list */
int mic_reg_start(struct decon_lcd *lcd);
int mic_reg_stop(struct decon_lcd *lcd);

/* CAL raw functions list */
int mic_reg_sw_reset(void);
void mic_reg_set_image_size(struct decon_lcd *lcd);
void mic_reg_set_mic_base_operation(struct decon_lcd *lcd, bool enable);
void mic_reg_set_update(u32);
void mic_reg_set_porch_timing(struct decon_lcd *lcd);
void mic_reg_set_output_timing(struct decon_lcd *lcd);

void mic_reg_set_win_update_conf(u32 w, u32 h);
#endif /* _MIC_REG_H */
