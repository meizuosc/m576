/*
 *  meizu_audio_primary.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

/* MEIZU (m76) Audio Diagram:
 *
 * +-------+                       +-------+
 * |       |-----------------------| HIFI  |
 * |       |                       +-------+
 * |  AP   |
 * |       |-----------                BT
 * |       |          |                |
 * +-------+      +------+         +-------+
 *                |      |         |       |--EP
 * +-------+      |  NR  |---------| CODEC |
 * |  BP   |------|      |         |       |--HS
 * +-------+      +------+         +-------+
 *                    |                |
 *                  DMIC              S.PA
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/notifier.h>

#include <linux/mfd/arizona/core.h>
#include "../samsung/i2s.h"
#include "../samsung/i2s-regs.h"
#include "../codecs/wm8998.h"

#include "board-meizu-audio.h"

#include "audience/es705-export.h"
#include "audience/meizu-es705-codec.h"


//#define PERF_MODE

#ifdef PERF_MODE
#include <linux/perf_mode.h>
#endif

#define MCLK_FREQ  24000000

#define DEFAULT_BOOST_TIME_US     (1 * USEC_PER_SEC)
#define DEFAULT_BOOST_HMP_TIME_US (DEFAULT_BOOST_TIME_US * 2)

static int mclk_src = ARIZONA_FLL_SRC_MCLK1;
static int amp_reset = -1;
static int incall_ref = 0;
static struct snd_soc_card meizu_primary;
static BLOCKING_NOTIFIER_HEAD(meizu_call_notifier_list);

enum {
	MEIZU_CALL_STATE_OFF =  0,
	MEIZU_CALL_STATE_ON =  1,
};

int meizu_call_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&meizu_call_notifier_list, nb);
}
EXPORT_SYMBOL(meizu_call_register_notifier);

int meizu_call_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&meizu_call_notifier_list, nb);
}
EXPORT_SYMBOL(meizu_call_unregister_notifier);

int meizu_call_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&meizu_call_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(meizu_call_notifier_call_chain);

#ifdef PERF_MODE
static inline void audio_pm_qos_request(void)
{
	request_perf_mode(PERF_MODE_NORMAL, PM_QOS_PERF_MODE, HMP_BOOST_SEMI, DEFAULT_BOOST_TIME_US, DEFAULT_BOOST_HMP_TIME_US);
	usleep_range(8000, 10000);
}
#endif

#ifdef CONFIG_SND_SOC_WM8998_CODEC_MASTER
static int meizu_media_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	dev_dbg(rtd->dev, "++%s()++: format=%d, rate=%d\n", __func__,
			params_format(params), params_rate(params));

	/* cpu dai format configuration: i2s1 -> slave */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

	/* cpu dai clk settings */
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
				0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set I2S_CDCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
				0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set I2S_OPCLK: %d\n", ret);
		return ret;
	}

	/* codec dai format configuration: es705-porta -> i2s|master */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

	return 0;
}

#else
static int meizu_media_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;
	int sclk, bfs, psr, rfs;
	unsigned long rclk;

	dev_dbg(rtd->dev, "++%s()++: format=%d, rate=%d\n", __func__,
			params_format(params), params_rate(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 48000:
		if (bfs == 48)
			rfs = 512;
		else
			rfs = 256;
		break;
	case 16000:
		rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 12288000:
		psr = 8;
		break;
	case 24576000:
		psr = 4;
		break;
	case 8192000:
		psr = 12;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;

#if 0 // TODO
	for (div = 2; div <= 16; div++) {
		if (sclk * div > ESPRESSO_AUD_PLL_FREQ)
			break;
	}
	pll = sclk * (div - 1);
	set_aud_pll_rate(pll);
#endif

	pr_info("%s: bfs %d, rfs %d, rclk %ld, sclk %d \n", __func__, bfs, rfs, rclk, sclk);

	/* codec dai format configuration: es705-porta */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

	/* cpu dai format configuration: i2s1 -> master */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

	/* cpu dai clk settings */
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
				0, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set I2S_CDCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
				0, MOD_OPCLK_PCLK);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set I2S_OPCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, sclk);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set cpu dai rclksrc: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set cpu dai bfs: %d\n", ret);
		return ret;
	}

	return 0;
}
#endif

