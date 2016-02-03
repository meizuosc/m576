/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_COMPANION_H
#define FIMC_IS_DEVICE_COMPANION_H

#include <mach/exynos-fimc-is-sensor.h>
#include <linux/interrupt.h>
#include "fimc-is-video.h"
#include "fimc-is-config.h"

struct fimc_is_video_ctx;

#define FIMC_IS_COMPANION_DEV_NAME "exynos-fimc-is-companion"

enum fimc_is_companion_state {
	FIMC_IS_COMPANION_OPEN,
	FIMC_IS_COMPANION_MCLK_ON,
	FIMC_IS_COMPANION_ICLK_ON,
	FIMC_IS_COMPANION_GPIO_ON,
	FIMC_IS_COMPANION_S_INPUT
};

struct fimc_is_device_companion {
	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	void __iomem					*regs;
	struct fimc_is_mem				mem;
	u32						instance;

	struct fimc_is_video_ctx			*vctx;
	struct fimc_is_video				video;

	unsigned long					state;

	struct fimc_is_resourcemgr			*resourcemgr;
	struct exynos_platform_fimc_is_companion	*pdata;
	struct fimc_is_module_enum			*module;
	void						*private_data;
};

int fimc_is_companion_open(struct fimc_is_device_companion *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_companion_close(struct fimc_is_device_companion *device);
int fimc_is_companion_s_input(struct fimc_is_device_companion *device,
	u32 input,
	u32 scenario);
int fimc_is_companion_g_module(struct fimc_is_device_companion *device,
	struct fimc_is_module_enum **module);
int fimc_is_companion_runtime_suspend(struct device *dev);
int fimc_is_companion_runtime_resume(struct device *dev);

#endif
