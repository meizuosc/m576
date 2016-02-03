/* linux/drivers/video/decon_display/decon_pm_core.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/clk-private.h>
#include <linux/exynos_iovmm.h>

#include <linux/platform_device.h>
#include "decon_display_driver.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"
#include "decon_pm_exynos.h"
#include "decon_pm.h"

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#include "regs-decon.h"
#include "decon_fb.h"
#else
#include "regs-fimd.h"
#include "fimd_fb.h"
#endif

#include <mach/cpufreq.h>

#include <../drivers/clk/samsung/clk.h>

#define GATE_LOCK_CNT 2

int decon_dbg = 5;
module_param(decon_dbg, int, 0644);

#define pm_info(fmt, args...)					\
	do {							\
		if (decon_dbg >= 5)				\
			printk("[INFO]%s: "fmt "\n",		\
				__func__, ##args);		\
	} while (0)

#define pm_debug(fmt, args...)					\
	do {							\
		if (decon_dbg >= 6)				\
			printk("[DEBUG]%s: "fmt "\n",		\
				__func__, ##args);		\
	} while (0)

#define call_pm_ops(q, ip, op, args...)			\
	(((q)->ip.ops->op) ? ((q)->ip.ops->op(args)) : 0)

#define call_block_pm_ops(q, op, args...)			\
		(((q)->pm_status.ops->op) ? ((q)->pm_status.ops->op(args)) : 0)

/* following values are for debugging */
unsigned int frame_done_count;
unsigned int te_count;

void display_block_clock_on(struct display_driver *dispdrv);
void display_block_clock_off(struct display_driver *dispdrv);
int display_hibernation_power_on(struct display_driver *dispdrv);
int display_hibernation_power_off(struct display_driver *dispdrv);
static void decon_clock_gating_handler(struct kthread_work *work);
static void decon_power_gating_handler(struct kthread_work *work);
static void request_dynamic_hotplug(bool hotplug);

struct pm_ops display_block_ops = {
	.clk_on		= display_block_clock_on,
	.clk_off 	= display_block_clock_off,
	.pwr_on		= display_hibernation_power_on,
	.pwr_off 	= display_hibernation_power_off,
};

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
extern struct pm_ops decon_pm_ops;
#ifdef CONFIG_DECON_MIC
extern struct pm_ops mic_pm_ops;
#endif
extern struct pm_ops dsi_pm_ops;
#endif

extern struct mipi_dsim_device *dsim_for_decon;

int disp_pm_set_plat_status(struct display_driver *dispdrv, bool platform_on)
{
	if (platform_on)
		dispdrv->platform_status = DISP_STATUS_PM1;
	else
		dispdrv->platform_status = DISP_STATUS_PM0;

	return 0;
}

int init_display_pm_status(struct display_driver *dispdrv) {
	dispdrv->pm_status.clock_enabled = 0;
	atomic_set(&dispdrv->pm_status.lock_count, 0);
	dispdrv->pm_status.clk_idle_count = 0;
	dispdrv->pm_status.pwr_idle_count = 0;
	dispdrv->platform_status = DISP_STATUS_PM0;
	disp_pm_set_plat_status(dispdrv, false);

	te_count = 0;
	frame_done_count = 0;
	return 0;
}

