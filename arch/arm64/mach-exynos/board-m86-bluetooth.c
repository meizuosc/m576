/*
 * Bluetooth Broadcom GPIO and Low Power Mode control
 *
 *  Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *  Copyright (C) 2011 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/serial_core.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>

#include <plat/gpio-cfg.h>
#include <linux/of_gpio.h>
#include <linux/board-mz-factory-test.h>

/* Enable meizu special features */
//#define MEIZU_SPECIAL
//#define USE_ONOFF
#define BT_LPM_ENABLE
#define BT_FACTORY_MODE
#define USE_WAKE_PEER

static int gpio_bt_power;//BT_POWER
static int gpio_bt_wake;//BT_WAKE
static int gpio_bt_host_wake;//BT_HOST_WAKE
static int gpio_bt_rts;//BT_RTS

static spinlock_t *bt_slock;
static struct rfkill *bt_rfkill;

struct bcm_bt_lpm {
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

#ifdef USE_WAKE_PEER
	struct uart_port *uport;
#endif

	struct wake_lock bt_wake_lock;
	struct wake_lock host_wake_lock;
} bt_lpm;

#ifndef USE_WAKE_PEER
static spinlock_t bt_slock_i;
#endif

int bt_init_uart(int enable);

static volatile int bt_is_running = 0;

int check_bt_running(void) {
	return bt_is_running;
}
EXPORT_SYMBOL(check_bt_running);

#ifdef MEIZU_SPECIAL
void bt_uart_rts_ctrl(int flag)
{
	if (!gpio_get_value(gpio_bt_power))
		return;
	if (flag) {
		// BT RTS Set to HIGH
		s3c_gpio_cfgpin(MEIZU_BT_RTS, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(MEIZU_BT_RTS, S3C_GPIO_PULL_NONE);
		gpio_set_value(MEIZU_BT_RTS, 1);
	} else {
		// restore BT RTS state
		s3c_gpio_cfgpin(MEIZU_BT_RTS, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(MEIZU_BT_RTS, S3C_GPIO_PULL_NONE);
	}
}
EXPORT_SYMBOL(bt_uart_rts_ctrl);
#endif

#ifdef USE_ONOFF
static int bcm4339_bt_onoff(void *data, bool blocked)
{
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	if (!blocked) {
		pr_info("[BT] Bluetooth Power On.\n");
		pr_info("%s(), set gpio_bt_wake first\n", __func__);
		gpio_set_value(gpio_bt_wake, 1);
		pr_info("sleep %d ms\n", 500);
		msleep(500);
		gpio_set_value(gpio_bt_power, 1);
		bt_is_running = 1;
		msleep(100);
	} else {
		pr_info("[BT] Bluetooth Power Off.\n");
		bt_is_running = 0;
		gpio_set_value(gpio_bt_power, 0);
		pr_info("%s(), clear gpio_bt_wake too\n", __func__);
		gpio_set_value(gpio_bt_wake, 0);
	}
	return 0;
}
#endif

static int bcm4339_bt_rfkill_set_power(void *data, bool blocked)
{
#ifndef USE_ONOFF
	int ret;
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	if (!blocked) {
		pr_debug("[BT] Bluetooth Power On.\n");

		pr_debug("%s(), please ensure BT_RTS low level\n", __func__);
		if (bt_init_uart(1)) {
			pr_err("%s(), ensure BT_RTS low failed!\n", __func__);
			return -EIO;
		}
		ret = gpio_request_one(gpio_bt_rts, GPIOF_OUT_INIT_LOW,
				"bt_rts");
		if (ret) {
			pr_err("%s(), can NOT request gpio_bt_rts: %d\n",
					__func__, ret);
		}

		gpio_set_value(gpio_bt_wake, 1);
		pr_debug("sleep %d ms\n", 60);
		msleep(60);
		gpio_set_value(gpio_bt_power, 1);
		bt_is_running = 1;
		msleep(80);

		if (bt_init_uart(0)) {
			pr_err("%s(), restore AUD-UART state failed\n", __func__);
		}
		gpio_free(gpio_bt_rts);
	} else {
		pr_info("[BT] Bluetooth Power Off.\n");
		bt_is_running = 0;
		gpio_set_value(gpio_bt_power, 0);
		gpio_set_value(gpio_bt_wake, 0);
	}
#else
	pr_info("%s(), pls use onoff to govern power,do nothing here\n", __func__);
#endif
	return 0;
}

static const struct rfkill_ops bcm4339_bt_rfkill_ops = {
	.set_block = bcm4339_bt_rfkill_set_power,
};

#ifdef BT_LPM_ENABLE
static void set_wake_locked(int wake)
{
	if (!wake) {
		pr_info("%s() +++, wake:%d\n",
				__func__, wake);
	}
	gpio_set_value(gpio_bt_wake, wake);

	if (wake)
		wake_lock(&bt_lpm.bt_wake_lock);
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer)
{
	unsigned long flags;
	spin_lock_irqsave(bt_slock, flags);
#ifdef USE_WAKE_PEER
	if (bt_lpm.uport != NULL)
		set_wake_locked(0);
#else
	/*
	 * For not defined USE_WAKE_PEER: may
	 * use a flag which is set if upper layer assert bt_wake
	 */
	set_wake_locked(0);
#endif
	bt_is_running = 0;
	wake_lock_timeout(&bt_lpm.bt_wake_lock, HZ/2);
	spin_unlock_irqrestore(bt_slock, flags);

	return HRTIMER_NORESTART;
}

#ifdef USE_WAKE_PEER
void bt_uart_wake_peer(struct uart_port *uport)
{
	bt_lpm.uport = uport;
	bt_slock = &bt_lpm.uport->lock;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);
	bt_is_running = 1;
	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
			HRTIMER_MODE_REL);
}
#else
void bt_uart_wake_peer(struct uart_port *uport) {}
#endif
EXPORT_SYMBOL(bt_uart_wake_peer);

