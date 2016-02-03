/*
 * RANGING IC driver for MEIZU M86.
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

#ifndef __H_RANGING_MEIZU_H__
#define __H_RANGING_MEIZU_H__
#include <linux/meizu-sys.h>

#define MZ_LAER_FACTORY_TEST
#define MZ_LASER_DEBUG
//#define MZ_LASER_SAVE_CALI

#define ABS(x)              (((x) > 0) ? (x) : (-(x)))

int mz_ranging_power_enable(struct i2c_client *client, bool enable);
int mz_ranging_parse_dt(struct i2c_client *client);

#ifdef CONFIG_PM
int mz_ranging_suspend(struct device *dev);
int mz_ranging_resume(struct device *dev);
#else
#define mz_ranging_suspend	NULL
#define mz_ranging_resume	NULL
#endif

#endif
