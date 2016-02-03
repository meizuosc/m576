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
#include "fimc-is-device-imx230.h"

#define SENSOR_NAME "IMX230"

static int mclk_on_index = -1;
static int mclk_off_index = -1;
static struct fimc_is_module_enum *g_module;

static struct fimc_is_sensor_cfg config_imx230[] = {
	/* 5344x4016@24fps */
	FIMC_IS_SENSOR_CFG(5344, 4016, 24, 30, 0),
	/* 5328x3000@30fps */
	FIMC_IS_SENSOR_CFG(5328, 3000, 30, 31, 1),
	/* 3856X2170@30fps */
	FIMC_IS_SENSOR_CFG(3856, 2170, 30, 15, 2),
	/* 2672X2002@30fps */
	FIMC_IS_SENSOR_CFG(2672, 2002, 30, 10, 3),
	/* 2672X2008@30fps */
	FIMC_IS_SENSOR_CFG(2672, 2008, 30, 10, 4),	 // HDR
	/* 1296X730@30fps */
	FIMC_IS_SENSOR_CFG(1296, 730, 30, 8, 5),
	/* 1296X730@120fps */
	FIMC_IS_SENSOR_CFG(1296, 730, 120, 8, 6),
	/* 5344x4016@long shutter*/
	FIMC_IS_SENSOR_CFG(5344, 4016, 1, 9, 7),
};

static struct fimc_is_vci vci_imx230[] = {
#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
	/* VC Channel 1 is for PDAF and this is coming from FIMC BNS-B and VC Channel 2 is for HDR STAT and this is coming from FIMCBNS-D
	Kundong.Kim@samsung.com*/
	{
		.pixelformat = V4L2_PIX_FMT_SBGGR10,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_PDAF_STAT}, {2, HW_FORMAT_HDR_STAT}, {3, 0}}
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR12,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_PDAF_STAT}, {2, HW_FORMAT_HDR_STAT}, {3, 0}}
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR16,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_PDAF_STAT}, {2, HW_FORMAT_HDR_STAT}, {3, 0}}
	}
#else
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
#endif
};

static int sensor_imx230_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);

	pr_info("[MOD:D:%d] %s(%d)\n", module->sensor_id, __func__, val);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_imx230_init
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops
};

int fimc_is_rear_power_clk(bool enable)
{
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *device;
	struct exynos_platform_fimc_is_module *pdata;
	struct exynos_platform_fimc_is_sensor *sensor_pdata;
	struct fimc_is_module_enum *module;

	bool sensor_status;
	int target_gpio_scenario;
	int ret;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	if (IS_ERR_OR_NULL(g_module)) {
		pr_err("%s(), err!g_module:%p\n",
			__func__, g_module);
		return -EINVAL;
	}

	module = g_module;
	pdata = module->pdata;
	device = &core->sensor[pdata->id];
	sensor_pdata = device->pdata;

	sensor_status = !!(test_bit(FIMC_IS_SENSOR_S_INPUT, &device->state));
	if (!sensor_status) {
		if (enable) {
			pr_debug("%s(), enable power clk while sensor is off\n", __func__);
			target_gpio_scenario = GPIO_SCENARIO_ON;
		} else {
			pr_debug("%s(), disable power clk while sensor is off\n", __func__);
			target_gpio_scenario = GPIO_SCENARIO_OFF;
		}
	} else {
		pr_warn("%s(), sensor is already on, do nothing for %d request",
			__func__, enable);
		ret = -EBUSY;
		goto flag;
	}

	if (enable) {
		ret = sensor_pdata->mclk_on(device->pdev, SENSOR_SCENARIO_NORMAL, 0);
	} else {
		ret = sensor_pdata->mclk_off(device->pdev, SENSOR_SCENARIO_NORMAL, 0);
	}
	ret = pdata->gpio_cfg(module->pdev, SENSOR_SCENARIO_NORMAL, target_gpio_scenario);

flag:
	return ret;
}

