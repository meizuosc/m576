/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Copyright (c) 2013 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains the utility functions to register the pll clocks.
*/

#include <linux/errno.h>
#include "clk.h"
#include "clk-pll.h"
#include <linux/clk-private.h>

#define to_clk_pll(_hw) container_of(_hw, struct samsung_clk_pll, hw)

static const struct samsung_pll_rate_table *samsung_get_pll_settings(
				struct samsung_clk_pll *pll, unsigned long rate)
{
	const struct samsung_pll_rate_table  *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
		if (rate == rate_table[i].rate)
			return &rate_table[i];
	}

	return NULL;
}

static long samsung_pll_round_rate(struct clk_hw *hw,
			unsigned long drate, unsigned long *prate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	const struct samsung_pll_rate_table *rate_table = pll->rate_table;
	int i;

	/* Assumming rate_table is in descending order */
	for (i = 0; i < pll->rate_count; i++) {
		if (drate >= rate_table[i].rate)
			return rate_table[i].rate;
	}

	/* return minimum supported value */
	return rate_table[i - 1].rate;
}

static int samsung_pll_clk_enable(struct clk_hw *hw)
{
	u32 tmp, pll_con0;
	struct samsung_clk_pll *pll = to_clk_pll(hw);

	pll_con0 = __raw_readl(pll->con_reg);

	/* PLL enable */
	pll_con0 |= (1 << pll->bit_enable);
	__raw_writel(pll_con0, pll->con_reg);

	/* wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(pll->con_reg);
	} while (!(tmp & (1 << pll->bit_lockstat)));

	return 0;
}

static void samsung_pll_clk_disable(struct clk_hw *hw)
{
	u32 pll_con0;
	struct samsung_clk_pll *pll = to_clk_pll(hw);

	pll_con0 = __raw_readl(pll->con_reg);

	/* PLL enable */
	pll_con0 &= ~(1 << pll->bit_enable);
	__raw_writel(pll_con0, pll->con_reg);
}

static int samsung_pll_clk_is_enabled(struct clk_hw *hw)
{
	u32 pll_con0;
	struct samsung_clk_pll *pll = to_clk_pll(hw);

	pll_con0 = __raw_readl(pll->con_reg);

	return (pll_con0 & (1 << pll->bit_enable)) ? true : false;
}

/*
 * PLL35xx Clock Type
 */

/* Maximum lock time can be 270 * PDIV cycles */
#define PLL35XX_LOCK_FACTOR	(270)

#define PLL35XX_MDIV_MASK       (0x3FF)
#define PLL35XX_PDIV_MASK       (0x3F)
#define PLL35XX_SDIV_MASK       (0x7)
#define PLL35XX_LOCK_STAT_MASK  (0x1)
#define PLL35XX_MDIV_SHIFT      (16)
#define PLL35XX_PDIV_SHIFT      (8)
#define PLL35XX_SDIV_SHIFT      (0)
#define PLL35XX_LOCK_STAT_SHIFT	(29)
#define PLL35XX_PLL_ENABLE_SHIFT	(31)

static unsigned long samsung_pll35xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	u32 mdiv, pdiv, sdiv, pll_con;
	u64 fvco = parent_rate;

	pll_con = __raw_readl(pll->con_reg);
	mdiv = (pll_con >> PLL35XX_MDIV_SHIFT) & PLL35XX_MDIV_MASK;
	pdiv = (pll_con >> PLL35XX_PDIV_SHIFT) & PLL35XX_PDIV_MASK;
	sdiv = (pll_con >> PLL35XX_SDIV_SHIFT) & PLL35XX_SDIV_MASK;

	fvco *= mdiv;
	do_div(fvco, (pdiv << sdiv));

	return (unsigned long)fvco;
}

static inline bool samsung_pll35xx_mp_change(u32 mdiv, u32 pdiv, u32 pll_con)
{
	if ((mdiv != ((pll_con >> PLL35XX_MDIV_SHIFT) & PLL35XX_MDIV_MASK)) ||
		(pdiv != ((pll_con >> PLL35XX_PDIV_SHIFT) & PLL35XX_PDIV_MASK)) ||
		!(pll_con & (0x1 << PLL35XX_PLL_ENABLE_SHIFT)))
		return 1;
	else
		return 0;
}

