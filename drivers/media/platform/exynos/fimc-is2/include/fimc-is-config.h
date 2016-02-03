/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CONFIG_H
#define FIMC_IS_CONFIG_H

/*
 * =================================================================================================
 * CONFIG - GLOBAL OPTIONS
 * =================================================================================================
 */
#define FIMC_IS_SENSOR_COUNT	4
#define FIMC_IS_STREAM_COUNT	3

#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
#define PDAF_STAT_SIZE (965)
#define NUM_STAT_INTERNAL_BUF		(4)
#endif

/*
 * =================================================================================================
 * CONFIG -PLATFORM CONFIG
 * =================================================================================================
 */

#if defined(CONFIG_SOC_EXYNOS5422)
#define SOC_30S
#define SOC_30C
#define SOC_30P
#define SOC_I0S
/* #define SOC_I0C */
/* #define SOC_I0P */
#define SOC_31S
#define SOC_31C
#define SOC_31P
/* #define SOC_I1S */
/* #define SOC_I1C */
/* #define SOC_I1P */
#define SOC_DRC
#define SOC_DIS
#define SOC_ODC
#define SOC_DNR
#define SOC_SCC
#define SOC_SCP
#define SOC_VRA

/* Post Processing Configruation */
#define ENABLE_DRC
#define ENABLE_DIS
/* #define ENABLE_DNR */
#define ENABLE_VRA

#elif defined(CONFIG_SOC_EXYNOS7420)
#define SOC_30S
#define SOC_30C
#define SOC_30P
#define SOC_I0S
#define SOC_I0C
#define SOC_I0P
#define SOC_31S
#define SOC_31C
#define SOC_31P
#define SOC_I1S
#define SOC_I1C
#define SOC_I1P
#define SOC_DRC
#define SOC_DIS
#define SOC_ODC
#define SOC_DNR
/* #define SOC_SCC */
#define SOC_SCP
#define SOC_VRA

/* Post Processing Configruation */
/* #define ENABLE_DRC */
/* #define ENABLE_DIS */
//#define ENABLE_DNR
#define ENABLE_VRA

#else
#error fimc-is driver can NOT support this platform

#endif

/*
 * =================================================================================================
 * CONFIG - FEATURE ENABLE
 * =================================================================================================
 */

#define FW_SUSPEND_RESUME
#define ENABLE_CLOCK_GATE
#define HAS_FW_CLOCK_GATE
/* #define ENABLE_CACHE */
#define ENABLE_FULL_BYPASS
#define ENABLE_ONE_SLOT
/* #define ENABLE_FAST_SHOT */
#define ENABLE_FAULT_HANDLER
/* #define ENABLE_MIF_400 */
#define ENABLE_DTP
/* #define USE_ION_ALLOC */
/* #define ENABLE_SETFILE */
#define ENABLE_FLITE_OVERFLOW_STOP
#define ENABLE_DBG_FS
#define ENABLE_RESERVED_MEM

//#define FIMC_IS_DBG_GPIO
#define FIMC_IS_DBG_CLK

#if defined(CONFIG_SOC_EXYNOS5422)
#define SUPPORTED_IS_CMD_VER	132

#elif defined(CONFIG_SOC_EXYNOS7420)
#define SUPPORTED_IS_CMD_VER	132

#else
#error fimc-is driver can NOT support this platform

#endif

#if defined(CONFIG_PM_DEVFREQ)
#define ENABLE_DVFS
#define START_DVFS_LEVEL FIMC_IS_SN_MAX
#endif

#if defined(CONFIG_ARM_EXYNOS7420_BUS_DEVFREQ)
#define CONFIG_FIMC_IS_BUS_DEVFREQ
#endif

/* notifier for MIF throttling */
#undef CONFIG_CPU_THERMAL_IPA
#if defined(CONFIG_CPU_THERMAL_IPA)
#define EXYNOS_MIF_ADD_NOTIFIER(nb) exynos_mif_add_notifier(nb)
#else
#define EXYNOS_MIF_ADD_NOTIFIER(nb)
#endif

/*
 * =================================================================================================
 * CONFIG - DEBUG OPTIONS
 * =================================================================================================
 */
#define SUPPORTED_A5_MEMORY_SIZE_UP // iankim

