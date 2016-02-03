/*
 * OIS IC driver for MEIZU M86.
 * Author: QuDao, qudao@meizu.com
 *
 * Copyright (C) 2015 MEIZU
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __H_OIS_MZ_SPECIAL_H__
#define __H_OIS_MZ_SPECIAL_H__

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

#include "fimc-is-device-eeprom.h"

#define OIS_NAME "rohm 63165"
#define MZ_OIS_MAX_ADDR_LEN 4
#define OTP_OIS_SIZE (0x2a)
#define SENSOR_SLAV_ADDR (0x34 >> 1)

struct ois_meizu {
	struct i2c_client *client;
	spinlock_t slock;
	int gpio_vcm_en;
	bool poweron;
	bool fadj_ready;
};

struct mz_ois_i2c_data {
	/* 0: read; 1: write */
	bool rw;
	/* slave addr */
	unsigned short slavaddr;
	/* max addr space inside slave is 4 bytes */
	const u8 *addr_buf;
	/* length of addr inside slave */
	u16 addr_len;

	/* buf && length of data read/write */
	u8 *data_buf;
	u16 data_len;
};

int mz_ois_i2c_read(struct mz_ois_i2c_data * const mz_i2c_data);
int mz_ois_i2c_write(struct mz_ois_i2c_data *const mz_i2c_data);
int mz_ois_get_fadj_data(_FACT_ADJ **p_fact_adj);
int mz_ois_get_module_id(void);

int main( void );
ADJ_STS mz_func_adj_angle_limit(OIS_UBYTE degree);

#endif
