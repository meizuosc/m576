/*
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/smc.h>
#include <linux/mm.h>

#include <linux/skbuff.h>
#include <linux/mipi-lli.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_lli.h"

//#define DEBUG_MODEM_IF_FLOW_CTRL

static int sleep_timeout = 100;
module_param(sleep_timeout, int, S_IRUGO);
MODULE_PARM_DESC(sleep_timeout, "LLI sleep timeout");

static int pm_enable = 1;
module_param(pm_enable, int, S_IRUGO);
MODULE_PARM_DESC(pm_enable, "LLI PM enable");

#ifdef DEBUG_CP_IRQSTRM
static int max_intr = 600;
module_param(max_intr, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(max_intr, "LLI maximum intr");

static int intr_cnt;
static unsigned long last_time, expired;

static inline void lli_mark_last_busy(void)
{
	intr_cnt = 0;
	ACCESS_ONCE(last_time) = jiffies;
	expired = ACCESS_ONCE(last_time) + msecs_to_jiffies(1000);
}

static inline bool lli_check_max_intr(void)
{
	if (++intr_cnt >= max_intr) {
		if (time_before(jiffies, expired)) {
			mif_info("cp_crash due to irq cnt %d\n", intr_cnt);
			intr_cnt = 0;
			modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);
			return true;
		}
		lli_mark_last_busy();
	}
	return false;
}
#else
static inline void lli_mark_last_busy(void) {}
static inline bool lli_check_max_intr(void) { return false; }
#endif

#ifdef LLI_SHMEM_DUMP
static void save_mem_dump(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	char *path = mld->dump_path;
	struct file *fp;
	struct utc_time t;

	get_utc_time(&t);
	snprintf(path, MIF_MAX_PATH_LEN, "%s/%s_%d%02d%02d_%02d%02d%02d.dump",
		MIF_LOG_DIR, ld->name, t.year, t.mon, t.day, t.hour, t.min,
		t.sec);

	fp = mif_open_file(path);
	if (!fp) {
		mif_err("%s: ERR! %s open fail\n", ld->name, path);
		return;
	}
	mif_err("%s: %s opened\n", ld->name, path);

	mif_save_file(fp, (char *)mld->base, mld->size);

	mif_close_file(fp);
}

/**
 * mem_dump_work
 * @ws: pointer to an instance of work_struct structure
 *
 * Performs actual file operation for saving a DPRAM dump.
 */
static void mem_dump_work(struct work_struct *ws)
{
	struct lli_link_device *shmd;

	shmd = container_of(ws, struct lli_link_device, dump_dwork.work);
	if (!shmd) {
		mif_err("ERR! no shmd\n");
		return;
	}

	save_mem_dump(shmd);
}
#endif

#ifndef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
static void print_pm_status(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct utc_time t;
	unsigned int magic;
	int ap_wakeup;
	int ap_status;
	int cp_wakeup;
	int cp_status;

	get_utc_time(&t);
	magic = get_magic(mld);
	ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	ap_status = gpio_get_value(mld->gpio_ap_status);
	cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);
	cp_status = gpio_get_value(mld->gpio_cp_status);

	/*
    ** PM {ap_wakeup:cp_wakeup:cp_status:ap_status:magic} <CALLER>
	*/
	pr_err(HMSU_FMT " %s: PM {%d:%d:%d:%d} %d <%pf>\n",
	    t.hour, t.min, t.sec, t.msec, ld->name,
	    ap_wakeup, cp_wakeup, cp_status, ap_status,
	    atomic_read(&mld->ref_cnt), CALLER);
}
#endif

static inline void change_irq_type(unsigned int irq, unsigned int value)
{
	unsigned int type;
	type = value ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH;
	irq_set_irq_type(irq, type);
}

static void send_ap2cp_irq(struct lli_link_device *mld, u16 mask)
{
#if 0
	int val;
	unsigned long flags;

	spin_lock_irqsave(&mld->sig_lock, flags);

	mipi_lli_send_interrupt(mask);

	val = gpio_get_value(mld->gpio_ipc_int2cp);
	val = 1 - val;

	gpio_set_value(mld->gpio_ipc_int2cp, val);

	spin_unlock_irqrestore(&mld->sig_lock, flags);
#endif
	mipi_lli_send_interrupt(mask);
}

#ifndef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
/**
@brief		finalize handling the PHONE_START command from CP

@param mld	the pointer to a lli_link_device instance
*/
static void finalize_cp_start(struct lli_link_device *mld)
{
	int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	int cp_status = gpio_get_value(mld->gpio_cp_status);

	change_irq_type(mld->irq_ap_wakeup.num, ap_wakeup);
	change_irq_type(mld->irq_cp_status.num, cp_status);

	if (ap_wakeup) {
		if (!wake_lock_active(&mld->ap_wlock))
			wake_lock(&mld->ap_wlock);
	} else {
		if (wake_lock_active(&mld->ap_wlock))
			wake_unlock(&mld->ap_wlock);
	}

	if (cp_status) {
		if (!wake_lock_active(&mld->ap_wlock))
			wake_lock(&mld->cp_wlock);
	} else {
		if (wake_lock_active(&mld->ap_wlock))
			wake_unlock(&mld->cp_wlock);
	}

	print_pm_status(mld);
}

static void release_cp_wakeup(struct work_struct *ws)
{
	struct lli_link_device *mld;
	int i;
	unsigned long flags;

	mld = container_of(ws, struct lli_link_device, cp_sleep_dwork.work);

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	spin_lock_irqsave(&mld->pm_lock, flags);
	i = atomic_read(&mld->ref_cnt);
	spin_unlock_irqrestore(&mld->pm_lock, flags);

#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	mif_info("%s: ref_cnt: %d\n", __func__, i);
#endif
	if (i > 0)
		goto reschedule;

	if (gpio_get_value(mld->gpio_ap_wakeup))
		goto reschedule;

	if (mipi_lli_get_link_status() != LLI_EVENT_WAITFORMOUNT) {
		gpio_set_value(mld->gpio_cp_wakeup, 0);
		gpio_set_value(mld->gpio_ap_status, 0);
	}
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	print_pm_status(mld);
#endif
	return;

reschedule:
	queue_delayed_work(system_nrt_wq, &mld->cp_sleep_dwork,
			   msecs_to_jiffies(sleep_timeout));
}
#endif

#ifdef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM

/**
@brief		forbid CP from going to sleep

Wakes up a CP if it can sleep and increases the "ref_cnt" counter in the
lli_link_device instance.

@param mld	the pointer to a lli_link_device instance

@remark		CAUTION!!! permit_cp_sleep() MUST be invoked after
		forbid_cp_sleep() success to decrease the "ref_cnt" counter.
*/
static void forbid_cp_sleep(struct lli_link_device *mld)
{
	struct modem_link_pm *pm = &mld->ld.pm;
	int ref_cnt;

	ref_cnt = atomic_inc_return(&mld->ref_cnt);
	mif_debug("ref_cnt %d\n", ref_cnt);

	if (ref_cnt > 1)
		return;

	if (pm->request_hold)
		pm->request_hold(pm);
}

/**
@brief	permit CP to go sleep

Decreases the "ref_cnt" counter in the lli_link_device instance if it can go
sleep and allows CP to go sleep only if the value of "ref_cnt" counter is less
than or equal to 0.

@param mld	the pointer to a lli_link_device instance

@remark		MUST be invoked after forbid_cp_sleep() success to decrease the
		"ref_cnt" counter.
*/
static void permit_cp_sleep(struct lli_link_device *mld)
{
	struct modem_link_pm *pm = &mld->ld.pm;
	int ref_cnt;

	ref_cnt = atomic_dec_return(&mld->ref_cnt);
	if (ref_cnt > 0)
		return;

	if (ref_cnt < 0) {
		mif_info("WARNING! ref_cnt %d < 0\n", ref_cnt);
		atomic_set(&mld->ref_cnt, 0);
		ref_cnt = 0;
	}

	if (pm->release_hold)
		pm->release_hold(pm);
}

static bool check_link_status(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct modem_ctl *mc = ld->mc;
	struct modem_link_pm *pm = &ld->pm;

	if (gpio_get_value(mld->gpio_cp_status) == 0)
		return false;

	if (mipi_lli_get_link_status() != LLI_MOUNTED)
		return false;

	if (cp_online(mc))
		return pm->link_active ? pm->link_active(pm) : true;

	return true;
}

static void pm_fail_cb(struct modem_link_pm *pm)
{
	mipi_lli_debug_info();
	modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);
}

static void pm_cp_fail_cb(struct modem_link_pm *pm)
{
	struct link_device *ld = container_of(pm, struct link_device, pm);
	struct lli_link_device *mld = container_of(ld, struct lli_link_device, ld);
	struct modem_ctl *mc = ld->mc;
	struct io_device *iod = mc->iod;

	unsigned long flags;

	if (mld->silent_cp_reset) {
		mif_err("%s: <by %pf> ALREADY in progress\n", ld->name, CALLER);
		return;
	}

	mipi_lli_debug_info();

	spin_lock_irqsave(&mc->lock, flags);

	if (cp_online(mc)) {
		spin_unlock_irqrestore(&mc->lock, flags);

		//if (mld->stop_pm)
		//	mld->stop_pm(mld);

		modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);
		return;
	}

	if (cp_booting(mc)) {
		iod->modem_state_changed(iod, STATE_OFFLINE);
		ld->reload(ld);
		spin_unlock_irqrestore(&mc->lock, flags);
		return;
	}

	spin_unlock_irqrestore(&mc->lock, flags);
}

static void start_pm(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct modem_link_pm *pm = &ld->pm;

	if (!pm->start)
		return;

	if (pm_enable)
		pm->start(pm, PM_EVENT_CP_BOOTING);
	else
		pm->start(pm, PM_EVENT_LOCK_ON);
}

static void stop_pm(struct lli_link_device *mld)
{
	struct modem_link_pm *pm = &mld->ld.pm;

	if (pm->stop)
		pm->stop(pm);
}

static int init_pm(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct modem_link_pm *pm = &ld->pm;
	struct link_pm_svc *pm_svc;
	int ret;

	atomic_set(&mld->ref_cnt, 0);

	pm_svc = NULL;

	ret = init_link_device_pm(ld, pm, pm_svc, pm_fail_cb, pm_cp_fail_cb);
	if (ret < 0)
		return ret;

	return 0;
}

static void lli_link_ready(struct link_device *ld)
{
	struct lli_link_device *mld = container_of(ld, struct lli_link_device, ld);

    mif_err("%s: PM %s <%pf>\n", ld->name, __func__, CALLER);
    stop_pm(mld);

    if (mld->forced_cp_crash)
        mld->forced_cp_crash = false;
}

static void lli_link_reset(struct link_device *ld)
{
    mif_err("%s: PM %s <%pf>\n", ld->name, __func__, CALLER);
    mipi_lli_intr_enable();
    mipi_lli_reset();
}

