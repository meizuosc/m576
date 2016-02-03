/*
 * exynos-mipi-lli-mphy.h - Exynos MIPI-LLI MPHY Header
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DRIVERS_EXYNOS_MIPI_LLI_MPHY_CAL_H
#define __DRIVERS_EXYNOS_MIPI_LLI_MPHY_CAL_H

enum lli_mphy_referclk_in_freq {
	LLI_MPHY_REFERENCE_CLK_IN_24MHZ  = 0x00ul,
	LLI_MPHY_REFERENCE_CLK_IN_26MHZ  = 0x01ul,
};

enum lli_mphy_ls_hs_mode {
	LLI_MPHY_PWM_MODE  = 0x01ul,
	LLI_MPHY_HS_MODE  = 0x02ul,
};

enum lli_mphy_gear {
	LLI_MPHY_G1  = 0x01ul,
	LLI_MPHY_G2  = 0x02ul,
	LLI_MPHY_G3  = 0x03ul,
	LLI_MPHY_G4  = 0x04ul,
	LLI_MPHY_G5  = 0x05ul,
};

enum lli_mphy_series {
	LLI_MPHY_RATEA  = 0x01ul,
	LLI_MPHY_RATEB  = 0x02ul,
};

struct refclk_info {
	enum lli_mphy_referclk_in_freq freq;
	bool is_shared;
};

#endif /* __DRIVERS_EXYNOS_MIPI_LLI_MPHY_CAL_H */
