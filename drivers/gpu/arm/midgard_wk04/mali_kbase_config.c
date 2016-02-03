/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_cpuprops.h>
#include <mali_kbase_config_defaults.h>

/* Specifies how many attributes are permitted in the config (excluding terminating attribute).
 * This is used in validation function so we can detect if configuration is properly terminated. This value can be
 * changed if we need to introduce more attributes or many memory regions need to be defined */
#define ATTRIBUTE_COUNT_MAX 32

/* Limits for gpu frequency configuration parameters. These will use for config validation. */
#define MAX_GPU_ALLOWED_FREQ_KHZ 1000000
#define MIN_GPU_ALLOWED_FREQ_KHZ 1

int kbasep_get_config_attribute_count(const kbase_attribute *attributes)
{
	int count = 1;

	if (!attributes)
		return -EINVAL;

	while (attributes->id != KBASE_CONFIG_ATTR_END) {
		attributes++;
		count++;
	}

	return count;
}

const kbase_attribute *kbasep_get_next_attribute(const kbase_attribute *attributes, int attribute_id)
{
	KBASE_DEBUG_ASSERT(attributes != NULL);

	while (attributes->id != KBASE_CONFIG_ATTR_END) {
		if (attributes->id == attribute_id)
			return attributes;

		attributes++;
	}
	return NULL;
}

KBASE_EXPORT_TEST_API(kbasep_get_next_attribute)

uintptr_t kbasep_get_config_value(struct kbase_device *kbdev, const kbase_attribute *attributes, int attribute_id)
{
	const kbase_attribute *attr;

	KBASE_DEBUG_ASSERT(attributes != NULL);

	attr = kbasep_get_next_attribute(attributes, attribute_id);
	if (attr != NULL)
		return attr->data;

	/* default values */
	switch (attribute_id) {
	case KBASE_CONFIG_ATTR_GPU_IRQ_THROTTLE_TIME_US:
		return DEFAULT_IRQ_THROTTLE_TIME_US;
		/* Begin scheduling defaults */
	case KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS:
		return DEFAULT_JS_SCHEDULING_TICK_NS;
	case KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS:
		return DEFAULT_JS_SOFT_STOP_TICKS;
	case KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS_CL:
		return DEFAULT_JS_SOFT_STOP_TICKS_CL;
	case KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS:
		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8408))
			return DEFAULT_JS_HARD_STOP_TICKS_SS_HW_ISSUE_8408;
		else
			return DEFAULT_JS_HARD_STOP_TICKS_SS;
	case KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_CL:
		return DEFAULT_JS_HARD_STOP_TICKS_CL;
	case KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS:
		return DEFAULT_JS_HARD_STOP_TICKS_NSS;
	case KBASE_CONFIG_ATTR_JS_CTX_TIMESLICE_NS:
		return DEFAULT_JS_CTX_TIMESLICE_NS;
	case KBASE_CONFIG_ATTR_JS_CFS_CTX_RUNTIME_INIT_SLICES:
		return DEFAULT_JS_CFS_CTX_RUNTIME_INIT_SLICES;
	case KBASE_CONFIG_ATTR_JS_CFS_CTX_RUNTIME_MIN_SLICES:
		return DEFAULT_JS_CFS_CTX_RUNTIME_MIN_SLICES;
	case KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS:
		if (kbase_hw_has_issue(kbdev, BASE_HW_ISSUE_8408))
			return DEFAULT_JS_RESET_TICKS_SS_HW_ISSUE_8408;
		else
			return DEFAULT_JS_RESET_TICKS_SS;
	case KBASE_CONFIG_ATTR_JS_RESET_TICKS_CL:
		return DEFAULT_JS_RESET_TICKS_CL;
	case KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS:
		return DEFAULT_JS_RESET_TICKS_NSS;
	case KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS:
		return DEFAULT_JS_RESET_TIMEOUT_MS;
		/* End scheduling defaults */
	case KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS:
		return 0;
	case KBASE_CONFIG_ATTR_PLATFORM_FUNCS:
		return 0;
	case KBASE_CONFIG_ATTR_SECURE_BUT_LOSS_OF_PERFORMANCE:
		return DEFAULT_SECURE_BUT_LOSS_OF_PERFORMANCE;
	case KBASE_CONFIG_ATTR_CPU_SPEED_FUNC:
		return DEFAULT_CPU_SPEED_FUNC;
	case KBASE_CONFIG_ATTR_GPU_SPEED_FUNC:
		return 0;
	case KBASE_CONFIG_ATTR_ARID_LIMIT:
		return DEFAULT_ARID_LIMIT;
	case KBASE_CONFIG_ATTR_AWID_LIMIT:
		return DEFAULT_AWID_LIMIT;
	case KBASE_CONFIG_ATTR_ALTERNATIVE_HWC:
		return DEFAULT_ALTERNATIVE_HWC;
	case KBASE_CONFIG_ATTR_POWER_MANAGEMENT_DVFS_FREQ:
		return DEFAULT_PM_DVFS_FREQ;
	case KBASE_CONFIG_ATTR_PM_SHADER_POWEROFF_TIME:
		return DEFAULT_PM_SHADER_POWEROFF_TIME;
	default:
		KBASE_DEBUG_PRINT_ERROR(KBASE_CORE, "kbasep_get_config_value. Cannot get value of attribute with id=%d and no default value defined", attribute_id);
		return 0;
	}
}