static struct snd_soc_ops meizu_media_ops = {
	.hw_params = meizu_media_hw_params,
};

static int meizu_media_be_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	unsigned int pll_out;

	printk("++%s()++: format=%d, rate=%d\n", __func__,
			params_format(params), params_rate(params));

	switch (params_rate(params)) {
	case 192000:
	case 96000:
	case 48000:
	case 32000:
	case 24000:
	case 16000:
	case 8000:
		pll_out = 24576000;
		break;
	case 176400:
	case 88200:
	case 44100:
	case 22050:
	case 11025:
		pll_out = 22579200;
		break;
	default:
		dev_err(rtd->dev, "Unsupported SampleRate: %d\n", -EINVAL);
		return -EINVAL;
	}

	/* es705: enable DHWPT */
	ret = snd_soc_dai_set_tristate(cpu_dai, ES705_DHWPT_D_A);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set dai tristate: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_SND_SOC_WM8998_CODEC_MASTER
	/* codec dai format configuration:
	 *   wm8998-aif1 -> master, i2s1 -> slave; es705 works at DHWPT mode */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}
#else
	/* codec dai format configuration:
	 *   wm8998-aif1 -> slave, i2s1 -> master; es705 works at DHWPT mode */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}
#endif

	/* codec clk settings */
	ret = snd_soc_codec_set_pll(codec, WM8998_FLL1_REFCLK,
				ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, WM8998_FLL1,
				mclk_src, MCLK_FREQ, pll_out);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
				ARIZONA_FLL_SRC_FLL1, pll_out, SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start sysclk: %d\n", ret);
		return ret;
	}

	return 0;
}

static int meizu_media_be_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	// workaround: lock some resource to fixup pop noise when usb plugin
#ifdef PERF_MODE	
	audio_pm_qos_request();
#endif

	/* aif1 uses sysclk domain */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to switch to SYSCLK: %d\n", ret);
		return ret;
	}
	return 0;
}

static void meizu_media_be_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_codec_set_sysclk(codec_dai->codec, ARIZONA_CLK_SYSCLK, 0, 0, 0);
	if (ret < 0)
		dev_err(codec_dai->dev, "Failed to stop SYSCLK: %d\n", ret);
}

static struct snd_soc_ops meizu_media_be_ops = {
	.hw_params = meizu_media_be_hw_params,
	.startup = meizu_media_be_startup,
	.shutdown = meizu_media_be_shutdown,
};

static int meizu_voice_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* codec dai format configuration: es705-portb -> pcm|slave */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

	// somewhere need to know the call event
	meizu_call_notifier_call_chain(MEIZU_CALL_STATE_ON, NULL);
	incall_ref++;

	return 0;
}

static void meizu_voice_shutdown(struct snd_pcm_substream *substream)
{
	// somewhere need to know this call event
	meizu_call_notifier_call_chain(MEIZU_CALL_STATE_OFF, NULL);
	incall_ref--;
}

static struct snd_soc_ops meizu_voice_ops = {
	.hw_params = meizu_voice_hw_params,
	.shutdown = meizu_voice_shutdown
};

static int meizu_voice_be_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	unsigned int pll_out;
	int bclk_rate;

	printk("++%s()++: format=%d, rate=%d\n", __func__,
			params_format(params), params_rate(params));

	switch (params_rate(params)) {
	case 48000:
	case 32000:
	case 24000:
	case 16000:
	case 8000:
		pll_out = 24576000;
		break;
	case 44100:
	case 22050:
	case 11025:
		pll_out = 22579200;
		break;
	default:
		dev_err(rtd->dev, "Unsupported SampleRate: %d\n", -EINVAL);
		return -EINVAL;
	}

	bclk_rate = params_rate(params);
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bclk_rate *= 32;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		bclk_rate *= 40;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bclk_rate *= 48;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		bclk_rate *= 64;
		break;
	default:
		return -EINVAL;
	}

	/* es705: reset DHWPT */
	ret = snd_soc_dai_set_tristate(cpu_dai, ES705_DHWPT_RST);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set dai tristate: %d\n", ret);
		return ret;
	}

	/* cpu dai format configuration: es705-portd -> master */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(cpu_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

	/* codec dai format configuration: wm8998-aif1 -> slave */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

#if 0
	/* codec clk settings */
	ret = snd_soc_codec_set_pll(codec, WM8998_FLL1_REFCLK,
				mclk_src, MCLK_FREQ, pll_out);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL1 REF: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, WM8998_FLL1,
				ARIZONA_FLL_SRC_AIF1BCLK, bclk_rate, pll_out);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}
