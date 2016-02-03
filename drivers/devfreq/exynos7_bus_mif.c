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
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/exynos-ss.h>
#include <mach/tmu.h>
#include <mach/devfreq.h>
#include <mach/asv-exynos.h>
#include <mach/apm-exynos.h>
#include <mach/regs-clock-exynos7420.h>
#include <plat/cpu.h>

#include "exynos7420_ppmu.h"
#include "exynos_ppmu_fw.h"
#include "devfreq_exynos.h"
#include "governor.h"
#include <mach/bts.h>

#define DEVFREQ_MIF_REBOOT_FREQ	(1656000/2)
#define DEVFREQ_INITIAL_FREQ	(3104000/2)
#define DEVFREQ_POLLING_PERIOD	(0)
#define DLL_ON_FREQ		(1656000/2)
#define SUSPEND_LV	0

extern struct devfreq_opp_table devfreq_mif_opp_list[];

static struct devfreq_simple_exynos_data exynos7_devfreq_mif_governor_data = {
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
	.pm_qos_class_max	= PM_QOS_BUS_THROUGHPUT_MAX,
	.urgentthreshold	= 80,
	.upthreshold		= 70,
	.downthreshold		= 60,
	.idlethreshold		= 50,
	.cal_qos_max		= (3104000/2),
};

static struct exynos_devfreq_platdata exynos7420_qos_mif = {
	.default_qos		= 552000/2,
};

static struct ppmu_info ppmu_mif[] = {
	{
		.base = (void __iomem *)PPMU0_0_GEN_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU0_1_CPU_ADDR,
	}, {
		.base = (void __iomem *)PPMU1_0_GEN_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU1_1_CPU_ADDR,
	}, {
		.base = (void __iomem *)PPMU2_0_GEN_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU2_1_CPU_ADDR,
	}, {
		.base = (void __iomem *)PPMU3_0_GEN_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU3_1_CPU_ADDR,
	},
};

static struct devfreq_exynos devfreq_mif_exynos = {
	.type = MIF,
	.ppmu_list = ppmu_mif,
	.ppmu_count = ARRAY_SIZE(ppmu_mif),
};

static unsigned long current_freq;
static bool is_freq_changing;

static struct pm_qos_request exynos7_mif_qos;
static struct pm_qos_request boot_mif_qos;
struct pm_qos_request min_mif_thermal_qos;
struct device *mif_dev;

extern struct devfreq_thermal_work devfreq_mif_thermal_work;

#ifdef CONFIG_EXYNOS_TRAFFIC_MONITOR
extern void set_mif_freq_range(unsigned long min_freq, unsigned long max);
extern void set_mif_usage(long long busy_time, long long total_time);
extern void set_mif_cur_freq(unsigned long cur_freq);
#endif

static int exynos7_devfreq_mif_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_mif = data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&data->lock);

	if (devfreq_mif_thermal_work.max_freq)
		*target_freq = min(*target_freq, devfreq_mif_thermal_work.max_freq);

#ifdef CONFIG_EXYNOS_TRAFFIC_MONITOR
	set_mif_cur_freq(*target_freq);
#endif

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		mutex_unlock(&data->lock);
		dev_err(dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);

	rcu_read_unlock();

	target_idx = devfreq_get_opp_idx(devfreq_mif_opp_list, data->max_state, *target_freq);
	old_idx = devfreq_get_opp_idx(devfreq_mif_opp_list, data->max_state,
						devfreq_mif->previous_freq);
	old_freq = devfreq_mif->previous_freq;

	if (target_idx < 0 || old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	pr_debug("MIF LV_%d(%lu) ================> LV_%d(%lu, volt: %lu)\n",
			old_idx, old_freq, target_idx, *target_freq, target_volt);

	is_freq_changing = true;

	exynos_ss_freq(ESS_FLAG_MIF, old_freq, ESS_FLAG_IN);
	exynos7_update_bts_param(target_idx, old_freq > *target_freq);
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	if (!data->volt_offset)
		exynos7420_cl_dvfs_stop(ID_MIF, target_idx);
#endif
	if (data->mif_set_freq)
		data->mif_set_freq(data, target_idx, old_idx);
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	if (!data->volt_offset)
		exynos7420_cl_dvfs_start(ID_MIF);
#endif
	exynos7_update_bts_param(target_idx, old_freq < *target_freq);
	exynos_ss_freq(ESS_FLAG_MIF, *target_freq, ESS_FLAG_OUT);

	data->cur_freq = *target_freq;

	current_freq = *target_freq;
	is_freq_changing = false;
out:
	mutex_unlock(&data->lock);

	return ret;
}

int is_dll_on(void)
{
	int ret = 0;

	if ((current_freq == 0) || current_freq >= DLL_ON_FREQ || is_freq_changing)
		ret = 1;

	return ret;
}
EXPORT_SYMBOL_GPL(is_dll_on);

