/*
 * SAMSUNG EXYNOS5430 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/exynos_ion.h>

#include <asm/mach/arch.h>
#include <mach/regs-pmu.h>

#include <plat/cpu.h>

#include "common.h"

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>

static int __init sss_reserved_mem_setup(struct reserved_mem *rmem)
{
	return 0;
}

RESERVEDMEM_OF_DECLARE(sss_debug, "secure,sss-debug", sss_reserved_mem_setup);
#endif

const struct of_device_id of_iommu_bus_match_table[] = {
	{ .compatible = "samsung,exynos-iommu-bus", },
	{} /* Empty terminated list */
};

static void __init espresso7420_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static void __init espresso7420_dt_machine_init(void)
{
	of_platform_bus_probe(NULL, of_iommu_bus_match_table, NULL);
	exynos_pmu_init();
}

static char const *espresso7420_dt_compat[] __initdata = {
	"samsung,exynos7420",
	NULL
};

DT_MACHINE_START(ESPRESSO7420, "SAMSUNG Exynos7420")
	.map_io		= espresso7420_dt_map_io,
	.init_machine	= espresso7420_dt_machine_init,
	.dt_compat	= espresso7420_dt_compat,
	.restart        = exynos_restart,
MACHINE_END
