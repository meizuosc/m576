/* drivers/video/decon_display/s6e3ha0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
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
#include "s6e3ha0_gamma.h"
#include "decon_display_driver.h"

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

struct decon_lcd s6e3ha0_lcd_info = {
#ifdef CONFIG_FB_I80_COMMAND_MODE
	.mode = COMMAND_MODE,
	.vfp = 1,
	.vbp = 15,
	.hfp = 1,
	.hbp = 1,

	.vsa = 1,
	.hsa = 1,

	.xres = 1440,
	.yres = 2560,
#else
	.mode = VIDEO_MODE,
	.vfp = 1,
	.vbp = 15,
	.hfp = 20,
	.hbp = 20,

	.vsa = 1,
	.hsa = 20,

	.xres = 1440,
	.yres = 2560,
#endif

	.width = 70,
	.height = 121,

	/* Mhz */
	.hs_clk = 1100,
	.esc_clk = 20,

	.fps = 60,
};

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;

static u8 F0[] = {0xF0, 0x5A, 0x5A};

static u8 F1[] = {0xF1, 0x5A, 0x5A};

static u8 FC[] = {0xFC, 0x5A, 0x5A};

static u8 F9[] = {0xF9, 0x29};

static u8 B6[] = {0xB6, 0x88, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x55,
	0x54, 0x20, 0x00, 0x06, 0x66, 0x6C, 0x0C}; 

static u8 B8[] = {0xB8, 0x00, 0x00, 0x40, 0x08, 0xA8, 0x00};

static u8 C7[] = {0xC7, 0x06};

static u8 F6[] = {0xF6, 0x47, 0x0D, 0x17, 0x01, 0xAA, 0x01, 0x3E};

/* Temporarily Removed
static u8 F4[] = {0xF4, 0x7B, 0x8D, 0x17, 0x9A, 0x0D, 0x8C, 0x01, 0x05, 0x00};
*/
static u8 CB[] = {0xCB, 0x08, 0x41, 0x01, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00,
	0xD2, 0x04, 0x00, 0xD2, 0x01, 0x00, 0x00, 0x41, 0x8F, 0x14,
	0x8F, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0xC0,
	0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0xE1,
	0xE0, 0x63, 0x02, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x0F, 0x0E, 0x04,
	0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00};

static u8 E7[] = {0xE7, 0xED, 0xC7, 0x23, 0x57, 0xa5, 0x40, 0xED, 0xC7, 0x23,
	0x57, 0x20, 0x40};

static u8 CA[] = {0xCA, 0x01, 0x24, 0x01, 0x05, 0x01, 0x53, 0xDA, 0xDB, 0xDA,
	0xD6, 0xD7, 0xD6, 0xBF, 0xC0, 0xC0, 0xC4, 0xC6, 0xC6, 0xD7,
	0xD7, 0xD9, 0xCF, 0xD4, 0xDC, 0xBA, 0xC3, 0xC3, 0x7D, 0xBA,
	0xB3, 0x02, 0x03, 0x02};

static u8 F2[] = {0xF2, 0x07, 0x00, 0x01, 0xA4, 0x03, 0x0d, 0xA0};

static u8 E8[] = {0xE8, 0xBF, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xA4, 0x08, 0x60,
	0x00, 0x00, 0x00, 0x00, 0x00};

static u8 FD[] = {0xFD, 0x90, 0x16, 0x05, 0x0A, 0x08, 0x00, 0x00, 0x01, 0xFC,
	0x00, 0x00, 0x00, 0x0C, 0x07, 0x03, 0x00, 0x0F, 0x08, 0x0A, 0x80, 0x5D,
	0x55, 0x55, 0xA5, 0xA0, 0x55, 0x00, 0x49, 0x16, 0x0A};

static u8 D0[] = {0xD0, 0x08};


struct decon_lcd * decon_get_lcd_info()
{
	return &s6e3ha0_lcd_info;
}
EXPORT_SYMBOL(decon_get_lcd_info);


static int s6e3ha0_get_brightness(struct backlight_device *bd)
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

static int s6e3ha0_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 0;
}

static const struct backlight_ops s6e3ha0_backlight_ops = {
	.get_brightness = s6e3ha0_get_brightness,
	.update_status = s6e3ha0_set_brightness,
};

static int s6e3ha0_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &s6e3ha0_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 0;
}

int init_lcd(struct mipi_dsim_device *dsim)
{
	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)F0,
				ARRAY_SIZE(F0)) == -1)
		printk(KERN_ERR "fail to write F0 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE,
			0x11, 0x0) == -1)
		printk(KERN_ERR "fail to write Exit_sleep init command.\n");

	msleep(120);

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)F1,
				ARRAY_SIZE(F1)) == -1)
		printk(KERN_ERR "fail to write F1 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)FC,
				ARRAY_SIZE(FC)) == -1)
		printk(KERN_ERR "fail to write FC init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)B6,
				ARRAY_SIZE(B6)) == -1)
		printk(KERN_ERR "fail to write B6 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)B8,
				ARRAY_SIZE(B8)) == -1)
		printk(KERN_ERR "fail to write B8 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)C7[0], (unsigned int)C7[1]) == -1)
		printk(KERN_ERR "fail to write C7 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)F9[0], (unsigned int)F9[1]) == -1)
		printk(KERN_ERR "fail to write F9 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)F6,
				ARRAY_SIZE(F6)) == -1)
		printk(KERN_ERR "fail to write F6 init command.\n");
	/* Temporarily Removed
	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)F4,
				ARRAY_SIZE(F4)) == -1)
		printk(KERN_ERR "fail to write F4 init command.\n");
	*/
	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)CB,
				ARRAY_SIZE(CB)) == -1)
		printk(KERN_ERR "fail to write CB init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)E7,
				ARRAY_SIZE(E7)) == -1)
		printk(KERN_ERR "fail to write E7 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)CA,
				ARRAY_SIZE(CA)) == -1)
		printk(KERN_ERR "fail to write CA init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)F2,
				ARRAY_SIZE(F2)) == -1)
		printk(KERN_ERR "fail to write F2 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)E8,
				ARRAY_SIZE(E8)) == -1)
		printk(KERN_ERR "fail to write E8 init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)FD,
				ARRAY_SIZE(FD)) == -1)
		printk(KERN_ERR "fail to write FD init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			0x35, 0x0) == -1)
		printk(KERN_ERR "fail to write TE_on init command.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			(unsigned int)D0[0], (unsigned int)D0[1]) == -1)
		printk(KERN_ERR "fail to write D0 init command.\n");
	/* This command has been moved to DECON driver
	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_SHORT_WRITE,
			0x29, 0x0) == -1)
		printk(KERN_ERR "fail to write Disp_on init command.\n");
	msleep(150);
	*/

	return 0;
}

static int s6e3ha0_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 0;
}

static int s6e3ha0_suspend(struct mipi_dsim_device *dsim)
{
	return 0;
}

static int s6e3ha0_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3ha0_mipi_lcd_driver = {
	.probe		= s6e3ha0_probe,
	.displayon	= s6e3ha0_displayon,
	.suspend	= s6e3ha0_suspend,
	.resume		= s6e3ha0_resume,
};
