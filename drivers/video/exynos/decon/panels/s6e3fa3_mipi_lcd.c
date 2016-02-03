/* linux/drivers/video/backlight/s6e3fa3_mipi_lcd.c
 *
 * exynos7420 SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 MEIZU
 *
 * WangBo, <wangbo@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>
#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "../dsim.h"

static int lcd_id[3] = {0};
static char *lcd_desc;
static char lcd_desc_data[10] = "unknow";
module_param_array(lcd_id, int, NULL, 0444);
module_param(lcd_desc, charp, 0444);

bool is_white_lcd(void)
{
	return ((lcd_id[2] & 0x03) ? true : false);
}

struct lcd_cmd {
	unsigned int type;
	unsigned char *data;
	unsigned int size;
	unsigned int delay;
};

#define INIT_CMD(cmd, type_v, delay_v) \
	static struct lcd_cmd cmd##_s = { \
		.type = type_v, \
		.data = cmd, \
		.size = ARRAY_SIZE(cmd), \
		.delay = delay_v, \
	}

#define WRITE_LCD(id, cmd) \
	do { \
		switch (cmd##_s.type) { \
		case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM: \
		case MIPI_DSI_DCS_SHORT_WRITE: \
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM: \
			if (dsim_wr_data(id, cmd##_s.type, cmd##_s.data[0], cmd##_s.data[1])) \
				pr_err("Error: failed to write %s command!\r\n", #cmd); \
			break; \
		default: \
			if (dsim_wr_data(id, cmd##_s.type, (unsigned long)cmd##_s.data, cmd##_s.size)) \
				pr_err("Error: failed to write %s command!\r\n", #cmd); \
			break; \
		}; \
		if (cmd##_s.delay) \
			usleep_range(cmd##_s.delay * 1000, cmd##_s.delay * 1000); \
	} while(0)

#define READ_LCD(dsim, addr, count, buf, ret) \
	do { \
		ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, addr, count, buf); \
	} while (0)

static unsigned char TEON[]	     = {0x35, 0x00};
static unsigned char SLEEP_OUT[]     = {0x11, 0x00};
static unsigned char DISP_ON[]	     = {0x29, 0x00};
static unsigned char DISP_OFF[]      = {0x28, 0x00};
static unsigned char SLEEP_IN[]      = {0x10, 0x00};

static unsigned char LEVEL3_UNLOCK[] = {0xFC, 0x5A, 0x5A};
static unsigned char GLOBAL_PARA[]   = {0xB0, 0x1E};
static unsigned char AVC_SETTING[]   = {0xFD, 0x94};
static unsigned char LEVEL3_LOCK[]   = {0xFC, 0xA5, 0xA5};

static unsigned char DIMMING_SPEED[] = {0x53, 0x20};
static unsigned char ACL_MODE[]      = {0x55, 0x00};
static unsigned char LUMINANCE[]     = {0x51, 0x00};

static unsigned char S_PASSWD_KEY_EN_0[]   = {0xF0, 0x5A, 0x5A};
static unsigned char J_MIC[]   = {0xF9, 0x12};
//static unsigned char LANE_C4[]   = {0xC4, 0x02};
static unsigned char S_PASSWD_KEY_EN_1[]   = {0xF0, 0xA5, 0xA5};

static unsigned char HBM_SWITCH[] = {0x53, 0x20};


INIT_CMD(TEON, 0x15, 20);
INIT_CMD(SLEEP_OUT, 0x05, 20);
INIT_CMD(DISP_ON, 0x05, 0);
INIT_CMD(DISP_OFF, 0x05, 10);
INIT_CMD(SLEEP_IN, 0x05, 150);

INIT_CMD(LEVEL3_UNLOCK, 0x29, 0);
INIT_CMD(GLOBAL_PARA, 0x15, 0);
INIT_CMD(AVC_SETTING, 0x15, 0);
INIT_CMD(LEVEL3_LOCK, 0x29, 0);

INIT_CMD(DIMMING_SPEED, 0x15, 0);
INIT_CMD(ACL_MODE, 0x15, 10);
INIT_CMD(LUMINANCE, 0x15, 80);

INIT_CMD(S_PASSWD_KEY_EN_0, 0x29, 0);
INIT_CMD(J_MIC, 0x15, 0);
//INIT_CMD(LANE_C4, 0x29, 0);
INIT_CMD(S_PASSWD_KEY_EN_1, 0x29, 0);

INIT_CMD(HBM_SWITCH, 0x15, 0);


#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEF_BRIGHTNESS 68

static struct backlight_device *bd;

static int s6e3fa3_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int update_brightness(struct dsim_device *dsim, int brightness)
{
	unsigned char brightness_60nit[2] = {
		0x51, DEF_BRIGHTNESS
	};

	if (dsim == NULL) {
		printk("dsim struct has been set to NULL,check it\n");
		BUG();
	}

#ifdef CONFIG_DECON_SIMULATE_DISPLAY
	if (dsim->isConnectLcd != 1)
		return 1;
#endif

	if (brightness < MIN_BRIGHTNESS) brightness = MIN_BRIGHTNESS;
	if (brightness > MAX_BRIGHTNESS) brightness = MAX_BRIGHTNESS;

	brightness_60nit[1] = brightness;
	dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long)brightness_60nit, 2);
	return 1;
}

static int s6e3fa3_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct dsim_device *dsim = dev_get_drvdata(&bd->dev);

#ifdef CONFIG_RECOVERY_KERNEL
	if (bd->isNotify == 1) {
		printk("[Info]: fb event[%d]!\r\n", bd->props.brightness);
		return 1;
	}
#endif

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		pr_info("Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(dsim, brightness);

	return 1;
}

static const struct backlight_ops s6e3fa3_backlight_ops = {
	.get_brightness = s6e3fa3_get_brightness,
	.update_status = s6e3fa3_set_brightness,
};

static int s6e3fa3_enterHBM(struct dsim_device *dsim, int onoff)
{
	switch (onoff) {
	case 0:
		HBM_SWITCH[1] = 0x20;
		break;
	case 1:
		HBM_SWITCH[1] = 0xE0;
		break;
	default:
		HBM_SWITCH[1] = 0x20;
		break;
	}

	WRITE_LCD(dsim->id, HBM_SWITCH);

	return 1;
}

static int s6e3fa3_probe(struct dsim_device *dsim)
{
	int i, ret = 0;
	int count = 3;
	u8 buf[4] = {0};
	u32 rd_addr = 0x04;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		dsim, &s6e3fa3_backlight_ops, NULL);
	if (IS_ERR(bd))
		pr_info("failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEF_BRIGHTNESS;
#ifdef CONFIG_RECOVERY_KERNEL
	bd->isNotify = 0;
#endif

#ifdef CONFIG_DECON_SIMULATE_DISPLAY
	dsim->isConnectLcd = 1;
#endif

	READ_LCD(dsim, rd_addr, count, buf, ret);
	if (ret < 0) {
		printk("[LCD] ERROR: failed to read lcd ID!\r\n");
#ifdef CONFIG_DECON_SIMULATE_DISPLAY
		dsim->isConnectLcd = 0;
#endif
		goto out;
	}

	for (i = 0; i < 3; i++)
		lcd_id[i] = (int)buf[i];

	if (is_white_lcd()) {
		memcpy (lcd_desc_data, "white", sizeof("white"));
	} else {
		memcpy (lcd_desc_data, "black", sizeof("black"));
	}
	lcd_desc = lcd_desc_data;

	printk("[LCD] INFO: succeed to communicate with lcd and it is %s [0x%x,0x%x,0x%x].\r\n", (is_white_lcd() ? "white lcd" : "black lcd"), lcd_id[0], lcd_id[1], lcd_id[2]);

out:
	return 1;
}

void lcd_init(int id, struct decon_lcd *lcd)
{
	WRITE_LCD(id, S_PASSWD_KEY_EN_0);
	//WRITE_LCD(id, LANE_C4);
	WRITE_LCD(id, SLEEP_OUT);
	WRITE_LCD(id, J_MIC);
	WRITE_LCD(id, S_PASSWD_KEY_EN_1);
	WRITE_LCD(id, TEON);
	WRITE_LCD(id, LEVEL3_UNLOCK);
	WRITE_LCD(id, GLOBAL_PARA);
	WRITE_LCD(id, AVC_SETTING);
	WRITE_LCD(id, LEVEL3_LOCK);
	WRITE_LCD(id, DIMMING_SPEED);
	WRITE_LCD(id, LUMINANCE);
	WRITE_LCD(id, ACL_MODE);
	WRITE_LCD(id, DISP_ON);
}

static int s6e3fa3_displayon(struct dsim_device *dsim)
{
#ifdef CONFIG_DECON_SIMULATE_DISPLAY
	if (dsim->isConnectLcd != 1)
		return 1;
#endif

	lcd_init(dsim->id, &dsim->lcd_info);
#ifndef CONFIG_RECOVERY_KERNEL
	update_brightness(dsim, DEF_BRIGHTNESS);
#endif
	return 1;
}

static int s6e3fa3_suspend(struct dsim_device *dsim)
{
#ifdef CONFIG_DECON_SIMULATE_DISPLAY
	if (dsim->isConnectLcd != 1)
		return 1;
#endif

	WRITE_LCD(dsim->id, DISP_OFF);
	WRITE_LCD(dsim->id, SLEEP_IN);

	return 1;
}

static int s6e3fa3_resume(struct dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6e3fa3_mipi_lcd_driver= {
	.probe		= s6e3fa3_probe,
	.displayon	= s6e3fa3_displayon,
	.suspend	= s6e3fa3_suspend,
	.resume		= s6e3fa3_resume,
	.enterHBM	= s6e3fa3_enterHBM,
};
