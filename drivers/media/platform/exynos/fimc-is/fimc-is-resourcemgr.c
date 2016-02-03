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

#include "fimc-is-resourcemgr.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-clk-gate.h"

struct pm_qos_request exynos_isp_qos_cpu_min;
struct pm_qos_request exynos_isp_qos_cpu_max;
struct pm_qos_request exynos_isp_qos_int;
struct pm_qos_request exynos_isp_qos_mem;
struct pm_qos_request exynos_isp_qos_cam;
struct pm_qos_request exynos_isp_qos_disp;
#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
struct pm_qos_request max_big_qos;
#endif

extern struct fimc_is_sysfs_debug sysfs_debug;

int fimc_is_resource_probe(struct fimc_is_resourcemgr *resourcemgr,
	void *private_data)
{
	int ret = 0;

	BUG_ON(!resourcemgr);
	BUG_ON(!private_data);

	resourcemgr->private_data = private_data;

	atomic_set(&resourcemgr->rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor0.rsccount, 0);
	atomic_set(&resourcemgr->resource_sensor1.rsccount, 0);
	atomic_set(&resourcemgr->resource_ischain.rsccount, 0);

	resourcemgr->hal_version = IS_HAL_VER_1_0;
#ifdef ENABLE_DVFS
	/* dvfs controller init */
	ret = fimc_is_dvfs_init(resourcemgr);
	if (ret)
		err("%s: fimc_is_dvfs_init failed!\n", __func__);
#endif

	info("%s\n", __func__);
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

	if (rsccount >= 5) {
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
		case RESOURCE_TYPE_SENSOR0:
			break;
		case RESOURCE_TYPE_SENSOR1:
			break;
		case RESOURCE_TYPE_ISCHAIN:
			core->debug_cnt = 0;

			ret = fimc_is_interface_open(&core->interface);
			if (ret) {
				err("fimc_is_interface_open is fail(%d)", ret);
				goto p_err;
			}

			ret = fimc_is_ischain_power(&core->ischain[0], 1);
			if (ret) {
				err("fimc_is_ischain_power is fail (%d)", ret);
				goto p_err;
			}

			/* W/A for a lower version MCUCTL */
			fimc_is_interface_reset(&core->interface);

#ifdef ENABLE_CLOCK_GATE
			if (sysfs_debug.en_clk_gate &&
					sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
				fimc_is_clk_gate_init(core);
#endif
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
	info("[RSC] rsctype : %d, rsccount : %d\n", rsc_type, rsccount);
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

	if (atomic_read(&resource->rsccount) == 1) {
		/* clear hal version, default 1.0 */
		resourcemgr->hal_version = IS_HAL_VER_1_0;

		switch (rsc_type) {
		case RESOURCE_TYPE_SENSOR0:
			break;
		case RESOURCE_TYPE_SENSOR1:
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
#ifndef RESERVED_MEM
			/* 5. Dealloc memroy */
			ret = fimc_is_ishcain_deinitmem(&core->ischain[0]);
			if (ret)
				err("fimc_is_ishcain_deinitmem is fail(%d)", ret);
#endif
			break;

		default:
			err("[RSC] resource type(%d) is invalid", rsc_type);
			BUG();
			break;
		}
	}

	atomic_dec(&resource->rsccount);
	atomic_dec(&core->rsccount);

p_err:
	info("[RSC] rsctype : %d, rsccount : %d\n", rsc_type, rsccount);
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
