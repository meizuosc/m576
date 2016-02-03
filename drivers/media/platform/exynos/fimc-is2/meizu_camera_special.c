/*
 * Meizu special feature of camera
 *
 * Copyright (C) 2015 Meizu Technology Co.Ltd, Zhuhai, China
 * Author: 	QuDao	<qudao@meizu.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include "meizu_camera_special.h"
#define CAM_EEPROM_NAME "cam_eeprom"

extern int fimc_is_rear_power_clk(bool enable);

static int camera_power_on(void)
{
	int ret;
	ret = fimc_is_rear_power_clk(true);
	if (ret) {
		pr_err("%s() failed:%d\n", __func__, ret);
	}
	return ret;
}

static int camera_power_off(void)
{
	int ret;
	ret = fimc_is_rear_power_clk(false);
	if (ret) {
		pr_err("%s() failed:%d\n", __func__, ret);
	}
	return ret;
}

int camera_module_active(bool enable)
{
	int ret;
	//struct fimc_is_core *core = device->private_data;

	if (enable) {
		ret = camera_power_on();
	} else {
		ret = camera_power_off();
	}

	if (ret) {
		pr_err("%s(), power %d failed:%d\n",
			__func__, enable, ret);
		return ret;
	}

	//fimc_is_i2c_enable_irq(core, enable);
	return 0;
}

static int cam_eeprom_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	dev_info(&client->dev, "%s(), addr:0x%x, name:%s\n",
		__func__, client->addr, client->name);

	LC898212XD_probe(client);
	return 0;
}

static int cam_eeprom_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id cam_eeprom_id[] = {
	{CAM_EEPROM_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cam_eeprom_id);

#ifdef CONFIG_OF
static struct of_device_id cam_eeprom_of_match_table[] = {
	{
		.compatible = "meizu,cam_eeprom",
	},
	{},
};
MODULE_DEVICE_TABLE(of, cam_eeprom_of_match_table);
#else
#define cam_eeprom_of_match_table NULL
#endif

static struct i2c_driver cam_eeprom_i2c_driver = {
	.driver = {
		   .name = CAM_EEPROM_NAME,
		   .owner = THIS_MODULE,
		   .pm = NULL,
			.of_match_table = cam_eeprom_of_match_table,
		   },
	.probe = cam_eeprom_probe,
	.remove = cam_eeprom_remove,
	.id_table = cam_eeprom_id,
};

int meizu_special_feature_probe(struct i2c_client *client)
{
	i2c_add_driver(&cam_eeprom_i2c_driver);
	return 0;
}

