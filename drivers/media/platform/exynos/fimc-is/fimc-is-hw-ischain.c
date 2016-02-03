/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
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
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/devfreq.h>
#include <mach/bts.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/exynos5-mipiphy.h>

#include "fimc-is-config.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

extern struct pm_qos_request exynos_isp_qos_cpu_min;
extern struct pm_qos_request exynos_isp_qos_cpu_max;
extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_disp;
#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
extern struct pm_qos_request max_cluster1_qos;
#endif

#if defined(CONFIG_PM_DEVFREQ)
inline static void fimc_is_set_qos_init(struct fimc_is_core *core, bool on)
{
	int cpu_min_qos, cpu_max_qos, int_qos, mif_qos, cam_qos, disp_qos;

	cpu_min_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CPU_MIN, START_DVFS_LEVEL);
	cpu_max_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CPU_MAX, START_DVFS_LEVEL);
	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, START_DVFS_LEVEL);
	mif_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_MIF, START_DVFS_LEVEL);
	cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, START_DVFS_LEVEL);
	disp_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_DISP, START_DVFS_LEVEL);

	core->resourcemgr.dvfs_ctrl.cur_cpu_min_qos = cpu_min_qos;
	core->resourcemgr.dvfs_ctrl.cur_cpu_max_qos = cpu_max_qos;
	core->resourcemgr.dvfs_ctrl.cur_int_qos = int_qos;
	core->resourcemgr.dvfs_ctrl.cur_mif_qos = mif_qos;
	core->resourcemgr.dvfs_ctrl.cur_cam_qos = cam_qos;
	core->resourcemgr.dvfs_ctrl.cur_disp_qos = disp_qos;

	if (on) {
		/* DEVFREQ lock */
		if (cpu_min_qos > 0)
			pm_qos_add_request(&exynos_isp_qos_cpu_min, PM_QOS_CLUSTER1_FREQ_MIN, cpu_min_qos);
		if (cpu_max_qos > 0)
			pm_qos_add_request(&exynos_isp_qos_cpu_max, PM_QOS_CLUSTER1_FREQ_MAX, cpu_max_qos);
		if (int_qos > 0)
			pm_qos_add_request(&exynos_isp_qos_int, PM_QOS_DEVICE_THROUGHPUT, int_qos);
		if (mif_qos > 0)
			pm_qos_add_request(&exynos_isp_qos_mem, PM_QOS_BUS_THROUGHPUT, mif_qos);
		if (cam_qos > 0)
			pm_qos_add_request(&exynos_isp_qos_cam, PM_QOS_CAM_THROUGHPUT, cam_qos);
		if (disp_qos > 0)
			pm_qos_add_request(&exynos_isp_qos_disp, PM_QOS_DISPLAY_THROUGHPUT, disp_qos);

		pr_info("[RSC] %s: QoS LOCK [INT(%d), MIF(%d), CAM(%d), DISP(%d) CPU(%d/%d)]\n",
				__func__, int_qos, mif_qos, cam_qos, disp_qos, cpu_min_qos, cpu_max_qos);
	} else {
		/* DEVFREQ unlock */
		if (cpu_min_qos > 0)
			pm_qos_remove_request(&exynos_isp_qos_cpu_min);
		if (cpu_max_qos > 0)
			pm_qos_remove_request(&exynos_isp_qos_cpu_max);
		if (int_qos > 0)
			pm_qos_remove_request(&exynos_isp_qos_int);
		if (mif_qos > 0)
			pm_qos_remove_request(&exynos_isp_qos_mem);
		if (cam_qos > 0)
			pm_qos_remove_request(&exynos_isp_qos_cam);
		if (disp_qos > 0)
			pm_qos_remove_request(&exynos_isp_qos_disp);

		pr_info("[RSC] %s: QoS UNLOCK\n", __func__);
	}
}
#endif


