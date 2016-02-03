/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Copyright (c) 2013 Linaro Ltd.
 * Author: Thomas Abraham <thomas.ab@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for all Samsung platforms
*/

#ifndef __SAMSUNG_CLK_H
#define __SAMSUNG_CLK_H

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk-pll.h"

#define MHZ (1000*1000)
#define CLK_GATE_MULTI_BIT_SET   BIT(30)

/**
 * struct samsung_fixed_rate_clock: information about fixed-rate clock
 * @id: platform specific id of the clock.
 * @name: name of this fixed-rate clock.
 * @parent_name: optional parent clock name.
 * @flags: optional fixed-rate clock flags.
 * @fixed-rate: fixed clock rate of this clock.
 */
struct samsung_fixed_rate_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		fixed_rate;
};

#define FRATE(_id, cname, pname, f, frate)		\
	{						\
		.id		= _id,			\
		.name		= cname,		\
		.parent_name	= pname,		\
		.flags		= f,			\
		.fixed_rate	= frate,		\
	}

/*
 * struct samsung_fixed_factor_clock: information about fixed-factor clock
 * @id: platform specific id of the clock.
 * @name: name of this fixed-factor clock.
 * @parent_name: parent clock name.
 * @mult: fixed multiplication factor.
 * @div: fixed division factor.
 * @flags: optional fixed-factor clock flags.
 */
struct samsung_fixed_factor_clock {
	unsigned int		id;
	char			*name;
	const char		*parent_name;
	unsigned long		mult;
	unsigned long		div;
	unsigned long		flags;
};

#define FFACTOR(_id, cname, pname, m, d, f)		\
	{						\
		.id		= _id,			\
		.name		= cname,		\
		.parent_name	= pname,		\
		.mult		= m,			\
		.div		= d,			\
		.flags		= f,			\
	}

/**
 * struct samsung_mux_clock: information about mux clock
 * @id: platform specific id of the clock.
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this mux clock.
 * @parent_names: array of pointer to parent clock names.
 * @num_parents: number of parents listed in @parent_names.
 * @flags: optional flags for basic clock.
 * @offset: offset of the register for configuring the mux.
 * @shift: starting bit location of the mux control bit-field in @reg.
 * @width: width of the mux control bit-field in @reg.
 * @mux_flags: flags for mux-type clock.
 * @alias: optional clock alias name to be assigned to this clock.
 */

struct samsung_mux_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		**parent_names;
	u8			num_parents;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			mux_flags;
	const char		*alias;
	unsigned long		stat_offset;
	u8			stat_shift;
	u8			stat_width;
};

#define __MUX(_id, dname, cname, pnames, o, s, w, f, mf, a, so, ss, sw)	\
{							\
	.id		= _id,				\
	.dev_name	= dname,			\
	.name		= cname,			\
	.parent_names	= pnames,			\
	.num_parents	= ARRAY_SIZE(pnames),		\
	.flags		= f,				\
	.offset		= o,				\
	.shift		= s,				\
	.width		= w,				\
	.mux_flags	= mf,				\
	.alias		= a,				\
	.stat_offset	= so,				\
	.stat_shift	= ss,				\
	.stat_width	= sw,				\
}

#define MUX(_id, cname, pnames, o, s, w)			\
	__MUX(_id, NULL, cname, pnames, (unsigned long)o, s, w, 0, 0, NULL, 0, 0, 0)

#define MUX_A(_id, cname, pnames, o, s, w, a)			\
	__MUX(_id, NULL, cname, pnames, (unsigned long)o, s, w, 0, 0, a, 0, 0, 0)

#define MUX_STAT(_id, cname, pnames, o, s, w, so, ss, sw)			\
	__MUX(_id, NULL, cname, pnames, (unsigned long)o, s, w, 0, 0, NULL, (unsigned long)so, ss, sw)

#define MUX_S_A(_id, cname, pnames, o, s, w, a, so, ss, sw)			\
	__MUX(_id, NULL, cname, pnames, o, s, w, 0, 0, a, so, ss, sw)

#define CMX_S_A(_id, o, s, w, a, so, ss, sw) \
	MUX_S_A(_id, #_id, _id##_p, (unsigned long)o, s, w, a, (unsigned long)so, ss, sw)

/**
 * @id: platform specific id of the clock.
 * struct samsung_div_clock: information about div clock
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this div clock.
 * @parent_name: name of the parent clock.
 * @flags: optional flags for basic clock.
 * @offset: offset of the register for configuring the div.
 * @shift: starting bit location of the div control bit-field in @reg.
 * @div_flags: flags for div-type clock.
 * @alias: optional clock alias name to be assigned to this clock.
 */
struct samsung_div_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			shift;
	u8			width;
	u8			div_flags;
	const char		*alias;
};

#define __DIV(_id, dname, cname, pname, o, s, w, f, df, a)	\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_name	= pname,			\
		.flags		= f,				\
		.offset		= o,				\
		.shift		= s,				\
		.width		= w,				\
		.div_flags	= df,				\
		.alias		= a,				\
	}

#define DIV(_id, cname, pname, o, s, w)				\
	__DIV(_id, NULL, cname, pname, (unsigned long)o, s, w, 0, 0, NULL)

#define DIV_A(_id, cname, pname, o, s, w, a)			\
	__DIV(_id, NULL, cname, pname, (unsigned long)o, s, w, 0, 0, a)

#define DIV_F(_id, cname, pname, o, s, w, f, df)		\
	__DIV(_id, NULL, cname, pname, (unsigned long)o, s, w, f, df, NULL)

