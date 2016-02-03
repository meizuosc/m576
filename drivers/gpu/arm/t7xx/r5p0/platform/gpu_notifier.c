/* drivers/gpu/arm/.../platform/gpu_notifier.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_notifier.c
 */

#include <mali_kbase.h>

#include <linux/suspend.h>
#include <linux/pm_runtime.h>
#include <mach/apm-exynos.h>
#include <mach/asv-exynos.h>

#include <linux/pm_qos.h>


#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_notifier.h"
#include "gpu_control.h"

#ifdef CONFIG_EXYNOS_THERMAL
#include <mach/tmu.h>
#endif /* CONFIG_EXYNOS_THERMAL */

#ifdef CONFIG_EXYNOS_NOC_DEBUGGING
#include <linux/exynos-noc.h>
#endif
extern struct kbase_device *pkbdev;

#ifdef CONFIG_EXYNOS_THERMAL
static int gpu_tmu_hot_check_and_work(struct kbase_device *kbdev, unsigned long event)
{
#ifdef CONFIG_MALI_DVFS
	struct exynos_context *platform;
	int lock_clock;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	switch (event) {
	case GPU_THROTTLING1:
		lock_clock = platform->tmu_lock_clk[THROTTLING1];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING1\n");
		break;
	case GPU_THROTTLING2:
		lock_clock = platform->tmu_lock_clk[THROTTLING2];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING2\n");
		break;
	case GPU_THROTTLING3:
		lock_clock = platform->tmu_lock_clk[THROTTLING3];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING3\n");
		break;
	case GPU_THROTTLING4:
		lock_clock = platform->tmu_lock_clk[THROTTLING4];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "THROTTLING4\n");
		break;
	case GPU_TRIPPING:
		lock_clock = platform->tmu_lock_clk[TRIPPING];
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "TRIPPING\n");
		break;
	default:
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: wrong event, %lu\n", __func__, event);
		return 0;
	}

	gpu_dvfs_clock_lock(GPU_DVFS_MAX_LOCK, TMU_LOCK, lock_clock);
#endif /* CONFIG_MALI_DVFS */
	return 0;
}

static void gpu_tmu_normal_work(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_DVFS
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return;

	gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, TMU_LOCK, 0);
#endif /* CONFIG_MALI_DVFS */
}

static int gpu_tmu_notifier(struct notifier_block *notifier,
				unsigned long event, void *v)
{
	struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (!platform->tmu_status)
		return NOTIFY_OK;

	platform->voltage_margin = 0;

	if (event == GPU_COLD) {
		platform->voltage_margin = platform->gpu_default_vol_margin;
	} else if (event == GPU_NORMAL) {
		gpu_tmu_normal_work(pkbdev);
	} else if (event >= GPU_THROTTLING1 && event <= GPU_TRIPPING) {
		if (gpu_tmu_hot_check_and_work(pkbdev, event))
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to open device", __func__);
	}

	GPU_LOG(DVFS_DEBUG, LSI_TMU_VALUE, 0u, event, "tmu event %ld\n", event);

	gpu_set_target_clk_vol(platform->cur_clock, false);

	return NOTIFY_OK;
}

static struct notifier_block gpu_tmu_nb = {
	.notifier_call = gpu_tmu_notifier,
};
#endif /* CONFIG_EXYNOS_THERMAL */

#ifdef CONFIG_MALI_RT_PM
extern int kbase_device_suspend(struct kbase_device *kbdev);
extern int kbase_device_resume(struct kbase_device *kbdev);

static int gpu_pm_notifier(struct notifier_block *nb, unsigned long event, void *cmd)
{
	int err = NOTIFY_OK;
	struct kbase_device *kbdev = pkbdev;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		if (kbdev)
			kbase_device_suspend(kbdev);
		GPU_LOG(DVFS_DEBUG, LSI_SUSPEND, 0u, 0u, "%s: suspend event\n", __func__);
		break;
	case PM_POST_SUSPEND:
		if (kbdev)
			kbase_device_resume(kbdev);
		GPU_LOG(DVFS_DEBUG, LSI_RESUME, 0u, 0u, "%s: resume event\n", __func__);
		break;
	default:
		break;
	}
	return err;
}

static int gpu_noc_notifier(struct notifier_block *nb, unsigned long event, void *cmd)
{
	if (strstr((char *)cmd, "G3D")) {
		GPU_LOG(DVFS_ERROR, LSI_RESUME, 0u, 0u, "%s: gpu_noc_notifier\n", __func__);
		gpu_register_dump();
	}
	return 0;
}

static int gpu_power_on(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power on\n");

	gpu_control_disable_customization(kbdev);

	if (pm_runtime_resume(kbdev->dev)) {
		if (platform->early_clk_gating_status) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "already power on\n");
			gpu_control_enable_clock(kbdev);
		}
		return 0;
	} else {
		return 1;
	}
}

static void gpu_power_off(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power off\n");
	gpu_control_enable_customization(kbdev);

	pm_schedule_suspend(kbdev->dev, platform->runtime_pm_delay_time);

	if (platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);
}

static void gpu_power_suspend(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power suspend\n");
	gpu_control_enable_customization(kbdev);

	pm_runtime_suspend(kbdev->dev);

	if (platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);
}

static struct notifier_block gpu_pm_nb = {
	.notifier_call = gpu_pm_notifier
};

static struct notifier_block gpu_noc_nb = {
	.notifier_call = gpu_noc_notifier
};

static mali_error gpu_device_runtime_init(struct kbase_device *kbdev)
{
	pm_suspend_ignore_children(kbdev->dev, true);
	return 0;
}

static void gpu_device_runtime_disable(struct kbase_device *kbdev)
{
	pm_runtime_disable(kbdev->dev);
}

