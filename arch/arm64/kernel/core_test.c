/*
 * Copyright 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/module.h>
#include <asm/core_regs.h>

struct register_type reg_sctlr = {
	.name = "SCTLR",
	.read_reg = armv8_sctlr_read
};

struct register_type reg_mair = {
	.name = "MAIR",
	.read_reg = armv8_mair_read
};

struct register_type reg_cpuactlr = {
	.name = "CPUACTLR",
	.read_reg = armv8_cpuactlr_read
};

struct register_type reg_cpuectlr = {
	.name = "CPUECTLR",
	.read_reg = armv8_cpuectlr_read
};

struct register_type reg_l2ctlr = {
	.name = "L2CTLR",
	.read_reg = armv8_l2ctlr_read
};

struct register_type reg_l2actlr = {
	.name = "L2ACTLR",
	.read_reg = armv8_l2actlr_read
};

struct register_type reg_l2ectlr = {
	.name = "L2ECTLR",
	.read_reg = armv8_l2ectlr_read
};

struct register_type reg_mpidr = {
	.name = "MPIDR",
	.read_reg = armv8_mpidr_read
};

struct register_type reg_midr = {
	.name = "MIDR",
	.read_reg = armv8_midr_read
};

struct register_type reg_revidr = {
	.name = "REVIDR",
	.read_reg = armv8_revidr_read
};

#define CORE_REG_DO_NOT_COMPARE UL(0xAAAABBBBCCCCDDDD)
static struct core_register a53_regs[] = {
	{
		.reg = &reg_sctlr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_mair,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_cpuactlr,
		.val = 0x00000000080CA000,
	}, {
		.reg = &reg_cpuectlr,
		.val = 0x0000000000000040,
	}, {
		.reg = &reg_l2ctlr,
		.val = 0x0000000003000020,
	}, {
		.reg = &reg_l2actlr,
		.val = 0x0000000000000008,
	}, {
		.reg = &reg_l2ectlr,
		.val = 0x0000000000000007,
	}, {
		.reg = &reg_mpidr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_midr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_revidr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {}
};

static struct core_register a57_regs[] = {
	{
		.reg = &reg_sctlr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_mair,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_cpuactlr,
		.val = 0x0808100001000010,
	}, {
		.reg = &reg_cpuectlr,
		.val = 0x0000001B00000040,
	}, {
		.reg = &reg_l2ctlr,
		.val = 0x0000000003702482,
	}, {
		.reg = &reg_l2actlr,
		.val = 0x0000000000000018,
	}, {
		.reg = &reg_l2ectlr,
		.val = 0x0000000000000007,
	}, {
		.reg = &reg_mpidr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_midr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {
		.reg = &reg_revidr,
		.val = CORE_REG_DO_NOT_COMPARE,
	}, {}
};

enum armv8_core_type {
	A57_CORE,
	A53_CORE,
};

static enum armv8_core_type get_core_type(void)
{
	u64 cluster_id = armv8_mpidr_read() >> 8 & 0xff;

	if (cluster_id & 0xff)
		return A53_CORE;
	else
		return A57_CORE;
}

int coretest_init(void)
{
	u64 val;
	int fail_cnt = 0;
	enum armv8_core_type core_type = get_core_type();
	struct core_register *regs;
	u32 midr = (u32)armv8_midr_read();
	u32 revidr = (u32)armv8_revidr_read();

	pr_info("[Core type: %s, Rev: R%xP%x (revidr:0x%08x)]\n",
			core_type ? "A53" : "A57",
			(midr & 0xf00000) >> 20,
			midr & 0xf,
			revidr);

	switch (core_type) {
	case A57_CORE:
		regs = a57_regs;
		break;
	case A53_CORE:
		regs = a53_regs;
		break;
	}

	while (regs->reg) {
		val = regs->reg->read_reg();
		if (regs->val != CORE_REG_DO_NOT_COMPARE && val != regs->val) {
			pr_info("Fail: %15s: req: 0x%016llX -> set: 0x%016llX\n",
				regs->reg->name,
				regs->val,
				val);
			fail_cnt++;
		} else {
			pr_info("%15s: 0x%016llX\n", regs->reg->name, val);
		}
		regs++;
	}

	pr_info("Test result: %d fail\n", fail_cnt);
	return 0;
}

static int core;

static struct task_struct *task;
static int thread_func(void *data)
{
	coretest_init();
	return 0;
}

static int test_init(void)
{
	task = kthread_create(thread_func, NULL, "thread%u", 0);
	kthread_bind(task, core);
	wake_up_process(task);
	return 0;
}

static ssize_t core_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return sprintf(buf, "core = %d\n", core);
}

static ssize_t core_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%d", &core);
	test_init();

	return count;
}

static struct kobj_attribute core_attribute = __ATTR(core, 0666, core_show, core_store);

/* create group */
static struct attribute *attrs[] = {
	&core_attribute.attr,
	NULL,
};

/* if a name, subdir will be created */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *core_kobj;

static int __init core_init(void)
{
	int ret;

	/* location: /sys/kernel/ */
	core_kobj = kobject_create_and_add("core_reg", kernel_kobj);
	if (!core_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(core_kobj, &attr_group);
	if (ret)
		kobject_put(core_kobj);

	return ret;
}

static void __exit core_exit(void)
{
	kobject_put(core_kobj);
}

module_init(core_init);
module_exit(core_exit);
MODULE_LICENSE("GPL");
