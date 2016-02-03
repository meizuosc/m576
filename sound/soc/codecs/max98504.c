/*
 * max98504.c -- MAX98504 ALSA SoC Audio driver
 *
 * Copyright 2013-2014 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG_MAX98504
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/max98504.h>
#include "max98504.h"

#include <linux/version.h>

#define SUPPORT_DEVICE_TREE

#ifdef SUPPORT_DEVICE_TREE
#include <linux/regulator/consumer.h>
#endif

#ifdef DEBUG_MAX98504
#define msg_maxim(format, args...)	\
	printk(KERN_INFO "[MAX98504_DEBUG] %s " format, __func__, ## args)
#else
#define msg_maxim(format, args...)
#endif


static const u8 max98504_reg_def[MAX98504_REG_CNT] = {
	[MAX98504_REG_01_INTERRUPT_STATUS] = 0,
	[MAX98504_REG_02_INTERRUPT_FLAGS] = 0,
	[MAX98504_REG_03_INTERRUPT_ENABLES] = 0,
	[MAX98504_REG_04_INTERRUPT_FLAG_CLEARS] = 0,
	[MAX98504_REG_10_GPIO_ENABLE] = 0,
	[MAX98504_REG_11_GPIO_CONFIG] = 0,
	[MAX98504_REG_12_WATCHDOG_ENABLE] = 0,
	[MAX98504_REG_13_WATCHDOG_CONFIG] = 0,
	[MAX98504_REG_14_WATCHDOG_CLEAR] = 0,
	[MAX98504_REG_15_CLOCK_MONITOR_ENABLE] = 0,
	[MAX98504_REG_16_PVDD_BROWNOUT_ENABLE] = 0,
	[MAX98504_REG_17_PVDD_BROWNOUT_CONFIG_1] = 0,
	[MAX98504_REG_18_PVDD_BROWNOUT_CONFIG_2] = 0,
	[MAX98504_REG_19_PVDD_BROWNOUT_CONFIG_3] = 0,
	[MAX98504_REG_1A_PVDD_BROWNOUT_CONFIG_4] = 0,
	[MAX98504_REG_20_PCM_RX_ENABLES] = 0,
	[MAX98504_REG_21_PCM_TX_ENABLES] = 0,
	[MAX98504_REG_22_PCM_TX_HIZ_CONTROL] = 0,
	[MAX98504_REG_23_PCM_TX_CHANNEL_SOURCES] = 0,
	[MAX98504_REG_24_PCM_MODE_CONFIG] = 0,
	[MAX98504_REG_25_PCM_DSP_CONFIG] = 0,
	[MAX98504_REG_26_PCM_CLOCK_SETUP] = 0,
	[MAX98504_REG_27_PCM_SAMPLE_RATE_SETUP] = 0,
	[MAX98504_REG_28_PCM_TO_SPEAKER_MONOMIX] = 0,
	[MAX98504_REG_30_PDM_TX_ENABLES] = 0,
	[MAX98504_REG_31_PDM_TX_HIZ_CONTROL] = 0,
	[MAX98504_REG_32_PDM_TX_CONTROL] = 0,
	[MAX98504_REG_33_PDM_RX_ENABLE] = 0,
	[MAX98504_REG_34_SPEAKER_ENABLE] = 0,
	[MAX98504_REG_35_SPEAKER_SOURCE_SELECT] = 0,
	[MAX98504_REG_36_MEASUREMENT_ENABLES] = 0,
	[MAX98504_REG_37_ANALOGUE_INPUT_GAIN] = 0,
	[MAX98504_REG_38_TEMPERATURE_LIMIT_CONFIG] = 0,
	[MAX98504_REG_39_ANALOGUE_SPARE] = 0,
	[MAX98504_REG_40_GLOBAL_ENABLE] = 0,
	[MAX98504_REG_41_SOFTWARE_RESET] = 0,
	[MAX98504_REG_80_AUTHENTICATION_KEY_0] = 0,
	[MAX98504_REG_81_AUTHENTICATION_KEY_1] = 0,
	[MAX98504_REG_82_AUTHENTICATION_KEY_2] = 0,
	[MAX98504_REG_83_AUTHENTICATION_KEY_3] = 0,
	[MAX98504_REG_84_AUTHENTICATION_ENABLE] = 0,
	[MAX98504_REG_85_AUTHENTICATION_RESULT_0] = 0,
	[MAX98504_REG_86_AUTHENTICATION_RESULT_1] = 0,
	[MAX98504_REG_87_AUTHENTICATION_RESULT_2] = 0,
	[MAX98504_REG_88_AUTHENTICATION_RESULT_3] = 0,
	[MAX98504_REG_89_AUTHENTICATION_RESULT_4] = 0,
	[MAX98504_REG_8A_AUTHENTICATION_RESULT_5] = 0,
	[MAX98504_REG_8B_AUTHENTICATION_RESULT_6] = 0,
	[MAX98504_REG_8C_AUTHENTICATION_RESULT_7] = 0,
	[MAX98504_REG_8D_AUTHENTICATION_RESULT_8] = 0,
	[MAX98504_REG_8E_AUTHENTICATION_RESULT_9] = 0,
	[MAX98504_REG_8F_AUTHENTICATION_RESULT_10] = 0,
	[MAX98504_REG_90_AUTHENTICATION_RESULT_11] = 0,
	[MAX98504_REG_91_AUTHENTICATION_RESULT_12] = 0,
	[MAX98504_REG_92_AUTHENTICATION_RESULT_13] = 0,
	[MAX98504_REG_93_AUTHENTICATION_RESULT_14] = 0,
	[MAX98504_REG_94_AUTHENTICATION_RESULT_15] = 0,
};

static struct {
	u8 read;
	u8 write;
	u8 vol;
} max98504_reg_access[MAX98504_REG_CNT] = {
	[MAX98504_REG_01_INTERRUPT_STATUS] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_02_INTERRUPT_FLAGS] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_03_INTERRUPT_ENABLES] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_04_INTERRUPT_FLAG_CLEARS] = { 0x00, 0xFF, 0xFF },
	[MAX98504_REG_10_GPIO_ENABLE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_11_GPIO_CONFIG] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_12_WATCHDOG_ENABLE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_13_WATCHDOG_CONFIG] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_14_WATCHDOG_CLEAR] = { 0x00, 0xFF, 0xFF },
	[MAX98504_REG_15_CLOCK_MONITOR_ENABLE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_16_PVDD_BROWNOUT_ENABLE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_17_PVDD_BROWNOUT_CONFIG_1] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_18_PVDD_BROWNOUT_CONFIG_2] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_19_PVDD_BROWNOUT_CONFIG_3] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_1A_PVDD_BROWNOUT_CONFIG_4] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_20_PCM_RX_ENABLES] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_21_PCM_TX_ENABLES] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_22_PCM_TX_HIZ_CONTROL] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_23_PCM_TX_CHANNEL_SOURCES] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_24_PCM_MODE_CONFIG] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_25_PCM_DSP_CONFIG] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_26_PCM_CLOCK_SETUP] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_27_PCM_SAMPLE_RATE_SETUP] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_28_PCM_TO_SPEAKER_MONOMIX] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_30_PDM_TX_ENABLES] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_31_PDM_TX_HIZ_CONTROL] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_32_PDM_TX_CONTROL] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_33_PDM_RX_ENABLE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_34_SPEAKER_ENABLE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_35_SPEAKER_SOURCE_SELECT] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_36_MEASUREMENT_ENABLES] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_37_ANALOGUE_INPUT_GAIN] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_38_TEMPERATURE_LIMIT_CONFIG] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_39_ANALOGUE_SPARE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_40_GLOBAL_ENABLE] = { 0xFF, 0xFF, 0xFF },
	[MAX98504_REG_41_SOFTWARE_RESET] = { 0x00, 0xFF, 0xFF },
	[MAX98504_REG_80_AUTHENTICATION_KEY_0] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_81_AUTHENTICATION_KEY_1] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_82_AUTHENTICATION_KEY_2] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_83_AUTHENTICATION_KEY_3] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_84_AUTHENTICATION_ENABLE] = { 0xFF, 0xFF, 0x00 },
	[MAX98504_REG_85_AUTHENTICATION_RESULT_0] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_86_AUTHENTICATION_RESULT_1] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_87_AUTHENTICATION_RESULT_2] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_88_AUTHENTICATION_RESULT_3] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_89_AUTHENTICATION_RESULT_4] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_8A_AUTHENTICATION_RESULT_5] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_8B_AUTHENTICATION_RESULT_6] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_8C_AUTHENTICATION_RESULT_7] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_8D_AUTHENTICATION_RESULT_8] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_8E_AUTHENTICATION_RESULT_9] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_8F_AUTHENTICATION_RESULT_10] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_90_AUTHENTICATION_RESULT_11] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_91_AUTHENTICATION_RESULT_12] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_92_AUTHENTICATION_RESULT_13] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_93_AUTHENTICATION_RESULT_14] = { 0xFF, 0x00, 0xFF },
	[MAX98504_REG_94_AUTHENTICATION_RESULT_15] = { 0xFF, 0x00, 0xFF },
};

int max98504_write(struct i2c_client *max98504_i2c,
			  u16 reg, int val)
{
	u8 data[3];
	int ret;
	data[0] = (reg & 0xff00) >> 8;
	data[1] = (reg & 0x00ff);
	data[2] = val;

	ret = i2c_master_send(max98504_i2c, data, 3);

	if (ret < 0) {
		printk(KERN_ERR "%s: i2c_master_send failed ret = %d\n", __func__, ret);
	}

	return ret;
}

static int max98504_volatile_register(struct snd_soc_codec *codec, unsigned int reg)
{
	if (max98504_reg_access[reg].vol)
		return 1;
	else
		return 0;
}

static int max98504_readable(struct snd_soc_codec *codec, unsigned int reg)
{
	if (reg >= MAX98504_REG_CNT)
		return 0;
	return max98504_reg_access[reg].read != 0;
}

static int max98504_reset(struct snd_soc_codec *codec)
{
	int ret;
	msg_maxim("\n");

	/* Reset the codec by writing to this write-only reset register */
	ret = snd_soc_write(codec, MAX98504_REG_41_SOFTWARE_RESET, M98504_SOFTWARE_RESET_MASK);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to reset codec: %d\n", ret);
		return ret;
	}

	msleep(10);

	return ret;
}

