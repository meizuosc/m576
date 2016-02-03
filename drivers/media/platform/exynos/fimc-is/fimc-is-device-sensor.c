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
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/i2c.h>

#include <mach/map.h>
#include <mach/devfreq.h>
#include <mach/regs-clock.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-dt.h"
#include "fimc-is-dvfs.h"

#include "sensor/fimc-is-device-6b2.h"
#include "sensor/fimc-is-device-imx135.h"
#include "fimc-is-device-sensor.h"
#ifdef CONFIG_COMPANION_USE
#include "fimc-is-companion-dt.h"
#endif

extern struct device *camera_front_dev;
extern struct device *camera_rear_dev;
int fimc_is_sensor_runtime_resume(struct device *dev);
int fimc_is_sensor_runtime_suspend(struct device *dev);

extern int fimc_is_sen_video_probe(void *data);
struct pm_qos_request exynos_sensor_qos_cam;
struct pm_qos_request exynos_sensor_qos_int;
struct pm_qos_request exynos_sensor_qos_mem;

extern u32 __iomem *notify_fcount_sen0;
extern u32 __iomem *notify_fcount_sen1;
extern u32 __iomem *notify_fcount_sen2;
u32 notify_fcount_sen0_fw;
u32 notify_fcount_sen1_fw;
u32 notify_fcount_sen2_fw;
u32 notify_fcount_dummy;

#define BINNING(x, y) roundup((x) * 1000 / (y), 250)

int fimc_is_sensor_read8(struct i2c_client *client,
	u16 addr, u8 *val)
{
	int ret = 0;
	struct i2c_msg msg[2];
	u8 wbuf[2];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 2;
	msg[0].buf = wbuf;
	/* TODO : consider other size of buffer */
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		err("i2c treansfer fail");
		goto p_err;
	}

#ifdef PRINT_I2CCMD
	info("I2CR08(%d) [0x%04X] : 0x%04X\n", client->addr, addr, *val);
#endif

p_err:
	return ret;
}

int fimc_is_sensor_read16(struct i2c_client *client,
	u16 addr, u16 *val)
{
	int ret = 0;
	struct i2c_msg msg[2];
	u8 wbuf[2], rbuf[2];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	/* 1. I2C operation for writing. */
	msg[0].addr = client->addr;
	msg[0].flags = 0; /* write : 0, read : 1 */
	msg[0].len = 2;
	msg[0].buf = wbuf;
	/* TODO : consider other size of buffer */
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);

	/* 2. I2C operation for reading data. */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rbuf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		err("i2c treansfer fail");
		goto p_err;
	}

	*val = ((rbuf[0] << 8) | rbuf[1]);

#ifdef PRINT_I2CCMD
	info("I2CR16(%d) [0x%04X] : 0x%04X\n", client->addr, addr, *val);
#endif

p_err:
	return ret;
}

int fimc_is_sensor_write(struct i2c_client *client,
	u8 *buf, u32 size)
{
	int ret = 0;
	int retry_count = 5;
	struct i2c_msg msg = {client->addr, 0, size, buf};

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		msleep(10);
	} while (retry_count-- > 0);

	if (ret != 1) {
		dev_err(&client->dev, "%s: I2C is not working.\n", __func__);
		return -EIO;
	}

	return 0;
}

int fimc_is_sensor_write8(struct i2c_client *client,
	u16 addr, u8 val)
{
	int ret = 0;
	struct i2c_msg msg[1];
	u8 wbuf[3];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 3;
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	wbuf[2] = val;

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		err("i2c treansfer fail(%d)", ret);
		goto p_err;
	}

#ifdef PRINT_I2CCMD
	info("I2CW08(%d) [0x%04X] : 0x%04X\n", client->addr, addr, val);
#endif

p_err:
	return ret;
}

int fimc_is_sensor_write16(struct i2c_client *client,
	u16 addr, u16 val)
{
	int ret = 0;
	struct i2c_msg msg[1];
	u8 wbuf[4];

	if (!client->adapter) {
		err("Could not find adapter!\n");
		ret = -ENODEV;
		goto p_err;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = wbuf;
	wbuf[0] = (addr & 0xFF00) >> 8;
	wbuf[1] = (addr & 0xFF);
	wbuf[2] = (val & 0xFF00) >> 8;
	wbuf[3] = (val & 0xFF);

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0) {
		err("i2c treansfer fail(%d)", ret);
		goto p_err;
	}

#ifdef PRINT_I2CCMD
	info("I2CW16(%d) [0x%04X] : 0x%04X\n", client->addr, addr, val);
#endif

p_err:
	return ret;
}

#if defined(CONFIG_PM_DEVFREQ)
inline static void fimc_is_sensor_set_qos_init(struct fimc_is_device_sensor *device, bool on)
{
	int cam_qos = 0;
	int int_qos = 0;
	int mif_qos = 0;
	struct fimc_is_core *core =
		(struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);

	cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, START_DVFS_LEVEL);
	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, START_DVFS_LEVEL);

	if (on) {
		/* DEVFREQ lock */
		if (cam_qos > 0) {
			if (device->request_cam_qos == false) {
				pm_qos_add_request(&exynos_sensor_qos_cam, PM_QOS_CAM_THROUGHPUT, cam_qos);
				device->request_cam_qos = true;
			} else {
				err("Adding sensor cam_qos is not allowed");
			}
		}

		if (int_qos > 0) {
			if (device->request_int_qos == false) {
				pm_qos_add_request(&exynos_sensor_qos_int, PM_QOS_DEVICE_THROUGHPUT, int_qos);
				device->request_int_qos = true;
			} else {
				err("Adding sensor int_qos is not allowed");
			}
		}

		if (mif_qos > 0) {
			if (device->request_mif_qos == false) {
				pm_qos_add_request(&exynos_sensor_qos_mem, PM_QOS_BUS_THROUGHPUT, mif_qos);
				device->request_mif_qos = true;
			} else {
				err("Adding sensor mif_qos is not allowed");
			}
		}
		minfo("[SEN:D] %s: QoS LOCK [INT(%d), MIF(%d), CAM(%d)]\n", device,
				__func__, int_qos, mif_qos, cam_qos);
	} else {
		/* DEVFREQ unlock */
		if (cam_qos > 0) {
			if (device->request_cam_qos == true) {
				pm_qos_remove_request(&exynos_sensor_qos_cam);
				device->request_cam_qos = false;
			} else {
				err("Removing sensor cam_qos is not allowed");
			}
		}

		if (int_qos > 0) {
			if (device->request_int_qos == true) {
				pm_qos_remove_request(&exynos_sensor_qos_int);
				device->request_int_qos = false;
			} else {
				err("Removing sensor int_qos is not allowed");
			}
		}

		if (mif_qos > 0) {
			if (device->request_mif_qos == true) {
				pm_qos_remove_request(&exynos_sensor_qos_mem);
				device->request_mif_qos = false;
			} else {
				err("Removing sensor mif_qos is not allowed");
			}
		}
		minfo("[SEN:D] %s: QoS UNLOCK\n", device, __func__);
	}
}

