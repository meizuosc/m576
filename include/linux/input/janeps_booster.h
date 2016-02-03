/* linux/drivers/media/video/samsung/tvout/s5p_cec_ctrl.c
 *
 * Copyright (c) 2009 Samsung Electronics
 *		http://www.samsung.com/
 *
 * cec interface file for Samsung TVOut driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _JANUARY_BOOSTER_H
#define _JANUARY_BOOSTER_H

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>

typedef int Coord_t;
typedef int x_coord;
typedef int y_coord;

typedef enum {
	CL_LIT = 0,
	CL_BIG,
	CL_END,
} cl_type;

typedef enum {
	PRESS = 0,
	CONTACT,
	RELEASE,
#ifdef CONFIG_KEY_BOOSTER
	DOWN,
	UP,
#endif
} event_type;

typedef enum {
	JAN_IDLED,
	JAN_NOTIFIED,
	JAN_LOAD_RPT,
	JAN_PERF_DLV,
	JAN_PERF_REQ
} janeps_seq;

struct qos_perf {
	unsigned int perf;	        // Frequency or # of CPU
	unsigned long pulse_timeout;	// If not zero, request_pm_qos_timeout
};

// Utilization data to HAL
struct k_data {
	event_type	status;
	Coord_t		x;
	Coord_t 	y;
	unsigned int	avg_load[CL_END];
	u64		peak_load[CL_END];
};

// Booster data from HAL
struct p_data {
	struct qos_perf cpu[CL_END];		// For PM_QOS_KFC_FREQ_MIN, PM_QOS_CPU_FREQ_MIN
	struct qos_perf bus;			// For PM_QOS_BUS_THROUGHPUT
	struct qos_perf device; 		// For PM_QOS_DEVICE_THROUGHT
	struct qos_perf graphic;		// For PM_QOS_GPU_THROUGHT
	struct qos_perf nrcpu[CL_END];
	struct qos_perf big_boost;		// 0: Non, 1: SemiBoost, 2: Boost
};

/*
 * Future work : 
 *     Dynamically register the DVFS table for
 *     encapsulating a DVFS table info (/w ASV) and
 *     calculating a frequency for PM_QOS
 */
#if 0
int janeps_register_perftable(int perf_class, void * table);
#endif
int janeps_input_report(event_type evt_type, Coord_t x, Coord_t y);

#endif //_JANUARY_BOOSTER_H