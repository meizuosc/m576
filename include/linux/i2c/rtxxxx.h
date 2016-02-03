/*
 * esxxx.h - header for esxxx I2C interface
 *
 * Copyright (C) 2011-2012 Audience, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __RTXXXX_GPIO_H__
#define __RTXXXX_GPIO_H__

#include <linux/types.h>

struct rtxxxx_platform_data {
	unsigned int	irq_base, irq_end;
	unsigned int	reset_gpio;
	unsigned int	wakeup_gpio;
	unsigned int	gpioa_gpio;
	unsigned int	gpiob_gpio;
	unsigned int	accdet_gpio;
	unsigned int	int_gpio;
	
	bool asrc_en;
	int jd_mode; 
	/*
	0: disable,
	1: 3.3v, 2 port,
	2: 3.3v, 1 port,
	3: 1.8v, 1 port,
	*/
	bool in2_diff;
	bool in3_diff;
	bool in4_diff;
	bool bclk_32fs[4];
};

#endif /* __RTXXXX_GPIO_H__*/
