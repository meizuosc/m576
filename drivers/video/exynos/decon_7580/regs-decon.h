/*
 * drivers/video/exynos/decon/regs-decon.h
 *
 * Register definition file for Samsung DECON driver
 *
 * Copyright (c) 2014 Samsung Electronics
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_DECON_H
#define _REGS_DECON_H

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
#define VCLKCON0_CLKVALUP			(1 << 8)
#define VCLKCON0_VLCKFREE			(1 << 0)

#define VCLKCON1				(0x0014)
#define VCLKCON2				(0x0018)
#define VCLKCON_CLKVAL_F(_v)			((_v) << 16)
#define VCLKCON_CLKVAL_F_MASK			(0xff << 16)

#define SHADOWCON				0x0030
#define SHADOWCON_WIN_PROTECT(_win)		(1 << (8 + (_win)))
#define SHADOWCON_AUTO_PROTECT			(1 << 0)

#define WINCHMAP0				0x0040
#define WINCHMAP_MASK(_win)			(0x7 << ((_win) * 4))
#define WINCHMAP_DMA(_v, _win)			((_v) << ((_win) * 4))
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
#define WINCON_BURSTLEN_16WORD			(0x0 << 10)
#define WINCON_BURSTLEN_8WORD			(0x1 << 10)
#define WINCON_BURSTLEN_4WORD			(0x2 << 10)
#define WINCON_INTERPOLATION_EN			(1 << 9)
#define WINCON_RGB_TYPE_BT601W			(0 << 26)
#define WINCON_RGB_TYPE_BT601N			(1 << 26)
#define WINCON_RGB_TYPE_BT709W			(2 << 26)
#define WINCON_RGB_TYPE_BT709N			(3 << 26)

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
/*
 * Todo: both formats are working but if 0x18 is passed for NV21
 * and 0x19 is passed as NV12. This information is reversed in
 * user manual. Need to check with Hardware team.
 */
#define WINCON_BPPMODE_NV21			(0x18 << 2)
#define WINCON_BPPMODE_NV12			(0x19 << 2)
#define WINCON_ALPHA_SEL			(1 << 1)
#define WINCON_ENWIN				(1 << 0)

#define VIDW_ADD0(_win)				(0x0880 + ((_win) * 0x10))
#define VIDW_ADD2(_win)				(0x1020 + ((_win) * 0x20))
#define VIDW_ADD3(_win)				(0x1030 + ((_win) * 0x20))

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

#define FRAMEFIFO_REG7				0x052C
#define FRAMEFIFO_FIFO0_VALID_SIZE_GET(_v)	(((_v) >> 13) & 0x1fff)

#define VIDCON1(_x)				(0x0600 + ((_x) * 0x50))
#define VIDCON1_LINECNT_GET(_v)			(((_v) >> 17) & 0x1fff)
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

#define LINECNT_OP_THRESHOLD(_x)		(0x0630 + ((_x) * 0x50))

#define VIDTCON6(_x)				(0x0638 + ((_x) * 0x4C))
#define VIDTCON6_LINEVAL(_v)			(((_v) & 0x1fff) << 16)
#define VIDTCON6_HOZVAL(_v)			(((_v) & 0x1fff) << 0)

#define TRIGCON					0x06B0
#define TRIGCON_SWTRIGCMD_WB			(1 << 23)
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

#define DECON_UPDATE				0x0710
#define DECON_UPDATE_STANDALONE_F		(1 << 0)

#endif /* _REGS_DECON_H */