#else
	/* codec clk settings : workaround to open pcm device quickly*/
	ret = snd_soc_codec_set_pll(codec, WM8998_FLL1,
				mclk_src, MCLK_FREQ, pll_out);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL1: %d\n", ret);
		return ret;
	}
#endif

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_SYSCLK,
				ARIZONA_FLL_SRC_FLL1, pll_out, SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start sysclk: %d\n", ret);
		return ret;
	}

	return 0;
}

static int meizu_voice_be_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* aif1 uses sysclk domain */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to switch to SYSCLK: %d\n", ret);
		return ret;
	}
	return 0;
}

static void meizu_voice_be_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_codec_set_sysclk(codec_dai->codec, ARIZONA_CLK_SYSCLK, 0, 0, 0);
	if (ret < 0)
		dev_err(codec_dai->dev, "Failed to stop SYSCLK: %d\n", ret);
}

static struct snd_soc_ops meizu_voice_be_ops = {
	.hw_params = meizu_voice_be_hw_params,
	.startup = meizu_voice_be_startup,
	.shutdown = meizu_voice_be_shutdown,
};

static int meizu_amplifier_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	unsigned int pll_out;

	printk("++%s()++: format=%d, rate=%d\n", __func__,
			params_format(params), params_rate(params));

	switch (params_rate(params)) {
	case 48000:
	case 32000:
	case 24000:
	case 16000:
	case 8000:
		pll_out = 24576000;
		break;
	case 44100:
	case 22050:
	case 11025:
		pll_out = 22579200;
		break;
	default:
		dev_err(rtd->dev, "Unsupported SampleRate: %d\n", -EINVAL);
		return -EINVAL;
	}

	/* codec dai format configuration: wm8998-aif3 -> master, tfa9890 -> slave */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

#if 0
	/* codec clk settings */
	ret = snd_soc_codec_set_pll(codec, WM8998_FLL2_REFCLK,
				ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL2 REF: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, WM8998_FLL2,
				mclk_src, MCLK_FREQ, pll_out);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL2: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				ARIZONA_FLL_SRC_FLL2, pll_out, SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start asyncclk: %d\n", ret);
		return ret;
	}
#else
	/* AIF3: SAMPLE_RATE_2 */
	ret = snd_soc_dai_set_clkdiv(codec_dai, ARIZONA_AIF_RATE, SAMPLE_RATE_2);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to select sample rate: %d\n", ret);
		return ret;
	}
#endif

	/*  BICK and LRCK always on in master mode */
	ret = snd_soc_dai_set_clkdiv(codec_dai, ARIZONA_AIF_CLK_ALWAYS_ON, 1);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set always on: %d\n", ret);
		return ret;
	}

	/* if you want to the specified BICK, such as 64fs or 48fs, this function should
	 * be invoked in default, the bick is equal to channels * word_length. */
	ret = snd_soc_dai_set_clkdiv(codec_dai, ARIZONA_AIF_BICK_RATE, 64);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to select sample rate: %d\n", ret);
		return ret;
	}

	return 0;
}

static int meizu_amplifier_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* aif3 uses asyncclk domain */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to swtich to SYSCLK: %d\n", ret);
		return ret;
	}
	return 0;
}

static void meizu_amplifier_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_codec_set_sysclk(codec_dai->codec, ARIZONA_CLK_SYSCLK, 0, 0, 0);
	if (ret < 0)
		dev_err(codec_dai->dev, "Failed to stop SYSCLK: %d\n", ret);
}

