/*
 * Copyright 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 *  EXYNOS7420 - TMU CAL code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "cal_tmu7420.h"

void cal_tmu_control(struct cal_tmu_data *data, int id, bool on)
{
	unsigned int con, interrupt_en;
	int status;
	int timeout = 20000;
	int rsvd = 0;
	int i;

	con = Inp32(data->base[id] + EXYNOS_TMU_REG_CONTROL);

	if (con & 0x1) {
		con &= TMU_CONTROL_ONOFF_MASK;
		Outp32(data->base[id] + EXYNOS_TMU_REG_CONTROL, con);
	}

	while (1) {
		status = Inp32(data->base[id] + EXYNOS_TMU_REG_STATUS);
		if (status & 0x01)
			break;

		timeout--;
		if (!timeout) {
#ifdef CONFIG_THERMAL
			pr_err("%s: timeout TMU busy\n", __func__);
#endif
			break;
		}

		usleep_range(1, 2);
	};
	timeout = 20000;

	rsvd = (Inp32(data->base[id] + EXYNOS_TMU_REG_CONTROL) & TMU_CONTROL_RSVD_MASK);

	con = data->reference_voltage[id] << EXYNOS_TMU_REF_VOLTAGE_SHIFT |
		data->gain[id] << EXYNOS_TMU_GAIN_SHIFT;

	con |= data->noise_cancel_mode << EXYNOS_TMU_TRIP_MODE_SHIFT;

	con |= EXYNOS_PD_DET_EN;

	con |= rsvd;

	Outp32(data->base[id] + EXYNOS_TMU_REG_CONTROL, con);

	if (on) {
		con |= EXYNOS_TMU_CORE_ON;
		interrupt_en =
			data->trigger_level_en[7] << FALL_LEVEL7_SHIFT |
			data->trigger_level_en[6] << FALL_LEVEL6_SHIFT |
			data->trigger_level_en[5] << FALL_LEVEL5_SHIFT |
			data->trigger_level_en[4] << FALL_LEVEL4_SHIFT |
			data->trigger_level_en[3] << FALL_LEVEL3_SHIFT |
			data->trigger_level_en[2] << FALL_LEVEL2_SHIFT |
			data->trigger_level_en[1] << FALL_LEVEL1_SHIFT |
			data->trigger_level_en[0] << FALL_LEVEL0_SHIFT |
			data->trigger_level_en[7] << RISE_LEVEL7_SHIFT |
			data->trigger_level_en[6] << RISE_LEVEL6_SHIFT |
			data->trigger_level_en[5] << RISE_LEVEL5_SHIFT |
			data->trigger_level_en[4] << RISE_LEVEL4_SHIFT |
			data->trigger_level_en[3] << RISE_LEVEL3_SHIFT |
			data->trigger_level_en[2] << RISE_LEVEL2_SHIFT |
			data->trigger_level_en[1] << RISE_LEVEL1_SHIFT |
			data->trigger_level_en[0];

		Outp32(data->base[id] + EXYNOS_TMU_REG_CONTROL, con);
		con &= TMU_CONTROL_ONOFF_MASK;
		Outp32(data->base[id] + EXYNOS_TMU_REG_CONTROL, con);

		while (1) {
			status = Inp32(data->base[id] + EXYNOS_TMU_REG_STATUS);
			if (status & 0x01)
				break;

			timeout--;
			if (!timeout) {
#ifdef CONFIG_THERMAL
				pr_err("%s: timeout TMU busy\n", __func__);
#endif
				break;
			}

			usleep_range(1, 2);
		};

		con |= EXYNOS_TMU_CORE_ON;
		Outp32(data->base[id] + EXYNOS_TMU_REG_CONTROL, con);

		con |= EXYNOS_THERM_TRIP_EN;
		Outp32(data->base[id] + EXYNOS_TMU_REG_CONTROL, con);
	} else {
		con |= EXYNOS_TMU_CORE_OFF;
		interrupt_en = 0; /* Disable all interrupts */
		Outp32(data->base[id] + EXYNOS_TMU_REG_CONTROL, con);
	}

	for (i = 0; i < 5; i++)
		Outp32(data->base[id] + EXYNOS_TMU_REG_INTEN + (i * 0x10), interrupt_en);
}

