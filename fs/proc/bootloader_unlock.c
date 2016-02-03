#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>

static u32 bootloader_unlock = 0;

static int show_bootloader_unlock(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%u\n", bootloader_unlock);
	return 0;
}

static int bootloader_unlock_open(struct inode *inode, struct file *fp)
{
	return single_open(fp, show_bootloader_unlock, NULL);
}

static const struct file_operations proc_bootloader_unlock_fops = {
	.open = bootloader_unlock_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_bootloader_unlock_init(void)
{
	struct device_node *np;

	/*
	 * Create /proc/bootloader_unlock to indicate bootloader unlock or not.
	 * 0: locked, 1: unlocked.
	 */
	proc_create("bootloader_unlock", 0, NULL, &proc_bootloader_unlock_fops);

	np = of_find_node_by_path("/bootinfo");
	if (np == NULL) {
		pr_err("%s: Can't find /bootinfo node from FDT.\n", __func__);
		return -1;
	}

	if (of_property_read_u32(np, "bootloader_unlock", &bootloader_unlock) < 0) {
		pr_err("%s: read property bootloader_unlock error.\n", __func__);
		return -1;
	}

	return 0;
}

late_initcall(proc_bootloader_unlock_init);
