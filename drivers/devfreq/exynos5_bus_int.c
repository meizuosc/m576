/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#ifdef CONFIG_EXYNOS_THERMAL
#include <mach/tmu.h>
#endif
#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>
#include <mach/regs-clock-exynos5433.h>

#include "exynos5433_ppmu.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(400000)
#define DEVFREQ_POLLING_PERIOD	(0)

/* extern */
extern struct devfreq_opp_table devfreq_int_opp_list[];

extern int exynos5_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v);

#ifdef CONFIG_EXYNOS_THERMAL
extern unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset);
#endif

static struct devfreq_simple_ondemand_data exynos5_devfreq_int_governor_data = {
	.pm_qos_class		= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold		= 70,
	.downdifferential	= 20,
	.cal_qos_max		= 400000,
};

static struct exynos_devfreq_platdata exynos5433_qos_int = {
	.default_qos		= 100000,
};

static struct ppmu_info ppmu_int[] = {
	{
		.base = (void __iomem *)PPMU_D0_GEN_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_GEN_ADDR,
	},
};

static struct devfreq_exynos devfreq_int_exynos = {
	.type = INT,
	.ppmu_list = ppmu_int,
	.ppmu_count = ARRAY_SIZE(ppmu_int),
};

static struct pm_qos_request exynos5_int_qos;
static struct pm_qos_request boot_int_qos;
struct pm_qos_request min_int_thermal_qos;
struct pm_qos_request exynos5_int_bts_qos;
struct device *int_dev;

static int exynos5_devfreq_int_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_int = data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		mutex_unlock(&data->lock);
		dev_err(dev, "DEVFREQ(INT) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
	rcu_read_unlock();
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, data->volt_offset);
#endif
	/* just want to save voltage before apply constraint with isp */
	data->target_volt = target_volt;

	target_idx = exynos5_devfreq_get_idx(devfreq_int_opp_list, data->max_state,
						*target_freq);
	old_idx = exynos5_devfreq_get_idx(devfreq_int_opp_list, data->max_state,
						devfreq_int->previous_freq);

	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0 || old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	pr_debug("INT %lu ===================> %lu\n", old_freq, *target_freq);

	if (old_freq < *target_freq) {
		if (data->int_set_volt)
			data->int_set_volt(data, target_volt, target_volt + VOLT_STEP);
		set_match_abb(ID_INT, data->int_asv_abb_table[target_idx]);
		if (data->int_set_freq)
			data->int_set_freq(data, target_idx, old_idx);
	} else {
		if (data->int_set_freq)
			data->int_set_freq(data, target_idx, old_idx);
		set_match_abb(ID_INT, data->int_asv_abb_table[target_idx]);
		if (data->int_set_volt)
			data->int_set_volt(data, target_volt, target_volt + VOLT_STEP);
	}
out:
	mutex_unlock(&data->lock);

	return ret;
}

static int exynos5_devfreq_int_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_int *data = dev_get_drvdata(dev);

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_int_exynos.val_pmcnt;
	stat->total_time = devfreq_int_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_int_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_int_target,
	.get_dev_status	= exynos5_devfreq_int_get_dev_status,
};

static int exynos5_init_int_table(struct device *dev,
				struct devfreq_data_int *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	exynos5_devfreq_int_profile.max_state = data->max_state;
	data->int_asv_abb_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_int_opp_list[i].freq;
		volt = get_match_volt(ID_INT, freq);
		if (!volt)
			volt = devfreq_int_opp_list[i].volt;

		exynos5_devfreq_int_profile.freq_table[i] = freq;
		devfreq_int_opp_list[i].volt = volt;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(INT) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(INT) : %uKhz, %uV\n", freq, volt);
		}

		data->int_asv_abb_table[i] = get_match_abb(ID_INT, freq);

		pr_info("DEVFREQ(INT) : %uKhz, ABB %u\n", freq, data->int_asv_abb_table[i]);
	}

	return 0;
}

