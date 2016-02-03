/*
 * /include/media/exynos_fimc_is_sensor.h
 *
 * Copyright (C) 2012 Samsung Electronics, Co. Ltd
 *
 * Exynos series exynos_fimc_is_sensor device support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef MEDIA_EXYNOS_COMPANION_H
#define MEDIA_EXYNOS_COMPANION_H

#include <linux/platform_device.h>

struct exynos_platform_fimc_is_companion {
	int (*iclk_cfg)(struct platform_device *pdev, u32 scenario, u32 channel);
	int (*iclk_on)(struct platform_device *pdev,u32 scenario, u32 channel);
	int (*iclk_off)(struct platform_device *pdev, u32 scenario, u32 channel);
	int (*mclk_on)(struct platform_device *pdev, u32 scenario, u32 channel);
	int (*mclk_off)(struct platform_device *pdev, u32 scenario, u32 channel);
	u32 scenario;
	u32 mclk_ch;
	u32 id;
};

extern int exynos_fimc_is_companion_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_companion_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_companion_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_companion_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel);
extern int exynos_fimc_is_companion_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel);

#endif /* MEDIA_EXYNOS_COMPANION_H */
