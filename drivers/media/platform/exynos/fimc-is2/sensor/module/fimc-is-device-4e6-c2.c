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
#include "../../fimc-is-core.h"
#include "../../fimc-is-device-sensor.h"
#include "../../fimc-is-resourcemgr.h"
#include "../../fimc-is-dt.h"
#include "fimc-is-device-4e6-c2.h"

#define SENSOR_NAME "S5K4E6_C2"
#define DEFAULT_SENSOR_WIDTH	184
#define DEFAULT_SENSOR_HEIGHT	104
#define SENSOR_MEMSIZE DEFAULT_SENSOR_WIDTH * DEFAULT_SENSOR_HEIGHT

#define SENSOR_REG_VIS_DURATION_MSB			(0x6026)
#define SENSOR_REG_VIS_DURATION_LSB			(0x6027)
#define SENSOR_REG_VIS_FRAME_LENGTH_LINE_ALV_MSB	(0x4340)
#define SENSOR_REG_VIS_FRAME_LENGTH_LINE_ALV_LSB	(0x4341)
#define SENSOR_REG_VIS_LINE_LENGTH_PCLK_ALV_MSB		(0x4342)
#define SENSOR_REG_VIS_LINE_LENGTH_PCLK_ALV_LSB		(0x4343)
#define SENSOR_REG_VIS_GAIN_RED				(0x6029)
#define SENSOR_REG_VIS_GAIN_GREEN			(0x602A)
#define SENSOR_REG_VIS_AE_TARGET			(0x600A)
#define SENSOR_REG_VIS_AE_SPEED				(0x5034)
#define SENSOR_REG_VIS_AE_NUMBER_OF_PIXEL_MSB		(0x5030)
#define SENSOR_REG_VIS_AE_NUMBER_OF_PIXEL_LSB		(0x5031)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_1x1_2		(0x6000)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_1x3_4		(0x6001)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_2x1_2		(0x6002)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_2x3_4		(0x6003)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_3x1_2		(0x6004)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_3x3_4		(0x6005)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_4x1_2		(0x6006)
#define SENSOR_REG_VIS_AE_WINDOW_WEIGHT_4x3_4		(0x6007)
#define SENSOR_REG_VIS_AE_MANUAL_EXP_MSB		(0x5039)
#define SENSOR_REG_VIS_AE_MANUAL_EXP_LSB		(0x503A)
#define SENSOR_REG_VIS_AE_MANUAL_ANG_MSB		(0x503B)
#define SENSOR_REG_VIS_AE_MANUAL_ANG_LSB		(0x503C)
#define SENSOR_REG_VIS_BIT_CONVERTING_MSB		(0x602B)
#define SENSOR_REG_VIS_BIT_CONVERTING_LSB		(0x7203)
#define SENSOR_REG_VIS_AE_OFF				(0x5000)

static struct fimc_is_sensor_cfg config_4e6[] = {
	/* 1936x1090@30fps */
	FIMC_IS_SENSOR_CFG(2608, 1960, 30, 19, 0),
#if 0
	/* 1924x1082@30fps */
	FIMC_IS_SENSOR_CFG(1924, 1082, 30, 16, 1),
	/* 1444x1082@30fps */
	FIMC_IS_SENSOR_CFG(1444, 1082, 30, 16, 2),
	/* 1084x1082@30fps */
	FIMC_IS_SENSOR_CFG(1084, 1082, 30, 16, 3),
	/* 964x542@30fps */
	FIMC_IS_SENSOR_CFG(964, 542, 30, 16, 4),
	/* 724x542@30fps */
	FIMC_IS_SENSOR_CFG(724, 542, 30, 16, 5),
	/* 544x542@30fps */
	FIMC_IS_SENSOR_CFG(544, 542, 30, 16, 6),
	/* 320x180@10fps : only for vision(settle) */
	FIMC_IS_SENSOR_CFG(320, 180, 10, 4, 6),
	/* 1936x1090@24fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 24, 13, 7),
#endif
};

static int sensor_4e6_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4e6 *module_4e6;
	struct i2c_client *client;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4e6 = module->private_data;
	client = module->client;

	module_4e6->system_clock = 146 * 1000 * 1000;
	module_4e6->line_length_pck = 146 * 1000 * 1000;

	pr_info("%s\n", __func__);
	/* sensor init */
	fimc_is_sensor_write8(client, 0x4200, 0x01);
	fimc_is_sensor_write8(client, 0x4201, 0x24);
	fimc_is_sensor_write8(client, 0x4305, 0x06);
	fimc_is_sensor_write8(client, 0x4307, 0xC0);
	fimc_is_sensor_write8(client, 0x4342, 0x03);

