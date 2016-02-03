/*
 * Copyright 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 * EXYNOS - TMU CAL Header.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CAL_TMU_H
#define __CAL_TMU_H

#ifdef CONFIG_THERMAL
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_data/exynos_thermal.h>
#include <mach/otp.h>

#define Outp32(addr, data)	(__raw_writel(data, addr))
#define Inp32(addr)			(__raw_readl(addr))

typedef void __iomem *addr_u32;
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "types.h"
#include "util.h"

#define Outp32(addr, data)	(*(volatile u32 *)(addr) = (data))
#define Inp32(addr)			(*(volatile u32 *)(addr))
#define usleep_range(a, b)	(usleep(a))
#define HW_REG32(base, offset)		(*(volatile u32 *)(base + (offset)))

typedef unsigned long addr_u32;

enum calibration_type {
	TYPE_ONE_POINT_TRIMMING,
	TYPE_TWO_POINT_TRIMMING,
	TYPE_NONE,
};
#endif	/* CONFIG_THERMAL */

/* Exynos generic registers */
#define EXYNOS_TMU_REG_TRIMINFO			0x0
#define EXYNOS_TMU_REG_CONTROL			0x20
#define EXYNOS_TMU_REG_STATUS			0x28
#define EXYNOS_TMU_REG_SAMPLING_INTERVAL	0x2C
#define EXYNOS_TMU_REG_CURRENT_TEMP		0x40

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#define EXYNOS_THD_TEMP_RISE			0x50
#define EXYNOS_THD_TEMP_FALL			0x60
#define EXYNOS_THD_TEMP_RISE3_0			0x50
#define EXYNOS_THD_TEMP_RISE7_4			0x54
#define EXYNOS_THD_TEMP_FALL3_0			0x60
#define EXYNOS_THD_TEMP_FALL7_4			0x64
#define EXYNOS_TMU_REG_INTEN			0xC0
#define EXYNOS_TMU_REG_INTCLEAR			0xC8
#else
#define EXYNOS_THD_TEMP_RISE            	0x50
#define EXYNOS_THD_TEMP_FALL            	0x54
#define EXYNOS_TMU_REG_INTEN			0x70
#define EXYNOS_TMU_REG_INTSTAT			0x74
#define EXYNOS_TMU_REG_INTCLEAR			0x78
#endif

#define EXYNOS_TMU_TRIM_TEMP_MASK		0xff
#define EXYNOS_TMU_GAIN_SHIFT			8
#define EXYNOS_TMU_REF_VOLTAGE_SHIFT		24
#define EXYNOS_TMU_CORE_ON			1
#define EXYNOS_TMU_CORE_OFF			0
#define EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET	50

/* Exynos4210 specific registers */
#define EXYNOS4210_TMU_REG_THRESHOLD_TEMP	0x44
#define EXYNOS4210_TMU_REG_TRIG_LEVEL0		0x50
#define EXYNOS4210_TMU_REG_TRIG_LEVEL1		0x54
#define EXYNOS4210_TMU_REG_TRIG_LEVEL2		0x58
#define EXYNOS4210_TMU_REG_TRIG_LEVEL3		0x5C
#define EXYNOS4210_TMU_REG_PAST_TEMP0		0x60
#define EXYNOS4210_TMU_REG_PAST_TEMP1		0x64
#define EXYNOS4210_TMU_REG_PAST_TEMP2		0x68
#define EXYNOS4210_TMU_REG_PAST_TEMP3		0x6C

#define EXYNOS4210_TMU_TRIG_LEVEL0_MASK		0x1
#define EXYNOS4210_TMU_TRIG_LEVEL1_MASK		0x10
#define EXYNOS4210_TMU_TRIG_LEVEL2_MASK		0x100
#define EXYNOS4210_TMU_TRIG_LEVEL3_MASK		0x1000
#define EXYNOS4210_TMU_INTCLEAR_VAL		0x1111

/* Exynos5250 and Exynos4412 specific registers */
#define EXYNOS_TRIMINFO_RELOAD1			0x01
#define EXYNOS_TRIMINFO_RELOAD2			0x11
#define EXYNOS_TRIMINFO_CONFIG			0x10
#define EXYNOS_TRIMINFO_CONTROL			0x14

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#define EXYNOS_EMUL_CON				0x110
#else
#define EXYNOS_EMUL_CON				0x80
#endif

#define EXYNOS_TRIMINFO_RELOAD			0x1
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#define EXYNOS_TMU_CLEAR_RISE_INT      		0xff
#define EXYNOS_TMU_CLEAR_FALL_INT      		(0xff << 16)
#else
#define EXYNOS_TMU_CLEAR_RISE_INT		0x1111
#define EXYNOS_TMU_CLEAR_FALL_INT		(0x1111 << 16)
#endif
#define EXYNOS_MUX_ADDR_VALUE			6
#define EXYNOS_MUX_ADDR_SHIFT			20
#define EXYNOS_TMU_TRIP_MODE_SHIFT		13
#define EXYNOS_THERM_TRIP_EN			(1 << 12)
#define EXYNOS_MUX_ADDR				0x600000

