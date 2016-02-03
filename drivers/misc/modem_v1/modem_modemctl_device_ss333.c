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

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/mipi-lli.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_lli.h"

#define MIF_INIT_TIMEOUT	(30 * HZ)

void print_status(struct modem_ctl *mc)
{
	struct utc_time t;
	int ap_wakeup;
	int ap_status;
	int cp_wakeup;
	int cp_status;

	get_utc_time(&t);
	ap_wakeup = gpio_get_value(mc->gpio_ap_wakeup);
	ap_status = gpio_get_value(mc->gpio_ap_status);
	cp_wakeup = gpio_get_value(mc->gpio_cp_wakeup);
	cp_status = gpio_get_value(mc->gpio_cp_status);

	/*
    ** PM {ap_wakeup:cp_wakeup:cp_status:ap_status:magic} <CALLER>
	*/
	pr_err(HMSU_FMT " %s: PM {%d:%d:%d:%d} <%pf>\n",
	    t.hour, t.min, t.sec, t.msec, __func__,
	    ap_wakeup, cp_wakeup, cp_status, ap_status,
	    CALLER);
}

extern void dw_mci_fix_irq_cpreset(int cpcrash);

static void ss333_mc_state_fsm(struct modem_ctl *mc)
{
	int cp_on = gpio_get_value(mc->gpio_cp_on);
	int cp_reset  = gpio_get_value(mc->gpio_cp_reset);
	int cp_active = gpio_get_value(mc->gpio_phone_active);
	int old_state = mc->phone_state;
	int new_state = mc->phone_state;
	unsigned long flags;

	mif_err("old_state:%s cp_on:%d cp_reset:%d cp_active:%d\n",
		get_cp_state_str(old_state), cp_on, cp_reset, cp_active);

	spin_lock_irqsave(&mc->lock, flags);
	if (cp_active) {
		if (!cp_on)
			new_state = STATE_OFFLINE;
		else if (old_state == STATE_ONLINE) {
			new_state = STATE_CRASH_EXIT;
			if (!wake_lock_active(&mc->mc_wake_lock))
				wake_lock(&mc->mc_wake_lock);
		}
		else
			mif_err("don't care!!!\n");
	}

	if (old_state != new_state) {
		mif_err("new_state = %s\n", get_cp_state_str(new_state));
		dw_mci_fix_irq_cpreset(1); // CP Crash
		mc->bootd->modem_state_changed(mc->bootd, new_state);
		mc->iod->modem_state_changed(mc->iod, new_state);
	}
	spin_unlock_irqrestore(&mc->lock, flags);
}

static irqreturn_t phone_active_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	int cp_reset = gpio_get_value(mc->gpio_cp_reset);

	if (cp_reset)
		ss333_mc_state_fsm(mc);

	return IRQ_HANDLED;
}

static inline void make_gpio_floating(int gpio, bool floating)
{
	if (floating)
		gpio_direction_input(gpio);
	else
		gpio_direction_output(gpio, 0);
}

static int ss333_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	int cp_on = gpio_get_value(mc->gpio_cp_on);
	int cp_reset  = gpio_get_value(mc->gpio_cp_reset);
	int cp_active = gpio_get_value(mc->gpio_phone_active);
	int cp_status = gpio_get_value(mc->gpio_cp_status);
	unsigned long flags;

	mif_err("+++\n");
	mif_err("cp_on:%d cp_reset:%d cp_active:%d cp_status:%d\n",
		cp_on, cp_reset, cp_active, cp_status);

	spin_lock_irqsave(&mc->lock, flags);
	mc->phone_state = STATE_OFFLINE;
	gpio_set_value(mc->gpio_pda_active, 1);

	if (!wake_lock_active(&mc->mc_wake_lock))
		wake_lock(&mc->mc_wake_lock);
	spin_unlock_irqrestore(&mc->lock, flags);
	if (ld->ready)
		ld->ready(ld);

	gpio_set_value(mc->gpio_cp_on, 0);
	msleep(100);

	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(500);

	gpio_set_value(mc->gpio_cp_on, 1);
	msleep(100);

	gpio_set_value(mc->gpio_dump_noti, 0);

	// mipi_lli_reload();
	if (ld->reset)
		ld->reset(ld);

	gpio_set_value(mc->gpio_cp_reset, 1);
	msleep(300);

	mif_err("---\n");
	return 0;
}

static int ss333_off(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	int cp_on = gpio_get_value(mc->gpio_cp_on);
	unsigned long flags;

	mif_err("+++\n");

	spin_lock_irqsave(&mc->lock, flags);
	if (mc->phone_state == STATE_OFFLINE || cp_on == 0) {
		spin_unlock_irqrestore(&mc->lock, flags);
		return 0;
	}

	mc->phone_state = STATE_OFFLINE;
	spin_unlock_irqrestore(&mc->lock, flags);

	if (ld->off)
		ld->off(ld);

	gpio_set_value(mc->gpio_cp_reset, 0);

	gpio_set_value(mc->gpio_cp_on, 0);

	mif_err("---\n");
	return 0;
}

static int ss333_reset(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (ss333_off(mc))
		return -EIO;

	usleep_range(10000, 11000);
	mif_err("---\n");
	return 0;
}

static int ss333_force_crash_exit(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);
	struct lli_link_device *mld = container_of(ld, struct lli_link_device, ld);
	mif_err("+++\n");

	mld->silent_cp_reset = true;

	/* Make DUMP start */
	ld->force_dump(ld, mc->bootd);

	mif_err("---\n");
	return 0;
}

