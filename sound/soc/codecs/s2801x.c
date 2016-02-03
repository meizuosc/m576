/*
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <mach/regs-pmu-exynos7580.h>

#include <sound/exynos.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "s2801x.h"

#define S2801_SYS_CLK_FREQ	(24576000U)

#define S2801_ALIVE_ON			0
#define S2801_ALIVE_OFF			1

#define S2801_SW_RESET			0
#define S2801_SW_UN_RESET		1

enum s2801x_type {
	SRC2801X,
};

struct s2801x_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct device *dev;
	struct clk *sclk_audmixer;
	struct clk *sclk_audmixer_bclk0;
	struct clk *sclk_audmixer_bclk1;
	struct clk *sclk_audmixer_bclk2;
	struct clk *sclk_mi2s_bclk;
	struct clk *dout_audmixer;
	void *sysreg_reset;
	void *sysreg_i2c_id;
	unsigned short i2c_addr;
};

struct s2801x_priv *s2801x;

/**
 * The registers are power-on reset values of the codecs. It is used with
 * register cache support. If this register is not present, the values are read
 * from the hardware after initialization.
 */
static struct reg_default s2801x_reg[] = {
	/* { reg, value } */
	{ 0x00, 0x03 },
	{ 0x01, 0x0c },
	{ 0x02, 0x62 },
	{ 0x03, 0x24 },
	{ 0x04, 0x0c },
	{ 0x05, 0x62 },
	{ 0x06, 0x24 },
	{ 0x07, 0x0c },
	{ 0x08, 0x62 },
	{ 0x09, 0x24 },
	{ 0x0a, 0x04 },
	{ 0x0b, 0x01 },
	{ 0x0c, 0x02 },
	{ 0x0d, 0x04 },
	{ 0x0e, 0x03 },
	{ 0x0f, 0x00 },
	{ 0x10, 0x00 },
	{ 0x11, 0x00 },
	{ 0x16, 0x00 },
	{ 0x17, 0x00 },
	{ 0x68, 0x00 },
	{ 0x69, 0x80 },
	{ 0x6a, 0x58 },
	{ 0x6b, 0x1a },
	{ 0x6c, 0x1a },
	{ 0x6d, 0x12 },
	{ 0x6e, 0x12 },
	{ 0x6f, 0x12 },
	{ 0x70, 0x17 },
	{ 0x71, 0x6c },
	{ 0x72, 0x6c },
};

/**
 * Return value:
 * true: if the register value cannot be cached, hence we have to read from the
 * register directly
 * false: if the register value can be read from cache
 */
static bool s2801x_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case S2801X_REG_00_SOFT_RSTB:
		return true;
	default:
		return false;
	}
}

/**
 * Return value:
 * true: if the register value can be read
 * flase: if the register cannot be read
 */
static bool s2801x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case S2801X_REG_00_SOFT_RSTB ... S2801X_REG_11_DMIX2:
	case S2801X_REG_16_DOUTMX1 ... S2801X_REG_17_DOUTMX2:
	case S2801X_REG_68_ALC_CTL ... S2801X_REG_72_ALC_SGR:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config s2801x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = S2801X_MAX_REGISTER,
	.reg_defaults = s2801x_reg,
	.num_reg_defaults = ARRAY_SIZE(s2801x_reg),
	.volatile_reg = s2801x_volatile_register,
	.readable_reg = s2801x_readable_register,
	.cache_type = REGCACHE_RBTREE,
};

static int s2801x_reset_sys_data(void)
{
	regmap_write(s2801x->regmap, S2801X_REG_00_SOFT_RSTB, 0x0);

	msleep(20);

	regmap_write(s2801x->regmap, S2801X_REG_00_SOFT_RSTB,
			BIT(SOFT_RSTB_DATA_RSTB_SHIFT) |
			BIT(SOFT_RSTB_SYS_RSTB_SHIFT));

	msleep(20);

	return 0;
}

static void s2801x_reset_data(void)
{
	/* Data reset sequence, toggle bit 1 */
	regmap_write(s2801x->regmap, S2801X_REG_00_SOFT_RSTB,
			BIT(SOFT_RSTB_SYS_RSTB_SHIFT));

	regmap_write(s2801x->regmap, S2801X_REG_00_SOFT_RSTB,
			BIT(SOFT_RSTB_DATA_RSTB_SHIFT) |
			BIT(SOFT_RSTB_SYS_RSTB_SHIFT));
}

static int s2801x_init_mixer(void)
{
	/**
	 * Set default configuration for AP/CP/BT interfaces
	 *
	 * BCLK = 32fs
	 * LRCLK polarity normal
	 * I2S data format is in I2S standard
	 * I2S data length is 16 bits per sample
	 */
	regmap_write(s2801x->regmap, S2801X_REG_02_IN1_CTL2,
			I2S_XFS_32FS << INCTL2_I2S_XFS_SHIFT |
			LRCLK_POL_LEFT << INCTL2_LRCK_POL_SHIFT |
			I2S_DF_I2S << INCTL2_I2S_DF_SHIFT |
			I2S_DL_16BIT << INCTL2_I2S_DL_SHIFT);

	regmap_write(s2801x->regmap, S2801X_REG_05_IN2_CTL2,
			I2S_XFS_32FS << INCTL2_I2S_XFS_SHIFT |
			LRCLK_POL_LEFT << INCTL2_LRCK_POL_SHIFT |
			I2S_DF_I2S << INCTL2_I2S_DF_SHIFT |
			I2S_DL_16BIT << INCTL2_I2S_DL_SHIFT);

	regmap_write(s2801x->regmap, S2801X_REG_08_IN3_CTL2,
			I2S_XFS_32FS << INCTL2_I2S_XFS_SHIFT |
			LRCLK_POL_LEFT << INCTL2_LRCK_POL_SHIFT |
			I2S_DF_I2S << INCTL2_I2S_DF_SHIFT |
			I2S_DL_16BIT << INCTL2_I2S_DL_SHIFT);

	/* Enable MCKO, BCK4 output is MCKO */
	regmap_write(s2801x->regmap, S2801X_REG_0A_HQ_CTL,
			BIT(HQ_CTL_MCKO_EN_SHIFT) |
			BIT(HQ_CTL_BCK4_MODE_SHIFT));

	/* Enable digital mixer */
	regmap_write(s2801x->regmap, S2801X_REG_0F_DIG_EN,
			BIT(DIG_EN_MIX_EN_SHIFT));

	/* Reset DATA path */
	s2801x_reset_data();

	return 0;
}

