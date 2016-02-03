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

#ifndef fimc_is_device_companion_H
#define fimc_is_device_companion_H

#include <mach/exynos-fimc-is-sensor.h>
#include <linux/interrupt.h>
#include "fimc-is-video.h"

struct fimc_is_video_ctx;

#define FIMC_IS_COMPANION_DEV_NAME "exynos-fimc-is-companion"

enum fimc_is_companion_state {
	FIMC_IS_COMPANION_OPEN,
	FIMC_IS_COMPANION_MCLK_ON,
	FIMC_IS_COMPANION_ICLK_ON,
	FIMC_IS_COMPANION_GPIO_ON,
};

enum fimc_is_companion_status {
	FIMC_IS_COMPANION_IDLE,
	FIMC_IS_COMPANION_OPENNING,
	FIMC_IS_COMPANION_OPENDONE,
};

struct fimc_is_device_companion {
	struct v4l2_device				v4l2_dev;
	struct platform_device				*pdev;
	void __iomem					*regs;
	struct fimc_is_mem				mem;

	struct fimc_is_video_ctx			*vctx;
	struct fimc_is_video				video;

	unsigned long					state;

	struct exynos_platform_fimc_is_companion	*pdata;
	void						*private_data;
	wait_queue_head_t				init_wait_queue;
	int						companion_status;
};

int fimc_is_companion_open(struct fimc_is_device_companion *device);
int fimc_is_companion_close(struct fimc_is_device_companion *device);
int fimc_is_companion_wait(struct fimc_is_device_companion *device);
int fimc_is_companion_runtime_suspend(struct device *dev);
int fimc_is_companion_runtime_resume(struct device *dev);

#endif
