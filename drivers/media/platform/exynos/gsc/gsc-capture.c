/* linux/drivers/media/video/exynos/gsc/gsc-capture.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series G-scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <linux/i2c.h>
#include <media/v4l2-ioctl.h>
#include <media/exynos_gscaler.h>

#include "gsc-core.h"

static int gsc_capture_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt, unsigned int *num_buffers,
			unsigned int *num_planes, unsigned int sizes[],
			void *allocators[])
{
	struct gsc_ctx *ctx = vq->drv_priv;
	struct gsc_fmt *ffmt = ctx->d_frame.fmt;
	int i, ret = 0;

	if (!ffmt)
		return -EINVAL;

	*num_planes = ffmt->num_planes;

	for (i = 0; i < ffmt->num_planes; i++) {
		sizes[i] = get_plane_size(&ctx->d_frame, i);
		allocators[i] = ctx->gsc_dev->alloc_ctx;
	}

	ret = vb2_queue_init(vq);
	if (ret) {
		gsc_err("failed to init vb2_queue");
		return ret;
	}

	return 0;
}
static int gsc_capture_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct gsc_ctx *ctx = vq->drv_priv;
	struct gsc_frame *frame = &ctx->d_frame;
	int i;

	if (frame->fmt == NULL)
		return -EINVAL;

	for (i = 0; i < frame->fmt->num_planes; i++) {
		unsigned long size = frame->payload[i];

		if (vb2_plane_size(vb, i) < size) {
			v4l2_err(ctx->gsc_dev->cap.vfd,
				 "User buffer too small (%ld < %ld)\n",
				 vb2_plane_size(vb, i), size);
			return -EINVAL;
		}
		vb2_set_plane_payload(vb, i, size);
	}

	vb2_ion_buf_prepare(vb);

	return 0;
}

static int gsc_cap_set_addr(struct gsc_dev *gsc,struct gsc_ctx *ctx,
			struct gsc_input_buf *buf, int index)
{
	int ret;

	ret = gsc_prepare_addr(ctx, &buf->vb, &ctx->d_frame,
			&ctx->d_frame.addr);
	if (ret) {
		gsc_err("Prepare G-Scaler address failed\n");
		return -EINVAL;
	}

	if (!ctx->d_frame.addr.y) {
		gsc_err("source address is null");
		return -EINVAL;
	}

	buf->addr = ctx->d_frame.addr;
	list_add_tail(&buf->list, &gsc->cap.active_buf_q);
	gsc_hw_set_output_addr_fixed(gsc, &ctx->d_frame.addr);
	buf->idx = index;

	return 0;
}

static void gsc_capture_buf_queue(struct vb2_buffer *vb)
{
	struct gsc_input_buf *buf
		= container_of(vb, struct gsc_input_buf, vb);
	struct vb2_queue *q = vb->vb2_queue;
	struct gsc_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct gsc_dev *gsc = ctx->gsc_dev;
	int ret;

	if (vb->acquire_fence) {
		gsc_dbg("acquire fence has..");
		ret = sync_fence_wait(vb->acquire_fence, 100);
		sync_fence_put(vb->acquire_fence);
		vb->acquire_fence = NULL;
		if (ret < 0) {
			gsc_err("synce_fence_wait() timeout");
			return;
		}
	}

	if (!q->streaming) {
		gsc_info("gsc-wb initialize");
		INIT_LIST_HEAD(&gsc->cap.active_buf_q);
		ret = v4l2_subdev_call(gsc->cap.sd, video,
						s_stream, 1);
		if (ret) {
			gsc_err("gsc s_stream failed");
			return;
		}
	}

	ret = gsc_cap_set_addr(gsc, ctx, buf, vb->v4l2_buf.index);
	if (ret) {
		gsc_err("Failed to prepare output addr");
		return;
	}

	if (!test_and_set_bit(ST_CAPT_RUN, &gsc->state)) {
		ret = gsc_set_scaler_info(ctx);
		if (ret) {
			gsc_err("Scaler setup error");
			return;
		}
		gsc_hw_set_in_size(ctx);
		gsc_hw_set_out_size(ctx);
		gsc_hw_set_prescaler(ctx);
		gsc_hw_set_mainscaler(ctx);
		gsc_hw_set_h_coef(ctx);
		gsc_hw_set_v_coef(ctx);

		gsc_hw_set_output_rotation(ctx);

		gsc_hw_set_global_alpha(ctx);
		if (is_rotation) {
			ret = gsc_check_rotation_size(ctx);
			if (ret < 0) {
				gsc_err("Scaler setup error");
				return;
			}
		}

		gsc_hw_set_sfr_update(ctx);
		gsc_hw_enable_control(gsc, true);
		ret = gsc_wait_operating(gsc);
		if (ret < 0) {
			gsc_err("gscaler wait operating timeout");
			return;
		}
		gsc_dbg("gsc-wb start");
	} else {
		gsc_err();
	}
}

static int gsc_capture_subdev_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct gsc_capture_device *cap = &gsc->cap;
	struct gsc_ctx *ctx = cap->ctx;
	int ret;

	if (enable) {
		gsc_hw_set_local_src(gsc, true);
		gsc_hw_set_sysreg_writeback(gsc, true);
		gsc_hw_set_sw_reset(gsc);
		ret = gsc_wait_reset(gsc);
		if (ret < 0) {
			gsc_err("gscaler s/w reset timeout");
			return ret;
		}
		ret = gsc_set_scaler_info(ctx);
		if (ret) {
			gsc_err("Scaler setup error");
			return ret;
		}
		gsc_hw_set_output_buf_fixed(gsc);
		gsc_hw_set_output_buf_masking(gsc, 0, false);
		gsc_hw_set_frm_done_irq_mask(gsc, false);
		gsc_hw_set_deadlock_irq_mask(gsc, false);
		gsc_hw_set_read_slave_error_mask(gsc, false);
		gsc_hw_set_write_slave_error_mask(gsc, false);
		gsc_hw_set_overflow_irq_mask(gsc, true);
		gsc_hw_set_gsc_irq_enable(gsc, true);
		gsc_hw_set_one_frm_mode(gsc, true);
		gsc_hw_set_freerun_clock_mode(gsc, false);

		gsc_hw_set_input_path(ctx);
		gsc_hw_set_in_size(ctx);
		gsc_hw_set_in_image_format(ctx);

		gsc_hw_set_output_path(ctx);
		gsc_hw_set_out_size(ctx);
		gsc_hw_set_out_image_format(ctx);

		gsc_hw_set_prescaler(ctx);
		gsc_hw_set_mainscaler(ctx);
		gsc_hw_set_h_coef(ctx);
		gsc_hw_set_v_coef(ctx);

		gsc_hw_set_output_rotation(ctx);

		gsc_hw_set_global_alpha(ctx);
		if (is_rotation) {
			ret = gsc_check_rotation_size(ctx);
			if (ret < 0) {
				gsc_err("Scaler setup error");
				return ret;
			}
		}

		gsc_hw_set_for_wb(gsc);
		gsc_hw_set_lookup_table(gsc);
		gsc_hw_set_smart_if_con(gsc, true);
		gsc_hw_set_buscon_realtime(gsc);
		gsc_hw_set_qos_enable(gsc);
	}

	return 0;
}

static int gsc_capture_start_streaming(struct vb2_queue *q, unsigned int count)
{
	return 0;
}

static int gsc_cap_stop_capture(struct gsc_dev *gsc)
{
	int ret;
	gsc_dbg("G-Scaler h/w disable control");
	ret = wait_event_timeout(gsc->irq_queue,
			!test_bit(ST_CAPT_RUN, &gsc->state),
			GSC_SHUTDOWN_TIMEOUT);
	if (ret == 0) {
		gsc_err("wait timeout");
		return -EBUSY;
	}

	return 0;
}

static int gsc_capture_stop_streaming(struct vb2_queue *q)
{
	struct gsc_ctx *ctx = q->drv_priv;
	struct gsc_dev *gsc = ctx->gsc_dev;

	vb2_wait_for_all_buffers(q);

	return gsc_cap_stop_capture(gsc);
}

static struct vb2_ops gsc_capture_qops = {
	.queue_setup		= gsc_capture_queue_setup,
	.buf_prepare		= gsc_capture_buf_prepare,
	.buf_finish		= vb2_ion_buf_finish,
	.buf_queue		= gsc_capture_buf_queue,
	.wait_prepare		= gsc_unlock,
	.wait_finish		= gsc_lock,
	.start_streaming	= gsc_capture_start_streaming,
	.stop_streaming		= gsc_capture_stop_streaming,
};

/*
 * The video node ioctl operations
 */
