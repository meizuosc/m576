/*
 * es9018k2m.c  --  ES9018K2M ALSA SoC Audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on wm8753.c by Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#endif
#include "es9018k2m.h"
#include <mach/hardware.h>
#include <linux/firmware.h>

#include <linux/cpufreq.h>
#include <mach/cpufreq.h>
#include <linux/pm_qos.h>

#define MUIC_DEVICE_QOS_FREQ		( 200000)
static struct pm_qos_request muic_device_qos;


#define MUTE_VER2 5
#define FIR_ENA
#define OP_EN

/*
 * es9018k2m register cache
 */
static const struct reg_default es9018k2m_reg_defaults[] = {
	{ 0, 0x00 },
	{ 1, 0x8c },
	{ 4, 0x00 },
	{ 5, 0x68 },
	{ 6, 0x42 },
	{ 7, 0x80 },
	{ 8, 0x10 },
	{ 9, 0x00 },
	{ 10,0x05 },
	{ 11,0x02 },
	{ 12,0x5a },
	{ 13,0x40 },
	{ 14,0x8a },
	{ 15,0x00 },
	{ 16,0x00 },
	{ 17,0xff },
	{ 18,0xff },
	{ 19,0xff },
	{ 20,0x7f },
	{ 21,0x00 },
	{ 26,0x00 },
	{ 27,0x00 },
	{ 28,0x00 },
	{ 29,0x00 },
	{ 30,0x00 },
	
};

#ifdef FIR_ENA
/* FIR COEFF DATA  */
static  int es9018k2m_fir_coeff_stage1[128] = {
	20,50,-32,-136,-2,314,114,-576,-416,933,1017,-1315,-2089,1602,3795,-1551,-6301,806,9699,1140,-13962,-4923,
	18859,11289,-23887,-21023,28185,34863,-30489,-53363,29093,76748,-21865,-104733,6296,136353,20406,-169789,
	-61181,202210,118977,-229618,-196622,246669,296758,-246377,-421910,219531,574791,-153378,-759012,28556,980458,
	188612,-1249634,-567680,1583619,1285814,-1985753,-2936870,2081023,8388607,8388607,2081023,-2936870,-1985753,
	1285814,1583619,-567680,-1249634,188612,980458,28556,-759012,-153378,574791,219531,-421910,-246377,296758,
	246669,-196622,-229618,118977,202210,-61181,-169789,20406,136353,6296,-104733,-21865,76748,29093,-53363,-30489,
	34863,28185,-21023,-23887,11289,18859,-4923,-13962,1140,9699,806,-6301,-1551,3795,1602,-2089,-1315,1017,933,
	-416,-576,114,314,-2,-136,-32,50,20,0,0,0,0
};

static  int es9018k2m_fir_coeff_stage2[16] = {
	548,3848,15950,49995,128081,280835,541717,934837,1461225,2086382,2737232,3314135,3713180,3855803,0,0
};

static int es9018k2m_get_FIR_coeff(struct snd_soc_codec *codec, char * path,int * coeff, int size)
{

	const struct firmware *fw_stage = NULL;
	const u8 *p;
	int *fir_data;
	int i = 0;
	int ret = 0;

	if(!path){
		pr_err("ess9018: No such file or directory\n");
		return -ENOENT;
	}

	ret = request_firmware(&fw_stage, path, codec->dev);
	if (ret) {
		pr_err("ess9018: Failed to request FIR data! ret=%d\n",ret);
		return ret;
	}
	pr_info("ess9018 custom FIR data size	= %ld\n",fw_stage->size);

	p = fw_stage->data;
	fir_data = coeff;

	do {
		sscanf(p,"%d",fir_data);
		p = strchr(p,'\n');
		if ( p != NULL) {
			p++;
			fir_data++;
		} else {

		}
	} while( p < (fw_stage->data + fw_stage->size));

	for (i = 0;i < size;i++)
	pr_debug("ess9018: %d :%d\n",i,coeff[i]);

	release_firmware(fw_stage);

	return ret;
}

static int es9018k2m_set_FIR_coeff(struct snd_soc_codec *codec, const int * coeff, int size)
{
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	int i;
	int address;

	if(es9018k2m->version == 0)
		return 0;
	if(size == 128 || size == 64)
		address = 0x0;
	else if(size == 16)
		address = 0x80;
	else {
		printk("%s:error FIR coeff,size=%d\n",__func__,size);
		return -EIO;
	}

	for(i = 0;i < size;i++) {
		snd_soc_write(codec, 26,address+i);
		snd_soc_write(codec, 27,coeff[i] & 0xff);
		snd_soc_write(codec, 28,(coeff[i] >> 8) & 0xff);
		snd_soc_write(codec, 29,(coeff[i] >> 16) & 0xff);
		snd_soc_write(codec, 30, 0x02);
	}

	snd_soc_write(codec, 30, 0x00);

	return 0;
}
#endif

static unsigned int es9018k2m_read(struct snd_soc_codec *codec, unsigned int reg)
{
	int ret = 0;
	struct regmap *map = es9018k2m_priv->regmap;
	unsigned int  val;

	mutex_lock(&es9018k2m_priv->access_mutex);
	if(es9018k2m_priv->active) {
		ret = regmap_read(map, reg, &val);
		if(ret == 0) {
			ret = val;
		}
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&es9018k2m_priv->access_mutex);

	return ret;
}

