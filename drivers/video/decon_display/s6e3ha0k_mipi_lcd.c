/* drivers/video/decon_display/s6e3ha0k_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2014 Samsung Electronics
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

#include "decon_mipi_dsi.h"
#include "s6e3ha0k_gamma.h"
#include "decon_display_driver.h"

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;

static u8 SEQ_TEST_KEY_ON_F0[] = {0xF0, 0x5A, 0x5A};
static u8 SEQ_MIPI_SINGLE_DSI_SET1[] = {0xF2, 0x07, 0x00, 0x01, 0xA4, 0x03, 0x0d, 0xA0};
static u8 SEQ_MIPI_SINGLE_DSI_SET2[] = {0xF9, 0x29};
static u8 SEQ_TEST_KEY_ON_FC[] = {0xFC, 0x5A, 0x5A};
static u8 SEQ_REG_FF[] = {0xFF, 0x00, 0x00, 0x20, 0x00};
static u8 SEQ_TEST_KEY_OFF_FC[] = {0xFC, 0xA5, 0xA5};
//static u8 SEQ_SLEEP_OUT[] = {0x11};
static u8 SEQ_CAPS_ELVSS_SET[] = {0xB6,
	0x98, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x55, 0x54,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x22, 0x22, 0x10};
//static u8 SEQ_TE_ON[] = {0x35};
static u8 SEQ_TOUCH_HSYNC_ON[] = {0xBD, 0x05};
static u8 SEQ_TOUCH_VSYNC_ON[] = {0xFF, 0x02};
static u8 SEQ_PENTILE_CONTROL[] = {0xC0, 0x74, 0x00, 0xD8, 0xD8};
static u8 SEQ_TEST_KEY_OFF_F0[] = {0xF0, 0xA5, 0xA5};
//static u8 SEQ_DISP_ON[] = {0x29};

static int s6e3ha0k_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int get_backlight_level(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0:
		backlightlevel = 0;
		break;
	case 1 ... 29:
		backlightlevel = 0;
		break;
	case 30 ... 34:
		backlightlevel = 1;
		break;
	case 35 ... 39:
		backlightlevel = 2;
		break;
	case 40 ... 44:
		backlightlevel = 3;
		break;
	case 45 ... 49:
		backlightlevel = 4;
		break;
	case 50 ... 54:
		backlightlevel = 5;
		break;
	case 55 ... 64:
		backlightlevel = 6;
		break;
	case 65 ... 74:
		backlightlevel = 7;
		break;
	case 75 ... 83:
		backlightlevel = 8;
		break;
	case 84 ... 93:
		backlightlevel = 9;
		break;
	case 94 ... 103:
		backlightlevel = 10;
		break;
	case 104 ... 113:
		backlightlevel = 11;
		break;
	case 114 ... 122:
		backlightlevel = 12;
		break;
	case 123 ... 132:
		backlightlevel = 13;
		break;
	case 133 ... 142:
		backlightlevel = 14;
		break;
	case 143 ... 152:
		backlightlevel = 15;
		break;
	case 153 ... 162:
		backlightlevel = 16;
		break;
	case 163 ... 171:
		backlightlevel = 17;
		break;
	case 172 ... 181:
		backlightlevel = 18;
		break;
	case 182 ... 191:
		backlightlevel = 19;
		break;
	case 192 ... 201:
		backlightlevel = 20;
		break;
	case 202 ... 210:
		backlightlevel = 21;
		break;
	case 211 ... 220:
		backlightlevel = 22;
		break;
	case 221 ... 230:
		backlightlevel = 23;
		break;
	case 231 ... 240:
		backlightlevel = 24;
		break;
	case 241 ... 250:
		backlightlevel = 25;
		break;
	case 251 ... 255:
		backlightlevel = 26;
		break;
	default:
		backlightlevel = 12;
		break;
	}

	return backlightlevel;
}

static int update_brightness(int brightness)
{
	int backlightlevel;

	backlightlevel = get_backlight_level(brightness);
	/* Need to implement
	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)gamma22_table[backlightlevel],
				GAMMA_PARAM_SIZE) == -1)
		printk(KERN_ERR "fail to write gamma value.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)0xF7, (unsigned int)0x03) == -1)
		printk(KERN_ERR "fail to update gamma value.\n");
	*/
	return 0;
}

