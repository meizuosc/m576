/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

static unsigned long g_mif_min_freq;
static unsigned long g_mif_max_freq;
static unsigned long g_mif_max_load;
static unsigned int g_mif_usage;
static unsigned long g_mif_cur_freq;

void set_mif_usage(long long busy_time, long long total_time)
{
	g_mif_usage = (unsigned int)div64_u64(busy_time * 100, total_time);
}

void set_mif_cur_freq(unsigned long cur_freq)
{
	g_mif_cur_freq = cur_freq / 1000;
}

void set_mif_freq_range(unsigned long min_freq, unsigned long max_freq)
{
	g_mif_min_freq = min_freq / 1000;
	g_mif_max_freq = max_freq / 1000;
	g_mif_max_load = g_mif_max_freq * 1;
}

#define BUS_EVENT_VALUE 188 //100/53

unsigned int calculate_load_usage(void)
{
	unsigned int load_usage = 0;
	// Max Freq : 1000 = curr_load : radio(0~100)
	// curr_load = curr_freq * usage
	load_usage = ((unsigned int)((g_mif_usage * g_mif_cur_freq / g_mif_max_load)*(BUS_EVENT_VALUE)))/100;
	if(load_usage > 100) {
//		printk("%s over 100\n",__func__);
		load_usage = 100;
	}
	return load_usage;	
}

static ssize_t data_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	ssize_t size = 0;
	unsigned int load_usage = calculate_load_usage();
	char *buffer = (char *)kzalloc(PAGE_SIZE, GFP_KERNEL);
	size = sprintf(buffer, "%u\n", load_usage);
	size = simple_read_from_buffer(data, count, ppos, buffer, size);
	kfree(buffer);
	return size;
}

static const struct file_operations data_ops = {
	.open		= simple_open,
	.read		= data_read,
	.llseek		= no_llseek,
};

static void initialize_debug_fs(void)
{
	struct dentry *root;
	root = debugfs_create_dir("exynos_traffic_monitor", NULL);
	if (IS_ERR(root)) {
		pr_err("Debug fs is failed. (%ld)\n", PTR_ERR(root));
	}
	debugfs_create_file("MIF", S_IRUGO, root, NULL, &data_ops);
}

static int exynos_traffic_monitor_probe(struct platform_device *pdev)
{
	int ret = 0;
	printk("exynos_traffic_monitor_probe\n");

	initialize_debug_fs();
	return ret;
}

static int exynos_traffic_monitor_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver exynos_traffic_monitor_driver = {
	.driver = {
		.name = "exynos_traffic",
		.owner = THIS_MODULE,
	},
	.probe = exynos_traffic_monitor_probe,
	.remove = exynos_traffic_monitor_remove,
};

static struct platform_device exynos_traffic_monitor_device = {
	.name	= "exynos_traffic",
	.id		= -1,
};

static int __init exynos_traffic_monitor_init(void)
{
	int ret;
	ret = platform_device_register(&exynos_traffic_monitor_device);
	if (ret)
		return ret;
	return platform_driver_register(&exynos_traffic_monitor_driver);
}
subsys_initcall(exynos_traffic_monitor_init);

static void __exit exynos_traffic_monitor_exit(void)
{
	platform_driver_unregister(&exynos_traffic_monitor_driver);
	platform_device_unregister(&exynos_traffic_monitor_device);
}
module_exit(exynos_traffic_monitor_exit);
