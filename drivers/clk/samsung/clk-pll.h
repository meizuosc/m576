/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Copyright (c) 2013 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for all PLL's in Samsung platforms
*/

#ifndef __SAMSUNG_CLK_PLL_H
#define __SAMSUNG_CLK_PLL_H

#define PLL_35XX_RATE(_rate, _m, _p, _s)			\
	{							\
		.rate	=	(_rate),				\
		.mdiv	=	(_m),				\
		.pdiv	=	(_p),				\
		.sdiv	=	(_s),				\
	}

#define PLL_36XX_RATE(_rate, _m, _p, _s, _k)			\
	{							\
		.rate	=	(_rate),				\
		.mdiv	=	(_m),				\
		.pdiv	=	(_p),				\
		.sdiv	=	(_s),				\
		.kdiv	=	(_k),				\
	}

/* NOTE: Rate table should be kept sorted in descending order. */

struct samsung_pll_rate_table {
	unsigned int rate;
	unsigned int pdiv;
	unsigned int mdiv;
	unsigned int sdiv;
	unsigned int kdiv;
};

enum pll45xx_type {
	pll_4500,
	pll_4502,
	pll_4508
};

enum pll46xx_type {
	pll_4600,
	pll_4650,
	pll_4650c,
};

enum pll_type {
	pll_1450x = 0,
	pll_1451x,
	pll_1452x,
	pll_1460x,
};

extern struct clk * __init samsung_clk_register_pll35xx(const char *name,
			const char *pname, void __iomem *lock_reg,
			void __iomem *con_reg,
			const struct samsung_pll_rate_table *rate_table,
			const unsigned int rate_count);
extern struct clk * __init samsung_clk_register_pll36xx(const char *name,
			const char *pname, void __iomem *lock_reg,
			void __iomem *con_reg,
			const struct samsung_pll_rate_table *rate_table,
			const unsigned int rate_count);
extern struct clk * __init samsung_clk_register_pll45xx(const char *name,
			const char *pname, const void __iomem *con_reg,
			enum pll45xx_type type);
extern struct clk * __init samsung_clk_register_pll46xx(const char *name,
			const char *pname, const void __iomem *con_reg,
			enum pll46xx_type type);
extern struct clk * __init samsung_clk_register_pll2550x(const char *name,
			const char *pname, const void __iomem *reg_base,
			const unsigned long offset);

enum pll_ops_kind {
	SIMPLE_PLL_OPS,
	NORMAL_PLL_OPS,
	FULL_PLL_OPS,
	MAX_PLL_OPS_TYPE,
};

struct samsung_clk_pll {
	struct clk_hw		hw;
	void __iomem		*lock_reg;
	void __iomem		*con_reg;
	const struct samsung_pll_rate_table *rate_table;
	unsigned int rate_count;
	unsigned int bit_enable;
	unsigned int bit_lockstat;
};

extern int set_pll35xx_ops(struct clk *clk, unsigned int ops_type);

struct samsung_pll_clock;

extern void __init samsung_clk_register_pll(struct samsung_pll_clock *list,
		unsigned int nr_pll);
#endif /* __SAMSUNG_CLK_PLL_H */