int init_display_pm(struct display_driver *dispdrv)
{
	init_display_pm_status(dispdrv);

	spin_lock_init(&dispdrv->pm_status.slock);
	mutex_init(&dispdrv->pm_status.pm_lock);
	mutex_init(&dispdrv->pm_status.clk_lock);
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	set_default_hibernation_mode(dispdrv);
	dispdrv->pm_status.hotplug_delay_msec = MAX_HOTPLUG_DELAY_MSEC;
#else
	dispdrv->pm_status.clock_gating_on = false;
	dispdrv->pm_status.power_gating_on = false;
	dispdrv->pm_status.hotplug_gating_on = false;
#endif

	init_kthread_worker(&dispdrv->pm_status.control_clock_gating);

	dispdrv->pm_status.control_clock_gating_thread = kthread_run(kthread_worker_fn,
			&dispdrv->pm_status.control_clock_gating,
			"decon_clk_thread");
	if (IS_ERR(dispdrv->pm_status.control_clock_gating_thread)) {
		int err = PTR_ERR(dispdrv->pm_status.control_clock_gating_thread);
		dispdrv->pm_status.control_clock_gating_thread = NULL;

		pr_err("failed to run control_clock_gating_thread\n");
		return err;
	}
	init_kthread_work(&dispdrv->pm_status.control_clock_gating_work,
		decon_clock_gating_handler);

	init_kthread_worker(&dispdrv->pm_status.control_power_gating);

	dispdrv->pm_status.control_power_gating_thread = kthread_run(kthread_worker_fn,
			&dispdrv->pm_status.control_power_gating,
			"decon_power_thread");
	if (IS_ERR(dispdrv->pm_status.control_power_gating_thread)) {
		int err = PTR_ERR(dispdrv->pm_status.control_power_gating_thread);
		dispdrv->pm_status.control_power_gating_thread = NULL;

		pr_err("failed to run control_power_gating_thread\n");
		return err;
	}
	init_kthread_work(&dispdrv->pm_status.control_power_gating_work,
		decon_power_gating_handler);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	dispdrv->pm_status.ops = &display_block_ops;
	dispdrv->decon_driver.ops = &decon_pm_ops;
	dispdrv->dsi_driver.ops = &dsi_pm_ops;
#ifdef CONFIG_DECON_MIC
	dispdrv->mic_driver.ops = &mic_pm_ops;
#endif
#endif
	return 0;
}

void disp_debug_power_info(void)
{
	struct display_driver *dispdrv = get_display_driver();

	pm_info("clk: %d,  \
		lock: %d, clk_idle: %d, pwr_idle: %d\n,	\
		output_on: %d, pwr_state: %d, vsync: %lld, frame_cnt: %d, te_cnt: %d",
		dispdrv->pm_status.clock_enabled,
		atomic_read(&dispdrv->pm_status.lock_count),
		dispdrv->pm_status.clk_idle_count,
		dispdrv->pm_status.pwr_idle_count,
		dispdrv->decon_driver.sfb->output_on,
		dispdrv->decon_driver.sfb->power_state,
		ktime_to_ns(dispdrv->decon_driver.sfb->vsync_info.timestamp),
		frame_done_count,
		te_count);
}

void disp_pm_gate_lock(struct display_driver *dispdrv, bool increase)
{
	if (increase)
		atomic_inc(&dispdrv->pm_status.lock_count);
	else if(atomic_read(&dispdrv->pm_status.lock_count) > 0)
		atomic_dec(&dispdrv->pm_status.lock_count);
}

static void init_gating_idle_count(struct display_driver *dispdrv)
{
	unsigned long flags;

	if (dispdrv->pm_status.clk_idle_count != 0 ||
		dispdrv->pm_status.pwr_idle_count != 0) {
		spin_lock_irqsave(&dispdrv->pm_status.slock, flags);
		dispdrv->pm_status.clk_idle_count = 0;
		dispdrv->pm_status.pwr_idle_count = 0;
		spin_unlock_irqrestore(&dispdrv->pm_status.slock, flags);
	}
}

void debug_function(struct display_driver *dispdrv, const char *buf)
{
	long input_time;
#ifndef CONFIG_FB_HIBERNATION_DISPLAY
	pm_info("%s: does not support", __func__);
	return;
#endif
	pm_info("calls [%s] to control gating function\n", buf);

	if (!kstrtol(buf, 10, &input_time)) {
		if (input_time == 0) {
			request_dynamic_hotplug(false);
			dispdrv->pm_status.hotplug_gating_on = false;
		} else {
			dispdrv->pm_status.hotplug_delay_msec = input_time;
			dispdrv->pm_status.hotplug_gating_on = true;
			if (dispdrv->decon_driver.sfb->power_state == POWER_HIBER_DOWN) {
				request_dynamic_hotplug(true);
			}
		}
		pm_info("Hotplug delay time is : %ld ms\n", input_time);
		pm_info("HOTPLUG GATING MODE: %s\n",
				dispdrv->pm_status.hotplug_gating_on == true? "TRUE":"FALSE");
		return;
	}
	if (!strcmp(buf, "clk-gate-on")) {
		dispdrv->pm_status.clock_gating_on = true;
	} else if (!strcmp(buf, "clk-gate-off")) {
		dispdrv->pm_status.clock_gating_on = false;
	} else if (!strcmp(buf, "pwr-gate-on")) {
		dispdrv->pm_status.power_gating_on = true;
	} else if (!strcmp(buf, "pwr-gate-off")) {
		dispdrv->pm_status.power_gating_on = false;
	} else if (!strcmp(buf, "hotplug-gate-on")) {
		dispdrv->pm_status.hotplug_gating_on = true;
	} else if (!strcmp(buf, "hotplug-gate-off")) {
		request_dynamic_hotplug(false);
		dispdrv->pm_status.hotplug_gating_on = false;
	} else {
		pr_err("INVALID parameter: '%s'\n", buf);
	}
	pm_info("CLOCK GATING MODE: %s\n",
		dispdrv->pm_status.clock_gating_on == true? "TRUE":"FALSE");
	pm_info("POWER GATING MODE: %s\n",
		dispdrv->pm_status.power_gating_on == true? "TRUE":"FALSE");
	pm_info("HOTPLUG GATING MODE: %s\n",
		dispdrv->pm_status.hotplug_gating_on == true? "TRUE":"FALSE");
}

