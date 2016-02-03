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
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#ifdef CONFIG_EXYNOS_THERMAL
#include <mach/tmu.h>
#endif
#include <mach/devfreq.h>
#include <mach/asv-exynos.h>
#include <mach/regs-clock-exynos5433.h>
#include <mach/asv-exynos_cal.h>

#include "exynos5433_ppmu.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(825000)
#define DEVFREQ_POLLING_PERIOD	(0)
#define DLL_ON_FREQ		(825000)

extern struct devfreq_opp_table devfreq_mif_opp_list[];

static struct devfreq_simple_exynos_data exynos5_devfreq_mif_governor_data = {
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
	.urgentthreshold	= 80,
	.upthreshold		= 70,
	.downthreshold		= 60,
	.idlethreshold		= 50,
	.cal_qos_max		= 825000,
};

static struct exynos_devfreq_platdata exynos5433_qos_mif = {
	.default_qos		= 109000,
};

static struct ppmu_info ppmu_mif[] = {
	{
		.base = (void __iomem *)PPMU_D0_CPU_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D0_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D0_GEN_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_CPU_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_GEN_ADDR,
	},
};

static struct devfreq_exynos devfreq_mif_exynos = {
	.type = MIF,
	.ppmu_list = ppmu_mif,
	.ppmu_count = ARRAY_SIZE(ppmu_mif),
};

static struct pm_qos_request exynos5_mif_qos;
static struct pm_qos_request boot_mif_qos;
struct pm_qos_request min_mif_thermal_qos;
struct pm_qos_request exynos5_mif_bts_qos;
struct device *mif_dev;

static inline int exynos5_devfreq_mif_get_idx(struct devfreq_opp_table *table,
				unsigned int size,
				unsigned long freq)
{
	int i;

	for (i = 0; i < size; ++i) {
		if (table[i].freq == freq)
			return i;
	}

	return -1;
}

static unsigned long current_freq;

extern struct devfreq_thermal_work devfreq_mif_ch0_work;
extern struct devfreq_thermal_work devfreq_mif_ch1_work;
static int exynos5_devfreq_mif_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *mif_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_mif = mif_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&mif_data->lock);

	*target_freq = min3(*target_freq,
			devfreq_mif_ch0_work.max_freq,
			devfreq_mif_ch1_work.max_freq);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, mif_data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list, mif_data->max_state, *target_freq);
	old_idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list, mif_data->max_state,
						devfreq_mif->previous_freq);
	old_freq = devfreq_mif->previous_freq;

	if (target_idx < 0 ||
		old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq) {
		if (mif_data->mif_set_timeout)
			mif_data->mif_set_timeout(mif_data, target_idx);
		goto out;
	}

	pr_debug("MIF %lu ================> %lu\n", old_freq, *target_freq);

	if (mif_data->mif_dynamic_setting)
		mif_data->mif_dynamic_setting(mif_data, false);

	if (mif_data->mif_pre_process) {
		if (mif_data->mif_pre_process(dev, mif_data, &target_idx, &old_idx, target_freq, &old_freq))
			goto out;
	}

	if (old_freq < *target_freq) {
		if (mif_data->mif_set_volt)
			mif_data->mif_set_volt(mif_data, target_volt, target_volt + VOLT_STEP);
		set_match_abb(ID_MIF, mif_data->mif_asv_abb_table[target_idx]);
		if (mif_data->mif_set_dll)
			mif_data->mif_set_dll(mif_data, target_volt, target_idx);
		if (mif_data->mif_set_and_change_timing_set)
			mif_data->mif_set_and_change_timing_set(mif_data, target_idx);
		if (mif_data->mif_set_freq)
			mif_data->mif_set_freq(mif_data, target_idx, old_idx);
		if (mif_data->mif_set_timeout)
			mif_data->mif_set_timeout(mif_data, target_idx);
	} else {
		if (mif_data->mif_set_timeout)
			mif_data->mif_set_timeout(mif_data, target_idx);
		if (mif_data->mif_set_and_change_timing_set)
			mif_data->mif_set_and_change_timing_set(mif_data, target_idx);
		if (mif_data->mif_set_freq)
			mif_data->mif_set_freq(mif_data, target_idx, old_idx);
		if (mif_data->mif_set_dll)
			mif_data->mif_set_dll(mif_data, target_volt, target_idx);
		set_match_abb(ID_MIF, mif_data->mif_asv_abb_table[target_idx]);
		if (mif_data->mif_set_volt)
			mif_data->mif_set_volt(mif_data, target_volt, target_volt + VOLT_STEP);
	}

	if (mif_data->mif_post_process) {
		if (mif_data->mif_post_process(dev, mif_data, &target_idx, &old_idx, target_freq, &old_freq))
			goto out;
	}

	if (mif_data->mif_dynamic_setting)
		mif_data->mif_dynamic_setting(mif_data, true);

	current_freq = *target_freq;
