/*
 *  meizu_audio_hifi.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include "../samsung/i2s.h"
#include "../samsung/i2s-regs.h"
#include "../codecs/es9018k2m.h"
#include "board-meizu-audio.h"

//test
//define CODEC_SLAVE
#define MEIZU_AUD_PLL_FREQ	196608009

static struct snd_soc_card meizu_hifi;

#ifndef CODEC_SLAVE // codec master
static int meizu_hifi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;
	int pll_out = 49152000;

	printk("+++%s(%d)+++: rate=%d\n", __func__, __LINE__, params_rate(params));

	switch (params_rate(params)) {
	case 192000:	
	case 96000:
	case 64000:
	case 48000:
	case 32000:
	case 16000:
	case 8000:
		pll_out = 49152000;
		break;
	case 176400:	
	case 88200:
	case 44100:
	case 22050:
	case 11025:
		pll_out = 45158400;
		break;
	default:
		dev_err(cpu_dai->dev, "Unsupported SampleRate, ret = %d\n", -EINVAL);
		return -EINVAL;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, ES9018K2M_SYSCLK_MCLK,
					pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK,64);
	if (ret < 0)
		return ret;

	printk("---%s(%d)---\n", __func__, __LINE__);
	return 0;
}

#else // AP master
static int meizu_hifi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, div, sclk, bfs, psr, rfs, ret;
	unsigned long rclk;
	int pll_out = 49152000;

	printk("+++%s(%d)+++: rate=%d\n", __func__, __LINE__, params_rate(params));

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
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 192000:	
	case 96000:
	case 64000:
	case 48000:
	case 32000:
	case 16000:
	case 8000:
		pll_out = 49152000;
		break;
	case 176400:	
	case 88200:
	case 44100:
	case 22050:
	case 11025:
		pll_out = 45158400;
		break;
	default:
		dev_err(cpu_dai->dev, "Unsupported SampleRate, ret = %d\n", -EINVAL);
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	for (div = 2; div <= 16; div++) {
		if (sclk * div > MEIZU_AUD_PLL_FREQ)
			break;
	}
	pll = sclk * (div - 1);
	set_aud_pll_rate(cpu_dai->dev, pll);

	ret = snd_soc_dai_set_sysclk(codec_dai, ES9018K2M_SYSCLK_MCLK,
					pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* Set Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	printk("---%s(%d)---\n", __func__, __LINE__);
	return 0;
}		
#endif

static struct snd_soc_ops meizu_hifi_ops = {
	.hw_params = meizu_hifi_hw_params,
};

static struct snd_soc_dai_link meizu_hifi_dai[] = {
	[0] = { /*HiFi Secondary DAI i/f */
		.name = "HiFi SEC",
		.stream_name = "i2s0-hifi-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "ess9018k2m-hifi",
		.ops = &meizu_hifi_ops,
	},
	[1] = { /*HiFi Primary DAI i/f */
		.name = "HiFi PRIMARY",
		.stream_name = "i2s0-hifi-pri",
		.codec_dai_name = "ess9018k2m-hifi",
		.ops = &meizu_hifi_ops,
	}, 	
};

static struct snd_soc_card meizu_hifi = {
	.name = "Meizu-HiFi",
	.owner = THIS_MODULE,
	.dai_link = meizu_hifi_dai,
	.num_links = ARRAY_SIZE(meizu_hifi_dai),
};

static int meizu_hifi_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &meizu_hifi;
	card->dev = &pdev->dev;

	for (n = 0; np && n < ARRAY_SIZE(meizu_hifi_dai); n++) {
		if (!meizu_hifi_dai[n].cpu_dai_name) {
			meizu_hifi_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu", n);

			if (!meizu_hifi_dai[n].cpu_of_node) {
				dev_err(&pdev->dev, "Property "
				"'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!meizu_hifi_dai[n].platform_name)
			meizu_hifi_dai[n].platform_of_node = meizu_hifi_dai[n].cpu_of_node;

		meizu_hifi_dai[n].codec_name = NULL;
		meizu_hifi_dai[n].codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", n);
		if (!meizu_hifi_dai[0].codec_of_node) {
			dev_err(&pdev->dev,
			"Property 'samsung,audio-codec' missing or invalid\n");
			ret = -EINVAL;
		}
	}

	ret = snd_soc_register_card(card);

	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	return ret;
}

static int meizu_hifi_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id meizu_ess9018k2m_of_match[] = {
	{ .compatible = "samsung,meizu-ess9018k2m", },
	{},
};
MODULE_DEVICE_TABLE(of, meizu_ess9018k2m_of_match);
#endif

static struct platform_driver meizu_hifi_driver = {
	.driver		= {
		.name	= "meizu-audio-hifi",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(meizu_ess9018k2m_of_match),
	},
	.probe		= meizu_hifi_probe,
	.remove		= meizu_hifi_remove,
};

module_platform_driver(meizu_hifi_driver);

MODULE_DESCRIPTION("ALSA SoC Meizu-HiFi Audio");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:meizu-audio-hifi");