static int gsc_vidioc_querycap_capture(struct file *file, void *priv,
				       struct v4l2_capability *cap)
{
	struct gsc_dev *gsc = video_drvdata(file);

	strncpy(cap->driver, gsc->pdev->name, sizeof(cap->driver) - 1);
	strncpy(cap->card, gsc->pdev->name, sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	return 0;
}

static int gsc_capture_enum_fmt_mplane(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	return gsc_enum_fmt_mplane(f);
}

static int gsc_capture_try_fmt_mplane(struct file *file, void *fh,
				   struct v4l2_format *f)
{
	struct gsc_dev *gsc = video_drvdata(file);

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	return gsc_try_fmt_mplane(gsc->cap.ctx, f);
}

static int gsc_capture_s_fmt_mplane(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *frame;
	struct v4l2_pix_format_mplane *pix;
	int i, ret = 0;

	ret = gsc_capture_try_fmt_mplane(file, fh, f);
	if (ret)
		return ret;

	if (vb2_is_streaming(&gsc->cap.vbq)) {
		gsc_err("queue (%d) busy", f->type);
		return -EBUSY;
	}

	frame = &ctx->d_frame;

	pix = &f->fmt.pix_mp;
	frame->fmt = find_format(&pix->pixelformat, NULL, 0);
	if (!frame->fmt)
		return -EINVAL;

	for (i = 0; i < frame->fmt->num_planes; i++)
		frame->payload[i] = pix->plane_fmt[i].sizeimage;

	gsc_set_frame_size(frame, pix->width, pix->height);

	ctx->state |= GSC_SRC_FMT;

	gsc_dbg("f_w: %d, f_h: %d", frame->f_width, frame->f_height);

	return 0;
}

static int gsc_capture_g_fmt_mplane(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_ctx *ctx = gsc->cap.ctx;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	return gsc_g_fmt_mplane(ctx, f);
}

static int gsc_capture_reqbufs(struct file *file, void *priv,
			    struct v4l2_requestbuffers *reqbufs)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_capture_device *cap = &gsc->cap;
	struct gsc_frame *frame;
	int ret;

