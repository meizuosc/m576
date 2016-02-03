/*
 * linux/arch/arm/mach-exynos/exynos-coresight.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/smp.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/suspend.h>
#include <linux/smpboot.h>
#include <linux/delay.h>
#include <linux/exynos-ss.h>

#include <asm/core_regs.h>
#include <mach/pmu.h>
#include "coresight-priv.h"

#define etm_writel(base, val, off)	__raw_writel((val), base + off)
#define etm_readl(base, off)		__raw_readl(base + off)

#define SOFT_LOCK(base)				\
do { dsb(); isb(); etm_writel(base, 0x0, LAR); } while (0)

#define SOFT_UNLOCK(base)			\
do { etm_writel(base, OSLOCK_MAGIC, LAR); dsb(); isb(); } while (0)

struct cpu_info {
	void __iomem		*base;
	unsigned int		funnel_port;
};

struct exynos_etm_info {
	struct cpu_info		cpu[NR_CPUS];
	spinlock_t			spinlock;
	uint32_t			procsel;
	uint32_t			config;
	uint32_t			sync_period;
	uint32_t			victlr;
	uint32_t			etr_buf_addr;
	uint32_t			etr_buf_size;
	uint32_t			etr_buf_pointer;
};

static struct exynos_etm_info *g_etm_info;
static void __iomem *g_funnel_base[2];
#ifdef CONFIG_EXYNOS_CORESIGHT_ETB
static void __iomem *g_etb_base;
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETF
static void __iomem *g_etf_base;
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
static void __iomem *g_etr_base;
#endif
static DEFINE_PER_CPU(struct task_struct *, etm_task);

static void exynos_funnel_enable(void __iomem *funnel0_base,
					void __iomem *funnel1_base)
{
	unsigned int tmp;

	SOFT_UNLOCK(funnel0_base);
	tmp = etm_readl(funnel0_base, FUNCTRL);
	tmp = (tmp & 0x3ff) | 0x300;
	etm_writel(funnel0_base, tmp, FUNCTRL);
	etm_writel(funnel0_base, 0x0, FUNPRIORCTRL);
	SOFT_LOCK(funnel0_base);

	SOFT_UNLOCK(funnel1_base);
#ifdef CONFIG_EXYNOS_CORESIGHT_STM
	etm_writel(funnel1_base, 0x381, FUNCTRL);
#else
	etm_writel(funnel1_base, 0x301, FUNCTRL);
#endif
	etm_writel(funnel1_base, 0x0, FUNPRIORCTRL);
	SOFT_LOCK(funnel1_base);
}

#ifdef CONFIG_EXYNOS_CORESIGHT_ETF
static void exynos_etf_enable(void __iomem *etf_base)
{
	SOFT_UNLOCK(etf_base);
	etm_writel(etf_base, 0x0, TMCCTL);
	etm_writel(etf_base, 0x800, TMCRSZ);
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
    etm_writel(etf_base, 0x2, TMCMODE);
#else
	etm_writel(etf_base, 0x0, TMCMODE);
#endif
	etm_writel(etf_base, 0x0, TMCTGR);
	etm_writel(etf_base, 0x0, TMCFFCR);
	etm_writel(etf_base, 0x1, TMCCTL);
	SOFT_LOCK(etf_base);
}
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
static void exynos_etr_enable(void __iomem *etr_base)
{
	SOFT_UNLOCK(etr_base);
	etm_writel(etr_base, 0x0, TMCCTL);
	etm_writel(etr_base, g_etm_info->etr_buf_size, TMCRSZ);
	etm_writel(etr_base, 0x4, TMCTGR);
	etm_writel(etr_base, 0x0, TMCAXICTL);
	etm_writel(etr_base, g_etm_info->etr_buf_addr, TMCDBALO);
	etm_writel(etr_base, 0x0, TMCDBAHI);
	etm_writel(etr_base, g_etm_info->etr_buf_pointer, TMCRWP);
	etm_writel(etr_base, 0x0, TMCMODE);
	etm_writel(etr_base, 0x2001, TMCFFCR);
	etm_writel(etr_base, 0x1, TMCCTL);
	SOFT_LOCK(etr_base);
}
#endif

#ifdef CONFIG_EXYNOS_CORESIGHT_ETB
static void exynos_etb_enable(void __iomem *etb_base, int src)
{
	int i;
	unsigned int depth = etm_readl(etb_base, TMCRSZ);

	SOFT_UNLOCK(etb_base);
	etm_writel(etb_base, 0x0, TMCCTL);
	etm_writel(etb_base, 0x0, TMCRWP);

	/* clear entire RAM buffer */
	for (i = 0; i < depth; i++)
		etm_writel(etb_base, 0x0, TMCRWD);

	/* reset write RAM pointer address */
	etm_writel(etb_base, 0x0, TMCRWP);
	/* reset read RAM pointer address */
	etm_writel(etb_base, 0x0, TMCRRP);
	etm_writel(etb_base, 0x1, TMCTGR);

	if (src) {
		etm_writel(etb_base, 0x0, TMCFFCR);
		pr_info("Data formatter disabled!\n");
	} else {
		etm_writel(etb_base, 0x2001, TMCFFCR);
		pr_info("Data formatter enabled!\n");
	}

	/* ETB trace capture enable */
	etm_writel(etb_base, 0x1, TMCCTL);
	SOFT_LOCK(etb_base);
}

