/*
 * es9018k2m.h  --  es9018k2m Soc Audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on es9018k2m.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES9018K2M_H
#define _ES9018K2M_H

#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <sound/soc.h>
#include <linux/mutex.h>

/* ES9018K2M register space */

#define ES9018K2M_SYSTEM_SETTING    			0x00
#define ES9018K2M_INPUT_CONFIG   			0x01
#define ES9018K2M_AUTOMUTE_TIME   			0x04
#define ES9018K2M_AUTOMUTE_LEVEL   			0x05
#define ES9018K2M_DEEMPHASIS    			0x06
#define ES9018K2M_GENERAL_SET   			0x07
#define ES9018K2M_GPIO_CONFIG      			0x08
#define ES9018K2M_W_MODE_CONTROL    			0x09
#define ES9018K2M_V_MODE_CONTROL    			0x0A
#define ES9018K2M_CHANNEL_MAP    			0x0B
#define ES9018K2M_DPLL   				0x0C
#define ES9018K2M_THD_COMPENSATION   			0x0D
#define ES9018K2M_SOFT_START	   			0x0E
#define ES9018K2M_VOLUME1	   			0x0F
#define ES9018K2M_VOLUME2	   			0x10
#define ES9018K2M_MASTERTRIM0	   			0x11
#define ES9018K2M_MASTERTRIM1	   			0x12
#define ES9018K2M_MASTERTRIM2	  			0x13
#define ES9018K2M_MASTERTRIM3	   			0x14
#define ES9018K2M_INPUT_SELECT	   			0x15
#define ES9018K2M_2_HARMONIC_COMPENSATION_0	    	0x16
#define ES9018K2M_2_HARMONIC_COMPENSATION_1	    	0x17
#define ES9018K2M_3_HARMONIC_COMPENSATION_0	    	0x18
#define ES9018K2M_3_HARMONIC_COMPENSATION_1	    	0x19
//#for V version
#define ES9018K2M_program_FIR_ADDR	      		0x1A
#define ES9018K2M_program_FIR_DATA1	   		0x1B
#define ES9018K2M_program_FIR_DATA2	   		0x1C
#define ES9018K2M_program_FIR_DATAC	   		0x1D
#define ES9018K2M_program_FIR_CONTROL	   		0x1E

#define ES9018K2M_CACHEREGNUM 	0x1E

#define ES9018K2M_SYSCLK_MCLK 1

struct es9018k2m_platform_data {
	int gpio_osc_48khz;
	int gpio_osc_44khz;
	int gpio_resetb;
	int gpio_mute;
	int gpio_op_en;
	int gpio_high_gain;
	int gpio_low_gain;
	int gpio_out_select;
	int gpio_dvdd_on;
};

enum es9018k2m_gain {
	ES9018K2M_GAIN_INVAL = -1,
	ES9018K2M_GAIN_CODEC = 0,
	ES9018K2M_GAIN_LOW,
	ES9018K2M_GAIN_HIGH,
	ES9018K2M_GAIN_IDLE,
};

enum es9018k2m_mute {
	ES9018K2M_UNMUTE = 0,
	ES9018K2M_MUTE = 1,
};

enum es9018k2m_output {
	ES9018K2M_OUTPUT_WM8998 = 0,
	ES9018K2M_OUTPUT_ES9018 = 1,
};


/* codec private data */
struct es9018k2m_priv {
	struct regmap *regmap;
	struct regulator *vdd18_dac;
	struct snd_soc_codec *codec;
	unsigned int sysclk;
	int playback_fs;
	bool master;
	bool deemph;
	bool version;
	int active;
	struct mutex access_mutex;
	struct mutex gain_mutex;
	int gain;
	int mute;
	int custom_fir_enable;
	int stage1[128];
	int stage2[16];
	struct blocking_notifier_head *es9018k2m_notifier_list;
	struct delayed_work op_en_work;
};

extern struct es9018k2m_priv *es9018k2m_priv;
extern void es9018k2m_blocking_notifier_call_chain(unsigned long val);

#endif

