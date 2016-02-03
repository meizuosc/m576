/*
 * Copyright (C) 2012 Samsung Electronics.
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

#ifndef __MODEM_LINK_PM_H__
#define __MODEM_LINK_PM_H__

#include "include/modem_debug.h"
#include "include/modem_v1.h"
#include "include/link_pm.h"

#ifdef CONFIG_OF
#define PM_DT_NODE_NAME		"link_pm"
#endif

#define MAX_NAME_LEN		64
enum pm_state {
	PM_STATE_UNMOUNTED,
	PM_STATE_SUSPENDED,
	PM_STATE_HOLDING,
	PM_STATE_RESETTING,
	PM_STATE_MOUNTING,
	PM_STATE_ACTIVE,
	PM_STATE_AP_FREE,
	PM_STATE_CP_FREE,
	PM_STATE_ACTIVATING,
	PM_STATE_UNMOUNTING,
	PM_STATE_CP_BOOTING,
	PM_STATE_LOCKED_ON,
	PM_STATE_WDOG_TIMEOUT,
	PM_STATE_CP_FAIL,
	PM_STATE_AP_FAIL
};

static const char const *pm_state_string[] = {
	[PM_STATE_UNMOUNTED] = "UNMOUNTED",
	[PM_STATE_SUSPENDED] = "SUSPENDED",
	[PM_STATE_HOLDING] = "HOLDING",
	[PM_STATE_RESETTING] = "RESETTING",
	[PM_STATE_MOUNTING] = "MOUNTING",
	[PM_STATE_ACTIVE] = "ACTIVE",
	[PM_STATE_AP_FREE] = "AP_FREE",
	[PM_STATE_CP_FREE] = "CP_FREE",
	[PM_STATE_ACTIVATING] = "ACTIVATING",
	[PM_STATE_UNMOUNTING] = "UNMOUNTING",
	[PM_STATE_CP_BOOTING] = "CP_BOOTING",
	[PM_STATE_LOCKED_ON] = "LOCKED_ON",
	[PM_STATE_WDOG_TIMEOUT] = "WDOG_TIMEOUT",
	[PM_STATE_CP_FAIL] = "CP_FAIL",
	[PM_STATE_AP_FAIL] = "AP_FAIL"
};

static const inline char *pm_state2str(enum pm_state state)
{
	if (unlikely(state > PM_STATE_AP_FAIL))
		return "INVALID";
	else
		return pm_state_string[state];
}

enum pm_event {
	PM_EVENT_NO_EVENT,
	PM_EVENT_LOCK_ON,
	PM_EVENT_CP_BOOTING,
	PM_EVENT_CP2AP_WAKEUP_LOW,
	PM_EVENT_CP2AP_WAKEUP_HIGH,
	PM_EVENT_CP2AP_STATUS_LOW,
	PM_EVENT_CP2AP_STATUS_HIGH,
	PM_EVENT_CP_HOLD_REQUEST,
	PM_EVENT_CP_HOLD_TIMEOUT,
	PM_EVENT_LINK_RESET,
	PM_EVENT_LINK_UNMOUNTED,
	PM_EVENT_LINK_SUSPENDED,
	PM_EVENT_LINK_RESUMED,
	PM_EVENT_LINK_MOUNTED,
	PM_EVENT_LINK_ERROR,
	PM_EVENT_WDOG_TIMEOUT,
	PM_EVENT_STOP_PM
};

static const char const *pm_event_string[] = {
	[PM_EVENT_NO_EVENT] = "NO_EVENT",
	[PM_EVENT_LOCK_ON] = "LOCK_ON",
	[PM_EVENT_CP_BOOTING] = "CP_BOOTING",
	[PM_EVENT_CP2AP_WAKEUP_LOW] = "CP2AP_WAKEUP_LOW",
	[PM_EVENT_CP2AP_WAKEUP_HIGH] = "CP2AP_WAKEUP_HIGH",
	[PM_EVENT_CP2AP_STATUS_LOW] = "CP2AP_STATUS_LOW",
	[PM_EVENT_CP2AP_STATUS_HIGH] = "CP2AP_STATUS_HIGH",
	[PM_EVENT_CP_HOLD_REQUEST] = "CP_HOLD_REQUEST",
	[PM_EVENT_CP_HOLD_TIMEOUT] = "CP_HOLD_TIMEOUT",
	[PM_EVENT_LINK_RESET] = "LINK_RESET",
	[PM_EVENT_LINK_UNMOUNTED] = "LINK_UNMOUNTED",
	[PM_EVENT_LINK_SUSPENDED] = "LINK_SUSPENDED",
	[PM_EVENT_LINK_RESUMED] = "LINK_RESUMED",
	[PM_EVENT_LINK_MOUNTED] = "LINK_MOUNTED",
	[PM_EVENT_LINK_ERROR] = "LINK_ERROR",
	[PM_EVENT_WDOG_TIMEOUT] = "WDOG_TIMEOUT",
	[PM_EVENT_STOP_PM] = "STOP_PM"
};

static const inline char *pm_event2str(enum pm_event event)
{
	if (unlikely(event > PM_EVENT_STOP_PM))
		return "INVALID";
	else
		return pm_event_string[event];
}

struct pm_fsm {
	enum pm_state prev_state;
	enum pm_event event;
	enum pm_state state;
};

struct pm_wdog {
	struct timer_list timer;

	/*
	The state of the FSM in which a WDOG is wating for the $w_event
	*/
	enum pm_state state;

	/*
	The state to which the FSM must switch its state before the WDOG timer
	expires
	*/
	enum pm_state w_state;

	/*
	The event needed by the FSM to switch its state to $w_state before the
	WDOG barks
	*/
	enum pm_event w_event;

	char msg[256];
};

