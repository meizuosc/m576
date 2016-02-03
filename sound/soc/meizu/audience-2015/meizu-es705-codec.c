/*
 * meizu-es705-codec.c  --  Audience eScore I2S codec
 *
 * Copyright 2011 Audience, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "esxxx.h"
#include "escore.h"

#include "meizu-es705-codec.h"

#define DRV_NAME "es705-codec"
#define PORT_ID(base)   (((base) - ES705_PORT_A_ID) >> 8)
#define PORT_NAME(base) ('A' + PORT_ID(base))

struct es705_port_priv {
	int rate;
	int wl;
	unsigned int fmt;
	int dhwpt;
};

static struct es705_port_priv es705_ports[4];
static struct snd_soc_codec_driver soc_codec_es705;
static struct platform_device *es705_codec_device = NULL;

int es705_codec_add_dev(void)
{
	int rc = 0;

	es705_codec_device = platform_device_alloc(DRV_NAME, -1);
	if (!es705_codec_device ) {
		pr_err("%s(): es705 codec platform device allocation failed\n",
				__func__);
		rc = -ENOMEM;
		goto out;
	}

	if (platform_device_add(es705_codec_device)) {
		pr_err("%s(): Error while adding es705 codec device\n", __func__);
		platform_device_put(es705_codec_device);
	}
out:
	return rc;
}

static int es705_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	unsigned int base = dai->driver->base;
	unsigned int msg, resp;
	int ret, val;
	u32 cmd_block[3];

	return 0; // we will configure es705 ports in command preset

	if (escore_priv.flag.is_fw_ready == 0) {
		pr_warn("%s(port-%c): es705 firmware is not ready, abort\n",
				__func__, PORT_NAME(base));
		return 0;
	}

	/* word length */
	//if (es705_ports[PORT_ID(base)].wl != params_format(params))
	{
		printk("++%s(port-%c)++: format=%d\n", __func__, PORT_NAME(base),
			   params_format(params));

		val = snd_pcm_format_width(params_format(params)) - 1;
		cmd_block[0] = ES705_PORT_PARAM_ID + base + ES705_PORT_WORDLENGHT;
		cmd_block[1] = ES705_PORT_SET_PARAM + val;
		cmd_block[2] = 0xffffffff;
		ret = escore_write_block(&escore_priv, cmd_block);
		if (ret < 0) {
			dev_err(dai->dev, "escore_cmd: send [%08x %08x] failed\n",
				cmd_block[0], cmd_block[2]);
			goto out;
		}

		es705_ports[PORT_ID(base)].wl = params_format(params);
	}

	/* sample rate */
	//if (es705_ports[PORT_ID(base)].rate != params_rate(params))
	{
		printk("++%s(port-%c)++: rate=%d\n", __func__, PORT_NAME(base),
			   params_rate(params));

		msg = ES705_PORT_GET_PARAM + base + ES705_PORT_CLOCK;
		ret = escore_cmd(&escore_priv, msg, &resp);
		if (ret < 0) {
			dev_err(dai->dev, "escore_cmd: send %08x failed\n", msg);
			goto out;
		}

		val = (resp & 0x100) + (params_rate(params) / 1000);
		cmd_block[0] = ES705_PORT_PARAM_ID + base + ES705_PORT_CLOCK;
		cmd_block[1] = ES705_PORT_SET_PARAM + val;
		cmd_block[2] = 0xffffffff;
		ret = escore_write_block(&escore_priv, cmd_block);
		if (ret < 0) {
			dev_err(dai->dev, "escore_cmd: send [%08x %08x] failed\n",
				cmd_block[0], cmd_block[2]);
			goto out;
		}

		es705_ports[PORT_ID(base)].rate = params_rate(params);
	}

out:
	return ret;
}