inline static void fimc_is_sensor_set_qos_update(struct fimc_is_device_sensor *device, u32 scenario)
{
	int cam_qos = 0;
	int int_qos = 0;
	int mif_qos = 0;
	struct fimc_is_core *core =
		(struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);

	/* HACK: This is considerated only front camera vision scenario. */
	if (scenario == SENSOR_SCENARIO_VISION) {
		cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, FIMC_IS_SN_FRONT_PREVIEW);
		int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, FIMC_IS_SN_FRONT_PREVIEW);
	}

	/* DEVFREQ update */
	if (cam_qos > 0)
		pm_qos_update_request(&exynos_sensor_qos_cam, cam_qos);
	if (int_qos > 0)
		pm_qos_update_request(&exynos_sensor_qos_int, int_qos);
	if (mif_qos > 0)
		pm_qos_update_request(&exynos_sensor_qos_mem, mif_qos);

	minfo("[SEN:D] %s: QoS UPDATE(%d) [INT(%d), MIF(%d), CAM(%d)]\n", device,
			__func__, scenario, int_qos, mif_qos, cam_qos);
}
#endif

static int get_sensor_mode(struct fimc_is_sensor_cfg *cfg,
	u32 cfgs, u32 width, u32 height, u32 framerate)
{
	int mode = -1;
	int idx = -1;
	u32 i;

	/* find sensor mode by w/h and fps range */
	for (i = 0; i < cfgs; i++) {
		if ((cfg[i].width == width) &&
				(cfg[i].height == height)) {
			if (cfg[i].framerate == framerate) {
				/* You don't need to find another sensor mode */
				mode = cfg[i].mode;
				idx = i;
				break;
			} else if (cfg[i].framerate > framerate) {
				/* try to find framerate smaller than previous */
				if (mode < 0) {
					mode = cfg[i].mode;
					idx = i;
				} else {
					/* try to compare previous with current */
					if (cfg[idx].framerate > cfg[i].framerate) {
						mode = cfg[i].mode;
						idx = i;
					}
				}
			}
		}
	}

	if (idx < 0)
		err("could not find proper sensor mode: %dx%d@%dfps",
			width, height, framerate);
	else
		pr_info("sensor mode(%dx%d@%d) = %d\n",
			cfg[idx].width,
			cfg[idx].height,
			cfg[idx].framerate,
			cfg[idx].mode);

	return mode;
}

