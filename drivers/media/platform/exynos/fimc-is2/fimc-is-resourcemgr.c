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

#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <mach/regs-clock.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/tmu.h>

#include "fimc-is-resourcemgr.h"
#include "fimc-is-hw.h"
#include "fimc-is-debug.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-clk-gate.h"

struct pm_qos_request exynos_isp_qos_int;
struct pm_qos_request exynos_isp_qos_mem;
struct pm_qos_request exynos_isp_qos_cam;
struct pm_qos_request exynos_isp_qos_disp;
#ifdef CONFIG_SOC_EXYNOS5422
struct pm_qos_request max_cpu_qos;
#endif

extern struct fimc_is_sysfs_debug sysfs_debug;

static int fimc_is_resourcemgr_allocmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
	void *fw_cookie;
	size_t fw_size = FIMC_IS_A5_MEM_SIZE;
#ifdef FW_SUSPEND_RESUME
	fw_size += FIMC_IS_BACKUP_SIZE;
#endif
#ifdef ENABLE_ODC
	fw_size += SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF;
#endif
#ifdef ENABLE_VDIS
	fw_size += SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF;
#endif
#ifdef ENABLE_DNR
	fw_size += SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF;
#endif
#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
	fw_size += MAX_STAT_INTERNEL_BUF_SIZE * NUM_STAT_INTERNAL_BUF;
#endif

	fw_size = PAGE_ALIGN(fw_size);
	dbg_core("Allocating memory for FIMC-IS firmware.\n");

	fw_cookie = vb2_ion_private_alloc(resourcemgr->mem.alloc_ctx, fw_size, 1, 0);

	if (IS_ERR(fw_cookie)) {
		err("Allocating bitprocessor buffer failed");
		fw_cookie = NULL;
		ret = -ENOMEM;
		goto p_err;
	}

	ret = vb2_ion_dma_address(fw_cookie, &resourcemgr->minfo.dvaddr);
	if ((ret < 0) || (resourcemgr->minfo.dvaddr  & FIMC_IS_FW_BASE_MASK)) {
		err("The base memory is not aligned to 64MB.");
		vb2_ion_private_free(fw_cookie);
		resourcemgr->minfo.dvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto p_err;
	}

#ifdef PRINT_BUFADDR
	info("[RSC] daddr = %pa, size = %08X\n", &resourcemgr->minfo.dvaddr, FIMC_IS_A5_MEM_SIZE);
#endif

	resourcemgr->minfo.kvaddr = (ulong)vb2_ion_private_vaddr(fw_cookie);
	if (IS_ERR((void *)resourcemgr->minfo.kvaddr)) {
		err("Bitprocessor memory remap failed");
		vb2_ion_private_free(fw_cookie);
		resourcemgr->minfo.kvaddr = 0;
		fw_cookie = NULL;
		ret = -EIO;
		goto p_err;
	}

	vb2_ion_sync_for_device(fw_cookie, 0, fw_size, DMA_BIDIRECTIONAL);

p_err:
	info("[RSC] Device virtual for internal: %08lx\n", resourcemgr->minfo.kvaddr);
	resourcemgr->minfo.fw_cookie = fw_cookie;

	return ret;
}

static int fimc_is_resourcemgr_initmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;
#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
	int num_stat_buf = 0;
#endif
	u32 offset;

	dbg_core("fimc_is_init_mem - ION\n");

	ret = fimc_is_resourcemgr_allocmem(resourcemgr);
	if (ret) {
		err("Couldn't alloc for FIMC-IS firmware\n");
		ret = -ENOMEM;
		goto p_err;
	}

	offset = FW_SHARED_OFFSET;
	resourcemgr->minfo.dvaddr_fshared = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_fshared = resourcemgr->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
	resourcemgr->minfo.dvaddr_region = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_region = resourcemgr->minfo.kvaddr + offset;

	offset = FIMC_IS_A5_MEM_SIZE;
#ifdef ENABLE_ODC
	resourcemgr->minfo.dvaddr_odc = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_odc = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_ODC_INTERNAL_BUF * NUM_ODC_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_odc = 0;
	resourcemgr->minfo.kvaddr_odc = 0;
#endif

#ifdef ENABLE_VDIS
	resourcemgr->minfo.dvaddr_dis = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_dis = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_dis = 0;
	resourcemgr->minfo.kvaddr_dis = 0;
#endif

