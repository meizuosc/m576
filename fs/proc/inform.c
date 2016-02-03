/*
 * export boot mode register
 */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <mach/regs-pmu.h>
#include <linux/io.h>

static u32 bootinfo_val = 0;

static int show_inform(struct seq_file *p, void *v)
{
	seq_printf(p, "0x%08x\n", bootinfo_val);
	return 0;
}

static int inform_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_inform, NULL);
}

static const struct file_operations proc_inform_fops = {
	.open       = inform_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int __init proc_inform_init(void)
{
	struct proc_dir_entry *inform_dir;

	bootinfo_val = __raw_readl(EXYNOS_PMU_SYSIP_DAT0);
	__raw_writel(0x01000000, EXYNOS_PMU_SYSIP_DAT0);

	inform_dir = proc_mkdir("inform", NULL);
	proc_create("bootinfo", 0, inform_dir, &proc_inform_fops);

	return 0;
}
module_init(proc_inform_init);
