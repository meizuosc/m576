/* drivers/video/exynos_decon/lcd.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_LCD__
#define __DECON_LCD__

enum decon_psr_mode {
	DECON_VIDEO_MODE = 0,
	DECON_DP_PSR_MODE = 1,
	DECON_MIPI_COMMAND_MODE = 2,
};

enum mic_ver {
	MIC_VER_1_1,
	MIC_VER_1_2,
	MIC_VER_2_0,
};

enum type_of_ddi {
	TYPE_OF_SM_DDI = 0,
	TYPE_OF_MAGNA_DDI,
	TYPE_OF_NORMAL_DDI,
};

struct decon_lcd {
	enum decon_psr_mode mode;
	unsigned int vfp;
	unsigned int vbp;
	unsigned int hfp;
	unsigned int hbp;

	unsigned int vsa;
	unsigned int hsa;

	unsigned int xres;
	unsigned int yres;

	unsigned int width;
	unsigned int height;

	unsigned int hs_clk;
	unsigned int esc_clk;

	unsigned int fps;
	unsigned int mic_enabled;
	enum mic_ver mic_ver;
	enum type_of_ddi ddi_type;
};

#endif
