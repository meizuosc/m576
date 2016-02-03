/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */

#ifndef _REGS_DECON_H
#define _REGS_DECON_H

#define DSD_CFG					0x0830
#define DSD_CFG_IDMA_VG0			(1 << 13)
#define DSD_CFG_IDMA_VG1			(1 << 12)
#define DSD_CFG_IDMA_VGR0			(1 << 11)
#define DSD_CFG_IDMA_VGR1			(1 << 10)

#define DISP_CFG				0x2910
#define DISP_CFG_TV_MIPI_EN			(1 << 0)
#define DISP_CFG_UNMASK_GLOBAL			(1 << 4)

#define VIDCON0					0x0000
#define VIDCON0_SWRESET				(1 << 28)
#define VIDCON0_DECON_STOP_STATUS		(1 << 2)
#define VIDCON0_ENVID				(1 << 1)
#define VIDCON0_ENVID_F				(1 << 0)

#define VIDOUTCON0				0x0004
#define VIDOUTCON0_TV_MODE			(0x1 << 26)
#define VIDOUTCON0_LCD_DUAL_ON_F		(0x1 << 25)
#define VIDOUTCON0_LCD_ON_F			(0x1 << 24)
#define VIDOUTCON0_IF_MASK			(0x1 << 23)
#define VIDOUTCON0_RGBIF_F			(0x0 << 23)
#define VIDOUTCON0_I80IF_F			(0x1 << 23)
#define VIDOUTCON0_WB_SRC_SEL_F			(0x1 << 17)
#define VIDOUTCON0_WB_F				(0x1 << 16)

#define VCLKCON0				0x0010
#define VCLKCON0_ECLK_IDLE_GATE_EN		(1 << 12)
#define VCLKCON0_CLKVALUP			(1 << 8)
#define VCLKCON0_VLCKFREE			(1 << 0)

#define VCLKCON1				(0x0014)
#define VCLKCON2				(0x0018)
#define VCLKCON_CLKVAL_F(_v)			((_v) << 16)
#define VCLKCON_CLKVAL_F_MASK			(0xff << 16)

#define SHADOWCON				0x0030
#ifdef CONFIG_SOC_EXYNOS7580
#define SHADOWCON_WIN_PROTECT(_win)		(1 << (8 + (_win)))
#define SHADOWCON_AUTO_PROTECT			(1 << 0)
#else
#define SHADOWCON_WIN_PROTECT(_win)		(1 << (10 + (_win)))
#define SHADOWCON_STANDALONE_UPDATE_ALWAYS	(1 << 0)
#endif

#define WINCHMAP0				0x0040
#define WINCHMAP_MASK(_win)			(0x7 << ((_win) * 4))
#define WINCHMAP_DMA(_v, _win)			((_v) << ((_win) * 4))
#define WINCHMAP_VPP(_id, _v, _win)		((_v + 2 - _id) << ((_win) * 4))
#define WINCHMAP0_W5CHMAP_F(_v)			((_v) << 20)
#define WINCHMAP0_W5CHMAP_F_MASK		(0x7 << 20)
#define WINCHMAP0_W4CHMAP_F(_v)			((_v) << 16)
#define WINCHMAP0_W4CHMAP_F_MASK		(0x7 << 16)
#define WINCHMAP0_W3CHMAP_F(_v)			((_v) << 12)
#define WINCHMAP0_W3CHMAP_F_MASK		(0x7 << 12)
#define WINCHMAP0_W2CHMAP_F(_v)			((_v) << 8)
#define WINCHMAP0_W2CHMAP_F_MASK		(0x7 << 8)
#define WINCHMAP0_W1CHMAP_F(_v)			((_v) << 4)
#define WINCHMAP0_W1CHMAP_F_MASK		(0x7 << 4)
#define WINCHMAP0_W0CHMAP_F(_v)			((_v) << 0)
#define WINCHMAP0_W0CHMAP_F_MASK		(0x7 << 0)