	frame = ctx_get_frame(cap->ctx, reqbufs->type);
	frame->cacheable = cap->ctx->gsc_ctrls.cacheable->val;
	gsc->vb2->set_cacheable(gsc->alloc_ctx, frame->cacheable);

	ret = vb2_reqbufs(&cap->vbq, reqbufs);
	if (!ret)
		cap->reqbufs_cnt = reqbufs->count;

	return ret;
}

static int gsc_capture_querybuf(struct file *file, void *priv,
			   struct v4l2_buffer *buf)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_capture_device *cap = &gsc->cap;

	return vb2_querybuf(&cap->vbq, buf);
}

static int gsc_capture_qbuf(struct file *file, void *priv,
			  struct v4l2_buffer *buf)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_capture_device *cap = &gsc->cap;

	return vb2_qbuf(&cap->vbq, buf);
}

static int gsc_capture_dqbuf(struct file *file, void *priv,
			   struct v4l2_buffer *buf)
{
	struct gsc_dev *gsc = video_drvdata(file);
	return vb2_dqbuf(&gsc->cap.vbq, buf,
		file->f_flags & O_NONBLOCK);
}

static int gsc_capture_cropcap(struct file *file, void *fh,
			    struct v4l2_cropcap *cr)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct gsc_ctx *ctx = gsc->cap.ctx;

	if (cr->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	cr->bounds.left		= 0;
	cr->bounds.top		= 0;
	cr->bounds.width	= ctx->d_frame.f_width;
	cr->bounds.height	= ctx->d_frame.f_height;
	cr->defrect		= cr->bounds;

	return 0;
}

static int gsc_capture_enum_input(struct file *file, void *priv,
			       struct v4l2_input *i)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct exynos_platform_gscaler *pdata = gsc->pdata;
	struct exynos_isp_info *isp_info;

	if (i->index >= MAX_CAMIF_CLIENTS)
		return -EINVAL;

	isp_info = pdata->isp_info[i->index];
	if (isp_info == NULL)
		return -EINVAL;

	i->type = V4L2_INPUT_TYPE_CAMERA;

	strncpy(i->name, isp_info->board_info->type, 32);

	return 0;
}

static int gsc_capture_s_input(struct file *file, void *priv, unsigned int i)
{
	return i == 0 ? 0 : -EINVAL;
}