static void lli_link_off(struct link_device *ld)
{
	struct lli_link_device *mld = container_of(ld, struct lli_link_device, ld);

    mif_err("%s: PM %s <%pf>\n", ld->name, __func__, CALLER);
    mipi_lli_intr_disable();
    stop_pm(mld);
}

static bool lli_link_unmounted(struct link_device *ld)
{
    return (mipi_lli_get_link_status() == LLI_UNMOUNTED);
}

static bool lli_link_suspended(struct link_device *ld)
{
    return mipi_lli_suspended() ? true : false;
}

#else
/**
@brief		forbid CP from going to sleep

Wakes up a CP if it can sleep and increases the "ref_cnt" counter in the
lli_link_device instance.

@param mld	the pointer to a lli_link_device instance

@remark		CAUTION!!! permit_cp_sleep() MUST be invoked after
		forbid_cp_sleep() success to decrease the "ref_cnt" counter.
*/
static void forbid_cp_sleep(struct lli_link_device *mld)
{
	int ref_cnt;
	unsigned long flags;
	int cp_wakeup;

	spin_lock_irqsave(&mld->pm_lock, flags);

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	ref_cnt = atomic_inc_return(&mld->ref_cnt);
	mif_debug("ref_cnt %d\n", ref_cnt);

	cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);
	gpio_set_value(mld->gpio_cp_wakeup, 1);

	if (cp_wakeup == 0)
		print_pm_status(mld);

	spin_unlock_irqrestore(&mld->pm_lock, flags);
}

/**
@brief	permit CP to go sleep

Decreases the "ref_cnt" counter in the lli_link_device instance if it can go
sleep and allows CP to go sleep only if the value of "ref_cnt" counter is less
than or equal to 0.

@param mld	the pointer to a lli_link_device instance

@remark		MUST be invoked after forbid_cp_sleep() success to decrease the
		"ref_cnt" counter.
*/
static void permit_cp_sleep(struct lli_link_device *mld)
{
	int ref_cnt;
	unsigned long flags;

	spin_lock_irqsave(&mld->pm_lock, flags);

	ref_cnt = atomic_dec_return(&mld->ref_cnt);
	if (ref_cnt > 0)
		goto exit;

	if (ref_cnt < 0) {
		mif_info("WARNING! ref_cnt %d < 0\n", ref_cnt);
		atomic_set(&mld->ref_cnt, 0);
	}

exit:
	spin_unlock_irqrestore(&mld->pm_lock, flags);
}

static bool check_link_status(struct lli_link_device *mld)
{
	if (mipi_lli_get_link_status() != LLI_MOUNTED)
		return false;

	if (gpio_get_value(mld->gpio_cp_status) == 0)
		return false;

	return true;
}

static void start_pm(struct lli_link_device *mld)
{
	if (pm_enable) {
		int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
		int cp_status = gpio_get_value(mld->gpio_cp_status);

		print_pm_status(mld);

		change_irq_type(mld->irq_ap_wakeup.num, ap_wakeup);
		mif_enable_irq(&mld->irq_ap_wakeup);

		change_irq_type(mld->irq_cp_status.num, cp_status);
		mif_enable_irq(&mld->irq_cp_status);

		lli_mark_last_busy();
	} else wake_lock(&mld->ap_wlock);
}

static void stop_pm(struct lli_link_device *mld)
{
	print_pm_status(mld);

	mif_disable_irq(&mld->irq_ap_wakeup);
	mif_disable_irq(&mld->irq_cp_status);
}

/**
@brief	interrupt handler for a wakeup interrupt

1) Reads the interrupt value\n
2) Performs interrupt handling\n

@param irq	the IRQ number
@param data	the pointer to a data
*/
static irqreturn_t ap_wakeup_interrupt(int irq, void *data)
{
	struct lli_link_device *mld = (struct lli_link_device *)data;
	struct link_device *ld = &mld->ld;
	struct modem_ctl *mc = ld->mc;
	int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	int cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);

#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	mif_err("[+] Enter\n");
	mif_err("ap_wakeup = %d,  lli state = %d\n", ap_wakeup, mipi_lli_get_link_status());
#endif
	change_irq_type(irq, ap_wakeup);

	if (!cp_online(mc))
		goto exit;

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	if (ap_wakeup) {
		wake_lock(&mld->ap_wlock);
		if (!cp_wakeup)
			gpio_set_value(mld->gpio_cp_wakeup, 1);

		if (!wake_lock_active(&mld->ap_wlock))
			wake_lock(&mld->ap_wlock);

		if (mipi_lli_get_link_status() == LLI_UNMOUNTED) {
			if (!gpio_get_value(mld->gpio_cp_status)) {
				mif_info("reload is called\n");
				lli_link_reload(ld);
				mipi_lli_set_link_status(LLI_WAITFORMOUNT);
			}
		}

		if (!mipi_lli_suspended()) {
			mdelay(1);
			gpio_set_value(mld->gpio_ap_status, 1);
		} else {
			mif_info("abnormal case %x\n", mipi_lli_get_link_status());
			print_status(mc);
		}
	} else {
		if (wake_lock_active(&mld->ap_wlock))
			wake_unlock(&mld->ap_wlock);

		if (mipi_lli_get_link_status() & LLI_WAITFORMOUNT)
			mipi_lli_set_link_status(LLI_UNMOUNTED);
		queue_delayed_work(system_nrt_wq, &mld->cp_sleep_dwork,
				msecs_to_jiffies(sleep_timeout));
	}

exit:
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	print_status(mc);
	mif_err("[-] Exit\n");
#endif
	return IRQ_HANDLED;
}

static irqreturn_t cp_status_handler(int irq, void *data)
{
	struct lli_link_device *mld = (struct lli_link_device *)data;
	struct link_device *ld = &mld->ld;
	struct modem_ctl *mc = ld->mc;
	int cp_status = gpio_get_value(mld->gpio_cp_status);
	unsigned long flags;

	spin_lock_irqsave(&mld->pm_lock, flags);

	change_irq_type(irq, cp_status);

	if (!cp_online(mc))
		goto exit;

	if (cp_status) {
		if (!wake_lock_active(&mld->cp_wlock))
			wake_lock(&mld->cp_wlock);
	} else {
		gpio_set_value(mld->gpio_ap_status, 0);

		if (wake_lock_active(&mld->cp_wlock))
			wake_unlock(&mld->cp_wlock);
	}

exit:
#ifdef DEBUG_MODEM_IF_FLOW_CTRL
	print_status(mc);
#endif
	spin_unlock_irqrestore(&mld->pm_lock, flags);
	return IRQ_HANDLED;
}

static int init_pm(struct lli_link_device *mld)
{
	int err;
	unsigned int gpio;
	unsigned int irq_ap_wakeup;
	unsigned int irq_cp_status;
	unsigned long flags;

	gpio_set_value(mld->gpio_ap_status, 0);

	/*
	Retrieve GPIO#, IRQ#, and IRQ flags for PM
	*/
	gpio = mld->gpio_ap_wakeup;
	irq_ap_wakeup = gpio_to_irq(gpio);
	mif_err("CP2AP_WAKEUP GPIO:%d IRQ:%d\n", gpio, irq_ap_wakeup);

	gpio = mld->gpio_cp_wakeup;
	mif_err("AP2CP_WAKEUP GPIO:%d\n", gpio);

	gpio = mld->gpio_cp_status;
	irq_cp_status = gpio_to_irq(gpio);
	mif_err("CP2AP_STATUS GPIO:%d IRQ:%d\n", gpio, irq_cp_status);

	gpio = mld->gpio_ap_status;
	mif_err("AP2CP_STATUS GPIO:%d\n", gpio);

	/*
	Initialize locks, completions, bottom halves, etc.
	*/
	wake_lock_init(&mld->ap_wlock, WAKE_LOCK_SUSPEND, "lli_ap_wlock");

	wake_lock_init(&mld->cp_wlock, WAKE_LOCK_SUSPEND, "lli_cp_wlock");

	INIT_DELAYED_WORK(&mld->cp_sleep_dwork, release_cp_wakeup);

	spin_lock_init(&mld->pm_lock);
	atomic_set(&mld->ref_cnt, 0);

	/*
	Enable IRQs for PM
	*/
	print_pm_status(mld);

	flags = (IRQF_TRIGGER_HIGH | IRQF_ONESHOT);

	mif_init_irq(&mld->irq_ap_wakeup, irq_ap_wakeup, "lli_cp2ap_wakeup", flags);
	err = mif_request_threaded_irq(&mld->irq_ap_wakeup,	ap_wakeup_interrupt, NULL, mld);
	if (err)
		return err;
	mif_disable_irq(&mld->irq_ap_wakeup);

	mif_init_irq(&mld->irq_cp_status, irq_cp_status, "lli_cp2ap_status", flags);
	err = mif_request_irq(&mld->irq_cp_status, cp_status_handler, mld);
	if (err)
		return err;
	mif_disable_irq(&mld->irq_cp_status);

	return 0;
}
#endif

static void lli_link_reload(struct link_device *ld)
{
    mif_debug("%s: PM %s <%pf>\n", ld->name, __func__, CALLER);
    mipi_lli_reload();
}

/**
 * recv_int2ap
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of the CP-to-AP interrupt register.
 */
static inline u16 recv_int2ap(struct lli_link_device *mld)
{
	return 0;
}

/**
 * send_int2cp
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mask: value to be written to the AP-to-CP interrupt register
 */
static inline void send_int2cp(struct lli_link_device *mld, u16 mask)
{
	if (likely(mld->send_ap2cp_irq))
		mld->send_ap2cp_irq(mld, mask);
}

/**
 * get_shmem_status
 * @mld: pointer to an instance of lli_link_device structure
 * @dir: direction of communication (TX or RX)
 * @mst: pointer to an instance of mem_status structure
 *
 * Takes a snapshot of the current status of a SHMEM.
 */
static void get_shmem_status(struct lli_link_device *mld,
			enum direction dir, struct mem_status *mst)
{
#ifdef DEBUG_MODEM_IF
	getnstimeofday(&mst->ts);
#endif

	mst->dir = dir;
	mst->magic = get_magic(mld);
	mst->access = get_access(mld);
	mst->head[IPC_FMT][TX] = get_txq_head(mld, IPC_FMT);
	mst->tail[IPC_FMT][TX] = get_txq_tail(mld, IPC_FMT);
	mst->head[IPC_FMT][RX] = get_rxq_head(mld, IPC_FMT);
	mst->tail[IPC_FMT][RX] = get_rxq_tail(mld, IPC_FMT);
	mst->head[IPC_RAW][TX] = get_txq_head(mld, IPC_RAW);
	mst->tail[IPC_RAW][TX] = get_txq_tail(mld, IPC_RAW);
	mst->head[IPC_RAW][RX] = get_rxq_head(mld, IPC_RAW);
	mst->tail[IPC_RAW][RX] = get_rxq_tail(mld, IPC_RAW);
	mst->int2ap = recv_int2ap(mld);
	mst->int2cp = read_int2cp(mld);
}