static struct snd_soc_ops meizu_amplifier_ops = {
	.hw_params = meizu_amplifier_hw_params,
	.startup = meizu_amplifier_startup,
	.shutdown = meizu_amplifier_shutdown,
};

static int meizu_bt_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = rtd->codec;
	int ret;
	unsigned int pll_out;

	printk("++%s()++: format=%d, rate=%d\n", __func__,
			params_format(params), params_rate(params));

	switch (params_rate(params)) {
	case 48000:
	case 16000:
	case 8000:
		pll_out = 24576000;
		break;
	default:
		dev_err(rtd->dev, "Unsupported SampleRate: %d\n", -EINVAL);
		return -EINVAL;
	}

	/* codec dai format configuration: wm8998-aif2 -> master, btsco -> slave */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to set dai format: %d\n", ret);
		return ret;
	}

#if 0
	/* codec clk settings */
	ret = snd_soc_codec_set_pll(codec, WM8998_FLL2_REFCLK,
				ARIZONA_FLL_SRC_NONE, 0, 0);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL2 REF: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, WM8998_FLL2,
				mclk_src, MCLK_FREQ, pll_out);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start FLL2: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec, ARIZONA_CLK_ASYNCCLK,
				ARIZONA_FLL_SRC_FLL2, pll_out, SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to start asyncclk: %d\n", ret);
		return ret;
	}
#else
	/* AIF2: SAMPLE_RATE_3 */
	ret = snd_soc_dai_set_clkdiv(codec_dai, ARIZONA_AIF_RATE, SAMPLE_RATE_3);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to select sample rate: %d\n", ret);
		return ret;
	}
#endif

	return 0;
}

static int meizu_bt_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* aif3 uses sysclk domain */
	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK, 0, 0);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Failed to swtich to SYSCLK: %d\n", ret);
		return ret;
	}
	return 0;
}

static void meizu_bt_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	ret = snd_soc_codec_set_sysclk(codec_dai->codec, ARIZONA_CLK_SYSCLK, 0, 0, 0);
	if (ret < 0)
		dev_err(codec_dai->dev, "Failed to stop SYSCLK: %d\n", ret);
}

static struct snd_soc_ops meizu_bt_ops = {
	.hw_params = meizu_bt_hw_params,
	.startup = meizu_bt_startup,
	.shutdown = meizu_bt_shutdown,
};

static struct snd_soc_dai_link meizu_dais[] = {
	[0] = { /* Primary DAI i/f */
		.name = "Media PRI",
		.stream_name = "i2s0-es705-pri",
		.codec_dai_name = "es705-porta",
		.codec_name = "es705-codec",
		.ops = &meizu_media_ops,
	},
	[1] = { /* Secondary DAI i/f: not use in m86, wm8998<>dsp<>i2s1 */
		.name = "Media SEC",
		.stream_name = "i2s0-es705-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "es705-porta",
		.codec_name = "es705-codec",
		.ops = &meizu_media_ops,
	},
	[2] = { /* Media Back-end DAI i/f */
		.name = "Media Back End",
		.stream_name = "es705-codec-media",
		.cpu_dai_name = "es705-portd",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "wm8998-aif1",
		.codec_name = "wm8998-codec",
		.ops = &meizu_media_be_ops,
		//.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[3] = { /* Voice DAI i/f */
		.name = "Voice",
		.stream_name = "baseband-es705",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "es705-portb",
		.codec_name = "es705-codec",
		.ops = &meizu_voice_ops,
		.ignore_suspend = 1,
	},
	[4] = { /* Voice Back-end DAI i/f */
		.name = "Voice Back End",
		.stream_name = "es705-codec-voice",
		.cpu_dai_name = "es705-portd",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "wm8998-aif1",
		.codec_name = "wm8998-codec",
		.ops = &meizu_voice_be_ops,
		//.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	[5] = { /* BT-SCO DAI i/f */
		.name = "WM8998 bt-sco",
		.stream_name = "bt-sco",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "wm8998-aif2",
		.codec_name = "wm8998-codec",
		.ops = &meizu_bt_ops,
		.ignore_suspend = 1,
	},
	[6] = { /* Smart-PA DAI i/f */
		.name = "WM8998 Amplifier",
		.stream_name = "amplifier",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
		.codec_dai_name = "wm8998-aif3",
		.codec_name = "wm8998-codec",
		.ops = &meizu_amplifier_ops,
		.ignore_suspend = 1,
	},
};