static int gsc_capture_g_input(struct file *file, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int gsc_capture_ctrls_create(struct gsc_dev *gsc)
{
	int ret;

	if (WARN_ON(gsc->cap.ctx == NULL))
		return -ENXIO;
	if (gsc->cap.ctx->ctrls_rdy)
		return 0;
	ret = gsc_ctrls_create(gsc->cap.ctx);
	if (ret)
		return ret;

	return 0;
}

static int gsc_capture_open(struct file *file)
{
	struct gsc_dev *gsc = video_drvdata(file);
	int ret = v4l2_fh_open(file);

	if (ret)
		return ret;

	if (gsc_m2m_opened(gsc) || gsc_out_opened(gsc) || gsc_cap_opened(gsc)) {
		v4l2_fh_release(file);
		return -EBUSY;
	}

	set_bit(ST_CAPT_OPEN, &gsc->state);

	pm_runtime_get_sync(&gsc->pdev->dev);

	if (++gsc->cap.refcnt == 1) {
		ret = gsc_capture_ctrls_create(gsc);
		if (ret) {
			gsc_err("failed to create controls\n");
			goto err;
		}
	}

	gsc->isr_cnt = 0;
	gsc_dbg("pid: %d, state: 0x%lx", task_pid_nr(current), gsc->state);

	return 0;

err:
	pm_runtime_put_sync(&gsc->pdev->dev);
	v4l2_fh_release(file);
	clear_bit(ST_CAPT_OPEN, &gsc->state);
	return ret;
}

static int gsc_capture_close(struct file *file)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct vb2_queue *q = &gsc->cap.vbq;

	gsc_dbg("pid: %d, state: 0x%lx", task_pid_nr(current), gsc->state);

	if (q->streaming)
		gsc_capture_stop_streaming(q);

	if (--gsc->cap.refcnt == 0) {
		clear_bit(ST_CAPT_OPEN, &gsc->state);
		gsc_dbg("G-Scaler h/w disable control");
		clear_bit(ST_CAPT_RUN, &gsc->state);
		vb2_queue_release(&gsc->cap.vbq);
		gsc_ctrls_delete(gsc->cap.ctx);
	}

	pm_runtime_put_sync(&gsc->pdev->dev);

	return v4l2_fh_release(file);
}

static unsigned int gsc_capture_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct gsc_dev *gsc = video_drvdata(file);

	return vb2_poll(&gsc->cap.vbq, file, wait);
}

static int gsc_capture_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gsc_dev *gsc = video_drvdata(file);

	return vb2_mmap(&gsc->cap.vbq, vma);
}

static int gsc_capture_streamon(struct file *file, void *priv,
				enum v4l2_buf_type type)
{
	struct gsc_dev *gsc = video_drvdata(file);

	if (gsc_cap_active(gsc)) {
		gsc_err("gsc didn't stop complete");
		return -EBUSY;
	}

	return vb2_streamon(&gsc->cap.vbq, type);
}

static int gsc_capture_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	struct gsc_dev *gsc = video_drvdata(file);
	int ret;

	ret = vb2_streamoff(&gsc->cap.vbq, type);

	return ret;
}

static struct v4l2_subdev *gsc_cap_remote_subdev(struct gsc_dev *gsc, u32 *pad)
{
	struct media_pad *remote;

	remote = media_entity_remote_source(&gsc->cap.vd_pad);

	if (remote == NULL ||
	    media_entity_type(remote->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
		return NULL;

	if (pad)
		*pad = remote->index;

	return media_entity_to_v4l2_subdev(remote->entity);
}

static int gsc_capture_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct v4l2_subdev *subdev;
	struct v4l2_subdev_crop subdev_crop;
	int ret;

	subdev = gsc_cap_remote_subdev(gsc, NULL);
	if (subdev == NULL)
		return -EINVAL;

	/* Try the get crop operation first and fallback to get format if not
	 * implemented.
	 */
	subdev_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	subdev_crop.pad = GSC_PAD_SOURCE;
	ret = v4l2_subdev_call(subdev, pad, get_crop, NULL, &subdev_crop);
	if (ret < 0)
		return ret == -ENOIOCTLCMD ? -EINVAL : ret;

	crop->c.left = subdev_crop.rect.left;
	crop->c.top = subdev_crop.rect.top;
	crop->c.width = subdev_crop.rect.width;
	crop->c.height = subdev_crop.rect.height;

	return 0;
}

static int gsc_capture_s_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct gsc_dev *gsc = video_drvdata(file);
	struct v4l2_subdev *subdev;
	struct v4l2_subdev_crop subdev_crop;
	int ret;

