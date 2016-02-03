/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/smc.h>

#include <plat/cpu.h>
#include <mach/bts.h>

#include "s5p_mfc_common.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_reg.h"
#include "s5p_mfc_ctrl.h"

#define CLK_DEBUG


#if defined(CONFIG_ARCH_EXYNOS4)

#define MFC_PARENT_CLK_NAME	"mout_mfc0"
#define MFC_CLKNAME		"sclk_mfc"
#define MFC_GATE_CLK_NAME	"mfc"

int s5p_mfc_init_pm(struct s5p_mfc_dev *dev)
{
	struct clk *parent, *sclk;
	int ret = 0;

	parent = clk_get(dev->device, "mfc");
	if (IS_ERR(parent)) {
		printk(KERN_ERR "failed to get parent clock\n");
		ret = -ENOENT;
		goto err_p_clk;
	}

	ret = clk_prepare(parent);
	if (ret) {
		printk(KERN_ERR "clk_prepare() failed\n");
		return ret;
	}

	atomic_set(&dev->pm.power, 0);
	atomic_set(&dev->clk_ref, 0);

	dev->pm.device = dev->device;
	pm_runtime_enable(dev->pm.device);

	return 0;

err_g_clk:
	clk_put(sclk);
err_s_clk:
	clk_put(parent);
err_p_clk:
	return ret;
}

#elif defined(CONFIG_ARCH_EXYNOS5) || defined(CONFIG_ARCH_EXYNOS7)

#define MFC_PARENT_CLK_NAME	"aclk_333"
#define MFC_CLKNAME		"sclk_mfc"
#define MFC_GATE_CLK_NAME	"mfc"

int s5p_mfc_init_pm(struct s5p_mfc_dev *dev)
{
	int ret = 0;

#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0)
		dev->pm.clock = clk_get(dev->device, "gate_mfc0");
	else if (dev->id == 1)
		dev->pm.clock = clk_get(dev->device, "gate_mfc1");
#elif defined(CONFIG_SOC_EXYNOS5422)
	dev->pm.clock = clk_get(dev->device, "mfc");
#elif defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420)
	dev->pm.clock = clk_get(dev->device, "aclk_mfc");
#endif

	if (IS_ERR(dev->pm.clock)) {
		printk(KERN_ERR "failed to get parent clock\n");
		ret = -ENOENT;
		goto err_p_clk;
	}

	ret = clk_prepare(dev->pm.clock);
	if (ret) {
		printk(KERN_ERR "clk_prepare() failed\n");
		return ret;
	}

	spin_lock_init(&dev->pm.clklock);
	atomic_set(&dev->pm.power, 0);
	atomic_set(&dev->clk_ref, 0);

	dev->pm.device = dev->device;
	dev->pm.clock_off_steps = 0;
	pm_runtime_enable(dev->pm.device);

	clk_put(dev->pm.clock);

	return 0;

err_p_clk:
	clk_put(dev->pm.clock);

	return ret;
}

int s5p_mfc_set_clock_parent(struct s5p_mfc_dev *dev)
{
	struct clk *clk_child = NULL;
#ifndef CONFIG_SOC_EXYNOS7420
	struct clk *clk_parent = NULL;
#endif
#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0) {
		clk_child = clk_get(dev->device, "mout_aclk_mfc0_333_user");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
		clk_parent = clk_get(dev->device, "aclk_mfc0_333");
		if (IS_ERR(clk_parent)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}
		clk_set_parent(clk_child, clk_parent);
	} else if (dev->id == 1) {
		clk_child = clk_get(dev->device, "mout_aclk_mfc1_333_user");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
		clk_parent = clk_get(dev->device, "aclk_mfc1_333");
		if (IS_ERR(clk_parent)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}
		clk_set_parent(clk_child, clk_parent);
	}
#elif defined(CONFIG_SOC_EXYNOS5422)
	clk_child = clk_get(dev->device, "mout_aclk_333_user");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
	clk_parent = clk_get(dev->device, "mout_aclk_333_sw");
	if (IS_ERR(clk_parent)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
		return PTR_ERR(clk_parent);
	}
	clk_set_parent(clk_child, clk_parent);
#elif defined(CONFIG_SOC_EXYNOS5433)
	clk_child = clk_get(dev->device, "mout_aclk_mfc_400_user");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
	clk_parent = clk_get(dev->device, "aclk_mfc_400");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
		return PTR_ERR(clk_parent);
	}
	/* before set mux register, all source clock have to enabled */
	clk_prepare_enable(clk_parent);
	if (clk_set_parent(clk_child, clk_parent)) {
		pr_err("Unable to set parent %s of clock %s \n",
			__clk_get_name(clk_parent), __clk_get_name(clk_child));
	}
	/* expected mfc related ref clock value be set above 1 */
	clk_put(clk_child);
	clk_put(clk_parent);