static void exynos_etb_disable(void __iomem *etb_base, int src)
{
	uint32_t ffcr;

	SOFT_UNLOCK(etb_base);
	if (src) {
		etm_writel(etb_base, 0x2001, TMCFFCR);
		pr_info("Data formatter enabled!\n");
	} else {
		etm_writel(etb_base, 0x0, TMCFFCR);
		pr_info("Data formatter disabled!\n");
	}

	ffcr = etm_readl(etb_base, TMCFFCR);
	ffcr |= BIT(6);
	etm_writel(etb_base, ffcr, TMCFFCR);

	udelay(1500);
	etm_writel(etb_base, 0x0, TMCCTL);

	udelay(1500);
	SOFT_LOCK(etb_base);
}

extern void exynos_etb_etm(void)
{
	exynos_etb_disable(g_etb_base, 0);
	exynos_etb_enable(g_etb_base, 0);
}

extern void exynos_etb_stm(void)
{
	exynos_etb_disable(g_etb_base, 1);
	exynos_etb_enable(g_etb_base, 1);
}
#endif

static int etm_init_default_data(struct exynos_etm_info *etm_info)
{
	/* Main control and Configuration */
	spin_lock_init(&g_etm_info->spinlock);
	etm_info->procsel = 0;
	etm_info->config = 0;
	etm_info->sync_period = 0x8;
	etm_info->victlr = 0x0;
	etm_info->etr_buf_addr = exynos_ss_get_item_paddr("log_etm");
	if(!etm_info->etr_buf_addr)
		return -ENOMEM;
	etm_info->etr_buf_size = exynos_ss_get_item_size("log_etm") / 4;
	if(!etm_info->etr_buf_size)
		return -ENOMEM;
	etm_info->etr_buf_pointer = 0;
	return 0;
}

