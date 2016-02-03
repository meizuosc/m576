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
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-ion.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>
#include <media/exynos_mc.h>

#include "fimc-is-time.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-debug.h"

#define SPARE_PLANE 1
#define SPARE_SIZE (32 * 1024)

struct fimc_is_fmt fimc_is_formats[] = {
	{
		.name		= "YUV 4:4:4 planar, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUV444,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= V4L2_MBUS_FMT_YUV8_1X24,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= V4L2_MBUS_FMT_UYVY8_2X8,
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "YUV 4:2:0 planar, YCrCb",
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2 + SPARE_PLANE,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2 + SPARE_PLANE,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.num_planes	= 3 + SPARE_PLANE,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.num_planes	= 3 + SPARE_PLANE,
	}, {
		.name		= "BAYER 8 bit",
		.pixelformat	= V4L2_PIX_FMT_SGRBG8,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "BAYER 10 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "BAYER 12 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "BAYER 16 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR16,
		.num_planes	= 1 + SPARE_PLANE,
	}, {
		.name		= "JPEG",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= V4L2_MBUS_FMT_JPEG_1X8,
	}
};

struct fimc_is_fmt *fimc_is_find_format(u32 *pixelformat,
	u32 *mbus_code)
{
	struct fimc_is_fmt *fmt;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(fimc_is_formats); ++i) {
		fmt = &fimc_is_formats[i];
		if (pixelformat && fmt->pixelformat == *pixelformat)
			return fmt;
		if (mbus_code && fmt->mbus_code == *mbus_code)
			return fmt;
	}

	return NULL;
}

int get_plane_size_flite(int width, int height)
{
    int PlaneSize;
    int Alligned_Width;
    int Bytes;

    Alligned_Width = (width + 9) / 10 * 10;
    Bytes = Alligned_Width * 8 / 5 ;

    PlaneSize = Bytes * height;

    return PlaneSize;
}

