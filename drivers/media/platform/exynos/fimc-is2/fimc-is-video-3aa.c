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
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>
#include <media/exynos_mc.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-metadata.h"

const struct v4l2_file_operations fimc_is_3aa_video_fops;
const struct v4l2_ioctl_ops fimc_is_3aa_video_ioctl_ops;
const struct vb2_ops fimc_is_3aa_qops;

#if defined(DEBUG_LOG_LEVEL_CHANGE)
#define FIMC_IS_DEFAULT_CONSOLE_LEVEL 7
static int previous_console_level = 0xff;
#endif

int fimc_is_30s_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_video *video;

	BUG_ON(!data);

	core = (struct fimc_is_core *)data;
	video = &core->video_30s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		FIMC_IS_VIDEO_3XS_NAME(0),
		FIMC_IS_VIDEO_30S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		&fimc_is_3aa_video_fops,
		&fimc_is_3aa_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int fimc_is_31s_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_video *video;

	BUG_ON(!data);

	core = (struct fimc_is_core *)data;
	video = &core->video_31s;
	video->resourcemgr = &core->resourcemgr;

	if (!core->pdev) {
		probe_err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		FIMC_IS_VIDEO_3XS_NAME(1),
		FIMC_IS_VIDEO_31S_NUM,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		&fimc_is_3aa_video_fops,
		&fimc_is_3aa_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/*
 * =============================================================================
 * Video File Opertation
 * =============================================================================
 */

static int fimc_is_3aa_video_open(struct file *file)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_ischain *device;
	struct fimc_is_resourcemgr *resourcemgr;

	vctx = NULL;
	device = NULL;
	video = video_drvdata(file);
	resourcemgr = video->resourcemgr;
	if (!resourcemgr) {
		err("resourcemgr is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	#if defined(DEBUG_LOG_LEVEL_CHANGE)
	if (console_loglevel > FIMC_IS_DEFAULT_CONSOLE_LEVEL) {
		previous_console_level = console_loglevel;
		console_loglevel = FIMC_IS_DEFAULT_CONSOLE_LEVEL;
	}
	#endif

	ret = fimc_is_resource_open(resourcemgr, RESOURCE_TYPE_ISCHAIN, (void **)&device);
	if (ret) {
		err("fimc_is_resource_open is fail(%d)", ret);
		goto p_err;
	}

	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	minfo("[3%dS:V] %s\n", device, GET_3XS_ID(video), __func__);

	ret = open_vctx(file, video, &vctx, device->instance, FRAMEMGR_ID_3XS);
	if (ret) {
		merr("open_vctx is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_video_open(vctx,
		device,
		VIDEO_3XS_READY_BUFFERS,
		video,
		&fimc_is_3aa_qops,
		&fimc_is_ischain_3aa_ops);
	if (ret) {
		merr("fimc_is_video_open is fail(%d)", device, ret);
		close_vctx(file, video, vctx);
		goto p_err;
	}

	ret = fimc_is_ischain_3aa_open(device, vctx);
	if (ret) {
		merr("fimc_is_ischain_3aa_open is fail(%d)", device, ret);
		close_vctx(file, video, vctx);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_3aa_video_close(struct file *file)
{
	int ret = 0;
	int refcount;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_video *video;
	struct fimc_is_device_ischain *device;

	BUG_ON(!file);
	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!GET_DEVICE(vctx));

	video = GET_VIDEO(vctx);
	device = GET_DEVICE(vctx);

	#if defined(DEBUG_LOG_LEVEL_CHANGE)
	if (previous_console_level != 0xff) {
		console_loglevel = previous_console_level;
		previous_console_level = 0xff;
	}
	#endif

	ret = fimc_is_ischain_3aa_close(device, vctx);
	if (ret)
		merr("fimc_is_ischain_3aa_close is fail(%d)", device, ret);

	ret = fimc_is_video_close(vctx);
	if (ret)
		merr("fimc_is_video_close is fail(%d)", device, ret);

	refcount = close_vctx(file, video, vctx);
	if (refcount < 0)
		merr("close_vctx is fail(%d)", device, refcount);

	minfo("[3%dS:V] %s(%d,%d):%d\n", device, GET_3XS_ID(video), __func__, atomic_read(&device->open_cnt), refcount, ret);

	return ret;
}

static unsigned int fimc_is_3aa_video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_poll(file, vctx, wait);
	if (ret)
		merr("fimc_is_video_poll is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_3aa_video_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_mmap(file, vctx, vma);
	if (ret)
		merr("fimc_is_video_mmap is fail(%d)", vctx, ret);

	return ret;
}

const struct v4l2_file_operations fimc_is_3aa_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_3aa_video_open,
	.release	= fimc_is_3aa_video_close,
	.poll		= fimc_is_3aa_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fimc_is_3aa_video_mmap,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int fimc_is_3aa_video_querycap(struct file *file, void *fh,
	struct v4l2_capability *cap)
{
	/* Todo : add to query capability code */
	return 0;
}

static int fimc_is_3aa_video_enum_fmt_mplane(struct file *file, void *priv,
	struct v4l2_fmtdesc *f)
{
	/* Todo : add to enumerate format code */
	return 0;
}

static int fimc_is_3aa_video_get_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	/* Todo : add to get format code */
	return 0;
}

static int fimc_is_3aa_video_set_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	BUG_ON(!vctx);
	BUG_ON(!format);

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = fimc_is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("fimc_is_video_set_format_mplane is fail(%d)", vctx, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_3aa_video_cropcap(struct file *file, void *fh,
	struct v4l2_cropcap *cropcap)
{
	/* Todo : add to crop capability code */
	return 0;
}

static int fimc_is_3aa_video_get_crop(struct file *file, void *fh,
	struct v4l2_crop *crop)
{
	/* Todo : add to get crop control code */
	return 0;
}

static int fimc_is_3aa_video_set_crop(struct file *file, void *fh,
	struct v4l2_crop *crop)
{
	/* Todo : add to set crop control code */
	return 0;
}

static int fimc_is_3aa_video_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);

	mdbgv_3aa("%s(buffers : %d)\n", vctx, __func__, buf->count);

	device = vctx->device;
	if (!device) {
		merr("device is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
		ret = fimc_is_ischain_3aa_reqbufs(device, buf->count);
		if (ret) {
			merr("3a0_reqbufs is fail(%d)", vctx, ret);
			goto p_err;
		}
	}

	ret = fimc_is_video_reqbufs(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_reqbufs is fail(%d)", vctx, ret);

p_err:
	return ret;
}

static int fimc_is_3aa_video_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = fimc_is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_querybuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_3aa_video_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	BUG_ON(!vctx);

#ifdef DBG_STREAMING
	mdbgv_3aa("%s(%02d:%d)\n", vctx, __func__, buf->type, buf->index);
#endif

	ret = fimc_is_video_qbuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_qbuf(type %d) is fail(%d)", vctx, buf->type, ret);

	return ret;
}

static int fimc_is_3aa_video_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	BUG_ON(!vctx);

#ifdef DBG_STREAMING
	mdbgv_3aa("%s\n", vctx, __func__);
#endif

	ret = fimc_is_video_dqbuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_dqbuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_3aa_video_streamon(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = fimc_is_video_streamon(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamon is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_3aa_video_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_3aa("%s\n", vctx, __func__);

	ret = fimc_is_video_streamoff(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamoff is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_3aa_video_enum_input(struct file *file, void *priv,
	struct v4l2_input *input)
{
	/* Todo: add enum input control code */
	return 0;
}

static int fimc_is_3aa_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	/* Todo: add to get input control code */
	return 0;
}

static int fimc_is_3aa_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	u32 stream, module, vindex, memory, leader;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);
	BUG_ON(!vctx->device);

	mdbgv_3aa("%s(input : %08X)\n", vctx, __func__, input);

	device = GET_DEVICE(vctx);
	stream = (input & INPUT_STREAM_MASK) >> INPUT_STREAM_SHIFT;
	module = (input & INPUT_MODULE_MASK) >> INPUT_MODULE_SHIFT;
	vindex = (input & INPUT_VINDEX_MASK) >> INPUT_VINDEX_SHIFT;
	memory = (input & INPUT_MEMORY_MASK) >> INPUT_MEMORY_SHIFT;
	leader = (input & INPUT_LEADER_MASK) >> INPUT_LEADER_SHIFT;

	ret = fimc_is_video_s_input(file, vctx);
	if (ret) {
		merr("fimc_is_video_s_input is fail(%d)", vctx, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_3aa_s_input(device, stream, module, vindex, memory, leader);
	if (ret) {
		merr("fimc_is_ischain_3aa_s_input is fail(%d)", vctx, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_3aa_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));
	BUG_ON(!ctrl);

	mdbgv_3aa("%s\n", vctx, __func__);

	device = GET_DEVICE(vctx);

	switch (ctrl->id) {
	case V4L2_CID_IS_INTENT:
		device->group_3aa.intent_ctl.aa.captureIntent = ctrl->value;
		minfo("[3AA:V] s_ctrl intent(%d)\n", vctx, ctrl->value);
		break;
	case V4L2_CID_IS_FORCE_DONE:
		set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &device->group_3aa.state);
		break;
	case V4L2_CID_IS_CAMERA_TYPE:
		switch (ctrl->value) {
			case 0:
				/* change value to X when !TWIZ | front */
				fimc_is_itf_set_fwboot(device, COLD_BOOT);
				break;
			case 1:
				/* change value to X when TWIZ & back | frist time back camera */
				if(!device->interface->first_launch) {
					fimc_is_itf_set_fwboot(device, FIRST_LAUNCHING);
					device->interface->first_launch = true;
				} else {
					fimc_is_itf_set_fwboot(device, WARM_BOOT);
				}
				break;
			default:
				err("unsupported ioctl(0x%X)", ctrl->id);
				ret = -EINVAL;
				break;
		}
		break;
	default:
		ret = fimc_is_video_s_ctrl(file, vctx, ctrl);
		if (ret) {
			merr("fimc_is_video_s_ctrl is fail(%d)", device, ret);
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int fimc_is_3aa_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	/* Todo: add to get control code */
	return 0;
}

static int fimc_is_3aa_video_g_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	/* Todo: add to get extra control code */
	return 0;
}

const struct v4l2_ioctl_ops fimc_is_3aa_video_ioctl_ops = {
	.vidioc_querycap		= fimc_is_3aa_video_querycap,

	.vidioc_enum_fmt_vid_out_mplane	= fimc_is_3aa_video_enum_fmt_mplane,
	.vidioc_enum_fmt_vid_cap_mplane	= fimc_is_3aa_video_enum_fmt_mplane,

	.vidioc_g_fmt_vid_out_mplane	= fimc_is_3aa_video_get_format_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= fimc_is_3aa_video_get_format_mplane,

	.vidioc_s_fmt_vid_out_mplane	= fimc_is_3aa_video_set_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= fimc_is_3aa_video_set_format_mplane,

	.vidioc_querybuf		= fimc_is_3aa_video_querybuf,
	.vidioc_reqbufs			= fimc_is_3aa_video_reqbufs,

	.vidioc_qbuf			= fimc_is_3aa_video_qbuf,
	.vidioc_dqbuf			= fimc_is_3aa_video_dqbuf,

	.vidioc_streamon		= fimc_is_3aa_video_streamon,
	.vidioc_streamoff		= fimc_is_3aa_video_streamoff,

	.vidioc_enum_input		= fimc_is_3aa_video_enum_input,
	.vidioc_g_input			= fimc_is_3aa_video_g_input,
	.vidioc_s_input			= fimc_is_3aa_video_s_input,

	.vidioc_s_ctrl			= fimc_is_3aa_video_s_ctrl,
	.vidioc_g_ctrl			= fimc_is_3aa_video_g_ctrl,
	.vidioc_g_ext_ctrls		= fimc_is_3aa_video_g_ext_ctrl,

	.vidioc_cropcap			= fimc_is_3aa_video_cropcap,
	.vidioc_g_crop			= fimc_is_3aa_video_get_crop,
	.vidioc_s_crop			= fimc_is_3aa_video_set_crop,
};

static int fimc_is_3aa_queue_setup(struct vb2_queue *vbq,
	const struct v4l2_format *fmt,
	unsigned int *num_buffers,
	unsigned int *num_planes,
	unsigned int sizes[],
	void *allocators[])
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!vctx->video);

	mdbgv_3aa("%s\n", vctx, __func__);

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		allocators);
	if (ret)
		merr("fimc_is_queue_setup is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_3aa_buffer_prepare(struct vb2_buffer *vb)
{
	return fimc_is_queue_prepare(vb);
}

static inline void fimc_is_3aa_wait_prepare(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_prepare(vbq);
}

static inline void fimc_is_3aa_wait_finish(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_finish(vbq);
}

static int fimc_is_3aa_start_streaming(struct vb2_queue *vbq,
	unsigned int count)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));

	mdbgv_3aa("%s\n", vctx, __func__);

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_start_streaming(queue, device);
	if (ret) {
		merr("fimc_is_queue_start_streaming is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_3aa_stop_streaming(struct vb2_queue *vbq)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));

	mdbgv_3aa("%s\n", vctx, __func__);

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_stop_streaming(queue, device);
	if (ret) {
		merr("fimc_is_queue_stop_streaming is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static void fimc_is_3aa_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_device_ischain *device;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));
	BUG_ON(!GET_VIDEO(vctx));

#ifdef DBG_STREAMING
	mdbgv_3aa("%s(%d)\n", vctx, __func__, vb->v4l2_buf.index);
#endif

	device = GET_DEVICE(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_buffer_queue(queue, video->vb2, vb);
	if (ret) {
		merr("fimc_is_queue_buffer_queue is fail(%d)", device, ret);
		return;
	}

	ret = fimc_is_ischain_3aa_buffer_queue(device, queue, vb->v4l2_buf.index);
	if (ret) {
		merr("fimc_is_ischain_3aa_buffer_queue is fail(%d)", device, ret);
		return;
	}
}

static int fimc_is_3aa_buffer_finish(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));

#ifdef DBG_STREAMING
	mdbgv_3aa("%s(%d)\n", vctx, __func__, vb->v4l2_buf.index);
#endif

	device = GET_DEVICE(vctx);

	ret = fimc_is_ischain_3aa_buffer_finish(device, vb->v4l2_buf.index);
	if (ret) {
		merr("fimc_is_ischain_3aa_buffer_finish is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct vb2_ops fimc_is_3aa_qops = {
	.queue_setup		= fimc_is_3aa_queue_setup,
	.buf_prepare		= fimc_is_3aa_buffer_prepare,
	.buf_queue		= fimc_is_3aa_buffer_queue,
	.buf_finish		= fimc_is_3aa_buffer_finish,
	.wait_prepare		= fimc_is_3aa_wait_prepare,
	.wait_finish		= fimc_is_3aa_wait_finish,
	.start_streaming	= fimc_is_3aa_start_streaming,
	.stop_streaming		= fimc_is_3aa_stop_streaming,
};

