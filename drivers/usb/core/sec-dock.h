/*
 * drivers/usb/core/sec-dock.h
 *
 * Copyright (C) 2013 Samsung Electronics
 * Author: Woo-kwang Lee <wookwang.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/power_supply.h>

#define PSY_CHG_NAME "battery"

int usb_open_count;
bool is_smartdock;
bool is_lan_hub;

static struct usb_device_id battery_notify_exception_table[] = {
/* add exception table list */
{ USB_DEVICE(0x1d6b, 0x0003), }, /* XHCI Host Controller */
{ USB_DEVICE(0x1d6b, 0x0002), }, /* EHCI Host Controller */
{ USB_DEVICE(0x1519, 0x0443), }, /* CDC Modem */
{ USB_DEVICE(0x8087, 0x0716), }, /* Qualcomm modem */
{ USB_DEVICE(0x08bb, 0x2704), }, /* TI USB Audio DAC 1 */
{ USB_DEVICE(0x08bb, 0x27c4), }, /* TI USB Audio DAC 2 */
{ USB_DEVICE(0x0424, 0x9512), }, /* SMSC USB LAN HUB 9512 */
{ USB_DEVICE(0x0424, 0xec00), }, /* SMSC LAN Driver */
{ }	/* Terminating entry */
};


/* real battery driver notification function */
static void set_online(int host_state)
{
	struct power_supply *psy = power_supply_get_by_name(PSY_CHG_NAME);
	union power_supply_propval value;

	if (!psy) {
		pr_err("%s: fail to get %s psy\n", __func__, PSY_CHG_NAME);
		return;
	}
	if (host_state)
		value.intval = POWER_SUPPLY_TYPE_SMART_OTG;
	else
		value.intval = POWER_SUPPLY_TYPE_SMART_NOTG;

	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	return;
}

static int call_battery_notify(struct usb_device *dev, bool bOnOff)
{
	struct usb_device_id	*id = battery_notify_exception_table;

	/* Smart Dock hub must be skipped */
	if ((le16_to_cpu(dev->descriptor.idVendor) == 0x1a40 &&
	     le16_to_cpu(dev->descriptor.idProduct) == 0x0101) ||
	     (le16_to_cpu(dev->descriptor.idVendor) == 0x0424 &&
	     le16_to_cpu(dev->descriptor.idProduct) == 0x2514)) {
		if (bOnOff) {
			is_smartdock = 1;
			usb_open_count = 0;
			pr_info("%s : smartdock is connected\n", __func__);
		} else {
			is_smartdock = 0;
			usb_open_count = 0;
			pr_info("%s : smartdock is disconnected\n", __func__);
		}
		return 0;
	}

	/* Lan Hub adapter must be skipped */
	else if ((le16_to_cpu(dev->descriptor.idVendor) == 0x0424 &&
	     le16_to_cpu(dev->descriptor.idProduct) == 0x9512)) {
		if (bOnOff) {
			is_lan_hub = 1;
			usb_open_count = 0;
			pr_info("%s : Lan Hub adapter is connected\n", __func__);
		} else {
			is_lan_hub = 0;
			usb_open_count = 0;
			pr_info("%s : Lan Hub adapter is disconnected\n", __func__);
		}
		return 0;
	}

	if (is_smartdock) {
		/* check VID, PID */
		for (id = battery_notify_exception_table; id->match_flags; id++) {
			if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
				(id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
				id->idVendor == le16_to_cpu(dev->descriptor.idVendor) &&
				id->idProduct == le16_to_cpu(dev->descriptor.idProduct)) {
					pr_info("%s : VID : 0x%x, PID : 0x%x skipped.\n",
						__func__, id->idVendor, id->idProduct);
				return 0;
			}
		}
		if (bOnOff)
			usb_open_count++;
		else
			usb_open_count--;

		/* battery driver notification */
		if (usb_open_count == 1 && bOnOff && is_smartdock) {
				pr_info("%s : VID : 0x%x, PID : 0x%x set 1000mA.\n",
						__func__,
						le16_to_cpu(dev->descriptor.idVendor),
						le16_to_cpu(dev->descriptor.idProduct));
				set_online(1);
		} else if (usb_open_count == 0 && !bOnOff) {
				pr_info("%s : VID : 0x%x, PID : 0x%x set 1700mA.\n",
						__func__,
						le16_to_cpu(dev->descriptor.idVendor),
						le16_to_cpu(dev->descriptor.idProduct));
				set_online(0);
		} else {
			pr_info("%s : VID : 0x%x, PID : 0x%x meaningless, bOnOff=%d, usb_open_count=%d\n",
					__func__, id->idVendor, id->idProduct, bOnOff, usb_open_count);
				/* Nothing to do */
		}
	}

	return 1;
}

