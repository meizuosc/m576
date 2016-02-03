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
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/syscalls.h>
#include <linux/bug.h>
#include <linux/smc.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/devfreq.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl-samsung.h>
#include <linux/gpio.h>
#if defined(CONFIG_SOC_EXYNOS3470)
#include <mach/bts.h>
#endif

#include "fimc-is-binary.h"
#include "fimc-is-time.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-hw.h"
#include "fimc-is-spi.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-clk-gate.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-device-companion.h"
#include "fimc-is-device-eeprom.h"

/* Default setting values */
#define DEFAULT_PREVIEW_STILL_WIDTH		(1280) /* sensor margin : 16 */
#define DEFAULT_PREVIEW_STILL_HEIGHT		(720) /* sensor margin : 12 */
#define DEFAULT_CAPTURE_VIDEO_WIDTH		(1920)
#define DEFAULT_CAPTURE_VIDEO_HEIGHT		(1080)
#define DEFAULT_CAPTURE_STILL_WIDTH		(2560)
#define DEFAULT_CAPTURE_STILL_HEIGHT		(1920)
#define DEFAULT_CAPTURE_STILL_CROP_WIDTH	(2560)
#define DEFAULT_CAPTURE_STILL_CROP_HEIGHT	(1440)
#define DEFAULT_PREVIEW_VIDEO_WIDTH		(640)
#define DEFAULT_PREVIEW_VIDEO_HEIGHT		(480)

/* sysfs variable for debug */
extern struct fimc_is_sysfs_debug sysfs_debug;

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;


extern bool crc32_check;
extern bool crc32_header_check;
extern bool crc32_check_front;
extern bool crc32_header_check_front;

static int cam_id;
bool is_dumped_fw_loading_needed = false;

static int fimc_is_ischain_3aa_stop(void *qdevice,
	struct fimc_is_queue *queue);
static int fimc_is_ischain_isp_stop(void *qdevice,
	struct fimc_is_queue *queue);
static int fimc_is_ischain_dis_stop(void *qdevice,
	struct fimc_is_queue *queue);

static int fimc_is_ischain_3aa_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame);
static int fimc_is_ischain_isp_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame);
static int fimc_is_ischain_dis_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame);

static int fimc_is_ischain_3aa_cfg(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes);
static int fimc_is_ischain_isp_cfg(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes);
static int fimc_is_ischain_dis_cfg(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes);

static const struct sensor_param init_sensor_param = {
	.config = {
#ifdef FIXED_FPS_DEBUG
		.frametime = (1000 * 1000) / FIXED_FPS_VALUE,
		.min_target_fps = FIXED_FPS_VALUE,
		.max_target_fps = FIXED_FPS_VALUE,
#else
		.frametime = (1000 * 1000) / 30,
		.min_target_fps = 15,
		.max_target_fps = 30,
#endif
	},
};

static const struct taa_param init_taa_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.format = OTF_INPUT_FORMAT_BAYER,
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.vdma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.format = 0,
		.bitwidth = 0,
		.order = 0,
		.plane = 0,
		.width = 0,
		.height = 0,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = 0,
	},
	.ddma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	},
	.vdma4_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = DMA_INPUT_PLANE_1,
		.order = DMA_INPUT_ORDER_YCbCr,
		.selection = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.vdma2_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_BAYER,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_GB_BG,
		.selection = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.ddma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
};

static const struct isp_param init_isp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
	},
	.vdma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0,
		.height = 0,
		.format = 0,
		.bitwidth = 0,
		.plane = 0,
		.order = 0,
		.err = 0,
	},
	.vdma3_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.vdma4_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
	.vdma5_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
};

