/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PLAT_MIPI_PHY_H
#define __PLAT_MIPI_PHY_H __FILE__

#include <mach/regs-pmu.h>

#define EXYNOS_PMU_MIPI_PHY_CONTROL(_nr)		(EXYNOS_PMU_MIPI_PHY_M4S4_CONTROL \
									+ ((_nr) * 0x04))

#if defined(CONFIG_SOC_EXYNOS7580)
#define S7P_MIPI_DPHY_CONTROL(n)	(EXYNOS_PMUREG(0x0710 + (n) * 0x4))
#define S7P_MIPI_DPHY_SYSREG		(EXYNOS7580_VA_SYSREG_DISP + 0x100C)
#else
#define S7P_MIPI_DPHY_CONTROL(n)	(EXYNOS_PMUREG(0x070C + (n) * 0x4))
#define S7P_MIPI_DPHY_SYSREG		(EXYNOS7420_VA_SYSREG + 0x2930)
#endif

extern int exynos5_csis_phy_enable(int id, bool on);
extern int exynos5_dism_phy_enable(int id, bool on);

#endif /* __PLAT_MIPI_PHY_H */
