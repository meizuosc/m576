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

#include "exynos5433_ppmu.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(66000)
#define DEVFREQ_POLLING_PERIOD	(0)

/* extern */
extern struct devfreq_opp_table devfreq_isp_opp_list[];
extern int exynos5_devfreq_isp_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v);

static struct devfreq_simple_ondemand_data exynos5_devfreq_isp_governor_data = {
	.pm_qos_class		= PM_QOS_CAM_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 777000,
};

static struct exynos_devfreq_platdata exynos5433_qos_isp = {
	.default_qos		= 66000,
};

static struct pm_qos_request exynos5_isp_qos;
static struct pm_qos_request boot_isp_qos;
struct pm_qos_request min_isp_thermal_qos;
struct device *isp_dev;

static int exynos5_devfreq_isp_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_isp *data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_isp = data->devfreq;
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
		dev_err(dev, "DEVFREQ(ISP) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_get_idx(devfreq_isp_opp_list, data->max_state,
						*target_freq);
	old_idx = exynos5_devfreq_get_idx(devfreq_isp_opp_list, data->max_state,
						devfreq_isp->previous_freq);
	old_freq = devfreq_isp->previous_freq;

	if (target_idx < 0 ||
		old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	pr_debug("ISP %lu ================> %lu\n", old_freq, *target_freq);

	if (old_freq < *target_freq) {
		if (data->isp_set_volt)
			data->isp_set_volt(data, target_volt, target_volt + VOLT_STEP, false);
		if (data->isp_set_freq)
			data->isp_set_freq(data, target_idx, old_idx);
	} else {
		if (data->isp_set_freq)
			data->isp_set_freq(data, target_idx, old_idx);
		if (data->isp_set_volt)
			data->isp_set_volt(data, target_volt, target_volt + VOLT_STEP, true);
	}
out:
	mutex_unlock(&data->lock);

	return ret;
}

static int exynos5_devfreq_isp_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_isp *data = dev_get_drvdata(dev);

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = 0;
	stat->total_time = 1;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_isp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_isp_target,
	.get_dev_status	= exynos5_devfreq_isp_get_dev_status,
};

static int exynos5_init_isp_table(struct device *dev, struct devfreq_data_isp *data)

{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_isp_opp_list[i].freq;
		volt = get_match_volt(ID_ISP, freq);

		if (!volt)
			volt = devfreq_isp_opp_list[i].volt;

		exynos5_devfreq_isp_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(ISP) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(ISP) : %uKhz, %uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos5_devfreq_isp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	unsigned long freq = exynos5433_qos_isp.default_qos;
	struct devfreq_data_isp *data = dev_get_drvdata(isp_dev);
	struct devfreq *devfreq_isp = data->devfreq;

	devfreq_isp->max_freq = freq;
	mutex_lock(&devfreq_isp->lock);
	update_devfreq(devfreq_isp);
	mutex_unlock(&devfreq_isp->lock);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_isp_reboot_notifier = {
	.notifier_call = exynos5_devfreq_isp_reboot_notifier,
};

static int exynos5_devfreq_isp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_isp *data;
	struct exynos_devfreq_platdata *plat_data;
	struct opp *target_opp;
	unsigned long freq;
	unsigned long volt;

	data = kzalloc(sizeof(struct devfreq_data_isp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5433_devfreq_isp_init(data);

	exynos5_devfreq_isp_profile.max_state = data->max_state;
	exynos5_devfreq_isp_profile.freq_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos5_devfreq_isp_profile.freq_table == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_isp_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);
	data->initial_freq = exynos5_devfreq_isp_profile.initial_freq;

	data->volt_offset = 0;
	isp_dev =
	data->dev = &pdev->dev;
	data->vdd_isp = regulator_get(NULL, "vdd_disp_cam0");

	if (IS_ERR_OR_NULL(data->vdd_isp)) {
		pr_err("DEVFREQ(ISP) : Failed to get regulator\n");
		goto err_inittable;
	}

	freq = DEVFREQ_INITIAL_FREQ;
	rcu_read_lock();
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(ISP) : Invalid OPP to set voltage");
		ret = PTR_ERR(target_opp);
		goto err_opp;
	}
	volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	volt = get_limit_voltage(volt, data->volt_offset);
#endif
	rcu_read_unlock();

	if (data->isp_set_volt)
	data->isp_set_volt(data, volt, volt + VOLT_STEP, false);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_isp_profile,
						"simple_ondemand",
						&exynos5_devfreq_isp_governor_data);
	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_isp_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_isp_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_isp_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_opp:
	regulator_put(data->vdd_isp);
err_inittable:
	devfreq_remove_device(data->devfreq);
	kfree(exynos5_devfreq_isp_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_isp_remove(struct platform_device *pdev)
{
	struct devfreq_data_isp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_isp_thermal_qos);
	pm_qos_remove_request(&exynos5_isp_qos);
	pm_qos_remove_request(&boot_isp_qos);

	regulator_put(data->vdd_isp);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_isp_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_isp_qos))
		pm_qos_update_request(&exynos5_isp_qos, exynos5_devfreq_isp_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_isp_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_isp_qos))
		pm_qos_update_request(&exynos5_isp_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_isp_pm = {
	.suspend	= exynos5_devfreq_isp_suspend,
	.resume		= exynos5_devfreq_isp_resume,
};

static struct platform_driver exynos5_devfreq_isp_driver = {
	.probe	= exynos5_devfreq_isp_probe,
	.remove	= exynos5_devfreq_isp_remove,
	.driver	= {
		.name	= "exynos5-devfreq-isp",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_isp_pm,
	},
};

static struct platform_device exynos5_devfreq_isp_device = {
	.name	= "exynos5-devfreq-isp",
	.id	= -1,
};

static int exynos5_devfreq_isp_qos_init(void)
{
	pm_qos_add_request(&exynos5_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos5433_qos_isp.default_qos);
	pm_qos_add_request(&min_isp_thermal_qos, PM_QOS_CAM_THROUGHPUT, exynos5433_qos_isp.default_qos);
	pm_qos_add_request(&boot_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos5433_qos_isp.default_qos);

	return 0;
}
device_initcall(exynos5_devfreq_isp_qos_init);

static int __init exynos5_devfreq_isp_init(void)
{
	int ret;

	exynos5_devfreq_isp_device.dev.platform_data = &exynos5433_qos_isp;

	ret = platform_device_register(&exynos5_devfreq_isp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_isp_driver);
}
late_initcall(exynos5_devfreq_isp_init);

static void __exit exynos5_devfreq_isp_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_isp_driver);
	platform_device_unregister(&exynos5_devfreq_isp_device);
}
module_exit(exynos5_devfreq_isp_exit);
