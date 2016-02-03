/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/memblock.h>

#define NUM_MC		4
#define MC_OFFSET	0x100000

#define REG_MEMCONTROL	0x4
#define DSREF_EN	(0x1 << 5)
#define DPWRDN_EN	(0x1 << 1)

static volatile void __iomem *base;
static struct device *dev_mc;


static int enable_mc_powerdn(void)
{
	int i;
	u32 reg;

	for (i = 0; i < NUM_MC; i++) {
		reg = __raw_readl(base + MC_OFFSET * i + REG_MEMCONTROL);

		dev_info(dev_mc, "old val of MEMCONTROL(ch%d) is 0x%08x\n", i, reg);

		reg |= DSREF_EN;
		reg |= DPWRDN_EN;
		__raw_writel(reg, base + MC_OFFSET * i + REG_MEMCONTROL);

		dev_info(dev_mc, "new val of MEMCONTROL(ch%d) is 0x%08x\n", i, reg);
	}

	return 0;
}

int disable_mc_powerdn(void)
{
	int i;
	u32 reg;

	for (i = 0; i < NUM_MC; i++) {
		reg = __raw_readl(base + MC_OFFSET * i + REG_MEMCONTROL);

		dev_info(dev_mc, "old val of MEMCONTROL(ch%d) is 0x%08x\n", i, reg);

		reg &= ~DSREF_EN;
		reg &= ~DPWRDN_EN;
		__raw_writel(reg, base + MC_OFFSET * i + REG_MEMCONTROL);

		dev_info(dev_mc, "new val of MEMCONTROL(ch%d) is 0x%08x\n", i, reg);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(disable_mc_powerdn);

static int mc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Fail to get map register\n");
		return -1;
	}
	base = devm_request_and_ioremap(dev, res);

	dev_info(dev, "Loaded driver for memory controller\n");
	dev_info(dev, "Base address is 0x%p\n", base);

	dev_mc = dev;

	enable_mc_powerdn();

	return ret;
}

static int mc_remove(struct platform_device *pdev)
{
	/* unmapping */
	iounmap(base);

	return 0;
}

static const struct of_device_id mc_dt_match[] = {
	{
		.compatible = "samsung,exynos7420-mc",
	},
	{},
};

static struct platform_driver exynos_mc_driver = {
	.probe		= mc_probe,
	.remove		= mc_remove,
	.driver		= {
		.name	= "exynos-mc",
		.owner	= THIS_MODULE,
		.of_match_table = mc_dt_match,
	}
};

static int __init memory_controller_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&exynos_mc_driver);
	if (!ret)
		pr_info("%s: init function called\n",
			exynos_mc_driver.driver.name);

	return ret;
}

static void __exit memory_controller_exit(void)
{
	platform_driver_unregister(&exynos_mc_driver);
}
late_initcall(memory_controller_init);
module_exit(memory_controller_exit);

MODULE_DESCRIPTION("memory_controller");
MODULE_AUTHOR("Sungjinn Chung <sungjinn.chung@samsung.com>");
MODULE_LICENSE("GPL");