static int es9018k2m_write(struct snd_soc_codec *codec, unsigned int reg,
		    unsigned int value)
{
	int ret;
	struct regmap *map = es9018k2m_priv->regmap;

	mutex_lock(&es9018k2m_priv->access_mutex);
	if(es9018k2m_priv->active)
		ret = regmap_write(map, reg, value);
	else
		ret = -EINVAL;
	mutex_unlock(&es9018k2m_priv->access_mutex);

	return ret;
}

/**from V0 to V6:  gpio_mute:L(MUTE),gpio_mute:H(UNMUTE)
             V7:gpio_mute:H(MUTE),gpio_mute:L(UNMUTE)
             enable : 1, mute
             enable :0, unmute**/
static void es9018k2m_set_mute(struct snd_soc_codec *codec, int enable)
{
	//struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	bool value = !!enable;

	if(meizu_board_version() >= MUTE_VER2)
		value = enable;
	else
		value = !enable;

	gpio_set_value(pdata->gpio_mute, value);

	return;
}
/**from V0 to V6:  gpio_out_select:L(ES9018),gpio_out_select:H(WM8998)
             V7:gpio_out_select:H(ES9018),gpio_out_select:L(WM8998)
             enable:1,ES9018
             enable:0,wm8998**/

static void es9018k2m_select_output(struct snd_soc_codec *codec, int enable)
{
	//struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	bool value ;

	if(meizu_board_version() >= MUTE_VER2)
		value = enable;
	else
		value = !enable;

	gpio_set_value(pdata->gpio_out_select, value);
	return;
}

