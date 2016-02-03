/* drivers/devfreq/exynos7420_ppmu.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS7420 - PPMU control.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include "exynos7420_ppmu.h"
#include "exynos_ppmu_fw.h"

#define MIF_DEFAULT_PPMU_WEIGHT	100

static struct workqueue_struct *exynos_ppmu_wq;
static struct delayed_work exynos_ppmu_work;
#ifdef CONFIG_HYBRID_INVOKING
static struct delayed_work exynos_ppmu_work_df;
#endif
static unsigned long exynos_ppmu_polling_period;
static unsigned long long mif_weight = MIF_DEFAULT_PPMU_WEIGHT;

static DEFINE_MUTEX(exynos_ppmu_lock);
static LIST_HEAD(exynos_ppmu_list);

static struct srcu_notifier_head exynos_ppmu_notifier_list[DEVFREQ_TYPE_COUNT];

static struct ppmu_info ppmu[PPMU_COUNT] = {
	[PPMU0_0_GEN_RT] = {
		.base = (void __iomem *)PPMU0_0_GEN_RT_ADDR,
	},
	[PPMU0_1_CPU] = {
		.base = (void __iomem *)PPMU0_1_CPU_ADDR,
	},
	[PPMU1_0_GEN_RT] = {
		.base = (void __iomem *)PPMU1_0_GEN_RT_ADDR,
	},
	[PPMU1_1_CPU] = {
		.base = (void __iomem *)PPMU1_1_CPU_ADDR,
	},
	[PPMU2_0_GEN_RT] = {
		.base = (void __iomem *)PPMU2_0_GEN_RT_ADDR,
	},
	[PPMU2_1_CPU] = {
		.base = (void __iomem *)PPMU2_1_CPU_ADDR,
	},
	[PPMU3_0_GEN_RT] = {
		.base = (void __iomem *)PPMU3_0_GEN_RT_ADDR,
	},
	[PPMU3_1_CPU] = {
		.base = (void __iomem *)PPMU3_1_CPU_ADDR,
	},
};

static int exynos7420_ppmu_notifier_list_init(void)
{
	int i;

	for (i = 0; i < DEVFREQ_TYPE_COUNT; ++i)
		srcu_init_notifier_head(&exynos_ppmu_notifier_list[i]);

	return 0;
}

int exynos7420_devfreq_init(struct devfreq_exynos *de)
{
	INIT_LIST_HEAD(&de->node);

	return 0;
}

int exynos7420_devfreq_register(struct devfreq_exynos *de)
{
	int i;

	for (i = 0; i < de->ppmu_count; ++i) {
		if (ppmu_init(&de->ppmu_list[i]))
			return -EINVAL;
	}

	mutex_lock(&exynos_ppmu_lock);
	list_add_tail(&de->node, &exynos_ppmu_list);
	mutex_unlock(&exynos_ppmu_lock);

	return 0;
}

int exynos7420_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&exynos_ppmu_notifier_list[type], nb);
}

int exynos7420_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&exynos_ppmu_notifier_list[type], nb);
}

#ifdef CONFIG_HYBRID_INVOKING
static int exynos7420_ppmu_notify(enum DEVFREQ_TYPE type, int * data)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos_ppmu_notifier_list[type], 0, (void *) data);
}
#else // CONFIG_HYBRID_INVOKING
static int exynos7420_ppmu_notify(enum DEVFREQ_TYPE type)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos_ppmu_notifier_list[type], 0, NULL);
}
#endif // CONFIG_HYBRID_INVOKING

static void exynos7420_update_polling(unsigned int period)
{
	exynos_ppmu_polling_period = period;

	cancel_delayed_work_sync(&exynos_ppmu_work);
#ifdef CONFIG_HYBRID_INVOKING
	cancel_delayed_work_sync(&exynos_ppmu_work_df);
#endif

	if (period == 0)
		return;

	queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work,
			msecs_to_jiffies(period));
}

static int exynos7420_ppmu_mif_calculate(struct ppmu_info *ppmu,
						unsigned int size,
						unsigned long long *ccnt,
						unsigned long long *pmcnt)
{
	unsigned int i;
	unsigned long long val_ccnt = 0;
	unsigned long long val_pmcnt0 = 0;
	unsigned long long val_pmcnt1 = 0;
	unsigned long long val_pmcnt3 = 0;
	unsigned long long ppmu_n = 0;

	*pmcnt = 0;
	for (i = 0; i < size; ++i) {
		if (ppmu_count(ppmu + i, &val_ccnt, &val_pmcnt0, &val_pmcnt1, &val_pmcnt3))
			return -EINVAL;

		if (*ccnt < val_ccnt)
			*ccnt = val_ccnt;

		val_pmcnt3 = div_u64(val_pmcnt3 * mif_weight, 100);

		if (i % 2 == 0) {
			ppmu_n = val_pmcnt3;

			continue;
		} else {
			ppmu_n += val_pmcnt3;

			*pmcnt = max(*pmcnt, ppmu_n);
		}
	}

	return 0;
}

static int exynos7420_ppmu_int_calculatate(struct ppmu_info *ppmu,
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

static void exynos7420_ppmu_update(void)
{
	struct devfreq_exynos *devfreq;
	pfn_ppmu_count pfn_count;

	/* before getting ppmu count, it first should stop ppmu */
	if (ppmu_count_stop(ppmu,
				ARRAY_SIZE(ppmu))) {
		pr_err("DEVFREQ(PPMU) : ppmu can't stop\n");
		return;
	}

	list_for_each_entry(devfreq, &exynos_ppmu_list, node) {
		switch (devfreq->type) {
		case MIF:
			pfn_count = exynos7420_ppmu_mif_calculate;
			break;
		case INT:
			pfn_count = exynos7420_ppmu_int_calculatate;
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
			continue;
		}
	}
}

