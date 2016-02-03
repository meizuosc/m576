/*
 * rt5647.c  --  RT5647 ALSA SoC audio codec driver
 *
 * Copyright 2012 Realtek Semiconductor Corp.
 * Author: Bard Liao <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
// #define DEBUG
 
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define USE_RIL
#define RTK_IOCTL
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
#include "rt_codec_ioctl.h"
#include "rt5647_ioctl.h"
#endif
#endif

#include "rt5647.h"

#define RT5647_DET_EXT_MIC 0
/* #define USE_INT_CLK */
// for v02
//#define CONFIG_SND_JD1_FUNC 
//for v01
//#define CONFIG_SND_JD2_FUNC


/* #define ALC_DRC_FUNC */
#define MANGO_T5260_ASRC
//#define USE_ASRC  
/* #define USE_TDM */

#define VERSION "0.0.9 alsa 1.0.25"

static struct pm_qos_request exynos5_audio_mif_qos;

struct rt5647_init_reg {
	u8 reg;
	u16 val;
};

static struct rt5647_init_reg init_list[] = {
//viola @ 2014.4.30 : to use manual mode	
	//{RT5647_CJ_CTRL1  	, 0x0022},
	{RT5647_CJ_CTRL1  	, 0x5022},	
	{ RT5647_DIG_MISC	, 0x0121 },
	{ RT5647_ADDA_CLK1	, 0x0000 },
	{ RT5647_IL_CMD2	, 0x0010 }, /* set Inline Command Window */
	{ RT5647_PRIV_INDEX	, 0x003d },
	{ RT5647_PRIV_DATA	, 0x3600 },
#if 0
	{ RT5647_A_JD_CTRL1	, 0x0202 },/* for combo jack 1.8v */
#endif
	{ RT5647_GEN_CTRL2	, 0x0028 },
	{ RT5647_ASRC_4		, 0x0100 }, /* I2S2 tracking source = LRCK2 */
#if 0 //shoot issue: spk+hp always output simultaneously  viola@ 2014.5.6
	/* playback */
	{ RT5647_DAC_CTRL	, 0x0011 },
	{ RT5647_STO_DAC_MIXER	, 0x1616 },/* Dig inf 1 -> Sto DAC mixer -> DACL */
	{ RT5647_MONO_DAC_MIXER	, 0x4444 },
	{ RT5647_OUTMIXL_CTRL3	, 0x01fe },/* DACL1 -> OUTMIXL */
	{ RT5647_OUTMIXR_CTRL3	, 0x01fe },/* DACR1 -> OUTMIXR */
	{ RT5647_LOUT_MIXER	, 0xc000 },
	{ RT5647_LOUT1		, 0x8888 },
#if 0 /* HP direct path */
	{ RT5647_HPO_MIXER	, 0x2000 },/* DAC1 -> HPOLMIX */
#else /* HP via mixer path */
	{ RT5647_HPOMIXL_CTRL	, 0x001c },/* DAC1/2 -> HPOVOL */
	{ RT5647_HPOMIXR_CTRL	, 0x001c },/* DAC1/2 -> HPOVOL */
	{ RT5647_HPO_MIXER	, 0x4000 },/* HPOVOL -> HPOLMIX */
#endif
	{ RT5647_HP_VOL		, 0x8888 },/* OUTMIX -> HPVOL */
#if 0 /* SPK direct path */
	{ RT5647_SPO_MIXER	, 0x7803 },/* DAC1 -> SPO */
#else /* SPK via mixer path */
	{ RT5647_SPK_L_MIXER	, 0x0038 },/* DAC1/2 -> SPKVOL */
	{ RT5647_SPK_R_MIXER	, 0x0038 },/* DAC1/2 -> SPKVOL */
	{ RT5647_SPO_MIXER	, 0xd806 },/* SPKVOL -> SPO */
#endif
	{ RT5647_SPK_VOL	, 0x8888 },
#endif  //viola@ 2014.5.6

	/* record */
	{ RT5647_IN2		, 0x0200 },/* IN2 boost 24db and signal ended mode */
	{ RT5647_REC_L2_MIXER	, 0x007d },/* Mic1 -> RECMIXL */
	{ RT5647_REC_R2_MIXER	, 0x007d },/* Mic1 -> RECMIXR */
#if 1 /* DMIC1 */
	{ RT5647_STO1_ADC_MIXER	, 0x5840 },
	{ RT5647_MONO_ADC_MIXER	, 0x5858 },
#endif
#if 0 /* DMIC2 */
	{ RT5647_STO1_ADC_MIXER	, 0x5940 },
	{ RT5647_MONO_ADC_MIXER	, 0x5858 },
#endif
#if 0 /* AMIC */
	{ RT5647_STO1_ADC_MIXER	, 0x3020 },/* ADC -> Sto ADC mixer */
	{ RT5647_MONO_ADC_MIXER	, 0x3838 },
#endif
	{ RT5647_DMIC_CTRL1	, 0x1806 },
	/* { RT5647_STO1_ADC_DIG_VOL, 0xafaf }, */ /* Mute STO1 ADC for depop, Digital Input Gain */
	{ RT5647_STO1_ADC_DIG_VOL, 0xd7d7 },/* Mute STO1 ADC for depop, Digital Input Gain */
	{ RT5647_GPIO_CTRL1	, 0xc000 },
	{ RT5647_GPIO_CTRL2	, 0x0004 },
#ifdef CONFIG_SND_JD1_FUNC
	{ RT5647_IRQ_CTRL2	, 0x0280 },
	{ RT5647_A_JD_CTRL1	, 0x0001 },
	{ RT5647_MICBIAS	, 0x0008 },
//to use manual mode in V0.2 viola @ 2014.5.8 		
//	{ RT5647_GEN_CTRL3	, 0x1000 },
	{RT5647_GEN_CTRL3	, 0x0900},
#endif
#ifdef CONFIG_SND_JD2_FUNC
	{ RT5647_IRQ_CTRL2	, 0x0008 },   /*en_irq_jd2 */
	{ RT5647_MICBIAS	, 0x0008 },
//viola @ 2014.4.30 : to use manual mode	
//	{ RT5647_GEN_CTRL3	, 0x1080 },
	{RT5647_GEN_CTRL3	, 0x0900},
#endif
#ifdef MANGO_T5260_ASRC
	{RT5647_ASRC_4, 0x2000},
	{RT5647_ASRC_3, 0x0011},
	{RT5647_ASRC_2, 0x0110},
	{RT5647_ASRC_1, 0xffff},
#endif
//viola @2014.9.25: to increase hp mic recording volume
	{RT5647_CJ_CTRL3, 0x4000},

};
#define RT5647_INIT_REG_LEN ARRAY_SIZE(init_list)

#ifdef ALC_DRC_FUNC
static struct rt5647_init_reg alc_drc_list[] = {
	{ RT5647_ALC_DRC_CTRL1	, 0x0000 },
	{ RT5647_ALC_DRC_CTRL2	, 0x0000 },
	{ RT5647_ALC_CTRL_2	, 0x0000 },
	{ RT5647_ALC_CTRL_3	, 0x0000 },
	{ RT5647_ALC_CTRL_4	, 0x0000 },
	{ RT5647_ALC_CTRL_1	, 0x0000 },
};
#define RT5647_ALC_DRC_REG_LEN ARRAY_SIZE(alc_drc_list)
#endif

static int rt5647_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5647_INIT_REG_LEN; i++)
		snd_soc_write(codec, init_list[i].reg, init_list[i].val);
#ifdef ALC_DRC_FUNC
	for (i = 0; i < RT5647_ALC_DRC_REG_LEN; i++)
		snd_soc_write(codec, alc_drc_list[i].reg, alc_drc_list[i].val);
#endif

	return 0;
}

static int rt5647_index_sync(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5647_INIT_REG_LEN; i++)
		if (RT5647_PRIV_INDEX == init_list[i].reg ||
			RT5647_PRIV_DATA == init_list[i].reg)
			snd_soc_write(codec, init_list[i].reg,
					init_list[i].val);
	return 0;
}

static const u16 rt5647_reg[RT5647_VENDOR_ID2 + 1] = {
	[RT5647_HP_VOL] = 0xc8c8,
	[RT5647_SPK_VOL] = 0xc8c8,
	[RT5647_LOUT1] = 0xc8c8,
	[RT5647_MONO_OUT] = 0xc80a,
	[RT5647_CJ_CTRL1] = 0x5022,
	[RT5647_CJ_CTRL2] = 0x08a7,
	[RT5647_CJ_CTRL3] = 0x4000,
	[RT5647_INL1_INR1_VOL] = 0x0808,
	[RT5647_SIDETONE_CTRL] = 0x018b,
	[RT5647_DAC1_DIG_VOL] = 0xafaf,
	[RT5647_DAC2_DIG_VOL] = 0xafaf,
	[RT5647_DAC_CTRL] = 0x0001,
	[RT5647_STO1_ADC_DIG_VOL] = 0x2f2f,
	[RT5647_MONO_ADC_DIG_VOL] = 0x2f2f,
	[RT5647_STO1_ADC_MIXER] = 0x7060,
	[RT5647_MONO_ADC_MIXER] = 0x7070,
	[RT5647_AD_DA_MIXER] = 0x8080,
	[RT5647_STO_DAC_MIXER] = 0x5656,
	[RT5647_MONO_DAC_MIXER] = 0x5454,
	[RT5647_DIG_MIXER] = 0xaaa0,
	[RT5647_DIG_INF1_DATA] = 0x1002,
	[RT5647_PDM_OUT_CTRL] = 0x5000,
	[RT5647_REC_L2_MIXER] = 0x007f,
	[RT5647_REC_R2_MIXER] = 0x007f,
	[RT5647_HPOMIXL_CTRL] = 0x001f,
	[RT5647_HPOMIXR_CTRL] = 0x001f,
	[RT5647_HPO_MIXER] = 0x6000,
	[RT5647_SPK_L_MIXER] = 0x003e,
	[RT5647_SPK_R_MIXER] = 0x003e,
	[RT5647_SPO_MIXER] = 0xf807,
	[RT5647_SPO_CLSD_RATIO] = 0x0004,
	[RT5647_MONO_MIXER2] = 0x031f,
	[RT5647_OUTMIXL_CTRL3] = 0x01ff,
 	[RT5647_OUTMIXR_CTRL3] = 0x01ff,
	[RT5647_LOUT_MIXER] = 0xf000,
	[RT5647_HAPTIC_CTRL1] = 0x0111,
	[RT5647_HAPTIC_CTRL2] = 0x0064,
	[RT5647_HAPTIC_CTRL3] = 0xef0e,
	[RT5647_HAPTIC_CTRL4] = 0xf0f0,
	[RT5647_HAPTIC_CTRL5] = 0xef0e,
	[RT5647_HAPTIC_CTRL6] = 0xf0f0,
	[RT5647_HAPTIC_CTRL7] = 0xef0e,
	[RT5647_HAPTIC_CTRL8] = 0xf0f0,
	[RT5647_HAPTIC_CTRL9] = 0xf000,
	[RT5647_PWR_DIG1] = 0x0300,
	[RT5647_PWR_ANLG1] = 0x00c2,
	[RT5647_I2S1_SDP] = 0x8000,
	[RT5647_I2S2_SDP] = 0x8000,
	[RT5647_I2S3_SDP] = 0x8000,
	[RT5647_ADDA_CLK1] = 0x1110,
	[RT5647_ADDA_CLK2] = 0x3e00,
	[RT5647_DMIC_CTRL1] = 0x2409,
	[RT5647_DMIC_CTRL2] = 0x000a,
	[RT5647_TDM_CTRL_3] = 0x0123,
	[RT5647_ASRC_3] = 0x0003,
	[RT5647_DEPOP_M1] = 0x0004,
	[RT5647_DEPOP_M2] = 0x1100,
	[RT5647_DEPOP_M3] = 0x0646,
	[RT5647_CHARGE_PUMP] = 0x0c06,
	[RT5647_MICBIAS] = 0x3000,
	[RT5647_A_JD_CTRL1] = 0x0200,
	[RT5647_VAD_CTRL1] = 0x2184,
	[RT5647_VAD_CTRL2] = 0x010a,
	[RT5647_VAD_CTRL3] = 0x0aea,
	[RT5647_VAD_CTRL4] = 0x000c,
	[RT5647_VAD_CTRL5] = 0x0400,
	[RT5647_CLSD_OUT_CTRL] = 0xa0a8,
	[RT5647_CLSD_OUT_CTRL1] = 0x0059,
	[RT5647_CLSD_OUT_CTRL2] = 0x0001,
	[RT5647_ADC_EQ_CTRL1] = 0x6000,
	[RT5647_EQ_CTRL1] = 0x6000,
	[RT5647_ALC_DRC_CTRL2] = 0x001f,
	[RT5647_ALC_CTRL_1] = 0x020c,
	[RT5647_ALC_CTRL_2] = 0x1f00,
	[RT5647_ALC_CTRL_4] = 0x4000,
	[RT5647_INT_IRQ_ST] = 0x0180,
	[RT5647_GPIO_CTRL4] = 0x2000,
	[RT5647_BASE_BACK] = 0x1813,
	[RT5647_MP3_PLUS1] = 0x0690,
	[RT5647_MP3_PLUS2] = 0x1c17,
	[RT5647_ADJ_HPF1] = 0xb320,
	[RT5647_HP_CALIB_AMP_DET] = 0x0400,
	[RT5647_SV_ZCD1] = 0x0809,
	[RT5647_IL_CMD] = 0x0003,
	[RT5647_IL_CMD2] = 0x0049,
	[RT5647_IL_CMD3] = 0x001b,
	[RT5647_DRC1_HL_CTRL1] = 0x8000,
	[RT5647_DRC1_HL_CTRL2] = 0x0200,
	[RT5647_DRC2_HL_CTRL1] = 0x8000,
	[RT5647_DRC2_HL_CTRL2] = 0x0200,
	[RT5647_MUTI_DRC_CTRL1] = 0x0f20,
	[RT5647_ADC_MONO_HP_CTRL1] = 0xb300,
	[RT5647_DRC2_CTRL1] = 0x001f,
	[RT5647_DRC2_CTRL2] = 0x020c,
	[RT5647_DRC2_CTRL3] = 0x1f00,
	[RT5647_DRC2_CTRL5] = 0x4000,
	[RT5647_DIG_MISC] = 0x0120,
};

static int rt5647_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, RT5647_RESET, 0);
}

/**
 * rt5647_index_write - Write private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 * @value: Private register Data.
 *
 * Modify private register for advanced setting. It can be written through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5647_index_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5647_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5647_PRIV_DATA, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	return 0;

err:
	return ret;
}

/**
 * rt5647_index_read - Read private register.
 * @codec: SoC audio codec device.
 * @reg: Private register index.
 *
 * Read advanced setting from private register. It can be read through
 * private index (0x6a) and data (0x6c) register.
 *
 * Returns private register value or negative error code.
 */
static unsigned int rt5647_index_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5647_PRIV_INDEX, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		return ret;
	}
	return snd_soc_read(codec, RT5647_PRIV_DATA);
}

