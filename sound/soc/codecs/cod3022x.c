/*
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *
 * Author: Sayanta Pattanayak <sayanta.p@samsung.com>
 *				<sayantapattanayak@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

#include "cod3022x.h"

#define AUDIO_MIC_ON_AUTO_MODE
#define COD3022x_HP_ADD_DELAY

static inline void cod3022x_usleep(unsigned int u_sec)
{
#ifdef COD33022x_HP_ADD_DELAY
	usleep_range(u_sec, u_sec + 10);
#endif
}

/**
 * Return value:
 * true: if the register value cannot be cached, hence we have to read from the
 * hardware directly
 * false: if the register value can be read from cache
 */
static bool cod3022x_volatile_register(struct device *dev, unsigned int reg)
{
	/**
	 * For the time being, there is no explicit list of registers
	 * that can be safely read from cache, hence returning true
	 * for all the registers, thereby forcing to read from
	 * hardware always.
	 */
	return true;
}

/**
 * Return value:
 * true: if the register value can be read
 * flase: if the register cannot be read
 */
static bool cod3022x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case COD3022X_01_IRQ1 ... COD3022X_0B_STATUS3:
	case COD3022X_10_PD_REF ... COD3022X_1C_SV_DA:
	case COD3022X_20_VOL_AD1 ... COD3022X_24_DSM_ADS:
	case COD3022X_30_VOL_HPL ... COD3022X_38_DCT_CLK1:
	case COD3022X_40_DIGITAL_POWER ... COD3022X_44_ADC_R_VOL:
	case COD3022X_50_DAC1 ... COD3022X_5F_SPKLIMIT3:
	case COD3022X_60_OFFSET1 ... COD3022X_62_IRQ_R:
	case COD3022X_70_CLK1_AD ... COD3022X_7A_SL_DA2:
	case COD3022X_80_DET_PDB ... COD3022X_88_KEY_TIME:
	case COD3022X_D0_CTRL_IREF1 ... COD3022X_DE_CTRL_SPKS2:
		return true;
	default:
		dev_err(dev, "Error reading invalid register\n");
		return false;
	}
}

static bool cod3022x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	/* Reg-0x09 to Reg-0x0B are read-only status registers */
	case COD3022X_01_IRQ1 ... COD3022X_08_IRQ4M:
	case COD3022X_10_PD_REF ... COD3022X_1C_SV_DA:
	case COD3022X_20_VOL_AD1 ... COD3022X_24_DSM_ADS:
	case COD3022X_30_VOL_HPL ... COD3022X_38_DCT_CLK1:
	case COD3022X_40_DIGITAL_POWER ... COD3022X_44_ADC_R_VOL:
	case COD3022X_50_DAC1 ... COD3022X_5F_SPKLIMIT3:
	/* Reg-0x61 is reserved, Reg-0x62 is read-only */
	case COD3022X_60_OFFSET1:
	case COD3022X_70_CLK1_AD ... COD3022X_7A_SL_DA2:
	case COD3022X_80_DET_PDB ... COD3022X_88_KEY_TIME:
	case COD3022X_D0_CTRL_IREF1 ... COD3022X_DE_CTRL_SPKS2:
		return true;
	default:
		dev_err(dev, "Error writing to read-only register\n");
		return false;
	}
}

const struct regmap_config cod3022x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = COD3022X_MAX_REGISTER,
	.readable_reg = cod3022x_readable_register,
	.writeable_reg = cod3022x_writeable_register,
	.volatile_reg = cod3022x_volatile_register,

	.cache_type = REGCACHE_RBTREE,
};

/**
 * TLV_DB_SCALE_ITEM
 *
 * (TLV: Threshold Limit Value)
 *
 * For various properties, the dB values don't change linearly with respect to
 * the digital value of related bit-field. At most, they are quasi-linear,
 * that means they are linear for various ranges of digital values. Following
 * table define such ranges of various properties.
 *
 * TLV_DB_RANGE_HEAD(num)
 * num defines the number of linear ranges of dB values.
 *
 * s0, e0, TLV_DB_SCALE_ITEM(min, step, mute),
 * s0: digital start value of this range (inclusive)
 * e0: digital end valeu of this range (inclusive)
 * min: dB value corresponding to s0
 * step: the delta of dB value in this range
 * mute: ?
 *
 * Example:
 *	TLV_DB_RANGE_HEAD(3),
 *	0, 1, TLV_DB_SCALE_ITEM(-2000, 2000, 0),
 *	2, 4, TLV_DB_SCALE_ITEM(1000, 1000, 0),
 *	5, 6, TLV_DB_SCALE_ITEM(3800, 8000, 0),
 *
 * The above code has 3 linear ranges with following digital-dB mapping.
 * (0...6) -> (-2000dB, 0dB, 1000dB, 2000dB, 3000dB, 3800dB, 4600dB),
 *
 * DECLARE_TLV_DB_SCALE
 *
 * This macro is used in case where there is a linear mapping between
 * the digital value and dB value.
 *
 * DECLARE_TLV_DB_SCALE(name, min, step, mute)
 *
 * name: name of this dB scale
 * min: minimum dB value corresponding to digital 0
 * step: the delta of dB value
 * mute: ?
 *
 * NOTE: The information is mostly for user-space consumption, to be viewed
 * alongwith amixer.
 */

/**
 * cod3022x_ctvol_bst_tlv
 *
 * Map: (0x0, 0dB), (0x1, 12dB), (0x2, 20dB)
 *
 * CTVOL_BST1, reg(0x20), shift(5), width(2)
 * CTVOL_BST2, reg(0x21), shift(5), width(2)
 */
static const unsigned int cod3022x_ctvol_bst_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 1, TLV_DB_SCALE_ITEM(0, 1200, 0),
	2, 2, TLV_DB_SCALE_ITEM(2000, 0, 0),
};