static int exynos7_devfreq_mif_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_mif *data = dev_get_drvdata(dev);
	int idx = -1;
	int above_idx = 0;
	int below_idx = data->max_state - 1;

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;

	idx = devfreq_get_opp_idx(devfreq_mif_opp_list, data->max_state,
						stat->current_frequency);

	if (idx < 0)
                return -EAGAIN;

	above_idx = idx - 1;
	below_idx = idx + 1;

	if (above_idx < 0)
		above_idx = 0;

	if (below_idx >= data->max_state)
		below_idx = data->max_state - 1;

	exynos7_devfreq_mif_governor_data.above_freq = devfreq_mif_opp_list[above_idx].freq;
	exynos7_devfreq_mif_governor_data.below_freq = devfreq_mif_opp_list[below_idx].freq;

	stat->busy_time = devfreq_mif_exynos.val_pmcnt;
	stat->total_time = devfreq_mif_exynos.val_ccnt;

#ifdef CONFIG_EXYNOS_TRAFFIC_MONITOR
	set_mif_usage(stat->busy_time, stat->total_time);
#endif
	return 0;
}

static struct devfreq_dev_profile exynos7_devfreq_mif_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos7_devfreq_mif_target,
	.get_dev_status	= exynos7_devfreq_mif_get_dev_status,
#ifdef CONFIG_HYBRID_INVOKING
	.AlwaysDeferred = false,
#endif
};

#define CHIPID_1_1 0x34

static int exynos7_init_mif_table(struct device *dev,
				struct devfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;
	uint32_t rev_num, subrev_num;
	uint32_t f_val;

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_mif_opp_list[i].freq;
		volt = get_match_volt(ID_MIF, freq);

		if (!volt)
			volt = devfreq_mif_opp_list[i].volt;

		devfreq_mif_opp_list[i].volt = volt;
		exynos7_devfreq_mif_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(MIF) : Failed to add opp entries %uKhz, %uuV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(MIF) : %7uKhz, %7uuV\n", freq, volt);
		}
	}
	if (soc_is_exynos7420()) {
		rev_num = (readl(S5P_VA_CHIPID) & 0x000000F0) >> 4;
		subrev_num = (readl(S5P_VA_CHIPID) & 0x0000000F);

		switch (exynos_get_table_ver()) {
		case 1 :
			/* EVT1 */
			if (rev_num == 1 && subrev_num == 0) {
				opp_disable(dev, devfreq_mif_opp_list[0].freq);
				opp_disable(dev, devfreq_mif_opp_list[1].freq);
				opp_disable(dev, devfreq_mif_opp_list[2].freq);
				opp_disable(dev, devfreq_mif_opp_list[3].freq);
			} else {
				opp_disable(dev, devfreq_mif_opp_list[0].freq);
				opp_disable(dev, devfreq_mif_opp_list[1].freq);
			}
			break;
		case 0 :
		case 5 :
			opp_disable(dev, devfreq_mif_opp_list[0].freq);
			opp_disable(dev, devfreq_mif_opp_list[1].freq);
			opp_disable(dev, devfreq_mif_opp_list[2].freq);
			opp_disable(dev, devfreq_mif_opp_list[3].freq);
			break;
		case 4 :
			opp_disable(dev, devfreq_mif_opp_list[0].freq);
			opp_disable(dev, devfreq_mif_opp_list[1].freq);
			break;
		}

		f_val = (readl(S5P_VA_CHIPID + CHIPID_1_1) >> 16) & 0x03;
		pr_info("DEVFREQ(MIF) : f_val: %d\n", f_val);

		switch (f_val) {
		case 0b01 :
			opp_disable(dev, devfreq_mif_opp_list[0].freq);
			break;
		case 0b11 :
			opp_disable(dev, devfreq_mif_opp_list[0].freq);
			opp_disable(dev, devfreq_mif_opp_list[1].freq);
			break;
		}
	}

	data->volt_of_avail_max_freq = get_volt_of_avail_max_freq(dev);
	pr_info("DEVFREQ(MIF) : voltage of available max freq : %7uuV\n",
			data->volt_of_avail_max_freq);
	return 0;
}

static int exynos7_devfreq_mif_notifier(struct notifier_block *nb, unsigned long val,
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

static int exynos7_devfreq_mif_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	unsigned long freq = DEVFREQ_MIF_REBOOT_FREQ;
	struct devfreq_data_mif *data = dev_get_drvdata(mif_dev);
	struct devfreq *devfreq_mif = data->devfreq;

	devfreq_mif->max_freq = freq;
	mutex_lock(&devfreq_mif->lock);
	update_devfreq(devfreq_mif);
	mutex_unlock(&devfreq_mif->lock);

	return NOTIFY_DONE;
}

static struct notifier_block exynos7_mif_reboot_notifier = {
	.notifier_call = exynos7_devfreq_mif_reboot_notifier,
};

