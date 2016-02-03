/*
 *  avl7420_rt5647.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/module.h>


#include <sound/tlv.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <linux/i2c/rtxxxx.h>

#include <mach/regs-pmu-exynos7420.h>
#include <mach/regs-clock-exynos7420.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "../codecs/rt5647.h"

#ifdef CONFIG_SND_ESA_SA_EFFECT
#include "esa_sa_effect.h"
#endif

#if 0
#define ESPRESSO_RT5647_AUD_PLL_FREQ	(196608000)
#endif
#define ESPRESSO_RT5647_AUD_PLL_FREQ	(24000000)

#define BIT_HEADSET             (1<<0)  // Speaker and Mic
#define BIT_HEADSET_NO_MIC      (1<<1)  // Only Speaker


/* ESPRESSO use CLKOUT from AP */
#define ESPRESSO_MCLK_FREQ		24000000
#define ESPRESSO_AUD_PLL_FREQ		491520000

#define ESPRESSO_DEFAULT_MCLK1		24000000
#define ESPRESSO_DEFAULT_MCLK2		32768

#define ESPRESSO_TELCLK_RATE		(48000 * 1024)
struct snd_soc_codec *this_codec;

//static DECLARE_TLV_DB_SCALE(digital_tlv, -6400, 50, 0);
struct espresso_machine_priv {
	int clock_mode;
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct snd_soc_dai *aif[3];
	struct delayed_work mic_work;
	int aif2mode;

	int aif1rate;
	int aif2rate;

	unsigned int hp_impedance_step;
};

const char *aif2_mode_text[] = {
	"Slave", "Master"
};

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};
#if 0
static struct {
	int min;           /* Minimum impedance */
	int max;           /* Maximum impedance */
	unsigned int gain; /* Register value to set for this measurement */
} hp_gain_table[] = {
	{    0,      42, 0 },
	{   43,     100, 2 },
	{  101,     200, 4 },
	{  201,     450, 6 },
	{  451,    1000, 8 },
	{ 1001, INT_MAX, 0 },
};
#endif
static bool clkout_enabled;
static struct snd_soc_card espresso;
static struct snd_soc_codec *the_codec;

static void hp_detect(struct snd_soc_codec *codec)
{

        struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
        unsigned int val = 0;
        int jack_insert = 0;
        int jack_type = 0;
        int report_state =0;
        int previous_jacktype = 0;

#ifdef CONFIG_SND_JD1_FUNC
	val = snd_soc_read(codec,RT5647_INT_IRQ_ST )&(0x1<< 12);
	pr_debug("%s : JD1=> hardware V02 \n", __func__);
#endif
#ifdef CONFIG_SND_JD2_FUNC
        /*check sta_jd2  reg 0xbf bit 14 */
	val = snd_soc_read(codec, RT5647_INT_IRQ_ST) & 0x4000;
	pr_debug("%s : JD2=>hardware V01 \n", __func__);
#endif
        if(val ){
                jack_insert = 1;
                /*write 0 to clear*/
                /*snd_soc_update_bits(codec, RT5647_INT_IRQ_ST, 0x4000, 0x0);*/
        }else if(val == 0){
                jack_insert = 0;
        }

	previous_jacktype = rt5647->jack_type;
	pr_debug("%s[%d] : rt5647 previous jack type = %d \n",__func__,__LINE__,previous_jacktype);
	jack_type = rt5647_headset_detect(codec, jack_insert);
	pr_debug("%s[%d] : rt5647 current jack type = %d \n",__func__,__LINE__,jack_type);

        if(jack_type != previous_jacktype){
                snd_soc_jack_report(rt5647->jack, jack_type, SND_JACK_HEADSET);

                switch(jack_type){
                        case 1:report_state = BIT_HEADSET_NO_MIC; break;
                        case 3:report_state = BIT_HEADSET; break;
                        default: report_state = 0;break;
                        };
                pr_info("%s[%d] : report_state = %d\n", __func__,__LINE__,report_state);
                switch_set_state(&rt5647->sdev, report_state);
        }
        rt5647->jack_type = jack_type;

}

static void rt5647_irq_work(struct work_struct *work)
{
    struct snd_soc_codec *codec=this_codec;
    struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);

    hp_detect(codec);
    enable_irq(rt5647->hp_jd_irq);
    dev_dbg(codec->dev, "%s() hp detect irq.\n", __func__);
}

