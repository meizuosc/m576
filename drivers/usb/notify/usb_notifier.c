/*
 * Copyright (C) 2011 Samsung Electronics Co. Ltd.
 *  Inchul Im <inchul.im@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define pr_fmt(fmt) "usb_notifier: " fmt

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/usb_notify.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_MUIC_NOTIFIER
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif
#include <linux/battery/sec_charging_common.h>

struct gadget_notify_dev {
	struct device	*dev;
	u16				gadget_state;
	bool		is_ready;
	struct delayed_work	notify_ready_work;
};

struct usb_notifier_platform_data {
	struct	gadget_notify_dev g_ndev;
	struct	notifier_block usb_nb;
	int	gpio_redriver_en;
};

enum usb_notifier_gadget_cmd {
	GADGET_NOTIFIER_DETACH,
	GADGET_NOTIFIER_ATTACH,
	GADGET_NOTIFIER_DEFAULT,
};

extern int dwc3_exynos_id_event(struct device *dev, int state);
extern int dwc3_exynos_vbus_event(struct device *dev, int state);
extern int exynos_otg_vbus_event(struct platform_device *pdev, int state);

#ifdef CONFIG_OF
static void of_get_usb_redriver_dt(struct device_node *np,
		struct usb_notifier_platform_data *pdata)
{
	int gpio = 0;

	gpio = of_get_named_gpio(np, "gpios_redriver_en", 0);
	if (!gpio_is_valid(gpio)) {
		pdata->gpio_redriver_en = -1;
		pr_err("%s: usb30_redriver_en: Invalied gpio pins\n", __func__);
	} else
		pdata->gpio_redriver_en = gpio;

	pr_info("%s, gpios_redriver_en %d\n", __func__, gpio);
	return;
}

static int of_usb_notifier_dt(struct device *dev,
		struct usb_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	of_get_usb_redriver_dt(np, pdata);
	return 0;
}

static struct usb_notifier_platform_data *of_get_usb_notifier_pdata(void)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct usb_notifier_platform_data *pdata = NULL;

	np = of_find_compatible_node(NULL, NULL, "samsung,usb-notifier");
	if (!np) {
		pr_err("%s: failed to get the usb-notifier device node\n"
				, __func__);
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: failed to get platform_device\n", __func__);
		return NULL;
	}
	pdata = pdev->dev.platform_data;
	of_node_put(np);

	return pdata;
}
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
static void check_usb_vbus_state(unsigned long state)
{
	struct usb_notifier_platform_data *pdata = of_get_usb_notifier_pdata();
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;

#ifdef CONFIG_USB_S3C_OTGD
	np = of_find_compatible_node(NULL, NULL, "samsung,exynos_udc");
#else
	np = of_find_compatible_node(NULL, NULL, "samsung,exynos5-dwusb3");
#endif
	if (!np) {
		pr_err("%s: failed to get the %s device node\n",
			__func__, np->name);
		return;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: failed to get the %s platform_device\n",
			__func__, np->name);
		return;
	}

	of_node_put(np);

	pr_info("usb: %s is_ready:%d ,vbus state:%d\n",
				__func__, pdata->g_ndev.is_ready,
					(int)pdata->g_ndev.gadget_state);

	if (pdata->g_ndev.is_ready)
#ifdef CONFIG_USB_S3C_OTGD
		exynos_otg_vbus_event(pdev, pdata->g_ndev.gadget_state);
#else
		dwc3_exynos_vbus_event(&pdev->dev, pdata->g_ndev.gadget_state);
#endif
	else
		pr_info("usb: %s usb_gadget_notifier is not ready.\n",
								__func__);
	return;
}

#ifdef CONFIG_USB_HOST_NOTIFY
static void check_usb_id_state(int state)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;

	pr_info("%s id state = %d\n", __func__, state);

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos5-dwusb3");
	if (!np) {
		pr_err("%s: failed to get the exynos5-dwusb3 device node\n",
			__func__);
		return;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: failed to get the exynos5-dwusb3 platform_device\n",
			__func__);
		return;
	}

	dwc3_exynos_id_event(&pdev->dev, state);
	of_node_put(np);
}
#endif

static void usbgadget_ready(struct work_struct *work)
{
	struct usb_notifier_platform_data *pdata = of_get_usb_notifier_pdata();

	pr_info("usb: %s,gadget_state:%d\n", __func__,
				pdata->g_ndev.gadget_state);
	pdata->g_ndev.is_ready = true;
	if (pdata->g_ndev.gadget_state != GADGET_NOTIFIER_DEFAULT)
		check_usb_vbus_state(pdata->g_ndev.gadget_state);
}

static int usb_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	struct otg_notify *o_notify;

	o_notify = get_otg_notify();

	pr_info("%s action=%lu, attached_dev=%d\n",
		__func__, action, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUS, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_HOST, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s - USB_HOST_TEST_DETACHED\n", __func__);
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER, 0);
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			pr_info("%s - USB_HOST_TEST_ATTACHED\n", __func__);
			send_otg_notify(o_notify, NOTIFY_EVENT_VBUSPOWER, 1);
		} else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_SMARTDOCK_TA, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_SMARTDOCK_TA, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify
				(o_notify, NOTIFY_EVENT_SMARTDOCK_USB, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify
				(o_notify, NOTIFY_EVENT_SMARTDOCK_USB, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_AUDIODOCK, 0);
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			send_otg_notify(o_notify, NOTIFY_EVENT_AUDIODOCK, 1);
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	return 0;
}
#endif

static int otg_accessory_power(bool enable)
{
	u8 on = (u8)!!enable;
	union power_supply_propval val;
	struct device_node *np_charger = NULL;
	char *charger_name;

	pr_info("otg accessory power = %d\n", on);

	np_charger = of_find_node_by_name(NULL, "battery");
	if (!np_charger) {
		pr_err("%s: failed to get the battery device node\n", __func__);
		return 0;
	} else {
		if (!of_property_read_string(np_charger, "battery,charger_name",
					(char const **)&charger_name)) {
			pr_info("%s: charger_name = %s\n", __func__,
					charger_name);
		} else {
			pr_err("%s: failed to get the charger name\n"
								, __func__);
			return 0;
		}
	}

	val.intval = enable;
	psy_do_property(charger_name, set,
			POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, val);

	return 0;
}

static int set_online(int event, int state)
{
	union power_supply_propval val;
	struct device_node *np_charger = NULL;
	char *charger_name;

	pr_info("request smartdock charging current = %s\n",
		state ? "1000mA" : "1700mA");

	np_charger = of_find_node_by_name(NULL, "battery");
	if (!np_charger) {
		pr_err("%s: failed to get the battery device node\n", __func__);
		return 0;
	} else {
		if (!of_property_read_string(np_charger, "battery,charger_name",
					(char const **)&charger_name)) {
			pr_info("%s: charger_name = %s\n", __func__,
					charger_name);
		} else {
			pr_err("%s: failed to get the charger name\n",
								 __func__);
			return 0;
		}
	}

	if (state)
		val.intval = POWER_SUPPLY_TYPE_SMART_OTG;
	else
		val.intval = POWER_SUPPLY_TYPE_SMART_NOTG;

	psy_do_property(charger_name, set,
			POWER_SUPPLY_PROP_ONLINE, val);

	return 0;
}

#ifdef CONFIG_USB_HOST_NOTIFY
static int exynos_set_host(bool enable)
{
	if (!enable) {
		pr_info("%s USB_HOST_DETACHED\n", __func__);
#ifdef CONFIG_OF
		check_usb_id_state(1);
#endif
	} else {
		pr_info("%s USB_HOST_ATTACHED\n", __func__);
#ifdef CONFIG_OF
		check_usb_id_state(0);
#endif
	}

	return 0;
}
#endif
#if defined(CONFIG_USB_SUPER_HIGH_SPEED_SWITCH_CHANGE)
extern u8 usb30en;
#endif
static int exynos_set_peripheral(bool enable)
{
	struct otg_notify *o_notify;
	struct usb_notifier_platform_data *pdata;
	o_notify = get_otg_notify();
	pdata = get_notify_data(o_notify);

	if (!enable) {
		pdata->g_ndev.gadget_state = GADGET_NOTIFIER_DETACH;
#if defined(CONFIG_USB_SUPER_HIGH_SPEED_SWITCH_CHANGE)
		usb30en = 0;
#endif
		check_usb_vbus_state(pdata->g_ndev.gadget_state);
	} else {
		pdata->g_ndev.gadget_state = GADGET_NOTIFIER_ATTACH;
		check_usb_vbus_state(pdata->g_ndev.gadget_state);
	}

	return 0;
}

static struct otg_notify dwc_lsi_notify = {
	.vbus_drive	= otg_accessory_power,
#ifdef CONFIG_USB_HOST_NOTIFY
	.set_host = exynos_set_host,
#endif
	.set_peripheral	= exynos_set_peripheral,
	.vbus_detect_gpio = -1,
	.is_wakelock = 1,
	.set_battcall = set_online,
};

static int usb_notifier_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct usb_notifier_platform_data *pdata = NULL;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct usb_notifier_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = of_usb_notifier_dt(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get device of_node\n");
			return ret;
		}

		pdev->dev.platform_data = pdata;
	} else
		pdata = pdev->dev.platform_data;

	pdata->g_ndev.gadget_state = GADGET_NOTIFIER_DEFAULT;

	/*
	When a device is booted up with usb cable,
	Sometimes you can't show usb icon on device.
	if MUIC notify is called before usb composite is up,
	usb state UEVENT is not happened.
	*/
	INIT_DELAYED_WORK(&pdata->g_ndev.notify_ready_work, usbgadget_ready);
	schedule_delayed_work(&pdata->g_ndev.notify_ready_work,
					msecs_to_jiffies(15000));

	dwc_lsi_notify.redriver_en_gpio = pdata->gpio_redriver_en;
	set_otg_notify(&dwc_lsi_notify);
	set_notify_data(&dwc_lsi_notify, pdata);

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&pdata->usb_nb, usb_handle_notification,
			       MUIC_NOTIFY_DEV_USB);
#endif

	dev_info(&pdev->dev, "usb notifier probe\n");
	return 0;
}

static int usb_notifier_remove(struct platform_device *pdev)
{
	struct usb_notifier_platform_data *pdata = dev_get_platdata(&pdev->dev);

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_unregister(&pdata->usb_nb);
#endif
	cancel_delayed_work_sync(&pdata->g_ndev.notify_ready_work);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id usb_notifier_dt_ids[] = {
	{ .compatible = "samsung,usb-notifier",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, usb_notifier_dt_ids);
#endif

static struct platform_driver usb_notifier_driver = {
	.probe		= usb_notifier_probe,
	.remove		= usb_notifier_remove,
	.driver		= {
		.name	= "usb_notifier",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(usb_notifier_dt_ids),
#endif
	},
};

static int __init usb_notifier_init(void)
{
	return platform_driver_register(&usb_notifier_driver);
}

static void __init usb_notifier_exit(void)
{
	platform_driver_unregister(&usb_notifier_driver);
}

late_initcall(usb_notifier_init);
module_exit(usb_notifier_exit);

MODULE_AUTHOR("inchul.im <inchul.im@samsung.com>");
MODULE_DESCRIPTION("USB notifier");
MODULE_LICENSE("GPL");
