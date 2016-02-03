/**
 * dwc3-exynos.c - Samsung EXYNOS DWC3 Specific Glue layer
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Anton Tikhomirov <av.tikhomirov@samsung.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/platform_data/dwc3-exynos.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/usb/otg.h>
#include <linux/usb/nop-usb-xceiv.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>
#include <linux/workqueue.h>
#include <linux/muic_tsu6721.h>

#include "../phy/phy-fsm-usb.h"

static const char *dwc3_exynos_clk_names[] = {"aclk", "aclk_axius", "sclk_ref",
	"sclk", "oscclk_phy", "phyclock", "pipe_pclk", "aclk_ahb_usblinkh", NULL};

struct dwc3_exynos_rsw {
	struct otg_fsm		*fsm;
	struct work_struct	work;
	struct notifier_block	muic_usb_notifier;

	int			id_gpio;
	int			b_sess_gpio;
};

struct dwc3_exynos {
	struct platform_device	*usb2_phy;
	struct platform_device	*usb3_phy;
	struct device		*dev;

	struct clk		*clk;
	struct clk		**clocks;

	struct dwc3_exynos_rsw	rsw;
};

void dwc3_otg_run_sm(struct otg_fsm *fsm);

#ifdef CONFIG_OF
static const struct of_device_id exynos_dwc3_match[] = {
	{ .compatible = "samsung,exynos5250-dwusb3" },
	{ .compatible = "samsung,exynos5-dwusb3" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_dwc3_match);
#endif

static inline int dwc3_exynos_get_id_state(struct dwc3_exynos_rsw *rsw)
{
	if (gpio_is_valid(rsw->id_gpio))
		return gpio_get_value(rsw->id_gpio);

	/* use setting before */
	return rsw->fsm->id;
}

static inline int dwc3_exynos_get_b_sess_state(struct dwc3_exynos_rsw *rsw)
{
	if (gpio_is_valid(rsw->b_sess_gpio))
		return gpio_get_value(rsw->b_sess_gpio);
	return rsw->fsm->b_sess_vld;
}

static irqreturn_t dwc3_exynos_rsw_thread_interrupt(int irq, void *_rsw)
{
	struct dwc3_exynos_rsw	*rsw = (struct dwc3_exynos_rsw *)_rsw;
	struct dwc3_exynos	*exynos = container_of(rsw,
						struct dwc3_exynos, rsw);

	dev_vdbg(exynos->dev, "%s\n", __func__);

	dwc3_otg_run_sm(rsw->fsm);

	return IRQ_HANDLED;
}

static void dwc3_exynos_rsw_work(struct work_struct *w)
{
	struct dwc3_exynos_rsw	*rsw = container_of(w,
					struct dwc3_exynos_rsw, work);

	dwc3_exynos_rsw_thread_interrupt(-1, rsw);
}

