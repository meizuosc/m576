/*
 * Copyright (c) 2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

#include <mach/irqs.h>

/* MobiCore Interrupt. */
#define MC_INTR_SSIQ                          IRQ_SPI(214)

/* Enable mobicore mem traces */
#define MC_MEM_TRACES

#define COUNT_OF_CPUS 8

/* Values of MPIDR regs in  cpu0, cpu1, cpu2, cpu3*/
#define CPU_IDS {0x0100, 0x0101, 0x0102, 0x0103, 0x0000, 0x0001, 0x0002, 0x0003}

#define MC_VM_UNMAP

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

/* Enable LPAE */
#define LPAE_SUPPORT

#define TBASE_CORE_SWITCHER

#define MC_AARCH32_FC

/* Set Parameters for Secure OS Boosting */
#define DEFAULT_LITTLE_CORE		0
#define DEFAULT_BIG_CORE		4

#define MC_INTR_LOCAL_TIMER		(IRQ_SPI(116) + DEFAULT_BIG_CORE)

#define LOCAL_TIMER_PERIOD		50

#define DEFAULT_SECOS_BOOST_TIME	5000

#define DUMP_TBASE_HALT_STATUS

#endif /* _MC_DRV_PLATFORM_H_ */