static inline bool samsung_pll35xx_s_change(u32 sdiv, u32 pll_con)
{
	if (sdiv != ((pll_con >> PLL35XX_SDIV_SHIFT) & PLL35XX_SDIV_MASK))
		return 1;
	else
		return 0;
}

static int samsung_pll35xx_set_rate(struct clk_hw *hw, unsigned long drate,
					unsigned long prate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	const struct samsung_pll_rate_table *rate;
	u32 tmp;

	/* Get required rate settings from table */
	rate = samsung_get_pll_settings(pll, drate);
	if (!rate) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
			drate, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	tmp = __raw_readl(pll->con_reg);

	if (!(samsung_pll35xx_mp_change(rate->mdiv, rate->pdiv, tmp))) {
		if (!samsung_pll35xx_s_change(rate->sdiv, tmp))
			return 0;
		/* If only s change, change just s value only*/
		tmp &= ~(PLL35XX_SDIV_MASK << PLL35XX_SDIV_SHIFT);
		tmp |= rate->sdiv << PLL35XX_SDIV_SHIFT;
		__raw_writel(tmp, pll->con_reg);
	} else {
		/* Set PLL lock time. */
		__raw_writel(rate->pdiv * PLL35XX_LOCK_FACTOR, pll->lock_reg);

		/* Change PLL PMS values */
		tmp &= ~((PLL35XX_MDIV_MASK << PLL35XX_MDIV_SHIFT) |
				(PLL35XX_PDIV_MASK << PLL35XX_PDIV_SHIFT) |
				(PLL35XX_SDIV_MASK << PLL35XX_SDIV_SHIFT));
		tmp |= (rate->mdiv << PLL35XX_MDIV_SHIFT) |
				(rate->pdiv << PLL35XX_PDIV_SHIFT) |
				(rate->sdiv << PLL35XX_SDIV_SHIFT);
		tmp |= (1 << PLL35XX_PLL_ENABLE_SHIFT);

		__raw_writel(tmp, pll->con_reg);

		/* wait_lock_time */
		do {
			cpu_relax();
			tmp = __raw_readl(pll->con_reg);
		} while (!(tmp & (PLL35XX_LOCK_STAT_MASK
				<< PLL35XX_LOCK_STAT_SHIFT)));
	}

	return 0;
}

static const struct clk_ops samsung_pll35xx_clk_simple_ops = {
	.recalc_rate = samsung_pll35xx_recalc_rate,
};

static const struct clk_ops samsung_pll35xx_clk_normal_ops = {
	.recalc_rate = samsung_pll35xx_recalc_rate,
	.round_rate = samsung_pll_round_rate,
	.set_rate = samsung_pll35xx_set_rate,
};

static const struct clk_ops samsung_pll35xx_clk_full_ops = {
	.recalc_rate = samsung_pll35xx_recalc_rate,
	.round_rate = samsung_pll_round_rate,
	.set_rate = samsung_pll35xx_set_rate,
	.enable = samsung_pll_clk_enable,
	.disable = samsung_pll_clk_disable,
	.is_enabled = samsung_pll_clk_is_enabled,
};

int set_pll35xx_ops(struct clk *clk, unsigned int ops_type)
{
	if (clk == NULL)
		return -EINVAL;

	switch (ops_type) {
		case NORMAL_PLL_OPS :
			clk->ops = &samsung_pll35xx_clk_normal_ops;
			break;
		case FULL_PLL_OPS :
			clk->ops = &samsung_pll35xx_clk_full_ops;
			break;
		default :
			clk->ops = &samsung_pll35xx_clk_simple_ops;
			break;
	}

	return 0;
}