static void etm_enable(unsigned int cpu)
{
	void __iomem *etm_base = g_etm_info->cpu[cpu].base;
	unsigned int filter;

	SOFT_UNLOCK(etm_base);
	etm_writel(etm_base, 0x1, ETMOSLAR);
	etm_writel(etm_base, 0x0, ETMCTLR);

	/* Main control and Configuration */
	etm_writel(etm_base, g_etm_info->procsel, ETMPROCSELR);
	etm_writel(etm_base, g_etm_info->config, ETMCONFIG);
	etm_writel(etm_base, g_etm_info->sync_period, ETMSYNCPR);

	etm_writel(etm_base, cpu+1, ETMTRACEIDR);

	/* additional register setting */
	etm_writel(etm_base, 0x1000, ETMEVENTCTL0R);
	etm_writel(etm_base, 0x0, ETMEVENTCTL1R);
	etm_writel(etm_base, 0xc, ETMSTALLCTLR);
	etm_writel(etm_base, 0x801, ETMCONFIG);
	etm_writel(etm_base, 0x0, ETMTSCTLR);
	etm_writel(etm_base, 0x4, ETMCCCCTLR);

	etm_writel(etm_base, 0x201, ETMVICTLR);
	etm_writel(etm_base, 0x0, ETMVIIECTLR);
	etm_writel(etm_base, 0x0, ETMVISSCTLR);
	etm_writel(etm_base, 0x2, ETMAUXCTLR);

	etm_writel(etm_base, 0x1, ETMCTLR);
	etm_writel(etm_base, 0x0, ETMOSLAR);
	SOFT_LOCK(etm_base);

	spin_lock(&g_etm_info->spinlock);
	SOFT_UNLOCK(g_funnel_base[0]);
	filter = etm_readl(g_funnel_base[0], FUNCTRL);
	filter |= BIT(g_etm_info->cpu[cpu].funnel_port);
	etm_writel(g_funnel_base[0], filter, FUNCTRL);
	SOFT_LOCK(g_funnel_base[0]);
	spin_unlock(&g_etm_info->spinlock);
}

static void etm_disable(unsigned int cpu)
{
	void __iomem *etm_base = g_etm_info->cpu[cpu].base;
	unsigned int filter;

	spin_lock(&g_etm_info->spinlock);
	SOFT_UNLOCK(g_funnel_base[0]);
	filter = etm_readl(g_funnel_base[0], FUNCTRL);
	filter &= ~(BIT(g_etm_info->cpu[cpu].funnel_port));
	etm_writel(g_funnel_base[0], filter, FUNCTRL);
	SOFT_LOCK(g_funnel_base[0]);
	spin_unlock(&g_etm_info->spinlock);

	SOFT_UNLOCK(etm_base);
	etm_writel(etm_base, 0x0, ETMCTLR);
	etm_writel(etm_base, 0x1, ETMOSLAR);
	SOFT_LOCK(etm_base);
}

extern void exynos_trace_start(void)
{
	exynos_funnel_enable(g_funnel_base[0], g_funnel_base[1]);
#ifdef CONFIG_EXYNOS_CORESIGHT_ETB
	exynos_etb_enable(g_etb_base, 0);
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETF
	exynos_etf_enable(g_etf_base);
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	exynos_etr_enable(g_etr_base);
#endif
}

extern void exynos_trace_stop(void)
{
	SOFT_UNLOCK(g_funnel_base[1]);
	etm_writel(g_funnel_base[1], 0x300, FUNCTRL);
	SOFT_LOCK(g_funnel_base[1]);
#ifdef CONFIG_EXYNOS_CORESIGHT_ETF
	SOFT_UNLOCK(g_etf_base);
	etm_writel(g_etf_base, 0x0, TMCCTL);
	SOFT_LOCK(g_etf_base);
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	SOFT_UNLOCK(g_etr_base);
	etm_writel(g_etr_base, 0x0, TMCCTL);
	g_etm_info->etr_buf_pointer = etm_readl(g_etr_base, TMCRWP);
	SOFT_LOCK(g_etr_base);
#endif
	etm_disable(raw_smp_processor_id());
}

static void exynos_trace_ipi(void *info)
{
	int *hcpu = (int *)info;

	etm_disable(*hcpu);
}

