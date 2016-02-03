#include <linux/types.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <linux/err.h>

#include "fimc-is-fan53555.h"

/**
 * FAN53555 Vout Tables
 */
static const struct fan53555_vout vout_tables[] = {
	{0, FAN53555_VOUT_1P00, "1.00"}, /* defualt voltage */
	{1, FAN53555_VOUT_0P88, "0.88"},
	{2, FAN53555_VOUT_0P90, "0.90"},
	{3, FAN53555_VOUT_0P93, "0.93"},
	{4, FAN53555_VOUT_0P95, "0.95"},
	{5, FAN53555_VOUT_0P98, "0.98"},
	{6, FAN53555_VOUT_1P00, "1.00"},
};

/**
 * fan53555_get_vout_val: get i2c register value to set vout of dcdc regulator.
 */
int fan53555_get_vout_val(int sel)
{
	int i, vout = vout_tables[0].val;

	if (sel < 0)
		pr_err("%s: error, invalid sel %d\n", __func__, sel);

	for (i = 0; ARRAY_SIZE(vout_tables); i++) {
		if (vout_tables[i].sel == sel) {
			return vout_tables[i].val;
		}
	}

	pr_err("%s: warning, default voltage selected. sel %d\n", __func__, sel);

	return vout;
}

/**
 * fan53555_get_vout_name: get voltage name of vout as string.
 */
 const char *fan53555_get_vout_str(int sel)
{
	const char *vout = vout_tables[0].vout;
	int i;

	if (sel < 0)
		pr_err("%s: error, invalid sel %d\n", __func__, sel);

	for (i = 0; ARRAY_SIZE(vout_tables); i++) {
		if (vout_tables[i].sel == sel) {
			return vout_tables[i].vout;
		}
	}

	pr_err("%s: warning, default voltage selected. sel %d\n", __func__, sel);

	return vout;
}

int fan53555_enable_vsel0(struct i2c_client *client, int on_off)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
	if(ret < 0){
		pr_err("%s: read error = %d , try again", __func__, ret);
		ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
		if (ret < 0)
			pr_err("%s: read 2nd error = %d", __func__, ret);
	}

	ret &= (~VSEL0_BUCK_EN0);
	ret |= (on_off << VSEL0_BUCK_EN0_SHIFT);

	ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
	if (ret < 0){
		pr_err("%s: write error = %d , try again", __func__, ret);
		ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
		if (ret < 0)
			pr_err("%s: write 2nd error = %d", __func__, ret);
	}
	return ret;
}

/**
 * fan53555_set_vsel0_vout: set dcdc vout with i2c register value.
 */
int fan53555_set_vsel0_vout(struct i2c_client *client, int vout)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
	if(ret < 0){
		pr_err("%s: read error = %d , try again", __func__, ret);
		ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
		if (ret < 0)
			pr_err("%s: read 2nd error = %d", __func__, ret);
	}

	ret &= (~VSEL0_NSEL0);
	ret |= vout;

	ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
	if (ret < 0){
		pr_err("%s: write error = %d , try again", __func__, ret);
		ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
		if (ret < 0)
			pr_err("%s: write 2nd error = %d", __func__, ret);
	}
	return ret;
}