/**
 * samsung_clk_register_pll35xx - register a 35xx compatible PLL
 *
 * Can be used to register a PLL that is 35xx compatible.
 *
 * @name: The name of the PLL output, like "fout_apll"
 * @pname: The name of the parent, like "fin_pll"
 * @lock_reg: Pointer to iomapped memory for the LOCK register
 * @con_reg: Pointer to iomapped memory for the CON registers
 * @rate_table: A table of rates that this PLL can run at, sorted fastest rate
 *     first.
 * @rate_count: The number of rates in rate_table.
 */
struct clk * __init samsung_clk_register_pll35xx(const char *name,
			const char *pname, void __iomem *lock_reg,
			void __iomem *con_reg,
			const struct samsung_pll_rate_table *rate_table,
			const unsigned int rate_count)
{
	struct samsung_clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	if (rate_table && rate_count)
		init.ops = &samsung_pll35xx_clk_normal_ops;
	else
		init.ops = &samsung_pll35xx_clk_simple_ops;

	pll->hw.init = &init;
	pll->lock_reg = lock_reg;
	pll->con_reg = con_reg;
	pll->rate_table = rate_table;
	pll->rate_count = rate_count;
	pll->bit_enable = PLL35XX_PLL_ENABLE_SHIFT;
	pll->bit_lockstat = PLL35XX_LOCK_STAT_SHIFT;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	return clk;
}

/*
 * PLL36xx Clock Type
 */

/* Maximum lock time can be 3000 * PDIV cycles */
#define PLL36XX_LOCK_FACTOR	(3000)

#define PLL36XX_KDIV_MASK	(0xFFFF)
#define PLL36XX_MDIV_MASK	(0x1FF)
#define PLL36XX_PDIV_MASK	(0x3F)
#define PLL36XX_SDIV_MASK	(0x7)
#define PLL36XX_MDIV_SHIFT	(16)
#define PLL36XX_PDIV_SHIFT	(8)
#define PLL36XX_SDIV_SHIFT	(0)
#define PLL36XX_KDIV_SHIFT	(0)
#define PLL36XX_LOCK_STAT_SHIFT	(29)
#define PLL36XX_ENABLE_SHIFT	(31)

static unsigned long samsung_pll36xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	u32 mdiv, pdiv, sdiv, pll_con0, pll_con1;
	s16 kdiv;
	u64 fvco = parent_rate;

	pll_con0 = __raw_readl(pll->con_reg);
	pll_con1 = __raw_readl(pll->con_reg + 4);
	mdiv = (pll_con0 >> PLL36XX_MDIV_SHIFT) & PLL36XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL36XX_PDIV_SHIFT) & PLL36XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL36XX_SDIV_SHIFT) & PLL36XX_SDIV_MASK;
	kdiv = (s16)(pll_con1 & PLL36XX_KDIV_MASK);

	fvco *= (mdiv << 16) + kdiv;
	do_div(fvco, (pdiv << sdiv));
	fvco >>= 16;

	return (unsigned long)fvco;
}