#define DEBUG_LOG_MEMORY
/* #define DEBUG */
#define DBG_VIDEO
#define DBG_DEVICE
#define DBG_PER_FRAME
/* #define DBG_STREAMING */
#define DBG_STREAM_ID 0xF
#define PANIC_ENABLE
/* #define FIXED_FPS_DEBUG */
#define FIXED_FPS_VALUE 5
/* #define DBG_CSIISR */
/* #define DBG_FLITEISR */
/* #define DBG_IMAGE_KMAPPING */
/* #define DBG_IMAGE_DUMP */
#define DBG_IMAGE_DUMP_COUNT 10000
#define DBG_IMAGE_DUMP_VIDEO 2
#define DBG_IMAGE_DUMP_INDEX 4
/* #define USE_ADVANCED_DZOOM */
/* #define TASKLET_MSG */
/* #define PRINT_CAPABILITY */
#define PRINT_BUFADDR
/* #define PRINT_DZOOM */
/* #define PRINT_PARAM */
/* #define PRINT_I2CCMD */
#define ISDRV_VERSION 244
#define DEBUG_LOG_LEVEL_CHANGE

/*
 * driver version extension
 */
#ifdef ENABLE_CLOCK_GATE
#define get_drv_clock_gate() 0x1
#else
#define get_drv_clock_gate() 0x0
#endif
#ifdef ENABLE_DVFS
#define get_drv_dvfs() 0x2
#else
#define get_drv_dvfs() 0x0
#endif

#define GET_SSX_ID(video) (video->id - FIMC_IS_VIDEO_SS0_NUM)
#define GET_3XS_ID(video) ((video->id < FIMC_IS_VIDEO_31S_NUM) ? 0 : 1)
#define GET_3XC_ID(video) ((video->id < FIMC_IS_VIDEO_31S_NUM) ? 0 : 1)
#define GET_3XP_ID(video) ((video->id < FIMC_IS_VIDEO_31S_NUM) ? 0 : 1)
#define GET_IXS_ID(video) ((video->id < FIMC_IS_VIDEO_I1S_NUM) ? 0 : 1)
#define GET_IXC_ID(video) ((video->id < FIMC_IS_VIDEO_I1S_NUM) ? 0 : 1)
#define GET_IXP_ID(video) ((video->id < FIMC_IS_VIDEO_I1S_NUM) ? 0 : 1)

/* sync log with HAL, FW */
#define log_sync(sync_id) info("FIMC_IS_SYNC %d\n", sync_id)

