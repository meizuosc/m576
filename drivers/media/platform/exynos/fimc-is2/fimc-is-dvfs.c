/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;

#if defined(CONFIG_CAMERA_CUSTOM_SUPPORT)
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS);

#else

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT2);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE);

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS);
#endif

#if defined(ENABLE_DVFS)
/*
 * Static Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'static_scenarios'
 */
#if defined(CONFIG_CAMERA_CUSTOM_SUPPORT)
static struct fimc_is_dvfs_scenario static_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_FHD),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_UHD),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_FHD),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_WHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_WHD),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_UHD),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_PREVIEW),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING_WHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING_WHD),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD),
	},
};

/*
 * Dynamic Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'dynamic_scenarios'
 */
static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_REAR_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE),
	},
};
#else
static struct fimc_is_dvfs_scenario static_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DUAL_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_UHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_WHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_WHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_UHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_VT1,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VT1),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_VT2,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VT2),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT2),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING_WHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING_WHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_COMPANION_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_COMPANION_PREVIEW),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD),
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD),
	},
};

/*
 * Dynamic Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'dynamic_scenarios'
 */
static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DUAL_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_COMPANION_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_COMPANION_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func 	= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE),

	},
};
#endif
#else
/*
 * Default Scenario can not be seleted, this declaration is for static variable.
 */
static struct fimc_is_dvfs_scenario static_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DEFAULT,
		.scenario_nm		= NULL,
		.keep_frame_tick	= 0,
		.check_func		= NULL,
	},
};
static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DEFAULT,
		.scenario_nm		= NULL,
		.keep_frame_tick	= 0,
		.check_func		= NULL,
	},
};
#endif

static inline int fimc_is_get_open_sensor_cnt(struct fimc_is_core *core) {
	int i, sensor_cnt = 0;

	for (i = 0; i < FIMC_IS_STREAM_COUNT; i++)
		if (test_bit(FIMC_IS_SENSOR_OPEN, &(core->sensor[i].state)))
			sensor_cnt++;

	return sensor_cnt;
}

#if defined(CONFIG_CAMERA_CUSTOM_SUPPORT)
/* rear 120fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) >= 60 &&
			 fimc_is_sensor_g_framerate(device->sensor) < 240) && setfile_flag)

		return 1;
	else
		return 0;
}

/* rear camcording FHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_VIDEO);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_FHD) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear camcording UHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_VIDEO);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height > SIZE_FHD) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_UHD) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear preview FHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_FHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))

		return 1;
	else
		return 0;
}

/* rear preview WHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height > SIZE_FHD) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_WHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* rear preview UHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height > SIZE_WHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* front recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VIDEO_HIGH_SPEED_120FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
			(fimc_is_sensor_g_framerate(device->sensor) >= 60 &&
			 fimc_is_sensor_g_framerate(device->sensor) < 240) && setfile_flag)

		return 1;
	else
		return 0;
}


/* front recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((device->setfile & FIMC_IS_SETFILE_MASK)  == ISS_SUB_SCENARIO_VIDEO) &&
		(device->scp.output.width < 2560)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((device->setfile & FIMC_IS_SETFILE_MASK) == ISS_SUB_SCENARIO_VIDEO) &&
		(device->scp.output.width >= 2560)
		)
		return 1;
	else
		return 0;
}

/* front preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_STILL_PREVIEW))
		return 1;
	else
		return 0;
}

/* front capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_PREVIEW) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_PREVIEW))
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_CAMCORDING) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_CAMCORDING_WHD)) &&
		(device->scp.output.width < 2560)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_CAMCORDING) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_CAMCORDING_WHD)) &&
		(device->scp.output.width >= 2560)
		)
		return 1;
	else
		return 0;
}

/* rear capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_REAR_PREVIEW_FHD) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_REAR_PREVIEW_UHD))
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_REAR_CAMCORDING_FHD) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_REAR_CAMCORDING_UHD)) &&
		(device->scp.output.width < 4096)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_REAR_CAMCORDING_FHD) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_REAR_CAMCORDING_UHD)) &&
		(device->scp.output.width  >= 4096)
		)
		return 1;
	else
		return 0;
}

/* dis */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE)
{
	return 0;
}