static const struct drc_param init_drc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = DMA_INPUT_PLANE_1,
		.order = DMA_INPUT_ORDER_YCbCr,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct scc_param init_scc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.effect = {
		.cmd = 0,
		.arbitrary_cb = 128, /* default value : 128 */
		.arbitrary_cr = 128, /* default value : 128 */
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.err = 0,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_CROP_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_CROP_HEIGHT,
		.in_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.in_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.out_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.out_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.err = 0,
	},
	.output_crop = {
		.cmd = SCALER_CROP_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_CrYCbY,
		.selection = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct odc_param init_odc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct tpu_param init_tpu_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.config = {
		.odc_bypass = true,
		.dis_bypass = true,
		.tdnr_bypass = true,
		.err = 0
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV420,
		.order = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct tdnr_param init_tdnr_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.frame = {
		.cmd = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_2,
		.order = DMA_OUTPUT_ORDER_CbCr,
		.selection = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct scp_param init_scp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.effect = {
		.cmd = 0,
		.arbitrary_cb = 128, /* default value : 128 */
		.arbitrary_cr = 128, /* default value : 128 */
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.err = 0,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.crop_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.in_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.in_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.out_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.out_height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.err = 0,
	},
	.output_crop = {
		.cmd = SCALER_CROP_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.crop_height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
		.err = 0,
	},
	.rotation = {
		.cmd = 0,
		.err = 0,
	},
	.flip = {
		.cmd = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_3,
		.order = DMA_OUTPUT_ORDER_NO,
		.selection = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct vra_param init_vra_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.format = 0,
		.bitwidth = 0,
		.order = 0,
		.plane = 0,
		.width = 0,
		.height = 0,
		.err = 0,
	},
	.config = {
		.cmd = FD_CONFIG_COMMAND_MAXIMUM_NUMBER |
			FD_CONFIG_COMMAND_ROLL_ANGLE |
			FD_CONFIG_COMMAND_YAW_ANGLE |
			FD_CONFIG_COMMAND_SMILE_MODE |
			FD_CONFIG_COMMAND_BLINK_MODE |
			FD_CONFIG_COMMAND_EYES_DETECT |
			FD_CONFIG_COMMAND_MOUTH_DETECT |
			FD_CONFIG_COMMAND_ORIENTATION |
			FD_CONFIG_COMMAND_ORIENTATION_VALUE,
		.max_number = CAMERA2_MAX_FACES,
		.roll_angle = FD_CONFIG_ROLL_ANGLE_FULL,
		.yaw_angle = FD_CONFIG_YAW_ANGLE_45_90,
		.smile_mode = FD_CONFIG_SMILE_MODE_DISABLE,
		.blink_mode = FD_CONFIG_BLINK_MODE_DISABLE,
		.eye_detect = FD_CONFIG_EYES_DETECT_ENABLE,
		.mouth_detect = FD_CONFIG_MOUTH_DETECT_DISABLE,
		.orientation = FD_CONFIG_ORIENTATION_DISABLE,
		.orientation_value = 0,
		.err = ERROR_FD_NO,
	},
};

static void fimc_is_ischain_cache_flush(struct fimc_is_device_ischain *this,
	u32 offset, u32 size)
{
	vb2_ion_sync_for_device(this->imemory.fw_cookie,
		offset,
		size,
		DMA_TO_DEVICE);
}

static void fimc_is_ischain_region_invalid(struct fimc_is_device_ischain *device)
{
	vb2_ion_sync_for_device(
		device->imemory.fw_cookie,
		device->imemory.offset_region,
		sizeof(struct is_region),
		DMA_FROM_DEVICE);
}

static void fimc_is_ischain_region_flush(struct fimc_is_device_ischain *device)
{
	vb2_ion_sync_for_device(
		device->imemory.fw_cookie,
		device->imemory.offset_region,
		sizeof(struct is_region),
		DMA_TO_DEVICE);
}

void fimc_is_ischain_meta_flush(struct fimc_is_frame *frame)
{
#ifdef ENABLE_CACHE
	vb2_ion_sync_for_device(
		(void *)frame->cookie_shot,
		0,
		frame->shot_size,
		DMA_TO_DEVICE);
#endif
}

void fimc_is_ischain_meta_invalid(struct fimc_is_frame *frame)
{
#ifdef ENABLE_CACHE
	vb2_ion_sync_for_device(
		(void *)frame->cookie_shot,
		0,
		frame->shot_size,
		DMA_FROM_DEVICE);
#endif
}

void fimc_is_ischain_version(enum fimc_is_bin_type type, const char *load_bin, u32 size)
{
	char version_str[60];

	if (type == FIMC_IS_BIN_FW) {
		memcpy(version_str, &load_bin[size - FIMC_IS_VERSION_SIZE],
			FIMC_IS_VERSION_SIZE);
		version_str[FIMC_IS_VERSION_SIZE] = '\0';

		info("FW version : %s\n", version_str);
	} else {
		memcpy(version_str, &load_bin[size - FIMC_IS_SETFILE_VER_OFFSET],
			FIMC_IS_SETFILE_VER_SIZE);
		version_str[FIMC_IS_SETFILE_VER_SIZE] = '\0';

		info("SETFILE version : %s\n", version_str);
	}
}

void fimc_is_ischain_savefirm(struct fimc_is_device_ischain *this)
{
#ifdef DEBUG_DUMP_FIRMWARE
	loff_t pos;

	write_data_to_file("/data/firmware.bin", (char *)this->imemory.kvaddr,
		(size_t)FIMC_IS_A5_MEM_SIZE, &pos);
#endif
}

static int fimc_is_ischain_loadfirm(struct fimc_is_device_ischain *device)
{
	const struct firmware *fw_blob = NULL;
	u8 *buf = NULL;
	int ret = 0;
	int location = 0;
	int retry_count = 0;
#ifdef SDCARD_FW
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;
	char fw_path[100];

#ifdef CONFIG_USE_VENDER_FEATURE
	struct fimc_is_from_info *sysfs_finfo;
	fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
#endif

	mdbgd_ischain("%s\n", device, __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(FIMC_IS_FW_SDCARD, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
#ifdef CONFIG_USE_VENDER_FEATURE
		if (is_dumped_fw_loading_needed &&
			device->pdev->id == SENSOR_POSITION_REAR) {
			snprintf(fw_path, sizeof(fw_path), "%s%s",
				FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW);
			fp = filp_open(fw_path, O_RDONLY, 0);
			if (IS_ERR_OR_NULL(fp)) {
				fp = NULL;
				ret = -EIO;
				set_fs(old_fs);
				goto out;
			}
		} else
#endif
			goto request_fw;
	}

	location = 1;
	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n",
			is_dumped_fw_loading_needed ? fw_path : FIMC_IS_FW_SDCARD, fsize);
	buf = vmalloc(fsize);
	if (!buf) {
		dev_err(&device->pdev->dev,
			"failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}
	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		dev_err(&device->pdev->dev,
			"failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto out;
	}

	memcpy((void *)device->imemory.kvaddr, (void *)buf, fsize);
	fimc_is_ischain_cache_flush(device, 0, fsize + 1);
	fimc_is_ischain_version(FIMC_IS_BIN_FW, buf, fsize);

request_fw:
	if (fw_requested) {
		set_fs(old_fs);
#endif
		retry_count = 3;

#ifdef VENDER_PATH
		ret = request_firmware(&fw_blob, sysfs_finfo->load_fw_name, &device->pdev->dev);
		while (--retry_count && ret == -EAGAIN) {
			err("request_firmware retry(count:%d)", retry_count);
			ret = request_firmware(&fw_blob, sysfs_finfo->load_fw_name, &device->pdev->dev);
		}
#else
		ret = request_firmware(&fw_blob, FIMC_IS_FW, &device->pdev->dev);
		while (--retry_count && ret == -EAGAIN) {
			err("request_firmware retry(count:%d)", retry_count);
			ret = request_firmware(&fw_blob, FIMC_IS_FW, &device->pdev->dev);
		}
#endif

		if (ret) {
			err("request_firmware is fail(%d)", ret);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob) {
			merr("fw_blob is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob->data) {
			merr("fw_blob->data is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		memcpy((void *)device->imemory.kvaddr, fw_blob->data, fw_blob->size);
		fimc_is_ischain_cache_flush(device, 0, fw_blob->size + 1);
		fimc_is_ischain_version(FIMC_IS_BIN_FW, fw_blob->data, fw_blob->size);
#ifdef SDCARD_FW
	}
#endif

out:
#ifdef SDCARD_FW
	if (!fw_requested) {
		if (buf) {
			vfree(buf);
		}
		if (!IS_ERR_OR_NULL(fp)) {
			filp_close(fp, current->files);
		}
		set_fs(old_fs);
	} else
#endif
	{
		if (!IS_ERR_OR_NULL(fw_blob))
			release_firmware(fw_blob);
	}
	if (ret)
		err("firmware loading is fail");
	else
		info("Camera: the %s FW were applied successfully.\n",
			((cam_id == CAMERA_SINGLE_REAR) &&
				is_dumped_fw_loading_needed) ? "dumped" : "default");

	return ret;
}

static int fimc_is_ischain_loadsetf(struct fimc_is_device_ischain *device,
	u32 load_addr, char *setfile_name)
{
	int ret = 0;
	int location = 0;
	void *address;
	const struct firmware *fw_blob = NULL;
	u8 *buf = NULL;

#ifdef SDCARD_FW
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;
	char setfile_path[256];
	u32 retry;

	mdbgd_ischain("%s\n", device, __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	memset(setfile_path, 0x00, sizeof(setfile_path));
	snprintf(setfile_path, sizeof(setfile_path), "%s%s",
		FIMC_IS_SETFILE_SDCARD_PATH, setfile_name);
	fp = filp_open(setfile_path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
#ifdef CONFIG_USE_VENDER_FEATURE
		if (is_dumped_fw_loading_needed &&
			device->pdev->id == SENSOR_POSITION_REAR) {
			memset(setfile_path, 0x00, sizeof(setfile_path));
			snprintf(setfile_path, sizeof(setfile_path), "%s%s",
				FIMC_IS_FW_DUMP_PATH, setfile_name);
			fp = filp_open(setfile_path, O_RDONLY, 0);
			if (IS_ERR_OR_NULL(fp)) {
				ret = -EIO;
				fp = NULL;
				set_fs(old_fs);
				goto out;
			}
		} else
#endif
			goto request_fw;
	}

	location = 1;
	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n",
		setfile_path, fsize);

	buf = vmalloc(fsize);
	if (!buf) {
		dev_err(&device->pdev->dev,
			"failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}
	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		dev_err(&device->pdev->dev,
			"failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto out;
	}

	address = (void *)(device->imemory.kvaddr + load_addr);
	memcpy((void *)address, (void *)buf, fsize);
	fimc_is_ischain_cache_flush(device, load_addr, fsize + 1);
	fimc_is_ischain_version(FIMC_IS_BIN_SETFILE, buf, fsize);

request_fw:
	if (fw_requested) {
		set_fs(old_fs);
#endif

		retry = 4;
		ret = request_firmware((const struct firmware **)&fw_blob,
			setfile_name, &device->pdev->dev);
		while (--retry && ret) {
			mwarn("request_firmware is fail(%d)", device, ret);
			ret = request_firmware((const struct firmware **)&fw_blob,
				setfile_name, &device->pdev->dev);
		}

		if (!retry) {
			merr("request_firmware is fail(%d)", device, ret);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob) {
			merr("fw_blob is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob->data) {
			merr("fw_blob->data is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		address = (void *)(device->imemory.kvaddr + load_addr);
		memcpy(address, fw_blob->data, fw_blob->size);
		fimc_is_ischain_cache_flush(device, load_addr, fw_blob->size + 1);
		fimc_is_ischain_version(FIMC_IS_BIN_SETFILE, fw_blob->data, (u32)fw_blob->size);
#ifdef SDCARD_FW
	}
#endif

out:
#ifdef SDCARD_FW
	if (!fw_requested) {
		if (buf) {
			vfree(buf);
		}
		if (!IS_ERR_OR_NULL(fp)) {
			filp_close(fp, current->files);
		}
		set_fs(old_fs);
	} else
#endif
	{
		if (!IS_ERR_OR_NULL(fw_blob))
			release_firmware(fw_blob);
	}

	if (ret)
		err("setfile loading is fail");
	else
		info("Camera: the %s Setfile were applied successfully.\n",
			((cam_id == CAMERA_SINGLE_REAR) &&
				is_dumped_fw_loading_needed) ? "dumped" : "default");

	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
static int fimc_is_ischain_loadcalb_eeprom(struct fimc_is_device_ischain *device,
	struct fimc_is_module_enum *active_sensor, int id)
{
	int ret = 0;
	struct meizu_otp *cal_buf = NULL;
	char *cal_ptr;
	u32 start_addr = 0;
	int cal_size = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
	}

	mdbgd_ischain("%s\n", device, __func__);

	start_addr = FIMC_IS_CAL_START_ADDR;
	cal_size = FIMC_IS_MAX_CAL_SIZE;
	fimc_is_eeprom_get_cal_buf(&cal_buf);

	cal_ptr = (char *)(device->imemory.kvaddr + start_addr);

	/* CRC check */
	if (fimc_is_eeprom_check_state() >= FIMC_IS_EEPROM_STATE_READONE) {
		memcpy((void *)(cal_ptr) ,(void *)cal_buf->data, FIMC_IS_MAX_CAL_SIZE);
		info("Camera : the dumped Cal. data was applied successfully.\n");
	} else if (fimc_is_eeprom_check_state() == FIMC_IS_EEPROM_STATE_INIT) {
		memset((void *)(cal_ptr), 0xFF, FIMC_IS_MAX_CAL_SIZE);
		info("Camera : did not try to read eeprom. so fill all data with oxff.\n");
	} else {
		memset((void *)(cal_ptr), 0xFF, FIMC_IS_MAX_CAL_SIZE);
		info("Camera : CRC32 or I2C error , so fill all data with oxff .\n");
		ret = -EIO;
	}

	fimc_is_ischain_cache_flush(device, start_addr, cal_size);
	if (ret)
		mwarn("calibration loading is fail", device);
	else
		mwarn("calibration loading is success", device);

	return ret;
}
#endif

static int fimc_is_ischain_config_secure(struct fimc_is_device_ischain *device)
{
	int ret = 0;

#ifdef CONFIG_ARM_TRUSTZONE
	u32 i;

	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(PA_FIMC_IS_GIC_C + 0x4), 0x000000FF, 0);
	for (i = 0; i < 5; i++)
		exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(PA_FIMC_IS_GIC_D + 0x80 + (i * 4)), 0xFFFFFFFF, 0);
	for (i = 0; i < 40; i++)
		exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(PA_FIMC_IS_GIC_D + 0x400 + (i * 4)), 0x10101010, 0);

#ifdef CONFIG_ARM64
	{
		ulong debug;
		exynos_smc_readsfr(PA_FIMC_IS_GIC_C + 0x4, &debug);
		info("%s : PA_FIMC_IS_GIC_C : 0x%08lx\n", __func__, debug);
		if (debug == 0x00) {
			merr("secure configuration is fail[0x131E0004:%08lX]", device, debug);
			ret = -EINVAL;
		}
	}
#else
	{
		u32 debug;
		exynos_smc_readsfr(PA_FIMC_IS_GIC_C + 0x4, &debug);
		info("%s : PA_FIMC_IS_GIC_C : 0x%08x\n", __func__, debug);
		if (debug == 0x00) {
			merr("secure configuration is fail[0x131E0004:%08X]", device, debug);
			ret = -EINVAL;
		}
	}
#endif
#endif

	return ret;
}

static int fimc_is_itf_s_param(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 lindex,
	u32 hindex,
	u32 indexes)
{
	int ret = 0;
	u32 flag, index;
	ulong dst_base, src_base;

	BUG_ON(!device);

	if (frame) {
		dst_base = (ulong)&device->is_region->parameter;
#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
		src_base = (ulong)frame->shot->ctl.vendor_entry.parameter;

		frame->shot->ctl.vendor_entry.lowIndexParam |= lindex;
		frame->shot->ctl.vendor_entry.highIndexParam |= hindex;
#else
		src_base = (ulong)frame->shot->ctl.entry.parameter;

		frame->shot->ctl.entry.lowIndexParam |= lindex;
		frame->shot->ctl.entry.highIndexParam |= hindex;
#endif

		for (index = 0; lindex && (index < 32); index++) {
			flag = 1 << index;
			if (lindex & flag) {
				memcpy((ulong *)(dst_base + (index * PARAMETER_MAX_SIZE)),
					(ulong *)(src_base + (index * PARAMETER_MAX_SIZE)),
					PARAMETER_MAX_SIZE);
				lindex &= ~flag;
			}
		}

		for (index = 0; hindex && (index < 32); index++) {
			flag = 1 << index;
			if (hindex & flag) {
				memcpy((u32 *)(dst_base + ((32 + index) * PARAMETER_MAX_SIZE)),
					(u32 *)(src_base + ((32 + index) * PARAMETER_MAX_SIZE)),
					PARAMETER_MAX_SIZE);
				hindex &= ~flag;
			}
		}

		fimc_is_ischain_region_flush(device);
	} else {
		/*
		 * this check code is commented until per-frame control is worked fully
		 *
		 * if ( test_bit(FIMC_IS_ISCHAIN_START, &device->state)) {
		 *	merr("s_param is fail, device already is started", device);
		 *	BUG();
		 * }
		 */

		fimc_is_ischain_region_flush(device);

		if (lindex || hindex) {
			ret = fimc_is_hw_s_param(device->interface,
				device->instance,
				lindex,
				hindex,
				indexes);
		}
	}

	return ret;
}

void * fimc_is_itf_g_param(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 index)
{
	ulong dst_base, src_base, dst_param, src_param;

	BUG_ON(!device);

	if (frame) {
#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
		dst_base = (ulong)&frame->shot->ctl.vendor_entry.parameter[0];
#else
		dst_base = (ulong)&frame->shot->ctl.entry.parameter[0];
#endif
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
		src_base = (ulong)&device->is_region->parameter;
		src_param = (src_base + (index * PARAMETER_MAX_SIZE));
		memcpy((ulong *)dst_param, (ulong *)src_param, PARAMETER_MAX_SIZE);
	} else {
		dst_base = (ulong)&device->is_region->parameter;
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
	}

	return (void *)dst_param;
}

int fimc_is_itf_a_param(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;
	u32 setfile;

	BUG_ON(!device);

	setfile = (device->setfile & FIMC_IS_SETFILE_MASK);

	ret = fimc_is_hw_a_param(device->interface,
		device->instance,
		group,
		setfile);

	return ret;
}

static int fimc_is_itf_f_param(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 setfile;
	u32 group = 0;
	struct fimc_is_path_info *path;
#ifdef DEBUG
	u32 navailable = 0;
	struct is_region *region = device->is_region;

	minfo(" NAME       SIZE   FORMAT\n", device);
	minfo("  SEN  %04dx%04d      %3d\n", device,
		region->parameter.sensor.config.width,
		region->parameter.sensor.config.height,
		navailable);

	minfo(" NAME        CMD   BYPASS\n", device);
	minfo("  3AA          %d        %d\n", device,
		region->parameter.taa.control.cmd,
		region->parameter.taa.control.bypass);
	minfo(" PATH       SIZE   FORMAT\n", device);
	if (region->parameter.taa.otf_input.cmd)
		minfo("   OI  %04dx%04d      %3d\n", device,
			region->parameter.taa.otf_input.width,
			region->parameter.taa.otf_input.height,
			region->parameter.taa.otf_input.format);
	if (region->parameter.taa.vdma1_input.cmd)
		minfo("   DI  %04dx%04d      %3d\n", device,
			region->parameter.taa.vdma1_input.width,
			region->parameter.taa.vdma1_input.height,
			region->parameter.taa.vdma1_input.format);
	if (region->parameter.taa.vdma4_output.cmd)
		minfo("  DO1  %04dx%04d      %3d\n", device,
			region->parameter.taa.vdma4_output.width,
			region->parameter.taa.vdma4_output.height,
			region->parameter.taa.vdma4_output.format);
	if (region->parameter.taa.vdma2_output.cmd)
		minfo("  DO2  %04dx%04d      %3d\n", device,
			region->parameter.taa.vdma2_output.width,
			region->parameter.taa.vdma2_output.height,
			region->parameter.taa.vdma2_output.format);
	if (region->parameter.taa.otf_output.cmd)
		minfo("   OO  %04dx%04d      %3d\n", device,
			region->parameter.taa.otf_output.width,
			region->parameter.taa.otf_output.height,
			region->parameter.taa.otf_output.format);

	minfo(" NAME        CMD   BYPASS\n", device);
	minfo("  ISP          %d        %d\n", device,
		region->parameter.isp.control.cmd,
		region->parameter.isp.control.bypass);
	minfo(" PATH       SIZE   FORMAT\n", device);
	if (region->parameter.isp.otf_input.cmd)
		minfo("   OI  %04dx%04d      %3d\n", device,
			region->parameter.isp.otf_input.width,
			region->parameter.isp.otf_input.height,
			region->parameter.isp.otf_input.format);
	if (region->parameter.isp.vdma1_input.cmd)
		minfo("   DI  %04dx%04d      %3d\n", device,
			region->parameter.isp.vdma1_input.width,
			region->parameter.isp.vdma1_input.height,
			region->parameter.isp.vdma1_input.format);
	if (region->parameter.isp.vdma4_output.cmd)
		minfo("  DO1  %04dx%04d      %3d\n", device,
			region->parameter.isp.vdma4_output.width,
			region->parameter.isp.vdma4_output.height,
			region->parameter.isp.vdma4_output.format);
	if (region->parameter.isp.vdma5_output.cmd)
		minfo("  DO2  %04dx%04d      %3d\n", device,
			region->parameter.isp.vdma5_output.width,
			region->parameter.isp.vdma5_output.height,
			region->parameter.isp.vdma5_output.format);
	if (region->parameter.isp.otf_output.cmd)
		minfo("   OO  %04dx%04d      %3d\n", device,
			region->parameter.isp.otf_output.width,
			region->parameter.isp.otf_output.height,
			region->parameter.isp.otf_output.format);

	minfo(" NAME        CMD   BYPASS\n", device);
	minfo("  DRC          %d        %d\n", device,
		region->parameter.drc.control.cmd,
		region->parameter.drc.control.bypass);
	minfo(" PATH       SIZE   FORMAT\n", device);
	if (region->parameter.drc.otf_input.cmd)
		minfo("   OI  %04dx%04d      %3d\n", device,
			region->parameter.drc.otf_input.width,
			region->parameter.drc.otf_input.height,
			region->parameter.drc.otf_input.format);
	if (region->parameter.drc.otf_output.cmd)
		minfo("   OO  %04dx%04d      %3d\n", device,
			region->parameter.drc.otf_output.width,
			region->parameter.drc.otf_output.height,
			region->parameter.drc.otf_output.format);
#ifdef SOC_SCC
	minfo(" NAME        CMD   BYPASS\n", device);
	minfo("  SCC          %d        %d\n", device,
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass);
	minfo(" PATH       SIZE   FORMAT\n", device);
	if (region->parameter.scalerc.otf_input.cmd)
		minfo("   OI  %04dx%04d      %3d\n", device,
			region->parameter.scalerc.otf_input.width,
			region->parameter.scalerc.otf_input.height,
			region->parameter.scalerc.otf_input.format);
	if (region->parameter.scalerc.dma_output.cmd)
		minfo("   DO  %04dx%04d      %3d\n", device,
			region->parameter.scalerc.dma_output.width,
			region->parameter.scalerc.dma_output.height,
			region->parameter.scalerc.dma_output.format);
	if (region->parameter.scalerc.otf_output.cmd)
		minfo("   OO  %04dx%04d      %3d\n", device,
			region->parameter.scalerc.otf_output.width,
			region->parameter.scalerc.otf_output.height,
			region->parameter.scalerc.otf_output.format);
#endif
	minfo(" NAME        CMD   BYPASS\n", device);
	minfo("  DIS          %d        %d\n", device,
		region->parameter.dis.control.cmd,
		region->parameter.dis.control.bypass);
	minfo(" PATH       SIZE   FORMAT\n", device);
	if (region->parameter.dis.otf_input.cmd)
		minfo("   OI  %04dx%04d      %3d\n", device,
			region->parameter.dis.otf_input.width,
			region->parameter.dis.otf_input.height,
			region->parameter.dis.otf_input.format);
	if (region->parameter.dis.otf_output.cmd)
		minfo("   OO  %04dx%04d      %3d\n", device,
			region->parameter.dis.otf_output.width,
			region->parameter.dis.otf_output.height,
			region->parameter.dis.otf_output.format);

	minfo(" NAME        CMD   BYPASS\n", device);
	minfo("  SCP          %d        %d\n", device,
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass);
	minfo(" PATH       SIZE   FORMAT\n", device);
	if (region->parameter.scalerp.otf_input.cmd)
		minfo("   OI  %04dx%04d      %3d\n", device,
			region->parameter.scalerp.otf_input.width,
			region->parameter.scalerp.otf_input.height,
			region->parameter.scalerp.otf_input.format);
	if (region->parameter.scalerp.dma_output.cmd)
		minfo("   DO  %04dx%04d      %3d\n", device,
			region->parameter.scalerp.dma_output.width,
			region->parameter.scalerp.dma_output.height,
			region->parameter.scalerp.dma_output.format);
	if (region->parameter.scalerp.otf_output.cmd)
		minfo("   OO  %04dx%04d      %3d\n", device,
			region->parameter.scalerp.otf_output.width,
			region->parameter.scalerp.otf_output.height,
			region->parameter.scalerp.otf_output.format);

	minfo(" NAME        CMD   BYPASS\n", device);
	minfo("  VRA          %d        %d\n", device,
		region->parameter.vra.control.cmd,
		region->parameter.vra.control.bypass);
	minfo(" PATH       SIZE   FORMAT\n", device);
	if (region->parameter.vra.otf_input.cmd)
		minfo("   OI  %04dx%04d      %3d\n", device,
			region->parameter.vra.otf_input.width,
			region->parameter.vra.otf_input.height,
			region->parameter.vra.otf_input.format);

	minfo(" NAME   CMD    IN_SZIE   OT_SIZE      CROP       POS\n", device);
	minfo("SCC CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerc.input_crop.cmd,
		region->parameter.scalerc.input_crop.in_width,
		region->parameter.scalerc.input_crop.in_height,
		region->parameter.scalerc.input_crop.out_width,
		region->parameter.scalerc.input_crop.out_height,
		region->parameter.scalerc.input_crop.crop_width,
		region->parameter.scalerc.input_crop.crop_height,
		region->parameter.scalerc.input_crop.pos_x,
		region->parameter.scalerc.input_crop.pos_y
		);
	minfo("SCC CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerc.output_crop.cmd,
		navailable,
		navailable,
		navailable,
		navailable,
		region->parameter.scalerc.output_crop.crop_width,
		region->parameter.scalerc.output_crop.crop_height,
		region->parameter.scalerc.output_crop.pos_x,
		region->parameter.scalerc.output_crop.pos_y
		);
	minfo("SCP CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerp.input_crop.cmd,
		region->parameter.scalerp.input_crop.in_width,
		region->parameter.scalerp.input_crop.in_height,
		region->parameter.scalerp.input_crop.out_width,
		region->parameter.scalerp.input_crop.out_height,
		region->parameter.scalerp.input_crop.crop_width,
		region->parameter.scalerp.input_crop.crop_height,
		region->parameter.scalerp.input_crop.pos_x,
		region->parameter.scalerp.input_crop.pos_y
		);
	minfo("SCP CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerp.output_crop.cmd,
		navailable,
		navailable,
		navailable,
		navailable,
		region->parameter.scalerp.output_crop.crop_width,
		region->parameter.scalerp.output_crop.crop_height,
		region->parameter.scalerp.output_crop.pos_x,
		region->parameter.scalerp.output_crop.pos_y
		);
#endif

	path = &device->path;

	if (path->group[GROUP_SLOT_3AA] != GROUP_ID_INVALID)
		group |= (GROUP_ID(path->group[GROUP_SLOT_3AA]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_ISP] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_isp.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_ISP]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_DIS] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_dis.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_DIS]) & GROUP_ID_PARM_MASK);

	setfile = (device->setfile & FIMC_IS_SETFILE_MASK);

	ret = fimc_is_hw_a_param(device->interface,
		device->instance,
		(group & GROUP_ID_PARM_MASK),
		setfile);
	return ret;
}

static int fimc_is_itf_enum(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_hw_enum(device->interface);
	if (ret) {
		merr("fimc_is_itf_enum is fail(%d)", device, ret);
		CALL_POPS(device, print_clk);
	}

	return ret;
}

void fimc_is_itf_storefirm(struct fimc_is_device_ischain *device)
{
	mdbgd_ischain("%s()\n", device, __func__);

	fimc_is_storefirm(device->interface);
}

void fimc_is_itf_restorefirm(struct fimc_is_device_ischain *device)
{
	mdbgd_ischain("%s()\n", device, __func__);

	fimc_is_restorefirm(device->interface);
}

int fimc_is_itf_set_fwboot(struct fimc_is_device_ischain *device, u32 val)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_set_fwboot(device->interface, val);
	if (ret) {
		merr("fimc_is_set_fwboot is fail(%d)", device, ret);
	}

	return ret;
}

static int fimc_is_itf_open(struct fimc_is_device_ischain *device,
	u32 module_id,
	u32 flag,
	struct fimc_is_path_info *path,
	struct sensor_open_extended *ext_info)
{
	int ret = 0;
	u32 offset_ext, offset_path;
	struct is_region *region;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!device->sensor);
	BUG_ON(!device->interface);
	BUG_ON(!ext_info);

	if (test_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state)) {
		merr("stream is already open", device);
		ret = -EINVAL;
		goto p_err;
	}

	region = device->is_region;
	offset_ext = 0;
	memcpy(&region->shared[offset_ext], ext_info, sizeof(struct sensor_open_extended));

	offset_path = (sizeof(struct sensor_open_extended) / 4) + 1;
	memcpy(&region->shared[offset_path], path, sizeof(struct fimc_is_path_info));

	fimc_is_ischain_region_flush(device);

	ret = fimc_is_hw_open(device->interface,
		device->instance,
		module_id,
		device->imemory.dvaddr_shared,
		device->imemory.dvaddr_shared + (offset_path * 4),
		flag,
		&device->margin_width,
		&device->margin_height);
	if (ret) {
		merr("fimc_is_hw_open is fail", device);
		CALL_POPS(device, print_clk);
		fimc_is_sensor_gpio_dbg(device->sensor);
		ret = -EINVAL;
		goto p_err;
	}

	/* HACK */
	device->margin_left = 8;
	device->margin_right = 8;
	device->margin_top = 6;
	device->margin_bottom = 4;
	device->margin_width = device->margin_left + device->margin_right;
	device->margin_height = device->margin_top + device->margin_bottom;
	mdbgd_ischain("margin %dx%d\n", device, device->margin_width, device->margin_height);

	fimc_is_ischain_region_invalid(device);

	if (region->shared[MAX_SHARED_COUNT-1] != MAGIC_NUMBER) {
		merr("MAGIC NUMBER error", device);
		ret = -EINVAL;
		goto p_err;
	}

	memset(&region->parameter, 0x0, sizeof(struct is_param_region));

	memcpy(&region->parameter.sensor, &init_sensor_param,
		sizeof(struct sensor_param));
	memcpy(&region->parameter.taa, &init_taa_param,
		sizeof(struct taa_param));
	memcpy(&region->parameter.isp, &init_isp_param,
		sizeof(struct isp_param));
	memcpy(&region->parameter.drc, &init_drc_param,
		sizeof(struct drc_param));
	memcpy(&region->parameter.scalerc, &init_scc_param,
		sizeof(struct scc_param));
	memcpy(&region->parameter.tpu, &init_tpu_param,
		sizeof(struct tpu_param));
	memcpy(&region->parameter.scalerp, &init_scp_param,
		sizeof(struct scp_param));
	memcpy(&region->parameter.vra, &init_vra_param,
		sizeof(struct vra_param));

	set_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state);

p_err:
	return ret;
}

static int fimc_is_itf_close(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	BUG_ON(!device);
	BUG_ON(!device->interface);

	if (!test_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state)) {
		mwarn("stream is already close", device);
		goto p_err;
	}

	ret = fimc_is_hw_close(device->interface, device->instance);
	if (ret)
		merr("fimc_is_hw_close is fail", device);

	clear_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state);

p_err:
	return ret;
}

