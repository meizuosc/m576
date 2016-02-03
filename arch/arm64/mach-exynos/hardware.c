/* arch/arm/mach-exynos/hardware.c
 *
 * Meizu Mobilephone Hardware information support
 *
 * Copyright (c) 2014 MEIZU Technology Co., Ltd.
 * Author: xuhanlong <xuhanlong@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

static char machine_type[64] = "Unknown";
static u32 board_version = 0;

extern int sd_partition_rw(const char *part_name, int write, loff_t offset,
						void *buffer, size_t len);
extern int get_bt_mac_from_device(unsigned char *buf);


u32 meizu_board_version(void)
{
	return board_version;
}

static u64 meizu_hardware_version(void)
{
	return 0;
}

extern bool is_white_lcd(void);
static int show_hwinfo(struct seq_file *seq, void *v)
{
	seq_printf(seq, "SOC             : %s\n", "EXYNOS7420");
	seq_printf(seq, "SOC Vendor      : %s\n", "SAMSUNG");
	seq_printf(seq, "Machine Type    : %s\n", machine_type);
	seq_printf(seq, "Board Version   : %d\n", board_version);
	seq_printf(seq, "Hardware Version: %#llx\n", meizu_hardware_version());
	seq_printf(seq, "TP Type         : %s\n", is_white_lcd() ? "W" : "B");
	return 0;
}

static int hwinfo_open(struct inode *inode, struct file *fp)
{
	return single_open(fp, show_hwinfo, NULL);
}

static const struct file_operations proc_hwinfo_fops = {
	.open = hwinfo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_hwinfo_init(void)
{
	struct device_node *np;
	const char *type;

	proc_create("hwinfo", 0, NULL, &proc_hwinfo_fops);

	np = of_find_node_by_path("/bootinfo");
	if (np == NULL) {
		pr_err("%s: Can't find /bootinfo node from FDT.\n", __func__);
		return -1;
	}

	if (of_property_read_u32(np, "board_version", &board_version) < 0) {
		pr_err("%s: read property board_version error.\n", __func__);
		return -1;
	}

	if (of_property_read_string(np, "machine_type", &type) < 0) {
		pr_err("%s: read property machine_type error.\n", __func__);
		return -1;
	}
	snprintf(machine_type, sizeof(machine_type), "%s", type);

	return 0;
}

late_initcall(proc_hwinfo_init);

MODULE_DESCRIPTION("Meizu Mobilephone Hardware Information");
MODULE_AUTHOR("xuhanlong <xuhanlong@meizu.com>");
MODULE_LICENSE("GPL");