void fimc_is_set_plane_size(struct fimc_is_frame_cfg *frame, unsigned int sizes[])
{
	u32 plane;
	u32 width[FIMC_IS_MAX_PLANES];

	for (plane = 0; plane < FIMC_IS_MAX_PLANES; ++plane)
		width[plane] = frame->width + frame->width_stride[plane];

	switch (frame->format.pixelformat) {
	case V4L2_PIX_FMT_YUV444:
		dbg("V4L2_PIX_FMT_YUV444(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height * 4;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_YUYV:
		dbg("V4L2_PIX_FMT_YUYV(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height * 2;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_NV12:
		dbg("V4L2_PIX_FMT_NV12(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height * 3 / 2;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_NV12M:
		dbg("V4L2_PIX_FMT_NV12M(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height;
		sizes[1] = width[1] * frame->height / 2;
		sizes[2] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_NV21:
		dbg("V4L2_PIX_FMT_NV21(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height * 3 / 2;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_NV21M:
		dbg("V4L2_PIX_FMT_NV21M(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height;
		sizes[1] = width[1] * frame->height / 2;
		sizes[2] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		dbg("V4L2_PIX_FMT_YVU420M(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height;
		sizes[1] = width[1] * frame->height / 4;
		sizes[2] = width[2] * frame->height / 4;
		sizes[3] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SGRBG8:
		dbg("V4L2_PIX_FMT_SGRBG8(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = frame->width * frame->height;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SBGGR10:
		dbg("V4L2_PIX_FMT_SBGGR10(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = get_plane_size_flite(frame->width,frame->height);
		if (frame->bytesperline[0]) {
			if (frame->bytesperline[0] >= frame->width * 5 / 4) {
			sizes[0] = frame->bytesperline[0]
			    * frame->height;
			} else {
				err("Bytesperline too small\
					(fmt(V4L2_PIX_FMT_SBGGR10), W(%d), Bytes(%d))",
				frame->width,
				frame->bytesperline[0]);
			}
		}
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SBGGR16:
		dbg("V4L2_PIX_FMT_SBGGR16(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = frame->width*frame->height * 2;
		if (frame->bytesperline[0]) {
			if (frame->bytesperline[0] >= frame->width * 2) {
				sizes[0] = frame->bytesperline[0] * frame->height;
			} else {
				err("Bytesperline too small\
					(fmt(V4L2_PIX_FMT_SBGGR16), W(%d), Bytes(%d))",
				frame->width,
				frame->bytesperline[0]);
			}
		}
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SBGGR12:
		dbg("V4L2_PIX_FMT_SBGGR12(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = get_plane_size_flite(frame->width,frame->height);
		if (frame->bytesperline[0]) {
			if (frame->bytesperline[0] >= frame->width * 3 / 2) {
				sizes[0] = frame->bytesperline[0] * frame->height;
			} else {
				err("Bytesperline too small\
				(fmt(V4L2_PIX_FMT_SBGGR12), W(%d), Bytes(%d))",
				frame->width,
				frame->bytesperline[0]);
			}
		}
		sizes[1] = SPARE_SIZE;
		break;
	default:
		err("unknown pixelformat\n");
		break;
	}
}

static inline void vref_init(struct fimc_is_video *video)
{
	atomic_set(&video->refcount, 0);
}

static inline int vref_get(struct fimc_is_video *video)
{
	return atomic_inc_return(&video->refcount) - 1;
}

static inline int vref_put(struct fimc_is_video *video,
	void (*release)(struct fimc_is_video *video))
{
	int ret = 0;

	ret = atomic_sub_and_test(1, &video->refcount);
	if (ret)
		pr_debug("closed all instacne");

	return atomic_read(&video->refcount);
}

static int queue_init(void *priv, struct vb2_queue *vbq,
	struct vb2_queue *vbq_dst)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = priv;
	struct fimc_is_video *video;
	u32 type;

	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!vbq);

	video = GET_VIDEO(vctx);

	if (video->type == FIMC_IS_VIDEO_TYPE_CAPTURE)
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	vbq->type		= type;
	vbq->io_modes		= VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	vbq->drv_priv		= vctx;
	vbq->ops		= vctx->vb2_ops;
	vbq->mem_ops		= vctx->mem_ops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
	vbq->timestamp_type	= V4L2_BUF_FLAG_TIMESTAMP_COPY;
#endif

	ret = vb2_queue_init(vbq);
	if (ret) {
		err("vb2_queue_init fail(%d)", ret);
		goto p_err;
	}

	vctx->queue.vbq = vbq;

p_err:
	return ret;
}

int open_vctx(struct file *file,
	struct fimc_is_video *video,
	struct fimc_is_video_ctx **vctx,
	u32 instance,
	u32 id)
{
	int ret = 0;

	BUG_ON(!file);
	BUG_ON(!video);

	if (atomic_read(&video->refcount) > FIMC_IS_MAX_NODES) {
		err("can't open vctx, refcount is invalid");
		ret = -EINVAL;
		goto p_err;
	}

	*vctx = kzalloc(sizeof(struct fimc_is_video_ctx), GFP_KERNEL);
	if (*vctx == NULL) {
		err("kzalloc is fail");
		ret = -ENOMEM;
		goto p_err;
	}

	(*vctx)->refcount = vref_get(video);
	(*vctx)->instance = instance;
	(*vctx)->queue.id = id;
	(*vctx)->state = BIT(FIMC_IS_VIDEO_CLOSE);

	file->private_data = *vctx;

p_err:
	return ret;
}

int close_vctx(struct file *file,
	struct fimc_is_video *video,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;

	kfree(vctx);
	file->private_data = NULL;
	ret = vref_put(video, NULL);

	return ret;
}

/*
 * =============================================================================
 * Queue Opertation
 * =============================================================================
 */

static int fimc_is_queue_open(struct fimc_is_queue *queue,
	u32 rdycount)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	queue->buf_rdycount = rdycount;
	queue->buf_req = 0;
	queue->buf_pre = 0;
	queue->buf_que = 0;
	queue->buf_com = 0;
	queue->buf_dqe = 0;
	clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
	memset(&queue->framecfg, 0, sizeof(struct fimc_is_frame_cfg));
	fimc_is_frame_probe(&queue->framemgr, queue->id);

	return ret;
}

static int fimc_is_queue_close(struct fimc_is_queue *queue)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
	fimc_is_frame_close(&queue->framemgr);

	return ret;
}

static int fimc_is_queue_set_format_mplane(struct fimc_is_queue *queue,
	void *device,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 plane;
	struct v4l2_pix_format_mplane *pix;
	struct fimc_is_fmt *fmt;

	BUG_ON(!queue);

	pix = &format->fmt.pix_mp;
	fmt = fimc_is_find_format(&pix->pixelformat, NULL);
	if (!fmt) {
		err("pixel format is not found\n");
		ret = -EINVAL;
		goto p_err;
	}

	queue->framecfg.format.pixelformat	= fmt->pixelformat;
	queue->framecfg.colorspace		= pix->colorspace;
	queue->framecfg.format.mbus_code	= fmt->mbus_code;
	queue->framecfg.format.num_planes	= fmt->num_planes;
	queue->framecfg.width			= pix->width;
	queue->framecfg.height			= pix->height;

	for (plane = 0; plane < fmt->num_planes; ++plane) {
		if (pix->plane_fmt[plane].bytesperline) {
			queue->framecfg.bytesperline[plane] =
				pix->plane_fmt[plane].bytesperline;
			queue->framecfg.width_stride[plane] =
				pix->plane_fmt[plane].bytesperline - pix->width;
		} else {
			queue->framecfg.bytesperline[plane] = 0;
			queue->framecfg.width_stride[plane] = 0;
		}
	}

	ret = CALL_QOPS(queue, s_format, device, queue);
	if (ret) {
		err("s_format is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_queue_setup(struct fimc_is_queue *queue,
	void *alloc_ctx,
	unsigned int *num_planes,
	unsigned int sizes[],
	void *allocators[])
{
	u32 ret = 0;
	u32 plane;

	BUG_ON(!queue);
	BUG_ON(!alloc_ctx);
	BUG_ON(!num_planes);
	BUG_ON(!sizes);
	BUG_ON(!allocators);

	*num_planes = (unsigned int)(queue->framecfg.format.num_planes);
	fimc_is_set_plane_size(&queue->framecfg, sizes);

	for (plane = 0; plane < *num_planes; plane++) {
		allocators[plane] = alloc_ctx;
		queue->framecfg.size[plane] = sizes[plane];
		mdbgv_vid("queue[%d] size : %d\n", plane, sizes[plane]);
	}

	return ret;
}

int fimc_is_queue_buffer_queue(struct fimc_is_queue *queue,
	const struct fimc_is_vb2 *vb2,
	struct vb2_buffer *vb)
{
	u32 ret = 0, i;
	u32 index;
	u32 ext_size;
	u32 spare;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	#ifdef PRINT_BUFADDR
	struct vb2_ion_cookie *vb2cookie;
	#endif

	index = vb->v4l2_buf.index;
	framemgr = &queue->framemgr;
	vctx = container_of(queue, struct fimc_is_video_ctx, queue);

	BUG_ON(framemgr->id == FRAMEMGR_ID_INVALID);

	/* plane address is updated for checking everytime */
	for (i = 0; i < vb->num_planes; i++) {
		queue->buf_box[index][i] = vb2->plane_cookie(vb, i);
		queue->buf_dva[index][i] = vb2->plane_dvaddr(queue->buf_box[index][i], i);
#ifdef DBG_IMAGE_KMAPPING
		queue->buf_kva[index][i] = vb2->plane_kvaddr(vb, i);
#endif
		#ifdef PRINT_BUFADDR
		{
			vb2cookie = (struct vb2_ion_cookie *)queue->buf_box[index][i];
			queue->size[index][i] = PAGE_ALIGN(vb->v4l2_planes[i].length);
		}
		#endif

	}

	frame = &framemgr->frame[index];

	/* uninitialized frame need to get info */
	if (!test_bit(FRAME_INI_MEM, &frame->memory))
		goto set_info;

	/* plane count check */
	if (frame->planes != vb->num_planes) {
		merr("plane count is changed(%08X != %08X)", vctx,
			frame->planes, vb->num_planes);
		ret = -EINVAL;
		goto exit;
	}

	/* plane address check */
	for (i = 0; i < frame->planes; i++) {
		if (frame->dvaddr_buffer[i] != queue->buf_dva[index][i]) {
			merr("buffer[%d][%d] is changed(%08X != %08X)", vctx, index, i,
				frame->dvaddr_buffer[i],
				queue->buf_dva[index][i]);
			ret = -EINVAL;
			goto exit;
		}
	}

	goto exit;

set_info:
	if (test_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state)) {
		merr("already prepared but new index(%d) is came", vctx, index);
		ret = -EINVAL;
		goto exit;
	}

	frame->vb = vb;
	frame->planes = vb->num_planes;
	spare = frame->planes - 1;

	for (i = 0; i < frame->planes; i++) {
		frame->dvaddr_buffer[i] = queue->buf_dva[index][i];
#ifdef PRINT_BUFADDR
		frame->sizes[i] = queue->size[index][i];
		minfo("%04X %d.%d %08X size %d\n", vctx, framemgr->id, index, i,
			frame->dvaddr_buffer[i], queue->size[index][i]);
#endif
	}

	if (framemgr->id & FRAMEMGR_ID_SHOT) {
		ext_size = sizeof(struct camera2_shot_ext) - sizeof(struct camera2_shot);

		/* Create Kvaddr for Metadata */
		queue->buf_kva[index][spare] = vb2->plane_kvaddr(vb, spare);
		if (!queue->buf_kva[index][spare]) {
			merr("plane_kvaddr is fail(%08X)", vctx, framemgr->id);
			ret = -EINVAL;
			goto exit;
		}

		frame->dvaddr_shot = queue->buf_dva[index][spare] + ext_size;
		frame->kvaddr_shot = queue->buf_kva[index][spare] + ext_size;
		frame->cookie_shot = queue->buf_box[index][spare];
		frame->shot = (struct camera2_shot *)frame->kvaddr_shot;
		frame->shot_ext = (struct camera2_shot_ext *)queue->buf_kva[index][spare];
		frame->shot_size = queue->framecfg.size[spare] - ext_size;
#ifdef MEASURE_TIME
		frame->tzone = (struct timeval *)frame->shot_ext->timeZone;
#endif
	} else {
		/* Create Kvaddr for frame sync */
		queue->buf_kva[index][spare] = vb2->plane_kvaddr(vb, spare);
		if (!queue->buf_kva[index][spare]) {
			merr("plane_kvaddr is fail(%08X)", vctx, framemgr->id);
			ret = -EINVAL;
			goto exit;
		}

		frame->stream = (struct camera2_stream *)queue->buf_kva[index][spare];
		frame->stream->address = queue->buf_kva[index][spare];
		frame->stream_size = queue->framecfg.size[spare];
	}

	set_bit(FRAME_INI_MEM, &frame->memory);

	queue->buf_refcount++;

	if (queue->buf_rdycount == queue->buf_refcount)
		set_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);

	if (queue->buf_maxcount == queue->buf_refcount)
		set_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);

exit:
	queue->buf_que++;
	return ret;
}

int fimc_is_queue_prepare(struct vb2_buffer *vb)
{
	struct fimc_is_video_ctx *vctx;

	BUG_ON(!vb);
	BUG_ON(!vb->vb2_queue);

	vctx = vb->vb2_queue->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return -EINVAL;
	}

	vctx->queue.buf_pre++;

	return 0;
}

void fimc_is_queue_wait_prepare(struct vb2_queue *vbq)
{
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_video *video;

	BUG_ON(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	mutex_unlock(&video->lock);
}

void fimc_is_queue_wait_finish(struct vb2_queue *vbq)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_video *video;

	BUG_ON(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	ret = mutex_lock_interruptible(&video->lock);
	if (ret)
		err("mutex_lock_interruptible is fail(%d)", ret);
}

int fimc_is_queue_start_streaming(struct fimc_is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	BUG_ON(!queue);

	if (test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		err("already stream on(%ld)", queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (queue->buf_rdycount && !test_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state)) {
		err("buffer state is not ready(%ld)", queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, start_streaming, qdevice, queue);
	if (ret) {
		err("start_streaming is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);

p_err:
	return ret;
}

int fimc_is_queue_stop_streaming(struct fimc_is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	BUG_ON(!queue);

	if (!test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		err("already stream off(%ld)", queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, stop_streaming, qdevice, queue);
	if (ret) {
		err("stop_streaming is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_frame_flush(&queue->framemgr);
	if (ret) {
		err("fimc_is_frame_flush is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	clear_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);

p_err:
	return ret;
}

int fimc_is_video_probe(struct fimc_is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct fimc_is_mem *mem,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_file_operations *fops,
	const struct v4l2_ioctl_ops *ioctl_ops)
{
	int ret = 0;
	u32 video_id;

	vref_init(video);
	mutex_init(&video->lock);
	snprintf(video->vd.name, sizeof(video->vd.name), "%s", video_name);
	video->id		= video_number;
	video->vb2		= mem->vb2;
	video->alloc_ctx	= mem->alloc_ctx;
	video->type		= (vfl_dir == VFL_DIR_RX) ? FIMC_IS_VIDEO_TYPE_CAPTURE : FIMC_IS_VIDEO_TYPE_LEADER;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	video->vd.vfl_dir	= vfl_dir;
#endif
	video->vd.v4l2_dev	= v4l2_dev;
	video->vd.fops		= fops;
	video->vd.ioctl_ops	= ioctl_ops;
	video->vd.minor		= -1;
	video->vd.release	= video_device_release;
	video->vd.lock		= &video->lock;
	video_set_drvdata(&video->vd, video);

	video_id = EXYNOS_VIDEONODE_FIMC_IS + video_number;
	ret = video_register_device(&video->vd, VFL_TYPE_GRABBER, video_id);
	if (ret) {
		err("Failed to register video device");
		goto p_err;
	}

p_err:
	info("[VID] %s(%d) is created\n", video_name, video_id);
	return ret;
}

int fimc_is_video_open(struct fimc_is_video_ctx *vctx,
	void *device,
	u32 buf_rdycount,
	struct fimc_is_video *video,
	const struct vb2_ops *vb2_ops,
	const struct fimc_is_queue_ops *qops)
{
	int ret = 0;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!video);
	BUG_ON(!video->vb2);
	BUG_ON(!vb2_ops);

	if (!(vctx->state & BIT(FIMC_IS_VIDEO_CLOSE))) {
		err("[V%02d] already open(%lX)", video->id, vctx->state);
		return -EEXIST;
	}

	queue = GET_QUEUE(vctx);
	queue->vbq = NULL;
	queue->qops = qops;

	vctx->device		= device;
	vctx->subdev		= NULL;
	vctx->video		= video;
	vctx->vb2_ops		= vb2_ops;
	vctx->mem_ops		= video->vb2->ops;
	mutex_init(&vctx->lock);

	ret = fimc_is_queue_open(queue, buf_rdycount);
	if (ret) {
		err("fimc_is_queue_open is fail(%d)", ret);
		goto p_err;
	}

	queue->vbq = kzalloc(sizeof(struct vb2_queue), GFP_KERNEL);
	if (!queue->vbq) {
		err("kzalloc is fail");
		ret = -ENOMEM;
		goto p_err;
	}

	ret = queue_init(vctx, queue->vbq, NULL);
	if (ret) {
		err("queue_init fail");
		kfree(queue->vbq);
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_OPEN);

p_err:
	return ret;
}

int fimc_is_video_close(struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (vctx->state < BIT(FIMC_IS_VIDEO_OPEN)) {
		mverr("already close(%lX)", vctx, video, vctx->state);
		return -ENOENT;
	}

	fimc_is_queue_close(queue);
	vb2_queue_release(queue->vbq);
	kfree(queue->vbq);

	/*
	 * vb2 release can call stop callback
	 * not if video node is not stream off
	 */
	vctx->device = NULL;
	vctx->state = BIT(FIMC_IS_VIDEO_CLOSE);

	return ret;
}

int fimc_is_video_s_input(struct file *file,
	struct fimc_is_video_ctx *vctx)
{
	u32 ret = 0;

	if (!(vctx->state & (BIT(FIMC_IS_VIDEO_OPEN) | BIT(FIMC_IS_VIDEO_S_INPUT) | BIT(FIMC_IS_VIDEO_S_BUFS)))) {
		err("[V%02d] invalid s_input is requested(%lX)", vctx->video->id, vctx->state);
		return -EINVAL;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_S_INPUT);

	return ret;
}

int fimc_is_video_poll(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);

	queue = GET_QUEUE(vctx);
	ret = vb2_poll(queue->vbq, file, wait);

	return ret;
}

int fimc_is_video_mmap(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct vm_area_struct *vma)
{
	u32 ret = 0;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);

	queue = GET_QUEUE(vctx);

	ret = vb2_mmap(queue->vbq, vma);

	return ret;
}

int fimc_is_video_reqbufs(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_requestbuffers *request)
{
	int ret = 0;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;

	BUG_ON(!vctx);
	BUG_ON(!request);

	if (!(vctx->state & (BIT(FIMC_IS_VIDEO_S_FORMAT) | BIT(FIMC_IS_VIDEO_STOP) | BIT(FIMC_IS_VIDEO_S_BUFS)))) {
		err("[V%02d] invalid reqbufs is requested(%lX)", vctx->video->id, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	if (test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		err("video is stream on, not applied");
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_reqbufs(queue->vbq, request);
	if (ret) {
		err("vb2_reqbufs is fail(%d)", ret);
		goto p_err;
	}

	framemgr = &queue->framemgr;
	queue->buf_maxcount = request->count;
	if (queue->buf_maxcount == 0) {
		queue->buf_req = 0;
		queue->buf_pre = 0;
		queue->buf_que = 0;
		queue->buf_com = 0;
		queue->buf_dqe = 0;
		queue->buf_refcount = 0;
		clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
		clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
		fimc_is_frame_close(framemgr);
	} else {
		if (queue->buf_maxcount < queue->buf_rdycount) {
			err("buffer count is not invalid(%d < %d)",
				queue->buf_maxcount, queue->buf_rdycount);
			ret = -EINVAL;
			goto p_err;
		}

		ret = fimc_is_frame_open(framemgr, queue->buf_maxcount);
		if (ret) {
			err("[V%02d] fimc_is_frame_open is fail(%d)", vctx->video->id, ret);
			goto p_err;
		}
	}

	vctx->state = BIT(FIMC_IS_VIDEO_S_BUFS);

p_err:
	return ret;
}

int fimc_is_video_querybuf(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_queue *queue;

	queue = GET_QUEUE(vctx);

	ret = vb2_querybuf(queue->vbq, buf);

	return ret;
}

int fimc_is_video_set_format_mplane(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 condition;
	void *device;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!format);

	device = GET_DEVICE(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	/* capture video node can skip s_input */
	if (video->type  == FIMC_IS_VIDEO_TYPE_LEADER)
		condition = BIT(FIMC_IS_VIDEO_S_INPUT) | BIT(FIMC_IS_VIDEO_S_BUFS);
	else
		condition = BIT(FIMC_IS_VIDEO_S_INPUT) | BIT(FIMC_IS_VIDEO_S_BUFS) | BIT(FIMC_IS_VIDEO_OPEN);

	if (!(vctx->state & condition)) {
		err("[V%02d] invalid s_format is requested(%lX)", video->id, vctx->state);
		return -EINVAL;
	}

	ret = fimc_is_queue_set_format_mplane(queue, device, format);
	if (ret) {
		err("[V%02d] fimc_is_queue_set_format_mplane is fail(%d)", video->id, ret);
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_S_FORMAT);

p_err:
	mdbgv_vid("set_format(%d x %d)\n", queue->framecfg.width, queue->framecfg.height);
	return ret;
}

int fimc_is_video_qbuf(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;

	BUG_ON(!file);
	BUG_ON(!vctx);
	BUG_ON(!buf);

	buf->flags &= ~V4L2_BUF_FLAG_USE_SYNC;
	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;

	if (!vbq) {
		merr("vbq is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->fileio) {
		merr("file io in progress", vctx);
		ret = -EBUSY;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		merr("buf type is invalid(%d != %d)", vctx,
			buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->index >= vbq->num_buffers) {
		merr("buffer index%d out of range", vctx, buf->index);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->memory != vbq->memory) {
		merr("invalid memory type%d", vctx, buf->memory);
		ret = -EINVAL;
		goto p_err;
	}

	vb = vbq->bufs[buf->index];
	if (!vb) {
		merr("vb is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_req++;

	ret = vb2_qbuf(queue->vbq, buf);
	if (ret) {
		merr("vb2_qbuf is fail(index : %d, %d)", vctx, buf->index, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_video_dqbuf(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	bool blocking;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!file);
	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!buf);

	blocking = file->f_flags & O_NONBLOCK;
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		mverr("buf type is invalid(%d != %d)", vctx, video, buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("queue is not streamon(%ld)", vctx,  video, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_dqe++;

	ret = vb2_dqbuf(queue->vbq, buf, blocking);
	if (ret) {
		mverr("vb2_dqbuf is fail(%d)", vctx,  video, ret);
		goto p_err;
	}

#ifdef DBG_IMAGE_DUMP
	if ((vctx->video->id == DBG_IMAGE_DUMP_VIDEO) && (buf->index == DBG_IMAGE_DUMP_INDEX))
		imgdump_request(queue->buf_box[buf->index][0], queue->buf_kva[buf->index][0], queue->framecfg.size[0]);
#endif

p_err:
	return ret;
}

int fimc_is_video_streamon(struct file *file,
	struct fimc_is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct vb2_queue *vbq;

	BUG_ON(!file);
	BUG_ON(!vctx);

	if (!(vctx->state & (BIT(FIMC_IS_VIDEO_S_BUFS) | BIT(FIMC_IS_VIDEO_STOP)))) {
		err("[V%02d] invalid streamon is requested(%lX)", vctx->video->id, vctx->state);
		return -EINVAL;
	}

	vbq = GET_QUEUE(vctx)->vbq;
	if (!vbq) {
		merr("vbq is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->type != type) {
		merr("invalid stream type(%d != %d)", vctx, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->streaming) {
		merr("streamon: already streaming", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamon(vbq, type);
	if (ret) {
		err("[V%02d] vb2_streamon is fail(%d)", vctx->video->id, ret);
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_START);

p_err:
	return ret;
}

int fimc_is_video_streamoff(struct file *file,
	struct fimc_is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	u32 qcount;
	struct fimc_is_queue *queue;
	struct vb2_queue *vbq;
	struct fimc_is_framemgr *framemgr;

	BUG_ON(!file);
	BUG_ON(!vctx);

	if (!(vctx->state & BIT(FIMC_IS_VIDEO_START))) {
		err("[V%02d] invalid streamoff is requested(%lX)", vctx->video->id, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	framemgr = &queue->framemgr;
	vbq = queue->vbq;
	if (!vbq) {
		merr("vbq is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irq(framemgr, FMGR_IDX_0);
	qcount = framemgr->frame_req_cnt +
		framemgr->frame_pro_cnt +
		framemgr->frame_com_cnt;
	framemgr_x_barrier_irq(framemgr, FMGR_IDX_0);

	if (qcount > 0)
		mwarn("video%d stream off : queued buffer is not empty(%d)", vctx,
			vctx->video->id, qcount);

	if (vbq->type != type) {
		merr("invalid stream type(%d != %d)", vctx, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!vbq->streaming) {
		merr("streamoff: not streaming", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamoff(vbq, type);
	if (ret) {
		err("[V%02d] vb2_streamoff is fail(%d)", vctx->video->id, ret);
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_STOP);

p_err:
	return ret;
}

int fimc_is_video_s_ctrl(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	/* hack for 64bit addr */
	ulong value_to_addr = 0;
	struct fimc_is_video *video;
	struct fimc_is_device_ischain *device;
	struct fimc_is_resourcemgr *resourcemgr;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!ctrl);

	device = GET_DEVICE(vctx);
	video = GET_VIDEO(vctx);
	resourcemgr = device->resourcemgr;

	switch (ctrl->id) {
	case V4L2_CID_IS_END_OF_STREAM:
		ret = fimc_is_ischain_open_wrap(device, true);
		if (ret) {
			merr("fimc_is_ischain_open_wrap is fail(%d)", device, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_IS_SET_SETFILE:
		if (test_bit(FIMC_IS_ISCHAIN_START, &device->state)) {
			merr("device is already started, setfile applying is fail", device);
			ret = -EINVAL;
			goto p_err;
		}

		device->setfile = ctrl->value;
		break;
	case V4L2_CID_IS_HAL_VERSION:
		if (ctrl->value < 0 || ctrl->value >= IS_HAL_VER_MAX) {
			merr("hal version(%d) is invalid", device, ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		resourcemgr->hal_version = ctrl->value;
		break;
	case V4L2_CID_IS_DEBUG_DUMP:
		info("Print fimc-is info dump by HAL");
		fimc_is_hw_logdump(device->interface);
		fimc_is_hw_regdump(device->interface);
		CALL_POPS(device, print_clk);

		if (ctrl->value)
			panic("intentional panic from camera HAL");
		break;
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
		fimc_is_logsync(device->interface, ctrl->value, IS_MSG_TEST_SYNC_LOG);
		break;
	case V4L2_CID_IS_MAP_BUFFER:
		{
			struct fimc_is_queue *queue;
			struct fimc_is_framemgr *framemgr;
			struct fimc_is_frame *frame;
			struct dma_buf *dmabuf;
			struct dma_buf_attachment *attachment;
			dma_addr_t dva;
			struct v4l2_buffer *buf;
			struct v4l2_plane *planes;
			size_t size;
			u32 plane, group_id;

			size = sizeof(struct v4l2_buffer);
			buf = kmalloc(size, GFP_KERNEL);
			if (!buf) {
				mverr("kmalloc is fail(%p)", device, video, buf);
				ret = -EINVAL;
				goto p_err;
			}
			/* hack for 64bit addr */
			value_to_addr = ctrl->value;

			ret = copy_from_user(buf, (void __user *)value_to_addr, size);
			if (ret) {
				mverr("copy_from_user is fail(%d)", device, video, ret);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			if (!V4L2_TYPE_IS_OUTPUT(buf->type)) {
				mverr("capture video type is not supported", device, video);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			if (!V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
				mverr("single plane is not supported", device, video);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			if (buf->index >= FRAMEMGR_MAX_REQUEST) {
				mverr("buffer index is invalid(%d)", device, video, buf->index);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			if (buf->length > VIDEO_MAX_PLANES) {
				mverr("planes[%d] is invalid", device, video, buf->length);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			queue = GET_QUEUE(vctx);
			if (queue->vbq->memory != V4L2_MEMORY_DMABUF) {
				mverr("memory type(%d) is not supported", device, video, queue->vbq->memory);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			size = sizeof(struct v4l2_plane) * buf->length;
			planes = kmalloc(size, GFP_KERNEL);
			if (IS_ERR(planes)) {
				mverr("kmalloc is fail(%p)", device, video, planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			ret = copy_from_user(planes, (void __user *)buf->m.planes, size);
			if (ret) {
				mverr("copy_from_user is fail(%d)", device, video, ret);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			framemgr = &queue->framemgr;
			frame = &framemgr->frame[buf->index];
			if (test_bit(FRAME_MAP_MEM, &frame->memory)) {
				mverr("this buffer(%d) is already mapped", device, video, buf->index);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			/* only last buffer need to map */
			if (buf->length <= 1) {
				mverr("this buffer(%d) have no meta plane", device, video, buf->length);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			plane = buf->length - 1;
			dmabuf = dma_buf_get(planes[plane].m.fd);
			if (IS_ERR(dmabuf)) {
				mverr("dma_buf_get is fail(%p)", device, video, dmabuf);
				kfree(planes);
				kfree(buf);
				ret = -EINVAL;
				goto p_err;
			}

			attachment = dma_buf_attach(dmabuf, &device->pdev->dev);
			if (IS_ERR(attachment)) {
				mverr("dma_buf_attach is fail(%p)", device, video, attachment);
				kfree(planes);
				kfree(buf);
				dma_buf_put(dmabuf);
				ret = -EINVAL;
				goto p_err;
			}

			/* only support output(read) video node */
			dva = ion_iovmm_map(attachment, 0, dmabuf->size, 0, plane);
			if (IS_ERR_VALUE(dva)) {
				mverr("ion_iovmm_map is fail(%pa)", device, video, &dva);
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
				mverr("fimc_is_itf_map is fail(%d)", device, video, ret);
				kfree(planes);
				kfree(buf);
				dma_buf_detach(dmabuf, attachment);
				dma_buf_put(dmabuf);
				goto p_err;
			}

			mvinfo(" B%d.P%d MAP\n", device, video, buf->index, plane);
			set_bit(FRAME_MAP_MEM, &frame->memory);
			dma_buf_detach(dmabuf, attachment);
			dma_buf_put(dmabuf);
			kfree(planes);
			kfree(buf);
		}
		break;
	default:
		err("unsupported ioctl(0x%X)", ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

int buffer_done(struct fimc_is_video_ctx *vctx,
	u32 index, u32 state)
{
	int ret = 0;
	struct vb2_buffer *vb;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!vctx->video);
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	queue = GET_QUEUE(vctx);

	if (!queue->vbq) {
		err("vbq is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	vb = queue->vbq->bufs[index];
	if (!vb) {
		err("vb is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		warn("%d video queue is not stream on", vctx->video->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (vb->state != VB2_BUF_STATE_ACTIVE) {
		err("vb buffer[%d] state is not active(%d)", index, vb->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_com++;

	vb2_buffer_done(vb, state);

p_err:
	return ret;
}

