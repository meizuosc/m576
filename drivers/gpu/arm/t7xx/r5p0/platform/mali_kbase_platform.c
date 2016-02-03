/* drivers/gpu/arm/.../platform/mali_kbase_platform.c
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
 * @file mali_kbase_platform.c
 * Platform-dependent init.
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_custom_interface.h"
#include "gpu_dvfs_handler.h"
#include "gpu_notifier.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"

/* MALI_SEC_SECURE_RENDERING */
#include <linux/smc.h>
#include <asm/cacheflush.h>

struct kbase_device *pkbdev;
static int gpu_debug_level;

struct kbase_device *gpu_get_device_structure(void)
{
	return pkbdev;
}

void gpu_set_debug_level(int level)
{
	gpu_debug_level = level;
}

int gpu_get_debug_level(void)
{
	return gpu_debug_level;
}

#ifdef CONFIG_MALI_EXYNOS_TRACE
static int gpu_trace_level;

void gpu_set_trace_level(int level)
{
	int i;

	if (level == TRACE_ALL) {
		for (i = TRACE_NONE + 1; i < TRACE_ALL; i++)
			gpu_trace_level |= (1U << i);
	} else if (level == TRACE_NONE) {
		gpu_trace_level = TRACE_NONE;
	} else {
		gpu_trace_level |= (1U << level);
	}
}

bool gpu_check_trace_level(int level)
{
	if (gpu_trace_level & (1U << level))
		return true;
	return false;
}

bool gpu_check_trace_code(int code)
{
	int level;
	switch (code) {
	case KBASE_TRACE_CODE(DUMMY):
		return false;
	case KBASE_TRACE_CODE(LSI_CLOCK_VALUE):
	case KBASE_TRACE_CODE(LSI_CLOCK_ON):
	case KBASE_TRACE_CODE(LSI_CLOCK_OFF):
	case KBASE_TRACE_CODE(LSI_GPU_MAX_LOCK):
	case KBASE_TRACE_CODE(LSI_GPU_MIN_LOCK):
		level = TRACE_CLK;
		break;
	case KBASE_TRACE_CODE(LSI_VOL_VALUE):
		level = TRACE_VOL;
		break;
	case KBASE_TRACE_CODE(LSI_GPU_ON):
	case KBASE_TRACE_CODE(LSI_GPU_OFF):
	case KBASE_TRACE_CODE(LSI_SUSPEND):
	case KBASE_TRACE_CODE(LSI_RESUME):
	case KBASE_TRACE_CODE(LSI_TMU_VALUE):
		level = TRACE_NOTIFIER;
		break;
	case KBASE_TRACE_CODE(LSI_REGISTER_DUMP):
		level = TRACE_DUMP;
		break;
	default:
		level = TRACE_DEFAULT;
		break;
	}

	return gpu_check_trace_level(level);
}
#endif /* CONFIG_MALI_EXYNOS_TRACE */

uintptr_t gpu_get_attrib_data(gpu_attribute *attrib, int id)
{
	int i;

	for (i = 0; i < GPU_CONFIG_LIST_END; i++) {
		if (attrib[i].id == id)
			return attrib[i].data;
	}

	return 0;
}

static int gpu_validate_attrib_data(struct exynos_context *platform)
{
	uintptr_t data;
	gpu_attribute *attrib = (gpu_attribute *)gpu_get_config_attributes();

	platform->attrib = attrib;

	data = gpu_get_attrib_data(attrib, GPU_MAX_CLOCK);
	platform->gpu_max_clock = data == 0 ? 500 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_MAX_CLOCK_LIMIT);
	platform->gpu_max_clock_limit = data == 0 ? 500 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_MIN_CLOCK);
	platform->gpu_min_clock = data == 0 ? 160 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_DVFS_BL_CONFIG_CLOCK);
	platform->gpu_dvfs_config_clock = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_DVFS_START_CLOCK);
	platform->gpu_dvfs_start_clock = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_DVFS_BL_CONFIG_CLOCK);
	platform->gpu_dvfs_config_clock = data == 0 ? 266 : (u32) data;