static inline void update_rxq_tail_status(struct lli_link_device *mld,
					  int dev, struct mem_status *mst)
{
	mst->tail[dev][RX] = get_rxq_tail(mld, dev);
}

/**
 * handle_cp_crash
 * @mld: pointer to an instance of lli_link_device structure
 *
 * Actual handler for the CRASH_EXIT command from a CP.
 */
static void handle_cp_crash(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct modem_ctl *mc = ld->mc;
	struct io_device *iod;
	int i;
	unsigned long flags;

	if (mld->forced_cp_crash)
		mld->forced_cp_crash = false;

	ld->mode = LINK_MODE_ULOAD;

	/* Stop network interfaces */
	mif_netif_stop(ld);

	/* Purge the skb_txq in every IPC device (IPC_FMT, IPC_RAW, etc.) */
	for (i = 0; i < MAX_EXYNOS_DEVICES; i++)
		skb_queue_purge(ld->skb_txq[i]);

	spin_lock_irqsave(&mc->lock, flags);
	/* Change the modem state to STATE_CRASH_EXIT for the FMT IO device */
	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (iod)
		iod->modem_state_changed(iod, STATE_CRASH_EXIT);

	/* time margin for taking state changes by rild */
	mdelay(100);

	/* Change the modem state to STATE_CRASH_EXIT for the BOOT IO device */
	iod = link_get_iod_with_format(ld, IPC_BOOT);
	if (iod)
		iod->modem_state_changed(iod, STATE_CRASH_EXIT);
	spin_unlock_irqrestore(&mc->lock, flags);
}

/**
 * trigger_forced_cp_crash
 * @mld: pointer to an instance of lli_link_device structure
 *
 * Triggers an enforced CP crash.
 */
static void trigger_forced_cp_crash(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct utc_time t;

	if (mld->forced_cp_crash) {
		mif_err("%s: <by %pf> ALREADY in progress\n", ld->name, CALLER);
		return;
	}
	mld->forced_cp_crash = true;

	get_utc_time(&t);
	mif_err("%s: [%02d:%02d:%02d.%03d] <by %pf>\n",
		ld->name, t.hour, t.min, t.sec, t.msec, CALLER);

	if (!wake_lock_active(&mld->wlock))
		wake_lock(&mld->wlock);

#ifdef LLI_SHMEM_DUMP
	if (in_interrupt())
		queue_delayed_work(system_nrt_wq, &mld->dump_dwork, 0);
	else
		save_mem_dump(mld);
#endif
	gpio_set_value(mld->gpio_dump_noti, 1);

	return;
}

/**
 * cmd_crash_reset_handler
 * @mld: pointer to an instance of lli_link_device structure
 *
 * Handles the CRASH_RESET command from a CP.
 */
static void cmd_crash_reset_handler(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct modem_ctl *mc = ld->mc;
	struct io_device *iod = NULL;
	int i;
	struct utc_time t;
	unsigned long flags;

	ld->mode = LINK_MODE_ULOAD;

	if (!wake_lock_active(&mld->wlock))
		wake_lock(&mld->wlock);

	get_utc_time(&t);
	mif_err("%s: ERR! [%02d:%02d:%02d.%03d] Recv 0xC7 (CRASH_RESET)\n",
		ld->name, t.hour, t.min, t.sec, t.msec);

#ifdef LLI_SHMEM_DUMP
	queue_delayed_work(system_nrt_wq, &mld->dump_dwork, 0);
#endif
	/* Stop network interfaces */
	mif_netif_stop(ld);

	/* Purge the skb_txq in every IPC device (IPC_FMT, IPC_RAW, etc.) */
	for (i = 0; i < MAX_EXYNOS_DEVICES; i++)
		skb_queue_purge(ld->skb_txq[i]);

	mif_err("%s: Recv 0xC7 (CRASH_RESET)\n", ld->name);

	spin_lock_irqsave(&mc->lock, flags);
	/* Change the modem state to STATE_CRASH_RESET for the FMT IO device */
	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (iod)
		iod->modem_state_changed(iod, STATE_CRASH_RESET);

	/* time margin for taking state changes by rild */
	mdelay(100);

	/* Change the modem state to STATE_CRASH_RESET for the BOOT IO device */
	iod = link_get_iod_with_format(ld, IPC_BOOT);
	if (iod)
		iod->modem_state_changed(iod, STATE_CRASH_RESET);
	spin_unlock_irqrestore(&mc->lock, flags);
}

/**
 * cmd_crash_exit_handler
 * @mld: pointer to an instance of lli_link_device structure
 *
 * Handles the CRASH_EXIT command from a CP.
 */
static void cmd_crash_exit_handler(struct lli_link_device *mld)
{
	struct link_device *ld = &mld->ld;
	struct utc_time t;

	ld->mode = LINK_MODE_ULOAD;

	del_timer(&mld->crash_ack_timer);

	if (!wake_lock_active(&mld->wlock))
		wake_lock(&mld->wlock);

	get_utc_time(&t);
	mif_err("%s: ERR! [%02d:%02d:%02d.%03d] Recv 0xC9 (CRASH_EXIT)\n",
		ld->name, t.hour, t.min, t.sec, t.msec);
#ifdef LLI_SHMEM_DUMP
	queue_delayed_work(system_nrt_wq, &mld->dump_dwork, 0);
#endif

	handle_cp_crash(mld);
}

/**
 * cmd_phone_start_handler
 * @mld: pointer to an instance of lli_link_device structure
 *
 * Handles the PHONE_START command from a CP.
 */
static void cmd_phone_start_handler(struct lli_link_device *mld)
{
	int err;
	struct link_device *ld = &mld->ld;
	struct modem_ctl *mc = ld->mc;
	struct io_device *iod;
	unsigned long flags;

	mif_err("%s: Recv 0xC8 (CP_START)\n", ld->name);
	if (mld->start_pm)
		mld->start_pm(mld);

	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (!iod) {
		mif_err("%s: ERR! no FMT iod\n", ld->name);
		return;
	}

	msq_reset(&mld->rx_msq);

	err = init_shmem_ipc(mld);
	if (err)
		return;

	if (wake_lock_active(&mld->wlock))
		wake_unlock(&mld->wlock);

	if (mld->silent_cp_reset)
		mld->silent_cp_reset = false;


	mif_err("%s: Send 0xC2 (INIT_END)\n", ld->name);
	send_int2cp(mld, INT_CMD(INT_CMD_INIT_END));

	spin_lock_irqsave(&mc->lock, flags);
	iod->modem_state_changed(iod, STATE_ONLINE);
	spin_unlock_irqrestore(&mc->lock, flags);
}

/**
 * cmd_handler: processes a SHMEM command from a CP
 * @mld: pointer to an instance of lli_link_device structure
 * @cmd: SHMEM command from a CP
 */
static void cmd_handler(struct lli_link_device *mld, u16 cmd)
{
	struct link_device *ld = &mld->ld;

	switch (INT_CMD_MASK(cmd)) {
	case INT_CMD_CRASH_RESET:
		cmd_crash_reset_handler(mld);
		break;

	case INT_CMD_CRASH_EXIT:
		cmd_crash_exit_handler(mld);
		break;

	case INT_CMD_PHONE_START:
		cmd_phone_start_handler(mld);
		complete_all(&ld->init_cmpl);
		break;

	default:
		mif_err("%s: unknown command 0x%04X\n", ld->name, cmd);
	}
}

/**
 * ipc_rx_work
 * @ws: pointer to an instance of work_struct structure
 *
 * Invokes the recv method in the io_device instance to perform receiving IPC
 * messages from each skb.
 */
static void msg_rx_work(struct work_struct *ws)
{
	struct lli_link_device *mld;
	struct link_device *ld;
	struct io_device *iod;
	struct sk_buff *skb;
	int i;

	mld = container_of(ws, struct lli_link_device, msg_rx_dwork.work);
	ld = &mld->ld;

	for (i = 0; i < MAX_EXYNOS_DEVICES; i++) {
		while (1) {
			skb = skb_dequeue(ld->skb_rxq[i]);
			if (!skb)
				break;
			iod = skbpriv(skb)->iod;
			iod->recv_skb(iod, ld, skb);
		}
	}
}

/**
 * rx_ipc_frames
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @mst: pointer to an instance of mem_status structure
 *
 * Returns
 *   ret < 0  : error
 *   ret == 0 : ILLEGAL status
 *   ret > 0  : valid data
 *
 * Must be invoked only when there is data in the corresponding RXQ.
 *
 * Requires a recv_skb method in the io_device instance, so this function must
 * be used for only EXYNOS.
 */
static int rx_ipc_frames(struct lli_link_device *mld, int dev,
			struct circ_status *circ)
{
	struct link_device *ld = &mld->ld;
	struct io_device *iod;
	struct sk_buff_head *rxq = ld->skb_rxq[dev];
	struct sk_buff *skb;
	/**
	 * variables for the status of the circular queue
	 */
	u8 *src;
	u8 hdr[EXYNOS_HEADER_SIZE];
	/**
	 * variables for RX processing
	 */
	int qsize;	/* size of the queue			*/
	int rcvd;	/* size of data in the RXQ or error	*/
	int rest;	/* size of the rest data		*/
	int out;	/* index to the start of current frame	*/
	int tot;	/* total length including padding data	*/

	src = circ->buff;
	qsize = circ->qsize;
	out = circ->out;
	rcvd = circ->size;

	rest = circ->size;
	tot = 0;

	while (rest > 0) {
		u8 ch;

		/* Copy the header in the frame to the header buffer */
		circ_read(hdr, src, qsize, out, EXYNOS_HEADER_SIZE);

		/* Check the config field in the header */
		if (unlikely(!exynos_start_valid(hdr))) {
			mif_err("%s: ERR! %s INVALID config 0x%02X " \
				"(rcvd %d, rest %d)\n", ld->name,
				get_dev_name(dev), hdr[0], rcvd, rest);
			goto bad_msg;
		}

		/* Verify the total length of the frame (data + padding) */
		tot = exynos_get_total_len(hdr);
		if (unlikely(tot > rest)) {
			mif_err("%s: ERR! %s tot %d > rest %d (rcvd %d)\n",
				ld->name, get_dev_name(dev), tot, rest, rcvd);
			goto bad_msg;
		}

		/* Allocate an skb */
		skb = dev_alloc_skb(tot);
		if (!skb) {
			mif_err("%s: ERR! %s dev_alloc_skb(%d) fail\n",
				ld->name, get_dev_name(dev), tot);
			goto no_mem;
		}

		/* Set the attribute of the skb as "single frame" */
		skbpriv(skb)->single_frame = true;

		/* Read the frame from the RXQ */
		circ_read(skb_put(skb, tot), src, qsize, out, tot);

		ch = exynos_get_ch(skb->data);
		iod = link_get_iod_with_channel(ld, ch);
		if (!iod) {
		    mif_err("%s: ERR! iod for CH-%d\n", ld->name, ch);
		    kfree_skb(skb);
		    break;
		}

		skbpriv(skb)->lnk_hdr = iod->link_header;
		skbpriv(skb)->exynos_ch = ch;
		skbpriv(skb)->iod = iod;

		/* Store the skb to the corresponding skb_rxq */
		skb_queue_tail(rxq, skb);

		/* Calculate new out value */
		rest -= tot;
		out += tot;
		if (unlikely(out >= qsize))
			out -= qsize;
	}