/**
 * cod3022x_ctvol_bst_pga_tlv
 *
 * Range: -16.5dB to +18dB, step 1.5dB
 *
 * CTVOL_BST_PGA1, reg(0x20), shift(0), width(5), invert(1), max(31)
 * CTVOL_BST_PGA2, reg(0x21), shift(0), width(5), invert(1), max(31)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_bst_pga_tlv, -1650, 150, 0);

/**
 * cod3022x_ctvol_line_tlv
 *
 * Range: -12dB to +6dB, step 3dB
 *
 * This range is used for following controls
 * CTVOL_LNL, reg(0x22), shift(4), width(3), invert(0), max(6)
 * CTVOL_LNR, reg(0x22), shift(0), width(3), invert(0), max(6)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_line_tlv, -1200, 300, 0);

/**
 * cod3022x_ctvol_hp_tlv
 *
 * Range: -57dB to +6dB, step 1dB
 *
 * CTVOL_HPL, reg(0x30), shift(0), width(6), invert(1), max(63)
 * CTVOL_HPR, reg(0x31), shift(0), width(6), invert(1), max(63)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_hp_tlv, -5700, 100, 0);

/**
 * cod3019_ctvol_ep_tlv
 *
 * Range: 0dB to +12dB, step 1dB
 *
 * CTVOL_EP, reg(0x32), shift(4), width(4), invert(0), max(12)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_ep_tlv, 0, 100, 0);

/**
 * cod3022x_ctvol_spk_pga_tlv
 *
 * Range: -6dB to +3dB, step 1dB
 *
 * CTVOL_SPK_PGA, reg(0x32), shift(0), width(4), invert(0), max(9)
 */
static const DECLARE_TLV_DB_SCALE(cod3022x_ctvol_spk_pga_tlv, -600, 100, 0);

/**
 * cod3022x_dvol_adc_tlv
 *
 * Map as per data-sheet:
 * (0x00 to 0x86) -> (+12dB to -55dB, step 0.5dB)
 * (0x87 to 0x91) -> (-56dB to -66dB, step 1dB)
 * (0x92 to 0x94) -> (-68dB to -72dB, step 2dB)
 * (0x95 to 0x96) -> (-78dB to -84dB, step 6dB)
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_ADL, reg(0x43), shift(0), width(8), invert(1), max(0x96)
 * DVOL_ADR, reg(0x44), shift(0), width(8), invert(1), max(0x96)
 * DVOL_DAL, reg(0x51), shift(0), width(8), invert(1), max(0x96)
 * DVOL_DAR, reg(0x52), shift(0), width(8), invert(1), max(0x96)
 */
static const unsigned int cod3022x_dvol_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x00, 0x01, TLV_DB_SCALE_ITEM(-8400, 600, 0),
	0x02, 0x04, TLV_DB_SCALE_ITEM(-7200, 200, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-6600, 100, 0),
	0x10, 0x96, TLV_DB_SCALE_ITEM(-5500, 50, 0),
};

/**
 * struct snd_kcontrol_new cod3022x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/**
 * All the data goes into cod3022x_snd_controls.
 * All path inter-connections goes into cod3022x_dapm_routes
 */
static const struct snd_kcontrol_new cod3022x_snd_controls[] = {
	SOC_SINGLE_TLV("MIC1 Boost Volume", COD3022X_20_VOL_AD1,
			VOLAD1_CTVOL_BST1_SHIFT,
			(BIT(VOLAD1_CTVOL_BST1_WIDTH) - 1), 0,
			cod3022x_ctvol_bst_tlv),

	SOC_SINGLE_TLV("MIC1 Volume", COD3022X_20_VOL_AD1,
			VOLAD1_CTVOL_BST_PGA1_SHIFT,
			(BIT(VOLAD1_CTVOL_BST_PGA1_WIDTH) - 1), 0,
			cod3022x_ctvol_bst_pga_tlv),

	SOC_SINGLE_TLV("MIC2 Boost Volume", COD3022X_21_VOL_AD2,
			VOLAD2_CTVOL_BST2_SHIFT,
			(BIT(VOLAD2_CTVOL_BST2_WIDTH) - 1), 0,
			cod3022x_ctvol_bst_tlv),

	SOC_SINGLE_TLV("MIC2 Volume", COD3022X_21_VOL_AD2,
			VOLAD2_CTVOL_BST_PGA2_SHIFT,
			(BIT(VOLAD2_CTVOL_BST_PGA2_WIDTH) - 1), 0,
			cod3022x_ctvol_bst_pga_tlv),

	SOC_DOUBLE_TLV("Line-in Volume", COD3022X_22_VOL_AD3,
			VOLAD3_CTVOL_LNL_SHIFT, VOLAD3_CTVOL_LNR_SHIFT,
			(BIT(VOLAD3_CTVOL_LNL_WIDTH) - 1), 0,
			cod3022x_ctvol_line_tlv),

	SOC_DOUBLE_R_TLV("Headphone Volume", COD3022X_30_VOL_HPL,
			COD3022X_31_VOL_HPR, VOLHP_CTVOL_HP_SHIFT,
			(BIT(VOLHP_CTVOL_HP_WIDTH) - 1), 1,
			cod3022x_ctvol_hp_tlv),

	SOC_SINGLE_TLV("Earphone Volume", COD3022X_32_VOL_EP_SPK,
			CTVOL_EP_SHIFT,
			(BIT(CTVOL_EP_WIDTH) - 1), 0,
			cod3022x_ctvol_ep_tlv),

	SOC_SINGLE_TLV("Speaker Volume", COD3022X_32_VOL_EP_SPK,
			CTVOL_SPK_PGA_SHIFT,
			(BIT(CTVOL_SPK_PGA_WIDTH) - 1), 0,
			cod3022x_ctvol_spk_pga_tlv),

	SOC_DOUBLE_R_TLV("ADC Gain", COD3022X_43_ADC_L_VOL,
			COD3022X_44_ADC_R_VOL, AD_DA_DVOL_SHIFT,
			AD_DA_DVOL_MAXNUM, 1, cod3022x_dvol_tlv),

	SOC_DOUBLE_R_TLV("DAC Gain", COD3022X_51_DAC_L_VOL,
			COD3022X_52_DAC_R_VOL, AD_DA_DVOL_SHIFT,
			AD_DA_DVOL_MAXNUM, 1, cod3022x_dvol_tlv),
};

