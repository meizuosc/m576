/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SUBDEV_H
#define FIMC_IS_SUBDEV_H

#include "fimc-is-param.h"
#include "fimc-is-video.h"

struct fimc_is_device_ischain;

enum fimc_is_subdev_state {
	FIMC_IS_SUBDEV_OPEN,
	FIMC_IS_SUBDEV_START,
	FIMC_IS_SUBDEV_RUN,
	FIMC_IS_SUBDEV_FORCE_SET
};

struct fimc_is_subdev_path {
	u32					width;
	u32					height;
	struct fimc_is_crop			crop;
};

enum fimc_is_subdev_id {
	ENTRY_3AA,
	ENTRY_3AC,
	ENTRY_3AP,
	ENTRY_ISP,
	ENTRY_IXC,
	ENTRY_IXP,
	ENTRY_DRC,
	ENTRY_DIS,
	ENTRY_ODC,
	ENTRY_DNR,
	ENTRY_SCC,
	ENTRY_SCP,
	ENTRY_VRA,
	ENTRY_END
};

struct fimc_is_subdev {
	u32					id;
	u32					vid; /* video id */
	u32					cid; /* capture node id */
	char					name[4];
	u32					instance;
	unsigned long				state;

	struct fimc_is_subdev_path		input;
	struct fimc_is_subdev_path		output;

	struct fimc_is_video_ctx		*vctx;
	struct fimc_is_subdev			*leader;
};

int fimc_is_ischain_subdev_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_ischain_subdev_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);

/*common subdev*/
int fimc_is_subdev_probe(struct fimc_is_subdev *subdev,
	u32 instance,
	u32 id,
	char *name);
int fimc_is_subdev_open(struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx,
	const struct param_control *init_ctl);
int fimc_is_subdev_close(struct fimc_is_subdev *subdev);
int fimc_is_subdev_buffer_queue(struct fimc_is_subdev *subdev, u32 index);
int fimc_is_subdev_buffer_finish(struct fimc_is_subdev *subdev, u32 index);

void fimc_is_subdev_dis_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dis_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dis_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
void fimc_is_subdev_dnr_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dnr_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dnr_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
void fimc_is_subdev_drc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_drc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_drc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
void fimc_is_subdev_odc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_odc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_odc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);

struct fimc_is_subdev * video2subdev(struct fimc_is_device_ischain *device,
	u32 vid);

#define GET_SUBDEV_FRAMEMGR(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->queue.framemgr) : NULL)
#define GET_SUBDEV_QUEUE(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->queue) : NULL)

#endif
