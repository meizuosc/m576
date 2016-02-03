/* drivers/gpu/arm/.../platform/gpu_exynos5433.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_exynos5433.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>
#include <linux/mfd/samsung/core.h>

#include <mach/asv-exynos.h>
#include <mach/asv-exynos_cal.h>
#include <mach/pm_domains.h>
#if defined(CONFIG_EXYNOS5433_BTS)
#include <mach/bts.h>
#endif /* CONFIG_EXYNOS5433_BTS */

#include <linux/sec_debug.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

extern struct kbase_device *pkbdev;

#define CPU_MAX PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE
#define G3D_RBB_VALUE	0x8d

#define GPU_OSC_CLK	24000

/*  clk,vol,abb,min,max,down stay,time_in_state,pm_qos mem,pm_qos int,pm_qos cpu_kfc_min,pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
	{700, 1150000, 0, 98, 100, 1, 0, 921000, 400000, 1300000, 1300000},
	{600, 1150000, 0, 98,  99, 1, 0, 921000, 400000, 1300000, 1300000},
	{550, 1125000, 0, 98,  99, 1, 0, 825000, 400000, 1300000, 1800000},
	{500, 1075000, 0, 98,  99, 1, 0, 825000, 400000, 1300000, 1800000},
	{420, 1025000, 0, 80,  99, 1, 0, 667000, 200000,  900000, 1800000},
	{350, 1025000, 0, 80,  90, 1, 0, 543000, 160000,       0, CPU_MAX},
	{266, 1000000, 0, 80,  90, 3, 0, 413000, 133000,       0, CPU_MAX},
	{160, 1000000, 0,  0,  90, 1, 0, 272000, 133000,       0, CPU_MAX},
};

static int mif_min_table[] = {
	78000, 109000, 136000,
	167000, 222000, 272000,
	413000, 543000, 667000,
	825000,
};

static int available_max_clock[] = {GPU_L2, GPU_L2, GPU_L0, GPU_L0, GPU_L0};

static gpu_attribute gpu_config_attributes[] = {
	{GPU_MAX_CLOCK, 700},
	{GPU_MAX_CLOCK_LIMIT, 600},
	{GPU_MIN_CLOCK, 160},
	{GPU_DVFS_START_CLOCK, 266},
	{GPU_DVFS_BL_CONFIG_CLOCK, 266},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 266},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 266},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 266},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_DEFAULT_VOLTAGE, 937500},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 37500},
	{GPU_TMU_CONTROL, 1},
	{GPU_TEMP_THROTTLING1, 420},
	{GPU_TEMP_THROTTLING2, 350},
	{GPU_TEMP_THROTTLING3, 266},
	{GPU_TEMP_THROTTLING4, 160},
	{GPU_TEMP_TRIPPING, 160},
	{GPU_BOOST_MIN_LOCK, 600},
	{GPU_BOOST_EGL_MIN_LOCK, 1600000},
	{GPU_POWER_COEFF, 460}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 1},
	{GPU_DYNAMIC_ABB, 1},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_DVS, 1},
	{GPU_PERF_GATHERING, 0},
#ifdef MALI_SEC_HWCNT
	{GPU_HWCNT_GATHERING, 1},
	{GPU_HWCNT_GPR, 1},
	{GPU_HWCNT_DUMP_PERIOD, 50}, /* ms */
	{GPU_HWCNT_CHOOSE_JM , 0},
	{GPU_HWCNT_CHOOSE_SHADER , 0x560},
	{GPU_HWCNT_CHOOSE_TILER , 0},
	{GPU_HWCNT_CHOOSE_L3_CACHE , 0},
	{GPU_HWCNT_CHOOSE_MMU_L2 , 0},
#endif
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 100},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
};

int gpu_dvfs_decide_max_clock(struct exynos_context *platform)
{
	int table_id;
	int level;

	if (!platform)
		return -1;

	table_id = cal_get_table_ver();

	if (table_id < 0)
		return -1;

	if (table_id >= GPU_DVFS_TABLE_LIST_SIZE(available_max_clock))
		table_id = GPU_DVFS_TABLE_LIST_SIZE(available_max_clock)-1;

	level = available_max_clock[table_id];

	platform->gpu_max_clock = MIN(platform->gpu_max_clock, platform->table[level].clock);

	if (is_max_limit_sample())
		platform->gpu_max_clock = MIN(platform->gpu_max_clock, platform->table[GPU_L3].clock);

	return 0;
}

void *gpu_get_config_attributes(void)
{
	return &gpu_config_attributes;
}

