/*
 *  s2mg001_fuelgauge.c
 *  Samsung S2MG001 Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/power/sec_fuelgauge.h>

static int s2mg001_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);

	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

static int s2mg001_read_reg(struct i2c_client *client, int reg, u8 *buf)
{
	int ret = 0;

	buf[0] = i2c_smbus_read_byte_data(client, reg);
	if (buf[0] < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	buf[1] = i2c_smbus_read_byte_data(client, reg + 1);
	if (buf[1] < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return 0;
}

static int s2mg001_init_regs(struct i2c_client *client)
{
	int ret = 0;
	u8 data;

	ret = i2c_smbus_write_byte_data(client, S2MG001_REG_START, 0x03);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

	data = i2c_smbus_read_byte_data(client, 0x2e);
	data &= ~(0x01 << 3);
	ret = i2c_smbus_write_byte_data(client, 0x2e, data);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

static void s2mg001_alert_init(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	u8 data[2];

	/* VBAT Threshold setting */
	data[0] = 0x00 & 0x0f;

	/* SOC Threshold setting */
	data[0] = data[0] | (fuelgauge->pdata->fuel_alert_soc << 4);

	data[1] = 0x00;
	s2mg001_write_reg(client, S2MG001_REG_IRQ_LVL, data);
}

static bool s2mg001_check_status(struct i2c_client *client)
{
	u8 data[2];
	bool ret = false;

	/* check if Smn was generated */
	if (s2mg001_read_reg(client, S2MG001_REG_STATUS, data) < 0)
		return ret;

	dev_dbg(&client->dev, "%s: status to (%02x%02x)\n",
		__func__, data[1], data[0]);

	if (data[1] & (0x1 << 1))
		return true;
	else
		return false;
}

static int s2mg001_set_temperature(struct i2c_client *client, int temperature)
{
	u8 data[2];
	char val;

	val = temperature / 10;
	data[0] = val;
	data[1] = 0x00;
	s2mg001_write_reg(client, S2MG001_REG_RTEMP, data);

	dev_dbg(&client->dev, "%s: temperature to (%d)\n",
		__func__, temperature);

	return temperature;
}

static int s2mg001_get_temperature(struct i2c_client *client)
{
	u8 data[2];
	s32 temperature = 0;

	if (s2mg001_read_reg(client, S2MG001_REG_RTEMP, data) < 0)
		return -ERANGE;

	/* data[] store 2's compliment format number */
	if (data[0] & (0x1 << 7)) {
		/* Negative */
		temperature = ((~(data[0])) & 0xFF) + 1;
		temperature *= -10;
	} else {
		temperature = data[0] & 0x7F;
		temperature *= 10;
	}

	dev_dbg(&client->dev, "%s: temperature (%d)\n",
		__func__, temperature);

	return temperature;
}

/* soc should be 0.01% unit */
static int s2mg001_get_soc(struct i2c_client *client)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;

	for (i = 0; i < 50; i++) {
		if (s2mg001_read_reg(client, S2MG001_REG_RSOC, data) < 0)
			return -EINVAL;
		if (s2mg001_read_reg(client, S2MG001_REG_RSOC, check_data) < 0)
			return -EINVAL;
		if ((data[0] == check_data[0]) && (data[1] == check_data[1]))
			break;
	}

	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 12);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 12));
	}

	dev_info(&client->dev, "%s: raw capacity (0x%x:%d)\n", __func__,
		compliment, rsoc);

	return min(rsoc, 10000);
}

static int s2mg001_get_ocv(struct i2c_client *client)
{
	u8 data[2];
	u32 rocv = 0;

	if (s2mg001_read_reg(client, S2MG001_REG_ROCV, data) < 0)
		return -EINVAL;

	rocv = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&client->dev, "%s: rocv (%d)\n", __func__, rocv);

	return rocv;
}