int s2801x_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			int bfs, int interface)
{
	int xfs, dl_bit;
	int ret;

	dev_dbg(s2801x->dev, "%s called for aif%d\n", __func__, interface);

	/* Only I2S_DL_16BIT is verified now */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		dl_bit = I2S_DL_24BIT;
		break;

	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		dl_bit = I2S_DL_16BIT;
		break;

	default:
		dev_err(s2801x->dev, "%s: Unsupported format\n", __func__);
		return -EINVAL;
	}

	/* Only I2S_XFS_32FS is verified now */
	switch (bfs) {
	case 32:
		xfs = I2S_XFS_32FS;
		break;

	case 48:
		xfs = I2S_XFS_48FS;
		break;

	case 64:
		xfs = I2S_XFS_64FS;
		break;

	default:
		dev_err(s2801x->dev, "%s: Unsupported bfs (%d)\n",
				__func__, bfs);
		return -EINVAL;
	}

	switch (interface) {
	case 1:
		ret = regmap_update_bits(s2801x->regmap,
			S2801X_REG_02_IN1_CTL2,
			(INCTL2_I2S_XFS_MASK << INCTL2_I2S_XFS_SHIFT) |
			(INCTL2_I2S_DL_MASK << INCTL2_I2S_DL_SHIFT),
			(xfs << INCTL2_I2S_XFS_SHIFT) | dl_bit);

		break;

	case 2:
		ret = regmap_update_bits(s2801x->regmap,
			S2801X_REG_05_IN2_CTL2,
			(INCTL2_I2S_XFS_MASK << INCTL2_I2S_XFS_SHIFT) |
			(INCTL2_I2S_DL_MASK << INCTL2_I2S_DL_SHIFT),
			(xfs << INCTL2_I2S_XFS_SHIFT) | dl_bit);

		break;

	case 3:
		ret = regmap_update_bits(s2801x->regmap,
			S2801X_REG_08_IN3_CTL2,
			(INCTL2_I2S_XFS_MASK << INCTL2_I2S_XFS_SHIFT) |
			(INCTL2_I2S_DL_MASK << INCTL2_I2S_DL_SHIFT),
			(xfs << INCTL2_I2S_XFS_SHIFT) | dl_bit);
		break;

	default:
		dev_err(s2801x->dev, "%s: Unsupported interface (%d)\n",
				__func__, interface);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2801x_hw_params);

/**
 * TLV_DB_SCALE_ITEM
 *
 * (TLV: Threshold Limit Value)
 *
 * For various properties, the dB values don't change linearly with respect to
 * the digital value of related bit-field. At most, they are quasi-linear,
 * that means they are linear for various ranges of digital values. Following
 * table define such ranges of various properties.
 *
 * TLV_DB_RANGE_HEAD(num)
 * num defines the number of linear ranges of dB values.
 *
 * s0, e0, TLV_DB_SCALE_ITEM(min, step, mute),
 * s0: digital start value of this range (inclusive)
 * e0: digital end valeu of this range (inclusive)
 * min: dB value corresponding to s0
 * step: the delta of dB value in this range
 * mute: ?
 *
 * Example:
 *	TLV_DB_RANGE_HEAD(3),
 *	0, 1, TLV_DB_SCALE_ITEM(-2000, 2000, 0),
 *	2, 4, TLV_DB_SCALE_ITEM(1000, 1000, 0),
 *	5, 6, TLV_DB_SCALE_ITEM(3800, 8000, 0),
 *
 * The above code has 3 linear ranges with following digital-dB mapping.
 * (0...6) -> (-2000dB, 0dB, 1000dB, 2000dB, 3000dB, 3800dB, 4600dB),
 *
 * DECLARE_TLV_DB_SCALE
 *
 * This macro is used in case where there is a linear mapping between
 * the digital value and dB value.
 *
 * DECLARE_TLV_DB_SCALE(name, min, step, mute)
 *
 * name: name of this dB scale
 * min: minimum dB value corresponding to digital 0
 * step: the delta of dB value
 * mute: ?
 *
 * NOTE: The information is mostly for user-space consumption, to be viewed
 * alongwith amixer.
 */

/**
 * DONE
 * s2801x_rmix_tlv
 *
 * Range:
 * 0dB, -2.87dB, -6.02dB, -9.28dB, -12.04dB, -14.54dB, -18.06dB, -20.56dB
 *
 * This range is used for following controls
 * RMIX1_LVL, reg(0x0d), shift(0), width(3), invert(1), max(7)
 * RMIX2_LVL, reg(0x0d), shift(4), width(3), invert(1), max(7)
 * MIX1_LVL,  reg(0x10), shift(0), width(3), invert(1), max(7)
 * MIX2_LVL,  reg(0x10), shift(4), width(3), invert(1), max(7)
 * MIX3_LVL,  reg(0x11), shift(0), width(3), invert(1), max(7)
 * MIX4_LVL,  reg(0x11), shift(4), width(3), invert(1), max(7)
 */
static const unsigned int s2801x_mix_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x0, 0x1, TLV_DB_SCALE_ITEM(0, 287, 0),
	0x2, 0x3, TLV_DB_SCALE_ITEM(602, 326, 0),
	0x4, 0x5, TLV_DB_SCALE_ITEM(1204, 250, 0),
	0x6, 0x7, TLV_DB_SCALE_ITEM(1806, 250, 0),
};

/**
 * s2801x_alc_ng_hys_tlv
 *
 * Range: 3dB to 12dB, step 3dB
 *
 * ALC_NG_HYS, reg(0x68), shift(6), width(2), invert(0), max(31)
 */
static const DECLARE_TLV_DB_SCALE(s2801x_alc_ng_hys_tlv, 300, 300, 0);

