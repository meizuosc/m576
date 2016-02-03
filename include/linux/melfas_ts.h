/*
 * include/linux/melfas_ts.h - platform data structure for MCS Series sensor
 *
 * Copyright (C) 2010 Melfas, Inc.
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

#ifndef _LINUX_MELFAS_TS_H
#define _LINUX_MELFAS_TS_H

#define MELFAS_TS_NAME "melfas-ts"
#define ENABLE					1
#define DISABLE					0

enum
{
	None = 0,
	TOUCH_SCREEN,
	TOUCH_KEY
};

struct muti_touch_info
{
	int pressure;
	int area;
	int posX;
	int posY;
};

struct melfas_tsi_platform_data {
	uint32_t version;
	int max_x;
	int max_y;
	int max_pressure;
	int max_area;
	int gpio_scl;
	int gpio_sda;
	int (*power_enable)(int en);
	const char *regulator_dvdd;
	const char *regulator_avdd;
	int gpio;
	int irq_type;
};

#define P5_MAX_TOUCH	255 /*macroliu*/
#define SLOT_TYPE	0

#define I2C_RETRY_CNT 5 //Fixed value
#define DOWNLOAD_RETRY_CNT 5 //Fixed value
#define MELFAS_DOWNLOAD 1 //Fixed value


#define TS_READ_LEN_ADDR 0x0F //Fixed value
#define TS_READ_START_ADDR 0x10 //Fixed value
#define TS_READ_REGS_LEN 66 //Fixed value
#define TS_WRITE_REGS_LEN 16 //Fixed value


#define TS_MAX_TOUCH 	10 //Model Dependent
#define TS_READ_HW_VER_ADDR 0xF1 //Model Dependent
#define TS_READ_SW_VER_ADDR 0xF5 //Model Dependent



#define MELFAS_HW_REVISON 0x01 //Model Dependent
#define MELFAS_FW_VERSION 0x02 //0x02 //Model Dependent

#define DEBUG_PRINT 0 //Model Dependent

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif


#endif /* _LINUX_MELFAS_TS_H */