	fimc_is_sensor_write8(client, 0x4343, 0x90);
	fimc_is_sensor_write8(client, 0x4345, 0x08);
	fimc_is_sensor_write8(client, 0x4347, 0x08);
	fimc_is_sensor_write8(client, 0x4348, 0x07);
	fimc_is_sensor_write8(client, 0x4349, 0x83); /* 10 */

	fimc_is_sensor_write8(client, 0x434A, 0x04);
	fimc_is_sensor_write8(client, 0x434B, 0x3A);
	fimc_is_sensor_write8(client, 0x434C, 0x01);
	fimc_is_sensor_write8(client, 0x434D, 0x40);
	fimc_is_sensor_write8(client, 0x434E, 0x00);

	fimc_is_sensor_write8(client, 0x434F, 0xB4);
	fimc_is_sensor_write8(client, 0x4381, 0x01);
	fimc_is_sensor_write8(client, 0x4383, 0x05);
	fimc_is_sensor_write8(client, 0x4385, 0x06);
	fimc_is_sensor_write8(client, 0x4387, 0x06); /* 20 */

	fimc_is_sensor_write8(client, 0x5004, 0x01);
	fimc_is_sensor_write8(client, 0x5005, 0x1E);
#ifdef VISION_30FPS
	fimc_is_sensor_write8(client, 0x5014, 0x05);
	fimc_is_sensor_write8(client, 0x5015, 0x73);
#else
	fimc_is_sensor_write8(client, 0x5014, 0x13);
	fimc_is_sensor_write8(client, 0x5015, 0x33);
#endif
	fimc_is_sensor_write8(client, 0x5016, 0x00);

	fimc_is_sensor_write8(client, 0x5017, 0x02);
	fimc_is_sensor_write8(client, 0x5030, 0x0E);
	fimc_is_sensor_write8(client, 0x5031, 0x10);
	fimc_is_sensor_write8(client, 0x5034, 0x00);
	fimc_is_sensor_write8(client, 0x5035, 0x02); /* 30 */

	fimc_is_sensor_write8(client, 0x5036, 0x00);
	fimc_is_sensor_write8(client, 0x5037, 0x04);
	fimc_is_sensor_write8(client, 0x5038, 0xC0);
	fimc_is_sensor_write8(client, 0x503D, 0x20);
	fimc_is_sensor_write8(client, 0x503E, 0x70);

	fimc_is_sensor_write8(client, 0x503F, 0x02);
	fimc_is_sensor_write8(client, 0x600A, 0x3A);
	fimc_is_sensor_write8(client, 0x600E, 0x05);
	fimc_is_sensor_write8(client, 0x6014, 0x27);
	fimc_is_sensor_write8(client, 0x6015, 0x1D); /* 40 */

	fimc_is_sensor_write8(client, 0x6018, 0x01);
	fimc_is_sensor_write8(client, 0x6026, 0x00);
#ifdef VISION_30FPS
	fimc_is_sensor_write8(client, 0x6027, 0x1B);
#else
	fimc_is_sensor_write8(client, 0x6027, 0x52);
#endif
	fimc_is_sensor_write8(client, 0x6029, 0x08);
	fimc_is_sensor_write8(client, 0x602A, 0x08);

	fimc_is_sensor_write8(client, 0x602B, 0x00);
	fimc_is_sensor_write8(client, 0x602c, 0x00);
	fimc_is_sensor_write8(client, 0x6032, 0x63);
	fimc_is_sensor_write8(client, 0x6033, 0x94);
	fimc_is_sensor_write8(client, 0x7007, 0x18); /* 50 */

	fimc_is_sensor_write8(client, 0x7015, 0x28);
	fimc_is_sensor_write8(client, 0x7016, 0x2C);
	fimc_is_sensor_write8(client, 0x7027, 0x14);
	fimc_is_sensor_write8(client, 0x7028, 0x3C);
	fimc_is_sensor_write8(client, 0x7029, 0x02);

	fimc_is_sensor_write8(client, 0x702A, 0x02);
	fimc_is_sensor_write8(client, 0x703A, 0x04);
	fimc_is_sensor_write8(client, 0x703B, 0x36);
	fimc_is_sensor_write8(client, 0x7042, 0x04);
	fimc_is_sensor_write8(client, 0x7043, 0x36); /* 60 */

	fimc_is_sensor_write8(client, 0x7058, 0x6F);
	fimc_is_sensor_write8(client, 0x705A, 0x01);
	fimc_is_sensor_write8(client, 0x705C, 0x40);
	fimc_is_sensor_write8(client, 0x7060, 0x07);
	fimc_is_sensor_write8(client, 0x7061, 0x40);

