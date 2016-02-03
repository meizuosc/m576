#ifndef _REGS_DPU_H
#define _REGS_DPU_H

#define DPU_TSC_RED(_v) 	(((_v) & 0xff) << 0)
#define DPU_TSC_GREEN(_v)	(((_v) & 0xff) << 8)
#define DPU_TSC_BLUE(_v)	(((_v) & 0xff) << 16)
#define DPU_TSC_MAGENTA(_v)	(((_v) & 0xff) << 0)
#define DPU_TSC_YELLOW(_v)	(((_v) & 0xff) << 8)
#define DPU_TSC_ONOFF(_v)	(((_v) & 0x1) << 3)
#define DPU_TSC_SCALE(_v)	(((_v) & 0xf) << 0)
#define DPU_TSC_SHIFT(_v)	(((_v) & 0x1ff) << 8)
#define DPU_TSC_GAIN(_v)	(((_v) & 0xff) << 0)

#define DPU_BRI_GAIN(_v)        (((_v) & 0x0f) << 16)

#define DPU_AD_ONOFF(_v) 	(((_v) & 0x1) << 4)
#define DPU_AD_BACKLIGHT(_v)	(((_v) & 0xffff) << 0)
#define DPU_AD_AMBIENT(_v)	(((_v) & 0xffff) << 0)

#define DPU_HUE_RED(_v) 	(((_v) & 0x1ff) << 0)
#define DPU_HUE_GREEN(_v)	(((_v) & 0x1ff) << 9)
#define DPU_HUE_BLUE(_v)	(((_v) & 0x1ff) << 18)
#define DPU_HUE_CYAN(_v)	(((_v) & 0x1ff) << 0)
#define DPU_HUE_MAGENTA(_v)	(((_v) & 0x1ff) << 9)
#define DPU_HUE_YELLOW(_v)	(((_v) & 0x1ff) << 18)
#define DPU_HUE_ONOFF(_v)	(((_v) & 0x1) << 1)

#define DPUGAMMALUT_X_Y_BASE	0x0028
#define DPUGAMMALUT_MAX		65*3
#define DPU_GAMMA_LUT_Y(_v)	(((_v) & 0x1ff) << 2)
#define DPU_GAMMA_LUT_X(_v)	(((_v) & 0x1ff) << 18)

#define DPU_AD_OPT_SEL_TRIG(_v)	(((_v) & 0xf) << 21)
#define DPU_AD_OPT_SEL_FUNC(_v)	(((_v) & 0xf) << 16)


#define DPUCON			0x0000
#define DPU_MASK_CTRL		0x500
#define DPU_MASK_CTRL_MASK	0x1

#define DPU_DITHER_ON		0
#define DPU_DITHER_ON_MASK	(0x1 << DPU_DITHER_ON)
#define DPU_SCR_ON		1
#define DPU_SCR_ON_MASK		(0x1 << DPU_SCR_ON)
#define DPU_GAMMA_ON		2
#define DPU_GAMMA_ON_MASK	(0x1 << DPU_GAMMA_ON)
#define DPU_HSC_ON		3
#define DPU_HSC_ON_MASK		(0x1 << DPU_HSC_ON)
#define DPU_AD_ON		4
#define DPU_AD_ON_MASK		(0x1 << DPU_AD_ON)

#define DPUIMG_SIZESET		0x0004
#define FRAME_WIDTH(_v)		(((_v) & 0x3FFF) << 16)
#define FRAME_WIDTH_MASK        (0x3FFF << 16)
#define FRAME_HEIGHT(_v)        (((_v) & 0x3FFF) << 0)
#define FRAME_HEIGHT_MASK       (0x3FFF << 0)
/* Contrast : Gamma */
#define DPU_GAMMA_LUT_Y_MASK	(0x1ff << 2)
#define DPU_GAMMA_LUT_X_MASK	(0x1ff << 18)

