/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <asm/page.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include <mach/exynos-powermode.h>

#include "cpuidle_profiler.h"

static bool profile_ongoing;
static struct workqueue_struct *profiler_wq;
static struct delayed_work profiler_work;
static int profile_power_mode;

static DEFINE_PER_CPU(struct cpuidle_profile_info, profile_info);
static struct cpuidle_profile_info cpd_info[NUM_CLUSTER];
static struct cpuidle_profile_info lpc_info;
static struct cpuidle_profile_info lpm_info;

/*********************************************************************
 *                         helper function                           *
 *********************************************************************/
#define state_entered(state)	(((int)state < (int)0) ? 0 : 1)

static void profile_start(struct cpuidle_profile_info *info , int state, ktime_t now)
{
	if (state_entered(info->cur_state))
		return;

	info->cur_state = state;
	info->last_entry_time = now;

	info->usage[state].entry_count++;
}

static void count_earlywakeup(struct cpuidle_profile_info *info , int state)
{
	if (!state_entered(info->cur_state))
		return;

	info->cur_state = -EINVAL;
	info->usage[state].early_wakeup_count++;
}

static void update_time(struct cpuidle_profile_info *info , int state, ktime_t now)
{
	s64 diff;

	if (!state_entered(info->cur_state))
		return;

	info->cur_state = -EINVAL;

	diff = ktime_to_us(ktime_sub(now, info->last_entry_time));
	info->usage[state].time += diff;
}

/*********************************************************************
 *                    sub state exclusive function                   *
 *********************************************************************/
#define is_state_cpd(state)		((state & CPD_STATE) ? 1 : 0)
#define to_cluster(cpu)			MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1)

static void cpd_profile_start(int state, ktime_t now, int cpu)
{
	if (!is_state_cpd(state))
		return;

	profile_start(&cpd_info[to_cluster(cpu)], 0, now);
}

static void cpd_earlywakeup(int cpu)
{
	count_earlywakeup(&cpd_info[to_cluster(cpu)], 0);
}

static void cpd_update_time(ktime_t now, int cpu)
{
	update_time(&cpd_info[to_cluster(cpu)], 0, now);
}

#define is_state_lpc(state)		((state & LPC_STATE) ? 1 : 0)

static void lpc_profile_start(int state, ktime_t now)
{
	if (!is_state_lpc(state))
		return;

	profile_start(&lpc_info, 0, now);
}

static void lpc_earlywakeup(void)
{
	count_earlywakeup(&lpc_info, 0);
}

static void lpc_update_time(ktime_t now)
{
	update_time(&lpc_info, 0, now);
}

#define get_lpm_substate(state)		(state >> LPM_SUB_STATE_OFFSET)

static void lpm_profile_start(int state, ktime_t now)
{
	if ((state & MAJOR_STATE) != LPM_STATE)
		return;

	profile_start(&lpm_info, get_lpm_substate(state), now);
}

static void lpm_earlywakeup(void)
{
	count_earlywakeup(&lpm_info, lpm_info.cur_state);
}

static void lpm_update_time(ktime_t now)
{
	update_time(&lpm_info, lpm_info.cur_state, now);
}

/*********************************************************************
 *                Information gathering function                     *
 *********************************************************************/
static DEFINE_SPINLOCK(substate_lock);

void cpuidle_profile_start(int cpu, int state)
{
	struct cpuidle_profile_info *info;
	ktime_t now;

	if (!profile_ongoing)
		return;

	now = ktime_get();
	info = &per_cpu(profile_info, cpu);

	profile_start(info, state & MAJOR_STATE, now);

	spin_lock(&substate_lock);
	cpd_profile_start(state, now, cpu);
	lpc_profile_start(state, now);
	lpm_profile_start(state, now);
	spin_unlock(&substate_lock);
}

void cpuidle_profile_finish(int cpu, int early_wakeup)
{
	struct cpuidle_profile_info *info;
	int state;
	ktime_t now;

	if (!profile_ongoing)
		return;

	info = &per_cpu(profile_info, cpu);
	state = info->cur_state;

	if (early_wakeup) {
		count_earlywakeup(info, state);

		spin_lock(&substate_lock);
		cpd_earlywakeup(cpu);
		lpc_earlywakeup();
		lpm_earlywakeup();
		spin_unlock(&substate_lock);

		/*
		 * If cpu cannot enter power mode, residency time
		 * should not be updated.
		 */
		return;
	}

	now = ktime_get();
	update_time(info, state, now);

	spin_lock(&substate_lock);
	cpd_update_time(now, cpu);
	lpc_update_time(now);
	lpm_update_time(now);
	spin_unlock(&substate_lock);
}

#define MAX_NUM_BLOCKER		2

static int lpa_blocker[MAX_NUM_BLOCKER];
void lpa_blocking_counter(int blocker)
{
	if (!profile_ongoing)
		return;

	if (blocker >= MAX_NUM_BLOCKER)
		return;

	lpa_blocker[blocker]++;
}

/*********************************************************************
 *                            Show result                            *
 *********************************************************************/
static ktime_t profile_start_time;
static ktime_t profile_finish_time;
static s64 profile_time;

