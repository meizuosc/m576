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

 #include <linux/module.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-device-ischain.h"

struct fimc_is_subdev * video2subdev(struct fimc_is_device_ischain *device,
	u32 vid)
{
	struct fimc_is_subdev *subdev = NULL;

	switch (vid) {
	case FIMC_IS_VIDEO_30S_NUM:
	case FIMC_IS_VIDEO_31S_NUM:
		subdev = &device->group_3aa.leader;
		break;
	case FIMC_IS_VIDEO_30C_NUM:
	case FIMC_IS_VIDEO_31C_NUM:
		subdev = &device->txc;
		break;
	case FIMC_IS_VIDEO_30P_NUM:
	case FIMC_IS_VIDEO_31P_NUM:
		subdev = &device->txp;
		break;
	case FIMC_IS_VIDEO_I0S_NUM:
	case FIMC_IS_VIDEO_I1S_NUM:
		subdev = &device->group_isp.leader;
		break;
	case FIMC_IS_VIDEO_I0C_NUM:
	case FIMC_IS_VIDEO_I1C_NUM:
		subdev = &device->ixc;
		break;
	case FIMC_IS_VIDEO_I0P_NUM:
	case FIMC_IS_VIDEO_I1P_NUM:
		subdev = &device->ixp;
		break;
	case FIMC_IS_VIDEO_DIS_NUM:
		subdev = &device->group_dis.leader;
		break;
	case FIMC_IS_VIDEO_SCC_NUM:
		subdev = &device->scc;
		break;
	case FIMC_IS_VIDEO_SCP_NUM:
		subdev = &device->scp;
		break;
	case FIMC_IS_VIDEO_VRA_NUM:
		subdev = &device->vra;
		break;
	default:
		merr("vid %d is NOT found", device, vid);
		break;
	}

	return subdev;
}

int fimc_is_subdev_probe(struct fimc_is_subdev *subdev,
	u32 instance,
	u32 id,
	char *name)
{
	BUG_ON(!subdev);
	BUG_ON(!name);

	subdev->id = id;
	subdev->instance = instance;
	memset(subdev->name, 0x0, sizeof(subdev->name));
	strncpy(subdev->name, name, sizeof(char[3]));
	clear_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return 0;
}

int fimc_is_subdev_open(struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx,
	const struct param_control *init_ctl)
{
	int ret = 0;

	BUG_ON(!subdev);

	if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("already open", subdev, subdev);
		ret = -EPERM;
		goto p_err;
	}

	subdev->vctx = vctx;
	subdev->vid = (vctx && GET_VIDEO(vctx)) ? GET_VIDEO(vctx)->id : 0;
	subdev->cid = CAPTURE_NODE_MAX;
	subdev->input.width = 0;
	subdev->input.height = 0;
	subdev->input.crop.x = 0;
	subdev->input.crop.y = 0;
	subdev->input.crop.w = 0;
	subdev->input.crop.h = 0;
	subdev->output.width = 0;
	subdev->output.height = 0;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = 0;
	subdev->output.crop.h = 0;

	if (init_ctl) {
		set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

		if (subdev->id == ENTRY_VRA) {
			/* vra only control by command for enabling or disabling */
			if (init_ctl->cmd == CONTROL_COMMAND_STOP)
				clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
		} else {
			if (init_ctl->bypass == CONTROL_BYPASS_ENABLE)
				clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
		}
	} else {
		clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);
		clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	}

	set_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return ret;
}