struct clk *fin_pll;
struct clk *fout_g3d_pll;
struct clk *aclk_g3d;
struct clk *mout_g3d_pll;
struct clk *dout_aclk_g3d;

#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
#endif /* CONFIG_REGULATOR */

int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS_PMU_G3D_STATUS) & POWER_ON_STATUS) == POWER_ON_STATUS) ? 1 : 0;
}

int gpu_power_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power initialized\n");

	return 0;
}

int gpu_get_cur_clock(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (!aclk_g3d) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: clock is not initialized\n", __func__);
		return -1;
	}

	return clk_get_rate(aclk_g3d)/MHZ;
}

int gpu_is_clock_on(void)
{
	return __clk_is_enabled(aclk_g3d);
}

static int gpu_clock_on(struct exynos_context *platform)
{
	int ret = 0;
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: can't set clock on in power off status\n", __func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 1) {
		ret = 0;
		goto err_return;
	}

	if (aclk_g3d) {
		(void) clk_prepare_enable(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u, "clock is enabled\n");
	}

	platform->clk_g3d_status = 1;

err_return:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

static int gpu_clock_off(struct exynos_context *platform)
{
	int ret = 0;

	if (!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock off in power off status\n", __func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 0) {
		ret = 0;
		goto err_return;
	}

	if (aclk_g3d) {
		(void)clk_disable_unprepare(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF, 0u, 0u, "clock is disabled\n");
	}

	platform->clk_g3d_status = 0;

err_return:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

int gpu_register_dump(void)
{
	if (gpu_is_power_on()) {
		/* G3D PMU */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x105C4064, __raw_readl(EXYNOS5433_G3D_STATUS),
							"REG_DUMP: EXYNOS5433_G3D_STATUS %x\n", __raw_readl(EXYNOS_PMU_G3D_STATUS));
		/* G3D PLL */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0000, __raw_readl(EXYNOS5430_G3D_PLL_LOCK),
							"REG_DUMP: EXYNOS5433_G3D_PLL_LOCK %x\n", __raw_readl(EXYNOS5430_G3D_PLL_LOCK));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0100, __raw_readl(EXYNOS5430_G3D_PLL_CON0),
							"REG_DUMP: EXYNOS5433_G3D_PLL_CON0 %x\n", __raw_readl(EXYNOS5430_G3D_PLL_CON0));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0104, __raw_readl(EXYNOS5430_G3D_PLL_CON1),
							"REG_DUMP: EXYNOS5433_G3D_PLL_CON1 %x\n", __raw_readl(EXYNOS5430_G3D_PLL_CON1));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA010c, __raw_readl(EXYNOS5430_G3D_PLL_FREQ_DET),
							"REG_DUMP: EXYNOS5433_G3D_PLL_FREQ_DET %x\n", __raw_readl(EXYNOS5430_G3D_PLL_FREQ_DET));

		/* G3D SRC */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0200, __raw_readl(EXYNOS5430_SRC_SEL_G3D),
							"REG_DUMP: EXYNOS5430_SRC_SEL_G3D %x\n", __raw_readl(EXYNOS5430_SRC_SEL_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0300, __raw_readl(EXYNOS5430_SRC_ENABLE_G3D),
							"REG_DUMP: EXYNOS5430_SRC_ENABLE_G3D %x\n", __raw_readl(EXYNOS5430_SRC_ENABLE_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0400, __raw_readl(EXYNOS5430_SRC_STAT_G3D),
							"REG_DUMP: EXYNOS5430_SRC_STAT_G3D %x\n", __raw_readl(EXYNOS5430_SRC_STAT_G3D));

		/* G3D DIV */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0600, __raw_readl(EXYNOS5430_DIV_G3D),
							"REG_DUMP: EXYNOS5430_DIV_G3D %x\n", __raw_readl(EXYNOS5430_DIV_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0604, __raw_readl(EXYNOS5430_DIV_G3D_PLL_FREQ_DET),
							"REG_DUMP: EXYNOS5430_DIV_G3D_PLL_FREQ_DET %x\n", __raw_readl(EXYNOS5430_DIV_G3D_PLL_FREQ_DET));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0700, __raw_readl(EXYNOS5430_DIV_STAT_G3D),
							"REG_DUMP: EXYNOS5430_DIV_STAT_G3D %x\n", __raw_readl(EXYNOS5430_DIV_STAT_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0704, __raw_readl(EXYNOS5430_DIV_STAT_G3D_PLL_FREQ_DET),
							"REG_DUMP: EXYNOS5430_DIV_STAT_G3D_PLL_FREQ_DET %x\n", __raw_readl(EXYNOS5430_DIV_STAT_G3D_PLL_FREQ_DET));

		/* G3D ENABLE */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0800, __raw_readl(EXYNOS5430_ENABLE_ACLK_G3D),
							"REG_DUMP: EXYNOS5430_ENABLE_ACLK_G3D %x\n", __raw_readl(EXYNOS5430_ENABLE_ACLK_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0900, __raw_readl(EXYNOS5430_ENABLE_PCLK_G3D),
							"REG_DUMP: EXYNOS5430_ENABLE_PCLK_G3D %x\n", __raw_readl(EXYNOS5430_ENABLE_PCLK_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0A00, __raw_readl(EXYNOS5430_ENABLE_SCLK_G3D),
							"REG_DUMP: EXYNOS5430_ENABLE_SCLK_G3D %x\n", __raw_readl(EXYNOS5430_ENABLE_SCLK_G3D));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0B00, __raw_readl(EXYNOS5430_ENABLE_IP_G3D0),
							"REG_DUMP: EXYNOS5430_ENABLE_IP_G3D0 %x\n", __raw_readl(EXYNOS5430_ENABLE_IP_G3D0));
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0B0A, __raw_readl(EXYNOS5430_ENABLE_IP_G3D1),
							"REG_DUMP: EXYNOS5430_ENABLE_IP_G3D1 %x\n", __raw_readl(EXYNOS5430_ENABLE_IP_G3D1));
	}

	return 0;
}

