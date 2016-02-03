/* drivers/devfreq/exynos_ppmu_fw.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - PPMU control with firmware header.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __DEVFREQ_EXYNOS_PPMU_FW_H
#define __DEVFREQ_EXYNOS_PPMU_FW_H __FILE__

#include <linux/io.h>

struct ppmu_info {
	void __iomem *base;
};

typedef int (*pfn_ppmu_count)(struct ppmu_info *ppmu,
			unsigned int size,
			unsigned long long *ccnt,
			unsigned long long *pmcnt);

/* Helper Function */
int ppmu_init(struct ppmu_info *ppmu);
int ppmu_init_fw(struct ppmu_info *ppmu);
int ppmu_term(struct ppmu_info *ppmu);
int ppmu_disable(struct ppmu_info *ppmu);
int ppmu_reset(struct ppmu_info *ppmu);
int ppmu_reset_total(struct ppmu_info *ppmu,
			unsigned int size);
int ppmu_count(struct ppmu_info *ppmu,
		unsigned long long *ccnt,
		unsigned long long *pmcnt0,
		unsigned long long *pmcnt1,
		unsigned long long *pmcnt3);
int ppmu_count_total(struct ppmu_info *ppmu,
			unsigned int size,
			pfn_ppmu_count pfn_count,
			unsigned long long *ccnt,
			unsigned long long *pmcnt);
int ppmu_count_stop(struct ppmu_info *ppmu,
			unsigned int size);
#endif /* __DEVFREQ_EXYNOS_PPMU_FW_H */
