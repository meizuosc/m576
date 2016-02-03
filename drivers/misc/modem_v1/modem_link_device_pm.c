/*
 * Exynos MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#ifdef CONFIG_OF
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#endif

#include <linux/mipi-lli.h>
#include "modem_link_device_lli.h"
#include "modem_prj.h"
#include "modem_pm.h"
#include "modem_utils.h"

#ifdef DEBUG_MODEM_IF
static inline void print_pm_event(struct modem_link_pm *pm, enum pm_event event)
{
	int cp2ap_wakeup;
	int ap2cp_wakeup;
	int cp2ap_status;
	int ap2cp_status;

	cp2ap_wakeup = gpio_get_value(pm->gpio_cp2ap_wakeup);
	ap2cp_wakeup = gpio_get_value(pm->gpio_ap2cp_wakeup);
	cp2ap_status = gpio_get_value(pm->gpio_cp2ap_status);
	ap2cp_status = gpio_get_value(pm->gpio_ap2cp_status);

	/*
	PM {cp2ap_wakeup:ap2cp_wakeup:cp2ap_status:ap2cp_status}{event:state}
	   <CALLER>
	*/
	pr_info("%s: PM {%d:%d:%d:%d}{%s:%s}\n", pm->link_name,
		cp2ap_wakeup, ap2cp_wakeup, cp2ap_status, ap2cp_status,
		pm_event2str(event), pm_state2str(pm->fsm.state));
}

static inline void print_pm_fsm(struct modem_link_pm *pm)
{
	struct pm_wdog *wdog = &pm->wdog;
	int cp2ap_wakeup;
	int ap2cp_wakeup;
	int cp2ap_status;
	int ap2cp_status;

	cp2ap_wakeup = gpio_get_value(pm->gpio_cp2ap_wakeup);
	ap2cp_wakeup = gpio_get_value(pm->gpio_ap2cp_wakeup);
	cp2ap_status = gpio_get_value(pm->gpio_cp2ap_status);
	ap2cp_status = gpio_get_value(pm->gpio_ap2cp_status);

	/*
	PM {cp2ap_wakeup:ap2cp_wakeup:cp2ap_status:ap2cp_status}\
	   {event:current_state->next_state} <CALLER>
	*/
	pr_info("%s: PM {%d:%d:%d:%d}{%s:%s->%s}\n", pm->link_name,
		cp2ap_wakeup, ap2cp_wakeup, cp2ap_status, ap2cp_status,
		pm_event2str(pm->fsm.event), pm_state2str(pm->fsm.prev_state),
		pm_state2str(pm->fsm.state));

	if (wdog->msg[0]) {
		pr_err("%s\n", wdog->msg);
		wdog->msg[0] = 0;
	}
}
#endif

static void pm_wdog_bark(unsigned long data);

