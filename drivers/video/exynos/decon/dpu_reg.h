/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DPU_H__
#define __SAMSUNG_DPU_H__


#define DPU_VFP		4000
#define DPU_VSA		0
#define DPU_VBP		0
#define DPU_HFP		0
#define DPU_HSA		0
#define DPU_HBP		0

#include "decon.h"
#define DPU_BASE 0x4000
#define DPU_GAMMA_OFFSET	132

static inline u32 dpu_read(u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(0);
	return readl(decon->regs + DPU_BASE + reg_id);
}

static inline u32 dpu_read_mask(u32 reg_id, u32 mask)
{
	u32 val = dpu_read(reg_id);
	val &= (mask);
	return val;
}

static inline void dpu_write(u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(0);
	writel(val, decon->regs + DPU_BASE + reg_id);
}

static inline void dpu_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 old = dpu_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->regs + DPU_BASE + reg_id);
}

void dpu_reg_start(u32 w, u32 h);
void dpu_reg_stop(void);

#if defined(CONFIG_EXYNOS_DECON_DPU)
void dpu_reg_en_dither(u32 en);
void dpu_reg_set_ad_onoff(u32 val);
void dpu_reg_set_ad(u32 val);
void dpu_reg_set_ad_backlight(u32 val);
void dpu_reg_set_ad_ambient(u32 val);
void dpu_reg_set_scr_onoff(u32 val);
void dpu_reg_set_scr(u32 reg, u32 val);
void dpu_reg_set_control(u32 val);
void dpu_reg_set_saturation_onoff(u32 val);
void dpu_reg_set_bri_gain(u32 val);
void dpu_reg_set_saturation_tscgain(u32 val);
void dpu_reg_set_saturation_rgb(u32 mask, u32 val);
void dpu_reg_set_saturation_cmy(u32 mask, u32 val);
void dpu_reg_set_saturation_shift(u32 mask, u32 val);
void dpu_reg_set_contrast_onoff(u32 val);
void dpu_reg_set_contrast(u32 offset, u32 mask, u32 val);
void dpu_reg_set_hue_onoff(u32 val);
void dpu_reg_set_hue(u32 val);
void dpu_reg_set_hue_rgb(u32 mask, u32 val);
void dpu_reg_set_hue_cmy(u32 mask, u32 val);
void dpu_reg_set_bright(u32 val);
void dpu_reg_set_lut_fi(int *val, int count);
void dpu_reg_set_lut_cc(int *val, int count);
void dpu_reg_set_iridix_control(int val);
void dpu_reg_set_white_level(int val);
void dpu_reg_set_black_level(int val);
void dpu_reg_set_variance(int val);
void dpu_reg_set_limit_ampl(int val);
void dpu_reg_set_iridix_dither(int val);
void dpu_reg_set_slope_max(int val);
void dpu_reg_set_slope_min(int val);
void dpu_reg_set_dither_control(int val);
void dpu_reg_set_frame_width(int val);
void dpu_reg_set_frame_height(int val);
void dpu_reg_set_logo_top(int val);
void dpu_reg_set_logo_left(int val);
void dpu_reg_set_mode(int val);
void dpu_reg_set_al_calib_lut(int *val, int count);
void dpu_reg_set_backlight_min(int val);
void dpu_reg_set_backlight_max(int val);
void dpu_reg_set_backlight_scale(int val);
void dpu_reg_set_ambient_light_min(int val);
void dpu_reg_set_ambient_filter_a(int val);
void dpu_reg_set_ambient_filter_b(int val);
void dpu_reg_set_calibration_a(int val);
void dpu_reg_set_calibration_b(int val);
void dpu_reg_set_calibration_c(int val);
void dpu_reg_set_calibration_d(int val);
void dpu_reg_set_strength_limit(int val);
void dpu_reg_set_t_filter_control(int val);
ssize_t dpu_reg_get_backlight_out(void);
#endif

#endif