static int s6e3ha0k_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 0;
}

static const struct backlight_ops s6e3ha0k_backlight_ops = {
	.get_brightness = s6e3ha0k_get_brightness,
	.update_status = s6e3ha0k_set_brightness,
};

static int s6e3ha0k_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &s6e3ha0k_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 0;
}

int init_lcd(struct mipi_dsim_device *dsim)
{
	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) == -1)
		printk(KERN_ERR "fail to write F0 init command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_MIPI_SINGLE_DSI_SET1,
				ARRAY_SIZE(SEQ_MIPI_SINGLE_DSI_SET1)) == -1)
		printk(KERN_ERR "fail to write DSI_SET1 command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)SEQ_MIPI_SINGLE_DSI_SET2[0],
			(unsigned int)SEQ_MIPI_SINGLE_DSI_SET2[1]) == -1)
		printk(KERN_ERR "fail to write DSI_SET2 command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) == -1)
		printk(KERN_ERR "fail to write FC init command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_REG_FF,
				ARRAY_SIZE(SEQ_REG_FF)) == -1)
		printk(KERN_ERR "fail to write SEQ_REG_FF command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_OFF_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) == -1)
		printk(KERN_ERR "fail to write FC OFF command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE,
			0x11, 0x0) == -1)
		printk(KERN_ERR "fail to write Exit_sleep init command.\n");

	msleep(100);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_CAPS_ELVSS_SET,
				ARRAY_SIZE(SEQ_CAPS_ELVSS_SET)) == -1)
		printk(KERN_ERR "fail to write SEQ_CAPS_ELVSS_SET command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_OFF_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) == -1)
		printk(KERN_ERR "fail to write KEY_OFF_F0 command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			0x35, 0x0) == -1)
		printk(KERN_ERR "fail to write TE_on init command.\n");
	msleep(50);
	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) == -1)
		printk(KERN_ERR "fail to write KEY_OFF_F0 command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) == -1)
		printk(KERN_ERR "fail to write FC init command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)SEQ_TOUCH_HSYNC_ON[0],
			(unsigned int)SEQ_TOUCH_HSYNC_ON[1]) == -1)
		printk(KERN_ERR "fail to write SEQ_TOUCH_HSYNC_ON command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)SEQ_TOUCH_VSYNC_ON[0],
			(unsigned int)SEQ_TOUCH_VSYNC_ON[1]) == -1)
		printk(KERN_ERR "fail to write SEQ_TOUCH_VSYNC_ON command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_OFF_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) == -1)
		printk(KERN_ERR "fail to write KEY_OFF_FC command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_PENTILE_CONTROL,
				ARRAY_SIZE(SEQ_PENTILE_CONTROL)) == -1)
		printk(KERN_ERR "fail to write SEQ_PENTILE_CONTROL command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_OFF_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) == -1)
		printk(KERN_ERR "fail to write KEY_OFF_F0 command.\n");
	msleep(50);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE,
			0x29, 0x0) == -1)
		printk(KERN_ERR "fail to write Disp_on init command.\n");

	return 0;
}

static int s6e3ha0k_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 0;
}

static int s6e3ha0k_suspend(struct mipi_dsim_device *dsim)
{
	return 0;
}

static int s6e3ha0k_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3ha0k_mipi_lcd_driver = {
	.probe		= s6e3ha0k_probe,
	.displayon	= s6e3ha0k_displayon,
	.suspend	= s6e3ha0k_suspend,
	.resume		= s6e3ha0k_resume,
};
