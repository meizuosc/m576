/* sound/soc/samsung/lpass-exynos7580.c
 *
 * Low Power Audio SubSystem driver for Samsung Exynos
 *
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *	Sayanta Pattanayak <sayanta.p@samsung.com>
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

#include <mach/regs-clock-exynos7580.h>
#include "lpass.h"

#define AUD_PLL_FREQ			(393216000U)
#define AUD_MI2S_FREQ			(131072000U) /*98304000*/
#define AUD_S2801_SYS_CLK_FREQ		(24576000U)

/* Default ACLK gate for
 aclk_dmac, aclk_sramc */
#define INIT_ACLK_GATE_MASK	(1 << 0 | 1 << 1)

/* Default PCLK gate for
 pclk_pcm, pclk_i2s, pclk_uart */
#define INIT_PCLK_GATE_MASK	(1 << 0 | 1 << 1 | 1 << 2)

/* Default SCLK gate for
 bclk2,1,0,mixer sysclk, sclk_ext sclk_i2s, sclk_pcm, i2s_clk,i2scd_clk */
#define INIT_SCLK_GATE_MASK	(1 << 10 | 1 << 9 | \
				1 << 8 | 1 << 7 | 1 << 3 | \
				1 << 2 | 1 << 1 | 1 << 0)

static struct lpass_cmu_info {
	struct clk		*clk_fin_pll;
	struct clk		*clk_fout_aud_pll;
	struct clk		*clk_mout_aud_pll_user;
	struct clk		*clk_mout_sclk_mi2s;
	struct clk		*clk_mout_sclk_pcm;
	struct clk		*clk_dout_sclk_mi2s;
	struct clk		*clk_dout_sclk_mixer;
} lpass;

void __iomem *lpass_cmu_save[] = {
	EXYNOS7580_MUX_SEL_AUD0,
	EXYNOS7580_MUX_SEL_AUD1,
	EXYNOS7580_MUX_EN_AUD0,
	EXYNOS7580_MUX_EN_AUD1,
	EXYNOS7580_DIV_AUD0,
	EXYNOS7580_DIV_AUD1,
	EXYNOS7580_EN_ACLK_AUD,
	EXYNOS7580_EN_PCLK_AUD,
	EXYNOS7580_EN_SCLK_AUD,
	NULL,	/* endmark */
};

int lpass_set_clk_heirachy(struct device *dev)
{
	int ret;

	lpass.clk_fin_pll = clk_get(dev, "fin_pll");
	if (IS_ERR(lpass.clk_fin_pll)) {
		dev_err(dev, "fin_pll clk not found\n");
		goto err0;
	}

	lpass.clk_fout_aud_pll = devm_clk_get(dev, "fout_aud_pll");
	if (IS_ERR(lpass.clk_fout_aud_pll)) {
		dev_err(dev, "fout_aud_pll clk not found\n");
		goto err1;
	}

	lpass.clk_mout_aud_pll_user = devm_clk_get(dev, "mout_aud_pll_user");
	if (IS_ERR(lpass.clk_mout_aud_pll_user)) {
		dev_err(dev, "mout_aud_pll_user clk not found\n");
		goto err2;
	}

	lpass.clk_mout_sclk_mi2s = clk_get(dev, "mout_sclk_mi2s");
	if (IS_ERR(lpass.clk_mout_sclk_mi2s)) {
		dev_err(dev, "mout_sclk_mi2s clk not found\n");
		goto err3;
	}

	lpass.clk_mout_sclk_pcm = devm_clk_get(dev, "mout_sclk_pcm");
	if (IS_ERR(lpass.clk_mout_sclk_pcm)) {
		dev_err(dev, "mout_sclk_pcm clk not found\n");
		goto err4;
	}

	lpass.clk_dout_sclk_mi2s = clk_get(dev, "dout_sclk_mi2s");
	if (IS_ERR(lpass.clk_dout_sclk_mi2s)) {
		dev_err(dev, "dout_sclk_mi2s clk not found\n");
		goto err5;
	}

	lpass.clk_dout_sclk_mixer = clk_get(dev, "audmixer_dout");
	if (IS_ERR(lpass.clk_dout_sclk_mixer)) {
		dev_err(dev, "dout_sclk_mixer clk not found\n");
		goto err6;
	}

	clk_prepare_enable(lpass.clk_mout_aud_pll_user);
	clk_set_rate(lpass.clk_fout_aud_pll, AUD_PLL_FREQ);

	ret = clk_set_parent(lpass.clk_mout_sclk_mi2s,
			lpass.clk_mout_aud_pll_user);
	if (ret) {
		dev_err(dev, "error in setting parent of mout_sclk_mi2s clk\n");
		goto err6;
	}

	clk_set_parent(lpass.clk_mout_sclk_pcm, lpass.clk_mout_aud_pll_user);
	if (ret) {
		dev_err(dev, "error in setting parent of mout_sclk_pcm clk\n");
		goto err6;
	}

	clk_set_rate(lpass.clk_dout_sclk_mi2s, AUD_MI2S_FREQ);
	clk_set_rate(lpass.clk_dout_sclk_mixer, AUD_S2801_SYS_CLK_FREQ);

	return 0;

err6:
	clk_put(lpass.clk_dout_sclk_mi2s);
err5:
	clk_put(lpass.clk_mout_sclk_pcm);
err4:
	clk_put(lpass.clk_mout_sclk_mi2s);
err3:
	clk_put(lpass.clk_mout_aud_pll_user);
err2:
	clk_put(lpass.clk_fout_aud_pll);
err1:
	clk_put(lpass.clk_fin_pll);
err0:
	return -1;
}

/**
 * AUD_PLL_USER Mux is defined as USERMUX. Enabling the USERMUX selects
 * the underlying PLL as the parent of this MUX and disabling sets the
 * oscillator clock as the parent of this clock.
 */
void lpass_set_mux_pll(void)
{
	clk_prepare_enable(lpass.clk_mout_aud_pll_user);
}

void lpass_set_mux_osc(void)
{
	clk_disable_unprepare(lpass.clk_mout_aud_pll_user);
}

void lpass_enable_pll(bool on)
{
	if (on) {
		clk_prepare_enable(lpass.clk_fout_aud_pll);
		clk_prepare_enable(lpass.clk_mout_aud_pll_user);
	} else {
		clk_disable_unprepare(lpass.clk_fout_aud_pll);
		clk_disable_unprepare(lpass.clk_mout_aud_pll_user);
	}
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
	writel(0xFFFFFFFF, EXYNOS7580_EN_ACLK_AUD);
	writel(0xFFFFFFFF, EXYNOS7580_EN_PCLK_AUD);
	writel(0xFFFFFFFF, EXYNOS7580_EN_SCLK_AUD);
}

/* Module information */
MODULE_AUTHOR("Sayanta Pattanayak, <sayanta.p@samsung.com>");
MODULE_LICENSE("GPL");