static int es9018k2m_set_gain(struct snd_soc_codec *codec, int value)
{
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	int ret = 0;

	printk("%s: pre gain=%d,curr gain=%d\n",__func__,es9018k2m->gain,value);
	mutex_lock(&es9018k2m->gain_mutex);
	if(value ==  es9018k2m->gain) {
		goto err;
	}

	switch (value) {
	case ES9018K2M_GAIN_IDLE:
		gpio_set_value(pdata->gpio_mute, 1);
		gpio_set_value(pdata->gpio_out_select, 0);
		break;
	case ES9018K2M_GAIN_CODEC:
		es9018k2m_set_mute(codec,ES9018K2M_MUTE);
		es9018k2m_select_output(codec,ES9018K2M_OUTPUT_WM8998);
		msleep(5);
		es9018k2m_set_mute(codec,ES9018K2M_UNMUTE);
		break;
	case ES9018K2M_GAIN_LOW:
		//es9018k2m_set_mute(codec,ES9018K2M_MUTE);
		snd_soc_update_bits(codec, ES9018K2M_GENERAL_SET, 0x03, 0x03);//ES9018 mute
		es9018k2m_select_output(codec,ES9018K2M_OUTPUT_ES9018);
		gpio_set_value(pdata->gpio_low_gain, 1);

		snd_soc_update_bits(codec, ES9018K2M_GPIO_CONFIG, 0x0f, 0x07);//GPIO1 LOW

		es9018k2m_set_mute(codec,ES9018K2M_UNMUTE);
		snd_soc_update_bits(codec, ES9018K2M_GENERAL_SET, 0x03, 0x00);//ES9018 unmute

		break;
	case ES9018K2M_GAIN_HIGH:
		//es9018k2m_set_mute(codec,ES9018K2M_MUTE);
		snd_soc_update_bits(codec, ES9018K2M_GENERAL_SET, 0x03, 0x03);//ES9018 mute

		es9018k2m_select_output(codec,ES9018K2M_OUTPUT_ES9018);

		snd_soc_update_bits(codec, ES9018K2M_GPIO_CONFIG, 0x0f, 0x0f);//GPIO1 high

		gpio_set_value(pdata->gpio_low_gain, 0);
			
		msleep(5);
		snd_soc_update_bits(codec, ES9018K2M_GENERAL_SET, 0x03, 0x00);//ES9018 unmute
		es9018k2m_set_mute(codec,ES9018K2M_UNMUTE);

		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	es9018k2m->gain = value;
	printk("%s: gain=%d\n",__func__,es9018k2m->gain);
err:
	mutex_unlock(&es9018k2m->gain_mutex);
	return ret;
}


static int es9018k2m_poweron(struct snd_soc_codec *codec, int enable)
{
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	int ret = 0;

	printk("es9018k2m %s:%d %s\n", __func__, __LINE__, enable > 0 ? "on" :"off");
	printk("es9018k2m %s:%d board_version=%d\n", __func__, __LINE__, meizu_board_version());

	if(enable) {

#ifdef OP_EN
		cancel_delayed_work_sync(&es9018k2m->op_en_work);
#endif

		es9018k2m_set_mute(codec,ES9018K2M_MUTE);

		/*enable the VCCA AVCC_L and AVCC_R for es9018 : 1.8v*/
		gpio_set_value(pdata->gpio_low_gain, 1);

		ret = regulator_enable(es9018k2m->vdd18_dac);
		if (ret != 0) {
			pr_err("%s : fail to enable vdd18_dav\n",__func__);
			return ret;
		}
		gpio_set_value(pdata->gpio_dvdd_on, 1);

		printk("es9018k2m %s :%d on \n", __func__, __LINE__);
		/*Enable the voltage for opa1612*/
		gpio_set_value(pdata->gpio_op_en, 1);

		usleep_range(1000, 1500);
	} else {

		printk("es9018k2m %s:%d off \n",__func__,__LINE__);

		es9018k2m_set_mute(codec,ES9018K2M_MUTE);
		
		/* set soft_start low while in standby mode */
		if(pdata->gpio_osc_44khz != 0 || pdata->gpio_osc_48khz != 0)
			snd_soc_update_bits(codec, ES9018K2M_SOFT_START, 0x80, 0x00);
		/* Disable RESETB */
		gpio_set_value(pdata->gpio_resetb, 0);
		/*gate the osc output */
		gpio_set_value(pdata->gpio_osc_44khz, 0);
		gpio_set_value(pdata->gpio_osc_48khz, 0);

		mutex_lock(&es9018k2m->access_mutex);
		es9018k2m->active = 0;
		mutex_unlock(&es9018k2m->access_mutex);

		/*disable the voltage for AVCC*/
		gpio_set_value(pdata->gpio_low_gain, 0);

		if (regulator_is_enabled(es9018k2m->vdd18_dac))
			regulator_disable(es9018k2m->vdd18_dac);

		gpio_set_value(pdata->gpio_dvdd_on, 0);

		mutex_lock(&es9018k2m->gain_mutex);
		if (es9018k2m->gain != ES9018K2M_GAIN_IDLE) {
			es9018k2m_select_output(codec,ES9018K2M_OUTPUT_WM8998);
			es9018k2m->gain = ES9018K2M_GAIN_CODEC;
		}

		mutex_unlock(&es9018k2m->gain_mutex);


#ifdef OP_EN
		schedule_delayed_work(&es9018k2m->op_en_work,
				      msecs_to_jiffies(200));
#else
		gpio_set_value(pdata->gpio_op_en, 0);
		msleep(300);
		if (es9018k2m->gain == ES9018K2M_GAIN_IDLE) {
			gpio_set_value(pdata->gpio_mute, 1);
			gpio_set_value(pdata->gpio_out_select, 0);
		} else {
			es9018k2m_set_mute(codec,ES9018K2M_UNMUTE);
		}
#endif
		regcache_mark_dirty(es9018k2m->regmap);
	}

	return 0;
}

struct es9018k2m_priv *es9018k2m_priv = NULL;

static BLOCKING_NOTIFIER_HEAD(es9018k2m_hp_notifier_list);

static int es9018k2m_headphone_detect(struct notifier_block *self,
							unsigned long action, void *data)
{
	if(action == 1) {
		//selection wm8998 hp;
		printk("es9018 plugin out headphone %s:\n",__func__);
		es9018k2m_set_gain(es9018k2m_priv->codec,ES9018K2M_GAIN_IDLE);
		return NOTIFY_OK;
	} else if(action == 0) {
		printk("es9018 plugin in headphone %s:\n",__func__);
		es9018k2m_set_gain(es9018k2m_priv->codec,ES9018K2M_GAIN_CODEC);
		return NOTIFY_OK;
	} else {
		return NOTIFY_DONE;
	}
}


static struct notifier_block es9018k2m_headphone_cb = {
	.notifier_call = es9018k2m_headphone_detect,
};

void es9018k2m_register_notify(struct blocking_notifier_head *list,
		struct notifier_block *nb)
{
	blocking_notifier_chain_register(list, nb);
}

void es9018k2m_unregister_notify(struct blocking_notifier_head *list,
		struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(list,nb);
}

void es9018k2m_blocking_notifier_call_chain(unsigned long val)
{
	blocking_notifier_call_chain(es9018k2m_priv->es9018k2m_notifier_list,
				val, es9018k2m_priv);
}

EXPORT_SYMBOL_GPL(es9018k2m_blocking_notifier_call_chain);

static int es9018k2m_deemph[] = { 32000, 44100, 48000 };

static int es9018k2m_set_deemph(struct snd_soc_codec *codec)
{
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	int val, i, best;

	/* If we're using deemphasis select the nearest available sample
	 * rate.
	 */
	if (es9018k2m->deemph) {
		best = 1;
		for (i = 2; i < ARRAY_SIZE(es9018k2m_deemph); i++) {
			if (abs(es9018k2m_deemph[i] - es9018k2m->playback_fs) <
			    abs(es9018k2m_deemph[best] - es9018k2m->playback_fs))
				best = i;
		}

		val = (best << 4) & 0xbf;
	} else {
		best = 0;
		val = 0;
		val |= 0x40;
		
	}

	dev_dbg(codec->dev, "Set deemphasis %d (%dHz),val=0x%x\n",
		best, es9018k2m_deemph[best],val);

	return snd_soc_update_bits(codec, ES9018K2M_DEEMPHASIS, 0x70, val);
}

static int es9018k2m_get_deemph(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = es9018k2m->deemph;

	return 0;
}

static int es9018k2m_put_deemph(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	int deemph = ucontrol->value.enumerated.item[0];
	int ret = 0;

	if (deemph > 1)
		return -EINVAL;

	mutex_lock(&codec->mutex);
	if (es9018k2m->deemph != deemph) {
		es9018k2m->deemph = deemph;

		es9018k2m_set_deemph(codec);

		ret = 1;
	}
	mutex_unlock(&codec->mutex);

	return ret;
}

static const DECLARE_TLV_DB_SCALE(out_tlv, -12750, 50, 1);

static const char *DPLL_BW_txt[] = {
	"No Bandwidth0", "Lowest Bandwidth0","Low Bandwidth0","Med-low Bandwidth0",
	"Medium Bandwidth0","Med-High Bandwidth0","High Bandwidth0","Hihgest Bandwidth0",
	"No Bandwidth1", "Lowest Bandwidth1","Low Bandwidth1","Med-low Bandwidth1",
	"Medium Bandwidth1","Med-High Bandwidth1","High Bandwidth1","Hihgest Bandwidth1",
	};
static const struct soc_enum DPLL_BW =
	SOC_ENUM_SINGLE(ES9018K2M_DPLL, 4, 15, DPLL_BW_txt);

static const char *filter_shape_txt[] = {
	"fast rolloff", "slow rolloff","minimum phase","reserved",
	};
static const struct soc_enum  filter_shape =
	SOC_ENUM_SINGLE(ES9018K2M_GENERAL_SET, 5, 3, filter_shape_txt);

static const char *iir_bandwidth_txt[] = {
	"47.44khz", "50khz","60khz","70khz",
	};
static const struct soc_enum  iir_bandwidth =
	SOC_ENUM_SINGLE(ES9018K2M_GENERAL_SET, 2, 4, iir_bandwidth_txt);


static const char * const es9018k2m_gain_texts[] = {
	"codec hp", "Hifi low gain", "Hifi high gain","hifi idle",
};
static const struct soc_enum es9018k2m_gain_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(es9018k2m_gain_texts),
			es9018k2m_gain_texts);

