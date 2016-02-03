/*
 * RANGING IC driver for MEIZU M86.
 * Author: QuDao, qudao@meizu.com
 *
 * Copyright (C) 2015 MEIZU
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include "vl6180x_def.h"
#include "stmvl6180.h"
#include "ranging_meizu.h"

int mz_ranging_power_enable(struct i2c_client *client, bool enable)
{
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	struct regulator *regulator = NULL;
	char *regu_name = "vdd28_ps";
	int ret;

	dev_info(&client->dev, "%s(), enable:%d\n",
		__func__, enable);
	if (data->hw_enable == enable) {
		pr_warn("%s(), power is already %d\n", __func__, enable);
		return 0;
	}

	regulator = regulator_get(NULL, regu_name);
	if (IS_ERR_OR_NULL(regulator)) {
		pr_err("%s : regulator_get(%s) fail\n", __func__, regu_name);
		return PTR_ERR(regulator);
	}

	if (enable) {
		ret = regulator_enable(regulator);
		if (ret) {
			pr_err("%s : regulator_enable(%s) fail\n", __func__, regu_name);
			goto flag;
		}
		gpio_set_value(data->gpio_ps_en, 1);
		msleep(5);
	} else {
		gpio_set_value(data->gpio_ps_en, 0);
		ret = regulator_disable(regulator);
		if (ret) {
			pr_err("%s : regulator_disable(%s) fail\n", __func__, regu_name);
			goto flag;
		}
	}

	data->hw_enable = enable;

flag:
	regulator_put(regulator);
	return ret;
}

#ifdef CONFIG_OF
int mz_ranging_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct stmvl6180_data *data;
	int gpio = -1;
	char *pin_name = NULL;

	data = i2c_get_clientdata(client);

	pin_name = "gpio_ps_en";
	if (of_property_read_bool(np, pin_name)) {
		gpio = of_get_named_gpio_flags(np, pin_name, 0, NULL);
		pr_info("%s(), got gpio %s:%d\n", __func__, pin_name, gpio);
		data->gpio_ps_en = gpio;
		gpio_request_one(gpio, GPIOF_OUT_INIT_LOW,
				    pin_name);
		gpio_free(gpio);
	} else {
		pr_err("%s(), Err! can not get gpio %s\n", __func__, pin_name);
		return -ENODEV;
	}

	pin_name = "gpio_ps_int";
	if (of_property_read_bool(np, pin_name)) {
		gpio = of_get_named_gpio_flags(np, pin_name, 0, NULL);
		pr_info("%s(), got gpio %s:%d\n", __func__, pin_name, gpio);
		data->gpio_ps_int= gpio;
		gpio_request_one(gpio, GPIOF_IN,
				    pin_name);
		gpio_free(gpio);
	} else {
		pr_err("%s(), Err! can not get gpio %s\n", __func__, pin_name);
		return -ENODEV;
	}

	return 0;
}
#else
int mz_ranging_parse_dt(struct i2c_client *client)
{
	return -EINVAL;
}
#endif

#ifdef CONFIG_PM
int mz_ranging_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);

	dev_info(&i2c->dev, "%s(), +++++\n", __func__);

	return 0;
}

int mz_ranging_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);

	dev_info(&i2c->dev, "%s(), +++++\n", __func__);

	return 0;
}
#endif