int fimc_is_ischain_subdev_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));

	subdev = video2subdev(device, GET_VIDEO(vctx)->id);
	if (!subdev) {
		merr("video2subdev is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = subdev;

	ret = fimc_is_subdev_open(subdev, vctx, NULL);
	if (ret) {
		merr("fimc_is_subdev_open is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_open_wrap(device, false);
	if (ret) {
		merr("fimc_is_ischain_open_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_subdev_close(struct fimc_is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("subdev is already close", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	subdev->leader = NULL;
	subdev->vctx = NULL;
	subdev->vid = 0;

	clear_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return 0;
}

int fimc_is_ischain_subdev_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	ret = fimc_is_subdev_close(subdev);
	if (ret) {
		merr("fimc_is_subdev_close is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_close_wrap(device);
	if (ret) {
		merr("fimc_is_ischain_close_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_subdev_start(struct fimc_is_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_subdev *leader;

	BUG_ON(!subdev);
	BUG_ON(!subdev->leader);

	leader = subdev->leader;

	if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		merr("already start", subdev);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		merr("leader%d is ALREADY started", subdev, leader->id);
		goto p_err;
	}

	set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_subdev_start(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_INIT, &device->state)) {
		mserr("device is not yet init", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_subdev_start(subdev);
	if (ret) {
		mserr("fimc_is_subdev_start is fail(%d)", device, subdev, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_subdev_stop(struct fimc_is_subdev *subdev)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_subdev *leader;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);
	BUG_ON(!subdev->leader);
	BUG_ON(!GET_SUBDEV_FRAMEMGR(subdev));

	leader = subdev->leader;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);

	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		merr("already stop", subdev);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		merr("leader%d is NOT stopped", subdev, leader->id);
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	if (framemgr->frame_pro_cnt > 0) {
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);
		merr("being processed, can't stop", subdev);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_frame_complete_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_com_to_fre(framemgr, frame);
		fimc_is_frame_complete_head(framemgr, &frame);
	}

	fimc_is_frame_request_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_req_to_fre(framemgr, frame);
		fimc_is_frame_request_head(framemgr, &frame);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_subdev_stop(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_subdev_stop(subdev);
	if (ret) {
		merr("fimc_is_subdev_stop is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_subdev_s_format(struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	u32 pixelformat, width, height;

	BUG_ON(!subdev);
	BUG_ON(!queue);

	pixelformat = queue->framecfg.format.pixelformat;
	width = queue->framecfg.width;
	height = queue->framecfg.height;

	switch (subdev->id) {
	case ENTRY_SCC:
	case ENTRY_SCP:
		switch(pixelformat) {
		/*
		 * YUV422 1P, YUV422 2P : x8
		 * YUV422 3P : x16
		 */
		case V4L2_PIX_FMT_YUV422P:
			if (width % 8) {
				merr("width(%d) of format(%d) is not supported size",
					subdev, width, pixelformat);
				ret = -EINVAL;
				goto p_err;
			}
			break;
		/*
		 * YUV420 2P : x8
		 * YUV420 3P : x16
		 */
		case V4L2_PIX_FMT_NV12M:
		case V4L2_PIX_FMT_NV21M:
			if (width % 8) {
				merr("width(%d) of format(%d) is not supported size",
					subdev, width, pixelformat);
				ret = -EINVAL;
				goto p_err;
			}
			break;
		case V4L2_PIX_FMT_YUV420M:
		case V4L2_PIX_FMT_YVU420M:
			if (width % 16) {
				merr("width(%d) of format(%d) is not supported size",
					subdev, width, pixelformat);
				ret = -EINVAL;
				goto p_err;
			}
			break;
		default:
			merr("format(%d) is not supported", subdev, pixelformat);
			ret = -EINVAL;
			goto p_err;
			break;
		}
		break;
	default:
		break;
	}

	subdev->output.width = width;
	subdev->output.height = height;

	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;

p_err:
	return ret;
}

static int fimc_is_ischain_subdev_s_format(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_subdev_s_format(subdev, queue);
	if (ret) {
		merr("fimc_is_subdev_s_format is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_subdev_buffer_queue(struct fimc_is_subdev *subdev,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_FRAMEMGR(subdev));
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);

	/* 1. check frame validation */
	frame = &framemgr->frame[index];
	if (!frame) {
		mserr("frame is null\n", subdev, subdev);
		ret = EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_INI_MEM, &frame->memory))) {
		mserr("frame %d is NOT init", subdev, subdev, index);
		ret = EINVAL;
		goto p_err;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_FREE) {
		if (frame->req_flag) {
			mswarn("frame %d done is not generated\n", subdev, subdev, frame->index);
			frame->req_flag = 0;
		}

		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	} else {
		mserr("frame %d is invalid state(%d)\n", subdev, subdev, index, frame->state);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

p_err:
	return ret;
}

int fimc_is_subdev_buffer_finish(struct fimc_is_subdev *subdev,
	u32 index)
{
	int ret = 0;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_FRAMEMGR(subdev));
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);

	framemgr_e_barrier_irq(framemgr, FMGR_IDX_18);

	fimc_is_frame_complete_head(framemgr, &frame);
	if (frame) {
		if (frame->index == index) {
			fimc_is_frame_trans_com_to_fre(framemgr, frame);
		} else {
			merr("buffer index is NOT matched(%d != %d)\n", subdev, index, frame->index);
			fimc_is_frame_print_all(framemgr);
			ret = -EINVAL;
		}
	} else {
		merr("frame is empty from complete", subdev);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_subdev_ops = {
	.start_streaming	= fimc_is_ischain_subdev_start,
	.stop_streaming		= fimc_is_ischain_subdev_stop,
	.s_format		= fimc_is_ischain_subdev_s_format
};

void fimc_is_subdev_dis_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dis start */
}

void fimc_is_subdev_dis_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dis stop */
}

void fimc_is_subdev_dis_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_tpu_config *config_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	config_param = fimc_is_itf_g_param(device, frame, PARAM_TPU_CONFIG);
	config_param->dis_bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_TPU_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_TPU_CONFIG);
	(*indexes)++;
}

void fimc_is_subdev_dnr_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dnr start */
}

void fimc_is_subdev_dnr_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dnr stop */
}

void fimc_is_subdev_dnr_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_tpu_config *config_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	config_param = fimc_is_itf_g_param(device, frame, PARAM_TPU_CONFIG);
	config_param->tdnr_bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_TPU_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_TPU_CONFIG);
	(*indexes)++;
}

void fimc_is_subdev_drc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	struct param_control *ctl_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_DRC_CONTROL);
	ctl_param->cmd = CONTROL_COMMAND_START;
	ctl_param->bypass = CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_drc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	struct param_control *ctl_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_DRC_CONTROL);
	ctl_param->cmd = CONTROL_COMMAND_STOP;
	ctl_param->bypass = CONTROL_BYPASS_ENABLE;
	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_drc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_control *ctl_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_DRC_CONTROL);
	ctl_param->cmd = CONTROL_COMMAND_START;
	ctl_param->bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_odc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for odc start */
}

void fimc_is_subdev_odc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for odc stop */
}

void fimc_is_subdev_odc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_tpu_config *config_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	config_param = fimc_is_itf_g_param(device, frame, PARAM_TPU_CONFIG);
	config_param->odc_bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_TPU_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_TPU_CONFIG);
	(*indexes)++;
}
