/* linux/drivers/usb/phy/phy-samsung-dwc-usb2.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Kyounghye Yun <k-hye.yun@samsung.com>
 *
 * Samsung USB2.0 PHY transceiver; talks to S3C HS OTG controller, EHCI-S5P and
 * OHCI-EXYNOS controllers.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/usb/otg.h>
#include <linux/usb/samsung_usb_phy.h>

#include <mach/exynos-pm.h>

#include "phy-samsung-usb.h"
#include "phy-samsung-dwc-usb2.h"

static const char *samsung_dwc_usb2phy_clk_names[] = {"otg_aclk", "otg_hclk",
	"upsizer_otg", "xiu_d_fsys1", "upsizer_ahb_usbhs", NULL};
static const char *samsung_dwc_usb2_phyclk_names[] = {"phy_otg", NULL};

static int samsung_dwc_usb2phy_clk_get(struct samsung_usbphy *sphy)
{
	const char *clk_id;
	struct clk *clk;
	int i;

	dev_info(sphy->dev, "IP clock gating is N/A\n");

	sphy->clocks = (struct clk **) devm_kmalloc(sphy->dev,
			ARRAY_SIZE(samsung_dwc_usb2phy_clk_names) *
				sizeof(struct clk *),
			GFP_KERNEL);
	if (!sphy->clocks)
		return -ENOMEM;

	sphy->phy_clocks = (struct clk **) devm_kmalloc(sphy->dev,
			ARRAY_SIZE(samsung_dwc_usb2_phyclk_names) *
				sizeof(struct clk *),
			GFP_KERNEL);
	if (!sphy->phy_clocks)
		return -ENOMEM;

	for (i = 0; samsung_dwc_usb2phy_clk_names[i] != NULL; i++) {
		clk_id = samsung_dwc_usb2phy_clk_names[i];
		clk = devm_clk_get(sphy->dev, clk_id);
		if (IS_ERR_OR_NULL(clk))
			goto err;

		sphy->clocks[i] = clk;
	}

	sphy->clocks[i] = NULL;

	for (i = 0; samsung_dwc_usb2_phyclk_names[i] != NULL; i++) {
		clk_id = samsung_dwc_usb2_phyclk_names[i];
		clk = devm_clk_get(sphy->dev, clk_id);
		if (IS_ERR_OR_NULL(clk))
			goto err;

		sphy->phy_clocks[i] = clk;
	}

	sphy->phy_clocks[i] = NULL;

	return 0;

err:
	dev_err(sphy->dev, "couldn't get %s clock\n", clk_id);

	return -EINVAL;

}

static int samsung_dwc_usb2phy_clk_prepare(struct samsung_usbphy *sphy)
{
	int i, j;
	int ret;

	for (i = 0; sphy->clocks[i] != NULL; i++) {
		ret = clk_prepare(sphy->clocks[i]);
		if (ret)
			goto err1;
	}
	for (j = 0; sphy->phy_clocks[j] != NULL; j++) {
		ret = clk_prepare(sphy->phy_clocks[j]);
		if (ret)
			goto err2;
	}

	return 0;

err2:
	/* roll back */
	for (j = j - 1; j >= 0; j--)
		clk_unprepare(sphy->phy_clocks[j]);

err1:
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(sphy->clocks[i]);

	return ret;
}

static int samsung_dwc_usb2phy_clk_enable(struct samsung_usbphy *sphy,
						bool umux)
{
	int i;
	int ret;

	if (!umux) {
		for (i = 0; sphy->clocks[i] != NULL; i++) {
			ret = clk_enable(sphy->clocks[i]);
			if (ret)
				goto err1;
		}
	} else {
		/* enable USERMUX for phy clock */
		switch (sphy->phy_type) {
		case USB_PHY_TYPE_DEVICE:
			for (i = 0; sphy->phy_clocks[i] != NULL; i++) {
				ret = clk_enable(sphy->phy_clocks[i]);
				if (ret)
					goto err2;

		}
			break;

		case USB_PHY_TYPE_HOST:
			break;
		}
	}

	return 0;
err1:
	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_disable(sphy->clocks[i]);

	return ret;
err2:
	for (i = i - 1; i >= 0; i--)
		clk_disable(sphy->phy_clocks[i]);

	return ret;
}

static void samsung_dwc_usb2phy_clk_unprepare(struct samsung_usbphy *sphy)
{
	int i;

	for (i = 0; sphy->clocks[i] != NULL; i++)
		clk_unprepare(sphy->clocks[i]);

	for (i = 0; sphy->phy_clocks[i] != NULL; i++)
		clk_unprepare(sphy->phy_clocks[i]);
}

