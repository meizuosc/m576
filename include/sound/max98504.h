/*
 * Platform data for MAX98504
 *
 * Copyright 2013-2014 Maxim Integrated Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __SOUND_MAX98504_PDATA_H__
#define __SOUND_MAX98504_PDATA_H__

enum max98504_mode {
	MODE_AIN,
	MODE_PCM,
	MODE_PDM,
};

struct max98504_pdata {
	int irq;
	u32 rx_mode;
	u32 tx_dither_en;
	u32 rx_dither_en;
	u32 meas_dc_block_en;
	u32 rx_flt_mode;
	u32 rx_ch_en;
	u32 tx_ch_en;

	u32 tx_hiz_ch_en;
	u32 tx_ch_src;

	u32 auth_en;
	u32 wdog_time_out;
};

#endif
