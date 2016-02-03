/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
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

#if (FIMC_IS_CSI_VERSION == CSI_VERSION_0310_0100)
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
#else
/* CSIS global control */
#define S5PCSIS_CTRL					(0x00)
#define S5PCSIS_CTRL_DPDN_SWAP_CLOCK_DEFAULT		(0 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP_CLOCK			(1 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP_DATA_DEFAULT		(0 << 30)
#define S5PCSIS_CTRL_DPDN_SWAP_DATA			(1 << 30)
#define S5PCSIS_CTRL_INTERLEAVE_MODE(x)			((x & 0x3) << 22)
#define S5PCSIS_CTRL_ALIGN_32BIT			(1 << 20)
#define S5PCSIS_CTRL_UPDATE_SHADOW(x)			((1 << (x)) << 16)
#define S5PCSIS_CTRL_WCLK_EXTCLK			(1 << 8)
#define S5PCSIS_CTRL_RESET				(1 << 4)
#define S5PCSIS_CTRL_NUMOFDATALANE(x)			((x) << 2)
#define S5PCSIS_CTRL_ENABLE				(1 << 0)

/* D-PHY control */
#define S5PCSIS_DPHYCTRL				(0x04)
#define S5PCSIS_DPHYCTRL_DPHY_ON(lanes)			((~(0x1f << (lanes + 1))) & 0x1f)
#if defined(CONFIG_SOC_EXYNOS5260)
#define S5PCSIS_DPHYCTRL_HSS_MASK			(0x1f << 27)
#else
#define S5PCSIS_DPHYCTRL_HSS_MASK			(0xff << 24)
#define S5PCSIS_DPHYCTRL_CLKSETTLEMASK			(0x3 << 22)
#endif

/* Configuration */
#define S5PCSIS_CONFIG					(0x08)
#define S5PCSIS_CONFIG_CH1				(0x40)
#define S5PCSIS_CONFIG_CH2				(0x50)
#define S5PCSIS_CONFIG_CH3				(0x60)
#define S5PCSIS_CFG_LINE_INTERVAL(x)			(x << 26)
#define S5PCSIS_CFG_START_INTERVAL(x)			(x << 20)
#define S5PCSIS_CFG_END_INTERVAL(x)			(x << 8)
#define S5PCSIS_CFG_FMT_YCBCR422_8BIT			(0x1e << 2)
#define S5PCSIS_CFG_FMT_RAW8				(0x2a << 2)
#define S5PCSIS_CFG_FMT_RAW10				(0x2b << 2)
#define S5PCSIS_CFG_FMT_RAW12				(0x2c << 2)
/* User defined formats, x = 1...4 */
#define S5PCSIS_CFG_FMT_USER(x)				((0x30 + x - 1) << 2)
#define S5PCSIS_CFG_FMT_MASK				(0x3f << 2)
#define S5PCSIS_CFG_VIRTUAL_CH(x)			(x << 0)
#define S5PCSIS_CFG_NR_LANE_MASK			(3)

/* Interrupt mask. */
#define S5PCSIS_INTMSK					(0x10)
#if defined(CONFIG_SOC_EXYNOS5260)
#define S5PCSIS_INTMSK_EN_ALL				(0xfc00103f)
#else
#define S5PCSIS_INTMSK_EN_ALL				(0xf1101117)
#endif
#define S5PCSIS_INTMSK_EVEN_BEFORE			(1 << 31)
#define S5PCSIS_INTMSK_EVEN_AFTER			(1 << 30)
#define S5PCSIS_INTMSK_ODD_BEFORE			(1 << 29)
#define S5PCSIS_INTMSK_ODD_AFTER			(1 << 28)
#define S5PCSIS_INTMSK_FRAME_START_CH3			(1 << 27)
#define S5PCSIS_INTMSK_FRAME_START_CH2			(1 << 26)
#define S5PCSIS_INTMSK_FRAME_START_CH1			(1 << 25)
#define S5PCSIS_INTMSK_FRAME_START_CH0			(1 << 24)
#define S5PCSIS_INTMSK_FRAME_END_CH3			(1 << 23)
#define S5PCSIS_INTMSK_FRAME_END_CH2			(1 << 22)
#define S5PCSIS_INTMSK_FRAME_END_CH1			(1 << 21)
#define S5PCSIS_INTMSK_FRAME_END_CH0			(1 << 20)
#define S5PCSIS_INTMSK_ERR_SOT_HS			(1 << 16)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH3			(1 << 15)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH2			(1 << 14)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH1			(1 << 13)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH0			(1 << 12)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH3			(1 << 11)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH2			(1 << 10)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH1			(1 << 9)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH0			(1 << 8)
#define S5PCSIS_INTMSK_ERR_OVER_CH3			(1 << 7)
#define S5PCSIS_INTMSK_ERR_OVER_CH2			(1 << 6)
#define S5PCSIS_INTMSK_ERR_OVER_CH1			(1 << 5)
#define S5PCSIS_INTMSK_ERR_OVER_CH0			(1 << 4)
#define S5PCSIS_INTMSK_ERR_ECC				(1 << 2)
#define S5PCSIS_INTMSK_ERR_CRC				(1 << 1)
#define S5PCSIS_INTMSK_ERR_ID				(1 << 0)

