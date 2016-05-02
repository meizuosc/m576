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
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/serial_s3c.h>
#include <linux/board-mz-factory-test.h>

//#include <asm/mach-types.h>

#ifdef CONFIG_SOC_EXYNOS7420
#include <asm/gpio.h>
#else
#include <mach/gpio.h>
#endif
#include <plat/gpio-cfg.h>

#define BT_LPM_ENABLE

extern s3c_wake_peer_t s3c2410_serial_wake_peer[CONFIG_SERIAL_SAMSUNG_UARTS];

static struct rfkill *bt_rfkill;
#ifdef BT_LPM_ENABLE
static int bt_wake_state = -1;
#endif

struct bcm_bt_lpm {
	int host_wake;
	int dev_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uport;

	struct wake_lock host_wake_lock;
	struct wake_lock bt_wake_lock;
};

struct bcm_bt_gpio {
	int bt_en;
	int bt_wake;
	int bt_hostwake;
	int irq;
};

static struct bcm_bt_lpm bt_lpm;
static struct bcm_bt_gpio bt_gpio;
static int bt_is_running=0;
static int bt_uport = -1;

int check_bt_op(void)
{
	return bt_is_running;
}
EXPORT_SYMBOL(check_bt_op);

extern int get_bt_mac_from_device(unsigned char *buf);

static int bcm43241_bt_get_mac_addr(unsigned char *buf)
{
	struct file *fp      = NULL;
	char macbuffer[18]	 = {0};
	mm_segment_t oldfs    = {0};
	char *bt_mac_file       = "/factory/bluetooth/bt_mac_addr";
	int ret = 0;
	int no_mac = 0;

	no_mac = !!get_bt_mac_from_device(buf);
	if(!no_mac) {
		fp = filp_open(bt_mac_file, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			pr_info("%s(), open %s err: %ld\n", __func__, bt_mac_file, PTR_ERR(fp));
			snprintf(macbuffer, sizeof(macbuffer),"%02X:%02X:%02X:%02X:%02X:%02X\n",
					buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
			fp = filp_open(bt_mac_file, O_RDWR | O_CREAT, 0644);
			if (IS_ERR(fp)) {
				pr_err("%s:create file %s error(%ld)\n", __func__, bt_mac_file, PTR_ERR(fp));
			} else {
				oldfs = get_fs();
				set_fs(get_ds());

				if (fp->f_mode & FMODE_WRITE) {
					ret = fp->f_op->write(fp, (const char *)macbuffer,
							sizeof(macbuffer), &fp->f_pos);
					if (ret < 0)
						pr_err("%s:write file %s error(%d)\n", __func__, bt_mac_file, ret);
				}
				set_fs(oldfs);
				filp_close(fp, NULL);
			}
		}
		pr_info("bt mac address mac=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
	}
	return 0;
}

static int bcm43241_bt_rfkill_set_power(void *data, bool blocked)
{
	unsigned char bt_buf[6] = {0};
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	if (!blocked) {
		pr_info("[BT] Bluetooth Power On.\n");

		bcm43241_bt_get_mac_addr(bt_buf);
#ifdef BT_LPM_ENABLE
		if ( irq_set_irq_wake(bt_gpio.irq, 1)) {
			pr_err("[BT] Set_irq_wake failed.\n");
			return -1;
		}
#endif
		gpio_set_value(bt_gpio.bt_en, 1);
		bt_is_running = 1;
		msleep(100);
		gpio_set_value(bt_gpio.bt_en, 0);
		msleep(100);
		gpio_set_value(bt_gpio.bt_en, 1);

	} else {
		pr_info("[BT] Bluetooth Power Off.\n");

#ifdef BT_LPM_ENABLE
		if (gpio_get_value(bt_gpio.bt_en) && irq_set_irq_wake(bt_gpio.irq, 0)) {
			pr_err("[BT] Release_irq_wake failed.\n");
			return -1;
		}
#endif
		bt_is_running = 0;
		gpio_set_value(bt_gpio.bt_en, 0);
	}
	return 0;
}

static const struct rfkill_ops bcm43241_bt_rfkill_ops = {
	.set_block = bcm43241_bt_rfkill_set_power,
};

#ifdef BT_LPM_ENABLE
static void set_wake_locked(int wake)
{
#ifdef CONFIG_BT_UART_IN_AUDIO
	struct uart_port *port = bt_lpm.uport;
#endif

	if (wake)
		wake_lock(&bt_lpm.bt_wake_lock);

	gpio_set_value(bt_gpio.bt_wake, wake);
	bt_lpm.dev_wake = wake;

	if (bt_wake_state != wake)
	{
#ifdef CONFIG_BT_UART_IN_AUDIO
		if(bt_lpm.host_wake)
		{
			if(wake)
				port->ops->set_wake(port, wake);
		}
		else
		{
			port->ops->set_wake(port, wake);
		}
#endif
		pr_info("[BT] set_wake_locked value = %d\n", wake);
		bt_wake_state = wake;
	}
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer)
{
	if (bt_lpm.uport != NULL)
		set_wake_locked(0);

	bt_is_running = 0;

	wake_lock_timeout(&bt_lpm.bt_wake_lock, HZ/2);

	return HRTIMER_NORESTART;
}

void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport)
{
	bt_lpm.uport = uport;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);
	bt_is_running = 1;
	set_wake_locked(1);

//	pr_info("[BT] bcm_bt_lpm_exit_lpm_locked\n");
	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);
}

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
		//pr_err("[BT] update_host_wake_locked host_wake is deasserted. release wakelock in 1s\n");
		wake_lock_timeout(&bt_lpm.host_wake_lock, HZ/2);
	}
}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
#ifdef CONFIG_BT_UART_IN_AUDIO
	struct uart_port *port = bt_lpm.uport;
