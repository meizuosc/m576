/* linux/arm/arm/mach-exynos/include/mach/regs-clock.h
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 - Header file for exynos pm support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_EXYNOS_PM_H
#define _LINUX_EXYNOS_PM_H

#include <linux/kernel.h>
#include <linux/notifier.h>

/*
 * Event codes for PM states
 */
enum exynos_pm_event {
	/* CPU is preparing to enter the LPA state */
	LPA_PREPARE,

	/* CPU is entering the LPA state */
	LPA_ENTER,

	/* CPU failed to enter the LPA state */
	LPA_ENTER_FAIL,

	/* CPU is exiting the LPA state */
	LPA_EXIT,

	/* CPU is preparing to enter the LPC */
	LPC_PREPARE,
};

#ifdef CONFIG_CPU_IDLE
int exynos_pm_register_notifier(struct notifier_block *nb);
int exynos_pm_unregister_notifier(struct notifier_block *nb);
int exynos_lpa_prepare(void);
int exynos_lpa_enter(void);
int exynos_lpa_exit(void);
int exynos_lpc_prepare(void);
#else /* !CONFIG_CPU_IDLE */
static inline int exynos_pm_register_notifier(struct notifier_block *nb)
{
	return -EINVAL;
}

static inline int exynos_pm_unregister_notifier(struct notifier_block *nb)
{
	return -EINVAL;
}

static inline int exynos_lpa_prepare(void)
{
	return 0;
}

static inline int exynos_lpa_enter(void)
{
	return 0;
}

static inline int exynos_lpa_exit(void)
{
	return 0;
}

static inline int exynos_lpc_prepare(void)
{
	return 0;
}

#endif
#endif