static irqreturn_t rt5647_threaded_isr(int irq, void *data)
{
	struct snd_soc_codec *codec = data;
    struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);

	disable_irq_nosync(rt5647->hp_jd_irq);

	if (!work_pending(&rt5647->hp_event_work)) {
		queue_work(rt5647->rt5647_workqueue, &rt5647->hp_event_work);
	}
	return IRQ_HANDLED;
}

static int espresso_7420_rt5647_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct device_node *np = rtd->card->dev->of_node;
	struct rt5647_priv *rt5647 = snd_soc_codec_get_drvdata(codec);
	int err;
	int ret = 0;
	this_codec=codec;

	rt5647->sdev.name = "h2w";
	ret = switch_dev_register(&rt5647->sdev);
	if (ret < 0){
		pr_err("%s :switch_dev_register error!\n", __func__);
		goto err_switch_dev_register;
	}

	INIT_WORK(&rt5647->hp_event_work, rt5647_irq_work);
	rt5647->rt5647_workqueue = create_singlethread_workqueue("rt5647_theaded_isr");
	if (!rt5647->rt5647_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
	rt5647->hp_jd_irq=gpio_to_irq(of_get_named_gpio(np, "rt5647,headphone_jack_detect_gpio", 0));
	ret= request_irq(rt5647->hp_jd_irq,
                        rt5647_threaded_isr, IRQF_DISABLED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                        "rt5647_theaded_isr",codec);
	if (ret< 0) {
		pr_err("Error in setting interrupt mode :%d int_gpio:%d\n", ret, of_get_named_gpio(np, "rt5647,headphone_jack_detect_gpio", 0));
		goto  int_gpio_request_error;
	}

	pr_info("%s() : OK\n", __func__);

	return 0;

int_gpio_request_error:
	switch_dev_unregister(&rt5647->sdev);

exit_create_singlethread:
	pr_err("[RT5647] %s: err_create_singlethread failed_\n", __func__);

err_switch_dev_register:
	return -1;
}

static void espresso_enable_mclk(bool on)
{
	pr_debug("%s: %s\n", __func__, on ? "on" : "off");
	clkout_enabled = on;
	writel(on ? 0x1000 : 0x1001, EXYNOS_PMU_PMU_DEBUG);
}

#ifndef CONFIG_SND_SOC_RT5647_CODEC_MASTER
static int set_aud_pll_rate(unsigned long rate)
{
	struct clk *fout_aud_pll;

	fout_aud_pll = clk_get(espresso.dev, "fout_aud_pll");
	if (IS_ERR(fout_aud_pll)) {
		pr_err("%s: failed to get fout_aud_pll\n", __func__);
		return PTR_ERR(fout_aud_pll);
	}

	if(clk_get_rate(fout_aud_pll) == rate)
		goto out;

	clk_set_rate(fout_aud_pll, rate);
	pr_info("%s: aud_pll rate = %ld\n", __func__, clk_get_rate(fout_aud_pll));

out:
	clk_put(fout_aud_pll);
	return 0;
}
#endif

static int get_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct espresso_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);

	ucontrol->value.integer.value[0] = priv->aif2mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct espresso_machine_priv *priv
		= snd_soc_card_get_drvdata(codec->card);

	priv->aif2mode = ucontrol->value.integer.value[0];

	dev_info(codec->dev, "set aif2 mode: %s\n",
					 aif2_mode_text[priv->aif2mode]);
	return  0;
}

static const struct snd_kcontrol_new espresso_codec_controls[] = {
	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),

};

static const struct snd_kcontrol_new espresso_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("HDMI"),
	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
};

const struct snd_soc_dapm_widget espresso_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("HDMIL"),
	SND_SOC_DAPM_OUTPUT("HDMIR"),
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPK("SPK", NULL),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Main Mic", NULL),
	SND_SOC_DAPM_MIC("Sub Mic", NULL),
};

const struct snd_soc_dapm_route espresso_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "Main Mic", NULL, "MICBIAS1" },
	{ "Sub Mic", NULL, "MICBIAS2" },
	{ "IN1L", NULL, "Main Mic" },
	{ "IN2L", NULL, "Sub Mic" },
};