extern void exynos7_devfreq_init_thermal(void);
extern struct attribute_group devfreq_mif_attr_group;
static int exynos7_devfreq_mif_probe(struct platform_device *pdev)
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

	data->dev = &pdev->dev;
	mif_dev = data->dev;
	exynos7420_devfreq_mif_init(data);
	exynos7_devfreq_mif_profile.max_state = data->max_state;
	data->cur_freq = exynos7_devfreq_mif_profile.initial_freq;
	data->default_qos = exynos7420_qos_mif.default_qos;
	data->initial_freq = exynos7_devfreq_mif_profile.initial_freq;
	data->cal_qos_max = exynos7_devfreq_mif_governor_data.cal_qos_max;

	data->use_dvfs = false;
	mutex_init(&data->lock);

	exynos7_devfreq_mif_profile.freq_table = kzalloc(sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos7_devfreq_mif_profile.freq_table == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos7_init_mif_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);

	data->volt_offset = 0;
	data->vdd_mif = regulator_get(NULL, "vdd_mif");

	rcu_read_lock();
	freq = exynos7_devfreq_mif_governor_data.cal_qos_max;
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(MIF) : Invalid OPP to set voltage\n");
		ret = PTR_ERR(target_opp);
		goto err_opp;
	}
	data->old_volt = opp_get_voltage(target_opp);

	rcu_read_unlock();

	data->devfreq = devfreq_add_device(data->dev,
						&exynos7_devfreq_mif_profile,
						"simple_exynos",
						&exynos7_devfreq_mif_governor_data);

	exynos7_devfreq_init_thermal();

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos7_devfreq_mif_notifier;

	exynos7420_devfreq_register(&devfreq_mif_exynos);
	exynos7420_ppmu_register_notifier(MIF, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos7_devfreq_mif_governor_data.cal_qos_max;

#ifdef CONFIG_EXYNOS_TRAFFIC_MONITOR
	set_mif_freq_range(data->devfreq->min_freq, data->devfreq->max_freq);
#endif

	register_reboot_notifier(&exynos7_mif_reboot_notifier);

	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_mif_attr_group);

#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_opp:
	regulator_put(data->vdd_mif);
err_inittable:
	kfree(exynos7_devfreq_mif_profile.freq_table);
err_freqtable:
	mutex_destroy(&data->lock);
	kfree(data);
err_data:
	return ret;
}

static int exynos7_devfreq_mif_remove(struct platform_device *pdev)
{
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_mif_thermal_qos);
	pm_qos_remove_request(&exynos7_mif_qos);
	pm_qos_remove_request(&boot_mif_qos);

	exynos7420_devfreq_mif_deinit(data);

	regulator_put(data->vdd_mif);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

extern void exynos7_devfreq_set_tREFI_1x(struct devfreq_data_mif *data);
extern unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset,
					unsigned int max_volt);
extern int exynos7_devfreq_mif_set_volt(struct devfreq_data_mif *data,
					unsigned long volt,
					unsigned long volt_range);
static int exynos7_devfreq_mif_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);
	unsigned long suspend_volt;

	exynos7_devfreq_set_tREFI_1x(data);

	suspend_volt = get_limit_voltage(devfreq_mif_opp_list[SUSPEND_LV].volt, data->volt_offset,
					data->volt_of_avail_max_freq);

	if (pm_qos_request_active(&exynos7_mif_qos))
		pm_qos_update_request(&exynos7_mif_qos, exynos7_devfreq_mif_profile.initial_freq);

	exynos7_devfreq_mif_set_volt(data, suspend_volt, suspend_volt + VOLT_STEP);

	pr_debug("MIF suspend voltage(%ld %d)\n", suspend_volt, regulator_get_voltage(data->vdd_mif));

	return 0;
}

static int exynos7_devfreq_mif_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos7_mif_qos))
		pm_qos_update_request(&exynos7_mif_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos7_devfreq_mif_pm = {
	.suspend	= exynos7_devfreq_mif_suspend,
	.resume		= exynos7_devfreq_mif_resume,
};

static struct platform_driver exynos7_devfreq_mif_driver = {
	.probe	= exynos7_devfreq_mif_probe,
	.remove	= exynos7_devfreq_mif_remove,
	.driver	= {
		.name	= "exynos7-devfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos7_devfreq_mif_pm,
	},
};

static struct platform_device exynos7_devfreq_mif_device = {
	.name	= "exynos7-devfreq-mif",
	.id	= -1,
};

static int exynos7_devfreq_mif_qos_init(void)
{
	pm_qos_add_request(&exynos7_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos7420_qos_mif.default_qos);
	pm_qos_add_request(&min_mif_thermal_qos, PM_QOS_BUS_THROUGHPUT, exynos7420_qos_mif.default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos7420_qos_mif.default_qos);
	pm_qos_update_request_timeout(&boot_mif_qos,
					exynos7_devfreq_mif_profile.initial_freq, PM_BOOT_TIME_LEN * USEC_PER_SEC);

	return 0;
}
device_initcall(exynos7_devfreq_mif_qos_init);

static int __init exynos7_devfreq_mif_init(void)
{
	int ret;

	exynos7_devfreq_mif_device.dev.platform_data = &exynos7420_qos_mif;

	ret = platform_device_register(&exynos7_devfreq_mif_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos7_devfreq_mif_driver);
}
late_initcall(exynos7_devfreq_mif_init);

static void __exit exynos7_devfreq_mif_exit(void)
{
	platform_driver_unregister(&exynos7_devfreq_mif_driver);
	platform_device_unregister(&exynos7_devfreq_mif_device);
}
module_exit(exynos7_devfreq_mif_exit);
