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

#ifndef FIMC_IS_DEVICE_OV5670_H
#define FIMC_IS_DEVICE_OV5670_H

#define SENSOR_OV5670_INSTANCE	1
#define SENSOR_OV5670_NAME		SENSOR_NAME_OV5670

int sensor_ov5670_probe(struct platform_device *pdev);

#endif