static int __cpuinit core_notify(struct notifier_block *self, unsigned long action, void *data)
{
	int hcpu = (unsigned long)data;

	switch (action) {
		case CPU_DYING:
			smp_call_function_single(hcpu, exynos_trace_ipi, &hcpu, 0);
			break;
	};
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata core_nb = {
	.notifier_call = core_notify,
};

static int exynos_c2_etm_pm_notifier(struct notifier_block *self,
						unsigned long action, void *v)
{
	int cpu = raw_smp_processor_id();

	switch (action) {
	case CPU_PM_ENTER:
		etm_disable(cpu);
		break;
        case CPU_PM_ENTER_FAILED:
        case CPU_PM_EXIT:
		etm_enable(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER:
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata exynos_c2_etm_pm_nb = {
	.notifier_call = exynos_c2_etm_pm_notifier,
};

static int exynos_etm_pm_notifier(struct notifier_block *notifier,
						unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		exynos_trace_stop();
		break;
	case PM_POST_SUSPEND:
		exynos_trace_start();
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_etm_pm_nb = {
	.notifier_call = exynos_etm_pm_notifier,
};

#ifdef CONFIG_OF
static const struct of_device_id etm_dt_match[] = {
	{ .compatible = "exynos,coresight",},
};
#endif

static int exynos_cs_etm_init_dt(void)
{
	struct device_node *np = NULL;
	const unsigned int *cs_reg, *offset, *port;
	unsigned int cs_offset, cs_reg_base;
	int len, i = 0, j = 0;

	g_etm_info = kmalloc(sizeof(struct exynos_etm_info), GFP_KERNEL);
	if (!g_etm_info)
		return -ENOMEM;

	np = of_find_matching_node(NULL, etm_dt_match);
	cs_reg = of_get_property(np, "reg", &len);
	if (!cs_reg)
		return -ESPIPE;

	cs_reg_base = be32_to_cpup(cs_reg);

	while ((np = of_find_node_by_type(np, "etm"))) {
		offset = of_get_property(np, "offset", &len);
		if (!offset)
			return -ESPIPE;

		cs_offset = be32_to_cpup(offset);
		g_etm_info->cpu[i].base = ioremap(cs_reg_base + cs_offset, SZ_4K);
		if (!g_etm_info->cpu[i].base) {
			pr_err("etm ioreamp error\n");
			return -ENOMEM;
		}
		port = of_get_property(np, "funnel_port", &len);
		if (!port)
			return -ESPIPE;
		g_etm_info->cpu[i].funnel_port = be32_to_cpup(port);
		i++;
	}

	while ((np = of_find_node_by_type(np, "funnel"))) {
		offset = of_get_property(np, "offset", &len);
		if (!offset)
			return -ESPIPE;
		cs_offset = be32_to_cpup(offset);
		g_funnel_base[j] = ioremap(cs_reg_base + cs_offset, SZ_4K);
		j++;
	}
#ifdef CONFIG_EXYNOS_CORESIGHT_ETB
	np = of_find_node_by_type(np, "etb");
	offset = of_get_property(np, "offset", &len);
	if (!offset)
		return -ESPIPE;
	cs_offset = be32_to_cpup(offset);
	g_etb_base = ioremap(cs_reg_base + cs_offset, SZ_4K);
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETF
	np = of_find_node_by_type(np, "etf");
	offset = of_get_property(np, "offset", &len);
	if (!offset)
		return -ESPIPE;
	cs_offset = be32_to_cpup(offset);
	g_etf_base = ioremap(cs_reg_base + cs_offset, SZ_4K);
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	np = of_find_node_by_type(np, "etr");
	offset = of_get_property(np, "offset", &len);
	if (!offset)
		return -ESPIPE;
	cs_offset = be32_to_cpup(offset);
	g_etr_base = ioremap(cs_reg_base + cs_offset, SZ_4K);
#endif
	return 0;
}

static void etm_hotplug_out(unsigned int cpu)
{
	etm_disable(cpu);
}

static void etm_hotplug_in(unsigned int cpu)
{
	etm_enable(cpu);
}

static int etm_should_run(unsigned int cpu) { return 0; }

static void etm_thread_fn(unsigned int cpu) { }

static struct smp_hotplug_thread etm_threads = {
	.store				= &etm_task,
	.thread_should_run	= etm_should_run,
	.thread_fn			= etm_thread_fn,
	.thread_comm		= "etm/%u",
	.setup				= etm_hotplug_in,
	.park				= etm_hotplug_out,
	.unpark				= etm_hotplug_in,
};

static int __init exynos_etm_init(void)
{
	int ret;

	ret = exynos_cs_etm_init_dt();
	if (ret < 0)
		goto err;

	ret = etm_init_default_data(g_etm_info);
	if (ret < 0)
		goto err;

	ret = smpboot_register_percpu_thread(&etm_threads);
	if (ret < 0)
		goto err;

	register_pm_notifier(&exynos_etm_pm_nb);
	register_cpu_notifier(&core_nb);
	cpu_pm_register_notifier(&exynos_c2_etm_pm_nb);

	pr_info("coresight ETM enable.\n");
	return 0;
err:
	pr_err("coresight ETM enable FAILED.\n");
	return -ENODEV;
}
early_initcall(exynos_etm_init);

static int __init exynos_tmc_init(void)
{
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	struct clk *etr_clk;
#endif
	exynos_funnel_enable(g_funnel_base[0], g_funnel_base[1]);
#ifdef CONFIG_EXYNOS_CORESIGHT_ETB
	exynos_etb_enable(g_etb_base, 0);
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	etr_clk = clk_get(NULL, "etr_clk");
	clk_prepare_enable(etr_clk);
	exynos_etr_enable(g_etr_base);
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT_ETF
	exynos_etf_enable(g_etf_base);
#endif
	return 0;
}
late_initcall(exynos_tmc_init);

#ifdef CONFIG_EXYNOS_CORESIGHT_ETM_SYSFS
static unsigned int etm_en_status = 1;

static struct bus_type etm_subsys = {
	.name = "exynos-etm",
	.dev_name = "exynos-etm",
};

static ssize_t etm_show_enable(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, 25, "ETM is %sabled !!\n", \
			etm_en_status ? "en" : "dis");
}

static ssize_t etm_store_enable(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;

	if (sscanf(buf, "%ld", &val) != 1)
		return -EINVAL;

	if (val) {
		etm_en_status = 1;
		SOFT_UNLOCK(g_funnel_base[1]);
#ifdef CONFIG_EXYNOS_CORESIGHT_STM
		etm_writel(g_funnel_base[1], 0x381, FUNCTRL);
#else
		etm_writel(g_funnel_base[1], 0x301, FUNCTRL);
#endif
		SOFT_LOCK(g_funnel_base[1]);
	} else {
		etm_en_status = 0;
		SOFT_UNLOCK(g_funnel_base[1]);
#ifdef CONFIG_EXYNOS_CORESIGHT_STM
		etm_writel(g_funnel_base[1], 0x380, FUNCTRL);
#else
		etm_writel(g_funnel_base[1], 0x300, FUNCTRL);
#endif
		SOFT_LOCK(g_funnel_base[1]);
	}
	return count;
}

static struct kobj_attribute etm_enable_attr =
	__ATTR(enable, 0644, etm_show_enable, etm_store_enable);

static struct attribute *etm_sysfs_attrs[] = {
	&etm_enable_attr.attr,
	NULL,
};

static struct attribute_group etm_sysfs_group = {
	.attrs = etm_sysfs_attrs,
};

static const struct attribute_group *etm_sysfs_groups[] = {
	&etm_sysfs_group,
	NULL,
};

static int __init exynos_etm_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&etm_subsys, etm_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-etm subsys\n");

	return ret;
}
late_initcall(exynos_etm_sysfs_init);
#endif