static int espresso_aif1_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static void espresso_aif1_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);
}

static int espresso_start_sysclk(struct snd_soc_card *card);

#ifdef CONFIG_SND_SOC_RT5647_CODEC_MASTER
/*
 * ESPRESSO I2S Slave DAI operations.(Default)
 */
static int espresso_aif1_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct espresso_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int ret;
	unsigned int pll_out;

	dev_info(card->dev, "aif1: %dch, %dHz, %dbytes\n",
			params_channels(params), params_rate(params),
			params_buffer_bytes(params));

	espresso_enable_mclk(true);
	priv->aif1rate = params_rate(params) * 512;
	pll_out = params_rate(params) * 512;

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

	/*pll sources from mclk*/
	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5647_PLL1_S_MCLK,
					ESPRESSO_RT5647_AUD_PLL_FREQ, pll_out);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, RT5647_SCLK_S_PLL1,
					pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;


	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;
	return 0;
}
#else

/*
 * ESPRESSO I2S Master DAI operations.
 */
static int espresso_aif1_hw_params(struct snd_pcm_substream *substream,
                    struct snd_pcm_hw_params *params)
{
        struct snd_soc_pcm_runtime *rtd = substream->private_data;
        struct snd_soc_dai *codec_dai = rtd->codec_dai;
        struct snd_soc_card *card = rtd->card;
        struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
        int psr, pll, bfs, rfs, ret;
        unsigned long rclk, sclk, div;

        dev_info(card->dev, "aif1: %dch, %dHz, %dbytes\n",
                    params_channels(params), params_rate(params),
                    params_buffer_bytes(params));

        switch (params_format(params)){
                case SNDRV_PCM_FORMAT_U24:
                case SNDRV_PCM_FORMAT_S24:
			bfs = 64;
			break;
		case SNDRV_PCM_FORMAT_U16_LE:
		case SNDRV_PCM_FORMAT_S16_LE:
			bfs = 32;
			break;
		default:
			dev_err(card->dev, "Format Error: PCM FORMAT %d is not supported.\n", params_format(params));
			return -EINVAL;
        }

        switch (params_rate(params)) {
                case 48000:
                case 96000:
                case 192000:
                        if (bfs == 64)
                                rfs = 512;
                        else
                                rfs = 256;
                        break;
                case 11025:
                case 22050:
                case 44100:
                case 88200:
                case 176400:
                case 8000:
                case 12000:
                case 16000:
                case 24000:
                case 32000:
                case 64000:
                default:
                        dev_err(card->dev, "Sampling rate Error: Sampling rate %d hz is not supported.\n", params_rate(params));
                        return -EINVAL;
        }

        rclk = params_rate(params) * rfs;

        switch (rclk) {
                case 12288000:
                case 18432000:
                case 24576000:
                case 36864000:
                        psr = 2;
                        break;
                case 49152000:
                case 73728000:
                case 98304000:
                        psr = 1;
                        break;
                default:
                        dev_err(card->dev, "RCLK Error: Rclk %ld Not yet supported!\n", rclk);
        }

        /* Set AUD_PLL Frequency */
        sclk = rclk * psr;
        for (div = 2; div <= 16; div++) {
                if (sclk * div > ESPRESSO_AUD_PLL_FREQ)
                        break;
        }

	pll = sclk * (div - 1);

	set_aud_pll_rate(pll);

        /* Set Codec DAI configuration */
        ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
                         | SND_SOC_DAIFMT_NB_NF
                         | SND_SOC_DAIFMT_CBS_CFS);
        if (ret < 0) {
                dev_err(card->dev, "Failed to set codec fmt: %d\n", ret);
                return ret;
        }

        /* Set CPU DAI configuration */
        ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
                         | SND_SOC_DAIFMT_NB_NF
                         | SND_SOC_DAIFMT_CBS_CFS);
        if (ret < 0) {
                dev_err(card->dev, "Failed to set cpu fmt: %d\n", ret);
                return ret;
        }

        /* Set CPU DAI sysclk */
        ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
                        rfs, SND_SOC_CLOCK_OUT);
        if (ret < 0) {
                dev_err(card->dev, "Failed to set SAMSUNG_I2S_CDCL: %d\n", ret);
                return ret;
        }

        ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
                        0, MOD_OPCLK_PCLK);
        if (ret < 0) {
                dev_err(card->dev, "Failed to set SAMSUNG_I2S_OPCL: %d\n", ret);
                return ret;
        }

        ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1, 0, 0);
        if (ret < 0) {
                dev_err(card->dev, "Failed to set SAMSUNG_I2S_RCLKSRC_1 %d\n", ret);
                return ret;
        }

        ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
        if (ret < 0) {
                dev_err(card->dev, "Failed to set SAMSUNG_I2S_DIV_BCK %d\n", ret);
                return ret;
        }

	/* Set CODEC DAI sysclk */
	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5647_PLL1_S_MCLK,
				ESPRESSO_MCLK_FREQ, params_rate(params)*256);
	if (ret < 0)
		return ret;
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5647_SCLK_S_PLL1,
					params_rate(params)*256, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;
        return ret;
}