static inline void change_irq_level(unsigned int irq, unsigned int value)
{
	irq_set_irq_type(irq, value ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
}

static inline void release_ap2cp_wakeup(struct modem_link_pm *pm)
{
	gpio_set_value(pm->gpio_ap2cp_wakeup, 0);
}

static inline void assert_ap2cp_wakeup(struct modem_link_pm *pm)
{
	gpio_set_value(pm->gpio_ap2cp_wakeup, 1);
}

static inline void release_ap2cp_status(struct modem_link_pm *pm)
{
	gpio_set_value(pm->gpio_ap2cp_status, 0);
}

static inline void assert_ap2cp_status(struct modem_link_pm *pm)
{
	gpio_set_value(pm->gpio_ap2cp_status, 1);
}

static inline void schedule_cp_free(struct modem_link_pm *pm)
{
	/* Hold gpio_ap2cp_wakeup for CP_HOLD_TIME */
	if (work_pending(&pm->cp_free_dwork.work))
		return;

	queue_delayed_work(pm->wq, &pm->cp_free_dwork,
			   msecs_to_jiffies(CP_HOLD_TIME));
}

static inline void cancel_cp_free(struct modem_link_pm *pm)
{
	if (work_pending(&pm->cp_free_dwork.work))
		cancel_delayed_work(&pm->cp_free_dwork);
}

static inline void start_pm_wdog(struct modem_link_pm *pm, enum pm_state state,
				 enum pm_state w_state, enum pm_event w_event,
				 unsigned long ms)
{
	struct pm_wdog *wdog = &pm->wdog;
	struct timer_list *timer = &wdog->timer;
	unsigned long expire = msecs_to_jiffies(ms);

	wdog->state = state;
	wdog->w_state = w_state;
	wdog->w_event = w_event;

#if 0
	snprintf(wdog->msg, MAX_STR_LEN, "%s: PM WDOG wait for {%s@%s}",
		 pm->link_name, pm_event2str(w_event), pm_state2str(state));
#endif

	mif_add_timer(timer, expire, pm_wdog_bark, (unsigned long)wdog);
}

static inline void stop_pm_wdog(struct modem_link_pm *pm, enum pm_state state,
				enum pm_event event)
{
	struct pm_wdog *wdog = &pm->wdog;

	if (state == wdog->state) {
		struct timer_list *timer = &wdog->timer;

		if (timer_pending(timer))
			del_timer(timer);

#ifdef DEBUG_MODEM_IF
		mif_debug("%s: PM WDOG kicked by {%s@%s}\n",
			pm->link_name, pm_event2str(event),
			pm_state2str(state));
#endif
	} else {
		mif_err("%s: ERR! PM WDOG illegal state {%s@%s}\n",
			pm->link_name, pm_event2str(event),
			pm_state2str(state));
	}
}

static inline bool link_suspended(struct modem_link_pm *pm)
{
	struct link_device *ld = container_of(pm, struct link_device, pm);
	return ld->suspended ? ld->suspended(ld) : false;
}

static inline void forbid_link_suspend(struct modem_link_pm *pm)
{
	struct link_pm_svc *pm_svc = pm->pm_svc;
	if (pm_svc && pm_svc->lock_link)
		pm_svc->lock_link(pm);
}

static inline void permit_link_suspend(struct modem_link_pm *pm)
{
	struct link_pm_svc *pm_svc = pm->pm_svc;
	if (pm_svc && pm_svc->unlock_link)
		pm_svc->unlock_link(pm);
}

static inline void reload_link(struct modem_link_pm *pm)
{
	struct link_device *ld = container_of(pm, struct link_device, pm);
	if (ld->reload)
		ld->reload(ld);
}

static inline void handle_pm_fail(struct modem_link_pm *pm)
{
	if (pm->fail_handler)
		pm->fail_handler(pm);
}

static inline void handle_cp_fail(struct modem_link_pm *pm)
{
	if (pm->cp_fail_handler)
		pm->cp_fail_handler(pm);
}

static inline void handle_wdog_timeout(struct modem_link_pm *pm)
{
	if (gpio_get_value(pm->gpio_cp2ap_status))
		handle_pm_fail(pm);
	else
		handle_cp_fail(pm);
}

static inline void lock_pm_wake(struct modem_link_pm *pm)
{
	if (!wake_lock_active(&pm->wlock))
		wake_lock(&pm->wlock);
}

static inline void unlock_pm_wake(struct modem_link_pm *pm)
{
	if (wake_lock_active(&pm->wlock))
		wake_unlock(&pm->wlock);
}

static inline void prepare_mount(struct modem_link_pm *pm)
{
	lock_pm_wake(pm);
	forbid_link_suspend(pm);
}

static inline void unmounted_to_resetting(struct modem_link_pm *pm)
{
	reload_link(pm);
	assert_ap2cp_wakeup(pm);

	mif_info("%s: state: ap2cp_wakeup_pin_done\n", __func__);
	start_pm_wdog(pm, PM_STATE_RESETTING, PM_STATE_MOUNTING,
		      PM_EVENT_LINK_RESET, LINKPM_WATCHDOG_TIMEOUT);
}

static inline void unmounted_to_holding(struct modem_link_pm *pm)
{
//	reload_link(pm);
	assert_ap2cp_wakeup(pm);
	start_pm_wdog(pm, PM_STATE_HOLDING, PM_STATE_RESETTING,
		      PM_EVENT_CP2AP_WAKEUP_HIGH, LINKPM_WATCHDOG_TIMEOUT);
}

static inline void holding_to_resetting(struct modem_link_pm *pm)
{
	stop_pm_wdog(pm, PM_STATE_HOLDING, PM_EVENT_CP2AP_WAKEUP_HIGH);
	reload_link(pm);
	start_pm_wdog(pm, PM_STATE_RESETTING, PM_STATE_MOUNTING,
		      PM_EVENT_LINK_RESET, LINKPM_WATCHDOG_TIMEOUT);
}

static inline enum pm_state next_state_from_resume(struct modem_link_pm *pm)
{
	if (gpio_get_value(pm->gpio_cp2ap_wakeup)) {
		pm->hold_requested = false;
		return PM_STATE_RESETTING;
	}

	if (pm->hold_requested) {
		pm->hold_requested = false;
		return PM_STATE_HOLDING;
	}

	return PM_STATE_UNMOUNTED;
}

#if 1
#endif

static inline bool fsm_locked(enum pm_state state)
{
	if (state == PM_STATE_LOCKED_ON
	    || state == PM_STATE_WDOG_TIMEOUT
	    || state == PM_STATE_CP_FAIL)
		return true;
	else
		return false;
}

static inline bool hold_possible(enum pm_state state)
{
	if (state == PM_STATE_UNMOUNTED
	    || state == PM_STATE_AP_FREE
	    || state == PM_STATE_CP_FREE)
		return true;
	else
		return false;
}

static inline void set_pm_fsm(struct modem_link_pm *pm, enum pm_state c_state,
			      enum pm_state n_state, enum pm_event event)
{
	struct pm_fsm *fsm = &pm->fsm;

	fsm->prev_state = c_state;
	fsm->event = event;
	fsm->state = n_state;
}

static inline void decide_pm_wake(struct modem_link_pm *pm,
				  enum pm_state c_state,
				  enum pm_state n_state)
{
	if (n_state == c_state)
		return;

	switch (n_state) {
	case PM_STATE_UNMOUNTED:
		unlock_pm_wake(pm);
		break;

	case PM_STATE_ACTIVE:
		lock_pm_wake(pm);
		break;

	default:
		break;
	}
}

static inline void check_pm_fail(struct modem_link_pm *pm,
				 enum pm_state c_state,
				 enum pm_state n_state)
{
	if (n_state == c_state)
		return;

	switch (n_state) {
	case PM_STATE_WDOG_TIMEOUT:
		handle_wdog_timeout(pm);
		break;

	case PM_STATE_CP_FAIL:
		handle_cp_fail(pm);
		break;

	case PM_STATE_AP_FAIL:
#ifdef CONFIG_SEC_MODEM_DEBUG
		panic("%s: PM_STATE_AP_FAIL\n", pm->link_name);
#else
		handle_cp_fail(pm);
#endif
		break;

	default:
		break;
	}
}

static void run_pm_fsm(struct modem_link_pm *pm, enum pm_event event)
{
	struct link_device *ld = container_of(pm, struct link_device, pm);
	struct modem_ctl *mc = ld->mc;
	struct pm_fsm *fsm = &pm->fsm;
	enum pm_state c_state;
	enum pm_state n_state;
	unsigned long flags;

	spin_lock_irqsave(&pm->lock, flags);

	c_state = fsm->state;
	n_state = fsm->state;

	if (!pm->active) {
		release_ap2cp_wakeup(pm);
		if (event == PM_EVENT_LINK_ERROR)
			n_state = PM_STATE_CP_FAIL;
		goto exit;
	}

	if (fsm_locked(c_state))
		goto exit;

	if (event == PM_EVENT_CP_HOLD_REQUEST) {
		if (!cp_online(mc))
			goto exit;

		pm->hold_requested = true;

		if (!hold_possible(c_state))
			goto exit;
	}

#ifdef DEBUG_MODEM_IF
	print_pm_event(pm, event);
#endif
	switch (c_state) {
	case PM_STATE_UNMOUNTED:
		if (event == PM_EVENT_LINK_SUSPENDED) {
			n_state = PM_STATE_SUSPENDED;
		} else if (event == PM_EVENT_CP2AP_WAKEUP_HIGH
			   || event == PM_EVENT_CP_HOLD_REQUEST) {
			if (link_suspended(pm)) {
				n_state = PM_STATE_SUSPENDED;
				prepare_mount(pm);
			} else {
				n_state = next_state_from_resume(pm);
				if (n_state == PM_STATE_RESETTING) {
					prepare_mount(pm);
					unmounted_to_resetting(pm);
				} else if (n_state == PM_STATE_HOLDING) {
					prepare_mount(pm);
					unmounted_to_holding(pm);
				}
			}
		}
		break;

	case PM_STATE_SUSPENDED:
		if (event == PM_EVENT_LINK_RESUMED) {
			n_state = next_state_from_resume(pm);
			if (n_state == PM_STATE_RESETTING) {
				prepare_mount(pm);
				unmounted_to_resetting(pm);
			} else if (n_state == PM_STATE_HOLDING) {
				prepare_mount(pm);
				unmounted_to_holding(pm);
			}
		} else if (event == PM_EVENT_CP2AP_WAKEUP_HIGH
			   || event == PM_EVENT_CP_HOLD_REQUEST) {
			n_state = PM_STATE_SUSPENDED;
			prepare_mount(pm);
		}
		break;

	case PM_STATE_HOLDING:
		if (event == PM_EVENT_CP2AP_WAKEUP_HIGH) {
			n_state = PM_STATE_RESETTING;
			holding_to_resetting(pm);
		} else if (event == PM_EVENT_WDOG_TIMEOUT) {
			/*
			It is not guaranteed for FSM to succeed in getting GPIO
			interrupt events or for stop_pm_wdog() to succeed in
			deleting the WDOG timer always.
			So, gpio_cp2ap_wakeup and gpio_cp2ap_status must always
			be checked before state transition.
			*/
			if (gpio_get_value(pm->gpio_cp2ap_wakeup)) {
				n_state = PM_STATE_RESETTING;
				holding_to_resetting(pm);
			} else {
				n_state = PM_STATE_WDOG_TIMEOUT;
			}
		}
		break;

	case PM_STATE_RESETTING:
		if (event == PM_EVENT_LINK_RESET) {
			n_state = PM_STATE_MOUNTING;
			stop_pm_wdog(pm, c_state, event);
			assert_ap2cp_status(pm);

			mif_info("%s: state: ap2cp_status_pin_done\n", __func__);
			start_pm_wdog(pm, n_state, PM_STATE_ACTIVE,
				      PM_EVENT_LINK_MOUNTED, LINKPM_WATCHDOG_TIMEOUT);
		} else if (event == PM_EVENT_WDOG_TIMEOUT) {
			n_state = PM_STATE_AP_FAIL;
		} else if (event == PM_EVENT_CP2AP_WAKEUP_LOW) {
			n_state = PM_STATE_CP_FAIL;
		}
		break;

	case PM_STATE_MOUNTING:
		if (event == PM_EVENT_LINK_MOUNTED
			|| event == PM_EVENT_CP2AP_STATUS_HIGH) {
			n_state = PM_STATE_ACTIVE;
			stop_pm_wdog(pm, c_state, event);
		} else if (event == PM_EVENT_WDOG_TIMEOUT) {
			n_state = PM_STATE_WDOG_TIMEOUT;
		} else if (event == PM_EVENT_CP2AP_WAKEUP_LOW) {
#if 0
			n_state = PM_STATE_CP_FAIL;
#else
			n_state = PM_STATE_AP_FAIL;
#endif
		}
		break;

	case PM_STATE_ACTIVE:
		if (event == PM_EVENT_CP2AP_WAKEUP_LOW) {
			n_state = PM_STATE_AP_FREE;
			schedule_cp_free(pm);
#if 0
			if (mipi_lli_get_link_status() == LLI_MOUNTED) {
				n_state = PM_STATE_AP_FREE;
				schedule_cp_free(pm);
			}
#endif
		} else if (event == PM_EVENT_CP2AP_STATUS_LOW) {
#ifdef REPORT_CRASHDMP
			n_state = PM_STATE_CP_FAIL;
#else
			n_state = PM_STATE_AP_FREE;
			schedule_cp_free(pm);
#endif
		}
		break;

	case PM_STATE_AP_FREE:
		if (event == PM_EVENT_CP2AP_WAKEUP_HIGH) {
			n_state = PM_STATE_ACTIVE;
			cancel_cp_free(pm);
			assert_ap2cp_wakeup(pm);
		} else if (event == PM_EVENT_CP_HOLD_REQUEST) {
			n_state = PM_STATE_AP_FREE;
			cancel_cp_free(pm);
			assert_ap2cp_wakeup(pm);
			schedule_cp_free(pm);
		} else if (event == PM_EVENT_CP_HOLD_TIMEOUT) {
			/*
			It is not guaranteed for cancel_cp_free() to succeed
			in canceling the cp_free_dwork always.
			So, cp2ap_wakeup must always be checked before state
			transition.
			*/
			if (!gpio_get_value(pm->gpio_cp2ap_wakeup)) {
				n_state = PM_STATE_CP_FREE;
				pm->hold_requested = false;
				release_ap2cp_wakeup(pm);
			} else {
				n_state = PM_STATE_ACTIVE;
				cancel_cp_free(pm);
				assert_ap2cp_wakeup(pm);
			}
		} else if (event == PM_EVENT_CP2AP_STATUS_LOW) {
			n_state = PM_STATE_CP_FAIL;
		}
		break;

	case PM_STATE_CP_FREE:
		if (event == PM_EVENT_CP2AP_STATUS_LOW) {
			n_state = PM_STATE_UNMOUNTING;
			start_pm_wdog(pm, n_state, PM_STATE_UNMOUNTED,
				      PM_EVENT_LINK_UNMOUNTED, LINKPM_WATCHDOG_TIMEOUT);
		} else if (event == PM_EVENT_CP2AP_WAKEUP_HIGH) {
			n_state = PM_STATE_ACTIVE;
			assert_ap2cp_wakeup(pm);
		} else if (event == PM_EVENT_CP_HOLD_REQUEST) {
			n_state = PM_STATE_ACTIVATING;
			assert_ap2cp_wakeup(pm);
			start_pm_wdog(pm, n_state, PM_STATE_ACTIVE,
				      PM_EVENT_CP2AP_WAKEUP_HIGH, LINKPM_WATCHDOG_TIMEOUT);
		}
		break;

	case PM_STATE_ACTIVATING:
		if (event == PM_EVENT_CP2AP_WAKEUP_HIGH) {
			n_state = PM_STATE_ACTIVE;
			stop_pm_wdog(pm, c_state, event);
			assert_ap2cp_wakeup(pm);
		} else if (event == PM_EVENT_CP2AP_STATUS_LOW) {
			n_state = PM_STATE_UNMOUNTING;
			stop_pm_wdog(pm, c_state, event);
			release_ap2cp_wakeup(pm);
		} else if (event == PM_EVENT_WDOG_TIMEOUT) {
			/*
			It is not guaranteed for FSM to succeed in getting GPIO
			interrupt events or for stop_pm_wdog() to succeed in
			deleting the WDOG timer always.
			So, gpio_cp2ap_wakeup and gpio_cp2ap_status must always
			be checked before state transition.
			*/
			if (gpio_get_value(pm->gpio_cp2ap_wakeup))
				n_state = PM_STATE_ACTIVE;
			else if (!gpio_get_value(pm->gpio_cp2ap_status))
				n_state = PM_STATE_UNMOUNTING;
			else
				n_state = PM_STATE_WDOG_TIMEOUT;
		}
		break;

	case PM_STATE_UNMOUNTING:
		if (event == PM_EVENT_LINK_UNMOUNTED) {
			if (pm->hold_requested) {
				if (cp_online(mc))
					n_state = PM_STATE_HOLDING;
				else
					n_state = PM_STATE_UNMOUNTED;
				pm->hold_requested = false;
			} else {
				n_state = PM_STATE_UNMOUNTED;
			}
			stop_pm_wdog(pm, c_state, event);
			release_ap2cp_status(pm);
			if (n_state == PM_STATE_HOLDING) {
				prepare_mount(pm);
				unmounted_to_holding(pm);
			}
		} else if (event == PM_EVENT_WDOG_TIMEOUT) {
			n_state = PM_STATE_WDOG_TIMEOUT;
		}
		break;

	case PM_STATE_CP_BOOTING:
		if (event == PM_EVENT_CP2AP_WAKEUP_HIGH) {
			n_state = PM_STATE_ACTIVE;
			assert_ap2cp_wakeup(pm);
		} else if (event == PM_EVENT_LINK_ERROR) {
			n_state = PM_STATE_CP_FAIL;
		}
		break;

	default:
		break;
	}

	set_pm_fsm(pm, c_state, n_state, event);

#ifdef DEBUG_MODEM_IF
	print_pm_fsm(pm);
#endif

	decide_pm_wake(pm, c_state, n_state);

exit:
	spin_unlock_irqrestore(&pm->lock, flags);

	check_pm_fail(pm, c_state, n_state);
}

#if 1
#endif

/**
@brief	interrupt handler for a wakeup interrupt

1) Reads the interrupt value\n
2) Performs interrupt handling\n

@param irq	the IRQ number
@param data	the pointer to a data
*/
static irqreturn_t cp2ap_wakeup_handler(int irq, void *data)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)data;
	int cp2ap_wakeup = gpio_get_value(pm->gpio_cp2ap_wakeup);

	mif_debug("%s: cp2ap_wakeup[%d]\n", __func__, cp2ap_wakeup);
	if (cp2ap_wakeup) {
		run_pm_fsm(pm, PM_EVENT_CP2AP_WAKEUP_HIGH);
	}
	else {
		run_pm_fsm(pm, PM_EVENT_CP2AP_WAKEUP_LOW);
	}

	change_irq_level(irq, cp2ap_wakeup);

	return IRQ_HANDLED;
}