int exynos7420_ppmu_activate(void)
{
	int i;

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < PPMU_COUNT; ++i) {
		if (ppmu_init(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}

		if (ppmu_init_fw(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}

		if (ppmu_reset(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}
	}
	mutex_unlock(&exynos_ppmu_lock);

	exynos7420_update_polling(100);
	return 0;

err:
	for (; i >= 0; --i)
		ppmu_term(&ppmu[i]);

	return -EINVAL;
}

int exynos7420_ppmu_deactivate(void)
{
	int i;

	exynos7420_update_polling(0);

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < PPMU_COUNT; ++i) {
		if (ppmu_disable(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}
	}

	mutex_unlock(&exynos_ppmu_lock);

	return 0;

err:
	pr_err("DEVFREQ(PPMU) : can't deactivate counter\n");
	return -EINVAL;
}

static int exynos7420_ppmu_reset(void)
{
	if (ppmu_reset_total(ppmu,
			ARRAY_SIZE(ppmu))) {
		pr_err("DEVFREQ(PPMU) : ppmu can't reset data\n");
		return -EAGAIN;
	}

	return 0;
}

static void exynos7420_monitor(struct work_struct *work)
{
	int i;
#ifdef CONFIG_HYBRID_INVOKING
	int deferred= 1;
	int noti_ret = 1;
#endif

	mutex_lock(&exynos_ppmu_lock);

	exynos7420_ppmu_update();

	for (i = 0; i < DEVFREQ_TYPE_COUNT; ++i) {
#ifdef CONFIG_HYBRID_INVOKING
		if(exynos7420_ppmu_notify(i, &noti_ret) != NOTIFY_OK)
			noti_ret = 1;
		deferred = deferred & noti_ret;
		noti_ret = 1;
#else	// CONFIG_HYBRID_INVOKING
			exynos7420_ppmu_notify(i);
#endif	// CONFIG_HYBRID_INVOKING
	}

	exynos7420_ppmu_reset();

	mutex_unlock(&exynos_ppmu_lock);

#ifdef CONFIG_HYBRID_INVOKING
	if(deferred) {
		queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work_df,
				msecs_to_jiffies(exynos_ppmu_polling_period));
	}
	else {
		queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work,
				msecs_to_jiffies(exynos_ppmu_polling_period));
	}
#else
	queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work,
			msecs_to_jiffies(exynos_ppmu_polling_period));
#endif

}

static int exynos7420_ppmu_probe(struct platform_device *pdev)
{
	int ret = 0;
	exynos_ppmu_wq = create_freezable_workqueue("exynos7420_ppmu_wq");

#if defined(CONFIG_DEFERRABLE_INVOKING)
		INIT_DEFERRABLE_WORK(&exynos_ppmu_work, exynos7420_monitor);
#elif defined(CONFIG_HYBRID_INVOKING)
		INIT_DEFERRABLE_WORK(&exynos_ppmu_work_df, exynos7420_monitor);
		INIT_DELAYED_WORK(&exynos_ppmu_work, exynos7420_monitor);
#else
		INIT_DELAYED_WORK(&exynos_ppmu_work, exynos7420_monitor);
#endif

	ret = exynos7420_ppmu_activate();

	return ret;
}

static int exynos7420_ppmu_remove(struct platform_device *pdev)
{
	exynos7420_ppmu_deactivate();
	flush_workqueue(exynos_ppmu_wq);
	destroy_workqueue(exynos_ppmu_wq);

	return 0;
}

static int exynos7420_ppmu_suspend(struct device *dev)
{
	exynos7420_ppmu_deactivate();

	return 0;
}

static int exynos7420_ppmu_resume(struct device *dev)
{
	int i;

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < PPMU_COUNT; ++i) {
		if (ppmu_init_fw(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}
	}
	exynos7420_ppmu_reset();
	mutex_unlock(&exynos_ppmu_lock);

	exynos7420_update_polling(100);

	return 0;
err:
	pr_err("DEVFREQ(PPMU) : ppmu can't resume.\n");

	return -EINVAL;
}

static struct dev_pm_ops exynos7420_ppmu_pm = {
	.suspend	= exynos7420_ppmu_suspend,
	.resume		= exynos7420_ppmu_resume,
};

static struct platform_driver exynos7420_ppmu_driver = {
	.probe	= exynos7420_ppmu_probe,
	.remove	= exynos7420_ppmu_remove,
	.driver	= {
		.name	= "exynos7420-ppmu",
		.owner	= THIS_MODULE,
		.pm	= &exynos7420_ppmu_pm,
	},
};

static struct platform_device exynos7420_ppmu_device = {
	.name	= "exynos7420-ppmu",
	.id	= -1,
};

static int __init exynos7420_ppmu_early_init(void)
{
	return exynos7420_ppmu_notifier_list_init();
}
arch_initcall_sync(exynos7420_ppmu_early_init);

static int __init exynos7420_ppmu_init(void)
{
	int ret;

	ret = platform_device_register(&exynos7420_ppmu_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos7420_ppmu_driver);
}
late_initcall_sync(exynos7420_ppmu_init);

static void __exit exynos7420_ppmu_exit(void)
{
	platform_driver_unregister(&exynos7420_ppmu_driver);
	platform_device_unregister(&exynos7420_ppmu_device);
}
module_exit(exynos7420_ppmu_exit);
