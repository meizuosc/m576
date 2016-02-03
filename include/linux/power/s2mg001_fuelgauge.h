/*
 * s2mg001_fuelgauge.h
 * Samsung S2MG001 Fuel Gauge Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __S2MG001_FUELGAUGE_H
#define __S2MG001_FUELGAUGE_H __FILE__

#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#endif

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#define SEC_FUELGAUGE_I2C_SLAVEADDR 0x76

#define S2MG001_REG_STATUS		0x00
#define S2MG001_REG_IRQ			0x02
#define S2MG001_REG_RVBAT		0x04
#define S2MG001_REG_ROCV		0x06
#define S2MG001_REG_RSOC		0x0A
#define S2MG001_REG_RTEMP		0x0E
#define S2MG001_REG_RBATCAP		0x10
#define S2MG001_REG_RZADJ		0x12
#define S2MG001_REG_RBATZ0		0x14
#define S2MG001_REG_RBATZ1		0x16
#define S2MG001_REG_IRQ_LVL		0x18
#define S2MG001_REG_START		0x1A

struct battery_data_t {
	u8 *type_str;
};

struct sec_fg_info {
	bool dummy;
};

#endif /* __S2MG001_FUELGAUGE_H */

