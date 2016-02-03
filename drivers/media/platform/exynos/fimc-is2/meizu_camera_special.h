/*
 * Meizu special feature of camera
 *
 * Copyright (C) 2015 Meizu Technology Co.Ltd, Zhuhai, China
 * Author: 	QuDao	<qudao@meizu.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __MEIZU_CAMERA_SPECIAL_H__
#define __MEIZU_CAMERA_SPECIAL_H__

#include "fimc-is-core.h"
#include "fimc-is-device-eeprom.h"
#define IGNORE_POWER_STATUS

int meizu_special_feature_probe(struct i2c_client *client);
int LC898212XD_probe(struct i2c_client *client);
void fimc_is_i2c_enable_irq(struct fimc_is_core *core, bool enable);
int camera_module_active(bool enable);
int get_module_type(struct fimc_is_device_sensor *device);

#endif