static int es9018k2m_get_gain_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.enumerated.item[0] = es9018k2m->gain;

	return 0;
}

static int es9018k2m_put_gain_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int value;
	int ret = 0;

	value = ucontrol->value.enumerated.item[0];
	es9018k2m_set_gain(codec, value);
	return ret;
}

static const char * const es9018k2m_mute_texts[] = {
	"unmute", "mute",
};
static const struct soc_enum es9018k2m_mute_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(es9018k2m_mute_texts),
			es9018k2m_mute_texts);

static int es9018k2m_get_mute_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	
	mutex_lock(&codec->mutex);
	//es9018k2m->mute = !(gpio_get_value(pdata->gpio_mute));
	if(meizu_board_version() >= MUTE_VER2)
		es9018k2m->mute = (gpio_get_value(pdata->gpio_mute));
	else
		es9018k2m->mute = !(gpio_get_value(pdata->gpio_mute));

	mutex_unlock(&codec->mutex);
	ucontrol->value.enumerated.item[0] = es9018k2m->mute;

	return 0;
}

static int es9018k2m_put_mute_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	bool value;
	int ret = 0;

	value = !!(ucontrol->value.enumerated.item[0]);
	mutex_lock(&codec->mutex);
	if(meizu_board_version() < MUTE_VER2)
		value = !value;

	gpio_set_value(pdata->gpio_mute,value);
	es9018k2m->mute = value;
	mutex_unlock(&codec->mutex);
	return ret;
}

static const char * const es9018k2m_out_select_texts[] = {
	"wm8998", "es9018",
};
static const struct soc_enum es9018k2m_out_select_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(es9018k2m_out_select_texts),
			es9018k2m_out_select_texts);
static int es9018k2m_get_out_select_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	int val;
	val = gpio_get_value(pdata->gpio_out_select);

	if(meizu_board_version() < MUTE_VER2)
		val = !val;

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}
static int es9018k2m_put_out_select_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	unsigned int value;
	int ret = 0;
	value = ucontrol->value.enumerated.item[0];
	if(meizu_board_version() < MUTE_VER2)
		value = !value;
	gpio_set_value(pdata->gpio_out_select,!!value);
	return ret;
}

static const char * const es9018k2m_op_en_texts[] = {
	"Off", "On",
};
static const struct soc_enum es9018k2m_op_en_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(es9018k2m_op_en_texts),
			es9018k2m_op_en_texts);
static int es9018k2m_get_op_en_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	int val;
	val = gpio_get_value(pdata->gpio_op_en);
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}
static int es9018k2m_put_op_en_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	unsigned int value;
	int ret = 0;
	value = ucontrol->value.enumerated.item[0];
	gpio_set_value(pdata->gpio_op_en,!!value);
	return ret;
}


#ifdef FIR_ENA

static const char * const es9018k2m_custom_fir_texts[] = {
	"Disable", "Enable",
};
static const struct soc_enum es9018k2m_custom_fir_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(es9018k2m_custom_fir_texts),
			es9018k2m_custom_fir_texts);

static int es9018k2m_get_custom_fir_enum(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.enumerated.item[0] = es9018k2m->custom_fir_enable;

	return 0;
}

static int es9018k2m_put_custom_fir_enum(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	unsigned int value;
	int ret = 0;
	int *data;

	value = ucontrol->value.enumerated.item[0];

	if (value) {
		ret  = es9018k2m_get_FIR_coeff(codec,"stage1.txt",es9018k2m->stage1,128);
		if (ret)
			data = es9018k2m_fir_coeff_stage1;
		else
			data = es9018k2m->stage1;

		es9018k2m_set_FIR_coeff(codec,data,128);
		ret  = es9018k2m_get_FIR_coeff(codec,"stage2.txt",es9018k2m->stage2,16);
		if (ret)
			data = es9018k2m_fir_coeff_stage2;
		else
			data = es9018k2m->stage2;

		es9018k2m_set_FIR_coeff(codec,data,16);
		//snd_soc_write(codec,30,0x1);//sin
		snd_soc_write(codec,30,0x5);//cos
	} else {
		snd_soc_write(codec,30,0x0);
	}

	es9018k2m->custom_fir_enable = value;

	return ret;
}

static const char *stage2_filter_txt[] = {
	"sine", "cosine",
	};
static const struct soc_enum  stage2_filter =
	SOC_ENUM_SINGLE(30, 2, 2, stage2_filter_txt);

#endif