int state_count;
/* enum value is used for cpu profiler parameter 0*/
enum {
	STATE0,		/* 0 */
	STATE1,		/* 1 */
	STATE2,		/* 2 */
	LPC,		/* 3 */
	LPM,		/* 4 */
	ALL     	/* 5 */
};

static char * sys_powerdown_str[NUM_SYS_POWERDOWN] = {
	"AFTR",
	"STOP",
	"DSTOP",
	"DSTOP_PSR",
	"LPD",
	"LPA",
	"ALPA",
	"SLEEP"
};

static char * lpa_blocker_str[MAX_NUM_BLOCKER] = {
	"IP",
	"REGISTER",
};

#define get_sys_powerdown_str(mode)	sys_powerdown_str[mode]
#define get_lpa_blocker_str(mode)	lpa_blocker_str[mode]

static int calculate_percent(s64 residency)
{
	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile_time);

	return residency;
}
static void show_state_n_result(int n)
{
		int i, cpu;
		struct cpuidle_profile_info *info;

		pr_info("[state%d]\n", n);
		pr_info("#cpu   #entry   #early      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
				info = &per_cpu(profile_info, cpu);
				pr_info("cpu%d   %5u   %5u   %10lluus   %3u%%\n", cpu,
						info->usage[n].entry_count,
						info->usage[n].early_wakeup_count,
						info->usage[n].time,
						calculate_percent(info->usage[n].time));
		}

		pr_info("\n");
		pr_info("[CPD] - Cluster Power Down\n");
		pr_info("#cluster    #entry   #early      #time     #ratio\n");
		for (i = 0; i < NUM_CLUSTER; i++) {
				pr_info("cluster%d    %5u    %5u  %10lluus    %3u%%\n", i,
						cpd_info[n].usage->entry_count,
						cpd_info[i].usage->early_wakeup_count,
						cpd_info[i].usage->time,
						calculate_percent(cpd_info[i].usage->time));
		}

		pr_info("\n");

}
static void show_lpc_result(void)
{
		pr_info("[LPC] - Low Power mode with Clock down\n");
		pr_info("            #entry   #early      #time     #ratio\n");
		pr_info("system      %5u    %5u  %10lluus    %3u%%\n",
					lpc_info.usage->entry_count,
					lpc_info.usage->early_wakeup_count,
					lpc_info.usage->time,
					calculate_percent(lpc_info.usage->time));

		pr_info("\n");

}
static void show_lpm_result(void)
{
		int i;
		pr_info("[LPM] - Low Power Mode\n");
		pr_info("#mode       #entry   #early      #time     #ratio\n");
		for (i = 0; i < NUM_SYS_POWERDOWN; i++) {
				pr_info("%-9s   %5u    %5u  %10lluus    %3u%%\n",
						get_sys_powerdown_str(i),
						lpm_info.usage[i].entry_count,
						lpm_info.usage[i].early_wakeup_count,
						lpm_info.usage[i].time,
						calculate_percent(lpm_info.usage[i].time));
		}

		pr_info("\n");

		pr_info("[LPA blockers]\n");
		for (i = 0; i < MAX_NUM_BLOCKER; i++)
				pr_info("%-9s: %d\n", get_lpa_blocker_str(i), lpa_blocker[i]);

		pr_info("\n");

}

static void show_result(void)
{
	int i, state_limit;
	state_limit = 3;
	pr_info("#######################################################################\n");
	pr_info("Profiling Time : %lluus\n", profile_time);

	switch (profile_power_mode) {
		case STATE0:
		case STATE1:
		case STATE2:
			show_state_n_result(profile_power_mode);
			break;
		case LPC:
			show_lpc_result();
			break;
		case LPM:
			show_lpm_result();
			break;
		case ALL:
		default:
			for(i=0; i< state_limit; i++)
				show_state_n_result(i);
			show_lpc_result();
			show_lpm_result();
			break;
	}

	pr_info("#######################################################################\n");
}

/*********************************************************************
 *                      Main profile function                        *
 *********************************************************************/
static void clear_profile_state_usage(struct cpuidle_profile_state_usage *usage)
{
	usage->entry_count = 0;
	usage->early_wakeup_count = 0;
	usage->time = 0;
}

static void clear_profile_info(struct cpuidle_profile_info *info, int state_num)
{
	int state;

	for (state = 0; state < state_num; state++)
		clear_profile_state_usage(&info->usage[state]);

	info->cur_state = -EINVAL;
	info->last_entry_time.tv64 = 0;
}

static void clear_time(void)
{
	int i;

	profile_start_time.tv64 = 0;
	profile_finish_time.tv64 = 0;

	for_each_possible_cpu(i)
		clear_profile_info(&per_cpu(profile_info, i), state_count);

	for (i = 0; i < NUM_CLUSTER; i++)
		clear_profile_info(&cpd_info[i], 1);

	clear_profile_info(&lpc_info, 1);
	clear_profile_info(&lpm_info, NUM_SYS_POWERDOWN);

	for (i = 0; i < MAX_NUM_BLOCKER; i++)
		lpa_blocker[i] = 0;
}

