/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Seungook yang(swy.yang@samsung.com)
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
#include <linux/module.h>

#include <mach/tmu.h>
#include <mach/asv-exynos.h>

#include "devfreq_exynos.h"
#include "governor.h"

static struct devfreq_simple_ondemand_data exynos7_devfreq_isp_governor_data;
static struct exynos_devfreq_platdata exynos7_qos_isp;

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

	pr_debug("ISP LV_%d(%lu) ================> LV_%d(%lu, volt: %lu)\n",
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
out:
	mutex_unlock(&data->lock);

	return ret;
}

static struct devfreq_dev_profile exynos7_devfreq_isp_profile = {
	.target		= exynos7_devfreq_isp_target,
	.get_dev_status	= NULL,
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
			pr_err("DEVFREQ(ISP) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(ISP) : %7uKhz, %7uV\n", freq, volt);
		}
	}

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
	struct device_node *np;
	const struct of_device_id *match;
	devfreq_init_of_fn init_func;
	struct devfreq_data_isp *data;
	struct exynos_devfreq_platdata *plat_data;
	u64 cal_qos_max;
	u64 default_qos;
	u64 initial_freq;

	dev_set_drvdata(&pdev->dev, &exynos7_qos_isp);
	data = kzalloc(sizeof(struct devfreq_data_isp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "pm_qos_class", &exynos7_devfreq_isp_governor_data.pm_qos_class);
	if (ret < 0) {
		pr_err("%s pm_qos_class error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "upthreshold", &exynos7_devfreq_isp_governor_data.upthreshold);
	if (ret < 0) {
		pr_err("%s upthreshold error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "cal_qos_max", &cal_qos_max);
	if (ret < 0) {
		pr_err("%s cal_qos_max error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "default_qos", &default_qos);
	if (ret < 0) {
		pr_err("%s default_qos error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "initial_freq", &initial_freq);
	if (ret < 0) {
		pr_err("%s initial_freq error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "polling_period", &exynos7_devfreq_isp_profile.polling_ms);
	if (ret < 0) {
		pr_err("%s polling_period error %d\n", __func__, ret);
		goto err_freqtable;
	}

	exynos7_devfreq_isp_governor_data.cal_qos_max = (unsigned long)cal_qos_max;
	exynos7_qos_isp.default_qos = (unsigned long)default_qos;
	exynos7_devfreq_isp_profile.initial_freq = (unsigned long)initial_freq;

	/* Initialize QoS for ISP */
	pm_qos_add_request(&exynos7_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos7_qos_isp.default_qos);
	pm_qos_add_request(&min_isp_thermal_qos, PM_QOS_CAM_THROUGHPUT, exynos7_qos_isp.default_qos);
	pm_qos_add_request(&boot_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos7_qos_isp.default_qos);

	/* Find proper init function for isp */
	for_each_matching_node_and_match(np, __devfreq_init_of_table, &match) {
		if (!strcmp(np->name, "bus_isp")) {
			init_func = match->data;
			init_func(data);
			break;
		}
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

	data->volt_offset = 0;
	data->dev = &pdev->dev;

	data->vdd_isp_cam0 = regulator_get(NULL, "vdd_intmif");

	data->devfreq = devfreq_add_device(data->dev,
						&exynos7_devfreq_isp_profile,
						"simple_ondemand",
						&exynos7_devfreq_isp_governor_data);
	plat_data = &exynos7_qos_isp;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos7_devfreq_isp_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos7_isp_reboot_notifier);

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

	regulator_put(data->vdd_isp_cam0);

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
	struct exynos_devfreq_platdata *pdata = dev_get_drvdata(dev);

	if (pm_qos_request_active(&exynos7_isp_qos))
		pm_qos_update_request(&exynos7_isp_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos7_devfreq_isp_pm = {
	.suspend	= exynos7_devfreq_isp_suspend,
	.resume		= exynos7_devfreq_isp_resume,
};

static const struct of_device_id exynos7_devfreq_isp_match[] = {
	{ .compatible = "samsung,exynos7-devfreq-isp"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos7_devfreq_isp_match);

static struct platform_driver exynos7_devfreq_isp_driver = {
	.probe	= exynos7_devfreq_isp_probe,
	.remove	= exynos7_devfreq_isp_remove,
	.driver	= {
		.name	= "exynos7-devfreq-isp",
		.owner	= THIS_MODULE,
		.pm		= &exynos7_devfreq_isp_pm,
		.of_match_table	= of_match_ptr(exynos7_devfreq_isp_match),
	},
};
module_platform_driver(exynos7_devfreq_isp_driver);
