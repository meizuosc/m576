#include <sound/soc.h>
#include <sound/tlv.h>

#include <linux/module.h>
#include <linux/io.h>

#include "esa_sa_effect.h"
#include "seiren/seiren.h"

#define COMPRESSED_LR_VOL_MAX_STEPS     0x2000

const DECLARE_TLV_DB_LINEAR(compr_vol_gain,  0, COMPRESSED_LR_VOL_MAX_STEPS);
struct compr_pdata aud_vol;

static int esa_ctl_sa_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 19;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;
	return 0;
}

static int esa_ctl_my_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 13;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;
	return 0;
}

static int esa_ctl_ps_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1000;
	return 0;
}

static int esa_ctl_sb_info(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 3;
	uinfo->value.integer.min = -50;
	uinfo->value.integer.max = 50;
	return 0;
}

static int esa_ctl_sa_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[SA_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < SA_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(SOUNDALIVE, effect_val, SA_MAX_COUNT);

	return 0;
}

static int esa_ctl_my_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[MYSOUND_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < MYSOUND_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(MYSOUND, effect_val, MYSOUND_MAX_COUNT);

	return 0;
}

static int esa_ctl_ps_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[VSP_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < VSP_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(PLAYSPEED, effect_val, VSP_MAX_COUNT);

	return 0;
}

static int esa_ctl_sb_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int i;
	int effect_val[LRSM_MAX_COUNT];

	pr_info("%s\n", __func__);

	for (i = 0; i < LRSM_MAX_COUNT; i++) {
		effect_val[i] = ucontrol->value.integer.value[i];
	}

	esa_effect_write(SOUNDBALANCE, effect_val, LRSM_MAX_COUNT);

	return 0;
}

static int esa_ctl_sa_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_my_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_ps_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int esa_ctl_sb_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s\n", __func__);

	return 0;
}

#define ESA_CTL_EQ_SWITCH(xname, xval, xinfo, xget_effect, xset_effect) {\
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,\
	.info = xinfo, \
	.get = xget_effect, .put = xset_effect, \
	.private_value = xval }

int esa_compr_set_volume(struct audio_processor *ap, int left, int right)
{
	void __iomem *mailbox;

	mailbox = esa_compr_get_mem();
	writel(left, mailbox + COMPR_LEFT_VOL);
	writel(right, mailbox + COMPR_RIGHT_VOL);

	return 0;
}

static int compr_volume_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct audio_processor *ap = aud_vol.ap[mc->reg];
	uint32_t *volume = aud_vol.volume[mc->reg];

	volume[0] = ucontrol->value.integer.value[0];
	volume[1] = ucontrol->value.integer.value[1];
	pr_info("%s: mc->reg %d left_vol %d right_vol %d\n",
			__func__, mc->reg, volume[0], volume[1]);
	if (ap)
		esa_compr_set_volume(ap, volume[0], volume[1]);

	return 0;
}

static int compr_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	uint32_t *volume = aud_vol.volume[mc->reg];

	pr_info("%s: mc->reg %d left_vol %d right_vol %d\n",
		__func__, mc->reg, volume[0], volume[1]);
	ucontrol->value.integer.value[0] = volume[0];
	ucontrol->value.integer.value[1] = volume[1];

	return 0;
}

static const struct snd_kcontrol_new esa_effect_controls[] = {
	ESA_CTL_EQ_SWITCH("SA data", 0, \
		esa_ctl_sa_info, esa_ctl_sa_get, esa_ctl_sa_put),
	ESA_CTL_EQ_SWITCH("Audio DHA data", 0, \
		esa_ctl_my_info, esa_ctl_my_get, esa_ctl_my_put),
	ESA_CTL_EQ_SWITCH("VSP data", 0, \
		esa_ctl_ps_info, esa_ctl_ps_get, esa_ctl_ps_put),
	ESA_CTL_EQ_SWITCH("LRSM data", 0, \
		esa_ctl_sb_info, esa_ctl_sb_get, esa_ctl_sb_put),
	SOC_DOUBLE_EXT_TLV("Compress Playback 3 Volume", COMPR_DAI_MULTIMEDIA_1,
			0, 8, COMPRESSED_LR_VOL_MAX_STEPS, 0,
			compr_volume_get, compr_volume_put, compr_vol_gain),
};

int esa_effect_register(struct snd_soc_card *card)
{
	int ret;

	ret = snd_soc_add_card_controls(card, esa_effect_controls,
			ARRAY_SIZE(esa_effect_controls));

	if (ret < 0)
		pr_err("Failed to add controls : %d", ret);

	return ret;
}

MODULE_AUTHOR("HaeKwang Park, <haekwang0808.park@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC SEIREN effect Driver");
MODULE_LICENSE("GPL");