static irqreturn_t cp2ap_status_handler(int irq, void *data)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)data;
	int cp2ap_status = gpio_get_value(pm->gpio_cp2ap_status);

	mif_debug("%s: cp2ap_status[%d]\n", __func__, cp2ap_status);
	if (cp2ap_status)
		run_pm_fsm(pm, PM_EVENT_CP2AP_STATUS_HIGH);
	else
		run_pm_fsm(pm, PM_EVENT_CP2AP_STATUS_LOW);

	change_irq_level(irq, cp2ap_status);

	return IRQ_HANDLED;
}

static void cp_free_work_func(struct work_struct *ws)
{
	struct modem_link_pm *pm;

	pm = container_of(ws, struct modem_link_pm, cp_free_dwork.work);

	run_pm_fsm(pm, PM_EVENT_CP_HOLD_TIMEOUT);
}

static void pm_wdog_bark(unsigned long data)
{
	struct pm_wdog *wdog = (struct pm_wdog *)data;
	struct modem_link_pm *pm = wdog_to_pm(wdog);
	struct pm_fsm *fsm = &pm->fsm;
	enum pm_state c_state;
	unsigned long flags;

	spin_lock_irqsave(&pm->lock, flags);
	c_state = fsm->state;
	spin_unlock_irqrestore(&pm->lock, flags);

	if (wdog->w_state == c_state) {
		mif_err("%s: PM WDOG lost event {%s@%s}\n", pm->link_name,
			pm_event2str(wdog->w_event), pm_state2str(wdog->state));
		return;
	}

	run_pm_fsm(pm, PM_EVENT_WDOG_TIMEOUT);
}

