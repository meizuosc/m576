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

int fimc_is_3a0_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_video *video;

	BUG_ON(!data);

	core = (struct fimc_is_core *)data;
	video = &core->video_3a0;

	if (!core->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		FIMC_IS_VIDEO_3AA_NAME(0),
		FIMC_IS_VIDEO_3A0_NUM,
		VFL_DIR_M2M,
		&core->mem,
		&core->v4l2_dev,
		&video->lock,
		&fimc_is_3aa_video_fops,
		&fimc_is_3aa_video_ioctl_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

int fimc_is_3a1_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_video *video;

	BUG_ON(!data);

	core = (struct fimc_is_core *)data;
	video = &core->video_3a1;

	if (!core->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		FIMC_IS_VIDEO_3AA_NAME(1),
		FIMC_IS_VIDEO_3A1_NUM,
		VFL_DIR_M2M,
		&core->mem,
		&core->v4l2_dev,
		&video->lock,
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
	u32 refcount;
	struct fimc_is_core *core;
	struct fimc_is_video *video;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_ischain *device;

	vctx = NULL;
	video = video_drvdata(file);

	if (video->id == FIMC_IS_VIDEO_3A0_NUM)
		core = container_of(video, struct fimc_is_core, video_3a0);
	else
		core = container_of(video, struct fimc_is_core, video_3a1);

	ret = open_vctx(file, video, &vctx, FRAMEMGR_ID_3AA_GRP, FRAMEMGR_ID_3AAP);
	if (ret) {
		err("open_vctx is fail(%d)", ret);
		goto p_err;
	}

	info("[3A%d:V:%d] %s\n", GET_3AA_ID(video), vctx->instance, __func__);

	refcount = atomic_read(&core->video_isp.refcount);
	if (refcount > FIMC_IS_MAX_NODES || refcount < 1) {
		err("invalid ischain refcount(%d)", refcount);
		close_vctx(file, video, vctx);
		ret = -EINVAL;
		goto p_err;
	}

	device = &core->ischain[refcount - 1];
	info("[3A%d:V:%d] <-> [ISP:V:%d] \n", GET_3AA_ID(video),
		vctx->instance, device->instance);

	ret = fimc_is_video_open(vctx,
		device,
		VIDEO_3AA_READY_BUFFERS,
		video,
		FIMC_IS_VIDEO_TYPE_M2M,
		&fimc_is_3aa_qops,
		&fimc_is_ischain_3aa_ops,
		&fimc_is_ischain_sub_ops);
	if (ret) {
		err("fimc_is_video_open is fail");
		close_vctx(file, video, vctx);
		goto p_err;
	}

	ret = fimc_is_ischain_3aa_open(device, vctx);
	if (ret) {
		err("fimc_is_ischain_3aa_open is fail");
		close_vctx(file, video, vctx);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_3aa_video_close(struct file *file)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = NULL;
	struct fimc_is_video *video = NULL;
	struct fimc_is_device_ischain *device = NULL;

	BUG_ON(!file);

	vctx = file->private_data;
	if (!vctx) {
		err("vctx is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video = vctx->video;
	if (!video) {
		err("video is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	info("[3A%d:V:%d] %s\n", GET_3AA_ID(video), vctx->instance, __func__);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_ischain_3aa_close(device, vctx);
	fimc_is_video_close(vctx);

	ret = close_vctx(file, video, vctx);
	if (ret < 0)
		err("close_vctx is fail(%d)", ret);

p_err:
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
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!vctx->device);

	mdbgv_3aa("%s\n", vctx, __func__);

	device = vctx->device;
	leader = &device->group_3aa.leader;

	ret = fimc_is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("fimc_is_video_set_format_mplane is fail(%d)", vctx, ret);
		goto p_err;
	}

	if (V4L2_TYPE_IS_OUTPUT(format->type)) {
		queue = &vctx->q_src;
		fimc_is_ischain_3aa_s_format(device,
			queue->framecfg.width,
			queue->framecfg.height);
	} else {
		queue = &vctx->q_dst;
		fimc_is_subdev_s_format(leader,
			queue->framecfg.width,
			queue->framecfg.height);
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
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_group *group;
	struct fimc_is_subdev *subdev;

	BUG_ON(!vctx);

	mdbgv_3aa("%s\n", vctx, __func__);

	ischain = vctx->device;
	BUG_ON(!ischain);
	group = &ischain->group_3aa;
	BUG_ON(!group);
	subdev = &group->leader;

	if (V4L2_TYPE_IS_OUTPUT(crop->type))
		fimc_is_ischain_3aa_s_format(ischain,
			crop->c.width, crop->c.height);
	else
		fimc_is_subdev_s_format(subdev,
			crop->c.width, crop->c.height);

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
	/* HACK : this log is commented until timeout issue fixed */
	/* if (ret)
		merr("fimc_is_video_dqbuf is fail(%d)", vctx, ret);
	*/
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
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_ischain *device;

	BUG_ON(!vctx);
	BUG_ON(!vctx->device);

	mdbgv_3aa("%s(input : %08X)\n", vctx, __func__, input);

	device = vctx->device;

	ret = fimc_is_ischain_3aa_s_input(device, input);
	if (ret) {
		merr("fimc_is_ischain_3aa_s_input is fail", vctx);
		goto p_err;
	}

	ret = fimc_is_ischain_init_wrap(device, input);
	if (ret) {
		merr("fimc_is_device_init(%d) is fail", vctx, input);
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

	dbg_isp("%s\n", __func__);

	device = vctx->device;
	if (!device) {
		merr("device is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_INTENT:
		device->group_3aa.intent_ctl.aa.captureIntent = ctrl->value;
		minfo("[3AA:V] s_ctrl intent(%d)\n", vctx, ctrl->value);
		break;
	case V4L2_CID_IS_FORCE_DONE:
		set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &device->group_3aa.state);
		break;
	case V4L2_CID_IS_MAP_BUFFER:
		{
			/* hack for 64bit addr */
			ulong value_to_addr;
			struct fimc_is_queue *queue;
			struct fimc_is_framemgr *framemgr;
			struct fimc_is_frame *frame;
			struct dma_buf *dmabuf;
			struct dma_buf_attachment *attachment;
			dma_addr_t dva;
			struct v4l2_buffer *buf = NULL;
			struct v4l2_plane *planes;
			size_t size;
			u32 write, plane, group_id;

			size = sizeof(struct v4l2_buffer);
			buf = kmalloc(size, GFP_KERNEL);
			if (!buf) {
				merr("kmalloc is fail", vctx);
				ret = -EINVAL;
				goto p_err;
			}
			/* hack for 64bit addr */
			value_to_addr = ctrl->value;

			ret = copy_from_user(buf, (void __user *)value_to_addr, size);
			if (ret) {
				merr("copy_from_user is fail(%d)", vctx, ret);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			if (!V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
				merr("single plane is not supported", vctx);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			if (buf->index >= FRAMEMGR_MAX_REQUEST) {
				merr("buffer index is invalid(%d)", vctx, buf->index);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			if (buf->length > VIDEO_MAX_PLANES) {
				merr("planes[%d] is invalid", vctx, buf->length);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			queue = GET_QUEUE(vctx, buf->type);
			if (queue->vbq->memory != V4L2_MEMORY_DMABUF) {
				merr("memory type(%d) is not supported", vctx, queue->vbq->memory);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			size = sizeof(struct v4l2_plane) * buf->length;
			planes = kmalloc(size, GFP_KERNEL);
			if (IS_ERR(planes)) {
				merr("kmalloc is fail(%p)", vctx, planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			ret = copy_from_user(planes, (void __user *)buf->m.planes, size);
			if (ret) {
				merr("copy_from_user is fail(%d)", vctx, ret);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			framemgr = &queue->framemgr;
			frame = &framemgr->frame[buf->index];
			if (test_bit(FRAME_MAP_MEM, &frame->memory)) {
				merr("this buffer(%d) is already mapped", vctx, buf->index);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			/* only last buffer need to map */
			if (buf->length > 0) {
				plane = buf->length - 1;
			} else {
				merr("buf size is abnormal(%d)", vctx, buf->length);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}
			dmabuf = dma_buf_get(planes[plane].m.fd);
			if (IS_ERR(dmabuf)) {
				merr("dma_buf_get is fail(%p)", vctx, dmabuf);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			attachment = dma_buf_attach(dmabuf, &device->pdev->dev);
			if (IS_ERR(attachment)) {
				merr("dma_buf_attach is fail(%p)", vctx, attachment);
				kfree(planes);
				kfree(buf);
				dma_buf_put(dmabuf);
				ret = -EINVAL;
				goto p_err;
			}

			write = !V4L2_TYPE_IS_OUTPUT(buf->type);
			dva = ion_iovmm_map(attachment, 0, dmabuf->size, write, plane);
			if (IS_ERR_VALUE(dva)) {
				merr("ion_iovmm_map is fail(%pa)", vctx, &dva);
				kfree(planes);
				kfree(buf);
				dma_buf_detach(dmabuf, attachment);
				dma_buf_put(dmabuf);
				ret = -EINVAL;
				goto p_err;
			}

			group_id = GROUP_ID(device->group_3aa.id);
			ret = fimc_is_itf_map(device, group_id, dva, dmabuf->size);
			if (ret) {
				merr("fimc_is_itf_map is fail(%d)", vctx, ret);
				kfree(planes);
				kfree(buf);
				dma_buf_detach(dmabuf, attachment);
				dma_buf_put(dmabuf);
				goto p_err;
			}

			minfo("[3AA:V] buffer%d.plane%d mapping\n", vctx, buf->index, plane);
			set_bit(FRAME_MAP_MEM, &frame->memory);
			dma_buf_detach(dmabuf, attachment);
			dma_buf_put(dmabuf);
			kfree(planes);
			kfree(buf);
		}
		break;
	default:
		err("unsupported ioctl(%d)\n", ctrl->id);
		ret = -EINVAL;
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

	queue = GET_VCTX_QUEUE(vctx, vbq);
	video = vctx->video;

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
	/* Todo : add to prepare buffer */
	return 0;
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
	struct fimc_is_subdev *subdev;

	BUG_ON(!vctx);
	BUG_ON(!vctx->device);

	mdbgv_3aa("%s\n", vctx, __func__);

	device = vctx->device;

	if (V4L2_TYPE_IS_OUTPUT(vbq->type)) {
		queue = GET_SRC_QUEUE(vctx);
		subdev = &device->group_3aa.leader;
	} else {
		queue = GET_DST_QUEUE(vctx);
		subdev = &device->taap;
	}

	ret = fimc_is_queue_start_streaming(queue, device, subdev, vctx);
	if (ret)
		merr("fimc_is_queue_start_streaming is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_3aa_stop_streaming(struct vb2_queue *vbq)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *subdev;

	BUG_ON(!vctx);
	BUG_ON(!vctx->device);

	mdbgv_3aa("%s\n", vctx, __func__);

	device = vctx->device;

	if (V4L2_TYPE_IS_OUTPUT(vbq->type)) {
		queue = GET_SRC_QUEUE(vctx);
		subdev = &device->group_3aa.leader;
	} else {
		queue = GET_DST_QUEUE(vctx);
		subdev = &device->taap;
	}

	ret = fimc_is_queue_stop_streaming(queue, device, subdev, vctx);
	if (ret)
		merr("fimc_is_queue_stop_streaming is fail(%d)", vctx, ret);

	return ret;
}

static void fimc_is_3aa_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	u32 index;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_device_ischain *device;
	struct fimc_is_subdev *leader;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	index = vb->v4l2_buf.index;

#ifdef DBG_STREAMING
	mdbgv_3aa("%s(%02d:%d)\n", vctx, __func__, vb->v4l2_buf.type, index);
#endif

	video = vctx->video;
	device = vctx->device;
	leader = &device->group_3aa.leader;

	if (V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type)) {
		queue = GET_SRC_QUEUE(vctx);
		ret = fimc_is_queue_buffer_queue(queue, video->vb2, vb);
		if (ret) {
			merr("fimc_is_queue_buffer_queue is fail(%d)", vctx, ret);
			return;
		}

		ret = fimc_is_ischain_3aa_buffer_queue(device, queue, index);
		if (ret) {
			merr("fimc_is_ischain_3aa_buffer_queue is fail(%d)", vctx, ret);
			return;
		}
	} else {
		queue = GET_DST_QUEUE(vctx);
		ret = fimc_is_queue_buffer_queue(queue, video->vb2, vb);
		if (ret) {
			merr("fimc_is_queue_buffer_queue is fail(%d)", vctx, ret);
			return;
		}

		ret = fimc_is_subdev_buffer_queue(leader, index);
		if (ret) {
			merr("fimc_is_subdev_buffer_queue is fail(%d)", vctx, ret);
			return;
		}
	}
}

static int fimc_is_3aa_buffer_finish(struct vb2_buffer *vb)
{
	int ret = 0;
	u32 index = vb->v4l2_buf.index;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_device_ischain *device = vctx->device;
	struct fimc_is_group *group = &device->group_3aa;
	struct fimc_is_subdev *subdev = &group->leader;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!device);

#ifdef DBG_STREAMING
	mdbgv_3aa("%s(%02d:%d)\n", vctx, __func__, vb->v4l2_buf.type, index);
#endif

	if (V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type)) {
		queue = &vctx->q_src;
		fimc_is_ischain_3aa_buffer_finish(device, index);
	} else {
		queue = &vctx->q_dst;
		fimc_is_subdev_buffer_finish(subdev, index);
	}

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