#ifdef CONFIG_OF
static int sensor_imx230_power_setpin(struct platform_device *pdev,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct device *dev;
	struct device_node *dnode;
	int gpio_reset = 0;
	int gpio_none = 0;
	#ifdef GOVERN_RANGING_IC
	int gpio_ps_en = 0;
	int gpio_ps_int = 0;
	#endif
	int gpio_flash_hwen = 0;

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

	#ifdef GOVERN_RANGING_IC
	gpio_ps_en = of_get_named_gpio(dnode, "gpio_ps_en", 0);
	if (!gpio_is_valid(gpio_ps_en)) {
		dev_err(dev, "failed to get gpio_ps_en\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_ps_en, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_ps_en);
	}

	gpio_ps_int = of_get_named_gpio(dnode, "gpio_ps_int", 0);
	if (!gpio_is_valid(gpio_ps_int)) {
		dev_err(dev, "failed to get gpio_ps_int\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_ps_int, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_ps_int);
	}
	pr_info("%s(), gpio_ps_en:%d, gpio_ps_int:%d\n",
		__func__, gpio_ps_en, gpio_ps_int);
	#endif

	gpio_flash_hwen = of_get_named_gpio(dnode, "gpio_flash_hwen", 0);
	if (!gpio_is_valid(gpio_flash_hwen)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_flash_hwen, GPIOF_OUT_INIT_LOW, "FLASH_GPIO_OUTPUT_LOW");
		gpio_free(gpio_flash_hwen);
	}

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF);
#ifdef CONFIG_OIS_USE
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF);
#endif

	/*
	* REAR CAEMRA - POWER ON
	*/
	/* ON MCLK, for "pin1" in dts	*/
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none,
		"pin", PIN_FUNCTION, 1, 0);
	mclk_on_index =
		pdata->pinctrl_index[SENSOR_SCENARIO_NORMAL][GPIO_SCENARIO_ON] - 1;
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_reset, "sen_rst low", PIN_OUTPUT, 0, 0);

	/* VCC25_BCAM  */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "vcc25_bcam", PIN_REGULATOR, 1, 0, 0);
	/* VDD18_BCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "vdd18_bcam", PIN_REGULATOR, 1, 0, 0);
	/* VDD11_BCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "vdd11_bcam", PIN_REGULATOR, 1, 0, 0);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_reset, "sen_rst high", PIN_OUTPUT, 1, 3000);

	#ifdef GOVERN_OIS_IC
	/* DVDD20_VCM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "dvdd20_vcm", PIN_REGULATOR, 1, 0, 0);
	/* AVDD28_DRV */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "avdd28_drv", PIN_REGULATOR, 1, 0, 0);
	#endif

	#ifdef GOVERN_RANGING_IC
	/* VDD28_PS */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_none, "vdd28_ps", PIN_REGULATOR, 1, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_ps_en, "sen_ps_en high", PIN_OUTPUT, 1, 2000);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_ps_int, "sen_ps_int high", PIN_OUTPUT, 1, 0);
	#endif

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON,
		gpio_flash_hwen, "flash_gpio_high", PIN_OUTPUT, 1, 0);

	/*
	* REAR CAEMRA - POWER OFF
	*/
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_reset, "sen_rst low", PIN_OUTPUT, 0 ,0);
	/* OFF MCLK, For "pin0" in dts */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "pin", PIN_FUNCTION, 0, 0);
	mclk_off_index =
		pdata->pinctrl_index[SENSOR_SCENARIO_NORMAL][GPIO_SCENARIO_OFF] -1;

	#ifdef GOVERN_RANGING_IC
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_ps_en, "sen_ps_en low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_ps_int, "sen_ps_int low", PIN_OUTPUT, 0, 0);
	/* VDD28_PS */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "vdd28_ps", PIN_REGULATOR, 0, 0, 0);
	#endif

	#ifdef GOVERN_OIS_IC
	/* AVDD28_DRV */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "avdd28_drv", PIN_REGULATOR, 0, 0, 0);
	/* DVDD20_VCM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "dvdd20_vcm", PIN_REGULATOR, 0, 0, 0);
	#endif

	/* VCC25_BCAM  */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "vcc25_bcam", PIN_REGULATOR, 0, 0, 0);
	/* VDD18_BCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "vdd18_bcam", PIN_REGULATOR, 0, 0, 0);
	/* VDD11_BCAM */
	SET_PIN_VOLTAGE(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_none, "vdd11_bcam", PIN_REGULATOR, 0, 0, 0);

	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF,
		gpio_flash_hwen, "flash_gpio_low", PIN_OUTPUT, 0, 0);

	dev_info(dev, "%s X v4\n", __func__);
	return 0;
}
#endif /* CONFIG_OF */

int sensor_imx230_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	struct exynos_platform_fimc_is_module *pdata;
	struct device *dev;
	struct mz_module_info *module_info;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &pdev->dev;

#ifdef CONFIG_OF
	fimc_is_sensor_module_parse_dt(pdev, sensor_imx230_power_setpin);