#if (FIMC_IS_VERSION == FIMC_IS_VERSION_250)
int fimc_is_runtime_suspend(struct device *dev)
{
#ifndef CONFIG_PM_RUNTIME
	int ret = 0;
	u32 val;
#endif
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core = (struct fimc_is_core *)platform_get_drvdata(pdev);

	BUG_ON(!core);
	BUG_ON(!core->pdata);
	BUG_ON(!core->pdata->clk_off);

	info("FIMC_IS runtime suspend in\n");

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_detach_iommu(core->mem.alloc_ctx);
#endif

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	exynos5_update_media_layers(TYPE_FIMC_LITE, false);
#endif

#ifndef CONFIG_PM_RUNTIME
	/* ISP1 */
	/* 1. set internal clock reset */
	val = __raw_readl(PMUREG_CMU_RESET_ISP1_SYS_PWR);
	val = (val & ~(0x1 << 0)) | (0x0 << 0);
	__raw_writel(val, PMUREG_CMU_RESET_ISP1_SYS_PWR);

	/* 2. change to OSCCLK */
	ret = core->pdata->clk_off(pdev);
	if (ret)
		warn("clk_off is fail(%d)", ret);

	/* 3. set feedback mode */
	val = __raw_readl(PMUREG_ISP1_OPTION);
	val = (val & ~(0x3 << 0)) | (0x2 << 0);
	__raw_writel(val, PMUREG_ISP1_OPTION);

	/* 4. power off */
	val = __raw_readl(PMUREG_ISP1_CONFIGURATION);
	val = (val & ~(0x7 << 0)) | (0x0 << 0);
	__raw_writel(val, PMUREG_ISP1_CONFIGURATION);

	/* ISP0 */
	/* 1. set internal clock reset */
	val = __raw_readl(PMUREG_CMU_RESET_ISP0_SYS_PWR);
	val = (val & ~(0x1 << 0)) | (0x0 << 0);
	__raw_writel(val, PMUREG_CMU_RESET_ISP0_SYS_PWR);

	/* 2. set standbywfi a5 */
	val = __raw_readl(PMUREG_CENTRAL_SEQ_OPTION);
	val = (val & ~(0x1 << 18)) | (0x1 << 18);
	__raw_writel(val, PMUREG_CENTRAL_SEQ_OPTION);

	/* 3. stop a5 */
	__raw_writel(0x00010000, PMUREG_ISP_ARM_OPTION);

	/* 4. reset a5 */
	val = __raw_readl(PMUREG_ISP_ARM_SYS_PWR_REG);
	val = (val & ~(0x1 << 0)) | (0x1 << 0);
	__raw_writel(val, PMUREG_ISP_ARM_SYS_PWR_REG);

	/* 5. change to OSCCLK */

	/* 6. set feedback mode */
	val = __raw_readl(PMUREG_ISP0_OPTION);
	val = (val & ~(0x3 << 0)) | (0x2 << 0);
	__raw_writel(val, PMUREG_ISP0_OPTION);

	/* 7. power off */
	val = __raw_readl(PMUREG_ISP0_CONFIGURATION);
	val = (val & ~(0x7 << 0)) | (0x0 << 0);
	__raw_writel(val, PMUREG_ISP0_CONFIGURATION);

	/* 8. a5 power off */
	val = __raw_readl(PMUREG_ISP_ARM_CONFIGURATION);
	val = (val & ~(0x1 << 0)) | (0x0 << 0);
	__raw_writel(val, PMUREG_ISP_ARM_CONFIGURATION);
#endif

#if defined(CONFIG_PM_DEVFREQ)
	/* DEVFREQ release */
	fimc_is_set_qos_init(core, false);
#endif

#ifdef CONFIG_PM_RUNTIME
	if (CALL_POPS(core, clk_off, pdev) < 0)
		warn("clk_off is fail\n");
#endif

	info("FIMC_IS runtime suspend out\n");
	pm_relax(dev);
	return 0;
}

int fimc_is_runtime_resume(struct device *dev)
{
	int ret = 0;
	u32 val;

	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core = (struct fimc_is_core *)platform_get_drvdata(pdev);

	BUG_ON(!core);
	BUG_ON(!core->pdata);
	BUG_ON(!core->pdata->clk_cfg);
	BUG_ON(!core->pdata->clk_on);

	info("FIMC_IS runtime resume in\n");

	val  = __raw_readl(PMUREG_ISP0_STATUS);
	if((val & 0x7) != 0x7){
	    err("FIMC_IS runtime resume ISP0 : %d Power down\n",val);
	    BUG();
	}

	val = __raw_readl(PMUREG_ISP1_STATUS);
	if((val & 0x7) != 0x7){
	    err("FIMC_IS runtime resume ISP1 : %d Power down\n",val);
	    BUG();
	}

#ifndef CONFIG_PM_RUNTIME
	/* ISP0 */
	/* 1. set feedback mode */
	val = __raw_readl(PMUREG_ISP0_OPTION);
	val = (val & ~(0x3<< 0)) | (0x2 << 0);
	__raw_writel(val, PMUREG_ISP0_OPTION);

	/* 2. power on isp0 */
	val = __raw_readl(PMUREG_ISP0_CONFIGURATION);
	val = (val & ~(0x7 << 0)) | (0x7 << 0);
	__raw_writel(val, PMUREG_ISP0_CONFIGURATION);

	/* ISP1 */
	/* 3. set feedback mode */
	val = __raw_readl(PMUREG_ISP1_OPTION);
	val = (val & ~(0x3<< 0)) | (0x2 << 0);
	__raw_writel(val, PMUREG_ISP1_OPTION);

	/* 4. power on isp1 */
	val = __raw_readl(PMUREG_ISP1_CONFIGURATION);
	val = (val & ~(0x7 << 0)) | (0x7 << 0);
	__raw_writel(val, PMUREG_ISP1_CONFIGURATION);
#endif

	ret = core->pdata->clk_cfg(pdev);
	if (ret) {
		err("clk_cfg is fail(%d)", ret);
		goto p_err;
	}

	/* HACK: DVFS lock sequence is change.
	 * DVFS level should be locked after power on.
	 */
#if defined(CONFIG_PM_DEVFREQ)
	/* DEVFREQ set */
	fimc_is_set_qos_init(core, true);
#endif

	/* Clock on */
	ret = core->pdata->clk_on(pdev);
	if (ret) {
		err("clk_on is fail(%d)", ret);
		goto p_err;
	}

#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_attach_iommu(core->mem.alloc_ctx);
#endif

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	exynos5_update_media_layers(TYPE_FIMC_LITE, true);
#endif

	pm_stay_awake(dev);

p_err:
	info("FIMC-IS runtime resume out\n");
	return ret;
}