static int dac_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		int event)
{
	dev_dbg(w->codec->dev, "%s called\n", __func__);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Default value of DIGITAL BLOCK */
		snd_soc_write(w->codec, COD3022X_40_DIGITAL_POWER, 0xf9);

		/* DAC digital power On */
		snd_soc_update_bits(w->codec, COD3022X_40_DIGITAL_POWER,
			PDB_DACDIG_MASK | RSTB_OVFW_DA_MASK,
			(0x1 << PDB_DACDIG_SHIFT));

		snd_soc_update_bits(w->codec, COD3022X_40_DIGITAL_POWER,
			RSTB_DAT_DA_MASK, 0x0);

		snd_soc_update_bits(w->codec, COD3022X_40_DIGITAL_POWER,
			RSTB_DAT_DA_MASK, (0x1 << RSTB_DAT_DA_SHIFT));

		snd_soc_update_bits(w->codec, COD3022X_71_CLK1_DA,
				SEL_CHCLK_DA_MASK | EN_HALF_CHOP_HP_MASK |
				EN_HALF_CHOP_DA_MASK,
				(DAC_CHOP_CLK_1_BY_32 << SEL_CHCLK_DA_SHIFT) |
			(DAC_HP_PHASE_SEL_3_BY_4 << EN_HALF_CHOP_HP_SHIFT) |
			(DAC_PHASE_SEL_1_BY_4 << EN_HALF_CHOP_DA_SHIFT));
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* Default value of DIGITAL BLOCK during power off */
		snd_soc_write(w->codec, COD3022X_40_DIGITAL_POWER, 0xe9);
		break;

	default:
		break;
	}

	return 0;
}

static int adc_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		int event)
{
	int dac_on;

	dev_dbg(w->codec->dev, "%s called\n", __func__);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;

	case SND_SOC_DAPM_POST_PMU:
		break;

	case SND_SOC_DAPM_PRE_PMD:
		dac_on = snd_soc_read(w->codec,
					COD3022X_40_DIGITAL_POWER);
		if (PDB_DACDIG_MASK & dac_on)
			snd_soc_update_bits(w->codec,
				COD3022X_40_DIGITAL_POWER,
					RSTB_DAT_AD_MASK, 0x0);
		else
			snd_soc_update_bits(w->codec,
					COD3022X_40_DIGITAL_POWER,
					RSTB_DAT_AD_MASK | RSTB_OVFW_DA_MASK,
					(0x1 << RSTB_OVFW_DA_SHIFT));
		break;

	default:
		break;
	}

	return 0;
}

static int cod3022x_capture_init(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);
	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
					PDB_ADCDIG_MASK | RSTB_OVFW_DA_MASK,
					(0x1 << PDB_ADCDIG_SHIFT));

	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
				RSTB_DAT_AD_MASK, 0x0);
	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
				RSTB_DAT_AD_MASK, (0x1 << RSTB_DAT_AD_SHIFT));

	snd_soc_update_bits(codec, COD3022X_41_FORMAT,
			DATA_WORD_LENGTH_MASK,
			(DATA_WORD_LENGTH_24 << DATA_WORD_LENGTH_SHIFT));

	/* init Capture gain control to default */
	snd_soc_write(codec, COD3022X_43_ADC_L_VOL, 0x18);
	snd_soc_write(codec, COD3022X_44_ADC_R_VOL, 0x18);

#ifndef AUDIO_MIC_ON_AUTO_MODE
	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
			0xff, 0x71);
	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
			PDB_VMID_MASK, (0x1 << PDB_VMID_SHIFT));
	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
				PDB_IGEN_MASK, (0x1 << PDB_IGEN_SHIFT));
	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
				PDB_IGEN_AD_MASK, (0x1 << PDB_IGEN_AD_SHIFT));
#endif

	return 0;
}

static int cod3022_power_on_mic1(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

#ifdef AUDIO_MIC_ON_AUTO_MODE
	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_MIC1_MASK,
			0x01 << APW_AUTO_AD_SHIFT | 0x01 << APW_MIC1_SHIFT);
	msleep(2);
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK,
		0x01 << EN_MIX_MIC1L_SHIFT | 0x01 << EN_MIX_MIC1R_SHIFT);
#else
	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
				PDB_MCB1_MASK | PDB_MCB_LDO_CODEC_MASK,
				((0x1 << PDB_MCB1_SHIFT) | 0x1));

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				0xff, 0x00);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				PDB_MIC1_MASK, 0x1 << PDB_MIC1_SHIFT);

	snd_soc_update_bits(codec, COD3022X_19_ZCD_AD,
				EN_MIC1_ZCD_MASK, 0x01 << EN_MIC1_ZCD_SHIFT);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				PDB_MIXL_MASK, 0x1 << PDB_MIXL_SHIFT);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				PDB_MIXR_MASK, 0x1 << PDB_MIXR_SHIFT);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
				PDB_RESETB_DSML_DSMR_MASK,
				PDB_RESETB_DSML_DSMR_MASK);

	snd_soc_update_bits(codec, COD3022X_20_VOL_AD1,
						0x7f, 0x48);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK,
		0x01 << EN_MIX_MIC1L_SHIFT | 0x01 << EN_MIX_MIC1R_SHIFT);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
				RESETB_DSMR_MASK | RESETB_DSML_MASK, 0x0);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
				RESETB_DSMR_MASK | RESETB_DSML_MASK,
				(0x1 << RESETB_DSML_SHIFT) |
				(0x1 << RESETB_DSMR_SHIFT));