/**
 * rt5647_index_update_bits - update private register bits
 * @codec: audio codec
 * @reg: Private register index.
 * @mask: register mask
 * @value: new value
 *
 * Writes new register value.
 *
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int rt5647_index_update_bits(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;
	int change, ret;

	ret = rt5647_index_read(codec, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read private reg: %d\n", ret);
		goto err;
	}

	old = ret;
	new = (old & ~mask) | (value & mask);
	change = old != new;
	if (change) {
		ret = rt5647_index_write(codec, reg, new);
		if (ret < 0) {
			dev_err(codec->dev,
				"Failed to write private reg: %d\n", ret);
			goto err;
		}
	}
	return change;

err:
	return ret;
}
#if 0
static unsigned rt5647_pdm1_read(struct snd_soc_codec *codec,
		unsigned int reg)
{
	int ret;

	ret = snd_soc_write(codec, RT5647_PDM1_DATA_CTRL2, reg<<8);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5647_PDM_DATA_CTRL1, 0x0200);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	do {
		ret = snd_soc_read(codec, RT5647_PDM_DATA_CTRL1);
	} while(ret & 0x0100);
	return snd_soc_read(codec, RT5647_PDM1_DATA_CTRL4);
err:
	return ret;
}

static int rt5647_pdm1_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value)
{
	int ret;

	ret = snd_soc_write(codec, RT5647_PDM1_DATA_CTRL3, value);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private addr: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5647_PDM1_DATA_CTRL2, reg<<8);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5647_PDM_DATA_CTRL1, 0x0600);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5647_PDM_DATA_CTRL1, 0x3600);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set private value: %d\n", ret);
		goto err;
	}
	do {
		ret = snd_soc_read(codec, RT5647_PDM_DATA_CTRL1);
	} while(ret & 0x0100);
	return 0;

err:
	return ret;
}
#endif
static int rt5647_volatile_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5647_RESET:
	case RT5647_PDM_DATA_CTRL1:
	case RT5647_PDM1_DATA_CTRL4:
	case RT5647_PRIV_DATA:
	case RT5647_CJ_CTRL1:
	case RT5647_CJ_CTRL2:
	case RT5647_CJ_CTRL3:
	case RT5647_A_JD_CTRL1:
	case RT5647_VAD_CTRL5:
	case RT5647_ADC_EQ_CTRL1:
	case RT5647_EQ_CTRL1:
	case RT5647_ALC_CTRL_1:
	case RT5647_IRQ_CTRL2:
	case RT5647_IRQ_CTRL3:
	case RT5647_INT_IRQ_ST:
	case RT5647_IL_CMD:
	case RT5647_VENDOR_ID:
	case RT5647_VENDOR_ID1:
	case RT5647_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

static int rt5647_readable_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5647_RESET:
	case RT5647_SPK_VOL:
	case RT5647_HP_VOL:
	case RT5647_LOUT1:
	case RT5647_LOUT2:
	case RT5647_MONO_OUT:
	case RT5647_CJ_CTRL1:
	case RT5647_CJ_CTRL2:
	case RT5647_CJ_CTRL3:
	case RT5647_IN2:
	case RT5647_IN3:
	case RT5647_INL1_INR1_VOL:
	case RT5647_SIDETONE_CTRL:
	case RT5647_DAC1_DIG_VOL:
	case RT5647_DAC2_DIG_VOL:
	case RT5647_DAC_CTRL:
	case RT5647_STO1_ADC_DIG_VOL:
	case RT5647_MONO_ADC_DIG_VOL:
	case RT5647_ADC_BST_VOL1:
	case RT5647_ADC_BST_VOL2:
	case RT5647_STO1_ADC_MIXER:
	case RT5647_MONO_ADC_MIXER:
	case RT5647_AD_DA_MIXER:
	case RT5647_STO_DAC_MIXER:
	case RT5647_MONO_DAC_MIXER:
	case RT5647_DIG_MIXER:
	case RT5647_DIG_INF1_DATA:
	case RT5647_PDM_OUT_CTRL:
	case RT5647_PDM_DATA_CTRL1:
	case RT5647_PDM1_DATA_CTRL2:
	case RT5647_PDM1_DATA_CTRL3:
	case RT5647_PDM1_DATA_CTRL4:
	case RT5647_REC_L1_MIXER:
	case RT5647_REC_L2_MIXER:
	case RT5647_REC_R1_MIXER:
	case RT5647_REC_R2_MIXER:
	case RT5647_HPMIXL_CTRL:
	case RT5647_HPOMIXL_CTRL:
	case RT5647_HPMIXR_CTRL:
	case RT5647_HPOMIXR_CTRL:
	case RT5647_HPO_MIXER:
	case RT5647_SPK_L_MIXER:
	case RT5647_SPK_R_MIXER:
	case RT5647_SPO_MIXER:
	case RT5647_SPO_CLSD_RATIO:
	case RT5647_MONO_MIXER1:
	case RT5647_MONO_MIXER2:
	case RT5647_OUTMIXL_CTRL1:
	case RT5647_OUTMIXL_CTRL2:
	case RT5647_OUTMIXL_CTRL3:
	case RT5647_OUTMIXR_CTRL1:
	case RT5647_OUTMIXR_CTRL2:
	case RT5647_OUTMIXR_CTRL3:
	case RT5647_LOUT_MIXER:
	case RT5647_HAPTIC_CTRL1:
	case RT5647_HAPTIC_CTRL2:
	case RT5647_HAPTIC_CTRL3:
	case RT5647_HAPTIC_CTRL4:
	case RT5647_HAPTIC_CTRL5:
	case RT5647_HAPTIC_CTRL6:
	case RT5647_HAPTIC_CTRL7:
	case RT5647_HAPTIC_CTRL8:
	case RT5647_HAPTIC_CTRL9:
	case RT5647_HAPTIC_CTRL10:
	case RT5647_PWR_DIG1:
	case RT5647_PWR_DIG2:
	case RT5647_PWR_ANLG1:
	case RT5647_PWR_ANLG2:
	case RT5647_PWR_MIXER:
	case RT5647_PWR_VOL:
	case RT5647_PRIV_INDEX:
	case RT5647_PRIV_DATA:
	case RT5647_I2S1_SDP:
	case RT5647_I2S2_SDP:
	case RT5647_I2S3_SDP:
	case RT5647_ADDA_CLK1:
	case RT5647_ADDA_CLK2:
	case RT5647_DMIC_CTRL1:
	case RT5647_DMIC_CTRL2:
	case RT5647_TDM_CTRL_1:
	case RT5647_TDM_CTRL_2:
	case RT5647_TDM_CTRL_3:
	case RT5647_GLB_CLK:
	case RT5647_PLL_CTRL1:
	case RT5647_PLL_CTRL2:
	case RT5647_ASRC_1:
	case RT5647_ASRC_2:
	case RT5647_ASRC_3:
	case RT5647_ASRC_4:
	case RT5647_DEPOP_M1:
	case RT5647_DEPOP_M2:
	case RT5647_DEPOP_M3:
	case RT5647_CHARGE_PUMP:
	case RT5647_MICBIAS:
	case RT5647_A_JD_CTRL1:
	case RT5647_VAD_CTRL1:
	case RT5647_VAD_CTRL2:
	case RT5647_VAD_CTRL3:
	case RT5647_VAD_CTRL4:
	case RT5647_VAD_CTRL5:
	case RT5647_CLSD_OUT_CTRL:
	case RT5647_CLSD_OUT_CTRL1:
	case RT5647_CLSD_OUT_CTRL2:
	case RT5647_ADC_EQ_CTRL1:
	case RT5647_ADC_EQ_CTRL2:
	case RT5647_EQ_CTRL1:
	case RT5647_EQ_CTRL2:
	case RT5647_ALC_DRC_CTRL1:
	case RT5647_ALC_DRC_CTRL2:
	case RT5647_ALC_CTRL_1:
	case RT5647_ALC_CTRL_2:
	case RT5647_ALC_CTRL_3:
	case RT5647_JD_CTRL:
	case RT5647_IRQ_CTRL1:
	case RT5647_IRQ_CTRL2:
	case RT5647_IRQ_CTRL3:
	case RT5647_INT_IRQ_ST:
	case RT5647_GPIO_CTRL1:
	case RT5647_GPIO_CTRL2:
	case RT5647_GPIO_CTRL3:
	case RT5647_GPIO_CTRL4:
	case RT5647_SCRABBLE_FUN:
	case RT5647_SCRABBLE_CTRL:
	case RT5647_BASE_BACK:
	case RT5647_MP3_PLUS1:
	case RT5647_MP3_PLUS2:
	case RT5647_ADJ_HPF1:
	case RT5647_ADJ_HPF2:
	case RT5647_HP_CALIB_AMP_DET:
	case RT5647_SV_ZCD1:
	case RT5647_SV_ZCD2:
	case RT5647_IL_CMD:
	case RT5647_IL_CMD2:
	case RT5647_IL_CMD3:
	case RT5647_DRC1_HL_CTRL1:
	case RT5647_DRC1_HL_CTRL2:
	case RT5647_ADC_MONO_HP_CTRL1:
	case RT5647_ADC_MONO_HP_CTRL2:
	case RT5647_DRC2_CTRL1:
	case RT5647_DRC2_CTRL2:
	case RT5647_DRC2_CTRL3:
	case RT5647_DRC2_CTRL4:
	case RT5647_DRC2_CTRL5:
	case RT5647_JD_CTRL3:
	case RT5647_JD_CTRL4:
	case RT5647_DIG_MISC:
	case RT5647_GEN_CTRL2:
	case RT5647_GEN_CTRL3:
	case RT5647_VENDOR_ID:
	case RT5647_VENDOR_ID1:
	case RT5647_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}

void dc_calibrate(struct snd_soc_codec *codec)
{
	unsigned int sclk_src;

	sclk_src = snd_soc_read(codec, RT5647_GLB_CLK) &
		RT5647_SCLK_SRC_MASK;

	snd_soc_update_bits(codec, RT5647_PWR_ANLG2,
		RT5647_PWR_MB1, RT5647_PWR_MB1);
	snd_soc_update_bits(codec, RT5647_DEPOP_M2,
                RT5647_DEPOP_MASK, RT5647_DEPOP_MAN);
        snd_soc_update_bits(codec, RT5647_DEPOP_M1,
                RT5647_HP_CP_MASK | RT5647_HP_SG_MASK | RT5647_HP_CB_MASK,
                RT5647_HP_CP_PU | RT5647_HP_SG_DIS | RT5647_HP_CB_PU);

	snd_soc_update_bits(codec, RT5647_GLB_CLK,
		RT5647_SCLK_SRC_MASK, 0x2 << RT5647_SCLK_SRC_SFT);
        rt5647_index_write(codec, RT5647_HP_DCC_INT1, 0x9f01);
	snd_soc_update_bits(codec, RT5647_PWR_ANLG2,
		RT5647_PWR_MB1, 0);
	snd_soc_update_bits(codec, RT5647_GLB_CLK,
		RT5647_SCLK_SRC_MASK, sclk_src);
}

/**
 * rt5647_headset_detect - Detect headset.
 * @codec: SoC audio codec device.
 * @jack_insert: Jack insert or not.
 *
 * Detect whether is headset or not when jack inserted.
 *
 * Returns detect status.
 */
 #if 0
int rt5647_headset_detect(struct snd_soc_codec *codec, int jack_insert)
{
//	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	int jack_type = 0, val = 0;

	if (jack_insert) {
		snd_soc_dapm_force_enable_pin(&codec->dapm, "micbias1");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "micbias2");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "LDO2");
//		snd_soc_dapm_force_enable_pin(&codec->dapm, "JD Power");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "Mic Det Power");
//		snd_soc_dapm_force_enable_pin(&codec->dapm, "BST1");
		snd_soc_dapm_sync(&codec->dapm);
		snd_soc_write(codec, RT5647_CJ_CTRL1, 0x0006);

		snd_soc_write(codec, RT5647_JD_CTRL3, 0x00b0);
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_MN_JD, 0);
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_MN_JD, RT5647_CBJ_MN_JD);
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_DET_MODE, RT5647_CBJ_DET_MODE);
		msleep(400);
		val = snd_soc_read(codec, RT5647_CJ_CTRL3) & 0x7;

		switch (val) {
		case 0x1: /* Nokia type*/
		case 0x2: /* iPhone type*/
			jack_type = SND_JACK_HEADSET;
			break;
		default:
//			snd_soc_dapm_disable_pin(&codec->dapm, "micbias1");
//			snd_soc_dapm_disable_pin(&codec->dapm, "micbias2");
//			snd_soc_dapm_disable_pin(&codec->dapm, "LDO2");
//			snd_soc_dapm_disable_pin(&codec->dapm, "JD Power");
//			snd_soc_dapm_disable_pin(&codec->dapm, "Mic Det Power");
//			snd_soc_dapm_disable_pin(&codec->dapm, "BST1");
//			snd_soc_dapm_sync(&codec->dapm);
//			snd_soc_update_bits(codec, RT5647_INT_IRQ_ST, 0x8, 0x0);
			jack_type = SND_JACK_HEADPHONE;
			break;
		}
	} else {
		snd_soc_update_bits(codec, RT5647_INT_IRQ_ST, 0x8, 0x0);
		snd_soc_dapm_disable_pin(&codec->dapm, "micbias1");
		snd_soc_dapm_disable_pin(&codec->dapm, "micbias2");
		snd_soc_dapm_disable_pin(&codec->dapm, "LDO2");
//		snd_soc_dapm_disable_pin(&codec->dapm, "JD Power");
		snd_soc_dapm_disable_pin(&codec->dapm, "Mic Det Power");
//		snd_soc_dapm_disable_pin(&codec->dapm, "BST1");
		snd_soc_dapm_sync(&codec->dapm);
		jack_type = 0;
	}

//	rt5647->jack_type = jack_type;
	pr_info("%s:val= 0x%x jack_type=%d\n", __func__, val, jack_type);
	
	return jack_type;
}
  EXPORT_SYMBOL(rt5647_headset_detect);
 #endif
 
// need dynamically change 0bh.7 to make hoth HP playback and jacktype detection OK
 #if 0
 int rt5647_headset_detect(struct snd_soc_codec *codec, int jack_insert)
{
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	int jack_type = 0, val;

	if (jack_insert) {
		snd_soc_dapm_force_enable_pin(&codec->dapm, "micbias1");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "micbias2");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "LDO2");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "Mic Det Power");
		snd_soc_dapm_sync(&codec->dapm);
		snd_soc_write(codec, RT5647_CJ_CTRL1, 0x0006);

		snd_soc_write(codec, RT5647_JD_CTRL3, 0x00b0);
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_MN_JD, 0);
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_MN_JD, RT5647_CBJ_MN_JD);
		
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_DET_MODE, 0x0<<7);
		
		msleep(400);
		val = snd_soc_read(codec, RT5647_CJ_CTRL3) & 0x7;
		pr_debug("val=%d\n", val);

		switch (val) {
		case 0x1: /* Nokia type*/
		case 0x2: /* iPhone type*/
			jack_type = SND_JACK_HEADSET;
			break;
		default:
			snd_soc_dapm_disable_pin(&codec->dapm, "micbias1");
			snd_soc_dapm_disable_pin(&codec->dapm, "micbias2");
			snd_soc_dapm_disable_pin(&codec->dapm, "LDO2");
			snd_soc_dapm_disable_pin(&codec->dapm, "Mic Det Power");
			snd_soc_dapm_sync(&codec->dapm);
			snd_soc_update_bits(codec, RT5647_INT_IRQ_ST, 0x8, 0x0);
			snd_soc_update_bits(codec, RT5647_GEN_CTRL2, 0x8, 0x0);
			jack_type = SND_JACK_HEADPHONE;
			break;
		}
	} else {
		snd_soc_update_bits(codec, RT5647_INT_IRQ_ST, 0x8, 0x0);
		//debug
		snd_soc_update_bits(codec, RT5647_GEN_CTRL2, 0x8, 0x0);
		snd_soc_dapm_disable_pin(&codec->dapm, "micbias1");
		snd_soc_dapm_disable_pin(&codec->dapm, "micbias2");
		snd_soc_dapm_disable_pin(&codec->dapm, "LDO2");
		snd_soc_dapm_disable_pin(&codec->dapm, "Mic Det Power");
		snd_soc_dapm_sync(&codec->dapm);
		jack_type = 0;
	}

	rt5647->jack_type = jack_type;
	pr_debug("jack_type=%d\n",jack_type);

	
	//debug
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_DET_MODE, RT5647_CBJ_DET_MODE);
	return jack_type;
}
EXPORT_SYMBOL(rt5647_headset_detect);
 #endif
 //update @ 2014.4.28 by realtek 
#if 1
int rt5647_headset_detect(struct snd_soc_codec *codec, int jack_insert) 
{ 
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec); 
	int reg63, reg64,val=0; 

#ifdef CONFIG_SND_JD1_FUNC 

		printk("CONFIG_SND_JD1_FUNC\n");			
#endif
#ifdef CONFIG_SND_JD2_FUNC

		printk("CONFIG_SND_JD2_FUNC\n");
#endif

	if (jack_insert) { 
//viola @ 2014.4.29: to use manual mode
		snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_DET_MODE, 0x1<<7);
		snd_soc_dapm_force_enable_pin(&codec->dapm, "micbias1");
//		snd_soc_dapm_force_enable_pin(&codec->dapm, "micbias2");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "LDO2");
//		snd_soc_dapm_force_enable_pin(&codec->dapm, "Mic Det Power");
		snd_soc_dapm_sync(&codec->dapm);

		msleep(500); 
		reg63 = snd_soc_read(codec, RT5647_PWR_ANLG1); 
		reg64 = snd_soc_read(codec, RT5647_PWR_ANLG2);

		snd_soc_update_bits(codec, RT5647_PWR_ANLG1, 
			RT5647_PWR_MB | RT5647_PWR_BG | RT5647_LDO_SEL_MASK, 
			RT5647_PWR_MB | RT5647_PWR_BG | 0x2); 
		snd_soc_update_bits(codec, RT5647_PWR_ANLG2, 
			RT5647_PWR_MB1, RT5647_PWR_MB1); 
		snd_soc_update_bits(codec, RT5647_MICBIAS, 
			RT5647_MIC1_OVCD_MASK, RT5647_MIC1_OVCD_EN); 

		msleep(500); 

//		printk("%s: RT5647_IRQ_CTRL3 = %x \n", __func__, snd_soc_read(codec, RT5647_IRQ_CTRL3) );
		val = snd_soc_read(codec, RT5647_IRQ_CTRL3) & 0x300;
//		printk("val = %d \n", val);
		
		if (val) { 
			rt5647->jack_type = SND_JACK_HEADPHONE; 
			snd_soc_write(codec, RT5647_PWR_ANLG1, reg63); 
			snd_soc_write(codec, RT5647_PWR_ANLG2, reg64); 
		} else { 
			rt5647->jack_type = SND_JACK_HEADSET; 
		//	snd_soc_update_bits(codec, RT5647_IRQ_CTRL3, 
		//	RT5647_IRQ_MB1_OC_MASK, RT5647_IRQ_MB1_OC_NOR); 
			snd_soc_write(codec, RT5647_PWR_ANLG1, reg63); 
			snd_soc_write(codec, RT5647_PWR_ANLG2, reg64); 
		} 
	} else { 
		snd_soc_update_bits(codec, RT5647_MICBIAS, 
			RT5647_MIC1_OVCD_MASK, RT5647_MIC1_OVCD_DIS); 
		snd_soc_update_bits(codec, RT5647_IRQ_CTRL3, 
			RT5647_IRQ_MB1_OC_MASK, RT5647_IRQ_MB1_OC_BP); 

		snd_soc_dapm_disable_pin(&codec->dapm, "micbias1");
		snd_soc_dapm_sync(&codec->dapm);

		rt5647->jack_type = 0; 
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) { 
			snd_soc_write(codec, RT5647_PWR_ANLG1, 0x2000); 
			snd_soc_write(codec, RT5647_PWR_ANLG2, 0x0004); 
	} 
		}
	return rt5647->jack_type; 
}

EXPORT_SYMBOL(rt5647_headset_detect); 
#endif

void rt5647_enable_push_button_irq(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, RT5647_INT_IRQ_ST, 0x8, 0x8);
	snd_soc_update_bits(codec, RT5647_IL_CMD, 0x40, 0x40);
	snd_soc_read(codec, RT5647_IL_CMD);
}
EXPORT_SYMBOL(rt5647_enable_push_button_irq);

int rt5647_button_detect(struct snd_soc_codec *codec)
{
	int btn_type, val;

	if (!(snd_soc_read(codec, RT5647_INT_IRQ_ST) & 0x4)) 
		return 0;

	snd_soc_update_bits(codec, RT5647_IL_CMD, 0x40, 0x40);
		
	val = snd_soc_read(codec, RT5647_IL_CMD);
	btn_type = val & 0xff80;
	pr_debug("btn_type=0x%x\n",btn_type);
	snd_soc_write(codec, RT5647_IL_CMD, val);
	msleep(20);
	if (btn_type == 0 ||
		((snd_soc_read(codec, RT5647_IL_CMD) & 0xff80) == 0)) {
			pr_debug("button release\n");
			btn_type = 0;
	}
	
	return btn_type;
}
EXPORT_SYMBOL(rt5647_button_detect);

int rt5647_check_irq_event(struct snd_soc_codec *codec)
{
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	int val, ret = RT5647_UN_EVENT;

	val = snd_soc_read(codec, RT5647_A_JD_CTRL1) & 0x0020;
	pr_debug(" rt5647_check_irq_event : val = 0x%x rt5647->jack_type=0x%x\n", val, rt5647->jack_type);
	switch (val) {
	case 0x20: /* No plug */
		//rt5647->jack_type = rt5647_headset_detect(codec, 0);
		ret = RT5647_J_OUT_EVENT;
		break;
	case 0x0: /* plug */
		switch (rt5647->jack_type) {
		case 0:
			ret = RT5647_J_IN_EVENT;
			break;
		case SND_JACK_HEADSET:
			if (rt5647_button_detect(codec))
				ret = RT5647_BP_EVENT;
			else
				ret = RT5647_BR_EVENT;
			break;
		case SND_JACK_HEADPHONE:
		default:
			pr_err("There is no button event on a headphone\n");
			break;
		}
                break;
	default:
		pr_err("Unknown JD value\n");
	}
	return ret;
}
EXPORT_SYMBOL(rt5647_check_irq_event);

void rt5647_i2s2_func_switch(struct snd_soc_codec *codec, bool enable)
{
	snd_soc_update_bits(codec, RT5647_GPIO_CTRL1, 
		RT5647_I2S2_SEL, (!enable) << RT5647_I2S2_SEL_SFT);
}
EXPORT_SYMBOL(rt5647_i2s2_func_switch);

static const DECLARE_TLV_DB_SCALE(out_vol_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -65625, 375, 0);
static const DECLARE_TLV_DB_SCALE(dac_gain_tlv, -900, 300, 0);
static const DECLARE_TLV_DB_SCALE(out_gain_tlv, -600, 600, 0);
static const DECLARE_TLV_DB_SCALE(in_vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -17625, 375, 0);
static const DECLARE_TLV_DB_SCALE(adc_bst_tlv, 0, 1200, 0);
static const DECLARE_TLV_DB_SCALE(adc_com_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(mix_bst_tlv, -1800, 300, 0);
static const DECLARE_TLV_DB_SCALE(bb_bst_tlv, 0, 150, 0);

/* {0, +20, +24, +30, +35, +40, +44, +50, +52} dB */
static unsigned int bst_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(2000, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(2400, 0, 0),
	3, 5, TLV_DB_SCALE_ITEM(3000, 500, 0),
	6, 6, TLV_DB_SCALE_ITEM(4400, 0, 0),
	7, 7, TLV_DB_SCALE_ITEM(5000, 0, 0),
	8, 8, TLV_DB_SCALE_ITEM(5200, 0, 0),
};

/* {-6dB, -4.5dB, -3dB, -1.5dB, 0dB, 0.83dB, 1.58dB, 2.28dB} */
static unsigned int speaker_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0, 4, TLV_DB_SCALE_ITEM(-6000, 1500, 0),
	5, 5, TLV_DB_SCALE_ITEM(830, 0, 0),
	6, 6, TLV_DB_SCALE_ITEM(1580, 0, 0),
	7, 7, TLV_DB_SCALE_ITEM(2280, 0, 0),
};