int disp_pm_runtime_enable(struct display_driver *dispdrv)
{
#ifdef DISP_RUNTIME_PM_DEBUG
	pm_debug("runtime pm for disp-driver enabled\n");
#endif
	pm_runtime_enable(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_get_sync(struct display_driver *dispdrv)
{
	if (!dispdrv->pm_status.clock_gating_on) {
		pm_runtime_get_sync(dispdrv->display_driver);
		return 0;
	}

	init_gating_idle_count(dispdrv);

	/* guarantee clock and power gating */
	flush_kthread_worker(&dispdrv->pm_status.control_clock_gating);
	flush_kthread_worker(&dispdrv->pm_status.control_power_gating);
	pm_runtime_get_sync(dispdrv->display_driver);
	display_block_clock_on(dispdrv);

	return 0;
}

int disp_pm_runtime_put_sync(struct display_driver *dispdrv)
{
	if (!dispdrv->pm_status.clock_gating_on) {
		pm_runtime_put_sync(dispdrv->display_driver);
		return 0;
	}

	flush_kthread_worker(&dispdrv->pm_status.control_clock_gating);
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}

/* disp_pm_te_triggered - check clock gating or not.
 * this function is called in the TE interrupt handler */
void disp_pm_te_triggered(struct display_driver *dispdrv)
{
	te_count++;

	if (!dispdrv->pm_status.clock_gating_on) return;

	spin_lock(&dispdrv->pm_status.slock);
	if (dispdrv->platform_status > DISP_STATUS_PM0 &&
		atomic_read(&dispdrv->pm_status.lock_count) == 0) {
		if (dispdrv->pm_status.clock_enabled &&
			MAX_CLK_GATING_COUNT > 0) {
			++dispdrv->pm_status.clk_idle_count;
			if (dispdrv->pm_status.clk_idle_count > MAX_CLK_GATING_COUNT) {
				disp_pm_gate_lock(dispdrv, true);
				pm_debug("display_block_clock_off +");
				queue_kthread_work(&dispdrv->pm_status.control_clock_gating,
						&dispdrv->pm_status.control_clock_gating_work);
			}
		} else {
			++dispdrv->pm_status.pwr_idle_count;
			if (dispdrv->pm_status.power_gating_on &&
				dispdrv->pm_status.pwr_idle_count > MAX_PWR_GATING_COUNT) {
				disp_pm_gate_lock(dispdrv, true);
				queue_kthread_work(&dispdrv->pm_status.control_power_gating,
						&dispdrv->pm_status.control_power_gating_work);
			}
		}

	}
	spin_unlock(&dispdrv->pm_status.slock);
}

/* disp_pm_sched_power_on - it is called in the early start of the
 * fb_ioctl to exit HDM */
int disp_pm_sched_power_on(struct display_driver *dispdrv, unsigned int cmd)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	init_gating_idle_count(dispdrv);

	/* First WIN_CONFIG should be on clock and power-gating */
	if (dispdrv->platform_status < DISP_STATUS_PM1) {
		if (cmd == S3CFB_WIN_CONFIG)
			disp_pm_set_plat_status(dispdrv, true);
	}

	flush_kthread_worker(&dispdrv->pm_status.control_power_gating);
	if (sfb->power_state == POWER_HIBER_DOWN) {
		switch (cmd) {
		case S3CFB_PLATFORM_RESET:
			disp_pm_gate_lock(dispdrv, true);
			queue_kthread_work(&dispdrv->pm_status.control_power_gating,
				&dispdrv->pm_status.control_power_gating_work);
			/* Prevent next clock and power-gating */
			disp_pm_set_plat_status(dispdrv, false);
			break;
		case S3CFB_WIN_PSR_EXIT:
		case S3CFB_WIN_CONFIG:
			request_dynamic_hotplug(false);
			disp_pm_gate_lock(dispdrv, true);
			queue_kthread_work(&dispdrv->pm_status.control_power_gating,
				&dispdrv->pm_status.control_power_gating_work);
			break;
		default:
			return -EBUSY;
		}
	} else {
		switch (cmd) {
		case S3CFB_PLATFORM_RESET:
			/* Prevent next clock and power-gating */
			disp_pm_set_plat_status(dispdrv, false);
			break;
		}
	}

	return 0;
}

void disp_set_pm_status(int flag)
{
	struct display_driver *dispdrv = get_display_driver();
	if ((flag >= DISP_STATUS_PM0) || (flag < DISP_STATUS_PM_MAX))
		dispdrv->platform_status = flag;
}

/* disp_pm_add_refcount - it is called in the early start of the
 * update_reg_handler */
int disp_pm_add_refcount(struct display_driver *dispdrv)
{
	if (dispdrv->platform_status == DISP_STATUS_PM0) return 0;

	if (!dispdrv->pm_status.clock_gating_on) return 0;

	if (dispdrv->decon_driver.sfb->power_state == POWER_DOWN)
		return 0;

	init_gating_idle_count(dispdrv);

	flush_kthread_worker(&dispdrv->pm_status.control_clock_gating);
	flush_kthread_worker(&dispdrv->pm_status.control_power_gating);
	if (dispdrv->decon_driver.sfb->power_state == POWER_HIBER_DOWN) {
		request_dynamic_hotplug(false);
		display_hibernation_power_on(dispdrv);
	}

	display_block_clock_on(dispdrv);

	return 0;
}

/* disp_pm_dec_refcount - it is called at the DSI frame done */
int disp_pm_dec_refcount(struct display_driver *dispdrv)
{
	++frame_done_count;

	if (!dispdrv->pm_status.clock_gating_on)
		return 0;

	return 0;
}

static void decon_clock_gating_handler(struct kthread_work *work)
{
	struct display_driver *dispdrv = get_display_driver();

	if (dispdrv->pm_status.clk_idle_count > MAX_CLK_GATING_COUNT)
		display_block_clock_off(dispdrv);

	init_gating_idle_count(dispdrv);
	disp_pm_gate_lock(dispdrv, false);
	pm_debug("display_block_clock_off -");
}

static void decon_power_gating_handler(struct kthread_work *work)
{
	struct display_driver *dispdrv = get_display_driver();

	if (dispdrv->pm_status.pwr_idle_count > MAX_PWR_GATING_COUNT) {
		if (!check_camera_is_running()) {
			display_hibernation_power_off(dispdrv);
			init_gating_idle_count(dispdrv);
		}
	} else if (dispdrv->decon_driver.sfb->power_state == POWER_HIBER_DOWN) {
		display_hibernation_power_on(dispdrv);
	}
	disp_pm_gate_lock(dispdrv, false);
}

static int __display_hibernation_power_on(struct display_driver *dispdrv)
{
	/* already clocks are on */

	/* DSIM -> MIC -> DECON */
	call_pm_ops(dispdrv, dsi_driver, pwr_on, dispdrv);
#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, pwr_on, dispdrv);
#endif
	call_pm_ops(dispdrv, decon_driver, pwr_on, dispdrv);
	return 0;
}

static int __display_hibernation_power_off(struct display_driver *dispdrv)
{
	call_block_pm_ops(dispdrv, clk_on, dispdrv);

	/* DECON -> MIC -> DSIM */
	call_pm_ops(dispdrv, decon_driver, pwr_off, dispdrv);

#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, pwr_off, dispdrv);
#endif
	call_pm_ops(dispdrv, dsi_driver, pwr_off, dispdrv);

	return 0;
}

static void __display_block_clock_on(struct display_driver *dispdrv)
{
	/* DSIM -> MIC -> DECON -> SMMU */
	call_pm_ops(dispdrv, dsi_driver, clk_on, dispdrv);
#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, clk_on, dispdrv);
#endif
	call_pm_ops(dispdrv, decon_driver, clk_on, dispdrv);

#ifdef CONFIG_ION_EXYNOS
	if (dispdrv->platform_status > DISP_STATUS_PM0) {
		if (iovmm_activate(dispdrv->decon_driver.sfb->dev) < 0)
			pr_err("%s: failed to reactivate vmm\n", __func__);
	}
#endif
}

static int __display_block_clock_off(struct display_driver *dispdrv)
{
	if (get_display_line_count(dispdrv)) {
		pm_debug("wait until last frame is totally transferred %d:",
				get_display_line_count(dispdrv));
		return -EBUSY;
	}

	/* SMMU -> DECON -> MIC -> DSIM */
#ifdef CONFIG_ION_EXYNOS
	if (dispdrv->platform_status > DISP_STATUS_PM0)
		iovmm_deactivate(dispdrv->decon_driver.sfb->dev);
#endif
	call_pm_ops(dispdrv, decon_driver, clk_off, dispdrv);
#ifdef CONFIG_DECON_MIC
	call_pm_ops(dispdrv, mic_driver, clk_off, dispdrv);
#endif
	call_pm_ops(dispdrv, dsi_driver, clk_off, dispdrv);
	return 0;
}

static void request_dynamic_hotplug(bool hotplug)
{
#ifdef CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG
	struct display_driver *dispdrv = get_display_driver();
	if ((dispdrv->pm_status.hotplug_gating_on) &&
		(dispdrv->platform_status == DISP_STATUS_PM1))
		force_dynamic_hotplug(hotplug,
			dispdrv->pm_status.hotplug_delay_msec);
#endif
}

int display_hibernation_power_on(struct display_driver *dispdrv)
{
	int ret = 0;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	pm_info("##### +");
	disp_pm_gate_lock(dispdrv, true);
	mutex_lock(&dispdrv->pm_status.pm_lock);
	if (sfb->power_state == POWER_ON) {
		pr_info("%s, DECON are already power on state\n", __func__);
		goto done;
	}

	pm_runtime_get_sync(dispdrv->display_driver);
	__display_hibernation_power_on(dispdrv);
	sfb->power_state = POWER_ON;

done:
	mutex_unlock(&dispdrv->pm_status.pm_lock);
	disp_pm_gate_lock(dispdrv, false);
	pm_info("##### -\n");
	return ret;
}

int display_hibernation_power_off(struct display_driver *dispdrv)
{
	int ret = 0;
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	disp_pm_gate_lock(dispdrv, true);
	mutex_lock(&dispdrv->pm_status.pm_lock);
	if (sfb->power_state == POWER_DOWN) {
		pr_info("%s, DECON are already power off state\n", __func__);
		goto done;
	}

	if (atomic_read(&dispdrv->pm_status.lock_count) > GATE_LOCK_CNT) {
		pr_info("%s, DECON does not need power-off\n", __func__);
		goto done;
	}

	/* Should be clock on before check a H/W LINECNT */
	display_block_clock_on(dispdrv);
	if (get_display_line_count(dispdrv)) {
		pm_debug("wait until last frame is totally transferred %d:",
				get_display_line_count(dispdrv));
		goto done;
	}

	pm_info("##### +");
	sfb->power_state = POWER_HIBER_DOWN;
	__display_hibernation_power_off(dispdrv);
	disp_pm_runtime_put_sync(dispdrv);

	request_dynamic_hotplug(true);
	pm_info("##### -\n");
done:
	mutex_unlock(&dispdrv->pm_status.pm_lock);
	disp_pm_gate_lock(dispdrv, false);

	return ret;
}

void display_block_clock_on(struct display_driver *dispdrv)
{
	if (!get_display_power_status()) {
		pm_info("Requested a pm_runtime_get_sync, but power still off");
		pm_runtime_get_sync(dispdrv->display_driver);
		if (!get_display_power_status())
			BUG();
	}

	mutex_lock(&dispdrv->pm_status.clk_lock);
	if (!dispdrv->pm_status.clock_enabled) {
		pm_debug("+");
		__display_block_clock_on(dispdrv);
		dispdrv->pm_status.clock_enabled = 1;
		pm_debug("-");
	}
	mutex_unlock(&dispdrv->pm_status.clk_lock);
}

void display_block_clock_off(struct display_driver *dispdrv)
{
	mutex_lock(&dispdrv->pm_status.clk_lock);
	if (dispdrv->pm_status.clock_enabled) {
		pm_debug("+");
		if (__display_block_clock_off(dispdrv) == 0)
			dispdrv->pm_status.clock_enabled = 0;
		pm_debug("-");
	}
	mutex_unlock(&dispdrv->pm_status.clk_lock);
}