static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	bt_is_running = 1;
	if (host_wake) {
		wake_lock(&bt_lpm.host_wake_lock);
	} else  {
		/* Take a timed wakelock, so that upper layers can take it.
		 * The chipset deasserts the hostwake lock, when there is no
		 * more data to send.
		 */
		pr_debug("%s(), host_wake: 0, lock bt_lpm.host_wake_lock for 6s\n",
				__func__);
		wake_lock_timeout(&bt_lpm.host_wake_lock, 6*HZ);
	}
}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;
	unsigned long flags;

	host_wake = gpio_get_value(gpio_bt_host_wake);
#if 1
	pr_info("%s(), ++++, host_wake:%d\n",
			__func__, host_wake);
#endif
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	/*
	 * For not defined USE_WAKE_PEER: may
	 * use a flag which is set if upper layer assert bt_wake
	 */
#ifdef USE_WAKE_PEER
	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}
#endif

	spin_lock_irqsave(bt_slock, flags);
	update_host_wake_locked(host_wake);
	spin_unlock_irqrestore(bt_slock, flags);

	return IRQ_HANDLED;
}

static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int irq;
	int ret;

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(10, 0);  /* 10 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;
	bt_is_running = 0;

	wake_lock_init(&bt_lpm.host_wake_lock, WAKE_LOCK_SUSPEND,
			"bt_host_wake");
	wake_lock_init(&bt_lpm.bt_wake_lock, WAKE_LOCK_SUSPEND,
			"bt_wake");

	irq = gpio_to_irq(gpio_bt_host_wake);
	pr_info("%s(), bt_host_wake irq:%d\n", __func__, irq);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
			"bt host_wake", NULL);
	if (ret) {
		pr_err("[BT] Request_host wake irq failed.\n");
		return ret;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		pr_err("[BT] Set_irq_wake failed.\n");
		return ret;
	}

	return 0;
}
#endif

#ifdef BT_FACTORY_MODE
static struct delayed_work bt_test_dwork;
static int bt_in_test_mode = 0;

static void bt_test_func(struct work_struct *work)
{
	static int gpio_value = 0;

	mx_set_factory_test_led(gpio_value);
	gpio_value = !gpio_value;
	schedule_delayed_work(&bt_test_dwork, msecs_to_jiffies(250));

	return;
}

static ssize_t bt_test_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if(bt_in_test_mode)
		return sprintf(buf, "1\n");

	if(mx_is_factory_test_mode(MX_FACTORY_TEST_BT)) {
		msleep(100);
		if(mx_is_factory_test_mode(MX_FACTORY_TEST_BT)) {
			printk("in BT_TEST_MODE\n");
			bt_in_test_mode = 1; //test mode

			mx_set_factory_test_led(1);
			INIT_DEFERRABLE_WORK(&bt_test_dwork, bt_test_func);
		}
	}

	return sprintf(buf, "%d\n",  bt_in_test_mode);

}

static ssize_t bt_test_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long flash = simple_strtoul(buf, NULL, 10);

	if(bt_in_test_mode) {
		if(flash) {
			cancel_delayed_work_sync(&bt_test_dwork);
			schedule_delayed_work(&bt_test_dwork, 0);
		} else {
			cancel_delayed_work_sync(&bt_test_dwork);
			mx_set_factory_test_led(0);
		}
	}
	return count;
}

static DEVICE_ATTR(bt_test_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bt_test_mode_show, bt_test_mode_store);
#endif

static ssize_t bt_wake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gpio_get_value(gpio_bt_wake));
}

static ssize_t bt_wake_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;

	switch (buf[0]) {
		case '0':
			value = 0;
			bt_is_running = 0;
			break;
		case '1':
			value = 1;
			bt_is_running = 1;
			break;
		default:
			pr_err("%s(), err!unkown input:%s", __func__, buf);
			return -EINVAL;
	}

	pr_info("%s(), will set gpio bt_wake to %d\n",
			__func__, value);
	gpio_set_value(gpio_bt_wake, value);

	return count;
}

static DEVICE_ATTR(bt_wake, S_IRUGO|S_IWUSR | S_IWGRP,
		bt_wake_show, bt_wake_store);

#ifdef CONFIG_OF
static int bluetooth_parse_dt(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	pr_info("%s(), ++++++++++++\n", __func__);

	if (!np) {
		pr_info("%s(), Err!device_node is NULL\n", __func__);
		return -EINVAL;
	}

	gpio_bt_power = of_get_gpio(np, 0);
	pr_info("gpio_bt_power:%d\n", gpio_bt_power);
	gpio_bt_wake = of_get_gpio(np, 1);
	pr_info("gpio_bt_wake:%d\n", gpio_bt_wake);
	gpio_bt_host_wake = of_get_gpio(np, 2);
	pr_info("gpio_bt_host_wake:%d\n", gpio_bt_host_wake);

	gpio_bt_rts = of_get_gpio(np, 3);
	pr_info("gpio_bt_rts:%d\n", gpio_bt_rts);
	return 0;
}
#else
static int bluetooth_parse_dt(struct platform_device *pdev)
{
	return -ENXIO;
}
#endif

#ifdef USE_ONOFF
static ssize_t show_onoff(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", gpio_get_value(gpio_bt_power) ? "on": "off");
}

static ssize_t store_onoff(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	switch (buf[0]) {
		case '0':
			bcm4339_bt_onoff(NULL, 1);
			break;
		case '1':
			bcm4339_bt_onoff(NULL, 0);
			break;
		default:
			pr_err("%s(), unkown input:%s", __func__, buf);
	}

	return count;
}

static DEVICE_ATTR(onoff,  S_IRUGO | S_IWUGO, show_onoff, store_onoff);
static struct attribute *gpio_attrs[] = {
	&dev_attr_onoff.attr,
	NULL,
};

static struct attribute_group gpio_attr_group = {
	.name	= "gpio",
	.attrs	= gpio_attrs,
};
#endif

static int bcm4339_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;

	rc = bluetooth_parse_dt(pdev);
	if (rc) {
		pr_info("%s(), parse dt err! ret:%d\n", __func__, rc);
		return rc;
	}

	rc = gpio_request(gpio_bt_power, "bcm4339_bten_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_EN request failed.\n");
		return rc;
	}
	rc = gpio_request(gpio_bt_wake, "bcm4339_btwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_WAKE request failed.\n");
		gpio_free(gpio_bt_power);
		return rc;
	}

	rc = gpio_request(gpio_bt_host_wake, "bcm4339_bthostwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_HOST_WAKE request failed.\n");
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_power);
		return rc;
	}

	/*
	 * gpio_bt_host_wake has been configured as int with pull up
	 * in dts.
	 */
	gpio_direction_output(gpio_bt_wake, 0);
	gpio_direction_output(gpio_bt_power, 0);

	bt_rfkill = rfkill_alloc("bcm4339 Bluetooth", &pdev->dev,
			RFKILL_TYPE_BLUETOOTH, &bcm4339_bt_rfkill_ops,
			NULL);
	if (unlikely(!bt_rfkill)) {
		pr_err("[BT] bt_rfkill alloc failed.\n");
		gpio_free(gpio_bt_host_wake);
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_power);
		return -ENOMEM;
	}

	rfkill_init_sw_state(bt_rfkill, true);

	rc = rfkill_register(bt_rfkill);
	if (unlikely(rc)) {
		pr_err("[BT] bt_rfkill register failed.\n");
		rfkill_destroy(bt_rfkill);
		gpio_free(gpio_bt_host_wake);
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_power);
		return -1;
	}

	rfkill_set_sw_state(bt_rfkill, true);