#else

/* dual camcording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING)
{
	if (((device->setfile & FIMC_IS_SETFILE_MASK) == ISS_SUB_SCENARIO_DUAL_VIDEO))
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE)
{
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((device->setfile & FIMC_IS_SETFILE_MASK) == ISS_SUB_SCENARIO_DUAL_VIDEO))
		return 1;
	else
		return 0;
}

/* dual preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW)
{
	if (((device->setfile & FIMC_IS_SETFILE_MASK) == ISS_SUB_SCENARIO_DUAL_STILL))
		return 1;
	else
		return 0;
}

/* dual capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE)
{
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((device->setfile & FIMC_IS_SETFILE_MASK) == ISS_SUB_SCENARIO_DUAL_STILL))
		return 1;
	else
		return 0;
}

/* fastAE */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) > 30) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_STILL_PREVIEW))
		return 1;
	else
		return 0;
}

/* 60fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask & ISS_SUB_SCENARIO_FHD_60FPS);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) >= 60) &&
			(fimc_is_sensor_g_framerate(device->sensor) < 120) && setfile_flag)
		return 1;
	else
		return 0;
}

/* 120fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask & ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) >= 120 &&
			 fimc_is_sensor_g_framerate(device->sensor) < 240) && setfile_flag)

		return 1;
	else
		return 0;
}

/* 240fps recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask & ISS_SUB_SCENARIO_FHD_240FPS);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) >= 240) &&  setfile_flag)

		return 1;
	else
		return 0;
}

/* rear camcording FHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_VIDEO) ||
			(mask == ISS_SUB_SCENARIO_VIDEO_WDR);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_FHD) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear camcording UHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD)
{
	u32 mask = (device->setfile & FIMC_IS_SETFILE_MASK);
	bool setfile_flag = (mask == ISS_SUB_SCENARIO_UHD_30FPS) ||
			(mask == ISS_SUB_SCENARIO_UHD_30FPS_WDR);

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height > SIZE_FHD) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_UHD) &&
			setfile_flag)
		return 1;
	else
		return 0;
}

/* rear preview FHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_FHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))

		return 1;
	else
		return 0;
}

/* rear preview WHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height > SIZE_FHD) &&
			(device->scp.output.width * device->scp.output.height <= SIZE_WHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* rear preview UHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->scp.output.width * device->scp.output.height > SIZE_WHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* front vt1 */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_FRONT_VT1))
		return 1;
	else
		return 0;
}

/* front vt2 */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT2)
{
	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_FRONT_VT2))
		return 1;
	else
		return 0;
}

/* front recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((device->setfile & FIMC_IS_SETFILE_MASK)  == ISS_SUB_SCENARIO_VIDEO) &&
		(device->scp.output.width < 2560)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((device->setfile & FIMC_IS_SETFILE_MASK) == ISS_SUB_SCENARIO_VIDEO) &&
		(device->scp.output.width >= 2560)
		)
		return 1;
	else
		return 0;
}

/* front preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_STILL_PREVIEW))
		return 1;
	else
		return 0;
}

/* front capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAPTURE)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_PREVIEW) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_PREVIEW))
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_CAMCORDING) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_CAMCORDING_WHD)) &&
		(device->scp.output.width < 2560)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(!(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state))) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_CAMCORDING) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_CAMCORDING_WHD)) &&
		(device->scp.output.width >= 2560)
		)
		return 1;
	else
		return 0;
}

/* front companion recording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state)) &&
		((device->setfile & FIMC_IS_SETFILE_MASK)  == ISS_SUB_SCENARIO_VIDEO) &&
		(device->scp.output.width < 2560)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state)) &&
		((device->setfile & FIMC_IS_SETFILE_MASK) == ISS_SUB_SCENARIO_VIDEO) &&
		(device->scp.output.width >= 2560)
		)
		return 1;
	else
		return 0;
}

/* front companion preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_PREVIEW)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state)) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_STILL_PREVIEW))
		return 1;
	else
		return 0;
}

/* front companion capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAPTURE)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state)) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_COMPANION_PREVIEW) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_COMPANION_PREVIEW))
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state)) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE)) &&
		(device->scp.output.width < 2560)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE)
{
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_FRONT) &&
		(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) &&
		(test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state)) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE)) &&
		(device->scp.output.width >= 2560)
		)
		return 1;
	else
		return 0;
}

/* rear capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_REAR_PREVIEW_FHD) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_REAR_PREVIEW_UHD))
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_REAR_CAMCORDING_FHD) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_REAR_CAMCORDING_UHD)) &&
		(device->scp.output.width < 4096)
		)
		return 1;
	else
		return 0;
}

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE)
{
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl = device->resourcemgr->dvfs_ctrl.static_ctrl;

	if ((fimc_is_sensor_g_postion(device->sensor) == SENSOR_POSITION_REAR) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) &&
		((static_ctrl->cur_scenario_id >= FIMC_IS_SN_REAR_CAMCORDING_FHD) && (static_ctrl->cur_scenario_id <= FIMC_IS_SN_REAR_CAMCORDING_UHD)) &&
		(device->scp.output.width  >= 4096)
		)
		return 1;
	else
		return 0;
}

/* dis */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE)
{
	return 0;
}

