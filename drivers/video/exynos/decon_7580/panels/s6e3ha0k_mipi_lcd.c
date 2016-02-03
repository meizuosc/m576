/* drivers/video/decon_display/s6e3ha0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/lcd.h>

#include <video/mipi_display.h>

#include "dynamic_aid_s6e3ha0k.h"
#include "s6e3ha0k_param.h"
#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "../dsim.h"

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)

#define NORMAL_TEMPERATURE		25	/* 25 degrees Celsius*/

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		134

#define MIN_GAMMA			2
#define MAX_GAMMA			350

#define DEFAULT_GAMMA_INDEX		IBRIGHTNESS_183NT

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_ID2_REG			0xD6
#define LDI_ID2_LEN			5
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			87	/* MTP + HBM */
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			(ELVSS_PARAM_SIZE - 1)
#define LDI_TSET_REG			0xB8
#define LDI_TSET_LEN			5
#define TSET_PARAM_SIZE		(LDI_TSET_LEN + 1)

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

static const unsigned int DIM_TABLE[IBRIGHTNESS_MAX] = {
	2,	3,	4,	5,	6,
	7,	8,	9,	10,	11,
	12,	13,	14,	15,	16,
	17,	19,	20,	21,	22,
	24,	25,	27,	29,	30,
	32,	34,	37,	39,	41,
	44,	47,	50,	53,	56,
	60,	64,	68,	72,	77,
	82,	87,	93,	98,	105,
	111,	119,	126,	134,	143,
	152,	162,	172,	183,	195,
	207,	220,	234,	249,	265,
	282,	300,	316,	333,	350,
	500
};

static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {
	SEQ_ACL_OFF,
	SEQ_ACL_15,
};

static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {
	SEQ_ACL_OFF_OPR_AVR,
	SEQ_ACL_ON_OPR_AVR,
};

union elvss_info {
	u32 value;
	struct {
		u8 mps;
		u8 offset;
		u8 hbm;
		u8 reserved;
	};
};

struct lcd_drvdata {
	unsigned int		bl;
	unsigned int		auto_brightness;
	unsigned int		acl_enable;
	unsigned int		siop_enable;
	unsigned int		current_acl;
	unsigned int		current_bl;
	union elvss_info		current_elvss;
	unsigned int		ldi_enable;
	unsigned int		power;
	struct mutex		lock;
	struct mutex		bl_lock;