static int max98504_rxpcm_gain_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int sel = ucontrol->value.integer.value[0];

	msg_maxim("val=%d\n",sel);

	snd_soc_update_bits(codec, MAX98504_REG_25_PCM_DSP_CONFIG,
	    M98504_PCM_DSP_CFG_RX_GAIN_MASK,
		sel << M98504_PCM_DSP_CFG_RX_GAIN_SHIFT);

	return 0;

}

static int max98504_rxpcm_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val = snd_soc_read(codec, MAX98504_REG_25_PCM_DSP_CONFIG);

	val = (val & M98504_PCM_DSP_CFG_RX_GAIN_MASK) >> M98504_PCM_DSP_CFG_RX_GAIN_SHIFT;

	ucontrol->value.integer.value[0] = val;
	msg_maxim("val=%d\n",val);

	return 0;

}

static int max98504_ain_gain_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int sel = ucontrol->value.integer.value[0];

	msg_maxim("val=%d\n",sel);

	snd_soc_update_bits(codec, MAX98504_REG_37_ANALOGUE_INPUT_GAIN,
	    M98504_ANALOG_INPUT_GAIN_MASK,
		sel << M98504_ANALOG_INPUT_GAIN_SHIFT);

	return 0;

}

static int max98504_ain_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val = snd_soc_read(codec, MAX98504_REG_37_ANALOGUE_INPUT_GAIN);

	val = (val & M98504_ANALOG_INPUT_GAIN_MASK) >> M98504_ANALOG_INPUT_GAIN_SHIFT;

	ucontrol->value.integer.value[0] = val;
	msg_maxim("val=%d\n",val);

	return 0;

}
static const unsigned int max98504_rxpcm_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 12, TLV_DB_SCALE_ITEM(0, 100, 0),
};

static const unsigned int max98504_ain_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 1, TLV_DB_SCALE_ITEM(1200, 600, 0),
};

static const char * max98504_enableddisabled_text[] = {"Disabled", "Enabled"};

static const struct soc_enum max98504_ispken_enum =
	SOC_ENUM_SINGLE(MAX98504_REG_36_MEASUREMENT_ENABLES, M98504_MEAS_I_EN_MASK,
		ARRAY_SIZE(max98504_enableddisabled_text), max98504_enableddisabled_text);

