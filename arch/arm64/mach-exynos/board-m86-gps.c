/*
 * Broadcom gps driver, Author: QuDao [qudao@meizu.com]
 * This is based UBLOX gps driver of jerrymo@meizu.com
 * Copyright (c) 2010 meizu Corporation
 *
 */

#define pr_fmt(fmt)	"BRCM_GPS: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>

//#define MEIZU_SPECIAL
#define USE_HOST_WAKE
static int gps_power;

static struct miscdevice gps_miscdev = {
	.name	= "gps",
	.minor	= MISC_DYNAMIC_MINOR,
};

int check_gps_op(void)
{
	return gpio_get_value(gps_power);
}

#ifdef USE_HOST_WAKE
static int gps_host_wake;

struct gps_lpm {
	int host_wake;
	struct wake_lock host_wake_lock;
} gps_lpm;
#endif

static ssize_t gps_power_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int value;

	value = check_gps_op();
	pr_info("%s():power %d\n", __func__, value);
	return sprintf(buf, "%d\n",  value);
}

static ssize_t gps_power_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long value = simple_strtoul(buf, NULL, 10);

	pr_info("%s():power %ld.\n", __func__, value);

	gpio_set_value(gps_power, !!value);

	return count;
}

static DEVICE_ATTR(pwr, S_IRUGO | S_IWUSR,
		   gps_power_show, gps_power_store);

static struct attribute *gps_attributes[] = {
	&dev_attr_pwr.attr,
	NULL
};

static struct attribute_group gps_attribute_group = {
	.attrs = gps_attributes
};

#ifdef USE_HOST_WAKE
static void update_host_wake_locked(int host_wake)
{
	if (host_wake == gps_lpm.host_wake) {
		pr_info("%s(), host_wake:%d, gps_lpm.host_wake:%d\n",
			__func__, host_wake, gps_lpm.host_wake);
		return;
	}

	gps_lpm.host_wake = host_wake;

	if (host_wake) {
		pr_info("%s(), hold wake_lock \n", __func__);
		wake_lock(&gps_lpm.host_wake_lock);
	} else  {
		/* Take a timed wakelock, so that upper layers can take it.
		 * The chipset deasserts the hostwake lock, when there is no
		 * more data to send.
		 */
		pr_info("%s(), hold wake_lock_timeout \n", __func__);
		wake_lock_timeout(&gps_lpm.host_wake_lock, HZ/2);
	}
}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;

	host_wake = gpio_get_value(gps_host_wake);
	pr_info("%s(), read gps_host_wake gpio:%d\n", __func__, host_wake);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	update_host_wake_locked(host_wake);

	return IRQ_HANDLED;
}

