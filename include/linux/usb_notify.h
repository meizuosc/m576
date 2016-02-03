/*
 *  usb notify header
 *
 * Copyright (C) 2011-2013 Samsung, Inc.
 * Author: Dongrak Shin <dongrak.shin@samsung.com>
 *
*/

#ifndef __LINUX_USB_NOTIFY_H__
#define __LINUX_USB_NOTIFY_H__

#include <linux/notifier.h>
#include <linux/host_notify.h>

enum otg_notify_events {
	NOTIFY_EVENT_NONE,
	NOTIFY_EVENT_VBUS,
	NOTIFY_EVENT_HOST,
	NOTIFY_EVENT_CHARGER,
	NOTIFY_EVENT_SMARTDOCK_TA,
	NOTIFY_EVENT_SMARTDOCK_USB,
	NOTIFY_EVENT_AUDIODOCK,
	NOTIFY_EVENT_LANHUB,
	NOTIFY_EVENT_LANHUB_TA,
	NOTIFY_EVENT_MMDOCK,
	NOTIFY_EVENT_DRIVE_VBUS,
	NOTIFY_EVENT_OVERCURRENT,
	NOTIFY_EVENT_SMSC_OVC,
	NOTIFY_EVENT_SMTD_EXT_CURRENT,
	NOTIFY_EVENT_MMD_EXT_CURRENT,
	NOTIFY_EVENT_VBUSPOWER,
};

enum otg_notify_evt_type {
	NOTIFY_EVENT_EXTRA,
	NOTIFY_EVENT_STATE,
};

enum otg_notify_gpio {
	NOTIFY_VBUS,
	NOTIFY_REDRIVER,
};

enum ovc_check_value {
	HNOTIFY_LOW,
	HNOTIFY_HIGH,
	HNOTIFY_INITIAL,
};

struct otg_notify {
	struct atomic_notifier_head	otg_notifier;
	struct blocking_notifier_head extra_notifier;
	int vbus_detect_gpio;
	int redriver_en_gpio;
	int is_wakelock;
	int unsupport_host;
	int smsc_ovc_poll_sec;
	const char *muic_name;
	int (*pre_gpio) (int gpio, int use);
	int (*post_gpio) (int gpio, int use);
	int (*vbus_drive) (bool);
	int (*set_host) (bool);
	int (*set_peripheral)(bool);
	int (*set_charger)(bool);
	int (*post_vbus_detect)(bool);
	int (*set_lanhubta)(int);
	int (*set_battcall)(int, int);
	void *o_data;
};

struct otg_booster {
	char *name;
	int (*booster) (bool);
};

#ifdef CONFIG_USB_NOTIFY_LAYER
extern void send_otg_notify(struct otg_notify *n,
					unsigned long event, int enable);
extern struct otg_booster *find_get_booster(void);
extern int register_booster(struct otg_booster *b);
extern int register_ovc_func(int (*check_state)(void *), void *data);
extern int get_usb_mode(void);
extern unsigned long get_cable_type(void);
extern void *get_notify_data(struct otg_notify *n);
extern void set_notify_data(struct otg_notify *n, void *data);
extern struct otg_notify *get_otg_notify(void);
extern int set_otg_notify(struct otg_notify *n);
extern void put_otg_notify(struct otg_notify *n);
#else
static inline void send_otg_notify(struct otg_notify *n,
					unsigned long event, int enable) { }
static inline struct otg_booster *find_get_booster(void) {return NULL; }
static inline int register_booster(struct otg_booster *b) {return 0; }
static inline int register_ovc_func
		(int (*check_state)(void *), void *data) {return 0; }
static inline int get_usb_mode(void) {return 0; }
static inline unsigned long get_cable_type(void) {return 0; }
static inline void *get_notify_data(struct otg_notify *n) {return NULL; }
static inline void set_notify_data(struct otg_notify *n, void *data) {}
static inline struct otg_notify *get_otg_notify(void) {return NULL; }
static inline int set_otg_notify(struct otg_notify *n) {return 0; }
static inline void put_otg_notify(struct otg_notify *n) {}
#endif

#endif /* __LINUX_USB_NOTIFY_H__ */