static const struct snd_kcontrol_new es9018k2m_snd_controls[] = {

SOC_DOUBLE_R_TLV("Master Playback Volume", ES9018K2M_VOLUME1, ES9018K2M_VOLUME2,
		 0, 255, 0, out_tlv),
SOC_ENUM("Filter Shape", filter_shape),
SOC_ENUM("IIR bandwidth", iir_bandwidth),
SOC_SINGLE("Playback Left mute", ES9018K2M_GENERAL_SET, 0, 1, 0),
SOC_SINGLE("Playback Right mute", ES9018K2M_GENERAL_SET, 1, 1, 0),
SOC_SINGLE("bypass IIR", ES9018K2M_INPUT_SELECT, 2, 1, 0),
SOC_SINGLE("Bypass FIR", ES9018K2M_INPUT_SELECT, 0, 1, 0),
SOC_ENUM("DPLL Bandwidth", DPLL_BW),

SOC_SINGLE("THD Compensation", ES9018K2M_THD_COMPENSATION, 6, 1, 1),
SOC_SINGLE("2nd Harmonic Compensation", ES9018K2M_2_HARMONIC_COMPENSATION_1, 0, 255, 0),
SOC_SINGLE("3nd Harmonic Compensation", ES9018K2M_3_HARMONIC_COMPENSATION_1, 0, 255, 0),

SOC_SINGLE_BOOL_EXT("Playback Deemphasis Switch", 0,
		    es9018k2m_get_deemph, es9018k2m_put_deemph),
SOC_ENUM_EXT("Gain selection",
				 es9018k2m_gain_enum,
				 es9018k2m_get_gain_enum, es9018k2m_put_gain_enum),
SOC_ENUM_EXT("Playback mute",
				 es9018k2m_mute_enum,
				 es9018k2m_get_mute_enum, es9018k2m_put_mute_enum), 
#ifdef FIR_ENA
SOC_ENUM_EXT("custom FIR enable",
				 es9018k2m_custom_fir_enum,
				 es9018k2m_get_custom_fir_enum, es9018k2m_put_custom_fir_enum),
SOC_ENUM("stage2 filter type", stage2_filter),
#endif
SOC_ENUM_EXT("Out select",
				 es9018k2m_out_select_enum,
				 es9018k2m_get_out_select_enum, es9018k2m_put_out_select_enum),
SOC_ENUM_EXT("op_enable",
					 es9018k2m_op_en_enum,
					 es9018k2m_get_op_en_enum, es9018k2m_put_op_en_enum),

};

static int es9018k2m_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);

	int bick_div = es9018k2m->sysclk / params_rate(params)/64;
	u8 bick = 0;
	u8 iface = snd_soc_read(codec, ES9018K2M_INPUT_CONFIG) & 0x3f;

	pr_debug("ess9018: %s ########start\n",__func__);

	es9018k2m->playback_fs = params_rate(params);

	if(es9018k2m->playback_fs == 192000 || es9018k2m->playback_fs == 176400) {
		pm_qos_update_request(&muic_device_qos, MUIC_DEVICE_QOS_FREQ);
	}

	if(es9018k2m->master) {
		switch(bick_div) {
		case 16:
			bick = 0x40;
			break;
		case 8:
			bick = 0x20;
			break;
		case 4:
			bick = 0x00;
			break;
		default:
			return -EINVAL;
		}

		iface |= 0x80;

		if (es9018k2m->version)
			snd_soc_update_bits(codec, ES9018K2M_V_MODE_CONTROL, 0x60, bick);
		else
			snd_soc_update_bits(codec, ES9018K2M_W_MODE_CONTROL, 0x60, bick);

		pr_debug("ess9018: %s reg=0x%x value=0x%x,0x%x #####end\n",__func__,ES9018K2M_V_MODE_CONTROL,bick,snd_soc_read(codec, ES9018K2M_V_MODE_CONTROL));
		pr_debug("ess9018: %s reg=0x%x value=0x%x,0x%x #####end\n",__func__,ES9018K2M_W_MODE_CONTROL,bick,snd_soc_read(codec, ES9018K2M_W_MODE_CONTROL));

	} else {
		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x0;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			iface |= 0x40;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x80;
			break;
		default:
			return -EINVAL;
		}
	}
	//es9018k2m_set_deemph(codec);

	snd_soc_write(codec, ES9018K2M_INPUT_CONFIG, iface);
	pr_debug("ess9018: %s reg=0x%x value=0x%x,0x%x #####end\n",__func__,ES9018K2M_INPUT_CONFIG,iface,snd_soc_read(codec, ES9018K2M_INPUT_CONFIG));

	/*set soft_start high */
	snd_soc_update_bits(codec, ES9018K2M_SOFT_START, 0x80, 0x80);
	es9018k2m_set_gain(codec,ES9018K2M_GAIN_LOW);
	return 0;
}

static int es9018k2m_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 mute_reg = snd_soc_read(codec, ES9018K2M_GENERAL_SET) & 0xfc;

	if (mute)
		snd_soc_write(codec, ES9018K2M_GENERAL_SET, mute_reg | 0x3);
	else
		snd_soc_write(codec, ES9018K2M_GENERAL_SET, mute_reg);
	
	return 0;
}