	struct device		*dev;
	struct lcd_device		*ld;
	struct backlight_device	*bd;
	unsigned char		id[LDI_ID_LEN];
	unsigned char		ddi_id[LDI_ID2_LEN];
	unsigned char		**gamma_table;
	unsigned char		**elvss_table[ACL_STATUS_MAX][TEMP_MAX];
	struct dynamic_aid_param_t	daid;
	unsigned char	aor[IBRIGHTNESS_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int		connected;

	unsigned char		tset_table[TSET_PARAM_SIZE];
	int			temperature;
	unsigned int		coordinate[2];
	unsigned int		partial_range[2];
	unsigned char		date[2];

	unsigned int		width;
	unsigned int		height;

	struct dsim_device		*dsim;
};

int s6e3ha0_write(struct lcd_drvdata *lcd, const u8 *seq, u32 len)
{
	int ret = 0;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	retry = 3;
write_data:
	if (!retry) {
		dsim_err("%s failed: exceed retry count\n", __func__);
		goto write_err;
	}

	if (len > 2)
		ret = dsim_write_data(lcd->dsim,
			MIPI_DSI_DCS_LONG_WRITE, (unsigned long)seq, len);
	else if (len == 2)
		ret = dsim_write_data(lcd->dsim,
			MIPI_DSI_DCS_SHORT_WRITE_PARAM, seq[0], seq[1]);
	else if (len == 1)
		ret = dsim_write_data(lcd->dsim,
			MIPI_DSI_DCS_SHORT_WRITE, seq[0], 0);
	else {
		ret = -EINVAL;
		goto write_err;
	}

	if (ret != 0) {
		dsim_dbg("mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

int s6e3ha0_read(struct lcd_drvdata *lcd, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);
	if (len > 2)
		cmd = MIPI_DSI_DCS_READ;
	else if (len == 2)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
	else {
		ret = -EINVAL;
		goto read_err;
	}
	retry = 5;

read_data:
	if (!retry) {
		dsim_err("%s failed: exceed retry count\n", __func__);
		goto read_err;
	}
	ret = dsim_read_data(lcd->dsim, cmd, addr, len, buf);
	if (ret != len) {
		dsim_err("mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}

read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static int s6e3ha0_read_coordinate(struct lcd_drvdata *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = s6e3ha0_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dsim_err("%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */

	return ret;
}

static int s6e3ha0_read_id(struct lcd_drvdata *lcd, u8 *buf)
{
	int ret;

	ret = s6e3ha0_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);

	if (ret < 1) {
		lcd->connected = 0;
		dsim_info("panel is not connected well\n");
	}

	return ret;
}

static int s6e3ha0_read_ddi_id(struct lcd_drvdata *lcd, u8 *buf)
{
	int ret;

	ret = s6e3ha0_read(lcd, LDI_ID2_REG, buf, LDI_ID2_LEN);

	if (ret < 1)
		dsim_info("%s failed\n", __func__);

	return ret;
}

static int s6e3ha0_read_mtp(struct lcd_drvdata *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3ha0_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

	if (ret < 1)
		dsim_err("%s failed\n", __func__);

	smtd_dbg("%s: %02xh\n", __func__, LDI_MTP_REG);
	for (i = 0; i < LDI_MTP_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	/* manufacture date */
	lcd->date[0] = buf[40];
	lcd->date[1] = buf[41];

	return ret;
}

static int s6e3ha0_read_elvss(struct lcd_drvdata *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3ha0_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_ELVSS_REG);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6e3ha0_read_tset(struct lcd_drvdata *lcd)
{
	int ret, i;

	ret = s6e3ha0_read(lcd, LDI_TSET_REG,
			&lcd->tset_table[1], LDI_TSET_LEN);

	smtd_dbg("%s: %02xh\n", __func__, LDI_TSET_REG);
	for (i = 0; i < LDI_TSET_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i, lcd->tset_table[i]);

	lcd->tset_table[0] = LDI_TSET_REG;

	return ret;
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel = DEFAULT_GAMMA_INDEX;
	int i, gamma;

	gamma = (brightness * MAX_GAMMA) / MAX_BRIGHTNESS;
	for (i = 0; i < IBRIGHTNESS_500NT; i++) {
		if (brightness <= MIN_GAMMA) {
			backlightlevel = 0;
			break;
		}

		if (DIM_TABLE[i] > gamma)
			break;

		backlightlevel = i;
	}

	return backlightlevel;
}

static int s6e3ha0_gamma_ctl(struct lcd_drvdata *lcd)
{
	int ret = 0;

	ret = s6e3ha0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3ha0_aid_parameter_ctl(struct lcd_drvdata *lcd, u8 force)
{
	int ret = 0;

	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][1] !=  lcd->aor[lcd->current_bl][1])
		goto aid_update;
	else if (lcd->aor[lcd->bl][2] !=  lcd->aor[lcd->current_bl][2])
		goto aid_update;
	else
		goto exit;

aid_update:
	ret = s6e3ha0_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);
	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int s6e3ha0_set_acl(struct lcd_drvdata *lcd, u8 force)
{
	int ret = 0, level = ACL_STATUS_15P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e3ha0_write(lcd,
			ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		ret += s6e3ha0_write(lcd,
			ACL_OPR_TABLE[level], OPR_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dsim_info("acl: %d, auto_brightness: %d\n",
			lcd->current_acl, lcd->auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3ha0_set_elvss(struct lcd_drvdata *lcd, u8 force)
{
	int ret = 0, i, elvss_level;
	u32 nit, temperature;
	union elvss_info elvss;

	nit = DIM_TABLE[lcd->bl];
	elvss_level = ELVSS_STATUS_350;
	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		if (nit <= ELVSS_DIM_TABLE[i]) {
			elvss_level = i;
			break;
		}
	}

	temperature =
		(lcd->temperature <= -20) ?
		TEMP_BELOW_MINUS_20_DEGREE :
		TEMP_ABOVE_MINUS_20_DEGREE;
	elvss.mps =
	lcd->elvss_table[lcd->acl_enable][temperature][elvss_level][1];
	elvss.offset =
	lcd->elvss_table[lcd->acl_enable][temperature][elvss_level][2];
	elvss.hbm =
	lcd->elvss_table[lcd->acl_enable][temperature][elvss_level][21];

	if (force)
		goto elvss_update;
	else if (lcd->current_elvss.value != elvss.value)
		goto elvss_update;
	else
		goto exit;

elvss_update:
	ret = s6e3ha0_write(lcd,
		lcd->elvss_table[lcd->acl_enable][temperature][elvss_level],
		ELVSS_PARAM_SIZE);
	lcd->current_elvss.value = elvss.value;

	if (!ret)
		ret = -EPERM;

exit:
	return ret;
}

static int s6e3ha0_set_tset(struct lcd_drvdata *lcd, u8 force)
{
	int ret = 0;
	u8 tset;

	tset = ((lcd->temperature >= 0) ? 0 : BIT(7)) | abs(lcd->temperature);

	if (force || lcd->tset_table[LDI_TSET_LEN] != tset) {
		lcd->tset_table[LDI_TSET_LEN] = tset;
		ret = s6e3ha0_write(lcd, lcd->tset_table, TSET_PARAM_SIZE);
		dsim_info("temperature: %d, tset: %d\n",
				lcd->temperature, tset);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static void init_dynamic_aid(struct lcd_drvdata *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;

	lcd->daid.br_base = brightness_base_table;
	lcd->daid.offset_gra = offset_gradation;
	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;
}

static void init_mtp_data(struct lcd_drvdata *lcd, const u8 *mtp_data)
{
	int i, c, j;

	int *mtp;

	mtp = lcd->daid.mtp;

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_MAX - 2; i >= 0; i--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] =
					(mtp_data[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j];
		}
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp_data[%d] = %d\n", j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp[%d] = %d\n", j, mtp[j]);

	for (i = 0, j = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%04d ", mtp[j]);
		smtd_dbg("\n");
	}
}

static int init_gamma_table(struct lcd_drvdata *lcd , const u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kzalloc(IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (!gamma) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (!gamma[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->gamma_table = kzalloc(IBRIGHTNESS_MAX * sizeof(u8 *),
			GFP_KERNEL);
	if (!lcd->gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8),
			GFP_KERNEL);
		if (!lcd->gamma_table[i]) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
err_alloc_gamma_table2:
	i = IBRIGHTNESS_MAX;
err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}

static int init_aid_dimming_table(struct lcd_drvdata *lcd)
{
	int i;

	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		memcpy(lcd->aor[i], SEQ_AOR_CONTROL,
			ARRAY_SIZE(SEQ_AOR_CONTROL));

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		lcd->aor[i][1] = aor_cmd[i][0];
		lcd->aor[i][2] = aor_cmd[i][1];
	}

	return 0;
}

static int init_elvss_table(struct lcd_drvdata *lcd, const u8 *elvss_data)
{
	int i, temp, acl, ret;

	for (acl = 0; acl < ACL_STATUS_MAX; acl++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			lcd->elvss_table[acl][temp] =
				kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *),
				GFP_KERNEL);

			if (IS_ERR_OR_NULL(lcd->elvss_table[acl][temp])) {
				pr_err(
				"failed to allocate elvss table\n");
				ret = -ENOMEM;
				goto err_alloc_elvss_table;
			}

			for (i = 0; i < ELVSS_STATUS_MAX; i++) {
				lcd->elvss_table[acl][temp][i] =
				kzalloc(ELVSS_PARAM_SIZE * sizeof(u8),
					GFP_KERNEL);
				if (IS_ERR_OR_NULL(
				lcd->elvss_table[acl][temp][i])) {
				pr_err("failed to allocate elvss\n");
					ret = -ENOMEM;
					goto err_alloc_elvss;
				}

				/* Duplicate same value with default one */
				memcpy(lcd->elvss_table[acl][temp][i],
					SEQ_ELVSS_SET,
					ARRAY_SIZE(SEQ_ELVSS_SET));

				lcd->elvss_table[acl][temp][i][1] =
					MPS_TABLE[acl][temp];
				lcd->elvss_table[acl][temp][i][2] =
					ELVSS_TABLE[acl][i];
				lcd->elvss_table[acl][temp][i][21] =
					elvss_data[20];
			}
		}

		/*
		 * this (elvss_table[acl][1]) is elvss table to support
		 * T ¢®A -20¢®E low temperature
		 */
		for (i = 0; i < ELVSS_STATUS_MAX; i++)
		lcd->elvss_table[acl][TEMP_BELOW_MINUS_20_DEGREE][i][21] =
		(elvss_data[20] > 3) ? (elvss_data[20] - 3) : 0;
	}

	return 0;

err_alloc_elvss:
	/* should be kfree elvss with acl */
	while (temp >= 0) {
		while (i > 0)
			kfree(lcd->elvss_table[acl][temp][--i]);

		i = ELVSS_STATUS_MAX;
		temp--;
	}
	temp = TEMP_MAX;
err_alloc_elvss_table:
	while (temp > 0)
		kfree(lcd->elvss_table[acl][--temp]);

	return ret;
}

static int init_hbm_parameter(struct lcd_drvdata *lcd, const u8 *mtp_data)
{
	int i, temp, acl;

	/* CA: 1~6 = C8: 34~39 */
	for (i = 0; i < 6; i++)
		lcd->gamma_table[IBRIGHTNESS_500NT][1 + i] =
			mtp_data[33 + i];

	/* CA: 7~21 = C8: 73~87 */
	for (i = 0; i < 15; i++)
		lcd->gamma_table[IBRIGHTNESS_500NT][7 + i] =
			mtp_data[72 + i];

	/* B6: 21 = C8h 40 */
	for (acl = 0; acl < ACL_STATUS_MAX; acl++) {
		for (temp = 0; temp < TEMP_BELOW_MINUS_20_DEGREE; temp++)
		lcd->elvss_table[acl][temp][ELVSS_STATUS_HBM][21] =
			mtp_data[39];
	}

	return 0;
}

static void show_lcd_table(struct lcd_drvdata *lcd)
{
	int i, j, acl, temp;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%02X ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		smtd_dbg("%03d: ", index_brightness_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%03d ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}
	smtd_dbg("\n");

	for (acl = 0; acl < ACL_STATUS_MAX; acl++) {
		for (temp = 0; temp < TEMP_MAX; temp++) {
			smtd_dbg("acl: %d, temp: %d\n", acl, temp);
			for (i = 0; i < ELVSS_STATUS_MAX; i++) {
				smtd_dbg("%03d: ", ELVSS_DIM_TABLE[i]);
				for (j = 0; j < ELVSS_PARAM_SIZE; j++)
					smtd_dbg("%02X ",
				lcd->elvss_table[acl][temp][i][j]);
				smtd_dbg("\n");
			}
			smtd_dbg("\n");
		}
		smtd_dbg("\n");
	}
}

static int update_brightness(struct lcd_drvdata *lcd, u8 force)
{
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness ==
		lcd->bd->props.max_brightness))
		lcd->bl = IBRIGHTNESS_500NT;

	if (force || (lcd->ldi_enable && (lcd->current_bl != lcd->bl))) {
		s6e3ha0_write(lcd, SEQ_TEST_KEY_ON_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		s6e3ha0_gamma_ctl(lcd);
		s6e3ha0_aid_parameter_ctl(lcd, force);
		s6e3ha0_set_elvss(lcd, force);
		s6e3ha0_write(lcd, SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		s6e3ha0_write(lcd, SEQ_GAMMA_UPDATE_L,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
		s6e3ha0_set_acl(lcd, force);
		s6e3ha0_set_tset(lcd, force);
		s6e3ha0_write(lcd, SEQ_TEST_KEY_OFF_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

		lcd->current_bl = lcd->bl;

		dsim_info("brightness=%d, bl=%d, candela=%d\n",
			brightness, lcd->bl, DIM_TABLE[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int s6e3ha0_ldi_disable(struct lcd_drvdata *lcd)
{
	int ret = 0;

	dsim_info("+ %s\n", __func__);

	s6e3ha0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

	dsim_info("DISPLAY_OFF\n");

	/* Wait 2 frame (33.4ms) */
	msleep(34);

	/* Sleep In (10h) */
	s6e3ha0_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));

	/* Wait 120ms */
	msleep(120);

	dsim_info("- %s\n", __func__);

	return ret;
}

static int s6e3ha0_power_on(struct lcd_drvdata *lcd)
{
	int ret = 0;

	dsim_info("+ %s\n", __func__);

	lcd->connected = 1;

	lcd_init(lcd->dsim->id, &lcd->dsim->lcd_info);

	lcd_enable(lcd->dsim->id);

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 1;
	mutex_unlock(&lcd->bl_lock);

	update_brightness(lcd, 1);

	dsim_info("- %s\n", __func__);

	return ret;
}

static int s6e3ha0_power_off(struct lcd_drvdata *lcd)
{
	int ret = 0;

	dsim_info("+ %s\n", __func__);

	mutex_lock(&lcd->bl_lock);
	lcd->ldi_enable = 0;
	mutex_unlock(&lcd->bl_lock);

	ret = s6e3ha0_ldi_disable(lcd);

	dsim_info("- %s\n", __func__);

	return ret;
}

static int s6e3ha0_power(struct lcd_drvdata *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e3ha0_power_on(lcd);

	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e3ha0_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e3ha0_set_power(struct lcd_device *ld, int power)
{
	struct lcd_drvdata *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dsim_err("power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e3ha0_power(lcd, power);
}

static int s6e3ha0_get_power(struct lcd_device *ld)
{
	struct lcd_drvdata *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int s6e3ha0_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static int s6e3ha0_get_brightness(struct backlight_device *bd)
{
	struct lcd_drvdata *lcd = bl_get_data(bd);

	return DIM_TABLE[lcd->bl];
}

static int s6e3ha0_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_drvdata *lcd = bl_get_data(bd);

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dsim_err("lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, lcd->bd->props.max_brightness,
				brightness);
		return -EINVAL;
	}

	if (lcd->ldi_enable) {
		ret = update_brightness(lcd, 0);
		if (ret < 0) {
			dsim_err("err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int check_fb_brightness(struct backlight_device *bd,
		struct fb_info *fb)
{
	return 0;
}

static struct lcd_ops s6e3ha0_lcd_ops = {
	.set_power = s6e3ha0_set_power,
	.get_power = s6e3ha0_get_power,
	.check_fb  = s6e3ha0_check_fb,
};

static const struct backlight_ops s6e3ha0_backlight_ops = {
	.get_brightness = s6e3ha0_get_brightness,
	.update_status = s6e3ha0_set_brightness,
	.check_fb = check_fb_brightness,
};

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", lcd->id[0],
			lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X %02X %02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t ddi_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);

	sprintf(buf, "ddi id : %02X %02X %02X %02X %02X\n",
		lcd->ddi_id[0], lcd->ddi_id[1], lcd->ddi_id[2],
			lcd->ddi_id[3], lcd->ddi_id[4]);

	return strlen(buf);
}

static ssize_t gamma_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);
	int i, j;

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->gamma_table[i][j]);
		printk("\n");
	}

	return strlen(buf);
}

static ssize_t test_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t test_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);
	int value;
	int rc;
	int bl_bak = lcd->bd->props.brightness;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (value == 0) {
			while (value != 255) {
				mutex_lock(&lcd->bl_lock);
				lcd->bd->props.brightness = value++;
				mutex_unlock(&lcd->bl_lock);
				if (lcd->ldi_enable)
					update_brightness(lcd, 0);
				usleep_range(5000, 6000);
			}
			while (value != 0) {
				mutex_lock(&lcd->bl_lock);
				lcd->bd->props.brightness = value--;
				mutex_unlock(&lcd->bl_lock);
				if (lcd->ldi_enable)
					update_brightness(lcd, 0);
				usleep_range(5000, 6000);
			}
			mutex_lock(&lcd->bl_lock);
			lcd->bd->props.brightness = bl_bak;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 0);
		} else
			dsim_err("%s: Invalid value! Please input 0\n",
				__func__);
	}
	return size;
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->auto_brightness);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dsim_info("%s: %d, %d\n", __func__,
				lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 0);
		}
	}
	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", lcd->coordinate[0], lcd->coordinate[1]);

	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month;

	year = ((lcd->date[0] & 0xF0) >> 4) + 2011;
	month = lcd->date[0] & 0xF;

	sprintf(buf, "%d, %d, %d\n", year, month, lcd->date[1]);

	return strlen(buf);
}

static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(dev);
	u8 temp[256];
	int i, j, k;
	int *mtp;

	mtp = lcd->daid.mtp;
	for (i = 0, j = 0; i < IV_MAX; i++, j += 3) {
		if (i == 0)
			dsim_info("MTP Offset VT R:%d G:%d B:%d\n",
				mtp[j], mtp[j + 1], mtp[j + 2]);
		else
			dsim_info("MTPOffset V%3d R:%04d G:%04d B:%04d\n",
				lcd->daid.iv_tbl[i], mtp[j], mtp[j + 1],
				mtp[j + 2]);
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < GAMMA_PARAM_SIZE; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = lcd->gamma_table[i][j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d",
				lcd->gamma_table[i][j] + k);
		}

		dsim_info("nit : %3d  %s\n", lcd->daid.ibr_tbl[i], temp);
	}