static int samsung_pll36xx_set_rate(struct clk_hw *hw, unsigned long drate,
					unsigned long parent_rate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	u32 tmp, pll_con0, pll_con1;
	const struct samsung_pll_rate_table *rate;

	rate = samsung_get_pll_settings(pll, drate);
	if (!rate) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
			drate, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	pll_con0 = __raw_readl(pll->con_reg);
	pll_con1 = __raw_readl(pll->con_reg + 4);

	/* Set PLL lock time. */
	__raw_writel(rate->pdiv * PLL36XX_LOCK_FACTOR, pll->lock_reg);

	 /* Change PLL PMS values */
	pll_con0 &= ~((PLL36XX_MDIV_MASK << PLL36XX_MDIV_SHIFT) |
			(PLL36XX_PDIV_MASK << PLL36XX_PDIV_SHIFT) |
			(PLL36XX_SDIV_MASK << PLL36XX_SDIV_SHIFT));
	pll_con0 |= (rate->mdiv << PLL36XX_MDIV_SHIFT) |
			(rate->pdiv << PLL36XX_PDIV_SHIFT) |
			(rate->sdiv << PLL36XX_SDIV_SHIFT);
	__raw_writel(pll_con0, pll->con_reg);

	pll_con1 &= ~(PLL36XX_KDIV_MASK << PLL36XX_KDIV_SHIFT);
	pll_con1 |= rate->kdiv << PLL36XX_KDIV_SHIFT;
	__raw_writel(pll_con1, pll->con_reg + 4);

	/* wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(pll->con_reg);
	} while (!(tmp & (1 << PLL36XX_LOCK_STAT_SHIFT)));

	return 0;
}

static const struct clk_ops samsung_pll36xx_clk_ops = {
	.recalc_rate = samsung_pll36xx_recalc_rate,
	.set_rate = samsung_pll36xx_set_rate,
	.round_rate = samsung_pll_round_rate,
	.enable = samsung_pll_clk_enable,
	.disable = samsung_pll_clk_disable,
	.is_enabled = samsung_pll_clk_is_enabled,
};

static const struct clk_ops samsung_pll36xx_clk_min_ops = {
	.recalc_rate = samsung_pll36xx_recalc_rate,
};

/**
 * samsung_clk_register_pll36xx - register a 36xx compatible PLL
 *
 * Can be used to register a PLL that is 36xx compatible.  This includes
 * PLL2650x on exynos5420.
 *
 * @name: The name of the PLL output, like "fout_epll"
 * @pname: The name of the parent, like "fin_pll"
 * @lock_reg: Pointer to iomapped memory for the LOCK register
 * @con_reg: Pointer to iomapped memory for the CON registers
 * @rate_table: A table of rates that this PLL can run at, sorted fastest rate
 *     first.
 * @rate_count: The number of rates in rate_table.
 */

struct clk * __init samsung_clk_register_pll36xx(const char *name,
			const char *pname, void __iomem *lock_reg,
			void __iomem *con_reg,
			const struct samsung_pll_rate_table *rate_table,
			const unsigned int rate_count)
{
	struct samsung_clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	if (rate_table && rate_count)
		init.ops = &samsung_pll36xx_clk_ops;
	else
		init.ops = &samsung_pll36xx_clk_min_ops;

	pll->hw.init = &init;
	pll->lock_reg = lock_reg;
	pll->con_reg = con_reg;
	pll->rate_table = rate_table;
	pll->rate_count = rate_count;
	pll->bit_enable = PLL36XX_ENABLE_SHIFT;
	pll->bit_lockstat = PLL36XX_LOCK_STAT_SHIFT;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	return clk;
}

/*
 * PLL45xx Clock Type
 */

#define PLL45XX_MDIV_MASK	(0x3FF)
#define PLL45XX_PDIV_MASK	(0x3F)
#define PLL45XX_SDIV_MASK	(0x7)
#define PLL45XX_MDIV_SHIFT	(16)
#define PLL45XX_PDIV_SHIFT	(8)
#define PLL45XX_SDIV_SHIFT	(0)

struct samsung_clk_pll45xx {
	struct clk_hw		hw;
	enum pll45xx_type	type;
	const void __iomem	*con_reg;
};

#define to_clk_pll45xx(_hw) container_of(_hw, struct samsung_clk_pll45xx, hw)

static unsigned long samsung_pll45xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll45xx *pll = to_clk_pll45xx(hw);
	u32 mdiv, pdiv, sdiv, pll_con;
	u64 fvco = parent_rate;

	pll_con = __raw_readl(pll->con_reg);
	mdiv = (pll_con >> PLL45XX_MDIV_SHIFT) & PLL45XX_MDIV_MASK;
	pdiv = (pll_con >> PLL45XX_PDIV_SHIFT) & PLL45XX_PDIV_MASK;
	sdiv = (pll_con >> PLL45XX_SDIV_SHIFT) & PLL45XX_SDIV_MASK;

	if (pll->type == pll_4508)
		sdiv = sdiv - 1;

	fvco *= mdiv;
	do_div(fvco, (pdiv << sdiv));

	return (unsigned long)fvco;
}

static const struct clk_ops samsung_pll45xx_clk_ops = {
	.recalc_rate = samsung_pll45xx_recalc_rate,
};