	/* Update tail (out) pointer to empty out the RXQ */
	set_rxq_tail(mld, dev, circ->in);
	return rcvd;

no_mem:
	/* Update tail (out) pointer to the frame to be read in the future */
	set_rxq_tail(mld, dev, out);
	rcvd -= rest;
	return rcvd;

bad_msg:
#ifdef DEBUG_MODEM_IF
	mif_err("%s: ERR! rcvd:%d tot:%d rest:%d\n", ld->name, rcvd, tot, rest);
	pr_ipc(1, "shmem: ERR! CP2MIF", (src + out), (rest > 20) ? 20 : rest);
#endif
	return -EBADMSG;
}

static inline void done_req_ack(struct lli_link_device *mld, int dev)
{
	u16 mask;
#ifdef DEBUG_MODEM_IF
	struct mem_status mst;
#endif

	if (unlikely(mld->dev[dev]->req_ack_rcvd < 0))
		mld->dev[dev]->req_ack_rcvd = 0;

	if (likely(mld->dev[dev]->req_ack_rcvd == 0))
		return;

	mask = get_mask_res_ack(mld, dev);
	send_int2cp(mld, INT_NON_CMD(mask));
	mld->dev[dev]->req_ack_rcvd -= 1;

#ifdef DEBUG_MODEM_IF
	get_shmem_status(mld, TX, &mst);
	print_res_ack(mld, dev, &mst);
#endif
}

static inline void recv_res_ack(struct lli_link_device *mld,
				struct mem_status *mst)
{
	u16 intr = mst->int2ap;

	if (intr & get_mask_res_ack(mld, IPC_FMT)) {
#ifdef DEBUG_MODEM_IF
		mif_info("%s: recv FMT RES_ACK\n", mld->ld.name);
#endif
		complete(&mld->req_ack_cmpl[IPC_FMT]);
	}

	if (intr & get_mask_res_ack(mld, IPC_RAW)) {
#ifdef DEBUG_MODEM_IF
		mif_info("%s: recv RAW RES_ACK\n", mld->ld.name);
#endif
		complete(&mld->req_ack_cmpl[IPC_RAW]);
	}
}

static inline void recv_req_ack(struct lli_link_device *mld,
				struct mem_status *mst)
{
	u16 intr = mst->int2ap;

	if (intr & get_mask_req_ack(mld, IPC_FMT)) {
		mld->dev[IPC_FMT]->req_ack_rcvd += 1;
#ifdef DEBUG_MODEM_IF
		print_req_ack(mld, IPC_FMT, mst);
#endif
	}

	if (intr & get_mask_req_ack(mld, IPC_RAW)) {
		mld->dev[IPC_RAW]->req_ack_rcvd += 1;
#ifdef DEBUG_MODEM_IF
		print_req_ack(mld, IPC_RAW, mst);
#endif
	}
}

/**
 * msg_handler: receives IPC messages from every RXQ
 * @mld: pointer to an instance of lli_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * 1) Receives all IPC message frames currently in every IPC RXQ.
 * 2) Sends RES_ACK responses if there are REQ_ACK requests from a CP.
 * 3) Completes all threads waiting for the corresponding RES_ACK from a CP if
 *    there is any RES_ACK response.
 */
static void msg_handler(struct lli_link_device *mld, struct mem_status *mst)
{
	struct link_device *ld = &mld->ld;
	struct circ_status circ;
	int i = 0;
	int ret = 0;

	if (!ipc_active(mld)) {
		mif_err("%s: ERR! IPC is NOT ACTIVE!!!\n", ld->name);
		trigger_forced_cp_crash(mld);
		return;
	}

	for (i = 0; i < MAX_EXYNOS_DEVICES; i++) {
		/* Skip RX processing if there is no data in the RXQ */
		if (mst->head[i][RX] == mst->tail[i][RX]) {
			done_req_ack(mld, i);
			continue;
		}

		/* Get the size of data in the RXQ */
		ret = get_rxq_rcvd(mld, i, mst, &circ);
		if (unlikely(ret < 0)) {
			mif_err("%s: ERR! get_rxq_rcvd fail (err %d)\n",
				ld->name, ret);
			trigger_forced_cp_crash(mld);
			return;
		}

		/* Read data in the RXQ */
		ret = rx_ipc_frames(mld, i, &circ);
		if (unlikely(ret < 0)) {
			trigger_forced_cp_crash(mld);
			return;
		}

		if (ret < circ.size)
			break;

		/* Process REQ_ACK (At this point, the RXQ may be empty.) */
		done_req_ack(mld, i);
	}
}

/**
 * ipc_rx_task: processes a SHMEM command or receives IPC messages
 * @mld: pointer to an instance of lli_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * Invokes cmd_handler for commands or msg_handler for IPC messages.
 */
static void ipc_rx_task(unsigned long data)
{
	struct lli_link_device *mld = (struct lli_link_device *)data;

	while (1) {
		struct mem_status *mst;
		int i;
		u16 intr;

		mst = msq_get_data_slot(&mld->rx_msq);
		if (!mst)
			break;

		intr = mst->int2ap;

		/* Process a SHMEM command */
		if (unlikely(INT_CMD_VALID(intr))) {
			cmd_handler(mld, intr);
			continue;
		}

		/* Update tail variables with the current tail pointers */
		for (i = 0; i < MAX_EXYNOS_DEVICES; i++)
			update_rxq_tail_status(mld, i, mst);

		/* Check and receive RES_ACK from CP */
		if (unlikely(intr & INT_MASK_RES_ACK_SET))
			recv_res_ack(mld, mst);

		/* Check and receive REQ_ACK from CP */
		if (unlikely(intr & INT_MASK_REQ_ACK_SET))
			recv_req_ack(mld, mst);

		msg_handler(mld, mst);

		queue_delayed_work(system_nrt_wq, &mld->msg_rx_dwork, 0);
	}
}

/**
 * rx_udl_frames
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @mst: pointer to an instance of mem_status structure
 *
 * Returns
 *   ret < 0  : error
 *   ret == 0 : ILLEGAL status
 *   ret > 0  : valid data
 *
 * Must be invoked only when there is data in the corresponding RXQ.
 *
 * Requires a recv_skb method in the io_device instance, so this function must
 * be used for only EXYNOS.
 */
static int rx_udl_frames(struct lli_link_device *mld, int dev,
			struct circ_status *circ)
{
	struct link_device *ld = &mld->ld;
	struct io_device *iod;
	struct sk_buff *skb;
	int ret;
	int alloc_cnt = 0;
	/**
	 * variables for the status of the circular queue
	 */
	u8 *src;
	u8 hdr[EXYNOS_HEADER_SIZE];
	/**
	 * variables for RX processing
	 */
	int qsize;	/* size of the queue			*/
	int rcvd;	/* size of data in the RXQ or error	*/
	int rest;	/* size of the rest data		*/
	int out;	/* index to the start of current frame	*/
	int tot;	/* total length including padding data	*/

	src = circ->buff;
	qsize = circ->qsize;
	out = circ->out;
	rcvd = circ->size;
	rest = circ->size;
	tot = 0;
	while (rest > 0) {
		u8 ch;

		/* Copy the header in the frame to the header buffer */
		circ_read(hdr, src, qsize, out, EXYNOS_HEADER_SIZE);

		/* Check the config field in the header */
		if (unlikely(!exynos_start_valid(hdr))) {
			mif_err("%s: ERR! %s INVALID config 0x%02X " \
				"(rest %d, rcvd %d)\n", ld->name,
				get_dev_name(dev), hdr[0], rest, rcvd);
			pr_ipc(1, "UDL", (src + out), (rest > 20) ? 20 : rest);
			ret = -EBADMSG;
			goto exit;
		}

		/* Verify the total length of the frame (data + padding) */
		tot = exynos_get_total_len(hdr);
		if (unlikely(tot > rest)) {
			mif_err("%s: ERR! %s tot %d > rest %d (rcvd %d)\n",
				ld->name, get_dev_name(dev), tot, rest, rcvd);
			ret = -ENODATA;
			goto exit;
		}

		/* Allocate an skb */
		while(true) {
			skb = alloc_skb(tot + NET_SKB_PAD, GFP_KERNEL);
			if (!skb) {
				if (alloc_cnt++ < MAX_ALLOC_CNT) {
					mif_err("%s: ERR! [%d]alloc_skb fail\n", ld->name, tot+NET_SKB_PAD);
					usleep_range(1000, 1100);
					continue;
				} else {
					mif_err("%s: ERR! [%d]alloc_skb fail\n", ld->name, tot+NET_SKB_PAD);
					/* debug for memory check */
					show_mem(SHOW_MEM_FILTER_NODES);
					ret = -ENOMEM;
					goto free_skb;
				}
			} else {
				break;
			}
		}
		skb_reserve(skb, NET_SKB_PAD);

		/* Set the attribute of the skb as "single frame" */
		skbpriv(skb)->single_frame = true;

		/* Read the frame from the RXQ */
		circ_read(skb_put(skb, tot), src, qsize, out, tot);

		/* Pass the skb to an iod */
		ch = exynos_get_ch(skb->data);
		iod = link_get_iod_with_channel(ld, ch);
		if (!iod) {
			mif_err("%s: ERR! no iod for CH-%d\n", ld->name, ch);
			kfree_skb(skb);
			break;
		}

		skbpriv(skb)->lnk_hdr = iod->link_header;
		skbpriv(skb)->exynos_ch = ch;
		skbpriv(skb)->iod = iod;

#ifdef DEBUG_MODEM_IF
		if (!std_udl_with_payload(std_udl_get_cmd(skb->data))) {
			if (ld->mode == LINK_MODE_DLOAD) {
				pr_ipc(0, "[CP->AP] DL CMD", skb->data,
					(skb->len > 20 ? 20 : skb->len));
			} else {
				pr_ipc(0, "[CP->AP] UL CMD", skb->data,
					(skb->len > 20 ? 20 : skb->len));
			}
		}
#endif

		iod->recv_skb(iod, ld, skb);

		/* Calculate new out value */
		rest -= tot;
		out += tot;
		if (unlikely(out >= qsize))
			out -= qsize;
	}