	subdev = gsc_cap_remote_subdev(gsc, NULL);
	if (subdev == NULL)
		return -EINVAL;

	subdev_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	subdev_crop.pad = GSC_PAD_SOURCE;
	subdev_crop.rect.left = crop->c.left;
	subdev_crop.rect.top = crop->c.top;
	subdev_crop.rect.width = crop->c.width;
	subdev_crop.rect.height = crop->c.height;

	ret = v4l2_subdev_call(subdev, pad, set_crop, NULL, &subdev_crop);

	return ret == -ENOIOCTLCMD ? -EINVAL : ret;
}


static const struct v4l2_ioctl_ops gsc_capture_ioctl_ops = {
	.vidioc_querycap		= gsc_vidioc_querycap_capture,

	.vidioc_enum_fmt_vid_cap_mplane	= gsc_capture_enum_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= gsc_capture_try_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= gsc_capture_s_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= gsc_capture_g_fmt_mplane,

	.vidioc_reqbufs			= gsc_capture_reqbufs,
	.vidioc_querybuf		= gsc_capture_querybuf,

	.vidioc_qbuf			= gsc_capture_qbuf,
	.vidioc_dqbuf			= gsc_capture_dqbuf,

	.vidioc_streamon		= gsc_capture_streamon,
	.vidioc_streamoff		= gsc_capture_streamoff,

	.vidioc_g_crop			= gsc_capture_g_crop,
	.vidioc_s_crop			= gsc_capture_s_crop,
	.vidioc_cropcap			= gsc_capture_cropcap,

	.vidioc_enum_input		= gsc_capture_enum_input,
	.vidioc_s_input			= gsc_capture_s_input,
	.vidioc_g_input			= gsc_capture_g_input,
};

static const struct v4l2_file_operations gsc_capture_fops = {
	.owner		= THIS_MODULE,
	.open		= gsc_capture_open,
	.release	= gsc_capture_close,
	.poll		= gsc_capture_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= gsc_capture_mmap,
};

/*
 * __gsc_cap_get_format - helper function for getting gscaler format
 * @res   : pointer to resizer private structure
 * @pad   : pad number
 * @fh    : V4L2 subdev file handle
 * @which : wanted subdev format
 * return zero
 */
static struct v4l2_mbus_framefmt *__gsc_cap_get_format(struct gsc_dev *gsc,
				struct v4l2_subdev_fh *fh, unsigned int pad,
				enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &gsc->cap.mbus_fmt[pad];
}
static int gsc_cap_check_limit_size(struct gsc_dev *gsc, unsigned int pad,
				   struct v4l2_mbus_framefmt *fmt)
{
	struct gsc_variant *variant = gsc->variant;
	u32 src_crop = fmt->width * fmt->height;
	u32 max_src_size =
		variant->pix_max->otf_w * variant->pix_max->otf_h;
	switch (pad) {
	case GSC_PAD_SINK:
		if (src_crop > max_src_size) {
			gsc_err("%d x %d is not supported",
					fmt->width, fmt->height);
			return -EINVAL;
		}
	break;

	default:
		gsc_err("unsupported pad");
		return -EINVAL;
	}

	return 0;
}

static int gsc_cap_try_format(struct gsc_dev *gsc,
			       struct v4l2_subdev_fh *fh, unsigned int pad,
			       struct v4l2_mbus_framefmt *fmt,
			       enum v4l2_subdev_format_whence which)
{
	struct gsc_fmt *gfmt;
	int ret = 0;

	gfmt = find_format(NULL, &fmt->code, 0);
	WARN_ON(!gfmt);

	if (pad == GSC_PAD_SINK) {
		struct gsc_ctx *ctx = gsc->cap.ctx;
		struct gsc_frame *frame = &ctx->s_frame;

		frame->fmt = gfmt;
	}

	ret = gsc_cap_check_limit_size(gsc, pad, fmt);
	if (ret)
		return -EINVAL;

	fmt->colorspace = V4L2_COLORSPACE_JPEG;
	fmt->field = V4L2_FIELD_NONE;

	return ret;
}