/**
 * s2801x_alc_max_gain_tlv
 *
 * Range:
 * 0x6c to 0x9c => 0dB to 24dB, step 0.5dB
 *
 * ALC_MAX_GAIN,     reg(0x69), shift(0), width(8), min(0x6c), max(0x9c)
 * ALC_START_GAIN_L, reg(0x71), shift(0), width(8), min(0x6c), max(0x9c)
 * ALC_START_GAIN_R, reg(0x72), shift(0), width(8), min(0x6c), max(0x9c)
 */
static const DECLARE_TLV_DB_SCALE(s2801x_alc_max_gain_tlv, 0, 50, 0);

/**
 * s2801x_alc_min_gain_tlv
 *
 * Range:
 * 0x00 to 0x6c => -54dB to 0dB, step 0.5dB
 *
 * ALC_MIN_GAIN, reg(0x6a), shift(0), width(8), invert(0), max(0x6c)
 */
static const DECLARE_TLV_DB_SCALE(s2801x_alc_min_gain_tlv, -5400, 50, 0);

/**
 * s2801x_alc_lvl_tlv
 *
 * Range: -48dB to 0, step 1.5dB
 *
 * ALC_LVL_L, reg(0x6b), shift(0), width(5), invert(0), max(31)
 * ALC_LVL_R, reg(0x6c), shift(0), width(5), invert(0), max(31)
 */
static const DECLARE_TLV_DB_SCALE(s2801x_alc_lvl_tlv, -4800, 150, 0);


/**
 * s2801x_alc_ng_th_tlv
 *
 * Range: -76.5dB to -30dB, step 1.5dB
 *
 * ALCNGTH, reg(0x70), shift(0), width(5), invert(0), max(31)
 */
static const DECLARE_TLV_DB_SCALE(s2801x_alc_ng_th_tlv, -7650, 150, 0);

/**
 * s2801x_alc_winsel
 *
 * ALC Window-Length Select
 */
static const char *s2801x_alc_winsel_text[] = {
	"600fs", "1200fs", "2400fs", "300fs"
};

static const struct soc_enum s2801x_alc_winsel_enum =
	SOC_ENUM_SINGLE(S2801X_REG_68_ALC_CTL, ALC_CTL_ALC_NG_HYS_SHIFT,
			ARRAY_SIZE(s2801x_alc_winsel_text),
			s2801x_alc_winsel_text);


/**
 * s2801x_mpcm_srate
 *
 * Master PCM sample rate selection
 */
static const char *s2801x_mpcm_master_srate_text[] = {
	"8KHz", "16KHz", "24KHz", "32KHz"
};

static const struct soc_enum s2801x_mpcm_srate1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_01_IN1_CTL1, INCTL1_MPCM_SRATE_SHIFT,
			ARRAY_SIZE(s2801x_mpcm_master_srate_text),
			s2801x_mpcm_master_srate_text);

static const struct soc_enum s2801x_mpcm_srate2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_04_IN2_CTL1, INCTL1_MPCM_SRATE_SHIFT,
			ARRAY_SIZE(s2801x_mpcm_master_srate_text),
			s2801x_mpcm_master_srate_text);

static const struct soc_enum s2801x_mpcm_srate3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_07_IN3_CTL1, INCTL1_MPCM_SRATE_SHIFT,
			ARRAY_SIZE(s2801x_mpcm_master_srate_text),
			s2801x_mpcm_master_srate_text);

/**
 * mpcm_slot_sel
 *
 * Master PCM slot selection
 */
static const char *s2801x_mpcm_slot_text[] = {
	"1 slot", "2 slots", "3 slots", "4 slots"
};

static const struct soc_enum s2801x_mpcm_slot1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_01_IN1_CTL1, INCTL1_MPCM_SLOT_SHIFT,
			ARRAY_SIZE(s2801x_mpcm_slot_text),
			s2801x_mpcm_slot_text);

static const struct soc_enum s2801x_mpcm_slot2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_04_IN2_CTL1, INCTL1_MPCM_SLOT_SHIFT,
			ARRAY_SIZE(s2801x_mpcm_slot_text),
			s2801x_mpcm_slot_text);

static const struct soc_enum s2801x_mpcm_slot3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_07_IN3_CTL1, INCTL1_MPCM_SLOT_SHIFT,
			ARRAY_SIZE(s2801x_mpcm_slot_text),
			s2801x_mpcm_slot_text);

/**
 * bclk_pol
 *
 * Polarity of various bit-clocks
 */
static const char *s2801x_clock_pol_text[] = {
	"Normal", "Inverted"
};

static const struct soc_enum s2801x_bck_pol1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_01_IN1_CTL1, INCTL1_BCK_POL_SHIFT,
			ARRAY_SIZE(s2801x_clock_pol_text),
			s2801x_clock_pol_text);

static const struct soc_enum s2801x_bck_pol2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_04_IN2_CTL1, INCTL1_BCK_POL_SHIFT,
			ARRAY_SIZE(s2801x_clock_pol_text),
			s2801x_clock_pol_text);

static const struct soc_enum s2801x_bck_pol3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_07_IN3_CTL1, INCTL1_BCK_POL_SHIFT,
			ARRAY_SIZE(s2801x_clock_pol_text),
			s2801x_clock_pol_text);

static const struct soc_enum s2801x_lrck_pol1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_02_IN1_CTL2, INCTL2_LRCK_POL_SHIFT,
			ARRAY_SIZE(s2801x_clock_pol_text),
			s2801x_clock_pol_text);

static const struct soc_enum s2801x_lrck_pol2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_05_IN2_CTL2, INCTL2_LRCK_POL_SHIFT,
			ARRAY_SIZE(s2801x_clock_pol_text),
			s2801x_clock_pol_text);

static const struct soc_enum s2801x_lrck_pol3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_08_IN3_CTL2, INCTL2_LRCK_POL_SHIFT,
			ARRAY_SIZE(s2801x_clock_pol_text),
			s2801x_clock_pol_text);

/**
 * i2s_pcm
 *
 * Input Audio Mode
 */
static const char *s2801x_i2s_pcm_text[] = {
	"I2S", "PCM"
};

static const struct soc_enum s2801x_i2s_pcm1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_01_IN1_CTL1, INCTL1_I2S_PCM_SHIFT,
			ARRAY_SIZE(s2801x_i2s_pcm_text),
			s2801x_i2s_pcm_text);