struct clk * __init samsung_clk_register_pll45xx(const char *name,
			const char *pname, const void __iomem *con_reg,
			enum pll45xx_type type)
{
	struct samsung_clk_pll45xx *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll45xx_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->con_reg = con_reg;
	pll->type = type;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	return clk;
}

/*
 * PLL46xx Clock Type
 */

#define PLL46XX_MDIV_MASK	(0x1FF)
#define PLL46XX_PDIV_MASK	(0x3F)
#define PLL46XX_SDIV_MASK	(0x7)
#define PLL46XX_MDIV_SHIFT	(16)
#define PLL46XX_PDIV_SHIFT	(8)
#define PLL46XX_SDIV_SHIFT	(0)

#define PLL46XX_KDIV_MASK	(0xFFFF)
#define PLL4650C_KDIV_MASK	(0xFFF)
#define PLL46XX_KDIV_SHIFT	(0)

struct samsung_clk_pll46xx {
	struct clk_hw		hw;
	enum pll46xx_type	type;
	const void __iomem	*con_reg;
};

#define to_clk_pll46xx(_hw) container_of(_hw, struct samsung_clk_pll46xx, hw)

static unsigned long samsung_pll46xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll46xx *pll = to_clk_pll46xx(hw);
	u32 mdiv, pdiv, sdiv, kdiv, pll_con0, pll_con1, shift;
	u64 fvco = parent_rate;

	pll_con0 = __raw_readl(pll->con_reg);
	pll_con1 = __raw_readl(pll->con_reg + 4);
	mdiv = (pll_con0 >> PLL46XX_MDIV_SHIFT) & PLL46XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL46XX_PDIV_SHIFT) & PLL46XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL46XX_SDIV_SHIFT) & PLL46XX_SDIV_MASK;
	kdiv = pll->type == pll_4650c ? pll_con1 & PLL4650C_KDIV_MASK :
					pll_con1 & PLL46XX_KDIV_MASK;

	shift = pll->type == pll_4600 ? 16 : 10;
	fvco *= (mdiv << shift) + kdiv;
	do_div(fvco, (pdiv << sdiv));
	fvco >>= shift;

	return (unsigned long)fvco;
}

static const struct clk_ops samsung_pll46xx_clk_ops = {
	.recalc_rate = samsung_pll46xx_recalc_rate,
};

struct clk * __init samsung_clk_register_pll46xx(const char *name,
			const char *pname, const void __iomem *con_reg,
			enum pll46xx_type type)
{
	struct samsung_clk_pll46xx *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll46xx_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->con_reg = con_reg;
	pll->type = type;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	return clk;
}

/*
 * PLL2550x Clock Type
 */

#define PLL2550X_R_MASK       (0x1)
#define PLL2550X_P_MASK       (0x3F)
#define PLL2550X_M_MASK       (0x3FF)
#define PLL2550X_S_MASK       (0x7)
#define PLL2550X_R_SHIFT      (20)
#define PLL2550X_P_SHIFT      (14)
#define PLL2550X_M_SHIFT      (4)
#define PLL2550X_S_SHIFT      (0)

struct samsung_clk_pll2550x {
	struct clk_hw		hw;
	const void __iomem	*reg_base;
	unsigned long		offset;
};

#define to_clk_pll2550x(_hw) container_of(_hw, struct samsung_clk_pll2550x, hw)

static unsigned long samsung_pll2550x_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll2550x *pll = to_clk_pll2550x(hw);
	u32 r, p, m, s, pll_stat;
	u64 fvco = parent_rate;

	pll_stat = __raw_readl(pll->reg_base + pll->offset * 3);
	r = (pll_stat >> PLL2550X_R_SHIFT) & PLL2550X_R_MASK;
	if (!r)
		return 0;
	p = (pll_stat >> PLL2550X_P_SHIFT) & PLL2550X_P_MASK;
	m = (pll_stat >> PLL2550X_M_SHIFT) & PLL2550X_M_MASK;
	s = (pll_stat >> PLL2550X_S_SHIFT) & PLL2550X_S_MASK;

	fvco *= m;
	do_div(fvco, (p << s));

	return (unsigned long)fvco;
}