static int gsc_capture_subdev_set_fmt(struct v4l2_subdev *sd,
				      struct v4l2_subdev_fh *fh,
				      struct v4l2_subdev_format *fmt)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf;
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *frame;

	mf = __gsc_cap_get_format(gsc, fh, fmt->pad, fmt->which);
	if (mf == NULL)
		return -EINVAL;

	gsc_cap_try_format(gsc, fh, fmt->pad, &fmt->format, fmt->which);
	*mf = fmt->format;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	frame = gsc_capture_get_frame(ctx, fmt->pad);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		frame->crop.left = 0;
		frame->crop.top = 0;
		frame->f_width = mf->width;
		frame->f_height = mf->height;
		frame->crop.width = mf->width;
		frame->crop.height = mf->height;
	}
	gsc_dbg("offs_h : %d, offs_v : %d, f_width : %d, f_height :%d,\
				width : %d, height : %d", frame->crop.left,\
				frame->crop.top, frame->f_width,
				frame->f_height,\
				frame->crop.width, frame->crop.height);

	return 0;
}

static int gsc_capture_subdev_get_fmt(struct v4l2_subdev *sd,
				      struct v4l2_subdev_fh *fh,
				      struct v4l2_subdev_format *fmt)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf;

	mf = __gsc_cap_get_format(gsc, fh, fmt->pad, fmt->which);
	if (mf == NULL)
		return -EINVAL;

	fmt->format = *mf;

	return 0;
}

static int __gsc_cap_get_crop(struct gsc_dev *gsc, struct v4l2_subdev_fh *fh,
			      unsigned int pad, enum v4l2_subdev_format_whence which,
				struct v4l2_rect *crop)
{
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *frame = gsc_capture_get_frame(ctx, pad);

	if (which == V4L2_SUBDEV_FORMAT_TRY) {
		*crop = *v4l2_subdev_get_try_crop(fh, pad);
	} else {
		crop->left = frame->crop.left;
		crop->top = frame->crop.top;
		crop->width = frame->crop.width;
		crop->height = frame->crop.height;
	}

	return 0;
}

static void gsc_cap_try_crop(struct gsc_dev *gsc, struct v4l2_rect *crop, u32 pad)
{
	struct gsc_variant *variant = gsc->variant;
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *frame = gsc_capture_get_frame(ctx, pad);

	u32 crop_min_w = variant->pix_min->target_w;
	u32 crop_min_h = variant->pix_min->target_h;
	u32 crop_max_w = frame->f_width;
	u32 crop_max_h = frame->f_height;

	crop->left = clamp_t(u32, crop->left, 0, crop_max_w - crop_min_w);
	crop->top = clamp_t(u32, crop->top, 0, crop_max_h - crop_min_h);
	crop->width = clamp_t(u32, crop->width, crop_min_w, crop_max_w);
	crop->height = clamp_t(u32, crop->height, crop_min_h, crop_max_h);
}

static int gsc_capture_subdev_set_crop(struct v4l2_subdev *sd,
				       struct v4l2_subdev_fh *fh,
				       struct v4l2_subdev_crop *crop)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct gsc_ctx *ctx = gsc->cap.ctx;
	struct gsc_frame *frame = gsc_capture_get_frame(ctx, crop->pad);

	if ((crop->pad == GSC_PAD_SINK) && (crop->rect.width % 8)) {
		gsc_err("%d is not aligned 8", crop->rect.width);
		return -EINVAL;
	}
	gsc_cap_try_crop(gsc, &crop->rect, crop->pad);

	if (crop->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		frame->crop = crop->rect;

	return 0;
}

static int gsc_capture_subdev_get_crop(struct v4l2_subdev *sd,
				       struct v4l2_subdev_fh *fh,
				       struct v4l2_subdev_crop *crop)
{
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct v4l2_rect gcrop = {0, };

	__gsc_cap_get_crop(gsc, fh, crop->pad, crop->which, &gcrop);
	crop->rect = gcrop;

	return 0;
}

static struct v4l2_subdev_pad_ops gsc_cap_subdev_pad_ops = {
	.get_fmt = gsc_capture_subdev_get_fmt,
	.set_fmt = gsc_capture_subdev_set_fmt,
	.get_crop = gsc_capture_subdev_get_crop,
	.set_crop = gsc_capture_subdev_set_crop,
};

static struct v4l2_subdev_video_ops gsc_cap_subdev_video_ops = {
	.s_stream = gsc_capture_subdev_s_stream,
};

static struct v4l2_subdev_ops gsc_cap_subdev_ops = {
	.pad = &gsc_cap_subdev_pad_ops,
	.video = &gsc_cap_subdev_video_ops,
};

