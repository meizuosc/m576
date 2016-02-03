/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __EXYNOS_MEMOMY_COMPRESSOR_H
#define __EXYNOS_MEMOMY_COMPRESSOR_H

int memory_comp_start_compress(u32 disk_num, u32 nr_pages);
void memory_comp_init(u32 nr_disk, u32 **sswap_disk, u32 **comp_buf,
					u32 **comp_info, struct tasklet_struct *tasklet);
#endif

/* regs */
#ifndef __EXYNOS__REGS_MEMORY_COMPRESSOR_H
#define __EXYNOS__REGS_MEMORY_COMPRESSOR_H

#define CMD			0x0000
#define CMD_START		0x1
#define CMD_PAGES		16

#define IER			0x0004
#define IER_ENABLE		0x1

#define ISR			0x0008
#define ISR_CLEAR		1

#define DISK_ADDR		0x000C
#define COMPBUF_ADDR		0x0010
#define COMPINFO_ADDR		0x0014

#define CONTROL			0x0018
#define CONTROL_DRCG_DISABLE	8
#define CONTROL_ARCACHE		12
#define CONTROL_AWCACHE		16
#define CONTROL_THRESHOLD	20
#define CONTROL_AWUSER		28
#define CONTROL_ARUSER		29	

#define BUS_CONFIG		0x0010C
#define VERSION			0x0F00

#endif