#define WINCON(_win)				(0x0050 + ((_win) * 4))
#define WINCON_RESET_VALUE			(0x00060000)
#define WINCON_BLK_EN_F				(1 << 23)
#define WINCON_BUF_MODE_TRIPLE			(1 << 18)
#define WINCON_OUTSTAND_MAX_DEFAULT		(0x10)
#define WINCON_OUTSTAND_MAX_POS			(13)
#define WINCON_OUTSTAND_MAX_MASK		(0x1F << 13)
#ifdef CONFIG_SOC_EXYNOS7580
#define WINCON_BURSTLEN_16WORD			(0x0 << 10)
#define WINCON_BURSTLEN_8WORD			(0x1 << 10)
#define WINCON_BURSTLEN_4WORD			(0x2 << 10)
#else
#define WINCON_BURSTLEN_16WORD			(0x0 << 11)
#define WINCON_BURSTLEN_8WORD			(0x1 << 11)
#endif
#define WINCON_BLD_PLANE			(0 << 8)
#define WINCON_BLD_PIX				(1 << 8)
#define WINCON_ALPHA_MUL			(1 << 7)
#define WINCON_BPPMODE_ARGB8888			(0x0 << 2)
#define WINCON_BPPMODE_ABGR8888			(0x1 << 2)
#define WINCON_BPPMODE_RGBA8888			(0x2 << 2)
#define WINCON_BPPMODE_BGRA8888			(0x3 << 2)
#define WINCON_BPPMODE_XRGB8888			(0x4 << 2)
#define WINCON_BPPMODE_XBGR8888			(0x5 << 2)
#define WINCON_BPPMODE_RGBX8888			(0x6 << 2)
#define WINCON_BPPMODE_BGRX8888			(0x7 << 2)
#define WINCON_BPPMODE_RGB565			(0x8 << 2)
#define WINCON_BPPMODE_NV12			(0x18 << 2)
#define WINCON_BPPMODE_NV21			(0x19 << 2)
#define WINCON_ALPHA_SEL			(1 << 1)
#define WINCON_ENWIN				(1 << 0)

#define OPE_VIDW0_ADD(_win)			(0x0100 + ((_win) * 4))

#ifdef CONFIG_SOC_EXYNOS7580
#define VIDW_ADD0(_win)				(0x0880 + ((_win) * 0x10))
#define VIDW_ADD2(_win)				(0x1020 + ((_win) * 0x20))
#define VIDW_ADD3(_win)				(0x1030 + ((_win) * 0x20))

#define OPE_VIDW0_ADD2(_win)				(0x1000 + ((_win) * 8))
#define OPE_VIDW0_ADD3(_win)				(0x1004 + ((_win) * 8))
#else
#define VIDW_ADD0(_win)				(0x0080 + ((_win) * 0x10))
#define VIDW_ADD2(_win)				(0x0088 + ((_win) * 0x10))
#endif

#define VIDW_WHOLE_X(_win)			(0x0130 + ((_win) * 8))
#define VIDW_WHOLE_Y(_win)			(0x0134 + ((_win) * 8))
#define VIDW_OFFSET_X(_win)			(0x0170 + ((_win) * 8))
#define VIDW_OFFSET_Y(_win)			(0x0174 + ((_win) * 8))
#define VIDW_BLKOFFSET(_win)			(0x01B0 + ((_win) * 4))
#define VIDW_BLKSIZE(_win)			(0x0200 + ((_win) * 4))

#define VIDW_BLKOFFSET_Y_F(_v)			(((_v) & 0x1fff) << 13)
#define VIDW_BLKOFFSET_X_F(_v)			((_v) & 0x1fff)
#define VIDW_BLKOFFSET_MASK			(0x3ffffff)
#define VIDW_BLKSIZE_MASK			(0x3ffffff)
#define VIDW_BLKSIZE_H_F(_v)			(((_v) & 0x1fff) << 13)
#define VIDW_BLKSIZE_W_F(_v)			((_v) & 0x1fff)

#define VIDOSD_A(_win)				(0x0230 + ((_win) * 0x20))
#define VIDOSD_A_TOPLEFT_X(_v)			(((_v) & 0x1fff) << 13)
#define VIDOSD_A_TOPLEFT_Y(_v)			(((_v) & 0x1fff) << 0)

