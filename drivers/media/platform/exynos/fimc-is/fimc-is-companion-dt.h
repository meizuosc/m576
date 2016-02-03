/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_COMPANION_DT_H
#define FIMC_IS_COMPANION_DT_H

int fimc_is_sensor_parse_dt_with_companion(struct platform_device *pdev);
int fimc_is_companion_parse_dt(struct platform_device *pdev);
#endif

