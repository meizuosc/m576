/*
 * OTP driver for MEIZU M86.
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

#ifndef FIMC_IS_DEVICE_EEPROM_H
#define FIMC_IS_DEVICE_EEPROM_H

#include "fimc-is-device-sensor.h"

#define EEPROM_SECTION1_SIZE 52
#define EEPROM_CHECKSUM_SIZE 4
#define PRIMAX_OTP_NAME "otp-primax"
#define FIMC_IS_MAX_CAL_SIZE	(1U << 16)
#define EEPROM_MANUFAC_OFFSET (0x54)
#define EEPROM_YEAR_OFFSET (0x66)
#define EEPROM_WEEK_OFFSET (0x69)
#define EEPROM_SN_OFFSET (0x6a)
//#define IGNORE_CHECKSUM
#define SAVE_CCM_INFO
#define BCAM_CALI_FILE_PATH "/data/misc/media/RMO.bin"
//#define EEPROM_CHECKSUM_DEBUG

#define otp_assin4(buf, base) (buf[base+0] | (buf[base+1] <<8) | (buf[base+2] <<16) | (buf[base+3] << 24))

enum fimc_is_eeprom_state {
	FIMC_IS_EEPROM_STATE_INIT,
	FIMC_IS_EEPROM_STATE_I2CFAIL,
	FIMC_IS_EEPROM_STATE_CHECKSUM_FAIL,
	FIMC_IS_EEPROM_STATE_READONE,
	FIMC_IS_EEPROM_STATE_SAVED,
};

struct meizu_otp {
	int ois_start;
	int ois_size;
	char data[FIMC_IS_MAX_CAL_SIZE];
};

int fimc_is_ext_eeprom_read(struct fimc_is_device_sensor *device);
int fimc_is_eeprom_get_cal_buf(struct meizu_otp **pmeizu_otp);
u32 fimc_is_eeprom_check_state(void);
int mz_get_module_id(int sensor_id);
int mz_save_rear_cali(void);

#endif /* FIMC_IS_DEVICE_EEPROM_H */