#ifdef err
#undef err
#endif
#define err(fmt, args...) \
	err_common("[@][ERR]%s:%d:", fmt "\n", __func__, __LINE__, ##args)

/* multi-stream */
#define merr(fmt, object, args...) \
	merr_common("[@][%d][ERR]%s:%d:", fmt "\n", object->instance, __func__, __LINE__, ##args)

/* multi-stream & group error */
#define mgerr(fmt, object, group, args...) \
	merr_common("[@][%d][GP%d][ERR]%s:%d:", fmt "\n", object->instance, group->id, __func__, __LINE__, ##args)

/* multi-stream & subdev error */
#define mserr(fmt, object, subdev, args...) \
	merr_common("[@][%d][%s][ERR]%s:%d:", fmt "\n", object->instance, subdev->name, __func__, __LINE__, ##args)

#define msrerr(fmt, object, subdev, frame, args...) \
	merr_common("[%d][%s][F%d]", fmt, object->instance, subdev->name, frame->fcount, ##args)

/* multi-stream & video error */
#define mverr(fmt, object, video, args...) \
	merr_common("[@][%d][V%02d][ERR]%s:%d:", fmt "\n", object->instance, video->id, __func__, __LINE__, ##args)

/* multi-stream & runtime error */
#define mrerr(fmt, object, frame, args...) \
	merr_common("[@][%d][F%d][ERR]%s:%d:", fmt "\n", object->instance, frame->fcount, __func__, __LINE__, ##args)

/* multi-stream & group & runtime error */
#define mgrerr(fmt, object, group, frame, args...) \
	merr_common("[@][%d][GP%d][F%d][ERR]%s:%d:", fmt "\n", object->instance, group->id, frame->fcount, __func__, __LINE__, ##args)

#ifdef warn
#undef warn
#endif
#define warn(fmt, args...) \
	warn_common("%s() [@][WRN]", fmt "\n", __func__, ##args)

#define mwarn(fmt, object, args...) \
	mwarn_common("[%d][WRN]", fmt "\n", object->instance, ##args)

#define mgwarn(fmt, object, group, args...) \
	mwarn_common("[%d][GP%d][WRN]", fmt "\n", object->instance, group->id, ##args)

#define mrwarn(fmt, object, frame, args...) \
	mwarn_common("[%d][F%d][WRN]", fmt, object->instance, frame->fcount, ##args)

#define mswarn(fmt, object, subdev, args...) \
	mwarn_common("[%d][%s][WRN]", fmt "\n", object->instance, subdev->name, ##args)

#define info(fmt, args...) \
	dbg_common("[@]", fmt, ##args)

#define minfo(fmt, object, args...) \
	minfo_common("[%d]", fmt, object->instance, ##args)

#define mvinfo(fmt, object, video, args...) \
	minfo_common("[%d][V%02d]", fmt, object->instance, video->id, ##args)

#define msinfo(fmt, object, subdev, args...) \
	minfo_common("[%d][%s]", fmt, object->instance, subdev->name, ##args)

#define msrinfo(fmt, object, subdev, frame, args...) \
	minfo_common("[%d][%s][F%d]", fmt, object->instance, subdev->name, frame->fcount, ##args)

#define mginfo(fmt, object, group, args...) \
	minfo_common("[%d][GP%d]", fmt, object->instance, group->id, ##args)

#define mrinfo(fmt, object, frame, args...) \
	minfo_common("[%d][F%d]", fmt, object->instance, frame->fcount, ##args)

#define mgrinfo(fmt, object, group, frame, args...) \
	minfo_common("[%d][GP%d][F%d]", fmt, object->instance, group->id, frame->fcount, ##args)

#if (defined(DEBUG) && defined(DBG_VIDEO))
#define dbg(fmt, args...)

#define dbg_warning(fmt, args...) \
	dbg_common("%s[WAR] Warning! ", fmt, __func__, ##args)

/* debug message for video node */
#define mdbgv_vid(fmt, args...) \
	dbg_common("[@][VID:V] ", fmt, ##args)

#define mdbgv_sensor(fmt, this, args...) \
	mdbg_common("[%d][SS%d:V] ", fmt, ((struct fimc_is_device_sensor *)this->device)->instance, GET_SSX_ID(this->video), ##args)

#define mdbgv_3aa(fmt, this, args...) \
	mdbg_common("[%d][3%dS:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, GET_3XS_ID(this->video), ##args)

#define mdbgv_3xc(fmt, this, args...) \
	mdbg_common("[%d][3%dC:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, GET_3XC_ID(this->video), ##args)

#define mdbgv_3xp(fmt, this, args...) \
	mdbg_common("[%d][3%dP:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, GET_3XP_ID(this->video), ##args)

#define mdbgv_isp(fmt, this, args...) \
	mdbg_common("[%d][I%dS:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, GET_IXS_ID(this->video), ##args)

#define mdbgv_ixc(fmt, this, args...) \
	mdbg_common("[%d][I%dC:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, GET_IXC_ID(this->video), ##args)

#define mdbgv_ixp(fmt, this, args...) \
	mdbg_common("[%d][I%dP:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, GET_IXP_ID(this->video), ##args)

#define mdbgv_scp(fmt, this, args...) \
	mdbg_common("[%d][SCP:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, ##args)

#define mdbgv_scc(fmt, this, args...) \
	mdbg_common("[%d][SCC:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, ##args)

#define mdbgv_dis(fmt, this, args...) \
	mdbg_common("[%d][VDO:V] ", fmt, ((struct fimc_is_device_ischain *)this->device)->instance, ##args)
#else
#define dbg(fmt, args...)

/* debug message for video node */
#define mdbgv_vid(fmt, this, args...)
#define dbg_sensor(fmt, args...)
#define mdbgv_sensor(fmt, this, args...)
#define mdbgv_3aa(fmt, this, args...)
#define mdbgv_3xc(fmt, this, args...)
#define mdbgv_3xp(fmt, this, args...)
#define mdbgv_isp(fmt, this, args...)
#define mdbgv_ixc(fmt, this, args...)
#define mdbgv_ixp(fmt, this, args...)
#define mdbgv_scp(fmt, this, args...)
#define mdbgv_scc(fmt, this, args...)
#define mdbgv_dis(fmt, this, args...)
#endif

#if (defined(DEBUG) && defined(DBG_DEVICE))
/* debug message for device */
#define mdbgd_sensor(fmt, this, args...) \
	mdbg_common("[%d][SEN:D] ", fmt, this->instance, ##args)

#define mdbgd_front(fmt, this, args...) \
	mdbg_common("[%d][FRT:D] ", fmt, this->instance, ##args)

#define mdbgd_back(fmt, this, args...) \
	mdbg_common("[%d][BAK:D] ", fmt, this->instance, ##args)

#define mdbgd_3a0(fmt, this, args...) \
	mdbg_common("[%d][3A0:D] ", fmt, this->instance, ##args)

#define mdbgd_3a1(fmt, this, args...) \
	mdbg_common("[%d][3A1:D] ", fmt, this->instance, ##args)

#define mdbgd_isp(fmt, this, args...) \
	mdbg_common("[%d][ISP:D] ", fmt, this->instance, ##args)

#define mdbgd_ischain(fmt, this, args...) \
	mdbg_common("[%d][ISC:D] ", fmt, this->instance, ##args)

#define mdbgd_group(fmt, group, args...) \
	mdbg_common("[%d][GP%d:D] ", fmt, group->device->instance, group->id, ##args)

#define dbg_resource(fmt, args...) \
	dbg_common("[@][RSC] ", fmt, ##args)

#define dbg_core(fmt, args...) \
	dbg_common("[@][COR] ", fmt, ##args)
#else
/* debug message for device */
#define mdbgd_sensor(fmt, this, args...)
#define mdbgd_front(fmt, this, args...)
#define mdbgd_back(fmt, this, args...)
#define mdbgd_isp(fmt, this, args...)
#define mdbgd_ischain(fmt, this, args...)
#define mdbgd_group(fmt, group, args...)
#define dbg_resource(fmt, args...)
#define dbg_core(fmt, args...)
#endif

#if (defined(DEBUG) && defined(DBG_STREAMING))
#define dbg_interface(fmt, args...) \
	dbg_common("[@][ITF] ", fmt, ##args)
#define dbg_frame(fmt, args...) \
	dbg_common("[@][FRM] ", fmt, ##args)
#else
#define dbg_interface(fmt, args...)
#define dbg_frame(fmt, args...)
#endif

#if defined(DBG_PER_FRAME)
#define mdbg_pframe(fmt, object, subdev, frame, args...) \
	mdbg_common("[%d][%s][F%d]", fmt, object->instance, subdev->name, frame->fcount, ##args)
#else
#define mdbg_pframe(fmt, object, subdev, frame, args...)
#endif

/* log at probe */
#define probe_info(fmt, ...)		\
	pr_info(fmt, ##__VA_ARGS__)
#define probe_err(fmt, args...)		\
	pr_err("[@][ERR]%s:%d:" fmt "\n", __func__, __LINE__, ##args)
#define probe_warn(fmt, args...)	\
	pr_warning("[@][WRN]" fmt "\n", ##args)

#if defined(DEBUG_LOG_MEMORY)
#define fimc_is_err(fmt, ...)	printk(KERN_ERR fmt, ##__VA_ARGS__)
#define fimc_is_warn(fmt, ...)	printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define fimc_is_dbg(fmt, ...)	printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#define fimc_is_info(fmt, ...)	printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define fimc_is_err(fmt, ...)	pr_err(fmt, ##__VA_ARGS__)
#define fimc_is_warn(fmt, ...)	pr_warning(fmt, ##__VA_ARGS__)
#define fimc_is_dbg(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#define fimc_is_info(fmt, ...)	pr_info(fmt, ##__VA_ARGS__)
#endif

#define merr_common(prefix, fmt, instance, args...)				\
	do {									\
		if ((1<<(instance)) & DBG_STREAM_ID)				\
			fimc_is_err("[@]" prefix fmt, instance, ##args);		\
	} while (0)

#define mwarn_common(prefix, fmt, instance, args...)				\
	do {									\
		if ((1<<(instance)) & DBG_STREAM_ID)				\
			fimc_is_warn("[@]" prefix fmt, instance, ##args);		\
	} while (0)

#define mdbg_common(prefix, fmt, instance, args...)				\
	do {									\
		if ((1<<(instance)) & DBG_STREAM_ID)				\
			fimc_is_dbg("[@]" prefix fmt, instance, ##args);		\
	} while (0)

#define minfo_common(prefix, fmt, instance, args...)				\
	do {									\
		if ((1<<(instance)) & DBG_STREAM_ID)				\
			fimc_is_info("[@]" prefix fmt, instance, ##args);		\
	} while (0)

#define err_common(prefix, fmt, args...)				\
	fimc_is_err(prefix fmt, ##args)

#define warn_common(prefix, fmt, args...)		\
	fimc_is_warn(prefix fmt, ##args)

#define dbg_common(prefix, fmt, args...)	\
	fimc_is_dbg(prefix fmt, ##args)


#endif