static const struct snd_kcontrol_new meizu_wm8998_controls[] =
{
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Earpiece"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Sec Mic"),
	SOC_DAPM_PIN_SWITCH("Ter Mic"),
};

static const struct snd_soc_dapm_widget meizu_wm8998_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_HP("Earpiece", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Sec Mic", NULL),
	SND_SOC_DAPM_MIC("Ter Mic", NULL),
};

static const struct snd_soc_dapm_route meizu_wm8998_routes[] = {
	{"Headset Mic", NULL, "MICBIAS1"},
	{"Sec Mic", NULL, "MICBIAS2"},
	{"Ter Mic", NULL, "MICBIAS2"},

	{"Headphone", NULL, "HPOUTL"},
	{"Headphone", NULL, "HPOUTR"},
	{"Earpiece", NULL, "EPOUT"},
	{"IN2B", NULL, "Headset Mic"},

	// actually sec-mic connected to es705,
	// "Sec Mic->IN2A->IN2L->AIF1 Capture->AIF1 Tx Input" is a complete path
	// which brings "MICBIAS2" to power-up.
	{"IN2A", NULL, "Sec Mic"},
	{"IN1AL", NULL, "Ter Mic"},
};

static int meizu_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[2].codec; // codec: wm8998
	struct arizona *arizona = dev_get_drvdata(codec->dev->parent);
	int rc;

	switch (arizona->pdata.clk32k_src) {
	case ARIZONA_32KZ_MCLK1:
		mclk_src = ARIZONA_FLL_SRC_MCLK2;
		break;
	case ARIZONA_32KZ_MCLK2:
	default:
		mclk_src = ARIZONA_FLL_SRC_MCLK1;
		break;
	}

	/* reset amplifier tfa9890 */
	amp_reset = of_get_named_gpio(card->dev->of_node,
					      "samsung,amp-reset-gpio", 0);
	if (amp_reset < 0) {
		dev_err(card->dev, "%s(): get gpio of amp_reset failed\n", __func__);
	}
	else {
		rc = gpio_request(amp_reset, "amp_reset");
		if (rc < 0) {
			dev_warn(card->dev, "%s(): amp_reset already requested", __func__);
		} else {
			rc = gpio_direction_output(amp_reset, 1);
			if (rc < 0)
				dev_err(card->dev, "%s(): amp_reset direction failed", __func__);
			else {
				gpio_set_value(amp_reset, 1);
				usleep_range(1000, 1000);
				gpio_set_value(amp_reset, 0);
			}
		}
	}

	snd_soc_dapm_disable_pin(&card->dapm, "Headphone");
	snd_soc_dapm_disable_pin(&card->dapm, "Earpiece");
	snd_soc_dapm_disable_pin(&card->dapm, "Headset Mic");
	snd_soc_dapm_disable_pin(&card->dapm, "Sec Mic");

	/* scenario: incall while AP is in suspend */
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headphone");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Earpiece");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&card->dapm, "Sec Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Playback");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2 Capture");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3 Capture");

	snd_soc_dapm_sync(&card->dapm);
	snd_soc_dapm_sync(&codec->dapm);

	es705_remote_add_codec_controls(codec);
	return 0;
}

#if 0
static int meizu_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai = card->rtd[2].codec_dai; // codec: wm8998
	int ret = 0;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (codec_dai->codec->active)
			break;
	case SND_SOC_BIAS_OFF:
		printk("++%s()++: stop codec fll\n", __func__);
		ret = snd_soc_codec_set_pll(codec_dai->codec, WM8998_FLL1, 0, 0, 0);
		if (ret != 0) {
			dev_err(codec_dai->dev, "Failed to stop FLL1: %d\n", ret);
			goto out;
		}
		ret = snd_soc_codec_set_pll(codec_dai->codec, WM8998_FLL2, 0, 0, 0);
		if (ret != 0) {
			dev_err(codec_dai->dev, "Failed to stop FLL2: %d\n", ret);
			goto out;
		}
		break;
	default:
		break;
	}

