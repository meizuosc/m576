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
#include <linux/exynos-ss.h>

#include <mach/tmu.h>
#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>
#include <mach/regs-clock-exynos7420.h>

#include "exynos7420_ppmu.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(400000)
#define DEVFREQ_POLLING_PERIOD	(0)

/* extern */
extern struct devfreq_opp_table devfreq_int_opp_list[];

static int exynos7420_mif_table_for_int[] = {
	632000,		/* INT_LV0 */
	632000,		/* INT_LV1 */
	543000,		/* INT_LV2 */
	543000,		/* INT_LV3 */
	543000,		/* INT_LV4 */
	543000,		/* INT_LV5 */
	543000,		/* INT_LV6 */
	543000,		/* INT_LV7 */
	0,			/* INT_LV8 */
	0,			/* INT_LV9 */
	0,			/* INT_LV10 */
	0,			/* INT_LV11 */
};

static struct devfreq_simple_ondemand_data exynos7_devfreq_int_governor_data = {
	.pm_qos_class		= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold		= 70,
	.downdifferential	= 20,
	.cal_qos_max		= 560000,
};

static struct exynos_devfreq_platdata exynos7420_qos_int = {
	.default_qos		= 100000,
};

static struct ppmu_info ppmu_int[] = {
	{
		.base = (void __iomem *)PPMU0_0_GEN_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU1_0_GEN_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU2_0_GEN_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU3_0_GEN_RT_ADDR,
	},
};

static struct devfreq_exynos devfreq_int_exynos = {
	.type = INT,
	.ppmu_list = ppmu_int,
	.ppmu_count = ARRAY_SIZE(ppmu_int),
};

static struct pm_qos_request exynos7_int_qos;
static struct pm_qos_request exynos7_mif_qos_for_int;
static struct pm_qos_request boot_int_qos;
struct pm_qos_request min_int_thermal_qos;
struct device *int_dev;

static int exynos7_devfreq_int_target(struct device *dev,
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
		//mutex_unlock(&data->lock);
		dev_err(dev, "DEVFREQ(INT) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);

	/* just want to save voltage before apply constraint with isp */
	data->target_volt = target_volt;
	rcu_read_unlock();

	target_idx = devfreq_get_opp_idx(devfreq_int_opp_list, data->max_state,
						*target_freq);
	old_idx = devfreq_get_opp_idx(devfreq_int_opp_list, data->max_state,
						devfreq_int->previous_freq);

	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0 || old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, data->volt_offset, data->volt_of_avail_max_freq);
#endif

	pr_debug("INT LV_%d(%lu) ================> LV_%d(%lu, volt: %lu)\n",
			old_idx, old_freq, target_idx, *target_freq, target_volt);

	exynos_ss_freq(ESS_FLAG_INT, old_freq, ESS_FLAG_IN);
	if (data->old_volt < target_volt) {
		if (pm_qos_request_active(&exynos7_mif_qos_for_int)) {
			if (target_idx < ARRAY_SIZE(exynos7420_mif_table_for_int))
				pm_qos_update_request(&exynos7_mif_qos_for_int, exynos7420_mif_table_for_int[target_idx]);
		}
		if (data->int_set_volt)
			data->int_set_volt(data, target_volt, target_volt + VOLT_STEP);
		if (data->int_set_freq)
			data->int_set_freq(data, target_idx, old_idx);
	} else {
		if (data->int_set_freq)
			data->int_set_freq(data, target_idx, old_idx);
		if (data->int_set_volt)
			data->int_set_volt(data, target_volt, target_volt + VOLT_STEP);
		if (pm_qos_request_active(&exynos7_mif_qos_for_int)) {
			if (target_idx < ARRAY_SIZE(exynos7420_mif_table_for_int))
				pm_qos_update_request(&exynos7_mif_qos_for_int, exynos7420_mif_table_for_int[target_idx]);
		}
	}
	exynos_ss_freq(ESS_FLAG_INT, *target_freq, ESS_FLAG_OUT);
	data->cur_freq = *target_freq;
out:
	mutex_unlock(&data->lock);

	return ret;
}

static int exynos7_devfreq_int_get_dev_status(struct device *dev,
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

static struct devfreq_dev_profile exynos7_devfreq_int_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos7_devfreq_int_target,
	.get_dev_status	= exynos7_devfreq_int_get_dev_status,
#ifdef CONFIG_HYBRID_INVOKING
	.AlwaysDeferred = false,
#endif
};

static int exynos7_init_int_table(struct device *dev,
				struct devfreq_data_int *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	exynos7_devfreq_int_profile.max_state = data->max_state;

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_int_opp_list[i].freq;
		volt = get_match_volt(ID_INT, freq);
		if (!volt)
			volt = devfreq_int_opp_list[i].volt;

		exynos7_devfreq_int_profile.freq_table[i] = freq;
		devfreq_int_opp_list[i].volt = volt;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(INT) : Failed to add opp entries %uKhz, %uuV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(INT) : %7uKhz, %7uuV\n", freq, volt);
		}
	}

	data->volt_of_avail_max_freq = get_volt_of_avail_max_freq(dev);
	pr_info("DEVFREQ(INT) : voltage of available max freq : %7uuV\n",
			data->volt_of_avail_max_freq);

	return 0;
}

static int exynos7_devfreq_int_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;
#ifdef CONFIG_HYBRID_INVOKING
	int * deferred = v;
#endif
	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
