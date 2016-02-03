/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * NOC BUS Debugging Driver for Samsung EXYNOS SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_NOC_H
#define EXYNOS_NOC_H

#ifdef CONFIG_EXYNOS_NOC_DEBUGGING
extern void noc_notifier_chain_register(struct notifier_block *n);
#else
#define noc_notifier_chain_register(x)		do { } while(0)
#endif

#endif