#define VIDOSD_B(_win)				(0x0234 + ((_win) * 0x20))
#define VIDOSD_B_BOTRIGHT_X(_v)			(((_v) & 0x1fff) << 13)
#define VIDOSD_B_BOTRIGHT_Y(_v)			(((_v) & 0x1fff) << 0)

#define VIDOSD_C(_win)				(0x0238 + ((_win) * 0x20))
#define VIDOSD_C_ALPHA0_R_F(_v)			(((_v) & 0xFF) << 16)
#define VIDOSD_C_ALPHA0_G_F(_v)			(((_v) & 0xFF) << 8)
#define VIDOSD_C_ALPHA0_B_F(_v)			(((_v) & 0xFF) << 0)

#define VIDOSD_D(_win)				(0x023C + ((_win) * 0x20))
#define VIDOSD_D_ALPHA1_R_F(_v)			(((_v) & 0xFF) << 16)
#define VIDOSD_D_ALPHA1_G_F(_v)			(((_v) & 0xFF) << 8)
#define VIDOSD_D_ALPHA1_B_F(_v)			(((_v) & 0xFF) >> 0)

#define VIDOSD_E(_win)				(0x0240 + ((_win) * 0x20))
#define VIDOSD_E_OSDSIZE_F(_v)			(((_v) & 0x7FFFFFF) << 0)

#define LOCAL_SIZE(_win)			(0x0310 + ((_win) * 4))

#define WIN_MAP(_win)				(0x0340 + ((_win) * 4))
#define WIN_MAP_MAP				(1 << 24)
#define WIN_MAP_MAP_COLOUR(_v)			((_v) << 0)
#define WIN_MAP_MAP_COLOUR_MASK			(0xffffff << 0)

#define W_KEYCON0(_win)				(0x0370 + ((_win) * 8))
#define W_KEYCON1(_win)				(0x0374 + ((_win) * 8))
#define W_KEYALPHA(_win)			(0x03A0 + ((_win) * 4))
#define BLENDE(_win)				(0x03C0 + ((_win) * 4))
#define BLENDE_COEF_ZERO			0x0
#define BLENDE_COEF_ONE				0x1
#define BLENDE_COEF_ALPHA_A			0x2
#define BLENDE_COEF_ONE_MINUS_ALPHA_A		0x3
#define BLENDE_COEF_ALPHA_B			0x4
#define BLENDE_COEF_ONE_MINUS_ALPHA_B		0x5
#define BLENDE_COEF_ALPHA0			0x6
#define BLENDE_COEF_A				0xA
#define BLENDE_COEF_ONE_MINUS_A			0xB
#define BLENDE_COEF_B				0xC
#define BLENDE_COEF_ONE_MINUS_B			0xD
#define BLENDE_Q_FUNC(_v)			((_v) << 18)
#define BLENDE_P_FUNC(_v)			((_v) << 12)
#define BLENDE_B_FUNC(_v)			((_v) << 6)
#define BLENDE_A_FUNC(_v)			((_v) << 0)

#define BLENDCON				0x03D8
#define BLENDCON_NEW_8BIT_ALPHA_VALUE		(1 << 0)
#define BLENDCON_NEW_4BIT_ALPHA_VALUE		(0 << 0)

#define VIDWB_ADD0				0x0404
#define VIDWB_WHOLE_X				0x0420
#define VIDWB_WHOLE_Y				0x0424

#define R0QoSLUT07_00				0x0448
#define R0QoSLUT15_08				0x044C
#define R1QoSLUT07_00				0x0450
#define R1QoSLUT15_08				0x0454
#define R2QoSLUT07_00				0x0458
#define R2QoSLUT15_08				0x045C
#define QoSCtrl					0X0460

#define RDMA_CON(_x)				(0x0490 + ((_x) * 0x30))
#define RDMA_BLOCK_OFFSET(_x)			(0x0494 + ((_x) * 0x30))
#define RDMA_BLOCK_SIZE(_x)			(0x0498 + ((_x) * 0x30))
#define RDMA_VBASEU(_x)				(0x049C + ((_x) * 0x30))

