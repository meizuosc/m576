/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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
#ifndef _MC_OPS_H_
#define _MC_OPS_H_

#include <linux/workqueue.h>
#include "fastcall.h"
#include "platform.h"

#define UUID_LENGTH			16

#define MC_STATUS_HALT			3
#define SYS_STATE_HALT			(4 << 8)

#define MC_EXT_INFO_ID_MCI_VERSION	0	/* Version of the MobiCore Control Interface (MCI) */
#define MC_EXT_INFO_ID_FLAGS		1	/* MobiCore control flags */
#define MC_EXT_INFO_ID_HALT_CODE	2	/* MobiCore halt condition code */
#define MC_EXT_INFO_ID_HALT_IP		3	/* MobiCore halt condition instruction pointer */
#define MC_EXT_INFO_ID_FAULT_CNT	4	/* MobiCore fault counter */
#define MC_EXT_INFO_ID_FAULT_CAUSE	5	/* MobiCore last fault cause */
#define MC_EXT_INFO_ID_FAULT_META	6	/* MobiCore last fault meta */
#define MC_EXT_INFO_ID_FAULT_THREAD	7	/* MobiCore last fault threadid */
#define MC_EXT_INFO_ID_FAULT_IP		8	/* MobiCore last fault instruction pointer */
#define MC_EXT_INFO_ID_FAULT_SP		9	/* MobiCore last fault stack pointer */
#define MC_EXT_INFO_ID_FAULT_ARCH_DFSR	10	/* MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_ADFSR	11	/* MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_DFAR	12	/* MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_IFSR	13	/* MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_AIFSR	14	/* MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_FAULT_ARCH_IFAR	15	/* MobiCore last fault ARM arch information */
#define MC_EXT_INFO_ID_MC_CONFIGURED	16	/* MobiCore configured by Daemon via fc_init flag */
#define MC_EXT_INFO_ID_MC_SCHED_STATUS	17	/* MobiCore scheduling status: idle/non-idle */
#define MC_EXT_INFO_ID_MC_STATUS	18	/* MobiCore runtime status: initialized, halted */
#define MC_EXT_INFO_ID_MC_EXC_PARTNER	19	/* MobiCore exception handler last partner */
#define MC_EXT_INFO_ID_MC_EXC_IPCPEER	20	/* MobiCore exception handler last peer */
#define MC_EXT_INFO_ID_MC_EXC_IPCMSG	21	/* MobiCore exception handler last IPC message */
#define MC_EXT_INFO_ID_MC_EXC_IPCDATA	22	/* MobiCore exception handler last IPC data */
#define MC_EXT_INFO_ID_MC_EXC_UUID_0	23	/* MobiCore excpetion handler UUID of halted TA or secure driver */
#define MC_EXT_INFO_ID_MC_EXC_UUID_1	24	/* MobiCore excpetion handler UUID of halted TA or secure driver */
#define MC_EXT_INFO_ID_MC_EXC_UUID_2	25	/* MobiCore excpetion handler UUID of halted TA or secure driver */
#define MC_EXT_INFO_ID_MC_EXC_UUID_3	26	/* MobiCore excpetion handler UUID of halted TA or secure driver */

int mc_yield(void);
int mc_nsiq(void);
int _nsiq(void);
uint32_t mc_get_version(void);

int mc_info(uint32_t ext_info_id, uint32_t *state, uint32_t *ext_info);
int mc_init(phys_addr_t base, uint32_t  nq_length, uint32_t mcp_offset,
		uint32_t  mcp_length);
#ifdef TBASE_CORE_SWITCHER
uint32_t mc_active_core(void);
int mc_switch_core(uint32_t core_num);
#endif

bool mc_fastcall(void *data);

int mc_fastcall_init(struct mc_context *context);
void mc_fastcall_destroy(void);

#endif /* _MC_OPS_H_ */