#define DPUSCR_RED              0x0008
#define DPUSCR_GREEN            0x000C
#define DPUSCR_BLUE             0x0010
#define DPUSCR_CYAN             0x0014
#define DPUSCR_MAGENTA          0x0018
#define DPUSCR_YELLOW           0x001C
#define DPUSCR_WHITE            0x0020
#define DPUSCR_BLACK            0x0024

/* Saturation : TSC */
#define DPUHSC_CONTROL		0x01B4
#define PAIM_ON 		(0x1 << 5)
#define Y_COMP_ON		(0x1 << 4)
#define TSC_ON			(0x1 << 3)
#define GP_ON			(0x1 << 2)
#define PPHC_ON 		(0x1 << 1)
#define SKIN_ON 		(0x1 << 0)

/* Saturation : Blue, Green, Red */
#define DPUHSC_PAIMGAIN2_1_0	0x01B8
#define PAIM_GAIN2		(0xff << 16)
#define PAIM_GAIN1		(0xff << 8)
#define PAIM_GAIN0		(0xff << 0)

/* Saturation : Yellow, Magenta */
#define DPUHSC_PAIMGAIN5_4_3	0x01BC
#define PAIM_GAIN5		(0xff << 16)
#define PAIM_GAIN4		(0xff << 8)
#define PAIM_GAIN3		(0xff << 0)

/* Saturation : Decreasing, Enhancement */
#define DPUHSC_PAIMSCALE_SHIFT	0x01C0
#define PAIM_SHIFT		(0x1ff << 8)
#define PAIM_SCALE		(0xf << 0)

/* Hue : Blue, Green, Red */
#define DPUHSC_PPHCGAIN2_1_0	0x01C4
#define DPU_PPHC_GAIN2_MASK	(0x1ff << 18)
#define DPU_PPHC_GAIN1_MASK	(0x1ff << 9)
#define DPU_PPHC_GAIN0_MASK	(0x1ff << 0)

/* Hue : Yellow, Magenta, Cyan */
#define DPUHSC_PPHCGAIN5_4_3	0x01C8
#define DPU_PPHC_GAIN5_MASK	(0x1ff << 18)
#define DPU_PPHC_GAIN4_MASK	(0x1ff << 9)
#define DPU_PPHC_GAIN3_MASK	(0x1ff << 0)

/* Y compensation ratio, Total saturation control gain */
#define DPUHSC_TSCGAIN_YRATIO	0x01CC
#define YCOM_RATIO		(0xf << 16)
#define TSC_GAIN		(0xff << 0)

#define DPUAD_VP_CONTROL0			0x01D0
#define DPUAD_VP_CONTROL0_LIMIT_AMPL_BRIGHT(_v)	 (((_v) & 0xF) << 28)
#define DPUAD_VP_CONTROL0_LIMIT_AMPL_DARK(_v)	(((_v) & 0xF) << 24)
#define DPUAD_VP_CONTROL0_LIMIT_AMPL(_v)			(((_v) & 0xFF) << 24)
#define DPUAD_VP_CONTROL0_LIMIT_AMPL_MASK		( 0xFF << 24)
#define DPUAD_VP_CONTROL0_VARIANCE_INTENSITY(_v)	(((_v) &0xF) << 20)
#define DPUAD_VP_CONTROL0_VARIANCE_SPACE(_v)	(((_v) &0xF) << 16)
#define DPUAD_VP_CONTROL0_VARIANCE(_v)	(((_v) &0xFF) << 16)
#define DPUAD_VP_CONTROL0_VARIANCE_MASK		(0xFF << 16)
#define DPUAD_VP_CONTROL0_CCTL(_v)			(((_v) &0x1) << 4)
#define DPUAD_VP_CONTROL0_LC2(_v)			(((_v) &0x1) << 3)
#define DPUAD_VP_CONTROL0_LC0_1(_v) 		(((_v) &0x3) << 1)
#define DPUAD_VP_CONTROL0_ENABLE(_v)		(((_v) &0x1) << 0)
#define DPUAD_VP_IRIDIX_CONTROL0(_v)		(((_v) &0x7) << 0)
#define DPUAD_VP_IRIDIX_CONTROL0_MASK (0x7 << 0)

