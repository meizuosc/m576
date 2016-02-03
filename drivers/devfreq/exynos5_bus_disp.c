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

#include <mach/asv-exynos.h>

#include "exynos5433_ppmu.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(334000)
#define DEVFREQ_POLLING_PERIOD	(100)

/* extern */
extern struct devfreq_opp_table devfreq_disp_opp_list[];

static struct devfreq_simple_ondemand_data exynos5_devfreq_disp_governor_data = {
	.pm_qos_class		= PM_QOS_DISPLAY_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 334000,
};

static struct exynos_devfreq_platdata exynos5433_qos_disp = {
	.default_qos		= 134000,
};

static struct pm_qos_request exynos5_disp_qos;
static struct pm_qos_request boot_disp_qos;
static struct pm_qos_request min_disp_thermal_qos;
static struct pm_qos_request exynos5_disp_bts_qos;
struct device *disp_dev;

extern void exynos5_update_district_int_level(int aclk_disp_333_freq);

void exynos5_update_district_disp_level(unsigned int idx)
{
	if (pm_qos_request_active(&exynos5_disp_bts_qos))
		pm_qos_update_request(&exynos5_disp_bts_qos, devfreq_disp_opp_list[idx].freq);
}

static int exynos5_devfreq_disp_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_disp *data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_disp = data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long old_freq;

	mutex_lock(&data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		mutex_unlock(&data->lock);
		dev_err(dev, "DEVFREQ(DISP) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}

	*target_freq = opp_get_freq(target_opp);
	rcu_read_unlock();

	target_idx = exynos5_devfreq_get_idx(devfreq_disp_opp_list, data->max_state,
						*target_freq);
	old_idx = exynos5_devfreq_get_idx(devfreq_disp_opp_list, data->max_state,
						devfreq_disp->previous_freq);
	old_freq = devfreq_disp->previous_freq;

	if (target_idx < 0 || old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	//pr_debug("DISP %lu ================> %lu\n", old_freq, *target_freq);

	if (old_freq < *target_freq) {
		exynos5_update_district_int_level(target_idx);
		if (data->disp_set_freq)
			data->disp_set_freq(data, target_idx, old_idx);
	} else {
		if (data->disp_set_freq)
			data->disp_set_freq(data, target_idx, old_idx);
		exynos5_update_district_int_level(target_idx);
	}
out:
	mutex_unlock(&data->lock);

	return ret;
}

static int exynos5_devfreq_disp_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_disp *data = dev_get_drvdata(dev);

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = 0;
	stat->total_time = 1;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_disp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_disp_target,
	.get_dev_status	= exynos5_devfreq_disp_get_dev_status,
};

static int exynos5_init_disp_table(struct device *dev, struct devfreq_data_disp *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_disp_opp_list[i].freq;

		exynos5_devfreq_disp_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, 0);
		if (ret) {
			pr_err("DEVFREQ(DISP) : Failed to add opp entries %uKhz\n", freq);
			return ret;
		} else {
			pr_info("DEVFREQ(DISP) : %uKhz\n", freq);
		}
	}

	return 0;
}

static int exynos5_devfreq_disp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	unsigned long freq = exynos5433_qos_disp.default_qos;
	struct devfreq_data_disp *data = dev_get_drvdata(disp_dev);
	struct devfreq *devfreq_disp = data->devfreq;

	devfreq_disp->max_freq = freq;
	mutex_lock(&devfreq_disp->lock);
	update_devfreq(devfreq_disp);
	mutex_unlock(&devfreq_disp->lock);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_disp_reboot_notifier = {
	.notifier_call = exynos5_devfreq_disp_reboot_notifier,
};

static int exynos5_devfreq_disp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_disp *data;
	struct exynos_devfreq_platdata *plat_data;
	int target_idx;

	data = kzalloc(sizeof(struct devfreq_data_disp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5433_devfreq_disp_init(data);
	exynos5_devfreq_disp_profile.max_state = data->max_state;
	exynos5_devfreq_disp_profile.freq_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos5_devfreq_disp_profile.freq_table == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_disp_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	disp_dev =
	data->dev = &pdev->dev;
	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_disp_profile,
						"simple_ondemand",
						&exynos5_devfreq_disp_governor_data);
	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_disp_governor_data.cal_qos_max;
	register_reboot_notifier(&exynos5_disp_reboot_notifier);
	data->use_dvfs = true;

	target_idx = exynos5_devfreq_get_idx(devfreq_disp_opp_list, exynos5_devfreq_disp_profile.max_state,
			exynos5_devfreq_disp_profile.initial_freq);
	exynos5_update_district_int_level(target_idx);

	return ret;
err_inittable:
	devfreq_remove_device(data->devfreq);
	kfree(exynos5_devfreq_disp_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_disp_remove(struct platform_device *pdev)
{
	struct devfreq_data_disp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_disp_thermal_qos);
	pm_qos_remove_request(&exynos5_disp_qos);
	pm_qos_remove_request(&boot_disp_qos);
	pm_qos_remove_request(&exynos5_disp_bts_qos);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_disp_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, exynos5_devfreq_disp_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_disp_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_disp_pm = {
	.suspend	= exynos5_devfreq_disp_suspend,
	.resume		= exynos5_devfreq_disp_resume,
};

static struct platform_driver exynos5_devfreq_disp_driver = {
	.probe	= exynos5_devfreq_disp_probe,
	.remove	= exynos5_devfreq_disp_remove,
	.driver	= {
		.name	= "exynos5-devfreq-disp",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_disp_pm,
	},
};

static struct platform_device exynos5_devfreq_disp_device = {
	.name	= "exynos5-devfreq-disp",
	.id	= -1,
};

static int __init exynos5_devfreq_disp_qos_init(void)
{
	pm_qos_add_request(&exynos5_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5433_qos_disp.default_qos);
	pm_qos_add_request(&min_disp_thermal_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5433_qos_disp.default_qos);
	pm_qos_add_request(&boot_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5433_qos_disp.default_qos);
	pm_qos_add_request(&exynos5_disp_bts_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5433_qos_disp.default_qos);
	pm_qos_update_request_timeout(&exynos5_disp_qos,
					exynos5_devfreq_disp_profile.initial_freq, 40000 * 1000);
	return 0;
}
device_initcall(exynos5_devfreq_disp_qos_init);

static int __init exynos5_devfreq_disp_init(void)
{
	int ret;

	exynos5_devfreq_disp_device.dev.platform_data = &exynos5433_qos_disp;

	ret = platform_device_register(&exynos5_devfreq_disp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_disp_driver);
}
late_initcall(exynos5_devfreq_disp_init);

static void __exit exynos5_devfreq_disp_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_disp_driver);
	platform_device_unregister(&exynos5_devfreq_disp_device);
}
module_exit(exynos5_devfreq_disp_exit);