#elif defined(CONFIG_SOC_EXYNOS7420)
	unsigned long index;
	char *str_child[] = {"aclk_lh_s_mfc_0", "aclk_lh_s_mfc_1",
			"pclk_mfc", "aclk_lh_mfc0", "aclk_lh_mfc1",
			"aclk_noc_bus1_nrt", "pclk_gpio_bus1"};
	for (index = 0; index < (sizeof(str_child)/sizeof(char *)); index++) {
		clk_child = clk_get(dev->device, str_child[index]);
		if (IS_ERR_OR_NULL(clk_child)) {
			pr_err("failed to get %s clock\n", str_child[index]);
			return PTR_ERR(clk_child);
		}
		clk_prepare_enable(clk_child);
		clk_put(clk_child);
	}
#elif defined(CONFIG_SOC_EXYNOS7580)
	clk_child = clk_get(dev->device, "mout_aclk_mfc_266_user");
	if (IS_ERR(clk_child)) {
		dev_err(dev->device, "failed to get clk_child\n");
		return PTR_ERR(clk_child);
	}
	clk_parent = clk_get(dev->device, "dout_aclk_mfcmscl_266");
	if (IS_ERR(clk_parent)) {
		dev_err(dev->device, "failed to get clk_parent\n");
		clk_put(clk_child);
		return PTR_ERR(clk_parent);
	}
	/* Set defined clk rate and re-parent */
	if (clk_set_rate(clk_parent, dev->pdata->clock_rate))
		dev_warn(dev->device, "failed to set default clk rate\n");
	clk_set_parent(clk_child, clk_parent);
#endif


	return 0;
}

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ

/* int_div_lock is only needed for EXYNOS5410 */
#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
extern spinlock_t int_div_lock;
#endif

static int s5p_mfc_clock_set_rate(struct s5p_mfc_dev *dev, unsigned long rate)
{
	struct clk *clk_child = NULL;

#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0) {
		clk_child = clk_get(dev->device, "dout_aclk_mfc0_333");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}

	} else if (dev->id == 1) {
		clk_child = clk_get(dev->device, "dout_aclk_mfc1_333");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
	}
#elif defined(CONFIG_SOC_EXYNOS5422)
	clk_child = clk_get(dev->device, "dout_aclk_333");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
#elif defined(CONFIG_SOC_EXYNOS5433)
	clk_child = clk_get(dev->device, "dout_aclk_mfc_400");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
#elif defined(CONFIG_SOC_EXYNOS7420)
	/* Do not set clock rate */
	return 0;

	/*
	clk_child = clk_get(dev->device, "dout_aclk_mfc_532");
	if (IS_ERR_OR_NULL(clk_child)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
	*/
#endif

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	spin_lock(&int_div_lock);
#endif
	if(clk_child)
		clk_set_rate(clk_child, rate * 1000);

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	spin_unlock(&int_div_lock);
#endif

	if(clk_child)
		clk_put(clk_child);

	return 0;
}
#endif
#endif

void s5p_mfc_final_pm(struct s5p_mfc_dev *dev)
{
	clk_put(dev->pm.clock);

	pm_runtime_disable(dev->pm.device);
}

int s5p_mfc_clock_on(struct s5p_mfc_dev *dev)
{
	int ret = 0;
	int state, val;
	unsigned long flags;

	dev->pm.clock_on_steps = 1;
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	MFC_TRACE_DEV("++ clock_on: Set clock rate(%d)\n", dev->curr_rate);
	mutex_lock(&dev->curr_rate_lock);
	s5p_mfc_clock_set_rate(dev, dev->curr_rate);
	mutex_unlock(&dev->curr_rate_lock);
#endif
	dev->pm.clock_on_steps = 2;
	ret = clk_enable(dev->pm.clock);
	if (ret < 0)
		return ret;

	dev->pm.clock_on_steps = 3;
	if (dev->pm.base_type != MFCBUF_INVALID)
		s5p_mfc_init_memctrl(dev, dev->pm.base_type);

	if (dev->curr_ctx_drm && dev->is_support_smc) {
		spin_lock_irqsave(&dev->pm.clklock, flags);
		mfc_debug(3, "Begin: enable protection\n");
		ret = exynos_smc(SMC_PROTECTION_SET, 0,
					dev->id, SMC_PROTECTION_ENABLE);
		if (!ret) {
			printk("Protection Enable failed! ret(%u)\n", ret);
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
			clk_disable(dev->pm.clock);
			return -EACCES;
		}
		mfc_debug(3, "End: enable protection\n");
		spin_unlock_irqrestore(&dev->pm.clklock, flags);
	} else {
		ret = s5p_mfc_mem_resume(dev->alloc_ctx[0]);
		if (ret < 0) {
			dev->pm.clock_on_steps = 4;
			clk_disable(dev->pm.clock);
			return ret;
		}
	}

	dev->pm.clock_on_steps = 5;
	if (IS_MFCV6(dev)) {
		if ((!dev->wakeup_status) && (dev->sys_init_status)) {
			spin_lock_irqsave(&dev->pm.clklock, flags);
			if ((atomic_inc_return(&dev->clk_ref) == 1) &&
					FW_HAS_BUS_RESET(dev)) {
				val = s5p_mfc_read_reg(dev, S5P_FIMV_MFC_BUS_RESET_CTRL);
				val &= ~(0x1);
				s5p_mfc_write_reg(dev, val, S5P_FIMV_MFC_BUS_RESET_CTRL);
			}
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
		} else {
			atomic_inc_return(&dev->clk_ref);
		}
	} else {
		atomic_inc_return(&dev->clk_ref);
	}

	dev->pm.clock_on_steps = 6;
	state = atomic_read(&dev->clk_ref);
	mfc_debug(2, "+ %d\n", state);
	MFC_TRACE_DEV("-- clock_on : ref state(%d)\n", state);

	return 0;
}