static const struct soc_enum s2801x_i2s_pcm2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_04_IN2_CTL1, INCTL1_I2S_PCM_SHIFT,
			ARRAY_SIZE(s2801x_i2s_pcm_text),
			s2801x_i2s_pcm_text);

static const struct soc_enum s2801x_i2s_pcm3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_07_IN3_CTL1, INCTL1_I2S_PCM_SHIFT,
			ARRAY_SIZE(s2801x_i2s_pcm_text),
			s2801x_i2s_pcm_text);

/**
 * i2s_xfs
 *
 * BCK vs LRCK condition
 */
static const char *s2801x_i2s_xfs_text[] = {
	"32fs", "48fs", "64fs", "64fs"
};

static const struct soc_enum s2801x_i2s_xfs1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_02_IN1_CTL2, INCTL2_I2S_XFS_SHIFT,
			ARRAY_SIZE(s2801x_i2s_xfs_text),
			s2801x_i2s_xfs_text);

static const struct soc_enum s2801x_i2s_xfs2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_05_IN2_CTL2, INCTL2_I2S_XFS_SHIFT,
			ARRAY_SIZE(s2801x_i2s_xfs_text),
			s2801x_i2s_xfs_text);

static const struct soc_enum s2801x_i2s_xfs3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_08_IN3_CTL2, INCTL2_I2S_XFS_SHIFT,
			ARRAY_SIZE(s2801x_i2s_xfs_text),
			s2801x_i2s_xfs_text);

/**
 * i2s_df
 *
 * I2S Data Format
 */
static const char *s2801x_i2s_df_text[] = {
	"I2S", "Left-Justified", "Right-Justified", "Invalid"
};

static const struct soc_enum s2801x_i2s_df1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_02_IN1_CTL2, INCTL2_I2S_DF_SHIFT,
			ARRAY_SIZE(s2801x_i2s_df_text),
			s2801x_i2s_df_text);

static const struct soc_enum s2801x_i2s_df2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_05_IN2_CTL2, INCTL2_I2S_DF_SHIFT,
			ARRAY_SIZE(s2801x_i2s_df_text),
			s2801x_i2s_df_text);

static const struct soc_enum s2801x_i2s_df3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_08_IN3_CTL2, INCTL2_I2S_DF_SHIFT,
			ARRAY_SIZE(s2801x_i2s_df_text),
			s2801x_i2s_df_text);

/**
 * i2s_dl
 *
 * I2S Data Length
 */
static const char *s2801x_i2s_dl_text[] = {
	"16-bit", "18-bit", "20-bit", "24-bit"
};

static const struct soc_enum s2801x_i2s_dl1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_02_IN1_CTL2, INCTL2_I2S_DL_SHIFT,
			ARRAY_SIZE(s2801x_i2s_dl_text),
			s2801x_i2s_dl_text);

static const struct soc_enum s2801x_i2s_dl2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_05_IN2_CTL2, INCTL2_I2S_DL_SHIFT,
			ARRAY_SIZE(s2801x_i2s_dl_text),
			s2801x_i2s_dl_text);

static const struct soc_enum s2801x_i2s_dl3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_08_IN3_CTL2, INCTL2_I2S_DL_SHIFT,
			ARRAY_SIZE(s2801x_i2s_dl_text),
			s2801x_i2s_dl_text);

/**
 * pcm_dad
 *
 * PCM Data Additional Delay
 */
static const char *s2801x_pcm_dad_text[] = {
	"1 bck", "0 bck", "2 bck", "", "3 bck", "", "4 bck", ""
};

static const struct soc_enum s2801x_pcm_dad1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_03_IN1_CTL3, INCTL3_PCM_DAD_SHIFT,
			ARRAY_SIZE(s2801x_pcm_dad_text),
			s2801x_pcm_dad_text);

static const struct soc_enum s2801x_pcm_dad2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_06_IN2_CTL3, INCTL3_PCM_DAD_SHIFT,
			ARRAY_SIZE(s2801x_pcm_dad_text),
			s2801x_pcm_dad_text);

static const struct soc_enum s2801x_pcm_dad3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_09_IN3_CTL3, INCTL3_PCM_DAD_SHIFT,
			ARRAY_SIZE(s2801x_pcm_dad_text),
			s2801x_pcm_dad_text);

/**
 * pcm_df
 *
 * PCM Data Format
 */
static const char *s2801x_pcm_df_text[] = {
	"", "", "", "", "Short Frame", "", "", "",
	"", "", "", "", "Long Frame"
};

static const struct soc_enum s2801x_pcm_df1_enum =
	SOC_ENUM_SINGLE(S2801X_REG_03_IN1_CTL3, INCTL3_PCM_DF_SHIFT,
			ARRAY_SIZE(s2801x_pcm_df_text),
			s2801x_pcm_dad_text);

static const struct soc_enum s2801x_pcm_df2_enum =
	SOC_ENUM_SINGLE(S2801X_REG_06_IN2_CTL3, INCTL3_PCM_DF_SHIFT,
			ARRAY_SIZE(s2801x_pcm_df_text),
			s2801x_pcm_dad_text);

static const struct soc_enum s2801x_pcm_df3_enum =
	SOC_ENUM_SINGLE(S2801X_REG_09_IN3_CTL3, INCTL3_PCM_DF_SHIFT,
			ARRAY_SIZE(s2801x_pcm_df_text),
			s2801x_pcm_dad_text);

/**
 * bck4_mode
 *
 * BCK4 Output Selection
 */
static const char *s2801x_bck4_mode_text[] = {
	"Normal BCK", "MCKO"
};

static const struct soc_enum s2801x_bck4_mode_enum =
	SOC_ENUM_SINGLE(S2801X_REG_0A_HQ_CTL, HQ_CTL_BCK4_MODE_SHIFT,
			ARRAY_SIZE(s2801x_bck4_mode_text),
			s2801x_bck4_mode_text);

/**
 * dout_sel1
 *
 * CH1 Digital Output Selection
 */
static const char *s2801x_dout_sel1_text[] = {
	"DMIX_OUT", "AIF4IN", "RMIX_OUT"
};