static const struct soc_enum max98504_vspken_enum =
	SOC_ENUM_SINGLE(MAX98504_REG_36_MEASUREMENT_ENABLES, M98504_MEAS_V_EN_MASK,
		ARRAY_SIZE(max98504_enableddisabled_text), max98504_enableddisabled_text);

static const char * max98504_vbatbrown_code_text[] = { "2.6V", "2.65V","Reserved", "Reserved","Reserved","Reserved","Reserved","Reserved",\
														"Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","Reserved",\
														"Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","Reserved",\
														"Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","Reserved","3.7V"};

static const struct soc_enum max98504_brownout_code_enum =
	SOC_ENUM_SINGLE(MAX98504_REG_17_PVDD_BROWNOUT_CONFIG_1, M98504_PVDD_BROWNOUT_CFG1_CODE_SHIFT, 31, max98504_vbatbrown_code_text);

static const char * max98504_vbatbrown_max_atten_text[] = {"0dB","1dB","2dB","3dB","4dB","5dB","6dB"};

static const struct soc_enum max98504_brownout_max_atten_enum =
	SOC_ENUM_SINGLE(MAX98504_REG_17_PVDD_BROWNOUT_CONFIG_1, M98504_PVDD_BROWNOUT_CFG1_MAX_ATTEN_SHIFT, 6, max98504_vbatbrown_max_atten_text);

static const char * max98504_flt_mode_text[] = {"Voice", "Music"};

static const struct soc_enum max98504_pcm_rx_flt_mode_enum =
	SOC_ENUM_SINGLE(MAX98504_REG_25_PCM_DSP_CONFIG, M98504_PCM_DSP_CFG_RX_FLT_MODE_SHIFT, 1, max98504_flt_mode_text);

static const char * max98504_pcm_bsel_text[] = {"Reserved","Reserved","32","48", "64", "Reserved", "128", "Reserved", "256"};

static const struct soc_enum max98504_pcm_bsel_enum =
	SOC_ENUM_SINGLE(MAX98504_REG_26_PCM_CLOCK_SETUP, M98504_PCM_CLK_SETUP_BSEL_SHIFT, 8, max98504_pcm_bsel_text);