static void request_hold(struct modem_link_pm *pm)
{
	run_pm_fsm(pm, PM_EVENT_CP_HOLD_REQUEST);
}

static void release_hold(struct modem_link_pm *pm)
{
#if 0
	unsigned long flags;

	spin_lock_irqsave(&pm->lock, flags);

	pm->hold_requested = false;

	spin_unlock_irqrestore(&pm->lock, flags);
#endif
	return;
}

static inline void link_reset_cb(void *owner)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)owner;
	run_pm_fsm(pm, PM_EVENT_LINK_RESET);
}

static inline void link_unmount_cb(void *owner)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)owner;
	run_pm_fsm(pm, PM_EVENT_LINK_UNMOUNTED);
}

static inline void link_suspend_cb(void *owner)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)owner;
	run_pm_fsm(pm, PM_EVENT_LINK_SUSPENDED);
}

static inline void link_resume_cb(void *owner)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)owner;
	run_pm_fsm(pm, PM_EVENT_LINK_RESUMED);
}

static inline void link_mount_cb(void *owner)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)owner;
	run_pm_fsm(pm, PM_EVENT_LINK_MOUNTED);
}

static inline void link_error_cb(void *owner)
{
	struct modem_link_pm *pm = (struct modem_link_pm *)owner;
	run_pm_fsm(pm, PM_EVENT_LINK_ERROR);
}