static int gsc_capture_init_formats(struct v4l2_subdev *sd,
				    struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	struct gsc_ctx *ctx = gsc->cap.ctx;

	ctx->s_frame.fmt = get_format(2);
	memset(&format, 0, sizeof(format));
	format.pad = GSC_PAD_SINK;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = ctx->s_frame.fmt->mbus_code;
	format.format.width = DEFAULT_GSC_SINK_WIDTH;
	format.format.height = DEFAULT_GSC_SINK_HEIGHT;
	gsc_capture_subdev_set_fmt(sd, fh, &format);

	/* G-scaler should not propagate, because it is possible that sink
	 * format different from source format. But the operation of source pad
	 * is not needed.
	 */
	ctx->d_frame.fmt = get_format(2);

	return 0;
}

static int gsc_capture_subdev_close(struct v4l2_subdev *sd,
				    struct v4l2_subdev_fh *fh)
{
	gsc_dbg("");

	return 0;
}

static int gsc_capture_subdev_registered(struct v4l2_subdev *sd)
{
	gsc_dbg("");

	return 0;
}

static void gsc_capture_subdev_unregistered(struct v4l2_subdev *sd)
{
	gsc_dbg("");
}

static const struct v4l2_subdev_internal_ops gsc_cap_v4l2_internal_ops = {
	.open = gsc_capture_init_formats,
	.close = gsc_capture_subdev_close,
	.registered = gsc_capture_subdev_registered,
	.unregistered = gsc_capture_subdev_unregistered,
};

static int gsc_clk_enable_for_wb(struct gsc_dev *gsc)
{
	struct clk *gsd;
	int ret = 0;

	gsd = devm_clk_get(&gsc->pdev->dev, "gate_gsd");
	if (IS_ERR(gsd)) {
		gsc_err("fail to get gsd clock");
		return PTR_ERR(gsd);
	}

	ret = clk_prepare_enable(gsd);
	if (ret) {
		gsc_err("fail to enable gsd");
		return -EINVAL;
	}

	clk_put(gsd);

	return 0;
}

static int gsc_clk_disable_for_wb(struct gsc_dev *gsc)
{
	struct clk *gsd;

	gsd = devm_clk_get(&gsc->pdev->dev, "gate_gsd");
	if (IS_ERR(gsd)) {
		gsc_err("fail to get gsd clock");
		return PTR_ERR(gsd);
	}

	clk_disable_unprepare(gsd);
	clk_put(gsd);

	return 0;
}

static int gsc_capture_link_setup(struct media_entity *entity,
				  const struct media_pad *local,
				  const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct gsc_dev *gsc = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (local->flags == MEDIA_PAD_FL_SINK) {
		if (flags & MEDIA_LNK_FL_ENABLED) {
			gsc_info("gsc-wb link enable");
			gsc->pipeline.disp =
				media_entity_to_v4l2_subdev(remote->entity);

			ret = gsc_clk_enable_for_wb(gsc);
			if (ret)
				return ret;
		} else {
			gsc_info("gsc-wb link disable");
			gsc->pipeline.disp = NULL;
			ret = gsc_clk_disable_for_wb(gsc);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static const struct media_entity_operations gsc_cap_media_ops = {
	.link_setup = gsc_capture_link_setup,
};

static int gsc_capture_create_subdev(struct gsc_dev *gsc)
{
	struct v4l2_device *v4l2_dev;
	struct v4l2_subdev *sd;
	int ret;

	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd)
	       return -ENOMEM;

	v4l2_subdev_init(sd, &gsc_cap_subdev_ops);
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), "gsc-wb-sd");

	gsc->cap.sd_pads[GSC_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	gsc->cap.sd_pads[GSC_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&sd->entity, GSC_PADS_NUM,
				gsc->cap.sd_pads, 0);
	if (ret)
		goto err_ent;

	sd->internal_ops = &gsc_cap_v4l2_internal_ops;
	sd->entity.ops = &gsc_cap_media_ops;
	v4l2_dev = &gsc->mdev[MDEV_CAPTURE]->v4l2_dev;

	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret)
		goto err_sub;

	gsc->mdev[MDEV_CAPTURE]->gsc_wb_sd = sd;
	gsc->cap.sd = sd;
	v4l2_set_subdevdata(sd, gsc);
//	gsc_capture_init_formats(sd, NULL);

	return 0;

err_sub:
	media_entity_cleanup(&sd->entity);
err_ent:
	kfree(sd);
	return ret;
}