/* Interrupt source */
#define S5PCSIS_INTSRC					(0x14)
#define S5PCSIS_INTSRC_EVEN_BEFORE			(1 << 31)
#define S5PCSIS_INTSRC_EVEN_AFTER			(1 << 30)
#define S5PCSIS_INTSRC_EVEN				(0x3 << 30)
#define S5PCSIS_INTSRC_ODD_BEFORE			(1 << 29)
#define S5PCSIS_INTSRC_ODD_AFTER			(1 << 28)
#define S5PCSIS_INTSRC_ODD				(0x3 << 28)
#define S5PCSIS_INTSRC_FRAME_START			(0xf << 24)
#define S5PCSIS_INTSRC_FRAME_END			(0xf << 20)
#define S5PCSIS_INTSRC_ERR_SOT_HS			(0xf << 16)
#define S5PCSIS_INTSRC_ERR_LOST_FS			(0xf << 12)
#define S5PCSIS_INTSRC_ERR_LOST_FE			(0xf << 8)
#define S5PCSIS_INTSRC_ERR_OVER				(0xf << 4)
#define S5PCSIS_INTSRC_ERR_ECC				(1 << 2)
#define S5PCSIS_INTSRC_ERR_CRC				(1 << 1)
#define S5PCSIS_INTSRC_ERR_ID				(1 << 0)
#define S5PCSIS_INTSRC_ERRORS				(0xf1111117)

/* Pixel resolution */
#define S5PCSIS_RESOL					(0x2c)
#define CSIS_MAX_PIX_WIDTH				(0xffff)
#define CSIS_MAX_PIX_HEIGHT				(0xffff)
#endif

#if (FIMC_IS_CSI_VERSION == CSI_VERSION_0310_0100)

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
	case V4L2_PIX_FMT_SBGGR16:
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
	u32 vc_src, u32 vc_dst, u32 pixelformat, u32 width, u32 height)
{
	int ret = 0;
	u32 val, format;

	switch (pixelformat) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		format = HW_FORMAT_RAW8;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		format = HW_FORMAT_RAW10;
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		format = HW_FORMAT_RAW10;
		/* HACK : format = HW_FORMAT_RAW12; */
		break;
	case V4L2_PIX_FMT_SBGGR16:
		format = HW_FORMAT_RAW10;
		/* HACK : format = HW_FORMAT_RAW12; */
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_NV21:
		format = HW_FORMAT_YUV420_8BIT;
		break;
	case V4L2_PIX_FMT_YUYV:
		format = HW_FORMAT_YUV422_8BIT;
		break;
	case V4L2_PIX_FMT_JPEG:
		format = HW_FORMAT_USER;
		break;
	default:
		err("unsupported format(%X)", pixelformat);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	switch (vc_src) {
	case CSI_VIRTUAL_CH_0:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG0));
		val = (val & ~(0x3 << 0)) | (vc_dst << 0);
		val = (val & ~(0x3f << 2)) | (format << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG0));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL0));
		break;
	case CSI_VIRTUAL_CH_1:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG1));
		val = (val & ~(0x3 << 0)) | (vc_dst << 0);
		val = (val & ~(0x3f << 2)) | (format << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG1));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL1));
		break;
	case CSI_VIRTUAL_CH_2:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG2));
		val = (val & ~(0x3 << 0)) | (vc_dst << 0);
		val = (val & ~(0x3f << 2)) | (format << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG2));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL2));
		break;
	case CSI_VIRTUAL_CH_3:
		val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG3));
		val = (val & ~(0x3 << 0)) | (vc_dst << 0);
		val = (val & ~(0x3f << 2)) | (format << 2);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CONFIG3));

		val = (width << 16) | (height << 0);
		writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_RESOL3));
		break;
	default:
		err("invalid channel(%d)", vc_src);
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

int csi_hw_enable(u32 __iomem *base_reg)
{
	u32 val;

	/* update shadow */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	val |= (0xF << 16);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	/* DPHY on */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));
	val |= (0x1f << 0);
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

	/* DPHY on */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));
	val &= ~(0x1f << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_DPHYCTRL));

	/* csi enable */
	val = readl(base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));
	val &= ~(0x1 << 0);
	writel(val, base_reg + TO_WORD_OFFSET(CSI_REG_CTRL));

	return 0;
}