static int ss333_dump_reset(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	unsigned int gpio_cp_reset = mc->gpio_cp_reset;
	mif_err("+++\n");

	if (!wake_lock_active(&mc->mc_wake_lock))
		wake_lock(&mc->mc_wake_lock);

	gpio_set_value(gpio_cp_reset, 0);
	udelay(200);

	//mipi_lli_reload();
	if (ld->reset)
		ld->reset(ld);

	gpio_set_value(gpio_cp_reset, 1);
	msleep(300);

	gpio_set_value(mc->gpio_ap_status, 1);

	mif_err("---\n");
	return 0;
}

static int modemctl_notify_call(struct notifier_block *nfb,
				unsigned long event, void *arg)
{
	struct modem_ctl *mc = container_of(nfb, struct modem_ctl, event_nfb);

	mif_info("got event: %ld\n", event);

	switch (event) {
	case MDM_EVENT_CP_FORCE_RESET:
		ss333_force_crash_exit(mc);
		if (mc->iod && mc->iod->modem_state_changed)
			mc->iod->modem_state_changed(mc->iod, STATE_CRASH_RESET);
		if (mc->bootd && mc->bootd->modem_state_changed)
			mc->bootd->modem_state_changed(mc->bootd, STATE_CRASH_RESET);
		break;
	case MDM_EVENT_CP_FORCE_CRASH:
		ss333_force_crash_exit(mc);
		break;
	}

	return 0;
}

static int ss333_boot_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	unsigned long flags;

	mif_err("+++\n");

	disable_irq_nosync(mc->irq_phone_active);

	gpio_set_value(mc->gpio_ap_status, 1);

	INIT_COMPLETION(ld->init_cmpl);

	spin_lock_irqsave(&mc->lock, flags);
	mc->bootd->modem_state_changed(mc->bootd, STATE_BOOTING);
	mc->iod->modem_state_changed(mc->iod, STATE_BOOTING);
	spin_unlock_irqrestore(&mc->lock, flags);

	mif_err("---\n");
	return 0;
}

static int ss333_boot_off(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);
	unsigned long remain;
	mif_err("+++\n");

	remain = wait_for_completion_timeout(&ld->init_cmpl, MIF_INIT_TIMEOUT);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		mif_err("xxx\n");
		return -EAGAIN;
	}

	mif_err("---\n");
	return 0;
}

static int ss333_boot_done(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (wake_lock_active(&mc->mc_wake_lock))
		wake_unlock(&mc->mc_wake_lock);

	enable_irq(mc->irq_phone_active);

	dw_mci_fix_irq_cpreset(0); // CP Crash end
	mif_err("---\n");
	return 0;
}

static void ss333_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = ss333_on;
	mc->ops.modem_off = ss333_off;
	mc->ops.modem_reset = ss333_reset;
	mc->ops.modem_boot_on = ss333_boot_on;
	mc->ops.modem_boot_off = ss333_boot_off;
	mc->ops.modem_boot_done = ss333_boot_done;
	mc->ops.modem_force_crash_exit = ss333_force_crash_exit;
	mc->ops.modem_dump_reset = ss333_dump_reset;
}

int ss333_init_modemctl_device(struct modem_ctl *mc, struct modem_data *pdata)
{
	int ret = 0;
	int irq = 0;
	unsigned long flag = 0;
	mif_err("+++\n");

	if (!pdata->gpio_cp_on || !pdata->gpio_cp_reset
	    || !pdata->gpio_pda_active || !pdata->gpio_phone_active
	    || !pdata->gpio_ap_wakeup || !pdata->gpio_ap_status
	    || !pdata->gpio_cp_wakeup || !pdata->gpio_cp_status) {
		mif_err("ERR! no GPIO data\n");
		mif_err("xxx\n");
		return -ENXIO;
	}

	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_ap_wakeup = pdata->gpio_ap_wakeup;
	mc->gpio_ap_status = pdata->gpio_ap_status;
	mc->gpio_cp_wakeup = pdata->gpio_cp_wakeup;
	mc->gpio_cp_status = pdata->gpio_cp_status;
	mc->gpio_dump_noti = pdata->gpio_dump_noti;

	gpio_set_value(mc->gpio_cp_reset, 0);

	gpio_set_value(mc->gpio_cp_on, 0);

	ss333_get_ops(mc);
	dev_set_drvdata(mc->dev, mc);

	spin_lock_init(&mc->lock);
	wake_lock_init(&mc->mc_wake_lock, WAKE_LOCK_SUSPEND, "umts_wake_lock");

	mc->irq_phone_active = gpio_to_irq(mc->gpio_phone_active);
	if (!mc->irq_phone_active) {
		mif_err("ERR! no irq_phone_active\n");
		mif_err("xxx\n");
		return -1;
	}
	mif_err("PHONE_ACTIVE IRQ# = %d\n", mc->irq_phone_active);

	mc->event_nfb.notifier_call = modemctl_notify_call;
	register_cp_crash_notifier(&mc->event_nfb);

	irq = mc->irq_phone_active;
	flag = IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND;
	ret = request_irq(irq, phone_active_handler, flag, "umts_active", mc);
	if (ret) {
		mif_err("ERR! request_irq(#%d) fail (err %d)\n", irq, ret);
		mif_err("xxx\n");
		return ret;
	}
	ret = enable_irq_wake(irq);
	if (ret)
		mif_err("enable_irq_wake(#%d) fail (err %d)\n", irq, ret);

	mif_err("---\n");
	return 0;
}