#endif

static int fimc_is_set_pwm(struct fimc_is_device_ischain *device, u32 pwm_qos)
{
	int ret = 0;
	u32 base_addr;
	void __iomem *addr;

	/* TODO */
	base_addr = 0;

	if (base_addr) {
		addr = ioremap(base_addr + FIMC_IS_PWM_TCNTB0, SZ_4);
		writel(pwm_qos, addr);
		dbg("PWM SFR Read(%08X), pwm_qos(%08X)\n", readl(addr), pwm_qos);
		iounmap(addr);
	} else {
		merr("fimc_is_set_pwm is not implemented", device);
		BUG();
	}

	return ret;
}

int fimc_is_dvfs_init(struct fimc_is_resourcemgr *resourcemgr)
{
	int i;

	BUG_ON(!resourcemgr);

	resourcemgr->dvfs_ctrl.cur_int_qos = 0;
	resourcemgr->dvfs_ctrl.cur_mif_qos = 0;
	resourcemgr->dvfs_ctrl.cur_cam_qos = 0;
	resourcemgr->dvfs_ctrl.cur_i2c_qos = 0;
	resourcemgr->dvfs_ctrl.cur_disp_qos = 0;

	/* init spin_lock for clock gating */
	mutex_init(&resourcemgr->dvfs_ctrl.lock);

	if (!(resourcemgr->dvfs_ctrl.static_ctrl))
		resourcemgr->dvfs_ctrl.static_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(resourcemgr->dvfs_ctrl.dynamic_ctrl))
		resourcemgr->dvfs_ctrl.dynamic_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);

	if (!resourcemgr->dvfs_ctrl.static_ctrl || !resourcemgr->dvfs_ctrl.dynamic_ctrl) {
		err("dvfs_ctrl alloc is failed!!\n");
		return -ENOMEM;
	}

	/* set priority by order */
	for (i = 0; i < ARRAY_SIZE(static_scenarios); i++)
		static_scenarios[i].priority = i;
	for (i = 0; i < ARRAY_SIZE(dynamic_scenarios); i++)
		dynamic_scenarios[i].priority = i;

	resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_id	= -1;
	resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_idx	= -1;
	resourcemgr->dvfs_ctrl.static_ctrl->scenarios		= static_scenarios;
	if (static_scenarios[0].scenario_id == FIMC_IS_SN_DEFAULT)
		resourcemgr->dvfs_ctrl.static_ctrl->scenario_cnt	= 0;
	else
		resourcemgr->dvfs_ctrl.static_ctrl->scenario_cnt	= ARRAY_SIZE(static_scenarios);

	resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_scenario_id	= -1;
	resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_scenario_idx	= -1;
	resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_frame_tick	= -1;
	resourcemgr->dvfs_ctrl.dynamic_ctrl->scenarios		= dynamic_scenarios;
	if (static_scenarios[0].scenario_id == FIMC_IS_SN_DEFAULT)
		resourcemgr->dvfs_ctrl.dynamic_ctrl->scenario_cnt	= 0;
	else
		resourcemgr->dvfs_ctrl.dynamic_ctrl->scenario_cnt	= ARRAY_SIZE(dynamic_scenarios);

	/* default value is 0 */
	resourcemgr->dvfs_ctrl.dvfs_table_idx = 0;

	return 0;
}