static int es9018k2m_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	int version = 0;

	pr_debug("ess9018: %s ########start\n",__func__);

	switch (clk_id) {
	case ES9018K2M_SYSCLK_MCLK:
		break;
	default:
		return -EINVAL;
	}

	switch (freq) {
	case 49152000:
		/* Enable the corresponding clock */
		gpio_set_value(pdata->gpio_osc_48khz,1);
		gpio_set_value(pdata->gpio_osc_44khz,0);
		es9018k2m->sysclk = freq;
		break;	
	case 45158400:
		gpio_set_value(pdata->gpio_osc_44khz,1);
		gpio_set_value(pdata->gpio_osc_48khz,0);
		es9018k2m->sysclk = freq;
		break;
	default:
		printk("%s: errot system clock %d\n",__func__,freq);
		return -EINVAL;
	}
	usleep_range(1000, 1500);
	
	gpio_set_value(pdata->gpio_resetb, 1);

	mutex_lock(&es9018k2m->access_mutex);
	es9018k2m->active = 1;
	mutex_unlock(&es9018k2m->access_mutex);
	
	usleep_range(1000, 1500);
	
	pr_debug("es9018k2m reg=0x%d,value=0x%x\n",64,snd_soc_read(codec,64));
	
	version = (snd_soc_read(codec,64)>>2) & 0x0f;
	
	if(version == 0x4)
		es9018k2m->version = 0;//w version
	else if (version == 0xc)	
		es9018k2m->version = 1;//V version
	else {
		printk("es9018k2m can't support this version error\n");
		return -ENODEV;
	}
	regcache_sync(es9018k2m->regmap);
	
	pr_debug("es9018k2m %s :version:%d\n",__func__,es9018k2m->version);
	return 0;
}


static int es9018k2m_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	u8 iface = 0;
	u8 format = 0;

	pr_debug("ess9018: %s ########start\n",__func__);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface |= 0x80;
		es9018k2m->master = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface |= 0x00;
		es9018k2m->master = 0;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format &= ~0x30;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format |= 0x10;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
	case SND_SOC_DAIFMT_IB_NF:
	case SND_SOC_DAIFMT_NB_IF:
	default:
		return -EINVAL;
	}

	/* set iface */
	if (es9018k2m->version)
		snd_soc_write(codec, ES9018K2M_V_MODE_CONTROL, iface);
	else
		snd_soc_write(codec, ES9018K2M_W_MODE_CONTROL, iface);
	
	snd_soc_write(codec, ES9018K2M_INPUT_CONFIG, format);

	pr_debug("ess9018: %s ########end\n",__func__);

	return 0;
}

static int es9018k2m_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *codec_dai)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	es9018k2m_poweron(codec, 1);

	return 0;
}
static void es9018k2m_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *codec_dai)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	es9018k2m_poweron(codec, 0);

	pm_qos_update_request(&muic_device_qos, 0);

}


#define ES9018K2M_RATES (SNDRV_PCM_RATE_8000_44100 | SNDRV_PCM_RATE_64000 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |SNDRV_PCM_RATE_96000|\
					 SNDRV_PCM_RATE_176400| SNDRV_PCM_RATE_192000)

#define ES9018K2M_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_3LE|\
	SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops es9018k2m_dai_ops = {
	.hw_params	= es9018k2m_hw_params,
	.digital_mute	= es9018k2m_mute,
	.set_sysclk	= es9018k2m_set_dai_sysclk,
	.set_fmt	= es9018k2m_set_dai_fmt,
	.startup = es9018k2m_startup,
	.shutdown = es9018k2m_shutdown,
};

static struct snd_soc_dai_driver es9018k2m_dai = {
	.name = "ess9018k2m-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = ES9018K2M_RATES,
		.formats = ES9018K2M_FORMATS,},
	.ops = &es9018k2m_dai_ops,
	.symmetric_rates = 1,
};

#ifdef CONFIG_PM
static int es9018k2m_suspend(struct snd_soc_codec *codec)
{

	return 0;
}

static int es9018k2m_resume(struct snd_soc_codec *codec)
{
	//struct es9018k2m_platform_data *pdata = codec->dev->platform_data;
	//gpio_set_value(pdata->gpio_mute, 1);//for wm8998 headphone output louder

	return 0;
}
#else
#define es9018k2m_suspend NULL
#define es9018k2m_resume NULL
#endif

#ifdef CONFIG_OF
static struct es9018k2m_platform_data *ess9018k2m_parse_dt(struct device *dev)
{
	struct es9018k2m_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int gpio;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "ess9018k2m failed to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	dev->platform_data = pdata;

	gpio = of_get_gpio(np, 0);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_osc_48khz t gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_osc_48khz = gpio;

	gpio = of_get_gpio(np, 1);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_osc_44khz gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_osc_44khz = gpio;

	gpio = of_get_gpio(np, 2);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_resetb gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_resetb = gpio;

	gpio = of_get_gpio(np, 3);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_mute gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_mute = gpio;

	gpio = of_get_gpio(np, 4);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_op_en gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_op_en= gpio;

	gpio = of_get_gpio(np, 5);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_low_gain gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_low_gain = gpio;
	gpio = of_get_gpio(np, 6);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_out_select gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_out_select = gpio;
	gpio = of_get_gpio(np, 7);
	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "ess9018k2m failed to get gpio_dvdd_on gpio\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->gpio_dvdd_on = gpio;

	dev->platform_data = pdata;

	return pdata;
}
#else
static struct es9018k2m_platform_data *ess9018k2m_parse_dt(struct device *dev)
{
	struct es9018k2m_platform_data *pdata = dev->platform_data;

	return pdata;
}
#endif

