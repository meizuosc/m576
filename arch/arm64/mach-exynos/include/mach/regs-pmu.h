/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management Unit register definition
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __REGS_PMU_H__
#define __REGS_PMU_H__ __FILE__

#if defined(CONFIG_SOC_EXYNOS5433)
#include <mach/regs-pmu-exynos5433.h>
#elif defined(CONFIG_SOC_EXYNOS7420)
#include <mach/regs-pmu-exynos7420.h>
#elif defined(CONFIG_SOC_EXYNOS7580)
#include <mach/regs-pmu-exynos7580.h>
#endif

#endif  /* __REGS_PMU_H__ */
