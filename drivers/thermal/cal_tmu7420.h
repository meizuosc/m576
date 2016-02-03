/*
 * Copyright 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 * EXYNOS7420 - TMU CAL Header.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CAL_TMU7420_H
#define __CAL_TMU7420_H

#ifdef CONFIG_THERMAL
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_data/exynos_thermal.h>

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
#define EXYNOS_TMU_REG_CONTROL1			0x24
#define EXYNOS_TMU_REG_STATUS			0x28
#define EXYNOS_TMU_REG_SAMPLING_INTERVAL	0x2C
#define EXYNOS_TMU_REG_CURRENT_TEMP0	0x40
#define EXYNOS_TMU_REG_CURRENT_TEMP1	0x44

#define EXYNOS_THD_TEMP_RISE7_6			0x50
#define EXYNOS_THD_TEMP_RISE5_4			0x54
#define EXYNOS_THD_TEMP_RISE3_2			0x58
#define EXYNOS_THD_TEMP_RISE1_0			0x5C
#define EXYNOS_THD_TEMP_FALL7_6			0x60
#define EXYNOS_THD_TEMP_FALL5_4			0x64
#define EXYNOS_THD_TEMP_FALL3_2			0x68
#define EXYNOS_THD_TEMP_FALL1_0			0x6C
#define EXYNOS_THD_OFFSET				0x4

#define EXYNOS_TMU_REG_INTEN			0x110
#define EXYNOS_TMU_REG_INTCLEAR			0x118

#define EXYNOS_TMU_TEMP_MASK			0x1FF
#define EXYNOS_TMU_TEMP_SHIFT			9

#define EXYNOS_TMU_TRIM_TEMP_MASK		0x1ff
#define EXYNOS_TMU_TRIM_BUF_VREF_MASK	0x1f
#define EXYNOS_TMU_TRIM_BUF_VREF_SHIFT	18

#define EXYNOS_TMU_GAIN_SHIFT			8
#define EXYNOS_TMU_REF_VOLTAGE_SHIFT		24
#define EXYNOS_TMU_CORE_ON			1
#define EXYNOS_TMU_CORE_OFF			0
#define EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET	10

#define EXYNOS_EMUL_CON				0x160

#define EXYNOS_TMU_CLEAR_RISE_INT      		0xff
#define EXYNOS_TMU_CLEAR_FALL_INT      		(0xff << 16)

#define EXYNOS_PD_DET_EN				(1 << 23)
#define EXYNOS_MUX_ADDR_VALUE			6
#define EXYNOS_MUX_ADDR_SHIFT			20
#define EXYNOS_TMU_TRIP_MODE_SHIFT		13
#define EXYNOS_THERM_TRIP_EN			(1 << 12)
#define EXYNOS_MUX_ADDR				0x600000

#define EXYNOS_VALID_P0_SHIFT			12
#define EXYNOS_CUR_SENSING_PROBE_MASK		0x7
#define EXYNOS_CUR_SENSING_PROBE_SHIFT		8

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
#define EXYNOS_EMUL_DATA_SHIFT			7
#define EXYNOS_EMUL_DATA_MASK			0x1FF
#define EXYNOS_EMUL_ENABLE			0x1
#endif /* CONFIG_THERMAL_EMULATION */

/* Rising, Falling interrupt bit number*/
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

#define EXYNOS_ZONE_COUNT			1
#define EXYNOS_TMU_COUNT			4
#define TRIP_EN_COUNT				8
#define EXYNOS_GPU_NUMBER			2

#define EXYNOS_TMU_MAX_PROBE		1
#define EXYNOS_TMU_NUM_PROBE_SHIFT	16
#define EXYNOS_TMU_NUM_PROBE_MASK	0x7


#define CALIB_SEL_MASK			0x00800000
#define VPTAT_CTRL_MASK			0x00700000
#define VPTAT_CTRL_SHIFT		20
#define BUF_VREF_SEL_2POINT		23
#define IDLE_MAX_TIME			2000

#define TMU_CONTROL_RSVD_MASK  		0xe07f00fe
#define TMU_CONTROL_ONOFF_MASK		0xfffffffe
#define TMU_CONTROL1_RSVD_MASK		0xfff8ffff

#define MIN_TEMP				15
#define MAX_TEMP				125

enum probe_sensor_num {
	SENSOR_P0 = 0,
	SENSOR_P1,
	SENSOR_P2,
	SENSOR_P3,
	SENSOR_P4,
};

struct cal_tmu_data {
	addr_u32 base[EXYNOS_TMU_COUNT];
	u16 temp_error1[EXYNOS_TMU_COUNT];
	u16 temp_error2[EXYNOS_TMU_COUNT];
	u8 vptat[EXYNOS_TMU_COUNT];
	u32 gain[EXYNOS_TMU_COUNT];
	u32 reference_voltage[EXYNOS_TMU_COUNT];
	u8 noise_cancel_mode;
	enum calibration_type cal_type;
	bool trigger_level_en[8];
	u16 last_temperature[EXYNOS_TMU_COUNT];
};

void cal_tmu_control(struct cal_tmu_data *data, int id, bool on);
int cal_tmu_code_to_temp(struct cal_tmu_data *data, u16 temp_code, int id);
int cal_tmu_temp_to_code(struct cal_tmu_data *data, u16 temp, int id);
int cal_tmu_read(struct cal_tmu_data *data, int id);
void cal_tmu_interrupt(struct cal_tmu_data *data, int id, bool en);

#endif /* __CAL_TMU7420_H */
