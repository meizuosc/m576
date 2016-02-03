/*
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CPUIDLE_PROFILE_H
#define CPUIDLE_PROFILE_H __FILE__

#include <linux/ktime.h>
#include <linux/cpuidle.h>

#include <asm/cputype.h>

struct cpuidle_profile_state_usage {
	unsigned int entry_count;
	unsigned int early_wakeup_count;
	unsigned long long time;
};

struct cpuidle_profile_info {
	unsigned int cur_state;
	ktime_t last_entry_time;

	struct cpuidle_profile_state_usage *usage;
};

extern void cpuidle_profile_start(int cpu, int state);
extern void cpuidle_profile_finish(int cpuid, int early_wakeup);
extern void cpuidle_profile_state_init(struct cpuidle_driver *drv);

#define NUM_CLUSTER	2

#endif /* CPUIDLE_PROFILE_H */
