/* drivers/devfreq/exynos7420_ppmu.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS7420 - PPMU control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS7420_PPMU_H
#define __DEVFREQ_EXYNOS7420_PPMU_H __FILE__

#include <linux/notifier.h>

#include "exynos_ppmu_fw.h"

enum DEVFREQ_TYPE {
	MIF,
	INT,
	DEVFREQ_TYPE_COUNT,
};

enum DEVFREQ_PPMU {
	PPMU0_0_GEN_RT,
	PPMU0_1_CPU,
	PPMU1_0_GEN_RT,
	PPMU1_1_CPU,
	PPMU2_0_GEN_RT,
	PPMU2_1_CPU,
	PPMU3_0_GEN_RT,
	PPMU3_1_CPU,
	PPMU_COUNT,
};

enum DEVFREQ_PPMU_ADDR {
	PPMU0_0_GEN_RT_ADDR =	0x10830000,
	PPMU0_1_CPU_ADDR =	0x10860000,
	PPMU1_0_GEN_RT_ADDR =	0x10930000,
	PPMU1_1_CPU_ADDR =	0x10960000,
	PPMU2_0_GEN_RT_ADDR =	0x10A30000,
	PPMU2_1_CPU_ADDR =	0x10A60000,
	PPMU3_0_GEN_RT_ADDR =	0x10B30000,
	PPMU3_1_CPU_ADDR =	0x10B60000,
};

struct devfreq_exynos {
	struct list_head node;
	struct ppmu_info *ppmu_list;
	unsigned int ppmu_count;
	unsigned long long val_ccnt;
	unsigned long long val_pmcnt;
	enum DEVFREQ_TYPE type;
};

int exynos7420_devfreq_init(struct devfreq_exynos *de);
int exynos7420_devfreq_register(struct devfreq_exynos *de);
int exynos7420_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);
int exynos7420_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);
#ifdef CONFIG_HYBRID_INVOKING
int exynos7420_ppmu_check_deferrable(unsigned long target, unsigned long min, unsigned long locked_min);
#endif	// CONFIG_HYBRID_INVOKING

#endif /* __DEVFREQ_EXYNOS7420_PPMU_H */