	dsim_info("%s\n", __func__);

	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(ddi_id, 0444, ddi_id_show, NULL);
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);
static DEVICE_ATTR(test_brightness, 0644, test_brightness_show,
			test_brightness_store);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show,
			auto_brightness_store);
static DEVICE_ATTR(color_coordinate, 0444,
			color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444,
			manufacture_date_show, NULL);
static DEVICE_ATTR(aid_log, 0444,
			aid_log_show, NULL);

static int s6e3ha0_probe(struct dsim_device *dsim)
{
	int ret;
	struct lcd_drvdata *lcd;

	u8 mtp_data[LDI_MTP_LEN] = {0,};
	u8 elvss_data[LDI_ELVSS_LEN] = {0,};

	lcd = kzalloc(sizeof(struct lcd_drvdata), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd,
			&s6e3ha0_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}
	dsim->lcd = lcd->ld;

	lcd->bd = backlight_device_register("pwm-backlight.0", dsim->dev,
		lcd, &s6e3ha0_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = DEFAULT_GAMMA_INDEX;
	lcd->current_bl = lcd->bl;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
#ifdef CONFIG_S5P_LCD_INIT
	lcd->power = FB_BLANK_POWERDOWN;
#else
	lcd->power = FB_BLANK_UNBLANK;
#endif
	lcd->auto_brightness = 0;
	lcd->connected = 1;
	lcd->siop_enable = 0;
	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->width = dsim->lcd_info.xres;
	lcd->height = dsim->lcd_info.yres;

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_window_type);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_ddi_id);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_gamma_table);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_test_brightness);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_aid_log);
	if (ret < 0)
		dsim_err("failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e3ha0_write(lcd, SEQ_TEST_KEY_ON_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3ha0_read_id(lcd, lcd->id);
	s6e3ha0_read_ddi_id(lcd, lcd->ddi_id);
	s6e3ha0_read_coordinate(lcd);
	s6e3ha0_read_mtp(lcd, mtp_data);
	s6e3ha0_read_elvss(lcd, elvss_data);
	s6e3ha0_read_tset(lcd);
	s6e3ha0_write(lcd, SEQ_TEST_KEY_OFF_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));

	dsim_info("ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_hbm_parameter(lcd, mtp_data);

	if (ret)
		dsim_info("gamma table generation is failed\n");

	show_lcd_table(lcd);

	lcd->ldi_enable = 1;

	update_brightness(lcd, 1);

	dsim_info("%s lcd panel driver has been probed.\n", __FILE__);

	return 0;

out_free_backlight:
	lcd_device_unregister(lcd->ld);
	kfree(lcd);
	return ret;

out_free_lcd:
	kfree(lcd);
	return ret;

err_alloc:
	return ret;
}

static int s6e3ha0_displayon(struct dsim_device *dsim)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3ha0_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6e3ha0_suspend(struct dsim_device *dsim)
{
	struct lcd_drvdata *lcd = dev_get_drvdata(&dsim->lcd->dev);

	s6e3ha0_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e3ha0_resume(struct dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3ha0_mipi_lcd_driver = {
	.probe		= s6e3ha0_probe,
	.displayon	= s6e3ha0_displayon,
	.suspend	= s6e3ha0_suspend,
	.resume		= s6e3ha0_resume,
};
