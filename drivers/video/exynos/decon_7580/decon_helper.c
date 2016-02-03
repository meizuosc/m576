/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Helper file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/err.h>

#include "decon.h"
#include "decon_helper.h"
#include "./panels/lcd_ctrl.h"

int decon_clk_set_parent(struct device *dev, const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;

	p = clk_get(dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, parent);
		return -ENODEV;
	}

	c = clk_get(dev, child);
	if (IS_ERR_OR_NULL(c)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, child);
		return -ENODEV;
	}

	clk_set_parent(c, p);
	clk_put(p);
	clk_put(c);

	return 0;
}

int decon_clk_set_rate(struct device *dev, const char *conid, unsigned int rate)
{
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		decon_err("%s: couldn't get clock : %s\n", __func__, conid);
		return -ENODEV;
	}

	clk_set_rate(target, rate);
	clk_put(target);

	return 0;
}

void decon_to_psr_info(struct decon_device *decon, struct decon_psr_info *psr)
{
	psr->psr_mode = decon->pdata->psr_mode;
	psr->trig_mode = decon->pdata->trig_mode;
	psr->out_type = decon->out_type;
}

void decon_to_init_param(struct decon_device *decon, struct decon_init_param *p)
{
	struct decon_lcd *lcd_info = decon->lcd_info;
	struct v4l2_mbus_framefmt mbus_fmt;

	mbus_fmt.width = 0;
	mbus_fmt.height = 0;
	mbus_fmt.code = 0;
	mbus_fmt.field = 0;
	mbus_fmt.colorspace = 0;

	p->lcd_info = lcd_info;
	p->psr.psr_mode = decon->pdata->psr_mode;
	p->psr.trig_mode = decon->pdata->trig_mode;
	p->psr.out_type = decon->out_type;
	p->nr_windows = decon->pdata->max_win;
}
