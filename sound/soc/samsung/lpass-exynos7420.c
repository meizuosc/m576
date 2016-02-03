/* sound/soc/samsung/lpass-exynos7420.c
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
#include <mach/regs-clock-exynos7420.h>

#ifdef CONFIG_MACH_ESPRESSO7420
#include "../../../drivers/pinctrl/core.h"
#endif

#include "lpass.h"

/* Default ACLK gate for
   aclk_dmac, aclk_sramc */
#define INIT_ACLK_GATE_MASK	(1 << 31 | 1 << 30)

/* Default PCLK gate for
   pclk_wdt0, pclk_wdt1, pclk_slimbus,
   pclk_pcm, pclk_i2s, pclk_timer */
#define INIT_PCLK_GATE_MASK	(1 << 22 | 1 << 23 | 1 << 24 | \
				 1 << 26 | 1 << 27 | 1 << 28)

/* Default SCLK gate for
   sclk_ca5, sclk_slimbus, sclk_uart,
   sclk_i2s, sclk_pcm, sclk_slimbus_clkin */
#define INIT_SCLK_GATE_MASK	(1 << 31 | 1 << 30 | 1 << 29 | \
				 1 << 28 | 1 << 27 | 1 << 26)

static struct lpass_cmu_info {
	struct clk		*clk_fin_pll;
	struct clk		*clk_fout_aud_pll;
	struct clk		*clk_dout_sclk_aud_pll;
	struct clk		*clk_mout_aud_pll_user;
	struct clk		*clk_dout_aud_cdclk;
	struct clk		*clk_mout_sclk_i2s;
	struct clk		*clk_dout_sclk_i2s;
	struct clk		*clk_mout_aud_pll_top0;
	struct clk		*clk_mout_sclk_i2s1;
	struct clk		*clk_dout_sclk_i2s1;
	struct clk		*clk_dout_aclk_aud;
	struct clk		*clk_dout_aclk_ca5;
} lpass;

void __iomem *lpass_cmu_save[] = {
	EXYNOS7420_MUX_SEL_AUD,
	EXYNOS7420_DIV_AUD0,
	EXYNOS7420_DIV_AUD1,
	EXYNOS7420_ENABLE_ACLK_AUD,
	EXYNOS7420_ENABLE_PCLK_AUD,
	EXYNOS7420_ENABLE_SCLK_AUD,
	NULL,	/* endmark */
};

#ifdef CONFIG_MACH_ESPRESSO7420
static struct pinctrl *pinctrl_dbg;
static struct pinctrl_state *pin_state_dbg;
#endif

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

	lpass.clk_dout_sclk_aud_pll = clk_get(dev, "dout_sclk_aud_pll");
	if (IS_ERR(lpass.clk_dout_sclk_aud_pll)) {
		dev_err(dev, "dout_sclk_aud_pll clk not found\n");
		goto err2;
	}

	lpass.clk_mout_aud_pll_user = clk_get(dev, "mout_aud_pll_user");
	if (IS_ERR(lpass.clk_mout_aud_pll_user)) {
		dev_err(dev, "mout_aud_pll_user clk not found\n");
		goto err3;
	}

	lpass.clk_dout_aud_cdclk = clk_get(dev, "dout_aud_cdclk");
	if (IS_ERR(lpass.clk_dout_aud_cdclk)) {
		dev_err(dev, "dout_aud_cdclk clk not found\n");
		goto err4;
	}

	lpass.clk_mout_sclk_i2s = clk_get(dev, "mout_sclk_i2s");
	if (IS_ERR(lpass.clk_mout_sclk_i2s)) {
		dev_err(dev, "mout_sclk_i2s clk not found\n");
		goto err5;
	}

	lpass.clk_dout_sclk_i2s = clk_get(dev, "dout_sclk_i2s");
	if (IS_ERR(lpass.clk_dout_sclk_i2s)) {
		dev_err(dev, "dout_sclk_i2s clk not found\n");
		goto err6;
	}

	lpass.clk_mout_aud_pll_top0 = clk_get(dev, "mout_aud_pll_top0");
	if (IS_ERR(lpass.clk_mout_aud_pll_top0)) {
		dev_err(dev, "mout_aud_pll_top0 clk not found\n");
		goto err7;
	}

	lpass.clk_mout_sclk_i2s1 = clk_get(dev, "mout_sclk_i2s1");
	if (IS_ERR(lpass.clk_mout_sclk_i2s1)) {
		dev_err(dev, "mout_sclk_i2s1 clk not found\n");
		goto err8;
	}

	lpass.clk_dout_sclk_i2s1 = clk_get(dev, "dout_sclk_i2s1");
	if (IS_ERR(lpass.clk_dout_sclk_i2s1)) {
		dev_err(dev, "dout_sclk_i2s1 clk not found\n");
		goto err9;
	}

	lpass.clk_dout_aclk_aud = clk_get(dev, "dout_aclk_aud");
	if (IS_ERR(lpass.clk_dout_aclk_aud)) {
		dev_err(dev, "dout_aclk_aud clk not found\n");
		goto err9;
	}

	lpass.clk_dout_aclk_ca5 = clk_get(dev, "dout_aclk_ca5");
	if (IS_ERR(lpass.clk_dout_aclk_ca5)) {
		dev_err(dev, "dout_aclk_ca5 clk not found\n");
		goto err9;
	}

	clk_set_rate(lpass.clk_fout_aud_pll, 196608050);
	clk_set_parent(lpass.clk_mout_aud_pll_user, lpass.clk_fout_aud_pll);

	clk_set_rate(lpass.clk_dout_aud_cdclk, 196608050);
	clk_set_parent(lpass.clk_mout_sclk_i2s, lpass.clk_dout_aud_cdclk);
	clk_set_rate(lpass.clk_dout_sclk_i2s, 49152010);

	clk_set_rate(lpass.clk_dout_sclk_aud_pll, 196608050);
	clk_set_parent(lpass.clk_mout_sclk_i2s1, lpass.clk_mout_aud_pll_top0);
	clk_set_rate(lpass.clk_dout_sclk_i2s1, 49152010);

	clk_set_rate(lpass.clk_dout_aclk_ca5, 196608050);
	clk_set_rate(lpass.clk_dout_aclk_aud, 98304050);

