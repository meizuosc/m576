/*
 * snd-dbmd2.c -- ASoC Machine Driver for DBMD2
 *
 *  Copyright (C) 2014 DSPG Technologies GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/of.h>

#define DRIVER_NAME "snd-dbmd2-mach-drv"
#define CODEC_NAME "codec" /* "dbmd2" */
#define PLATFORM_DEV_NAME "dbmd2-snd-soc-platform"

static int board_dai_init(struct snd_soc_pcm_runtime *rtd);

static struct snd_soc_dai_link board_dbmd2_dai_link[] = {
	{
		.name = "dbmd2_dai_link.1",
		.stream_name = "voice_sensory",
		/* asoc Cpu-Dai  device name */
		.cpu_dai_name = "DBMD2_codec_dai",
		/* asoc Codec-Dai device name */
		.codec_dai_name = "DBMD2_codec_dai",
		.init = board_dai_init,
	},
};

static int board_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct snd_soc_card dspg_dbmd2_card = {
	.name		= "dspg-dbmd2",
	.dai_link	= board_dbmd2_dai_link,
	.num_links	= ARRAY_SIZE(board_dbmd2_dai_link),
	.set_bias_level		= NULL,
	.set_bias_level_post	= NULL,
};

static int dbmd2_init_dai_link(struct snd_soc_card *card)
{
	int cnt;
	struct snd_soc_dai_link *dai_link;
	struct device_node *codec_node, *platform_node;

	codec_node = of_find_node_by_name(0, CODEC_NAME);
	if (!of_device_is_available(codec_node))
		codec_node = of_find_node_by_name(codec_node, CODEC_NAME);

	if (!codec_node) {
		pr_err("Codec node not found\n");
		return -1;
	}

	platform_node = of_find_node_by_name(0, PLATFORM_DEV_NAME);
	if (!platform_node) {
		pr_err("Platform node not found\n");
		return -1;
	}

	for (cnt = 0; cnt < card->num_links; cnt++) {
		dai_link = &card->dai_link[cnt];
		dai_link->codec_of_node = codec_node;
		dai_link->platform_of_node = platform_node;
	}

	return 0;
}

static int dbmd2_snd_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct snd_soc_card *card = &dspg_dbmd2_card;
	/* struct device_node *np = pdev->dev.of_node; */

	/* note: platform_set_drvdata() here saves pointer to the card's data
	 * on the device's 'struct device_private *p'->driver_data
	 */
	card->dev = &pdev->dev;
	if (dbmd2_init_dai_link(card) < 0) {
		dev_err(&pdev->dev, "initialization of DAI links failed\n");
		ret = -1;
		goto ERR_CLEAR;
	}

	/* Register ASoC sound Card */
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "registering of sound card failed\n");
		goto ERR_CLEAR;
	}

	dev_info(&pdev->dev, "DBMD2 ASoC card registered\n");

	return 0;

ERR_CLEAR:
	return ret;
}

static int dbmd2_snd_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static struct of_device_id snd_dbmd2_of_ids[] = {
	{ .compatible = "dspg,snd-dbmd2-mach-drv" },
	{ },
};

static struct platform_driver board_dbmd2_snd_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = snd_dbmd2_of_ids,
		.pm = &snd_soc_pm_ops,
	},
	.probe = dbmd2_snd_probe,
	.remove = dbmd2_snd_remove,
};

static int __init board_dbmd2_mod_init(void)
{
	return platform_driver_register(&board_dbmd2_snd_drv);
}
module_init(board_dbmd2_mod_init);

static void __exit board_dbmd2_mod_exit(void)
{
	platform_driver_unregister(&board_dbmd2_snd_drv);
}
module_exit(board_dbmd2_mod_exit);

MODULE_DESCRIPTION("ASoC machine driver for DSPG DBMD2");
MODULE_AUTHOR("DSP Group");
MODULE_LICENSE("GPL");
