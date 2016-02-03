/* sound/soc/samsung/lpass-exynos5430.c
 *
 * Low Power Audio SubSystem driver for Samsung Exynos
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/exynos.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock-exynos5430.h>

#include "lpass.h"

/* Default clk gate for slimbus, pcm, i2s, pclk_dbg, ca5 */
#define INIT_CLK_GATE_MASK	(1 << 11 | 1 << 10 | 1 << 8 | 1 << 7 | \
				 1 <<  6 | 1 <<  5 | 1 << 1 | 1 << 0)

static struct lpass_cmu_info {
	struct clk		*clk_fin_pll;
	struct clk		*clk_fout_aud_pll;
	struct clk		*clk_mout_aud_pll;
	struct clk		*clk_mout_aud_pll_user_top;
	struct clk		*clk_mout_aud_pll_user;
} lpass;

void __iomem *lpass_cmu_save[] = {
	EXYNOS5430_SRC_ENABLE_AUD0,
	EXYNOS5430_SRC_ENABLE_AUD1,
	EXYNOS5430_DIV_AUD0,
	EXYNOS5430_DIV_AUD1,
	EXYNOS5430_ENABLE_IP_AUD0,
	EXYNOS5430_ENABLE_IP_AUD1,
	NULL,	/* endmark */
};

int lpass_set_clk_heirachy(struct device *dev)
{
	lpass.clk_fin_pll = clk_get(dev, "fin_pll");
	if (IS_ERR(lpass.clk_fin_pll)) {
		dev_err(dev, "fin_pll clk not found\n");
		goto err0;
	}

	lpass.clk_fout_aud_pll = clk_get(dev, "fout_aud_pll");
	if (IS_ERR(lpass.clk_fout_aud_pll)) {
		dev_err(dev, "fout_aud_pll clk not found\n");
		goto err1;
	}

	lpass.clk_mout_aud_pll = clk_get(dev, "mout_aud_pll");
	if (IS_ERR(lpass.clk_mout_aud_pll)) {
		dev_err(dev, "mout_aud_pll clk not found\n");
		goto err2;
	}

	lpass.clk_mout_aud_pll_user_top = clk_get(dev, "mout_aud_pll_user_top");
	if (IS_ERR(lpass.clk_mout_aud_pll_user_top)) {
		dev_err(dev, "mout_aud_pll_user_top clk not found\n");
		goto err3;
	}

	lpass.clk_mout_aud_pll_user = clk_get(dev, "mout_aud_pll_user");
	if (IS_ERR(lpass.clk_mout_aud_pll_user)) {
		dev_err(dev, "mout_aud_pll_user clk not found\n");
		goto err4;
	}

	clk_set_parent(lpass.clk_mout_aud_pll, lpass.clk_fout_aud_pll);
	clk_set_parent(lpass.clk_mout_aud_pll_user, lpass.clk_fout_aud_pll);

	return 0;
err4:
	clk_put(lpass.clk_mout_aud_pll_user_top);
err3:
	clk_put(lpass.clk_mout_aud_pll);
err2:
	clk_put(lpass.clk_fout_aud_pll);
err1:
	clk_put(lpass.clk_fin_pll);
err0:
	return -1;
}

void lpass_set_mux_pll(void)
{
	/* AUD0 */
	clk_set_parent(lpass.clk_mout_aud_pll_user, lpass.clk_fout_aud_pll);

	/* TOP1 */
	clk_set_parent(lpass.clk_mout_aud_pll, lpass.clk_fout_aud_pll);
	clk_set_parent(lpass.clk_mout_aud_pll_user_top, lpass.clk_mout_aud_pll);
}

void lpass_set_mux_osc(void)
{
	/* TOP1 */
	clk_set_parent(lpass.clk_mout_aud_pll, lpass.clk_fin_pll);
	clk_set_parent(lpass.clk_mout_aud_pll_user_top, lpass.clk_fin_pll);

	/* AUD0 */
	clk_set_parent(lpass.clk_mout_aud_pll_user, lpass.clk_fin_pll);
}

void lpass_enable_pll(bool on)
{
	if (on)
		clk_prepare_enable(lpass.clk_fout_aud_pll);
	else
		clk_disable_unprepare(lpass.clk_fout_aud_pll);
}

void lpass_retention_pad_reg(void)
{
	writel(1, EXYNOS5430_GPIO_MODE_AUD_SYS_PWR_REG);
}

void lpass_release_pad_reg(void)
{
	writel(1 << 28, EXYNOS_PAD_RET_MAUDIO_OPTION);
	writel(1, EXYNOS5430_GPIO_MODE_AUD_SYS_PWR_REG);
}

void lpass_init_clk_gate(void)
{
	u32 val;

	val = readl(EXYNOS5430_ENABLE_IP_AUD0);
	val &= ~INIT_CLK_GATE_MASK;
	writel(val, EXYNOS5430_ENABLE_IP_AUD0);
}

void lpass_reset_clk_default(void)
{
	writel(0x000007FF, EXYNOS5430_ENABLE_PCLK_AUD);
	writel(0x00000003, EXYNOS5430_ENABLE_SCLK_AUD0);
	writel(0x0000003F, EXYNOS5430_ENABLE_SCLK_AUD1);
	writel(0x00003FFF, EXYNOS5430_ENABLE_IP_AUD0);
	writel(0x0000003F, EXYNOS5430_ENABLE_IP_AUD1);
}

/* Module information */
MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_LICENSE("GPL");