static int fimc_is_sensor_mclk_on(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (test_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state)) {
		merr("%s : already clk on", device, __func__);
		goto p_err;
	}

	if (!pdata->mclk_on) {
		merr("mclk_on is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_on(device->pdev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		merr("mclk_on is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_sensor_mclk_off(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state)) {
		merr("%s : already clk off", device, __func__);
		goto p_err;
	}

	if (!pdata->mclk_off) {
		merr("mclk_off is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_off(device->pdev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		merr("mclk_off is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_sensor_iclk_on(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);
	BUG_ON(!device->private_data);

	core = device->private_data;
	pdata = device->pdata;

	if (test_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state)) {
		merr("%s : already clk on", device, __func__);
		goto p_err;
	}

	if (!pdata->iclk_cfg) {
		merr("iclk_cfg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->iclk_on) {
		merr("iclk_on is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_cfg(core->pdev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_cfg is fail(%d)", device, ret);
		goto p_err;
	}

	ret = pdata->iclk_on(core->pdev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_on is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_sensor_iclk_off(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);
	BUG_ON(!device->private_data);

	core = device->private_data;
	pdata = device->pdata;

	if (!test_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state)) {
		merr("%s : already clk off", device, __func__);
		goto p_err;
	}

	if (!pdata->iclk_off) {
		merr("iclk_off is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_off(core->pdev, pdata->scenario, pdata->csi_ch);
	if (ret) {
		merr("iclk_off is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_sensor_gpio_on(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (test_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state)) {
		merr("%s : already gpio on", device, __func__);
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		merr("gpio_cfg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (pdata->id == 0) {
		core->running_rear_camera = true;
	} else {
		core->running_front_camera = true;
	}

	ret = pdata->gpio_cfg(device->pdev, pdata->scenario, GPIO_SCENARIO_ON);
	if (ret) {
		merr("gpio_cfg is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_sensor_gpio_off(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state)) {
		merr("%s : already gpio off", device, __func__);
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		merr("gpio_cfg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_cfg(device->pdev, pdata->scenario, GPIO_SCENARIO_OFF);
	if (ret) {
		merr("gpio_cfg is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);

p_err:
	if (pdata->id == 0) {
		core->running_rear_camera = false;
	} else {
		core->running_front_camera = false;
	}
	return ret;
}

#ifdef ENABLE_DTP
static void fimc_is_sensor_dtp(unsigned long data)
{
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_sensor *device = (struct fimc_is_device_sensor *)data;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	unsigned long flags;
	u32 i;

	BUG_ON(!device);

	err("forcely reset due to 0x%08lx", device->force_stop);
	device->force_stop = 0;

	set_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

	if (device->ischain) {
		set_bit(FIMC_IS_GROUP_FORCE_STOP, &device->ischain->group_3aa.state);
		set_bit(FIMC_IS_GROUP_FORCE_STOP, &device->ischain->group_isp.state);
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->ischain->group_3aa.state))
			up(&device->ischain->group_3aa.smp_trigger);
	}

	vctx = device->vctx;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	queue = GET_DST_QUEUE(vctx);
	framemgr = &queue->framemgr;
	if ((framemgr->frame_cnt == 0) || (framemgr->frame_cnt > FRAMEMGR_MAX_REQUEST)) {
		err("frame count of framemgr is invalid(%d)", framemgr->frame_cnt);
		return;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_4, flags);

	for (i = 0; i < framemgr->frame_cnt; i++) {
		frame = &framemgr->frame[i];
		if (frame->state == FIMC_IS_FRAME_STATE_REQUEST) {
			err("buffer done1!!!! %d", i);
			fimc_is_frame_trans_req_to_com(framemgr, frame);
			queue_done(vctx, queue, i, VB2_BUF_STATE_ERROR);
		} else if (frame->state == FIMC_IS_FRAME_STATE_PROCESS) {
			err("buffer done2!!!! %d", i);
			fimc_is_frame_trans_pro_to_com(framemgr, frame);
			queue_done(vctx, queue, i, VB2_BUF_STATE_ERROR);
		}
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_4, flags);
}
#endif

static int fimc_is_sensor_start(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	BUG_ON(!device);

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		struct v4l2_subdev *subdev;

		subdev = device->subdev_module;
		if (!subdev) {
			merr("subdev is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}
		ret = v4l2_subdev_call(subdev, video, s_stream, true);
		if (ret) {
			merr("v4l2_subdev_call(s_stream) is fail(%d)", device, ret);
			goto p_err;
		}
	} else {
		struct fimc_is_device_ischain *ischain;

		ischain = device->ischain;
		if (!ischain) {
			merr("ischain is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		ret = fimc_is_itf_stream_on(ischain);
		if (ret) {
			merr("fimc_is_itf_stream_on is fail(%d)", device, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int fimc_is_sensor_stop(struct fimc_is_device_sensor *device)
{
	int ret = 0;

	BUG_ON(!device);

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		struct v4l2_subdev *subdev;

		subdev = device->subdev_module;
		if (!subdev) {
			merr("subdev is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}
		ret = v4l2_subdev_call(subdev, video, s_stream, false);
		if (ret) {
			merr("v4l2_subdev_call(s_stream) is fail(%d)", device, ret);
			goto p_err;
		}
	} else {
		struct fimc_is_device_ischain *ischain;

		ischain = device->ischain;
		if (!ischain) {
			merr("ischain is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}
		ret = fimc_is_itf_stream_off(ischain);
		if (ret) {
			merr("fimc_is_itf_stream_off is fail(%d)", device, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int fimc_is_sensor_tag(struct fimc_is_device_sensor *device,
	struct fimc_is_frame *frame)
{
	int ret = 0;
	frame->shot->dm.request.frameCount = frame->fcount;
	frame->shot->dm.sensor.timeStamp = fimc_is_get_timestamp();

	return ret;
}

static void fimc_is_sensor_control(struct work_struct *data)
{
/*
 * HAL can't send meta data for vision
 * We accepted vision control by s_ctrl
 */
#if 0
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct camera2_sensor_ctl *rsensor_ctl;
	struct camera2_sensor_ctl *csensor_ctl;
	struct fimc_is_device_sensor *device;

	device = container_of(data, struct fimc_is_device_sensor, control_work);
	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		return;
	}

	module = v4l2_get_subdevdata(subdev_module);
	rsensor_ctl = &device->control_frame->shot->ctl.sensor;
	csensor_ctl = &device->sensor_ctl;

	if (rsensor_ctl->exposureTime != csensor_ctl->exposureTime) {
		CALL_MOPS(module, s_exposure, subdev_module, rsensor_ctl->exposureTime);
		csensor_ctl->exposureTime = rsensor_ctl->exposureTime;
	}

	if (rsensor_ctl->frameDuration != csensor_ctl->frameDuration) {
		CALL_MOPS(module, s_duration, subdev_module, rsensor_ctl->frameDuration);
		csensor_ctl->frameDuration = rsensor_ctl->frameDuration;
	}

	if (rsensor_ctl->sensitivity != csensor_ctl->sensitivity) {
		CALL_MOPS(module, s_again, subdev_module, rsensor_ctl->sensitivity);
		csensor_ctl->sensitivity = rsensor_ctl->sensitivity;
	}
#endif
}

static int fimc_is_sensor_notify_by_fstr(struct fimc_is_device_sensor *device, void *arg)
{
	int ret = 0;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!device);
	BUG_ON(!arg);

	device->fcount = *(u32 *)arg;
	framemgr = GET_DST_FRAMEMGR(device->vctx);

	if (device->instant_cnt) {
		device->instant_cnt--;
		if (device->instant_cnt == 0)
			wake_up(&device->instant_wait);
	}

	framemgr_e_barrier(framemgr, FMGR_IDX_5);

	fimc_is_frame_process_head(framemgr, &frame);
	if (frame) {
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
		do_gettimeofday(&frame->tzone[TM_FLITE_STR]);
#endif
#endif
		if (frame->has_fcount) {
			struct list_head *temp;
			struct fimc_is_frame *next_frame;
			bool finded = false;

			list_for_each(temp, &framemgr->frame_process_head) {
				next_frame = list_entry(temp, struct fimc_is_frame, list);
				if (next_frame->has_fcount) {
					continue;
				} else {
					finded = true;
					break;
				}
			}

			if (finded) {
				/* finded frame in processing frame list */
				next_frame->has_fcount = true;
				next_frame->fcount = device->fcount;
				fimc_is_sensor_tag(device, next_frame);
			}
		} else {
			frame->fcount = device->fcount;
			fimc_is_sensor_tag(device, frame);
			frame->has_fcount = true;
		}
	}
#ifdef TASKLET_MSG
	if (!frame) {
		merr("[SEN] process is empty", device);
		fimc_is_frame_print_all(framemgr);
	}
#endif

	framemgr_x_barrier(framemgr, FMGR_IDX_5);

	return ret;
}

static int fimc_is_sensor_notify_by_fend(struct fimc_is_device_sensor *device, void *arg)
{
	int ret = 0;
	struct fimc_is_frame *frame;

	BUG_ON(!device);
	BUG_ON(!device->vctx);

#ifdef ENABLE_DTP
	if (device->dtp_check) {
		device->dtp_check = false;
		del_timer(&device->dtp_timer);
	}

	if (device->force_stop)
		fimc_is_sensor_dtp((unsigned long)device);
#endif

	frame = (struct fimc_is_frame *)arg;
	if (frame) {
		frame->has_fcount = false;
		buffer_done(device->vctx, frame->index);

		/* device driving */
		if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
			device->control_frame = frame;
			schedule_work(&device->control_work);
		}
	}

	return ret;
}

static void fimc_is_sensor_notify(struct v4l2_subdev *subdev,
	unsigned int notification,
	void *arg)
{
	int ret = 0;
	struct fimc_is_device_sensor *device;

	BUG_ON(!subdev);

	device = v4l2_get_subdev_hostdata(subdev);

	switch(notification) {
	case FLITE_NOTIFY_FSTART:
		ret = fimc_is_sensor_notify_by_fstr(device, arg);
		if (ret)
			merr("fimc_is_sensor_notify_by_fstr is fail(%d)", device, ret);
		break;
	case FLITE_NOTIFY_FEND:
		ret = fimc_is_sensor_notify_by_fend(device, arg);
		if (ret)
			merr("fimc_is_sensor_notify_by_fend is fail(%d)", device, ret);
		break;
	}
}

static void fimc_is_sensor_instanton(struct work_struct *data)
{
	int ret = 0;
	u32 instant_cnt;
	struct fimc_is_device_sensor *device;

	BUG_ON(!data);

	device = container_of(data, struct fimc_is_device_sensor, instant_work);
	instant_cnt = device->instant_cnt;

	clear_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

	ret = fimc_is_sensor_start(device);
	if (ret) {
		merr("fimc_is_sensor_start is fail(%d)\n", device, ret);
		goto p_err;
	}
	set_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);

#ifdef ENABLE_DTP
	if (device->dtp_check) {
		setup_timer(&device->dtp_timer, fimc_is_sensor_dtp, (unsigned long)device);
		mod_timer(&device->dtp_timer, jiffies +  msecs_to_jiffies(300));
		info("DTP checking...\n");
	}
#endif

	if (instant_cnt) {
		u32 timetowait, timetoelapse, timeout;

		timeout = FIMC_IS_FLITE_STOP_TIMEOUT + msecs_to_jiffies(instant_cnt*60);
		timetowait = wait_event_timeout(device->instant_wait,
			(device->instant_cnt == 0),
			timeout);
		if (!timetowait) {
			merr("wait_event_timeout is invalid", device);
			ret = -ETIME;
		}

		fimc_is_sensor_front_stop(device);

		timetoelapse = (jiffies_to_msecs(timeout) - jiffies_to_msecs(timetowait));
		info("[FRT:D:%d] instant off(fcount : %d, time : %dms)", device->instance,
			device->instant_cnt,
			timetoelapse);
	}

p_err:
	device->instant_ret = ret;
}

static int fimc_is_sensor_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 instance = -1;
	atomic_t device_id;
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *device;
	struct device *dev;
	void *pdata;

	BUG_ON(!pdev);

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed");
		pdev->dev.init_name = FIMC_IS_SENSOR_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

#ifdef CONFIG_OF
#ifdef CONFIG_COMPANION_USE
	ret = fimc_is_sensor_parse_dt_with_companion(pdev);
	if (ret) {
		err("parsing device tree is fail(%d)", ret);
		goto p_err;
	}
#else
	ret = fimc_is_sensor_parse_dt(pdev);
	if (ret) {
		err("parsing device tree is fail(%d)", ret);
		goto p_err;
	}
#endif /* CONFIG_COMPANION_USE */
#endif

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		err("pdata is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* 1. get device */
	atomic_set(&device_id, pdev->id);
	device = &core->sensor[pdev->id];

	/* v4l2 device and device init */
	memset(&device->v4l2_dev, 0, sizeof(struct v4l2_device));
	instance = v4l2_device_set_name(&device->v4l2_dev, FIMC_IS_SENSOR_DEV_NAME, &device_id);
	device->v4l2_dev.notify = fimc_is_sensor_notify;
	device->instance = instance;
	device->resourcemgr = &core->resourcemgr;
	device->pdev = pdev;
	device->private_data = core;
	device->pdata = pdata;
	platform_set_drvdata(pdev, device);
	init_waitqueue_head(&device->instant_wait);
	INIT_WORK(&device->instant_work, fimc_is_sensor_instanton);
	INIT_WORK(&device->control_work, fimc_is_sensor_control);
	spin_lock_init(&device->slock_state);
	device_init_wakeup(&pdev->dev, true);

	/* 3. state init*/
	clear_bit(FIMC_IS_SENSOR_OPEN, &device->state);
	clear_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

#ifdef ENABLE_DTP
	device->dtp_check = false;
#endif

	ret = fimc_is_mem_probe(&device->mem, device->pdev);
	if (ret) {
		merr("fimc_is_mem_probe is fail(%d)", device, ret);
		goto p_err;
	}

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#if defined(CONFIG_VIDEOBUF2_ION)
	if (device->mem.alloc_ctx)
		vb2_ion_attach_iommu(device->mem.alloc_ctx);
#endif
#endif

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

	ret = v4l2_device_register(&pdev->dev, &device->v4l2_dev);
	if (ret) {
		merr("v4l2_device_register is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_csi_probe(device, device->pdata->csi_ch);
	if (ret) {
		merr("fimc_is_csi_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_flite_probe(device, device->pdata->flite_ch);
	if (ret) {
		merr("fimc_is_flite_probe is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_sen_video_probe(device);
	if (ret) {
		merr("fimc_is_sensor_video_probe is fail(%d)", device, ret);
		goto p_err;
	}

	dev = pdev->id ? camera_front_dev : camera_rear_dev;
	if (dev)
		dev_set_drvdata(dev, device->pdata);

p_err:
	info("[%d][SEN:D] %s(%d)\n", instance, __func__, ret);
	return ret;
}

static int fimc_is_sensor_remove(struct platform_device *pdev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

int fimc_is_sensor_open(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (test_bit(FIMC_IS_SENSOR_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);
	clear_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);
	clear_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state);
	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);
	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

	device->vctx = vctx;
	device->fcount = 0;
	device->instant_cnt = 0;
	device->instant_ret = 0;
	device->ischain = NULL;
	device->subdev_module = NULL;
	device->exposure_time = 0;
	device->frame_duration = 0;
	device->force_stop = 0;
	device->request_cam_qos = 0;
	device->request_int_qos = 0;
	device->request_mif_qos = 0;
	memset(&device->sensor_ctl, 0, sizeof(struct camera2_sensor_ctl));
	memset(&device->lens_ctl, 0, sizeof(struct camera2_lens_ctl));
	memset(&device->flash_ctl, 0, sizeof(struct camera2_flash_ctl));

	if (pdata->id == 0) {
		core->running_rear_camera = true;
	} else {
		core->running_front_camera = true;
	}

	/* for mediaserver force close */
	ret = fimc_is_resource_get(device->resourcemgr, device->instance);
	if (ret) {
		merr("fimc_is_resource_get is fail", device);
		goto p_err;
	}

	ret = fimc_is_csi_open(device->subdev_csi);
	if (ret) {
		merr("fimc_is_csi_open is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_flite_open(device->subdev_flite, GET_DST_FRAMEMGR(vctx));
	if (ret) {
		merr("fimc_is_flite_open is fail(%d)", device, ret);
		goto p_err;
	}

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_get_sync(&device->pdev->dev);
#else
	fimc_is_sensor_runtime_resume(&device->pdev->dev);
#endif

#ifdef ENABLE_DTP
	device->dtp_check = true;
#endif

	set_bit(FIMC_IS_SENSOR_OPEN, &device->state);

p_err:
	info("[SEN:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

int fimc_is_sensor_close(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_group *group_3aa;
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_SENSOR_OPEN, &device->state)) {
		merr("already close", device);
		ret = -EMFILE;
		goto p_err;
	}

	/* for mediaserver force close */
	ischain = device->ischain;
	if (ischain) {
		group_3aa = &ischain->group_3aa;
		if (test_bit(FIMC_IS_GROUP_READY, &group_3aa->state)) {
			info("media server is dead, 3ax forcely done\n");
			set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group_3aa->state);
		}
	}

	ret = fimc_is_sensor_back_stop(device);
	if (ret)
		merr("fimc_is_sensor_back_stop is fail(%d)", device, ret);

	ret = fimc_is_sensor_front_stop(device);
	if (ret)
		merr("fimc_is_sensor_front_stop is fail(%d)", device, ret);

	ret = fimc_is_csi_close(device->subdev_csi);
	if (ret)
		merr("fimc_is_flite_close is fail(%d)", device, ret);

	ret = fimc_is_flite_close(device->subdev_flite);
	if (ret)
		merr("fimc_is_flite_close is fail(%d)", device, ret);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_put_sync(&device->pdev->dev);
#else
	fimc_is_sensor_runtime_suspend(&device->pdev->dev);
#endif

	/* cancel a work and wait for it to finish */
	cancel_work_sync(&device->control_work);
	cancel_work_sync(&device->instant_work);

	if (device->subdev_module) {
		v4l2_device_unregister_subdev(device->subdev_module);
		device->subdev_module = NULL;
	}

	/* for mediaserver force close */
	ret = fimc_is_resource_put(device->resourcemgr, device->instance);
	if (ret)
		merr("fimc_is_resource_put is fail", device);

	clear_bit(FIMC_IS_SENSOR_OPEN, &device->state);
p_err:
	if (pdata->id == 0) {
		core->running_rear_camera = false;
	} else {
		core->running_front_camera = false;
	}
	info("[SEN:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

int fimc_is_sensor_s_input(struct fimc_is_device_sensor *device,
	u32 input,
	u32 scenario)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct v4l2_subdev *subdev_flite;
	struct fimc_is_module_enum *module = NULL;
	u32 sensor_ch, actuator_ch;
#if defined(CONFIG_OIS_USE)
	u32 ois_ch, ois_addr;
#endif
	u32 sensor_addr, actuator_addr;
	u32 sensor_index = 0;

	BUG_ON(!device);
	BUG_ON(!device->pdata);
	BUG_ON(!device->subdev_csi);
	BUG_ON(input >= SENSOR_NAME_END);

	for (sensor_index = 0; sensor_index < SENSOR_MAX_ENUM; sensor_index++) {
		if (&device->module_enum[sensor_index] &&
			device->module_enum[sensor_index].id == input) {
			module = &device->module_enum[sensor_index];

			/*
			 * If it is not normal scenario,
			 * try to find proper sensor module which has a i2c client
			 */
			if (scenario != SENSOR_SCENARIO_NORMAL &&
				module->client == NULL)
				continue;
			else
				break;
		}
	}

	if (sensor_index >= SENSOR_MAX_ENUM) {
		merr("module is not probed", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		merr("module is not probed", device);
		ret = -EINVAL;
		goto p_err;
	}

	/* change i2c channel info */
	if (module->ext.sensor_con.peri_type == SE_I2C) {
		sensor_ch = device->pdata->i2c_ch & SENSOR_I2C_CH_MASK;
		sensor_ch >>= SENSOR_I2C_CH_SHIFT;
		sensor_addr = device->pdata->i2c_addr & SENSOR_I2C_ADDR_MASK;
		sensor_addr >>= SENSOR_I2C_ADDR_SHIFT;
		module->ext.sensor_con.peri_setting.i2c.channel = sensor_ch;
		module->ext.sensor_con.peri_setting.i2c.slave_address = sensor_addr;
	}

	if (module->ext.actuator_con.peri_type == SE_I2C) {
		actuator_ch = device->pdata->i2c_ch & ACTUATOR_I2C_CH_MASK;
		actuator_ch >>= ACTUATOR_I2C_CH_SHIFT;
		actuator_addr = device->pdata->i2c_addr & ACTUATOR_I2C_ADDR_MASK;
		actuator_addr >>= ACTUATOR_I2C_ADDR_SHIFT;
		module->ext.actuator_con.peri_setting.i2c.channel = actuator_ch;
		module->ext.actuator_con.peri_setting.i2c.slave_address = actuator_addr;
	}

#if defined(CONFIG_OIS_USE)
	if (module->ext.ois_con.peri_type == SE_I2C) {
		ois_ch = device->pdata->i2c_ch & OIS_I2C_CH_MASK;
		ois_ch >>= OIS_I2C_CH_SHIFT;
		ois_addr = device->pdata->i2c_addr & OIS_I2C_ADDR_MASK;
		ois_addr >>= OIS_I2C_ADDR_SHIFT;
		module->ext.ois_con.peri_setting.i2c.channel = ois_ch;
		module->ext.ois_con.peri_setting.i2c.slave_address = ois_addr;
	}
#endif

	/* send csi chennel to FW */
	module->ext.sensor_con.reserved[0] = device->pdata->csi_ch;
	module->ext.sensor_con.reserved[0] |= 0x0100;

	module->ext.flash_con.peri_setting.gpio.first_gpio_port_no = device->pdata->flash_first_gpio;
	module->ext.flash_con.peri_setting.gpio.second_gpio_port_no = device->pdata->flash_second_gpio;

#ifdef CONFIG_COMPANION_USE
	/* Data Type For Comapnion:
	 * Companion use user defined data type.
	 */
	if (module->ext.companion_con.product_name &&
	module->ext.companion_con.product_name != COMPANION_NAME_NOTHING)
		device->image.format.field = V4L2_FIELD_INTERLACED;
#endif

	subdev_csi = device->subdev_csi;
	subdev_flite = device->subdev_flite;
	device->image.framerate = min_t(u32, SENSOR_DEFAULT_FRAMERATE, module->max_framerate);
	device->image.window.width = module->pixel_width;
	device->image.window.height = module->pixel_height;
	device->image.window.o_width = device->image.window.width;
	device->image.window.o_height = device->image.window.height;

	if (scenario) {
		device->pdata->scenario = scenario;
		set_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	} else {
		device->pdata->scenario = SENSOR_SCENARIO_NORMAL;
		clear_bit(FIMC_IS_SENSOR_DRIVING, &device->state);
	}

	if (device->subdev_module) {
		mwarn("subdev_module is already registered", device);
		v4l2_device_unregister_subdev(device->subdev_module);
	}

	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_module);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto p_err;
	} else {
		device->subdev_module = subdev_module;
	}

#if defined(CONFIG_PM_DEVFREQ)
	/* DEVFREQ set */
	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state))
		fimc_is_sensor_set_qos_init(device, true);
#endif

	/* configuration clock control */
	ret = fimc_is_sensor_iclk_on(device);
	if (ret) {
		merr("fimc_is_sensor_iclk_on is fail(%d)", device, ret);
		goto p_err;
	}

#if defined(CONFIG_PM_DEVFREQ)
	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state))
		fimc_is_sensor_set_qos_update(device, device->pdata->scenario);
#endif

	/* Sensor power on */
	ret = fimc_is_sensor_gpio_on(device);
	if (ret) {
		merr("fimc_is_sensor_gpio_on is fail(%d)", device, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_flite, core, init, device->pdata->csi_ch);
	if (ret) {
		merr("v4l2_flite_call(init) is fail(%d)", device, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, core, init, sensor_index);
	if (ret) {
		merr("v4l2_csi_call(init) is fail(%d)", device, ret);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		ret = v4l2_subdev_call(subdev_module, core, init, 0);
		if (ret) {
			merr("v4l2_module_call(init) is fail(%d)", device, ret);
			goto p_err;
		}
	}

p_err:

	minfo("[SEN:D] %s(%d, %d, %d)\n", device, __func__, input, scenario, ret);
	return ret;
}

int fimc_is_sensor_s_format(struct fimc_is_device_sensor *device,
	struct fimc_is_fmt *format,
	u32 width,
	u32 height)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct v4l2_subdev *subdev_flite;
	struct fimc_is_module_enum *module;
	struct v4l2_mbus_framefmt subdev_format;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!device->subdev_flite);
	BUG_ON(!device->subdev_module);
	BUG_ON(!format);

	subdev_module = device->subdev_module;
	subdev_csi = device->subdev_csi;
	subdev_flite = device->subdev_flite;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		goto p_err;
	}

	/* Data Type For Comapnion:
	 * Companion use user defined data type.
	 */
	if (device->image.format.field == V4L2_FIELD_INTERLACED)
		format->field = V4L2_FIELD_INTERLACED;

	device->image.format = *format;
	device->image.window.offs_h = 0;
	device->image.window.offs_v = 0;
	device->image.window.width = width;
	device->image.window.o_width = width;
	device->image.window.height = height;
	device->image.window.o_height = height;

	subdev_format.code = format->pixelformat;
	subdev_format.field = format->field;
	subdev_format.width = width;
	subdev_format.height = height;

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		ret = v4l2_subdev_call(subdev_module, video, s_mbus_fmt, &subdev_format);
		if (ret) {
			merr("v4l2_module_call(s_format) is fail(%d)", device, ret);
			goto p_err;
		}
	}

	ret = v4l2_subdev_call(subdev_csi, video, s_mbus_fmt, &subdev_format);
	if (ret) {
		merr("v4l2_csi_call(s_format) is fail(%d)", device, ret);
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_flite, video, s_mbus_fmt, &subdev_format);
	if (ret) {
		merr("v4l2_flite_call(s_format) is fail(%d)", device, ret);
		goto p_err;
	}

	/* if sensor is driving mode, skip finding sensor mode */
	if (!test_bit(FIMC_IS_SENSOR_DRIVING, &device->state))
		device->mode = get_sensor_mode(module->cfg, module->cfgs,
				device->image.window.width, device->image.window.height,
				device->image.framerate);
	else
		device->mode = 0;

	/* can't find proper sensor mode */
	if (device->mode < 0) {
		ret = -EINVAL;
		goto p_err;
	}
p_err:
	return ret;
}

int fimc_is_sensor_s_framerate(struct fimc_is_device_sensor *device,
	struct v4l2_streamparm *param)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct fimc_is_module_enum *module;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u32 framerate = 0;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!param);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->numerator) {
		merr("numerator is 0", device);
		ret = -EINVAL;
		goto p_err;
	}

	framerate = tpf->denominator / tpf->numerator;
	subdev_module = device->subdev_module;
	subdev_csi = device->subdev_csi;

	if (framerate == 0) {
		mwarn("frame rate 0 request is ignored", device);
		goto p_err;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		merr("front is already stream on", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		merr("type is invalid(%d)", device, param->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (framerate > module->max_framerate) {
		merr("framerate is invalid(%d > %d)", device, framerate, module->max_framerate);
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, video, s_parm, param);
	if (ret) {
		merr("v4l2_csi_call(s_param) is fail(%d)", device, ret);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state)) {
		ret = v4l2_subdev_call(subdev_module, video, s_parm, param);
		if (ret) {
			merr("v4l2_module_call(s_param) is fail(%d)", device, ret);
			goto p_err;
		}
	}

	device->image.framerate = framerate;

	device->mode = get_sensor_mode(module->cfg, module->cfgs,
			device->image.window.width, device->image.window.height,
			framerate);

	info("[SEN:D:%d] framerate: req@%dfps, cur@%dfps\n", device->instance,
		framerate, device->image.framerate);

p_err:
	return ret;
}

int fimc_is_sensor_s_ctrl(struct fimc_is_device_sensor *device,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!ctrl);

	subdev_module = device->subdev_module;

	ret = v4l2_subdev_call(subdev_module, core, s_ctrl, ctrl);
	if (ret) {
		err("s_ctrl is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_s_bns(struct fimc_is_device_sensor *device,
	u32 ratio)
{
	int ret = 0;
	struct v4l2_subdev *subdev_flite;
	u32 sensor_width, sensor_height;

	BUG_ON(!device);
	BUG_ON(!device->subdev_flite);

	subdev_flite = device->subdev_flite;

	sensor_width = fimc_is_sensor_g_width(device);
	sensor_height = fimc_is_sensor_g_height(device);
	if (!sensor_width || !sensor_height) {
		merr("Sensor size is zero. Sensor set_format first.\n", device);
		ret = -EINVAL;
		goto p_err;
	}

	device->image.window.otf_width
		= rounddown((sensor_width * 1000 / ratio), 4);
	device->image.window.otf_height
		= rounddown((sensor_height * 1000 / ratio), 2);

p_err:
	return ret;
}

int fimc_is_sensor_s_frame_duration(struct fimc_is_device_sensor *device,
	u32 framerate)
{
	int ret = 0;
	u64 frame_duration;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* unit : nano */
	frame_duration = (1000 * 1000 * 1000) / framerate;
	if (frame_duration <= 0) {
		err("it is wrong frame duration(%lld)", frame_duration);
		ret = -EINVAL;
		goto p_err;
	}

	if (device->frame_duration != frame_duration) {
		CALL_MOPS(module, s_duration, subdev_module, frame_duration);
		device->frame_duration = frame_duration;
	}

p_err:
	return ret;
}

int fimc_is_sensor_s_exposure_time(struct fimc_is_device_sensor *device,
	u32 exposure_time)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);

	subdev_module = device->subdev_module;
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module = v4l2_get_subdevdata(subdev_module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (exposure_time <= 0) {
		err("it is wrong exposure time (%d)", exposure_time);
		ret = -EINVAL;
		goto p_err;
	}

	if (device->exposure_time != exposure_time) {
		CALL_MOPS(module, s_exposure, subdev_module, exposure_time);
		device->exposure_time = exposure_time;
	}
p_err:
	return ret;
}

int fimc_is_sensor_g_ctrl(struct fimc_is_device_sensor *device,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);
	BUG_ON(!device->subdev_csi);
	BUG_ON(!ctrl);

	subdev_module = device->subdev_module;

	ret = v4l2_subdev_call(subdev_module, core, g_ctrl, ctrl);
	if (ret) {
		err("g_ctrl is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}


int fimc_is_sensor_g_instance(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->instance;
}

int fimc_is_sensor_g_fcount(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->fcount;
}

int fimc_is_sensor_g_framerate(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->image.framerate;
}

int fimc_is_sensor_g_width(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->image.window.width;
}

int fimc_is_sensor_g_height(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	return device->image.window.height;
}

int fimc_is_sensor_g_bns_width(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);

	if (device->image.window.otf_width)
		return device->image.window.otf_width;

	return device->image.window.width;
}

int fimc_is_sensor_g_bns_height(struct fimc_is_device_sensor *device)
{
	BUG_ON(!device);
	if (device->image.window.otf_height)
		return device->image.window.otf_height;

	return device->image.window.height;
}

int fimc_is_sensor_g_bns_ratio(struct fimc_is_device_sensor *device)
{
	int binning = 0;
	u32 sensor_width, sensor_height;
	u32 bns_width, bns_height;

	BUG_ON(!device);

	sensor_width = fimc_is_sensor_g_width(device);
	sensor_height = fimc_is_sensor_g_height(device);
	bns_width = fimc_is_sensor_g_bns_width(device);
	bns_height = fimc_is_sensor_g_bns_height(device);

	binning = min(BINNING(sensor_width, bns_width),
		BINNING(sensor_height, bns_height));

	return binning;
}

int fimc_is_sensor_g_bratio(struct fimc_is_device_sensor *device)
{
	int binning = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (!module) {
		merr("module is NULL", device);
		goto p_err;
	}

	binning = min(BINNING(module->active_width, device->image.window.width),
		BINNING(module->active_height, device->image.window.height));

p_err:
	return binning;
}

int fimc_is_sensor_g_module(struct fimc_is_device_sensor *device,
	struct fimc_is_module_enum **module)
{
	int ret = 0;

	BUG_ON(!device);
	BUG_ON(!device->subdev_module);

	*module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (!*module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_buffer_queue(struct fimc_is_device_sensor *device,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = &device->vctx->q_dst.framemgr;
	if (framemgr == NULL) {
		err("framemgr is null\n");
		ret = EINVAL;
		goto p_err;
	}

	frame = &framemgr->frame[index];
	if (frame == NULL) {
		err("frame is null\n");
		ret = EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_INI_MEM, &frame->memory))) {
		err("frame %d is NOT init", index);
		ret = EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_6, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_FREE) {
		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	} else {
		err("frame(%d) is not free state(%d)", index, frame->state);
		fimc_is_frame_print_all(framemgr);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_6, flags);

p_err:
	return ret;
}

int fimc_is_sensor_buffer_finish(struct fimc_is_device_sensor *device,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_frame *frame;
	struct fimc_is_framemgr *framemgr;

	if (index >= FRAMEMGR_MAX_REQUEST) {
		err("index(%d) is invalid", index);
		ret = -EINVAL;
		goto exit;
	}

	framemgr = &device->vctx->q_dst.framemgr;
	frame = &framemgr->frame[index];

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_7, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_COMPLETE) {
		if (!frame->shot->dm.request.frameCount)
			err("request.frameCount is 0\n");
		fimc_is_frame_trans_com_to_fre(framemgr, frame);

		frame->shot_ext->free_cnt = framemgr->frame_fre_cnt;
		frame->shot_ext->request_cnt = framemgr->frame_req_cnt;
		frame->shot_ext->process_cnt = framemgr->frame_pro_cnt;
		frame->shot_ext->complete_cnt = framemgr->frame_com_cnt;
	} else {
		err("frame(%d) is not com state(%d)", index, frame->state);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_7, flags);

exit:
	return ret;
}

int fimc_is_sensor_back_start(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	int enable;
	struct v4l2_subdev *subdev_flite;
	struct fimc_is_device_flite *flite;

	BUG_ON(!device);
	BUG_ON(!device->subdev_flite);

	subdev_flite = device->subdev_flite;
	enable = FLITE_ENABLE_FLAG;

	if (test_bit(FIMC_IS_SENSOR_BACK_START, &device->state)) {
		err("already back start");
		ret = -EINVAL;
		goto p_err;
	}

	flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(subdev_flite);
	if (!flite) {
		merr("flite is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	/* to determine flite buffer done mode (early/normal) when not vision mode */
	if (!test_bit(FIMC_IS_SENSOR_DRIVING, &device->state) && flite->chk_early_buf_done) {
		flite->chk_early_buf_done(flite, device->image.framerate,
			device->pdev->id);
	}

	ret = v4l2_subdev_call(subdev_flite, video, s_stream, enable);
	if (ret) {
		merr("v4l2_flite_call(s_stream) is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_SENSOR_BACK_START, &device->state);

p_err:
	minfo("[SEN:D] %s(%dx%d, %d)\n", device, __func__,
		device->image.window.width, device->image.window.height, ret);
	return ret;
}

int fimc_is_sensor_back_stop(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	int enable;
	struct v4l2_subdev *subdev_flite;

	BUG_ON(!device);
	BUG_ON(!device->subdev_flite);

	enable = 0;
	subdev_flite = device->subdev_flite;

	if (!test_bit(FIMC_IS_SENSOR_BACK_START, &device->state)) {
		warn("already back stop");
		goto p_err;
	}

	if (test_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state)) {
		warn("fimc_is_flite_stop, no waiting...");
		enable = FLITE_NOWAIT_FLAG << FLITE_NOWAIT_SHIFT;
	}

	ret = v4l2_subdev_call(subdev_flite, video, s_stream, enable);
	if (ret) {
		merr("v4l2_flite_call(s_stream) is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_BACK_START, &device->state);

p_err:
	minfo("[BAK:D] %s(%d)\n", device, __func__, ret);
	return ret;
}

int fimc_is_sensor_front_start(struct fimc_is_device_sensor *device,
	u32 instant_cnt,
	u32 nonblock)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct v4l2_subdev *subdev_csi;
	struct fimc_is_module_enum *module;

	BUG_ON(!device);
	BUG_ON(!device->pdata);
	BUG_ON(!device->subdev_csi);

	if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		merr("already front start", device);
		ret = -EINVAL;
		goto p_err;
	}

	device->instant_cnt = instant_cnt;
	subdev_csi = device->subdev_csi;
	subdev_module = device->subdev_module;
	if (!subdev_module) {
		merr("subdev_module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	if (!module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = v4l2_subdev_call(subdev_csi, video, s_stream, IS_ENABLE_STREAM);
	if (ret) {
		merr("v4l2_csi_call(s_stream) is fail(%d)", device, ret);
		goto p_err;
	}

	mdbgd_sensor("%s(snesor id : %d, csi ch : %d, size : %d x %d)\n", device,
		__func__,
		module->id,
		device->pdata->csi_ch,
		device->image.window.width,
		device->image.window.height);

	if (nonblock) {
		schedule_work(&device->instant_work);
	} else {
		fimc_is_sensor_instanton(&device->instant_work);
		if (device->instant_ret) {
			merr("fimc_is_sensor_instanton is fail(%d)", device, device->instant_ret);
			ret = device->instant_ret;
			goto p_err;
		}
	}

p_err:
	return ret;
}

int fimc_is_sensor_front_stop(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct v4l2_subdev *subdev_csi;

	BUG_ON(!device);

	if (!test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
		warn("already front stop");
		goto p_err;
	}

	subdev_csi = device->subdev_csi;

	ret = fimc_is_sensor_stop(device);
	if (ret)
		merr("sensor stream off is failed(%d)\n", device, ret);

	ret = v4l2_subdev_call(subdev_csi, video, s_stream, IS_DISABLE_STREAM);
	if (ret)
		merr("v4l2_csi_call(s_stream) is fail(%d)", device, ret);

	clear_bit(FIMC_IS_SENSOR_FRONT_START, &device->state);
	set_bit(FIMC_IS_SENSOR_BACK_NOWAIT_STOP, &device->state);

p_err:
	minfo("[FRT:D] %s(%d)\n", device, __func__, ret);
	return ret;
}

int fimc_is_sensor_gpio_off_softlanding(struct fimc_is_device_sensor *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state)) {
		merr("%s : already gpio off", device, __func__);
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		merr("gpio_cfg is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_cfg(device->pdev, pdata->scenario, GPIO_SCENARIO_OFF);
	if (ret) {
		merr("gpio_cfg is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_SENSOR_GPIO_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_sensor_suspend(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static int fimc_is_sensor_resume(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

int fimc_is_sensor_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_sensor *device;
	struct v4l2_subdev *subdev_csi;

	info("%s\n", __func__);

	BUG_ON(!pdev);

	device = (struct fimc_is_device_sensor *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		return -EINVAL;
	}

#if !(defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433))
#if defined(CONFIG_VIDEOBUF2_ION)
	if (device->mem.alloc_ctx)
		vb2_ion_detach_iommu(device->mem.alloc_ctx);
#endif
#endif

	subdev_csi = device->subdev_csi;
	if (!subdev_csi)
		mwarn("subdev_csi is NULL", device);

	/* gpio uninit */
	if(device->pdata->is_softlanding == false) {
		ret = fimc_is_sensor_gpio_off(device);
		if (ret)
			mwarn("fimc_is_sensor_gpio_off is fail(%d)", device, ret);
	}

	/* GSCL internal clock off */
	ret = fimc_is_sensor_iclk_off(device);
	if (ret)
		mwarn("fimc_is_sensor_iclk_off is fail(%d)", device, ret);

	/* Sensor clock on */
	ret = fimc_is_sensor_mclk_off(device);
	if (ret)
		mwarn("fimc_is_sensor_mclk_off is fail(%d)", device, ret);

	ret = v4l2_subdev_call(subdev_csi, core, s_power, 0);
	if (ret)
		mwarn("v4l2_csi_call(s_power) is fail(%d)", device, ret);

#if defined(CONFIG_PM_DEVFREQ)
	/* DEVFREQ set */
	if (test_bit(FIMC_IS_SENSOR_DRIVING, &device->state))
		fimc_is_sensor_set_qos_init(device, false);
#endif

	info("[SEN:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return 0;
}

int fimc_is_sensor_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_sensor *device;
	struct v4l2_subdev *subdev_csi;

	device = (struct fimc_is_device_sensor *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		return -EINVAL;
	}

	subdev_csi = device->subdev_csi;
	if (!subdev_csi) {
		merr("subdev_csi is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

/* HACK */
/* at xyref 4415, when runtime_suspend operating, isp0 power is off thoroughly
   so it needs to power on operation at sensor_runtime_resume operation */
#if defined(CONFOG_SOC_EXYNOS4415) && !defined(CONFIG_PM_RUNTIME)
	{
		u32 val;
		/* ISP0 */
		/* 1. set feedback mode */
		val = __raw_readl(PMUREG_ISP0_OPTION);
		val = (val & ~(0x3<< 0)) | (0x2 << 0);
		__raw_writel(val, PMUREG_ISP0_OPTION);

		/* 2. power on isp0 */
		val = __raw_readl(PMUREG_ISP0_CONFIGURATION);
		val = (val & ~(0x7 << 0)) | (0x7 << 0);
		__raw_writel(val, PMUREG_ISP0_CONFIGURATION);
	}
#endif

	/* 1. Enable MIPI */
	ret = v4l2_subdev_call(subdev_csi, core, s_power, 1);
	if (ret) {
		merr("v4l2_csi_call(s_power) is fail(%d)", device, ret);
		goto p_err;
	}

	/* 2. Sensor clock on */
	ret = fimc_is_sensor_mclk_on(device);
	if (ret) {
		merr("fimc_is_sensor_mclk_on is fail(%d)", device, ret);
		goto p_err;
	}

#if !(defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433))
#if defined(CONFIG_VIDEOBUF2_ION)
	if (device->mem.alloc_ctx)
		vb2_ion_attach_iommu(device->mem.alloc_ctx);
	pr_debug("FIMC_IS runtime resume - ion attach complete\n");
#endif
#endif

p_err:
	info("[SEN:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

static const struct dev_pm_ops fimc_is_sensor_pm_ops = {
	.suspend		= fimc_is_sensor_suspend,
	.resume			= fimc_is_sensor_resume,
	.runtime_suspend	= fimc_is_sensor_runtime_suspend,
	.runtime_resume		= fimc_is_sensor_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_match);

static struct platform_driver fimc_is_sensor_driver = {
	.probe	= fimc_is_sensor_probe,
	.remove	= fimc_is_sensor_remove,
	.driver = {
		.name	= FIMC_IS_SENSOR_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_sensor_pm_ops,
		.of_match_table = exynos_fimc_is_sensor_match,
	}
};

module_platform_driver(fimc_is_sensor_driver);
#else
static struct platform_device_id fimc_is_sensor_driver_ids[] = {
	{
		.name		= FIMC_IS_SENSOR_DEV_NAME,
		.driver_data	= 0,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, fimc_is_sensor_driver_ids);

static struct platform_driver fimc_is_sensor_driver = {
	.probe	  = fimc_is_sensor_probe,
	.remove	  = __devexit_p(fimc_is_sensor_remove),
	.id_table = fimc_is_sensor_driver_ids,
	.driver	  = {
		.name	= FIMC_IS_SENSOR_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_sensor_pm_ops,
	}
};

static int __init fimc_is_sensor_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&fimc_is_sensor_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}

static void __exit fimc_is_sensor_exit(void)
{
	platform_driver_unregister(&fimc_is_sensor_driver);
}
module_init(fimc_is_sensor_init);
module_exit(fimc_is_sensor_exit);
#endif

MODULE_AUTHOR("Gilyeon lim<kilyeon.im@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS_SENSOR driver");
MODULE_LICENSE("GPL");