static int fimc_is_itf_setfile(struct fimc_is_device_ischain *device,
	char *setfile_name)
{
	int ret = 0;
	u32 setfile_addr = 0;
	struct fimc_is_interface *itf;

	BUG_ON(!device);
	BUG_ON(!device->interface);
	BUG_ON(!setfile_name);

	itf = device->interface;

	mdbgd_ischain("%s(setfile : %s)\n", device, __func__, setfile_name);

	ret = fimc_is_hw_saddr(itf, device->instance, &setfile_addr);
	if (ret) {
		merr("fimc_is_hw_saddr is fail(%d)", device, ret);
		goto p_err;
	}

	if (!setfile_addr) {
		merr("setfile address is NULL", device);
		pr_err("cmd : %08X\n", readl(&itf->com_regs->ihcmd));
		pr_err("id : %08X\n", readl(&itf->com_regs->ihc_stream));
		pr_err("param1 : %08X\n", readl(&itf->com_regs->ihc_param1));
		pr_err("param2 : %08X\n", readl(&itf->com_regs->ihc_param2));
		pr_err("param3 : %08X\n", readl(&itf->com_regs->ihc_param3));
		pr_err("param4 : %08X\n", readl(&itf->com_regs->ihc_param4));
		goto p_err;
	}

	ret = fimc_is_ischain_loadsetf(device, setfile_addr, setfile_name);
	if (ret) {
		merr("fimc_is_ischain_loadsetf is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_hw_setfile(itf, device->instance);
	if (ret) {
		merr("fimc_is_hw_setfile is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_itf_map(struct fimc_is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size)
{
	int ret = 0;

	BUG_ON(!device);

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_hw_map(device->interface, device->instance, group, shot_addr, shot_size);
	if (ret)
		merr("fimc_is_hw_map is fail(%d)", device, ret);

	return ret;
}

static int fimc_is_itf_unmap(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_hw_unmap(device->interface, device->instance, group);
	if (ret)
		merr("fimc_is_hw_unmap is fail(%d)", device, ret);

	return ret;
}

int fimc_is_itf_stream_on(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 retry = 30000;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group_leader;
	struct fimc_is_resourcemgr *resourcemgr;
	u32 scount, async_shots;

	BUG_ON(!device);
	BUG_ON(!device->groupmgr);
	BUG_ON(!device->resourcemgr);
	BUG_ON(!device->sensor);
	BUG_ON(!device->sensor->pdata);

	groupmgr = device->groupmgr;
	resourcemgr = device->resourcemgr;
	group_leader = groupmgr->leader[device->instance];

	if (!group_leader) {
		merr("stream leader is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	async_shots = group_leader->async_shots;
	scount = atomic_read(&group_leader->scount);

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group_leader->state)) {
		/* 3ax  group should be started */
		if (!test_bit(FIMC_IS_GROUP_START, &group_leader->state)) {
			merr("stream leader is NOT started", device);
			ret = -EINVAL;
			goto p_err;
		}

		while (--retry && (scount < async_shots)) {
			udelay(100);
			scount = atomic_read(&group_leader->scount);
		}
	}

	if (retry)
		minfo("[ISC:D] stream on ready(%d, %d)\n", device, scount, async_shots);
	else
		merr("[ISC:D] stream on NOT ready(%d, %d)\n", device, scount, async_shots);

#ifdef ENABLE_DVFS
	if ((!pm_qos_request_active(&device->user_qos)) && (sysfs_debug.en_dvfs)) {
		int scenario_id;
		int dvfs_table_idx = 0;
		struct fimc_is_core *core = (struct fimc_is_core *)platform_get_drvdata(device->pdev);

		mutex_lock(&resourcemgr->dvfs_ctrl.lock);

		/*
		 * Setting dvfs table by HAL ver.
		 * Default value is 0
		 */
		switch(resourcemgr->hal_version) {
		case IS_HAL_VER_1_0:
			resourcemgr->dvfs_ctrl.dvfs_table_idx =
				core->pdata->dvfs_hal_1_0_table_idx;
			minfo("[ISC:D] tbl[%d] is selected(1.0 HAL)\n", device,
				core->pdata->dvfs_hal_1_0_table_idx);
			break;
		case IS_HAL_VER_3_2:
			resourcemgr->dvfs_ctrl.dvfs_table_idx =
				core->pdata->dvfs_hal_3_2_table_idx;
			minfo("[ISC:D] tbl[%d] is selected(3.2 HAL)\n", device,
				core->pdata->dvfs_hal_3_2_table_idx);
			break;
		default:
			resourcemgr->dvfs_ctrl.dvfs_table_idx = 0;
			mwarn("[ISC:D] hal version invalid(%d)", device,
				resourcemgr->hal_version);
			break;
		}
		dvfs_table_idx = resourcemgr->dvfs_ctrl.dvfs_table_idx;

		/* try to find dynamic scenario to apply */
		scenario_id = fimc_is_dvfs_sel_static(device);
		if (scenario_id >= 0) {
			struct fimc_is_dvfs_scenario_ctrl *static_ctrl =
				resourcemgr->dvfs_ctrl.static_ctrl;
			minfo("[ISC:D] tbl[%d] static scenario(%d)-[%s]\n", device,
				dvfs_table_idx,	scenario_id,
				static_ctrl->scenarios[static_ctrl->cur_scenario_idx].scenario_nm);
			fimc_is_set_dvfs(device, scenario_id);
		}

		mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
	}
#endif

	ret = fimc_is_hw_stream_on(device->interface, device->instance);
	if (ret) {
		merr("fimc_is_hw_stream_on is fail(%d)", device, ret);
		CALL_POPS(device, print_clk);
		fimc_is_sensor_gpio_dbg(device->sensor);
	}

p_err:
	return ret;
}

int fimc_is_itf_stream_off(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	BUG_ON(!device);
	BUG_ON(!device->sensor);
	BUG_ON(!device->sensor->pdata);

	minfo("[ISC:D] stream off ready\n", device);

	ret = fimc_is_hw_stream_off(device->interface, device->instance);
	if (ret) {
		merr("fimc_is_hw_stream_off is fail(%d)", device, ret);
		CALL_POPS(device, print_clk);
		fimc_is_sensor_gpio_dbg(device->sensor);
	}

	return ret;
}

int fimc_is_itf_process_start(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	ret = fimc_is_hw_process_on(device->interface, device->instance, group);

	return ret;
}

int fimc_is_itf_process_stop(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif

	ret = fimc_is_hw_process_off(device->interface, device->instance, group, 0);

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, device->instance, false);
#endif

	return ret;
}

int fimc_is_itf_force_stop(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
#endif

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif

	ret = fimc_is_hw_process_off(device->interface, device->instance, group, 1);

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, device->instance, false);
#endif

	return ret;
}

static int fimc_is_itf_init_process_start(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;
	struct fimc_is_path_info *path;

	path = &device->path;

	if (path->group[GROUP_SLOT_3AA] != GROUP_ID_INVALID)
		group |= (GROUP_ID(path->group[GROUP_SLOT_3AA]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_ISP] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_isp.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_ISP]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_DIS] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_dis.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_DIS]) & GROUP_ID_PARM_MASK);

	ret = fimc_is_hw_process_on(device->interface, device->instance, group);

	return ret;
}

static int fimc_is_itf_init_process_stop(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;
	struct fimc_is_path_info *path;
#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
#endif

	path = &device->path;

	if (path->group[GROUP_SLOT_3AA] != GROUP_ID_INVALID)
		group |= (GROUP_ID(path->group[GROUP_SLOT_3AA]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_ISP] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_isp.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_ISP]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_DIS] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_dis.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_DIS]) & GROUP_ID_PARM_MASK);

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif

	ret = fimc_is_hw_process_off(device->interface, device->instance, group, 0);

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, device->instance, false);
#endif
	return ret;
}

int fimc_is_itf_i2c_lock(struct fimc_is_device_ischain *this,
	int i2c_clk, bool lock)
{
	int ret = 0;
	struct fimc_is_interface *itf = this->interface;

	if (lock)
		fimc_is_interface_lock(itf);

	ret = fimc_is_hw_i2c_lock(itf, this->instance, i2c_clk, lock);

	if (!lock)
		fimc_is_interface_unlock(itf);

	return ret;
}

int fimc_is_itf_g_capability(struct fimc_is_device_ischain *this)
{
	int ret = 0;
#ifdef PRINT_CAPABILITY
	u32 metadata;
	u32 index;
	struct camera2_sm *capability;
#endif

	ret = fimc_is_hw_g_capability(this->interface, this->instance,
		(this->imemory.kvaddr_shared - this->imemory.kvaddr));

	fimc_is_ischain_region_invalid(this);

#ifdef PRINT_CAPABILITY
	memcpy(&this->capability, &this->is_region->shared,
		sizeof(struct camera2_sm));
	capability = &this->capability;

	printk(KERN_INFO "===ColorC================================\n");
	printk(KERN_INFO "===ToneMapping===========================\n");
	metadata = capability->tonemap.maxCurvePoints;
	printk(KERN_INFO "maxCurvePoints : %d\n", metadata);

	printk(KERN_INFO "===Scaler================================\n");
	printk(KERN_INFO "foramt : %d, %d, %d, %d\n",
		capability->scaler.availableFormats[0],
		capability->scaler.availableFormats[1],
		capability->scaler.availableFormats[2],
		capability->scaler.availableFormats[3]);

	printk(KERN_INFO "===StatisTicsG===========================\n");
	index = 0;
	metadata = capability->stats.availableFaceDetectModes[index];
	while (metadata) {
		printk(KERN_INFO "availableFaceDetectModes : %d\n", metadata);
		index++;
		metadata = capability->stats.availableFaceDetectModes[index];
	}
	printk(KERN_INFO "maxFaceCount : %d\n",
		capability->stats.maxFaceCount);
	printk(KERN_INFO "histogrambucketCount : %d\n",
		capability->stats.histogramBucketCount);
	printk(KERN_INFO "maxHistogramCount : %d\n",
		capability->stats.maxHistogramCount);
	printk(KERN_INFO "sharpnessMapSize : %dx%d\n",
		capability->stats.sharpnessMapSize[0],
		capability->stats.sharpnessMapSize[1]);
	printk(KERN_INFO "maxSharpnessMapValue : %d\n",
		capability->stats.maxSharpnessMapValue);

	printk(KERN_INFO "===3A====================================\n");
	printk(KERN_INFO "maxRegions : %d\n", capability->aa.maxRegions);

	index = 0;
	metadata = capability->aa.aeAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.aeAvailableModes[index];
	}
	printk(KERN_INFO "aeCompensationStep : %d,%d\n",
		capability->aa.aeCompensationStep.num,
		capability->aa.aeCompensationStep.den);
	printk(KERN_INFO "aeCompensationRange : %d ~ %d\n",
		capability->aa.aeCompensationRange[0],
		capability->aa.aeCompensationRange[1]);
	index = 0;
	metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	while (metadata) {
		printk(KERN_INFO "TargetFpsRanges : %d ~ %d\n", metadata,
			capability->aa.aeAvailableTargetFpsRanges[index][1]);
		index++;
		metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	}
	index = 0;
	metadata = capability->aa.aeAvailableAntibandingModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableAntibandingModes : %d\n",
			metadata);
		index++;
		metadata = capability->aa.aeAvailableAntibandingModes[index];
	}
	index = 0;
	metadata = capability->aa.awbAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "awbAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.awbAvailableModes[index];
	}
	index = 0;
	metadata = capability->aa.afAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "afAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.afAvailableModes[index];
	}
#endif
	return ret;
}

int fimc_is_itf_power_down(struct fimc_is_interface *interface)
{
	int ret = 0;
#ifdef ENABLE_CLOCK_GATE
	/* HACK */
	struct fimc_is_core *core = (struct fimc_is_core *)interface->core;
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, 0, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif

	ret = fimc_is_hw_power_down(interface, 0);

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, 0, false);
#endif

	return ret;
}

int fimc_is_itf_sys_ctl(struct fimc_is_device_ischain *this,
			int cmd, int val)
{
	int ret = 0;
	struct fimc_is_interface *itf = this->interface;

	ret = fimc_is_hw_sys_ctl(itf, this->instance,
				cmd, val);

	return ret;
}

static int fimc_is_itf_sensor_mode(struct fimc_is_device_ischain *ischain)
{
	struct fimc_is_device_sensor *sensor = ischain->sensor;

	return fimc_is_hw_sensor_mode(ischain->interface,
			ischain->instance,
			((sensor->mode << 16) | (ischain->module & 0xFFFF)));
}

int fimc_is_itf_grp_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame)
{
	int ret = 0;

	BUG_ON(!device);
	BUG_ON(!group);
	BUG_ON(!frame);
	BUG_ON(!frame->shot);

	frame->shot->uctl.scalerUd.sourceAddress[0] = frame->dvaddr_buffer[0];
	frame->shot->uctl.scalerUd.sourceAddress[1] = frame->dvaddr_buffer[1];
	frame->shot->uctl.scalerUd.sourceAddress[2] = frame->dvaddr_buffer[2];
	frame->shot->uctl.scalerUd.sourceAddress[3] = frame->dvaddr_buffer[3];

	/* Cache Flush */
	fimc_is_ischain_meta_flush(frame);

	if (frame->shot->magicNumber != SHOT_MAGIC_NUMBER) {
		merr("shot magic number error(0x%08X)\n", device, frame->shot->magicNumber);
		merr("shot_ext size : %zd", device, sizeof(struct camera2_shot_ext));
		ret = -EINVAL;
		goto p_err;
	}

#ifdef ENABLE_CLOCK_GATE
	{
		struct fimc_is_core *core = (struct fimc_is_core *)device->resourcemgr->private_data;

		/* HACK */
		/* dynamic clock on */
		if (sysfs_debug.en_clk_gate &&
				sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
			fimc_is_clk_gate_set(core, group->id, true, false, true);
	}
#endif

	PROGRAM_COUNT(11);

#ifdef DBG_STREAMING
	mgrinfo(" SHOT(%d)\n", device, group, frame, frame->index);
#endif

	ret = fimc_is_hw_shot_nblk(device->interface,
		device->instance,
		GROUP_ID(group->id),
		frame->dvaddr_shot,
		frame->fcount,
		frame->rcount);

p_err:
	return ret;
}

int fimc_is_ischain_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core = (struct fimc_is_core *)platform_get_drvdata(pdev);
	struct exynos_platform_fimc_is *pdata;

#if defined(CONFIG_PM_DEVFREQ)
	int int_qos, mif_qos, cam_qos;
#endif
	pdata = dev_get_platdata(dev);
	if (!pdata)
		err("pdata is null");

	BUG_ON(!pdata);
	BUG_ON(!pdata->clk_off);

	info("FIMC_IS runtime suspend in\n");

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->resourcemgr.mem.alloc_ctx)
		vb2_ion_detach_iommu(core->resourcemgr.mem.alloc_ctx);
#endif

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	exynos7_update_media_layers(TYPE_FIMC_LITE, false);
#endif

	ret = pdata->clk_off(pdev);
	if (ret)
		err("clk_off is fail(%d)", ret);

#if defined(CONFIG_PM_DEVFREQ)
	/* DEVFREQ release */
	dbg_resource("[RSC] %s: QoS UNLOCK\n", __func__);
	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, FIMC_IS_SN_MAX);
	mif_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_MIF, FIMC_IS_SN_MAX);
	cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, START_DVFS_LEVEL);

	if (int_qos > 0)
		pm_qos_remove_request(&exynos_isp_qos_int);
	if (mif_qos > 0)
		pm_qos_remove_request(&exynos_isp_qos_mem);
	if (cam_qos > 0)
                pm_qos_remove_request(&exynos_isp_qos_cam);
#endif

	info("FIMC_IS runtime suspend out\n");
	pm_relax(dev);
	return 0;
}

int fimc_is_ischain_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core = (struct fimc_is_core *)platform_get_drvdata(pdev);
	struct exynos_platform_fimc_is *pdata;

#if defined(CONFIG_PM_DEVFREQ)
	int int_qos, mif_qos, cam_qos;
#endif
	pdata = dev_get_platdata(dev);
	if (!pdata)
		err("pdata is null");

	BUG_ON(!pdata);
	BUG_ON(!pdata->clk_cfg);
	BUG_ON(!pdata->clk_on);

	info("FIMC_IS runtime resume in\n");

	ret = fimc_is_ischain_runtime_resume_pre(dev);
	if (ret) {
		err("fimc_is_runtime_resume_pre is fail(%d)", ret);
		goto p_err;
	}

	ret = pdata->clk_cfg(pdev);
	if (ret) {
		err("clk_cfg is fail(%d)", ret);
		goto p_err;
	}

	/* HACK: DVFS lock sequence is change.
	 * DVFS level should be locked after power on.
	 */
#if defined(CONFIG_PM_DEVFREQ)
	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, START_DVFS_LEVEL);
        mif_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_MIF, START_DVFS_LEVEL);
        cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, START_DVFS_LEVEL);

        /* DEVFREQ lock */
        if (int_qos > 0)
                pm_qos_add_request(&exynos_isp_qos_int, PM_QOS_DEVICE_THROUGHPUT, int_qos);
        if (mif_qos > 0)
                pm_qos_add_request(&exynos_isp_qos_mem, PM_QOS_BUS_THROUGHPUT, mif_qos);
        if (cam_qos > 0)
                pm_qos_add_request(&exynos_isp_qos_cam, PM_QOS_CAM_THROUGHPUT, cam_qos);

        info("[RSC] %s: QoS LOCK [INT(%d), MIF(%d), CAM(%d)]\n",
		__func__, int_qos, mif_qos, cam_qos);
#endif

	/* Clock on */
	ret = pdata->clk_on(pdev);
	if (ret) {
		err("clk_on is fail(%d)", ret);
		goto p_err;
	}

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->resourcemgr.mem.alloc_ctx)
		vb2_ion_attach_iommu(core->resourcemgr.mem.alloc_ctx);
#endif

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	exynos7_update_media_layers(TYPE_FIMC_LITE, true);
#endif

	pm_stay_awake(dev);

p_err:
	info("FIMC-IS runtime resume out\n");
	return ret;
}

int fimc_is_ischain_power(struct fimc_is_device_ischain *device, int on)
{
	int ret = 0;
	u32 val;
	struct device *dev;
	struct fimc_is_core *core;

	BUG_ON(!device);
	BUG_ON(!device->interface);

	dev = &device->pdev->dev;
	core = (struct fimc_is_core *)platform_get_drvdata(device->pdev);

	if (on) {
		/* 2. FIMC-IS local power enable */
#if defined(CONFIG_PM_RUNTIME)
		int rpm_ret;
		mdbgd_ischain("pm_runtime_suspended = %d\n", device, pm_runtime_suspended(dev));
		rpm_ret = pm_runtime_get_sync(dev);
		if (rpm_ret < 0)
			err("pm_runtime_get_sync() return error: %d", rpm_ret);
#else
		fimc_is_ischain_runtime_resume(dev);
		info("%s(%d) - fimc_is runtime resume complete\n", __func__, on);
#endif

#ifdef CONFIG_USE_VENDER_FEATURE
		ret = fimc_is_sec_run_fw_sel(device);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
			goto p_err;
		}
#endif

		if (core->current_position == SENSOR_POSITION_FRONT) {
			fimc_is_itf_set_fwboot(device, COLD_BOOT);
		}

		if (test_bit(IS_IF_RESUME, &device->interface->fw_boot)) {
#ifdef FW_SUSPEND_RESUME
			fimc_is_itf_restorefirm(device);
#endif
		} else {
			ret = fimc_is_ischain_loadfirm(device);
			if (ret) {
				err("fimc_is_ischain_loadfirm is fail(%d)", ret);
				ret = -EINVAL;
				goto p_err;
			}
		}

		set_bit(FIMC_IS_ISCHAIN_LOADED, &device->state);

		/* 4. A5 start address setting */
		mdbgd_ischain("imemory.base(dvaddr) : 0x%08x\n", device, device->imemory.dvaddr);
		mdbgd_ischain("imemory.base(kvaddr) : 0x%08lX\n", device, device->imemory.kvaddr);

		if (!device->imemory.dvaddr) {
			err("firmware device virtual is NULL");
			ret = -ENOMEM;
			goto p_err;
		}

		writel(device->imemory.dvaddr, device->interface->regs + BBOAR);
		val = __raw_readl(device->interface->regs + BBOAR);
		if(device->imemory.dvaddr != val) {
			err("DVA(%x), BBOAR(%x) is not conincedence", device->imemory.dvaddr, val);
			ret = -EINVAL;
			goto p_err;
		}

		ret = fimc_is_ischain_config_secure(device);
		if (ret) {
			err("fimc_is_ischain_config_secure is fail(%d)", ret);
			goto p_err;
		}

		ret = fimc_is_ischain_runtime_resume_post(dev);
		if (ret) {
			err("fimc_is_ischain_runtime_resume_post is fail(%d)", ret);
			goto p_err;
		}

		set_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state);
	} else {
		if (test_bit(IS_IF_SUSPEND, &device->interface->fw_boot)) {
#ifdef FW_SUSPEND_RESUME
			fimc_is_itf_storefirm(device);
#endif
		}

		/* Check FW state for WFI of A5 */
		info("A5 state(0x%x)\n", readl(device->interface->regs + ISSR6));

		/* FIMC-IS local power down */
#if defined(CONFIG_PM_RUNTIME)
		ret = pm_runtime_put_sync(dev);
		if (ret)
			err("pm_runtime_put_sync is fail(%d)", ret);
#else
		ret = fimc_is_ischain_runtime_suspend(dev);
		if (ret)
			err("fimc_is_runtime_suspend is fail(%d)", ret);
#endif

		ret = fimc_is_ischain_runtime_suspend_post(dev);
		if (ret)
			err("fimc_is_runtime_suspend_post is fail(%d)", ret);

		clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state);
	}

p_err:
	info("%s(%d):%d\n", __func__, on, ret);
	return ret;
}

