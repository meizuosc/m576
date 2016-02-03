#ifndef _ESA_EFFECT_AUDIO_H
#define _ESA_EFFECT_AUDIO_H

enum {
	SOUNDALIVE = 0,
	MYSOUND,
	PLAYSPEED,
	SOUNDBALANCE,
};

/* Effect offset */
#define EFFECT_BASE		(0x37000)

#define SA_BASE			(0x0)
#define SA_CHANGE_BIT		(0x0)
#define SA_OUT_DEVICE		(0x10)
#define	SA_PRESET 		(0x14)
#define SA_EQ_BEGIN		(0x18)
#define SA_EQ_END		(0x30)
#define SA_3D_LEVEL		(0x34)
#define SA_BE_LEVEL		(0x38)
#define SA_REVERB		(0x3C)
#define SA_ROOMSIZE		(0x40)
#define SA_CLA_LEVEL		(0x44)
#define SA_VOLUME_LEVEL		(0x48)
#define SA_SQUARE_ROW		(0x4C)
#define SA_SQUARE_COLUMN	(0x50)
#define SA_TAB_INFO		(0x54)
#define SA_NEW_UI		(0x58)
#define SA_MAX_COUNT		(19)

#define MYSOUND_BASE		(0x100)
#define MYSOUND_CHANGE_BIT	(0x0)
#define MYSOUND_PARAM_BEGIN	(0x10)
#define MYSOUND_PARAM_END	(0x14)
#define MYSOUND_MAX_COUNT	(13)

#define VSP_BASE		(0x200)
#define VSP_CHANGE_BIT		(0x0)
#define VSP_INDEX		(0x10)
#define VSP_MAX_COUNT		(2)

#define LRSM_BASE		(0x300)
#define LRSM_CHANGE_BIT		(0x0)
#define LRSM_INDEX0		(0x10)
#define LRSM_INDEX1		(0x20)
#define LRSM_MAX_COUNT		(3)

#define CHANGE_BIT		(1)

int esa_effect_register(struct snd_soc_card *card);

enum {
	COMPR_DAI_MULTIMEDIA_1 = 0,
	COMPR_DAI_MAX,
};

struct compr_pdata {
	struct audio_processor *ap[COMPR_DAI_MAX];
	uint32_t volume[COMPR_DAI_MAX][2]; /* Left & Right */
};
extern struct compr_pdata aud_vol;
#endif
