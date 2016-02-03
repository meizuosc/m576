/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
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

#include "fimc-is-device-companion.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-video.h"
#include "fimc-is-resourcemgr.h"

const struct v4l2_file_operations fimc_is_comp_video_fops;
const struct v4l2_ioctl_ops fimc_is_comp_video_ioctl_ops;
const struct vb2_ops fimc_is_comp_qops;

int fimc_is_comp_video_probe(void *data)
{
	int ret = 0;
	char name[255];
	u32 number;
	struct fimc_is_device_companion *device;
	struct fimc_is_video *video;

	BUG_ON(!data);

	device = (struct fimc_is_device_companion *)data;
	video = &device->video;
	video->resourcemgr = device->resourcemgr;
	snprintf(name, sizeof(name), "%s%d", FIMC_IS_VIDEO_SSX_NAME, 8);
	number = FIMC_IS_VIDEO_SS0_NUM + 8;

	if (!device->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		name,
		number,
		VFL_DIR_RX,
		&device->mem,
		&device->v4l2_dev,
		&fimc_is_comp_video_fops,
		&fimc_is_comp_video_ioctl_ops);
	if (ret)
		dev_err(&device->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/*
 * =============================================================================
 * Video File Opertation
 * =============================================================================
 */

static int fimc_is_comp_video_open(struct file *file)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_companion *device;
	struct fimc_is_resourcemgr *resourcemgr;

	vctx = NULL;
	video = video_drvdata(file);
	device = container_of(video, struct fimc_is_device_companion, video);
	resourcemgr = video->resourcemgr;
	if (!resourcemgr) {
		err("resourcemgr is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_resource_open(resourcemgr, RESOURCE_TYPE_COMPANION, NULL);
	if (ret) {
		err("fimc_is_resource_open is fail(%d)", ret);
		goto p_err;
	}

	minfo("[COM:V] %s\n", device, __func__);

	ret = open_vctx(file, video, &vctx, device->instance, FRAMEMGR_ID_INVALID);
	if (ret) {
		err("open_vctx is fail(%d)", ret);
		goto p_err;
	}

	vctx->device = device;

	ret = fimc_is_companion_open(device, vctx);
	if (ret) {
		merr("fimc_is_comp_open is fail(%d)", device, ret);
		close_vctx(file, video, vctx);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_comp_video_close(struct file *file)
{
	int ret = 0;
	int refcount;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_video *video = NULL;
	struct fimc_is_device_companion *device = NULL;

	BUG_ON(!vctx);

	video = video_drvdata(file);
	device = container_of(video, struct fimc_is_device_companion, video);

	ret = fimc_is_companion_close(device);
	if (ret)
		err("fimc_is_companion_close is fail(%d)", ret);

	vctx->device = NULL;

	refcount = close_vctx(file, video, vctx);
	if (refcount < 0)
		merr("close_vctx is fail(%d)", device, refcount);

	minfo("[COM:V] %s():%d\n", device, __func__, ret);

	return ret;
}

const struct v4l2_file_operations fimc_is_comp_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_comp_video_open,
	.release	= fimc_is_comp_video_close,
	.unlocked_ioctl	= video_ioctl2,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int fimc_is_comp_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_companion *device = NULL;

	BUG_ON(!vctx);

	mdbgv_sensor("%s(input : %08X)\n", vctx, __func__, input);

	device = GET_DEVICE(vctx);
	scenario = (input & SENSOR_SCENARIO_MASK) >> SENSOR_SCENARIO_SHIFT;
	input = (input & SENSOR_MODULE_MASK) >> SENSOR_MODULE_SHIFT;

	ret = fimc_is_companion_s_input(device, input, scenario);
	if (ret) {
		merr("fimc_is_companion_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops fimc_is_comp_video_ioctl_ops = {
	.vidioc_s_input			= fimc_is_comp_video_s_input,
};

const struct vb2_ops fimc_is_comp_qops = {
};