void check_lli_irq(struct modem_link_pm *pm, enum mipi_lli_event event)
{
	switch (event) {
	case LLI_EVENT_RESET:
		link_reset_cb(pm);
		break;
	case LLI_EVENT_WAITFORMOUNT:
		break;
	case LLI_EVENT_MOUNTED:
		link_mount_cb(pm);
		break;
	case LLI_EVENT_UNMOUNTED:
		link_unmount_cb(pm);
		break;
	case LLI_EVENT_SUSPEND:
		link_suspend_cb(pm);
		break;
	case LLI_EVENT_RESUME:
		link_resume_cb(pm);
		break;
	default:
		break;
	}
}

static bool link_active(struct modem_link_pm *pm)
{
	bool ret;
	struct pm_fsm *fsm = &pm->fsm;
	unsigned long flags;

	spin_lock_irqsave(&pm->lock, flags);

	if (fsm->state == PM_STATE_LOCKED_ON
	    || fsm->state == PM_STATE_ACTIVE
	    || fsm->state == PM_STATE_AP_FREE)
		ret = true;
	else
		ret = false;

	spin_unlock_irqrestore(&pm->lock, flags);

	return ret;
}

static void start_link_pm(struct modem_link_pm *pm, enum pm_event event)
{
	int cp2ap_wakeup;
	int cp2ap_status;
	enum pm_state state;
	unsigned long flags;

	spin_lock_irqsave(&pm->lock, flags);

	if (pm->active) {
		mif_err("%s: PM is already ACTIVE\n", pm->link_name);
		goto exit;
	}

	lock_pm_wake(pm);

	if (event == PM_EVENT_LOCK_ON) {
		state = PM_STATE_LOCKED_ON;
		assert_ap2cp_wakeup(pm);
		assert_ap2cp_status(pm);
	} else if (event == PM_EVENT_CP_BOOTING) {
		state = PM_STATE_CP_BOOTING;
		assert_ap2cp_wakeup(pm);
		assert_ap2cp_status(pm);
	} else {
		state = PM_STATE_UNMOUNTED;
		release_ap2cp_wakeup(pm);
		release_ap2cp_status(pm);
	}

	/*
	Enable every CP-to-AP IRQ and set it as a wake-up source
	*/
	cp2ap_wakeup = gpio_get_value(pm->gpio_cp2ap_wakeup);
	change_irq_level(pm->cp2ap_wakeup_irq.num, cp2ap_wakeup);
	mif_enable_irq(&pm->cp2ap_wakeup_irq);

	cp2ap_status = gpio_get_value(pm->gpio_cp2ap_status);
	change_irq_level(pm->cp2ap_status_irq.num, cp2ap_status);
	mif_enable_irq(&pm->cp2ap_status_irq);

	set_pm_fsm(pm, PM_STATE_UNMOUNTED, state, event);

	pm->hold_requested = false;

	pm->active = true;

#ifdef DEBUG_MODEM_IF
	print_pm_fsm(pm);
#endif

exit:
	spin_unlock_irqrestore(&pm->lock, flags);
}