static const struct snd_kcontrol_new max98504_snd_controls[] = {
	SOC_SINGLE("GPIO Pin Switch", MAX98504_REG_10_GPIO_ENABLE, M98504_GPIO_ENALBE_SHIFT, 1, 0),
	SOC_SINGLE("Watchdog Enable Switch", MAX98504_REG_12_WATCHDOG_ENABLE, M98504_WDOG_ENABLE_SHIFT, 1, 0),
	SOC_SINGLE("Watchdog Config Switch", MAX98504_REG_13_WATCHDOG_CONFIG, M98504_WDOG_CONFIG_SHIFT, 3, 0),
	SOC_SINGLE("Watchdog Clear Switch", MAX98504_REG_14_WATCHDOG_CLEAR, M98504_WDOG_CLEAR_SHIFT, 0xe9, 0),
	SOC_SINGLE("Clock Monitor Switch", MAX98504_REG_15_CLOCK_MONITOR_ENABLE, M98504_CMON_ENA_SHIFT, 1, 0),
	SOC_SINGLE("Brownout Protection Switch", MAX98504_REG_16_PVDD_BROWNOUT_ENABLE, M98504_CMON_ENA_SHIFT, 1, 0),
	SOC_ENUM("Brownout Threshold", max98504_brownout_code_enum),
	SOC_ENUM("Brownout Attenuation Value", max98504_brownout_max_atten_enum),
	SOC_SINGLE("Brownout Attack Hold Time", MAX98504_REG_18_PVDD_BROWNOUT_CONFIG_2, M98504_PVDD_BROWNOUT_CFG2_ATTK_HOLD_SHIFT, 255, 0),
	SOC_SINGLE("Brownout Timed Hold", MAX98504_REG_19_PVDD_BROWNOUT_CONFIG_3, M98504_PVDD_BROWNOUT_CFG3_TIMED_HOLD_SHIFT, 255, 0),
	SOC_SINGLE("Brownout Release", MAX98504_REG_1A_PVDD_BROWNOUT_CONFIG_4, M98504_PVDD_BROWNOUT_CFG4_RELEASE_SHIFT, 255, 0),

	SOC_SINGLE("PCM BCLK Edge", MAX98504_REG_24_PCM_MODE_CONFIG, M98504_PCM_MODE_CFG_BCLKEDGE_SHIFT, 1, 0),
	SOC_SINGLE("PCM Channel Select", MAX98504_REG_24_PCM_MODE_CONFIG, M98504_PCM_MODE_CFG_CHSEL_SHIFT, 1, 0),
	SOC_SINGLE("PCM Transmit Extra HiZ Switch", MAX98504_REG_24_PCM_MODE_CONFIG, M98504_PCM_MODE_CFG_TX_EXTRA_HIZ_SHIFT, 1, 0),
	SOC_SINGLE("PCM Output Dither Switch", MAX98504_REG_25_PCM_DSP_CONFIG, M98504_PCM_DSP_CFG_TX_DITH_EN_SHIFT, 1, 0),
	SOC_SINGLE("PCM Measurement DC Blocking Filter Switch", MAX98504_REG_25_PCM_DSP_CONFIG, M98504_PCM_DSP_CFG_MEAS_DCBLK_EN_SHIFT, 1, 0),
	SOC_SINGLE("PCM Input Dither Switch", MAX98504_REG_25_PCM_DSP_CONFIG, M98504_PCM_DSP_CFG_RX_DITH_EN_SHIFT, 1, 0),
	SOC_ENUM("PCM Output Filter Mode", max98504_pcm_rx_flt_mode_enum),
	SOC_SINGLE_EXT_TLV("PCM Rx Gain",
		MAX98504_REG_25_PCM_DSP_CONFIG, M98504_PCM_DSP_CFG_RX_GAIN_SHIFT,
		M98504_PCM_DSP_CFG_RX_GAIN_WIDTH - 1, 1, max98504_rxpcm_gain_get, max98504_rxpcm_gain_set,
		max98504_rxpcm_gain_tlv),

	SOC_SINGLE("DAC MONOMIX", MAX98504_REG_28_PCM_TO_SPEAKER_MONOMIX,
		M98504_PCM_TO_SPK_MONOMIX_CFG_SHIFT, 3, 0),

	SOC_ENUM("PCM BCLK rate", max98504_pcm_bsel_enum),

	SOC_ENUM("Speaker Current Sense Enable", max98504_ispken_enum),
	SOC_ENUM("Speaker Voltage Sense Enable", max98504_vspken_enum),

	SOC_SINGLE_EXT_TLV("AIN Gain",
		MAX98504_REG_37_ANALOGUE_INPUT_GAIN, M98504_ANALOG_INPUT_GAIN_SHIFT,
		M98504_ANALOG_INPUT_GAIN_WIDTH - 1, 1, max98504_ain_gain_get, max98504_ain_gain_set,
		max98504_ain_gain_tlv),

	SOC_SINGLE("AUTH_STATUS", MAX98504_REG_01_INTERRUPT_STATUS, 0, M98504_INT_INTERRUPT_STATUS_MASK, 0),

	SOC_SINGLE("AUTH_KEY0", MAX98504_REG_80_AUTHENTICATION_KEY_0, 0, M98504_AUTH_KEY_0_MASK, 0),
	SOC_SINGLE("AUTH_KEY1", MAX98504_REG_81_AUTHENTICATION_KEY_1, 0, M98504_AUTH_KEY_1_MASK, 0),
	SOC_SINGLE("AUTH_KEY2", MAX98504_REG_82_AUTHENTICATION_KEY_2, 0, M98504_AUTH_KEY_2_MASK, 0),
	SOC_SINGLE("AUTH_KEY3", MAX98504_REG_83_AUTHENTICATION_KEY_3, 0, M98504_AUTH_KEY_3_MASK, 0),

	SOC_SINGLE("AUTH_ENABLE", MAX98504_REG_84_AUTHENTICATION_ENABLE, 0, M98504_AUTH_EN_MASK, 0),

	SOC_SINGLE("AUTH_RESULT0", MAX98504_REG_85_AUTHENTICATION_RESULT_0, 0, M98504_AUTH_RESULT_0_MASK, 0),
	SOC_SINGLE("AUTH_RESULT1", MAX98504_REG_86_AUTHENTICATION_RESULT_1, 0, M98504_AUTH_RESULT_1_MASK, 0),
	SOC_SINGLE("AUTH_RESULT2", MAX98504_REG_87_AUTHENTICATION_RESULT_2, 0, M98504_AUTH_RESULT_2_MASK, 0),
	SOC_SINGLE("AUTH_RESULT3", MAX98504_REG_88_AUTHENTICATION_RESULT_3, 0, M98504_AUTH_RESULT_3_MASK, 0),
	SOC_SINGLE("AUTH_RESULT4", MAX98504_REG_89_AUTHENTICATION_RESULT_4, 0, M98504_AUTH_RESULT_4_MASK, 0),
	SOC_SINGLE("AUTH_RESULT5", MAX98504_REG_8A_AUTHENTICATION_RESULT_5, 0, M98504_AUTH_RESULT_5_MASK, 0),
	SOC_SINGLE("AUTH_RESULT6", MAX98504_REG_8B_AUTHENTICATION_RESULT_6, 0, M98504_AUTH_RESULT_6_MASK, 0),
	SOC_SINGLE("AUTH_RESULT7", MAX98504_REG_8C_AUTHENTICATION_RESULT_7, 0, M98504_AUTH_RESULT_7_MASK, 0),
	SOC_SINGLE("AUTH_RESULT8", MAX98504_REG_8D_AUTHENTICATION_RESULT_8, 0, M98504_AUTH_RESULT_8_MASK, 0),
	SOC_SINGLE("AUTH_RESULT9", MAX98504_REG_8E_AUTHENTICATION_RESULT_9, 0, M98504_AUTH_RESULT_9_MASK, 0),
	SOC_SINGLE("AUTH_RESULT10", MAX98504_REG_8F_AUTHENTICATION_RESULT_10, 0, M98504_AUTH_RESULT_10_MASK, 0),
	SOC_SINGLE("AUTH_RESULT11", MAX98504_REG_90_AUTHENTICATION_RESULT_11, 0, M98504_AUTH_RESULT_11_MASK, 0),
	SOC_SINGLE("AUTH_RESULT12", MAX98504_REG_91_AUTHENTICATION_RESULT_12, 0, M98504_AUTH_RESULT_12_MASK, 0),
	SOC_SINGLE("AUTH_RESULT13", MAX98504_REG_92_AUTHENTICATION_RESULT_13, 0, M98504_AUTH_RESULT_13_MASK, 0),
	SOC_SINGLE("AUTH_RESULT14", MAX98504_REG_93_AUTHENTICATION_RESULT_14, 0, M98504_AUTH_RESULT_14_MASK, 0),
	SOC_SINGLE("AUTH_RESULT15", MAX98504_REG_94_AUTHENTICATION_RESULT_15, 0, M98504_AUTH_RESULT_15_MASK, 0),
};

static const char *spk_src_mux_text[] = { "PCM", "AIN", "PDM_CH0", "PDM_CH1" };

static const struct soc_enum spk_src_mux_enum =
	SOC_ENUM_SINGLE(MAX98504_REG_35_SPEAKER_SOURCE_SELECT, M98504_SPK_SRC_SEL_SHIFT,
		ARRAY_SIZE(spk_src_mux_text), spk_src_mux_text);

static const struct snd_kcontrol_new max98504_spk_src_mux =
	SOC_DAPM_ENUM("SPK_SRC Mux", spk_src_mux_enum);

