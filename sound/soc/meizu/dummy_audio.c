/*
 *  dummy_audio.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

static struct snd_soc_dai_link dummy_dais[] = {
	{
		.name = "dummy",
		.stream_name = "dummy stream",
		.codec_dai_name = "dummy-aif1",
		//.codec_name = "dummy-codec",
		.cpu_dai_name = "snd-soc-dummy-dai",
		.platform_name = "snd-soc-dummy",
	},
};

static struct snd_soc_card dummy_audio = {
	.name = "Dummy-Audio",
	.owner = THIS_MODULE,
	.dai_link = dummy_dais,
	.num_links = ARRAY_SIZE(dummy_dais),
};

static int dummy_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &dummy_audio;
	card->dev = &pdev->dev;

	printk("+++%s(%d)+++\n", __func__, __LINE__);

	for (n = 0; np && n < ARRAY_SIZE(dummy_dais); n++) {
		if (!dummy_dais[n].cpu_dai_name) {
			dummy_dais[n].cpu_of_node = of_parse_phandle(np,
					"dummy,audio-cpu", n);
			if (!dummy_dais[n].cpu_of_node) {
				dev_err(&pdev->dev,
						"'dummy,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!dummy_dais[n].platform_name)
			dummy_dais[n].platform_of_node = dummy_dais[n].cpu_of_node;

		if (!dummy_dais[n].codec_name) {
			dummy_dais[n].codec_of_node = of_parse_phandle(np,
					"dummy,audio-codec", n);
			if (!dummy_dais[0].codec_of_node) {
				dev_err(&pdev->dev,
						"'dummy,audio-codec' missing or invalid\n");
				ret = -EINVAL;
			}
		}
	}

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	printk("---%s(%d)---\n", __func__, __LINE__);
	return ret;
}

static int dummy_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id dummy_audio_of_match[] = {
	{ .compatible = "samsung,dummy-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, dummy_audio_of_match);
#endif /* CONFIG_OF */

static struct platform_driver dummy_audio_driver = {
	.driver		= {
		.name	= "dummy-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(dummy_audio_of_match),
	},
	.probe		= dummy_audio_probe,
	.remove		= dummy_audio_remove,
};

module_platform_driver(dummy_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Dummy Audio");
MODULE_AUTHOR("Loon <loonzhong@meizu.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:dummy-audio");