#endif
	int host_wake;

	host_wake = gpio_get_value(bt_gpio.bt_hostwake);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);

	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		pr_err("[BT] host_wake_isr uport is null\n");
		return IRQ_HANDLED;
	}

#ifdef CONFIG_BT_UART_IN_AUDIO
	if(bt_lpm.dev_wake)
	{
		if(host_wake)
			port->ops->set_wake(port, host_wake);
	}
	else
	{
		port->ops->set_wake(port, host_wake);
	}
#endif
	update_host_wake_locked(host_wake);

	return IRQ_HANDLED;
}

static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int ret;

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */ /*1->3*//*3->4*/
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;

	wake_lock_init(&bt_lpm.host_wake_lock, WAKE_LOCK_SUSPEND,
			 "bt_host_wake");
	wake_lock_init(&bt_lpm.bt_wake_lock, WAKE_LOCK_SUSPEND,
			 "bt_device_wake");

	s3c2410_serial_wake_peer[bt_uport] = (s3c_wake_peer_t) bcm_bt_lpm_exit_lpm_locked;

	bt_gpio.irq = gpio_to_irq(bt_gpio.bt_hostwake);
	ret = request_irq(bt_gpio.irq, host_wake_isr, IRQF_TRIGGER_RISING,
		"bt_host_wake", NULL);
	if (ret) {
		pr_err("[BT] Request_host wake irq failed.\n");
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_FACTORY_KERNEL
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
	return sprintf(buf, "%d\n", gpio_get_value(bt_gpio.bt_wake));
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
	gpio_set_value(bt_gpio.bt_wake, value);

	return count;
}

static DEVICE_ATTR(bt_wake, S_IRUGO|S_IWUSR | S_IWGRP,
		bt_wake_show, bt_wake_store);