KBASE_EXPORT_TEST_API(kbasep_get_config_value)

mali_bool kbasep_platform_device_init(kbase_device *kbdev)
{
	kbase_platform_funcs_conf *platform_funcs;

	platform_funcs = (kbase_platform_funcs_conf *) kbasep_get_config_value(kbdev, kbdev->config_attributes, KBASE_CONFIG_ATTR_PLATFORM_FUNCS);
	if (platform_funcs) {
		if (platform_funcs->platform_init_func)
			return platform_funcs->platform_init_func(kbdev);
	}
	return MALI_TRUE;
}

void kbasep_platform_device_term(kbase_device *kbdev)
{
	kbase_platform_funcs_conf *platform_funcs;

	platform_funcs = (kbase_platform_funcs_conf *) kbasep_get_config_value(kbdev, kbdev->config_attributes, KBASE_CONFIG_ATTR_PLATFORM_FUNCS);
	if (platform_funcs) {
		if (platform_funcs->platform_term_func)
			platform_funcs->platform_term_func(kbdev);
	}
}

static mali_bool kbasep_validate_gpu_clock_freq(kbase_device *kbdev, const kbase_attribute *attributes)
{
	uintptr_t freq_min = kbasep_get_config_value(kbdev, attributes, KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN);
	uintptr_t freq_max = kbasep_get_config_value(kbdev, attributes, KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX);

	if ((freq_min > MAX_GPU_ALLOWED_FREQ_KHZ) || (freq_min < MIN_GPU_ALLOWED_FREQ_KHZ) || (freq_max > MAX_GPU_ALLOWED_FREQ_KHZ) || (freq_max < MIN_GPU_ALLOWED_FREQ_KHZ) || (freq_min > freq_max)) {
		KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Invalid GPU frequencies found in configuration: min=%ldkHz, max=%ldkHz.", freq_min, freq_max);
		return MALI_FALSE;
	}

	return MALI_TRUE;
}

static mali_bool kbasep_validate_pm_callback(const kbase_pm_callback_conf *callbacks)
{
	if (callbacks == NULL) {
		/* Having no callbacks is valid */
		return MALI_TRUE;
	}

	if ((callbacks->power_off_callback != NULL && callbacks->power_on_callback == NULL) || (callbacks->power_off_callback == NULL && callbacks->power_on_callback != NULL)) {
		KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Invalid power management callbacks: Only one of power_off_callback and power_on_callback was specified");
		return MALI_FALSE;
	}
	return MALI_TRUE;
}

static mali_bool kbasep_validate_cpu_speed_func(kbase_cpuprops_clock_speed_function fcn)
{
	return fcn != NULL;
}

mali_bool kbasep_validate_configuration_attributes(kbase_device *kbdev, const kbase_attribute *attributes)
{
	int i;
	mali_bool had_gpu_freq_min = MALI_FALSE, had_gpu_freq_max = MALI_FALSE;

	KBASE_DEBUG_ASSERT(attributes);

	for (i = 0; attributes[i].id != KBASE_CONFIG_ATTR_END; i++) {
		if (i >= ATTRIBUTE_COUNT_MAX) {
			KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "More than ATTRIBUTE_COUNT_MAX=%d configuration attributes defined. Is attribute list properly terminated?", ATTRIBUTE_COUNT_MAX);
			return MALI_FALSE;
		}

		switch (attributes[i].id) {
		case KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN:
			had_gpu_freq_min = MALI_TRUE;
			if (MALI_FALSE == kbasep_validate_gpu_clock_freq(kbdev, attributes)) {
				/* Warning message handled by kbasep_validate_gpu_clock_freq() */
				return MALI_FALSE;
			}
			break;

		case KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX:
			had_gpu_freq_max = MALI_TRUE;
			if (MALI_FALSE == kbasep_validate_gpu_clock_freq(kbdev, attributes)) {
				/* Warning message handled by kbasep_validate_gpu_clock_freq() */
				return MALI_FALSE;
			}
			break;

			/* Only non-zero unsigned 32-bit values accepted */
		case KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS:
#if CSTD_CPU_64BIT
			if (attributes[i].data == 0u || (u64) attributes[i].data > (u64) U32_MAX)
#else
			if (attributes[i].data == 0u)
#endif
			{
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Invalid Job Scheduling Configuration attribute for " "KBASE_CONFIG_ATTR_JS_SCHEDULING_TICKS_NS: %d", (int)attributes[i].data);
				return MALI_FALSE;
			}
			break;

			/* All these Job Scheduling attributes are FALLTHROUGH: only unsigned 32-bit values accepted */
		case KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS:
		case KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS_CL:
		case KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS:
		case KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_CL:
		case KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS:
		case KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS:
		case KBASE_CONFIG_ATTR_JS_RESET_TICKS_CL:
		case KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS:
		case KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS:
		case KBASE_CONFIG_ATTR_JS_CTX_TIMESLICE_NS:
		case KBASE_CONFIG_ATTR_JS_CFS_CTX_RUNTIME_INIT_SLICES:
		case KBASE_CONFIG_ATTR_JS_CFS_CTX_RUNTIME_MIN_SLICES:
#if CSTD_CPU_64BIT
			if ((u64) attributes[i].data > (u64) U32_MAX) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Job Scheduling Configuration attribute exceeds 32-bits: " "id==%d val==%d", attributes[i].id, (int)attributes[i].data);
				return MALI_FALSE;
			}
#endif
			break;

		case KBASE_CONFIG_ATTR_GPU_IRQ_THROTTLE_TIME_US:
#if CSTD_CPU_64BIT
			if ((u64) attributes[i].data > (u64) U32_MAX) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "IRQ throttle time attribute exceeds 32-bits: " "id==%d val==%d", attributes[i].id, (int)attributes[i].data);
				return MALI_FALSE;
			}
