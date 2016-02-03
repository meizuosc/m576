/* drivers/gpu/t6xx/kbase/src/platform/gpu_exynos7580.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_exynos7580.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>

#include <mach/asv-exynos.h>
#ifdef MALI64_PORTING
#include <mach/asv-exynos7_cal.h>
#endif
#include <mach/pm_domains.h>
#include <mach/regs-pmu.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#include "../mali_midg_regmap.h"

#define L2CONFIG_MO_1BY8			(0b0101)
#define L2CONFIG_MO_1BY4			(0b1010)
#define L2CONFIG_MO_1BY2			(0b1111)
#define L2CONFIG_MO_NO_RESTRICT			(0)

/*  clk,vol,abb,min,max,down stay, pm_qos mem,
	pm_qos int, pm_qos cpu_kfc_min, pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
	{800, 1200000, 0,  90, 100, 1, 0, 828000, 267000,  900000, 1800000},
	{734, 1150000, 0,  80,  90, 1, 0, 828000, 267000,  900000, 1800000},
	{668, 1100000, 0,  60,  80, 1, 0, 416000, 200000,  500000, 1800000},
	{534, 1000000, 0,  40,  60, 1, 0, 276000, 160000,  500000, 1800000},
	{440, 1000000, 0,  30,  40, 1, 0, 276000, 160000,  500000, 1800000},
	{350,  900000, 0,  20,  30, 1, 0, 276000, 160000,  500000, 1800000},
	{266,  900000, 0,  15,  20, 1, 0, 276000, 160000,  500000, 1800000},
	{160,  900000, 0,  10,  15, 1, 0, 276000, 160000,  500000, 1800000},
};

static int mif_min_table[] = {
	100000, 133000, 167000,
	276000, 348000, 416000,
	543000, 632000, 828000,
};

static gpu_attribute gpu_config_attributes[] = {
	{GPU_MAX_CLOCK, 800},
	{GPU_MAX_CLOCK_LIMIT, 668},
	{GPU_MIN_CLOCK, 160},
	{GPU_DVFS_START_CLOCK, 160},
	{GPU_DVFS_BL_CONFIG_CLOCK, 160},
	{GPU_GOVERNOR_TYPE, G3D_DVFS_GOVERNOR_INTERACTIVE},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 160},
	{GPU_GOVERNOR_START_CLOCK_INTERACTIVE, 160},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 160},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 160},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_INTERACTIVE, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT,
		GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE,
		GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC,
		GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER,
		GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK, 420},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD, 95},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY, 0},
	{GPU_DEFAULT_VOLTAGE, 900000},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 37500},
	{GPU_TMU_CONTROL, 1},
	{GPU_TEMP_THROTTLING1, 668},
	{GPU_TEMP_THROTTLING2, 668},
	{GPU_TEMP_THROTTLING3, 668},
	{GPU_TEMP_THROTTLING4, 534},
	{GPU_TEMP_THROTTLING5, 440},
	{GPU_TEMP_THROTTLING6, 350},
	{GPU_TEMP_THROTTLING7, 266},
	{GPU_TEMP_THROTTLING8, 160},
	{GPU_TEMP_TRIPPING, 266},
	{GPU_POWER_COEFF, 46}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 1},
	{GPU_DYNAMIC_ABB, 0},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_DVS, 0},
	{GPU_PERF_GATHERING, 0},
#ifdef SEC_HWCNT
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
	{GPU_DVFS_POLLING_TIME, 30},
	{GPU_PMQOS_INT_DISABLE, 1},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
};

#define G3D_CORE_MASK_EFUSE_OFFSET	0x1DC
#define G3D_CORE_MASK_EFUSE_OFFSET2	0x1D4
#define G3D_CORE_MASK_EFUSE_OFFSET3	0x16C

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>

static int __init iram_vector_reserved_mem_setup(struct reserved_mem *rmem)
{
	pr_debug("%s: reserved-mem\n", __func__);
	return 0;
}

RESERVEDMEM_OF_DECLARE(iram_vector, "mali,iram-vector",
			iram_vector_reserved_mem_setup);
#endif

int gpu_dvfs_decide_max_clock(struct exynos_context *platform)
{
	if (!platform)
		return -1;

	return 0;
}

void *gpu_get_config_attributes(void)
{
	return &gpu_config_attributes;
}

uintptr_t gpu_get_max_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MAX_CLOCK) * 1000;
}

uintptr_t gpu_get_min_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MIN_CLOCK) * 1000;
}

struct clk *fin_pll;
struct clk *fout_g3d_pll;
struct clk *aclk_g3d;
struct clk *mout_g3d;
struct clk *dout_aclk_g3d;
static struct clk *aclk_g3d_400;
#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
struct regulator *dvs_en_regulator;
#endif /* CONFIG_REGULATOR */

