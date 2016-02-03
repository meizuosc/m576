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

#include "fimc-is-video.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-companion.h"
#include "fimc-is-sec-define.h"
#if defined(CONFIG_OIS_USE)
#include "fimc-is-device-ois.h"
#endif
#ifdef CONFIG_COMPANION_USE
#include "fimc-is-companion-dt.h"
#endif
extern int fimc_is_comp_video_probe(void *data);

int fimc_is_companion_wait(struct fimc_is_device_companion *device)
{
	int ret = 0;

	ret = wait_event_timeout(device->init_wait_queue,
		(device->companion_status == FIMC_IS_COMPANION_OPENDONE),
		FIMC_IS_COMPANION_TIMEOUT);
	if (ret) {
		ret = 0;
	} else {
		err("timeout");
		device->companion_status = FIMC_IS_COMPANION_IDLE;
		ret = -ETIME;
	}

	return ret;
}

static void fimc_is_companion_wakeup(struct fimc_is_device_companion *device)
{
	wake_up(&device->init_wait_queue);
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
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio on", __func__);
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_cfg(device->pdev, pdata->scenario, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_gpio_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_companion *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio off", __func__);
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_cfg(device->pdev, pdata->scenario, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	return ret;
}


int fimc_is_companion_open(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct fimc_is_core *core;
	/* Workaround for Host to use ISP-SPI. Will be removed later.*/
	struct fimc_is_spi_gpio *spi_gpio;
#if defined(USE_VENDER_SENSOR)
	static char companion_fw_name[100];
	static char master_setf_name[100];
	static char mode_setf_name[100];
	static char fw_name[100];
	static char setf_name[100];
#endif

	BUG_ON(!device);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	spi_gpio = &core->spi_gpio;

	if (test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already open");
		ret = -EMFILE;
		goto p_err;
	}

	device->companion_status = FIMC_IS_COMPANION_OPENNING;
	core->running_rear_camera = true;
#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_get_sync(&device->pdev->dev);
#else
	fimc_is_companion_runtime_resume(&device->pdev->dev);
#endif
#if defined(USE_VENDER_SENSOR)
	ret = fimc_is_sec_fw_sel(core, &device->pdev->dev, fw_name, setf_name, 0);
	if (ret < 0) {
		err("failed to select firmware (%d)", ret);
		goto p_err_pm;
	}

	ret = fimc_is_sec_concord_fw_sel(core, &device->pdev->dev, companion_fw_name, master_setf_name, mode_setf_name, 0);
#endif

	/* TODO: loading firmware */
	fimc_is_s_int_comb_isp(core, false, INTMR2_INTMCIS22);

	// Workaround for Host to use ISP-SPI. Will be removed later.
	/* set pin output for Host to use SPI*/
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, FUNC_OUTPUT));

	fimc_is_set_spi_config(spi_gpio, FIMC_IS_SPI_FUNC, false);

	if (fimc_is_comp_is_valid(core) == 0) {
		ret = fimc_is_comp_loadfirm(core);
		if (ret) {
			err("fimc_is_comp_loadfirm() fail");
			goto p_err_pm;
                }
		ret = fimc_is_comp_loadcal(core);
		if (ret) {
			err("fimc_is_comp_loadcal() fail");
		}

		fimc_is_power_binning(core);

		ret = fimc_is_comp_loadsetf(core);
		if (ret) {
			err("fimc_is_comp_loadsetf() fail");
			goto p_err_pm;
		}
	}

	// Workaround for Host to use ISP-SPI. Will be removed later.
	/* Set SPI pins to low before changing pin function */
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_sclk,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_ssn,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_miso,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_mois,
		PINCFG_PACK(PINCFG_TYPE_DAT, 0));

	/* Set pin function for A5 to use SPI */
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_ssn,
		PINCFG_PACK(PINCFG_TYPE_FUNC, 2));

	set_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	device->companion_status = FIMC_IS_COMPANION_OPENDONE;
	fimc_is_companion_wakeup(device);