#define RDMA_WHOLE_SIZE(_x)			(0x04A8 + ((_x) * 0x30))
#define RDMA_IMG_OFFSET(_x)			(0x04AC + ((_x) * 0x30))
#define RDMA_IMG_SIZE(_x)			(0x04B0 + ((_x) * 0x30))
#ifdef CONFIG_SOC_EXYNOS7580
#define RDMA_FIFO_LEVEL(_x)			(0x04B4 + ((_x) * 0x30))
#else
#define RDMA_FIFO_LEVEL(_x)			(0x04BC + ((_x) * 0x30))
#endif

#define VIDINTCON0				0x0500
#define VIDINTCON0_INT_EXTRA_EN			(1 << 21)
#define VIDINTCON0_INT_I80_EN_DUAL		(1 << 18)
#define VIDINTCON0_INT_I80_EN			(1 << 17)
#define VIDINTCON0_FRAMESEL0_BACKPORCH		(0x0 << 15)
#define VIDINTCON0_FRAMESEL0_VSYNC		(0x1 << 15)
#define VIDINTCON0_FRAMESEL0_ACTIVE		(0x2 << 15)
#define VIDINTCON0_FRAMESEL0_FRONTPORCH		(0x3 << 15)
#define VIDINTCON0_INT_FRAME_DUAL		(1 << 12)
#define VIDINTCON0_INT_FRAME			(1 << 11)
#define VIDINTCON0_FIFOLEVEL_EMPTY		(0x0 << 3)
#define VIDINTCON0_FIFOLEVEL_TO25PC		(0x1 << 3)
#define VIDINTCON0_FIFOLEVEL_TO50PC		(0x2 << 3)
#define VIDINTCON0_FIFOLEVEL_FULL		(0x4 << 3)
#define VIDINTCON0_INT_FIFO_DUAL		(1 << 2)
#define VIDINTCON0_INT_FIFO			(1 << 1)
#define VIDINTCON0_INT_ENABLE			(1 << 0)

#define VIDINTCON1				0x0504
#define VIDINTCON1_FIFOEP_TH_SHIFT		10
#define VIDINTCON1_INT_DPU1			(1 << 5)
#define VIDINTCON1_INT_DPU0			(1 << 4)
#define VIDINTCON1_INT_EXTRA			(1 << 3)
#define VIDINTCON1_INT_I80			(1 << 2)
#define VIDINTCON1_INT_FRAME			(1 << 1)
#define VIDINTCON1_INT_FIFO			(1 << 0)

#define VIDINTCON2				0x0508
#define VIDINTCON2_WB_FRAME_DONE		(1 << 4)

#define VIDINTCON3				0x050C
#define VIDINTCON3_WB_FRAME_DONE		(1 << 4)

#define FRAMEFIFO_REG0				0x0510
#define FRAMEFIFO_REG1				0x0514
#define FRAMEFIFO_REG2				0x0518
#define FRAMEFIFO_REG3				0x051C

#define FRAMEFIFO_REG4				0x0520
#define FRAMEFIFO_REG5				0x0524
#define FRAMEFIFO_REG6				0x0528

#define FRAMEFIFO_REG7				0x052C
#define FRAMEFIFO_FIFO0_VALID_SIZE_GET(_v)	(((_v) >> 13) & 0x1fff)

#define FRAMEFIFO_REG8				0x0530
#define FRAMEFIFO_FIFO1_VALID_SIZE_GET(_v)	(((_v) >> 13) & 0x1fff)

#define FRAMEFIFO_STATUS			0x0534
#define FRAMEFIFO_REG9				0x0538

#define FRAMEFIFO_REG10				0x053C
#define FRAMEFIFO_WB_MODE_F			(0x1 << 26)