#endif /* CONFIG_SND_SOC_RT5647_CODEC_MASTER */

static int espresso_aif1_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static int espresso_aif1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);

	return 0;
}

static int espresso_aif1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_dbg(card->dev, "%s\n", __func__);
	printk( "%s\n", __func__);

	return 0;
}

static struct snd_soc_ops espresso_aif1_ops = {
	.startup = espresso_aif1_startup,
	.shutdown = espresso_aif1_shutdown,
	.hw_params = espresso_aif1_hw_params,
	.hw_free = espresso_aif1_hw_free,
	.prepare = espresso_aif1_prepare,
	.trigger = espresso_aif1_trigger,
};


/*
 * ESPRESSO HDMI I2S DAI operations.
 */
static int espresso_hdmi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int pll, div, sclk, bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 64;
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
	case 96000:
	case 192000:
		if (bfs == 64)
			rfs = 512;
		else
			rfs = 256;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 12288000:
	case 18432000:
	case 24576000:
	case 36864000:
		psr = 2;
		break;
	case 49152000:
	case 73728000:
	case 98304000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	/* Set AUD_PLL frequency */
	sclk = rclk * psr;
	for (div = 2; div <= 16; div++) {
		if (sclk * div > ESPRESSO_AUD_PLL_FREQ)
			break;
	}
	pll = sclk * (div - 1);
// later	set_aud_pll_rate(pll);

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

	return 0;
}

static struct snd_soc_ops espresso_hdmi_ops = {
	.hw_params = espresso_hdmi_hw_params,
};

static struct snd_soc_dai_link espresso_dai[] = {
	{ /* playback & recording */
		.name = "espresso-rt5647 playback",
		.stream_name = "i2s0-pri",
		.codec_dai_name = "rt5647-aif1",
		.init = espresso_7420_rt5647_init,
		.ops = &espresso_aif1_ops,
	}, { /* deep buffer playback */
		.name = "espresso-rt5647 multimedia playback",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_dai_name = "rt5647-aif1",
		.ops = &espresso_aif1_ops,
	}, { /* Aux DAI i/f */
		.name = "HDMI",
		.stream_name = "i2s1",
		.codec_dai_name = "dummy-aif1",
		.ops = &espresso_hdmi_ops,
	}

};

