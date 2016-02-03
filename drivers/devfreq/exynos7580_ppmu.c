/* drivers/devfreq/exynos7580_ppmu.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - PPMU control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_address.h>

#include "exynos7580_ppmu.h"
#include "exynos_ppmu_fw.h"

#define PPMU_LIST 3
#define MIF_DEFAULT_PPMU_WEIGHT	100

static struct workqueue_struct *exynos_ppmu_wq;
static struct delayed_work exynos_ppmu_work;
static unsigned long exynos_ppmu_polling_period;
static unsigned long long mif_weight = MIF_DEFAULT_PPMU_WEIGHT;

static DEFINE_MUTEX(exynos_ppmu_lock);
static LIST_HEAD(exynos_ppmu_list);

static struct srcu_notifier_head exynos_ppmu_notifier_list[DEVFREQ_TYPE_COUNT];
static struct devfreq_ppmu ppmu[PPMU_LIST];

static int is_ppmu;

int exynos_ppmu_is_initialized(void)
{
	return is_ppmu;
}

static int exynos_ppmu_notifier_list_init(void)
{
	int i;

	for (i = 0; i < DEVFREQ_TYPE_COUNT; ++i)
		srcu_init_notifier_head(&exynos_ppmu_notifier_list[i]);

	return 0;
}

int exynos_devfreq_register(struct devfreq_exynos *de)
{
	int i;

	if (!de)
		return -EINVAL;

	de->ppmu_count = ppmu[de->type].ppmu_count;
	de->ppmu_list = kzalloc(sizeof(struct ppmu_info) * de->ppmu_count, GFP_KERNEL);

	for (i = 0; i < de->ppmu_count; ++i)
		de->ppmu_list[i].base = ppmu[de->type].ppmu_list[i].base;

	mutex_lock(&exynos_ppmu_lock);
	list_add_tail(&de->node, &exynos_ppmu_list);
	mutex_unlock(&exynos_ppmu_lock);

	return 0;
}

int exynos_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&exynos_ppmu_notifier_list[type], nb);
}

int exynos_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&exynos_ppmu_notifier_list[type], nb);
}

static int exynos_ppmu_notify(enum DEVFREQ_TYPE type)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos_ppmu_notifier_list[type], 0, NULL);
}

static void exynos_update_polling(unsigned int period)
{
	mutex_lock(&exynos_ppmu_lock);
	exynos_ppmu_polling_period = period;

	cancel_delayed_work_sync(&exynos_ppmu_work);

	if (period == 0) {
		mutex_unlock(&exynos_ppmu_lock);
		return;
	}

	queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work,
			msecs_to_jiffies(period));

	mutex_unlock(&exynos_ppmu_lock);
}

static int exynos_ppmu_mif_calculatate(struct ppmu_info *ppmu,
						unsigned int size,
						unsigned long long *ccnt,
						unsigned long long *pmcnt)
{
	unsigned int i;
	unsigned long long val_ccnt = 0;
	unsigned long long val_pmcnt0 = 0;
	unsigned long long val_pmcnt1 = 0;
	unsigned long long val_pmcnt3 = 0;

	for (i = 0; i < size; ++i) {
		if (ppmu_count(ppmu + i, &val_ccnt, &val_pmcnt0, &val_pmcnt1, &val_pmcnt3))
			return -EINVAL;

		if (*ccnt < val_ccnt)
			*ccnt = val_ccnt;

		val_pmcnt3 = div_u64(val_pmcnt3 * mif_weight, 100);

		*pmcnt += val_pmcnt3;
	}

	return 0;
}

static int exynos_ppmu_int_calculatate(struct ppmu_info *ppmu,
						unsigned int size,
						unsigned long long *ccnt,
						unsigned long long *pmcnt)
{
	unsigned int i;
	unsigned long long val_ccnt = 0;
	unsigned long long val_pmcnt0 = 0;
	unsigned long long val_pmcnt1 = 0;
	unsigned long long val_pmcnt3 = 0;

	for (i = 0; i < size; ++i) {
		if (ppmu_count(ppmu + i, &val_ccnt, &val_pmcnt0, &val_pmcnt1, &val_pmcnt3))
			return -EINVAL;

		if (*ccnt < val_ccnt)
			*ccnt = val_ccnt;

		*pmcnt = max3(*pmcnt, val_pmcnt0, val_pmcnt1);
	}

	return 0;
}

static void exynos_ppmu_update(void)
{
	struct devfreq_exynos *devfreq;
	pfn_ppmu_count pfn_count;

	/* before getting ppmu count, it first should stop ppmu */
	if (ppmu_count_stop(ppmu[2].ppmu_list,
				ppmu[2].ppmu_count)) {
		pr_err("DEVFREQ(PPMU) : ppmu can't stop\n");
		return;
	}

	list_for_each_entry(devfreq, &exynos_ppmu_list, node) {
		switch (devfreq->type) {
		case MIF:
			pfn_count = exynos_ppmu_mif_calculatate;
			break;
		case INT:
			pfn_count = exynos_ppmu_int_calculatate;
			break;
		default:
			pfn_count = NULL;
			break;
		}

		if (ppmu_count_total(devfreq->ppmu_list,
					devfreq->ppmu_count,
					pfn_count,
					&devfreq->val_ccnt,
					&devfreq->val_pmcnt)) {
			pr_err("DEVFREQ(PPMU) : ppmu can't update data\n");
		}
	}
}

