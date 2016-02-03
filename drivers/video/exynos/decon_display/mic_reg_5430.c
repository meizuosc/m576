/* linux/drivers/video/decon_display/mic_reg_5430.c
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

#include "mic_reg.h"

/******************* CAL raw functions implementation *************************/
int mic_reg_sw_reset(void)
{
	int tries;

	mic_write_mask(MIC_OP, ~0, MIC_OP_SW_RST);

	for (tries = 100; tries; --tries) {
		if (~(mic_read(MIC_OP) & MIC_OP_SW_RST))
			break;
		udelay(1);
	}

	if (!tries) {
		mic_err("failed to reset MIC\n");
		return -EBUSY;
	}

	return 0;
}

void mic_reg_set_image_size(struct decon_lcd *lcd)
{
	u32 val;

	val = MIC_IMG_V_SIZE(lcd->yres) | MIC_IMG_H_SIZE(lcd->xres);
	mic_write(MIC_IMG_SIZE, val);
}

void mic_reg_set_mic_base_operation(struct decon_lcd *lcd, bool enable)
{
	u32 data = mic_read(MIC_OP);

	if (enable) {
		data &= ~(MIC_OP_CORE_ENABLE | MIC_OP_COMMAND_MODE |
			MIC_OP_OLD_CORE | MIC_OP_PSR_EN |
			MIC_OP_BS_CHG_OUT | MIC_OP_ON_REG |
			MIC_OP_UPDATE_REG);
		data |= MIC_OP_CORE_ENABLE | MIC_OP_NEW_CORE |
			MIC_OP_UPDATE_REG | MIC_OP_ON_REG;

		if(lcd->mic_bitstream_swap)
			data |= MIC_OP_BS_CHG_OUT;

		if (lcd->mode == COMMAND_MODE)
			data |= MIC_OP_COMMAND_MODE;
		else
			data |= MIC_OP_VIDEO_MODE;
	} else {
		data &= ~MIC_OP_CORE_ENABLE;
		data |= MIC_OP_UPDATE_REG;
	}

	mic_write(MIC_OP, data);
}

void mic_reg_set_update(u32 en)
{
	u32 val = en ? ~0 : 0;
	mic_write_mask(MIC_OP, val, MIC_OP_UPDATE_REG);
}

void mic_reg_set_porch_timing(struct decon_lcd *lcd)
{
	u32 data, v_period, h_period;

	if(lcd->mode != COMMAND_MODE) {
		v_period = lcd->vsa + lcd->yres + lcd->vbp + lcd->vfp;
		data = MIC_V_TIMING_0_V_PULSE_WIDTH(lcd->vsa) |
			MIC_V_TIMING_0_V_PERIOD_LINE(v_period);
		mic_write(MIC_V_TIMING_0, data);

		data = MIC_V_TIMING_1_VBP_SIZE(lcd->vbp) |
			MIC_V_TIMING_1_VFP_SIZE(lcd->vfp);
		mic_write(MIC_V_TIMING_1, data);

		h_period = lcd->hsa + lcd->xres + lcd->hbp + lcd->hfp;
		data = MIC_INPUT_TIMING_0_H_PULSE_WIDTH(lcd->hsa) |
			MIC_INPUT_TIMING_0_H_PERIOD_PIXEL(h_period);
		mic_write(MIC_INPUT_TIMING_0, data);

		data = MIC_INPUT_TIMING_1_HBP_SIZE(lcd->hbp) |
			MIC_INPUT_TIMING_1_HFP_SIZE(lcd->hfp);
		mic_write(MIC_INPUT_TIMING_1, data);
	}
}

void mic_reg_set_output_timing(struct decon_lcd *lcd)
{
	u32 data, h_period_2d;
	u32 bs_2d = (lcd->xres >> 1) + (lcd->xres % 4);
	u32 hsa_2d = lcd->hsa;
	u32 hbp_2d = lcd->hbp;
	u32 hfp_2d = lcd->hfp + bs_2d;

	/* is it correct? plus bs_2d twice */
	h_period_2d = hsa_2d + hbp_2d + bs_2d + hfp_2d;

	if(lcd->mode != COMMAND_MODE) {
		data = MIC_2D_OUTPUT_TIMING_0_H_PULSE_WIDTH_2D(hsa_2d) |
			MIC_2D_OUTPUT_TIMING_0_H_PERIOD_PIXEL_2D(h_period_2d);
		mic_write(MIC_2D_OUTPUT_TIMING_0, data);

		data = MIC_2D_OUTPUT_TIMING_1_HBP_SIZE_2D(hbp_2d) |
			MIC_2D_OUTPUT_TIMING_1_HFP_SIZE_2D(hfp_2d);
		mic_write(MIC_2D_OUTPUT_TIMING_1, data);
	}

	mic_write(MIC_2D_OUTPUT_TIMING_2, bs_2d);
}

void mic_reg_set_win_update_conf(u32 w, u32 h)
{
	u32 data;
	u32 bs_2d = ((w >> 2) << 1) + (w & 0x3);

	mic_reg_set_update(0);
	/* Image Size */
	data = MIC_IMG_V_SIZE(h) | MIC_IMG_H_SIZE(w);
	mic_write(MIC_IMG_SIZE, data);

	/* 2d_bit_stream_size */
	mic_write(MIC_2D_OUTPUT_TIMING_2, bs_2d);
	mic_reg_set_update(1);

	/* TODO: porch settings */
}

/***************** CAL APIs implementation *******************/
int mic_reg_start(struct decon_lcd *lcd)
{
	mic_reg_sw_reset();
	mic_reg_set_porch_timing(lcd);
	mic_reg_set_image_size(lcd);
	mic_reg_set_output_timing(lcd);
	mic_reg_set_mic_base_operation(lcd, 1);

	return 0;
}

int mic_reg_stop(struct decon_lcd *lcd)
{
	mic_reg_set_mic_base_operation(lcd, 0);

	return 0;
}