static SOC_ENUM_SINGLE_DECL(s2801x_dout_sel1_enum, S2801X_REG_16_DOUTMX1,
		DOUTMX1_DOUT_SEL1_SHIFT, s2801x_dout_sel1_text);

/**
 * dout_sel2
 *
 * CH2 Digital Output Selection
 */
static const char *s2801x_dout_sel2_text[] = {
	"DMIX_OUT", "AIF4IN", "AIF3IN"
};

static SOC_ENUM_SINGLE_DECL(s2801x_dout_sel2_enum, S2801X_REG_16_DOUTMX1,
		DOUTMX1_DOUT_SEL2_SHIFT, s2801x_dout_sel2_text);

/**
 * dout_sel3
 *
 * CH3 Digital Output Selection
 */
static const char *s2801x_dout_sel3_text[] = {
	"DMIX_OUT", "AIF4IN", "AIF2IN"
};

static SOC_ENUM_SINGLE_DECL(s2801x_dout_sel3_enum, S2801X_REG_17_DOUTMX2,
		DOUTMX2_DOUT_SEL3_SHIFT, s2801x_dout_sel3_text);


static const char *s2801x_off_on_text[] = {
	"Off", "On"
};

static const struct soc_enum s2801x_hq_en_enum =
SOC_ENUM_SINGLE(S2801X_REG_0A_HQ_CTL, HQ_CTL_HQ_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);

static const struct soc_enum s2801x_ch3_rec_en_enum =
SOC_ENUM_SINGLE(S2801X_REG_0A_HQ_CTL, HQ_CTL_CH3_SEL_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);

static const struct soc_enum s2801x_mcko_en_enum =
SOC_ENUM_SINGLE(S2801X_REG_0A_HQ_CTL, HQ_CTL_MCKO_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);

static const struct soc_enum s2801x_rmix1_en_enum =
SOC_ENUM_SINGLE(S2801X_REG_0D_RMIX_CTL, RMIX_CTL_RMIX1_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);

static const struct soc_enum s2801x_rmix2_en_enum =
SOC_ENUM_SINGLE(S2801X_REG_0D_RMIX_CTL, RMIX_CTL_RMIX2_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);

static const struct soc_enum mixer_ch1_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_10_DMIX1, DMIX1_MIX_EN1_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);
static const struct soc_enum mixer_ch2_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_10_DMIX1, DMIX1_MIX_EN2_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);
static const struct soc_enum mixer_ch3_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_11_DMIX2, DMIX2_MIX_EN3_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);
static const struct soc_enum mixer_ch4_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_11_DMIX2, DMIX2_MIX_EN4_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);

static const struct soc_enum mixer_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_0F_DIG_EN, DIG_EN_MIX_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);
static const struct soc_enum src3_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_0F_DIG_EN, DIG_EN_SRC3_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);
static const struct soc_enum src2_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_0F_DIG_EN, DIG_EN_SRC2_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);
static const struct soc_enum src1_enable_enum =
SOC_ENUM_SINGLE(S2801X_REG_0F_DIG_EN, DIG_EN_SRC1_EN_SHIFT,
		ARRAY_SIZE(s2801x_off_on_text), s2801x_off_on_text);

/**
 * struct snd_kcontrol_new s2801x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/**
 * All the data goes into s2801x_snd_controls.
 */