static void samsung_dwc_usb2phy_clk_disable(struct samsung_usbphy *sphy,
						bool umux)
{
	int i;

	if (!umux) {
		for (i = 0; sphy->clocks[i] != NULL; i++)
			clk_disable(sphy->clocks[i]);
	} else {
		/* disable USERMUX for phy clock */
		switch (sphy->phy_type) {
		case USB_PHY_TYPE_DEVICE:
			for (i = 0; sphy->phy_clocks[i] != NULL; i++)
				clk_disable(sphy->phy_clocks[i]);
			break;

		case USB_PHY_TYPE_HOST:
			break;
		}
	}
}

static int samsung_dwc_usbphy_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	if (!otg)
		return -ENODEV;

	if (!otg->host)
		otg->host = host;

	return 0;
}

static void samsung_dwc_usb2phy_enable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;

	/*
	 * CAL (Chip Abstraction Layer) is used to keep synchronization
	 * between kernel code and F/W verification code for the manageability.
	 * When CAL option is enabled, CAL enable function will be called,
	 * and the legacy enable routine will be skipped.
	 */

	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos_cal_dwc_usb2phy_enable(regs, sphy->ref_clk_freq,
					sphy->phy_type);
		return;
	}
}

static void samsung_dwc_usb2phy_disable(struct samsung_usbphy *sphy)
{
	void __iomem *regs = sphy->regs;

	if (IS_ENABLED(CONFIG_SAMSUNG_USB2PHY_CAL)) {
		samsung_exynos_cal_dwc_usb2phy_disable(regs);
		return;
	}
}

/*
 * The function passed to the usb driver for phy initialization
 */
static int samsung_dwc_usb2phy_init(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy;
	struct usb_bus *host = NULL;
	unsigned long flags;
	int ret = 0;

	sphy = phy_to_sphy(phy);

	dev_vdbg(sphy->dev, "%s\n", __func__);

	host = phy->otg->host;

	/* Enable the phy clock */
	ret = samsung_dwc_usb2phy_clk_enable(sphy, false);
	if (ret) {
		dev_err(sphy->dev, "%s: clk_enable failed\n", __func__);
		return ret;
	}

	spin_lock_irqsave(&sphy->lock, flags);

	sphy->usage_count++;

	if (sphy->usage_count - 1) {
		dev_vdbg(sphy->dev, "PHY is already initialized\n");
		goto exit;
	}

	if (host) {
		/* setting default phy-type for USB 2.0 */
		if (!strstr(dev_name(host->controller), "ehci") ||
				!strstr(dev_name(host->controller), "ohci"))
			samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_HOST);
	} else {
		samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_DEVICE);
	}

	/* Disable phy isolation */
	samsung_usbphy_set_isolation(sphy, false);

	/* Selecting Host/OTG mode; After reset USB2.0PHY_CFG: HOST */
	samsung_usbphy_cfg_sel(sphy);

	/* Initialize usb phy registers */
	samsung_dwc_usb2phy_enable(sphy);

	/* Enable Usermux for PHY Clock from PHY
	   USERMUX should be enabled after PHY initialization*/
	ret = samsung_dwc_usb2phy_clk_enable(sphy, true);
	if (ret) {
		dev_err(sphy->dev, "%s: user mux enable failed\n"
				, __func__);
		spin_unlock_irqrestore(&sphy->lock, flags);
		return ret;
	}

exit:
	spin_unlock_irqrestore(&sphy->lock, flags);

	dev_dbg(sphy->dev, "end of %s\n", __func__);

	return ret;
}

/*
 * The function passed to the usb driver for phy shutdown
 */
static void samsung_dwc_usb2phy_shutdown(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy;
	struct usb_bus *host = NULL;
	unsigned long flags;

	sphy = phy_to_sphy(phy);

	dev_vdbg(sphy->dev, "%s\n", __func__);

	host = phy->otg->host;

	spin_lock_irqsave(&sphy->lock, flags);

	if (!sphy->usage_count) {
		dev_vdbg(sphy->dev, "PHY is already shutdown\n");
		goto exit;
	}

	sphy->usage_count--;

	if (sphy->usage_count) {
		dev_vdbg(sphy->dev, "PHY is still in use\n");
		goto exit;
	}

	if (host) {
		/* setting default phy-type for USB 2.0 */
		if (!strstr(dev_name(host->controller), "ehci") ||
				!strstr(dev_name(host->controller), "ohci"))
			samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_HOST);
	} else {
		samsung_usbphy_set_type(&sphy->phy, USB_PHY_TYPE_DEVICE);
	}

	samsung_dwc_usb2phy_clk_disable(sphy, true);

	/* De-initialize usb phy registers */
	samsung_dwc_usb2phy_disable(sphy);

	/* Enable phy isolation */
	samsung_usbphy_set_isolation(sphy, true);

	dev_dbg(sphy->dev, "%s: End of setting for shutdown\n", __func__);
exit:
	spin_unlock_irqrestore(&sphy->lock, flags);

	samsung_dwc_usb2phy_clk_disable(sphy, false);
}