#ifdef CONFIG_MALI_DVFS
#ifdef CONFIG_CPU_THERMAL_IPA
	data = gpu_get_attrib_data(attrib, GPU_POWER_COEFF);
	platform->ipa_power_coeff_gpu = data == 0 ? 59 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_DVFS_TIME_INTERVAL);
	platform->gpu_dvfs_time_interval = data == 0 ? 5 : (u32) data;
#endif /* CONFIG_CPU_THERMAL_IPA */
	data = gpu_get_attrib_data(attrib, GPU_DEFAULT_WAKEUP_LOCK);
	platform->wakeup_lock = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TYPE);
	platform->governor_type = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_DEFAULT);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_DEFAULT, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_DEFAULT);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_DEFAULT, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_DEFAULT);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_DEFAULT, (u32) data);

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_STATIC);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_STATIC, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_STATIC);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_STATIC, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_STATIC);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_STATIC, (u32) data);

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_BOOSTER);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_BOOSTER, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_BOOSTER);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_BOOSTER, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_BOOSTER);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_BOOSTER, (u32) data);

	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_START_CLOCK_INTERACTIVE);
	gpu_dvfs_update_start_clk(G3D_DVFS_GOVERNOR_INTERACTIVE, data == 0 ? 266 : (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_INTERACTIVE);
	gpu_dvfs_update_table(G3D_DVFS_GOVERNOR_INTERACTIVE, (gpu_dvfs_info *) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE);
	gpu_dvfs_update_table_size(G3D_DVFS_GOVERNOR_INTERACTIVE, (u32) data);
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK);
	platform->interactive.highspeed_clock = data == 0 ? 500 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD);
	platform->interactive.highspeed_load = data == 0 ? 100 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY);
	platform->interactive.highspeed_delay = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_DVFS_POLLING_TIME);
	platform->polling_speed = data == 0 ? 100 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_PMQOS_INT_DISABLE);
	platform->pmqos_int_disable = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_PMQOS_MIF_MAX_CLOCK);
	platform->pmqos_mif_max_clock = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_PMQOS_MIF_MAX_CLOCK_BASE);
	platform->pmqos_mif_max_clock_base = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_CL_DVFS_START_BASE);
	platform->cl_dvfs_start_base = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING1);
	platform->tmu_lock_clk[THROTTLING1] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING2);
	platform->tmu_lock_clk[THROTTLING2] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING3);
	platform->tmu_lock_clk[THROTTLING3] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_THROTTLING4);
	platform->tmu_lock_clk[THROTTLING4] = data == 0 ? 266 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_TEMP_TRIPPING);
	platform->tmu_lock_clk[TRIPPING] = data == 0 ? 266 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_BOOST_MIN_LOCK);
	platform->boost_gpu_min_lock = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_BOOST_EGL_MIN_LOCK);
	platform->boost_egl_min_lock = data == 0 ? 0 : (u32) data;
#endif /* CONFIG_MALI_DVFS */

	data = gpu_get_attrib_data(attrib, GPU_TMU_CONTROL);
	platform->tmu_status = data == 0 ? 0 : data;

	data = gpu_get_attrib_data(attrib, GPU_DEFAULT_VOLTAGE);
	platform->gpu_default_vol = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_COLD_MINIMUM_VOL);
	platform->cold_min_vol = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_VOLTAGE_OFFSET_MARGIN);
	platform->gpu_default_vol_margin = data == 0 ? 37500 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_BUS_DEVFREQ);
	platform->devfreq_status = data == 0 ? 1 : data;
	data = gpu_get_attrib_data(attrib, GPU_DYNAMIC_ABB);
	platform->dynamic_abb_status = data == 0 ? 0 : data;
	data = gpu_get_attrib_data(attrib, GPU_EARLY_CLK_GATING);
	platform->early_clk_gating_status = data == 0 ? 0 : data;
	data = gpu_get_attrib_data(attrib, GPU_DVS);
	platform->dvs_status = data == 0 ? 0 : data;

	data = gpu_get_attrib_data(attrib, GPU_PERF_GATHERING);
	platform->perf_gathering_status = data == 0 ? 0 : data;