/* Use only in functions that first instance is guaranteed, like mfc_init_hw() */
int s5p_mfc_clock_on_with_base(struct s5p_mfc_dev *dev,
				enum mfc_buf_usage_type buf_type)
{
	int ret;
	dev->pm.base_type = buf_type;
	ret = s5p_mfc_clock_on(dev);
	dev->pm.base_type = MFCBUF_INVALID;

	return ret;
}

void s5p_mfc_clock_off(struct s5p_mfc_dev *dev)
{
	int state, val;
	unsigned long timeout, flags;
	int ret = 0;

	dev->pm.clock_off_steps = 1;

	MFC_TRACE_DEV("++ clock_off\n");
	if (IS_MFCV6(dev)) {
		spin_lock_irqsave(&dev->pm.clklock, flags);
		dev->pm.clock_off_steps = 2;
		if ((atomic_dec_return(&dev->clk_ref) == 0) &&
				FW_HAS_BUS_RESET(dev)) {
			s5p_mfc_write_reg(dev, 0x1, S5P_FIMV_MFC_BUS_RESET_CTRL);

			timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);
			/* Check bus status */
			do {
				if (time_after(jiffies, timeout)) {
					mfc_err_dev("Timeout while resetting MFC.\n");
					break;
				}
				val = s5p_mfc_read_reg(dev,
						S5P_FIMV_MFC_BUS_RESET_CTRL);
			} while ((val & 0x2) == 0);
		dev->pm.clock_off_steps = 3;
		}
		spin_unlock_irqrestore(&dev->pm.clklock, flags);
	} else {
		atomic_dec_return(&dev->clk_ref);
	}

	dev->pm.clock_off_steps = 4;
	state = atomic_read(&dev->clk_ref);
	if (state < 0) {
		mfc_err_dev("Clock state is wrong(%d)\n", state);
		atomic_set(&dev->clk_ref, 0);
		dev->pm.clock_off_steps = 5;
	} else {
		if (dev->curr_ctx_drm && dev->is_support_smc) {
			mfc_debug(3, "Begin: disable protection\n");
			spin_lock_irqsave(&dev->pm.clklock, flags);
			dev->pm.clock_off_steps = 6;
			ret = exynos_smc(SMC_PROTECTION_SET, 0,
					dev->id, SMC_PROTECTION_DISABLE);
			if (!ret) {
				printk("Protection Disable failed! ret(%u)\n", ret);
				spin_unlock_irqrestore(&dev->pm.clklock, flags);
				clk_disable(dev->pm.clock);
				return;
			}
			mfc_debug(3, "End: disable protection\n");
			dev->pm.clock_off_steps = 7;
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
		} else {
			dev->pm.clock_off_steps = 8;
			s5p_mfc_mem_suspend(dev->alloc_ctx[0]);
			dev->pm.clock_off_steps = 9;
		}
		dev->pm.clock_off_steps = 10;
		clk_disable(dev->pm.clock);
	}
	mfc_debug(2, "- %d\n", state);
	MFC_TRACE_DEV("-- clock_off: ref state(%d)\n", state);
	dev->pm.clock_off_steps = 11;
}

