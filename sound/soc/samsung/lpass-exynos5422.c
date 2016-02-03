/* sound/soc/samsung/lpass-exynos5422.c
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
#include <mach/regs-audss.h>

#include "lpass.h"

static struct lpass_cmu_info {
	struct clk		*clk_fin_pll;
	struct clk		*clk_fout_dpll;
	struct clk		*clk_mout_dpll_ctrl;
	struct clk		*clk_mout_mau_epll_clk;
	struct clk		*clk_mout_mau_epll_clk_user;
	struct clk		*clk_mout_ass_clk;
	struct clk		*clk_mout_ass_i2s;
} lpass;

void __iomem *lpass_cmu_save[] = {
	EXYNOS_CLKSRC_AUDSS,
	EXYNOS_CLKDIV_AUDSS,
	EXYNOS_CLKGATE_AUDSS,
	NULL,	/* endmark */
};

int lpass_set_clk_heirachy(struct device *dev)
{
	lpass.clk_fin_pll = clk_get(NULL, "fin_pll");
	if (IS_ERR(lpass.clk_fin_pll)) {
		dev_err(dev, "fin_pll clk not found\n");
		goto err0;
	}

	lpass.clk_fout_dpll = clk_get(NULL,"fout_dpll");
	if (IS_ERR_OR_NULL(lpass.clk_fout_dpll)) {
		dev_err(dev, "fout_dpll clk not found\n");
		goto err1;
	}

	lpass.clk_mout_dpll_ctrl = clk_get(dev,"mout_dpll_ctrl");
	if (IS_ERR_OR_NULL(lpass.clk_mout_dpll_ctrl)) {
		dev_err(dev, "mout_dpll_ctrl clk not found\n");
		goto err2;
	}

	lpass.clk_mout_mau_epll_clk = clk_get(dev,"mout_mau_epll_clk");
	if (IS_ERR_OR_NULL(lpass.clk_mout_mau_epll_clk)) {
		dev_err(dev, "mout_mau_epll_clk clk not found\n");
		goto err3;
	}

	lpass.clk_mout_mau_epll_clk_user = clk_get(dev,"mout_mau_epll_clk_user");
	if (IS_ERR_OR_NULL(lpass.clk_mout_mau_epll_clk_user)) {
		dev_err(dev, "mout_mau_epll_clk_user clk not found\n");
		goto err4;
	}

	lpass.clk_mout_ass_clk = clk_get(dev,"mout_ass_clk");
	if (IS_ERR_OR_NULL(lpass.clk_mout_ass_clk)) {
		dev_err(dev, "clk_mout_ass_clk clk not found\n");
		goto err5;
	}

	lpass.clk_mout_ass_i2s = clk_get(dev,"mout_ass_i2s");
	if (IS_ERR_OR_NULL(lpass.clk_mout_ass_i2s)) {
		dev_err(dev, "mout_ass_i2s clk not found\n");
		goto err6;
	}

	return 0;
err6:
	clk_put(lpass.clk_mout_ass_clk);
err5:
	clk_put(lpass.clk_mout_mau_epll_clk_user);
err4:
	clk_put(lpass.clk_mout_mau_epll_clk);
err3:
	clk_put(lpass.clk_mout_dpll_ctrl);
err2:
	clk_put(lpass.clk_fout_dpll);
err1:
	clk_put(lpass.clk_fin_pll);
err0:
	return -1;
}

void lpass_set_mux_pll(void)
{
	/* ASS_MUX_SEL */
	clk_set_parent(lpass.clk_mout_dpll_ctrl, lpass.clk_fout_dpll);
	clk_set_parent(lpass.clk_mout_mau_epll_clk, lpass.clk_mout_dpll_ctrl);
	clk_set_parent(lpass.clk_mout_mau_epll_clk_user, lpass.clk_mout_mau_epll_clk);
	clk_set_parent(lpass.clk_mout_ass_clk, lpass.clk_mout_mau_epll_clk_user);
	clk_set_parent(lpass.clk_mout_ass_i2s, lpass.clk_mout_ass_clk);
}

void lpass_set_mux_osc(void)
{
	/* ASS_MUX_SEL */
	clk_set_parent(lpass.clk_mout_ass_clk, lpass.clk_fin_pll);
}

void lpass_enable_pll(bool on)
{
	/* DPLL is always enabled */
}

void lpass_retention_pad_reg(void)
{
	/* N/A */
}

void lpass_release_pad_reg(void)
{
	/* N/A */
}

void lpass_init_clk_gate(void)
{
	/* N/A */
}

void lpass_reset_clk_default(void)
{
	writel(0x00000FFF, EXYNOS_CLKGATE_AUDSS);
}

/* Module information */
MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_LICENSE("GPL");
