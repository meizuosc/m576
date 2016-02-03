/*
 * arch/arm/mach-exynos/resetreason.h
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef _MACH_EXYNOS_RESETREASON_H_
#define _MACH_EXYNOS_RESETREASON_H_

#include <linux/kmsg_dump.h>

#ifdef CONFIG_RESET_REASON_EXYNOS7420
const char *exynos_get_resetreason(void);
extern void record_reset_reason(enum kmsg_dump_reason reason);
extern void record_normal_reboot_reason(const char *cmd);
#else
const char *exynos_get_resetreason(void)
{
	return "unknown reset reason";
}
void record_reset_reason(enum kmsg_dump_reason reason) {return;}
void record_normal_reboot_reason(const char *cmd) {return;}
#endif

#endif