#define DPUAD_VP_CONTROL1			0x01D4
#define DPUAD_VP_CONTROL1_WHITE_LEVEL(_v)	(((_v) &0x3FF) << 16)
#define DPUAD_VP_CONTROL1_WHITE_LEVEL_MASK (0x3FF << 16)
#define DPUAD_VP_CONTROL1_BLACK_LEVEL(_v)	(((_v) &0x3FF) << 0)
#define DPUAD_VP_CONTROL1_BLACK_LEVEL_MASK	(0x3FF << 0)

#define DPUAD_VP_CONTROL2			0x01D8
#define DPUAD_VP_CONTROL2_CONTROL_REG1(_v)	(((_v)&0x1) << 31)
#define DPUAD_VP_CONTROL2_CONTROL_REG0(_v)	(((_v)&0x3) << 16)
#define DPUAD_VP_CONTROL2_SLOPE_MAX(_v)	(((_v)&0xFF) << 8)
#define DPUAD_VP_CONTROL2_SLOPE_MAX_MASK	(0xFF << 8)
#define DPUAD_VP_CONTROL2_SLOPE_MIN(_v)	(((_v)&0xFF) << 0)
#define DPUAD_VP_CONTROL2_SLOPE_MIN_MASK	(0xFF << 0)

#define DPUAD_VP_CONTROL3			0x01DC
#define DPUAD_VP_CONTROL3_DITHER_CONTROL_STRENGTH(_v) 		(((_v)&0x3) <<26)
#define DPUAD_VP_CONTROL3_DITHER_CONTROL_SHIFT(_v) 		(((_v)&0x1) <<25)
#define DPUAD_VP_CONTROL3_DITHER_CONTROL_ENABLE(_v)		(((_v)&0x1) <<24)
#define DPUAD_VP_CONTROL3_IRIDIX_DITHER(_v) 		(((_v)&0x7) <<16)
#define DPUAD_VP_CONTROL3_IRIDIX_DITHER_MASK	(0x7 << 16)
#define DPUAD_VP_CONTROL3_LOGO_LEFT(_v) 		(((_v)&0xFF) <<8)
#define DPUAD_VP_CONTROL3_LOGO_LEFT_MASK		(0xFF <<8)
#define DPUAD_VP_CONTROL3_LOGO_TOP(_v) 		(((_v)&0xFF)<<0)
#define DPUAD_VP_CONTROL3_LOGO_TOP_MASK		(0xFF <<0)

#define DPUAD_LUT_FI(_v)		(0x1E4 + 4*_v)
#define DPUAD_LUT_CC(_v)		(0x268 + 4*_v)

#define DPUAD_NVP_CONTROL0			0x02EC
#define DPUAD_NVP_CONTROL0_CALIB_B(_v)	(((_v)&0xFFFF) << 16)
#define DPUAD_NVP_CONTROL0_CALIB_B_MASK	(0xFFFF << 16)
#define DPUAD_NVP_CONTROL0_CALIB_A(_v)	(((_v)&0xFFFF) << 0)
#define DPUAD_NVP_CONTROL0_CALIB_A_MASK	(0xFFFF << 0)

#define DPUAD_NVP_CONTROL1			0x02F0
#define DPUAD_NVP_CONTROL1_CALIB_D(_v)	(((_v)&0xFFFF) << 16)
#define DPUAD_NVP_CONTROL1_CALIB_D_MASK	(0xFFFF << 16)
#define DPUAD_NVP_CONTROL1_CALIB_C(_v)	(((_v)&0xFFFF) << 0)
#define DPUAD_NVP_CONTROL1_CALIB_C_MASK	(0xFFFF << 0)