static const struct snd_soc_dapm_widget max98504_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("Voltage Data"),
	SND_SOC_DAPM_INPUT("Current Data"),
	SND_SOC_DAPM_INPUT("Analog Input"),

	SND_SOC_DAPM_ADC("ADCV", NULL, MAX98504_REG_36_MEASUREMENT_ENABLES,
		M98504_MEAS_V_EN_SHIFT, 0),
	SND_SOC_DAPM_ADC("ADCI", NULL, MAX98504_REG_36_MEASUREMENT_ENABLES,
		M98504_MEAS_I_EN_SHIFT, 0),

	SND_SOC_DAPM_AIF_OUT("AIF1OUTL", "HiFi Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1OUTR", "HiFi Capture", 1, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("AIF2OUTL", "Aux Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2OUTR", "Aux Capture", 1, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("AIF1INL", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF1INR", "HiFi Playback", 1, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("AIF2INL", "Aux Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2INR", "Aux Playback", 1, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DAC Mono", NULL, MAX98504_REG_34_SPEAKER_ENABLE,
		M98504_SPK_EN_SHIFT, 0),

	SND_SOC_DAPM_DAC("PDM SPK IN", NULL, MAX98504_REG_33_PDM_RX_ENABLE,
		M98504_PDM_RX_EN_SHIFT, 0),

	SND_SOC_DAPM_MUX("SPK_SRC Mux", SND_SOC_NOPM,
		0, 0, &max98504_spk_src_mux),

	SND_SOC_DAPM_PGA("SPK Mono Out", MAX98504_REG_34_SPEAKER_ENABLE, M98504_SPK_EN_SHIFT, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("SPKOUT"),
};

static const struct snd_soc_dapm_route max98504_audio_map[] = {
	{"ADCV", NULL, "Voltage Data"},
	{"ADCI", NULL, "Current Data"},

	{"AIF1OUTL", NULL, "ADCV"},
	{"AIF1OUTR", NULL, "ADCI"},
	{"AIF2OUTL", NULL, "ADCV"},
	{"AIF2OUTR", NULL, "ADCI"},

	{"DAC Mono", NULL, "AIF1INL"},
	{"DAC Mono", NULL, "AIF1INR"},

	{"PDM SPK IN", NULL, "AIF2INL"},
	{"PDM SPK IN", NULL, "AIF2INR"},

	{"SPK_SRC Mux", "PCM", "DAC Mono"},
	{"SPK_SRC Mux", "AIN", "Analog Input"},
	{"SPK_SRC Mux", "PDM_CH0", "PDM SPK IN"},
	{"SPK_SRC Mux", "PDM_CH1", "PDM SPK IN"},

	{"SPK Mono Out", NULL, "SPK_SRC Mux"},

	{"SPKOUT", NULL, "SPK Mono Out"},
};

static int max98504_add_widgets(struct snd_soc_codec *codec)
{
	msg_maxim("\n");

	snd_soc_add_codec_controls(codec, max98504_snd_controls,
								ARRAY_SIZE(max98504_snd_controls));

	return 0;
}


/* codec sample rate config parameter table */
static const struct {
	u32 rate;
	u8  sr;
} rate_table[] = {
	{8000,  (0)},
	{11025,	(1)},
	{12000, (2)},
	{16000, (3)},
	{22050, (4)},
	{24000, (5)},
    {32000, (6)},
	{44100, (7)},
	{48000, (8)},
};
static inline int rate_value(int rate, u8 *value)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
	       if (rate_table[i].rate >= rate) {
	               *value = rate_table[i].sr;
	               return 0;
	       }
	}
	*value = rate_table[0].sr;
	return -EINVAL;
}


/* #define TDM */
static int max98504_set_tdm_slot(struct snd_soc_dai *codec_dai, unsigned int tx_mask,
				      unsigned int rx_mask, int slots, int slot_width)
{
	return 0;
}

static int max98504_dai_set_fmt(struct snd_soc_dai *codec_dai,
				 unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98504_priv *max98504 = snd_soc_codec_get_drvdata(codec);
	struct max98504_cdata *cdata;
	u8 regval;

	msg_maxim("\n");

	cdata = &max98504->dai[0];

	if (fmt != cdata->fmt) {
		cdata->fmt = fmt;

		regval = 0;

		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
		case SND_SOC_DAIFMT_CBM_CFM:
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
		default:
			dev_err(codec->dev, "DAI clock mode unsupported");
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			snd_soc_update_bits(codec, MAX98504_REG_24_PCM_MODE_CONFIG,
				M98504_PCM_MODE_CFG_FORMAT_MASK, M98504_PCM_MODE_CFG_FORMAT_I2S_MASK);
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			snd_soc_update_bits(codec, MAX98504_REG_24_PCM_MODE_CONFIG,
				M98504_PCM_MODE_CFG_FORMAT_MASK, M98504_PCM_MODE_CFG_FORMAT_LJ_MASK);
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			snd_soc_update_bits(codec, MAX98504_REG_24_PCM_MODE_CONFIG,
				M98504_PCM_MODE_CFG_FORMAT_MASK, M98504_PCM_MODE_CFG_FORMAT_RJ_MASK);
			break;
		case SND_SOC_DAIFMT_PDM:
			snd_soc_update_bits(codec, MAX98504_REG_30_PDM_TX_ENABLES,
				M98504_PDM_EX_EN_CH0_MASK|M98504_PDM_EX_EN_CH1_MASK,
				M98504_PDM_EX_EN_CH0_MASK|M98504_PDM_EX_EN_CH1_MASK);

			snd_soc_write(codec, MAX98504_REG_26_PCM_CLOCK_SETUP, 0);
			goto out;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			/* Not supported mode */
		default:
			dev_err(codec->dev, "DAI format unsupported");
			return -EINVAL;
		}

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
		case SND_SOC_DAIFMT_NB_IF:
			break;
		case SND_SOC_DAIFMT_IB_NF:
		case SND_SOC_DAIFMT_IB_IF:
			break;
		default:
			dev_err(codec->dev, "DAI invert mode unsupported");
			return -EINVAL;
		}

		snd_soc_write(codec, MAX98504_REG_26_PCM_CLOCK_SETUP, M98094_PCM_CLK_SETUP_DAI_BSEL64);

	}
out:
	return 0;
}

static int max98504_set_bias_level(struct snd_soc_codec *codec,
				   enum snd_soc_bias_level level)
{
	int ret;
	msg_maxim("level=%d \n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			ret = snd_soc_cache_sync(codec);

			if (ret != 0) {
				dev_err(codec->dev, "Failed to sync cache: %d\n", ret);
				return ret;
			}
		}

		snd_soc_write(codec, MAX98504_REG_40_GLOBAL_ENABLE, M98504_GLOBAL_EN_MASK);

		break;

	case SND_SOC_BIAS_PREPARE:
		break;


	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_OFF:
		snd_soc_write(codec, MAX98504_REG_40_GLOBAL_ENABLE, 0);
		codec->cache_sync = 1;
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static int max98504_dai_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98504_priv *max98504 = snd_soc_codec_get_drvdata(codec);
	struct max98504_cdata *cdata;
	struct max98504_pdata *pdata = max98504->pdata;

	unsigned int rate;
	u8 regval;

	msg_maxim("\n");