int exynos_ppmu_activate(void)
{
	int i;

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < ppmu[2].ppmu_count; ++i) {
		if (ppmu_init_fw(&ppmu[2].ppmu_list[i]))
			goto err;

		if (ppmu_reset(&ppmu[2].ppmu_list[i]))
			goto err;
	}
	mutex_unlock(&exynos_ppmu_lock);

	exynos_update_polling(100);
	return 0;

err:
	mutex_unlock(&exynos_ppmu_lock);
	for (; i >= 0; --i)
		ppmu_term(&ppmu[2].ppmu_list[i]);

	return -EINVAL;
}

int exynos_ppmu_deactivate(void)
{
	int i;

	exynos_update_polling(0);

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < ppmu[2].ppmu_count; ++i) {
		if (ppmu_disable(&ppmu[2].ppmu_list[i]))
			goto err;
	}

	mutex_unlock(&exynos_ppmu_lock);

	return 0;

err:
	mutex_unlock(&exynos_ppmu_lock);
	pr_err("DEVFREQ(PPMU) : can't deactivate counter\n");
	return -EINVAL;
}

static int exynos_ppmu_reset(void)
{
	if (ppmu_reset_total(ppmu[2].ppmu_list,
				ppmu[2].ppmu_count)) {
		pr_err("DEVFREQ(PPMU) : ppmu can't reset data\n");
		return -EAGAIN;
	}

	return 0;
}

static void exynos_monitor(struct work_struct *work)
{
	int i;

	mutex_lock(&exynos_ppmu_lock);

	exynos_ppmu_update();

	for (i = 0; i < DEVFREQ_TYPE_COUNT; ++i)
		exynos_ppmu_notify(i);

	exynos_ppmu_reset();

	queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work,
			msecs_to_jiffies(exynos_ppmu_polling_period));

	mutex_unlock(&exynos_ppmu_lock);
}