static bool samsung_dwc_usb2phy_is_active(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy = phy_to_sphy(phy);

	return !!sphy->usage_count;
}

static int
samsung_dwc_usb2phy_lpa_event(struct notifier_block *nb,
			  unsigned long event,
			  void *data)
{
	struct samsung_usbphy *sphy = container_of(nb,
					struct samsung_usbphy, lpa_nb);
	int err = 0;

	switch (event) {
	case LPA_PREPARE:
		if (samsung_dwc_usb2phy_is_active(&sphy->phy))
			err = -EBUSY;
		break;
	default:
		return NOTIFY_DONE;
	}

	return notifier_from_errno(err);
}

static int samsung_dwc_usb2phy_probe(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy;
	struct usb_otg *otg;
	const struct samsung_usbphy_drvdata *drv_data;
	struct device *dev = &pdev->dev;
	struct resource *phy_mem;
	void __iomem	*phy_base;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(dev, "This driver is required to be instantiated from device tree \n");
		return -EINVAL;
	}

	phy_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_base = devm_ioremap_resource(dev, phy_mem);
	if (IS_ERR(phy_base))
		return PTR_ERR(phy_base);

	sphy = devm_kzalloc(dev, sizeof(*sphy), GFP_KERNEL);
	if (!sphy)
		return -ENOMEM;

	otg = devm_kzalloc(dev, sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return -ENOMEM;

	drv_data = samsung_usbphy_get_driver_data(pdev);

	sphy->dev = dev;

	/* get clk for OTG*/
	ret = samsung_dwc_usb2phy_clk_get(sphy);
	if (ret)
		return ret;

	ret = samsung_usbphy_parse_dt(sphy);
	if (ret < 0)
		return ret;

	sphy->regs		= phy_base;
	sphy->drv_data		= drv_data;
	sphy->phy.dev		= sphy->dev;
	sphy->phy.label		= "samsung-usb2phy";
	sphy->phy.type		= USB_PHY_TYPE_USB2;
	sphy->phy.init		= samsung_dwc_usb2phy_init;
	sphy->phy.shutdown	= samsung_dwc_usb2phy_shutdown;
	sphy->phy.is_active	= samsung_dwc_usb2phy_is_active;
	sphy->ref_clk_freq	= samsung_usbphy_get_refclk_freq(sphy);

	sphy->phy.otg		= otg;
	sphy->phy.otg->phy	= &sphy->phy;
	sphy->phy.otg->set_host = samsung_dwc_usbphy_set_host;

	spin_lock_init(&sphy->lock);

	ret = samsung_dwc_usb2phy_clk_prepare(sphy);
	if (ret) {
		dev_err(dev, "clk_prepare failed\n");
		return ret;
	}

	platform_set_drvdata(pdev, sphy);

	ret = usb_add_phy_dev(&sphy->phy);
	if (ret) {
		dev_err(dev, "Failed to add PHY\n");
		goto err1;
	}

	sphy->lpa_nb.notifier_call = samsung_dwc_usb2phy_lpa_event;
	sphy->lpa_nb.next = NULL;
	sphy->lpa_nb.priority = 0;

	ret = exynos_pm_register_notifier(&sphy->lpa_nb);
	if (ret)
		dev_err(dev, "Failed to register lpa notifier\n");

	return 0;

err1:
	samsung_dwc_usb2phy_clk_unprepare(sphy);

	return ret;
}

static int samsung_dwc_usb2phy_remove(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy = platform_get_drvdata(pdev);

	exynos_pm_unregister_notifier(&sphy->lpa_nb);
	usb_remove_phy(&sphy->phy);
	samsung_dwc_usb2phy_clk_unprepare(sphy);

	if (sphy->pmuregs)
		iounmap(sphy->pmuregs);
	if (sphy->sysreg)
		iounmap(sphy->sysreg);

	return 0;
}

static struct samsung_usbphy_drvdata dwc_usb2phy_exynos7580 = {
	.cpu_type		= TYPE_EXYNOS7580,
	.devphy_reg_offset	= EXYNOS_USB_PHY_CTRL_OFFSET,
	.devphy_en_mask		= EXYNOS_USBPHY_ENABLE,
};

static const struct of_device_id samsung_usbphy_dt_match[] = {
	{
		.compatible = "samsung,exynos7580-dwc-usb2phy",
		.data = &dwc_usb2phy_exynos7580,
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_usbphy_dt_match);

static struct platform_driver samsung_dwc_usb2phy_driver = {
	.probe		= samsung_dwc_usb2phy_probe,
	.remove		= samsung_dwc_usb2phy_remove,
	.driver		= {
		.name	= "samsung-dwc-usb2phy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_usbphy_dt_match),
	},
};

module_platform_driver(samsung_dwc_usb2phy_driver);

MODULE_DESCRIPTION("Samsung DWC USB2.0 PHY controller");
MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:samsung-dwc-usb2phy");