/* {-11.625, -10.5, -9, -6.75, -4.5, -3, -1.875, -0.375, 0.375, 1.5
    2.25, 3, 3,75, 4.5, 4.875, 5.625, 6, 6.375, 7.125, 7.5} dB */
static unsigned int tt_bst_tlv[] = {
	TLV_DB_RANGE_HEAD(20),
	0, 0, TLV_DB_SCALE_ITEM(-1162, 0, 0),
	1, 1, TLV_DB_SCALE_ITEM(-1050, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(-900, 0, 0),
	3, 3, TLV_DB_SCALE_ITEM(-675, 0, 0),
	4, 4, TLV_DB_SCALE_ITEM(-450, 0, 0),
	5, 5, TLV_DB_SCALE_ITEM(-300, 0, 0),
	6, 6, TLV_DB_SCALE_ITEM(-187, 0, 0),
	7, 7, TLV_DB_SCALE_ITEM(-37, 0, 0),
	8, 8, TLV_DB_SCALE_ITEM(37, 0, 0),
	9, 9, TLV_DB_SCALE_ITEM(150, 0, 0),
	10, 10, TLV_DB_SCALE_ITEM(225, 0, 0),
	11, 11, TLV_DB_SCALE_ITEM(300, 0, 0),
	12, 12, TLV_DB_SCALE_ITEM(375, 0, 0),
	13, 13, TLV_DB_SCALE_ITEM(450, 0, 0),
	14, 14, TLV_DB_SCALE_ITEM(487, 0, 0),
	15, 15, TLV_DB_SCALE_ITEM(562, 0, 0),
	16, 16, TLV_DB_SCALE_ITEM(600, 0, 0),
	17, 17, TLV_DB_SCALE_ITEM(637, 0, 0),
	18, 18, TLV_DB_SCALE_ITEM(712, 0, 0),
	19, 19, TLV_DB_SCALE_ITEM(750, 0, 0),
};

/* IN1/IN2 Input Type */
static const char *rt5647_input_mode[] = {
	"Single ended", "Differential"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_in2_mode_enum, RT5647_IN2,
	RT5647_IN_SFT2, rt5647_input_mode);

static const SOC_ENUM_SINGLE_DECL(
	rt5647_in3_mode_enum, RT5647_IN3,
	RT5647_IN_SFT1, rt5647_input_mode);

/* Interface data select */
static const char *rt5647_data_select[] = {
	"Normal", "Swap", "left copy to right", "right copy to left"
};

static const SOC_ENUM_SINGLE_DECL(rt5647_if2_dac_enum, RT5647_DIG_INF1_DATA,
				RT5647_IF2_DAC_SEL_SFT, rt5647_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5647_if2_adc_enum, RT5647_DIG_INF1_DATA,
				RT5647_IF2_ADC_SEL_SFT, rt5647_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5647_if3_dac_enum, RT5647_DIG_INF1_DATA,
				RT5647_IF3_DAC_SEL_SFT, rt5647_data_select);

static const SOC_ENUM_SINGLE_DECL(rt5647_if3_adc_enum, RT5647_DIG_INF1_DATA,
				RT5647_IF3_ADC_SEL_SFT, rt5647_data_select);

/* Haptic */
static const char *rt5647_haptic_act_select[] = {
	"AC", "DC"
};

static const SOC_ENUM_SINGLE_DECL(rt5647_haptic_act_enum, RT5647_HAPTIC_CTRL1,
			RT5647_HAPTIC_TYPE_SFT, rt5647_haptic_act_select);

static const char *rt5647_haptic_trigger_select[] = {
	"Disable", "One shot", "Continuous", "Programmable"
};

static const SOC_ENUM_SINGLE_DECL(rt5647_haptic_trigger_enum, 
			RT5647_HAPTIC_CTRL1, RT5647_HAPTIC_TG_SFT,
			rt5647_haptic_trigger_select);

/* Class D speaker gain ratio */
static const char * const rt5647_clsd_spk_ratio[] = {
	"1.66x", "1.83x", "1.94x", "2x", "2.11x", "2.22x",
	"2.33x", "2.44x", "2.55x", "2.66x", "2.77x"
};

static const SOC_ENUM_SINGLE_DECL(rt5647_clsd_spk_ratio_enum,
				RT5647_CLSD_OUT_CTRL, RT5647_CLSD_RATIO_SFT,
				rt5647_clsd_spk_ratio);

#if 1 /* Only for debug */
static const char *rt5647_push_btn_mode[] = {
	"Disable", "read"
};

static const SOC_ENUM_SINGLE_DECL(rt5647_push_btn_enum, 0, 0, rt5647_push_btn_mode);

static int rt5647_push_btn_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int rt5647_push_btn_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	printk(KERN_INFO "ret=0x%x\n", rt5647_button_detect(codec));

	return 0;
}

static const char *rt5647_jack_type_mode[] = {
	"Disable", "read"
};

static const SOC_ENUM_SINGLE_DECL(rt5647_jack_type_enum, 0, 0, rt5647_jack_type_mode);

static int rt5647_jack_type_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int rt5647_jack_type_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	rt5647_headset_detect(codec, 1);

	return 0;
}
#endif