static int fimc_is_ischain_s_sensor_size(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_sensor_config *sensor_config;
	u32 binning, bns_binning;
	u32 sensor_width, sensor_height;
	u32 bns_width, bns_height;
	u32 framerate;

	BUG_ON(!device->sensor);

	binning = fimc_is_sensor_g_bratio(device->sensor);
	sensor_width = fimc_is_sensor_g_width(device->sensor);
	sensor_height = fimc_is_sensor_g_height(device->sensor);
	bns_width = fimc_is_sensor_g_bns_width(device->sensor);
	bns_height = fimc_is_sensor_g_bns_height(device->sensor);
	framerate = fimc_is_sensor_g_framerate(device->sensor);

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		bns_binning = 1000;
	else
		bns_binning = fimc_is_sensor_g_bns_ratio(device->sensor);

	sensor_config = fimc_is_itf_g_param(device, frame, PARAM_SENSOR_CONFIG);
	sensor_config->width = sensor_width;
	sensor_config->height = sensor_height;
	sensor_config->calibrated_width = sensor_width;
	sensor_config->calibrated_height = sensor_height;
	sensor_config->sensor_binning_ratio_x = binning;
	sensor_config->sensor_binning_ratio_y = binning;
	sensor_config->bns_binning_ratio_x = bns_binning;
	sensor_config->bns_binning_ratio_y = bns_binning;
	sensor_config->bns_margin_left = 0;
	sensor_config->bns_margin_top = 0;
	sensor_config->bns_output_width = bns_width;
	sensor_config->bns_output_height = bns_height;
	sensor_config->frametime = 10 * 1000 * 1000; /* max exposure time */
#ifdef FIXED_FPS_DEBUG
	sensor_config->min_target_fps = FIXED_FPS_VALUE;
	sensor_config->max_target_fps = FIXED_FPS_VALUE;
#else
	if (device->sensor->min_target_fps > 0)
		sensor_config->min_target_fps = device->sensor->min_target_fps;
	if (device->sensor->max_target_fps > 0)
		sensor_config->max_target_fps = device->sensor->max_target_fps;
#endif
	*lindex |= LOWBIT_OF(PARAM_SENSOR_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_SENSOR_CONFIG);
	(*indexes)++;

	return ret;
}

/**
 Utility function to adjust output crop size based on the
 H/W limitation of SCP scaling.
 output_crop_w and output_crop_h are call-by reference parameter,
 which contain intended cropping size. Adjusted size will be stored on
 those parameters when this function returns.
 */
static int fimc_is_ischain_scp_adjust_crop(struct fimc_is_device_ischain *device,
	struct scp_param *scp_param,
	u32 *output_crop_w, u32 *output_crop_h)
{
	int changed = 0;

	if (*output_crop_w > scp_param->otf_input.width * 8) {
		mwarn("Cannot be scaled up beyond 8 times(%d -> %d)",
			device, scp_param->otf_input.width, *output_crop_w);
		*output_crop_w = scp_param->otf_input.width * 4;
		changed |= 0x01;
	}

	if (*output_crop_h > scp_param->otf_input.height * 8) {
		mwarn("Cannot be scaled up beyond 8 times(%d -> %d)",
			device, scp_param->otf_input.height, *output_crop_h);
		*output_crop_h = scp_param->otf_input.height * 4;
		changed |= 0x02;
	}

	if (*output_crop_w < (scp_param->otf_input.width + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scp_param->otf_input.width, *output_crop_w);
		*output_crop_w = (scp_param->otf_input.width + 15) / 16;
		changed |= 0x10;
	}

	if (*output_crop_h < (scp_param->otf_input.height + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scp_param->otf_input.height, *output_crop_h);
		*output_crop_h = (scp_param->otf_input.height + 15) / 16;
		changed |= 0x20;
	}

	return changed;
}

static int fimc_is_ischain_s_path(struct fimc_is_device_ischain *device,
	u32 *lindex, u32 *hindex, u32 *indexes)
{
	int ret = 0;
	struct param_control *control;
	struct fimc_is_group *group_3aa, *group_isp, *group_dis;
	struct scc_param *scc_param;
	struct scp_param *scp_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	group_3aa = &device->group_3aa;
	group_isp = &device->group_isp;
	group_dis = &device->group_dis;
	scc_param = &device->is_region->parameter.scalerc;
	scp_param = &device->is_region->parameter.scalerp;

	if (test_bit(FIMC_IS_GROUP_START, &group_3aa->state)) {
		control = fimc_is_itf_g_param(device, NULL, PARAM_3AA_CONTROL);
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_3AA_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_3AA_CONTROL);
		(*indexes)++;
	} else {
		control = fimc_is_itf_g_param(device, NULL, PARAM_3AA_CONTROL);
		control->cmd = CONTROL_COMMAND_STOP;
		control->bypass = CONTROL_BYPASS_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_3AA_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_3AA_CONTROL);
		(*indexes)++;
	}

	control = fimc_is_itf_g_param(device, NULL, PARAM_ISP_CONTROL);
	control->cmd = CONTROL_COMMAND_START;
	control->bypass = CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_ISP_CONTROL);
	(*indexes)++;

	/* dis can be worked only when dma input is enabled by hardware bug */
	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group_dis->state)) {
		control = fimc_is_itf_g_param(device, NULL, PARAM_TPU_CONTROL);
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;
#ifdef ENABLE_DNR
		device->is_region->shared[350] = device->imemory.dvaddr_3dnr;
		control->buffer_number = SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF;
		control->buffer_address = device->imemory.dvaddr_shared + 350 * sizeof(u32);
#else
		control->buffer_number = 0;
		control->buffer_address = 0;
#endif
		*lindex |= LOWBIT_OF(PARAM_TPU_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_TPU_CONTROL);
		(*indexes)++;
	} else {
		control = fimc_is_itf_g_param(device, NULL, PARAM_TPU_CONTROL);
#ifdef ENABLE_FULL_BYPASS
		control->cmd = CONTROL_COMMAND_STOP;
		control->bypass = CONTROL_BYPASS_ENABLE;
#else
		/* HACK : TPU SHOULD BE FULL BYPASS NOT BYPASS AS HARDWARE STALL BUG */
		control->cmd = CONTROL_COMMAND_STOP;
		control->bypass = CONTROL_BYPASS_ENABLE;
#endif
		*lindex |= LOWBIT_OF(PARAM_TPU_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_TPU_CONTROL);
		(*indexes)++;
	}

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
#ifdef SOC_SCC
		scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		(*indexes)++;
#endif

		fimc_is_subdev_odc_stop(device, NULL, lindex, hindex, indexes);
		fimc_is_subdev_drc_stop(device, NULL, lindex, hindex, indexes);
		fimc_is_subdev_dnr_stop(device, NULL, lindex, hindex, indexes);

		scp_param->control.cmd = CONTROL_COMMAND_STOP;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_CONTROL);
		(*indexes)++;
	} else {
#ifdef SOC_SCC
		scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		(*indexes)++;
#endif

		fimc_is_subdev_odc_bypass(device, NULL, lindex, hindex, indexes, true);
		fimc_is_subdev_drc_bypass(device, NULL, lindex, hindex, indexes, true);
		fimc_is_subdev_dnr_bypass(device, NULL, lindex, hindex, indexes, true);

		scp_param->control.cmd = CONTROL_COMMAND_START;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_CONTROL);
		(*indexes)++;
	}

	return ret;
}

