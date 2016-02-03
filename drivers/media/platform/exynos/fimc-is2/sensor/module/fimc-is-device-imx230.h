/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEVICE_IMX230_H
#define FIMC_IS_DEVICE_IMX230_H

#define SENSOR_IMX230_INSTANCE	0
#define SENSOR_IMX230_NAME		SENSOR_NAME_IMX230
#define GOVERN_OIS_IC
#define GOVERN_RANGING_IC
int sensor_IMX230_probe(struct platform_device *pdev);

#endif