#else

void s5pcsis_enable_interrupts(u32 __iomem *base_reg,
	struct fimc_is_image *image, bool on)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_INTMSK));

	val = on ? val | S5PCSIS_INTMSK_EN_ALL :
		   val & ~S5PCSIS_INTMSK_EN_ALL;

	if (image->format.field == V4L2_FIELD_INTERLACED) {
		if (on) {
			val |= S5PCSIS_INTMSK_FRAME_START_CH2;
			val |= S5PCSIS_INTMSK_FRAME_END_CH2;
		} else {
			val &= ~S5PCSIS_INTMSK_FRAME_START_CH2;
			val &= ~S5PCSIS_INTMSK_FRAME_END_CH2;
		}
	}

#if defined(CONFIG_SOC_EXYNOS5260)
	/* FIXME: hard coded, only for rhea */
	writel(0xFFF01037, base_reg + TO_WORD_OFFSET(S5PCSIS_INTMSK));
#else
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_INTMSK));
#endif
}

void s5pcsis_reset(u32 __iomem *base_reg)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));

	writel(val | S5PCSIS_CTRL_RESET, base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
	udelay(10);
}

void s5pcsis_system_enable(u32 __iomem *base_reg, int on, u32 lanes)
{
	u32 val;

	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS5422)
	val |= S5PCSIS_CTRL_WCLK_EXTCLK;
#endif

	if (on) {
		val |= S5PCSIS_CTRL_ENABLE;
		val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	} else
		val &= ~S5PCSIS_CTRL_ENABLE;
#if defined(CONFIG_SOC_EXYNOS5260)
	/* FIXME: hard coded, only for rhea */
	writel(0x0000010D, base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
#else
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
#endif

	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
	if (on)
		val |= S5PCSIS_DPHYCTRL_DPHY_ON(lanes);
	else
		val &= ~S5PCSIS_DPHYCTRL_DPHY_ON(lanes);
#if defined(CONFIG_SOC_EXYNOS5260)
	/* FIXME: hard coded, only for rhea */
	writel(0x0E00001F, base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
#else
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
#endif
}

/* Called with the state.lock mutex held */
static void __s5pcsis_set_format(u32 __iomem *base_reg,
	struct fimc_is_image *image)
{
	u32 val;

	BUG_ON(!image);

	/* Color format */
	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG));

	if (image->format.pixelformat == V4L2_PIX_FMT_SGRBG8)
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW8;
	else
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW10;

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS5422)
	val |= S5PCSIS_CFG_END_INTERVAL(1);
#endif
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG));

	/* Pixel resolution */
	val = (image->window.o_width << 16) | image->window.o_height;
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_RESOL));

	/* Output channel2 for DT */
	if (image->format.field == V4L2_FIELD_INTERLACED) {
		val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG_CH2));
		val |= S5PCSIS_CFG_VIRTUAL_CH(2);
		val |= S5PCSIS_CFG_END_INTERVAL(1);
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_USER(1);
		writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG_CH2));
	}
}

void s5pcsis_set_hsync_settle(u32 __iomem *base_reg, u32 settle)
{
	u32 val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));

	val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (settle << 24);

#if defined(CONFIG_SOC_EXYNOS5260)
	/* FIXME: hard coded, only for rhea */
	writel(0x0E00001F, base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
#else
	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_DPHYCTRL));
#endif
}

void s5pcsis_set_params(u32 __iomem *base_reg,
	struct fimc_is_image *image, u32 lanes)
{
	u32 val;

#if defined(CONFIG_SOC_EXYNOS3470)
	writel(0x000000AC, base_reg + TO_WORD_OFFSET(S5PCSIS_CONFIG)); /* only for carmen */
#endif
	__s5pcsis_set_format(base_reg, image);

	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
	val &= ~S5PCSIS_CTRL_ALIGN_32BIT;

	val |= S5PCSIS_CTRL_NUMOFDATALANE(lanes);

	/* Interleaved data */
	if (image->format.field == V4L2_FIELD_INTERLACED) {
		pr_info("set DT only\n");
		val |= S5PCSIS_CTRL_INTERLEAVE_MODE(1); /* DT only */
		val |= S5PCSIS_CTRL_UPDATE_SHADOW(2); /* ch2 shadow reg */
	}

	/* Not using external clock. */
	val &= ~S5PCSIS_CTRL_WCLK_EXTCLK;

	writel(val, base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));

	/* Update the shadow register. */
	val = readl(base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
	writel(val | S5PCSIS_CTRL_UPDATE_SHADOW(0), base_reg + TO_WORD_OFFSET(S5PCSIS_CTRL));
}
#endif