static int espresso_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd[0].codec;
	struct snd_soc_dai *codec_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *cpu_dai = card->rtd[0].cpu_dai;
	struct espresso_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int i, ret;

	priv->codec = codec;
	the_codec = codec;

	for (i = 0; i < 3; i++)
		priv->aif[i] = card->rtd[i].codec_dai;

	codec_dai->driver->playback.channels_max =
				cpu_dai->driver->playback.channels_max;

	/* close codec device immediately when pcm is closed */
	codec->ignore_pmdown_time = true;

	espresso_enable_mclk(true);

	ret = snd_soc_add_codec_controls(codec, espresso_codec_controls,
					ARRAY_SIZE(espresso_codec_controls));

	if (ret < 0) {
		dev_err(codec->dev,
				"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_SND_ESA_SA_EFFECT
	ret = esa_effect_register(card);
	if (ret < 0) {
		pr_err("Failed to add controls to effect: %d\n", ret);
		return ret;
	}
#endif
	dev_info(card->dev, "%s: Successfully created\n", __func__);
	espresso_enable_mclk(false);

	return 0;
}

static int espresso_suspend_post(struct snd_soc_card *card)
{
	/* espresso_enable_mclk(false); */
	return 0;
}

static int espresso_resume_pre(struct snd_soc_card *card)
{
	/* espresso_enable_mclk(true); */
	return 0;
}

static int espresso_start_sysclk(struct snd_soc_card *card)
{
	struct espresso_machine_priv *priv = snd_soc_card_get_drvdata(card);
	int fs;

	if (!priv->aif1rate)
		priv->aif1rate = 48000;

	if (priv->aif1rate >= 192000)
		fs = 256;
	else
		fs = 1024;

	espresso_enable_mclk(true);

	return 0;
}

static int espresso_stop_sysclk(struct snd_soc_card *card)
{
	espresso_enable_mclk(false);

	return 0;
}

static int espresso_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct espresso_machine_priv *priv = snd_soc_card_get_drvdata(card);
	printk("%s\r\n",__func__);
	if (!priv->codec || dapm != &priv->codec->dapm)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (card->dapm.bias_level == SND_SOC_BIAS_OFF) {
			espresso_start_sysclk(card);
			if (IS_ERR(devm_pinctrl_get_select(card->dev, "default")))
				dev_err(card->dev, "no pinctrl for irq\n");
		}
		break;
	case SND_SOC_BIAS_OFF:
		if (IS_ERR(devm_pinctrl_get_select(card->dev, "idle")))
			dev_err(card->dev, "no pinctrl for irq\n");
		espresso_stop_sysclk(card);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	default:
	break;
	}

	card->dapm.bias_level = level;
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static int espresso_set_bias_level_post(struct snd_soc_card *card,
				     struct snd_soc_dapm_context *dapm,
				     enum snd_soc_bias_level level)
{
	dev_dbg(card->dev, "%s: %d\n", __func__, level);

	return 0;
}

static struct snd_soc_card espresso = {
	.name = "ESPRESSO-RT5647",
	.owner = THIS_MODULE,

	.dai_link = espresso_dai,
	.num_links = ARRAY_SIZE(espresso_dai),

	.controls = espresso_controls,
	.num_controls = ARRAY_SIZE(espresso_controls),
	.dapm_widgets = espresso_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(espresso_dapm_widgets),
	.dapm_routes = espresso_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(espresso_dapm_routes),

	.late_probe = espresso_late_probe,
	.suspend_post = espresso_suspend_post,
	.resume_pre = espresso_resume_pre,
	.set_bias_level = espresso_set_bias_level,
	.set_bias_level_post = espresso_set_bias_level_post,

};

static int espresso_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &espresso;
	struct espresso_machine_priv *priv;
	bool hdmi_avail = true;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	card->dev = &pdev->dev;

	for (n = 0; np && n < ARRAY_SIZE(espresso_dai); n++) {
		if (!espresso_dai[n].cpu_dai_name) {
			espresso_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu", n);

			if (!espresso_dai[n].cpu_of_node && hdmi_avail) {
				espresso_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu-hdmi", 0);
				hdmi_avail = false;
			}

			if (!espresso_dai[n].cpu_of_node) {
				dev_err(&pdev->dev, "Property "
				"'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!espresso_dai[n].platform_name)
			espresso_dai[n].platform_of_node = espresso_dai[n].cpu_of_node;

		espresso_dai[n].codec_name = NULL;
		espresso_dai[n].codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", n);
		if (!espresso_dai[0].codec_of_node) {
			dev_err(&pdev->dev,
			"Property 'samsung,audio-codec' missing or invalid\n");
			ret = -EINVAL;
		}
	}

	snd_soc_card_set_drvdata(card, priv);

	ret = snd_soc_register_card(card);

	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	return ret;
}

static int espresso_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct espresso_machine_priv *priv = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);
	kfree(priv);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id espresso_rt5647_of_match[] = {
	{ .compatible = "samsung,espresso_rt5647", },
	{},
};
MODULE_DEVICE_TABLE(of, espresso_rt5647_of_match);
#endif /* CONFIG_OF */

static struct platform_driver espresso_audio_driver = {
	.driver		= {
		.name	= "espresso-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(espresso_rt5647_of_match),
	},
	.probe		= espresso_audio_probe,
	.remove		= espresso_audio_remove,
};

module_platform_driver(espresso_audio_driver);

MODULE_DESCRIPTION("ALSA SoC ESPRESSO RT5647");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:espresso-audio");
