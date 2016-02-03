#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>

static char uboot_git_version_str[128] = "unknown";

static int show_uboot_git_version(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%s\n", uboot_git_version_str);
	return 0;
}

static int uboot_git_version_open(struct inode *inode, struct file *fp)
{
	return single_open(fp, show_uboot_git_version, NULL);
}

static const struct file_operations proc_uboot_git_version_fops = {
	.open = uboot_git_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_uboot_git_version_init(void)
{
	struct device_node *np;
	const char *str;

	/* Create /proc/uboot_git_version to indicate uboot git version */
	proc_create("uboot_git_version", 0, NULL, &proc_uboot_git_version_fops);

	np = of_find_node_by_path("/bootinfo");
	if (np == NULL) {
		pr_err("%s: Can't find /bootinfo node from FDT.\n", __func__);
		return -1;
	}

	if (of_property_read_string(np, "uboot_git_version", &str) < 0) {
		pr_err("%s: read property uboot_git_version error.\n", __func__);
		return -1;
	}

	strlcpy(uboot_git_version_str, str, sizeof(uboot_git_version_str));

	return 0;
}

late_initcall(proc_uboot_git_version_init);
