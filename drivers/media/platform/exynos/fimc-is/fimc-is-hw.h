/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_H
#define FIMC_IS_HW_H

#define CSI_VIRTUAL_CH_0	0
#define CSI_VIRTUAL_CH_1	1
#define CSI_VIRTUAL_CH_2	2
#define CSI_VIRTUAL_CH_3	3
#define CSI_VIRTUAL_CH_MAX	4

#define CSI_DATA_LANES_1	0
#define CSI_DATA_LANES_2	1
#define CSI_DATA_LANES_3	2
#define CSI_DATA_LANES_4	3

#define CSI_MODE_CH0_ONLY	0
#define CSI_MODE_DT_ONLY	1
#define CSI_MODE_VC_ONLY	2
#define CSI_MODE_VC_DT		3

#define HW_FORMAT_YUV420_8BIT	0x18
#define HW_FORMAT_YUV420_10BIT	0x19
#define HW_FORMAT_YUV422_8BIT	0x1E
#define HW_FORMAT_YUV422_10BIT	0x1F
#define HW_FORMAT_RGB565	0x22
#define HW_FORMAT_RGB666	0x23
#define HW_FORMAT_RGB888	0x24
#define HW_FORMAT_RAW6		0x28
#define HW_FORMAT_RAW7		0x29
#define HW_FORMAT_RAW8		0x2A
#define HW_FORMAT_RAW10		0x2B
#define HW_FORMAT_RAW12		0x2C
#define HW_FORMAT_RAW14		0x2D
#define HW_FORMAT_USER		0x30

struct fimc_is_vci {
	u32			pixelformat;
	u32			vc_map[CSI_VIRTUAL_CH_MAX];
};

int csi_hw_reset(u32 __iomem *base_reg);
int csi_hw_s_settle(u32 __iomem *base_reg, u32 settle);
int csi_hw_s_control(u32 __iomem *base_reg, u32 pixelformat, u32 mode, u32 lanes);
int csi_hw_s_config(u32 __iomem *base_reg, u32 vc_src, u32 vc_dst, u32 pixelformat, u32 width, u32 height);
int csi_hw_s_interrupt(u32 __iomem *base_reg, bool on);
int csi_hw_g_interrupt(u32 __iomem *base_reg);
int csi_hw_enable(u32 __iomem *base_reg);
int csi_hw_disable(u32 __iomem *base_reg);

int fimc_is_runtime_suspend(struct device *dev);
int fimc_is_runtime_resume(struct device *dev);
#endif
