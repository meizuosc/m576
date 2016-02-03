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

#include <mach/exynos-fimc-is-sensor.h>

#ifndef FIMC_IS_DT_H
#define FIMC_IS_DT_H

#define DT_READ_U32(node, key, value) do {\
		pprop = key; \
		temp = 0; \
		if (of_property_read_u32((node), key, &temp)) \
			pr_warn("%s: no property in the node.\n", pprop);\
		(value) = temp; \
	} while (0)

#define DT_READ_STR(node, key, value) do {\
		pprop = key; \
		if (of_property_read_string((node), key, &name)) \
			pr_warn("%s: no property in the node.\n", pprop);\
		(value) = name; \
	} while (0)

int get_pin_lookup_state(struct device *dev,
	struct pinctrl *pinctrl,
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX]);
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev);
int fimc_is_sensor_parse_dt(struct platform_device *pdev);
#endif