static int gps_lpm_init(struct platform_device *pdev)
{
	int ret;
	int irq_host_wake;

	gps_lpm.host_wake = 0;
	wake_lock_init(&gps_lpm.host_wake_lock, WAKE_LOCK_SUSPEND,
			 "gps_host_wake");

	irq_host_wake = gpio_to_irq(gps_host_wake);
	pr_info("%s(), gps_host_wake irq:%d\n", __func__, irq_host_wake);
	ret = request_irq(irq_host_wake, host_wake_isr, IRQF_TRIGGER_HIGH,
		"gps_host_wake", NULL);
	if (ret) {
		pr_err("%s(), Request_host wake irq failed:%d.\n", __func__, ret);
		return ret;
	}

	ret = irq_set_irq_wake(irq_host_wake, 1);
	if (ret) {
		pr_err("%s(), Set_irq_wake failed:%d.\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_OF
static int gps_parse_dt(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	pr_info("%s(), ++++++++++++\n", __func__);

	if (!np) {
		pr_info("%s(), Err!device_node is NULL\n", __func__);
		return -EINVAL;
	}

	gps_power = of_get_gpio(np, 0);
	pr_info("%s(), gps_power: %d\n", __func__, gps_power);
	#ifdef USE_HOST_WAKE
	gps_host_wake = of_get_gpio(np, 1);
	pr_info("%s(), gps_host_wake:%d\n", __func__, gps_host_wake);
	#else
	pr_info("%s(), gps_host_wake is NOT enabled\n", __func__);
	#endif

	return 0;
}
#else
static int gps_parse_dt(struct platform_device *pdev)
{
	return -ENXIO;
}
#endif

static int gps_probe(struct platform_device *pdev)
{
	int ret;
	ret = gps_parse_dt(pdev);
	if (ret) {
		pr_info("%s(), parse dt err! ret:%d\n", __func__, ret);
		return ret;
	}

	ret = gpio_request_one(gps_power, GPIOF_OUT_INIT_LOW, "gps_power");
	if (ret) {
		pr_err("%s():fail to request gpio (gps_power): %d\n", __func__, ret);
		return ret;
	}

#ifdef MEIZU_SPECIAL
	if (mx_is_factory_test_mode(MX_FACTORY_TEST_BT)) {
		s3c_gpio_cfgpin(MEIZU_GPS_RTS, S3C_GPIO_INPUT);
		s3c_gpio_cfgpin(MEIZU_GPS_CTS, S3C_GPIO_INPUT);
		s3c_gpio_cfgpin(MEIZU_GPS_RXD, S3C_GPIO_INPUT);
		s3c_gpio_cfgpin(MEIZU_GPS_TXD, S3C_GPIO_INPUT);
		s3c_gpio_setpull(MEIZU_GPS_RTS, GPIO_PULL_DOWN);
		s3c_gpio_setpull(MEIZU_GPS_CTS, GPIO_PULL_DOWN);
		s3c_gpio_setpull(MEIZU_GPS_RXD, GPIO_PULL_DOWN);
		gpio_direction_output(gps_power, 1);
		printk("GPS in test mode!\n");
	}
#endif	

	#ifdef USE_HOST_WAKE
	ret = gps_lpm_init(pdev);
	if (ret) {
		pr_err("%s(), register gps as misc failed:%d\n",
			__func__, ret);
		goto err1;
	}
	#endif

	gps_miscdev.parent= &pdev->dev;
	ret = misc_register(&gps_miscdev);
	if (ret) {
		pr_err("%s(), register gps as misc failed:%d\n",
			__func__, ret);
		goto err1;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &gps_attribute_group);
	if (ret < 0) {
		pr_err("%s():sys create group fail !!\n", __func__);
		goto err2;
	}

	pr_info("gps successfully probed!\n");
	return 0;

err2:
	misc_deregister(&gps_miscdev);
err1:
	gpio_free(gps_power);
	return ret;
}

static int gps_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &gps_attribute_group);
	gpio_free(gps_power);
	#ifdef USE_HOST_WAKE
	gpio_free(gps_host_wake);
	wake_lock_destroy(&gps_lpm.host_wake_lock);
	#endif
	misc_deregister(&gps_miscdev);
	return 0;
}

static void gps_shutdown(struct platform_device *pdev)
{

	if (gpio_get_value(gps_power))
		gpio_set_value(gps_power, 0);
}

static const struct of_device_id meizu_gps_match[] = {
	{
		.compatible = "broadcom,bcm47531_gps",
	},
	{},
};
MODULE_DEVICE_TABLE(of, meizu_gps_match);

/*platform driver data*/
static struct platform_driver gps_driver = {
	.driver = {
		.name = "bcm47531_gps",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(meizu_gps_match),
	},
	.probe =   gps_probe,
	.remove = gps_remove,
	.shutdown = gps_shutdown,
};

static int __init gps_init(void)
{
	return platform_driver_register(&gps_driver);
}

static void __exit gps_exit(void)
{
	platform_driver_unregister(&gps_driver);
}

module_init(gps_init);
module_exit(gps_exit);

MODULE_AUTHOR("QuDao <qudao@meizu.com>");
MODULE_DESCRIPTION("broadcom gps driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
