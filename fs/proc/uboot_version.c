#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>

static char uboot_version_str[32] = "unknown";

static int show_uboot_version(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%s\n", uboot_version_str);
	return 0;
}

static int uboot_version_open(struct inode *inode, struct file *fp)
{
	return single_open(fp, show_uboot_version, NULL);
}

static const struct file_operations proc_uboot_version_fops = {
	.open = uboot_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_uboot_version_init(void)
{
	struct device_node *np;
	const char *str;

	/* Create /proc/uboot_version to indicate uboot version, Eng or User */
	proc_create("uboot_version", 0, NULL, &proc_uboot_version_fops);

	np = of_find_node_by_path("/bootinfo");
	if (np == NULL) {
		pr_err("%s: Can't find /bootinfo node from FDT.\n", __func__);
		return -1;
	}

	if (of_property_read_string(np, "uboot_version", &str) < 0) {
		pr_err("%s: read property uboot_version error.\n", __func__);
		return -1;
	}

	strlcpy(uboot_version_str, str, sizeof(uboot_version_str));

	return 0;
}

late_initcall(proc_uboot_version_init);