static const struct snd_kcontrol_new rt5647_snd_controls[] = {

        /* For Camera main mic record */
        SOC_DOUBLE("DMIC2 Latch Switch", RT5647_DMIC_CTRL2,
        3, 2, 1, 0),

	/* Speaker Output Volume */
	SOC_DOUBLE("Speaker Playback Switch", RT5647_SPK_VOL,
		RT5647_L_MUTE_SFT, RT5647_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("Speaker Playback Volume", RT5647_SPK_VOL,
		RT5647_L_VOL_SFT, RT5647_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* Headphone Output Volume */
	SOC_DOUBLE("HP Playback Switch", RT5647_HP_VOL,
		RT5647_L_MUTE_SFT, RT5647_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("HP Playback Volume", RT5647_HP_VOL,
		RT5647_L_VOL_SFT, RT5647_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* Mono Output Volume */
	SOC_SINGLE("Mono Playback Switch", RT5647_MONO_OUT,
				RT5647_VOL_L_SFT, 1, 1), /*bard*/
	SOC_SINGLE_TLV("Mono Playback Volume", RT5647_MONO_OUT,
		RT5647_L_VOL_SFT, 39, 1, out_vol_tlv),
	/* OUTPUT Control */
	SOC_DOUBLE("OUT Playback Switch", RT5647_LOUT1,
		RT5647_L_MUTE_SFT, RT5647_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE("OUT Channel Switch", RT5647_LOUT1,
		RT5647_VOL_L_SFT, RT5647_VOL_R_SFT, 1, 1),
	SOC_DOUBLE_TLV("OUT Playback Volume", RT5647_LOUT1,
		RT5647_L_VOL_SFT, RT5647_R_VOL_SFT, 39, 1, out_vol_tlv),
	/* DAC Digital Volume */
	SOC_DOUBLE("DAC2 Playback Switch", RT5647_DAC_CTRL,
		RT5647_M_DAC_L2_VOL_SFT, RT5647_M_DAC_R2_VOL_SFT, 1, 1),
	SOC_DOUBLE_TLV("DAC1 Playback Volume", RT5647_DAC1_DIG_VOL,
			RT5647_L_VOL_SFT, RT5647_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	SOC_DOUBLE_TLV("Mono DAC Playback Volume", RT5647_DAC2_DIG_VOL,
			RT5647_L_VOL_SFT, RT5647_R_VOL_SFT,
			175, 0, dac_vol_tlv),
	/* DAC Digital Gain */
	SOC_DOUBLE_R_TLV("HPMIX DAC1 Gain", RT5647_HPMIXL_CTRL,
			RT5647_HPMIXR_CTRL, 8, 3, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("HPMIX DAC2 Gain", RT5647_HPMIXL_CTRL,
			RT5647_HPMIXR_CTRL, 6, 3, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("HPMIX IN Gain", RT5647_HPMIXL_CTRL,
			RT5647_HPMIXR_CTRL, 4, 3, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("HPMIX BST3 Gain", RT5647_HPMIXL_CTRL,
			RT5647_HPMIXR_CTRL, 2, 3, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("HPMIX BST2 Gain", RT5647_HPMIXL_CTRL,
			RT5647_HPMIXR_CTRL, 0, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("HPMIXL BST1 Gain", RT5647_HPMIXL_CTRL,
			0, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("HPMIXR BST2 Gain", RT5647_HPMIXR_CTRL,
			0, 3, 1, dac_gain_tlv),

	SOC_DOUBLE_R_TLV("SPKMIX IN Gain", RT5647_SPK_L_MIXER,
			RT5647_SPK_R_MIXER, 12, 3, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("SPKMIX DAC1 Gain", RT5647_SPK_L_MIXER,
			RT5647_SPK_R_MIXER, 10, 3, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("SPKMIX DAC2 Gain", RT5647_SPK_L_MIXER,
			RT5647_SPK_R_MIXER, 8, 3, 1, dac_gain_tlv),
	SOC_DOUBLE_R_TLV("SPKMIX BST3 Gain", RT5647_SPK_L_MIXER,
			RT5647_SPK_R_MIXER, 6, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("SPKMIXL BST1 Gain", RT5647_SPK_L_MIXER,
			14, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("SPKMIXR BST2 Gain", RT5647_SPK_R_MIXER,
			14, 3, 1, dac_gain_tlv),

	SOC_SINGLE_TLV("MonoMix BST3 Gain", RT5647_MONO_MIXER1,
			8, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("MonoMix DACL2 Gain", RT5647_MONO_MIXER1,
			6, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("MonoMix DACR2 Gain", RT5647_MONO_MIXER1,
			4, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("MonoMix DACR1 Gain", RT5647_MONO_MIXER1,
			2, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("MonoMix BST2 Gain", RT5647_MONO_MIXER1,
			0, 3, 1, dac_gain_tlv),

	SOC_DOUBLE_R_TLV("OUTMIX BST3 Gain", RT5647_OUTMIXL_CTRL1,
			RT5647_OUTMIXR_CTRL1, 13, 6, 1, mix_bst_tlv),
	SOC_DOUBLE_R_TLV("OUTMIX IN Gain", RT5647_OUTMIXL_CTRL1,
			RT5647_OUTMIXR_CTRL1, 4, 6, 1, mix_bst_tlv),
	SOC_DOUBLE_R_TLV("OUTMIX DAC2 Gain", RT5647_OUTMIXL_CTRL2,
			RT5647_OUTMIXR_CTRL2, 10, 6, 1, mix_bst_tlv),
	SOC_DOUBLE_R_TLV("OUTMIX DAC1 Gain", RT5647_OUTMIXL_CTRL2,
			RT5647_OUTMIXR_CTRL2, 7, 6, 1, mix_bst_tlv),
	SOC_SINGLE_TLV("OUTMIXL BST1 Gain", RT5647_OUTMIXL_CTRL1,
			7, 3, 1, dac_gain_tlv),
	SOC_SINGLE_TLV("OUTMIXR BST2 Gain", RT5647_OUTMIXR_CTRL1,
			7, 3, 1, dac_gain_tlv),
	/* OUT Gain */
	SOC_SINGLE_TLV("HPO Gain", RT5647_HPO_MIXER, 12, 1, 1, out_gain_tlv),
	SOC_SINGLE_TLV("Speaker Output Gain", RT5647_SPO_CLSD_RATIO,
		       0, 7, 0, speaker_gain_tlv),
	
	/* IN1/IN2 Control */
	SOC_SINGLE_TLV("IN1 Boost", RT5647_CJ_CTRL1,
		RT5647_CBJ_BST1_SFT, 8, 0, bst_tlv),
	SOC_SINGLE_TLV("IN2 Boost", RT5647_IN2,
		RT5647_BST_SFT2, 8, 0, bst_tlv),
	SOC_SINGLE_TLV("IN3 Boost", RT5647_IN3,
		RT5647_BST_SFT1, 8, 0, bst_tlv),
	SOC_ENUM("IN2 Mode Control", rt5647_in2_mode_enum),
	SOC_ENUM("IN3 Mode Control", rt5647_in3_mode_enum),
	/* INL/INR Volume Control */
	SOC_DOUBLE_TLV("IN Capture Volume", RT5647_INL1_INR1_VOL,
			RT5647_INL_VOL_SFT, RT5647_INR_VOL_SFT,
			31, 1, in_vol_tlv),
	/* ADC Digital Volume Control */
	SOC_DOUBLE("ADC Capture Switch", RT5647_STO1_ADC_DIG_VOL,
		RT5647_L_MUTE_SFT, RT5647_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("ADC Capture Volume", RT5647_STO1_ADC_DIG_VOL,
			RT5647_L_VOL_SFT, RT5647_R_VOL_SFT,
			127, 0, adc_vol_tlv),

	SOC_DOUBLE_TLV("Mono ADC Capture Volume", RT5647_MONO_ADC_DIG_VOL,
			RT5647_L_VOL_SFT, RT5647_R_VOL_SFT,
			127, 0, adc_vol_tlv),

	/* ADC Boost Volume Control */
	SOC_DOUBLE_TLV("STO1 ADC Boost Gain", RT5647_ADC_BST_VOL1,
			RT5647_STO1_ADC_L_BST_SFT, RT5647_STO1_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),

	SOC_DOUBLE_TLV("STO2 ADC Boost Gain", RT5647_ADC_BST_VOL1,
			RT5647_STO2_ADC_L_BST_SFT, RT5647_STO2_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),

	SOC_DOUBLE_TLV("Mono ADC Boost Gain", RT5647_ADC_BST_VOL2,
			RT5647_MONO_ADC_L_BST_SFT, RT5647_MONO_ADC_R_BST_SFT,
			3, 0, adc_bst_tlv),
 
	SOC_SINGLE_TLV("Mono ADC Comp Gain", RT5647_ADC_BST_VOL2,
			RT5647_MONO_ADC_COMP_SFT, 3, 0, adc_com_tlv),

	SOC_SINGLE_TLV("Stereo ADC Comp Gain", RT5647_ADC_BST_VOL1,
			RT5647_STO1_ADC_COMP_SFT, 3, 0, adc_com_tlv),

	/* RECMIX Boost Gain */
	SOC_DOUBLE_R_TLV("RECMIX BST3 Gain", RT5647_REC_L1_MIXER,
			RT5647_REC_R1_MIXER, RT5647_G_BST3_RM_L_SFT,
			6, 1, mix_bst_tlv),
	SOC_DOUBLE_R_TLV("RECMIX BST2 Gain", RT5647_REC_L1_MIXER,
			RT5647_REC_R1_MIXER, RT5647_G_BST2_RM_L_SFT,
			6, 1, mix_bst_tlv),
	SOC_DOUBLE_R_TLV("RECMIX BST1 Gain", RT5647_REC_L2_MIXER,
			RT5647_REC_R2_MIXER, RT5647_G_BST1_RM_L_SFT,
			6, 1, mix_bst_tlv),

	/* Class D speaker gain ratio */
	SOC_ENUM("ClassD Amp Ratio Gain Control", rt5647_clsd_spk_ratio_enum),

	/* SounzReal */
	SOC_SINGLE_TLV("Bass Back Boost Gain", RT5647_BASE_BACK,
		RT5647_G_BB_BST_SFT, 31, 0, bb_bst_tlv),
	SOC_SINGLE_TLV("TrueTreble Gain", RT5647_MP3_PLUS1,
		RT5647_EG_MP3_SFT, 20, 0, tt_bst_tlv),

	/* I2S2 function select */
	SOC_SINGLE("I2S2 Func Switch", RT5647_GPIO_CTRL1,
				RT5647_I2S2_SEL_SFT, 1, 1),

	/* Haptic Generator Control */
	SOC_ENUM("Haptic Actuator Type", rt5647_haptic_act_enum),
	SOC_ENUM("Haptic Trigger Type", rt5647_haptic_trigger_enum),
	SOC_SINGLE("Haptic Enable Switch", RT5647_HAPTIC_CTRL1,
				RT5647_HAPTIC_EN_SFT, 1, 0),

	SOC_ENUM("ADC IF2 Data Switch", rt5647_if2_adc_enum),
	SOC_ENUM("DAC IF2 Data Switch", rt5647_if2_dac_enum),
	SOC_ENUM("ADC IF3 Data Switch", rt5647_if3_adc_enum),
	SOC_ENUM("DAC IF3 Data Switch", rt5647_if3_dac_enum),

#if 1 /* Only for debug */
	SOC_ENUM_EXT("push button", rt5647_push_btn_enum,
		rt5647_push_btn_get, rt5647_push_btn_put),
	SOC_ENUM_EXT("jack type", rt5647_jack_type_enum,
		rt5647_jack_type_get, rt5647_jack_type_put),
#endif
};

/**
 * set_dmic_clk - Set parameter of dmic.
 *
 * @w: DAPM widget.
 * @kcontrol: The kcontrol of this widget.
 * @event: Event id.
 *
 * Choose dmic clock between 1MHz and 3MHz.
 * It is better for clock to approximate 3MHz.
 */
static int set_dmic_clk(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	int div[] = {2, 3, 4, 6, 8, 12};
	int idx = -EINVAL, i;
	int rate, red, bound, temp;

	rate = rt5647->lrck[rt5647->aif_pu] << 8;
	/* red = 3000000 * 12; */
	red = 2000000 * 12;
	for (i = 0; i < ARRAY_SIZE(div); i++) {
		bound = div[i] * 2000000;
		if (rate > bound)
			continue;
		temp = bound - rate;
		if (temp < red) {
			red = temp;
			idx = i;
		}
	}
#ifdef USE_ASRC
	idx = 4;
#endif
	if (idx < 0)
		dev_err(codec->dev, "Failed to set DMIC clock\n");
	else
		snd_soc_update_bits(codec, RT5647_DMIC_CTRL1, RT5647_DMIC_CLK_MASK,
					idx << RT5647_DMIC_CLK_SFT);
	return idx;
}

static int check_sysclk1_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	unsigned int val;

	val = snd_soc_read(source->codec, RT5647_GLB_CLK);
	val &= RT5647_SCLK_SRC_MASK;
	if (val == RT5647_SCLK_SRC_PLL1)
		return 1;
	else
		return 0;
}

/* Digital Mixer */
static const struct snd_kcontrol_new rt5647_sto1_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5647_STO1_ADC_MIXER,
			RT5647_M_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5647_STO1_ADC_MIXER,
			RT5647_M_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_sto1_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5647_STO1_ADC_MIXER,
			RT5647_M_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5647_STO1_ADC_MIXER,
			RT5647_M_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_mono_adc_l_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5647_MONO_ADC_MIXER,
			RT5647_M_MONO_ADC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5647_MONO_ADC_MIXER,
			RT5647_M_MONO_ADC_L2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_mono_adc_r_mix[] = {
	SOC_DAPM_SINGLE("ADC1 Switch", RT5647_MONO_ADC_MIXER,
			RT5647_M_MONO_ADC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("ADC2 Switch", RT5647_MONO_ADC_MIXER,
			RT5647_M_MONO_ADC_R2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_dac_l_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5647_AD_DA_MIXER,
			RT5647_M_ADCMIX_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5647_AD_DA_MIXER,
			RT5647_M_DAC1_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_dac_r_mix[] = {
	SOC_DAPM_SINGLE("Stereo ADC Switch", RT5647_AD_DA_MIXER,
			RT5647_M_ADCMIX_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC1 Switch", RT5647_AD_DA_MIXER,
			RT5647_M_DAC1_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_sto_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_DAC_L1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_DAC_L2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_DAC_R1_STO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("ANC Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_ANC_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_sto_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_DAC_R1_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_DAC_R2_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_DAC_L1_STO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("ANC Switch", RT5647_STO_DAC_MIXER,
			RT5647_M_ANC_DAC_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_mono_dac_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5647_MONO_DAC_MIXER,
			RT5647_M_DAC_L1_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_MONO_DAC_MIXER,
			RT5647_M_DAC_L2_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_MONO_DAC_MIXER,
			RT5647_M_DAC_R2_MONO_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5647_SIDETONE_CTRL,
			RT5647_M_ST_DACL2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_mono_dac_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_MONO_DAC_MIXER,
			RT5647_M_DAC_R1_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_MONO_DAC_MIXER,
			RT5647_M_DAC_R2_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_MONO_DAC_MIXER,
			RT5647_M_DAC_L2_MONO_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("Sidetone Switch", RT5647_SIDETONE_CTRL,
			RT5647_M_ST_DACR2_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_dig_l_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix L Switch", RT5647_DIG_MIXER,
			RT5647_M_STO_L_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_DIG_MIXER,
			RT5647_M_DAC_L2_DAC_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_DIG_MIXER,
			RT5647_M_DAC_R2_DAC_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_dig_r_mix[] = {
	SOC_DAPM_SINGLE("Sto DAC Mix R Switch", RT5647_DIG_MIXER,
			RT5647_M_STO_R_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_DIG_MIXER,
			RT5647_M_DAC_R2_DAC_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_DIG_MIXER,
			RT5647_M_DAC_L2_DAC_R_SFT, 1, 1),
};

/* Analog Input Mixer */
static const struct snd_kcontrol_new rt5647_rec_l_mix[] = {
	SOC_DAPM_SINGLE("MONO Switch", RT5647_REC_L2_MIXER,
			RT5647_M_MM_L_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPOL Switch", RT5647_REC_L2_MIXER,
			RT5647_M_HP_L_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5647_REC_L2_MIXER,
			RT5647_M_IN_L_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_REC_L2_MIXER,
			RT5647_M_BST3_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5647_REC_L2_MIXER,
			RT5647_M_BST2_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5647_REC_L2_MIXER,
			RT5647_M_BST1_RM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUT MIXL Switch", RT5647_REC_L2_MIXER,
			RT5647_M_OM_L_RM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_rec_r_mix[] = {
	SOC_DAPM_SINGLE("MONO Switch", RT5647_REC_R2_MIXER,
			RT5647_M_MM_R_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPOR Switch", RT5647_REC_R2_MIXER,
			RT5647_M_HP_R_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5647_REC_R2_MIXER,
			RT5647_M_IN_R_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_REC_R2_MIXER,
			RT5647_M_BST3_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5647_REC_R2_MIXER,
			RT5647_M_BST2_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5647_REC_R2_MIXER,
			RT5647_M_BST1_RM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUT MIXR Switch", RT5647_REC_R2_MIXER,
			RT5647_M_OM_R_RM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_spk_l_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5647_SPK_L_MIXER,
			RT5647_M_DAC_L1_SM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_SPK_L_MIXER,
			RT5647_M_DAC_L2_SM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5647_SPK_L_MIXER,
			RT5647_M_IN_L_SM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_SPK_L_MIXER,
			RT5647_M_BST3_L_SM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5647_SPK_L_MIXER,
			RT5647_M_BST1_L_SM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_spk_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_SPK_R_MIXER,
			RT5647_M_DAC_R1_SM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_SPK_R_MIXER,
			RT5647_M_DAC_R2_SM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5647_SPK_R_MIXER,
			RT5647_M_IN_R_SM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_SPK_R_MIXER,
			RT5647_M_BST3_R_SM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5647_SPK_R_MIXER,
			RT5647_M_BST2_R_SM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_out_l_mix[] = {
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_OUTMIXL_CTRL3,
			RT5647_M_BST3_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5647_OUTMIXL_CTRL3,
			RT5647_M_BST1_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5647_OUTMIXL_CTRL3,
			RT5647_M_IN_L_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_OUTMIXL_CTRL3,
			RT5647_M_DAC_L2_OM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5647_OUTMIXL_CTRL3,
			RT5647_M_DAC_L1_OM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_out_r_mix[] = {
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_OUTMIXR_CTRL3,
			RT5647_M_BST3_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5647_OUTMIXR_CTRL3,
			RT5647_M_BST2_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5647_OUTMIXR_CTRL3,
			RT5647_M_IN_R_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_OUTMIXR_CTRL3,
			RT5647_M_DAC_R2_OM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_OUTMIXR_CTRL3,
			RT5647_M_DAC_R1_OM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_mono_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_MONO_MIXER2,
			RT5647_M_DAC_R1_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R2 Switch", RT5647_MONO_MIXER2,
			RT5647_M_DAC_R2_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_MONO_MIXER2,
			RT5647_M_DAC_L2_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_MONO_MIXER2,
			RT5647_M_BST3_MM_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5647_MONO_MIXER2,
			RT5647_M_BST2_MM_SFT, 1, 1),
};


static const struct snd_kcontrol_new rt5647_spo_l_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_SPO_MIXER,
			RT5647_M_DAC_R1_SPM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5647_SPO_MIXER,
			RT5647_M_DAC_L1_SPM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("SPKVOL R Switch", RT5647_SPO_MIXER,
			RT5647_M_SV_R_SPM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("SPKVOL L Switch", RT5647_SPO_MIXER,
			RT5647_M_SV_L_SPM_L_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_SPO_MIXER,
			RT5647_M_BST3_SPM_L_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_spo_r_mix[] = {
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_SPO_MIXER,
			RT5647_M_DAC_R1_SPM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("SPKVOL R Switch", RT5647_SPO_MIXER,
			RT5647_M_SV_R_SPM_R_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_SPO_MIXER,
			RT5647_M_BST3_SPM_R_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_hpo_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5647_HPO_MIXER,
			RT5647_M_DAC1_HM_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPVOL Switch", RT5647_HPO_MIXER,
			RT5647_M_HPVOL_HM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_hpvoll_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5647_HPOMIXL_CTRL,
			RT5647_M_DAC1_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC2 Switch", RT5647_HPOMIXL_CTRL,
			RT5647_M_DAC2_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("INL Switch", RT5647_HPOMIXL_CTRL,
			RT5647_M_IN_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_HPOMIXL_CTRL,
			RT5647_M_BST3_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST1 Switch", RT5647_HPOMIXL_CTRL,
			RT5647_M_BST1_HV_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_hpvolr_mix[] = {
	SOC_DAPM_SINGLE("DAC1 Switch", RT5647_HPOMIXR_CTRL,
			RT5647_M_DAC1_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC2 Switch", RT5647_HPOMIXR_CTRL,
			RT5647_M_DAC2_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("INR Switch", RT5647_HPOMIXR_CTRL,
			RT5647_M_IN_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST3 Switch", RT5647_HPOMIXR_CTRL,
			RT5647_M_BST3_HV_SFT, 1, 1),
	SOC_DAPM_SINGLE("BST2 Switch", RT5647_HPOMIXR_CTRL,
			RT5647_M_BST2_HV_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_lout_mix[] = {
	SOC_DAPM_SINGLE("DAC L1 Switch", RT5647_LOUT_MIXER,
			RT5647_M_DAC_L1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("DAC R1 Switch", RT5647_LOUT_MIXER,
			RT5647_M_DAC_R1_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX L Switch", RT5647_LOUT_MIXER,
			RT5647_M_OV_L_LM_SFT, 1, 1),
	SOC_DAPM_SINGLE("OUTMIX R Switch", RT5647_LOUT_MIXER,
			RT5647_M_OV_R_LM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5647_monoamp_mix[] = {
	SOC_DAPM_SINGLE("DAC L2 Switch", RT5647_MONO_MIXER2,
			RT5647_M_DAC_L2_MA_SFT, 1, 1),
	SOC_DAPM_SINGLE("MONOVOL Switch", RT5647_MONO_MIXER2,
			RT5647_M_OV_L_MM_SFT, 1, 1),
};

/*DAC1 L/R source*/ /* MX-29 [9:8] [11:10] */
static const char *rt5647_dac1_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_dac1l_enum, RT5647_AD_DA_MIXER,
	RT5647_DAC1_L_SEL_SFT, rt5647_dac1_src);

static const struct snd_kcontrol_new rt5647_dac1l_mux =
	SOC_DAPM_ENUM("DAC1 L source", rt5647_dac1l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5647_dac1r_enum, RT5647_AD_DA_MIXER,
	RT5647_DAC1_R_SEL_SFT, rt5647_dac1_src);

static const struct snd_kcontrol_new rt5647_dac1r_mux =
	SOC_DAPM_ENUM("DAC1 R source", rt5647_dac1r_enum);

/*DAC2 L/R source*/ /* MX-1B [6:4] [2:0] */
static const char *rt5647_dac12_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "Mono ADC", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_dac2l_enum, RT5647_DAC_CTRL,
	RT5647_DAC2_L_SEL_SFT, rt5647_dac12_src);

static const struct snd_kcontrol_new rt5647_dac_l2_mux =
	SOC_DAPM_ENUM("DAC2 L source", rt5647_dac2l_enum);

static const char *rt5647_dacr2_src[] = {
	"IF1 DAC", "IF2 DAC", "IF3 DAC", "Mono ADC", "Haptic"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_dac2r_enum, RT5647_DAC_CTRL,
	RT5647_DAC2_R_SEL_SFT, rt5647_dacr2_src);

static const struct snd_kcontrol_new rt5647_dac_r2_mux =
	SOC_DAPM_ENUM("DAC2 R source", rt5647_dac2r_enum);


/* INL/R source */
static const char *rt5647_inl_src[] = {
	"IN2P", "MonoP"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_inl_enum, RT5647_INL1_INR1_VOL,
	RT5647_INL_SEL_SFT, rt5647_inl_src);

static const struct snd_kcontrol_new rt5647_inl_mux =
	SOC_DAPM_ENUM("INL source", rt5647_inl_enum);

static const char *rt5647_inr_src[] = {
	"IN2N", "MonoN"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_inr_enum, RT5647_INL1_INR1_VOL,
	RT5647_INR_SEL_SFT, rt5647_inr_src);

static const struct snd_kcontrol_new rt5647_inr_mux =
	SOC_DAPM_ENUM("INR source", rt5647_inr_enum);

/* Stereo1 ADC source */
/* MX-27 [12] */
static const char *rt5647_stereo_adc1_src[] = {
	"DAC MIX", "ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_stereo1_adc1_enum, RT5647_STO1_ADC_MIXER,
	RT5647_ADC_1_SRC_SFT, rt5647_stereo_adc1_src);

static const struct snd_kcontrol_new rt5647_sto_adc_l1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC L1 source", rt5647_stereo1_adc1_enum);

static const struct snd_kcontrol_new rt5647_sto_adc_r1_mux =
	SOC_DAPM_ENUM("Stereo1 ADC R1 source", rt5647_stereo1_adc1_enum);

/* MX-27 [11] */
static const char *rt5647_stereo_adc2_src[] = {
	"DAC MIX", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_stereo1_adc2_enum, RT5647_STO1_ADC_MIXER,
	RT5647_ADC_2_SRC_SFT, rt5647_stereo_adc2_src);

static const struct snd_kcontrol_new rt5647_sto_adc_2_mux =
	SOC_DAPM_ENUM("Stereo1 ADC 2 Mux", rt5647_stereo1_adc2_enum);

/* MX-27 [8] */
static const char *rt5647_stereo_dmic_src[] = {
	"DMIC1", "DMIC2"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_stereo1_dmic_enum, RT5647_STO1_ADC_MIXER,
	RT5647_DMIC_SRC_SFT, rt5647_stereo_dmic_src);

static const struct snd_kcontrol_new rt5647_sto1_dmic_mux =
	SOC_DAPM_ENUM("Stereo1 DMIC source", rt5647_stereo1_dmic_enum);

/* Mono ADC source */
/* MX-28 [12] */
static const char *rt5647_mono_adc_l1_src[] = {
	"Mono DAC MIXL", "ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_mono_adc_l1_enum, RT5647_MONO_ADC_MIXER,
	RT5647_MONO_ADC_L1_SRC_SFT, rt5647_mono_adc_l1_src);

static const struct snd_kcontrol_new rt5647_mono_adc_l1_mux =
	SOC_DAPM_ENUM("Mono ADC1 left source", rt5647_mono_adc_l1_enum);
/* MX-28 [11] */
static const char *rt5647_mono_adc_l2_src[] = {
	"Mono DAC MIXL", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_mono_adc_l2_enum, RT5647_MONO_ADC_MIXER,
	RT5647_MONO_ADC_L2_SRC_SFT, rt5647_mono_adc_l2_src);

static const struct snd_kcontrol_new rt5647_mono_adc_l2_mux =
	SOC_DAPM_ENUM("Mono ADC2 left source", rt5647_mono_adc_l2_enum);

/* MX-28 [8] */
static const char *rt5647_mono_dmic_src[] = {
	"DMIC1", "DMIC2"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_mono_dmic_l_enum, RT5647_MONO_ADC_MIXER,
	RT5647_MONO_DMIC_L_SRC_SFT, rt5647_mono_dmic_src);

static const struct snd_kcontrol_new rt5647_mono_dmic_l_mux =
	SOC_DAPM_ENUM("Mono DMIC left source", rt5647_mono_dmic_l_enum);
/* MX-28 [1:0] */
static const SOC_ENUM_SINGLE_DECL(
	rt5647_mono_dmic_r_enum, RT5647_MONO_ADC_MIXER,
	RT5647_MONO_DMIC_R_SRC_SFT, rt5647_mono_dmic_src);

static const struct snd_kcontrol_new rt5647_mono_dmic_r_mux =
	SOC_DAPM_ENUM("Mono DMIC Right source", rt5647_mono_dmic_r_enum);
/* MX-28 [4] */
static const char *rt5647_mono_adc_r1_src[] = {
	"Mono DAC MIXR", "ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_mono_adc_r1_enum, RT5647_MONO_ADC_MIXER,
	RT5647_MONO_ADC_R1_SRC_SFT, rt5647_mono_adc_r1_src);

static const struct snd_kcontrol_new rt5647_mono_adc_r1_mux =
	SOC_DAPM_ENUM("Mono ADC1 right source", rt5647_mono_adc_r1_enum);
/* MX-28 [3] */
static const char *rt5647_mono_adc_r2_src[] = {
	"Mono DAC MIXR", "DMIC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_mono_adc_r2_enum, RT5647_MONO_ADC_MIXER,
	RT5647_MONO_ADC_R2_SRC_SFT, rt5647_mono_adc_r2_src);

static const struct snd_kcontrol_new rt5647_mono_adc_r2_mux =
	SOC_DAPM_ENUM("Mono ADC2 right source", rt5647_mono_adc_r2_enum);

/* MX-2F [13:12] */
static const char *rt5647_if2_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_if2_adc_in_enum, RT5647_DIG_INF1_DATA,
	RT5647_IF2_ADC_IN_SFT, rt5647_if2_adc_in_src);

static const struct snd_kcontrol_new rt5647_if2_adc_in_mux =
	SOC_DAPM_ENUM("IF2 ADC IN source", rt5647_if2_adc_in_enum);

/* MX-2F [1:0] */
static const char *rt5647_if3_adc_in_src[] = {
	"IF_ADC1", "IF_ADC2", "VAD_ADC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_if3_adc_in_enum, RT5647_DIG_INF1_DATA,
	RT5647_IF3_ADC_IN_SFT, rt5647_if3_adc_in_src);

static const struct snd_kcontrol_new rt5647_if3_adc_in_mux =
	SOC_DAPM_ENUM("IF3 ADC IN source", rt5647_if3_adc_in_enum);

/* MX-31 [15] [13] [11] [9] */
static const char *rt5647_pdm_src[] = {
	"Mono DAC", "Stereo DAC"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_pdm1_l_enum, RT5647_PDM_OUT_CTRL,
	RT5647_PDM1_L_SFT, rt5647_pdm_src);

static const struct snd_kcontrol_new rt5647_pdm1_l_mux =
	SOC_DAPM_ENUM("PDM1 L source", rt5647_pdm1_l_enum);

static const SOC_ENUM_SINGLE_DECL(
	rt5647_pdm1_r_enum, RT5647_PDM_OUT_CTRL,
	RT5647_PDM1_R_SFT, rt5647_pdm_src);

static const struct snd_kcontrol_new rt5647_pdm1_r_mux =
	SOC_DAPM_ENUM("PDM1 R source", rt5647_pdm1_r_enum);

/* MX-18 [11:9] */
static const char *rt5647_sidetone_src[] = {
	"DMIC L1", "DMIC L2", "Reserved", "ADC L", "ADC R"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_sidetone_enum, RT5647_SIDETONE_CTRL,
	RT5647_ST_SEL_SFT, rt5647_sidetone_src);

static const struct snd_kcontrol_new rt5647_sidetone_mux =
	SOC_DAPM_ENUM("Sidetone source", rt5647_sidetone_enum);

/* MX-18 [6] */
static const char *rt5647_anc_src[] = {
	"SNC", "Sidetone"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_anc_enum, RT5647_SIDETONE_CTRL,
	RT5647_ST_EN_SFT, rt5647_anc_src);

static const struct snd_kcontrol_new rt5647_anc_mux =
	SOC_DAPM_ENUM("ANC source", rt5647_anc_enum);

/* MX-9D [9:8] */
static const char *rt5647_vad_adc_src[] = {
	"Sto1 ADC L", "Mono ADC L", "Mono ADC R"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5647_vad_adc_enum, RT5647_VAD_CTRL4,
	RT5647_VAD_SEL_SFT, rt5647_vad_adc_src);

static const struct snd_kcontrol_new rt5647_vad_adc_mux =
	SOC_DAPM_ENUM("VAD ADC source", rt5647_vad_adc_enum);

static const struct snd_kcontrol_new mono_vol_control =
	SOC_DAPM_SINGLE("Switch", RT5647_MONO_OUT,
		RT5647_VOL_L_SFT, 1, 1);

static const struct snd_kcontrol_new pdm_l_control =
	SOC_DAPM_SINGLE("Switch", RT5647_PDM_OUT_CTRL,
		RT5647_M_PDM1_L_SFT, 1, 1);

static const struct snd_kcontrol_new pdm_r_control =
	SOC_DAPM_SINGLE("Switch", RT5647_PDM_OUT_CTRL,
		RT5647_M_PDM1_R_SFT, 1, 1);

static const struct snd_kcontrol_new spk_l_vol_control =
	SOC_DAPM_SINGLE("Switch", RT5647_SPK_VOL,
		RT5647_VOL_L_SFT, 1, 1);

static const struct snd_kcontrol_new spk_r_vol_control =
	SOC_DAPM_SINGLE("Switch", RT5647_SPK_VOL,
		RT5647_VOL_R_SFT, 1, 1);

static const struct snd_kcontrol_new hp_l_vol_control =
	SOC_DAPM_SINGLE("Switch", RT5647_HP_VOL,
		RT5647_VOL_L_SFT, 1, 1);

static const struct snd_kcontrol_new hp_r_vol_control =
	SOC_DAPM_SINGLE("Switch", RT5647_HP_VOL,
		RT5647_VOL_R_SFT, 1, 1);

static int rt5647_adc_clk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5647_index_update_bits(codec,
			RT5647_CHOP_DAC_ADC, 0x1000, 0x1000);
		break;

	case SND_SOC_DAPM_POST_PMD:
		rt5647_index_update_bits(codec,
			RT5647_CHOP_DAC_ADC, 0x1000, 0x0000);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_sto1_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_STO1_ADC_DIG_VOL,
			RT5647_L_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_STO1_ADC_DIG_VOL,
			RT5647_L_MUTE,
			RT5647_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_sto1_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_STO1_ADC_DIG_VOL,
			RT5647_R_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_STO1_ADC_DIG_VOL,
			RT5647_R_MUTE,
			RT5647_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_mono_adcl_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_MONO_ADC_DIG_VOL,
			RT5647_L_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_MONO_ADC_DIG_VOL,
			RT5647_L_MUTE,
			RT5647_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_mono_adcr_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_MONO_ADC_DIG_VOL,
			RT5647_R_MUTE, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_MONO_ADC_DIG_VOL,
			RT5647_R_MUTE,
			RT5647_R_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static void hp_amp_power(struct snd_soc_codec *codec, int on)
{
	static int hp_amp_power_count;

	if(on) {
		if(hp_amp_power_count <= 0) {
			/* depop parameters */
			snd_soc_update_bits(codec, RT5647_DEPOP_M2,
				RT5647_DEPOP_MASK, RT5647_DEPOP_MAN);
			snd_soc_write(codec, RT5647_DEPOP_M1, 0x000d);
			rt5647_index_write(codec, RT5647_HP_DCC_INT1, 0x9f01);
			mdelay(150);
			/* headphone amp power on */
			snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
				RT5647_PWR_FV1 | RT5647_PWR_FV2 , 0);
			snd_soc_update_bits(codec, RT5647_PWR_VOL,
				RT5647_PWR_HV_L | RT5647_PWR_HV_R,
				RT5647_PWR_HV_L | RT5647_PWR_HV_R);
			snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
				RT5647_PWR_HP_L | RT5647_PWR_HP_R |
				RT5647_PWR_HA, RT5647_PWR_HP_L |
				RT5647_PWR_HP_R | RT5647_PWR_HA);
			msleep(5);
			snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
				RT5647_PWR_FV1 | RT5647_PWR_FV2,
				RT5647_PWR_FV1 | RT5647_PWR_FV2);

			snd_soc_update_bits(codec, RT5647_HP_CALIB_AMP_DET,
				RT5647_HPD_PS_MASK, RT5647_HPD_PS_EN);
			snd_soc_update_bits(codec, RT5647_DEPOP_M1,
				RT5647_HP_CO_MASK | RT5647_HP_SG_MASK,
				RT5647_HP_CO_EN | RT5647_HP_SG_EN);

			rt5647_index_write(codec, 0x14, 0x1aaa);
			rt5647_index_write(codec, 0x24, 0x0430);
		}
		hp_amp_power_count++;
	} else {
		hp_amp_power_count--;
		if(hp_amp_power_count <= 0) {
			snd_soc_update_bits(codec, RT5647_DEPOP_M1,
				RT5647_HP_SG_MASK | RT5647_HP_L_SMT_MASK |
				RT5647_HP_R_SMT_MASK, RT5647_HP_SG_DIS |
				RT5647_HP_L_SMT_DIS | RT5647_HP_R_SMT_DIS);
			/* headphone amp power down */
			/*
			snd_soc_update_bits(codec, RT5647_DEPOP_M1,
				RT5647_SMT_TRIG_MASK | RT5647_HP_CD_PD_MASK |
				RT5647_HP_CO_MASK | RT5647_HP_CP_MASK |
				RT5647_HP_SG_MASK | RT5647_HP_CB_MASK,
				RT5647_SMT_TRIG_DIS | RT5647_HP_CD_PD_EN |
				RT5647_HP_CO_DIS | RT5647_HP_CP_PD |
				RT5647_HP_SG_EN | RT5647_HP_CB_PD);
			*/
			snd_soc_write(codec, RT5647_DEPOP_M1, 0x0000);
			snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
				RT5647_PWR_HP_L | RT5647_PWR_HP_R | RT5647_PWR_HA,
				0);
		}
	}
}

static void rt5647_pmu_depop(struct snd_soc_codec *codec)
{
	hp_amp_power(codec, 1);
	/* headphone unmute sequence */
	snd_soc_update_bits(codec, RT5647_DEPOP_M3,
		RT5647_CP_FQ1_MASK | RT5647_CP_FQ2_MASK | RT5647_CP_FQ3_MASK,
		(RT5647_CP_FQ_192_KHZ << RT5647_CP_FQ1_SFT) |
		(RT5647_CP_FQ_12_KHZ << RT5647_CP_FQ2_SFT) |
		(RT5647_CP_FQ_192_KHZ << RT5647_CP_FQ3_SFT));
	rt5647_index_write(codec, RT5647_MAMP_INT_REG2, 0xfc00);
	snd_soc_update_bits(codec, RT5647_DEPOP_M1,
		RT5647_SMT_TRIG_MASK, RT5647_SMT_TRIG_EN);
	snd_soc_update_bits(codec, RT5647_DEPOP_M1,
		RT5647_RSTN_MASK, RT5647_RSTN_EN);
	snd_soc_update_bits(codec, RT5647_DEPOP_M1,
		RT5647_RSTN_MASK | RT5647_HP_L_SMT_MASK | RT5647_HP_R_SMT_MASK,
		RT5647_RSTN_DIS | RT5647_HP_L_SMT_EN | RT5647_HP_R_SMT_EN);
	snd_soc_update_bits(codec, RT5647_HP_VOL,
		RT5647_L_MUTE | RT5647_R_MUTE, 0);
	msleep(40);
	snd_soc_update_bits(codec, RT5647_DEPOP_M1,
		RT5647_HP_SG_MASK | RT5647_HP_L_SMT_MASK |
		RT5647_HP_R_SMT_MASK, RT5647_HP_SG_DIS |
		RT5647_HP_L_SMT_DIS | RT5647_HP_R_SMT_DIS);

}

static void rt5647_pmd_depop(struct snd_soc_codec *codec)
{
	/* headphone mute sequence */
	snd_soc_update_bits(codec, RT5647_DEPOP_M3,
		RT5647_CP_FQ1_MASK | RT5647_CP_FQ2_MASK | RT5647_CP_FQ3_MASK,
		(RT5647_CP_FQ_96_KHZ << RT5647_CP_FQ1_SFT) |
		(RT5647_CP_FQ_12_KHZ << RT5647_CP_FQ2_SFT) |
		(RT5647_CP_FQ_96_KHZ << RT5647_CP_FQ3_SFT));
	rt5647_index_write(codec, RT5647_MAMP_INT_REG2, 0xfc00);
	snd_soc_update_bits(codec, RT5647_DEPOP_M1,
		RT5647_HP_SG_MASK, RT5647_HP_SG_EN);
	snd_soc_update_bits(codec, RT5647_DEPOP_M1,
		RT5647_RSTP_MASK, RT5647_RSTP_EN);
	snd_soc_update_bits(codec, RT5647_DEPOP_M1,
		RT5647_RSTP_MASK | RT5647_HP_L_SMT_MASK |
		RT5647_HP_R_SMT_MASK, RT5647_RSTP_DIS |
		RT5647_HP_L_SMT_EN | RT5647_HP_R_SMT_EN);

	snd_soc_update_bits(codec, RT5647_HP_VOL,
		RT5647_L_MUTE | RT5647_R_MUTE, RT5647_L_MUTE | RT5647_R_MUTE);
	msleep(30);

	hp_amp_power(codec, 0);
}

static int rt5647_hp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5647_pmu_depop(codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5647_pmd_depop(codec);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_spk_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5647_index_write(codec, 0x1c, 0xfd20);
		rt5647_index_write(codec, 0x20, 0x611f);
		rt5647_index_write(codec, 0x21, 0x4040);
		rt5647_index_write(codec, 0x23, 0x0004);

		snd_soc_update_bits(codec, RT5647_PWR_DIG1,
			RT5647_PWR_CLS_D | RT5647_PWR_CLS_D_R | RT5647_PWR_CLS_D_L,
			RT5647_PWR_CLS_D | RT5647_PWR_CLS_D_R | RT5647_PWR_CLS_D_L);
		snd_soc_update_bits(codec, RT5647_SPK_VOL,
			RT5647_L_MUTE | RT5647_R_MUTE, 0);
	break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_SPK_VOL,
			RT5647_L_MUTE | RT5647_R_MUTE,
			RT5647_L_MUTE | RT5647_R_MUTE);
		snd_soc_update_bits(codec, RT5647_PWR_DIG1,
			RT5647_PWR_CLS_D | RT5647_PWR_CLS_D_R | RT5647_PWR_CLS_D_L, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_mono_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_MONO_OUT,
				RT5647_L_MUTE, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_MONO_OUT,
			RT5647_L_MUTE, RT5647_L_MUTE);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_lout_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		hp_amp_power(codec,1);
		snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
			RT5647_PWR_LM, RT5647_PWR_LM);
		snd_soc_update_bits(codec, RT5647_LOUT1,
			RT5647_L_MUTE | RT5647_R_MUTE, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_LOUT1,
			RT5647_L_MUTE | RT5647_R_MUTE,
			RT5647_L_MUTE | RT5647_R_MUTE);
		snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
			RT5647_PWR_LM, 0);
		hp_amp_power(codec,0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_bst2_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_PWR_ANLG2,
			RT5647_PWR_BST2_P, RT5647_PWR_BST2_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_PWR_ANLG2,
			RT5647_PWR_BST2_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_bst3_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_PWR_ANLG2,
			RT5647_PWR_BST3_P, RT5647_PWR_BST3_P);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_PWR_ANLG2,
			RT5647_PWR_BST3_P, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_pdm1_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_PDM_OUT_CTRL,
			RT5647_M_PDM1_L, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_PDM_OUT_CTRL,
			RT5647_M_PDM1_L, RT5647_M_PDM1_L);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_pdm1_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_PDM_OUT_CTRL,
			RT5647_M_PDM1_R, 0);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_PDM_OUT_CTRL,
			RT5647_M_PDM1_R, RT5647_M_PDM1_R);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_dac_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5647_update_eqmode(codec, EQ_CH_DACL, rt5647->eq_mode);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5647_update_eqmode(codec, EQ_CH_DACL, NORMAL);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_dac_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		rt5647_update_eqmode(codec, EQ_CH_DACR, rt5647->eq_mode);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		rt5647_update_eqmode(codec, EQ_CH_DACR, NORMAL);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_hpvol_l_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_PWR_MIXER,
			RT5647_PWR_HM_L, RT5647_PWR_HM_L);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_PWR_MIXER,
			RT5647_PWR_HM_L, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5647_hpvol_r_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5647_PWR_MIXER,
			RT5647_PWR_HM_R, RT5647_PWR_HM_R);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5647_PWR_MIXER,
			RT5647_PWR_HM_R, 0);
		break;

	default:
		return 0;
	}

	return 0;
}

// viola added 2014.8.13
static int rt5647_micbias2_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
//	  printk("%s\n",__func__);
        switch (event) {
 	case SND_SOC_DAPM_POST_PMU:
 		snd_soc_update_bits(w->codec, RT5647_PWR_ANLG2, RT5647_PWR_MB2, RT5647_PWR_MB2);

 		break;
 	case SND_SOC_DAPM_PRE_PMD:
 		snd_soc_update_bits(w->codec, RT5647_PWR_ANLG2, RT5647_PWR_MB2,0);
 	default:
 		return 0;
 	}
//	printk("RT5647_PWR_ANLG2=%x\n", snd_soc_read(w->codec, RT5647_PWR_ANLG2));	
	return 0;	
		
}

static int rt5647_af3_mic_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
//	  printk("%s\n",__func__);
        switch (event) {
 	case SND_SOC_DAPM_POST_PMU:
 		snd_soc_update_bits(w->codec, RT5647_GPIO_CTRL2, RT5647_GP1_PIN_MASK, RT5647_GP1_PIN_MASK);

 		break;
 	case SND_SOC_DAPM_PRE_PMD:
 		snd_soc_update_bits(w->codec, RT5647_GPIO_CTRL2, RT5647_GP1_PIN_MASK,0);
 	default:
 		return 0;
 	}
//	printk("RT5647_GPIO_CTRL2=%x\n", snd_soc_read(w->codec, RT5647_GPIO_CTRL2));	
	return 0;	
		
}

static int rt5647_asrc_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{

        pr_debug("%s\n",__func__);
        switch (event) {
 	case SND_SOC_DAPM_POST_PMU:
 		snd_soc_write(w->codec, RT5647_ASRC_1, 0xffff);
 		snd_soc_write(w->codec, RT5647_ASRC_2, 0x1221);
		snd_soc_write(w->codec, RT5647_ASRC_3, 0x0022);
 		break;
 	case SND_SOC_DAPM_PRE_PMD:
 		snd_soc_write(w->codec, RT5647_ASRC_1, 0);
 		snd_soc_write(w->codec, RT5647_ASRC_2, 0);
		snd_soc_write(w->codec, RT5647_ASRC_3, 0);
 	default:
 		return 0;
 	}

        return 0;
}

static const struct snd_soc_dapm_widget rt5647_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("ASRC enable", 2, SND_SOC_NOPM, 0, 0,
		rt5647_asrc_event, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY_S("micbias2 enable", 2, SND_SOC_NOPM, 0, 0,
		rt5647_micbias2_event, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY_S("AF3 MIC enable", 2, SND_SOC_NOPM, 0, 0,
		rt5647_af3_mic_event, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),
	
	SND_SOC_DAPM_SUPPLY("LDO2", RT5647_PWR_MIXER,
		RT5647_PWR_LDO2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL1", RT5647_PWR_ANLG2,
		RT5647_PWR_PLL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("JD Power", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Det Power", RT5647_PWR_VOL,
		RT5647_PWR_MIC_DET_BIT, 0, NULL, 0),

	/* Input Side */
	/* micbias */
	SND_SOC_DAPM_MICBIAS("micbias1", RT5647_PWR_ANLG2,
			RT5647_PWR_MB1_BIT, 0),
	SND_SOC_DAPM_MICBIAS("micbias2", RT5647_PWR_ANLG2,
			RT5647_PWR_MB2_BIT, 0),
	/* Input Lines */
	SND_SOC_DAPM_INPUT("DMIC L1"),
	SND_SOC_DAPM_INPUT("DMIC R1"),
	SND_SOC_DAPM_INPUT("DMIC L2"),
	SND_SOC_DAPM_INPUT("DMIC R2"),

	SND_SOC_DAPM_INPUT("IN1P"),
	SND_SOC_DAPM_INPUT("IN1N"),
	SND_SOC_DAPM_INPUT("IN2P"),
	SND_SOC_DAPM_INPUT("IN2N"),
	SND_SOC_DAPM_INPUT("IN3P"),
	SND_SOC_DAPM_INPUT("IN3N"),

	SND_SOC_DAPM_INPUT("Haptic Generator"),

	SND_SOC_DAPM_PGA("DMIC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DMIC2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("DMIC CLK", SND_SOC_NOPM, 0, 0,
		set_dmic_clk, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY("DMIC1 Power", RT5647_DMIC_CTRL1,
		RT5647_DMIC_1_EN_SFT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DMIC2 Power", RT5647_DMIC_CTRL1,
		RT5647_DMIC_2_EN_SFT, 0, NULL, 0),
	/* Boost */
	SND_SOC_DAPM_PGA("BST1", RT5647_PWR_ANLG2,
		RT5647_PWR_BST1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA_E("BST2", RT5647_PWR_ANLG2,
		RT5647_PWR_BST2_BIT, 0, NULL, 0, rt5647_bst2_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("BST3", RT5647_PWR_ANLG2,
		RT5647_PWR_BST3_BIT, 0, NULL, 0, rt5647_bst3_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	/* Input Volume */
	SND_SOC_DAPM_PGA("INL VOL", RT5647_PWR_VOL,
		RT5647_PWR_IN_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("INR VOL", RT5647_PWR_VOL,
		RT5647_PWR_IN_R_BIT, 0, NULL, 0),
	/* IN Mux */
	SND_SOC_DAPM_MUX("INL Mux", SND_SOC_NOPM, 0, 0, &rt5647_inl_mux),
	SND_SOC_DAPM_MUX("INR Mux", SND_SOC_NOPM, 0, 0, &rt5647_inr_mux),
	/* REC Mixer */
	SND_SOC_DAPM_MIXER("RECMIXL", RT5647_PWR_MIXER, RT5647_PWR_RM_L_BIT,
			0, rt5647_rec_l_mix, ARRAY_SIZE(rt5647_rec_l_mix)),
	SND_SOC_DAPM_MIXER("RECMIXR", RT5647_PWR_MIXER, RT5647_PWR_RM_R_BIT,
			0, rt5647_rec_r_mix, ARRAY_SIZE(rt5647_rec_r_mix)),
	/* ADCs */
	SND_SOC_DAPM_ADC("ADC L", NULL, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_ADC("ADC R", NULL, SND_SOC_NOPM,
		0, 0),

	SND_SOC_DAPM_SUPPLY("ADC L power",RT5647_PWR_DIG1,
			RT5647_PWR_ADC_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC R power",RT5647_PWR_DIG1,
			RT5647_PWR_ADC_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC clock",SND_SOC_NOPM,
			0, 0, rt5647_adc_clk_event,
			SND_SOC_DAPM_POST_PMD |
			SND_SOC_DAPM_POST_PMU),
	/* ADC Mux */
	SND_SOC_DAPM_MUX("Stereo1 DMIC Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_sto1_dmic_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_sto_adc_2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_sto_adc_2_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC L1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_sto_adc_l1_mux),
	SND_SOC_DAPM_MUX("Stereo1 ADC R1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_sto_adc_r1_mux),
	SND_SOC_DAPM_MUX("Mono DMIC L Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_mono_dmic_l_mux),
	SND_SOC_DAPM_MUX("Mono DMIC R Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_mono_dmic_r_mux),
	SND_SOC_DAPM_MUX("Mono ADC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_mono_adc_l2_mux),
	SND_SOC_DAPM_MUX("Mono ADC L1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_mono_adc_l1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R1 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_mono_adc_r1_mux),
	SND_SOC_DAPM_MUX("Mono ADC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_mono_adc_r2_mux),
	/* ADC Mixer */

	SND_SOC_DAPM_SUPPLY_S("adc stereo1 filter", 1, RT5647_PWR_DIG2,
		RT5647_PWR_ADC_S1F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("adc stereo2 filter", 1, RT5647_PWR_DIG2,
		RT5647_PWR_ADC_S2F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Sto1 ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5647_sto1_adc_l_mix, ARRAY_SIZE(rt5647_sto1_adc_l_mix),
		rt5647_sto1_adcl_event,	SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("Sto1 ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5647_sto1_adc_r_mix, ARRAY_SIZE(rt5647_sto1_adc_r_mix),
		rt5647_sto1_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("adc mono left filter", 1, RT5647_PWR_DIG2,
		RT5647_PWR_ADC_MF_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Mono ADC MIXL", SND_SOC_NOPM, 0, 0,
		rt5647_mono_adc_l_mix, ARRAY_SIZE(rt5647_mono_adc_l_mix),
		rt5647_mono_adcl_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("adc mono right filter", 1, RT5647_PWR_DIG2,
		RT5647_PWR_ADC_MF_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("Mono ADC MIXR", SND_SOC_NOPM, 0, 0,
		rt5647_mono_adc_r_mix, ARRAY_SIZE(rt5647_mono_adc_r_mix),
		rt5647_mono_adcr_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),

	/* ADC PGA */
	SND_SOC_DAPM_PGA("Stereo1 ADC MIXL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Stereo1 ADC MIXR", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Sto2 ADC LR MIX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("VAD_ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1_ADC4", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* IF2 3 Mux */
	SND_SOC_DAPM_MUX("IF2 ADC Mux", SND_SOC_NOPM,
		0, 0, &rt5647_if2_adc_in_mux),
	SND_SOC_DAPM_MUX("IF3 ADC Mux", SND_SOC_NOPM,
		0, 0, &rt5647_if3_adc_in_mux),

	/* Digital Interface */
	SND_SOC_DAPM_SUPPLY("I2S1", RT5647_PWR_DIG1,
		RT5647_PWR_I2S1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC1 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 DAC2 R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF1 ADC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S2", RT5647_PWR_DIG1,
		RT5647_PWR_I2S2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF2 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("I2S3", RT5647_PWR_DIG1,
		RT5647_PWR_I2S3_BIT, 0, NULL, 0),
 	SND_SOC_DAPM_PGA("IF3 DAC", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 DAC L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 DAC R", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("IF3 ADC", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Digital Interface Select */
	SND_SOC_DAPM_MUX("VAD ADC Mux", SND_SOC_NOPM,
		0, 0, &rt5647_vad_adc_mux),

	/* Audio Interface */
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2RX", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF3RX", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF3TX", "AIF3 Capture", 0, SND_SOC_NOPM, 0, 0),

	/* Audio DSP */
	SND_SOC_DAPM_PGA("Audio DSP", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Output Side */
	/* DAC mixer before sound effect  */
	SND_SOC_DAPM_MIXER_E("DAC1 MIXL", SND_SOC_NOPM, 0, 0,
		rt5647_dac_l_mix, ARRAY_SIZE(rt5647_dac_l_mix),
		rt5647_dac_l_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("DAC1 MIXR", SND_SOC_NOPM, 0, 0,
		rt5647_dac_r_mix, ARRAY_SIZE(rt5647_dac_r_mix),
		rt5647_dac_r_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA("DAC MIX", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DAC2 channel Mux */
	SND_SOC_DAPM_MUX("DAC L2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_dac_l2_mux),
	SND_SOC_DAPM_MUX("DAC R2 Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_dac_r2_mux),
	SND_SOC_DAPM_PGA("DAC L2 Volume", SND_SOC_NOPM,
			0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC R2 Volume", SND_SOC_NOPM,
			0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DAC1 L Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_dac1l_mux),
	SND_SOC_DAPM_MUX("DAC1 R Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_dac1r_mux),

	/* Sidetone */
	SND_SOC_DAPM_MUX("Sidetone Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_sidetone_mux),
	SND_SOC_DAPM_MUX("ANC Mux", SND_SOC_NOPM, 0, 0,
				&rt5647_anc_mux),
	SND_SOC_DAPM_PGA("SNC", SND_SOC_NOPM,
			0, 0, NULL, 0),
	/* DAC Mixer */
	SND_SOC_DAPM_SUPPLY_S("dac stereo1 filter", 1, RT5647_PWR_DIG2,
		RT5647_PWR_DAC_S1F_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("dac mono left filter", 1, RT5647_PWR_DIG2,
		RT5647_PWR_DAC_MF_L_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("dac mono right filter", 1, RT5647_PWR_DIG2,
		RT5647_PWR_DAC_MF_R_BIT, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5647_sto_dac_l_mix, ARRAY_SIZE(rt5647_sto_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Stereo DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5647_sto_dac_r_mix, ARRAY_SIZE(rt5647_sto_dac_r_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5647_mono_dac_l_mix, ARRAY_SIZE(rt5647_mono_dac_l_mix)),
	SND_SOC_DAPM_MIXER("Mono DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5647_mono_dac_r_mix, ARRAY_SIZE(rt5647_mono_dac_r_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXL", SND_SOC_NOPM, 0, 0,
		rt5647_dig_l_mix, ARRAY_SIZE(rt5647_dig_l_mix)),
	SND_SOC_DAPM_MIXER("DAC MIXR", SND_SOC_NOPM, 0, 0,
		rt5647_dig_r_mix, ARRAY_SIZE(rt5647_dig_r_mix)),

	/* DACs */
	SND_SOC_DAPM_SUPPLY("DAC L1 Power",RT5647_PWR_DIG1,
			RT5647_PWR_DAC_L1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC R1 Power",RT5647_PWR_DIG1,
			RT5647_PWR_DAC_R1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC L2 Power",RT5647_PWR_DIG1,
			RT5647_PWR_DAC_L2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DAC R2 Power",RT5647_PWR_DIG1,
			RT5647_PWR_DAC_R2_BIT, 0, NULL, 0),
	SND_SOC_DAPM_DAC("DAC L1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC L2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC R1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC R2", NULL, SND_SOC_NOPM, 0, 0),
	/* OUT Mixer */
	SND_SOC_DAPM_MIXER("SPK MIXL", RT5647_PWR_MIXER, RT5647_PWR_SM_L_BIT,
		0, rt5647_spk_l_mix, ARRAY_SIZE(rt5647_spk_l_mix)),
	SND_SOC_DAPM_MIXER("SPK MIXR", RT5647_PWR_MIXER, RT5647_PWR_SM_R_BIT,
		0, rt5647_spk_r_mix, ARRAY_SIZE(rt5647_spk_r_mix)),
	SND_SOC_DAPM_MIXER("OUT MIXL", RT5647_PWR_MIXER, RT5647_PWR_OM_L_BIT,
		0, rt5647_out_l_mix, ARRAY_SIZE(rt5647_out_l_mix)),
	SND_SOC_DAPM_MIXER("OUT MIXR", RT5647_PWR_MIXER, RT5647_PWR_OM_R_BIT,
		0, rt5647_out_r_mix, ARRAY_SIZE(rt5647_out_r_mix)),
	/* Ouput Volume */
	SND_SOC_DAPM_SWITCH("MONOVOL", RT5647_PWR_VOL,
		RT5647_PWR_MV_BIT, 0, &mono_vol_control),
	SND_SOC_DAPM_SWITCH("SPKVOL L", RT5647_PWR_VOL,
		RT5647_PWR_SV_L_BIT, 0, &spk_l_vol_control),
	SND_SOC_DAPM_SWITCH("SPKVOL R", RT5647_PWR_VOL,
		RT5647_PWR_SV_R_BIT, 0,	&spk_r_vol_control),
	SND_SOC_DAPM_MIXER_E("HPOVOL MIXL", RT5647_PWR_VOL, RT5647_PWR_HV_L_BIT,
		0, rt5647_hpvoll_mix, ARRAY_SIZE(rt5647_hpvoll_mix),
		rt5647_hpvol_l_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("HPOVOL MIXR", RT5647_PWR_VOL, RT5647_PWR_HV_R_BIT,
		0, rt5647_hpvolr_mix, ARRAY_SIZE(rt5647_hpvolr_mix),
		rt5647_hpvol_r_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA("DAC 1", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DAC 2", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HPOVOL", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SWITCH("HPOVOL L", SND_SOC_NOPM,
		0, 0, &hp_l_vol_control),
	SND_SOC_DAPM_SWITCH("HPOVOL R", SND_SOC_NOPM,
		0, 0, &hp_r_vol_control),


	/* HPO/LOUT/Mono Mixer */
	SND_SOC_DAPM_MIXER("SPOL MIX", SND_SOC_NOPM, 0,
		0, rt5647_spo_l_mix, ARRAY_SIZE(rt5647_spo_l_mix)),
	SND_SOC_DAPM_MIXER("SPOR MIX", SND_SOC_NOPM, 0,
		0, rt5647_spo_r_mix, ARRAY_SIZE(rt5647_spo_r_mix)),
	SND_SOC_DAPM_MIXER("HPO MIX", SND_SOC_NOPM, 0, 0,
		rt5647_hpo_mix, ARRAY_SIZE(rt5647_hpo_mix)),
	SND_SOC_DAPM_MIXER("LOUT MIX", SND_SOC_NOPM, 0, 0,
		rt5647_lout_mix, ARRAY_SIZE(rt5647_lout_mix)),
	SND_SOC_DAPM_MIXER("MONOVOL MIX", RT5647_PWR_MIXER, RT5647_PWR_MM_BIT,
		0, rt5647_mono_mix, ARRAY_SIZE(rt5647_mono_mix)),
	SND_SOC_DAPM_MIXER("MONOAmp MIX", SND_SOC_NOPM, 0, 0,
		rt5647_monoamp_mix, ARRAY_SIZE(rt5647_monoamp_mix)),

	SND_SOC_DAPM_PGA_S("HP amp", 1, SND_SOC_NOPM,
		0, 0, rt5647_hp_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("LOUT amp", 1, SND_SOC_NOPM,
		0, 0, rt5647_lout_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("Mono amp", 1, RT5647_PWR_ANLG1,
		RT5647_PWR_MA_BIT, 0, rt5647_mono_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("SPK amp", 2, SND_SOC_NOPM,
		0, 0, rt5647_spk_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* PDM */
	SND_SOC_DAPM_SUPPLY("PDM1 Power", RT5647_PWR_DIG2,
		RT5647_PWR_PDM1_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SWITCH("PDM L", SND_SOC_NOPM,
		RT5647_PWR_SV_L_BIT, 0, &pdm_l_control),
	SND_SOC_DAPM_SWITCH("PDM R", SND_SOC_NOPM,
		RT5647_PWR_SV_R_BIT, 0,	&pdm_r_control),
	SND_SOC_DAPM_MUX_E("PDM1 L Mux", SND_SOC_NOPM,
		0, 0, &rt5647_pdm1_l_mux, rt5647_pdm1_l_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX_E("PDM1 R Mux", SND_SOC_NOPM,
		0, 0, &rt5647_pdm1_r_mux, rt5647_pdm1_r_event,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

	/* Output Lines */
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
	SND_SOC_DAPM_OUTPUT("LOUTL"),
	SND_SOC_DAPM_OUTPUT("LOUTR"),
	SND_SOC_DAPM_OUTPUT("MonoP"),
	SND_SOC_DAPM_OUTPUT("MonoN"),
	SND_SOC_DAPM_OUTPUT("PDM1L"),
	SND_SOC_DAPM_OUTPUT("PDM1R"),
	SND_SOC_DAPM_OUTPUT("SPOL"),
	SND_SOC_DAPM_OUTPUT("SPOR"),
};

static const struct snd_soc_dapm_route rt5647_dapm_routes[] = {
#ifdef USE_ASRC
        {"I2S1", NULL, "ASRC enable"},
        {"I2S2", NULL, "ASRC enable"},
        {"I2S3", NULL, "ASRC enable"},

#endif
	//viola added for main mic 2014.8.13

	{ "IN2P", NULL, "micbias2 enable"},
	{ "IN2N", NULL, "micbias2 enable"},
	//end viola
	//AF3 MIC ENABLE 2014.8.13
        {"IF3 ADC Mux", NULL, "AF3 MIC enable"},
        //END AF3

	{ "IN1P", NULL, "LDO2" },
	{ "IN2P", NULL, "LDO2" },
	{ "IN3P", NULL, "LDO2" },

	{ "DMIC1", NULL, "DMIC L1" },
	{ "DMIC1", NULL, "DMIC R1" },
	{ "DMIC2", NULL, "DMIC L2" },
	{ "DMIC2", NULL, "DMIC R2" },

	{ "BST1", NULL, "IN1P" },
	{ "BST1", NULL, "IN1N" },
	{ "BST1", NULL, "JD Power" },
	{ "BST1", NULL, "Mic Det Power" },
	{ "BST2", NULL, "IN2P" },
	{ "BST2", NULL, "IN2N" },
	{ "BST3", NULL, "IN3P" },
	{ "BST3", NULL, "IN3N" },

	{ "INL VOL", NULL, "IN2P" },
	{ "INR VOL", NULL, "IN2N" },

	{ "RECMIXL", "MONO Switch", "MONOVOL" },
	{ "RECMIXL", "HPOL Switch", "HPOL" },
	{ "RECMIXL", "INL Switch", "INL VOL" },
	{ "RECMIXL", "BST3 Switch", "BST3" },
	{ "RECMIXL", "BST2 Switch", "BST2" },
	{ "RECMIXL", "BST1 Switch", "BST1" },
	{ "RECMIXL", "OUT MIXL Switch", "OUT MIXL" },

	{ "RECMIXR", "MONO Switch", "MONOVOL" },
	{ "RECMIXR", "HPOR Switch", "HPOR" },
	{ "RECMIXR", "INR Switch", "INR VOL" },
	{ "RECMIXR", "BST3 Switch", "BST3" },
	{ "RECMIXR", "BST2 Switch", "BST2" },
	{ "RECMIXR", "BST1 Switch", "BST1" },
	{ "RECMIXR", "OUT MIXR Switch", "OUT MIXR" },

	{ "ADC L", NULL, "RECMIXL" },
	{ "ADC L", NULL, "ADC L power" },
	{ "ADC L", NULL, "ADC clock" },
	{ "ADC R", NULL, "RECMIXR" },
	{ "ADC R", NULL, "ADC R power" },
	{ "ADC R", NULL, "ADC clock" },

	{"DMIC L1", NULL, "DMIC CLK"},
	{"DMIC L1", NULL, "DMIC1 Power"},
	{"DMIC R1", NULL, "DMIC CLK"},
	{"DMIC R1", NULL, "DMIC1 Power"},
	{"DMIC L2", NULL, "DMIC CLK"},
	{"DMIC L2", NULL, "DMIC2 Power"},
	{"DMIC R2", NULL, "DMIC CLK"},
	{"DMIC R2", NULL, "DMIC2 Power"},

	{ "Stereo1 DMIC Mux", "DMIC1", "DMIC1" },
	{ "Stereo1 DMIC Mux", "DMIC2", "DMIC2" },

	{ "Mono DMIC L Mux", "DMIC1", "DMIC L1" },
	{ "Mono DMIC L Mux", "DMIC2", "DMIC L2" },

	{ "Mono DMIC R Mux", "DMIC1", "DMIC R1" },
	{ "Mono DMIC R Mux", "DMIC2", "DMIC R2" },

	{ "Stereo1 ADC L2 Mux", "DMIC", "Stereo1 DMIC Mux" },
	{ "Stereo1 ADC L2 Mux", "DAC MIX", "DAC MIXL" },
	{ "Stereo1 ADC L1 Mux", "ADC", "ADC L" },
	{ "Stereo1 ADC L1 Mux", "DAC MIX", "DAC MIXL" },

	{ "Stereo1 ADC R1 Mux", "ADC", "ADC R" },
	{ "Stereo1 ADC R1 Mux", "DAC MIX", "DAC MIXR" },
	{ "Stereo1 ADC R2 Mux", "DMIC", "Stereo1 DMIC Mux" },
	{ "Stereo1 ADC R2 Mux", "DAC MIX", "DAC MIXR" },

	{ "Mono ADC L2 Mux", "DMIC", "Mono DMIC L Mux" },
	{ "Mono ADC L2 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "Mono DAC MIXL", "Mono DAC MIXL" },
	{ "Mono ADC L1 Mux", "ADC", "ADC L" },

	{ "Mono ADC R1 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },
	{ "Mono ADC R1 Mux", "ADC", "ADC R" },
	{ "Mono ADC R2 Mux", "DMIC", "Mono DMIC R Mux" },
	{ "Mono ADC R2 Mux", "Mono DAC MIXR", "Mono DAC MIXR" },

	{ "Sto1 ADC MIXL", "ADC1 Switch", "Stereo1 ADC L1 Mux" },
	{ "Sto1 ADC MIXL", "ADC2 Switch", "Stereo1 ADC L2 Mux" },
	{ "Sto1 ADC MIXR", "ADC1 Switch", "Stereo1 ADC R1 Mux" },
	{ "Sto1 ADC MIXR", "ADC2 Switch", "Stereo1 ADC R2 Mux" },

	{ "Stereo1 ADC MIXL", NULL, "Sto1 ADC MIXL" },
	{ "Stereo1 ADC MIXL", NULL, "adc stereo1 filter" },
	{ "adc stereo1 filter", NULL, "PLL1", check_sysclk1_source },

	{ "Stereo1 ADC MIXR", NULL, "Sto1 ADC MIXR" },
	{ "Stereo1 ADC MIXR", NULL, "adc stereo1 filter" },
	{ "adc stereo1 filter", NULL, "PLL1", check_sysclk1_source },

	{ "Mono ADC MIXL", "ADC1 Switch", "Mono ADC L1 Mux" },
	{ "Mono ADC MIXL", "ADC2 Switch", "Mono ADC L2 Mux" },
	{ "Mono ADC MIXL", NULL, "adc mono left filter" },
	{ "adc mono left filter", NULL, "PLL1", check_sysclk1_source },

	{ "Mono ADC MIXR", "ADC1 Switch", "Mono ADC R1 Mux" },
	{ "Mono ADC MIXR", "ADC2 Switch", "Mono ADC R2 Mux" },
	{ "Mono ADC MIXR", NULL, "adc mono right filter" },
	{ "adc mono right filter", NULL, "PLL1", check_sysclk1_source },

	{ "VAD ADC Mux", "Sto1 ADC L", "Stereo1 ADC MIXL" },
	{ "VAD ADC Mux", "Mono ADC L", "Mono ADC MIXL" },
	{ "VAD ADC Mux", "Mono ADC R", "Mono ADC MIXR" },

	{ "IF_ADC1", NULL, "Stereo1 ADC MIXL" },
	{ "IF_ADC1", NULL, "Stereo1 ADC MIXR" },
	{ "IF_ADC2", NULL, "Mono ADC MIXL" },
	{ "IF_ADC2", NULL, "Mono ADC MIXR" },
	{ "VAD_ADC", NULL, "VAD ADC Mux" },

	{ "IF2 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF2 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF2 ADC Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF3 ADC Mux", "IF_ADC1", "IF_ADC1" },
	{ "IF3 ADC Mux", "IF_ADC2", "IF_ADC2" },
	{ "IF3 ADC Mux", "VAD_ADC", "VAD_ADC" },

	{ "IF1 ADC", NULL, "I2S1" },
	{ "IF1 ADC", NULL, "IF_ADC1" },
#ifdef USE_TDM
	{ "IF1 ADC", NULL, "IF_ADC2" },
	{ "IF1 ADC", NULL, "VAD_ADC" },
#endif
	{ "IF2 ADC", NULL, "I2S2" },
	{ "IF2 ADC", NULL, "IF2 ADC Mux" },
	{ "IF3 ADC", NULL, "I2S3" },
	{ "IF3 ADC", NULL, "IF3 ADC Mux" },

	{ "AIF1TX", NULL, "IF1 ADC" },
	{ "AIF2TX", NULL, "IF2 ADC" },
	{ "AIF3TX", NULL, "IF3 ADC" },

	{ "IF1 DAC1", NULL, "AIF1RX" },
#ifdef USE_TDM
	{ "IF1 DAC2", NULL, "AIF1RX" },
#endif
	{ "IF2 DAC", NULL, "AIF2RX" },
	{ "IF3 DAC", NULL, "AIF3RX" },

	{ "IF1 DAC1", NULL, "I2S1" },
	{ "IF1 DAC2", NULL, "I2S1" },
	{ "IF2 DAC", NULL, "I2S2" },
	{ "IF3 DAC", NULL, "I2S3" },

	{ "IF1 DAC2 L", NULL, "IF1 DAC2" },
	{ "IF1 DAC2 R", NULL, "IF1 DAC2" },
	{ "IF1 DAC1 L", NULL, "IF1 DAC1" },
	{ "IF1 DAC1 R", NULL, "IF1 DAC1" },
	{ "IF2 DAC L", NULL, "IF2 DAC" },
	{ "IF2 DAC R", NULL, "IF2 DAC" },
	{ "IF3 DAC L", NULL, "IF3 DAC" },
	{ "IF3 DAC R", NULL, "IF3 DAC" },

	{ "Sidetone Mux", "DMIC L1", "DMIC L1" },
	{ "Sidetone Mux", "DMIC L2", "DMIC L2" },
	{ "Sidetone Mux", "ADC L", "ADC L" },
	{ "Sidetone Mux", "ADC R", "ADC R" },

	{ "DAC1 L Mux", "IF1 DAC", "IF1 DAC1 L" },
	{ "DAC1 L Mux", "IF2 DAC", "IF2 DAC L" },
	{ "DAC1 L Mux", "IF3 DAC", "IF3 DAC L" },

	{ "DAC1 R Mux", "IF1 DAC", "IF1 DAC1 R" },
	{ "DAC1 R Mux", "IF2 DAC", "IF2 DAC R" },
	{ "DAC1 R Mux", "IF3 DAC", "IF3 DAC R" },

	{ "DAC1 MIXL", "Stereo ADC Switch", "Stereo1 ADC MIXL" },
	{ "DAC1 MIXL", "DAC1 Switch", "DAC1 L Mux" },
	{ "DAC1 MIXL", NULL, "dac stereo1 filter" },
	{ "DAC1 MIXR", "Stereo ADC Switch", "Stereo1 ADC MIXR" },
	{ "DAC1 MIXR", "DAC1 Switch", "DAC1 R Mux" },
	{ "DAC1 MIXR", NULL, "dac stereo1 filter" },

	{ "DAC MIX", NULL, "DAC1 MIXL" },
	{ "DAC MIX", NULL, "DAC1 MIXR" },

	{ "Audio DSP", NULL, "DAC1 MIXL" },
	{ "Audio DSP", NULL, "DAC1 MIXR" },

	{ "DAC L2 Mux", "IF1 DAC", "IF1 DAC2 L" },
	{ "DAC L2 Mux", "IF2 DAC", "IF2 DAC L" },
	{ "DAC L2 Mux", "IF3 DAC", "IF3 DAC L" },
	{ "DAC L2 Mux", "Mono ADC", "Mono ADC MIXL" },
	{ "DAC L2 Mux", "VAD_ADC", "VAD_ADC" },
	{ "DAC L2 Volume", NULL, "DAC L2 Power" },
	{ "DAC L2 Volume", NULL, "DAC L2 Mux" },
	{ "DAC L2 Volume", NULL, "dac mono left filter" },

	{ "DAC R2 Mux", "IF1 DAC", "IF1 DAC2 R" },
	{ "DAC R2 Mux", "IF2 DAC", "IF2 DAC R" },
	{ "DAC R2 Mux", "IF3 DAC", "IF3 DAC R" },
	{ "DAC R2 Mux", "Mono ADC", "Mono ADC MIXR" },
	{ "DAC R2 Mux", "Haptic", "Haptic Generator" },
	{ "DAC R2 Volume", NULL, "DAC R2 Power" },
	{ "DAC R2 Volume", NULL, "DAC R2 Mux" },
	{ "DAC R2 Volume", NULL, "dac mono right filter" },

	{ "SNC", NULL, "ADC L" },
	{ "SNC", NULL, "ADC R" },

	{ "ANC Mux", "SNC", "SNC" },
	{ "ANC Mux", "Sidetone", "Sidetone Mux" },

	{ "Stereo DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXL", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Stereo DAC MIXL", "ANC Switch", "ANC Mux" },
	{ "Stereo DAC MIXL", NULL, "dac stereo1 filter" },
	{ "Stereo DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Stereo DAC MIXR", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Stereo DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Stereo DAC MIXR", "ANC Switch", "ANC Mux" },
	{ "Stereo DAC MIXR", NULL, "dac stereo1 filter" },

	{ "Mono DAC MIXL", "DAC L1 Switch", "DAC1 MIXL" },
	{ "Mono DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Mono DAC MIXL", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Mono DAC MIXL", "Sidetone Switch", "Sidetone Mux" },
	{ "Mono DAC MIXL", NULL, "dac mono left filter" },
	{ "Mono DAC MIXR", "DAC R1 Switch", "DAC1 MIXR" },
	{ "Mono DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "Mono DAC MIXR", "DAC L2 Switch", "DAC L2 Volume" },
	{ "Mono DAC MIXR", "Sidetone Switch", "Sidetone Mux" },
	{ "Mono DAC MIXR", NULL, "dac mono right filter" },

	{ "DAC MIXL", "Sto DAC Mix L Switch", "Stereo DAC MIXL" },
	{ "DAC MIXL", "DAC L2 Switch", "DAC L2 Volume" },
	{ "DAC MIXL", "DAC R2 Switch", "DAC R2 Volume" },
	{ "DAC MIXR", "Sto DAC Mix R Switch", "Stereo DAC MIXR" },
	{ "DAC MIXR", "DAC R2 Switch", "DAC R2 Volume" },
	{ "DAC MIXR", "DAC L2 Switch", "DAC L2 Volume" },

	{ "DAC L1", NULL, "Stereo DAC MIXL" },
	{ "DAC L1", NULL, "DAC L1 Power" },
	{ "DAC L1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R1", NULL, "Stereo DAC MIXR" },
	{ "DAC R1", NULL, "DAC R1 Power" },
	{ "DAC R1", NULL, "PLL1", check_sysclk1_source },
	{ "DAC L2", NULL, "Mono DAC MIXL" },
	{ "DAC L2", NULL, "DAC L2 Power" },
	{ "DAC L2", NULL, "PLL1", check_sysclk1_source },
	{ "DAC R2", NULL, "Mono DAC MIXR" },
	{ "DAC R2", NULL, "DAC R2 Power" },
	{ "DAC R2", NULL, "PLL1", check_sysclk1_source },

	{ "SPK MIXL", "BST1 Switch", "BST1" },
	{ "SPK MIXL", "INL Switch", "INL VOL" },
	{ "SPK MIXL", "DAC L1 Switch", "DAC L1" },
	{ "SPK MIXL", "DAC L2 Switch", "DAC L2" },
	{ "SPK MIXL", "BST3 Switch", "BST3" },
	{ "SPK MIXR", "BST2 Switch", "BST2" },
	{ "SPK MIXR", "INR Switch", "INR VOL" },
	{ "SPK MIXR", "DAC R1 Switch", "DAC R1" },
	{ "SPK MIXR", "DAC R2 Switch", "DAC R2" },
	{ "SPK MIXR", "BST3 Switch", "BST3" },

	{ "OUT MIXL", "BST3 Switch", "BST3" },
	{ "OUT MIXL", "BST1 Switch", "BST1" },
	{ "OUT MIXL", "INL Switch", "INL VOL" },
	{ "OUT MIXL", "DAC L2 Switch", "DAC L2" },
	{ "OUT MIXL", "DAC L1 Switch", "DAC L1" },

	{ "OUT MIXR", "BST3 Switch", "BST3" },
	{ "OUT MIXR", "BST2 Switch", "BST2" },
	{ "OUT MIXR", "INR Switch", "INR VOL" },
	{ "OUT MIXR", "DAC R2 Switch", "DAC R2" },
	{ "OUT MIXR", "DAC R1 Switch", "DAC R1" },

	{ "HPOVOL MIXL", "DAC1 Switch", "DAC L1" },
	{ "HPOVOL MIXL", "DAC2 Switch", "DAC L2" },
	{ "HPOVOL MIXL", "INL Switch", "INL VOL" },
	{ "HPOVOL MIXL", "BST1 Switch", "BST1" },
	{ "HPOVOL MIXL", "BST3 Switch", "BST3" },
	{ "HPOVOL MIXR", "DAC1 Switch", "DAC R1" },
	{ "HPOVOL MIXR", "DAC2 Switch", "DAC R2" },
	{ "HPOVOL MIXR", "INR Switch", "INR VOL" },
	{ "HPOVOL MIXR", "BST2 Switch", "BST2" },
	{ "HPOVOL MIXR", "BST3 Switch", "BST3" },

	{ "DAC 2", NULL, "DAC L2" },
	{ "DAC 2", NULL, "DAC R2" },
	{ "DAC 1", NULL, "DAC L1" },
	{ "DAC 1", NULL, "DAC R1" },
	{ "HPOVOL L", "Switch", "HPOVOL MIXL" },
	{ "HPOVOL R", "Switch", "HPOVOL MIXR" },
	{ "HPOVOL", NULL, "HPOVOL L" },
	{ "HPOVOL", NULL, "HPOVOL R" },
	{ "HPO MIX", "DAC1 Switch", "DAC 1" },
	{ "HPO MIX", "HPVOL Switch", "HPOVOL" },

	{ "SPKVOL L", "Switch", "SPK MIXL" },
	{ "SPKVOL R", "Switch", "SPK MIXR" },

	{ "SPOL MIX", "DAC R1 Switch", "DAC R1" },
	{ "SPOL MIX", "DAC L1 Switch", "DAC L1" },
	{ "SPOL MIX", "SPKVOL R Switch", "SPKVOL R" },
	{ "SPOL MIX", "SPKVOL L Switch", "SPKVOL L" },
	{ "SPOL MIX", "BST3 Switch", "BST3" },
	{ "SPOR MIX", "DAC R1 Switch", "DAC R1" },
	{ "SPOR MIX", "SPKVOL R Switch", "SPKVOL R" },
	{ "SPOR MIX", "BST3 Switch", "BST3" },

	{ "LOUT MIX", "DAC L1 Switch", "DAC L1" },
	{ "LOUT MIX", "DAC R1 Switch", "DAC R1" },
	{ "LOUT MIX", "OUTMIX L Switch", "OUT MIXL" },
	{ "LOUT MIX", "OUTMIX R Switch", "OUT MIXR" },

	{ "MONOVOL MIX", "DAC R1 Switch", "DAC R1" },
	{ "MONOVOL MIX", "DAC R2 Switch", "DAC R2" },
	{ "MONOVOL MIX", "DAC L2 Switch", "DAC L2" },
	{ "MONOVOL MIX", "BST3 Switch", "BST3" },
	{ "MONOVOL MIX", "BST2 Switch", "BST2" },

	{ "MONOVOL", "Switch", "MONOVOL MIX" },

	{ "MONOAmp MIX",  "DAC L2 Switch", "DAC L2" },
	{ "MONOAmp MIX",  "MONOVOL Switch", "MONOVOL" },

	{ "PDM1 L Mux", "Stereo DAC", "Stereo DAC MIXL" },
	{ "PDM1 L Mux", "Mono DAC", "Mono DAC MIXL" },
	{ "PDM1 L Mux", NULL, "PDM1 Power" },
	{ "PDM1 R Mux", "Stereo DAC", "Stereo DAC MIXR" },
	{ "PDM1 R Mux", "Mono DAC", "Mono DAC MIXR" },
	{ "PDM1 R Mux", NULL, "PDM1 Power" },

	{ "HP amp", NULL, "HPO MIX" },
	{ "HP amp", NULL, "JD Power" },
	{ "HP amp", NULL, "Mic Det Power" },
	{ "HP amp", NULL, "LDO2" },
	{ "HPOL", NULL, "HP amp" },
	{ "HPOR", NULL, "HP amp" },

	{ "LOUT amp", NULL, "LOUT MIX" },
	{ "LOUTL", NULL, "LOUT amp" },
	{ "LOUTR", NULL, "LOUT amp" },

	{ "Mono amp", NULL, "MONOAmp MIX" },
	{ "MonoP", NULL, "Mono amp" },
	{ "MonoN", NULL, "Mono amp" },

	{ "PDM L", "Switch", "PDM1 L Mux" },
	{ "PDM R", "Switch", "PDM1 R Mux" },
	{ "PDM1L", NULL, "PDM L" },
	{ "PDM1R", NULL, "PDM R" },

	{ "SPK amp", NULL, "SPOL MIX" },
	{ "SPK amp", NULL, "SPOR MIX" },
	{ "SPOL", NULL, "SPK amp" },
	{ "SPOR", NULL, "SPK amp" },
};

static int get_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16};

#ifdef USE_ASRC
	return 0;
#endif
	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 8;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int rt5647_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	unsigned int val_len = 0, val_clk, mask_clk;
	int pre_div, bclk_ms, frame_size;
	
	rt5647->lrck[dai->id] = params_rate(params);
	pre_div = get_clk_info(rt5647->sysclk, rt5647->lrck[dai->id]);
	if (pre_div < 0) {
		dev_err(codec->dev, "Unsupported clock setting\n");
		return -EINVAL;
	}
	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0) {
		dev_err(codec->dev, "Unsupported frame size: %d\n", frame_size);
		return -EINVAL;
	}
	bclk_ms = frame_size > 32 ? 1 : 0;
	rt5647->bclk[dai->id] = rt5647->lrck[dai->id] * (32 << bclk_ms);

	dev_dbg(dai->dev, "bclk is %dHz and lrck is %dHz\n",
		rt5647->bclk[dai->id], rt5647->lrck[dai->id]);
	dev_dbg(dai->dev, "bclk_ms is %d and pre_div is %d for iis %d\n",
				bclk_ms, pre_div, dai->id);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		val_len |= RT5647_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val_len |= RT5647_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S8:
		val_len |= RT5647_I2S_DL_8;
		break;
	default:
		return -EINVAL;
	}
//	printk("===========================rt5647_hw_params: dai->id=%d\n", dai->id);
	switch (dai->id) {
	case RT5647_AIF1:
 		mask_clk = RT5647_I2S_BCLK_MS1_MASK | RT5647_I2S_PD1_MASK;
		val_clk = bclk_ms << RT5647_I2S_BCLK_MS1_SFT |
			pre_div << RT5647_I2S_PD1_SFT;
		snd_soc_update_bits(codec, RT5647_I2S1_SDP,
			RT5647_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5647_ADDA_CLK1, mask_clk, val_clk);
		break;
	case  RT5647_AIF2:		
		mask_clk = RT5647_I2S_BCLK_MS2_MASK | RT5647_I2S_PD2_MASK;
		val_clk = bclk_ms << RT5647_I2S_BCLK_MS2_SFT |
			(pre_div << RT5647_I2S_PD2_SFT) |RT5647_I2S_PD2_12;
		snd_soc_update_bits(codec, RT5647_I2S2_SDP,
			RT5647_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5647_ADDA_CLK1, mask_clk, val_clk);
		break;
#ifdef USE_RIL		
//viola added for cp connection @2014.7.31		
	case  RT5647_AIF3:
		mask_clk = RT5647_I2S_BCLK_MS3_MASK | RT5647_I2S_PD3_MASK;
		val_clk = bclk_ms << RT5647_I2S_BCLK_MS3_SFT |
			(pre_div << RT5647_I2S_PD3_SFT)|RT5647_I2S_PD3_12;
		snd_soc_update_bits(codec, RT5647_I2S3_SDP,
			RT5647_I2S_DL_MASK, val_len);
		snd_soc_update_bits(codec, RT5647_ADDA_CLK1, mask_clk, val_clk);
		break;
#endif		
	default:
		dev_err(codec->dev, "Invalid dai->id: %d\n", dai->id);
		return -EINVAL;
	}

	return 0;
}

static int rt5647_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);

	rt5647->aif_pu = dai->id;
	return 0;
}

static int rt5647_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		rt5647->master[dai->id] = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		reg_val |= RT5647_I2S_MS_S;
		rt5647->master[dai->id] = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		reg_val |= RT5647_I2S_BP_INV;
		break;
	default:
		return -EINVAL;
	}
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg_val |= RT5647_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		reg_val |= RT5647_I2S_DF_PCM_A;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		reg_val |= RT5647_I2S_DF_PCM_B;
		break;
	default:
		return -EINVAL;
	}
	switch (dai->id) {
	case RT5647_AIF1:
		snd_soc_update_bits(codec, RT5647_I2S1_SDP,
			RT5647_I2S_MS_MASK | RT5647_I2S_BP_MASK |
			RT5647_I2S_DF_MASK, reg_val);
		break;
	case  RT5647_AIF2:
		snd_soc_update_bits(codec, RT5647_I2S2_SDP,
			RT5647_I2S_MS_MASK | RT5647_I2S_BP_MASK |
			RT5647_I2S_DF_MASK, reg_val);
		break;
#ifdef USE_RIL
//viola added for cp connection @2014.7.31			
	case  RT5647_AIF3:
		snd_soc_update_bits(codec, RT5647_I2S3_SDP,
			RT5647_I2S_MS_MASK | RT5647_I2S_BP_MASK |
			RT5647_I2S_DF_MASK, reg_val);
//		printk("rt5647_set_dai_fmt:RT5647_I2S3_SDP=%x\n",snd_soc_read(codec,RT5647_I2S3_SDP));
		break;		
#endif		
	default:
		dev_err(codec->dev, "Invalid dai->id: %d\n", dai->id);
		return -EINVAL;
	}
	return 0;
}

static int rt5647_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	if (freq == rt5647->sysclk && clk_id == rt5647->sysclk_src)
		return 0;

	switch (clk_id) {
	case RT5647_SCLK_S_MCLK:
		reg_val |= RT5647_SCLK_SRC_MCLK;
		break;
	case RT5647_SCLK_S_PLL1:
		reg_val |= RT5647_SCLK_SRC_PLL1;
		break;
	case RT5647_SCLK_S_RCCLK:
		reg_val |= RT5647_SCLK_SRC_RCCLK;
		break;
	default:
		dev_err(codec->dev, "Invalid clock id (%d)\n", clk_id);
		return -EINVAL;
	}
	snd_soc_update_bits(codec, RT5647_GLB_CLK,
		RT5647_SCLK_SRC_MASK, reg_val);
	rt5647->sysclk = freq;
	rt5647->sysclk_src = clk_id;

	dev_dbg(dai->dev, "Sysclk is %dHz and clock id is %d\n", freq, clk_id);

	return 0;
}

/**
 * rt5647_pll_calc - Calcualte PLL M/N/K code.
 * @freq_in: external clock provided to codec.
 * @freq_out: target clock which codec works on.
 * @pll_code: Pointer to structure with M, N, K and bypass flag.
 *
 * Calcualte M/N/K code to configure PLL for codec. And K is assigned to 2
 * which make calculation more efficiently.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5647_pll_calc(const unsigned int freq_in,
	const unsigned int freq_out, struct rt5647_pll_code *pll_code)
{
	int max_n = RT5647_PLL_N_MAX, max_m = RT5647_PLL_M_MAX;
	int k, n, m, red, n_t, m_t, pll_out, in_t, out_t;
	int red_t = abs(freq_out - freq_in);
	bool bypass = false;

	if (RT5647_PLL_INP_MAX < freq_in || RT5647_PLL_INP_MIN > freq_in)
		return -EINVAL;

	k = 100000000 / freq_out - 2;
	if (k > RT5647_PLL_K_MAX)
		k = RT5647_PLL_K_MAX;
	for (n_t = 0; n_t <= max_n; n_t++) {
		in_t = freq_in / (k + 2);
		pll_out = freq_out / (n_t + 2);
		if (in_t < 0)
			continue;
		if (in_t == pll_out) {
			bypass = true;
			n = n_t;
			goto code_find;
		}
		red = abs(in_t - pll_out);
		if (red < red_t) {
			bypass = true;
			n = n_t;
			m = m_t;
			if (red == 0)
				goto code_find;
			red_t = red;
		}
		for (m_t = 0; m_t <= max_m; m_t++) {
			out_t = in_t / (m_t + 2);
			red = abs(out_t - pll_out);
			if (red < red_t) {
				bypass = false;
				n = n_t;
				m = m_t;
				if (red == 0)
					goto code_find;
				red_t = red;
			}
		}
	}
	pr_debug("Only get approximation about PLL\n");

code_find:

	pll_code->m_bp = bypass;
	pll_code->m_code = m;
	pll_code->n_code = n;
	pll_code->k_code = k;
	return 0;
}

static int rt5647_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	struct rt5647_pll_code pll_code;
	int ret;

#if 0 //viola added for debug 2014.8.19:q
	if (source == rt5647->pll_src && freq_in == rt5647->pll_in &&
	    freq_out == rt5647->pll_out)
		return 0;
#endif	

	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");

		rt5647->pll_in = 0;
		rt5647->pll_out = 0;
		snd_soc_update_bits(codec, RT5647_GLB_CLK,
			RT5647_SCLK_SRC_MASK, RT5647_SCLK_SRC_MCLK);
		return 0;
	}

	switch (source) {
	case RT5647_PLL1_S_MCLK:
		snd_soc_update_bits(codec, RT5647_GLB_CLK,
			RT5647_PLL1_SRC_MASK, RT5647_PLL1_SRC_MCLK);
		break;
	case RT5647_PLL1_S_BCLK1:
	case RT5647_PLL1_S_BCLK2:
		switch (dai->id) {
		case RT5647_AIF1:
			snd_soc_update_bits(codec, RT5647_GLB_CLK,
				RT5647_PLL1_SRC_MASK, RT5647_PLL1_SRC_BCLK1);
			break;
		case  RT5647_AIF2:
			snd_soc_update_bits(codec, RT5647_GLB_CLK,
				RT5647_PLL1_SRC_MASK, RT5647_PLL1_SRC_BCLK2);
			break;
#ifdef USE_RIL
//viola added for aif3 @2014.7.31			
		case RT5647_AIF3:	
			snd_soc_update_bits(codec, RT5647_GLB_CLK,
				RT5647_PLL1_SRC_MASK,  RT5647_PLL1_SRC_BCLK3);//RT5647_PLL1_SRC_MCLK);
			break;
#endif
		default:
			dev_err(codec->dev, "Invalid dai->id: %d\n", dai->id);
			return -EINVAL;
		}
		break;
	default:
		dev_err(codec->dev, "Unknown PLL source %d\n", source);
		return -EINVAL;
	}

	ret = rt5647_pll_calc(freq_in, freq_out, &pll_code);

	if (ret < 0) {
		dev_err(codec->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

	dev_dbg(codec->dev, "bypass=%d m=%d n=%d k=%d\n",
		pll_code.m_bp, (pll_code.m_bp ? 0 : pll_code.m_code),
		pll_code.n_code, pll_code.k_code);
	
//viola added to force pll out @1024.8.19	
	if (0){ //(dai->id)> RT5647_AIF1){
		snd_soc_write(codec, RT5647_PLL_CTRL1,0x1895);
		snd_soc_write(codec, RT5647_PLL_CTRL2,0xb000);
		}
		else {
		snd_soc_write(codec, RT5647_PLL_CTRL1,
			pll_code.n_code << RT5647_PLL_N_SFT | pll_code.k_code);
		snd_soc_write(codec, RT5647_PLL_CTRL2,
			(pll_code.m_bp ? 0 : pll_code.m_code) << RT5647_PLL_M_SFT |
			pll_code.m_bp << RT5647_PLL_M_BP_SFT);
	}
	
//	printk("RT5647_PLL_CTRL1=%x, RT5647_PLL_CTRL2=%x\n", snd_soc_read(codec,RT5647_PLL_CTRL1 ),snd_soc_read(codec,RT5647_PLL_CTRL2 ));
		
	rt5647->pll_in = freq_in;
	rt5647->pll_out = freq_out;
	rt5647->pll_src = source;

	return 0;
}

static int rt5647_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
			unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int val = 0;

	if (rx_mask || tx_mask)
		val |= (1 << 14);

	switch (slots) {
	case 4:
		val |= (1 << 12);
		break;
	case 6:
		val |= (2 << 12);
		break;
	case 8:
		val |= (3 << 12);
		break;
	case 2:
	default:
		break;
	}

	switch (slot_width) {
	case 20:
		val |= (1 << 10);
		break;
	case 24:
		val |= (2 << 10);
		break;
	case 32:
		val |= (3 << 10);
		break;
	case 16:
	default:
		break;
	}

	snd_soc_update_bits(codec, RT5647_TDM_CTRL_1, 0x7c00, val);

	return 0;
}

/**
 * rt5647_index_show - Dump private registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all private registers.
 *
 * Returns buffer length.
 */
static ssize_t rt5647_index_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5647_priv *rt5647 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5647->codec;
	unsigned int val;
	int cnt = 0, i;

	cnt += sprintf(buf, "RT5647 index register\n");
	for (i = 0; i < 0xff; i++) {
		if (cnt + RT5647_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5647_index_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5647_REG_DISP_LEN,
				"%02x: %04x\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5647_index_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5647_priv *rt5647 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5647->codec;
	unsigned int val=0,addr=0;
	int i;

	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i)-'0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i + 1 ; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n",addr,val);
	if (addr > RT5647_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_info("0x%02x = 0x%04x\n", addr,
			rt5647_index_read(codec, addr));
	else
		rt5647_index_write(codec, addr, val);

	return count;
}
static DEVICE_ATTR(index_reg, 0666, rt5647_index_show, rt5647_index_store);

static ssize_t rt5647_codec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5647_priv *rt5647 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5647->codec;
	unsigned int val;
	int cnt = 0, i;
codec->cache_bypass = 1;
	for (i = 0; i <= RT5647_VENDOR_ID2; i++) {
		if (cnt + RT5647_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = snd_soc_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, RT5647_REG_DISP_LEN,
				"#rng%02x  #rv%04x  #rd0\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;
codec->cache_bypass = 0;
	return cnt;
}

static ssize_t rt5647_codec_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5647_priv *rt5647 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5647->codec;
	unsigned int val=0,addr=0;
	int i;

//	printk("register \"%s\" count=%d\n",buf, (unsigned int(count)));
	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i)-'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i + 1 ; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i)-'0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i)-'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}
	pr_debug("addr=0x%x val=0x%x\n",addr,val);
	if (addr > RT5647_VENDOR_ID2 || val > 0xffff || val < 0)
		return count;

	if (i == count)
		pr_info("0x%02x = 0x%04x\n",addr,codec->hw_read(codec, addr));
	else
		snd_soc_write(codec, addr, val);


	return count;
}

static DEVICE_ATTR(codec_reg, 0666, rt5647_codec_show, rt5647_codec_store);

static int rt5647_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		pm_qos_update_request(&exynos5_audio_mif_qos, 165000);
		break;

	case SND_SOC_BIAS_PREPARE:
		pm_qos_update_request(&exynos5_audio_mif_qos, 103000);
		if (SND_SOC_BIAS_STANDBY == codec->dapm.bias_level) {
			snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
				RT5647_PWR_VREF1 | RT5647_PWR_MB |
				RT5647_PWR_BG | RT5647_PWR_VREF2,
				RT5647_PWR_VREF1 | RT5647_PWR_MB |
				RT5647_PWR_BG | RT5647_PWR_VREF2);
			msleep(10);
			snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
				RT5647_PWR_FV1 | RT5647_PWR_FV2,
				RT5647_PWR_FV1 | RT5647_PWR_FV2);
			snd_soc_update_bits(codec, RT5647_DIG_MISC,
				RT5647_DIG_GATE_CTRL, RT5647_DIG_GATE_CTRL);
			snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
				RT5647_LDO_SEL_MASK, 0x2);
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		pm_qos_update_request(&exynos5_audio_mif_qos, 103000);
		break;

	case SND_SOC_BIAS_OFF:
		pm_qos_update_request(&exynos5_audio_mif_qos, 103000);
		snd_soc_write(codec, RT5647_DEPOP_M2, 0x1100);
		snd_soc_write(codec, RT5647_DIG_MISC, 0x0120);
		snd_soc_write(codec, RT5647_PWR_DIG1, 0x0000);
		snd_soc_write(codec, RT5647_PWR_DIG2, 0x0000);
		snd_soc_write(codec, RT5647_PWR_VOL, 0x0000);
		snd_soc_write(codec, RT5647_PWR_MIXER, 0x0000);
#ifdef CONFIG_SND_JD1_FUNC 
		snd_soc_write(codec, RT5647_PWR_ANLG1, 0x2000);
		snd_soc_write(codec, RT5647_PWR_ANLG2, 0x0004);
		printk("CONFIG_SND_JD1_FUNC\n");			
#endif
#ifdef CONFIG_SND_JD2_FUNC
		snd_soc_write(codec, RT5647_PWR_ANLG1, 0x2000);
		snd_soc_write(codec, RT5647_PWR_ANLG2, 0x0002); /*Pow_jd2*/
		printk("CONFIG_SND_JD2_FUNC\n");
#endif
//		snd_soc_write(codec, RT5647_PWR_ANLG1, 0x0000);
//		snd_soc_write(codec, RT5647_PWR_ANLG2, 0x0000);
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

int rt5647_jack_assign(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_jack *jack;

	dev_err(codec->dev, "%s:+++++",__func__);

	jack = kzalloc(sizeof(struct snd_soc_jack), GFP_KERNEL);
	ret = snd_soc_jack_new(codec, "rt5647 jack", SND_JACK_HEADSET, jack);
	if (ret != 0) {
		dev_err(codec->dev, "%s:snd_soc_jack_new is failed.",__func__);
	} else {
		rt5647->jack = jack;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(rt5647_jack_assign);

void rt5647_jack_remove(struct snd_soc_codec *codec)
{
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	kfree(rt5647->jack);
	rt5647->jack = NULL;
}
EXPORT_SYMBOL_GPL(rt5647_jack_remove);

static int rt5647_probe(struct snd_soc_codec *codec)
{
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	struct rt_codec_ops *ioctl_ops = rt_codec_get_ioctl_ops();
#endif
#endif
	int ret;

	pr_info("Codec driver version %s\n", VERSION); 
//viola @2014.6.16 : allow codec to enter suspend
//	codec->dapm.idle_bias_off = 1;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	ret = rt5647_reset(codec);
	if (ret <0) {
		dev_err(codec->dev,"i2c communication error, check whether the codec subboard is placed\n");
		return ret;
	}
	snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
		RT5647_PWR_VREF1 | RT5647_PWR_MB |
		RT5647_PWR_BG | RT5647_PWR_VREF2,
		RT5647_PWR_VREF1 | RT5647_PWR_MB |
		RT5647_PWR_BG | RT5647_PWR_VREF2);
	msleep(10);
	snd_soc_update_bits(codec, RT5647_PWR_ANLG1,
		RT5647_PWR_FV1 | RT5647_PWR_FV2,
		RT5647_PWR_FV1 | RT5647_PWR_FV2);

	rt5647_reg_init(codec);

	/*config gpio1 to irq*/
	snd_soc_update_bits(codec, RT5647_GPIO_CTRL1, RT5647_GP1_PIN_MASK, RT5647_GP1_PIN_IRQ);

#ifdef CONFIG_SND_JD1_FUNC 
	snd_soc_update_bits(codec, RT5647_HPO_MIXER, 0x1000, 0x1000);
	snd_soc_update_bits(codec, RT5647_PWR_ANLG2, 0x0004, 0x0004);
#endif

#ifdef CONFIG_SND_JD2_FUNC
	snd_soc_update_bits(codec, RT5647_HPO_MIXER, 0x1000, 0x1000);

	/*irq source is jd2*/	
/* disable hardware selection viola @ 2014.5.6*/	
	/*reg 0xf8  JD Trigger Source Selection for HPO*/
//	snd_soc_update_bits(codec, RT5647_JD_CTRL3, RT5647_JD_TRI_HPO_SEL_MASK,  RT5647_JD_F_JD2);
	
	 /*reg 0xf9   JD Trigger Source Selection for SPK_OUT*/
//	snd_soc_update_bits(codec, RT5647_JD_CTRL4, 0x7 << 12, 0x3 << 12);
/*end of viola @ 2014.5.6*/

	/*config irq source*/ 
	/*en_irq_jd2 = 1,  en_jd2_sticky = 0, inv_jd2 = 0*/
	snd_soc_update_bits(codec, RT5647_IRQ_CTRL2, 0x8, 0x8); 

/* disable hardware selection viola @ 2014.5.6*/	
	/*jd trigger HPO  and jd trigger SPK*/	
//	snd_soc_update_bits(codec, RT5647_JD_CTRL, RT5647_JD_HP_MASK|RT5647_JD_HP_TRG_MASK, 
//	                                                RT5647_JD_HP_EN|RT5647_JD_HP_TRG_LO); 
	
//	snd_soc_update_bits(codec, RT5647_JD_CTRL, RT5647_JD_SPL_MASK|RT5647_JD_SPR_MASK|RT5647_JD_SPL_TRG_MASK|RT5647_JD_SPR_TRG_MASK, 
//		                                        RT5647_JD_SPL_EN|RT5647_JD_SPR_EN|RT5647_JD_SPL_TRG_HI|RT5647_JD_SPR_TRG_HI); 
/*end of viola @ 2014.5.6*/
	/*Pow_jd2*/
	snd_soc_update_bits(codec, RT5647_PWR_ANLG2, 0x0002, 0x0002); 
#endif

	snd_soc_update_bits(codec, RT5647_PWR_ANLG1, RT5647_LDO_SEL_MASK, 0x0);

	/* dc_calibrate(codec); */
	rt5647->codec = codec;
	rt5647->combo_jack_en = true; /* enable combo jack */

	snd_soc_add_codec_controls(codec, rt5647_snd_controls,
			ARRAY_SIZE(rt5647_snd_controls));
	snd_soc_dapm_new_controls(&codec->dapm, rt5647_dapm_widgets,
			ARRAY_SIZE(rt5647_dapm_widgets));
	snd_soc_dapm_add_routes(&codec->dapm, rt5647_dapm_routes,
			ARRAY_SIZE(rt5647_dapm_routes));

#ifdef RTK_IOCTL
#if defined(CONFIG_SND_HWDEP) || defined(CONFIG_SND_HWDEP_MODULE)
	ioctl_ops->index_write = rt5647_index_write;
	ioctl_ops->index_read = rt5647_index_read;
	ioctl_ops->index_update_bits = rt5647_index_update_bits;
	ioctl_ops->ioctl_common = rt5647_ioctl_common;
	realtek_ce_init_hwdep(codec);
#endif
#endif

	ret = device_create_file(codec->dev, &dev_attr_index_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create index_reg sysfs files: %d\n", ret);
		return ret;
	}

	ret = device_create_file(codec->dev, &dev_attr_codec_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create codex_reg sysfs files: %d\n", ret);
		return ret;
	}

	rt5647->jack_type = 0;
//add qos 	
	pm_qos_add_request(&exynos5_audio_mif_qos, PM_QOS_BUS_THROUGHPUT, 103000);

	return 0;
}

static int rt5647_remove(struct snd_soc_codec *codec)
{
printk("%s =====================\n",__func__);
	rt5647_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int rt5647_suspend(struct snd_soc_codec *codec)
{
printk("%s =====================\n",__func__);
	rt5647_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int rt5647_resume(struct snd_soc_codec *codec)
{
printk("%s =====================\n",__func__);
	codec->cache_only = false;
	codec->cache_sync = 1;
	snd_soc_cache_sync(codec);
	rt5647_index_sync(codec);

	snd_soc_write(codec, RT5647_PWR_MIXER, 0x0002);
	snd_soc_write(codec, RT5647_A_JD_CTRL1, 0x0271);
	snd_soc_write(codec, RT5647_IRQ_CTRL2, 0x0280);
	snd_soc_update_bits(codec, RT5647_CJ_CTRL2,
			RT5647_CBJ_DET_MODE, 0x1<<7);
	
	return 0;
}
#else
#define rt5647_suspend NULL
#define rt5647_resume NULL
#endif

#define RT5647_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define RT5647_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt5647_aif_dai_ops = {
	.hw_params = rt5647_hw_params,
	.prepare = rt5647_prepare,
	.set_fmt = rt5647_set_dai_fmt,
	.set_sysclk = rt5647_set_dai_sysclk,
	.set_tdm_slot = rt5647_set_tdm_slot,
	.set_pll = rt5647_set_dai_pll,
};

struct snd_soc_dai_driver rt5647_dai[] = {
	{
		.name = "rt5647-aif1",
		.id = RT5647_AIF1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5647_STEREO_RATES,
			.formats = RT5647_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5647_STEREO_RATES,
			.formats = RT5647_FORMATS,
		},
		.ops = &rt5647_aif_dai_ops,
	},
	{
		.name = "rt5647-aif2",
		.id = RT5647_AIF2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5647_STEREO_RATES,
			.formats = RT5647_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5647_STEREO_RATES,
			.formats = RT5647_FORMATS,
		},
		.ops = &rt5647_aif_dai_ops,
	},
	{
		.name = "rt5647-aif3",
		.id = RT5647_AIF3,
		.playback = {
			.stream_name = "AIF3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5647_STEREO_RATES,
			.formats = RT5647_FORMATS,
		},
		.capture = {
			.stream_name = "AIF3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5647_STEREO_RATES,
			.formats = RT5647_FORMATS,
		},
		.ops = &rt5647_aif_dai_ops,
	},
};

static struct snd_soc_codec_driver soc_codec_dev_rt5647 = {
	.probe = rt5647_probe,
	.remove = rt5647_remove,
	.suspend = rt5647_suspend,
	.resume = rt5647_resume,
	.set_bias_level = rt5647_set_bias_level,
	.reg_cache_size = RT5647_VENDOR_ID2 + 1,
	.reg_word_size = sizeof(u16),
	.reg_cache_default = rt5647_reg,
	.volatile_register = rt5647_volatile_register,
	.readable_register = rt5647_readable_register,
	.reg_cache_step = 1,
};

static const struct i2c_device_id rt5647_i2c_id[] = {
	{ "rt5647", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5647_i2c_id);

static int __devinit rt5647_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	//struct rtxxxx_platform_data *pdata = i2c->dev.platform_data;
	struct rt5647_priv *rt5647;
	int ret;

	rt5647 = kzalloc(sizeof(struct rt5647_priv), GFP_KERNEL);
	if (NULL == rt5647)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5647);
	//rt5647->pdata = pdata;

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt5647,
			rt5647_dai, ARRAY_SIZE(rt5647_dai));
	if (ret < 0)
		kfree(rt5647);

	return ret;
}

static int __devexit rt5647_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

void rt5647_i2c_shutdown(struct i2c_client *client)
{
	struct rt5647_priv *rt5647 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5647->codec;

	if (codec != NULL)
		rt5647_set_bias_level(codec, SND_SOC_BIAS_OFF);
}


static const struct of_device_id rt5647_of_match[] = {
	{ .compatible = "realtek,rt5647", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt5647_of_match);

struct i2c_driver rt5647_i2c_driver = {
	.driver = {
		.name = "rt5647",
		.owner = THIS_MODULE,
		.of_match_table = rt5647_of_match,
	},
	.probe = rt5647_i2c_probe,
	.remove   = __devexit_p(rt5647_i2c_remove),
	.shutdown = rt5647_i2c_shutdown,
	.id_table = rt5647_i2c_id,
};

static int __init rt5647_modinit(void)
{
	return i2c_add_driver(&rt5647_i2c_driver);
}
module_init(rt5647_modinit);

static void __exit rt5647_modexit(void)
{
	i2c_del_driver(&rt5647_i2c_driver);
}
module_exit(rt5647_modexit);

MODULE_DESCRIPTION("ASoC RT5647 driver");
MODULE_AUTHOR("Bard Liao <bardliao@realtek.com>");
MODULE_LICENSE("GPL");