#endif
			break;

		case KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS:
			if (MALI_FALSE == kbasep_validate_pm_callback((kbase_pm_callback_conf *) attributes[i].data)) {
				/* Warning message handled by kbasep_validate_pm_callback() */
				return MALI_FALSE;
			}
			break;

		case KBASE_CONFIG_ATTR_SECURE_BUT_LOSS_OF_PERFORMANCE:
			if (attributes[i].data != MALI_TRUE && attributes[i].data != MALI_FALSE) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Value for KBASE_CONFIG_ATTR_SECURE_BUT_LOSS_OF_PERFORMANCE was not " "MALI_TRUE or MALI_FALSE: %u", (unsigned int)attributes[i].data);
				return MALI_FALSE;
			}
			break;

		case KBASE_CONFIG_ATTR_CPU_SPEED_FUNC:
			if (MALI_FALSE == kbasep_validate_cpu_speed_func((kbase_cpuprops_clock_speed_function) attributes[i].data)) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Invalid function pointer in KBASE_CONFIG_ATTR_CPU_SPEED_FUNC");
				return MALI_FALSE;
			}
			break;

		case KBASE_CONFIG_ATTR_GPU_SPEED_FUNC:
			if (0 == attributes[i].data) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Invalid function pointer in KBASE_CONFIG_ATTR_GPU_SPEED_FUNC");
				return MALI_FALSE;
			}
			break;

		case KBASE_CONFIG_ATTR_PLATFORM_FUNCS:
			/* any value is allowed */
			break;

		case KBASE_CONFIG_ATTR_AWID_LIMIT:
		case KBASE_CONFIG_ATTR_ARID_LIMIT:
			if ((u32) attributes[i].data > 0x3) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Invalid AWID or ARID limit");
				return MALI_FALSE;
			}
			break;

		case KBASE_CONFIG_ATTR_ALTERNATIVE_HWC:
			if (attributes[i].data != MALI_TRUE && attributes[i].data != MALI_FALSE) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Value for KBASE_CONFIG_ATTR_ALTERNATIVE_HWC was not " "MALI_TRUE or MALI_FALSE: %u", (unsigned int)attributes[i].data);
				return MALI_FALSE;
			}
			break;

		case KBASE_CONFIG_ATTR_POWER_MANAGEMENT_DVFS_FREQ:
#if CSTD_CPU_64BIT
			if ((u64) attributes[i].data > (u64) U32_MAX) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "PM DVFS interval exceeds 32-bits: " "id==%d val==%d", attributes[i].id, (int)attributes[i].data);
				return MALI_FALSE;
			}
#endif
			break;

		case KBASE_CONFIG_ATTR_PM_SHADER_POWEROFF_TIME:
#if CSTD_CPU_64BIT
			if ((u64) attributes[i].data > (u64) U32_MAX) {
				KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "PM shader poweroff time exceeds 32-bits: " "id==%d val==%d", attributes[i].id, (int)attributes[i].data);
				return MALI_FALSE;
			}
#endif
			break;

		default:
			KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Invalid attribute found in configuration: %d", attributes[i].id);
			return MALI_FALSE;
		}
	}

	if (!had_gpu_freq_min) {
		KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Configuration does not include mandatory attribute KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN");
		return MALI_FALSE;
	}

	if (!had_gpu_freq_max) {
		KBASE_DEBUG_PRINT_WARN(KBASE_CORE, "Configuration does not include mandatory attribute KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX");
		return MALI_FALSE;
	}

	return MALI_TRUE;
}