static int bcm43241_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;
	int ret = -1;
	struct device_node *uart_np = of_get_parent(pdev->dev.of_node);

	if (NULL != uart_np)
		ret = of_alias_get_id(uart_np, "uart");

	if (ret < 0) {
		pr_info("UART aliases are not defined(%d), use def uart port.\n", ret);
#ifdef CONFIG_SOC_EXYNOS5422
		bt_uport = 0;
#elif defined(CONFIG_SOC_EXYNOS7420)
		bt_uport = 4;
#else
		bt_uport = 3;
#endif
	} else {
		bt_uport = ret;
	}

	pr_info("[BT] bcm43241_bluetooth_probe, uart_port(%d).\n", bt_uport);

	bt_gpio.bt_en = of_get_gpio(pdev->dev.of_node, 0);

	rc = gpio_request(bt_gpio.bt_en, "bten_gpio");

	if (unlikely(rc)) {
		pr_err("[BT] bt_gpio.bt_en request failed.\n");
		return rc;
	}

	bt_gpio.bt_wake =of_get_gpio(pdev->dev.of_node, 1);

	rc = gpio_request(bt_gpio.bt_wake, "btwake_gpio");

	if (unlikely(rc)) {
		pr_err("[BT] bt_gpio.bt_wake request failed.\n");
		gpio_free(bt_gpio.bt_en);
		return rc;
	}

	bt_gpio.bt_hostwake =of_get_gpio(pdev->dev.of_node, 2);

	rc = gpio_request(bt_gpio.bt_hostwake,"bthostwake_gpio");

	if (unlikely(rc)) {
		pr_err("[BT] bt_gpio.bt_hostwake request failed.\n");
		gpio_free(bt_gpio.bt_wake);
		gpio_free(bt_gpio.bt_en);
		return rc;
	}

	gpio_direction_input(bt_gpio.bt_hostwake);
	gpio_direction_output(bt_gpio.bt_wake, 1);
	gpio_direction_output(bt_gpio.bt_en, 0);

	bt_rfkill = rfkill_alloc("bcm43241 Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bcm43241_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		pr_err("[BT] bt_rfkill alloc failed.\n");
		gpio_free(bt_gpio.bt_hostwake);
		gpio_free(bt_gpio.bt_wake);
		gpio_free(bt_gpio.bt_en);
		return -ENOMEM;
	}

	rfkill_init_sw_state(bt_rfkill, 0);

	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		pr_err("[BT] bt_rfkill register failed.\n");
		rfkill_destroy(bt_rfkill);
		gpio_free(bt_gpio.bt_hostwake);
		gpio_free(bt_gpio.bt_wake);
		gpio_free(bt_gpio.bt_en);
		return -1;
	}

	rfkill_set_sw_state(bt_rfkill, true);

#ifdef BT_LPM_ENABLE
	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);

		gpio_free(bt_gpio.bt_hostwake);
		gpio_free(bt_gpio.bt_wake);
		gpio_free(bt_gpio.bt_en);
	}
#endif

#ifdef CONFIG_FACTORY_KERNEL
	if (device_create_file(&pdev->dev, &dev_attr_bt_test_mode))
		pr_info("[BT] bcm4339 factory sys file create failed\n");
#endif

	if (device_create_file(&pdev->dev, &dev_attr_bt_wake))
		pr_info("[BT] bcm4339 bt_wake sys file create failed\n");

	pr_info("[BT] bcm43241_bluetooth_probe End \n");
	return rc;
}

static int bcm43241_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(bt_gpio.bt_en);
	gpio_free(bt_gpio.bt_wake);
	gpio_free(bt_gpio.bt_hostwake);

	wake_lock_destroy(&bt_lpm.host_wake_lock);
	wake_lock_destroy(&bt_lpm.bt_wake_lock);

	return 0;
}

#if defined (CONFIG_OF)
static const struct of_device_id exynos_bluetooth_match[] = {
	{
		.compatible = "broadcom,bcm43241",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_bluetooth_match);

static struct platform_driver bcm43241_bluetooth_platform_driver = {
	.probe = bcm43241_bluetooth_probe,
	.remove = bcm43241_bluetooth_remove,
	.driver = {
		   .name = "bcm43241_bluetooth",
		   .owner = THIS_MODULE,
		   .of_match_table = exynos_bluetooth_match,
		   },
};

module_platform_driver(bcm43241_bluetooth_platform_driver);
#endif
MODULE_ALIAS("platform:bcm43241");
MODULE_DESCRIPTION("bcm43241_bluetooth");
MODULE_LICENSE("GPL");