static const struct clk_ops samsung_pll2550x_clk_ops = {
	.recalc_rate = samsung_pll2550x_recalc_rate,
};

struct clk * __init samsung_clk_register_pll2550x(const char *name,
			const char *pname, const void __iomem *reg_base,
			const unsigned long offset)
{
	struct samsung_clk_pll2550x *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll2550x_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->reg_base = reg_base;
	pll->offset = offset;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	return clk;
}

/*
 * PLL145xx Clock Type: PLL1450x, PLL1451x, PLL1452x
 * Maximum lock time can be 150 * Pdiv cycles
 */
#define PLL145XX_LOCK_FACTOR		(150)
#define PLL145XX_MDIV_MASK		(0x3FF)
#define PLL145XX_PDIV_MASK		(0x03F)
#define PLL145XX_SDIV_MASK		(0x007)
#define PLL145XX_LOCKED_MASK		(0x1)
#define PLL145XX_MDIV_SHIFT		(16)
#define PLL145XX_PDIV_SHIFT		(8)
#define PLL145XX_SDIV_SHIFT		(0)
#define PLL145XX_LOCKED_SHIFT		(29)

static unsigned long samsung_pll145xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	u32 mdiv, pdiv, sdiv, pll_con;
	u64 fvco = parent_rate;

	pll_con = __raw_readl(pll->con_reg);
	mdiv = (pll_con >> PLL145XX_MDIV_SHIFT) & PLL145XX_MDIV_MASK;
	pdiv = (pll_con >> PLL145XX_PDIV_SHIFT) & PLL145XX_PDIV_MASK;
	sdiv = (pll_con >> PLL145XX_SDIV_SHIFT) & PLL145XX_SDIV_MASK;
	/* Do calculation */
	fvco *= mdiv;
	do_div(fvco, (pdiv << sdiv));

	return (unsigned long)fvco;
}

static inline bool samsung_pll145xx_mp_check(u32 mdiv, u32 pdiv, u32 pll_con)
{
	return ((mdiv != ((pll_con >> PLL145XX_MDIV_SHIFT) & PLL145XX_MDIV_MASK)) ||
		(pdiv != ((pll_con >> PLL145XX_PDIV_SHIFT) & PLL145XX_PDIV_MASK)));
}

static int samsung_pll145xx_set_rate(struct clk_hw *hw, unsigned long drate,
					unsigned long prate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	const struct samsung_pll_rate_table *rate;
	u32 pll_con;

	rate = samsung_get_pll_settings(pll, drate);
	if (!rate) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
				drate, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	pll_con = __raw_readl(pll->con_reg);
	if (!(samsung_pll145xx_mp_check(rate->mdiv, rate->pdiv, pll_con))) {
		if ((rate->sdiv) == ((pll_con >> PLL145XX_SDIV_SHIFT) & PLL145XX_SDIV_MASK))
			return 0;
		/* In the case of changing S value only */
		pll_con &= ~(PLL145XX_SDIV_MASK << PLL145XX_SDIV_SHIFT);
		pll_con |= rate->sdiv << PLL145XX_SDIV_SHIFT;
		__raw_writel(pll_con, pll->con_reg);

		return 0;
	}

	/* Set PLL lock time */
	__raw_writel(rate->pdiv * PLL145XX_LOCK_FACTOR, pll->lock_reg);
	/* Change PLL PMS values */
	pll_con &= ~((PLL145XX_MDIV_MASK << PLL145XX_MDIV_SHIFT) |
			(PLL145XX_PDIV_MASK << PLL145XX_PDIV_SHIFT) |
			(PLL145XX_SDIV_MASK << PLL145XX_SDIV_SHIFT));
	pll_con |= (rate->mdiv << PLL145XX_MDIV_SHIFT) |
			(rate->pdiv << PLL145XX_PDIV_SHIFT) |
			(rate->sdiv << PLL145XX_SDIV_SHIFT);
	__raw_writel(pll_con, pll->con_reg);

	do {
		cpu_relax();
		pll_con = __raw_readl(pll->con_reg);
	} while (!(pll_con & (PLL145XX_LOCKED_MASK << PLL145XX_LOCKED_SHIFT)));

	return 0;
}