#define EFUSE_MIN_VALUE				40
#define EFUSE_MAX_VALUE				100

/* In-kernel thermal framework related macros & definations */
#define SENSOR_NAME_LEN				16
#define MAX_TRIP_COUNT				9
#define MAX_COOLING_DEVICE 			5
#define MAX_THRESHOLD_LEVS 			8

#define PASSIVE_INTERVAL			100
#define ACTIVE_INTERVAL				300
#define IDLE_INTERVAL 				500
#define MCELSIUS				1000

#ifdef CONFIG_THERMAL_EMULATION
#define EXYNOS_EMUL_TIME			0x57F0
#define EXYNOS_EMUL_TIME_SHIFT			16
#define EXYNOS_EMUL_DATA_SHIFT			8
#define EXYNOS_EMUL_DATA_MASK			0xFF
#define EXYNOS_EMUL_ENABLE			0x1
#endif /* CONFIG_THERMAL_EMULATION */

/* Rising, Falling interrupt bit number*/
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#define RISE_LEVEL1_SHIFT      			1
#define RISE_LEVEL2_SHIFT      			2
#define RISE_LEVEL3_SHIFT				3
#define RISE_LEVEL4_SHIFT      			4
#define RISE_LEVEL5_SHIFT      			5
#define RISE_LEVEL6_SHIFT      			6
#define RISE_LEVEL7_SHIFT      			7
#define FALL_LEVEL0_SHIFT      			16
#define FALL_LEVEL1_SHIFT      			17
#define FALL_LEVEL2_SHIFT      			18
#define FALL_LEVEL3_SHIFT      			19
#define FALL_LEVEL4_SHIFT      			20
#define FALL_LEVEL5_SHIFT      			21
#define FALL_LEVEL6_SHIFT      			22
#define FALL_LEVEL7_SHIFT      			23
#else
#define RISE_LEVEL1_SHIFT				4
#define RISE_LEVEL2_SHIFT				8
#define RISE_LEVEL3_SHIFT				12
#define RISE_LEVEL4_SHIFT      			0
#define RISE_LEVEL5_SHIFT      			0
#define RISE_LEVEL6_SHIFT      			0
#define RISE_LEVEL7_SHIFT      			0
#define FALL_LEVEL0_SHIFT				16
#define FALL_LEVEL1_SHIFT				20
#define FALL_LEVEL2_SHIFT				24
#define FALL_LEVEL3_SHIFT				28
#define FALL_LEVEL4_SHIFT      			0
#define FALL_LEVEL5_SHIFT      			0
#define FALL_LEVEL6_SHIFT      			0
#define FALL_LEVEL7_SHIFT      			0
#endif

#define EXYNOS_ZONE_COUNT			1
#define EXYNOS_TMU_COUNT			5
#define EXYSNO_CLK_COUNT			2
#define TRIP_EN_COUNT				8
#ifdef CONFIG_SOC_EXYNOS5422
#define EXYNOS_GPU_NUMBER			4
#else
#define EXYNOS_GPU_NUMBER			2
#endif

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#define CALIB_SEL_MASK			0x00800000
#define VPTAT_CTRL_MASK			0x00700000
#define VPTAT_CTRL_SHIFT		20
#define BUF_VREF_SEL_2POINT		23
#define IDLE_MAX_TIME			1000
#endif

#define TMU_CONTROL_RSVD_MASK  		0xe08f00fe
#define TMU_CONTROL_ONOFF_MASK		0xfffffffe

#define MIN_TEMP				15
#define MAX_TEMP				125

struct cal_tmu_data {
	addr_u32 base[EXYNOS_TMU_COUNT];
	u8 temp_error1[EXYNOS_TMU_COUNT];
	u8 temp_error2[EXYNOS_TMU_COUNT];
	u8 vptat[EXYNOS_TMU_COUNT];
	u8 gain;
	u8 reference_voltage;
	u8 noise_cancel_mode;
	enum calibration_type cal_type;
	bool trigger_level_en[8];
};

void cal_tmu_control(struct cal_tmu_data *data, int id, bool on);
int cal_tmu_code_to_temp(struct cal_tmu_data *data, unsigned char temp_code, int id);
int cal_tmu_temp_to_code(struct cal_tmu_data *data, unsigned char temp, int id);
int cal_tmu_read(struct cal_tmu_data *data, int id);
#if defined(CONFIG_SOC_EXYNOS5433)
int cal_tmu_otp_read(struct cal_tmu_data *data);
#endif

#endif /* __CAL_TMU_H */