out:
	dapm->bias_level = level;
	return 0;
}
#endif

static int meizu_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_dai *codec_dai = card->rtd[2].codec_dai; // codec: wm8998
	int ret;

	if (incall_ref || codec_dai->active) {
		dev_warn(codec_dai->dev, "codec-dai is still active, keep mclk and fll active\n");
		return 0;
	}

	printk("++%s()++: stop codec fll\n", __func__);
	ret = snd_soc_codec_set_pll(codec_dai->codec, WM8998_FLL1, 0, 0, 0);
	if (ret != 0) {
		dev_err(codec_dai->dev, "Failed to stop FLL1: %d\n", ret);
		return ret;
	}
	ret = snd_soc_codec_set_pll(codec_dai->codec, WM8998_FLL2, 0, 0, 0);
	if (ret != 0) {
		dev_err(codec_dai->dev, "Failed to stop FLL2: %d\n", ret);
		return ret;
	}

	meizu_enable_mclk(false);
	return 0;
}

static int meizu_resume_pre(struct snd_soc_card *card)
{
	meizu_enable_mclk(true);
	return 0;
}

static struct snd_soc_card meizu_primary = {
	.name = "Meizu-Primary",
	.owner = THIS_MODULE,
	.dai_link = meizu_dais,
	.num_links = ARRAY_SIZE(meizu_dais),
	.controls = meizu_wm8998_controls,
	.num_controls = ARRAY_SIZE(meizu_wm8998_controls),
	.dapm_widgets = meizu_wm8998_widgets,
	.num_dapm_widgets = ARRAY_SIZE(meizu_wm8998_widgets),
	.dapm_routes = meizu_wm8998_routes,
	.num_dapm_routes = ARRAY_SIZE(meizu_wm8998_routes),
	.late_probe = meizu_late_probe,
	.suspend_post = meizu_suspend_post,
	.resume_pre = meizu_resume_pre,
	//.set_bias_level_post = meizu_set_bias_level_post,
};

static int meizu_primary_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &meizu_primary;
	card->dev = &pdev->dev;

	/* enable mclk */
	meizu_audio_clock_init();

	for (n = 0; np && n < ARRAY_SIZE(meizu_dais); n++) {
		if (!meizu_dais[n].cpu_dai_name) {
			meizu_dais[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu", n);
			if (!meizu_dais[n].cpu_of_node) {
				dev_err(&pdev->dev,
						"'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!meizu_dais[n].platform_name)
			meizu_dais[n].platform_of_node = meizu_dais[n].cpu_of_node;

		if (!meizu_dais[n].codec_name) {
			meizu_dais[n].codec_of_node = of_parse_phandle(np,
					"samsung,audio-codec", n);
			if (!meizu_dais[0].codec_of_node) {
				dev_err(&pdev->dev,
						"'samsung,audio-codec' missing or invalid\n");
				ret = -EINVAL;
			}
		}
	}

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	return ret;
}

static int meizu_primary_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	if (amp_reset != -1)
		gpio_free(amp_reset);

	snd_soc_unregister_card(card);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id meizu_wm8998_of_match[] = {
	{ .compatible = "samsung,meizu-wm8998", },
	{},
};
MODULE_DEVICE_TABLE(of, meizu_wm8998_of_match);
#endif /* CONFIG_OF */

static struct platform_driver meizu_primary_driver = {
	.driver		= {
		.name	= "meizu-audio-primary",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(meizu_wm8998_of_match),
	},
	.probe		= meizu_primary_probe,
	.remove		= meizu_primary_remove,
};

module_platform_driver(meizu_primary_driver);

MODULE_DESCRIPTION("ALSA SoC Meizu-Primary Audio");
MODULE_AUTHOR("Loon <loonzhong@meizu.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:meizu-audio-primary");
