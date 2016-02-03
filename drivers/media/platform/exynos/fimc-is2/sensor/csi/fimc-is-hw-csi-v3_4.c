/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/exynos5-mipiphy.h>

#include "fimc-is-config.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "fimc-is-hw.h"

#define CSI_REG_CTRL				(0x04)
#define CSI_REG_CLKCTRL				(0x08)
#define CSI_REG_DPHYCTRL			(0x24)
#define CSI_REG_INTMSK				(0x10)
#define CSI_REG_INTSRC				(0x14)
#define CSI_REG_CONFIG0				(0x40)
#define CSI_REG_CONFIG1				(0x50)
#define CSI_REG_CONFIG2				(0x60)
#define CSI_REG_RESOL0				(0x44)
#define CSI_REG_RESOL1				(0x54)
#define CSI_REG_RESOL2				(0x64)
#define CSI_REG_DPHYCTRL0_H			(0x30)
#define CSI_REG_DPHYCTRL0_L			(0x34)
#define CSI_REG_DPHYCTRL1_H			(0x38)
#define CSI_REG_DPHYCTRL1_L			(0x3c)

int csi_hw_reset(u32 __iomem *base_reg)
{
	int ret = 0;
	u32 retry = 10;
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	writel(val | (1 << 1), base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	while (--retry) {
		udelay(10);
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
		if (!(val & (1 << 1)))
			break;
	}

	if (!retry) {
		err("reset is fail(%d)", retry);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int csi_hw_s_settle(u32 __iomem *base_reg,
	u32 settle)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));
	val = (val & ~(0xFF << 24)) | (settle << 24);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));

	return 0;
}

int csi_hw_s_dphyctrl0(u32 __iomem *base_reg,
	u64 ctrl)
{
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL0_L));
	val = (val & ~(0xFFFFFFFF << 0)) | (ctrl << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL0_L));

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL0_H));
	val = (val & ~(0xFFFFFFFF << 0)) | (ctrl >>16);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL0_H));

	return 0;
}

int csi_hw_s_dphyctrl1(u32 __iomem *base_reg,
	u64 ctrl)
{
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL1_L));
	val = (val & ~(0xFFFFFFFF << 0)) | (ctrl << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL1_L));

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL1_H));
	val = (val & ~(0xFFFFFFFF << 0)) | (ctrl >>16);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL1_H));


	return 0;
}

int csi_hw_s_control(u32 __iomem *base_reg,
	u32 pixelformat, u32 mode, u32 lanes)
{
	int ret = 0;
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	val = (val & ~(0x3 << 8)) | (lanes << 8);
	val = (val & ~(0x3 << 10)) | (mode << 10);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CLKCTRL));
	/* all channel use extclk for wrapper clock source */
	val |= (0xF << 0);
	/* dynamic clock gating off */
	val &= ~(0xF << 4);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CLKCTRL));

	return ret;
}

int csi_hw_s_config(u32 __iomem *base_reg,
	u32 channel, struct fimc_is_vci_config *config, u32 width, u32 height)
{
	int ret = 0;
	u32 val, parallel;

	if ((config->hwformat == HW_FORMAT_YUV420_8BIT) ||
		(config->hwformat == HW_FORMAT_YUV422_8BIT))
		parallel = 1;
	else
		parallel = 0;

	switch (channel) {
	case CSI_VIRTUAL_CH_0:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG0));
		val = (val & ~(0x3 << 0)) | (config->map << 0);
		val = (val & ~(0x3f << 2)) | (config->hwformat << 2);
		val = (val & ~(0x1 << 11)) | (parallel << 11);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG0));

		val = (width << 0) | (height << 16);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL0));
		break;
	case CSI_VIRTUAL_CH_1:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG1));
		val = (val & ~(0x3 << 0)) | (config->map << 0);
		val = (val & ~(0x3f << 2)) | (config->hwformat << 2);
		val = (val & ~(0x1 << 11)) | (parallel << 11);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG1));

		val = (width << 0) | (height << 16);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL1));
		break;
	case CSI_VIRTUAL_CH_2:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG2));
		val = (val & ~(0x3 << 0)) | (config->map << 0);
		val = (val & ~(0x3f << 2)) | (config->hwformat << 2);
		val = (val & ~(0x1 << 11)) | (parallel << 11);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG2));

		val = (width << 0) | (height << 16);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL2));
		break;
	case CSI_VIRTUAL_CH_3:
		break;
	default:
		err("invalid channel(%d)", channel);
		ret = -EINVAL;
		goto p_err;
		break;
	}

p_err:
	return ret;
}

int csi_hw_s_interrupt(u32 __iomem *base_reg, bool on)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_INTMSK));
	val = on ? 0x0FFFFFFF : 0; /* all ineterrupt enable or disable */
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_INTMSK));

	return 0;
}

int csi_hw_g_interrupt(u32 __iomem *base_reg)
{
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_INTSRC));
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_INTSRC));

	return val;
}

int csi_hw_enable(u32 __iomem *base_reg, u32 lanes)
{
	u32 val;
	u32 lane;

	switch (lanes) {
	case CSI_DATA_LANES_1:
		/* lane 0 */
		lane = (0x1 << 1) | (0x1 << 0);
		break;
	case CSI_DATA_LANES_2:
		/* lane 0 + lane 1 */
		lane = (0x3 << 1) | (0x1 << 0);
		break;
	case CSI_DATA_LANES_3:
		/* lane 0 + lane 1 + lane 2 */
		lane = (0x7 << 1) | (0x1 << 0);
		break;
	case CSI_DATA_LANES_4:
		/* lane 0 + lane 1 + lane 2 + lane 3 */
		lane = (0xF << 1) | (0x1 << 0);
		break;
	default:
		err("lanes is invalid(%d)", lanes);
		lane = (0xF << 1) | (0x1 << 0);
		break;
	}

	/* update shadow */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	val |= (0xF << 16);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	/* DPHY on */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));
	val |= (lane << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));

	/* csi enable */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	val |= (0x1 << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	return 0;
}

int csi_hw_disable(u32 __iomem *base_reg)
{
	u32 val;

	/* DPHY off */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));
	val &= ~(0x1f << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));

	/* csi disable */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	val &= ~(1 << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	return 0;
}