/**
 * struct samsung_gate_clock: information about gate clock
 * @id: platform specific id of the clock.
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this gate clock.
 * @parent_name: name of the parent clock.
 * @flags: optional flags for basic clock.
 * @offset: offset of the register for configuring the gate.
 * @bit_idx: bit index of the gate control bit-field in @reg.
 * @gate_flags: flags for gate-type clock.
 * @alias: optional clock alias name to be assigned to this clock.
 */
struct samsung_gate_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		offset;
	u8			bit_idx;
	u8			gate_flags;
	const char		*alias;
	unsigned long		set_bit;
};

#define __GATE(_id, dname, cname, pname, o, b, f, gf, a, s)	\
	{							\
		.id		= _id,				\
		.dev_name	= dname,			\
		.name		= cname,			\
		.parent_name	= pname,			\
		.flags		= f,				\
		.offset		= o,				\
		.bit_idx	= b,				\
		.gate_flags	= gf,				\
		.alias		= a,				\
		.set_bit	= s,				\
	}

#define GATE(_id, cname, pname, o, b, f, gf)			\
	__GATE(_id, NULL, cname, pname, (unsigned long)o, b, f, gf, NULL, 0)

#define MGATE(_id, cname, pname, o, b, f, gf, s)			\
	__GATE(_id, NULL, cname, pname, o, b, f, gf, NULL, s)

#define GATE_A(_id, cname, pname, o, b, f, gf, a)		\
	__GATE(_id, NULL, cname, pname, o, b, f, gf, a, 0)

#define GATE_D(_id, dname, cname, pname, o, b, f, gf)		\
	__GATE(_id, dname, cname, pname, o, b, f, gf, NULL, 0)

#define GATE_DA(_id, dname, cname, pname, o, b, f, gf, a)	\
	__GATE(_id, dname, cname, pname, o, b, f, gf, a, 0)

#define CGTE_A(_id, pname, o, b, f, gf, a) \
	GATE_A(_id, #_id, pname, (unsigned long)o, b, f, gf, a)

#define PNAME(x) static const char *x[] __initdata

/*
 * struct samsung_pll_clock: information about pll clock
 * @id: platform specific id of the clock.
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this pll clock.
 * @parent_name: name of the parent clock.
 * @flags: optional flags for basic clock.
 * @con_offset: offset of the register for configuring the PLL.
 * @lock_offset: offset of the register for locking the PLL.
 * @type: Type of PLL to be registered.
 * @alias: optional clock alias name to be assigned to this clock.
 */
struct samsung_pll_clock {
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned long		con_offset;
	unsigned long		lock_offset;
	enum pll_type		type;
	const struct samsung_pll_rate_table *rate_table;
};

#define _PLL(_typ, _id, _dname, _name, _pname, _flags, _lock, _con,	\
		_rtable)						\
	{								\
		.id		= _id,					\
		.type		= _typ,					\
		.dev_name	= _dname,				\
		.name		= _name,				\
		.parent_name	= _pname,				\
		.flags		= _flags,				\
		.con_offset	= _con,					\
		.lock_offset	= _lock,				\
		.rate_table	= _rtable,				\
	}

#define PLL(_typ, _id, _name, _pname, _lock, _con, _rtable)		\
	_PLL(_typ, _id, NULL, _name, _pname, CLK_GET_RATE_NOCACHE,	\
		(unsigned long)_lock, (unsigned long)_con, _rtable)

#define PLL_A(_typ, _id, _name, _pname, _lock, _con, _rtable)		\
	_PLL(_typ, _id, NULL, _name, _pname, CLK_GET_RATE_NOCACHE,	\
		_lock, _con, _rtable)

/**
 * struct samsung_clk_reg_dump: register dump of clock controller registers.
 * @offset: clock register offset from the controller base address.
 * @value: the value to be register at offset.
 */
struct samsung_clk_reg_dump {
	unsigned long	offset;
	u32		value;
};

extern void __init samsung_clk_init(struct device_node *np, void __iomem *base,
		unsigned long nr_clks, unsigned long *rdump,
		unsigned long nr_rdump, unsigned long *soc_rdump,
		unsigned long nr_soc_rdump);
extern void __init samsung_clk_of_register_fixed_ext(
		struct samsung_fixed_rate_clock *fixed_rate_clk,
		unsigned int nr_fixed_rate_clk,
		struct of_device_id *clk_matches);

extern void samsung_clk_add_lookup(struct clk *clk, unsigned int id);

extern void __init samsung_clk_register_fixed_rate(
		struct samsung_fixed_rate_clock *clk_list, unsigned int nr_clk);
extern void __init samsung_clk_register_fixed_factor(
		struct samsung_fixed_factor_clock *list, unsigned int nr_clk);
extern void __init samsung_clk_register_mux(struct samsung_mux_clock *clk_list,
		unsigned int nr_clk);
extern void __init samsung_clk_register_div(struct samsung_div_clock *clk_list,
		unsigned int nr_clk);
extern void __init samsung_clk_register_gate(
		struct samsung_gate_clock *clk_list, unsigned int nr_clk);

extern unsigned long _get_rate(const char *clk_name);


extern int exynos_set_parent(const char *child, const char *parent);
extern struct clk *exynos_get_parent(const char *child);
extern int exynos_set_rate(const char *conid, unsigned int rate);
extern unsigned int  exynos_get_rate(const char *conid);

#endif /* __SAMSUNG_CLK_H */