#define CP_HOLD_TIME		100	/* ms */
#define LINKPM_WATCHDOG_TIMEOUT 5000	/* ms */

struct modem_link_pm {
	/*
	PM service provided by the corresponding physical link driver
	*/
	struct link_pm_svc *pm_svc;

	/*
	Private variables must be set first of all while the PM framework is set
	up by the corresponding link device driver in the modem interface
	*/
	char *link_name;
	void (*fail_handler)(struct modem_link_pm *pm);
	void (*cp_fail_handler)(struct modem_link_pm *pm);

	/*
	GPIO pins for PM
	*/
	unsigned int gpio_cp2ap_wakeup;
	unsigned int gpio_ap2cp_wakeup;
	unsigned int gpio_cp2ap_status;
	unsigned int gpio_ap2cp_status;

	/*
	IRQ numbers for PM
	*/
	struct modem_irq cp2ap_wakeup_irq;
	struct modem_irq cp2ap_status_irq;

	/*
	Common variables for PM
	*/
	spinlock_t lock;

	bool active;

	struct pm_fsm fsm;

	struct wake_lock wlock;
	char wlock_name[MAX_NAME_LEN];

	struct workqueue_struct *wq;
	char wq_name[MAX_NAME_LEN];

	struct delayed_work cp_free_dwork;	/* to hold ap2cp_wakeup */
	bool hold_requested;

	struct pm_wdog wdog;

	/*
	PM functions set by the common link PM framework and used by each link
	device driver
	*/
	void (*start)(struct modem_link_pm *pm, enum pm_event event);
	void (*stop)(struct modem_link_pm *pm);
	void (*request_hold)(struct modem_link_pm *pm);
	void (*release_hold)(struct modem_link_pm *pm);
	bool (*link_active)(struct modem_link_pm *pm);

	/*
	Linux notifier lists
	*/
	struct raw_notifier_head unmount_notifier_list;
};

#define wdog_to_pm(wdog)	container_of(wdog, struct modem_link_pm, wdog)

int pm_register_unmount_notifier(struct modem_link_pm *pm,
				 struct notifier_block *nb);

int pm_unregister_unmount_notifier(struct modem_link_pm *pm,
				   struct notifier_block *nb);

#endif