static void call_cpu_start_profile(void *p) {};
static void call_cpu_finish_profile(void *p) {};

static void cpuidle_profile_main_start(void)
{
	if (profile_ongoing) {
		pr_err("cpuidle profile is ongoing\n");
		return;
	}

	clear_time();
	profile_start_time = ktime_get();

	profile_ongoing = 1;

	/* Wakeup all cpus and clear own profile data to start profile */
	preempt_disable();
	clear_profile_info(&per_cpu(profile_info, smp_processor_id()), state_count);
	smp_call_function(call_cpu_start_profile, NULL, 1);
	preempt_enable();

	pr_info("cpuidle profile start\n");
}

static void cpuidle_profile_main_finish(struct work_struct *work)
{
	if (!profile_ongoing) {
		pr_err("CPUIDLE profile does not start yet\n");
		return;
	}

	pr_info("cpuidle profile finish\n");

	/* Wakeup all cpus to update own profile data to finish profile */
	preempt_disable();
	smp_call_function(call_cpu_finish_profile, NULL, 1);
	preempt_enable();

	profile_ongoing = 0;

	profile_finish_time = ktime_get();
	profile_time = ktime_to_us(ktime_sub(profile_finish_time,
						profile_start_time));

	show_result();
}

/*********************************************************************
 *                          Sysfs interface                          *
 *********************************************************************/
static ssize_t show_cpuidle_profile(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;

	if (profile_ongoing)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profile is ongoing\n");
	else {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"echo '<monitoring mode> <period>' > profile\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"<  monitoring mode  >\n 0   : state 0 \n 1   : state1  \n 2   : state2 \n 3   : lpc \n 4   : lpm \n 5  : all\n\n");
        ret += snprintf(buf + ret, PAGE_SIZE - ret,
                "< period > : up to 120 sec, unit : sec\n");
	}
	return ret;
}

static ssize_t store_cpuidle_profile(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int target_mode, prof_period_sec;
	if(!sscanf(buf, "%2d %3d", &target_mode, &prof_period_sec))
		return -EINVAL;

	if((target_mode >= 0) && (target_mode < 6) && (prof_period_sec <= 120)){
		profile_power_mode = target_mode;
		cpuidle_profile_main_start();
		queue_delayed_work_on(0, profiler_wq, &profiler_work,
							msecs_to_jiffies(prof_period_sec*1000));
	}else {
		pr_err("invalid value: please refer to 'cat profiler' cmd \n");
		return -EINVAL;
	}

	return count;
}

static struct kobj_attribute cpuidle_profile_attr =
	__ATTR(profile, 0644, show_cpuidle_profile, store_cpuidle_profile);

static struct attribute *cpuidle_profile_attrs[] = {
	&cpuidle_profile_attr.attr,
	NULL,
};

static const struct attribute_group cpuidle_profile_group = {
	.attrs = cpuidle_profile_attrs,
};


/*********************************************************************
 *                   Initialize cpuidle profiler                     *
 *********************************************************************/
static void __init cpuidle_profile_usage_init(void)
{
	int i, size;
	struct cpuidle_profile_info *info;

	if (!state_count) {
		pr_err("%s: cannot get the number of cpuidle state\n", __func__);
		return;
	}

	size = sizeof(struct cpuidle_profile_state_usage) * state_count;
	for_each_possible_cpu(i) {
		info = &per_cpu(profile_info, i);

		info->usage = kmalloc(size, GFP_KERNEL);
		if (!info->usage) {
			pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
			return;
		}
	}

	size = sizeof(struct cpuidle_profile_state_usage);

	for (i = 0; i < NUM_CLUSTER; i++) {
		cpd_info[i].usage = kmalloc(size, GFP_KERNEL);
		if (!cpd_info[i].usage) {
			pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
			return;
		}
	}

	lpc_info.usage = kmalloc(size, GFP_KERNEL);
	if (!lpc_info.usage) {
		pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
		return;
	}

	size = sizeof(struct cpuidle_profile_state_usage) * NUM_SYS_POWERDOWN;
	lpm_info.usage = kmalloc(size, GFP_KERNEL);
	if (!lpm_info.usage) {
		pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
		return;
	}
}

void __init cpuidle_profile_state_init(struct cpuidle_driver *drv)
{
	state_count = drv->state_count;

	cpuidle_profile_usage_init();
}

static int __init cpuidle_profile_init(void)
{
	struct class *class;
	struct device *dev;

	class = class_create(THIS_MODULE, "cpuidle");
	dev = device_create(class, NULL, 0, NULL, "cpuidle_profiler");

	if (sysfs_create_group(&dev->kobj, &cpuidle_profile_group)) {
		pr_err("CPUIDLE Profiler : error to create sysfs\n");
		return -EINVAL;
	}

	profiler_wq = create_freezable_workqueue("profiler_wq");
	if (IS_ERR(profiler_wq)) {
		class_destroy(class);
		pr_err("CPUIDLE Profiler : cannot create workqueue\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&profiler_work, cpuidle_profile_main_finish);

	return 0;
}
late_initcall(cpuidle_profile_init);