	/* Update tail (out) pointer to empty out the RXQ */
	set_rxq_tail(mld, dev, circ->in);

	return rcvd;

free_skb:
	kfree_skb(skb);

exit:
	return ret;
}

/**
 * udl_rx_work
 * @ws: pointer to an instance of the work_struct structure
 *
 * Invokes the recv method in the io_device instance to perform receiving IPC
 * messages from each skb.
 */
static void udl_rx_work(struct work_struct *ws)
{
	struct lli_link_device *mld;
	struct link_device *ld;
	struct sk_buff_head *rxq;

	mld = container_of(ws, struct lli_link_device, udl_rx_dwork.work);
	ld = &mld->ld;
	rxq = ld->skb_rxq[IPC_RAW];

	while (1) {
		struct mem_status *mst = msq_get_free_slot(&mld->rx_msq);
		struct circ_status circ;

		get_shmem_status(mld, RX, mst);	// is it OK?
//		mst = msq_get_data_slot(&mld->rx_msq);
//		if (!mst)
//			break;
		update_rxq_tail_status(mld, IPC_RAW, mst);

		/* Exit the loop if there is no more data in the RXQ */
		if (mst->tail[IPC_RAW][RX] == mst->head[IPC_RAW][RX])
			break;

		/* Invoke an RX function only when there is data in the RXQ */
		if (get_rxq_rcvd(mld, IPC_RAW, mst, &circ) < 0) {
			mif_err("%s: ERR! get_rxq_rcvd fail\n", ld->name);
#ifdef DEBUG_MODEM_IF
			trigger_forced_cp_crash(mld);
#endif
			break;
		}

		if (rx_udl_frames(mld, IPC_RAW, &circ) < 0) {
			skb_queue_purge(rxq);
			break;
		}
	}
}

/**
 * udl_handler: receives BOOT/DUMP IPC messages from every RXQ
 * @mld: pointer to an instance of lli_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * 1) Receives all IPC message frames currently in every IPC RXQ.
 * 2) Sends RES_ACK responses if there are REQ_ACK requests from a CP.
 * 3) Completes all threads waiting for the corresponding RES_ACK from a CP if
 *    there is any RES_ACK response.
 */
static void udl_handler(struct lli_link_device *mld, struct mem_status *mst)
{
	u16 intr = mst->int2ap;

	/* Process a SHMEM command */
	if (unlikely(INT_CMD_VALID(intr))) {
		cmd_handler(mld, intr);
		return;
	}

	/* Schedule soft IRQ for RX */
	queue_delayed_work(system_nrt_wq, &mld->udl_rx_dwork, 0);

	/* Check and process RES_ACK */
	if (intr & INT_MASK_RES_ACK_SET) {
		if (intr & get_mask_res_ack(mld, IPC_RAW)) {
#ifdef DEBUG_MODEM_IF
			struct link_device *ld = &mld->ld;
			mif_info("%s: recv RAW RES_ACK\n", ld->name);
			print_circ_status(ld, IPC_RAW, mst);
#endif
			complete(&mld->req_ack_cmpl[IPC_RAW]);
		}
	}
}

#if 1
/* Functions for IPC/BOOT/DUMP TX */
#endif

/**
 * write_ipc_to_txq
 * @mld: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @circ: pointer to an instance of circ_status structure
 * @skb: pointer to an instance of sk_buff structure
 *
 * Must be invoked only when there is enough space in the TXQ.
 */
static void write_ipc_to_txq(struct lli_link_device *mld, int dev,
			struct circ_status *circ, struct sk_buff *skb)
{
	u32 qsize = circ->qsize;
	u32 in = circ->in;
	u8 *buff = circ->buff;
	u8 *src = skb->data;
	u32 len = skb->len;

	/* Print send data to CP */
	log_ipc_pkt(skb, LINK, TX);

	/* Write data to the TXQ */
	circ_write(buff, src, qsize, in, len);

	/* Update new head (in) pointer */
	set_txq_head(mld, dev, circ_new_pointer(qsize, in, len));
}

/**
 * xmit_ipc_msg
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * Tries to transmit IPC messages in the skb_txq of @dev as many as possible.
 *
 * Returns total length of IPC messages transmit or an error code.
 */
static int xmit_ipc_msg(struct lli_link_device *mld, int dev)
{
	struct link_device *ld = &mld->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	struct sk_buff *skb;
	unsigned long flags;
	struct circ_status circ;
	int space;
	int copied = 0;
	bool chk_nospc = false;
	int lli_mount_cnt=0;

	while (!(mipi_lli_get_link_status() & LLI_MOUNTED)) {
		udelay(50);
		if (lli_mount_cnt++ > LLI_MOUNT_TIMEOUT)
			return -EBUSY;
	}

	/* Acquire the spin lock for a TXQ */
	spin_lock_irqsave(&mld->tx_lock[dev], flags);

	while (1) {
		/* Get the size of free space in the TXQ */
		space = get_txq_space(mld, dev, &circ);
		if (unlikely(space < 0)) {
#ifdef DEBUG_MODEM_IF
			/* Trigger a enforced CP crash */
			trigger_forced_cp_crash(mld);
#endif
			/* Empty out the TXQ */
			reset_txq_circ(mld, dev);
			copied = -EIO;
			break;
		}

		skb = skb_dequeue(txq);
		if (unlikely(!skb))
			break;

		/* CAUTION : Uplink size is limited to 16KB and
				this limitation is used ONLY in North America Prj.
		   Check the free space size,
		  - FMT : comparing with skb->len
		  - RAW : check used buffer size  */
#ifdef CONFIG_MACH_GARDA
		if (dev == IPC_FMT) {
			if (unlikely(space < skb->len))
				chk_nospc = true;
		} else { /* dev == IPC_RAW */
			if (unlikely((SHM_4M_RAW_TX_BUFF_SZ - space) >= SHM_4M_MAX_UPLINK_SIZE))
				chk_nospc = true;
		}
#else
		chk_nospc = (space < skb->len) ? true : false;
#endif
		if (unlikely(chk_nospc)) {
#ifdef DEBUG_MODEM_IF
			struct mem_status mst;
#endif
			/* Set res_required flag for the "dev" */
			atomic_set(&mld->res_required[dev], 1);

			/* Take the skb back to the skb_txq */
			skb_queue_head(txq, skb);

			mif_err("%s: <by %pf> NOSPC in %s_TXQ" \
				"{qsize:%u in:%u out:%u} free:%u < len:%u\n",
				ld->name, CALLER, get_dev_name(dev),
				circ.qsize, circ.in, circ.out, space, skb->len);
#ifdef DEBUG_MODEM_IF
			get_shmem_status(mld, TX, &mst);
			print_circ_status(ld, dev, &mst);
#endif
			copied = -ENOSPC;
			break;
		}

		/* TX only when there is enough space in the TXQ */
		write_ipc_to_txq(mld, dev, &circ, skb);
		copied += skb->len;
		dev_kfree_skb_any(skb);
	}

	/* Release the spin lock */
	spin_unlock_irqrestore(&mld->tx_lock[dev], flags);

	return copied;
}

/**
 * wait_for_res_ack
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * 1) Sends an REQ_ACK interrupt for @dev to CP.
 * 2) Waits for the corresponding RES_ACK for @dev from CP.
 *
 * Returns the return value from wait_for_completion_interruptible_timeout().
 */
static int __maybe_unused wait_for_res_ack(struct lli_link_device *mld, int dev)
{
	struct link_device *ld = &mld->ld;
	struct completion *cmpl = &mld->req_ack_cmpl[dev];
	unsigned long timeout = msecs_to_jiffies(RES_ACK_WAIT_TIMEOUT);
	static int timeout_cnt=0;
	int ret;
	u16 mask;

#ifdef DEBUG_MODEM_IF
	mif_info("%s: send %s REQ_ACK\n", ld->name, get_dev_name(dev));
#endif

	mask = get_mask_req_ack(mld, dev);
	send_int2cp(mld, INT_NON_CMD(mask));

	/* ret < 0 if interrupted, ret == 0 on timeout */
	ret = wait_for_completion_interruptible_timeout(cmpl, timeout);
	if (ret < 0) {
		mif_err("%s: %s: wait_for_completion interrupted! (ret %d)\n",
			ld->name, get_dev_name(dev), ret);
		goto exit;
	}

	if (ret == 0) {
		struct mem_status mst;

		memset(&mst, 0, sizeof(struct mem_status));
		get_shmem_status(mld, TX, &mst);

		mif_err("%s: wait_for_completion TIMEOUT! (no %s_RES_ACK)\n",
			ld->name, get_dev_name(dev));

		/*
		** The TXQ must be checked whether or not it is empty, because
		** an interrupt mask can be overwritten by the next interrupt.
		*/
		if (mst.head[dev][TX] == mst.tail[dev][TX]) {
			ret = get_txq_buff_size(mld, dev);
#ifdef DEBUG_MODEM_IF
			mif_err("%s: %s_TXQ has been emptied\n",
				ld->name, get_dev_name(dev));
			print_circ_status(ld, dev, &mst);
#endif
		}

		if (timeout_cnt++ > MAX_TIMEOUT_CNT) {
			mif_err("%s: Timeout counting is Max. Change state crash\n", ld->name);
			trigger_forced_cp_crash(mld);
		}

		goto exit;
	}

	timeout_cnt = 0;
#ifdef DEBUG_MODEM_IF
	mif_info("%s: recv %s RES_ACK\n", ld->name, get_dev_name(dev));
#endif

exit:
	return ret;
}

/**
 * process_res_ack
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * 1) Tries to transmit IPC messages in the skb_txq with xmit_ipc_msg().
 * 2) Sends an interrupt to CP if there is no error from xmit_ipc_msg().
 * 3) Restarts SHMEM flow control if xmit_ipc_msg() returns -ENOSPC.
 *
 * Returns the return value from xmit_ipc_msg().
 */
static int process_res_ack(struct lli_link_device *mld, int dev)
{
	int ret;
	u16 mask;

	ret = xmit_ipc_msg(mld, dev);
	if (ret > 0) {
		mask = get_mask_send(mld, dev);
		send_int2cp(mld, INT_NON_CMD(mask));
		get_shmem_status(mld, TX, msq_get_free_slot(&mld->tx_msq));
	}

	if (ret >= 0)
		atomic_set(&mld->res_required[dev], 0);

	return ret;
}

/**
 * fmt_tx_work: performs TX for FMT IPC device under SHMEM flow control
 * @ws: pointer to an instance of the work_struct structure
 *
 * 1) Starts waiting for RES_ACK of FMT IPC device.
 * 2) Returns immediately if the wait is interrupted.
 * 3) Restarts SHMEM flow control if there is a timeout from the wait.
 * 4) Otherwise, it performs processing RES_ACK for FMT IPC device.
 */
