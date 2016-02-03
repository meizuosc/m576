/* drivers/video/exynos/decon_7580/panels/s6e3fa2_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include "s6e3fa2_gamma.h"
#include "s6e3fa2_param.h"
#include "lcd_ctrl.h"
#include "../dsim.h"
#include <video/mipi_display.h>

#define GAMMA_PARAM_SIZE 26

void lcd_init(int id, struct decon_lcd *lcd)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)SEQ_TEST_KEY_ON_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("failed to send SEQ_TEST_KEY_ON_F0 command.\n");

	dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, 0x11, 0);

	msleep(120);

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)SEQ_TEST_KEY_OFF_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("failed to send SEQ_TEST_KEY_OFF_F0 command.\n");


	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
			SEQ_TE_ON[0], 0) < 0)
		dsim_err("fail to send SEQ_TE_ON command.\n");
}

void lcd_enable(int id)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
			SEQ_DISPLAY_ON[0], 0) < 0)
		dsim_err("fail to send SEQ_DISPLAY_ON command.\n");
}

void lcd_disable(int id)
{
	/* This function needs to implement */
}

int lcd_gamma_ctrl(int id, u32 backlightlevel)
{
	int ret;

	ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)gamma22_table[backlightlevel],
			GAMMA_PARAM_SIZE);
	if (ret)
		dsim_err("failed to write gamma value.\n");

	return ret;
}

int lcd_gamma_update(int id)
{
	int ret;

	ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret)
		dsim_err("failed to update gamma value.\n");

	return ret;
}