static const struct snd_kcontrol_new s2801x_snd_controls[] = {
	SOC_SINGLE_TLV("RMIX1_LVL", S2801X_REG_0D_RMIX_CTL,
			RMIX_CTL_RMIX2_LVL_SHIFT,
			BIT(RMIX_CTL_RMIX2_LVL_WIDTH) - 1, 0,
			s2801x_mix_tlv),

	SOC_SINGLE_TLV("RMIX2_LVL", S2801X_REG_0D_RMIX_CTL,
			RMIX_CTL_RMIX1_LVL_SHIFT,
			BIT(RMIX_CTL_RMIX1_LVL_WIDTH) - 1, 0,
			s2801x_mix_tlv),

	SOC_SINGLE_TLV("MIX1_LVL", S2801X_REG_10_DMIX1,
			DMIX1_MIX_LVL1_SHIFT,
			BIT(DMIX1_MIX_LVL1_WIDTH) - 1, 0,
			s2801x_mix_tlv),

	SOC_SINGLE_TLV("MIX2_LVL", S2801X_REG_10_DMIX1,
			DMIX1_MIX_LVL2_SHIFT,
			BIT(DMIX1_MIX_LVL2_WIDTH) - 1, 0,
			s2801x_mix_tlv),

	SOC_SINGLE_TLV("MIX3_LVL", S2801X_REG_11_DMIX2,
			DMIX2_MIX_LVL3_SHIFT,
			BIT(DMIX2_MIX_LVL3_WIDTH) - 1, 0,
			s2801x_mix_tlv),

	SOC_SINGLE_TLV("MIX4_LVL", S2801X_REG_11_DMIX2,
			DMIX2_MIX_LVL4_SHIFT,
			BIT(DMIX2_MIX_LVL4_WIDTH) - 1, 0,
			s2801x_mix_tlv),

	SOC_SINGLE_TLV("ALC NG HYS", S2801X_REG_68_ALC_CTL,
			ALC_CTL_ALC_NG_HYS_SHIFT,
			BIT(ALC_CTL_ALC_NG_HYS_WIDTH) - 1, 0,
			s2801x_alc_ng_hys_tlv),

	SOC_SINGLE_RANGE_TLV("ALC Max Gain", S2801X_REG_69_ALC_GA1,
			ALC_GA1_ALC_MAX_GAIN_SHIFT,
			ALC_GA1_ALC_MAX_GAIN_MINVAL,
			ALC_GA1_ALC_MAX_GAIN_MAXVAL, 0,
			s2801x_alc_max_gain_tlv),

	SOC_SINGLE_RANGE_TLV("ALC Min Gain", S2801X_REG_6A_ALC_GA2,
			ALC_GA2_ALC_MIN_GAIN_SHIFT,
			ALC_GA2_ALC_MIN_GAIN_MINVAL,
			ALC_GA2_ALC_MIN_GAIN_MAXVAL, 0,
			s2801x_alc_min_gain_tlv),

	SOC_SINGLE_RANGE_TLV("ALC Start Gain Left", S2801X_REG_71_ALC_SGL,
			ALC_SGL_START_GAIN_L_SHIFT,
			ALC_SGL_START_GAIN_L_MINVAL,
			ALC_SGL_START_GAIN_L_MAXVAL, 0,
			s2801x_alc_max_gain_tlv),

	SOC_SINGLE_RANGE_TLV("ALC Start Gain Right", S2801X_REG_72_ALC_SGR,
			S2801X_REG_72_ALC_SGR,
			ALC_SGR_START_GAIN_R_MINVAL,
			ALC_SGR_START_GAIN_R_MAXVAL, 0,
			s2801x_alc_max_gain_tlv),

	SOC_SINGLE_TLV("ALC LVL Left", S2801X_REG_6B_ALC_LVL,
			ALC_LVL_LVL_SHIFT,
			BIT(ALC_LVL_LVL_WIDTH) - 1, 0,
			s2801x_alc_lvl_tlv),

	SOC_SINGLE_TLV("ALC LVL Right", S2801X_REG_6C_ALC_LVR,
			ALC_LVR_LVL_SHIFT,
			BIT(ALC_LVR_LVL_WIDTH) - 1, 0,
			s2801x_alc_lvl_tlv),

	SOC_SINGLE_TLV("ALC Noise Gain Threshold", S2801X_REG_70_ALC_NG,
			ALC_NG_ALCNGTH_SHIFT,
			BIT(ALC_NG_ALCNGTH_WIDTH) - 1, 0,
			s2801x_alc_ng_th_tlv),

	SOC_ENUM("ALC Window Length", s2801x_alc_winsel_enum),

	SOC_ENUM("CH1 Master PCM Sample Rate", s2801x_mpcm_srate1_enum),
	SOC_ENUM("CH2 Master PCM Sample Rate", s2801x_mpcm_srate2_enum),
	SOC_ENUM("CH3 Master PCM Sample Rate", s2801x_mpcm_srate3_enum),

	SOC_ENUM("CH1 Master PCM Slot", s2801x_mpcm_slot1_enum),
	SOC_ENUM("CH2 Master PCM Slot", s2801x_mpcm_slot2_enum),
	SOC_ENUM("CH3 Master PCM Slot", s2801x_mpcm_slot3_enum),

	SOC_ENUM("CH1 BCLK Polarity", s2801x_bck_pol1_enum),
	SOC_ENUM("CH2 BCLK Polarity", s2801x_bck_pol2_enum),
	SOC_ENUM("CH3 BCLK Polarity", s2801x_bck_pol3_enum),

	SOC_ENUM("CH1 LRCLK Polarity", s2801x_lrck_pol1_enum),
	SOC_ENUM("CH2 LRCLK Polarity", s2801x_lrck_pol2_enum),
	SOC_ENUM("CH3 LRCLK Polarity", s2801x_lrck_pol3_enum),

	SOC_ENUM("CH1 Input Audio Mode", s2801x_i2s_pcm1_enum),
	SOC_ENUM("CH2 Input Audio Mode", s2801x_i2s_pcm2_enum),
	SOC_ENUM("CH3 Input Audio Mode", s2801x_i2s_pcm3_enum),

	SOC_ENUM("CH1 XFS", s2801x_i2s_xfs1_enum),
	SOC_ENUM("CH2 XFS", s2801x_i2s_xfs2_enum),
	SOC_ENUM("CH3 XFS", s2801x_i2s_xfs3_enum),

	SOC_ENUM("CH1 I2S Format", s2801x_i2s_df1_enum),
	SOC_ENUM("CH2 I2S Format", s2801x_i2s_df2_enum),
	SOC_ENUM("CH3 I2S Format", s2801x_i2s_df3_enum),

	SOC_ENUM("CH1 I2S Data Length", s2801x_i2s_dl1_enum),
	SOC_ENUM("CH2 I2S Data Length", s2801x_i2s_dl2_enum),
	SOC_ENUM("CH3 I2S Data Length", s2801x_i2s_dl3_enum),

	SOC_ENUM("CH1 PCM DAD", s2801x_pcm_dad1_enum),
	SOC_ENUM("CH2 PCM DAD", s2801x_pcm_dad2_enum),
	SOC_ENUM("CH3 PCM DAD", s2801x_pcm_dad3_enum),

	SOC_ENUM("CH1 PCM Data Format", s2801x_pcm_df1_enum),
	SOC_ENUM("CH2 PCM Data Format", s2801x_pcm_df2_enum),
	SOC_ENUM("CH3 PCM Data Format", s2801x_pcm_df3_enum),

	SOC_ENUM("CH3 Rec En", s2801x_ch3_rec_en_enum),
	SOC_ENUM("MCKO En", s2801x_mcko_en_enum),

	SOC_ENUM("RMIX1 En", s2801x_rmix1_en_enum),
	SOC_ENUM("RMIX2 En", s2801x_rmix2_en_enum),

	SOC_ENUM("BCK4 Output Selection", s2801x_bck4_mode_enum),

	SOC_ENUM("HQ En", s2801x_hq_en_enum),

	SOC_ENUM("CH1 DOUT Select", s2801x_dout_sel1_enum),
	SOC_ENUM("CH2 DOUT Select", s2801x_dout_sel2_enum),
	SOC_ENUM("CH3 DOUT Select", s2801x_dout_sel3_enum),

	SOC_ENUM("CH1 Mixer En", mixer_ch1_enable_enum),
	SOC_ENUM("CH2 Mixer En", mixer_ch2_enable_enum),
	SOC_ENUM("CH3 Mixer En", mixer_ch3_enable_enum),
	SOC_ENUM("CH4 Mixer En", mixer_ch4_enable_enum),
	SOC_ENUM("Mixer En", mixer_enable_enum),
	SOC_ENUM("SRC1 En", src1_enable_enum),
	SOC_ENUM("SRC2 En", src2_enable_enum),
	SOC_ENUM("SRC3 En", src3_enable_enum),
};

int s2801x_aif1_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	return 0;
}

/**
 * s2801_get_clk_mixer
 *
 * This function gets all the clock related to the audio i2smixer and stores in
 * mixer private structure.
 */