static void fmt_tx_work(struct work_struct *ws)
{
	struct link_device *ld;
	struct lli_link_device *mld;
	int ret;

	ld = container_of(ws, struct link_device, fmt_tx_dwork.work);
	mld = to_lli_link_device(ld);

#ifdef CONFIG_LINK_DEVICE_SHMEM
	ret = wait_for_res_ack(mld, IPC_FMT);
	/* ret < 0 if interrupted */
	if (ret < 0)
		return;

	/* ret == 0 on timeout */
	if (ret == 0) {
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_FMT], 0);
		return;
	}
#endif
	ret = process_res_ack(mld, IPC_FMT);
	if (ret >= 0)
		return;

	/* At this point, ret < 0 */
	if (ret == -ENOSPC || ret == -EBUSY) {
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_FMT],
				   msecs_to_jiffies(1));
	}
}

/**
 * raw_tx_work: performs TX for RAW IPC device under SHMEM flow control.
 * @ws: pointer to an instance of the work_struct structure
 *
 * 1) Starts waiting for RES_ACK of RAW IPC device.
 * 2) Returns immediately if the wait is interrupted.
 * 3) Restarts SHMEM flow control if there is a timeout from the wait.
 * 4) Otherwise, it performs processing RES_ACK for RAW IPC device.
 */
static void raw_tx_work(struct work_struct *ws)
{
	struct link_device *ld;
	struct lli_link_device *mld;
	int ret;
#ifdef CONFIG_LINK_DEVICE_SHMEM
	unsigned long delay = usecs_to_jiffies(1000);
#endif

	ld = container_of(ws, struct link_device, raw_tx_dwork.work);
	mld = to_lli_link_device(ld);

#ifdef CONFIG_LINK_DEVICE_SHMEM
	ret = wait_for_res_ack(mld, IPC_RAW);
	/* ret < 0 if interrupted */
	if (ret < 0)
		return;

	/* ret == 0 on timeout */
	if (ret == 0) {
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_RAW], delay);
		return;
	}
#endif
	ret = process_res_ack(mld, IPC_RAW);
	if (ret >= 0) {
		mif_netif_wake(ld);
		return;
	}

	/* At this point, ret < 0 */
	if (ret == -ENOSPC || ret == -EBUSY) {
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[IPC_RAW],
				   msecs_to_jiffies(1));
	}
}

/**
 * shmem_send_ipc
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Tries to transmit IPC messages in the skb_txq with xmit_ipc_msg().
 * 2) Sends an interrupt to CP if there is no error from xmit_ipc_msg().
 * 3) Starts SHMEM flow control if xmit_ipc_msg() returns -ENOSPC.
 */
static int shmem_send_ipc(struct lli_link_device *mld, int dev)
{
	struct link_device *ld = &mld->ld;
	int ret;
	u16 mask;

	if (atomic_read(&mld->res_required[dev]) > 0) {
		mif_err("%s: %s_TXQ is full\n", ld->name, get_dev_name(dev));
		return 0;
	}

	ret = xmit_ipc_msg(mld, dev);
	if (likely(ret > 0)) {
		mask = get_mask_send(mld, dev);
		send_int2cp(mld, INT_NON_CMD(mask));
		get_shmem_status(mld, TX, msq_get_free_slot(&mld->tx_msq));
		goto exit;
	}

	/* If there was no TX, just exit */
	if (ret == 0)
		goto exit;

	/* At this point, ret < 0 */
	if (ret == -ENOSPC || ret == -EBUSY) {
		/*----------------------------------------------------*/
		/* mld->res_required[dev] was set in xmit_ipc_msg(). */
		/*----------------------------------------------------*/

		if (dev == IPC_RAW)
			mif_netif_stop(ld);

		queue_delayed_work(ld->tx_wq, ld->tx_dwork[dev],
				   msecs_to_jiffies(1));
	}

exit:
	return ret;
}

/**
 * shmem_try_send_ipc
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Enqueues an skb to the skb_txq for @dev in the link device instance.
 * 2) Tries to transmit IPC messages with shmem_send_ipc().
 */