#ifdef ENABLE_DNR
	resourcemgr->minfo.dvaddr_3dnr = resourcemgr->minfo.dvaddr + offset;
	resourcemgr->minfo.kvaddr_3dnr = resourcemgr->minfo.kvaddr + offset;
	offset += (SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF);
#else
	resourcemgr->minfo.dvaddr_3dnr = 0;
	resourcemgr->minfo.kvaddr_3dnr = 0;
#endif

#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
	for (num_stat_buf = 0; num_stat_buf < NUM_STAT_INTERNAL_BUF; num_stat_buf++) {
		resourcemgr->minfo.dvaddr_stat[num_stat_buf] = resourcemgr->minfo.dvaddr + offset;
		resourcemgr->minfo.kvaddr_stat[num_stat_buf] = resourcemgr->minfo.kvaddr + offset;
		offset += (MAX_STAT_INTERNEL_BUF_SIZE);
	}
#endif
	dbg_core("fimc_is_init_mem done\n");

p_err:
	return ret;
}

#ifndef ENABLE_RESERVED_MEM
static int fimc_is_resourcemgr_deinitmem(struct fimc_is_resourcemgr *resourcemgr)
{
	int ret = 0;

	vb2_ion_private_free(resourcemgr->minfo.fw_cookie);

	return ret;
}
#endif

int fimc_is_tmu_notifier(struct notifier_block *nb,
	unsigned long state, void *data)
{
	int ret = 0;
	struct fimc_is_resourcemgr *resourcemgr;

	resourcemgr = container_of(nb, struct fimc_is_resourcemgr, notifier);

	switch (state) {
	case ISP_NORMAL:
		resourcemgr->tmu_state = ISP_NORMAL;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_COLD:
		resourcemgr->tmu_state = ISP_COLD;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_THROTTLING1:
		resourcemgr->tmu_state = ISP_THROTTLING1;
		resourcemgr->limited_fps = 0;
		break;
	case ISP_THROTTLING2:
		resourcemgr->tmu_state = ISP_THROTTLING2;
		resourcemgr->limited_fps = 0;
		warn("[RSC] THROTTLING2 : Unlimited FPS");
		break;
	case ISP_THROTTLING3:
		resourcemgr->tmu_state = ISP_THROTTLING3;
		resourcemgr->limited_fps = 15;
		warn("[RSC] THROTTLING3 : Limited 15FPS");
		break;
	case ISP_THROTTLING4:
		resourcemgr->tmu_state = ISP_THROTTLING4;
		resourcemgr->limited_fps = 5;
		warn("[RSC] THROTTLING4 : Limited 5FPS");
		break;
	case ISP_TRIPPING:
		resourcemgr->tmu_state = ISP_TRIPPING;
		resourcemgr->limited_fps = 5;
		warn("[RSC] THROTTLING5 : Limited 5FPS");
		break;
	default:
		err("[RSC] invalid tmu state(%ld)", state);
		break;
	}

	return ret;
}

int fimc_is_resourcemgr_probe(struct fimc_is_resourcemgr *resourcemgr,
	void *private_data)
{
	int ret = 0;

	BUG_ON(!resourcemgr);
	BUG_ON(!private_data);

	resourcemgr->private_data = private_data;

	clear_bit(FIMC_IS_RM_COM_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS0_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS1_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS2_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_SS3_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_ISC_POWER_ON, &resourcemgr->state);
	clear_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state);
	atomic_set(&resourcemgr->rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor0.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor1.rsccount, 0);
	atomic_set(&resourcemgr->resource_ischain.rsccount, 0);
	atomic_set(&resourcemgr->resource_companion.rsccount, 0);

	resourcemgr->hal_version = IS_HAL_VER_1_0;
	resourcemgr->tmu_state = ISP_NORMAL;
	resourcemgr->limited_fps = 0;
	resourcemgr->notifier.notifier_call = fimc_is_tmu_notifier;
	resourcemgr->notifier.priority = 0;
	ret = exynos_tmu_isp_add_notifier(&resourcemgr->notifier);
	if (ret) {
		probe_err("exynos_tmu_isp_add_notifier is fail(%d)", ret);
		goto p_err;
	}

#ifdef ENABLE_RESERVED_MEM
	ret = fimc_is_resourcemgr_initmem(resourcemgr);
	if (ret) {
		probe_err("fimc_is_resourcemgr_initmem is fail(%d)", ret);
		goto p_err;
	}
#endif