static int exynos5_devfreq_int_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos5_devfreq_int_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	unsigned long freq = exynos5433_qos_int.default_qos;
	struct devfreq_data_int *data = dev_get_drvdata(int_dev);
	struct devfreq *devfreq_int = data->devfreq;

	devfreq_int->max_freq = freq;
	mutex_lock(&devfreq_int->lock);
	update_devfreq(devfreq_int);
	mutex_unlock(&devfreq_int->lock);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_int_reboot_notifier = {
	.notifier_call = exynos5_devfreq_int_reboot_notifier,
};

static int exynos5_devfreq_int_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_int *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;

	data = kzalloc(sizeof(struct devfreq_data_int), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5433_devfreq_int_init(data);

	data->initial_freq = exynos5_devfreq_int_profile.initial_freq;

	exynos5_devfreq_int_profile.freq_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos5_devfreq_int_profile.freq_table == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_int_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	data->volt_constraint_isp = 0;
	data->volt_offset = 0;
	int_dev =
	data->dev = &pdev->dev;
	data->vdd_int = regulator_get(NULL, "vdd_int");

	if (IS_ERR_OR_NULL(data->vdd_int)) {
		pr_err("DEVFREQ(INT) : Failed to get regulator\n");
		goto err_inittable;
	}

	data->vdd_int_m = regulator_get(NULL, "vdd_int_m");
	if (IS_ERR(data->vdd_int)) {
		dev_err(data->dev, "DEVFREQ(INT) : failed to get regulator\n");
		goto err_regulator_m;
	}

	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_int_profile,
						"simple_ondemand",
						&exynos5_devfreq_int_governor_data);

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos5_devfreq_int_notifier;

	exynos5433_devfreq_register(&devfreq_int_exynos);
	exynos5433_ppmu_register_notifier(INT, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->default_qos = plat_data->default_qos;
	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = devfreq_int_opp_list[0].freq;

	register_reboot_notifier(&exynos5_int_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_int_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
	regulator_put(data->vdd_int_m);
err_regulator_m:
	regulator_put(data->vdd_int);
err_inittable:
	kfree(exynos5_devfreq_int_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_int_thermal_qos);
	pm_qos_remove_request(&exynos5_int_qos);
	pm_qos_remove_request(&boot_int_qos);
	pm_qos_remove_request(&exynos5_int_bts_qos);

	regulator_put(data->vdd_int);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_int_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, exynos5_devfreq_int_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_int_pm = {
	.suspend	= exynos5_devfreq_int_suspend,
	.resume		= exynos5_devfreq_int_resume,
};

static struct platform_driver exynos5_devfreq_int_driver = {
	.probe	= exynos5_devfreq_int_probe,
	.remove	= exynos5_devfreq_int_remove,
	.driver	= {
		.name	= "exynos5-devfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_int_pm,
	},
};

static struct platform_device exynos5_devfreq_int_device = {
	.name	= "exynos5-devfreq-int",
	.id	= -1,
};

static int __init exynos5_devfreq_int_qos_init(void)
{
	pm_qos_add_request(&exynos5_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5433_qos_int.default_qos);
	pm_qos_add_request(&min_int_thermal_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5433_qos_int.default_qos);
	pm_qos_add_request(&boot_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5433_qos_int.default_qos);
	pm_qos_add_request(&exynos5_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5433_qos_int.default_qos);
	pm_qos_update_request_timeout(&exynos5_int_qos,
					exynos5_devfreq_int_profile.initial_freq, 40000 * 1000);

	return 0;
}
device_initcall(exynos5_devfreq_int_qos_init);

static int __init exynos5_devfreq_int_init(void)
{
	int ret;

	exynos5_devfreq_int_device.dev.platform_data = &exynos5433_qos_int;

	ret = platform_device_register(&exynos5_devfreq_int_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_int_driver);
}
late_initcall(exynos5_devfreq_int_init);

static void __exit exynos5_devfreq_int_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_int_driver);
	platform_device_unregister(&exynos5_devfreq_int_device);
}
module_exit(exynos5_devfreq_int_exit);