#define DPUAD_NVP_CONTROL2			0x02F4
#define DPUAD_NVP_CONTROL2_T_FILTER_CONTROL(_v)		(((_v)&0xFF) << 24)
#define DPUAD_NVP_CONTROL2_T_FILTER_CONTROL_MASK		(0xFF << 24)
#define OPTION_SELECT_TRIGGER(_v)			(((_v)&0x7) << 21)
#define OPTION_SELECT_FUNCTION(_v)			(((_v)&0xF) << 16)
#define OPTION_SELECT_FUNCTION_MASK			(0xF << 16)
#define OPTION_SELECT(_v)				((((_v)&0xFF) | 0x80) <<16)
#define OPTION_SELECT_MASK				(0xFF <<16)
#define DPUAD_NVP_CONTROL2_STRENGHT_LIMIT(_v) 		(((_v)&0x3FF) << 0)
#define DPUAD_NVP_CONTROL2_STRENGHT_LIMIT_MASK 		(0x3FF << 0)

#define DPUAD_NVP_CONTROL3			0x02F8
#define DPUAD_NVP_CONTROL3_BACKLIGHT_MAX(_v)			(((_v)&0xFFFF) << 16)
#define DPUAD_NVP_CONTROL3_BACKLIGHT_MAX_MASK			(0xFFFF << 16)
#define DPUAD_NVP_CONTROL3_BACKLIGHT_MIN(_v)			(((_v)&0xFFFF) << 0)
#define DPUAD_NVP_CONTROL3_BACKLIGHT_MIN_MASK			(0xFFFF << 0)

#define DPUAD_NVP_CONTROL4			0x02FC
#define DPUAD_NVP_CONTROL4_AMBIENT_LIGHT_MIN(_v)			(((_v)&0xFFFF) << 16)
#define DPUAD_NVP_CONTROL4_AMBIENT_LIGHT_MIN_MASK			(0xFFFF << 16)
#define DPUAD_NVP_CONTROL4_BACKLIGHT_SCALE(_v)			(((_v)&0xFFFF) << 0)
#define DPUAD_NVP_CONTROL4_BACKLIGHT_SCALE_MASK			(0xFFFF << 0)

#define DPUAD_NVP_CONTROL5			0x0300
#define DPUAD_NVP_CONTROL5_BUFFER_GLOBAL(_v)			(((_v)&0x1)<< 26)
#define DPUAD_NVP_CONTROL5_BUFFER_MODE(_v)			(((_v)&0x3)<< 24)
#define DPUAD_NVP_CONTROL5_FILTER_B(_v)			(((_v)&0xFF)<< 16)
#define DPUAD_NVP_CONTROL5_FILTER_B_MASK		(0xFF<< 16)
#define DPUAD_NVP_CONTROL5_FILTER_A(_v)			(((_v)&0xFFFF)<< 0)
#define DPUAD_NVP_CONTROL5_FILTER_A_MASK			(0xFFFF<< 0)

#define DPUAD_AL_CALIB_LUT(_v)		(0x304 + 4*_v)

#define DPUAD_STRENGTH_MANUAL			0x0388
#define DPUADSTRENGTH_MANUAL_SET(_v)				(((_v)&0x3FF) << 0)

#define DPUAD_DRC_IN					0x038C
#define DPUAD_DRC_IN_SET(_v)							(((_v)&0xFFFF) << 0)

#define DPUAD_BACKLIGHT		0x0390
#define DPUAD_BACKLIGHT_SET(_v)	(((_v)&0x1) << 0)
#define DPUAD_BACKLIGHT_MASK	0xffff

#define DPUAD_AMBIENT_LIGHT 		0x0394
#define DPUAD_AMBIENT_LIGHT_SET 	(((_v)&0x1) << 0)
#define DPUAD_AMBIENT_LIGHT_MASK 	0xffff


#define DPUAD_STRENGTH_OUT			0x04A4
#define DPUAD_BACKLIGHT_OUT_MASK		0xFFFF
#define DPUAD_DRC_OUT					0x04A8
#define DPUAD_BACKLIGHT_OUT			0x04AC
#define DPUAD_START_CALC_DONE		0x04B0
#endif