static int es9018k2m_probe(struct snd_soc_codec *codec)
{
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	struct es9018k2m_platform_data *pdata = ess9018k2m_parse_dt(codec->dev);
	int ret = 0;

	pr_info("es9018k2m %s############# %d start\n",__func__,__LINE__);

	if (!pdata)
		return -EINVAL;

	if (pdata) {
		if (gpio_is_valid(pdata->gpio_osc_48khz)) {
			ret = gpio_request_one(pdata->gpio_osc_48khz,
					GPIOF_OUT_INIT_LOW, "es9018k2m osc 48khz");
			if (ret)
				goto err_out;
		}

		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);

		if (gpio_is_valid(pdata->gpio_osc_44khz)) {
			ret = gpio_request_one(pdata->gpio_osc_44khz,
					GPIOF_OUT_INIT_LOW, "es9018k2m osc 44.1khz");
			if (ret)
				goto err_gpio_osc_44khz;
		}

		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);

		if (gpio_is_valid(pdata->gpio_resetb)) {
			ret = gpio_request_one(pdata->gpio_resetb,
					GPIOF_OUT_INIT_LOW, "es9018k2m resetb");
			if (ret)
				goto err_gpio_osc_resetb;
		}

		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);
		
		if (gpio_is_valid(pdata->gpio_mute)) {
			ret = gpio_request_one(pdata->gpio_mute,
					GPIOF_OUT_INIT_HIGH, "es9018k2m mute");
			if (ret)
				goto err_gpio_amplifier;
		}

		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);
		if (gpio_is_valid(pdata->gpio_op_en)) {
			ret = gpio_request_one(pdata->gpio_op_en,
					GPIOF_OUT_INIT_LOW, "es9018k2m pa voltage");
			if (ret)
				goto err_gpio_op_en;
		}

		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);

		if (gpio_is_valid(pdata->gpio_low_gain)) {
			ret = gpio_request_one(pdata->gpio_low_gain,
					GPIOF_OUT_INIT_LOW, "es9018k2m gpio low gain");
			if (ret)
				goto err_gpio_low_gain;
		}
		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);
		if (gpio_is_valid(pdata->gpio_out_select)) {
			ret = gpio_request_one(pdata->gpio_out_select,
					GPIOF_OUT_INIT_LOW, "es9018k2m gpio out seletc");
			if (ret)
				goto err_gpio_out_select;
		}
		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);
		if (gpio_is_valid(pdata->gpio_dvdd_on)) {
			ret = gpio_request_one(pdata->gpio_dvdd_on,
					GPIOF_OUT_INIT_LOW, "es9018k2m gpio dvdd on");
			if (ret)
				goto err_gpio_dvdd_on;
		}

		pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);
	}

	codec->control_data = es9018k2m->regmap;
	codec->read = es9018k2m_read;
	codec->write = es9018k2m_write;

	pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);

	es9018k2m->vdd18_dac = regulator_get(codec->dev, "vdd18_audio");
	if (IS_ERR(es9018k2m->vdd18_dac)) {
		dev_err(codec->dev, "Failed to request vdd18_dac: %d\n", ret);
		goto err_gpio;
	}

	pr_debug("es9018k2m %s############# %d start\n",__func__,__LINE__);

	es9018k2m->codec = codec;

	pm_qos_add_request(&muic_device_qos, PM_QOS_DEVICE_THROUGHPUT, 0);

	printk("es9018k2m %s############# success\n",__func__);

	return 0;
	
err_gpio:
	if (gpio_is_valid(pdata->gpio_dvdd_on))
			gpio_free(pdata->gpio_dvdd_on);
err_gpio_dvdd_on:	
	if (gpio_is_valid(pdata->gpio_out_select))
			gpio_free(pdata->gpio_out_select);
err_gpio_out_select:
	if (gpio_is_valid(pdata->gpio_low_gain))
			gpio_free(pdata->gpio_low_gain);
err_gpio_low_gain:
	if (gpio_is_valid(pdata->gpio_op_en))
			gpio_free(pdata->gpio_op_en);
err_gpio_op_en:
	if (gpio_is_valid(pdata->gpio_mute))
		gpio_free(pdata->gpio_mute);
err_gpio_amplifier:
	if (gpio_is_valid(pdata->gpio_resetb))
		gpio_free(pdata->gpio_resetb);
err_gpio_osc_resetb:
	if (gpio_is_valid(pdata->gpio_osc_44khz))
		gpio_free(pdata->gpio_osc_44khz);
err_gpio_osc_44khz:
	if (gpio_is_valid(pdata->gpio_osc_48khz))
		gpio_free(pdata->gpio_osc_48khz);
err_out:	

	return ret;
}

static int es9018k2m_remove(struct snd_soc_codec *codec)
{
	struct es9018k2m_priv *es9018k2m = snd_soc_codec_get_drvdata(codec);
	struct es9018k2m_platform_data *pdata = codec->dev->platform_data;

	es9018k2m_poweron(codec, 0);

	if (regulator_is_enabled(es9018k2m->vdd18_dac))
		regulator_disable(es9018k2m->vdd18_dac);
	regulator_put(es9018k2m->vdd18_dac);

	gpio_free(pdata->gpio_mute);
	gpio_free(pdata->gpio_resetb);
	gpio_free(pdata->gpio_osc_44khz);
	gpio_free(pdata->gpio_osc_48khz);
	gpio_free(pdata->gpio_low_gain);
	gpio_free(pdata->gpio_out_select);
	gpio_free(pdata->gpio_op_en);
	gpio_free(pdata->gpio_dvdd_on);
#ifdef OP_EN
	cancel_delayed_work_sync(&es9018k2m_priv->op_en_work);
#endif

	pm_qos_remove_request(&muic_device_qos);

	return 0;
}
#ifdef OP_EN
static void es9018k2m_op_en_work(struct work_struct *work)
{
	struct es9018k2m_priv *es9018k2m = container_of(work,
							struct es9018k2m_priv,
							op_en_work.work);
	struct es9018k2m_platform_data *pdata = es9018k2m->codec->dev->platform_data;
	int unmute_state;
	int mute;

	if(meizu_board_version() >= MUTE_VER2) {
		unmute_state = 0;//mute
		mute = 1;
	} else {
		unmute_state = 1;//mute
		mute = 0;
	}

	mutex_lock(&es9018k2m->access_mutex);
	printk("%s:hifi active =%d\n",__func__,es9018k2m->active);
	if(es9018k2m->active == 0) {
		if(gpio_get_value(pdata->gpio_mute) == unmute_state)
			gpio_set_value(pdata->gpio_mute, mute);
		gpio_set_value(pdata->gpio_op_en, 0);
		msleep(300);
		if (es9018k2m->gain == ES9018K2M_GAIN_IDLE) {
			gpio_set_value(pdata->gpio_mute, 1);
			gpio_set_value(pdata->gpio_out_select, 0);
		} else {
			gpio_set_value(pdata->gpio_mute, unmute_state);
		}
	}
	mutex_unlock(&es9018k2m->access_mutex);
}
#endif

