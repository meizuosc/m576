/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * EXYNOS - support to view information of Core status
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include <mach/pmu.h>
#include <mach/regs-pmu.h>

/* /sys/devices/system/exynos_info */
static struct bus_type core_subsys = {
		.name = "exynos_info",
		.dev_name = "exynos_info",
};

static unsigned int cpu_state(unsigned int cpu)
{
	unsigned int state, offset, cluster, core;

	core = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 0);
	cluster = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	offset = (cluster << 2) + core;

	state = __raw_readl(EXYNOS_PMU_CPU_STATUS(offset));

	return (state & 0xf) != 0;
}

static unsigned int get_cluster_type(unsigned int cpu)
{
	unsigned int cluster;

	cluster = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);

	return cluster;
}

static unsigned int L2_state(enum cluster_type cluster)
{
	unsigned int state;

	state = __raw_readl(EXYNOS_PMU_L2_STATUS(cluster));

	return (state & 0x7) != 0;
}

static unsigned int noncpu_state(enum cluster_type cluster)
{
	unsigned int state;

	state = __raw_readl(EXYNOS_PMU_NONCPU_STATUS(cluster));

	return (state & 0xf) != 0;
}

static ssize_t exynos_core_status_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
	unsigned int cpu, cluster, v;

	for_each_possible_cpu(cpu) {
		v = cpu_state(cpu);
		cluster = get_cluster_type(cpu);
		n += scnprintf(buf + n, 20, "%s\tcpu %d : %d\n",
				cluster ? "LITTLE" : "big", cpu, v);
	}

	for (cluster = 0; cluster < CLUSTER_TYPE_MAX; cluster++) {
		v = L2_state(cluster);
		n += scnprintf(buf + n, 20, "%s	L2 : %d\t",
				cluster ? "LITTLE" : "big", v);

		v = noncpu_state(cluster);
		n += scnprintf(buf + n , 20, "noncpu : %d\n", v);
	}

	return n;
}

static struct kobj_attribute exynos_core_status_attr =
	__ATTR(core_status, 0644, exynos_core_status_show, NULL);

static struct attribute *exynos_core_sysfs_attrs[] = {
	&exynos_core_status_attr.attr,
	NULL,
};

static struct attribute_group exynos_core_sysfs_group = {
	.attrs = exynos_core_sysfs_attrs,
};

static const struct attribute_group *exynos_core_sysfs_groups[] = {
	&exynos_core_sysfs_group,
	NULL,
};

static int __init exynos_core_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&core_subsys, exynos_core_sysfs_groups);
	if (ret)
		pr_err("Fail to register exynos core subsys\n");

	return ret;
}

late_initcall(exynos_core_sysfs_init);