static int es705_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	unsigned int base = dai->driver->base;
	unsigned int msg, resp;
	int ret = 0, val;
	u32 cmd_block[3];

	return 0; // we will configure es705 ports in command preset

	if (escore_priv.flag.is_fw_ready == 0) {
		pr_warn("%s(): es705 firmware is not ready, abort\n", __func__);
		return 0;
	}

	//if (es705_ports[PORT_ID(base)].fmt == fmt)
	//	return 0;

	/* port mode */
	if (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		printk("++%s(port-%c)++: port-mode=%s\n", __func__, PORT_NAME(base),
			   (fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_I2S ?
			   "i2s" : "pcm");

		switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_DSP_A:
			val = 0x00;
			break;
		case SND_SOC_DAIFMT_I2S:
			val = 0x01;
			break;
		default:
			dev_err(dai->dev, "Unsupported DAI format %d\n",
					fmt & SND_SOC_DAIFMT_FORMAT_MASK);
			ret =  -EINVAL;
			goto out;
		}

		cmd_block[0] = ES705_PORT_PARAM_ID + base + ES705_PORT_MODE;
		cmd_block[1] = ES705_PORT_SET_PARAM + val;
		cmd_block[2] = 0xffffffff;
		ret = escore_write_block(&escore_priv, cmd_block);
		if (ret < 0) {
			dev_err(dai->dev, "escore_cmd: send [%08x %08x] failed\n",
				cmd_block[0], cmd_block[2]);
			goto out;
		}

		/* latch edge: Tx on Falling Edge, Rx on Rising Edge */
		if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A) {
			cmd_block[0] = ES705_PORT_PARAM_ID + base + ES705_PORT_LATCHEDGE;
			cmd_block[1] = ES705_PORT_SET_PARAM + 0x01;
			cmd_block[2] = 0xffffffff;
			ret = escore_write_block(&escore_priv, cmd_block);
			if (ret < 0) {
				dev_err(dai->dev, "escore_cmd: send [%08x %08x] failed\n",
					cmd_block[0], cmd_block[2]);
				goto out;
			}
		}
	}

	/* master mode */
	if (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		printk("++%s(port-%c)++: clock-mode=%s\n", __func__, PORT_NAME(base),
			   (fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBS_CFS ?
			   "slave" : "master");

		msg = ES705_PORT_GET_PARAM + base + ES705_PORT_CLOCK;
		ret = escore_cmd(&escore_priv, msg, &resp);
		if (ret < 0) {
			dev_err(dai->dev, "escore_cmd: send %08x failed\n", msg);
			goto out;
		}

		switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			val = resp & 0xff; // slave: bit8 = 0
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			val = (resp & 0xff) | 0x100; // master: bit8 = 1
			break;
		default:
			dev_err(dai->dev, "Unsupported master mode %d\n",
					fmt & SND_SOC_DAIFMT_MASTER_MASK);
			ret = -EINVAL;
			goto out;
		}

		cmd_block[0] = ES705_PORT_PARAM_ID + base + ES705_PORT_CLOCK;
		cmd_block[1] = ES705_PORT_SET_PARAM + val;
		cmd_block[2] = 0xffffffff;
		ret = escore_write_block(&escore_priv, cmd_block);
		if (ret < 0) {
			dev_err(dai->dev, "escore_cmd: send [%08x %08x] failed\n",
				cmd_block[0], cmd_block[2]);
			goto out;
		}
	}

	es705_ports[PORT_ID(base)].fmt = fmt;

out:
	return ret;
}

static int es705_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	unsigned int msg, resp;
	int ret;
	static int old_tristate = -1;

	return 0; // we will configure es705 ports in command preset

	if (escore_priv.flag.is_fw_ready == 0) {
		pr_warn("%s(): es705 firmware is not ready, abort\n", __func__);
		return 0;
	}

	//if (old_tristate == tristate)
	//	return 0;

	printk("++%s()++: %s DHWPT\n", __func__,
		   tristate == ES705_DHWPT_RST ? "disable" : "enable");

	switch (tristate) {
	case ES705_DHWPT_RST:
	case ES705_DHWPT_A_B:
	case ES705_DHWPT_A_C:
	case ES705_DHWPT_A_D:
	case ES705_DHWPT_B_A:
	case ES705_DHWPT_B_C:
	case ES705_DHWPT_B_D:
	case ES705_DHWPT_C_A:
	case ES705_DHWPT_C_B:
	case ES705_DHWPT_C_D:
	case ES705_DHWPT_D_A:
	case ES705_DHWPT_D_B:
	case ES705_DHWPT_D_C:
		break;
	default:
		dev_err(dai->dev, "Unsupported dhwpt mode %d\n", tristate);
		ret = -EINVAL;
		goto out;
	}

	msg = ES705_DHWPT_COMMAND + tristate;
	ret = escore_cmd(&escore_priv, msg, &resp);
	if (ret < 0) {
		dev_err(dai->dev, "escore_cmd: send %08x failed\n", msg);
		goto out;
	}

	old_tristate = tristate;

out:
	return ret;
}

const struct snd_soc_dai_ops es705_dai_ops = {
	.hw_params = es705_hw_params,
	.set_fmt = es705_set_fmt,
	.set_tristate = es705_set_tristate,
};

#define ES705_RATES	SNDRV_PCM_RATE_8000_192000
#define ES705_FORMATS	(SNDRV_PCM_FMTBIT_S8 | \
			SNDRV_PCM_FMTBIT_U8 | \
			SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | \
			SNDRV_PCM_FMTBIT_U32_LE | \
			SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE)

struct snd_soc_dai_driver es705_dais[] = {
	{
		.name = "es705-porta",
		.base = ES705_PORT_A_ID,
		.playback = {
			.stream_name = "PORTA Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTA Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_dai_ops,
	},
	{
		.name = "es705-portb",
		.base = ES705_PORT_B_ID,
		.playback = {
			.stream_name = "PORTB Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTB Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_dai_ops,
	},
	{
		.name = "es705-portc",
		.base = ES705_PORT_C_ID,
		.playback = {
			.stream_name = "PORTC Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_dai_ops,
	},
	{
		.name = "es705-portd",
		.base = ES705_PORT_D_ID,
		.playback = {
			.stream_name = "PORTD Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.capture = {
			.stream_name = "PORTD Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ES705_RATES,
			.formats = ES705_FORMATS,
		},
		.ops = &es705_dai_ops,
	},
};

static int es705_codec_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_es705,
			es705_dais, ARRAY_SIZE(es705_dais));
}

static int es705_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	platform_device_unregister(es705_codec_device);
	return 0;
}

static struct platform_driver es705_codec_driver = {
	.probe		= es705_codec_probe,
	.remove		= es705_codec_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(es705_codec_driver);
MODULE_DESCRIPTION("ASoC ES driver");
MODULE_AUTHOR("Loon <loonzhong@meizu.com>");
MODULE_LICENSE("GPL");