#endif
	return 0;
}

static int cod3022_power_on_mic2(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

#ifdef AUDIO_MIC_ON_AUTO_MODE
	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_MIC2_MASK,
			0x01 << APW_AUTO_AD_SHIFT | 0x01);
	msleep(2);
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK,
		0x01 << EN_MIX_MIC2L_SHIFT | 0x01);
#else
	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
				PDB_MCB2_MASK | PDB_MCB_LDO_CODEC_MASK,
				((0x1 << PDB_MCB2_SHIFT) | 0x1));

	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
				CTRM_MCB2_MASK, 0x1 << CTRM_MCB2_SHIFT);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				0xff, 0x00);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				PDB_MIC2_MASK, 0x1 << PDB_MIC2_SHIFT);

	snd_soc_update_bits(codec, COD3022X_19_ZCD_AD,
				EN_MIC2_ZCD_MASK, 0x01 << EN_MIC2_ZCD_SHIFT);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				PDB_MIXL_MASK, 0x1 << PDB_MIXL_SHIFT);

	snd_soc_update_bits(codec, COD3022X_12_PD_AD2,
				PDB_MIXR_MASK, 0x1 << PDB_MIXR_SHIFT);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
				PDB_RESETB_DSML_DSMR_MASK,
				PDB_RESETB_DSML_DSMR_MASK);

	snd_soc_update_bits(codec, COD3022X_21_VOL_AD2,
				0xff, 0x54);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK,
		0x01 << EN_MIX_MIC2L_SHIFT | 0x01);

	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
				RESETB_DSMR_MASK | RESETB_DSML_MASK, 0x0);
	snd_soc_update_bits(codec, COD3022X_11_PD_AD1,
				RESETB_DSMR_MASK | RESETB_DSML_MASK,
				(0x1 << RESETB_DSML_SHIFT) |
				(0x1 << RESETB_DSMR_SHIFT));

#endif
	return 0;
}

static int cod3022_power_off_mic1(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

#ifdef AUDIO_MIC_ON_AUTO_MODE
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_MIC1_MASK, 0);
#else
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC1L_MASK | EN_MIX_MIC1R_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_19_ZCD_AD,
				EN_MIC1_ZCD_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
				PDB_MCB1_MASK, 0);
#endif

	return 0;
}

static int cod3022_power_off_mic2(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

#ifdef AUDIO_MIC_ON_AUTO_MODE
	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_16_PWAUTO_AD,
			APW_AUTO_AD_MASK | APW_MIC2_MASK, 0);
#else
	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
				CTRM_MCB2_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_23_MIX_AD,
		EN_MIX_MIC2L_MASK | EN_MIX_MIC2R_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_19_ZCD_AD,
				EN_MIC2_ZCD_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_18_CTRL_REF,
				CTRM_MCB2_MASK, 0);

	snd_soc_update_bits(codec, COD3022X_10_PD_REF,
				PDB_MCB2_MASK, 0);
#endif
	return 0;
}


static int vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	dev_dbg(w->codec->dev, "%s called\n", __func__);

	return 0;
}

static int mic_vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	dev_dbg(w->codec->dev, "%s called\n", __func__);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022x_capture_init(w->codec);
		break;

	case SND_SOC_DAPM_POST_PMU:
		break;

	case SND_SOC_DAPM_PRE_PMD:
		break;

	default:
		break;
	}

	return 0;
}

static int cod3022x_hp_playback_init(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD3022X_41_FORMAT,
			DATA_WORD_LENGTH_MASK,
			(DATA_WORD_LENGTH_24 << DATA_WORD_LENGTH_SHIFT));

	snd_soc_update_bits(codec, COD3022X_54_DNC1,
				DNC_START_GAIN_MASK | DNC_LIMIT_SEL_MASK,
				(0x1 << DNC_START_GAIN_SHIFT) | 0x1);

	/* set DNC Gain Level */
	snd_soc_write(codec, COD3022X_5A_DNC7, 0x18);

	/* set HP volume Level */
	snd_soc_write(codec, COD3022X_30_VOL_HPL, 0x26);
	snd_soc_write(codec, COD3022X_31_VOL_HPR, 0x26);

	snd_soc_update_bits(codec, COD3022X_36_MIX_DA1,
				EN_HP_MIXL_DCTL_MASK | EN_HP_MIXR_DCTR_MASK,
				(0x1 << EN_HP_MIXL_DCTL_SHIFT) |
				(0x1 << EN_HP_MIXR_DCTR_SHIFT));

	return 0;
}

static int spkdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	dev_dbg(w->codec->dev, "%s called\n", __func__);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(w->codec, COD3022X_32_VOL_EP_SPK,
				CTVOL_SPK_PGA_MASK, 0x3);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				PW_AUTO_DA_MASK | APW_SPK_MASK,
				(0x1 << PW_AUTO_DA_SHIFT) |
				(0x1 << APW_SPK_SHIFT));

		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_SPK_MIX_DCTL_MASK | EN_SPK_MIX_DCTR_MASK,
				(0x1 << EN_SPK_MIX_DCTL_SHIFT) |
				(0x1 << EN_SPK_MIX_DCTR_SHIFT));

		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec, COD3022X_32_VOL_EP_SPK,
				CTVOL_SPK_PGA_MASK, 0x3);

		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_SPK_MIX_DCTL_MASK | EN_SPK_MIX_DCTR_MASK, 0);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				 APW_SPK_MASK, 0);
		break;
	default:
		break;
	}

	return 0;
}

