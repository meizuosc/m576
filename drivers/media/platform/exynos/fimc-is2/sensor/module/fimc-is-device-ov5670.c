/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-ov5670.h"

#define SENSOR_NAME "OV5670"

static struct fimc_is_sensor_cfg config_ov5670[] = {
	/* 2592x1944@30fps */
	FIMC_IS_SENSOR_CFG(2592, 1944, 30, 18, 0),
	/* 2592x1458@30fps */
	FIMC_IS_SENSOR_CFG(2592, 1458, 30, 18, 5),
	/* 2576x1450@30fps */
	FIMC_IS_SENSOR_CFG(2576, 1450, 30, 18, 1),
	/* 1296x730@30fps */
	FIMC_IS_SENSOR_CFG(1296, 730, 30, 18, 2),
	/* 648x486@30fps */
	FIMC_IS_SENSOR_CFG(648, 486, 30, 18, 3),
	/* 648x486@120fps */
	FIMC_IS_SENSOR_CFG(648, 486, 120, 18, 4),
};

static struct fimc_is_vci vci_ov5670[] = {
	{
		.pixelformat = V4L2_PIX_FMT_SBGGR10,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_USER}, {3, 0}}
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR12,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_USER}, {3, 0}}
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR16,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_USER}, {3, 0}}
	}
};

static int sensor_ov5670_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

	pr_info("[MOD:D:%d] %s(%d)\n", module->sensor_id, __func__, val);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_ov5670_init
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops
};

#ifdef CONFIG_OF
static int sensor_ov5670_power_setpin(struct platform_device *pdev,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct device *dev;
	struct device_node *dnode;
	int gpio_reset = 0;
	int gpio_none = 0;
	
	BUG_ON(!pdev);

	dev = &pdev->dev;
	dnode = dev->of_node;

	dev_info(dev, "%s E v4\n", __func__);

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF);

	/*
	* FRONT CAEMRA - POWER ON
	*/
	/* ON MCLK, for "pin1" in dts	*/
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none,
		"pin", PIN_FUNCTION, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_reset, "sen_rst low", PIN_OUTPUT, 0, 0);

	/* VDD18_FCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "vdd18_fcam", PIN_REGULATOR, 1, 10, 0);

	/* VCC28_FCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "vcc28_fcam", PIN_REGULATOR, 1, 10, 0);

	/* VDD12_BCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "vdd12_fcam", PIN_REGULATOR, 1, 0, 0);


	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_reset, "sen_rst high", PIN_OUTPUT, 1, 1000);

	/*
	* FRONT CAEMRA - POWER OFF
	*/
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_reset, "sen_rst low", PIN_OUTPUT, 0 ,0);
	/* OFF MCLK, For "pin0" in dts */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "pin", PIN_FUNCTION, 0, 0);

	/* VDD12_BCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "vdd12_fcam", PIN_REGULATOR, 0, 0, 0);

	/* VCC28_FCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "vcc28_fcam", PIN_REGULATOR, 0, 0, 0);

	/* VDD18_FCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "vdd18_fcam", PIN_REGULATOR, 0, 0, 0);

	dev_info(dev, "%s X v4\n", __func__);
	return 0;
}
#endif /* CONFIG_OF */

int sensor_ov5670_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	struct exynos_platform_fimc_is_module *pdata;
	struct device *dev;

	printk("@@@sensor_ov5670_probe@@@");
 
	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &pdev->dev;

#ifdef CONFIG_OF
	fimc_is_sensor_module_parse_dt(pdev, sensor_ov5670_power_setpin);
#endif

	pdata = dev_get_platdata(dev);

	device = &core->sensor[pdata->id];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		probe_err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->pdev = pdev;
	module->sensor_id = SENSOR_NAME_OV5670;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = 2576;
	module->active_height = 1934;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 300;
	module->position = pdata->position;
	module->mode = CSI_MODE_CH0_ONLY;
	module->lanes = CSI_DATA_LANES_2;
	module->vcis = ARRAY_SIZE(vci_ov5670);
	module->vci = vci_ov5670;
	module->sensor_maker = "OMNIVISION";
	module->sensor_name = "OV5670";
	module->setfile_name = "setfile_ov5670.bin";
	module->cfgs = ARRAY_SIZE(config_ov5670);
	module->cfg = config_ov5670;
	module->ops = NULL;
	module->private_data = NULL;

	ext = &module->ext;
	ext->mipi_lane_num = module->lanes;
	ext->I2CSclk = I2C_L0;

	ext->sensor_con.product_name = SENSOR_NAME_OV5670;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
	ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;
	ext->companion_con.product_name = COMPANION_NAME_NOTHING;
	ext->actuator_con.product_name = ACTUATOR_NAME_NOTHING;
	ext->flash_con.product_name = FLADRV_NAME_NOTHING;
	ext->ois_con.product_name = OIS_NAME_NOTHING;
	ext->ois_con.peri_type = SE_NULL;

	v4l2_subdev_init(subdev_module, &subdev_ops);
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

p_err:
	probe_info("%s(%d)\n", __func__, ret);
	return ret;
}

static int sensor_ov5670_remove(struct platform_device *pdev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_ov5670_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-ov5670",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_ov5670_match);

static struct platform_driver sensor_ov5670_driver = {
	.probe  = sensor_ov5670_probe,
	.remove = sensor_ov5670_remove,
	.driver = {
		.name   = SENSOR_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_ov5670_match,
	}
};

static int __init fimc_is_sensor_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sensor_ov5670_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}

static void __exit fimc_is_sensor_module_exit(void)
{
	platform_driver_unregister(&sensor_ov5670_driver);
}
module_init(fimc_is_sensor_module_init);
module_exit(fimc_is_sensor_module_exit);