#define VIDCON1(_x)				(0x0600 + ((_x) * 0x50))
#define VIDCON1_LINECNT_GET(_v)			(((_v) >> 17) & 0x1fff)
#define VIDCON1_VSTATUS_MASK			(0x7 << 13)
#define VIDCON1_VSTATUS_IDLE			(0x0 << 13)
#define VIDCON1_VSTATUS_VSYNC			(0x1 << 13)
#define VIDCON1_VSTATUS_BACKPORCH		(0x2 << 13)
#define VIDCON1_VSTATUS_ACTIVE			(0x3 << 13)
#define VIDCON1_VSTATUS_FRONTPORCH		(0x4 << 13)
#define VIDCON1_VCLK_MASK			(0x3 << 9)
#define VIDCON1_VCLK_HOLD			(0x0 << 9)
#define VIDCON1_VCLK_RUN			(0x1 << 9)
#define VIDCON1_VCLK_RUN_VDEN_DISABLE		(0x3 << 9)
#define VIDCON1_RGB_ORDER_O_MASK		(0x7 << 4)
#define VIDCON1_RGB_ORDER_O_RGB			(0x0 << 4)
#define VIDCON1_RGB_ORDER_O_GBR			(0x1 << 4)
#define VIDCON1_RGB_ORDER_O_BRG			(0x2 << 4)
#define VIDCON1_RGB_ORDER_O_BGR			(0x4 << 4)
#define VIDCON1_RGB_ORDER_O_RBG			(0x5 << 4)
#define VIDCON1_RGB_ORDER_O_GRB			(0x6 << 4)

#define VIDCON2(_x)				(0x0604 + ((_x) * 0x50))
#define VIDCON3(_x)				(0x0608 + ((_x) * 0x50))

#define VIDTCON0(_x)				(0x0610 + ((_x) * 0x50))
#define VIDTCON0_VBPD(_v)			((_v) << 16)
#define VIDTCON0_VFPD(_v)			((_v) << 0)

#define VIDTCON1(_x)				(0x0614 + ((_x) * 0x50))
#define VIDTCON1_VSPW(_v)			((_v) << 16)

#define VIDTCON2(_x)				(0x0618 + ((_x) * 0x50))
#define VIDTCON2_HBPD(_v)			((_v) << 16)
#define VIDTCON2_HFPD(_v)			((_v) << 0)

#define VIDTCON3(_x)				(0x061C + ((_x) * 0x50))
#define VIDTCON3_HSPW(_v)			((_v) << 16)

#define VIDTCON4(_x)				(0x0620 + ((_x) * 0x50))
#define VIDTCON4_LINEVAL(_v)			(((_v) & 0x1fff) << 16)
#define VIDTCON4_HOZVAL(_v)			(((_v) & 0x1fff) << 0)

#define VIDTCON5(_x)				(0x0624 + ((_x) * 0x50))
#define VIDTCON5_LINEVAL(_v)			(((_v) & 0x1fff) << 16)
#define VIDTCON5_HOZVAL(_v)			(((_v) & 0x1fff) << 0)
#define VIDTCONx_LINEVAL_GET(_v)		(((_v) >> 16) & 0x1fff)
#define VIDTCONx_HOZVAL_GET(_v)			(((_v) >> 0) & 0x1fff)

#define FRAME_SIZE0(_x)				(0x0628 + ((_x) * 0x50))
#define FRAME_SIZE1(_x)				(0x062C + ((_x) * 0x50))
#define LINECNT_OP_THRESHOLD(_x)		(0x0630 + ((_x) * 0x50))

#define VIDTCON6(_x)  				(0x0638 + ((_x) * 0x4C))
#define VIDTCON6_LINEVAL(_v)			(((_v) & 0x1fff) << 16)
#define VIDTCON6_HOZVAL(_v)			(((_v) & 0x1fff) << 0)

#define VIDTCONT4				0x0690
#define VIDTCONT5				0x0694
#define FRAME_SIZET				0x0698
#define FRAME_SIZETC				0x069C

#define TRIGCON					0x06B0
#define TRIGCON_SWTRIGCMD_WB			(1 << 23)
#define TRIGCON_TRIG_SAVE_DISABLE_SYNCMGR	(1 << 13)
#define TRIGCON_HWTRIG_AUTO_MASK		(1 << 6)
#define TRIGCON_HWTRIGMASK_DISPIF1		(1 << 5)
#define TRIGCON_HWTRIGMASK_DISPIF0		(1 << 4)
#define TRIGCON_HWTRIGEN_I80_RGB		(1 << 3)
#define TRIGCON_HWTRIG_INV_I80_RGB		(1 << 2)
#define TRIGCON_SWTRIGCMD_I80_RGB		(1 << 1)
#define TRIGCON_SWTRIGEN_I80_RGB		(1 << 0)