static int s2mg001_get_vbat(struct i2c_client *client)
{
	u8 data[2];
	u32 vbat = 0;

	if (s2mg001_read_reg(client, S2MG001_REG_RVBAT, data) < 0)
		return -EINVAL;

	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&client->dev, "%s: vbat (%d)\n", __func__, vbat);

	return vbat;
}

static int s2mg001_get_avgvbat(struct i2c_client *client)
{
	u8 data[2];
	u32 new_vbat, old_vbat = 0;
	int cnt;

	for (cnt = 0; cnt < 5; cnt++) {
		if (s2mg001_read_reg(client, S2MG001_REG_RVBAT, data) < 0)
			return -EINVAL;

		new_vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

		if (cnt == 0)
			old_vbat = new_vbat;
		else
			old_vbat = new_vbat / 2 + old_vbat / 2;
	}

	dev_dbg(&client->dev, "%s: avgvbat (%d)\n", __func__, old_vbat);

	return old_vbat;
}

bool sec_hal_fg_init(struct i2c_client *client)
{
	/* initialize fuel gauge registers */
	s2mg001_init_regs(client);

	return true;
}

bool sec_hal_fg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_reset(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_fuelalert_init(struct i2c_client *client, int soc)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	u8 data[2];

	/* 1. Set s2mg001 alert configuration. */
	s2mg001_alert_init(client);

	if (s2mg001_read_reg(client, S2MG001_REG_IRQ, data) < 0)
		return -1;

	/*Enable VBAT, SOC */
	data[1] &= 0xfc;

	/*Disable IDLE_ST, INIT)ST */
	data[1] |= 0x0c;

	s2mg001_write_reg(client, S2MG001_REG_IRQ, data);

	dev_dbg(&client->dev, "%s: irq_reg(%02x%02x) irq(%d)\n",
		 __func__, data[1], data[0], fuelgauge->pdata->fg_irq);

	return true;
}

bool sec_hal_fg_is_fuelalerted(struct i2c_client *client)
{
	return s2mg001_check_status(client);
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct sec_fuelgauge_info *fuelgauge = irq_data;
	int ret;

	ret = i2c_smbus_write_byte_data(fuelgauge->client, S2MG001_REG_IRQ, 0x00);
	if (ret < 0)
		dev_err(&fuelgauge->client->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

bool sec_hal_fg_full_charged(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_get_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	switch (psp) {
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mg001_get_vbat(client);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = s2mg001_get_avgvbat(client);
			break;
		case SEC_BATTERY_VOLTAGE_OCV:
			val->intval = s2mg001_get_ocv(client);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 0;
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		val->intval = 0;
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = 0;
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW)
			val->intval = s2mg001_get_soc(client);
		else
			val->intval = s2mg001_get_soc(client) / 10;

		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mg001_get_temperature(client);
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_fg_set_property(struct i2c_client *client,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	switch (psp) {
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		s2mg001_set_temperature(client, val->intval);
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
	int i = 0;

	switch (offset) {
/*	case FG_REG: */
/*		break; */
	case FG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%02x%02x\n",
			fg->reg_data[1], fg->reg_data[0]);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sec_hal_fg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_fuelgauge_info *fg =
		container_of(psy, struct sec_fuelgauge_info, psy_fg);
	int ret = 0;
	int x = 0;
	u8 data[2];

	switch (offset) {
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			fg->reg_addr = x;
			s2mg001_read_reg(fg->client,
				fg->reg_addr, fg->reg_data);
			dev_dbg(&fg->client->dev,
				"%s: (read) addr = 0x%x, data = 0x%02x%02x\n",
				 __func__, fg->reg_addr,
				 fg->reg_data[1], fg->reg_data[0]);
			ret = count;
		}
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data[0] = (x & 0xFF00) >> 8;
			data[1] = (x & 0x00FF);
			dev_dbg(&fg->client->dev,
				"%s: (write) addr = 0x%x, data = 0x%02x%02x\n",
				__func__, fg->reg_addr, data[1], data[0]);
			s2mg001_write_reg(fg->client,
				fg->reg_addr, data);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

