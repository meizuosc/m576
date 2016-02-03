/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *		http://www.samsung.com/
 *
 * Chip Abstraction Layer for System power down support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <mach/pmu.h>
#include <mach/pmu_cal_sys.h>

#define CHECK_CAL(cal) \
	if (!(cal)) { \
		pr_err("%s: Exynos PMU CAL is not registered\n", __func__); \
		return; \
	}

#define CHECK_CAL_ERR(cal) \
	if (!(cal)) { \
		pr_err("%s: Exynos PMU CAL is not registered\n", __func__); \
		return -1; \
	}

static const struct pmu_cal_sys_ops *cal;

int exynos_pmu_cal_sys_init(void)
{
	register_pmu_cal_sys_ops(&cal);

	CHECK_CAL_ERR(cal);
	cal->sys_init();
	return 0;
}

void exynos_pmu_cal_sys_prepare(enum sys_powerdown mode)
{
	CHECK_CAL(cal);
	cal->sys_prepare(mode);
}

void exynos_pmu_cal_sys_post(enum sys_powerdown mode)
{
	CHECK_CAL(cal);
	cal->sys_post(mode);
}

void exynos_pmu_cal_sys_earlywake(enum sys_powerdown mode)
{
	CHECK_CAL(cal);
	cal->sys_earlywake(mode);
}