#define CRCRDATA_E				0x06C0
#define CRCRDATA_V				0x06C4

#define CRCCTRL					0x06C8
#define CRCCTRL_CRCCLKEN			(0x1 << 2)
#define CRCCTRL_CRCSTART_F			(0x1 << 1)
#define CRCCTRL_CRCEN				(0x1 << 0)

#define ENHANCER_MIC_CTRL			0x06D0
#define ENHANCER_MIC_CTRL_MIC_ON_F		(1 << 16)
#if defined(CONFIG_EXYNOS_DECON_DPU)
#define ENHANCER_MIC_CTRL_DPU_ON_F		(1 << 12)
#define ENHANCER_MIC_CTRL_DPU_APB_CLK_GATE	(1 << 10)
#endif

#define DECON_CMU				0x0704
#define DECON_CMU_ALL_CLKGATE_ENABLE		(0x3)

#define DECON_UPDATE				0x0710
#define DECON_UPDATE_STANDALONE_F		(1 << 0)

#define DECON_CFRMID				0x0714
#define DECON_RRFRMID				0x0718

#define DECON_MIC_CON0				0x2404
#define DECON_MIC_CON0_DUMMY(_v)		(((_v) & 0x1ff) << 23)
#define DECON_MIC_CON0_WIDTH_C(_v)		(((_v) & 0x7fff) << 8)
#define DECON_MIC_CON0_MIC_ALIGN_ORDER_F	(1 << 5)
#define DECON_MIC_CON0_MIC_PIXEL_ORDER_F	(1 << 4)
#define DECON_MIC_CON0_MIC_OUTPUT_ALIGN_F	(1 << 3)
#define DECON_MIC_CON0_MIC_4x1_VC_F		(1 << 2)
#define DECON_MIC_CON0_MIC_PARA_CR_CTRL_F	(1 << 1)
#define DECON_MIC_CON0_MIC_PARA_P_UPD_EN_F	(1 << 0)

#define DECON_MIC_ENC_PARAM0			0x2410
#define DECON_MIC_ENC_PARAM1			0x2414
#define DECON_MIC_ENC_PARAM2			0x2418
#define DECON_MIC_ENC_PARAM3			0x241C

#if defined(CONFIG_EXYNOS_DECON_DPU)
#define DPU_PIXEL_COUNT_SE			0x2600
#define DPU_PIXEL_COUNT_SE_MASK			0xffffffff

#define DPU_IMG_SIZE_SE				0x2604
#define DPU_SE_HOZVAL_F				16
#define DPU_SE_LINEVAL_F			0
#define DPU_IMG_SIZE_SE_MASK			0x3fff3fff

#define DPU_VTIME0_SE				0x2608
#define DPU_VTIME0_SE_MASK			0xfff

#define DPU_VTIME1_SE				0x260C
#define DPU_SE_VSPW_F				16
#define DPU_SE_VFPD_F				0
#define DPU_VTIME1_SE_MASK			0xfff0fff

#define DPU_HTIME0_SE				0x2610
#define DPU_HTIME0_SE_MASK			0xfff

#define DPU_HTIME1_SE				0x2614
#define DPU_SE_HSPW_F				16
#define DPU_SE_HFPD_F				0
#define DPU_HTIME1_SE_MASK			0xfff0fff

#define DPU_BIT_ORDER_SE			0x2618
#define DPU_R1G1B1R0G0B0			0x0
#define DPU_B1G1R1B0G0R0			0x1
#define DPU_R0G0B0R1G1B1			0x2
#define DPU_B0G0R0B1G1R1			0x3
#define DPU_BIT_ORDER_SE_MASK			0xf
#endif

#define DECON_UPDATE_SHADOW			(DECON_UPDATE + 0x4000)

#define SHADOW_OFFSET				(0x7000)

#endif /* _REGS_DECON_H */
