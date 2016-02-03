/* linux/drivers/video/backlight/s6e8fa0x01_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>
#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "../dsim.h"

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0

static struct backlight_device *bd;
#if 0
int s5p_mipi_dsi_rd_data(struct mipi_dsim_device *dsim, u32 data_id,
	 u32 addr, u32 count, u8 *buf);
static int s6e8fa0x01_read(struct mipi_dsim_device *dsim, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;

	if (len > 2)
		cmd = MIPI_DSI_DCS_READ;
	else if (len == 2)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
	else {
		ret = -EINVAL;
		goto read_err;
	}

	retry = 5;
read_data:
	if (!retry) {
		printk("%s failed: exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(dsim, cmd, addr, len, buf);
	if (ret != 0) {
		printk("mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	return ret;
}
#endif
static int s6e8fa0x01_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}
static int update_brightness(struct dsim_device *dsim, int brightness)
{
	unsigned char brightness_60nit[2] = {
		0x51, 0x39
	};

	if (dsim == NULL) {
		printk("dsim struct has been set to NULL,check it\n");
		BUG();
	}

	if (brightness < MIN_BRIGHTNESS) brightness = MIN_BRIGHTNESS;
	if (brightness > MAX_BRIGHTNESS) brightness = MAX_BRIGHTNESS;

	brightness_60nit[1] = brightness;
	dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)brightness_60nit, 2);
	return 1;
}
static int s6e8fa0x01_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct dsim_device *dsim = dev_get_drvdata(&bd->dev);

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		pr_info("Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(dsim, brightness);

	return 1;
}

static const struct backlight_ops s6e8fa0x01_backlight_ops = {
	.get_brightness = s6e8fa0x01_get_brightness,
	.update_status = s6e8fa0x01_set_brightness,
};

static int s6e8fa0x01_probe(struct dsim_device *dsim)
{

	bd = backlight_device_register("pwm-backlight.0", NULL,
		dsim, &s6e8fa0x01_backlight_ops, NULL);
	if (IS_ERR(bd))
		pr_info("failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = MAX_BRIGHTNESS;
	
	return 1;
}

void lcd_init(int id, struct decon_lcd *lcd)
{
	printk("lcd_init\n");
	dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)0x11, 0);
	msleep(20);				
	dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)0x53, 0x28);
	msleep(5);
	dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)0x51, 0xff);
	msleep(10);					
	dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)0x29, 0);
	msleep(20);
}

static int s6e8fa0x01_displayon(struct dsim_device *dsim)
{
	lcd_init(dsim->id, &dsim->lcd_info);
	return 1;
}

static int s6e8fa0x01_suspend(struct dsim_device *dsim)
{
	dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)0x28, 0);
	dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)0x10, 0);
	return 1;
}

static int s6e8fa0x01_resume(struct dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6e8fa0x01_mipi_lcd_driver= {
	.probe		= s6e8fa0x01_probe,
	.displayon	= s6e8fa0x01_displayon,
	.suspend	= s6e8fa0x01_suspend,
	.resume		= s6e8fa0x01_resume,
};