static void stop_link_pm(struct modem_link_pm *pm)
{
	unsigned long flags;
	enum pm_state state;
	enum pm_event event;

	spin_lock_irqsave(&pm->lock, flags);

	if (!pm->active)
		goto exit;

	pm->active = false;

	mif_disable_irq(&pm->cp2ap_wakeup_irq);
	mif_err("%s: PM %s_irq#%d handler disabled\n", pm->link_name,
		pm->cp2ap_wakeup_irq.name, pm->cp2ap_wakeup_irq.num);

	mif_disable_irq(&pm->cp2ap_status_irq);
	mif_err("%s: PM %s_irq#%d handler disabled\n", pm->link_name,
		pm->cp2ap_status_irq.name, pm->cp2ap_status_irq.num);

	state = pm->fsm.state;
	event = PM_EVENT_STOP_PM;
	set_pm_fsm(pm, state, PM_STATE_UNMOUNTED, event);
#ifdef DEBUG_MODEM_IF
	print_pm_event(pm, event);
#endif

exit:
	spin_unlock_irqrestore(&pm->lock, flags);
}

static inline void init_pm_fsm(struct modem_link_pm *pm)
{
	set_pm_fsm(pm, PM_STATE_UNMOUNTED, PM_STATE_UNMOUNTED,
		   PM_EVENT_NO_EVENT);
}