	if(pdata->auth_en!=MODE_PCM)	return 0;

	cdata = &max98504->dai[0];

	rate = params_rate(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		snd_soc_update_bits(codec, MAX98504_REG_24_PCM_MODE_CONFIG,
			M98504_PCM_MODE_CFG_CH_SIZE_MASK, M98504_PCM_MODE_CFG_CH_SIZE_8_MASK);
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(codec, MAX98504_REG_24_PCM_MODE_CONFIG,
			M98504_PCM_MODE_CFG_CH_SIZE_MASK, M98504_PCM_MODE_CFG_CH_SIZE_16_MASK);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(codec, MAX98504_REG_24_PCM_MODE_CONFIG,
			M98504_PCM_MODE_CFG_CH_SIZE_MASK, M98504_PCM_MODE_CFG_CH_SIZE_24_MASK);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		snd_soc_update_bits(codec, MAX98504_REG_24_PCM_MODE_CONFIG,
			M98504_PCM_MODE_CFG_CH_SIZE_MASK, M98504_PCM_MODE_CFG_CH_SIZE_32_MASK);
		break;
	default:
		return -EINVAL;
	}

	if (rate_value(rate, &regval))
		return -EINVAL;

	/* Update sample rate mode */
	snd_soc_update_bits(codec, MAX98504_REG_27_PCM_SAMPLE_RATE_SETUP,
		M98504_PCM_SR_SETUP_SPK_SR_MASK, regval<<M98504_PCM_SR_SETUP_SPK_SR_SHIFT);

	snd_soc_update_bits(codec, MAX98504_REG_27_PCM_SAMPLE_RATE_SETUP,
		M98504_PCM_SR_SETUP_MEAS_SR_MASK, regval<<M98504_PCM_SR_SETUP_MEAS_SR_SHIFT);

	return 0;
}

/*
 * PLL / Sysclk
 */
static int max98504_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98504_priv *max98504 = snd_soc_codec_get_drvdata(codec);

	msg_maxim("clk_id;%d, freq:%d, dir:%d\n", clk_id, freq, dir);

	/* Requested clock frequency is already setup */
	if (freq == max98504->sysclk)
		return 0;

	max98504->sysclk = freq;

	return 0;
}

static int max98504_dai_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	msg_maxim("- mute:%d\n", mute);

	if(mute)	{
		#ifdef MAX98504_WATCHDOG_ENABLE
		snd_soc_write(codec, MAX98504_REG_12_WATCHDOG_ENABLE, 0);
		#endif
		snd_soc_update_bits(codec, MAX98504_REG_34_SPEAKER_ENABLE,
			M98504_SPK_EN_MASK, 0);
	}
	else	{
		#ifdef MAX98504_WATCHDOG_ENABLE
		snd_soc_write(codec, MAX98504_REG_12_WATCHDOG_ENABLE, M98504_WDOG_ENABLE_MASK);
		#endif
		snd_soc_update_bits(codec, MAX98504_REG_34_SPEAKER_ENABLE,
			M98504_SPK_EN_MASK, M98504_SPK_EN_MASK);
	}

	return 0;
}

#define MAX98504_RATES SNDRV_PCM_RATE_8000_48000
#define MAX98504_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops max98504_dai_ops = {
	.set_sysclk = max98504_dai_set_sysclk,
	.set_fmt = max98504_dai_set_fmt,
	.set_tdm_slot = max98504_set_tdm_slot,
	.hw_params = max98504_dai_hw_params,
	.digital_mute = max98504_dai_digital_mute,
};

static struct snd_soc_dai_driver max98504_dai[] = {
{
	.name = "max98504-aif1",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MAX98504_RATES,
		.formats = MAX98504_FORMATS,
	},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MAX98504_RATES,
		.formats = MAX98504_FORMATS,
	},
	 .ops = &max98504_dai_ops,
},
{
	.name = "max98504-aif2",
	.playback = {
		.stream_name = "Aux Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MAX98504_RATES,
		.formats = MAX98504_FORMATS,
	},
	.capture = {
		.stream_name = "Aux Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = MAX98504_RATES,
		.formats = MAX98504_FORMATS,
	},
	 .ops = &max98504_dai_ops,
}
};

static void max98504_handle_pdata(struct snd_soc_codec *codec)
{
	struct max98504_priv *max98504 = snd_soc_codec_get_drvdata(codec);
	struct max98504_pdata *pdata = max98504->pdata;

	u8 rx_en, tx_en, tx_hiz_en, tx_ch_src, auth_en, wdog_time_out;
	u8 regval;

	msg_maxim("\n");

	if (!pdata) {
		dev_dbg(codec->dev, "No platform data\n");
		return;
	}

	rx_en = pdata->rx_ch_en;
	tx_en = pdata->tx_ch_en;
	tx_hiz_en = pdata->tx_hiz_ch_en;
	tx_ch_src = pdata->tx_ch_src;
	auth_en = pdata->auth_en;
	wdog_time_out = pdata->wdog_time_out;

	if(pdata->rx_mode == MODE_PCM)	{
		/* filter */
		if(pdata->tx_dither_en)	regval = M98504_PCM_DSP_CFG_TX_DITH_EN_MASK;
		if(pdata->meas_dc_block_en)	regval |= M98504_PCM_DSP_CFG_MEAS_DCBLK_EN_MASK;
		if(pdata->rx_dither_en)	regval |= M98504_PCM_DSP_CFG_RX_DITH_EN_MASK;
		if(pdata->rx_flt_mode)	regval |= M98504_PCM_DSP_CFG_RX_FLT_MODE_MASK;

		snd_soc_update_bits(codec, MAX98504_REG_25_PCM_DSP_CONFIG,
			M98504_PCM_DSP_CFG_FLT_MASK, regval);

		snd_soc_write(codec, MAX98504_REG_20_PCM_RX_ENABLES, rx_en);
		snd_soc_write(codec, MAX98504_REG_21_PCM_TX_ENABLES, tx_en);
		snd_soc_write(codec, MAX98504_REG_22_PCM_TX_HIZ_CONTROL, tx_hiz_en);
		snd_soc_write(codec, MAX98504_REG_23_PCM_TX_CHANNEL_SOURCES, tx_ch_src);

	}
	else if(pdata->rx_mode==MODE_PDM)	{
		snd_soc_write(codec, MAX98504_REG_31_PDM_TX_HIZ_CONTROL, tx_hiz_en);
		snd_soc_write(codec, MAX98504_REG_32_PDM_TX_CONTROL, tx_ch_src);
	}

	snd_soc_write(codec, MAX98504_REG_84_AUTHENTICATION_ENABLE, auth_en & M98504_AUTH_EN_MASK);
	snd_soc_write(codec, MAX98504_REG_13_WATCHDOG_CONFIG, wdog_time_out);
}