#ifdef CONFIG_HYBRID_INVOKING
	if(devfreq_nb->df->profile->AlwaysDeferred ||
		devfreq_nb->df->previous_freq <= devfreq_nb->df->min_freq ||
		devfreq_nb->df->previous_freq <= devfreq_nb->df->locked_min_freq) {
		*deferred = 1;
	} else {
		*deferred = 0;
	}
#endif
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos7_devfreq_int_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	if (pm_qos_request_active(&boot_int_qos))
		pm_qos_update_request(&boot_int_qos, exynos7_devfreq_int_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos7_int_reboot_notifier = {
	.notifier_call = exynos7_devfreq_int_reboot_notifier,
};

static int exynos7_devfreq_int_probe(struct platform_device *pdev)
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

	ret = exynos7420_devfreq_int_init(data);
	if (ret) {
		pr_err("DEVFREQ(INT) : Failed to intialize data\n");
		goto err_freqtable;
	}

	data->initial_freq = exynos7_devfreq_int_profile.initial_freq;
	data->cur_freq = exynos7_devfreq_int_profile.initial_freq;
	exynos7_devfreq_int_profile.freq_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos7_devfreq_int_profile.freq_table == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos7_init_int_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	data->volt_offset = 0;
	data->dev = &pdev->dev;
	int_dev = data->dev;
	data->vdd_int = regulator_get(NULL, "vdd_int");
	if (data->vdd_int)
		data->old_volt = regulator_get_voltage(data->vdd_int);
	data->devfreq = devfreq_add_device(data->dev,
						&exynos7_devfreq_int_profile,
						"simple_ondemand",
						&exynos7_devfreq_int_governor_data);

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}
	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos7_devfreq_int_notifier;

	ret = exynos7420_devfreq_register(&devfreq_int_exynos);
	if (ret) {
		pr_err("DEVFREQ(INT) : Failed to register devfreq_int\n");
		goto err_devfreq_reg;
	}
	exynos7420_ppmu_register_notifier(INT, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;
	data->default_qos = plat_data->default_qos;
	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = devfreq_int_opp_list[0].freq;
	register_reboot_notifier(&exynos7_int_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_devfreq_reg:
	kfree(devfreq_nb);
err_nb:
	mutex_destroy(&data->lock);
	regulator_put(data->vdd_int);
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(exynos7_devfreq_int_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos7_devfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);
	devfreq_remove_device(data->devfreq);
	pm_qos_remove_request(&min_int_thermal_qos);
	pm_qos_remove_request(&exynos7_int_qos);
	pm_qos_remove_request(&exynos7_mif_qos_for_int);
	pm_qos_remove_request(&boot_int_qos);
	regulator_put(data->vdd_int);
	kfree(data);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#define SUSPEND_VOLT_OFFSET	900000

static int exynos7_devfreq_int_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *data = platform_get_drvdata(pdev);
	int int_volt = SUSPEND_VOLT_OFFSET;

	if (pm_qos_request_active(&exynos7_int_qos))
		pm_qos_update_request(&exynos7_int_qos, exynos7_devfreq_int_profile.initial_freq);

	mutex_lock(&data->lock);
	if (data->int_get_volt)
		int_volt = data->int_get_volt(data);

	if (int_volt < SUSPEND_VOLT_OFFSET) {
		if (data->int_set_volt)
			data->int_set_volt(data, SUSPEND_VOLT_OFFSET, SUSPEND_VOLT_OFFSET);
	}
	mutex_unlock(&data->lock);

	return 0;
}

static int exynos7_devfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos7_int_qos))
		pm_qos_update_request(&exynos7_int_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos7_devfreq_int_pm = {
	.suspend	= exynos7_devfreq_int_suspend,
	.resume		= exynos7_devfreq_int_resume,
};

static struct platform_driver exynos7_devfreq_int_driver = {
	.probe	= exynos7_devfreq_int_probe,
	.remove	= exynos7_devfreq_int_remove,
	.driver	= {
		.name	= "exynos7-devfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos7_devfreq_int_pm,
	},
};

static struct platform_device exynos7_devfreq_int_device = {
	.name	= "exynos7-devfreq-int",
	.id	= -1,
};

static int __init exynos7_devfreq_int_qos_init(void)
{
	pm_qos_add_request(&exynos7_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos7420_qos_int.default_qos);
	pm_qos_add_request(&exynos7_mif_qos_for_int, PM_QOS_BUS_THROUGHPUT, exynos7420_qos_int.default_qos);
	pm_qos_add_request(&min_int_thermal_qos, PM_QOS_DEVICE_THROUGHPUT, exynos7420_qos_int.default_qos);
	pm_qos_add_request(&boot_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos7420_qos_int.default_qos);
	pm_qos_update_request_timeout(&exynos7_int_qos,
					exynos7_devfreq_int_profile.initial_freq, PM_BOOT_TIME_LEN * USEC_PER_SEC);

	return 0;
}
device_initcall(exynos7_devfreq_int_qos_init);

static int __init exynos7_devfreq_int_init(void)
{
	int ret = 0;

	exynos7_devfreq_int_device.dev.platform_data = &exynos7420_qos_int;

	ret = platform_device_register(&exynos7_devfreq_int_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos7_devfreq_int_driver);
}
late_initcall(exynos7_devfreq_int_init);

static void __exit exynos7_devfreq_int_exit(void)
{
	platform_driver_unregister(&exynos7_devfreq_int_driver);
	platform_device_unregister(&exynos7_devfreq_int_device);
}
module_exit(exynos7_devfreq_int_exit);
