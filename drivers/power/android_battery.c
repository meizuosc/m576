/*
 * Android dummy test battery driver.
 * Based on goldfish_battery.c
 * Author: Nicu Pavel <npavel@mini-box.com>
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

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>


static int adbattery_bat_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val);

static int adbattery_ac_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val);

static struct platform_device *adbattery_pdev;

static enum power_supply_property adbattery_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property adbattery_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static struct power_supply adbattery_bat = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties 	=  adbattery_battery_props,
	.num_properties = ARRAY_SIZE(adbattery_battery_props),
	.get_property	= adbattery_bat_get_property,
	.use_for_apm 	= 1,
};

static struct power_supply adbattery_ac = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties 	=  adbattery_ac_props,
	.num_properties = ARRAY_SIZE(adbattery_ac_props),
	.get_property	= adbattery_ac_get_property,
};

static struct power_supply adbattery_usb = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties 	=  adbattery_ac_props,
	.num_properties = ARRAY_SIZE(adbattery_ac_props),
	.get_property	= adbattery_ac_get_property,
};


static int adbattery_ac_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int adbattery_bat_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = 100;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = 20;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = 5;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int __init adbattery_init(void)
{
	int ret = 0;

	adbattery_pdev = platform_device_register_simple("battery", 0, NULL, 0);
	if (IS_ERR(adbattery_pdev))
		return PTR_ERR(adbattery_pdev);
	ret = power_supply_register(&adbattery_pdev->dev, &adbattery_bat);
	if (ret)
		goto bat_failed;
	ret = power_supply_register(&adbattery_pdev->dev, &adbattery_ac);
	if (ret)
		goto ac_failed;
	ret = power_supply_register(&adbattery_pdev->dev, &adbattery_usb);
	if (ret)
		goto usb_failed;
	printk(KERN_INFO "adbattery: android dummy battery driver loaded\n");
	goto success;

bat_failed:
	power_supply_unregister(&adbattery_bat);
ac_failed:
	power_supply_unregister(&adbattery_ac);
usb_failed:
	power_supply_unregister(&adbattery_usb);
	platform_device_unregister(adbattery_pdev);
success:
	return ret;
}

static void __exit adbattery_exit(void)
{
	power_supply_unregister(&adbattery_bat);
	power_supply_unregister(&adbattery_ac);
	power_supply_unregister(&adbattery_usb);
	platform_device_unregister(adbattery_pdev);
	printk(KERN_INFO "adbattery: android dummy battery driver unloaded\n");
}

module_init(adbattery_init);
module_exit(adbattery_exit);
MODULE_AUTHOR("Nicu Pavel npavel@mini-box.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dummy battery driver for Android battery service ");

