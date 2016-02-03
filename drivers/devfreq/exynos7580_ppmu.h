/* drivers/devfreq/exynos7580_ppmu.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - PPMU control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS_PPMU_H
#define __DEVFREQ_EXYNOS_PPMU_H __FILE__

#include <linux/notifier.h>

#include "exynos_ppmu_fw.h"

enum DEVFREQ_TYPE {
	INT,
	MIF,
	DEVFREQ_TYPE_COUNT,
};

struct devfreq_exynos {
	struct list_head node;
	struct ppmu_info *ppmu_list;
	unsigned int ppmu_count;
	unsigned long long val_ccnt;
	unsigned long long val_pmcnt;
	enum DEVFREQ_TYPE type;
};

struct devfreq_ppmu {
	struct ppmu_info *ppmu_list;
	unsigned int ppmu_count;
};

int exynos_devfreq_register(struct devfreq_exynos *de);
int exynos_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);
int exynos_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);
int exynos_ppmu_is_initialized(void);

#endif /* __DEVFREQ_EXYNOS_PPMU_H */