int init_link_device_pm(struct link_device *ld,
			struct modem_link_pm *pm,
			struct link_pm_svc *pm_svc,
			void (*fail_fn)(struct modem_link_pm *),
			void (*cp_fail_fn)(struct modem_link_pm *))
{
	struct lli_link_device *mld = container_of(ld, struct lli_link_device, ld);
	int err;
	int cp2ap_wakeup;
	int cp2ap_status;
	unsigned int num;
	unsigned long flags;
	char name[MAX_NAME_LEN];

	/*
	Set up variables for PM
	*/
	pm->link_name = ld->name;
	pm->fail_handler = fail_fn;
	pm->cp_fail_handler = cp_fail_fn;

	/*
	Retrieve GPIO pins and IRQ numbers for PM
	*/
	pm->gpio_cp2ap_wakeup = mld->gpio_ap_wakeup;
	pm->gpio_ap2cp_wakeup = mld->gpio_cp_wakeup;
	pm->gpio_cp2ap_status = mld->gpio_cp_status;
	pm->gpio_ap2cp_status = mld->gpio_ap_status;

	num = gpio_to_irq(pm->gpio_cp2ap_wakeup);
	flags = IRQF_NO_THREAD | IRQF_NO_SUSPEND | IRQF_ONESHOT;
	snprintf(name, MAX_NAME_LEN, "%s_cp2ap_wakeup", pm->link_name);
	mif_init_irq(&pm->cp2ap_wakeup_irq, num, name, flags);

	num = gpio_to_irq(pm->gpio_cp2ap_status);
	flags = IRQF_NO_THREAD | IRQF_NO_SUSPEND | IRQF_ONESHOT;
	snprintf(name, MAX_NAME_LEN, "%s_cp2ap_status", pm->link_name);
	mif_init_irq(&pm->cp2ap_status_irq, num, name, flags);

	mif_err("CP2AP_WAKEUP GPIO:%d IRQ:%d\n",
		pm->gpio_cp2ap_wakeup, pm->cp2ap_wakeup_irq.num);

	mif_err("AP2CP_WAKEUP GPIO:%d\n", pm->gpio_ap2cp_wakeup);

	mif_err("CP2AP_STATUS GPIO:%d IRQ:%d\n",
		pm->gpio_cp2ap_status, pm->cp2ap_status_irq.num);

	mif_err("AP2CP_STATUS GPIO:%d\n", pm->gpio_ap2cp_status);

	/*
	Register cp2ap_wakeup IRQ handler
	*/
	cp2ap_wakeup = gpio_get_value(pm->gpio_cp2ap_wakeup);
	change_irq_level(pm->cp2ap_wakeup_irq.num, cp2ap_wakeup);

	err = mif_request_irq(&pm->cp2ap_wakeup_irq, cp2ap_wakeup_handler, pm);
	if (err) {
		mif_err("%s: ERR! request_irq(%s#%d) fail (%d)\n",
			pm->link_name, pm->cp2ap_wakeup_irq.name,
			pm->cp2ap_wakeup_irq.num, err);
		return err;
	}
	mif_disable_irq(&pm->cp2ap_wakeup_irq);

	mif_err("%s: %s_irq#%d handler registered\n", pm->link_name,
		pm->cp2ap_wakeup_irq.name, pm->cp2ap_wakeup_irq.num);

	/*
	Register cp2ap_status IRQ handler
	*/
	cp2ap_status = gpio_get_value(pm->gpio_cp2ap_status);
	change_irq_level(pm->cp2ap_status_irq.num, cp2ap_status);

	err = mif_request_irq(&pm->cp2ap_status_irq, cp2ap_status_handler, pm);
	if (err) {
		mif_err("%s: ERR! request_irq(%s#%d) fail (%d)\n",
			pm->link_name, pm->cp2ap_status_irq.name,
			pm->cp2ap_status_irq.num, err);
		free_irq(pm->cp2ap_wakeup_irq.num, pm);
		return err;
	}
	mif_disable_irq(&pm->cp2ap_status_irq);

	mif_err("%s: %s_irq#%d handler registered\n", pm->link_name,
		pm->cp2ap_status_irq.name, pm->cp2ap_status_irq.num);

	/*
	Initialize common variables for PM
	*/
	spin_lock_init(&pm->lock);

	snprintf(pm->wlock_name, MAX_NAME_LEN, "%s_pm_wlock", pm->link_name);
	wake_lock_init(&pm->wlock, WAKE_LOCK_SUSPEND, pm->wlock_name);

	snprintf(pm->wq_name, MAX_NAME_LEN, "%s_pm_wq", pm->link_name);
	flags = WQ_NON_REENTRANT | WQ_UNBOUND | WQ_HIGHPRI;
	pm->wq = alloc_workqueue(pm->wq_name, flags, 1);
	if (!pm->wq) {
		mif_err("%s: ERR! fail to create %s\n",
			pm->link_name, pm->wq_name);
		return -EFAULT;
	}

	INIT_DELAYED_WORK(&pm->cp_free_dwork, cp_free_work_func);

	init_pm_fsm(pm);

	/*
	Register PM functions set by the common link PM framework and used by
	each link device driver
	*/
	pm->start = start_link_pm;
	pm->stop = stop_link_pm;
	pm->request_hold = request_hold;
	pm->release_hold = release_hold;
	pm->link_active = link_active;

	return 0;
}