int cal_tmu_code_to_temp(struct cal_tmu_data *data, u16 temp_code, int id)
{
	int temp;
	int fuse_id = 0;

	fuse_id = id;

	switch (data->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp = (temp_code - data->temp_error1[fuse_id]) * (85 - 25) /
		    (data->temp_error2[fuse_id] - data->temp_error1[fuse_id]) + 25;
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp = temp_code - data->temp_error1[fuse_id] + 25;
		break;
	default:
		temp = temp_code - EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET;
		break;
	}

	/* temperature should range between minimum and maximum */
	if (temp > MAX_TEMP)
		temp = MAX_TEMP;
	else if (temp < MIN_TEMP)
		temp = MIN_TEMP;

	return temp;
}

int cal_tmu_temp_to_code(struct cal_tmu_data *data, u16 temp, int id)
{
	int temp_code;
	int fuse_id = 0;

	if (temp > MAX_TEMP)
		temp_code = MAX_TEMP;
	else if (temp < MIN_TEMP)
		temp_code = MIN_TEMP;

	fuse_id = id;

	switch (data->cal_type) {
	case TYPE_TWO_POINT_TRIMMING:
		temp_code = (temp - 25) *
		    (data->temp_error2[fuse_id] - data->temp_error1[fuse_id]) /
		    (85 - 25) + data->temp_error1[fuse_id];
		break;
	case TYPE_ONE_POINT_TRIMMING:
		temp_code = temp + data->temp_error1[fuse_id] - 25;
		break;
	default:
		temp_code = temp + EXYNOS_TMU_DEF_CODE_TO_TEMP_OFFSET;
		break;
	}

	return temp_code;
}


int cal_tmu_read(struct cal_tmu_data *data, int id)
{
	unsigned int temp_code, status;
	int temp = -1;

	status = Inp32(data->base[id] + EXYNOS_TMU_REG_STATUS) >> EXYNOS_VALID_P0_SHIFT;
	if (status & 0x1) {
		temp_code = Inp32(data->base[id] + EXYNOS_TMU_REG_CURRENT_TEMP0) & EXYNOS_TMU_TEMP_MASK;
		temp = cal_tmu_code_to_temp(data, temp_code, id);
		data->last_temperature[id] = temp;
	}

	return temp;
}

void cal_tmu_interrupt(struct cal_tmu_data *data, int id, bool en)
{
	int i = 0;
	int interrupt_en = 0;

	if (en) {
		interrupt_en =
			data->trigger_level_en[7] << FALL_LEVEL7_SHIFT |
			data->trigger_level_en[6] << FALL_LEVEL6_SHIFT |
			data->trigger_level_en[5] << FALL_LEVEL5_SHIFT |
			data->trigger_level_en[4] << FALL_LEVEL4_SHIFT |
			data->trigger_level_en[3] << FALL_LEVEL3_SHIFT |
			data->trigger_level_en[2] << FALL_LEVEL2_SHIFT |
			data->trigger_level_en[1] << FALL_LEVEL1_SHIFT |
			data->trigger_level_en[0] << FALL_LEVEL0_SHIFT |
			data->trigger_level_en[7] << RISE_LEVEL7_SHIFT |
			data->trigger_level_en[6] << RISE_LEVEL6_SHIFT |
			data->trigger_level_en[5] << RISE_LEVEL5_SHIFT |
			data->trigger_level_en[4] << RISE_LEVEL4_SHIFT |
			data->trigger_level_en[3] << RISE_LEVEL3_SHIFT |
			data->trigger_level_en[2] << RISE_LEVEL2_SHIFT |
			data->trigger_level_en[1] << RISE_LEVEL1_SHIFT |
			data->trigger_level_en[0];
	} else {
		interrupt_en = 0; /* Disable all interrupts */
	}

	writel(EXYNOS_TMU_CLEAR_RISE_INT | EXYNOS_TMU_CLEAR_FALL_INT,
			data->base[id] + EXYNOS_TMU_REG_INTCLEAR);

	for (i = 0; i < 5; i++)
		Outp32(data->base[id] + EXYNOS_TMU_REG_INTEN + (i * 0x10), interrupt_en);

}