static void shmem_try_send_ipc(struct lli_link_device *mld, int dev,
			struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = &mld->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	int ret;

	if (unlikely(txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err("%s: %s txq->qlen %d >= %d\n", ld->name,
			get_dev_name(dev), txq->qlen, MAX_SKB_TXQ_DEPTH);
		dev_kfree_skb_any(skb);
		return;
	}

	skb_queue_tail(txq, skb);

	ret = shmem_send_ipc(mld, dev);
	if (ret < 0) {
		mif_debug("%s->%s: WARN! shmem_send_ipc fail (err %d)\n",
			iod->name, ld->name, ret);
	}
}

static int shmem_send_udl_cmd(struct lli_link_device *mld, int dev,
			struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = &mld->ld;
	u8 *buff;
	u8 *src;
	u32 qsize;
	u32 in;
	int space;
	int tx_bytes;
	struct circ_status circ;

	if (iod->format == IPC_BOOT) {
		pr_ipc(0, "[AP->CP] DL CMD", skb->data,
			(skb->len > 20 ? 20 : skb->len));
	} else {
		pr_ipc(0, "[AP->CP] UL CMD", skb->data,
			(skb->len > 20 ? 20 : skb->len));
	}

	/* Get the size of free space in the TXQ */
	space = get_txq_space(mld, dev, &circ);
	if (space < 0) {
		reset_txq_circ(mld, dev);
		tx_bytes = -EIO;
		goto exit;
	}

	/* Get the size of data to be sent */
	tx_bytes = skb->len;

	/* Check the size of free space */
	if (space < tx_bytes) {
		mif_err("%s: NOSPC in %s_TXQ {qsize:%u in:%u out:%u}, " \
			"free:%u < tx_bytes:%u\n", ld->name, get_dev_name(dev),
			circ.qsize, circ.in, circ.out, space, tx_bytes);
		tx_bytes = -ENOSPC;
		goto exit;
	}

	/* Write data to the TXQ */
	buff = circ.buff;
	src = skb->data;
	qsize = circ.qsize;
	in = circ.in;
	circ_write(buff, src, qsize, in, tx_bytes);

	/* Update new head (in) pointer */
	set_txq_head(mld, dev, circ_new_pointer(qsize, circ.in, tx_bytes));

exit:
	dev_kfree_skb_any(skb);
	return tx_bytes;
}

static int shmem_send_udl_data(struct lli_link_device *mld, int dev)
{
	struct link_device *ld = &mld->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	struct sk_buff *skb;
	u8 *src;
	int tx_bytes;
	int copied;
	u8 *buff;
	u32 qsize;
	u32 in;
	u32 out;
	int space;
	struct circ_status circ;

	/* Get the size of free space in the TXQ */
	space = get_txq_space(mld, dev, &circ);
	if (space < 0) {
#ifdef DEBUG_MODEM_IF
		/* Trigger a enforced CP crash */
		trigger_forced_cp_crash(mld);
#endif
		/* Empty out the TXQ */
		reset_txq_circ(mld, dev);
		return -EFAULT;
	}

	buff = circ.buff;
	qsize = circ.qsize;
	in = circ.in;
	out = circ.out;
	space = circ.size;

	copied = 0;
	while (1) {
		skb = skb_dequeue(txq);
		if (!skb)
			break;

		/* Get the size of data to be sent */
		src = skb->data;
		tx_bytes = skb->len;

		/* Check the free space size comparing with skb->len */
		if (space < tx_bytes) {
			/* Set res_required flag for the "dev" */
			atomic_set(&mld->res_required[dev], 1);

			/* Take the skb back to the skb_txq */
			skb_queue_head(txq, skb);

			mif_info("NOSPC in RAW_TXQ {qsize:%u in:%u out:%u}, " \
				"space:%u < tx_bytes:%u\n",
				qsize, in, out, space, tx_bytes);
			break;
		}

		/*
		** TX only when there is enough space in the TXQ
		*/
		circ_write(buff, src, qsize, in, tx_bytes);

		copied += tx_bytes;
		in = circ_new_pointer(qsize, in, tx_bytes);
		space -= tx_bytes;

		dev_kfree_skb_any(skb);
	}

	/* Update new head (in) pointer */
	if (copied > 0) {
		in = circ_new_pointer(qsize, circ.in, copied);
		set_txq_head(mld, dev, in);
	}

	return copied;
}

/**
 * shmem_send_udl
 * @mld: pointer to an instance of lli_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Enqueues an skb to the skb_txq for @dev in the link device instance.
 * 2) Tries to transmit IPC messages in the skb_txq by invoking xmit_ipc_msg()
 *    function.
 * 3) Sends an interrupt to CP if there is no error from xmit_ipc_msg().
 * 4) Starts SHMEM flow control if xmit_ipc_msg() returns -ENOSPC.
 */
static void shmem_send_udl(struct lli_link_device *mld, int dev,
			struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = &mld->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	struct completion *cmpl = &mld->req_ack_cmpl[dev];
	struct std_dload_info *dl_info = &mld->dl_info;
	struct mem_status mst;
	u32 timeout = msecs_to_jiffies(RES_ACK_WAIT_TIMEOUT);
	u32 udl_cmd;
	int ret;
	u16 mask = get_mask_req_ack(mld, dev) | get_mask_send(mld, dev);

	memset(&mst, 0, sizeof(struct mem_status));

	udl_cmd = std_udl_get_cmd(skb->data);
	if (iod->format == IPC_DUMP || !std_udl_with_payload(udl_cmd)) {
		ret = shmem_send_udl_cmd(mld, dev, iod, skb);
		if (ret > 0)
			send_int2cp(mld, INT_NON_CMD(mask));
		else
			mif_err("ERR! shmem_send_udl_cmd fail (err %d)\n", ret);
		goto exit;
	}

	skb_queue_tail(txq, skb);
	if (txq->qlen < dl_info->num_frames)
		goto exit;

	while (1) {
		ret = shmem_send_udl_data(mld, dev);
		if (ret < 0) {
			mif_err("ERR! shmem_send_udl_data fail (err %d)\n", ret);
			skb_queue_purge(txq);
			break;
		}

		if (skb_queue_empty(txq)) {
			send_int2cp(mld, INT_NON_CMD(mask));
			break;
		}

		send_int2cp(mld, INT_NON_CMD(mask));

		do {
			ret = wait_for_completion_timeout(cmpl, timeout);
			get_shmem_status(mld, TX, &mst);
		} while (mst.head[dev][TX] != mst.tail[dev][TX]);
	}

exit:
	return;
}

/**
 * shmem_send
 * @ld: pointer to an instance of the link_device structure
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * Returns the length of data transmitted or an error code.
 *
 * Normal call flow for an IPC message:
 *   shmem_try_send_ipc -> shmem_send_ipc -> xmit_ipc_msg -> write_ipc_to_txq
 *
 * Call flow on congestion in a IPC TXQ:
 *   shmem_try_send_ipc -> shmem_send_ipc -> xmit_ipc_msg ,,, queue_delayed_work
 *   => xxx_tx_work -> wait_for_res_ack
 *   => msg_handler
 *   => process_res_ack -> xmit_ipc_msg (,,, queue_delayed_work ...)
 */
static int shmem_send(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb)
{
	struct lli_link_device *mld = to_lli_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	int dev = iod->format;
	int len = skb->len;

	if (cp_online(mc) && mld->forbid_cp_sleep)
		mld->forbid_cp_sleep(mld);

	switch (dev) {
	case IPC_FMT:
	case IPC_RAW:
		if (likely(ld->mode == LINK_MODE_IPC)) {
			if (unlikely(mld->forced_cp_crash)) {
				mif_err("%s:%s->%s: ERR! Forced CP Crash ...\n",
					ld->name, iod->name, mc->name);
				dev_kfree_skb_any(skb);
			} else {
				shmem_try_send_ipc(mld, dev, iod, skb);
			}
		} else {
			mif_err("%s:%s->%s: ERR! ld->mode != LINK_MODE_IPC\n",
				ld->name, iod->name, mc->name);
			dev_kfree_skb_any(skb);
		}
		break;

	case IPC_BOOT:
	case IPC_DUMP:
		shmem_send_udl(mld, IPC_RAW, iod, skb);
		break;

	default:
		mif_err("%s:%s->%s: ERR! Invalid IOD (format %d)\n",
			ld->name, iod->name, mc->name, dev);
		dev_kfree_skb_any(skb);
		len = -ENODEV;
		break;
	}

	if (cp_online(mc) && mld->permit_cp_sleep)
		mld->permit_cp_sleep(mld);

	return len;
}

int shmem_dload_start(struct link_device *ld, struct io_device *iod)
{
	struct lli_link_device *mld = to_lli_link_device(ld);
	u32 magic;

	ld->mode = LINK_MODE_DLOAD;
	clear_shmem_map(mld);
	msq_reset(&mld->rx_msq);
	set_magic(mld, SHM_BOOT_MAGIC);
	magic = get_magic(mld);

	if (magic != SHM_BOOT_MAGIC) {
		mif_err("%s: ERR! magic 0x%08X != SHM_BOOT_MAGIC 0x%08X\n",
			ld->name, magic, SHM_BOOT_MAGIC);
		return -EFAULT;
	}

	return 0;
}

/**
 * shmem_set_dload_info
 * @ld: pointer to an instance of link_device structure
 * @iod: pointer to an instance of io_device structure
 * @arg: pointer to an instance of std_dload_info structure in "user" memory
 *
 */
static int shmem_set_dload_info(struct link_device *ld, struct io_device *iod,
			unsigned long arg)
{
	struct lli_link_device *mld = to_lli_link_device(ld);
	struct std_dload_info *dl_info = &mld->dl_info;
	int ret;

	ret = copy_from_user(dl_info, (void __user *)arg,
			sizeof(struct std_dload_info));
	if (ret) {
		mif_err("ERR! copy_from_user fail!\n");
		return -EFAULT;
	}

	return 0;
}

static int shmem_force_dump(struct link_device *ld, struct io_device *iod)
{
	struct lli_link_device *mld = to_lli_link_device(ld);
	mif_err("+++\n");
	trigger_forced_cp_crash(mld);
	mif_err("---\n");
	return 0;
}

static int shmem_dump_start(struct link_device *ld, struct io_device *iod)
{
	struct lli_link_device *mld = to_lli_link_device(ld);

	ld->mode = LINK_MODE_ULOAD;

	clear_shmem_map(mld);
	msq_reset(&mld->rx_msq);

	mif_err("%s: magic = 0x%08X\n", ld->name, SHM_DUMP_MAGIC);
	set_magic(mld, SHM_DUMP_MAGIC);

	return 0;
}

static void remap_4mb_map_to_ipc_dev(struct lli_link_device *mld)
{
	struct shmem_4mb_phys_map *map;
	struct shmem_ipc_device *dev;

	map = (struct shmem_4mb_phys_map *)mld->base;

	/* Magic code and access enable fields */
	mld->ipc_map.magic = (u32 __iomem *)&map->magic;
	mld->ipc_map.access = (u32 __iomem *)&map->access;

	/* FMT */
	dev = &mld->ipc_map.dev[IPC_FMT];

	memmove(dev->name, "FMT", strlen("FMT"));

	dev->id = IPC_FMT;

	dev->txq.head = (u32 __iomem *)&map->fmt_tx_head;
	dev->txq.tail = (u32 __iomem *)&map->fmt_tx_tail;
	dev->txq.buff = (u8 __iomem *)&map->fmt_tx_buff[0];
	dev->txq.size = SHM_4M_FMT_TX_BUFF_SZ;

	dev->rxq.head = (u32 __iomem *)&map->fmt_rx_head;
	dev->rxq.tail = (u32 __iomem *)&map->fmt_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&map->fmt_rx_buff[0];
	dev->rxq.size = SHM_4M_FMT_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_F;
	dev->mask_res_ack = INT_MASK_RES_ACK_F;
	dev->mask_send    = INT_MASK_SEND_F;

	/* RAW */
	dev = &mld->ipc_map.dev[IPC_RAW];

	memmove(dev->name, "RAW", strlen("RAW"));
	dev->id = IPC_RAW;

	dev->txq.head = (u32 __iomem *)&map->raw_tx_head;
	dev->txq.tail = (u32 __iomem *)&map->raw_tx_tail;
	dev->txq.buff = (u8 __iomem *)&map->raw_tx_buff[0];
	dev->txq.size = SHM_4M_RAW_TX_BUFF_SZ;

	dev->rxq.head = (u32 __iomem *)&map->raw_rx_head;
	dev->rxq.tail = (u32 __iomem *)&map->raw_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&map->raw_rx_buff[0];
	dev->rxq.size = SHM_4M_RAW_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_R;
	dev->mask_res_ack = INT_MASK_RES_ACK_R;
	dev->mask_send    = INT_MASK_SEND_R;
}

/**
  @brief      setup the logical map for an IPC region

  @param mld  the pointer to a lli_link_device instance
  @param start    the physical address of an IPC region
  @param size the size of the IPC region
 */
int mem_setup_ipc_map(struct lli_link_device *mld, unsigned long start,
			unsigned long size)
{
	struct link_device *ld = &mld->ld;
	void __iomem *base;

	if (!mld->remap_region) {
		mif_err("%s: ERR! NO remap_region method\n", ld->name);
		return -EFAULT;
	}

	base = mld->remap_region(start, size);
	if (!base) {
		mif_err("%s: ERR! remap_region fail\n", ld->name);
		return -EINVAL;
	}

	mif_err("%s: IPC_RGN phys_addr:0x%08lx virt_addr:0x%p size:%lu\n",
		ld->name, start, (u8 __iomem *)base, size);
	mld->start = start;
	mld->size = size;
	mld->base = (char __iomem *)base;

	if (mld->size >= SZ_4M)
		remap_4mb_map_to_ipc_dev(mld);
	else
		return -EINVAL;

	memset(mld->base, 0, mld->size);

	mld->magic = mld->ipc_map.magic;
	mld->access = mld->ipc_map.access;

	mld->dev[IPC_FMT] = &mld->ipc_map.dev[IPC_FMT];
	mld->dev[IPC_RAW] = &mld->ipc_map.dev[IPC_RAW];

	return 0;
}

void __iomem *shm_request_region(unsigned int sh_addr,
		unsigned int size)
{
	int i;
	struct page **pages;
	void *pv;

	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(sh_addr);
		sh_addr += PAGE_SIZE;
	}

	pv = vmap(pages, size >> PAGE_SHIFT, VM_MAP,
	pgprot_writecombine(PAGE_KERNEL));

	kfree(pages);
	return (void __iomem *)pv;
}

void release_sh_region(void *rgn)
{
	vunmap(rgn);
}

static int shmem_xmit_bin(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	struct data_info di;
	struct modem_data *modem = ld->mdm_data;
	struct modem_ctl *mc = ld->mc;
	u32 size;
	u8 __iomem *mem_base;
	int err;

	memset(&di, 0, sizeof(struct data_info));

	err = copy_from_user(&di, (const void __user *)arg, sizeof(di));
	if (err) {
		mif_err("%s: ERR! INFO copy_from_user fail\n", ld->name);
		err = -EFAULT;
		goto exit;
	}

	if (di.len > di.total_size) {
		mif_err("Unexpected cp binary size : 0x%x\n", di.len);
		goto exit;
	}

	if ( di.stage == BOOT) {

		size = di.len;
		mif_info("Get cp bootloader(size : %d bytes)\n", di.total_size);

		if (di.total_size > SZ_8K) {
			mif_err("Unexpected cp bootloader size : 0x%x\n", di.total_size);
			goto exit;
		}

		if (mc->phone_state == STATE_CRASH_EXIT)
			mem_base = modem->dump_base;
		else
			mem_base = modem->modem_base + di.m_offset;

		err = copy_from_user(mem_base, (void __user *)di.buff, size);
		if (err) {
			mif_err("%s: ERR! BOOT copy_from_user fail\n", ld->name);
			err = -EFAULT;
			goto exit;
		}

		/*
		** change magic code for boot
		*/
		shmem_dload_start(ld, iod);
	}
	else if (di.stage == MAIN) {
		size = di.len;

		err = copy_from_user(modem->modem_base + di.m_offset - 0xebe0, (void __user *)di.buff, size);
		if (err) {
			mif_err("%s: ERR! MAIN copy_from_user fail\n", ld->name);
			err = -EFAULT;
			goto exit;
		}
	}
	else if (di.stage == NV) {
		size = di.len;

		err = copy_from_user(modem->modem_base + di.m_offset, (void __user *)di.buff, size);
		if (err) {
			mif_err("%s: ERR! NV copy_from_user fail\n", ld->name);
			err = -EFAULT;
			goto exit;
		}
	}
	else if (di.stage == NV_PROT) {
		size = di.len;

		err = copy_from_user(modem->modem_base + di.m_offset, (void __user *)di.buff, size);
		if (err) {
			mif_err("%s: ERR! NV_PROT copy_from_user fail\n", ld->name);
			err = -EFAULT;
			goto exit;
		}
	}
	else {
		goto exit;
	}

exit:
	return 0;
}

static int shmem_sec_init(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	enum cp_boot_mode mode = (enum cp_boot_mode)arg;
	int err = 0;

	mif_info("%s\n", __func__);

	err = exynos_smc(SMC_ID, mode, 0, 0);
	mif_info("%s:smc call return value: %d\n", __func__, err);

	return err;
}

