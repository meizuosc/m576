/* linux/drivers/video/exynos/decon/dpu_reg_7420.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Haowei Li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "dpu_reg.h"
#include "regs-dpu.h"

void dpu_reg_update_mask(u32 mask)
{
	dpu_write_mask(DPU_MASK_CTRL, mask, DPU_MASK_CTRL_MASK);
}

void dpu_reg_en_dither(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_DITHER_ON, DPU_DITHER_ON_MASK);
}

void dpu_reg_en_scr(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_SCR_ON, DPU_SCR_ON_MASK);
}

void dpu_reg_en_gamma(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_GAMMA_ON, DPU_GAMMA_ON_MASK);
}

void dpu_reg_en_hsc(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_HSC_ON, DPU_HSC_ON_MASK);
}

static inline bool dpu_reg_need_dis_hsc(u32 dis_val)
{
	u32 val;

	val = dpu_read(DPUHSC_CONTROL);
	return (((val & ~(dis_val) & 0x3f) == 0) ? true : false);
}

void dpu_reg_en_ad(u32 en)
{
	dpu_write_mask(DPUCON, en << DPU_AD_ON, DPU_AD_ON_MASK);
}

void dpu_reg_set_image_size(u32 width, u32 height)
{
	u32 data;

	data = (height << 16) | (width);
	dpu_write(DPUIMG_SIZESET, data);
}

void dpu_reg_module_on_off(bool en1, bool en2, bool en3, bool en4, bool en5)
{
	dpu_reg_update_mask(1);

	dpu_reg_en_dither(en1);
	dpu_reg_en_scr(en2);
	dpu_reg_en_gamma(en3);
	dpu_reg_en_hsc(en4);
	dpu_reg_en_ad(en5);

	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_reg_en_scr(val);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_scr(u32 reg, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write(reg, val);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_bright(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_BACKLIGHT, val, DPUAD_BACKLIGHT_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_contrast_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_reg_en_gamma(val);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_contrast(u32 offset, u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask((DPUGAMMALUT_X_Y_BASE + offset), val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_control(u32 val)
{
	dpu_reg_update_mask(1);
	if ((val & 0x3f) != 0)
		dpu_reg_en_hsc(1);
	else
		dpu_reg_en_hsc(0);
	dpu_write(DPUHSC_CONTROL, val);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	if (val == 0 && dpu_reg_need_dis_hsc((0x1 << 3) | (0x1 << 5)))
		dpu_reg_en_hsc(val);
	else if (val == 1)
		dpu_reg_en_hsc(val);
	dpu_write_mask(DPUHSC_CONTROL, (val << 3), TSC_ON);
	dpu_write_mask(DPUHSC_CONTROL, (val << 5), PAIM_ON);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_bri_gain(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_TSCGAIN_YRATIO, val, YCOM_RATIO);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_tscgain(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_TSCGAIN_YRATIO, val, TSC_GAIN);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_rgb(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PAIMGAIN2_1_0, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_cmy(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PAIMGAIN5_4_3, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_saturation_shift(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PAIMSCALE_SHIFT, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_hue_rgb(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PPHCGAIN2_1_0, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_hue_cmy(u32 mask, u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUHSC_PPHCGAIN5_4_3, val, mask);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_hue_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	if (val == 0 && dpu_reg_need_dis_hsc(0x1 << 1))
		dpu_reg_en_hsc(val);
	else if (val == 1)
		dpu_reg_en_hsc(val);
	dpu_write_mask(DPUHSC_CONTROL, val << 1, PPHC_ON);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_ad_onoff(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_reg_en_ad(val);
	dpu_write_mask(DPUAD_NVP_CONTROL2, DPU_AD_OPT_SEL_FUNC(0x1), OPTION_SELECT_FUNCTION_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_ad_backlight(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_BACKLIGHT, val, DPUAD_BACKLIGHT_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_ad_ambient(u32 val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_AMBIENT_LIGHT, val, DPUAD_AMBIENT_LIGHT_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_lut_fi(int *val, int count)
{
	int i = 0;
	dpu_reg_update_mask(1);
	for (i = 0; i < count; i++){
		dpu_write(DPUAD_LUT_FI(i), val[i]);
	}
	dpu_reg_update_mask(0);
}

void dpu_reg_set_lut_cc(int *val, int count)
{
	int i = 0;
	dpu_reg_update_mask(1);
	for (i = 0; i < count; i++){
		dpu_write(DPUAD_LUT_CC(i), val[i]);
	}
	dpu_reg_update_mask(0);
}

void dpu_reg_set_iridix_control(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL0, DPUAD_VP_IRIDIX_CONTROL0(val), DPUAD_VP_IRIDIX_CONTROL0_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_white_level(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL1, DPUAD_VP_CONTROL1_WHITE_LEVEL(val), DPUAD_VP_CONTROL1_WHITE_LEVEL_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_black_level(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL1, DPUAD_VP_CONTROL1_BLACK_LEVEL(val), DPUAD_VP_CONTROL1_BLACK_LEVEL_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_variance(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL0, DPUAD_VP_CONTROL0_VARIANCE(val), DPUAD_VP_CONTROL0_VARIANCE_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_limit_ampl(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL0, DPUAD_VP_CONTROL0_LIMIT_AMPL(val), DPUAD_VP_CONTROL0_LIMIT_AMPL_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_iridix_dither(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL3, DPUAD_VP_CONTROL3_IRIDIX_DITHER(val), DPUAD_VP_CONTROL3_IRIDIX_DITHER_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_slope_max(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL2, DPUAD_VP_CONTROL2_SLOPE_MAX(val), DPUAD_VP_CONTROL2_SLOPE_MAX_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_slope_min(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL2, DPUAD_VP_CONTROL2_SLOPE_MIN(val), DPUAD_VP_CONTROL2_SLOPE_MIN_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_dither_control(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL2, DPUAD_VP_CONTROL2_SLOPE_MIN(val), DPUAD_VP_CONTROL2_SLOPE_MIN_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_frame_width(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUIMG_SIZESET, FRAME_WIDTH(val), FRAME_WIDTH_MASK);
	dpu_reg_update_mask(0);
}
void dpu_reg_set_frame_height(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUIMG_SIZESET, FRAME_HEIGHT(val), FRAME_HEIGHT_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_logo_top(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL3, DPUAD_VP_CONTROL3_LOGO_TOP(val), DPUAD_VP_CONTROL3_LOGO_TOP_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_logo_left(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_VP_CONTROL3, DPUAD_VP_CONTROL3_LOGO_LEFT(val), DPUAD_VP_CONTROL3_LOGO_LEFT_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_mode(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL2, OPTION_SELECT(val), OPTION_SELECT_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_al_calib_lut(int *val, int count)
{
	int i = 0;
	dpu_reg_update_mask(1);
	for (i = 0; i < count; i++){
		dpu_write(DPUAD_AL_CALIB_LUT(i), val[i]);
	}
	dpu_reg_update_mask(0);
}

void dpu_reg_set_backlight_min(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL3, DPUAD_NVP_CONTROL3_BACKLIGHT_MIN(val), DPUAD_NVP_CONTROL3_BACKLIGHT_MIN_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_backlight_max(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL3, DPUAD_NVP_CONTROL3_BACKLIGHT_MAX(val), DPUAD_NVP_CONTROL3_BACKLIGHT_MAX_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_backlight_scale(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL4, DPUAD_NVP_CONTROL4_BACKLIGHT_SCALE(val), DPUAD_NVP_CONTROL4_BACKLIGHT_SCALE_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_ambient_light_min(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL4, DPUAD_NVP_CONTROL4_AMBIENT_LIGHT_MIN(val), DPUAD_NVP_CONTROL4_AMBIENT_LIGHT_MIN_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_ambient_filter_a(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL5, DPUAD_NVP_CONTROL5_FILTER_A(val), DPUAD_NVP_CONTROL5_FILTER_A_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_ambient_filter_b(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL5, DPUAD_NVP_CONTROL5_FILTER_B(val), DPUAD_NVP_CONTROL5_FILTER_B_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_calibration_a(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL0, DPUAD_NVP_CONTROL0_CALIB_A(val), DPUAD_NVP_CONTROL0_CALIB_A_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_calibration_b(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL0, DPUAD_NVP_CONTROL0_CALIB_B(val), DPUAD_NVP_CONTROL0_CALIB_B_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_calibration_c(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL1, DPUAD_NVP_CONTROL1_CALIB_C(val), DPUAD_NVP_CONTROL1_CALIB_C_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_calibration_d(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL1, DPUAD_NVP_CONTROL1_CALIB_D(val), DPUAD_NVP_CONTROL1_CALIB_D_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_strength_limit(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL2, DPUAD_NVP_CONTROL2_STRENGHT_LIMIT(val), DPUAD_NVP_CONTROL2_STRENGHT_LIMIT_MASK);
	dpu_reg_update_mask(0);
}

void dpu_reg_set_t_filter_control(int val)
{
	dpu_reg_update_mask(1);
	dpu_write_mask(DPUAD_NVP_CONTROL2, DPUAD_NVP_CONTROL2_T_FILTER_CONTROL(val), DPUAD_NVP_CONTROL2_T_FILTER_CONTROL_MASK);
	dpu_reg_update_mask(0);
}

ssize_t dpu_reg_get_backlight_out(void)
{
	ssize_t ret = 0;
	dpu_reg_update_mask(1);
	ret = dpu_read_mask(DPUAD_BACKLIGHT_OUT,  DPUAD_BACKLIGHT_OUT_MASK);
	dpu_reg_update_mask(0);

	return ret;
}
void dpu_reg_start(u32 w, u32 h)
{
	u32 id = 0;

	decon_reg_enable_apb_clk(id, 1);

	dpu_reg_module_on_off(0, 0, 0, 0, 0);

	dpu_reg_set_image_size(w, h);
	decon_reg_set_pixel_count_se(id, w, h);
	decon_reg_set_image_size_se(id, w, h);
	decon_reg_set_porch_se(id, DPU_VFP, DPU_VSA, DPU_VBP,
				DPU_HFP, DPU_HSA, DPU_HBP);
	decon_reg_set_bit_order_se(id, DPU_R1G1B1R0G0B0, DPU_R0G0B0R1G1B1);

	decon_reg_enable_dpu(id, 1);
}

void dpu_reg_stop(void)
{
	u32 id = 0;

	decon_reg_enable_dpu(id, 0);
	decon_reg_enable_apb_clk(id, 0);
}