#ifdef CONFIG_MACH_ESPRESSO7420
	pinctrl_dbg = devm_pinctrl_get_select(dev, "default");
	if (IS_ERR(pinctrl_dbg))
		pr_err("%s: pin ctrl err\n", __func__);

	pin_state_dbg = pinctrl_lookup_state(pinctrl_dbg, "default");
	if (IS_ERR(pin_state_dbg))
		pr_err("%s: pin state err\n", __func__);
#endif
	return 0;
err9:
	clk_put(lpass.clk_mout_sclk_i2s1);
err8:
	clk_put(lpass.clk_mout_aud_pll_top0);
err7:
	clk_put(lpass.clk_dout_sclk_i2s);
err6:
	clk_put(lpass.clk_mout_sclk_i2s);
err5:
	clk_put(lpass.clk_dout_aud_cdclk);
err4:
	clk_put(lpass.clk_mout_aud_pll_user);
err3:
	clk_put(lpass.clk_dout_sclk_aud_pll);
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
	clk_set_parent(lpass.clk_mout_sclk_i2s, lpass.clk_dout_aud_cdclk);
	clk_set_parent(lpass.clk_mout_sclk_i2s1, lpass.clk_mout_aud_pll_top0);

	/* TOP1 */
	/* clk_set_parent(lpass.clk_mout_aud_pll_user_top, lpass.clk_mout_aud_pll); */
	/*clk_set_parent(lpass.clk_mout_aud_pll_user_top, lpass.clk_dout_sclk_aud_pll);*/
}

void lpass_set_mux_osc(void)
{
#ifdef CONFIG_MACH_ESPRESSO7420
	pinctrl_dbg->state = NULL;
	if (pinctrl_select_state(pinctrl_dbg, pin_state_dbg) < 0)
		pr_err("%s: pinctrl select state err\n", __func__);
#endif
	/* TOP1 */
	/* clk_set_parent(lpass.clk_mout_aud_pll, lpass.clk_fin_pll); */
	/* clk_set_parent(lpass.clk_mout_aud_pll_user_top, lpass.clk_fin_pll);*/

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
	writel(1, EXYNOS_PMUREG(0x1340));		/* GPIO_MODE_AUD_SYS_PWR_REG */
}

void lpass_release_pad_reg(void)
{
	writel(1 << 28, EXYNOS_PMUREG(0x3028));		/* PAD_RETENTION_AUD_OPTION */
	writel(1, EXYNOS_PMUREG(0x1340));		/* GPIO_MODE_AUD_SYS_PWR_REG */
}

void lpass_init_clk_gate(void)
{
	int val;

	val = readl(EXYNOS7420_ENABLE_ACLK_AUD);
	val &= ~INIT_ACLK_GATE_MASK;
	writel(val, EXYNOS7420_ENABLE_ACLK_AUD);

	val = readl(EXYNOS7420_ENABLE_PCLK_AUD);
	val &= ~INIT_PCLK_GATE_MASK;
	writel(val, EXYNOS7420_ENABLE_PCLK_AUD);

	val = readl(EXYNOS7420_ENABLE_SCLK_AUD);
	val &= ~INIT_SCLK_GATE_MASK;
	writel(val, EXYNOS7420_ENABLE_SCLK_AUD);
}

void lpass_reset_clk_default(void)
{
	writel(0xFFFFFFFF, EXYNOS7420_ENABLE_ACLK_AUD);
	writel(0xFFFFFFFF, EXYNOS7420_ENABLE_PCLK_AUD);
	writel(0xFFFFFFFF, EXYNOS7420_ENABLE_SCLK_AUD);
}

/* Module information */
MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_LICENSE("GPL");