int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS_PMU_G3D_STATUS) &
			LOCAL_PWR_CFG) == LOCAL_PWR_CFG) ? 1 : 0;
}

void gpu_power_set_reg(void)
{
	__raw_writel(LOCAL_PWR_CFG, EXYNOS_PMU_G3D_CONFIGURATION);

	while (1) {
		if (gpu_is_power_on())
			break;
	}
}

int gpu_power_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform =
		(struct exynos_context *) kbdev->platform_context;

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
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: clock is not initialized\n", __func__);
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
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u,
			"%s: can't set clock on in power off status\n",
			__func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 1) {
		ret = 0;
		goto err_return;
	}

	if (aclk_g3d) {
		(void) clk_prepare_enable(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u,
			"clock is enabled\n");
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
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock off in power off status\n",
			__func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 0) {
		ret = 0;
		goto err_return;
	}

	if (aclk_g3d) {
		(void)clk_disable_unprepare(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF,
			0u, 0u, "clock is disabled\n");
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
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x10464064,
			__raw_readl(EXYNOS_PMU_G3D_STATUS),
			"REG_DUMP: EXYNOS_PMU_G3D_STATUS %x\n",
			__raw_readl(EXYNOS_PMU_G3D_STATUS));

		/* G3D PLL */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460000,
			__raw_readl(EXYNOS7580_G3D_PLL_LOCK),
			"REG_DUMP: EXYNOS7580_G3D_PLL_LOCK %x\n",
			__raw_readl(EXYNOS7580_G3D_PLL_LOCK));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460100,
			__raw_readl(EXYNOS7580_G3D_PLL_CON0),
			"REG_DUMP: EXYNOS7580_G3D_PLL_CON0 %x\n",
			__raw_readl(EXYNOS7580_G3D_PLL_CON0));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460104,
			__raw_readl(EXYNOS7580_G3D_PLL_CON1),
			"REG_DUMP: EXYNOS7580_G3D_PLL_CON1 %x\n",
			__raw_readl(EXYNOS7580_G3D_PLL_CON1));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x1146010C,
			__raw_readl(EXYNOS7580_G3D_PLL_FREQ_DET),
			"REG_DUMP: EXYNOS7580_G3D_PLL_FREQ_DET %x\n",
			__raw_readl(EXYNOS7580_G3D_PLL_FREQ_DET));

		/* G3D SRC */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460200,
			__raw_readl(EXYNOS7580_MUX_SEL_G3D0),
			"REG_DUMP: EXYNOS7580_SRC_SEL_G3D %x\n",
			__raw_readl(EXYNOS7580_MUX_SEL_G3D0));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460300,
			__raw_readl(EXYNOS7580_MUX_EN_G3D0),
			"REG_DUMP: EXYNOS7580_SRC_ENABLE_G3D %x\n",
			__raw_readl(EXYNOS7580_MUX_EN_G3D0));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460400,
			__raw_readl(EXYNOS7580_MUX_STAT_G3D0),
			"REG_DUMP: EXYNOS7580_SRC_STAT_G3D %x\n",
			__raw_readl(EXYNOS7580_MUX_STAT_G3D0));

		/* G3D DIV */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460600,
			__raw_readl(EXYNOS7580_DIV_G3D),
			"REG_DUMP: EXYNOS7580_DIV_G3D %x\n",
			__raw_readl(EXYNOS7580_DIV_G3D));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460700,
			__raw_readl(EXYNOS7580_DIV_STAT_G3D),
			"REG_DUMP: EXYNOS7580_DIV_STAT_G3D %x\n",
			__raw_readl(EXYNOS7580_DIV_STAT_G3D));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x14AA0700,
			__raw_readl(EXYNOS7580_DIV_STAT_G3D),
			"REG_DUMP: EXYNOS7580_DIV_STAT_G3D %x\n",
			__raw_readl(EXYNOS7580_DIV_STAT_G3D));

		/* G3D ENABLE */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460800,
			__raw_readl(EXYNOS7580_EN_ACLK_G3D),
			"REG_DUMP: EXYNOS7580_ENABLE_IP_G3D %x\n",
			__raw_readl(EXYNOS7580_EN_ACLK_G3D));
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
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock in the power-off state!\n",
			__func__);
		goto err;
	}

	if (!gpu_is_clock_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock in the clock-off state!\n",
			__func__);
		goto err;
	}

	g3d_rate_prev = clk_get_rate(aclk_g3d);

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {

		if (aclk_g3d_400) {
			(void) clk_prepare_enable(aclk_g3d_400);
			GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u,
				"clock is enabled\n");
		}

		ret = clk_set_parent(mout_g3d, aclk_g3d_400);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_parent [aclk_g3d_400]\n",
				__func__);
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(fout_g3d_pll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_rate"
				"[fout_g3d_pll]\n", __func__);
			goto err;
		}

		ret = clk_set_parent(mout_g3d, fout_g3d_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_parent [fout_g3d_pll]\n",
				__func__);
			goto err;
		}

		if (aclk_g3d_400) {
			(void)clk_disable_unprepare(aclk_g3d_400);
			GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF,
				0u, 0u, "clock is disabled\n");
		}

		g3d_rate_prev = g3d_rate;
	}

	platform->cur_clock = gpu_get_cur_clock(platform);

	if (platform->cur_clock != clk_get_rate(fout_g3d_pll)/MHZ)
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"clock value is wrong (aclk_g3d: %d,"
			" fout_g3d_pll: %d)\n",
			platform->cur_clock,
			(int) clk_get_rate(fout_g3d_pll)/MHZ);

	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, g3d_rate/MHZ,
		"clock set: %ld\n", g3d_rate/MHZ);
	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, platform->cur_clock,
		"clock get: %d\n", platform->cur_clock);
err:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}
static int gpu_get_clock(struct kbase_device *kbdev)
{
	struct exynos_context *platform =
		(struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	fin_pll = clk_get(kbdev->dev, "fin_pll");
	if (IS_ERR(fin_pll) || (fin_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [fin_pll]\n",
			__func__);
		return -1;
	}

	fout_g3d_pll = clk_get(kbdev->dev, "fout_g3d_pll");
	if (IS_ERR(fout_g3d_pll) || (fout_g3d_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [fout_g3d_pll]\n",
			__func__);
		clk_put(fin_pll);
		return -1;
	}

	aclk_g3d = clk_get(kbdev->dev, "aclk_g3d");
	if ((IS_ERR(aclk_g3d)) || (aclk_g3d == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [aclk_g3d]\n",
			__func__);
		clk_put(fout_g3d_pll);
		clk_put(fin_pll);
		return -1;
	}

	dout_aclk_g3d = clk_get(kbdev->dev, "dout_aclk_g3d");
	if (IS_ERR(dout_aclk_g3d) || (dout_aclk_g3d == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [dout_aclk_g3d]\n",
			__func__);
		clk_put(aclk_g3d);
		clk_put(fout_g3d_pll);
		clk_put(fin_pll);
		return -1;
	}

	mout_g3d = clk_get(kbdev->dev, "mout_g3d");
	if (IS_ERR(mout_g3d) || (mout_g3d == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [mout_g3d]\n",
			__func__);
		clk_put(dout_aclk_g3d);
		clk_put(aclk_g3d);
		clk_put(fout_g3d_pll);
		clk_put(fin_pll);
		return -1;
	}

	aclk_g3d_400 = clk_get(kbdev->dev, "aclk_g3d_400");
	if (IS_ERR(aclk_g3d_400) || (aclk_g3d_400 == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [aclk_g3d_400]\n",
			__func__);
		clk_put(mout_g3d);
		clk_put(dout_aclk_g3d);
		clk_put(aclk_g3d);
		clk_put(fout_g3d_pll);
		clk_put(fin_pll);
		return -1;
	}

	gpu_clock_on(platform);

	__raw_writel(0x1, EXYNOS7580_MUX_SEL_G3D0);

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
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: regulator is not initialized\n",
			__func__);
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
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set voltage in the power-off state!\n",
			__func__);
		return -1;
	}

#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_set_voltage(g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to set voltage, voltage: %d\n", __func__,
			vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	platform->cur_voltage = gpu_get_cur_voltage(platform);

	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, vol, platform->cur_voltage,
		"voltage set: %d, voltage get:%d\n",
		vol, platform->cur_voltage);

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
	.set_clock_to_osc = NULL,
	.set_clock = gpu_set_clock,
	.set_clock_pre = NULL,
	.set_clock_post = NULL,
	.enable_clock = gpu_clock_on,
	.disable_clock = gpu_clock_off,
};

struct gpu_control_ops *gpu_get_control_ops(void)
{
	return &ctr_ops;
}

#ifdef CONFIG_REGULATOR
int gpu_enable_dvs(struct exynos_context *platform) {
	return 0;
}

int gpu_disable_dvs(struct exynos_context *platform) {
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;

	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(g3d_regulator) || (g3d_regulator == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to get vdd_g3d regulator, 0x%p\n", __func__,
			g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	gpu_voltage = get_match_volt(ID_G3D,
			platform->gpu_dvfs_config_clock*1000);

	if (gpu_voltage == 0)
		gpu_voltage = platform->gpu_default_vol;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY,
			0u, 0u, "%s: failed to set voltage [%d]\n", __func__,
			gpu_voltage);
		regulator_put(g3d_regulator);
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
