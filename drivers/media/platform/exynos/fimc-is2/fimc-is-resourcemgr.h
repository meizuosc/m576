/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_RESOURCE_MGR_H
#define FIMC_IS_RESOURCE_MGR_H

#include <linux/notifier.h>
#include "fimc-is-groupmgr.h"
#include "fimc-is-interface.h"

#define RESOURCE_TYPE_SENSOR0	0
#define RESOURCE_TYPE_SENSOR1	1
#define RESOURCE_TYPE_ISCHAIN	2
#define RESOURCE_TYPE_COMPANION 3
#define RESOURCE_TYPE_MAX	4

enum fimc_is_resourcemgr_state {
	FIMC_IS_RM_COM_POWER_ON,
	FIMC_IS_RM_SS0_POWER_ON,
	FIMC_IS_RM_SS1_POWER_ON,
	FIMC_IS_RM_SS2_POWER_ON,
	FIMC_IS_RM_SS3_POWER_ON,
	FIMC_IS_RM_ISC_POWER_ON,
	FIMC_IS_RM_POWER_ON
};

struct fimc_is_dvfs_ctrl {
	struct mutex lock;
	int cur_int_qos;
	int cur_mif_qos;
	int cur_cam_qos;
	int cur_i2c_qos;
	int cur_disp_qos;
	int dvfs_table_idx;

	struct fimc_is_dvfs_scenario_ctrl *static_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *dynamic_ctrl;
};

struct fimc_is_clk_gate_ctrl {
	spinlock_t lock;
	unsigned long msk_state;
	int msk_cnt[GROUP_ID_MAX];
	u32 msk_lock_by_ischain[FIMC_IS_STREAM_COUNT];
	struct exynos_fimc_is_clk_gate_info *gate_info;
	u32 msk_clk_on_off_state; /* on/off(1/0) state per ip */
	/*
	 * For check that there's too long clock-on period.
	 * This var will increase when clock on,
	 * And will decrease when clock off.
	 */
	unsigned long chk_on_off_cnt[GROUP_ID_MAX];
};

struct fimc_is_resource {
        struct platform_device                  *pdev;
        void __iomem                            *regs;
        atomic_t                                rsccount;
        u32                                     private_data;
};

struct fimc_is_resourcemgr {
	unsigned long				state;
	atomic_t				rsccount;
	atomic_t				rsccount_module; /* sensor module */
	struct fimc_is_resource			resource_sensor0;
	struct fimc_is_resource			resource_sensor1;
	struct fimc_is_resource			resource_ischain;
	struct fimc_is_resource			resource_companion;

	struct fimc_is_mem			mem;
	struct fimc_is_minfo			minfo;

	struct fimc_is_dvfs_ctrl		dvfs_ctrl;
	struct fimc_is_clk_gate_ctrl		clk_gate_ctrl;
	u32					hal_version;

	/* tmu */
	struct notifier_block			notifier;
	u32					tmu_state;
	u32					limited_fps;

	void					*private_data;
};

int fimc_is_resourcemgr_probe(struct fimc_is_resourcemgr *resourcemgr, void *private_data);
int fimc_is_resource_open(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type, void **device);
int fimc_is_resource_get(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type);
int fimc_is_resource_put(struct fimc_is_resourcemgr *resourcemgr, u32 rsc_type);
int fimc_is_resource_ioctl(struct fimc_is_resourcemgr *resourcemgr, u32 control);
int fimc_is_logsync(struct fimc_is_interface *itf, u32 sync_id, u32 msg_test_id);

#define GET_RESOURCE(resourcemgr, type) \
	((type == RESOURCE_TYPE_SENSOR0) ? &resourcemgr->resource_sensor0 : \
	((type == RESOURCE_TYPE_SENSOR1) ? &resourcemgr->resource_sensor1 : \
	((type == RESOURCE_TYPE_ISCHAIN) ? &resourcemgr->resource_ischain : \
	((type == RESOURCE_TYPE_COMPANION) ? &resourcemgr->resource_companion : \
	NULL))))

#endif