#ifdef MALI_SEC_HWCNT
	data = gpu_get_attrib_data(attrib, GPU_HWCNT_GATHERING);
	platform->hwcnt_gathering_status = data == 0 ? 0 : data;

	data = gpu_get_attrib_data(attrib, GPU_HWCNT_GPR);
	platform->hwcnt_gpr_status = data == 0 ? 0 : data;

	data = gpu_get_attrib_data(attrib, GPU_HWCNT_POLLING_TIME);
	platform->hwcnt_polling_speed = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_HWCNT_UP_STEP);
	platform->hwcnt_up_step = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_HWCNT_DOWN_STEP);
	platform->hwcnt_down_step = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_HWCNT_DUMP_PERIOD);
	platform->hwcnt_dump_period = data == 0 ? 0 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_HWCNT_CHOOSE_JM);
	platform->hwcnt_choose_jm = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_HWCNT_CHOOSE_SHADER);
	platform->hwcnt_choose_shader = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_HWCNT_CHOOSE_TILER);
	platform->hwcnt_choose_tiler = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_HWCNT_CHOOSE_L3_CACHE);
	platform->hwcnt_choose_l3_cache = data == 0 ? 0 : (u32) data;
	data = gpu_get_attrib_data(attrib, GPU_HWCNT_CHOOSE_MMU_L2);
	platform->hwcnt_choose_mmu_l2 = data == 0 ? 0 : (u32) data;
#endif

	data = gpu_get_attrib_data(attrib, GPU_RUNTIME_PM_DELAY_TIME);
	platform->runtime_pm_delay_time = data == 0 ? 50 : (u32) data;

	data = gpu_get_attrib_data(attrib, GPU_DEBUG_LEVEL);
	gpu_debug_level = data == 0 ? DVFS_WARNING : (u32) data;
#ifdef CONFIG_MALI_EXYNOS_TRACE
	data = gpu_get_attrib_data(attrib, GPU_TRACE_LEVEL);
	gpu_set_trace_level(data == 0 ? TRACE_ALL : (u32) data);
#endif /* CONFIG_MALI_EXYNOS_TRACE */

	return 0;
}
extern void preload_balance_init(struct kbase_device *kbdev);
static int gpu_context_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform;

	platform = kmalloc(sizeof(struct exynos_context), GFP_KERNEL);

	if (platform == NULL)
		return -1;

	memset(platform, 0, sizeof(struct exynos_context));
	kbdev->platform_context = (void *) platform;
	pkbdev = kbdev;

	mutex_init(&platform->gpu_clock_lock);
	mutex_init(&platform->gpu_dvfs_handler_lock);
	spin_lock_init(&platform->gpu_dvfs_spinlock);

	gpu_validate_attrib_data(platform);
	preload_balance_init(kbdev);

	return 0;
}

/**
 ** Exynos5 hardware specific initialization
 **/
static mali_bool kbase_platform_exynos5_init(struct kbase_device *kbdev)
{
	/* gpu context init */
	if (gpu_context_init(kbdev) < 0)
		goto init_fail;

	/* gpu control module init */
	if (gpu_control_module_init(kbdev) < 0)
		goto init_fail;

	/* gpu utilization moduel init */
	gpu_dvfs_utilization_init(kbdev);

	/* dvfs governor init */
	gpu_dvfs_governor_init(kbdev);

#ifdef CONFIG_MALI_DVFS
	/* dvfs handler init */
	gpu_dvfs_handler_init(kbdev);
#endif /* CONFIG_MALI_DVFS */

	/* gpu notifier init */
	if (gpu_notifier_init(kbdev) < 0)
		goto init_fail;

#ifdef CONFIG_MALI_DEBUG_SYS
	/* gpu sysfs file init */
	if (gpu_create_sysfs_file(kbdev->dev) < 0)
		goto init_fail;
#endif /* CONFIG_MALI_DEBUG_SYS */

	return true;

init_fail:
	kfree(kbdev->platform_context);

	return false;
}

/**
 ** Exynos5 hardware specific termination
 **/
static void kbase_platform_exynos5_term(struct kbase_device *kbdev)
{
	struct exynos_context *platform;
	platform = (struct exynos_context *) kbdev->platform_context;

	gpu_notifier_term();

#ifdef CONFIG_MALI_DVFS
	gpu_dvfs_handler_deinit(kbdev);
#endif /* CONFIG_MALI_DVFS */

	gpu_dvfs_utilization_deinit(kbdev);

	gpu_control_module_term(kbdev);

	kfree(kbdev->platform_context);
	kbdev->platform_context = 0;

#ifdef CONFIG_MALI_DEBUG_SYS
	gpu_remove_sysfs_file(kbdev->dev);
#endif /* CONFIG_MALI_DEBUG_SYS */
}

kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_exynos5_init,
	.platform_term_func = &kbase_platform_exynos5_term,
};

/* MALI_SEC_SECURE_RENDERING */
static int exynos_secure_mode_enable(void)
{
	/* enable secure mode : TZPC */
	int ret = 0;

	ret = exynos_smc(SMC_PROTECTION_SET, 0,
                            0xc, SMC_PROTECTION_ENABLE);

	return ret;
}

static int exynos_secure_mode_disable(void)
{
	/* Turn off secure mode and reset GPU : TZPC */
	int ret = 0;

	ret = exynos_smc(SMC_PROTECTION_SET, 0,
                            0xc, SMC_PROTECTION_DISABLE);

	return ret;
}

static int exynos_secure_mem_enable(void)
{
	/* enable secure world mode : TZASC */
	int ret = 0;

	flush_all_cpu_caches();
	ret = exynos_smc(SMC_MEM_PROT_SET, 0, 0, 1);
	if( ret == SMC_CALL_ERROR ) {
		exynos_smc(SMC_MEM_PROT_SET, 0, 0, 0);
	}

	return ret;
}
static int exynos_secure_mem_disable(void)
{
	/* Turn off secure world mode : TZASC */
	int ret = 0;

	ret = exynos_smc(SMC_MEM_PROT_SET, 0, 0, 0);

	return ret;
}

static struct kbase_secure_ops exynos_secure_ops = {
	.secure_mode_enable  = exynos_secure_mode_enable,
	.secure_mode_disable = exynos_secure_mode_disable,
/* MALI_SEC_SECURE_RENDERING */
	.secure_mem_enable   = exynos_secure_mem_enable,
	.secure_mem_disable  = exynos_secure_mem_disable,
};

extern kbase_pm_callback_conf pm_callbacks;

static kbase_attribute config_attributes[] = {
#ifdef CONFIG_MALI_RT_PM
	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
		(uintptr_t)&pm_callbacks
	},
#endif /* CONFIG_MALI_RT_PM */
	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_DVFS_FREQ,
		100
	}, /* 100ms */
	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		(uintptr_t)&platform_funcs
	},
	{
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		50 /* 50ms before cancelling stuck jobs */
	},
	{
		KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
		6 /* 0.3sec */
	},
	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
		7 /* 0.35sec */
	},
	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
		8 /* 0.4sec */
	},
	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
		12 /* 0.6sec */
	},
	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
		14 /* 0.7sec */
	},
	{
		KBASE_CONFIG_ATTR_CPU_SPEED_FUNC,
		(uintptr_t)&get_cpu_clock_speed
	},
	{
		KBASE_CONFIG_ATTR_SECURE_CALLBACKS,
		(uintptr_t)&exynos_secure_ops
	},
	{
		KBASE_CONFIG_ATTR_END,
		0
	}
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static kbase_io_resources io_resources = {
	.job_irq_number   = JOB_IRQ_NUMBER,
	.mmu_irq_number   = MMU_IRQ_NUMBER,
	.gpu_irq_number   = GPU_IRQ_NUMBER,
	.io_memory_region = {
		.start = EXYNOS5_PA_G3D,
		.end   = EXYNOS5_PA_G3D + (4096 * 5) - 1
	}
};
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0) */

kbase_platform_config platform_config = {
		.attributes                = config_attributes,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		.io_resources              = &io_resources,
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0) */
};

int kbase_platform_early_init(struct platform_device *pdev)
{
	kbase_platform_config *config;
	int attribute_count;

	config = &platform_config;
	attribute_count = kbasep_get_config_attribute_count(config->attributes);

	return platform_device_add_data(
#ifndef CONFIG_MALI_PLATFORM_FAKE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
		pdev,
#else
		&exynos5_device_g3d,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0) */
#endif /* CONFIG_MALI_PLATFORM_FAKE */
		config->attributes,
		attribute_count * sizeof(config->attributes[0]));
}