static const struct clk_ops samsung_pll145xx_clk_ops = {
	.recalc_rate = samsung_pll145xx_recalc_rate,
	.round_rate = samsung_pll_round_rate,
	.set_rate = samsung_pll145xx_set_rate,
};
static const struct clk_ops samsung_pll145xx_clk_min_ops = {
	.recalc_rate = samsung_pll145xx_recalc_rate,
};

/*
 * PLL1460X Clock Type
 * Maximum lock time can be 3000 * Pdiv cycles
 */
#define PLL1460X_LOCK_FACTOR		(3000)
#define PLL1460X_MDIV_MASK		(0x03FF)
#define PLL1460X_PDIV_MASK		(0x003F)
#define PLL1460X_SDIV_MASK		(0x0007)
#define PLL1460X_KDIV_MASK		(0xFFFF)
#define PLL1460X_LOCKED_MASK		(0x1)
#define PLL1460X_MDIV_SHIFT		(16)
#define PLL1460X_PDIV_SHIFT		(8)
#define PLL1460X_SDIV_SHIFT		(0)
#define PLL1460X_KDIV_SHIFT		(0)
#define PLL1460X_LOCKED_SHIFT		(29)

static unsigned long samsung_pll1460x_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	u32 mdiv, pdiv, sdiv, pll_con0, pll_con1;
	s16 kdiv;
	u64 fvco = parent_rate;

	pll_con0 = __raw_readl(pll->con_reg);
	pll_con1 = __raw_readl(pll->con_reg + 4);
	mdiv = (pll_con0 >> PLL145XX_MDIV_SHIFT) & PLL145XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL145XX_PDIV_SHIFT) & PLL145XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL145XX_SDIV_SHIFT) & PLL145XX_SDIV_MASK;
	kdiv = (s16)((pll_con1 >> PLL1460X_KDIV_SHIFT) & PLL1460X_KDIV_MASK);
	/* Do calculation */
	fvco *= (mdiv << 16) + kdiv;
	do_div(fvco, (pdiv << sdiv));
	fvco >>= 16;

	return (unsigned long)fvco;
}

static inline bool samsung_pll1460x_mpk_check(u32 mdiv, u32 pdiv, u32 kdiv, u32 pll_con0, u32 pll_con1)
{
	return ((mdiv != ((pll_con0 >> PLL1460X_MDIV_SHIFT) & PLL1460X_MDIV_MASK)) ||
		(pdiv != ((pll_con0 >> PLL1460X_PDIV_SHIFT) & PLL1460X_PDIV_MASK)) ||
		(kdiv != ((pll_con1 >> PLL1460X_KDIV_SHIFT) & PLL1460X_KDIV_MASK)));
}

