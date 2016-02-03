/*
 * linux/arch/arm/mach-exynos/factory_test.c
 *
 * Copyright (C) 2014 Meizu Technology Co.Ltd, Zhuhai, China
 * Author: 	QuDao	<qudao@meizu.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/board-mz-factory-test.h>

static int gpio_factory_mode;
static int gpio_volup_irq;
static int gpio_voldown_irq;
#ifdef MX_FACTORY_HAS_LED
static int gpio_test_led;
#endif

/* Global variable for factory test */
int (*mx_is_factory_test_mode)(int type);
int (*mx_set_factory_test_led)(int on);


static int is_factory_test_mode(int type)
{
	int gpio1, gpio2, gpio3;
	int ret = 0;

	gpio1 = gpio_factory_mode;
	gpio2 = gpio_volup_irq;
	gpio3 = gpio_voldown_irq;

	switch(type) {
	case MX_FACTORY_TEST_BT:
		if(!gpio_get_value(gpio1) && !gpio_get_value(gpio2))
			ret = 1;
		break;
	case MX_FACTORY_TEST_CAMERA:
		if(!gpio_get_value(gpio1) && !gpio_get_value(gpio3))
			ret = 1;
		break;
	default:
		if(!gpio_get_value(gpio1))
			ret = 1;
		break;
	}

	pr_info("%s(), type:%d, ret:%d\n", __func__, type, ret);
	return ret;

}

#ifdef MX_FACTORY_HAS_LED
static int set_factory_test_led(int on)
{
	int gpio, ret;
	int gpio_value = on ? GPIOF_OUT_INIT_HIGH: GPIOF_OUT_INIT_LOW;

	gpio = gpio_test_led;

	ret = gpio_request_one(gpio, gpio_value, "mx_test_led");
	if (ret)
		return ret;

	gpio_free(gpio);

	return 0;
}
#else
static int set_factory_test_led(int on)
{
	return 0;
}
#endif

#ifdef CONFIG_OF
static int mz_factory_test_parse_dt(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	pr_info("%s(), ++++++++++++\n", __func__);

	if (!np) {
		pr_info("%s(), Err!device_node is NULL\n", __func__);
		return -EINVAL;
	}

	gpio_factory_mode = of_get_gpio(np, 0);
	pr_info("gpio_factory_mode:%d\n", gpio_factory_mode);
	gpio_volup_irq = of_get_gpio(np, 1);
	pr_info("gpio_volup_irq:%d\n", gpio_volup_irq);
	gpio_voldown_irq = of_get_gpio(np, 2);
	pr_info("gpio_voldown_irq:%d\n", gpio_voldown_irq);
	#ifdef MX_FACTORY_HAS_LED
	gpio_test_led = of_get_gpio(np, 3);
	pr_info("gpio_test_led:%d\n", gpio_test_led);
	#endif

	return 0;
}
#else
static int mz_factory_test_parse_dt(struct platform_device *pdev)
{
	return -ENXIO;
}
#endif


static int mz_factory_test_probe(struct platform_device *pdev)
{
	int ret;
	ret = mz_factory_test_parse_dt(pdev);
	if (ret) {
		pr_info("%s(), parse dt err! ret:%d\n", __func__, ret);
		return ret;
	}

	mx_is_factory_test_mode = is_factory_test_mode;
	mx_set_factory_test_led = set_factory_test_led;
	return 0;
}

static int mz_factory_test_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mz_factory_test[] = {
	{
		.compatible = "mz_factory_test",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mz_factory_test);


static struct platform_driver mz_factory_test_platform_driver = {
	.probe = mz_factory_test_probe,
	.remove = mz_factory_test_remove,
	.driver = {
		   .name = "mz_factory_test",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(mz_factory_test),
	},
};

static int __init mz_factory_test_init(void)
{
	return platform_driver_register(&mz_factory_test_platform_driver);
}

static void __exit mz_factory_test_exit(void)
{
	platform_driver_unregister(&mz_factory_test_platform_driver);
}

module_init(mz_factory_test_init);
module_exit(mz_factory_test_exit);

MODULE_ALIAS("platform:mz_factory_test");
MODULE_DESCRIPTION("mz_factory_test");
MODULE_AUTHOR("QuDao<qudao@meizu.com>");
MODULE_LICENSE("GPL");