int s5p_mfc_power_on(struct s5p_mfc_dev *dev)
{
	int ret;
#if defined(CONFIG_SOC_EXYNOS5430)
	struct clk *clk_child = NULL;
	struct clk *clk_parent = NULL;
#endif

	atomic_set(&dev->pm.power, 1);
	MFC_TRACE_DEV("++ Power on\n");

	ret = pm_runtime_get_sync(dev->pm.device);

#if defined(CONFIG_SOC_EXYNOS5422)
	bts_initialize("pd-mfc", true);
#endif

#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0) {
		clk_child = clk_get(dev->device, "mout_mphy_pll");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
		clk_parent = clk_get(dev->device, "fout_mphy_pll");
		if (IS_ERR(clk_parent)) {
			clk_put(clk_child);
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}
		/* 1. Enable MPHY_PLL */
		clk_prepare_enable(clk_parent);
		/* 2. Set parent as Fout_mphy */
		clk_set_parent(clk_child, clk_parent);
	}
#endif

	MFC_TRACE_DEV("-- Power on: ret(%d)\n", ret);

	return ret;
}

int s5p_mfc_power_off(struct s5p_mfc_dev *dev)
{
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	struct clk *clk_child = NULL;
	struct clk *clk_parent = NULL;
#endif

#if defined(CONFIG_SOC_EXYNOS5430)
	struct clk *clk_fout_mphy_pll = NULL;
#endif

#if defined(CONFIG_SOC_EXYNOS5433)
	struct clk *clk_old_parent = NULL;
#endif

#if defined(CONFIG_SOC_EXYNOS7420)
	struct clk *clk_child = NULL;
	unsigned long index;
	char *str_child[] = {"aclk_lh_s_mfc_0", "aclk_lh_s_mfc_1",
			"pclk_mfc", "aclk_lh_mfc0", "aclk_lh_mfc1",
			"aclk_noc_bus1_nrt", "pclk_gpio_bus1"};
#endif

	int ret;

	MFC_TRACE_DEV("++ Power off\n");
#if defined(CONFIG_SOC_EXYNOS5422)
	bts_initialize("pd-mfc", false);
#endif

#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0) {
		clk_fout_mphy_pll = clk_get(dev->device, "fout_mphy_pll");
		if (IS_ERR(clk_fout_mphy_pll)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_fout_mphy_pll));
			return PTR_ERR(clk_fout_mphy_pll);
		}

		clk_child = clk_get(dev->device, "mout_mphy_pll");
		if (IS_ERR(clk_child)) {
			clk_put(clk_fout_mphy_pll);
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
		clk_parent = clk_get(dev->device, "fin_pll");
		if (IS_ERR(clk_parent)) {
			clk_put(clk_child);
			clk_put(clk_fout_mphy_pll);
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}
		/* 1. Set parent as OSC */
		clk_set_parent(clk_child, clk_parent);
		/* 2. Disable MPHY_PLL */
		clk_disable_unprepare(clk_fout_mphy_pll);
	}
#elif defined(CONFIG_SOC_EXYNOS5433)
	clk_old_parent = clk_get(dev->device, "aclk_mfc_400");
	if (IS_ERR(clk_old_parent)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_old_parent));
		return PTR_ERR(clk_old_parent);
	}
	clk_child = clk_get(dev->device, "mout_aclk_mfc_400_user");
	if (IS_ERR(clk_child)) {
		clk_put(clk_old_parent);
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
	clk_parent = clk_get(dev->device, "oscclk");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_old_parent);
		pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
		return PTR_ERR(clk_parent);
	}
	/* before set mux register, all source clock have to enabled */
	clk_prepare_enable(clk_parent);
	if (clk_set_parent(clk_child, clk_parent)) {
		pr_err("Unable to set parent %s of clock %s \n",
			__clk_get_name(clk_parent), __clk_get_name(clk_child));
	}
	clk_disable_unprepare(clk_parent);
	clk_disable_unprepare(clk_old_parent);

	clk_put(clk_child);
	clk_put(clk_parent);
	clk_put(clk_old_parent);
	/* expected mfc related ref clock value be set 0 */
#elif defined(CONFIG_SOC_EXYNOS7420)
	for (index = 0; index < (sizeof(str_child)/sizeof(char *)); index++) {
		clk_child = clk_get(dev->device, str_child[index]);
	if (IS_ERR_OR_NULL(clk_child)) {
			pr_err("failed to get %s clock\n", str_child[index]);
			return PTR_ERR(clk_child);
		}
		clk_disable_unprepare(clk_child);
		clk_put(clk_child);
	}
#endif

	atomic_set(&dev->pm.power, 0);

	ret = pm_runtime_put_sync(dev->pm.device);
	MFC_TRACE_DEV("-- Power off: ret(%d)\n", ret);

	return ret;
}

int s5p_mfc_get_power_ref_cnt(struct s5p_mfc_dev *dev)
{
	return atomic_read(&dev->pm.power);
}

int s5p_mfc_get_clk_ref_cnt(struct s5p_mfc_dev *dev)
{
	return atomic_read(&dev->clk_ref);
}