out:
	mutex_unlock(&mif_data->lock);

	return ret;
}

int is_dll_on(void)
{
	int ret = 0;

	if ((current_freq == 0) || current_freq >= DLL_ON_FREQ)
		ret = 1;

	return ret;
}
EXPORT_SYMBOL_GPL(is_dll_on);

static int exynos5_devfreq_mif_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_mif *data = dev_get_drvdata(dev);
	int idx = -1;
	int above_idx = 0;
	int below_idx = data->max_state - 1;

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;

	idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list, data->max_state,
						stat->current_frequency);

	if (idx < 0)
                return -EAGAIN;

	above_idx = idx - 1;
	below_idx = idx + 1;

	if (above_idx < 0)
		above_idx = 0;

	if (below_idx >= data->max_state)
		below_idx = data->max_state - 1;

	exynos5_devfreq_mif_governor_data.above_freq = devfreq_mif_opp_list[above_idx].freq;
	exynos5_devfreq_mif_governor_data.below_freq = devfreq_mif_opp_list[below_idx].freq;

	stat->busy_time = devfreq_mif_exynos.val_pmcnt;
	stat->total_time = devfreq_mif_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_mif_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_mif_target,
	.get_dev_status	= exynos5_devfreq_mif_get_dev_status,
};

static int exynos5_init_mif_table(struct device *dev,
				struct devfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_mif_opp_list[i].freq;
		volt = get_match_volt(ID_MIF, freq);
		if (!volt)
			volt = devfreq_mif_opp_list[i].volt;

		exynos5_devfreq_mif_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(MIF) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(MIF) : %uKhz, %uV\n", freq, volt);
		}

		data->mif_asv_abb_table[i] = get_match_abb(ID_MIF, freq);

		pr_info("DEVFREQ(MIF) : %uKhz, ABB %u\n", freq, data->mif_asv_abb_table[i]);
	}

	return 0;
}

static int exynos5_devfreq_mif_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos5_devfreq_mif_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	unsigned long freq = exynos5433_qos_mif.default_qos;
	struct devfreq_data_mif *data = dev_get_drvdata(mif_dev);
	struct devfreq *devfreq_mif = data->devfreq;

	devfreq_mif->max_freq = freq;
	mutex_lock(&devfreq_mif->lock);
	update_devfreq(devfreq_mif);
	mutex_unlock(&devfreq_mif->lock);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_mif_reboot_notifier = {
	.notifier_call = exynos5_devfreq_mif_reboot_notifier,
};

#if 0
static int exynos5_devfreq_mif_init_dvfs(struct devfreq_data_mif *data)
{
	switch (exynos5430_get_memory_size()) {
	case 2:
		dmc_timing_parameter = dmc_timing_parameter_2gb;
		break;
	case 3:
		dmc_timing_parameter = dmc_timing_parameter_3gb;
		break;
	default:
		pr_err("DEVFREQ(MIF) : can't get information of memory size!!\n");
		break;
	}

	return 0;
}
#endif

extern int exynos5_devfreq_mif_init_clock(void);
extern int exynos5_devfreq_mif_init_parameter(struct devfreq_data_mif *data);
extern void exynos5_devfreq_set_dll_lock_value(struct devfreq_data_mif *data,
							unsigned long vdd_mif_l0);
extern void exynos5_devfreq_init_thermal(void);
extern struct attribute_group devfreq_mif_attr_group;
extern int exynos5_devfreq_mif_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v);
static int exynos5_devfreq_mif_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_mif *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;
	struct opp *target_opp;
	unsigned long freq;

	data = kzalloc(sizeof(struct devfreq_data_mif), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5433_devfreq_mif_init(data);
	data->default_qos = exynos5433_qos_mif.default_qos;

	exynos5_devfreq_mif_profile.max_state = data->max_state;
	data->initial_freq = exynos5_devfreq_mif_profile.initial_freq;
	data->cal_qos_max = exynos5_devfreq_mif_governor_data.cal_qos_max;
	devfreq_mif_ch0_work.max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	devfreq_mif_ch1_work.max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;

	exynos5_devfreq_mif_profile.freq_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos5_devfreq_mif_profile.freq_table == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	data->use_dvfs = false;
	mutex_init(&data->lock);

	ret = exynos5_init_mif_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);

	data->volt_offset = 0;
	mif_dev =
	data->dev = &pdev->dev;
	data->vdd_mif = regulator_get(NULL, "vdd_mif");

	if (IS_ERR_OR_NULL(data->vdd_mif)) {
		pr_err("DEVFREQ(MIF) : Failed to get regulator\n");
		goto err_inittable;
	}

	rcu_read_lock();
	freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(MIF) : Invalid OPP to set voltagen");
		ret = PTR_ERR(target_opp);
		goto err_opp;
	}
	data->old_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	data->old_volt = get_limit_voltage(data->old_volt, data->volt_offset);