#ifdef CONFIG_PM
static int max98504_suspend(struct snd_soc_codec *codec)
{
	msg_maxim("\n");

	return 0;
}

static int max98504_resume(struct snd_soc_codec *codec)
{
	msg_maxim("\n");
	return 0;
}
#else
#define max98504_suspend NULL
#define max98504_resume NULL
#endif

#ifdef MAX98504_WATCHDOG_ENABLE
static irqreturn_t max98504_interrupt(int irq, void *data)
{
	struct snd_soc_codec *codec = (struct snd_soc_codec *) data;
	struct max98504_priv *max98504 = snd_soc_codec_get_drvdata(codec);

	unsigned int mask;
	unsigned int flag;

	mask = snd_soc_read(codec, MAX98504_REG_03_INTERRUPT_ENABLES);
	flag = snd_soc_read(codec, MAX98504_REG_02_INTERRUPT_FLAGS);

	msg_maxim("flag=0x%02x mask=0x%02x -> flag=0x%02x\n",
		flag, mask, flag & mask);

	flag &= mask;

	if (!flag)
		return IRQ_NONE;

	/* Send work to be scheduled */
	if (flag & M98504_INT_GENFAIL_EN_MASK) {
		msg_maxim("M98504_INT_GENFAIL_EN_MASK active!");
	}

	if (flag & M98504_INT_AUTHDONE_EN_MASK) {
		msg_maxim("M98504_INT_AUTHDONE_EN_MASK active!");
	}

	if (flag & M98504_INT_VBATBROWN_EN_MASK) {
		msg_maxim("M98504_INT_VBATBROWN_EN_MASK active!");
	}

	if (flag & M98504_INT_WATCHFAIL_EN_MASK) {
		msg_maxim("M98504_INT_WATCHFAIL_EN_MASK active!");
		schedule_delayed_work(&max98504->work, msecs_to_jiffies(2000));
	}

	if (flag & M98504_INT_THERMWARN_END_EN_MASK) {
		msg_maxim("M98504_INT_THERMWARN_END_EN_MASK active!");
	}

	if (flag & M98504_INT_THERMWARN_BGN_EN_MASK) {
		msg_maxim("M98504_INT_THERMWARN_BGN_EN_MASK active!\n");
	}
	if (flag & M98504_INT_THERMSHDN_END_EN_MASK) {
		msg_maxim("M98504_INT_THERMSHDN_END_EN_MASK active!\n");
	}
	if (flag & M98504_INT_THERMSHDN_BGN_FLAG_MASK) {
		msg_maxim("M98504_INT_THERMSHDN_BGN_FLAG_MASK active!\n");
	}
	snd_soc_write(codec, MAX98504_REG_04_INTERRUPT_FLAG_CLEARS, flag&0xff);

	return IRQ_HANDLED;
}
#endif

#ifdef MAX98504_WATCHDOG_ENABLE
static void max98504_work(struct work_struct *work)
{
	struct max98504_priv *max98504 = container_of(work, struct max98504_priv, work.work);
	struct snd_soc_codec *codec= max98504->codec;

	if(codec->dapm.bias_level==SND_SOC_BIAS_ON)	{
		snd_soc_write(codec, MAX98504_REG_14_WATCHDOG_CLEAR, 0xE9);
		snd_soc_write(codec, MAX98504_REG_40_GLOBAL_ENABLE, M98504_GLOBAL_EN_MASK);
		msg_maxim("Watchdog Recovery\n");
	}
	else	msg_maxim("No Watchdog Recovery.\n");
}
#endif

static int max98504_probe(struct snd_soc_codec *codec)
{
	struct max98504_priv *max98504 = snd_soc_codec_get_drvdata(codec);
	struct max98504_cdata *cdata;
	int ret = 0;

	msg_maxim("\n");

	max98504->codec = codec;

	codec->cache_sync = 1;

	ret = snd_soc_codec_set_cache_io(codec, 16, 8, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/* reset the codec, the DSP core, and disable all interrupts */
	ret = max98504_reset(codec);
	if (ret < 0) {
		goto err_access;
	}

	/* initialize private data */

	max98504->sysclk = (unsigned)-1;

	cdata = &max98504->dai[0];
	cdata->rate = (unsigned)-1;
	cdata->fmt  = (unsigned)-1;

	ret = snd_soc_read(codec, MAX98504_REG_7FFF_REV_ID);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read device revision: %d\n",
			ret);
		goto err_access;
	}
	msg_maxim("REV ID=0x%x\n", ret);

#ifdef MAX98504_WATCHDOG_ENABLE
	snd_soc_write(codec, MAX98504_REG_03_INTERRUPT_ENABLES, M98504_INT_WATCHFAIL_EN_MASK);
	snd_soc_write(codec, MAX98504_REG_10_GPIO_ENABLE, M98504_GPIO_ENABLE_MASK);
	snd_soc_write(codec, MAX98504_REG_04_INTERRUPT_FLAG_CLEARS, 0xFF);

	if ( (request_threaded_irq(pdata->irq, NULL,
		max98504_interrupt, IRQF_TRIGGER_FALLING,
		"max98504_interrupt", codec)) < 0) {
		msg_maxim("request_irq failed\n");
	}
#endif

	max98504_handle_pdata(codec);

	max98504_add_widgets(codec);

#ifdef MAX98504_WATCHDOG_ENABLE
	INIT_DELAYED_WORK_DEFERRABLE(&max98504->work, max98504_work);
#endif

	snd_soc_write(codec, MAX98504_REG_40_GLOBAL_ENABLE, 1);
	snd_soc_write(codec, MAX98504_REG_35_SPEAKER_SOURCE_SELECT, 1);
	snd_soc_write(codec, MAX98504_REG_34_SPEAKER_ENABLE, 1);
	msg_maxim("done.");

err_access:
	return ret;
}

