/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/i2c.h>
#include <linux/pinctrl/pinctrl-samsung.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/exynos-fimc-is-companion.h>

#include "fimc-is-video.h"
#include "fimc-is-dt.h"
#ifdef CONFIG_COMPANION_USE
#include "fimc-is-companion-dt.h"
#endif
#include "fimc-is-spi.h"
#include "fimc-is-device-companion.h"
#include "fimc-is-sec-define.h"
#if defined(CONFIG_OIS_USE)
#include "fimc-is-device-ois.h"
#endif
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

extern int fimc_is_comp_video_probe(void *data);

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_disp;

int fimc_is_companion_g_module(struct fimc_is_device_companion *device,
	struct fimc_is_module_enum **module)
{
	int ret = 0;

	BUG_ON(!device);

	*module = device->module;
	if (!*module) {
		merr("module is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_companion_mclk_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	if (test_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state)) {
		err("%s : already clk on", __func__);
		goto p_err;
	}

	if (!pdata->mclk_on) {
		err("mclk_on is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_on(pdev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		err("mclk_on is fail(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_mclk_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	if (!test_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state)) {
		err("%s : already clk off", __func__);
		goto p_err;
	}

	if (!pdata->mclk_off) {
		err("mclk_off is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_off(pdev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		err("mclk_off is fail(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_iclk_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	if (test_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state)) {
		err("%s : already clk on", __func__);
		goto p_err;
	}

	if (!pdata->iclk_cfg) {
		err("iclk_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->iclk_on) {
		err("iclk_on is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_cfg(pdev, pdata->scenario, 0);
	if (ret) {
		err("iclk_cfg is fail(%d)", ret);
		goto p_err;
	}

	ret = pdata->iclk_on(pdev, pdata->scenario, 0);
	if (ret) {
		err("iclk_on is fail(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_iclk_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct platform_device *pdev;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdev = device->pdev;
	pdata = device->pdata;

	if (!test_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state)) {
		err("%s : already clk off", __func__);
		goto p_err;
	}

	if (!pdata->iclk_off) {
		err("iclk_off is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_off(pdev, pdata->scenario, 0);
	if (ret) {
		err("iclk_off is fail(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_gpio_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_module_enum *module = NULL;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	if (test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio on", __func__);
		goto p_err;
	}

	ret = fimc_is_companion_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (!test_and_set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state)) {
		struct exynos_platform_fimc_is_module *pdata;

		scenario = device->pdata->scenario;
		pdata = module->pdata;
		if (!pdata) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("pdata is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (!pdata->gpio_cfg) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		ret = pdata->gpio_cfg(module->pdev, scenario, GPIO_SCENARIO_ON);
		if (ret) {
			clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is fail(%d)", ret);
			goto p_err;
		}
	}

	set_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_gpio_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_module_enum *module = NULL;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	if (!test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio off", __func__);
		goto p_err;
	}

	ret = fimc_is_companion_g_module(device, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (test_and_clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state)) {
		struct exynos_platform_fimc_is_module *pdata;

		scenario = device->pdata->scenario;
		pdata = module->pdata;
		if (!pdata) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			merr("pdata is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		if (!pdata->gpio_cfg) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		ret = pdata->gpio_cfg(module->pdev, scenario, GPIO_SCENARIO_OFF);
		if (ret) {
			set_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
			err("gpio_cfg is fail(%d)", ret);
			goto p_err;
		}
	}

	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	return ret;
}

int fimc_is_companion_open(struct fimc_is_device_companion *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;

	BUG_ON(!device);

	if (test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already open");
		ret = -EMFILE;
		goto p_err;
	}

	ret = fimc_is_resource_get(device->resourcemgr, RESOURCE_TYPE_COMPANION);
	if (ret) {
		merr("fimc_is_resource_get is fail", device);
		goto p_err;
	}

	device->vctx = vctx;

	set_bit(FIMC_IS_COMPANION_OPEN, &device->state);

p_err:
	minfo("[COM:D] %s():%d\n", device, __func__, ret);
	return ret;
}

int fimc_is_companion_close(struct fimc_is_device_companion *device)
{
	int ret = 0;

	BUG_ON(!device);

	if (!test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already close");
		ret = -EMFILE;
		goto p_err;
	}

	ret = fimc_is_resource_put(device->resourcemgr, RESOURCE_TYPE_COMPANION);
	if (ret)
		merr("fimc_is_resource_put is fail", device);

	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	clear_bit(FIMC_IS_COMPANION_S_INPUT, &device->state);

p_err:
	minfo("[COM:D] %s(%d)\n", device, __func__, ret);
	return ret;
}

static int fimc_is_companion_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 instance = -1;
	atomic_t device_id;
	struct fimc_is_core *core;
	struct fimc_is_device_companion *device;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!pdev);

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed(companion)");
		pdev->dev.init_name = FIMC_IS_COMPANION_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

#ifdef CONFIG_OF
	ret = fimc_is_companion_parse_dt(pdev);
	if (ret) {
		err("parsing device tree is fail(%d)", ret);
		goto p_err;
	}
#endif

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		err("pdata is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	atomic_set(&device_id, 0);
	device = &core->companion;

	memset(&device->v4l2_dev, 0, sizeof(struct v4l2_device));
	instance = v4l2_device_set_name(&device->v4l2_dev, "exynos-fimc-is-companion", &device_id);
	device->instance = instance;
	device->resourcemgr = &core->resourcemgr;
	device->pdev = pdev;
	device->private_data = core;
	device->regs = core->regs;
	device->pdata = pdata;
	platform_set_drvdata(pdev, device);
	device_init_wakeup(&pdev->dev, true);

	/* init state */
	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	clear_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_S_INPUT, &device->state);

	ret = v4l2_device_register(&pdev->dev, &device->v4l2_dev);
	if (ret) {
		err("v4l2_device_register is fail(%d)", ret);
		goto p_err;
	}

	ret = fimc_is_comp_video_probe(device);
	if (ret) {
		err("fimc_is_companion_video_probe is fail(%d)", ret);
		goto p_err;
	}

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
#endif

p_err:
	info("[%d][COM:D] %s():%d\n", instance, __func__, ret);
	return ret;
}

static int fimc_is_companion_remove(struct platform_device *pdev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static int fimc_is_companion_suspend(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static int fimc_is_companion_resume(struct device *dev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

int fimc_is_companion_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_companion *device;

	info("%s\n", __func__);

	device = (struct fimc_is_device_companion *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* gpio uninit */
	ret = fimc_is_companion_gpio_off(device);
	if (ret) {
		err("fimc_is_companion_gpio_off is fail(%d)", ret);
		goto p_err;
	}

	/* periperal internal clock off */
	ret = fimc_is_companion_iclk_off(device);
	if (ret) {
		err("fimc_is_companion_iclk_off is fail(%d)", ret);
		goto p_err;
	}

	/* companion clock off */
	ret = fimc_is_companion_mclk_off(device);
	if (ret) {
		err("fimc_is_companion_mclk_off is fail(%d)", ret);
		goto p_err;
	}

	if(device->module) {
		device->module = NULL;
	}
p_err:
	info("[COM:D] %s(%d)\n", __func__, ret);
	return ret;
}

int fimc_is_companion_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_companion *device;

	device = (struct fimc_is_device_companion *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		return -EINVAL;
	}

	/* Sensor clock on */
	ret = fimc_is_companion_mclk_on(device);
	if (ret) {
		err("fimc_is_companion_mclk_on is fail(%d)", ret);
		goto p_err;
	}

p_err:
	info("[COM:D] %s(%d)\n", __func__, ret);
	return ret;
}

int fimc_is_companion_s_input(struct fimc_is_device_companion *device,
	u32 input,
	u32 scenario)
{
	int ret = 0;
	struct fimc_is_core *core;
	/* Workaround for Host to use ISP-SPI. Will be removed later.*/
	struct fimc_is_spi_gpio *spi_gpio;
#if defined(CONFIG_OIS_USE)
	struct fimc_is_device_ois *ois_device;
	struct fimc_is_ois_gpio *ois_gpio;
#endif
	struct exynos_platform_fimc_is_companion *pdata;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	BUG_ON(!device);
	BUG_ON(!device->pdata);
	BUG_ON(input >= SENSOR_NAME_END);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	spi_gpio = &core->spi1.gpio;
	pdata = device->pdata;
	pdata->scenario = scenario;

	ret = fimc_is_search_sensor_module(&core->sensor[pdata->id], input, &device->module);
	if (ret) {
		err("fimc_is_search_sensor_module is fail(%d)", ret);
		goto p_err;
	}

#if defined(CONFIG_OIS_USE)
	ois_device  = (struct fimc_is_device_ois *)i2c_get_clientdata(core->client1);
	ois_gpio = &ois_device->gpio;
#endif
	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
	}

	if (test_bit(FIMC_IS_COMPANION_S_INPUT, &device->state)) {
		err("already s_input");
		ret = -EMFILE;
		goto p_err;
	}

	/* gpio init */
	ret = fimc_is_companion_gpio_on(device);
	if (ret) {
		err("fimc_is_companion_gpio_on is fail(%d)", ret);
		goto p_err;
	}

	/* periperal internal clock on */
	ret = fimc_is_companion_iclk_on(device);
	if (ret) {
		err("fimc_is_companion_iclk_on is fail(%d)", ret);
		goto p_err;
	}

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	ret = fimc_is_sec_fw_sel(core, &device->pdev->dev, 0);
	if (ret < 0) {
		err("failed to select firmware (%d)", ret);
		goto p_err;
	}
#endif
	ret = fimc_is_sec_concord_fw_sel(core, &device->pdev->dev);

	/* TODO: loading firmware */
	fimc_is_s_int_comb_isp(core, false, INTMR2_INTMCIS22);

	// Workaround for Host to use ISP-SPI. Will be removed later.
	/* set pin output for Host to use SPI*/
	pin_config_set(spi_gpio->pinname, spi_gpio->ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, FUNC_OUTPUT));

	fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);

	if (fimc_is_comp_is_valid(core) == 0) {
		ret = fimc_is_comp_loadfirm(core);
		if (ret) {
			err("fimc_is_comp_loadfirm() fail");
			goto p_err;
                }
		ret = fimc_is_comp_loadcal(core);
		if (ret) {
			err("fimc_is_comp_loadcal() fail");
		}

#ifdef CONFIG_COMPANION_DCDC_USE
		fimc_is_power_binning(core);
#endif

		ret = fimc_is_comp_loadsetf(core);
		if (ret) {
			err("fimc_is_comp_loadsetf() fail");
			goto p_err;
		}
	}

	// Workaround for Host to use ISP-SPI. Will be removed later.
	/* Set SPI pins to low before changing pin function */
	pin_config_set(spi_gpio->pinname, spi_gpio->clk,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));
	pin_config_set(spi_gpio->pinname, spi_gpio->ssn,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));
	pin_config_set(spi_gpio->pinname, spi_gpio->miso,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));
	pin_config_set(spi_gpio->pinname, spi_gpio->mosi,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));

	/* Set pin function for A5 to use SPI */
	pin_config_set(spi_gpio->pinname, spi_gpio->ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, 2));

	set_bit(FIMC_IS_COMPANION_S_INPUT, &device->state);

#if defined(CONFIG_OIS_USE)
	if(core_pdata->use_ois) {
		if (!core_pdata->use_ois_hsi2c) {
			pin_config_set(ois_gpio->pinname, ois_gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
			pin_config_set(ois_gpio->pinname, ois_gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
		}

		if (!core->ois_ver_read) {
			fimc_is_ois_check_fw(core);
		}

		fimc_is_ois_exif_data(core);

		if (!core_pdata->use_ois_hsi2c) {
			pin_config_set(ois_gpio->pinname, ois_gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
			pin_config_set(ois_gpio->pinname, ois_gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		}
	}
#endif

p_err:
	minfo("[COM:D] %s(%d, %d):%d\n", device, __func__, scenario, input, ret);
	return ret;
}

static const struct dev_pm_ops fimc_is_companion_pm_ops = {
	.suspend		= fimc_is_companion_suspend,
	.resume			= fimc_is_companion_resume,
	.runtime_suspend	= fimc_is_companion_runtime_suspend,
	.runtime_resume		= fimc_is_companion_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_companion_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-companion",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_companion_match);

static struct platform_driver fimc_is_companion_driver = {
	.probe	= fimc_is_companion_probe,
	.remove	= fimc_is_companion_remove,
	.driver = {
		.name	= FIMC_IS_COMPANION_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_companion_pm_ops,
		.of_match_table = exynos_fimc_is_companion_match,
	}
};

module_platform_driver(fimc_is_companion_driver);
#else
static struct platform_device_id fimc_is_companion_driver_ids[] = {
	{
		.name		= FIMC_IS_COMPANION_DEV_NAME,
		.driver_data	= 0,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, fimc_is_companion_driver_ids);

static struct platform_driver fimc_is_companion_driver = {
	.probe	  = fimc_is_companion_probe,
	.remove	  = __devexit_p(fimc_is_companion_remove),
	.id_table = fimc_is_companion_driver_ids,
	.driver	  = {
		.name	= FIMC_IS_COMPANION_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_companion_pm_ops,
	}
};

static int __init fimc_is_companion_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&fimc_is_companion_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}

static void __exit fimc_is_companion_exit(void)
{
	platform_driver_unregister(&fimc_is_companion_driver);
}
module_init(fimc_is_companion_init);
module_exit(fimc_is_companion_exit);
#endif

MODULE_AUTHOR("Wooki Min<wooki.min@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS_COMPANION driver");
MODULE_LICENSE("GPL");