static int gpu_set_clock(struct exynos_context *platform, int clk)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = clk * MHZ;
	int ret = 0;

	if (aclk_g3d == 0)
		return -1;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the power-off state!\n", __func__);
		goto err;
	}

	if (!gpu_is_clock_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the clock-off state! %d\n", __func__, __raw_readl(EXYNOS5430_ENABLE_ACLK_G3D));
		goto err;
	}

	g3d_rate_prev = clk_get_rate(aclk_g3d);

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {
		/*change here for future stable clock changing*/
		ret = clk_set_parent(mout_g3d_pll, fin_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [mout_g3d_pll]\n", __func__);
			goto err;
		}

		if (g3d_rate_prev != GPU_OSC_CLK)
			sec_debug_aux_log(SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE,
				"[GPU] %7d <= %7d", g3d_rate / 1000, g3d_rate_prev / 1000);

		/*change g3d pll*/
		ret = clk_set_rate(fout_g3d_pll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_rate [fout_g3d_pll]\n", __func__);
			goto err;
		}

		/*restore parent*/
		ret = clk_set_parent(mout_g3d_pll, fout_g3d_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [mout_g3d_pll]\n", __func__);
			goto err;
		}
	}

	platform->cur_clock = gpu_get_cur_clock(platform);

	if (platform->cur_clock != clk_get_rate(fout_g3d_pll)/MHZ)
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "clock value is wrong (aclk_g3d: %d, fout_g3d_pll: %d)\n",
				platform->cur_clock, (int) clk_get_rate(fout_g3d_pll)/MHZ);

	if (g3d_rate != g3d_rate_prev)
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, g3d_rate/MHZ, platform->cur_clock, "clock set: %d, clock get: %d\n", (int) g3d_rate/MHZ, platform->cur_clock);
err:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

static int gpu_set_clock_to_osc(struct exynos_context *platform)
{
	int ret = 0;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't control clock in the power-off state!\n", __func__);
		goto err;
	}

	if (!gpu_is_clock_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't control clock in the clock-off state!\n", __func__);
		goto err;
	}

	/* change the mux to osc */
	ret = clk_set_parent(mout_g3d_pll, fin_pll);
	if (ret < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [mout_g3d_pll]\n", __func__);
		goto err;
	}

	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, platform->cur_clock, gpu_get_cur_clock(platform),
		"clock set to soc: %d (%d)\n", gpu_get_cur_clock(platform), platform->cur_clock);
err:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

static int gpu_set_clock_pre(struct exynos_context *platform, int clk, bool is_up)
{
	if (!platform)
		return -ENODEV;

#if defined(CONFIG_EXYNOS5433_BTS)
	if (!is_up) {
		if (clk < platform->table[GPU_L4].clock)
			bts_scen_update(TYPE_G3D_SCENARIO, 0);
		else
			bts_scen_update(TYPE_G3D_SCENARIO, 1);
	}
#endif /* CONFIG_EXYNOS5433_BTS */

	return 0;
}

