/* drivers/devfreq/exynos5433_ppmu.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS5433 - PPMU control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS5433_PPMU_H
#define __DEVFREQ_EXYNOS5433_PPMU_H __FILE__

#include <linux/notifier.h>

#include "exynos_ppmu_fw.h"

enum DEVFREQ_TYPE {
	MIF,
	INT,
	DISP,
	DEVFREQ_TYPE_COUNT,
};

enum DEVFREQ_PPMU {
	PPMU_D0_CPU,
	PPMU_D0_GEN,
	PPMU_D0_RT,
	PPMU_D1_CPU,
	PPMU_D1_GEN,
	PPMU_D1_RT,
	PPMU_COUNT,
};

enum DEVFREQ_PPMU_ADDR {
	PPMU_D0_CPU_ADDR = 0x10480000,
	PPMU_D0_GEN_ADDR = 0x10490000,
	PPMU_D0_RT_ADDR = 0x104A0000,
	PPMU_D1_CPU_ADDR = 0x104B0000,
	PPMU_D1_GEN_ADDR = 0x104C0000,
	PPMU_D1_RT_ADDR = 0x104D0000,
};

struct devfreq_exynos {
	struct list_head node;
	struct ppmu_info *ppmu_list;
	unsigned int ppmu_count;
	unsigned long long val_ccnt;
	unsigned long long val_pmcnt;
	enum DEVFREQ_TYPE type;
};

int exynos5433_devfreq_init(struct devfreq_exynos *de);
int exynos5433_devfreq_register(struct devfreq_exynos *de);
int exynos5433_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);
int exynos5433_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);

#endif /* __DEVFREQ_EXYNOS5433_PPMU_H */