#else
int fimc_is_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);

	BUG_ON(!core);
	BUG_ON(!core->pdata);
	BUG_ON(!core->pdata->clk_off);

	pr_info("FIMC_IS runtime suspend in\n");

#if !(defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433))
#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_detach_iommu(core->mem.alloc_ctx);
#endif
#endif

#if defined(CONFIG_PM_DEVFREQ)
	/* DEVFREQ set */
	fimc_is_set_qos_init(core, false);
#endif

#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	/* EGL Release */
	pm_qos_update_request(&max_cluster1_qos, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
	pm_qos_remove_request(&max_cluster1_qos);
#endif /* CONFIG_SOC_EXYNOS5422 */

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	/* BTS */
#if defined(CONFIG_SOC_EXYNOS5260)
	bts_initialize("spd-flite-a", false);
	bts_initialize("spd-flite-b", false);
#elif defined(CONFIG_SOC_EXYNOS3470)
	bts_initialize("pd-cam", false);
#else
	bts_initialize("pd-fimclite", false);
#endif
	/* media layer */
	exynos5_update_media_layers(TYPE_FIMC_LITE, false);
#endif /* CONFIG_FIMC_IS_BUS_DEVFREQ */

	if (CALL_POPS(core, clk_off, pdev) < 0)
		warn("clk_off is fail\n");

	pr_info("FIMC_IS runtime suspend out\n");

	pm_relax(dev);
	return 0;
}

int fimc_is_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(pdev);

	pm_stay_awake(dev);
	pr_info("FIMC_IS runtime resume in\n");

#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430)
	/* EGL Lock */
	pm_qos_add_request(&max_cluster1_qos, PM_QOS_CLUSTER1_FREQ_MAX, 1600000);
#elif defined(CONFIG_SOC_EXYNOS5433)
	/* EGL Lock */
	pm_qos_add_request(&max_cluster1_qos, PM_QOS_CLUSTER1_FREQ_MAX, 1700000);
#endif /* CONFIG_SOC_EXYNOS5422 */

	/* HACK: DVFS lock sequence is change.
	 * DVFS level should be locked after power on.
	 */
#if defined(CONFIG_PM_DEVFREQ)
	/* DEVFREQ set */
	fimc_is_set_qos_init(core, true);
#endif

	/* Low clock setting */
	if (CALL_POPS(core, clk_cfg, core->pdev) < 0) {
		err("clk_cfg is fail\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* Clock on */
	if (CALL_POPS(core, clk_on, core->pdev) < 0) {
		err("clk_on is fail\n");
		ret = -EINVAL;
		goto p_err;
	}
#if !(defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433))
#if defined(CONFIG_VIDEOBUF2_ION)
	if (core->mem.alloc_ctx)
		vb2_ion_attach_iommu(core->mem.alloc_ctx);
#endif
#endif

#if defined(CONFIG_FIMC_IS_BUS_DEVFREQ)
	/* BTS */
#if defined(CONFIG_SOC_EXYNOS5260)
	bts_initialize("spd-flite-a", true);
	bts_initialize("spd-flite-b", true);
#elif defined(CONFIG_SOC_EXYNOS3470)
	bts_initialize("pd-cam", true);
#else
	bts_initialize("pd-fimclite", true);
#endif
	/* media layer */
	exynos5_update_media_layers(TYPE_FIMC_LITE, true);
#endif /* CONFIG_FIMC_IS_BUS_DEVFREQ */

	pr_info("FIMC-IS runtime resume out\n");

	return 0;

p_err:
	pm_relax(dev);
	return ret;
}
#endif