int shmem_security_check(struct link_device *ld, struct io_device *iod,
				unsigned long arg)
{
	struct modem_ctl *mc = ld->mc;
	struct modem_data *modem = ld->mdm_data;
	struct sec_info info;
	int err = 0;

	err = copy_from_user(&info, (const void __user *)arg, sizeof(info));
	if (err) {
		mif_err("%s: ERR! security_check copy_from_user fail)\n", ld->name);
		err = -EFAULT;
		goto exit;
	}

	mif_err("%s: call requeset_security_check(mode: %d, boot_size: %d, main_size: %d\n",
		ld->name, info.mode, info.boot_size, info.main_size);

	if (mc->phone_state == STATE_CRASH_EXIT) {
		err = exynos_smc(SMC_ID, info.mode, info.boot_size, modem->dump_addr);
	}
	else
		err = exynos_smc(SMC_ID, info.mode, info.boot_size, info.main_size);

	mif_info("%s:smc call return value: %d\n", __func__, err);

exit:
	return err;
}

static void mem_irq_handler(struct lli_link_device *mld, struct mem_status *mst)
{
	struct link_device *ld = (struct link_device *)&mld->ld;
	u32 intr;

	intr = mst->int2ap;

	if (unlikely(!INT_VALID(intr))) {
		mif_info("%s: ERR! invalid intr 0x%X\n", ld->name, intr);
		dump_stack();
		return;
	}

	if (ld->mode == LINK_MODE_DLOAD || ld->mode == LINK_MODE_ULOAD)
		udl_handler(mld, mst);
	else
		tasklet_hi_schedule(&mld->rx_tsk);
}

#ifdef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
/**
  @brief  interrupt handler for a MIPI-LLI IPC interrupt

  1) Get a free mst buffer\n
  2) Reads the RXQ status and saves the status to the mst buffer\n
  3) Saves the interrupt value to the mst buffer\n
  4) Invokes mem_irq_handler that is common to all memory-type interfaces\n

  @param data the pointer to a lli_link_device instance
  @param intr the interrupt value
 */
static void lli_irq_handler(void *data, enum mipi_lli_event event, u32 intr)
{
	struct lli_link_device *mld = (struct lli_link_device *)data;
	struct mem_status *mst = msq_get_free_slot(&mld->rx_msq);

	if (event == LLI_EVENT_SIG) {
		get_shmem_status(mld, RX, mst);
		mst->int2ap = intr;

		mem_irq_handler(mld, mst);
	} else {
		struct link_device *ld = &mld->ld;
		struct modem_link_pm *pm = &ld->pm;

		check_lli_irq(pm, event);
	}
}

#else
/**
  @brief  interrupt handler for a MIPI-LLI IPC interrupt

  1) Get a free mst buffer\n
  2) Reads the RXQ status and saves the status to the mst buffer\n
  3) Saves the interrupt value to the mst buffer\n
  4) Invokes mem_irq_handler that is common to all memory-type interfaces\n

  @param data the pointer to a lli_link_device instance
  @param intr the interrupt value
  */
static void lli_irq_handler(void *data, enum mipi_lli_event event, u32 intr)
{
	struct lli_link_device *mld = (struct lli_link_device *)data;
	struct mem_status *mst = msq_get_free_slot(&mld->rx_msq);

	if (event == LLI_EVENT_SIG) {
		get_shmem_status(mld, RX, mst);
		mst->int2ap = intr;

		if (unlikely(lli_check_max_intr()))
			return;

		/* Prohibit CP from going to sleep */
		if (gpio_get_value(mld->gpio_cp_status) == 0
		|| gpio_get_value(mld->gpio_ap_status) == 0)
		print_pm_status(mld);

		if (gpio_get_value(mld->gpio_cp_wakeup) == 0)
		gpio_set_value(mld->gpio_cp_wakeup, 1);

		mem_irq_handler(mld, mst);
	}
}
#endif
static struct lli_link_device *mem_create_link_device(enum mem_iface_type type,
				struct modem_data *modem, struct device *dev)
{
	struct lli_link_device *mld;
	struct link_device *ld;
	int i;
	mif_err ("+++\n");

	/*
	** Alloc an instance of shmem_link_device structure
	*/
	mld = devm_kzalloc(dev, sizeof(struct lli_link_device), GFP_KERNEL);
	if (!mld) {
		mif_err("%s: ERR! mld kzalloc fail\n", modem->link_name);
		goto error;
	}
	mld->type = type;

	ld = &mld->ld;
	ld->name = modem->link_name;
	ld->aligned = true;
	ld->ipc_version = modem->ipc_version;
	ld->max_ipc_dev = MAX_EXYNOS_DEVICES;

	ld->send = shmem_send;
	ld->dload_start = shmem_dload_start;
	ld->firm_update = shmem_set_dload_info;

	ld->force_dump = shmem_force_dump;
	ld->dump_start = shmem_dump_start;

	ld->xmit_bin = shmem_xmit_bin;
	ld->check_security = shmem_security_check;
	ld->sec_init = shmem_sec_init;

	INIT_LIST_HEAD(&ld->list);

	skb_queue_head_init(&ld->sk_fmt_tx_q);
	skb_queue_head_init(&ld->sk_raw_tx_q);
	ld->skb_txq[IPC_FMT] = &ld->sk_fmt_tx_q;
	ld->skb_txq[IPC_RAW] = &ld->sk_raw_tx_q;

	skb_queue_head_init(&ld->sk_fmt_rx_q);
	skb_queue_head_init(&ld->sk_raw_rx_q);
	ld->skb_rxq[IPC_FMT] = &ld->sk_fmt_rx_q;
	ld->skb_rxq[IPC_RAW] = &ld->sk_raw_rx_q;

	init_completion(&ld->init_cmpl);

	ld->ready = lli_link_ready;
	ld->reset = lli_link_reset;
	ld->reload = lli_link_reload;
	ld->off = lli_link_off;

	ld->unmounted = lli_link_unmounted;
	ld->suspended = lli_link_suspended;

	/*
	** Initialize locks, completions, and bottom halves
	*/
	snprintf(mld->wlock_name, MIF_MAX_NAME_LEN, "%s_wlock", ld->name);
	wake_lock_init(&mld->wlock, WAKE_LOCK_SUSPEND, mld->wlock_name);

	init_completion(&mld->udl_cmpl);
	for (i = 0; i < MAX_EXYNOS_DEVICES; i++)
		init_completion(&mld->req_ack_cmpl[i]);

	tasklet_init(&mld->rx_tsk, ipc_rx_task, (unsigned long)mld);
	INIT_DELAYED_WORK(&mld->msg_rx_dwork, msg_rx_work);
	INIT_DELAYED_WORK(&mld->udl_rx_dwork, udl_rx_work);

	for (i = 0; i < MAX_EXYNOS_DEVICES; i++) {
		spin_lock_init(&mld->tx_lock[i]);
		atomic_set(&mld->res_required[i], 0);
	}

	ld->tx_wq = create_singlethread_workqueue("shmem_tx_wq");
	if (!ld->tx_wq) {
		mif_err("%s: ERR! fail to create tx_wq\n", ld->name);
		goto error;
	}
	INIT_DELAYED_WORK(&ld->fmt_tx_dwork, fmt_tx_work);
	INIT_DELAYED_WORK(&ld->raw_tx_dwork, raw_tx_work);
	ld->tx_dwork[IPC_FMT] = &ld->fmt_tx_dwork;
	ld->tx_dwork[IPC_RAW] = &ld->raw_tx_dwork;

	spin_lock_init(&mld->tx_msq.lock);
	spin_lock_init(&mld->rx_msq.lock);

#ifdef LLI_SHMEM_DUMP
	spin_lock_init(&mld->trace_list.lock);
	INIT_DELAYED_WORK(&mld->dump_dwork, mem_dump_work);
#endif
	mif_info("---\n");
	return mld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

struct link_device *lli_create_link_device(struct platform_device *pdev)
{
	struct modem_data *modem = NULL;
	struct lli_link_device *mld = NULL;
	struct link_device *ld = NULL;
	struct device *dev = &pdev->dev;
	int err;
	unsigned long start;
	unsigned long size;
	mif_err("+++\n");

	/**
	 * Get the modem (platform) data
	 */
	modem = (struct modem_data *)pdev->dev.platform_data;
	if (!modem) {
		mif_err("ERR! modem == NULL\n");
		return NULL;
	}

	if (!modem->gpio_ap_wakeup) {
		mif_err("ERR! no gpio_ap_wakeup\n");
		return NULL;
	}

	if (!modem->gpio_cp_status) {
		mif_err("ERR! no gpio_cp_status\n");
		return NULL;
	}

	mif_err("MODEM:%s LINK:%s\n", modem->name, modem->link_name);

	/**
	 * Create a MEMORY link device instance
	 */
	mld = mem_create_link_device(MEM_LLI_SHMEM, modem, dev);
	if (!mld) {
		mif_err("%s: ERR! create_link_device fail\n", modem->link_name);
		return NULL;
	}

	ld = &mld->ld;

	/**
	 * Link local functions to the corresponding function pointers that are
	 * mandatory for all memory-type link devices
	 */
	mld->remap_region = mipi_lli_request_sh_region;
	mld->send_ap2cp_irq = send_ap2cp_irq;

	/*
	** Link local functions to the corresponding function pointers
	*/
#ifndef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
	mld->finalize_cp_start = finalize_cp_start;
#endif

	mld->start_pm = start_pm;
	mld->stop_pm = stop_pm;
	mld->forbid_cp_sleep = forbid_cp_sleep;
	mld->permit_cp_sleep = permit_cp_sleep;
	mld->link_active = check_link_status;

#ifdef DEBUG_MODEM_IF
	mld->debug_info = mipi_lli_debug_info;
#endif

	/**
	 * Initialize SHMEM maps for IPC (physical map -> logical map)
	 */
	start = mipi_lli_get_phys_base();
	size = mipi_lli_get_phys_size();
	err = mem_setup_ipc_map(mld, start, size);
	if (err < 0) {
		mif_err("%s: ERR! setup_ipc_map fail (%d)\n", ld->name, err);
		goto error;
	}
#ifdef DEBUG_MODEM_IF
	mif_err("%s: IPC phys_addr:0x%08X virt_addr:0x%08X size:%lu\n",
		ld->name, (int)start, (int)mld->base, size);
#endif

	/**
	 * Register interrupt handlers
	 */
	err = mipi_lli_register_handler(lli_irq_handler, mld);
	if (err) {
		mif_err("%s: ERR! register_handler fail (%d)\n", ld->name, err);
		goto error;
	}

	/*
	** Retrieve GPIO#, IRQ#, and IRQ flags for PM
	*/
	mld->gpio_ap_wakeup = modem->gpio_ap_wakeup;
	mld->gpio_cp_wakeup = modem->gpio_cp_wakeup;
	mld->gpio_cp_status = modem->gpio_cp_status;
	mld->gpio_ap_status = modem->gpio_ap_status;
	mld->gpio_dump_noti = modem->gpio_dump_noti;

	/* Retrieve modem specific attribute value */
	mld->attr = modem->attr;

	mld->cmd_handler = cmd_handler;

	err = init_pm(mld);
	if (err)
		goto error;

	mif_err("---\n");
	return ld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