#ifdef USE_ONOFF
	rc = sysfs_create_group(&pdev->dev.kobj, &gpio_attr_group);
	if (unlikely(rc < 0)) {
		pr_err("[BT]sysfs_merge_group failed!\n");
	}
#endif

#ifdef BT_LPM_ENABLE
	rc = bcm_bt_lpm_init(pdev);
	if (rc) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);
		gpio_free(gpio_bt_host_wake);
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_power);
	}
#endif

#ifdef BT_FACTORY_MODE
	if (device_create_file(&pdev->dev, &dev_attr_bt_test_mode))
		pr_info("[BT] bcm4339 factory sys file create failed\n");
#endif

	if (device_create_file(&pdev->dev, &dev_attr_bt_wake))
		pr_info("[BT] bcm4339 bt_wake sys file create failed\n");

#ifndef USE_WAKE_PEER
	spin_lock_init(&bt_slock_i);
	bt_slock = &bt_slock_i;
#endif

	pr_info("[BT] bcm4339 probe END\n");
	return rc;
}

static int bcm4339_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);
	gpio_free(gpio_bt_host_wake);
	gpio_free(gpio_bt_wake);
	gpio_free(gpio_bt_power);

	wake_lock_destroy(&bt_lpm.host_wake_lock);
	wake_lock_destroy(&bt_lpm.bt_wake_lock);
	return 0;
}

static const struct of_device_id meizu_bt_match[] = {
	{
		.compatible = "broadcom,bcm43455_bluetooth",
	},
	{},
};
MODULE_DEVICE_TABLE(of, meizu_bt_match);

static struct platform_driver bcm4339_bluetooth_platform_driver = {
	.probe = bcm4339_bluetooth_probe,
	.remove = bcm4339_bluetooth_remove,
	.driver = {
		.name = "bcm4339_bluetooth",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(meizu_bt_match),
	},
};

static int __init bcm4339_bluetooth_init(void)
{
	return platform_driver_register(&bcm4339_bluetooth_platform_driver);
}

static void __exit bcm4339_bluetooth_exit(void)
{
	platform_driver_unregister(&bcm4339_bluetooth_platform_driver);
}


module_init(bcm4339_bluetooth_init);
module_exit(bcm4339_bluetooth_exit);

MODULE_ALIAS("platform:bcm4339");
MODULE_DESCRIPTION("bcm4339_bluetooth");
MODULE_AUTHOR("QuDao<qudao@meizu.com>");
MODULE_LICENSE("GPL");