#ifdef ENABLE_SETFILE
static int fimc_is_ischain_chg_setfile(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;
	struct fimc_is_path_info *path;
	u32 indexes, lindex, hindex;

	BUG_ON(!device);

	indexes = lindex = hindex = 0;
	path = &device->path;

	if (path->group[GROUP_SLOT_3AA] != GROUP_ID_INVALID)
		group |= (GROUP_ID(path->group[GROUP_SLOT_3AA]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_ISP] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_isp.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_ISP]) & GROUP_ID_PARM_MASK);

	if (((path->group[GROUP_SLOT_DIS] != GROUP_ID_INVALID)) &&
		!test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_dis.state))
		group |= (GROUP_ID(path->group[GROUP_SLOT_DIS]) & GROUP_ID_PARM_MASK);

	ret = fimc_is_itf_process_stop(device, group);
	if (ret) {
		merr("fimc_is_itf_process_stop fail", device);
		goto p_err;
	}

	ret = fimc_is_itf_a_param(device, group);
	if (ret) {
		merr("fimc_is_itf_a_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_process_start(device, group);
	if (ret) {
		merr("fimc_is_itf_process_start fail", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	minfo("[ISC:D] %s(%d):%d\n", device, __func__, device->setfile, ret);
	return ret;
}
#endif

#ifdef ENABLE_DRC
static int fimc_is_ischain_drc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	bool bypass)
{
	int ret = 0;
	u32 lindex, hindex, indexes;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader, *subdev;

	BUG_ON(!device);
	BUG_ON(!device->drc.leader);

	mdbgd_ischain("%s\n", device, __func__);

	subdev = &device->drc;
	leader = subdev->leader;
	group = container_of(leader, struct fimc_is_group, leader);
	lindex = hindex = indexes = 0;

	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		mserr("subdev is not start", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	if (bypass)
		fimc_is_subdev_drc_stop(device, frame, &lindex, &hindex, &indexes);
	else
		fimc_is_subdev_drc_start(device, frame, &lindex, &hindex, &indexes);

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	if (bypass)
		clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	else
		set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	msrinfo("bypass : %d\n", device, subdev, frame, bypass);
	return ret;
}
#endif

#ifdef ENABLE_DIS
static int fimc_is_ischain_dis_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	bool bypass)
{
	int ret = 0;
	u32 lindex, hindex, indexes;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader, *subdev;

	BUG_ON(!device);

	mdbgd_ischain("%s\n", device, __func__);

	subdev = &device->group_dis.leader;
	leader = subdev->leader;
	group = container_of(leader, struct fimc_is_group, leader);
	lindex = hindex = indexes = 0;

	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		mserr("subdev is not start", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_subdev_dis_bypass(device, frame, &lindex, &hindex, &indexes, bypass);

	 ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	if (bypass)
		clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	else
		set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	msrinfo("bypass : %d\n", device, subdev, frame, bypass);
	return ret;
}
#endif


#ifdef ENABLE_DNR
static int fimc_is_ischain_dnr_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	bool bypass)
{
	int ret = 0;
	u32 lindex, hindex, indexes;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader, *subdev;

	BUG_ON(!device);
	BUG_ON(!device->dnr.leader);

	mdbgd_ischain("%s\n", device, __func__);

	subdev = &device->dnr;
	leader = subdev->leader;
	group = container_of(leader, struct fimc_is_group, leader);
	lindex = hindex = indexes = 0;

	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		mserr("subdev is not start", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_subdev_dnr_bypass(device, frame, &lindex, &hindex, &indexes, bypass);

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	if (bypass)
		clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	else
		set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	msrinfo("bypass : %d\n", device, subdev, frame, bypass);
	return ret;
}
#endif

#ifdef ENABLE_VRA
static int fimc_is_ischain_vra_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	bool bypass)
{
	int ret = 0;
	u32 lindex, hindex, indexes;
	struct param_control *control;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader, *subdev;

	BUG_ON(!device);
	BUG_ON(!device->vra.leader);

	mdbgd_ischain("%s(%d)\n", device, __func__, bypass);

	subdev = &device->vra;
	leader = subdev->leader;
	group = container_of(leader, struct fimc_is_group, leader);
	lindex = hindex = indexes = 0;

	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		mserr("subdev is not start", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	control = fimc_is_itf_g_param(device, frame, PARAM_FD_CONTROL);
	if (bypass)
		control->cmd = CONTROL_COMMAND_STOP;
	else
		control->cmd = CONTROL_COMMAND_START;
	control->bypass = CONTROL_BYPASS_DISABLE;
	lindex |= LOWBIT_OF(PARAM_FD_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_FD_CONTROL);
	indexes++;

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		merr("fimc_is_itf_s_param is fail(%d)", device, ret);
		goto p_err;
	}

	if (bypass)
		clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	else
		set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	msrinfo("bypass : %d\n", device, subdev, frame, bypass);
	return ret;
}
#endif

int fimc_is_ischain_g_capability(struct fimc_is_device_ischain *this,
	ulong user_ptr)
{
	int ret = 0;

	ret = copy_to_user((void *)user_ptr, &this->capability,
		sizeof(struct camera2_sm));

	return ret;
}

int fimc_is_ischain_probe(struct fimc_is_device_ischain *device,
	struct fimc_is_interface *interface,
	struct fimc_is_resourcemgr *resourcemgr,
	struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_mem *mem,
	struct platform_device *pdev,
	u32 instance)
{
	int ret = 0;

	BUG_ON(!interface);
	BUG_ON(!mem);
	BUG_ON(!pdev);
	BUG_ON(!device);

	device->interface	= interface;
	device->mem		= mem;
	device->pdev		= pdev;
	device->pdata		= pdev->dev.platform_data;
	device->instance	= instance;
	device->groupmgr	= groupmgr;
	device->resourcemgr	= resourcemgr;
	device->sensor		= NULL;
	device->margin_left	= 0;
	device->margin_right	= 0;
	device->margin_width	= 0;
	device->margin_top	= 0;
	device->margin_bottom	= 0;
	device->margin_height	= 0;
	device->setfile		= 0;
	device->is_region	= NULL;

	atomic_set(&device->group_open_cnt, 0);
	atomic_set(&device->open_cnt, 0);
	atomic_set(&device->init_cnt, 0);

	fimc_is_group_probe(groupmgr, &device->group_3aa, device,
		fimc_is_ischain_3aa_shot,
		fimc_is_ischain_3aa_cfg,
		GROUP_SLOT_3AA, ENTRY_3AA, "3XS");
	fimc_is_group_probe(groupmgr, &device->group_isp, device,
		fimc_is_ischain_isp_shot,
		fimc_is_ischain_isp_cfg,
		GROUP_SLOT_ISP, ENTRY_ISP, "IXS");
	fimc_is_group_probe(groupmgr, &device->group_dis, device,
		fimc_is_ischain_dis_shot,
		fimc_is_ischain_dis_cfg,
		GROUP_SLOT_DIS, ENTRY_DIS, "DXS");

	fimc_is_subdev_probe(&device->txc, instance, ENTRY_3AC, "3XC");
	fimc_is_subdev_probe(&device->txp, instance, ENTRY_3AP, "3XP");
	fimc_is_subdev_probe(&device->ixc, instance, ENTRY_IXC, "IXC");
	fimc_is_subdev_probe(&device->ixp, instance, ENTRY_IXP, "IXP");
	fimc_is_subdev_probe(&device->drc, instance, ENTRY_DRC, "DRC");
	fimc_is_subdev_probe(&device->odc, instance, ENTRY_ODC, "ODC");
	fimc_is_subdev_probe(&device->dnr, instance, ENTRY_DNR, "DNR");
	fimc_is_subdev_probe(&device->scc, instance, ENTRY_SCC, "SCC");
	fimc_is_subdev_probe(&device->scp, instance, ENTRY_SCP, "SCP");
	fimc_is_subdev_probe(&device->vra, instance, ENTRY_VRA, "VRA");

	clear_bit(FIMC_IS_ISCHAIN_OPEN, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_LOADED, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state);

	/* clear group open state */
	clear_bit(FIMC_IS_GROUP_OPEN, &device->group_3aa.state);
	clear_bit(FIMC_IS_GROUP_OPEN, &device->group_isp.state);
	clear_bit(FIMC_IS_GROUP_OPEN, &device->group_dis.state);

	/* clear subdevice state */
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->group_3aa.leader.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->group_isp.leader.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->group_dis.leader.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->txc.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->txp.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->ixc.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->ixp.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->drc.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->odc.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->dnr.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->scc.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->scp.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->vra.state);

	clear_bit(FIMC_IS_SUBDEV_START, &device->group_3aa.leader.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->group_isp.leader.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->group_dis.leader.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->txc.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->txp.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->ixc.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->ixp.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->drc.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->odc.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->dnr.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->scc.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->scp.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->vra.state);

	return ret;
}

static int fimc_is_ischain_open(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	struct fimc_is_minfo *minfo;
	struct fimc_is_ishcain_mem *imemory;

	BUG_ON(!device);
	BUG_ON(!device->groupmgr);
	BUG_ON(!device->resourcemgr);

	mdbgd_ischain("%s", device, __func__);

	minfo = &device->resourcemgr->minfo;

	clear_bit(FIMC_IS_ISCHAIN_INITING, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_INIT, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_START, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state);

	/* 2. Init variables */
	memset(&device->cur_peri_ctl, 0, sizeof(struct camera2_uctl));
	memset(&device->peri_ctls, 0, sizeof(struct camera2_uctl)*SENSOR_MAX_CTL);
	memset(&device->capability, 0, sizeof(struct camera2_sm));

	/* initial state, it's real apply to setting when opening*/
	atomic_set(&device->init_cnt, 0);
	device->margin_left	= 0;
	device->margin_right	= 0;
	device->margin_width	= 0;
	device->margin_top	= 0;
	device->margin_bottom	= 0;
	device->margin_height	= 0;
	device->sensor		= NULL;
	device->module		= 0;

	imemory			= &device->imemory;
	imemory->base		= minfo->base;
	imemory->size		= minfo->size;
	imemory->vaddr_base	= minfo->vaddr_base;
	imemory->vaddr_curr	= minfo->vaddr_curr;
	imemory->fw_cookie	= minfo->fw_cookie;
	imemory->dvaddr		= minfo->dvaddr;
	imemory->kvaddr		= minfo->kvaddr;
	imemory->dvaddr_odc	= minfo->dvaddr_odc;
	imemory->kvaddr_odc	= minfo->kvaddr_odc;
	imemory->dvaddr_dis	= minfo->dvaddr_dis;
	imemory->kvaddr_dis	= minfo->kvaddr_dis;
	imemory->dvaddr_3dnr	= minfo->dvaddr_3dnr;
	imemory->kvaddr_3dnr	= minfo->kvaddr_3dnr;
#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
	imemory->dvaddr_stat	= minfo->dvaddr_stat;
	imemory->kvaddr_stat	= minfo->kvaddr_stat;
#endif
	imemory->offset_region	= (FIMC_IS_A5_MEM_SIZE - ((device->instance + 1) * FIMC_IS_REGION_SIZE));
	imemory->dvaddr_region	= imemory->dvaddr + imemory->offset_region;
	imemory->kvaddr_region	= imemory->kvaddr + imemory->offset_region;
	imemory->is_region	= (struct is_region *)imemory->kvaddr_region;
	imemory->offset_shared	= (ulong)&imemory->is_region->shared[0] - imemory->kvaddr;
	imemory->dvaddr_shared	= imemory->dvaddr + imemory->offset_shared;
	imemory->kvaddr_shared	= imemory->kvaddr + imemory->offset_shared;
	device->is_region	= imemory->is_region;

#ifdef SOC_DRC
	fimc_is_subdev_open(&device->drc, NULL, &init_drc_param.control);
#endif

#ifdef SOC_ODC
	fimc_is_subdev_open(&device->odc, NULL, &init_odc_param.control);
#endif

#ifdef SOC_DNR
	fimc_is_subdev_open(&device->dnr, NULL, &init_tdnr_param.control);
#endif

#ifdef SOC_VRA
	fimc_is_subdev_open(&device->vra, NULL, &init_vra_param.control);
#endif

	/* for mediaserver force close */
	ret = fimc_is_resource_get(device->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret) {
		merr("fimc_is_resource_get is fail", device);
		goto p_err;
	}

#ifdef ENABLE_CLOCK_GATE
	{
		struct fimc_is_core *core;
		if (sysfs_debug.en_clk_gate &&
				sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
			core = (struct fimc_is_core *)device->interface->core;
			fimc_is_clk_gate_lock_set(core, device->instance, true);
			fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
		}
	}
#endif

p_err:
	minfo("[ISC:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int fimc_is_ischain_open_wrap(struct fimc_is_device_ischain *device, bool EOS)
{
	int ret = 0;

	BUG_ON(!device);

	if (test_bit(FIMC_IS_ISCHAIN_CLOSING, &device->state)) {
		merr("open is invalid on closing", device);
		ret = -EPERM;
		goto p_err;
	}

	if (test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto p_err;
	}

	if (atomic_read(&device->open_cnt) > ENTRY_END) {
		merr("open count is invalid(%d)", device, atomic_read(&device->open_cnt));
		ret = -EMFILE;
		goto p_err;
	}

	if (EOS) {
		ret = fimc_is_ischain_open(device);
		if (ret) {
			merr("fimc_is_chain_open is fail(%d)", device, ret);
			goto p_err;
		}

		clear_bit(FIMC_IS_ISCHAIN_OPENING, &device->state);
		set_bit(FIMC_IS_ISCHAIN_OPEN, &device->state);
	} else {
		atomic_inc(&device->open_cnt);
		set_bit(FIMC_IS_ISCHAIN_OPENING, &device->state);
	}

p_err:
	return ret;
}

static int fimc_is_ischain_close(struct fimc_is_device_ischain *device)
{
	int ret = 0;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_spi_gpio *spi_gpio;
	struct fimc_is_core *core = (struct fimc_is_core *)device->resourcemgr->private_data;
#endif
	BUG_ON(!device);

#ifdef CONFIG_COMPANION_USE
	spi_gpio = &core->spi1.gpio;
#endif

#ifdef ENABLE_CLOCK_GATE
	{
		struct fimc_is_core *core = (struct fimc_is_core *)device->resourcemgr->private_data;
		if (sysfs_debug.en_clk_gate && (sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)) {
			fimc_is_clk_gate_lock_set(core, device->instance, true);
			fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
		}
	}
#endif

	/* subdev close */
	fimc_is_subdev_close(&device->drc);
	fimc_is_subdev_close(&device->odc);
	fimc_is_subdev_close(&device->dnr);
	fimc_is_subdev_close(&device->vra);

	ret = fimc_is_itf_close(device);
	if (ret)
		merr("fimc_is_itf_close is fail", device);

	/* for mediaserver force close */
	ret = fimc_is_resource_put(device->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret)
		merr("fimc_is_resource_put is fail", device);

#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_OUTPUT, true);
#endif

#ifdef ENABLE_CLOCK_GATE
	{
		struct fimc_is_core *core = (struct fimc_is_core *)device->resourcemgr->private_data;
		if (sysfs_debug.en_clk_gate && sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
			fimc_is_clk_gate_lock_set(core, device->instance, false);
	}
#endif

	atomic_set(&device->open_cnt, 0);
	clear_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state);

	minfo("[ISC:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int fimc_is_ischain_close_wrap(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	BUG_ON(!device);

	if (test_bit(FIMC_IS_ISCHAIN_OPENING, &device->state)) {
		merr("close is invalid on opening", device);
		ret = -EPERM;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
		merr("already close", device);
		ret = -EMFILE;
		goto p_err;
	}

	if (!atomic_read(&device->open_cnt)) {
		merr("open count is invalid(%d)", device, atomic_read(&device->open_cnt));
		ret = -ENOENT;
		goto p_err;
	}

	atomic_dec(&device->open_cnt);
	set_bit(FIMC_IS_ISCHAIN_CLOSING, &device->state);

	if (!atomic_read(&device->open_cnt)) {
		ret = fimc_is_ischain_close(device);
		if (ret) {
			merr("fimc_is_chain_close is fail(%d)", device, ret);
			goto p_err;
		}

		clear_bit(FIMC_IS_ISCHAIN_CLOSING, &device->state);
		clear_bit(FIMC_IS_ISCHAIN_OPEN, &device->state);
	}

p_err:
	return ret;
}

static int fimc_is_ischain_init(struct fimc_is_device_ischain *device,
	u32 module_id)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_path_info *path;
	u32 flag;

	BUG_ON(!device);
	BUG_ON(!device->sensor);

	mdbgd_ischain("%s(module : %d)\n", device, __func__, module_id);

	sensor = device->sensor;
	path = &device->path;

	if (test_bit(FIMC_IS_ISCHAIN_INIT, &device->state)) {
		merr("chain is already init", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_SENSOR_S_INPUT, &sensor->state)) {
		merr("I2C gpio is not yet set", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_g_module(sensor, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (module->sensor_id != module_id) {
		merr("module id is invalid(%d != %d)", device, module->sensor_id, module_id);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		/* sensor instance means flite channel */
		if(module->position == SENSOR_POSITION_REAR) {
			/* Load calibration data from sensor */
			module->ext.sensor_con.cal_address = FIMC_IS_CAL_START_ADDR;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			ret = fimc_is_ischain_loadcalb_eeprom(device, NULL, SENSOR_POSITION_REAR);
#endif
			if (ret) {
				err("loadcalb fail, load default caldata\n");
			}
		} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
			module->ext.sensor_con.cal_address = FIMC_IS_CAL_START_ADDR_FRONT;
			ret = fimc_is_ischain_loadcalb_eeprom(device, NULL, SENSOR_POSITION_FRONT);
			if (ret) {
				err("loadcalb fail, load default caldata\n");
			}
#else
			module->ext.sensor_con.cal_address = 0;
#endif
		}
	}

#ifdef CONFIG_COMPANION_USE
	{
		struct fimc_is_core *core;

		core = (struct fimc_is_core *)platform_get_drvdata(device->pdev);

		if((module->ext.companion_con.product_name != COMPANION_NAME_NOTHING) &&
			!test_bit(FIMC_IS_COMPANION_OPEN, &core->companion.state)) {
			merr("[ISC:D] companion is not ready\n", device);
			BUG();
		}

		fimc_is_s_int_comb_isp(core, false, INTMR2_INTMCIS22);
	}
#endif

	ret = fimc_is_itf_enum(device);
	if (ret) {
		err("enum fail");
		goto p_err;
	}

	flag = test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) ? 1 : 0;

#if (FW_HAS_SENSOR_MODE_CMD)
	ret = fimc_is_itf_open(device, ((sensor->mode << 16) | (module_id & 0xFFFF)),
			flag, path, &module->ext);
#else
	ret = fimc_is_itf_open(device, module_id, flag, path, &module->ext);
#endif
	if (ret) {
		merr("open fail", device);
		goto p_err;
	}

	ret = fimc_is_itf_setfile(device, module->setfile_name);
	if (ret) {
		merr("setfile fail", device);
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		ret = fimc_is_itf_stream_off(device);
		if (ret) {
			merr("streamoff fail", device);
			goto p_err;
		}
	}

	ret = fimc_is_itf_init_process_stop(device);
	if (ret) {
		merr("fimc_is_itf_init_process_stop is fail", device);
		goto p_err;
	}

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		monitor_period(&device->group_3aa.time, 1);
		monitor_period(&device->group_isp.time, 1);
		monitor_period(&device->group_dis.time, 1);
	} else {
		monitor_period(&device->group_3aa.time, 66);
		monitor_period(&device->group_isp.time, 66);
		monitor_period(&device->group_dis.time, 66);
	}
#endif
#endif

	device->module = module_id;
	set_bit(FIMC_IS_ISCHAIN_INIT, &device->state);

p_err:
	return ret;
}

static int fimc_is_ischain_init_wrap(struct fimc_is_device_ischain *device,
	u32 stream_type,
	u32 module_id)
{
	int ret = 0;
	u32 sindex;
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_module_enum *module;
	struct fimc_is_groupmgr *groupmgr;

	BUG_ON(!device);
	BUG_ON(!device->groupmgr);

	if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
		merr("NOT yet open", device);
		ret = -EMFILE;
		goto p_err;
	}

	if (test_bit(FIMC_IS_ISCHAIN_START, &device->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (atomic_read(&device->init_cnt) >= atomic_read(&device->group_open_cnt)) {
		merr("init count value(%d) is invalid", device, atomic_read(&device->init_cnt));
		ret = -EINVAL;
		goto p_err;
	}

	groupmgr = device->groupmgr;
	core = container_of(groupmgr, struct fimc_is_core, groupmgr);
	atomic_inc(&device->init_cnt);
	set_bit(FIMC_IS_ISCHAIN_INITING, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_INIT, &device->state);
	mdbgd_ischain("%s(%d, %d)\n", device, __func__,
		atomic_read(&device->init_cnt), atomic_read(&device->group_open_cnt));

	if (atomic_read(&device->init_cnt) == atomic_read(&device->group_open_cnt)) {
		if (test_bit(FIMC_IS_GROUP_OPEN, &device->group_3aa.state) &&
			!test_bit(FIMC_IS_GROUP_INIT, &device->group_3aa.state)) {
			merr("invalid 3aa group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(FIMC_IS_GROUP_OPEN, &device->group_isp.state) &&
			!test_bit(FIMC_IS_GROUP_INIT, &device->group_isp.state)) {
			merr("invalid isp group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(FIMC_IS_GROUP_OPEN, &device->group_dis.state) &&
			!test_bit(FIMC_IS_GROUP_INIT, &device->group_dis.state)) {
			merr("invalid dis group state", device);
			ret = -EINVAL;
			goto p_err;
		}

		for (sindex = 0; sindex <= FIMC_IS_STREAM_COUNT; ++sindex) {
			sensor = &core->sensor[sindex];

			if (!test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state))
				continue;

			if (!test_bit(FIMC_IS_SENSOR_S_INPUT, &sensor->state))
				continue;

			ret = fimc_is_sensor_g_module(sensor, &module);
			if (ret) {
				merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
				goto p_err;
			}

			if (module_id == module->sensor_id) {
				device->sensor = sensor;
				break;
			}
		}

		if (sindex > FIMC_IS_VIDEO_SS3_NUM) {
			merr("moduel id(%d) is invalid", device, module_id);
			ret = -EINVAL;
			goto p_err;
		}

		if (!sensor || !sensor->pdata) {
			merr("sensor is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		device->path.sensor_name = module_id;
		device->path.mipi_csi = sensor->pdata->csi_ch;
		device->path.fimc_lite = sensor->pdata->flite_ch;

		if (stream_type) {
			set_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state);
		} else {
			clear_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state);
			sensor->ischain = device;
		}

		ret = fimc_is_groupmgr_init(device->groupmgr, device);
		if (ret) {
			merr("fimc_is_groupmgr_init is fail(%d)", device, ret);
			goto p_err;
		}

		ret = fimc_is_ischain_init(device, module_id);
		if (ret) {
			merr("fimc_is_chain_close is fail(%d)", device, ret);
			goto p_err;
		}

		atomic_set(&device->init_cnt, 0);
		clear_bit(FIMC_IS_ISCHAIN_INITING, &device->state);
		set_bit(FIMC_IS_ISCHAIN_INIT, &device->state);
	}

p_err:
	return ret;
}

static int fimc_is_ischain_start(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 lindex = 0;
	u32 hindex = 0;
	u32 indexes = 0;

	BUG_ON(!device);
	BUG_ON(!device->sensor);

	mdbgd_isp("%s()\n", device, __func__);

	ret = fimc_is_ischain_s_sensor_size(device, NULL, &lindex, &hindex, &indexes);
	if (ret) {
		merr("fimc_is_ischain_s_sensor_size is fail(%d)", device, ret);
		goto p_err;
	}

	fimc_is_ischain_s_path(device, &lindex, &hindex, &indexes);

	if (test_bit(FIMC_IS_ISCHAIN_OPEN_STREAM, &device->state))
		fimc_is_itf_sensor_mode(device);

#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
	if (device->sensor->scene_mode >= AA_SCENE_MODE_DISABLED)
#else
	if (device->sensor->scene_mode >= AA_SCENE_MODE_UNSUPPORTED)
#endif
		device->is_region->parameter.taa.vdma1_input.scene_mode = device->sensor->scene_mode;

	lindex = 0xFFFFFFFF;
	hindex = 0xFFFFFFFF;
	indexes = 64;

	ret = fimc_is_itf_s_param(device , NULL, lindex, hindex, indexes);
	if (ret) {
		merr("fimc_is_itf_s_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_f_param(device);
	if (ret) {
		merr("fimc_is_itf_f_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_sys_ctl(device, IS_SYS_CLOCK_GATE, sysfs_debug.clk_gate_mode);
	if (ret) {
		merr("fimc_is_itf_sys_ctl is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * this code is enabled when camera 2.0 feature is enabled
	 * ret = fimc_is_itf_g_capability(device);
	 * if (ret) {
	 *	err("fimc_is_itf_g_capability is fail\n");
	 *	ret = -EINVAL;
	 *	goto p_err;
	 *}
	 */

	ret = fimc_is_itf_init_process_start(device);
	if (ret) {
		merr("fimc_is_itf_init_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(FIMC_IS_ISCHAIN_START, &device->state);

p_err:
	minfo("[ISC:D] %s(%d):%d\n", device, __func__, device->setfile, ret);
	return ret;
}

static int fimc_is_ischain_start_wrap(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group)
{
	int ret = 0;
	struct fimc_is_group *leader;

	if (!test_bit(FIMC_IS_ISCHAIN_INIT, &device->state)) {
		merr("device is not yet init", device);
		ret = -EINVAL;
		goto p_err;
	}

	leader = device->groupmgr->leader[device->instance];
	if (leader != group)
		goto p_err;

	if (test_bit(FIMC_IS_ISCHAIN_START, &device->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_groupmgr_start(device->groupmgr, device);
	if (ret) {
		merr("fimc_is_groupmgr_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_start(device);
	if (ret) {
		merr("fimc_is_chain_start is fail(%d)", device, ret);
		goto p_err;
	}

	set_bit(FIMC_IS_ISCHAIN_START, &device->state);

p_err:
	return ret;
}

static int fimc_is_ischain_stop(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	return ret;
}

static int fimc_is_ischain_stop_wrap(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group)
{
	int ret = 0;
	struct fimc_is_group *leader;

	leader = device->groupmgr->leader[device->instance];
	if (leader != group)
		goto p_err;

	if (!test_bit(FIMC_IS_ISCHAIN_START, &device->state)) {
		mwarn("already stop", device);
		goto p_err;
	}

	ret = fimc_is_groupmgr_stop(device->groupmgr, device);
	if (ret) {
		merr("fimc_is_groupmgr_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_stop(device);
	if (ret) {
		merr("fimc_is_ischain_stop is fail(%d)", device, ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_ISCHAIN_START, &device->state);

p_err:
	return ret;
}

int fimc_is_ischain_3aa_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	u32 group_id;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_3aa;
	group_id = GROUP_ID_3AA0 + GET_3XS_ID(GET_VIDEO(vctx));

	ret = fimc_is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("fimc_is_group_open is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_open_wrap(device, false);
	if (ret) {
		merr("fimc_is_ischain_open_wrap is fail(%d)", device, ret);
		goto p_err;
	}

	atomic_inc(&device->group_open_cnt);

p_err:
	return ret;
}

int fimc_is_ischain_3aa_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_queue *queue;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(FIMC_IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);
	}

	ret = fimc_is_ischain_3aa_stop(device, queue);
	if (ret)
		merr("fimc_is_ischain_3aa_stop is fail", device);

	ret = fimc_is_group_close(groupmgr, group);
	if (ret)
		merr("fimc_is_group_close is fail", device);

	ret = fimc_is_ischain_close_wrap(device);
	if (ret)
		merr("fimc_is_ischain_close_wrap is fail(%d)", device, ret);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int fimc_is_ischain_3aa_s_input(struct fimc_is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 otf_input,
	u32 stream_leader)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_groupmgr *groupmgr;

	BUG_ON(!device);
	BUG_ON(!device->groupmgr);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_group_init(groupmgr, group, otf_input, video_id, stream_leader);
	if (ret) {
		merr("fimc_is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_init_wrap(device, stream_type, module_id);
	if (ret) {
		merr("fimc_is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_3aa_start(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = fimc_is_group_start(groupmgr, group);
	if (ret) {
		merr("fimc_is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_start_wrap(device, group);
	if (ret) {
		merr("fimc_is_ischain_start_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_3aa_stop(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = fimc_is_group_stop(groupmgr, group);
	if (ret) {
		merr("fimc_is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("fimc_is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

int fimc_is_ischain_3aa_reqbufs(struct fimc_is_device_ischain *device,
	u32 count)
{
	int ret = 0;
	struct fimc_is_group *group;

	BUG_ON(!device);

	group = &device->group_3aa;

	if (!count) {
		ret = fimc_is_itf_unmap(device, GROUP_ID(group->id));
		if (ret)
			merr("fimc_is_itf_unmap is fail(%d)", device, ret);
	}

	return ret;
}

static int fimc_is_ischain_3aa_s_format(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_subdev *leader;

	BUG_ON(!device);
	BUG_ON(!queue);

	leader = &device->group_3aa.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int fimc_is_ischain_3aa_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state));

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = fimc_is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret)
		merr("fimc_is_group_buffer_queue is fail(%d)", device, ret);

	return ret;
}

int fimc_is_ischain_3aa_buffer_finish(struct fimc_is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = fimc_is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("fimc_is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

static int fimc_is_ischain_3aa_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop)
{
	int ret = 0;
	struct taa_param *taa_param;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_crop inparm, otparm;
	struct fimc_is_subdev *subdev, *leader;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!group);
	BUG_ON(!frame);

#ifdef DBG_STREAMING
	mdbgd_ischain("3AA TAG(request %d)\n", device, node->request);
#endif

	groupmgr = device->groupmgr;
	subdev = &group->leader;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	taa_param = &device->is_region->parameter.taa;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = taa_param->otf_input.bayer_crop_offset_x;
		inparm.y = taa_param->otf_input.bayer_crop_offset_y;
		inparm.w = taa_param->otf_input.bayer_crop_width;
		inparm.h = taa_param->otf_input.bayer_crop_height;
	} else {
		inparm.x = taa_param->vdma1_input.bayer_crop_offset_x;
		inparm.y = taa_param->vdma1_input.bayer_crop_offset_y;
		inparm.w = taa_param->vdma1_input.bayer_crop_width;
		inparm.h = taa_param->vdma1_input.bayer_crop_height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (test_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state)) {
		otparm.x = 0;
		otparm.y = 0;
		otparm.w = taa_param->otf_output.width;
		otparm.h = taa_param->otf_output.height;
	} else {
		otparm.x = otcrop->x;
		otparm.y = otcrop->y;
		otparm.w = otcrop->w;
		otparm.h = otcrop->h;
	}

	if (IS_NULL_CROP(otcrop))
		*otcrop = otparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		!COMPARE_CROP(otcrop, &otparm) ||
		test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

		CORRECT_NEGATIVE_CROP(incrop);
		CORRECT_NEGATIVE_CROP(otcrop);

		ret = fimc_is_ischain_3aa_cfg(device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("fimc_is_ischain_3aa_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			incrop->x, incrop->y, incrop->w, incrop->h);
		msrinfo("ot_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	}

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_3aa_ops = {
	.start_streaming	= fimc_is_ischain_3aa_start,
	.stop_streaming		= fimc_is_ischain_3aa_stop,
	.s_format		= fimc_is_ischain_3aa_s_format
};

static int fimc_is_ischain_3ap_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct taa_param *taa_param,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;
	u32 request_format, hw_format;

	request_format = queue->framecfg.format.pixelformat;
	if ((request_format == V4L2_PIX_FMT_SBGGR10) || (request_format == V4L2_PIX_FMT_SBGGR12)) {
		hw_format = DMA_OUTPUT_FORMAT_BAYER_PACKED12;
	} else if (request_format == V4L2_PIX_FMT_SBGGR16) {
		hw_format = DMA_OUTPUT_FORMAT_BAYER;
	} else {
		mwarn("Invalid bayer format", device);
		ret = -EINVAL;
		goto p_err;
	}

	if ((otcrop->w > taa_param->otf_input.bayer_crop_width) ||
		(otcrop->h > taa_param->otf_input.bayer_crop_height)) {
		mrerr("bds output size is invalid((%d, %d) > (%d, %d))", device, frame,
			otcrop->w,
			otcrop->h,
			taa_param->otf_input.bayer_crop_width,
			taa_param->otf_input.bayer_crop_height);
		ret = -EINVAL;
		goto p_err;
	}

	if (otcrop->x || otcrop->y) {
		mwarn("crop pos(%d, %d) is ignored", device, otcrop->x, otcrop->y);
		otcrop->x = 0;
		otcrop->y = 0;
	}

	/*
	 * 3AA BDS ratio limitation on width, height
	 * ratio = input * 256 / output
	 * real output = input * 256 / ratio
	 * real output &= ~1
	 * real output is same with output crop
	 */
	dma_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA2_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = hw_format;
	dma_output->bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->dma_crop_offset_x = otcrop->x;
	dma_output->dma_crop_offset_y = otcrop->y;
	dma_output->dma_crop_width = otcrop->w;
	dma_output->dma_crop_height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	(*indexes)++;

	subdev->output.crop = *otcrop;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_3ap_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *taa_vdma2_output;

	mdbgd_ischain("%s\n", device, __func__);

	taa_vdma2_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA2_OUTPUT);
	taa_vdma2_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_buf_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	u32 pixelformat,
	u32 width,
	u32 height,
	u32 target_addr[])
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!GET_SUBDEV_FRAMEMGR(subdev));

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_24, flags);

	fimc_is_frame_request_head(framemgr, &frame);
	if (frame) {
		if (!frame->stream) {
			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);
			merr("frame->stream is NULL", device);
			BUG();
		}

		switch (pixelformat) {
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_NV12:
			target_addr[0] = frame->dvaddr_buffer[0];
			target_addr[1] = target_addr[0] + (width * height);
			break;
		case V4L2_PIX_FMT_YVU420M:
			target_addr[0] = frame->dvaddr_buffer[0];
			target_addr[1] = frame->dvaddr_buffer[2];
			target_addr[2] = frame->dvaddr_buffer[1];
			break;
		default:
			target_addr[0] = frame->dvaddr_buffer[0];
			target_addr[1] = frame->dvaddr_buffer[1];
			target_addr[2] = frame->dvaddr_buffer[2];
			break;
		}

		frame->stream->findex = ldr_frame->index;
		frame->stream->fcount = ldr_frame->fcount;
		set_bit(subdev->id, &ldr_frame->out_flag);
		set_bit(REQ_FRAME, &frame->req_flag);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
	} else {
		target_addr[0] = 0;
		target_addr[1] = 0;
		target_addr[2] = 0;
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_24, flags);

	return ret;
}

static int fimc_is_ischain_3ap_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct camera2_scaler_uctl *scalerUd;
	struct taa_param *taa_param;
	struct fimc_is_crop *otcrop, otparm;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);
	BUG_ON(!node);

#ifdef DBG_STREAMING
	mdbgd_ischain("3AAP TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	taa_param = &device->is_region->parameter.taa;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	queue = GET_SUBDEV_QUEUE(subdev);

	if (node->request) {
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = taa_param->vdma2_output.width;
		otparm.h = taa_param->vdma2_output.height;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

			CORRECT_NEGATIVE_CROP(otcrop);

			ret = fimc_is_ischain_3ap_start(device,
				subdev,
				ldr_frame,
				queue,
				taa_param,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3ap_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			queue->framecfg.format.pixelformat,
			otcrop->w,
			otcrop->h,
			scalerUd->txpTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_3ap_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3ap_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[3AP:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		scalerUd->txpTargetAddress[0] = 0;
		scalerUd->txpTargetAddress[1] = 0;
		scalerUd->txpTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_3ac_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct taa_param *taa_param,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;
	u32 request_format, hw_format;

	request_format = queue->framecfg.format.pixelformat;
	if ((request_format == V4L2_PIX_FMT_SBGGR10) || (request_format == V4L2_PIX_FMT_SBGGR12)) {
		hw_format = DMA_OUTPUT_FORMAT_BAYER_PACKED12;
	} else if (request_format == V4L2_PIX_FMT_SBGGR16) {
		hw_format = DMA_OUTPUT_FORMAT_BAYER;
	} else {
		mwarn("Invalid bayer format", device);
		ret = -EINVAL;
		goto p_err;
	}

	if ((otcrop->w != taa_param->otf_input.bayer_crop_width) ||
		(otcrop->h != taa_param->otf_input.bayer_crop_height)) {
		merr("bds output size is invalid((%d, %d) != (%d, %d))", device,
			otcrop->w,
			otcrop->h,
			taa_param->otf_input.bayer_crop_width,
			taa_param->otf_input.bayer_crop_height);
		ret = -EINVAL;
		goto p_err;
	}

	if (otcrop->x || otcrop->y) {
		mwarn("crop pos(%d, %d) is ignored", device, otcrop->x, otcrop->y);
		otcrop->x = 0;
		otcrop->y = 0;
	}

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA4_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = hw_format;
	dma_output->bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->selection = 0;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	(*indexes)++;

	subdev->output.crop = *otcrop;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_3ac_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *taa_vdma4_output;

	mdbgd_ischain("%s\n", device, __func__);

	taa_vdma4_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA4_OUTPUT);
	taa_vdma4_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_3ac_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct camera2_scaler_uctl *scalerUd;
	struct taa_param *taa_param;
	struct fimc_is_crop *otcrop, otparm;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("3AAC TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	taa_param = &device->is_region->parameter.taa;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	queue = GET_SUBDEV_QUEUE(subdev);

	if (node->request) {
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = taa_param->otf_input.bayer_crop_width;
		otparm.h = taa_param->otf_input.bayer_crop_height;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

			CORRECT_NEGATIVE_CROP(otcrop);

			ret = fimc_is_ischain_3ac_start(device,
				subdev,
				ldr_frame,
				queue,
				taa_param,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3ac_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			queue->framecfg.format.pixelformat,
			otcrop->w,
			otcrop->h,
			scalerUd->txcTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_3ac_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3ac_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[3AC:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		scalerUd->txcTargetAddress[0] = 0;
		scalerUd->txcTargetAddress[1] = 0;
		scalerUd->txcTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_isp_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	u32 group_id;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));

	groupmgr = device->groupmgr;
	group = &device->group_isp;
	group_id = GROUP_ID_ISP0 + GET_IXS_ID(GET_VIDEO(vctx));

	ret = fimc_is_group_open(groupmgr,
		group,
		group_id,
		vctx);
	if (ret) {
		merr("fimc_is_group_open is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_open_wrap(device, false);
	if (ret) {
		merr("fimc_is_ischain_open_wrap is fail(%d)", device, ret);
		goto p_err;
	}

	atomic_inc(&device->group_open_cnt);

p_err:
	return ret;
}

int fimc_is_ischain_isp_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_queue *queue;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(FIMC_IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);
	}

	ret = fimc_is_ischain_isp_stop(device, queue);
	if (ret)
		merr("fimc_is_ischain_isp_stop is fail", device);

	ret = fimc_is_group_close(groupmgr, group);
	if (ret)
		merr("fimc_is_group_close is fail", device);

	ret = fimc_is_ischain_close_wrap(device);
	if (ret)
		merr("fimc_is_ischain_close_wrap is fail(%d)", device, ret);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int fimc_is_ischain_isp_s_input(struct fimc_is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 otf_input,
	u32 stream_leader)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_groupmgr *groupmgr;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_group_init(groupmgr, group, otf_input, video_id, stream_leader);
	if (ret) {
		merr("fimc_is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_init_wrap(device, stream_type, module_id);
	if (ret) {
		merr("fimc_is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_isp_start(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = fimc_is_group_start(groupmgr, group);
	if (ret) {
		merr("fimc_is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_start_wrap(device, group);
	if (ret) {
		merr("fimc_is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_isp_stop(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	mdbgd_isp("%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = fimc_is_group_stop(groupmgr, group);
	if (ret) {
		merr("fimc_is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("fimc_is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

int fimc_is_ischain_isp_reqbufs(struct fimc_is_device_ischain *device,
	u32 count)
{
	int ret = 0;
	struct fimc_is_group *group;

	BUG_ON(!device);

	group = &device->group_isp;

	if (!count) {
		ret = fimc_is_itf_unmap(device, GROUP_ID(group->id));
		if (ret)
			merr("fimc_is_itf_unmap is fail(%d)", device, ret);
	}

	return ret;
}

static int fimc_is_ischain_isp_s_format(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_subdev *leader;

	BUG_ON(!device);
	BUG_ON(!queue);

	leader = &device->group_isp.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int fimc_is_ischain_isp_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state));

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = fimc_is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret) {
		merr("fimc_is_group_buffer_queue is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_isp_buffer_finish(struct fimc_is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = fimc_is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("fimc_is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

static int fimc_is_ischain_isp_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop)
{
	int ret = 0;
	struct isp_param *isp_param;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_crop inparm;
	struct fimc_is_subdev *subdev, *leader;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!group);
	BUG_ON(!frame);

#ifdef DBG_STREAMING
	mdbgd_ischain("ISP TAG(request %d)\n", device, node->request);
#endif

	groupmgr = device->groupmgr;
	subdev = &group->leader;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	isp_param = &device->is_region->parameter.isp;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = isp_param->otf_input.width;
		inparm.h = isp_param->otf_input.height;
	} else {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = isp_param->vdma1_input.width;
		inparm.h = isp_param->vdma1_input.height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

		CORRECT_NEGATIVE_CROP(incrop);
		CORRECT_NEGATIVE_CROP(otcrop);

		ret = fimc_is_ischain_isp_cfg(device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("fimc_is_ischain_isp_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
	}

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_isp_ops = {
	.start_streaming	= fimc_is_ischain_isp_start,
	.stop_streaming		= fimc_is_ischain_isp_stop,
	.s_format		= fimc_is_ischain_isp_s_format
};

static int fimc_is_ischain_ixc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA5_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = DMA_OUTPUT_FORMAT_YUV422;
	dma_output->order = DMA_OUTPUT_ORDER_YCbYCr;
	dma_output->bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT;
	dma_output->plane = DMA_OUTPUT_PLANE_1;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = otcrop->w;
	dma_output->dma_crop_height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_ixc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *vdma5_output;

	mdbgd_ischain("%s\n", device, __func__);

	vdma5_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA5_OUTPUT);
	vdma5_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA5_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_ixc_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct camera2_scaler_uctl *scalerUd;
	struct isp_param *isp_param;
	struct fimc_is_crop *otcrop;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("ISPC TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	isp_param = &device->is_region->parameter.isp;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	queue = GET_SUBDEV_QUEUE(subdev);

	if (node->request) {
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;
		otcrop->x = 0;
		otcrop->y = 0;
		otcrop->w = isp_param->otf_input.width;
		otcrop->h = isp_param->otf_input.height;

		if (!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

			CORRECT_NEGATIVE_CROP(otcrop);

			ret = fimc_is_ischain_ixc_start(device,
				subdev,
				ldr_frame,
				queue,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_ixc_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			queue->framecfg.format.pixelformat,
			otcrop->w,
			otcrop->h,
			scalerUd->ixcTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_ixc_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_ixc_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mrinfo("[IXC:D] off\n", device, ldr_frame);
		}

		scalerUd->ixcTargetAddress[0] = 0;
		scalerUd->ixcTargetAddress[1] = 0;
		scalerUd->ixcTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_ixp_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA4_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = DMA_OUTPUT_FORMAT_YUV422_CHUNKER;
	dma_output->bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT;
	dma_output->order = DMA_OUTPUT_ORDER_YCbYCr;
	dma_output->plane = DMA_OUTPUT_PLANE_1;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = otcrop->w;
	dma_output->dma_crop_height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_ixp_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *vdma4_output;

	mdbgd_ischain("%s\n", device, __func__);

	vdma4_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA4_OUTPUT);
	vdma4_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA4_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_ixp_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct camera2_scaler_uctl *scalerUd;
	struct isp_param *isp_param;
	struct fimc_is_crop *otcrop;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("ISPP TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	isp_param = &device->is_region->parameter.isp;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	queue = GET_SUBDEV_QUEUE(subdev);

	if (node->request) {
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;
		otcrop->x = 0;
		otcrop->y = 0;
		otcrop->w = isp_param->otf_input.width;
		otcrop->h = isp_param->otf_input.height;

		if (!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

			CORRECT_NEGATIVE_CROP(otcrop);

			ret = fimc_is_ischain_ixp_start(device,
				subdev,
				ldr_frame,
				queue,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_ixp_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			queue->framecfg.format.pixelformat,
			otcrop->w,
			otcrop->h,
			scalerUd->ixpTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_ixp_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_ixp_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mrinfo("[IXP:D] off\n", device, ldr_frame);
		}

		scalerUd->ixpTargetAddress[0] = 0;
		scalerUd->ixpTargetAddress[1] = 0;
		scalerUd->ixpTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_dis_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_open(groupmgr,
		group,
		GROUP_ID_DIS0,
		vctx);
	if (ret) {
		merr("fimc_is_group_open is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_open_wrap(device, false);
	if (ret) {
		merr("fimc_is_ischain_open_wrap is fail(%d)", device, ret);
		goto p_err;
	}

	atomic_inc(&device->group_open_cnt);

p_err:
	return ret;
}

int fimc_is_ischain_dis_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_queue *queue;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_dis;
	queue = GET_QUEUE(vctx);

	/* for mediaserver dead */
	if (test_bit(FIMC_IS_GROUP_START, &group->state)) {
		mgwarn("sudden group close", device, group);
		set_bit(FIMC_IS_GROUP_REQUEST_FSTOP, &group->state);
	}

	ret = fimc_is_ischain_dis_stop(device, queue);
	if (ret)
		merr("fimc_is_ischain_dis_stop is fail", device);

	ret = fimc_is_group_close(groupmgr, group);
	if (ret)
		merr("fimc_is_group_close is fail", device);

	ret = fimc_is_ischain_close_wrap(device);
	if (ret)
		merr("fimc_is_ischain_close_wrap is fail(%d)", device, ret);

	atomic_dec(&device->group_open_cnt);

	return ret;
}

int fimc_is_ischain_dis_s_input(struct fimc_is_device_ischain *device,
	u32 stream_type,
	u32 module_id,
	u32 video_id,
	u32 otf_input,
	u32 stream_leader)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_groupmgr *groupmgr;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_group_init(groupmgr, group, otf_input, video_id, stream_leader);
	if (ret) {
		merr("fimc_is_group_init is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_init_wrap(device, stream_type, module_id);
	if (ret) {
		merr("fimc_is_ischain_init_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_dis_start(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_start(groupmgr, group);
	if (ret) {
		merr("fimc_is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_start_wrap(device, group);
	if (ret) {
		merr("fimc_is_group_start is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_dis_stop(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_stop(groupmgr, group);
	if (ret) {
		merr("fimc_is_group_stop is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_stop_wrap(device, group);
	if (ret) {
		merr("fimc_is_ischain_stop_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	mginfo("%s(%d):%d\n", device, group,  __func__, atomic_read(&group->scount), ret);
	return ret;
}

static int fimc_is_ischain_dis_s_format(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_subdev *leader;

	BUG_ON(!device);
	BUG_ON(!queue);

	leader = &device->group_dis.leader;

	leader->input.width = queue->framecfg.width;
	leader->input.height = queue->framecfg.height;

	leader->input.crop.x = 0;
	leader->input.crop.y = 0;
	leader->input.crop.w = leader->input.width;
	leader->input.crop.h = leader->input.height;

	return ret;
}

int fimc_is_ischain_dis_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state));

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret) {
		merr("fimc_is_group_buffer_queue is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_dis_buffer_finish(struct fimc_is_device_ischain *device,
	u32 index)
{
		int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("fimc_is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

static int fimc_is_ischain_dis_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop)
{
	int ret = 0;
	struct tpu_param *tpu_param;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_crop inparm;
	struct fimc_is_subdev *subdev, *leader;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!group);
	BUG_ON(!frame);

#ifdef DBG_STREAMING
	mdbgd_ischain("ISP TAG(request %d)\n", device, node->request);
#endif

	groupmgr = device->groupmgr;
	subdev = &group->leader;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	tpu_param = &device->is_region->parameter.tpu;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = tpu_param->otf_input.width;
		inparm.h = tpu_param->otf_input.height;
	} else {
		inparm.x = 0;
		inparm.y = 0;
		inparm.w = tpu_param->dma_input.width;
		inparm.h = tpu_param->dma_input.height;
	}

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm)||
		test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

		CORRECT_NEGATIVE_CROP(incrop);
		CORRECT_NEGATIVE_CROP(otcrop);

		ret = fimc_is_ischain_dis_cfg(device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("fimc_is_ischain_dis_cfg is fail(%d)", device, ret);
			goto p_err;
		}

		msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
			incrop->x, incrop->y, incrop->w, incrop->h);
	}

	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_dis_ops = {
	.start_streaming	= fimc_is_ischain_dis_start,
	.stop_streaming		= fimc_is_ischain_dis_stop,
	.s_format		= fimc_is_ischain_dis_s_format
};

static int fimc_is_ischain_scc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct scc_param *scc_param,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *scc_dma_output;
	struct param_otf_output *scc_otf_output;
	struct param_scaler_input_crop *scc_input_crop;
	struct param_scaler_output_crop *scc_output_crop;
	struct param_otf_input *scp_otf_input;
	struct param_scaler_imageeffect *scc_effect;

	if (otcrop->w > scc_param->otf_input.width * 4) {
		mwarn("Cannot be scaled up beyond 4 times(%d -> %d)",
			device, scc_param->otf_input.width, otcrop->w);
		otcrop->w = scc_param->otf_input.width * 4;
	}

	if (otcrop->h > scc_param->otf_input.height * 4) {
		mwarn("Cannot be scaled up beyond 4 times(%d -> %d)",
			device, scc_param->otf_input.height, otcrop->h);
		otcrop->h = scc_param->otf_input.height * 4;
	}

	if (otcrop->w < (scc_param->otf_input.width + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scc_param->otf_input.width, otcrop->w);
		otcrop->w = (scc_param->otf_input.width + 15) / 16;
	}

	if (otcrop->h < (scc_param->otf_input.height + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scc_param->otf_input.height, otcrop->h);
		otcrop->h = (scc_param->otf_input.height + 15) / 16;
	}

	/* setting always although otf output is not used. */
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		scc_otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_OUTPUT);
		scc_otf_output->width = otcrop->w;
		scc_otf_output->height = otcrop->h;
	} else {
		scc_otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_OUTPUT);
		scp_otf_input = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OTF_INPUT);
		scc_otf_output->width = scp_otf_input->width;
		scc_otf_output->height = scp_otf_input->height;
	}

	*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	(*indexes)++;

	scc_input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_INPUT_CROP);
	scc_input_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_input_crop->pos_x = incrop->w;
	scc_input_crop->pos_y = incrop->y;
	scc_input_crop->crop_width = incrop->w;
	scc_input_crop->crop_height = incrop->h;
	scc_input_crop->in_width = scc_param->otf_input.width;
	scc_input_crop->in_height = scc_param->otf_input.height;
	scc_input_crop->out_width = otcrop->w;
	scc_input_crop->out_height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	(*indexes)++;

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		scc_output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OUTPUT_CROP);
		scc_output_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
		scc_output_crop->pos_x = otcrop->x;
		scc_output_crop->pos_y = otcrop->y;
		scc_output_crop->crop_width = otcrop->w;
		scc_output_crop->crop_height = otcrop->h;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		(*indexes)++;

		scc_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_DMA_OUTPUT);
		scc_dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		scc_dma_output->plane = queue->framecfg.format.num_planes - 1;
		scc_dma_output->width = otcrop->w;
		scc_dma_output->height = otcrop->h;
		scc_dma_output->reserved[0] = SCALER_DMA_OUT_SCALED;
	} else {
		scc_output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OUTPUT_CROP);
		scc_output_crop->cmd = SCALER_CROP_COMMAND_DISABLE;
		scc_output_crop->pos_x = otcrop->x;
		scc_output_crop->pos_y = otcrop->y;
		scc_output_crop->crop_width = otcrop->w;
		scc_output_crop->crop_height = otcrop->h;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		(*indexes)++;

		scc_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_DMA_OUTPUT);
		scc_dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		scc_dma_output->plane = queue->framecfg.format.num_planes - 1;
#ifdef SCALER_PARALLEL_MODE
		scc_dma_output->width = incrop->w;
		scc_dma_output->height = incrop->h;
		scc_dma_output->reserved[0] = SCALER_DMA_OUT_SCALED;
#else
		scc_dma_output->width = incrop->w;
		scc_dma_output->height = incrop->h;
		scc_dma_output->reserved[0] = SCALER_DMA_OUT_UNSCALED;
#endif
	}

	switch (queue->framecfg.format.pixelformat) {
	case V4L2_PIX_FMT_YUYV:
		scc_dma_output->format = DMA_OUTPUT_FORMAT_YUV422,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_1;
		scc_dma_output->order = DMA_OUTPUT_ORDER_CrYCbY;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21:
		scc_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_2;
		scc_dma_output->order = DMA_OUTPUT_ORDER_CbCr;
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12:
		scc_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_2;
		scc_dma_output->order = DMA_OUTPUT_ORDER_CrCb;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		scc_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_3;
		scc_dma_output->order = DMA_OUTPUT_ORDER_NO;
		break;
	default:
		mwarn("unknown preview pixelformat", device);
		break;
	}

	*lindex |= LOWBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	(*indexes)++;

	scc_effect = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_IMAGE_EFFECT);
	if (queue->framecfg.colorspace == V4L2_COLORSPACE_JPEG) {
		scc_effect->yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL;
		mrinfo("[SCC] CRange:W\n", device, frame);
	} else {
		scc_effect->yuv_range = SCALER_OUTPUT_YUV_RANGE_NARROW;
		mrinfo("[SCC] CRange:N\n", device, frame);
	}
	*lindex |= LOWBIT_OF(PARAM_SCALERC_IMAGE_EFFECT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_IMAGE_EFFECT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_scc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct scc_param *scc_param,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct param_dma_output *dma_output;
	struct param_otf_output *otf_output;
	struct param_scaler_input_crop *intput_crop;
	struct param_scaler_output_crop *output_crop;

	mdbgd_ischain("%s\n", device, __func__);

	leader = subdev->leader;
	if (!leader) {
		mserr("leader is NULL", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_DMA_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	(*indexes)++;

	intput_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_INPUT_CROP);
	intput_crop->out_width = leader->input.crop.w;
	intput_crop->out_height = leader->input.crop.h;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	(*indexes)++;

	output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OUTPUT_CROP);
	output_crop->cmd = SCALER_CROP_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	(*indexes)++;

	/*
	 * Even if SCC otf output path is not used,
	 * otf output size should be same with input crop output size.
	 * Otherwise, scaler hang can be induced at digital zoom scenario.
	 */
	otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_OUTPUT);
	otf_output->width = leader->input.crop.w;
	otf_output->height = leader->input.crop.h;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_scc_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct camera2_scaler_uctl *scalerUd;
	struct scc_param *scc_param;
	struct fimc_is_crop *incrop, *otcrop, inparm, otparm;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("SCC TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	scc_param = &device->is_region->parameter.scalerc;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	queue = GET_SUBDEV_QUEUE(subdev);

	if (node->request) {
		incrop = (struct fimc_is_crop *)node->input.cropRegion;
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;

		inparm.x = scc_param->input_crop.pos_x;
		inparm.y = scc_param->input_crop.pos_y;
		inparm.w = scc_param->input_crop.crop_width;
		inparm.h = scc_param->input_crop.crop_height;

		otparm.x = scc_param->output_crop.pos_x;
		otparm.y = scc_param->output_crop.pos_y;
		otparm.w = scc_param->output_crop.crop_width;
		otparm.h = scc_param->output_crop.crop_height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(incrop, &inparm) ||
			!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

			CORRECT_NEGATIVE_CROP(incrop);
			CORRECT_NEGATIVE_CROP(otcrop);

			ret = fimc_is_ischain_scc_start(device,
				subdev,
				ldr_frame,
				queue,
				scc_param,
				incrop,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scc_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("in_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			queue->framecfg.format.pixelformat,
			otcrop->w,
			otcrop->h,
			scalerUd->sccTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_scc_stop(device,
				subdev,
				ldr_frame,
				scc_param,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scc_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[SCC:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		scalerUd->sccTargetAddress[0] = 0;
		scalerUd->sccTargetAddress[1] = 0;
		scalerUd->sccTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_scp_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct scp_param *scp_param,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *dma_output;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_scaler_input_crop *input_crop;
	struct param_scaler_output_crop *output_crop;
	struct param_scaler_imageeffect *imageeffect;
	u32 format, order, crange;

	fimc_is_ischain_scp_adjust_crop(device, scp_param, &otcrop->w, &otcrop->h);

	switch (queue->framecfg.format.pixelformat) {
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		format = DMA_OUTPUT_FORMAT_YUV420;
		order = DMA_OUTPUT_ORDER_NO;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21:
		format = DMA_OUTPUT_FORMAT_YUV420;
		order = DMA_OUTPUT_ORDER_CbCr;
		break;
	default:
		format = DMA_OUTPUT_FORMAT_YUV420;
		order = DMA_OUTPUT_ORDER_CbCr;
		mwarn("unknown preview pixelformat", device);
		break;
	}

	if (queue->framecfg.colorspace == V4L2_COLORSPACE_JPEG) {
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
		mdbg_pframe("CRange:W\n", device, subdev, frame);
	} else {
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;
		mdbg_pframe("CRange:N\n", device, subdev, frame);
	}

	input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_INPUT_CROP);
	input_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
	input_crop->pos_x = incrop->x;
	input_crop->pos_y = incrop->y;
	input_crop->crop_width = incrop->w;
	input_crop->crop_height = incrop->h;
	input_crop->in_width = scp_param->otf_input.width;
	input_crop->in_height = scp_param->otf_input.height;
	input_crop->out_width = otcrop->w;
	input_crop->out_height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_INPUT_CROP);
	(*indexes)++;

	/*
	 * scaler can't apply stride to each plane, only y plane.
	 * basically cb, cr plane should be half of y plane,
	 * and it's automatically set
	 *
	 * 3 plane : all plane should be 8 or 16 stride
	 * 2 plane : y plane should be 32, 16 stride, others should be half stride of y
	 * 1 plane : all plane should be 8 stride
	 */
	/*
	 * limitation of output_crop.pos_x and pos_y
	 * YUV422 3P, YUV420 3P : pos_x and pos_y should be x2
	 * YUV422 1P : pos_x should be x2
	 */
	output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OUTPUT_CROP);
	output_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
	output_crop->pos_x = otcrop->x;
	output_crop->pos_y = otcrop->y;
	output_crop->crop_width = otcrop->w;
	output_crop->crop_height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
	(*indexes)++;

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_DMA_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = format;
	dma_output->plane = queue->framecfg.format.num_planes - 1;
	dma_output->order = order;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	(*indexes)++;

	imageeffect = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_IMAGE_EFFECT);
	imageeffect->yuv_range = crange;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_IMAGE_EFFECT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_IMAGE_EFFECT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OTF_OUTPUT);
	otf_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	otf_output->width = otcrop->w;
	otf_output->height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
	(*indexes)++;

	otf_input = fimc_is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
	otf_input->width = otcrop->w;
	otf_input->height = otcrop->h;
	*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_scp_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct scp_param *scp_param,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *scp_dma_output;

	mdbgd_ischain("%s\n", device, __func__);

	scp_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_DMA_OUTPUT);
	scp_dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int fimc_is_ischain_scp_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct camera2_scaler_uctl *scalerUd;
	struct scp_param *scp_param;
	struct fimc_is_crop *incrop, *otcrop, inparm, otparm;
	u32 lindex, hindex, indexes;

	BUG_ON(!device);
	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_QUEUE(subdev));
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("SCP TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	leader = subdev->leader;
	scp_param = &device->is_region->parameter.scalerp;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	queue = GET_SUBDEV_QUEUE(subdev);

	if (node->request) {
		incrop = (struct fimc_is_crop *)node->input.cropRegion;
		otcrop = (struct fimc_is_crop *)node->output.cropRegion;

		inparm.x = scp_param->input_crop.pos_x;
		inparm.y = scp_param->input_crop.pos_y;
		inparm.w = scp_param->input_crop.crop_width;
		inparm.h = scp_param->input_crop.crop_height;

		otparm.x = scp_param->output_crop.pos_x;
		otparm.y = scp_param->output_crop.pos_y;
		otparm.w = scp_param->output_crop.crop_width;
		otparm.h = scp_param->output_crop.crop_height;

		if (IS_NULL_CROP(incrop))
			*incrop = inparm;

		if (IS_NULL_CROP(otcrop))
			*otcrop = otparm;

		if (!COMPARE_CROP(incrop, &inparm) ||
			!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(FIMC_IS_SUBDEV_FORCE_SET, &leader->state)) {

			CORRECT_NEGATIVE_CROP(incrop);
			CORRECT_NEGATIVE_CROP(otcrop);

			ret = fimc_is_ischain_scp_start(device,
				subdev,
				ldr_frame,
				queue,
				scp_param,
				incrop,
				otcrop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scp_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("in_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				incrop->x, incrop->y, incrop->w, incrop->h);
			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = fimc_is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			queue->framecfg.format.pixelformat,
			otcrop->w,
			otcrop->h,
			scalerUd->scpTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state)) {
			ret = fimc_is_ischain_scp_stop(device,
				subdev,
				ldr_frame,
				scp_param,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scp_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[SCP:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		scalerUd->scpTargetAddress[0] = 0;
		scalerUd->scpTargetAddress[1] = 0;
		scalerUd->scpTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_3aa_cfg(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_input *dma_input;
	u32 hw_format;

	BUG_ON(!device);
	BUG_ON(!device->sensor);
	BUG_ON(!incrop);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	hw_format = DMA_INPUT_FORMAT_BAYER;
	group = &device->group_3aa;
	leader = &group->leader;
	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		switch (queue->framecfg.format.pixelformat) {
		case V4L2_PIX_FMT_SBGGR10:
		case V4L2_PIX_FMT_SBGGR12:
			hw_format = DMA_INPUT_FORMAT_BAYER_PACKED12;
			break;
		case V4L2_PIX_FMT_SBGGR16:
			hw_format = DMA_INPUT_FORMAT_BAYER;
			break;
		default:
			merr("Invalid bayer format(%d)", device, queue->framecfg.format.pixelformat);
			ret = -EINVAL;
			goto p_err;
		}
	}

	/*
	 * bayer crop = bcrop1 + bcrop3
	 * hal should set full size input including cac margin
	 * and then full size is decreased as cac margin by driver internally
	 * size of 3AP take over only setting for BDS
	 */

	otf_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_OTF_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
		otf_input->width = fimc_is_sensor_g_bns_width(device->sensor);
		otf_input->height = fimc_is_sensor_g_bns_height(device->sensor);
	} else {
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
		otf_input->width = leader->input.width;
		otf_input->height = leader->input.height;
	}
	otf_input->format = OTF_INPUT_FORMAT_BAYER;
	otf_input->bitwidth = OTF_INPUT_BIT_WIDTH_12BIT;
	otf_input->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	otf_input->bayer_crop_offset_x = incrop->x;
	otf_input->bayer_crop_offset_y = incrop->y;
	otf_input->bayer_crop_width = incrop->w;
	otf_input->bayer_crop_height = incrop->h;
	*lindex |= LOWBIT_OF(PARAM_3AA_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_OTF_INPUT);
	(*indexes)++;

	dma_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA1_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = DMA_INPUT_BIT_WIDTH_12BIT;
	dma_input->order = DMA_INPUT_ORDER_GR_BG;
	dma_input->plane = 1;
	dma_input->width = leader->input.width;
	dma_input->height = leader->input.height;
	dma_input->dma_crop_offset = 0;
	dma_input->dma_crop_width = leader->input.width;
	dma_input->dma_crop_height = leader->input.height;
	dma_input->bayer_crop_offset_x = incrop->x;
	dma_input->bayer_crop_offset_y = incrop->y;
	dma_input->bayer_crop_width = incrop->w;
	dma_input->bayer_crop_height = incrop->h;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA1_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA1_INPUT);
	(*indexes)++;

	/* if 3aap is not start then otf output should be enabled */
	otf_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_OTF_OUTPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
	otf_output->width = otcrop->w;
	otf_output->height = otcrop->h;
	otf_output->format = OTF_OUTPUT_FORMAT_BAYER;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_OUTPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_3AA_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_OTF_OUTPUT);
	(*indexes)++;

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int fimc_is_ischain_isp_cfg(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_input *dma_input;
	u32 hw_format;
	u32 width, height;

	BUG_ON(!device);
	BUG_ON(!incrop);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	hw_format = DMA_INPUT_FORMAT_BAYER;
	width = incrop->w;
	height = incrop->h;
	group = &device->group_isp;
	leader = &group->leader;
	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		switch (queue->framecfg.format.pixelformat) {
		case V4L2_PIX_FMT_SBGGR10:
		case V4L2_PIX_FMT_SBGGR12:
			hw_format = DMA_INPUT_FORMAT_BAYER_PACKED12;
			break;
		case V4L2_PIX_FMT_SBGGR16:
			hw_format = DMA_INPUT_FORMAT_BAYER;
			break;
		default:
			merr("Invalid bayer format(%d)", device, queue->framecfg.format.pixelformat);
			ret = -EINVAL;
			goto p_err;
		}
	}

	/* ISP */
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_ISP_OTF_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	otf_input->width = width;
	otf_input->height = height;
	otf_input->format = OTF_INPUT_FORMAT_BAYER;
	otf_input->bayer_crop_offset_x = 0;
	otf_input->bayer_crop_offset_y = 0;
	otf_input->bayer_crop_width = width;
	otf_input->bayer_crop_height = height;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_INPUT);
	(*indexes)++;

	dma_input = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA1_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = DMA_INPUT_BIT_WIDTH_12BIT;
	dma_input->width = width;
	dma_input->height = height;
	dma_input->dma_crop_offset = 0;
	dma_input->dma_crop_width = width;
	dma_input->dma_crop_height = height;
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
	dma_input->bayer_crop_width = width;
	dma_input->bayer_crop_height = height;
	*lindex |= LOWBIT_OF(PARAM_ISP_VDMA1_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA1_INPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_OTF_OUTPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
	otf_output->width = width;
	otf_output->height = height;
	otf_output->format = OTF_OUTPUT_FORMAT_YUV444;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_OUTPUT);
	(*indexes)++;

#ifdef SOC_DRC
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_DRC_OTF_INPUT);
	otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	otf_input->width = width;
	otf_input->height = height;
	*lindex |= LOWBIT_OF(PARAM_DRC_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_DRC_OTF_INPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_DRC_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_output->width = width;
	otf_output->height = height;
	*lindex |= LOWBIT_OF(PARAM_DRC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_DRC_OTF_OUTPUT);
	(*indexes)++;
#endif

#ifdef SOC_SCC
	{
		struct param_scaler_input_crop *input_crop;
		struct param_scaler_output_crop *output_crop;
		struct fimc_is_subdev *scc;
		u32 scc_width, scc_height;

		scc = &device->scc;
		scc_width = scc->output.crop.w;
		scc_height = scc->output.crop.h;

		otf_input = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_INPUT);
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
		otf_input->width = width;
		otf_input->height = height;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_INPUT);
		(*indexes)++;

		/* SCC CROP */
		input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_INPUT_CROP);
		input_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
		input_crop->pos_x = 0;
		input_crop->pos_y = 0;
		input_crop->crop_width = width;
		input_crop->crop_height = height;
		input_crop->in_width = width;
		input_crop->in_height = height;
		input_crop->out_width = scc_width;
		input_crop->out_height = scc_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
		(*indexes)++;

		output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OUTPUT_CROP);
		output_crop->cmd = SCALER_CROP_COMMAND_DISABLE;
		output_crop->pos_x = 0;
		output_crop->pos_y = 0;
		output_crop->crop_width = scc_width;
		output_crop->crop_height = scc_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		(*indexes)++;

		otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_OUTPUT);
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
		otf_output->width = scc_width;
		otf_output->height = scc_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		(*indexes)++;
	}
#endif

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int fimc_is_ischain_dis_cfg(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_input *dma_input;
	struct param_dma_output *dma_output;
	u32 width, height, dis_width, dis_height, scp_width, scp_height;

	BUG_ON(!device);
	BUG_ON(!incrop);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	width = incrop->w;
	height = incrop->h;
	dis_width = otcrop->w;
	dis_height = otcrop->h;
	scp_width = dis_width;
	scp_height = dis_height;
	group = &device->group_dis;
	leader = &group->leader;
	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

#ifdef SOC_DIS
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_TPU_OTF_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		/* HACK : OTF ENABLE fail even though full bypass */
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;
	otf_input->width = width;
	otf_input->height = height;
	otf_input->format = OTF_INPUT_FORMAT_YUV422;
	*lindex |= LOWBIT_OF(PARAM_TPU_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_TPU_OTF_INPUT);
	(*indexes)++;

	dma_input = fimc_is_itf_g_param(device, frame, PARAM_TPU_DMA_INPUT);
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state))
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->width = width;
	dma_input->height = height;
	dma_input->format = DMA_INPUT_FORMAT_YUV422_CHUNKER;
	*lindex |= LOWBIT_OF(PARAM_TPU_DMA_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_TPU_DMA_INPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_TPU_OTF_OUTPUT);
	otf_output->width = dis_width;
	otf_output->height = dis_height;
	otf_output->format = OTF_OUTPUT_FORMAT_YUV444;
	*lindex |= LOWBIT_OF(PARAM_TPU_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_TPU_OTF_OUTPUT);
	(*indexes)++;
#endif

#if 0
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_TDNR_OTF_INPUT);
	otf_input->width = width;
	otf_input->height = height;
	*lindex |= LOWBIT_OF(PARAM_TDNR_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_OTF_INPUT);
	(*indexes)++;

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_TDNR_DMA_OUTPUT);
	dma_output->width = width;
	dma_output->height = height;
	*lindex |= LOWBIT_OF(PARAM_TDNR_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_DMA_OUTPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_TDNR_OTF_OUTPUT);
	otf_output->width = width;
	otf_output->height = height;
	*lindex |= LOWBIT_OF(PARAM_TDNR_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_OTF_OUTPUT);
	(*indexes)++;
#endif

#ifdef SOC_SCP
	{
		struct param_scaler_input_crop *input_crop;
		struct param_scaler_output_crop *output_crop;
		struct fimc_is_subdev *scp;

		scp = &device->scp;
		scp_width = scp->output.crop.w;
		scp_height = scp->output.crop.h;

		otf_input = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OTF_INPUT);
		otf_input->width = dis_width;
		otf_input->height = dis_height;
		otf_input->format = OTF_INPUT_FORMAT_YUV444;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_OTF_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_OTF_INPUT);
		(*indexes)++;

		input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_INPUT_CROP);
		input_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
		input_crop->pos_x = 0;
		input_crop->pos_y = 0;
		input_crop->crop_width = dis_width;
		input_crop->crop_height = dis_height;
		input_crop->in_width = dis_width;
		input_crop->in_height = dis_height;
		input_crop->out_width = scp_width;
		input_crop->out_height = scp_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_INPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_INPUT_CROP);
		(*indexes)++;

		output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OUTPUT_CROP);
		output_crop->cmd = SCALER_CROP_COMMAND_DISABLE;
		output_crop->pos_x = 0;
		output_crop->pos_y = 0;
		output_crop->crop_width = scp_width;
		output_crop->crop_height = scp_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
		(*indexes)++;

		dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_DMA_OUTPUT);
		dma_output->width = scp_width;
		dma_output->height = scp_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
		(*indexes)++;

		otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OTF_OUTPUT);
		otf_output->width = scp_width;
		otf_output->height = scp_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
		(*indexes)++;
	}
#endif

	otf_input = fimc_is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
	otf_input->width = scp_width;
	otf_input->height = scp_height;
	*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
	(*indexes)++;

	leader->input.crop = *incrop;

p_err:
	return ret;
}

static int fimc_is_ischain_3aa_group_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop)
{
	int ret = 0;
	u32 capture_id;
	struct fimc_is_group *group;
	struct fimc_is_subdev *subdev;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_3aa;
	node_group = &frame->shot_ext->node_group;

	ret = fimc_is_ischain_3aa_tag(device, group, frame, incrop, otcrop);
	if (ret) {
		merr("fimc_is_ischain_3aa_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case FIMC_IS_VIDEO_30C_NUM:
		case FIMC_IS_VIDEO_31C_NUM:
			subdev = group->subdev[ENTRY_3AC];
			if (subdev && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				ret = fimc_is_ischain_3ac_tag(device, subdev, frame, cap_node);
				if (ret) {
					merr("fimc_is_ischain_3ac_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case FIMC_IS_VIDEO_30P_NUM:
		case FIMC_IS_VIDEO_31P_NUM:
			subdev = group->subdev[ENTRY_3AP];
			if (subdev && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				ret = fimc_is_ischain_3ap_tag(device, subdev, frame, cap_node);
				if (ret) {
					merr("fimc_is_ischain_3ap_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static int fimc_is_ischain_isp_group_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop)
{
	int ret = 0;
	u32 capture_id;
	struct fimc_is_group *group;
	struct fimc_is_subdev *subdev, *drc;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_isp;
	drc = group->subdev[ENTRY_DRC];
	node_group = &frame->shot_ext->node_group;

#ifdef ENABLE_DRC
	if (drc) {
		if (frame->shot_ext->drc_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_RUN, &drc->state)) {
				ret = fimc_is_ischain_drc_bypass(device, frame, true);
				if (ret) {
					err("fimc_is_ischain_drc_bypass(1) is fail");
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_RUN, &drc->state)) {
				ret = fimc_is_ischain_drc_bypass(device, frame, false);
				if (ret) {
					err("fimc_is_ischain_drc_bypass(0) is fail");
					goto p_err;
				}
			}
		}
	}
#endif

	ret = fimc_is_ischain_isp_tag(device, group, frame, incrop, otcrop);
	if (ret) {
		merr("fimc_is_ischain_isp_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case FIMC_IS_VIDEO_I0C_NUM:
		case FIMC_IS_VIDEO_I1C_NUM:
			subdev = group->subdev[ENTRY_IXC];
			if (subdev && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				ret = fimc_is_ischain_ixc_tag(device, subdev, frame, cap_node);
				if (ret) {
					merr("fimc_is_ischain_ixc_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case FIMC_IS_VIDEO_I0P_NUM:
		case FIMC_IS_VIDEO_I1P_NUM:
			subdev = group->subdev[ENTRY_IXP];
			if (subdev && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				ret = fimc_is_ischain_ixp_tag(device, subdev, frame, cap_node);
				if (ret) {
					merr("fimc_is_ischain_ixp_tag is fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static int fimc_is_ischain_dis_group_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	struct fimc_is_crop *incrop,
	struct fimc_is_crop *otcrop)
{
	int ret = 0;
	u32 capture_id;
	struct fimc_is_group *group;
	struct fimc_is_subdev *subdev, *dis, *dnr, *vra;
	struct camera2_node_group *node_group;
	struct camera2_node *cap_node;

	group = &device->group_dis;
	dis = &group->leader;
	dnr = group->subdev[ENTRY_DNR];
	vra = group->subdev[ENTRY_VRA];
	node_group = &frame->shot_ext->node_group;

#ifdef ENABLE_DIS
	if (dis) {
		if (frame->shot_ext->dis_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_RUN, &dis->state)) {
				ret = fimc_is_ischain_dis_bypass(device, frame, true);
				if (ret) {
					merr("dis_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_RUN, &dis->state)) {
				ret = fimc_is_ischain_dis_bypass(device, frame, false);
				if (ret) {
					merr("dis_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

#ifdef ENABLE_DNR
	if (dnr) {
		if (frame->shot_ext->dnr_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_RUN, &dnr->state)) {
				ret = fimc_is_ischain_dnr_bypass(device, frame, true);
				if (ret) {
					merr("dnr_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_RUN, &dnr->state)) {
				ret = fimc_is_ischain_dnr_bypass(device, frame, false);
				if (ret) {
					merr("dnr_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

#ifdef ENABLE_VRA
	if (vra) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_RUN, &vra->state)) {
				ret = fimc_is_ischain_vra_bypass(device, frame, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_RUN, &vra->state)) {
				ret = fimc_is_ischain_vra_bypass(device, frame, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	ret = fimc_is_ischain_dis_tag(device, group, frame, incrop, otcrop);
	if (ret) {
		merr("fimc_is_ischain_dis_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		cap_node = &node_group->capture[capture_id];
		subdev = NULL;

		switch (cap_node->vid) {
		case 0:
			break;
		case FIMC_IS_VIDEO_SCC_NUM:
			subdev = group->subdev[ENTRY_SCC];
			if (subdev && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				ret = fimc_is_ischain_scc_tag(device, subdev, frame, cap_node);
				if (ret) {
					merr("fimc_is_ischain_scc_tag fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		case FIMC_IS_VIDEO_SCP_NUM:
			subdev = group->subdev[ENTRY_SCP];
			if (subdev && test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
				ret = fimc_is_ischain_scp_tag(device, subdev, frame, cap_node);
				if (ret) {
					merr("fimc_is_ischain_scp_tag fail(%d)", device, ret);
					goto p_err;
				}
			}
			break;
		default:
			break;
		}
	}

p_err:
	return ret;
}

static int fimc_is_ischain_3aa_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_group *group, *parent, *child;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_crop incrop, otcrop;
	struct camera2_node_group *node_group;

#ifdef ENABLE_FAST_SHOT
	uint32_t af_trigger_bk;
#endif

#ifdef DBG_STREAMING
	mdbgd_ischain("%s()\n", device, __func__);
#endif

	BUG_ON(!device);
	BUG_ON(!check_frame);

	frame = NULL;
	INIT_CROP(&incrop);
	INIT_CROP(&otcrop);
	group = &device->group_3aa;

	framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_frame_request_head(framemgr, &frame);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_MAP_MEM, &frame->memory))) {
		fimc_is_itf_map(device, GROUP_ID(group->id), frame->dvaddr_shot, frame->shot_size);
		set_bit(FRAME_MAP_MEM, &frame->memory);
	}

#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
#else
	frame->shot->ctl.entry.lowIndexParam = 0;
	frame->shot->ctl.entry.highIndexParam = 0;
	frame->shot->dm.entry.lowIndexParam = 0;
	frame->shot->dm.entry.highIndexParam = 0;
#endif
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

#ifdef ENABLE_SETFILE
	if ((frame->shot_ext->setfile != device->setfile) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		u32 setfile_save = device->setfile;
		device->setfile = frame->shot_ext->setfile;

		ret = fimc_is_ischain_chg_setfile(device);
		if (ret) {
			merr("fimc_is_ischain_chg_setfile is fail", device);
			device->setfile = setfile_save;
			goto p_err;
		}
	}
#endif

#ifdef ENABLE_FAST_SHOT
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		af_trigger_bk = frame->shot->ctl.aa.afTrigger;
		memcpy(&frame->shot->ctl.aa, &group->fast_ctl.aa,
			sizeof(struct camera2_aa_ctl));
		memcpy(&frame->shot->ctl.scaler, &group->fast_ctl.scaler,
			sizeof(struct camera2_scaler_ctl));
		frame->shot->ctl.aa.afTrigger = af_trigger_bk;
	}
#endif

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		enum aa_capture_intent captureIntent;
		captureIntent = group->intent_ctl.aa.captureIntent;

		if (captureIntent != AA_CAPTURE_INTENT_CUSTOM) {
			frame->shot->ctl.aa.captureIntent = captureIntent;
			group->intent_ctl.aa.captureIntent = AA_CAPTURE_INTENT_CUSTOM;
		}
	}

	/* fd information copy */
	memcpy(&frame->shot->uctl.fdUd, &device->cur_peri_ctl.fdUd, sizeof(struct camera2_fd_uctl));

	PROGRAM_COUNT(9);

	parent = NULL;
	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_3AA:
			incrop.x = node_group->leader.input.cropRegion[0];
			incrop.y = node_group->leader.input.cropRegion[1];
			incrop.w = node_group->leader.input.cropRegion[2];
			incrop.h = node_group->leader.input.cropRegion[3];
			otcrop.x = node_group->capture[child->junction->cid].output.cropRegion[0];
			otcrop.y = node_group->capture[child->junction->cid].output.cropRegion[1];
			otcrop.w = node_group->capture[child->junction->cid].output.cropRegion[2];
			otcrop.h = node_group->capture[child->junction->cid].output.cropRegion[3];
			ret = fimc_is_ischain_3aa_group_tag(device, frame, &incrop, &otcrop);
			if (ret) {
				merr("fimc_is_ischain_3aa_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_ISP:
			incrop.x = otcrop.x;
			incrop.y = otcrop.y;
			incrop.w = otcrop.w;
			incrop.h = otcrop.h;
			otcrop.x = incrop.x;
			otcrop.y = incrop.y;
			otcrop.w = incrop.w;
			otcrop.h = incrop.h;
			ret = fimc_is_ischain_isp_group_tag(device, frame, &incrop, &otcrop);
			if (ret) {
				merr("fimc_is_ischain_isp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_DIS:
			incrop.x = otcrop.x;
			incrop.y = otcrop.y;
			incrop.w = otcrop.w;
			incrop.h = otcrop.h;
			otcrop.x = incrop.x;
			otcrop.y = incrop.y;
			otcrop.w = incrop.w;
			otcrop.h = incrop.h;
			ret = fimc_is_ischain_dis_group_tag(device, frame, &incrop, &otcrop);
			if (ret) {
				merr("fimc_is_ischain_dis_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		parent = child;
		child = child->child;
	}

	clear_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	PROGRAM_COUNT(10);

p_err:
	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_25, flags);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_25, flags);
		set_bit(REQ_3AA_SHOT, &frame->req_flag);
	}

	return ret;
}

static int fimc_is_ischain_isp_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_group *group, *parent, *child;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_crop incrop, otcrop;
	struct camera2_node_group *node_group;

	BUG_ON(!device);
	BUG_ON(!check_frame);
	BUG_ON(device->instance_sensor >= FIMC_IS_STREAM_COUNT);

#ifdef DBG_STREAMING
	mdbgd_isp("%s\n", device, __func__);
#endif

	frame = NULL;
	INIT_CROP(&incrop);
	INIT_CROP(&otcrop);
	group = &device->group_isp;

	framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_frame_request_head(framemgr, &frame);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_MAP_MEM, &frame->memory))) {
		fimc_is_itf_map(device, GROUP_ID(group->id), frame->dvaddr_shot, frame->shot_size);
		set_bit(FRAME_MAP_MEM, &frame->memory);
	}

#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
#else
	frame->shot->ctl.entry.lowIndexParam = 0;
	frame->shot->ctl.entry.highIndexParam = 0;
	frame->shot->dm.entry.lowIndexParam = 0;
	frame->shot->dm.entry.highIndexParam = 0;
#endif
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

#ifdef ENABLE_SETFILE
	if ((frame->shot_ext->setfile != device->setfile) &&
		test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		u32 setfile_save = device->setfile;
		device->setfile = frame->shot_ext->setfile;

		ret = fimc_is_ischain_chg_setfile(device);
		if (ret) {
			merr("fimc_is_ischain_chg_setfile is fail", device);
			device->setfile = setfile_save;
			goto p_err;
		}
	}
#endif

	PROGRAM_COUNT(9);

	parent = NULL;
	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_ISP:
			incrop.x = node_group->leader.input.cropRegion[0];
			incrop.y = node_group->leader.input.cropRegion[1];
			incrop.w = node_group->leader.input.cropRegion[2];
			incrop.h = node_group->leader.input.cropRegion[3];
			otcrop.x = incrop.x;
			otcrop.y = incrop.y;
			otcrop.w = incrop.w;
			otcrop.h = incrop.h;
			ret = fimc_is_ischain_isp_group_tag(device, frame, &incrop, &otcrop);
			if (ret) {
				merr("fimc_is_ischain_isp_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		case GROUP_SLOT_DIS:
			incrop.x = otcrop.x;
			incrop.y = otcrop.y;
			incrop.w = otcrop.w;
			incrop.h = otcrop.h;
			otcrop.x = incrop.x;
			otcrop.y = incrop.y;
			otcrop.w = incrop.w;
			otcrop.h = incrop.h;
			ret = fimc_is_ischain_dis_group_tag(device, frame, &incrop, &otcrop);
			if (ret) {
				merr("fimc_is_ischain_dis_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		parent = child;
		child = child->child;
	}

#ifdef PRINT_PARAM
	if (frame->fcount == 1) {
		fimc_is_hw_memdump(device->interface,
			(ulong) &device->is_region->parameter,
			(ulong) &device->is_region->parameter + sizeof(device->is_region->parameter));
	}
#endif

	clear_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	PROGRAM_COUNT(10);

p_err:
	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_26, flags);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_26, flags);
		set_bit(REQ_ISP_SHOT, &frame->req_flag);
	}

	return ret;
}

static int fimc_is_ischain_dis_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_group *group, *parent, *child;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_crop incrop, otcrop;
	struct camera2_node_group *node_group;

#ifdef DBG_STREAMING
	mdbgd_ischain("%s()\n", device, __func__);
#endif

	BUG_ON(!device);
	BUG_ON(!check_frame);

	frame = NULL;
	INIT_CROP(&incrop);
	INIT_CROP(&otcrop);
	group = &device->group_dis;

	framemgr = GET_SUBDEV_FRAMEMGR(&group->leader);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_frame_request_head(framemgr, &frame);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%p != %p)", device, frame, check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_MAP_MEM, &frame->memory))) {
		fimc_is_itf_map(device, GROUP_ID(group->id), frame->dvaddr_shot, frame->shot_size);
		set_bit(FRAME_MAP_MEM, &frame->memory);
	}

#ifdef CONFIG_ENABLE_HAL3_2_META_INTERFACE
	frame->shot->ctl.vendor_entry.lowIndexParam = 0;
	frame->shot->ctl.vendor_entry.highIndexParam = 0;
	frame->shot->dm.vendor_entry.lowIndexParam = 0;
	frame->shot->dm.vendor_entry.highIndexParam = 0;
#else
	frame->shot->ctl.entry.lowIndexParam = 0;
	frame->shot->ctl.entry.highIndexParam = 0;
	frame->shot->dm.entry.lowIndexParam = 0;
	frame->shot->dm.entry.highIndexParam = 0;
#endif
	node_group = &frame->shot_ext->node_group;

	PROGRAM_COUNT(8);

	PROGRAM_COUNT(9);

	parent = NULL;
	child = group;
	while (child) {
		switch (child->slot) {
		case GROUP_SLOT_DIS:
			incrop.x = node_group->leader.input.cropRegion[0];
			incrop.y = node_group->leader.input.cropRegion[1];
			incrop.w = node_group->leader.input.cropRegion[2];
			incrop.h = node_group->leader.input.cropRegion[3];
			otcrop.x = incrop.x;
			otcrop.y = incrop.y;
			otcrop.w = incrop.w;
			otcrop.h = incrop.h;
			ret = fimc_is_ischain_dis_group_tag(device, frame, &incrop, &otcrop);
			if (ret) {
				merr("fimc_is_ischain_dis_group_tag is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		default:
			merr("group slot is invalid(%d)", device, child->slot);
			BUG();
		}

		parent = child;
		child = child->child;
	}

	clear_bit(FIMC_IS_SUBDEV_FORCE_SET, &group->leader.state);
	PROGRAM_COUNT(10);

p_err:
	if (ret) {
		mgrerr(" SKIP(%d) : %d\n", device, group, check_frame, check_frame->index, ret);
	} else {
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_27, flags);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_27, flags);
		set_bit(REQ_DIS_SHOT, &frame->req_flag);
	}

	return ret;
}

int fimc_is_ischain_camctl(struct fimc_is_device_ischain *this,
	struct fimc_is_frame *frame,
	u32 fcount)
{
	int ret = 0;
#ifdef ENABLE_SENSOR_DRIVER
	struct fimc_is_interface *itf;
	struct camera2_uctl *applied_ctl;

	struct camera2_sensor_ctl *isp_sensor_ctl;
	struct camera2_lens_ctl *isp_lens_ctl;
	struct camera2_flash_ctl *isp_flash_ctl;

	u32 index;

#ifdef DBG_STREAMING
	mdbgd_ischain("%s()\n", device, __func__);
#endif

	itf = this->interface;
	isp_sensor_ctl = &itf->isp_peri_ctl.sensorUd.ctl;
	isp_lens_ctl = &itf->isp_peri_ctl.lensUd.ctl;
	isp_flash_ctl = &itf->isp_peri_ctl.flashUd.ctl;

	/*lens*/
	index = (fcount + 0) & SENSOR_MAX_CTL_MASK;
	applied_ctl = &this->peri_ctls[index];
	applied_ctl->lensUd.ctl.focusDistance = isp_lens_ctl->focusDistance;

	/*sensor*/
	index = (fcount + 1) & SENSOR_MAX_CTL_MASK;
	applied_ctl = &this->peri_ctls[index];
	applied_ctl->sensorUd.ctl.exposureTime = isp_sensor_ctl->exposureTime;
	applied_ctl->sensorUd.ctl.frameDuration = isp_sensor_ctl->frameDuration;
	applied_ctl->sensorUd.ctl.sensitivity = isp_sensor_ctl->sensitivity;

	/*flash*/
	index = (fcount + 0) & SENSOR_MAX_CTL_MASK;
	applied_ctl = &this->peri_ctls[index];
	applied_ctl->flashUd.ctl.flashMode = isp_flash_ctl->flashMode;
	applied_ctl->flashUd.ctl.firingPower = isp_flash_ctl->firingPower;
	applied_ctl->flashUd.ctl.firingTime = isp_flash_ctl->firingTime;
#endif
	return ret;
}

int fimc_is_ischain_tag(struct fimc_is_device_ischain *ischain,
	struct fimc_is_frame *frame)
{
	int ret = 0;
#ifdef ENABLE_SENSOR_DRIVER
	struct camera2_uctl *applied_ctl;
	struct timeval curtime;
	u32 fcount;

	fcount = frame->fcount;
	applied_ctl = &ischain->peri_ctls[fcount & SENSOR_MAX_CTL_MASK];

	do_gettimeofday(&curtime);

	/* Request */
	frame->shot->dm.request.frameCount = fcount;

	/* Lens */
	frame->shot->dm.lens.focusDistance =
		applied_ctl->lensUd.ctl.focusDistance;

	/* Sensor */
	frame->shot->dm.sensor.exposureTime =
		applied_ctl->sensorUd.ctl.exposureTime;
	frame->shot->dm.sensor.sensitivity =
		applied_ctl->sensorUd.ctl.sensitivity;
	frame->shot->dm.sensor.frameDuration =
		applied_ctl->sensorUd.ctl.frameDuration;
	frame->shot->dm.sensor.timeStamp =
		(uint64_t)curtime.tv_sec*1000000 + curtime.tv_usec;

	/* Flash */
	frame->shot->dm.flash.flashMode =
		applied_ctl->flashUd.ctl.flashMode;
	frame->shot->dm.flash.firingPower =
		applied_ctl->flashUd.ctl.firingPower;
	frame->shot->dm.flash.firingTime =
		applied_ctl->flashUd.ctl.firingTime;
#else
	struct timespec curtime;

	do_posix_clock_monotonic_gettime(&curtime);

	frame->shot->dm.request.frameCount = frame->fcount;
	frame->shot->dm.sensor.timeStamp = fimc_is_get_timestamp();
#endif
	return ret;
}