int fimc_is_dvfs_sel_static(struct fimc_is_device_ischain *device)
{
	struct fimc_is_core *core;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl;
	struct fimc_is_dvfs_scenario *scenarios;
	struct fimc_is_resourcemgr *resourcemgr;
	int i, scenario_id, scenario_cnt;

	BUG_ON(!device);
	BUG_ON(!device->interface);

	core = (struct fimc_is_core *)device->interface->core;
	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	static_ctrl = dvfs_ctrl->static_ctrl;

	/* static scenario */
	if (!static_ctrl) {
		err("static_dvfs_ctrl is NULL\n");
		return -EINVAL;
	}

	if (static_ctrl->scenario_cnt == 0) {
		pr_debug("static_scenario's count is zero\n");
		return -EINVAL;
	}

	scenarios = static_ctrl->scenarios;
	scenario_cnt = static_ctrl->scenario_cnt;

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		if ((scenarios[i].check_func(device)) > 0) {
			scenario_id = scenarios[i].scenario_id;
			static_ctrl->cur_scenario_id = scenario_id;
			static_ctrl->cur_scenario_idx = i;
			static_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			return scenario_id;
		}
	}

	warn("couldn't find static dvfs scenario [sensor:(%d/%d)/fps:%d/setfile:%d/scp size:(%d/%d)]\n",
		fimc_is_get_open_sensor_cnt(core),
		device->sensor->pdev->id,
		fimc_is_sensor_g_framerate(device->sensor),
		(device->setfile & FIMC_IS_SETFILE_MASK),
		device->scp.output.width,
		device->scp.output.height);

	static_ctrl->cur_scenario_id = FIMC_IS_SN_DEFAULT;
	static_ctrl->cur_scenario_idx = -1;
	static_ctrl->cur_frame_tick = -1;

	return FIMC_IS_SN_DEFAULT;
}

int fimc_is_dvfs_sel_dynamic(struct fimc_is_device_ischain *device)
{
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *dynamic_ctrl;
	struct fimc_is_dvfs_scenario *scenarios;
	struct fimc_is_resourcemgr *resourcemgr;
	int i, scenario_id, scenario_cnt;

	BUG_ON(!device);

	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;

	/* dynamic scenario */
	if (!dynamic_ctrl) {
		err("dynamic_dvfs_ctrl is NULL\n");
		return -EINVAL;
	}

	if (dynamic_ctrl->scenario_cnt == 0) {
		pr_debug("dynamic_scenario's count is zero\n");
		return -EINVAL;
	}

	scenarios = dynamic_ctrl->scenarios;
	scenario_cnt = dynamic_ctrl->scenario_cnt;

	if (dynamic_ctrl->cur_frame_tick >= 0) {
		(dynamic_ctrl->cur_frame_tick)--;
		/*
		 * when cur_frame_tick is lower than 0, clear current scenario.
		 * This means that current frame tick to keep dynamic scenario
		 * was expired.
		 */
		if (dynamic_ctrl->cur_frame_tick < 0) {
			dynamic_ctrl->cur_scenario_id = -1;
			dynamic_ctrl->cur_scenario_idx = -1;
		}
	}

	if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		return -EAGAIN;

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		if ((scenarios[i].check_func(device)) > 0) {
			scenario_id = scenarios[i].scenario_id;
			dynamic_ctrl->cur_scenario_id = scenario_id;
			dynamic_ctrl->cur_scenario_idx = i;
			dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			return scenario_id;
		}
	}

	return  -EAGAIN;
}