static int hpdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022x_hp_playback_init(w->codec);
		break;

	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				PW_AUTO_DA_MASK | APW_HP_MASK,
				(0x1 << PW_AUTO_DA_SHIFT) |
				(0x1 << APW_HP_SHIFT));
		msleep(10);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK, 0);
		cod3022x_usleep(100);

		snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x1E);
		snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x1E);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK,
				0x1 << EN_HP_SV_SHIFT);
		cod3022x_usleep(100);

		snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x18);
		snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x18);
		cod3022x_usleep(100);

		/* Limiter level selection -0.2dB (defult) */
		snd_soc_update_bits(w->codec, COD3022X_54_DNC1,
				EN_DNC_MASK | DNC_START_GAIN_MASK |
				DNC_LIMIT_SEL_MASK,
				((0x1 << EN_DNC_SHIFT) |
				(0x1 < DNC_START_GAIN_SHIFT) | 0x1));
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec, COD3022X_54_DNC1,
				EN_DNC_MASK , 0);
		cod3022x_usleep(100);

		snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x1E);
		snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x1E);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK, 0);
		cod3022x_usleep(100);

		snd_soc_write(w->codec, COD3022X_30_VOL_HPL, 0x26);
		snd_soc_write(w->codec, COD3022X_31_VOL_HPR, 0x26);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_1C_SV_DA,
				EN_HP_SV_MASK,
				0x1 << EN_HP_SV_SHIFT);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				PW_AUTO_DA_MASK | APW_HP_MASK, 0);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_36_MIX_DA1,
				EN_HP_MIXL_DCTL_MASK | EN_HP_MIXR_DCTR_MASK, 0);
		break;

	default:
		break;
	}

	return 0;
}

static int epdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				PW_AUTO_DA_MASK | APW_EP_MASK,
				(0X1 << PW_AUTO_DA_SHIFT) |
				(0X1 << APW_EP_SHIFT));

		/* This sequence requires a wait time of more than 135ms */
		msleep(150);

		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_EP_MIX_DCTL_MASK | EN_EP_MIX_DCTR_MASK,
				(0x1 << EN_EP_MIX_DCTL_SHIFT) |
				(0x1 << EN_EP_MIX_DCTR_SHIFT));
		cod3022x_usleep(100);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec, COD3022X_37_MIX_DA2,
				EN_EP_MIX_DCTL_MASK | EN_EP_MIX_DCTR_MASK, 0x0);
		cod3022x_usleep(100);

		snd_soc_update_bits(w->codec, COD3022X_17_PWAUTO_DA,
				PW_AUTO_DA_MASK | APW_EP_MASK, 0x0);
		cod3022x_usleep(100);
	default:
		break;
	}

	return 0;
}