static int max98504_remove(struct snd_soc_codec *codec)
{
	msg_maxim("\n");

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_max98504 = {
	.probe   = max98504_probe,
	.remove  = max98504_remove,
	.suspend = max98504_suspend,
	.resume  = max98504_resume,
	.set_bias_level = max98504_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(max98504_reg_def),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = max98504_reg_def,
	.readable_register = max98504_readable,
	.volatile_register = max98504_volatile_register,
	.dapm_widgets	  = max98504_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max98504_dapm_widgets),
	.dapm_routes     = max98504_audio_map,
	.num_dapm_routes = ARRAY_SIZE(max98504_audio_map),
};

#ifdef SUPPORT_DEVICE_TREE
#if 0
static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
			regulator_set_optimum_mode(reg, load_uA) : 0;
}
static int max98504_regulator_config(struct i2c_client *i2c, bool pullup, bool on)
{
	struct regulator *max98504_vcc_i2c;
	int rc;
    #define VCC_I2C_MIN_UV	1800000
    #define VCC_I2C_MAX_UV	1800000
	#define I2C_LOAD_UA		300000

	if (pullup) {
		max98504_vcc_i2c = regulator_get(&i2c->dev, "vcc_i2c");

		if (IS_ERR(max98504_vcc_i2c)) {
			rc = PTR_ERR(max98504_vcc_i2c);
			pr_info("Regulator get failed rc=%d\n",	rc);
			goto error_get_vtg_i2c;
		}

		if (regulator_count_voltages(max98504_vcc_i2c) > 0) {
			rc = regulator_set_voltage(max98504_vcc_i2c, VCC_I2C_MIN_UV, VCC_I2C_MAX_UV);
			if (rc) {
				pr_info("regulator set_vtg failed rc=%d\n", rc);
				goto error_set_vtg_i2c;
			}
		}

		rc = reg_set_optimum_mode_check(max98504_vcc_i2c, I2C_LOAD_UA);
		if (rc < 0) {
			pr_info("Regulator vcc_i2c set_opt failed rc=%d\n", rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(max98504_vcc_i2c);
		if (rc) {
			pr_info("Regulator vcc_i2c enable failed rc=%d\n", rc);
			goto error_reg_en_vcc_i2c;
		}
	}

	return 0;

	error_set_vtg_i2c:
		regulator_put(max98504_vcc_i2c);
	error_get_vtg_i2c:
		if (regulator_count_voltages(max98504_vcc_i2c) > 0)
			regulator_set_voltage(max98504_vcc_i2c, 0, VCC_I2C_MAX_UV);
	error_reg_en_vcc_i2c:
		if(pullup)reg_set_optimum_mode_check(max98504_vcc_i2c, 0);
	error_reg_opt_i2c:
		regulator_disable(max98504_vcc_i2c);

	return rc;
}
#endif
#endif

static int max98504_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct max98504_priv *max98504;
	int ret;
#ifdef SUPPORT_DEVICE_TREE
	u32 read_val;
	struct max98504_pdata *pdata;
#endif

	msg_maxim("\n");

	max98504 = kzalloc(sizeof(struct max98504_priv), GFP_KERNEL);
	if (max98504 == NULL)
		return -ENOMEM;

	max98504->devtype = id->driver_data;
	i2c_set_clientdata(i2c, max98504);
	max98504->control_data = i2c;
#ifdef SUPPORT_DEVICE_TREE
	if (i2c->dev.of_node) {
		max98504->pdata = devm_kzalloc(&i2c->dev,
			sizeof(struct max98504_pdata), GFP_KERNEL);
		if (!max98504->pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		else pdata=max98504->pdata;

		/* Will change this to array.*/
		ret = of_property_read_u32(i2c->dev.of_node, "max98505,rx_mode", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->rx_mode = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,tx_dither_en", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->tx_dither_en = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,rx_dither_en", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->rx_dither_en = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,meas_dc_block_en", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->meas_dc_block_en = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,rx_flt_mode", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->rx_flt_mode = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,rx_ch_en", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->rx_ch_en = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,tx_ch_en", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->tx_ch_en = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,tx_hiz_ch_en", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->tx_hiz_ch_en = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,tx_ch_src", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->tx_ch_src = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,auth_en", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->auth_en = read_val;

		ret = of_property_read_u32(i2c->dev.of_node, "max98505,wdog_time_out", &read_val);
		if (ret) {
			dev_err(&i2c->dev, "Failed to read rx_mode.\n");
			return -EINVAL;
		}
		else pdata->wdog_time_out = read_val;
	}
	else		max98504->pdata = i2c->dev.platform_data;
//	max98504_regulator_config(i2c, of_property_read_bool(i2c->dev.of_node, "max98504,i2c-pull-up"), 1);
#else
	max98504->pdata = i2c->dev.platform_data;
#endif
	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_max98504, max98504_dai, ARRAY_SIZE(max98504_dai));

	max98504_write(i2c, MAX98504_REG_40_GLOBAL_ENABLE, 1);
	max98504_write(i2c, MAX98504_REG_35_SPEAKER_SOURCE_SELECT, 1);
	max98504_write(i2c, MAX98504_REG_34_SPEAKER_ENABLE, 1);
	msg_maxim("ret=%d\n", ret);

	if (ret < 0)
		kfree(max98504);
	return ret;
}

static int __devexit max98504_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	msg_maxim("\n");

	return 0;
}

static const struct i2c_device_id max98504_i2c_id[] = {
	{ "max98504", MAX98504 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max98504_i2c_id);

static struct i2c_driver max98504_i2c_driver = {
	.driver = {
		.name = "max98504",
		.owner = THIS_MODULE,
	},
	.probe  = max98504_i2c_probe,
	.remove = __devexit_p(max98504_i2c_remove),
	.id_table = max98504_i2c_id,
};

static int __init max98504_init(void)
{
	int ret;

	msg_maxim("%s\n", __func__);

	ret = i2c_add_driver(&max98504_i2c_driver);
	if (ret)
		pr_err("Failed to register MAX98504 I2C driver: %d\n", ret);
	else
		pr_info("MAX98504 driver built on %s at %s\n",
			__DATE__,
			__TIME__);

	return ret;
}

module_init(max98504_init);

static void __exit max98504_exit(void)
{
	i2c_del_driver(&max98504_i2c_driver);
}
module_exit(max98504_exit);

MODULE_DESCRIPTION("ALSA SoC MAX98504 driver");
MODULE_AUTHOR("Ryan Lee");
MODULE_LICENSE("GPL");
