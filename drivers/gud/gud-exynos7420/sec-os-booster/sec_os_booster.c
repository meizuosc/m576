/* drivers/gud/sec-os-ctrl/secos_booster.c
 *
 * Secure OS booster driver for Samsung Exynos
 *
 * Copyright (c) 2014 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/of_gpio.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <mach/secos_booster.h>

#include "platform.h"

#define MID_CPUFREQ	1700000

int mc_switch_core(uint32_t core_num);
void mc_set_schedule_policy(int core);
uint32_t mc_active_core(void);

unsigned int current_core;
struct timer_work {
	struct kthread_work work;
};

static struct pm_qos_request secos_booster_cluster1_qos;
static struct hrtimer timer;
static int max_cpu_freq;

static struct task_struct *mc_timer_thread;	/* Timer Thread task structure */
static DEFINE_KTHREAD_WORKER(mc_timer_worker);
static struct hrtimer mc_hrtimer;

static enum hrtimer_restart mc_hrtimer_func(struct hrtimer *timer)
{
	struct irq_desc *desc = irq_to_desc(MC_INTR_LOCAL_TIMER);

	if (desc->depth != 0)
		enable_irq(MC_INTR_LOCAL_TIMER);

	return HRTIMER_NORESTART;
}

static void mc_timer_work_func(struct kthread_work *work)
{
	hrtimer_start(&mc_hrtimer, ns_to_ktime((u64)LOCAL_TIMER_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
}

int mc_timer(void)
{
	struct timer_work t_work = {
		KTHREAD_WORK_INIT(t_work.work, mc_timer_work_func),
	};

	if (!queue_kthread_work(&mc_timer_worker, &t_work.work))
		return false;

	flush_kthread_work(&t_work.work);
	return true;
}

static int mc_timer_init(void)
{
	cpumask_t cpu;

	mc_timer_thread = kthread_create(kthread_worker_fn, &mc_timer_worker, "mc_timer");
	if (IS_ERR(mc_timer_thread)) {
		mc_timer_thread = NULL;
		pr_err("%s: timer thread creation failed!", __func__);
		return -EFAULT;
	}

	wake_up_process(mc_timer_thread);

	cpumask_setall(&cpu);
	cpumask_clear_cpu(DEFAULT_BIG_CORE, &cpu);
	set_cpus_allowed(mc_timer_thread, cpu);

	hrtimer_init(&mc_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mc_hrtimer.function = mc_hrtimer_func;

	return 0;
}

static void stop_wq(struct work_struct *work)
{
	int ret;

	ret = secos_booster_stop();
	if (ret)
		pr_err("%s: secos_booster_stop failed. err:%d\n", __func__, ret);

	return;
}

static DECLARE_WORK(stopwq, stop_wq);

static enum hrtimer_restart secos_booster_hrtimer_fn(struct hrtimer *timer)
{
	schedule_work_on(0, &stopwq);

	return HRTIMER_NORESTART;
}

int secos_booster_start(enum secos_boost_policy policy)
{
	int ret = 0;
	int freq;

	current_core = mc_active_core();

	/* migrate to big Core */
	if ((policy != MAX_PERFORMANCE) && (policy != MID_PERFORMANCE)
					&& (policy != MIN_PERFORMANCE)) {
		pr_err("%s: wrong secos boost policy:%d\n", __func__, policy);
		ret = -EINVAL;
		goto error;
	}

	/* cpufreq configuration */
	if (policy == MAX_PERFORMANCE)
		freq = max_cpu_freq;
	else if (policy == MID_PERFORMANCE)
		freq = MID_CPUFREQ;
	else
		freq = 0;
	pm_qos_update_request(&secos_booster_cluster1_qos, freq); /* KHz */

	if (!cpu_online(DEFAULT_BIG_CORE)) {
		pr_debug("%s: %d core is offline\n", __func__, DEFAULT_BIG_CORE);
		udelay(100);
		if (!cpu_online(DEFAULT_BIG_CORE)) {
			pr_debug("%s: %d core is offline\n", __func__, DEFAULT_BIG_CORE);
			pm_qos_update_request(&secos_booster_cluster1_qos, 0);
			ret = -EPERM;
			goto error;
		}
		pr_debug("%s: %d core is online\n", __func__, DEFAULT_BIG_CORE);
	}
	ret = mc_switch_core(DEFAULT_BIG_CORE);
	if (ret) {
		pr_err("%s: mc switch failed : err:%d\n", __func__, ret);
		pm_qos_update_request(&secos_booster_cluster1_qos, 0);
		goto error;
	}

	/* Change schedule policy */
	mc_set_schedule_policy(DEFAULT_BIG_CORE);

	/* Restore origin performance policy after default boost time */
	hrtimer_cancel(&timer);
	hrtimer_start(&timer, ns_to_ktime((u64)DEFAULT_SECOS_BOOST_TIME * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);

error:
	return ret;
}

int secos_booster_stop(void)
{
	int ret = 0;

	pr_debug("%s: mc switch to little core \n", __func__);
	mc_set_schedule_policy(current_core);

	ret = mc_switch_core(current_core);
	if (ret)
		pr_err("%s: mc switch core failed. err:%d\n", __func__, ret);

	pm_qos_update_request(&secos_booster_cluster1_qos, 0);

	return ret;
}

static int __init secos_booster_init(void)
{
	int ret;

	ret = mc_timer_init();
	if (ret) {
		pr_err("%s: mc timer init error :%d\n", __func__, ret);
		return ret;
	}

	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = secos_booster_hrtimer_fn;

	max_cpu_freq = cpufreq_quick_get_max(DEFAULT_BIG_CORE);

	pm_qos_add_request(&secos_booster_cluster1_qos, PM_QOS_CLUSTER1_FREQ_MIN, 0);

	return ret;
}
late_initcall(secos_booster_init);