	fimc_is_sensor_write8(client, 0x7064, 0x43);
	fimc_is_sensor_write8(client, 0x706D, 0x77);
	fimc_is_sensor_write8(client, 0x706E, 0xFA);
	fimc_is_sensor_write8(client, 0x7070, 0x0A);
	fimc_is_sensor_write8(client, 0x7073, 0x04); /* 70 */

	fimc_is_sensor_write8(client, 0x7087, 0x00);
	fimc_is_sensor_write8(client, 0x7090, 0x01);
	fimc_is_sensor_write8(client, 0x7115, 0x01);
	fimc_is_sensor_write8(client, 0x7209, 0xF5);
	fimc_is_sensor_write8(client, 0x720B, 0xF5);

	fimc_is_sensor_write8(client, 0x7245, 0xC4);
	fimc_is_sensor_write8(client, 0x7301, 0x02);
	fimc_is_sensor_write8(client, 0x7306, 0x02);
	fimc_is_sensor_write8(client, 0x7339, 0x03);
	fimc_is_sensor_write8(client, 0x7351, 0x00); /* 80 */

	fimc_is_sensor_write8(client, 0x7352, 0xC0);
	fimc_is_sensor_write8(client, 0x7405, 0x28);
	fimc_is_sensor_write8(client, 0x7406, 0x28);
	fimc_is_sensor_write8(client, 0x7407, 0xC0);
	fimc_is_sensor_write8(client, 0x740C, 0x60);

	fimc_is_sensor_write8(client, 0x740D, 0x00);
	fimc_is_sensor_write8(client, 0x7436, 0x03);
	fimc_is_sensor_write8(client, 0x7437, 0x03);
	fimc_is_sensor_write8(client, 0x7454, 0x01);
	fimc_is_sensor_write8(client, 0x7460, 0x00); /* 90 */

	fimc_is_sensor_write8(client, 0x7461, 0x01);
	fimc_is_sensor_write8(client, 0x7462, 0x68);
	fimc_is_sensor_write8(client, 0x7463, 0x1E);
	fimc_is_sensor_write8(client, 0x7464, 0x03);
	fimc_is_sensor_write8(client, 0x7465, 0x4B);

	fimc_is_sensor_write8(client, 0x7467, 0x20);
	fimc_is_sensor_write8(client, 0x7468, 0x20);
	fimc_is_sensor_write8(client, 0x7469, 0x20);
	fimc_is_sensor_write8(client, 0x746A, 0x20);
	fimc_is_sensor_write8(client, 0x746B, 0x20); /* 100 */

	fimc_is_sensor_write8(client, 0x746C, 0x20);
	fimc_is_sensor_write8(client, 0x746D, 0x02);
	fimc_is_sensor_write8(client, 0x746E, 0x80);
	fimc_is_sensor_write8(client, 0x746F, 0x01);
	fimc_is_sensor_write8(client, 0x4100, 0x01);

	pr_info("[MOD:D:%d] %s(%d)\n", module->sensor_id, __func__, val);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_4e6_init
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

#ifdef CONFIG_OF
static int sensor_4e6_c2_power_setpin(struct platform_device *pdev,
	struct exynos_platform_fimc_is_module *pdata)
{
	struct device *dev;
	struct device_node *dnode;
	int gpio_none = 0, gpio_reset = 0, gpio_standby = 0, gpio_comp_rst = 0;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	dnode = dev->of_node;

	gpio_comp_rst = of_get_named_gpio(dnode, "gpio_comp_reset", 0);
	if (!gpio_is_valid(gpio_comp_rst)) {
		dev_err(dev, "failed to get main comp reset gpio\n");
		return -EINVAL;
	}

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get PIN_RESET\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
	if (!gpio_is_valid(gpio_standby)) {
		dev_err(dev, "failed to get gpio_standby\n");
	} else {
		gpio_request_one(gpio_standby, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_standby);
	}

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF);