static int s2801_get_clk_mixer(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	s2801x->sclk_audmixer = clk_get(dev, "audmixer_sysclk");
	if (IS_ERR(s2801x->sclk_audmixer)) {
		dev_err(dev, "audmixer_sysclk clk not found\n");
		goto err0;
	}

	s2801x->sclk_audmixer_bclk0 = clk_get(dev, "audmixer_bclk0");
	if (IS_ERR(s2801x->sclk_audmixer_bclk0)) {
		dev_err(dev, "audmixer bclk0 clk not found\n");
		goto err1;
	}

	s2801x->sclk_audmixer_bclk1 = clk_get(dev, "audmixer_bclk1");
	if (IS_ERR(s2801x->sclk_audmixer_bclk1)) {
		dev_err(dev, "audmixer bclk1 clk not found\n");
		goto err2;
	}

	s2801x->sclk_audmixer_bclk2 = clk_get(dev, "audmixer_bclk2");
	if (IS_ERR(s2801x->sclk_audmixer_bclk2)) {
		dev_err(dev, "audmixer bclk2 clk not found\n");
		goto err3;
	}

	s2801x->sclk_mi2s_bclk = clk_get(dev, "mi2s_bclk");
	if (IS_ERR(s2801x->sclk_mi2s_bclk)) {
		dev_err(dev, "mi2s bclk clk not found\n");
		goto err4;
	}

	s2801x->dout_audmixer = clk_get(dev, "audmixer_dout");
	if (IS_ERR(s2801x->dout_audmixer)) {
		dev_err(dev, "audmixer_dout clk not found\n");
		goto err5;
	}

	clk_set_rate(s2801x->dout_audmixer, S2801_SYS_CLK_FREQ);

	return 0;

err5:
	clk_put(s2801x->sclk_mi2s_bclk);
err4:
	clk_put(s2801x->sclk_audmixer_bclk2);
err3:
	clk_put(s2801x->sclk_audmixer_bclk1);
err2:
	clk_put(s2801x->sclk_audmixer_bclk0);
err1:
	clk_put(s2801x->sclk_audmixer);
err0:
	return -1;
}

/**
 * s2801x_clk_put_all
 *
 * This function puts all the clock related to the audio i2smixer
 */
static void s2801x_clk_put_all(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	clk_put(s2801x->sclk_mi2s_bclk);
	clk_put(s2801x->sclk_audmixer_bclk2);
	clk_put(s2801x->sclk_audmixer_bclk1);
	clk_put(s2801x->sclk_audmixer_bclk0);
	clk_put(s2801x->sclk_audmixer);
}

/**
 * s2801x_clk_enable
 *
 * This function enables all the clock related to the audio i2smixer
 */
static void s2801x_clk_enable(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	clk_prepare_enable(s2801x->sclk_audmixer);
	clk_prepare_enable(s2801x->sclk_audmixer_bclk0);
	clk_prepare_enable(s2801x->sclk_audmixer_bclk1);
	clk_prepare_enable(s2801x->sclk_audmixer_bclk2);
	clk_prepare_enable(s2801x->sclk_mi2s_bclk);
	clk_set_rate(s2801x->sclk_audmixer, S2801_SYS_CLK_FREQ);
}

/**
 * s2801x_clk_disable
 *
 * This function disable all the clock related to the audio i2smixer
 */
static void s2801x_clk_disable(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	clk_disable_unprepare(s2801x->sclk_audmixer_bclk0);
	clk_disable_unprepare(s2801x->sclk_audmixer_bclk1);
	clk_disable_unprepare(s2801x->sclk_audmixer_bclk2);
	clk_disable_unprepare(s2801x->sclk_mi2s_bclk);
	clk_disable_unprepare(s2801x->sclk_audmixer);
}

/**
 * s2801x_power_on
 *
 * This function does the power on configurations for audio i2smixer
 */
static int s2801x_power_on(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);
	int ret;
	unsigned int sysreg;

	ret = of_property_read_u32(dev->of_node,
				"sysreg-reset", &sysreg);
	if (ret) {
		dev_err(dev, "Property 'sysreg-reset' not found\n");
		return -ENOENT;
	}

	s2801x->sysreg_reset = devm_ioremap(dev, sysreg, 0x4);
	if (s2801x->sysreg_reset == NULL) {
		dev_err(dev, "Cannot ioremap %x\n", sysreg);
		return -ENXIO;
	}

	ret = of_property_read_u32(dev->of_node,
				"sysreg-i2c", &sysreg);
	if (ret) {
		dev_err(dev, "Property 'sysreg-i2c' not found\n");
		return -ENOENT;
	}

	s2801x->sysreg_i2c_id = devm_ioremap(dev, sysreg, 0x4);
	if (s2801x->sysreg_i2c_id == NULL) {
		dev_err(dev, "Cannot ioremap %x\n", sysreg);
		return -ENXIO;
	}

	/* Audio mixer Alive Configuration */
	__raw_writel(S2801_ALIVE_ON, EXYNOS_PMU_AUD_PATH_CFG);

	/* Audio mixer unreset */
	if (s2801x->sysreg_reset == NULL) {
		dev_err(dev, "sysreg_reset registers not set\n");
		return -ENXIO;
	}
	writel(S2801_SW_UN_RESET, s2801x->sysreg_reset);

	/*write Audio mixer i2c address */
	if (s2801x->sysreg_i2c_id == NULL) {
		dev_err(dev, "sysreg_i2c_id registers not set\n");
		return -ENXIO;
	}
	writel(s2801x->i2c_addr, s2801x->sysreg_i2c_id);

	return 0;
}

/**
 * s2801x_power_off
 *
 * This function does the power off alive configurations for audio i2smixer
 */
static void s2801x_power_off(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	dev_dbg(dev, "%s called\n", __func__);

	if (s2801x->sysreg_reset != NULL)
		writel(S2801_SW_RESET, s2801x->sysreg_reset);

	/* Audio mixer Alive Configuration off */
	__raw_writel(S2801_ALIVE_OFF, EXYNOS_PMU_AUD_PATH_CFG);
}