static int pm_callback_dvfs_on(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	gpu_dvfs_timer_control(true);

	if (platform->dvfs_pending)
		platform->dvfs_pending = 0;

	return 0;
}

static int pm_callback_change_dvfs_level(struct kbase_device *kbdev, mali_bool enabledebug)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (enabledebug)
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "asv table[%u] clk[%d to %d]MHz, vol[%d (margin : %d) real: %d]mV\n",
				exynos_get_table_ver(), gpu_get_cur_clock(platform), platform->gpu_dvfs_start_clock,
				gpu_get_cur_voltage(platform), platform->voltage_margin, platform->cur_voltage);
	gpu_set_target_clk_vol(platform->gpu_dvfs_start_clock, false);
	gpu_dvfs_reset_env_data(kbdev);

	return 0;
}

static int pm_callback_runtime_on(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, LSI_GPU_ON, 0u, 0u, "runtime on callback\n");

	gpu_control_enable_clock(kbdev);
	gpu_dvfs_start_env_data_gathering(kbdev);
#ifdef CONFIG_MALI_DVFS
	if (platform->dvfs_status && platform->wakeup_lock)
		gpu_set_target_clk_vol(platform->gpu_dvfs_start_clock, false);
	else
#endif /* CONFIG_MALI_DVFS */
		gpu_set_target_clk_vol(platform->cur_clock, false);
	platform->power_status = true;

	return 0;
}
extern void preload_balance_setup(struct kbase_device *kbdev);
static void pm_callback_runtime_off(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return;

	GPU_LOG(DVFS_INFO, LSI_GPU_OFF, 0u, 0u, "runtime off callback\n");

	platform->power_status = false;

	gpu_dvfs_stop_env_data_gathering(kbdev);
#ifdef CONFIG_MALI_DVFS
	gpu_dvfs_timer_control(false);
	if (platform->dvfs_pending)
		platform->dvfs_pending = 0;
#endif /* CONFIG_MALI_DVFS */
	if (!platform->early_clk_gating_status)
		gpu_control_disable_clock(kbdev);

	preload_balance_setup(kbdev);
}

kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = gpu_power_on,
	.power_off_callback = gpu_power_off,
	.power_suspend_callback = gpu_power_suspend,
#ifdef CONFIG_MALI_RT_PM
	.power_runtime_init_callback = gpu_device_runtime_init,
	.power_runtime_term_callback = gpu_device_runtime_disable,
	.power_runtime_on_callback = pm_callback_runtime_on,
	.power_runtime_off_callback = pm_callback_runtime_off,
	.power_dvfs_on_callback = pm_callback_dvfs_on,
	.power_change_dvfs_level_callback = pm_callback_change_dvfs_level,
#else /* CONFIG_MALI_RT_PM */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
	.power_dvfs_on_callback = NULL,
	.power_change_dvfs_level_callback = NULL,
#endif /* CONFIG_MALI_RT_PM */
};
#endif /* CONFIG_MALI_RT_PM */




#ifdef CONFIG_EXYNOS_GPU_PM_QOS
static int exynos_gpu_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
        struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

        if (!platform) {
                GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is not initialized\n", __func__);
                return -ENODEV;
        }

       if (val)
       {
               gpu_dvfs_clock_lock(GPU_DVFS_MIN_LOCK, PMQOS_LOCK, val);
       }
       else
       {
               gpu_dvfs_clock_lock(GPU_DVFS_MIN_UNLOCK, PMQOS_LOCK, 0);
       }
       return NOTIFY_OK;
}

static struct notifier_block exynos_gpu_min_qos_notifier = {
       .notifier_call = exynos_gpu_min_qos_handler,
};

static int exynos_gpu_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
        struct exynos_context *platform = (struct exynos_context *)pkbdev->platform_context;

        if (!platform) {
                GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: platform context is not initialized\n", __func__);
                return -ENODEV;
        }

       if (val == platform->gpu_max_clock)
               gpu_dvfs_clock_lock(GPU_DVFS_MAX_UNLOCK, SYSFS_LOCK, 0);
       else
               gpu_dvfs_clock_lock(GPU_DVFS_MAX_LOCK, SYSFS_LOCK, val);

       return NOTIFY_OK;
}

static struct notifier_block exynos_gpu_max_qos_notifier = {
       .notifier_call = exynos_gpu_max_qos_handler,
};
#endif

int gpu_notifier_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	platform->voltage_margin = 0;
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_gpu_add_notifier(&gpu_tmu_nb);
#endif /* CONFIG_EXYNOS_THERMAL */

#ifdef CONFIG_MALI_RT_PM
	if (register_pm_notifier(&gpu_pm_nb))
		return -1;
#endif /* CONFIG_MALI_RT_PM */

#ifdef CONFIG_EXYNOS_NOC_DEBUGGING
	noc_notifier_chain_register(&gpu_noc_nb);
#endif
	pm_runtime_enable(kbdev->dev);

#if (defined CONFIG_EXYNOS_GPU_PM_QOS)
	pm_qos_add_notifier(PM_QOS_GPU_FREQ_MIN, &exynos_gpu_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_GPU_FREQ_MAX, &exynos_gpu_max_qos_notifier);
#endif

	return 0;
}

void gpu_notifier_term(void)
{
#if (defined CONFIG_EXYNOS_GPU_PM_QOS)
	pm_qos_remove_notifier(PM_QOS_GPU_FREQ_MIN, &exynos_gpu_min_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_GPU_FREQ_MAX, &exynos_gpu_max_qos_notifier);
#endif


#ifdef CONFIG_MALI_RT_PM
	unregister_pm_notifier(&gpu_pm_nb);
#endif /* CONFIG_MALI_RT_PM */
	return;
}