#endif

	pdata = dev_get_platdata(dev);

	device = &core->sensor[pdata->id];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		probe_err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	module_info = &device->mz_modu_info;

	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->pdev = pdev;
	module->sensor_id = SENSOR_NAME_IMX230;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = 5328;
	module->active_height = 4006;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 300;
	module->position = SENSOR_POSITION_REAR;
	module->position = pdata->position;
#if defined(CONFIG_CAMERA_SUPPORT_PDAF)
	module->mode = CSI_MODE_DT_ONLY;
#else
	module->mode = CSI_MODE_CH0_ONLY;
#endif
	module->lanes = CSI_DATA_LANES_4;
	module->vcis = ARRAY_SIZE(vci_imx230);
	module->vci = vci_imx230;
	module->sensor_maker = "SONY";
	module->sensor_name = "IMX230";
	module->setfile_name = "setfile_imx230.bin";
	module->cfgs = ARRAY_SIZE(config_imx230);
	module->cfg = config_imx230;
	module->ops = NULL;
	module->private_data = NULL;

	v4l2_subdev_init(subdev_module, &subdev_ops);
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);
	g_module = module;

	ext = &module->ext;
	ext->mipi_lane_num = module->lanes;
	ext->I2CSclk = 0;

	ext->sensor_con.product_name = SENSOR_NAME_IMX230;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
	ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	#if 0
	if (pdata->af_product_name != ACTUATOR_NAME_NOTHING) {
		ext->actuator_con.product_name = pdata->af_product_name;
		ext->actuator_con.peri_type = SE_I2C;
		ext->actuator_con.peri_setting.i2c.channel = pdata->af_i2c_ch;
		ext->actuator_con.peri_setting.i2c.slave_address = pdata->af_i2c_addr;
		ext->actuator_con.peri_setting.i2c.speed = 400000;
	}
	#else
	if (VCM_DRIVER_IC_LC898212 == module_info->driver_ic_id) {
		/* LC898212 */
		ext->actuator_con.product_name = ACTUATOR_NAME_LC898212;
		ext->actuator_con.peri_setting.i2c.slave_address = 0xe4;
		ext->actuator_con.peri_type = SE_I2C;
		ext->actuator_con.peri_setting.i2c.channel = 0;
		ext->actuator_con.peri_setting.i2c.speed = 400000;
		pr_info("%s(), use new vcm driver ic\n", __func__);
	} else if (VCM_DRIVER_IC_BU63165 == module_info->driver_ic_id) {
		/* BU63165 */
		ext->actuator_con.product_name = ACTUATOR_NAME_BU63165;
		ext->actuator_con.peri_setting.i2c.slave_address = 0x1c;
		ext->actuator_con.peri_type = SE_I2C;
		ext->actuator_con.peri_setting.i2c.channel = 0;
		ext->actuator_con.peri_setting.i2c.speed = 400000;
		pr_warn("%s(), use obsolete vcm driver ic\n", __func__);
	} else {
		pr_err("%s(), err!unkown vcm driver ic id:%d\n",
			__func__, module_info->driver_ic_id);
	}
	#endif

	if (pdata->flash_product_name != FLADRV_NAME_NOTHING) {
		ext->flash_con.product_name = pdata->flash_product_name;
		ext->flash_con.peri_type = SE_I2C;
	}

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

	ext->companion_con.product_name = COMPANION_NAME_NOTHING;

#if defined(CONFIG_OIS_USE)
	ext->ois_con.product_name = OIS_NAME_IDG2030;
	ext->ois_con.peri_type = SE_I2C;
	ext->ois_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C1;
	ext->ois_con.peri_setting.i2c.slave_address = 0x48;
	ext->ois_con.peri_setting.i2c.speed = 400000;
#else
	ext->ois_con.product_name = OIS_NAME_NOTHING;
	ext->ois_con.peri_type = SE_NULL;
#endif

	pr_debug("%s(), device:%p, module:%p, pdata:%p", __func__,
		device, module, pdata);
p_err:
	probe_info("%s(%d)\n", __func__, ret);
	return ret;
}

static int sensor_imx230_remove(struct platform_device *pdev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_imx230_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-imx230",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_imx230_match);

static struct platform_driver sensor_imx230_driver = {
	.probe  = sensor_imx230_probe,
	.remove = sensor_imx230_remove,
	.driver = {
		.name   = SENSOR_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_imx230_match,
	}
};

static int __init fimc_is_sensor_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sensor_imx230_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}

static void __exit fimc_is_sensor_module_exit(void)
{
	platform_driver_unregister(&sensor_imx230_driver);
}
module_init(fimc_is_sensor_module_init);
module_exit(fimc_is_sensor_module_exit);