static int gpu_set_clock_post(struct exynos_context *platform, int clk, bool is_up)
{
	if (!platform)
		return -ENODEV;

#if defined(CONFIG_EXYNOS5433_BTS)
	if (is_up) {
		if (clk < platform->table[GPU_L4].clock)
			bts_scen_update(TYPE_G3D_SCENARIO, 0);
		else
			bts_scen_update(TYPE_G3D_SCENARIO, 1);
	}
#endif /* CONFIG_EXYNOS5433_BTS */

	return 0;
}

static int gpu_get_clock(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	fin_pll = clk_get(kbdev->dev, "fin_pll");
	if (IS_ERR(fin_pll) || (fin_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [fin_pll]\n", __func__);
		return -1;
	}

	fout_g3d_pll = clk_get(NULL, "fout_g3d_pll");
	if (IS_ERR(fout_g3d_pll)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [fout_g3d_pll]\n", __func__);
		return -1;
	}

	aclk_g3d = clk_get(kbdev->dev, "aclk_g3d");
	if (IS_ERR(aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [aclk_g3d]\n", __func__);
		return -1;
	}

	dout_aclk_g3d = clk_get(kbdev->dev, "dout_aclk_g3d");
	if (IS_ERR(dout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [dout_aclk_g3d]\n", __func__);
		return -1;
	}

	mout_g3d_pll = clk_get(kbdev->dev, "mout_g3d_pll");
	if (IS_ERR(mout_g3d_pll)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [mout_g3d_pll]\n", __func__);
		return -1;
	}

	return 0;
}

int gpu_clock_init(struct kbase_device *kbdev)
{
	int ret;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return -1;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clock initialized\n");

	return 0;
}

int gpu_get_cur_voltage(struct exynos_context *platform)
{
	int ret = 0;
#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	ret = regulator_get_voltage(g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return ret;
}

static int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	if (gpu_get_cur_voltage(platform) == vol)
		return 0;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set voltage in the power-off state!\n", __func__);
		return -1;
	}

#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_set_voltage(g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage, voltage: %d\n", __func__, vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	platform->cur_voltage = gpu_get_cur_voltage(platform);

	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, vol, platform->cur_voltage, "voltage set: %d, voltage get:%d\n", vol, platform->cur_voltage);

	return 0;
}

static int gpu_set_voltage_pre(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (!is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static int gpu_set_voltage_post(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static struct gpu_control_ops ctr_ops = {
	.is_power_on = gpu_is_power_on,
	.set_voltage = gpu_set_voltage,
	.set_voltage_pre = gpu_set_voltage_pre,
	.set_voltage_post = gpu_set_voltage_post,
	.set_clock_to_osc = gpu_set_clock_to_osc,
	.set_clock = gpu_set_clock,
	.set_clock_pre = gpu_set_clock_pre,
	.set_clock_post = gpu_set_clock_post,
	.enable_clock = gpu_clock_on,
	.disable_clock = gpu_clock_off,
};

struct gpu_control_ops *gpu_get_control_ops(void)
{
	return &ctr_ops;
}

#ifdef CONFIG_REGULATOR
int gpu_enable_dvs(struct exynos_context *platform)
{
	if (!platform->dvs_status)
		return 0;

#if defined(CONFIG_REGULATOR_S2MPS13)
	if (s2m_set_dvs_pin(true) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to enable dvs\n", __func__);
		return -1;
	}
#endif /* CONFIG_REGULATOR_S2MPS13 */

	if (!cal_get_fs_abb()) {
		if (set_match_abb(ID_G3D, G3D_RBB_VALUE)) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to restore RBB setting\n", __func__);
			return -1;
		}
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is enabled (vol: %d)\n", gpu_get_cur_voltage(platform));
	return 0;
}

int gpu_disable_dvs(struct exynos_context *platform)
{
	if (!platform->dvs_status)
		return 0;

	if (!cal_get_fs_abb()) {
		if (set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb())) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to restore RBB setting\n", __func__);
			return -1;
		}
	}

#if defined(CONFIG_REGULATOR_S2MPS13)
	if (s2m_set_dvs_pin(false) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to disable dvs\n", __func__);
		return -1;
	}
#endif /* CONFIG_REGULATOR_S2MPS13 */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is disabled (vol: %d)\n", gpu_get_cur_voltage(platform));
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;

	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get vdd_g3d regulator, 0x%p\n", __func__, g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	gpu_voltage = get_match_volt(ID_G3D, platform->gpu_dvfs_config_clock*1000);

	if (gpu_voltage == 0)
		gpu_voltage = platform->gpu_default_vol;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage [%d]\n", __func__, gpu_voltage);
		return -1;
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */

int *get_mif_table(int *size)
{
	*size = ARRAY_SIZE(mif_min_table);
	return mif_min_table;
}