static int samsung_pll1460x_set_rate(struct clk_hw *hw, unsigned long drate,
					unsigned long parent_rate)
{
	struct samsung_clk_pll *pll = to_clk_pll(hw);
	u32 pll_con0, pll_con1;
	const struct samsung_pll_rate_table *rate;

	rate = samsung_get_pll_settings(pll, drate);
	if (!rate) {
		pr_err("%s: Invalid rate : %lu for pll clk %s\n", __func__,
				drate, __clk_get_name(hw->clk));
		return -EINVAL;
	}

	pll_con0 = __raw_readl(pll->con_reg);
	pll_con1 = __raw_readl(pll->con_reg + 4);
	if (!(samsung_pll1460x_mpk_check(rate->mdiv, rate->pdiv, rate->kdiv, pll_con0, pll_con1))) {
		if ((rate->sdiv) == ((pll_con0 >> PLL1460X_SDIV_SHIFT) & PLL1460X_SDIV_MASK))
			return 0;
		/* In the case of changing S value only */
		pll_con0 &= ~(PLL1460X_SDIV_MASK << PLL1460X_SDIV_SHIFT);
		pll_con0 |= (rate->sdiv << PLL1460X_SDIV_SHIFT);
		__raw_writel(pll_con0, pll->con_reg);

		return 0;
	}

	/* Set PLL lock time */
	__raw_writel(rate->pdiv * PLL1460X_LOCK_FACTOR, pll->lock_reg);
	pll_con0 &= ~((PLL1460X_MDIV_MASK << PLL1460X_MDIV_SHIFT) |
			(PLL1460X_PDIV_MASK << PLL1460X_PDIV_SHIFT) |
			(PLL1460X_SDIV_MASK << PLL1460X_SDIV_SHIFT));
	pll_con0 |= (rate->mdiv << PLL1460X_MDIV_SHIFT) |
			(rate->pdiv << PLL1460X_PDIV_SHIFT) |
			(rate->sdiv << PLL1460X_SDIV_SHIFT);
	__raw_writel(pll_con0, pll->con_reg);

	pll_con1 &= ~(PLL1460X_KDIV_MASK << PLL1460X_KDIV_SHIFT);
	pll_con1 |= (rate->kdiv << PLL1460X_KDIV_SHIFT);
	__raw_writel(pll_con1, pll->con_reg + 4);

	/* Wait lock time */
	do {
		cpu_relax();
		pll_con0 = __raw_readl(pll->con_reg);
	} while (!(pll_con0 & (PLL1460X_LOCKED_MASK << PLL1460X_LOCKED_SHIFT)));

	return 0;
}

static const struct clk_ops samsung_pll1460x_clk_ops = {
	.recalc_rate = samsung_pll1460x_recalc_rate,
	.round_rate = samsung_pll_round_rate,
	.set_rate = samsung_pll1460x_set_rate,
};
static const struct clk_ops samsung_pll1460x_clk_min_ops = {
	.recalc_rate = samsung_pll1460x_recalc_rate,
};

static void __init _samsung_clk_register_pll(struct samsung_pll_clock *list)
{
	struct samsung_clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;
	int len;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n",
			__func__, list->name);
		return;
	}

	init.name = list->name;
	init.flags = list->flags;
	init.parent_names = &list->parent_name;
	init.num_parents = 1;

	if (list->rate_table) {
		/* find count of rates in rate_table */
		for (len = 0; list->rate_table[len].rate != 0; )
			len++;

		pll->rate_count = len;
		pll->rate_table = kmemdup(list->rate_table,
					pll->rate_count *
					sizeof(struct samsung_pll_rate_table),
					GFP_KERNEL);
		WARN(!pll->rate_table,
				"%s: could not allocate rate table for %s\n",
				__func__, list->name);
	}

	if (list->type == pll_1450x) {
		if (pll->rate_table)
			init.ops = &samsung_pll145xx_clk_ops;
		else
			init.ops = &samsung_pll145xx_clk_min_ops;
	}
	else if (list->type == pll_1451x || list->type == pll_1452x) {
		if (pll->rate_table)
			init.ops = &samsung_pll145xx_clk_ops;
		else
			init.ops = &samsung_pll145xx_clk_min_ops;
	}
	else if (list->type == pll_1460x) {
		if (pll->rate_table)
			init.ops = &samsung_pll1460x_clk_ops;
		else
			init.ops = &samsung_pll1460x_clk_min_ops;
	}

	pll->hw.init = &init;
	pll->lock_reg = (void __iomem *)list->lock_offset;
	pll->con_reg = (void __iomem *)list->con_offset;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s : %ld\n",
			__func__, list->name, PTR_ERR(clk));
		kfree(pll);
		return;
	}

	samsung_clk_add_lookup(clk, list->id);

	/* register a clock lookup only if a clock alias is specified */
	if (clk_register_clkdev(clk, list->name, list->dev_name))
		pr_err("%s: failed to register lookup %s\n",
				__func__, list->name);
}

void __init samsung_clk_register_pll(struct samsung_pll_clock *pll_list,
				unsigned int nr_pll)
{
	int cnt;

	for (cnt = 0; cnt < nr_pll; cnt++)
		_samsung_clk_register_pll(&pll_list[cnt]);
}
