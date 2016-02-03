/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Taikyung Yu(taikyung.yu@samsung.com)
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
#include <linux/exynos-ss.h>

#include <mach/tmu.h>
#include <mach/asv-exynos.h>

#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(500000)
#define DEVFREQ_POLLING_PERIOD	(0)

/* extern */
extern struct devfreq_opp_table devfreq_isp_opp_list[];

static struct devfreq_simple_ondemand_data exynos7_devfreq_isp_governor_data = {
	.pm_qos_class		= PM_QOS_CAM_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 540000,
};

static struct exynos_devfreq_platdata exynos7420_qos_isp = {
	.default_qos		= 500000,
};

static struct pm_qos_request exynos7_isp_qos;
static struct pm_qos_request boot_isp_qos;
struct pm_qos_request min_isp_thermal_qos;

static int exynos7_devfreq_isp_target(struct device *dev,
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
	rcu_read_unlock();

	target_idx = devfreq_get_opp_idx(devfreq_isp_opp_list, data->max_state,
						*target_freq);
	old_idx = devfreq_get_opp_idx(devfreq_isp_opp_list, data->max_state,
						devfreq_isp->previous_freq);
	old_freq = devfreq_isp->previous_freq;

	if (target_idx < 0 || old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, data->volt_offset, data->volt_of_avail_max_freq);
#endif

	pr_info("ISP LV_%d(%lu) ================> LV_%d(%lu, volt: %lu)\n",
			old_idx, old_freq, target_idx, *target_freq, target_volt);

	exynos_ss_freq(ESS_FLAG_ISP, old_freq, ESS_FLAG_IN);
	if (old_freq < *target_freq) {
		if (data->isp_set_volt)
			data->isp_set_volt(data, target_volt, REGULATOR_MAX_MICROVOLT);
		if (data->isp_set_freq)
			data->isp_set_freq(data, target_idx, old_idx);
	} else {
		if (data->isp_set_freq)
			data->isp_set_freq(data, target_idx, old_idx);
		if (data->isp_set_volt)
			data->isp_set_volt(data, target_volt, REGULATOR_MAX_MICROVOLT);
	}
	exynos_ss_freq(ESS_FLAG_ISP, *target_freq, ESS_FLAG_OUT);
	data->cur_freq = *target_freq;
out:
	mutex_unlock(&data->lock);

	return ret;
}

static struct devfreq_dev_profile exynos7_devfreq_isp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos7_devfreq_isp_target,
	.get_dev_status	= NULL,
#ifdef CONFIG_HYBRID_INVOKING
	.AlwaysDeferred = true,
#endif
};

static int exynos7_init_isp_table(struct device *dev, struct devfreq_data_isp *data)

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

		devfreq_isp_opp_list[i].volt = volt;
		exynos7_devfreq_isp_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(ISP) : Failed to add opp entries %uKhz, %uuV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(ISP) : %7uKhz, %7uuV\n", freq, volt);
		}
	}

	data->volt_of_avail_max_freq = get_volt_of_avail_max_freq(dev);
	pr_info("DEVFREQ(ISP) : voltage of available max freq : %7uuV\n",
			data->volt_of_avail_max_freq);
	return 0;
}

static int exynos7_devfreq_isp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	if (pm_qos_request_active(&boot_isp_qos))
		pm_qos_update_request(&boot_isp_qos, exynos7_devfreq_isp_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos7_isp_reboot_notifier = {
	.notifier_call = exynos7_devfreq_isp_reboot_notifier,
};

static int exynos7_devfreq_isp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_isp *data;
	struct exynos_devfreq_platdata *plat_data;

	data = kzalloc(sizeof(struct devfreq_data_isp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	ret = exynos7420_devfreq_isp_init(data);
	if (ret) {
		pr_err("DEVFREQ(ISP) : Failed to intialize data\n");
		goto err_freqtable;
	}

	exynos7_devfreq_isp_profile.max_state = data->max_state;
	exynos7_devfreq_isp_profile.freq_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos7_devfreq_isp_profile.freq_table == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos7_init_isp_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);
	data->initial_freq = exynos7_devfreq_isp_profile.initial_freq;
	data->cur_freq = exynos7_devfreq_isp_profile.initial_freq;

	data->volt_offset = 0;
	data->dev = &pdev->dev;
	data->vdd_disp_cam0 = regulator_get(NULL, "vdd_disp_cam0");
	if (data->vdd_disp_cam0)
		data->old_volt = regulator_get_voltage(data->vdd_disp_cam0);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos7_devfreq_isp_profile,
						"simple_ondemand",
						&exynos7_devfreq_isp_governor_data);
	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos7_devfreq_isp_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos7_isp_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_inittable:
	devfreq_remove_device(data->devfreq);
	kfree(exynos7_devfreq_isp_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos7_devfreq_isp_remove(struct platform_device *pdev)
{
	struct devfreq_data_isp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_isp_thermal_qos);
	pm_qos_remove_request(&exynos7_isp_qos);
	pm_qos_remove_request(&boot_isp_qos);

	regulator_put(data->vdd_disp_cam0);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos7_devfreq_isp_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos7_isp_qos))
		pm_qos_update_request(&exynos7_isp_qos, exynos7_devfreq_isp_profile.initial_freq);

	return 0;
}

static int exynos7_devfreq_isp_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos7_isp_qos))
		pm_qos_update_request(&exynos7_isp_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos7_devfreq_isp_pm = {
	.suspend	= exynos7_devfreq_isp_suspend,
	.resume		= exynos7_devfreq_isp_resume,
};

static struct platform_driver exynos7_devfreq_isp_driver = {
	.probe	= exynos7_devfreq_isp_probe,
	.remove	= exynos7_devfreq_isp_remove,
	.driver	= {
		.name	= "exynos7-devfreq-isp",
		.owner	= THIS_MODULE,
		.pm	= &exynos7_devfreq_isp_pm,
	},
};

static struct platform_device exynos7_devfreq_isp_device = {
	.name	= "exynos7-devfreq-isp",
	.id	= -1,
};

static int exynos7_devfreq_isp_qos_init(void)
{
	pm_qos_add_request(&exynos7_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos7420_qos_isp.default_qos);
	pm_qos_add_request(&min_isp_thermal_qos, PM_QOS_CAM_THROUGHPUT, exynos7420_qos_isp.default_qos);
	pm_qos_add_request(&boot_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos7420_qos_isp.default_qos);

	return 0;
}
device_initcall(exynos7_devfreq_isp_qos_init);

static int __init exynos7_devfreq_isp_init(void)
{
	int ret = 0;

	exynos7_devfreq_isp_device.dev.platform_data = &exynos7420_qos_isp;

	ret = platform_device_register(&exynos7_devfreq_isp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos7_devfreq_isp_driver);
}
late_initcall(exynos7_devfreq_isp_init);

static void __exit exynos7_devfreq_isp_exit(void)
{
	platform_driver_unregister(&exynos7_devfreq_isp_driver);
	platform_device_unregister(&exynos7_devfreq_isp_device);
}
module_exit(exynos7_devfreq_isp_exit);