#if defined(CONFIG_OIS_USE)
	if(core->use_ois) {
		if (!core->use_ois_hsi2c) {
			pin_config_set(FIMC_IS_SPI_PINNAME, "gpc2-2",
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
			pin_config_set(FIMC_IS_SPI_PINNAME, "gpc2-3",
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
		}

		if (!core->ois_ver_read) {
			fimc_is_ois_check_fw(core);
		}

		fimc_is_ois_exif_data(core);

		if (!core->use_ois_hsi2c) {
			pin_config_set(FIMC_IS_SPI_PINNAME, "gpc2-2",
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
			pin_config_set(FIMC_IS_SPI_PINNAME, "gpc2-3",
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		}
	}
#endif

	info("[COMP:D] %s(%d)status(%d)\n", __func__, ret, device->companion_status);
	return ret;

p_err_pm:
#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_put_sync(&device->pdev->dev);
#else
	fimc_is_companion_runtime_suspend(&device->pdev->dev);
#endif

p_err:
	err("[COMP:D] open fail(%d)status(%d)", ret, device->companion_status);
	return ret;
}

int fimc_is_companion_close(struct fimc_is_device_companion *device)
{
	int ret = 0;
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	u32 timeout;
#endif
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

	BUG_ON(!device);

	if (!test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already close");
		ret = -EMFILE;
		goto p_err;
	}

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_put_sync(&device->pdev->dev);
#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
	if (core != NULL && !test_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state)) {
		warn("only companion device closing after open..");
		timeout = 2000;
		while ((readl(PMUREG_CAM1_STATUS) & 0x1) && timeout) {
			timeout--;
			usleep_range(1000, 1000);
			if (!(timeout % 100))
				warn("wait for CAM1 power down..(%d)", timeout);
		}
		if (timeout == 0)
			err("CAM1 power down failed(CAM1:0x%08x, A5:0x%08x)\n",
					readl(PMUREG_CAM1_STATUS), readl(PMUREG_ISP_ARM_STATUS));
	}
#endif
#else
	fimc_is_companion_runtime_suspend(&device->pdev->dev);
#endif /* CONFIG_PM_RUNTIME */

	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);

p_err:
	core->running_rear_camera = false;
	device->companion_status = FIMC_IS_COMPANION_IDLE;
	info("[COMP:D] %s(%d)\n", __func__, ret);
	return ret;
}

static int fimc_is_companion_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct fimc_is_device_companion *device;
	void *pdata;

	BUG_ON(!pdev);

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed");
		pdev->dev.init_name = FIMC_IS_COMPANION_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}


	device = kzalloc(sizeof(struct fimc_is_device_companion), GFP_KERNEL);
	if (!device) {
		err("fimc_is_device_companion is NULL");
		return -ENOMEM;
	}

	init_waitqueue_head(&device->init_wait_queue);

	device->companion_status = FIMC_IS_COMPANION_IDLE;

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

	device->pdev = pdev;
	device->private_data = core;
	device->regs = core->regs;
	device->pdata = pdata;
	platform_set_drvdata(pdev, device);
	device_init_wakeup(&pdev->dev, true);
	core->companion = device;
#ifdef CONFIG_OIS_USE
	core->pin_ois_en = device->pdata->pin_ois_en;
#endif

	/* init state */
	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	clear_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

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

	info("[COMP:D] %s(%d)\n", __func__, ret);

	return ret;

p_err:
	kfree(device);
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
#ifdef CONFIG_AF_HOST_CONTROL
	struct fimc_is_core *core;
#endif

	info("%s\n", __func__);

#ifdef CONFIG_AF_HOST_CONTROL
	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}
#endif
	device = (struct fimc_is_device_companion *)platform_get_drvdata(pdev);
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto err_dev_null;
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

p_err:
	info("[COMP:D] %s(%d)\n", __func__, ret);
err_dev_null:
	return ret;
}

int fimc_is_companion_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_device_companion *device;
#ifdef CONFIG_AF_HOST_CONTROL
	struct fimc_is_core *core;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}
#endif
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
p_err:
	info("[COMP:D] %s(%d)\n", __func__, ret);
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