#define s2801x_RATES SNDRV_PCM_RATE_8000_96000
#define s2801x_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver s2801x_dai = {
	.name = "HiFi",
	.playback = {
		.stream_name = "Primary",
		.channels_min = 2,
		.channels_max = 2,
		.rates = s2801x_RATES,
		.formats = s2801x_FORMATS,
	},
};

static int s2801x_probe(struct snd_soc_codec *codec)
{
	int ret = 0;

	dev_dbg(codec->dev, "(*) %s\n", __func__);

	s2801x->codec = codec;

	codec->control_data = s2801x->regmap;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	ret = s2801x_power_on(codec->dev);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to power on mixer\n");
		return ret;
	}

	/* Set Clock for Mixer */
	ret = s2801_get_clk_mixer(codec->dev);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to get clk for mixer\n");
		return ret;
	}

	s2801x_clk_enable(codec->dev);

	/* Reset the codec */
	s2801x_reset_sys_data();

	/* set ap path by defaut*/
	s2801x_init_mixer();

	return ret;
}

static int s2801x_remove(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "(*) %s\n", __func__);

	s2801x_clk_disable(codec->dev);
	s2801x_clk_put_all(codec->dev);
	s2801x_power_off(codec->dev);

	return 0;
}

static int s2801x_suspend(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "(*) %s\n", __func__);

	return 0;
}

static int s2801x_resume(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "(*) %s\n", __func__);

	return 0;
}

static int s2801x_set_bias_level(struct snd_soc_codec *codec,
					enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		dev_dbg(codec->dev, "(*) %s: level - ON\n", __func__);
		/*
		 * DATA reset needs to be called after all configuration for the
		 * mixer is done
		 */
		s2801x_reset_data();
		break;

	case SND_SOC_BIAS_PREPARE:
		dev_dbg(codec->dev, "(*) %s: level - PREPARE\n", __func__);
		break;

	case SND_SOC_BIAS_STANDBY:
		dev_dbg(codec->dev, "(*) %s: level - STANDBY\n", __func__);
		break;

	case SND_SOC_BIAS_OFF:
		dev_dbg(codec->dev, "(*) %s: level - OFF\n", __func__);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_s2801x = {
	.probe = s2801x_probe,
	.remove = s2801x_remove,
	.suspend = s2801x_suspend,
	.resume = s2801x_resume,
	.set_bias_level = s2801x_set_bias_level,
	.controls = s2801x_snd_controls,
	.num_controls = ARRAY_SIZE(s2801x_snd_controls),
};

static int s2801x_i2c_probe(struct i2c_client *i2c,
				 const struct i2c_device_id *id)
{
	int ret;

	dev_dbg(&i2c->dev, "%s called\n", __func__);

	s2801x = devm_kzalloc(&i2c->dev,
			sizeof(struct s2801x_priv), GFP_KERNEL);
	if (s2801x == NULL)
		return -ENOMEM;

	s2801x->dev = &i2c->dev;
	s2801x->i2c_addr = i2c->addr;

	s2801x->regmap = devm_regmap_init_i2c(i2c, &s2801x_regmap);
	if (IS_ERR(s2801x->regmap)) {
		ret = PTR_ERR(s2801x->regmap);
		dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
		return ret;
	}
	i2c_set_clientdata(i2c, s2801x);

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_s2801x, &s2801x_dai, 1);
	if (ret < 0)
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);

	return ret;
}

static int s2801x_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static int s2801x_sys_suspend(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);
	regcache_cache_only(s2801x->regmap, true);

	return 0;
}

static int s2801x_sys_resume(struct device *dev)
{
	dev_dbg(dev, "(*) %s\n", __func__);

	/* Audio mixer Alive Configuration */
	__raw_writel(S2801_ALIVE_ON, EXYNOS_PMU_AUD_PATH_CFG);

	/* Audio mixer unreset */
	if (s2801x->sysreg_reset == NULL) {
		dev_err(dev, "sysreg_reset registers not set\n");
		return -ENXIO;
	}
	writel(S2801_SW_UN_RESET, s2801x->sysreg_reset);

	/*write Audio mixer i2c address */
	if (s2801x->sysreg_i2c_id == NULL) {
		dev_err(dev, "sysreg_i2c_id registers not set\n");
		return -ENXIO;
	}
	writel(s2801x->i2c_addr, s2801x->sysreg_i2c_id);

	/* Reset the codec */
	s2801x_reset_sys_data();

	/* set ap path by defaut*/
	s2801x_init_mixer();

	regcache_cache_only(s2801x->regmap, false);
	regcache_sync(s2801x->regmap);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int s2801x_runtime_resume(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	regcache_cache_only(s2801x->regmap, false);

	regcache_sync(s2801x->regmap);

	return 0;
}

static int s2801x_runtime_suspend(struct device *dev)
{
	struct s2801x_priv *s2801x = dev_get_drvdata(dev);

	regcache_cache_only(s2801x->regmap, true);

	return 0;
}
#endif

static const struct dev_pm_ops s2801x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			s2801x_sys_suspend,
			s2801x_sys_resume
	)
	SET_RUNTIME_PM_OPS(
			s2801x_runtime_suspend,
			s2801x_runtime_resume,
			NULL
	)
};

static const struct i2c_device_id s2801x_i2c_id[] = {
	{ "s2801x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2801x_i2c_id);

static const struct of_device_id s2801x_dt_ids[] = {
	{ .compatible = "samsung,s2801x", },
	{ }
};
MODULE_DEVICE_TABLE(of, s2801x_dt_ids);

static struct i2c_driver s2801x_i2c_driver = {
	.driver = {
		.name = "s2801x",
		.owner = THIS_MODULE,
		.pm = &s2801x_pm,
		.of_match_table = of_match_ptr(s2801x_dt_ids),
	},
	.probe = s2801x_i2c_probe,
	.remove = s2801x_i2c_remove,
	.id_table = s2801x_i2c_id,
};

module_i2c_driver(s2801x_i2c_driver);
MODULE_DESCRIPTION("ASoC SRC2801X driver");
MODULE_AUTHOR("Tushar Behera <tushar.b@samsung.com>");
MODULE_AUTHOR("Sayanta Pattanayak <sayanta.p@samsung.com>");
MODULE_AUTHOR("R Chandrasekar <rcsekar@samsung.com>");
MODULE_LICENSE("GPL v2");
