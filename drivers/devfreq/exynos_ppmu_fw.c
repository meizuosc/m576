/* drivers/devfreq/exynos_ppmu_fw.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - PPMU control with firmware code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/printk.h>

#include "exynos_ppmu_fw.h"
#include "FW_exynos_ppmu.h"

static int ppmu_get_check_null(struct ppmu_info *ppmu)
{
	if (ppmu == NULL) {
		pr_err("DEVFREQ(PPMU) : ppmu has not address\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * Helper Function
 */
int ppmu_init(struct ppmu_info *ppmu)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (ppmu->base == NULL) {
		pr_err("DEVFREQ(PPMU) : ppmu base address is null\n");
		return -EINVAL;
	}

	ppmu->base = ioremap((unsigned long)ppmu->base, SZ_1K);
	if (ppmu->base == NULL) {
		pr_err("DEVFREQ(PPMU) : ppmu base remap failed\n");
		return -EINVAL;
	}

	return 0;
}

int ppmu_term(struct ppmu_info *ppmu)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	iounmap(ppmu->base);

	return 0;
}

int ppmu_init_fw(struct ppmu_info *ppmu)
{
	PPMU_Init(ppmu->base);
	PPMU_SetEvent(ppmu->base,
		PPMU2_EVENT_RD_DATA,
		PPMU2_EVENT_WR_DATA,
		0,
		PPMU2_EVENT3_RW_DATA);

	return 0;
}

int ppmu_reset(struct ppmu_info *ppmu)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	PPMU_Start(ppmu->base, PPMU_MODE_MANUAL);

	return 0;
}

int ppmu_disable(struct ppmu_info *ppmu)
{
	PPMU_DeInit(ppmu->base);

	return 0;
}

int ppmu_reset_total(struct ppmu_info *ppmu,
		unsigned int size)
{
	unsigned int i;

	for (i = 0; i < size; ++i) {
		PPMU_Reset((ppmu+i)->base);
		PPMU_Start((ppmu+i)->base, PPMU_MODE_MANUAL);
	}

	return 0;
}

int ppmu_count(struct ppmu_info *ppmu,
		unsigned long long *ccnt,
		unsigned long long *pmcnt0,
		unsigned long long *pmcnt1,
		unsigned long long *pmcnt3)
{
	unsigned int val_ccnt;
	unsigned int val_pmcnt0;
	unsigned int val_pmcnt1;
	unsigned int val_pmcnt2;
	unsigned long long val_pmcnt3;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	PPMU_GetResult(ppmu->base,
			&val_ccnt,
			&val_pmcnt0,
			&val_pmcnt1,
			&val_pmcnt2,
			&val_pmcnt3);

	*ccnt = val_ccnt;
	*pmcnt0 = val_pmcnt0;
	*pmcnt1 = val_pmcnt1;
	*pmcnt3 = val_pmcnt3;

	return 0;
}

int ppmu_count_total(struct ppmu_info *ppmu,
			unsigned int size,
			pfn_ppmu_count pfn_count,
			unsigned long long *ccnt,
			unsigned long long *pmcnt)
{
	unsigned int i;
	unsigned long long val_ccnt = 0;
	unsigned long long val_pmcnt0 = 0;
	unsigned long long val_pmcnt1 = 0;
	unsigned long long val_pmcnt3 = 0;

	if (ccnt == NULL ||
		pmcnt == NULL) {
		pr_err("DEVFREQ(PPMU) : count argument is NULL\n");
		return -EINVAL;
	}

	*ccnt = 0;
	*pmcnt = 0;

	if (pfn_count != NULL) {
		return pfn_count(ppmu, size, ccnt, pmcnt);
	} else {
		for (i = 0; i < size; ++i) {
			if (ppmu_count(ppmu + i, &val_ccnt, &val_pmcnt0, &val_pmcnt1, &val_pmcnt3))
				return -EINVAL;

			if (*ccnt < val_ccnt)
				*ccnt = val_ccnt;

			*pmcnt += val_pmcnt3;
		}
	}

	return 0;
}

int ppmu_count_stop(struct ppmu_info *ppmu, unsigned int size)
{
	int i;

	for (i = 0; i < size; ++i)
		PPMU_Stop((ppmu+i)->base);

	return 0;
}