#ifdef ENABLE_DVFS
	/* dvfs controller init */
	ret = fimc_is_dvfs_init(resourcemgr);
	if (ret) {
		probe_err("%s: fimc_is_dvfs_init failed!\n", __func__);
		goto p_err;
	}
#endif

p_err:
	probe_info("[RSC] %s(%d)\n", __func__, ret);
	return ret;
}

int fimc_is_resource_open(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type, void **device)
{
	int ret = 0;
	u32 stream;
	void *result;
	struct fimc_is_resource *resource;
	struct fimc_is_core *core;
	struct fimc_is_device_ischain *ischain;

	BUG_ON(!resourcemgr);
	BUG_ON(!resourcemgr->private_data);
	BUG_ON(rsc_type >= RESOURCE_TYPE_MAX);

	resource = GET_RESOURCE(resourcemgr, rsc_type);
	core = (struct fimc_is_core *)resourcemgr->private_data;
	result = NULL;

	switch (rsc_type) {
	case RESOURCE_TYPE_COMPANION:
		result = &core->companion;
		resource->pdev = core->companion.pdev;
		break;
	case RESOURCE_TYPE_SENSOR0:
		result = &core->sensor[RESOURCE_TYPE_SENSOR0];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR0].pdev;
		break;
	case RESOURCE_TYPE_SENSOR1:
		result = &core->sensor[RESOURCE_TYPE_SENSOR1];
		resource->pdev = core->sensor[RESOURCE_TYPE_SENSOR1].pdev;
		break;
	case RESOURCE_TYPE_ISCHAIN:
		for (stream = 0; stream < FIMC_IS_STREAM_COUNT; ++stream) {
			ischain = &core->ischain[stream];
			if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &ischain->state)) {
				result = ischain;
				resource->pdev = ischain->pdev;
				break;
			}
		}
		break;
	default:
		err("invalid resource type(%d)", rsc_type);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	if (device)
		*device = result;

p_err:
	dbg_resource("%s(0x%p)\n", __func__, *device);
	return ret;
}

int fimc_is_resource_get(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int ret = 0;
	u32 rsccount;
	struct fimc_is_resource *resource;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);
	BUG_ON(!resourcemgr->private_data);
	BUG_ON(rsc_type >= RESOURCE_TYPE_MAX);

	resource = GET_RESOURCE(resourcemgr, rsc_type);
	core = (struct fimc_is_core *)resourcemgr->private_data;
	rsccount = atomic_read(&core->rsccount);

	if (rsccount >= (FIMC_IS_STREAM_COUNT + FIMC_IS_VIDEO_SS3_NUM)) {
		err("[RSC] Invalid rsccount(%d)", rsccount);
		ret = -EMFILE;
		goto p_err;
	}

	if (rsccount == 0) {
#ifdef ENABLE_DVFS
		/* dvfs controller init */
		ret = fimc_is_dvfs_init(resourcemgr);
		if (ret) {
			err("%s: fimc_is_dvfs_init failed!\n", __func__);
			goto p_err;
		}
#endif
	}

	if (atomic_read(&resource->rsccount) == 0) {
		switch (rsc_type) {
		case RESOURCE_TYPE_COMPANION:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_companion_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_COM_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR0:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS0_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR1:
#ifdef CONFIG_PM_RUNTIME
			pm_runtime_get_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_resume(&resource->pdev->dev);
#endif
			set_bit(FIMC_IS_RM_SS1_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
			if (test_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state)) {
				err("all resource is not power off(%lX)", resourcemgr->state);
				ret = -EINVAL;
				goto p_err;
			}

#ifndef ENABLE_RESERVED_MEM
			ret = fimc_is_resourcemgr_initmem(resourcemgr);
			if (ret) {
				err("fimc_is_resourcemgr_initmem is fail(%d)\n", ret);
				goto p_err;
			}
#endif

			ret = fimc_is_debug_open(&resourcemgr->minfo);
			if (ret) {
				err("fimc_is_debug_open is fail(%d)", ret);
				goto p_err;
			}

			ret = fimc_is_interface_open(&core->interface);
			if (ret) {
				err("fimc_is_interface_open is fail(%d)", ret);
				goto p_err;
			}

			ret = fimc_is_ischain_power(&core->ischain[0], 1);
			if (ret) {
				err("fimc_is_ischain_power is fail(%d)", ret);
				fimc_is_ischain_power(&core->ischain[0], 0);
				goto p_err;
			}

			/* W/A for a lower version MCUCTL */
			fimc_is_interface_reset(&core->interface);

#ifdef ENABLE_CLOCK_GATE
			if (sysfs_debug.en_clk_gate &&
					sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
				fimc_is_clk_gate_init(core);
#endif

			set_bit(FIMC_IS_RM_ISC_POWER_ON, &resourcemgr->state);
			set_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state);
			break;
		default:
			err("[RSC] resource type(%d) is invalid", rsc_type);
			BUG();
			break;
		}
	}

	atomic_inc(&resource->rsccount);
	atomic_inc(&core->rsccount);