static irqreturn_t dwc3_exynos_id_interrupt(int irq, void *_rsw)
{
	struct dwc3_exynos_rsw	*rsw = (struct dwc3_exynos_rsw *)_rsw;
	struct dwc3_exynos	*exynos = container_of(rsw,
						struct dwc3_exynos, rsw);
	int			state;

	state = dwc3_exynos_get_id_state(rsw);

	dev_vdbg(exynos->dev, "IRQ: ID: %d\n", state);

	if (rsw->fsm->id != state) {
		rsw->fsm->id = state;
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

static irqreturn_t dwc3_exynos_b_sess_interrupt(int irq, void *_rsw)
{
	struct dwc3_exynos_rsw	*rsw = (struct dwc3_exynos_rsw *)_rsw;
	struct dwc3_exynos	*exynos = container_of(rsw,
						struct dwc3_exynos, rsw);
	int			state;

	state = dwc3_exynos_get_b_sess_state(rsw);

	dev_vdbg(exynos->dev, "IRQ: B_Sess: %sactive\n", state ? "" : "in");

	if (rsw->fsm->b_sess_vld != state) {
		rsw->fsm->b_sess_vld = state;
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

static struct device *dwc3_dev;
/**
 * dwc3_exynos_id_event - receive ID pin state change event.
 *
 * @state : New ID pin state.
 */
int dwc3_exynos_id_event(struct device *dev, int state)
{
	struct dwc3_exynos	*exynos;
	struct dwc3_exynos_rsw	*rsw;
	struct otg_fsm		*fsm;

	if (!dev)
		dev = dwc3_dev;
	dev_dbg(dev, "EVENT: ID: %d\n", state);

	exynos = dev_get_drvdata(dev);
	if (!exynos)
		return -ENOENT;

	rsw = &exynos->rsw;

	fsm = rsw->fsm;
	if (!fsm)
		return -ENOENT;

	if (fsm->id != state) {
		fsm->id = state;
		schedule_work(&rsw->work);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dwc3_exynos_id_event);

/**
 * dwc3_exynos_vbus_event - receive VBus change event.
 *
 * vbus_active : New VBus state, true if active, false otherwise.
 */
int dwc3_exynos_vbus_event(struct device *dev, bool vbus_active)
{
	struct dwc3_exynos	*exynos;
	struct dwc3_exynos_rsw	*rsw;
	struct otg_fsm		*fsm;

	if (!dev)
		dev = dwc3_dev;
	dev_dbg(dev, "EVENT: VBUS: %sactive\n", vbus_active ? "" : "in");

	exynos = dev_get_drvdata(dev);
	if (!exynos)
		return -ENOENT;

	rsw = &exynos->rsw;

	fsm = rsw->fsm;
	if (!fsm)
		return -ENOENT;

	/* usb remove */
	if (vbus_active == 0)
		fsm->suspend_with_vbus = false;

	if (fsm->suspend_with_vbus && fsm->b_sess_vld) {
		pr_info("re-init gadget core\n");
		fsm->b_sess_vld = 0;
		dwc3_otg_run_sm(rsw->fsm);
	}

	if (fsm->b_sess_vld != vbus_active) {
		fsm->b_sess_vld = vbus_active;
		schedule_work(&rsw->work);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dwc3_exynos_vbus_event);

int dwc3_exynos_rsw_start(struct device *dev)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;
	unsigned long		irq_flags = IRQF_TRIGGER_RISING |
					    IRQF_TRIGGER_FALLING;
	int			irq;
	int			ret;

	dev_dbg(dev, "%s\n", __func__);

	if (gpio_is_valid(rsw->id_gpio)) {
		rsw->fsm->id = dwc3_exynos_get_id_state(rsw);

		irq = gpio_to_irq(rsw->id_gpio);
		ret = devm_request_threaded_irq(exynos->dev, irq,
					dwc3_exynos_id_interrupt,
					dwc3_exynos_rsw_thread_interrupt,
					irq_flags, "dwc3_id", rsw);
		if (ret) {
			dev_err(exynos->dev, "failed to request irq #%d --> %d\n",
					irq, ret);
			return ret;
		}
	}

	if (gpio_is_valid(rsw->b_sess_gpio)) {
		rsw->fsm->b_sess_vld = dwc3_exynos_get_b_sess_state(rsw);

		irq = gpio_to_irq(rsw->b_sess_gpio);
		ret = devm_request_threaded_irq(exynos->dev, irq,
					dwc3_exynos_b_sess_interrupt,
					dwc3_exynos_rsw_thread_interrupt,
					irq_flags, "dwc3_b_sess", rsw);
		if (ret) {
			dev_err(exynos->dev, "failed to request irq #%d --> %d\n",
					irq, ret);
			return ret;
		}

		/* enable wake up */
#if 1
		ret = enable_irq_wake(irq);
		if(ret){
			dev_err(exynos->dev, "fail to register wake up sourc b_sess\n");
		}
#endif
	}

	return 0;
}

void dwc3_exynos_rsw_stop(struct device *dev)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;
	int			irq;

	dev_dbg(dev, "%s\n", __func__);

	if (gpio_is_valid(rsw->id_gpio)) {
		irq = gpio_to_irq(rsw->id_gpio);
		devm_free_irq(exynos->dev, irq, rsw);
	}
	if (gpio_is_valid(rsw->b_sess_gpio)) {
		irq = gpio_to_irq(rsw->b_sess_gpio);
		devm_free_irq(exynos->dev, irq, rsw);
	}
}

static int dwc3_usb_switch_notify(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct dwc3_exynos_rsw	*rsw =
		container_of(this, struct dwc3_exynos_rsw, muic_usb_notifier);
	struct dwc3_exynos *exynos = container_of(rsw, struct dwc3_exynos, rsw);

	switch(event){
		case USB_OTG_ATTACH:
			dwc3_exynos_id_event(exynos->dev, 0);
			break;
		case USB_OTG_DETACH:
			dwc3_exynos_id_event(exynos->dev, 1);
			break;
	}

	return NOTIFY_DONE;
}

int dwc3_exynos_rsw_setup(struct device *dev, struct otg_fsm *fsm)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;
	int			ret;

	dev_dbg(dev, "%s\n", __func__);

	if (gpio_is_valid(rsw->id_gpio)) {
		ret = devm_gpio_request(exynos->dev, rsw->id_gpio,
						"dwc3_id_gpio");
		if (ret) {
			dev_err(exynos->dev, "failed to request dwc3 id gpio");
			return ret;
		}
	}

	if (gpio_is_valid(rsw->b_sess_gpio)) {
		ret = devm_gpio_request_one(exynos->dev, rsw->b_sess_gpio,
						GPIOF_IN, "dwc3_b_sess_gpio");
		if (ret) {
			dev_err(exynos->dev, "failed to request dwc3 b_sess gpio");
			return ret;
		}
	}
	rsw->muic_usb_notifier.notifier_call = dwc3_usb_switch_notify;
	ret = register_muic_notifier(&rsw->muic_usb_notifier);
	if (ret) {
		dev_err(exynos->dev, "failed to register muic notifier");
		return ret;
	}

	INIT_WORK(&rsw->work, dwc3_exynos_rsw_work);

	/* B-device by default */
	fsm->id = 1;
	/* Not connected by default */
	fsm->b_sess_vld = 0;

	fsm->suspend_with_vbus = false;
	rsw->fsm = fsm;

	return 0;
}

void dwc3_exynos_rsw_exit(struct device *dev)
{
	struct dwc3_exynos	*exynos = dev_get_drvdata(dev);
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;

	dev_dbg(dev, "%s\n", __func__);

	cancel_work_sync(&rsw->work);

	rsw->fsm = NULL;
}

static struct dwc3_exynos *dwc3_exynos_match(struct device *dev)
{
	struct dwc3_exynos		*exynos = NULL;
	const struct of_device_id	*matches = NULL;

	if (!dev)
		return NULL;

#if IS_ENABLED(CONFIG_OF)
	matches = exynos_dwc3_match;
#endif
	if (of_match_device(matches, dev))
		exynos = dev_get_drvdata(dev);

	return exynos;
}

bool dwc3_exynos_rsw_available(struct device *dev)
{
	struct dwc3_exynos	*exynos;

	exynos = dwc3_exynos_match(dev);
	if (!exynos)
		return false;

	return true;
}

static void dwc3_exynos_rsw_init(struct dwc3_exynos *exynos)
{
	struct device		*dev = exynos->dev;
	struct dwc3_exynos_rsw	*rsw = &exynos->rsw;
	struct pinctrl		*pinctrl;
	unsigned int vbus_enable;
	
	if (!dev->of_node)
		return;

	/* ID gpio */
	rsw->id_gpio = of_get_named_gpio(dev->of_node,
					"samsung,id-gpio", 0);
	if (!gpio_is_valid(rsw->id_gpio))
		dev_info(dev, "id gpio is not available\n");

	/* B-Session gpio */
	rsw->b_sess_gpio = of_get_named_gpio(dev->of_node,
					"samsung,bsess-gpio", 0);
	if (!gpio_is_valid(rsw->b_sess_gpio))
		dev_info(dev, "b_sess gpio is not available\n");

	/* vbus_enable gpio */
	vbus_enable = of_get_named_gpio(dev->of_node,
					"samsung,vbus_en", 0);
	if (!gpio_is_valid(vbus_enable))
		dev_info(dev, "vbus enable is not available\n");
	else {
		gpio_request(vbus_enable,"vbus_enable_pin");
		gpio_direction_output(vbus_enable,1);
	}
	
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl))
		dev_info(exynos->dev, "failed to configure pins\n");
}

static int dwc3_exynos_register_phys(struct dwc3_exynos *exynos)
{
	struct nop_usb_xceiv_platform_data pdata;
	struct platform_device	*pdev;
	int			ret;

	memset(&pdata, 0x00, sizeof(pdata));

	pdev = platform_device_alloc("nop_usb_xceiv", PLATFORM_DEVID_AUTO);
	if (!pdev)
		return -ENOMEM;

	exynos->usb2_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB2;

	ret = platform_device_add_data(exynos->usb2_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err1;

	pdev = platform_device_alloc("nop_usb_xceiv", PLATFORM_DEVID_AUTO);
	if (!pdev) {
		ret = -ENOMEM;
		goto err1;
	}

	exynos->usb3_phy = pdev;
	pdata.type = USB_PHY_TYPE_USB3;

	ret = platform_device_add_data(exynos->usb3_phy, &pdata, sizeof(pdata));
	if (ret)
		goto err2;

	ret = platform_device_add(exynos->usb2_phy);
	if (ret)
		goto err2;

	ret = platform_device_add(exynos->usb3_phy);
	if (ret)
		goto err3;

	return 0;

err3:
	platform_device_del(exynos->usb2_phy);

err2:
	platform_device_put(exynos->usb3_phy);

err1:
	platform_device_put(exynos->usb2_phy);

	return ret;
}

static int dwc3_exynos_remove_child(struct device *dev, void *unused)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int dwc3_exynos_clk_prepare(struct dwc3_exynos *exynos)
{
	int i;
	int ret;

	if (exynos->clk) {
		ret = clk_prepare(exynos->clk);
		if (ret)
			return ret;
	} else {
		for (i = 0; exynos->clocks[i] != NULL; i++) {
			ret = clk_prepare(exynos->clocks[i]);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(exynos->clocks[i]);

	return ret;
}

static int dwc3_exynos_clk_enable(struct dwc3_exynos *exynos)
{
	int i;
	int ret;

	if (exynos->clk) {
		ret = clk_enable(exynos->clk);
		if (ret)
			return ret;
	} else {
		for (i = 0; exynos->clocks[i] != NULL; i++) {
			ret = clk_enable(exynos->clocks[i]);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_disable(exynos->clocks[i]);

	return ret;
}

static void dwc3_exynos_clk_unprepare(struct dwc3_exynos *exynos)
{
	int	i;

	if (exynos->clk) {
		clk_unprepare(exynos->clk);
	} else {
		for (i = 0; exynos->clocks[i] != NULL; i++)
			clk_unprepare(exynos->clocks[i]);
	}
}

static void dwc3_exynos_clk_disable(struct dwc3_exynos *exynos)
{
	int	i;

	if (exynos->clk) {
		clk_disable(exynos->clk);
	} else {
		for (i = 0; exynos->clocks[i] != NULL; i++)
			clk_disable(exynos->clocks[i]);
	}
}

static int dwc3_exynos_clk_get(struct dwc3_exynos *exynos)
{
	const char	*clk_id;
	struct clk	*clk;
	int		i;

	clk_id = "usbdrd30";
	clk = devm_clk_get(exynos->dev, clk_id);
	if (IS_ERR_OR_NULL(clk)) {
		dev_info(exynos->dev, "IP clock gating is N/A\n");
		exynos->clk = NULL;
		/* fallback to separate clock control */
		exynos->clocks = (struct clk **) devm_kmalloc(exynos->dev,
				ARRAY_SIZE(dwc3_exynos_clk_names) *
					sizeof(struct clk *),
				GFP_KERNEL);
		if (!exynos->clocks)
			return -ENOMEM;

		for (i = 0; dwc3_exynos_clk_names[i] != NULL; i++) {
			clk_id = dwc3_exynos_clk_names[i];

			clk = devm_clk_get(exynos->dev, clk_id);
			if (IS_ERR_OR_NULL(clk))
				goto err;

			exynos->clocks[i] = clk;
		}

		exynos->clocks[i] = NULL;

	} else {
		exynos->clk = clk;
	}

	return 0;

err:
	dev_err(exynos->dev, "couldn't get %s clock\n", clk_id);

	return -EINVAL;
}

static int dwc3_exynos_probe(struct platform_device *pdev)
{
	struct dwc3_exynos	*exynos;
	struct device		*dev = &pdev->dev;
	struct device_node	*node = dev->of_node;

	int			ret = -ENOMEM;

	exynos = devm_kzalloc(dev, sizeof(*exynos), GFP_KERNEL);
	if (!exynos) {
		dev_err(dev, "not enough memory\n");
		goto err1;
	}

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	if (!dev->coherent_dma_mask)
		dev->coherent_dma_mask = DMA_BIT_MASK(32);

	platform_set_drvdata(pdev, exynos);

	ret = dwc3_exynos_register_phys(exynos);
	if (ret) {
		dev_err(dev, "couldn't register PHYs\n");
		goto err1;
	}

	exynos->dev	= dev;

	ret = dwc3_exynos_clk_get(exynos);
	if (ret)
		goto err1;

	dwc3_exynos_clk_prepare(exynos);
	dwc3_exynos_clk_enable(exynos);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	dwc3_exynos_rsw_init(exynos);

	if (node) {
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret) {
			dev_err(dev, "failed to add dwc3 core\n");
			goto err2;
		}
	} else {
		dev_err(dev, "no device node, failed to add dwc3 core\n");
		ret = -ENODEV;
		goto err2;
	}
	dwc3_dev = dev;

	return 0;

err2:
	pm_runtime_disable(&pdev->dev);
	dwc3_exynos_clk_disable(exynos);
	dwc3_exynos_clk_unprepare(exynos);
	pm_runtime_set_suspended(&pdev->dev);
err1:
	return ret;
}

static int dwc3_exynos_remove(struct platform_device *pdev)
{
	struct dwc3_exynos	*exynos = platform_get_drvdata(pdev);

	dwc3_dev = NULL;
	device_for_each_child(&pdev->dev, NULL, dwc3_exynos_remove_child);
	platform_device_unregister(exynos->usb2_phy);
	platform_device_unregister(exynos->usb3_phy);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev)) {
		dwc3_exynos_clk_disable(exynos);
		pm_runtime_set_suspended(&pdev->dev);
	}
	dwc3_exynos_clk_unprepare(exynos);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int dwc3_exynos_runtime_suspend(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	dwc3_exynos_clk_disable(exynos);

	return 0;
}

static int dwc3_exynos_runtime_resume(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);
	int ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	ret = dwc3_exynos_clk_enable(exynos);
	if (ret) {
		dev_err(dev, "%s: clk_enable failed\n", __func__);
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int dwc3_exynos_suspend(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	if (exynos->rsw.fsm->b_sess_vld)
		exynos->rsw.fsm->suspend_with_vbus = true;

	dwc3_exynos_clk_disable(exynos);

	return 0;
}

static int dwc3_exynos_resume(struct device *dev)
{
	struct dwc3_exynos *exynos = dev_get_drvdata(dev);
	int ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	ret = dwc3_exynos_clk_enable(exynos);
	if (ret) {
		dev_err(dev, "%s: clk_enable failed\n", __func__);
		return ret;
	}

	/* runtime set active to reflect active state. */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}

static const struct dev_pm_ops dwc3_exynos_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_exynos_suspend, dwc3_exynos_resume)
	SET_RUNTIME_PM_OPS(dwc3_exynos_runtime_suspend,
			dwc3_exynos_runtime_resume, NULL)
};

#define DEV_PM_OPS	(&dwc3_exynos_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */

static struct platform_driver dwc3_exynos_driver = {
	.probe		= dwc3_exynos_probe,
	.remove		= dwc3_exynos_remove,
	.driver		= {
		.name	= "exynos-dwc3",
		.of_match_table = of_match_ptr(exynos_dwc3_match),
		.pm	= DEV_PM_OPS,
	},
};

module_platform_driver(dwc3_exynos_driver);

MODULE_ALIAS("platform:exynos-dwc3");
MODULE_AUTHOR("Anton Tikhomirov <av.tikhomirov@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 EXYNOS Glue Layer");
