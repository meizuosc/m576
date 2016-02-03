/*
 * Copyright (C) 2013 Samsung Electronics
 *               http://www.samsung.com/
 *               Sangkyu Kim <skwith.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __GPU_HWCNT_H
#define __GPU_HWCNT_H __FILE__

#define MALI_SIZE_OF_HWCBLK 64

#if SOC_NAME == 5430
#define GPU_MAX_CLOCK_LIMIT 550
#endif

enum HWCNT_OFFSET {
	OFFSET_TRIPIPE_ACTIVE = 26,
	OFFSET_ARITH_WORDS = 27,
	OFFSET_LS_ISSUES = 32,
	OFFSET_TEX_ISSUES = 42,
};

extern mali_error kbase_instr_hwcnt_util_dump(struct kbase_device *kbdev);

mali_error exynos_gpu_hwcnt_update(struct kbase_device *kbdev);

bool hwcnt_check_conditions(struct kbase_device *kbdev);
void hwcnt_utilization_equation(struct kbase_device *kbdev);
mali_error hwcnt_get_utilization_resouce(struct kbase_device *kbdev);

#endif /* __GPU_HWCNT_H */
