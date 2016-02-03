/* linux/drivers/video/decon_display/regs-mic.h
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Jiun Yu <jiun.yu@samsung.com>
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __REGS_MIC_H__
#define __REGS_MIC_H__

/* MIC Register Map */
#define MIC_OP					0x00
#define MIC_OP_UPDATE_REG			(1 << 31)
#define MIC_OP_ON_REG				(1 << 30)
#define MIC_OP_BS_CHG_OUT			(1 << 16)
#define MIC_OP_PSR_EN				(1 << 5)
#define MIC_OP_SW_RST				(1 << 4)
#define MIC_OP_NEW_CORE				(0 << 2)
#define MIC_OP_OLD_CORE				(1 << 2)
#define MIC_OP_VIDEO_MODE			(0 << 1)
#define MIC_OP_COMMAND_MODE			(1 << 1)
#define MIC_OP_CORE_ENABLE			(1 << 0)

#define MIC_VER					0x04

#define MIC_V_TIMING_0				0x08
#define MIC_V_TIMING_0_V_PULSE_WIDTH(_x)	((_x) << 16)
#define MIC_V_TIMING_0_V_PULSE_WIDTH_MASK	(0x3fff << 16)
#define MIC_V_TIMING_0_V_PERIOD_LINE(_x)	((_x) << 0)
#define MIC_V_TIMING_0_V_PERIOD_LINE_MASK	(0x3fff << 0)

#define MIC_V_TIMING_1				0x0C
#define MIC_V_TIMING_1_VBP_SIZE(_x)		((_x) << 16)
#define MIC_V_TIMING_1_VBP_SIZE_MASK		(0x3fff << 16)
#define MIC_V_TIMING_1_VFP_SIZE(_x)		((_x) << 0)
#define MIC_V_TIMING_1_VFP_SIZE_MASK		(0x3fff << 0)

#define MIC_IMG_SIZE				0x10
#define MIC_IMG_V_SIZE(_x)			((_x) << 16)
#define MIC_IMG_V_SIZE_MASK			(0x3fff << 16)
#define MIC_IMG_H_SIZE(_x)			((_x) << 0)
#define MIC_IMG_H_SIZE_MASK			(0x3fff << 0)

#define MIC_INPUT_TIMING_0			0x14
#define MIC_INPUT_TIMING_0_H_PULSE_WIDTH(_x)	((_x) << 16)
#define MIC_INPUT_TIMING_0_H_PULSE_WIDTH_MASK	(0x3fff << 16)
#define MIC_INPUT_TIMING_0_H_PERIOD_PIXEL(_x)	((_x) << 0)
#define MIC_INPUT_TIMING_0_H_PERIOD_PIXEL_MASK	(0x3fff << 0)

#define MIC_INPUT_TIMING_1			0x18
#define MIC_INPUT_TIMING_1_HBP_SIZE(_x)		((_x) << 16)
#define MIC_INPUT_TIMING_1_HBP_SIZE_MASK	(0x3fff << 16)
#define MIC_INPUT_TIMING_1_HFP_SIZE(_x)		((_x) << 0)
#define MIC_INPUT_TIMING_1_HFP_SIZE_MASK	(0x3fff << 0)

#define MIC_2D_OUTPUT_TIMING_0			0x1C
#define MIC_2D_OUTPUT_TIMING_0_H_PULSE_WIDTH_2D(_x)	((_x) << 16)
#define MIC_2D_OUTPUT_TIMING_0_H_PERIOD_PIXEL_2D(_x)	((_x) << 0)

#define MIC_2D_OUTPUT_TIMING_1			0x20
#define MIC_2D_OUTPUT_TIMING_1_HBP_SIZE_2D(_x)	((_x) << 16)
#define MIC_2D_OUTPUT_TIMING_1_HFP_SIZE_2D(_x)	((_x) << 0)

#define MIC_2D_OUTPUT_TIMING_2			0x24

#define MIC_3D_OUTPUT_TIMING_0			0x28
#define MIC_3D_OUTPUT_TIMING_1			0x2C
#define MIC_3D_OUTPUT_TIMING_2			0x30
#define MIC_ALG_PARA_0				0x34
#define MIC_ALG_PARA_1				0x38
#define MIC_CRC_CTRL				0x40
#define MIC_CRC_DATA				0x44

#endif /* __REGS_MIC_H__ */