	/* FRONT CAMERA  - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_standby, "standby_low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "rst_low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_VT", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDA_2.9V_VT", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_VT", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_RET_1.0V_COMP", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_COMP", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDA_1.8V_COMP", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_CORE_0.8V_COMP", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_CORE_1.0V_COMP", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDD_NORET_0.9V_COMP", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_comp_rst, "comp_rst high", PIN_OUTPUT, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 4, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "rst_high", PIN_OUTPUT, 1, 0);

	/* FRONT CAMERA  - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 5, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_standby, "standby_low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, "rst", PIN_RESET, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, "rst_in", PIN_INPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_comp_rst, "comp_rst low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_NORET_0.9V_COMP", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_CORE_1.0V_COMP", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_CORE_0.8V_COMP", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDA_1.8V_COMP", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_COMP", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_RET_1.0V_COMP", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDD_1.2V_VT", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDA_2.9V_VT", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_VT", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_CAM", PIN_REGULATOR, 0, 0);

	/* VISION CAMERA  - POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_reset, "rst_low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_standby, "standby_low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "VDDD_1.2V_VT", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "VDDA_2.9V_VT", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_VT", PIN_REGULATOR, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 2, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON, gpio_reset, "rst_high", PIN_OUTPUT, 1, 0);

	/* VISION CAMERA  - POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_reset, "rst", PIN_RESET, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_reset, "rst_in", PIN_INPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_standby, "standby_low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "VDDD_1.2V_VT", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "VDDA_2.9V_VT", PIN_REGULATOR, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_VT", PIN_REGULATOR, 0, 0);

	return 0;
}
#endif

int sensor_4e6_c2_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	static bool probe_retried = false;
	struct exynos_platform_fimc_is_module *pdata;
	struct device *dev;

	if (!fimc_is_dev)
		goto probe_defer;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &pdev->dev;

#ifdef CONFIG_OF
	fimc_is_sensor_module_parse_dt(pdev, sensor_4e6_c2_power_setpin);
#endif

	pdata = dev_get_platdata(dev);
	device = &core->sensor[pdata->id];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* S5K4E6 */
	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->pdev = pdev;
	module->sensor_id = SENSOR_NAME_S5K4E6;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = 2592;
	module->active_height = 1950;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 30;
	module->position = pdata->position;
	module->mode = CSI_MODE_CH0_ONLY;
	module->lanes = CSI_DATA_LANES_4;
	module->sensor_maker = "SLSI";
	module->sensor_name = "S5K4E6";
	module->setfile_name = "setfile_4e6.bin";
	module->cfgs = ARRAY_SIZE(config_4e6);
	module->cfg = config_4e6;
	module->private_data = kzalloc(sizeof(struct fimc_is_module_4e6), GFP_KERNEL);
	if (!module->private_data) {
		err("private_data is NULL");
		ret = -ENOMEM;
		kfree(subdev_module);
		goto p_err;
	}

	ext = &module->ext;
	ext->mipi_lane_num = module->lanes;
	ext->I2CSclk = I2C_L0;

	ext->sensor_con.product_name = SENSOR_NAME_S5K4E6;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
	ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

	if (pdata->companion_product_name != COMPANION_NAME_NOTHING) {
		ext->companion_con.product_name = pdata->companion_product_name;
		ext->companion_con.peri_info0.valid = true;
		ext->companion_con.peri_info0.peri_type = SE_SPI;
		ext->companion_con.peri_info0.peri_setting.spi.channel = pdata->companion_spi_channel;
		ext->companion_con.peri_info1.valid = true;
		ext->companion_con.peri_info1.peri_type = SE_I2C;
		ext->companion_con.peri_info1.peri_setting.i2c.channel = pdata->companion_i2c_ch;
		ext->companion_con.peri_info1.peri_setting.i2c.slave_address = pdata->companion_i2c_addr;
		ext->companion_con.peri_info1.peri_setting.i2c.speed = 400000;
		ext->companion_con.peri_info2.valid = true;
		ext->companion_con.peri_info2.peri_type = SE_FIMC_LITE;
		ext->companion_con.peri_info2.peri_setting.fimc_lite.channel = FLITE_ID_D;
	} else {
		ext->companion_con.product_name = pdata->companion_product_name;
	}

	v4l2_subdev_init(subdev_module, &subdev_ops);

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

p_err:
	probe_info("%s(%d)\n", __func__, ret);
	return ret;

probe_defer:
	if (probe_retried) {
		err("probe has already been retried!!");
		BUG();
	}

	probe_retried = true;
	err("core device is not yet probed");
	return -EPROBE_DEFER;
}

static int sensor_4e6_c2_remove(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_4e6_c2_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-4e6-c2",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_4e6_c2_match);

static struct platform_driver sensor_4e6_c2_driver = {
	.probe  = sensor_4e6_c2_probe,
	.remove = sensor_4e6_c2_remove,
	.driver = {
		.name   = SENSOR_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_4e6_c2_match,
	}
};

module_platform_driver(sensor_4e6_c2_driver);

static int __init fimc_is_sensor_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sensor_4e6_c2_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}

static void __exit fimc_is_sensor_module_exit(void)
{
	platform_driver_unregister(&sensor_4e6_c2_driver);
}
module_init(fimc_is_sensor_module_init);
module_exit(fimc_is_sensor_module_exit);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor 4E6 driver");
MODULE_LICENSE("GPL v2");