p_err:
	info("[RSC] rsctype : %d, rsccount : %d\n", rsc_type, rsccount + 1);
	return ret;
}

int fimc_is_resource_put(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type)
{
	int ret = 0;
	u32 rsccount;
	struct fimc_is_resource *resource;
	struct fimc_is_core *core;

	BUG_ON(!resourcemgr);
	BUG_ON(!resourcemgr->private_data);
	BUG_ON(rsc_type >= RESOURCE_TYPE_MAX);

	resource = GET_RESOURCE(resourcemgr, rsc_type);
	core = (struct fimc_is_core *)resourcemgr->private_data;
	rsccount = atomic_read(&core->rsccount);

	if (rsccount == 0) {
		err("[RSC] Invalid rsccount(%d)\n", rsccount);
		ret = -EMFILE;
		goto p_err;
	}

	/* local update */
	if (atomic_read(&resource->rsccount) == 1) {
		/* clear hal version, default 1.0 */
		resourcemgr->hal_version = IS_HAL_VER_1_0;

		switch (rsc_type) {
		case RESOURCE_TYPE_COMPANION:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_companion_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_COM_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR0:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS0_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_SENSOR1:
#if defined(CONFIG_PM_RUNTIME)
			pm_runtime_put_sync(&resource->pdev->dev);
#else
			fimc_is_sensor_runtime_suspend(&resource->pdev->dev);
#endif
			clear_bit(FIMC_IS_RM_SS1_POWER_ON, &resourcemgr->state);
			break;
		case RESOURCE_TYPE_ISCHAIN:
			ret = fimc_is_itf_power_down(&core->interface);
			if (ret)
				err("power down cmd is fail(%d)", ret);

			ret = fimc_is_ischain_power(&core->ischain[0], 0);
			if (ret)
				err("fimc_is_ischain_power is fail(%d)", ret);

			ret = fimc_is_interface_close(&core->interface);
			if (ret)
				err("fimc_is_interface_close is fail(%d)", ret);

			ret = fimc_is_debug_close();
			if (ret)
				err("fimc_is_debug_close is fail(%d)", ret);

#ifndef ENABLE_RESERVED_MEM
			ret = fimc_is_resourcemgr_deinitmem(resourcemgr);
			if (ret)
				err("fimc_is_resourcemgr_deinitmem is fail(%d)", ret);
#endif

			clear_bit(FIMC_IS_RM_ISC_POWER_ON, &resourcemgr->state);
			break;
		default:
			err("[RSC] resource type(%d) is invalid", rsc_type);
			BUG();
			break;
		}
	}

	/* global update */
	if (atomic_read(&core->rsccount) == 1) {
		ret = fimc_is_runtime_suspend_post(NULL);
		if (ret)
			err("fimc_is_runtime_suspend_post is fail(%d)", ret);

		clear_bit(FIMC_IS_RM_POWER_ON, &resourcemgr->state);
	}

	atomic_dec(&resource->rsccount);
	atomic_dec(&core->rsccount);

p_err:
	info("[RSC] rsctype : %d, rsccount : %d\n", rsc_type, rsccount - 1);
	return ret;
}

int fimc_is_resource_ioctl(struct fimc_is_resourcemgr *resourcemgr, u32 control)
{
	int ret = 0;

	BUG_ON(!resourcemgr);
	BUG_ON(!resourcemgr->private_data);


	info("[RSC] next video node open is EOS\n");
	return ret;
}

int fimc_is_logsync(struct fimc_is_interface *itf, u32 sync_id, u32 msg_test_id)
{
	int ret = 0;

	/* print kernel sync log */
	log_sync(sync_id);

#ifdef ENABLE_FW_SYNC_LOG
	ret = fimc_is_hw_msg_test(itf, sync_id, msg_test_id);
	if (ret)
	err("fimc_is_hw_msg_test(%d)", ret);
#endif
	return ret;
}