static int mic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int mic_on;

	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	mic_on = snd_soc_read(w->codec, COD3022X_75_CHOP_AD);
	if (!(mic_on & EN_MCB2_CHOP_MASK)) {
		dev_dbg(w->codec->dev, "%s: MIC2 is not enabled, returning.\n",
								__func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022_power_on_mic2(w->codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod3022_power_off_mic2(w->codec);
		break;

	default:
		break;
	}

	return 0;
}

static int mic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int mic_on;

	dev_dbg(w->codec->dev, "%s called, event = %d\n", __func__, event);

	mic_on = snd_soc_read(w->codec, COD3022X_75_CHOP_AD);
	if (!(mic_on & EN_MCB1_CHOP_MASK)) {
		dev_dbg(w->codec->dev, "%s: MIC1 is not enabled, returning.\n",
								__func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod3022_power_on_mic1(w->codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod3022_power_off_mic1(w->codec);
		break;

	default:
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new adcl_mix[] = {
	SOC_DAPM_SINGLE("MIC1L Switch", COD3022X_23_MIX_AD,
			EN_MIX_MIC1L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2L Switch", COD3022X_23_MIX_AD,
			EN_MIX_MIC2L_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINEL Switch", COD3022X_23_MIX_AD,
			EN_MIX_LNLL_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new adcr_mix[] = {
	SOC_DAPM_SINGLE("MIC1R Switch", COD3022X_23_MIX_AD,
			EN_MIX_MIC1R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("MIC2R Switch", COD3022X_23_MIX_AD,
			EN_MIX_MIC2R_SHIFT, 1, 0),
	SOC_DAPM_SINGLE("LINER Switch", COD3022X_23_MIX_AD,
			EN_MIX_LNRR_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new spk_on[] = {
	SOC_DAPM_SINGLE("SPK On", COD3022X_76_CHOP_DA,
			EN_SPK_PGA_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new hp_on[] = {
	SOC_DAPM_SINGLE("HP On", COD3022X_76_CHOP_DA, EN_HP_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new ep_on[] = {
	SOC_DAPM_SINGLE("EP On", COD3022X_76_CHOP_DA, EN_EP_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new mic1_on[] = {
	SOC_DAPM_SINGLE("MIC1 On", COD3022X_75_CHOP_AD,
					EN_MCB1_CHOP_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new mic2_on[] = {
	SOC_DAPM_SINGLE("MIC2 On", COD3022X_75_CHOP_AD,
					EN_MCB2_CHOP_SHIFT, 1, 0),
};

static const struct snd_soc_dapm_widget cod3022x_dapm_widgets[] = {
	SND_SOC_DAPM_SWITCH("SPK", SND_SOC_NOPM, 0, 0, spk_on),
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM, 0, 0, hp_on),
	SND_SOC_DAPM_SWITCH("EP", SND_SOC_NOPM, 0, 0, ep_on),
	SND_SOC_DAPM_SWITCH("MIC1", SND_SOC_NOPM, 0, 0, mic1_on),
	SND_SOC_DAPM_SWITCH("MIC2", SND_SOC_NOPM, 0, 0, mic2_on),

	SND_SOC_DAPM_SUPPLY("VMID", SND_SOC_NOPM, 0, 0, vmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("MIC_VMID", SND_SOC_NOPM, 0, 0, mic_vmid_ev,
			SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_OUT_DRV_E("SPKDRV", SND_SOC_NOPM, 0, 0, NULL, 0,
			spkdrv_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUT_DRV_E("EPDRV", SND_SOC_NOPM, 0, 0, NULL, 0,
			epdrv_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0, 0, NULL, 0,
			hpdrv_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("MIC1_PGA", SND_SOC_NOPM, 0, 0,
			NULL, 0, mic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("MIC2_PGA", SND_SOC_NOPM, 0, 0,
			NULL, 0, mic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER("ADCL Mixer", SND_SOC_NOPM, 0, 0, adcl_mix,
			ARRAY_SIZE(adcl_mix)),
	SND_SOC_DAPM_MIXER("ADCR Mixer", SND_SOC_NOPM, 0, 0, adcr_mix,
			ARRAY_SIZE(adcr_mix)),

	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0,
			dac_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_DAC_E("DAC", "AIF2 Playback", SND_SOC_NOPM, 0, 0,
			dac_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_ADC_E("ADC", "AIF Capture", SND_SOC_NOPM, 0, 0,
			adc_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("ADC", "AIF2 Capture", SND_SOC_NOPM, 0, 0,
			adc_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
	SND_SOC_DAPM_OUTPUT("HPOUTLN"),
	SND_SOC_DAPM_OUTPUT("EPOUTN"),
	SND_SOC_DAPM_OUTPUT("AIF4OUT"),

	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),

	SND_SOC_DAPM_INPUT("AIF4IN"),
};

static const struct snd_soc_dapm_route cod3022x_dapm_routes[] = {
	{"SPK" , "SPK On", "SPKDRV"},
	{"SPKDRV", NULL, "DAC"},
	{"SPKOUTLN", NULL, "SPK"},
	{"EP", "EP On", "EPDRV"},
	{"EPDRV", NULL, "DAC"},
	{"EPOUTN", NULL, "EP"},

	{"DAC" , NULL, "AIF Playback"},
	{"DAC" , NULL, "AIF2 Playback"},

	{"MIC1", "MIC1 On", "MIC1_PGA"},
	{"MIC1_PGA", NULL, "MIC_VMID"},
	{"MIC1_PGA", NULL, "IN1L"},
	{"ADCL Mixer", "MIC1L Switch", "MIC1"},
	{"ADCR Mixer", "MIC1R Switch", "MIC1"},

	{"MIC2", "MIC2 On", "MIC2_PGA"},
	{"MIC2_PGA", NULL, "MIC_VMID"},
	{"MIC2_PGA", NULL, "IN2L"},
	{"ADCL Mixer", "MIC2L Switch", "MIC2"},
	{"ADCR Mixer", "MIC2R Switch", "MIC2"},

	{"ADC", NULL, "ADCL Mixer"},
	{"AIF Capture", NULL, "ADC"},
	{"AIF2 Capture", NULL, "ADC"},

	{"HP", "HP On", "HPDRV"},
	{"HPDRV", NULL, "DAC"},
	{"HPOUTLN", NULL, "HP"},
};

static int cod3022x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int bclk, lrclk;

	dev_dbg(codec->dev, "%s called\n", __func__);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
		fmt = (0x1 << LRJ_AUDIO_FORMAT_SHIFT);
		break;

	case SND_SOC_DAIFMT_I2S:
		fmt = (I2S_AUDIO_FORMAT_I2S << I2S_AUDIO_FORMAT_SHIFT);
		break;

	default:
		pr_err("Unsupported DAI format %d\n",
				fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, COD3022X_41_FORMAT, I2S_AUDIO_FORMAT_MASK |
				LRJ_AUDIO_FORMAT_MASK, fmt);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		bclk = (0x0 << BCLK_POL_SHIFT);
		lrclk = (0x0 << LRCLK_POL_SHIFT);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		bclk = (0x1 << BCLK_POL_SHIFT);
		lrclk = (0x1 << LRCLK_POL_SHIFT);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		bclk = (0x1 << BCLK_POL_SHIFT);
		lrclk = (0x0 << LRCLK_POL_SHIFT);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		bclk = (0x0 << BCLK_POL_SHIFT);
		lrclk = (0x1 << LRCLK_POL_SHIFT);
		break;
	default:
		pr_err("Unsupported Polartiy selection %d\n",
				fmt & SND_SOC_DAIFMT_INV_MASK);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, COD3022X_41_FORMAT,
			BCLK_POL_MASK | LRCLK_POL_MASK, bclk | lrclk);
	return 0;
}

static int cod3022x_analog_config(struct snd_soc_codec *codec)
{
	int ret;

	/* Analog Option Register Configuration */
	dev_dbg(codec->dev, "%s called\n", __func__);
	ret = snd_soc_update_bits(codec, COD3022X_D0_CTRL_IREF1,
			CTMI_VCM_MASK | CTMI_MIX_MASK,
			(CTMI_VCM_4U << CTMI_VCM_SHIFT) | CTMI_MIX_2U);

	ret |= snd_soc_update_bits(codec, COD3022X_D1_CTRL_IREF2,
			CTMI_INT1_MASK, CTMI_INT1_4U);

	ret |= snd_soc_update_bits(codec, COD3022X_D2_CTRL_IREF3,
			CTMI_MIC2_MASK | CTMI_MIC1_MASK,
			(CTMI_MIC2_2U << CTMI_MIC2_SHIFT) | CTMI_MIC1_2U);

	ret |= snd_soc_update_bits(codec, COD3022X_D3_CTRL_IREF4,
			CTMI_BUFF_MASK | CTMI_LN_MASK,
			(CTMI_BUFF_2U << CTMI_BUFF_SHIFT) | CTMI_LN_2U);

	return ret;
}

int cod3022x_set_externel_jd(struct snd_soc_codec *codec)
{
	int ret;

	if (codec == NULL) {
		pr_err("Initilaise codec, before calling %s\n", __func__);
		return -1;
	}

	dev_dbg(codec->dev, "%s called\n", __func__);
	/* Enable External jack detecter */
	ret = snd_soc_update_bits(codec, COD3022X_83_JACK_DET1,
					CTMP_JD_MODE_MASK,
					0x1 << CTMP_JD_MODE_SHIFT);

	/* Disable Internel Jack detecter */
	ret |= snd_soc_update_bits(codec, COD3022X_81_DET_ON,
							EN_PDB_JD_MASK, 0);

	return ret;
}

static int cod3022x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

static void cod3022x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);
}

static int cod3022x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	int ret;

	dev_dbg(codec->dev, "%s called\n", __func__);

	/*
	 * Codec supports only 24bits per sample, Mixer performs the required
	 * conversion to 24 bits. BFS is fixed at 64fs for mixer<->codec
	 * interface.
	 */
	ret = snd_soc_update_bits(codec, COD3022X_41_FORMAT,
			DATA_WORD_LENGTH_MASK,
			(DATA_WORD_LENGTH_24 << DATA_WORD_LENGTH_SHIFT));
	if (ret < 0) {
		dev_err(codec->dev, "%s failed to set bits per sample\n",
				__func__);
		return ret;
	}

	return 0;
}

static const struct snd_soc_dai_ops cod3022x_dai_ops = {
	.set_fmt = cod3022x_dai_set_fmt,
	.startup = cod3022x_dai_startup,
	.shutdown = cod3022x_dai_shutdown,
	.hw_params = cod3022x_dai_hw_params,
};

#define COD3022X_RATES		SNDRV_PCM_RATE_8000_192000

#define COD3022X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S20_3LE |	\
				SNDRV_PCM_FMTBIT_S24_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver cod3022x_dai[] = {
	{
		.name = "cod3022x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.ops = &cod3022x_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "cod3022x-aif2",
		.id = 2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = COD3022X_RATES,
			.formats = COD3022X_FORMATS,
		},
		.ops = &cod3022x_dai_ops,
		.symmetric_rates = 1,
	},
};

static int cod3022x_regulators_enable(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = regulator_enable(cod3022x->vdd1);
	if (ret) {
		dev_err(codec->dev, "failed to enable vdd1\n");
		cod3022x->vdd1 = ERR_PTR(-EINVAL);
	}

	ret = regulator_enable(cod3022x->vdd2);
	if (ret) {
		dev_err(codec->dev, "failed to enable vdd2\n");
		cod3022x->vdd2 = ERR_PTR(-EINVAL);
	}

	/**
	 * TODO: Make these regulators mandatory after the Universal DTS file
	 * has been udpated with the node information, just return ret here.
	 */
	return 0;
}

static void cod3022x_regulators_disable(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	if (!IS_ERR(cod3022x->vdd1)) {
		regulator_disable(cod3022x->vdd1);
	}

	if (!IS_ERR(cod3022x->vdd2)) {
		regulator_disable(cod3022x->vdd2);
	}
}

static int cod3022x_codec_reset(struct snd_soc_codec *codec)
{
	int ret;

	dev_dbg(codec->dev, "%s called\n", __func__);
	ret = snd_soc_write(codec, COD3022X_81_DET_ON,
			EN_PDB_JD_CLK_MASK | EN_PDB_JD_MASK);
	if (ret) {
		dev_err(codec->dev, "(*) Error writing to codec register\n");
		return ret;
	}
	mdelay(10);
	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
			SYS_RSTB_MASK, 0);
	mdelay(1);
	snd_soc_update_bits(codec, COD3022X_40_DIGITAL_POWER,
			SYS_RSTB_MASK, 1 << SYS_RSTB_SHIFT);

	/* Button period (always on clk(0)), button on 14clk, Debounce 5 */
	snd_soc_update_bits(codec, COD3022X_86_DET_TIME,
			CTMF_DETB_PERIOD_MASK | CTMF_BTN_ON_MASK |
			CTMD_BTN_DBNC_MASK,
			(0x00 << CTMF_DETB_PERIOD_SHIFT) |
			(BTN_ON_14_CLK << CTMF_BTN_ON_SHIFT) |
			(BTN_DNBC_DEBOUNCE_5));

	ret = cod3022x_analog_config(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Error in setting reset config\n");
		return ret;
	}

	snd_soc_write(codec, COD3022X_75_CHOP_AD, 0);
	snd_soc_write(codec, COD3022X_76_CHOP_DA, 0);
	mdelay(10);
	snd_soc_update_bits(codec, COD3022X_75_CHOP_AD,
				EN_MIC_CHOP_MASK |
				EN_DSM_CHOP_MASK | EN_MIX_CHOP_MASK,
				(0x01 << EN_MIC_CHOP_SHIFT) |
				(0x01 << EN_DSM_CHOP_SHIFT) |
				(0x01 << EN_MIX_CHOP_SHIFT));

	snd_soc_update_bits(codec, COD3022X_76_CHOP_DA,
				EN_DCT_CHOP_MASK | EN_SPK_CHOP_MASK,
				(0x01 << EN_DCT_CHOP_SHIFT)|
				(0x01 << EN_SPK_CHOP_SHIFT));

	return 0;
}

static int cod3022x_codec_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "(*) %s\n", __func__);

	cod3022x->codec = codec;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret)
		return ret;

	cod3022x->vdd1 = regulator_get(codec->dev, "vdd1");
	if (IS_ERR(cod3022x->vdd1)) {
		dev_warn(codec->dev, "failed to get regulator vdd1\n");
		ret = PTR_ERR(cod3022x->vdd1);
	}

	cod3022x->vdd2 = regulator_get(codec->dev, "vdd2");
	if (IS_ERR(cod3022x->vdd2)) {
		dev_warn(codec->dev, "failed to get regulator vdd2\n");
		ret = PTR_ERR(cod3022x->vdd2);
	}

	ret = cod3022x_regulators_enable(codec);
	if (ret) {
		dev_err(codec->dev, "(*) Error enabling regulators\n");
		goto err;
	}

	ret = cod3022x_codec_reset(codec);
	if (ret) {
		dev_err(codec->dev, "(*) Error reset codec\n");
		goto err;
	}

	return 0;

err:
	cod3022x_regulators_disable(codec);
	return ret;
}

static int cod3022x_codec_remove(struct snd_soc_codec *codec)
{
	struct cod3022x_priv *cod3022x = snd_soc_codec_get_drvdata(codec);
	dev_dbg(codec->dev, "(*) %s called\n", __func__);

	cod3022x_regulators_disable(codec);
	regulator_put(cod3022x->vdd1);
	regulator_put(cod3022x->vdd2);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_cod3022x = {
	.probe = cod3022x_codec_probe,
	.remove = cod3022x_codec_remove,
	.controls = cod3022x_snd_controls,
	.num_controls = ARRAY_SIZE(cod3022x_snd_controls),
	.dapm_widgets = cod3022x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cod3022x_dapm_widgets),
	.dapm_routes = cod3022x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(cod3022x_dapm_routes),
};

static int cod3022x_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct cod3022x_priv *cod3022x;
	struct pinctrl *pinctrl;
	int ret;

	cod3022x = kzalloc(sizeof(struct cod3022x_priv), GFP_KERNEL);
	if (cod3022x == NULL)
		return -ENOMEM;
	cod3022x->dev = &i2c->dev;

	cod3022x->regmap = devm_regmap_init_i2c(i2c, &cod3022x_regmap);
	if (IS_ERR(cod3022x->regmap)) {
		dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
		return PTR_ERR(cod3022x->regmap);
	}

	pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(pinctrl)) {
		dev_warn(&i2c->dev, "did not get pins for codec: %li\n",
							PTR_ERR(pinctrl));
	} else {
		cod3022x->pinctrl = pinctrl;
	}

	i2c_set_clientdata(i2c, cod3022x);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_cod3022x,
			cod3022x_dai, ARRAY_SIZE(cod3022x_dai));
	if (ret < 0)
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);

	return ret;
}

static int cod3022x_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	return 0;
}

static void cod3022x_cfg_gpio(struct device *dev, const char *name)
{
	void __iomem *gpio_va_base = NULL;

	if (dev->of_node) {
		gpio_va_base = ioremap(0x110B0040, SZ_4K);
		if (IS_ERR_OR_NULL(gpio_va_base)) {
			dev_err(dev, "Gpio registers cannot be mapped\n");
			goto err;
		}

		writel(0x22222, gpio_va_base);
		writel(0x400, gpio_va_base + 0x20);
	} else {
		dev_err(dev, "Codec gpio nodes not found\n");
	}

	iounmap(gpio_va_base);
	return;
err:
	dev_err(dev, "Unable to configure COD3022X gpio as %s\n", name);
	iounmap(gpio_va_base);
	return;
}

static int cod3022x_sys_suspend(struct device *dev)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);

	regcache_cache_only(cod3022x->regmap, true);
	cod3022x_cfg_gpio(dev, "idle");
	return 0;
}

static int cod3022x_sys_resume(struct device *dev)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);
	cod3022x_cfg_gpio(dev, "default");
	cod3022x_regulators_enable(cod3022x->codec);
	mdelay(10);

	regcache_cache_only(cod3022x->regmap, false);
	regcache_sync(cod3022x->regmap);

	cod3022x_codec_reset(cod3022x->codec);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int cod3022x_runtime_resume(struct device *dev)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);
	regcache_cache_only(cod3022x->regmap, false);
	regcache_sync(cod3022x->regmap);

	return 0;
}

static int cod3022x_runtime_suspend(struct device *dev)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);
	regcache_cache_only(cod3022x->regmap, true);

	return 0;
}
#endif


static const struct dev_pm_ops cod3022x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			cod3022x_sys_suspend,
			cod3022x_sys_resume
	)
	SET_RUNTIME_PM_OPS(
			cod3022x_runtime_suspend,
			cod3022x_runtime_resume,
			NULL
	)
};


static const struct i2c_device_id cod3022x_i2c_id[] = {
	{ "cod3022x", 3022 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cod3022x_i2c_id);

const struct of_device_id cod3022x_of_match[] = {
	{ .compatible = "codec,cod3022x",},
	{},
};

static struct i2c_driver cod3022x_i2c_driver = {
	.driver = {
		.name = "cod3022x",
		.owner = THIS_MODULE,
		.pm = &cod3022x_pm,
		.of_match_table = of_match_ptr(cod3022x_of_match),
	},
	.probe = cod3022x_i2c_probe,
	.remove = cod3022x_i2c_remove,
	.id_table = cod3022x_i2c_id,
};

module_i2c_driver(cod3022x_i2c_driver);

MODULE_DESCRIPTION("ASoC COD3022X driver");
MODULE_AUTHOR("Sayanta Pattanayak <sayanta.p@samsung.com>");
MODULE_AUTHOR("Tushar Behera <tushar.b@samsung.com>");
MODULE_AUTHOR("R Chandrasekar <rcsekar@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:COD3022X-codec");