static int gsc_capture_create_link(struct gsc_dev *gsc)
{
	struct media_entity *source, *sink;
	int ret;

	/* GSC-SUBDEV ------> GSC-VIDEO (Always link enable) */
	source = &gsc->cap.sd->entity;
	sink = &gsc->cap.vfd->entity;
	if (source && sink) {
		ret = media_entity_create_link(source,
				GSC_PAD_SOURCE, sink, 0,
				MEDIA_LNK_FL_ENABLED |
				MEDIA_LNK_FL_IMMUTABLE);
		if (ret) {
			gsc_err("failed link sd-gsc to vd-gsc\n");
			return ret;
		}
	}

	return 0;
}

int gsc_register_capture_device(struct gsc_dev *gsc)
{
	struct video_device *vfd;
	struct gsc_capture_device *gsc_cap;
	struct gsc_ctx *ctx;
	struct vb2_queue *q;
	int ret = -ENOMEM;

	ctx = kzalloc(sizeof *ctx, GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->gsc_dev	 = gsc;
	ctx->in_path	 = GSC_WRITEBACK;
	ctx->out_path	 = GSC_DMA;
	ctx->state	 = GSC_CTX_CAP;

	vfd = video_device_alloc();
	if (!vfd) {
		printk("Failed to allocate video device\n");
		goto err_ctx_alloc;
	}

	snprintf(vfd->name, sizeof(vfd->name), "%s.capture",
		 dev_name(&gsc->pdev->dev));

	vfd->fops	= &gsc_capture_fops;
	vfd->ioctl_ops	= &gsc_capture_ioctl_ops;
	vfd->v4l2_dev	= &gsc->mdev[MDEV_CAPTURE]->v4l2_dev;
	vfd->minor	= -1;
	vfd->release	= video_device_release;
	vfd->lock	= &gsc->lock;
	vfd->vfl_dir	= VFL_DIR_RX;
	video_set_drvdata(vfd, gsc);

	gsc_cap	= &gsc->cap;
	gsc_cap->vfd = vfd;
	gsc_cap->refcnt = 0;
	gsc_cap->active_buf_cnt = 0;
	gsc_cap->reqbufs_cnt  = 0;

	INIT_LIST_HEAD(&gsc->cap.active_buf_q);
	spin_lock_init(&ctx->slock);
	gsc_cap->ctx = ctx;

	q = &gsc->cap.vbq;
	memset(q, 0, sizeof(*q));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = gsc->cap.ctx;
	q->ops = &gsc_capture_qops;
	q->mem_ops = gsc->vb2->ops;
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(q);
	if (ret) {
		gsc_err("failed to init vb2_queue");
		goto err_ctx_alloc;
	}

	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
			    EXYNOS_VIDEONODE_GSC_CAP(gsc->id));
	if (ret) {
		gsc_err("failed to register video device");
		goto err_ctx_alloc;
	}

	gsc->cap.vd_pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&vfd->entity, 1, &gsc->cap.vd_pad, 0);
	if (ret) {
		gsc_err("failed to initialize entity");
		goto err_ent;
	}

	ret = gsc_capture_create_subdev(gsc);
	if (ret) {
		gsc_err("failed create subdev");
		goto err_sd_reg;
	}

	ret = gsc_capture_create_link(gsc);
	if (ret) {
		gsc_err("failed create link");
		goto err_sd_reg;
	}

	vfd->ctrl_handler = &ctx->ctrl_handler;
	gsc_info("gsc capture driver registered as /dev/video%d", vfd->num);

	return 0;

err_sd_reg:
	media_entity_cleanup(&vfd->entity);
err_ent:
	video_device_release(vfd);
err_ctx_alloc:
	kfree(ctx);

	return ret;
}

static void gsc_capture_destroy_subdev(struct gsc_dev *gsc)
{
	struct v4l2_subdev *sd = gsc->cap.sd;

	if (!sd)
		return;
	media_entity_cleanup(&sd->entity);
	v4l2_device_unregister_subdev(sd);
	kfree(sd);
	sd = NULL;
}

void gsc_unregister_capture_device(struct gsc_dev *gsc)
{
	struct video_device *vfd = gsc->cap.vfd;

	if (vfd) {
		media_entity_cleanup(&vfd->entity);
		/* Can also be called if video device was
		   not registered */
		video_unregister_device(vfd);
	}
	gsc_capture_destroy_subdev(gsc);
	kfree(gsc->cap.ctx);
	gsc->cap.ctx = NULL;
}
