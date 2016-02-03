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

#define CSI_REG_CTRL					(0x00)
#define CSI_REG_DPHYCTRL				(0x04)
#define CSI_REG_INTMSK					(0x10)
#define CSI_REG_INTSRC					(0x14)
#define CSI_REG_CONFIG0					(0x08)
#define CSI_REG_CONFIG1					(0x40)
#define CSI_REG_CONFIG2					(0x50)
#define CSI_REG_CONFIG3					(0x60)
#define CSI_REG_RESOL0					(0x2c)
#define CSI_REG_RESOL1					(0x44)
#define CSI_REG_RESOL2					(0x54)
#define CSI_REG_RESOL3					(0x64)
#define CSI_REG_DPHYCTRL0				(0x20)
#define CSI_REG_DPHYCTRL1				(0x24)

int csi_hw_reset(u32 __iomem *base_reg)
{
	int ret = 0;
	u32 retry = 10;
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	writel(val | (1 << 4), base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	while (--retry) {
		udelay(10);
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
		if (!(val & (1 << 4)))
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
	u32 ctrl)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL0));
	val = (val & ~(0xFFFFFFFF << 0)) | (ctrl << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL0));

	return 0;
}

int csi_hw_s_dphyctrl1(u32 __iomem *base_reg,
	u32 ctrl)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL1));
	val = (val & ~(0xFFFFFFFF << 0)) | (ctrl << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL1));

	return 0;
}

int csi_hw_s_control(u32 __iomem *base_reg,
	u32 pixelformat, u32 mode, u32 lanes)
{
	int ret = 0;
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	val = (val & ~(0x3 << 2)) | (lanes << 2);
	val = (val & ~(0x3 << 22)) | (mode << 22);

	/* all channel use extclk for wrapper clock source */
	val |= (0xF << 8);

	switch (pixelformat) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_JPEG:
		/* output width of CH0 is not 32 bits(normal output) */
		val &= ~(1 << 20);
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_YUYV:
		/* output width of CH0 is 32 bits(32bit align) */
		val |= (1 << 20);
		break;
	default:
		err("unsupported format(%X)", pixelformat);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

p_err:
	return ret;
}

int csi_hw_s_config(u32 __iomem *base_reg,
	u32 channel, struct fimc_is_vci_config *config, u32 width, u32 height)
{
	int ret = 0;
	u32 val;

	BUG_ON(!config);

	switch (channel) {
	case CSI_VIRTUAL_CH_0:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG0));
		val = (val & ~(0x3 << 0)) | (config->map << 0);
		val = (val & ~(0x3f << 2)) | (config->hwformat << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG0));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL0));
		break;
	case CSI_VIRTUAL_CH_1:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG1));
		val = (val & ~(0x3 << 0)) | (config->map << 0);
		val = (val & ~(0x3f << 2)) | (config->hwformat << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG1));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL1));
		break;
	case CSI_VIRTUAL_CH_2:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG2));
		val = (val & ~(0x3 << 0)) | (config->map << 0);
		val = (val & ~(0x3f << 2)) | (config->hwformat << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG2));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL2));
		break;
	case CSI_VIRTUAL_CH_3:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG3));
		val = (val & ~(0x3 << 0)) | (config->map << 0);
		val = (val & ~(0x3f << 2)) | (config->hwformat << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG3));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL3));
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
	val = on ? (val | 0xFFF1FFF7) : (val & ~0xFFF1FFF7);
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
	val &= ~(0x1 << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	return 0;
}