int fimc_is_get_qos(struct fimc_is_core *core, u32 type, u32 scenario_id)
{
	struct exynos_platform_fimc_is	*pdata = NULL;
	int qos = 0;
	int dvfs_idx = core->resourcemgr.dvfs_ctrl.dvfs_table_idx;

	pdata = core->pdata;
	if (pdata == NULL) {
		err("pdata is NULL\n");
		return -EINVAL;
	}

	if (!pdata->get_int_qos || !pdata->get_mif_qos)
		goto struct_qos;

	switch (type) {
		case FIMC_IS_DVFS_INT:
			qos = pdata->get_int_qos(scenario_id);
			break;
		case FIMC_IS_DVFS_MIF:
			qos = pdata->get_mif_qos(scenario_id);
			break;
		case FIMC_IS_DVFS_I2C:
			if (pdata->get_i2c_qos)
				qos = pdata->get_i2c_qos(scenario_id);
			break;
	}
	goto exit;

struct_qos:
	if (max(0, (int)type) >= FIMC_IS_DVFS_END) {
		err("Cannot find DVFS value");
		return -EINVAL;
	}

	if (dvfs_idx < 0 || dvfs_idx >= FIMC_IS_DVFS_TABLE_IDX_MAX) {
		err("invalid dvfs index(%d)", dvfs_idx);
		dvfs_idx = 0;
	}

	qos = pdata->dvfs_data[dvfs_idx][scenario_id][type];

exit:
	return qos;
}

int fimc_is_set_dvfs(struct fimc_is_device_ischain *device, u32 scenario_id)
{
	int ret = 0;
	int int_qos, mif_qos, i2c_qos, cam_qos, disp_qos, pwm_qos = 0;
	int refcount;
	struct fimc_is_core *core;
	struct fimc_is_resourcemgr *resourcemgr;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;

	if (device == NULL) {
		err("device is NULL\n");
		return -EINVAL;
	}

	core = (struct fimc_is_core *)device->interface->core;
	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);

	refcount = atomic_read(&core->video_i0s.refcount);
	if (refcount < 0) {
		err("invalid ischain refcount");
		goto exit;
	}

	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, scenario_id);
	mif_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_MIF, scenario_id);
	i2c_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_I2C, scenario_id);
	cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, scenario_id);
	disp_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_DISP, scenario_id);
	pwm_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_PWM, scenario_id);

	if ((int_qos < 0) || (mif_qos < 0) || (i2c_qos < 0)
	|| (cam_qos < 0) || (disp_qos < 0) || (pwm_qos < 0)) {
		err("getting qos value is failed!!\n");
		return -EINVAL;
	}

	/* check current qos */
	if (int_qos && dvfs_ctrl->cur_int_qos != int_qos) {
		if (i2c_qos) {
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, true);
			if (ret) {
				err("fimc_is_itf_i2_clock fail\n");
				goto exit;
			}
		}

		if (pwm_qos) {
			fimc_is_set_pwm(device, pwm_qos);
			if (ret) {
				err("fimc_is_set_pwm fail\n");
				goto exit;
			}
		}

		pm_qos_update_request(&exynos_isp_qos_int, int_qos);
		dvfs_ctrl->cur_int_qos = int_qos;

		if (i2c_qos) {
			/* i2c unlock */
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, false);
			if (ret) {
				err("fimc_is_itf_i2c_unlock fail\n");
				goto exit;
			}
		}
	}

	if (mif_qos && dvfs_ctrl->cur_mif_qos != mif_qos) {
		pm_qos_update_request(&exynos_isp_qos_mem, mif_qos);
		dvfs_ctrl->cur_mif_qos = mif_qos;
	}

	if (cam_qos && dvfs_ctrl->cur_cam_qos != cam_qos) {
		pm_qos_update_request(&exynos_isp_qos_cam, cam_qos);
		dvfs_ctrl->cur_cam_qos = cam_qos;
	}

	pr_info("[RSC:%d]: New QoS [INT(%d), MIF(%d), CAM(%d), DISP(%d), I2C(%d), PWM(%d)]\n",
			device->instance, int_qos, mif_qos,
			cam_qos, disp_qos, i2c_qos, pwm_qos);
exit:
	return ret;
}