#endif
	rcu_read_unlock();
	regulator_set_voltage(data->vdd_mif, data->old_volt, data->old_volt + VOLT_STEP);
	exynos5_devfreq_set_dll_lock_value(data, data->old_volt);

	data->devfreq = devfreq_add_device(data->dev,
					&exynos5_devfreq_mif_profile,
					"simple_exynos",
					&exynos5_devfreq_mif_governor_data);

	exynos5_devfreq_init_thermal();

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos5_devfreq_mif_notifier;

	exynos5433_devfreq_register(&devfreq_mif_exynos);
	exynos5433_ppmu_register_notifier(MIF, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_mif_reboot_notifier);

	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_mif_attr_group);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_mif_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_opp:
	regulator_put(data->vdd_mif);
err_inittable:
	kfree(exynos5_devfreq_mif_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_mif_remove(struct platform_device *pdev)
{
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	iounmap(data->base_mif);
	iounmap(data->base_sysreg_mif);
	iounmap(data->base_drex0);
	iounmap(data->base_drex1);
	iounmap(data->base_lpddr_phy0);
	iounmap(data->base_lpddr_phy1);


	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_mif_thermal_qos);
	pm_qos_remove_request(&exynos5_mif_qos);
	pm_qos_remove_request(&boot_mif_qos);
	pm_qos_remove_request(&exynos5_mif_bts_qos);

	exynos5433_devfreq_mif_deinit(data);

	regulator_put(data->vdd_mif);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_mif_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, exynos5_devfreq_mif_profile.initial_freq);

	return 0;
}

extern int exynos5_devfreq_mif_update_timingset(struct devfreq_data_mif *data);
static int exynos5_devfreq_mif_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	data->dll_status = ((__raw_readl(data->base_lpddr_phy0 + 0xB0) & (0x1 << 5)) != 0);
	pr_info("DEVFREQ(MIF) : default dll satus : %s\n", (data->dll_status ? "on" : "off"));
#ifdef CONFIG_EXYNOS_THERMAL
	data->old_volt = get_limit_voltage(data->old_volt, data->volt_offset);
#endif
	exynos5_devfreq_set_dll_lock_value(data, data->old_volt);
	exynos5_devfreq_mif_update_timingset(data);

	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_mif_pm = {
	.suspend	= exynos5_devfreq_mif_suspend,
	.resume		= exynos5_devfreq_mif_resume,
};

static struct platform_driver exynos5_devfreq_mif_driver = {
	.probe	= exynos5_devfreq_mif_probe,
	.remove	= exynos5_devfreq_mif_remove,
	.driver	= {
		.name	= "exynos5-devfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_mif_pm,
	},
};

static struct platform_device exynos5_devfreq_mif_device = {
	.name	= "exynos5-devfreq-mif",
	.id	= -1,
};

static int exynos5_devfreq_mif_qos_init(void)
{
	pm_qos_add_request(&exynos5_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos5433_qos_mif.default_qos);
	pm_qos_add_request(&min_mif_thermal_qos, PM_QOS_BUS_THROUGHPUT, exynos5433_qos_mif.default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos5433_qos_mif.default_qos);
	pm_qos_add_request(&exynos5_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, exynos5433_qos_mif.default_qos);

	pm_qos_update_request_timeout(&boot_mif_qos,
					exynos5_devfreq_mif_profile.initial_freq, 40000 * 1000);

	return 0;
}
device_initcall(exynos5_devfreq_mif_qos_init);

static int __init exynos5_devfreq_mif_init(void)
{
	int ret;

	exynos5_devfreq_mif_device.dev.platform_data = &exynos5433_qos_mif;

	ret = platform_device_register(&exynos5_devfreq_mif_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_mif_driver);
}
late_initcall(exynos5_devfreq_mif_init);

static void __exit exynos5_devfreq_mif_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_mif_driver);
	platform_device_unregister(&exynos5_devfreq_mif_device);
}
module_exit(exynos5_devfreq_mif_exit);