#ifdef CONFIG_OF
int link_pm_parse_dt_gpio_pdata(struct device_node *np, struct modem_data *mdm)
{
	int err;
	char name[MAX_NAME_LEN];

	mdm->gpio_ap_wakeup = of_get_named_gpio(np, "mif,gpio_ap_wakeup", 0);
	if (gpio_is_valid(mdm->gpio_ap_wakeup)) {
		mif_err("gpio_cp2ap_wakeup: %d\n", mdm->gpio_ap_wakeup);

		snprintf(name, MAX_NAME_LEN, "%s_to_ap_wakeup", mdm->name);

		err = gpio_request(mdm->gpio_ap_wakeup, name);
		if (err) {
			mif_err("fail to gpio_request(cp2ap_wakeup):%d\n", err);
			return err;
		}

		gpio_direction_input(mdm->gpio_ap_wakeup);
	}

	mdm->gpio_cp_wakeup = of_get_named_gpio(np, "mif,gpio_cp_wakeup", 0);
	if (gpio_is_valid(mdm->gpio_cp_wakeup)) {
		mif_err("gpio_ap2cp_wakeup: %d\n", mdm->gpio_cp_wakeup);

		snprintf(name, MAX_NAME_LEN, "ap_to_%s_wakeup", mdm->name);

		err = gpio_request(mdm->gpio_cp_wakeup, name);
		if (err) {
			mif_err("fail to gpio_request(ap2cp_wakeup):%d\n", err);
			return err;
		}

		gpio_direction_output(mdm->gpio_cp_wakeup, 0);
	}

	mdm->gpio_cp_status = of_get_named_gpio(np, "mif,gpio_cp_status", 0);
	if (gpio_is_valid(mdm->gpio_cp_status)) {
		mif_err("gpio_cp2ap_status: %d\n", mdm->gpio_cp_status);

		snprintf(name, MAX_NAME_LEN, "%s_to_ap_status", mdm->name);

		err = gpio_request(mdm->gpio_cp_status, name);
		if (err) {
			mif_err("fail to gpio_request(cp2ap_status):%d\n", err);
			return err;
		}

		gpio_direction_input(mdm->gpio_cp_status);
	}

	mdm->gpio_ap_status = of_get_named_gpio(np, "mif,gpio_ap_status", 0);
	if (gpio_is_valid(mdm->gpio_ap_status)) {
		mif_err("gpio_ap2cp_status: %d\n", mdm->gpio_ap_status);

		snprintf(name, MAX_NAME_LEN, "ap_to_%s_status", mdm->name);

		err = gpio_request(mdm->gpio_ap_status, name);
		if (err) {
			mif_err("fail to gpio_request(ap2cp_status):%d\n", err);
			return err;
		}

		gpio_direction_output(mdm->gpio_ap_status, 0);
	}

	return 0;
}
EXPORT_SYMBOL(link_pm_parse_dt_gpio_pdata);
#endif