static struct snd_soc_codec_driver soc_codec_dev_es9018k2m = {
	.probe =	es9018k2m_probe,
	.remove =	es9018k2m_remove,
	.suspend =	es9018k2m_suspend,
	.resume =	es9018k2m_resume,
	.controls =	es9018k2m_snd_controls,
	.num_controls = ARRAY_SIZE(es9018k2m_snd_controls),
};

static bool es9018k2m_readable(struct device *dev, unsigned int reg)
{
	if(reg <= ES9018K2M_CACHEREGNUM && reg != 2 && reg !=3)
		return 1;
	else if(65 <= reg && reg <= 69)
		return 1;
	else if(70 <= reg && reg <= 93)
		return 1;
	else
		return 0;
}

static bool es9018k2m_writeable(struct device *dev, unsigned int reg)
{
	if(reg > ES9018K2M_CACHEREGNUM)
		return  0;
	else if(reg == 0x2 || reg == 0x3)
		return 0;
	else
		return 1;
}

static const struct regmap_config es9018k2m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 93,
	.readable_reg = es9018k2m_readable,
	.writeable_reg = es9018k2m_writeable,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = es9018k2m_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(es9018k2m_reg_defaults),
};

static int es9018k2m_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct es9018k2m_priv *es9018k2m;
	int ret;

	pr_info("es9018k2m %s############# start\n",__func__);
	es9018k2m_priv = NULL;

	es9018k2m = kzalloc(sizeof(struct es9018k2m_priv), GFP_KERNEL);
	if (es9018k2m == NULL)
		return -ENOMEM;

	es9018k2m->regmap = regmap_init_i2c(i2c, &es9018k2m_regmap);
	if (IS_ERR(es9018k2m->regmap)) {
		ret = PTR_ERR(es9018k2m->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		goto err;
	}

	i2c_set_clientdata(i2c, es9018k2m);

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_es9018k2m, &es9018k2m_dai, 1);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to register CODEC: %d\n", ret);
		goto err_regmap;
	}
	mutex_init(&es9018k2m->access_mutex);
	mutex_init(&es9018k2m->gain_mutex);
	es9018k2m->active = 0;
	es9018k2m->es9018k2m_notifier_list = &es9018k2m_hp_notifier_list;
	es9018k2m_priv = es9018k2m;
	es9018k2m_register_notify(es9018k2m_priv->es9018k2m_notifier_list,&es9018k2m_headphone_cb);
#ifdef OP_EN
	INIT_DELAYED_WORK(&es9018k2m->op_en_work, es9018k2m_op_en_work);
#endif

	pr_info("es9018k2m %s############# success\n",__func__);
	
	return 0;

err_regmap:
	regmap_exit(es9018k2m->regmap);
err:
	kfree(es9018k2m);
	return ret;
}

static int es9018k2m_i2c_remove(struct i2c_client *client)
{
	struct es9018k2m_priv *es9018k2m = i2c_get_clientdata(client);
	snd_soc_unregister_codec(&client->dev);
	regmap_exit(es9018k2m->regmap);
	es9018k2m_unregister_notify(es9018k2m_priv->es9018k2m_notifier_list,&es9018k2m_headphone_cb);
	es9018k2m_priv = NULL;
	kfree(es9018k2m);
	return 0;
}

static const struct i2c_device_id es9018k2m_i2c_id[] = {
	{ "ess9018k2m", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es9018k2m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id es9018k2m_of_match[] = {
	{ .compatible = "ess,ess9018k2m", },
	{ }
};

MODULE_DEVICE_TABLE(of, es9018k2m_of_match);
#endif

static struct i2c_driver es9018k2m_i2c_driver = {
	.driver = {
		.name = "ess9018k2m",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF		
		.of_match_table = es9018k2m_of_match,
#endif		
	},
	.probe =    es9018k2m_i2c_probe,
	.remove =   es9018k2m_i2c_remove,
	.id_table = es9018k2m_i2c_id,
};

static int __init es9018k2m_modinit(void)
{
	int ret = 0;
	ret = i2c_add_driver(&es9018k2m_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to register ES9018K2M I2C driver: %d\n", ret);
	}
	return ret;
}
module_init(es9018k2m_modinit);

static void __exit es9018k2m_exit(void)
{
	i2c_del_driver(&es9018k2m_i2c_driver);
}
module_exit(es9018k2m_exit);

MODULE_DESCRIPTION("ASoC ES9018K2M driver");
MODULE_AUTHOR("linfeng@meizu.com");
MODULE_LICENSE("GPL");