static int exynos_ppmu_probe(struct platform_device *pdev)
{
	int ret;
	int i, length;
	struct device_node *np = of_find_node_by_name(NULL, "ppmu");
	struct device_node *child;

	of_device_is_compatible(np, "samsung,exynos-ppmu");

	child = of_get_child_by_name(np, "ppmu-int");
	of_get_property(child, "reg", &length);
	ppmu[0].ppmu_count = (length / sizeof(unsigned long) / 2);
	ppmu[0].ppmu_list = kzalloc(sizeof(struct ppmu_info) * ppmu[0].ppmu_count, GFP_KERNEL);
	if (!ppmu[0].ppmu_list) {
		pr_err(KERN_ERR "Unable to kmalloc for ppmu!\n");
		return -ENOMEM;
	}

	for (i = 0; i < ppmu[0].ppmu_count; ++i) {
		ppmu[0].ppmu_list[i].base = of_iomap(child, i);
		if (!ppmu[0].ppmu_list[i].base)
			panic("unable to map ppmu registers\n");
	}

	child = of_get_child_by_name(np, "ppmu-mif");
	of_get_property(child, "reg", &length);
	ppmu[1].ppmu_count = (length / sizeof(unsigned long) / 2);
	ppmu[1].ppmu_list = kzalloc(sizeof(struct ppmu_info) * ppmu[1].ppmu_count, GFP_KERNEL);
	if (!ppmu[1].ppmu_list) {
		pr_err(KERN_ERR "Unable to kmalloc for ppmu!\n");
		return -ENOMEM;
	}

	for (i = 0; i < ppmu[1].ppmu_count; ++i) {
		ppmu[1].ppmu_list[i].base = of_iomap(child, i);
		if (!ppmu[1].ppmu_list[i].base)
			panic("unable to map ppmu registers\n");
	}

	child = of_get_child_by_name(np, "ppmu-total");
	of_get_property(child, "reg", &length);
	ppmu[2].ppmu_count = (length / sizeof(unsigned long) / 2);
	ppmu[2].ppmu_list = kzalloc(sizeof(struct ppmu_info) * ppmu[2].ppmu_count, GFP_KERNEL);
	if (!ppmu[2].ppmu_list) {
		pr_err(KERN_ERR "Unable to kmalloc for ppmu!\n");
		return -ENOMEM;
	}

	for (i = 0; i < ppmu[2].ppmu_count; ++i) {
		ppmu[2].ppmu_list[i].base = of_iomap(child, i);
		if (!ppmu[2].ppmu_list[i].base)
			panic("unable to map ppmu registers\n");
	}

	exynos_ppmu_wq = create_freezable_workqueue("exynos_ppmu_wq");
	if (IS_ERR(exynos_ppmu_wq)) {
		pr_err("%s: couldn't create workqueue\n", __FILE__);
		return PTR_ERR(exynos_ppmu_wq);
	}

	INIT_DELAYED_WORK(&exynos_ppmu_work, exynos_monitor);
	ret = exynos_ppmu_activate();
	if (ret)
		return ret;

	/* To check whether ppmu is probed or not */
	is_ppmu = 1;

	return 0;
}

static int exynos_ppmu_remove(struct platform_device *pdev)
{
	int ret;

	ret = exynos_ppmu_deactivate();
	if (ret)
		return ret;

	flush_workqueue(exynos_ppmu_wq);
	destroy_workqueue(exynos_ppmu_wq);

	return 0;
}

static int exynos_ppmu_suspend(struct device *dev)
{
	int ret;

	ret = exynos_ppmu_deactivate();
	if (!ret)
		return ret;

	return 0;
}

static int exynos_ppmu_resume(struct device *dev)
{
	int i;

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < ppmu[2].ppmu_count; ++i) {
		if (ppmu_init_fw(&ppmu[2].ppmu_list[i]))
			goto err;
	}
	mutex_unlock(&exynos_ppmu_lock);

	exynos_ppmu_reset();
	exynos_update_polling(100);

	return 0;
err:
	mutex_unlock(&exynos_ppmu_lock);
	pr_err("DEVFREQ(PPMU) : ppmu can't resume.\n");

	return -EINVAL;
}

static int __init exynos_ppmu_early_init(void)
{
	return exynos_ppmu_notifier_list_init();
}
arch_initcall_sync(exynos_ppmu_early_init);

static struct dev_pm_ops exynos_ppmu_pm = {
	.suspend	= exynos_ppmu_suspend,
	.resume		= exynos_ppmu_resume,
};

static const struct of_device_id exynos_ppmu_match[] = {
	{ .compatible = "samsung,exynos7580-ppmu"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ppmu_match);

static struct platform_driver exynos_ppmu_driver = {
	.probe	= exynos_ppmu_probe,
	.remove	= exynos_ppmu_remove,
	.driver	= {
		.name	= "exynos-ppmu",
		.owner	= THIS_MODULE,
		.pm	= &exynos_ppmu_pm,
		.of_match_table = of_match_ptr(exynos_ppmu_match),
	},
};
module_platform_driver(exynos_ppmu_driver);
