/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/syscore_ops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>

#include <mach/exynos-pm.h>
#include <plat/cpu.h>

#include "pci-noti.h"
#include "pcie-designware.h"
#include "pci-exynos.h"

#if defined(CONFIG_SOC_EXYNOS5433)
#include "pci-exynos5433.c"
#elif defined(CONFIG_SOC_EXYNOS7420)
#include "pci-exynos7420.c"
#include "pci-exynos7420_cal.c"
#endif

static struct exynos_pcie g_pcie[MAX_RC_NUM];

#if defined(CONFIG_BATTERY_SAMSUNG)
extern unsigned int lpcharge;
#endif

#ifdef CONFIG_CPU_IDLE
static struct raw_notifier_head pci_lpa_nh =
		RAW_NOTIFIER_INIT(pci_lpa_nh);
#endif

#ifdef CONFIG_PCI_EXYNOS_TEST
int wlan_gpio, bt_gpio;
#endif

#ifdef CONFIG_CPU_IDLE
static int exynos_pci_lpa_event(struct notifier_block *nb, unsigned long event, void *data);
#endif
static void exynos_pcie_resumed_phydown(struct pcie_port *);
static void exynos_pcie_sideband_dbi_r_mode(struct pcie_port *, bool);
static void exynos_pcie_sideband_dbi_w_mode(struct pcie_port *, bool);
static void exynos_pcie_assert_phy_reset(struct pcie_port *);
static int exynos_pcie_rd_own_conf(struct pcie_port *pp, int where, int size, u32 *val);
#if defined(CONFIG_PCI_MSI)
static void exynos_pcie_clear_irq_level(struct pcie_port *pp);
static irqreturn_t exynos_pcie_msi_irq_handler(int irq, void *arg);
static void exynos_pcie_msi_init(struct pcie_port *pp);
#endif


static ssize_t show_pcie(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{

	return snprintf(buf, PAGE_SIZE, "0: send pme turn off message\n");
}

static ssize_t store_pcie(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int enable;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (sscanf(buf, "%10d", &enable) != 1)
		return -EINVAL;

	if (enable == 0) {
		writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
		writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		gpio_set_value(exynos_pcie->perst_gpio, 0);
		writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);

	}
#ifdef CONFIG_PCI_EXYNOS_TEST
	else if (enable == 1) {
		gpio_set_value(bt_gpio, 1);
		gpio_set_value(wlan_gpio, 1);
		mdelay(100);
		exynos_pcie_poweron(exynos_pcie->ch_num);
	}
#endif

	return count;
}

static DEVICE_ATTR(pcie_sysfs, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			show_pcie, store_pcie);

static inline int create_pcie_sys_file(struct device *dev)
{
	return device_create_file(dev, &dev_attr_pcie_sysfs);
}

static inline void remove_pcie_sys_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_pcie_sysfs);
}

static void exynos_pcie_notify_callback(struct pcie_port *pp, int event)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	if (exynos_pcie->event_reg && exynos_pcie->event_reg->callback &&
			(exynos_pcie->event_reg->events & event)) {
		struct exynos_pcie_notify *notify = &exynos_pcie->event_reg->notify;
		notify->event = event;
		notify->user = exynos_pcie->event_reg->user;
		dev_info(pp->dev, "Callback for the event : %d\n", event);
		exynos_pcie->event_reg->callback(notify);
	} else {
		dev_info(pp->dev,
			"Client driver does not have registration of the event : %d\n", event);
	}
}

int exynos_pcie_reset(struct pcie_port *pp)
{
	struct device *dev = pp->dev;
	struct pinctrl *pinctrl_reset;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;
	u32 tmp0, tmp1;
	int count = 0, try_cnt = 0;
	void __iomem *phy_base = exynos_pcie->phy_base;

retry:
	if (soc_is_exynos7420() && exynos_pcie->ch_num == 0) {
		/* set #PERST Low */
		gpio_set_value(exynos_pcie->perst_gpio, 0);
		msleep(15);

		/* set RxElecIdle = 0 to access DBI area */
		tmp0 = readl(phy_base + 0x4A*4);
		tmp1 = readl(phy_base + 0x5C*4);
		writel(0xDF, phy_base + 0x4A*4);
		writel(0x54, phy_base + 0x5C*4);
	}


	/* link reset by using wake pin */
	if (soc_is_exynos5433()) {
		writel((readl(exynos_pcie->gpio_base + 0x8) & ~(0x3 << 12)) | (0x1 << 12),
				exynos_pcie->gpio_base + 0x8);
		udelay(20);
		writel(readl(exynos_pcie->gpio_base + 0x8) | (0x3 << 12),
				exynos_pcie->gpio_base + 0x8);
	} else if (soc_is_exynos7420()) {
		pinctrl_reset = devm_pinctrl_get_select(dev, "pcie_reset");
		if (IS_ERR(pinctrl_reset))
			dev_err(dev, "failed pcie link reset\n");

		udelay(20);

		pinctrl_reset = devm_pinctrl_get_select_default(dev);
		if (IS_ERR(pinctrl_reset))
			dev_err(dev, "failed pcie link reset\n");
	}

	/* set #PERST high */
	gpio_set_value(exynos_pcie->perst_gpio, 1);
	if (soc_is_exynos7420() && exynos_pcie->ch_num == 0)
		msleep(15);
	else
#ifdef CONFIG_PCI_EXYNOS_REDUCE_RESET_WAIT
		usleep_range(18000, 20000);
#else
		msleep(80);
#endif

	exynos_pcie_assert_phy_reset(pp);

	writel(0x1, exynos_pcie->elbi_base + PCIE_L1_BUG_FIX_ENABLE);

	if (soc_is_exynos5433()) {
		exynos_pcie_sideband_dbi_r_mode(pp, true);
		exynos_pcie_sideband_dbi_w_mode(pp, true);
	}

	/* setup root complex */
	dw_pcie_setup_rc(pp);

	/* Rollback PHY Changes for clearing RxElecIdle */
	if (soc_is_exynos7420() && exynos_pcie->ch_num == 0) {
		writel(tmp1, phy_base + 0x5C*4);
		writel(tmp0, phy_base + 0x4A*4);
	}

	if (soc_is_exynos5433()) {
		exynos_pcie_sideband_dbi_r_mode(pp, false);
		exynos_pcie_sideband_dbi_w_mode(pp, false);
	}

	dev_info(dev, "D state: %x, %x\n",
			readl(exynos_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7,
			readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP));

	/* assert LTSSM enable */
	writel(PCIE_ELBI_LTSSM_ENABLE, exynos_pcie->elbi_base + PCIE_APP_LTSSM_ENABLE);
	count = 0;
	while (count < MAX_TIMEOUT) {
		val = readl(exynos_pcie->elbi_base + PCIE_IRQ_LEVEL) & 0x10;
		if (val)
			break;

		count++;

		udelay(10);
	}

	/* wait to check whether link down again(D0 UNINIT) or not for retry */
	usleep_range(1000,1000);//msleep(1);
	val = readl(exynos_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7;
	if (count >= MAX_TIMEOUT || val == PCIE_D0_UNINIT_STATE) {
		try_cnt++;
		dev_err(dev, "%s: Link is not up, try count: %d, %x\n", __func__, try_cnt,
				readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP));
		if (try_cnt < 10) {
			gpio_set_value(exynos_pcie->perst_gpio, 0);
			/* LTSSM disable */
			writel(PCIE_ELBI_LTSSM_DISABLE,
					exynos_pcie->elbi_base + PCIE_APP_LTSSM_ENABLE);

			goto retry;
		} else {
			//BUG_ON(1);
			return -EPIPE;
		}
	} else {
		writel(readl(exynos_pcie->elbi_base + PCIE_IRQ_SPECIAL),
				exynos_pcie->elbi_base + PCIE_IRQ_SPECIAL);
		dev_info(dev, "%s: Link up:%x\n", __func__,
				readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) );

		if (soc_is_exynos5433())
			queue_delayed_work(exynos_pcie->pcie_wq,
				&exynos_pcie->work, msecs_to_jiffies(1000));
		else if (soc_is_exynos7420())
			writel(readl(exynos_pcie->elbi_base + PCIE_IRQ_EN_LEVEL)
				| IRQ_LINKDOWN_ENABLE,
				exynos_pcie->elbi_base + PCIE_IRQ_EN_LEVEL);
	}

	/* setup ATU for cfg/mem outbound */
	dw_pcie_prog_viewport_cfg0(pp, 0x1000000);
	dw_pcie_prog_viewport_mem_outbound(pp);

	/* L1.2 ASPM enable */
	dw_pcie_config_l1ss(pp);

	return 0;
}

static void handle_wake_func(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie = container_of(work, struct exynos_pcie,
			handle_wake_work);
	struct pcie_port *pp = &exynos_pcie->pp;
	struct device *dev = pp->dev;
	dev_info(dev, "PCIe: Wake work for RC%d\n", exynos_pcie->ch_num);

	if (exynos_pcie->probe_ok) {
		exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_WAKEUP);
	} else {
		exynos_pcie_poweron(exynos_pcie->ch_num);
		exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKUP);
	}
}

#if defined(CONFIG_BCMDHD_PCIE)
extern void dhd_host_recover_link(void);
#endif
void exynos_pcie_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie = container_of(work, struct exynos_pcie, work.work);
	struct pcie_port *pp = &exynos_pcie->pp;
	struct device *dev = pp->dev;
	u32 val;

	if (exynos_pcie->state == STATE_LINK_DOWN)
		return;

	mutex_lock(&exynos_pcie->lock);

	if (soc_is_exynos5433()) {
		val = readl(exynos_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7;

		/* check whether D0 uninit state and link down or not */
		if ((val != PCIE_D0_UNINIT_STATE) &&
				(~(readl(exynos_pcie->elbi_base + PCIE_IRQ_SPECIAL)) & (0x1 << 2))) {
			queue_delayed_work(exynos_pcie->pcie_wq, &exynos_pcie->work,
					msecs_to_jiffies(1000));
			goto exit;
		}

		if (val == PCIE_D0_UNINIT_STATE)
			dev_info(dev, "%s: d0uninit state\n", __func__);

		if (readl(exynos_pcie->elbi_base + PCIE_IRQ_SPECIAL) & (0x1 << 2))
			dev_info(dev, "%s: link down event\n", __func__);
	}

	exynos_pcie->d0uninit_cnt++;
	dev_info(dev, "link down and recovery cnt: %d\n", exynos_pcie->d0uninit_cnt);

	if (soc_is_exynos7420()) {
		if (exynos_pcie->ch_num == 0) {
			exynos_pcie_notify_callback(pp, EXYNOS_PCIE_EVENT_LINKDOWN);
		} else if (exynos_pcie->ch_num == 1) {
#ifdef CONFIG_PCI_EXYNOS_TEST
			exynos_pcie_poweroff(exynos_pcie->ch_num);
			exynos_pcie_poweron(exynos_pcie->ch_num);
#endif
#if defined(CONFIG_BCMDHD_PCIE)
	/* wifi off and on */
	dhd_host_recover_link();
#endif
		}
	}

exit:
	mutex_unlock(&exynos_pcie->lock);
}

static void exynos_pcie_sideband_dbi_w_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (on) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
	} else {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_AWMISC);
	}
}

static void exynos_pcie_sideband_dbi_r_mode(struct pcie_port *pp, bool on)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (on) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
		val |= PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
	} else {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
		val &= ~PCIE_ELBI_SLV_DBI_ENABLE;
		writel(val, exynos_pcie->elbi_base + PCIE_ELBI_SLV_ARMISC);
	}
}

static void exynos_pcie_assert_phy_reset(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *block_base = exynos_pcie->block_base;
	void __iomem *elbi_base = exynos_pcie->elbi_base;
	void __iomem *phy_base = exynos_pcie->phy_base;
	void __iomem *phy_pcs_base = exynos_pcie->phy_pcs_base;
	u32 val;

#if defined(CONFIG_SOC_EXYNOS5433)
	/* PHY Common Reset */
	val = readl(block_base + PCIE_PHY_COMMON_RESET);
	val |= 0x1;
	writel(val, block_base + PCIE_PHY_COMMON_RESET);

	/* PHY Mac Reset */
	val = readl(block_base + PCIE_PHY_MAC_RESET);
	val &= ~(0x1 << 4);
	writel(val, block_base + PCIE_PHY_MAC_RESET);

	/* PHY refclk 24MHz */
	val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
	val &= ~(0x10);
	val &= ~(0x06);
	val |= 0x1 << 1;
	writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

	/* PHY Global Reset */
	val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
	val &= ~(0x1);
	writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

	writel(0x11, phy_base + 0x03*4);

	/* jitter tunning */
	writel(0x34, phy_base + 0x04*4);
	writel(0x02, phy_base + 0x07*4);
	writel(0x41, phy_base + 0x21*4);
	writel(0x7F, phy_base + 0x14*4);
	writel(0xC0, phy_base + 0x15*4);
	writel(0x61, phy_base + 0x36*4);

	/* D0 uninit.. */
	writel(0x44, phy_base + 0x3D*4);

	/* 24MHz */
	writel(0x94, phy_base + 0x08*4);
	writel(0xA7, phy_base + 0x09*4);
	writel(0x93, phy_base + 0x0A*4);
	writel(0x6B, phy_base + 0x0C*4);
	writel(0xA5, phy_base + 0x0F*4);
	writel(0x34, phy_base + 0x16*4);
	writel(0xA3, phy_base + 0x17*4);
	writel(0xA7, phy_base + 0x1A*4);
	writel(0x71, phy_base + 0x23*4);
	writel(0x0E, phy_base + 0x26*4);
	writel(0x14, phy_base + 0x07*4);
	writel(0x48, phy_base + 0x43*4);
	writel(0x44, phy_base + 0x44*4);
	writel(0x03, phy_base + 0x45*4);
	writel(0xA7, phy_base + 0x48*4);
	writel(0x13, phy_base + 0x54*4);
	writel(0x04, phy_base + 0x31*4);
	writel(0x00, phy_base + 0x32*4);

	/* PHY Common Reset */
	val = readl(block_base + PCIE_PHY_COMMON_RESET);
	val &= ~(0x1);
	writel(val, block_base + PCIE_PHY_COMMON_RESET);

	/* PHY Mac Reset */
	val = readl(block_base + PCIE_PHY_MAC_RESET);
	val |= 0x1 << 4;
	writel(val, block_base + PCIE_PHY_MAC_RESET);
#elif defined(CONFIG_SOC_EXYNOS7420)
	exynos_pcie_phy_config(phy_base, phy_pcs_base, block_base, elbi_base);
	exynos_pcie_phy_clock_enable(&exynos_pcie->pp, 1);
#endif

	/* Bus number enable */
	val = readl(elbi_base + PCIE_SW_WAKE);
	val &= ~(0x1<<1);
	writel(val, elbi_base + PCIE_SW_WAKE);
}

static int exynos_pcie_establish_link(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;
	void __iomem *block_base = exynos_pcie->block_base;
	struct device *dev = pp->dev;
	int count = 0;
	u32 val;

	gpio_set_value(exynos_pcie->perst_gpio, 1);
#ifdef CONFIG_PCI_EXYNOS_REDUCE_RESET_WAIT
	usleep_range(18000, 20000);
#else
	mdelay(80);
#endif

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP);
	dev_info(dev, "LINK STATUS: %x\n", val);

	if (soc_is_exynos5433()) {
		/* APP_REQ_EXIT_L1_MODE : BIT5 (0x0 : H/W mode, 0x1 : S/W mode) */
		val = readl(block_base + PCIE_PHY_GLOBAL_RESET);
		val &= ~(0x1 << 5);
		writel(val, block_base + PCIE_PHY_GLOBAL_RESET);

		/* PCIE_L1SUB_CM_CON : BIT0 (0x0 : REFCLK GATING DISABLE, 0x1 : REFCLK GATING ENABLE) */
		val = readl(block_base + PCIE_L1SUB_CM_CON);
		val |= 0x1;
		writel(val, block_base + PCIE_L1SUB_CM_CON);
	} else if (soc_is_exynos7420()) {
		/* APP_REQ_EXIT_L1_MODE : BIT0 (0x0 : S/W mode, 0x1 : H/W mode) */
		writel(0x1, elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		writel(0x1, elbi_base + PCIE_CORE_RESETN_DISABLE);
		writel(readl(exynos_pcie->cmu_base) | 0x1, exynos_pcie->cmu_base);
	}

	exynos_pcie_assert_phy_reset(pp);

	writel(0x1, elbi_base + PCIE_L1_BUG_FIX_ENABLE);

	if (soc_is_exynos5433()) {
		exynos_pcie_sideband_dbi_r_mode(pp, true);
		exynos_pcie_sideband_dbi_w_mode(pp, true);
	}
	/* setup root complex */
	dw_pcie_setup_rc(pp);
	if (soc_is_exynos5433()) {
		exynos_pcie_sideband_dbi_r_mode(pp, false);
		exynos_pcie_sideband_dbi_w_mode(pp, false);
	}

	/* assert LTSSM enable */
	writel(PCIE_ELBI_LTSSM_ENABLE, elbi_base + PCIE_APP_LTSSM_ENABLE);
	while (count < MAX_TIMEOUT) {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		if (val >= 0x0d && val <= 0x15)
			break;
		count++;
		udelay(10);
	}

	if (count >= MAX_TIMEOUT) {
		dev_info(dev, "%s: Link is not up\n", __func__);
#ifdef CONFIG_BCMDHD_PCIE
		return -1;
#else
		return exynos_pcie_reset(pp);
#endif

	} else {
		writel(readl(exynos_pcie->elbi_base + PCIE_IRQ_SPECIAL),
				exynos_pcie->elbi_base + PCIE_IRQ_SPECIAL);
		dev_info(dev, "%s: Link up: %x\n", __func__, val);

		if (soc_is_exynos5433())
			queue_delayed_work(exynos_pcie->pcie_wq,
				&exynos_pcie->work, msecs_to_jiffies(1000));
		else if (soc_is_exynos7420())
			writel(readl(exynos_pcie->elbi_base + PCIE_IRQ_EN_LEVEL)
				| IRQ_LINKDOWN_ENABLE,
				exynos_pcie->elbi_base + PCIE_IRQ_EN_LEVEL);
	}


	return 0;
}

#ifdef CONFIG_BCMDHD
extern int test_flag;
#endif
static void exynos_pcie_clear_irq_pulse(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	val = readl(elbi_base + PCIE_IRQ_PULSE);
	writel(val, elbi_base + PCIE_IRQ_PULSE);

	val = readl(elbi_base + PCIE_IRQ_LEVEL);
	writel(val, elbi_base + PCIE_IRQ_LEVEL);

	val = readl(elbi_base + PCIE_IRQ_SPECIAL);
#ifdef CONFIG_BCMDHD
	if(1 == test_flag) 
		val= val | (0x1 << 2);
#endif
	if (soc_is_exynos7420() && ((exynos_pcie->state == STATE_LINK_UP) && (val & (0x1 << 2)))) {
		dev_info(pp->dev, "!!!PCIE LINK DOWN!!!\n");
		queue_work(exynos_pcie->pcie_wq, &exynos_pcie->work.work);
	}

	writel(val, elbi_base + PCIE_IRQ_SPECIAL);
#ifdef CONFIG_BCMDHD
	test_flag = 0;
#endif
	return;
}

static void exynos_pcie_enable_irq_pulse(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	/* enable INTX interrupt */
	val = IRQ_INTA_ASSERT | IRQ_INTB_ASSERT |
		IRQ_INTC_ASSERT | IRQ_INTD_ASSERT,
	writel(val, elbi_base + PCIE_IRQ_EN_PULSE);

	/* disable LEVEL interrupt */
	writel(0x0, elbi_base + PCIE_IRQ_EN_LEVEL);

	/* disable SPECIAL interrupt */
	writel(0x0, elbi_base + PCIE_IRQ_EN_SPECIAL);

	return;
}

static irqreturn_t exynos_pcie_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

#ifdef CONFIG_PCI_MSI
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (exynos_pcie->use_msi)
		exynos_pcie_msi_irq_handler(irq, arg);
#endif

	exynos_pcie_clear_irq_pulse(pp);
	return IRQ_HANDLED;
}

static irqreturn_t handle_wake_irq(int irq, void *data)
{
	struct pcie_port *pp = data;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	dev_info(pp->dev, "cp2ap_wake irq involked\n");
	schedule_work(&exynos_pcie->handle_wake_work);

	return IRQ_HANDLED;
}

static irqreturn_t exynos_pcie_eint_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	disable_irq_nosync(exynos_pcie->eint_irq);
	if (exynos_pcie->pcie_tpoweron_max) {
		exynos_pcie->pcie_tpoweron_max = 0;
		dw_pcie_set_tpoweron(pp, 0);
	}
	exynos_pcie->eint_flag = 0;

	return IRQ_HANDLED;
}


#ifdef CONFIG_PCI_MSI
static void exynos_pcie_clear_irq_level(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	val = readl(elbi_base + PCIE_IRQ_LEVEL);
	writel(val, elbi_base + PCIE_IRQ_LEVEL);
	return;
}

static irqreturn_t exynos_pcie_msi_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

	/* handle msi irq */
	dw_handle_msi_irq(pp);
	exynos_pcie_clear_irq_level(pp);

	return IRQ_HANDLED;
}

static void exynos_pcie_msi_init(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	void __iomem *elbi_base = exynos_pcie->elbi_base;

	if (!exynos_pcie->use_msi)
		return;

	dw_pcie_msi_init(pp);

	/* enable MSI interrupt */
	val = readl(elbi_base + PCIE_IRQ_EN_LEVEL);
	val |= IRQ_MSI_ENABLE;
	writel(val, elbi_base + PCIE_IRQ_EN_LEVEL);
	return;
}
#endif

static void exynos_pcie_enable_interrupts(struct pcie_port *pp)
{
	exynos_pcie_enable_irq_pulse(pp);

	return;
}

static int exynos_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	int ret = 0;

	if (soc_is_exynos5433()) {
		exynos_pcie_sideband_dbi_r_mode(pp, true);
		ret = cfg_read(pp->dbi_base + (where & ~0x3), where, size, val);
		exynos_pcie_sideband_dbi_r_mode(pp, false);
	} else if (soc_is_exynos7420()) {
		writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
		writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		ret = cfg_read(exynos_pcie->rc_dbi_base + (where & ~0x3), where, size, val);
		writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
	}
	return ret;
}

static int exynos_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	int ret = 0;

	if (soc_is_exynos5433()) {
		exynos_pcie_sideband_dbi_w_mode(pp, true);
		ret = cfg_write(pp->dbi_base + (where & ~0x3), where, size, val);
		exynos_pcie_sideband_dbi_w_mode(pp, false);
	} else if (soc_is_exynos7420()) {
		writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
		writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		ret = cfg_write(exynos_pcie->rc_dbi_base + (where & ~0x3), where, size, val);
		writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
	}
	return ret;
}

static int exynos_pcie_link_up(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;

	if (val >= 0x0d && val <= 0x15)
		return 1;

	return 0;
}

static struct pcie_host_ops exynos_pcie_host_ops = {
	.rd_own_conf = exynos_pcie_rd_own_conf,
	.wr_own_conf = exynos_pcie_wr_own_conf,
	.link_up = exynos_pcie_link_up,
};

static int add_pcie_port(struct pcie_port *pp, struct platform_device *pdev)
{
	int ret;

	pp->irq = platform_get_irq(pdev, 0);
	if (!pp->irq) {
		dev_err(&pdev->dev, "failed to get irq\n");
		return -ENODEV;
	}

	ret = devm_request_irq(&pdev->dev, pp->irq, exynos_pcie_irq_handler,
				IRQF_SHARED, "exynos-pcie", pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	pp->root_bus_nr = -1;
	pp->ops = &exynos_pcie_host_ops;

	spin_lock_init(&pp->conf_lock);
	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize host\n");
		return ret;
	}

	return 0;
}

static int __init exynos_pcie_probe(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie;
	struct pcie_port *pp;
	struct resource *elbi_base;
	struct resource *phy_base;
	struct resource *block_base;
	struct resource *pmu_base;
	struct resource *rc_dbi_base;
	struct resource *cmu_base;
	struct resource *phy_pcs_base;
	struct device_node *root_node = pdev->dev.of_node;
	int ch_num, irq;
	int ret = 0;

	pr_info("[info] %s pcie name = %s", __func__, dev_name(&pdev->dev));
	if (create_pcie_sys_file(&pdev->dev))
		dev_err(&pdev->dev, "Failed to create pcie sys file\n");

	if (soc_is_exynos5433()) {
		ch_num = 1;
	} else if (soc_is_exynos7420()) {
		if (of_property_read_u32(root_node, "ch-num", &ch_num)) {
			dev_err(&pdev->dev, "Failed to parse the channel number\n");
			return -EINVAL;
		}

#if defined(CONFIG_BATTERY_SAMSUNG)
		if (ch_num == 0 && lpcharge) {
			dev_info(&pdev->dev, "%s: pcie probe canceled - lpcharge mode\n", __func__);
			return -EINVAL;
		}
#endif

	} else {
		dev_err(&pdev->dev, "Failed to parse the channel number\n");
		return -EINVAL;
	}

	exynos_pcie = &g_pcie[ch_num];
	pp = &exynos_pcie->pp;
	pp->dev = &pdev->dev;

	if (soc_is_exynos7420()) {
		if (of_property_read_u32(root_node, "pcie-clk-num", &exynos_pcie->pcie_clk_num)) {
			dev_err(pp->dev, "Failed to parse the channel number\n");
			return -EINVAL;
		}
		if (of_property_read_u32(root_node, "phy-clk-num", &exynos_pcie->phy_clk_num)) {
			dev_err(pp->dev, "Failed to parse the channel number\n");
			return -EINVAL;
		}
	}
	exynos_pcie->use_msi = of_property_read_bool(root_node, "use-msi");
	exynos_pcie->ch_num = ch_num;
	exynos_pcie->l1ss_enable = 1;
	exynos_pcie->state = STATE_LINK_DOWN;

	exynos_pcie->perst_gpio = of_get_gpio(root_node, 0);
	if (exynos_pcie->perst_gpio < 0)
		dev_err(&pdev->dev, "cannot get perst_gpio\n");
	else {
		ret = devm_gpio_request_one(pp->dev, exynos_pcie->perst_gpio,
				GPIOF_OUT_INIT_LOW, dev_name(pp->dev));
		if (ret)
			goto probe_fail;
	}

#ifdef CONFIG_PCI_EXYNOS_TEST
	wlan_gpio = of_get_named_gpio(root_node, "pcie,wlan-gpio", 0);
	bt_gpio = of_get_named_gpio(root_node, "pcie,bt-gpio", 0);

	if (wlan_gpio < 0)
		dev_err(&pdev->dev, "cannot get wlan_gpio\n");
	else {
		ret = devm_gpio_request_one(pp->dev, wlan_gpio, GPIOF_OUT_INIT_LOW, dev_name(pp->dev));
		if (ret)
			goto probe_fail;
	}

	if (bt_gpio < 0)
		dev_err(&pdev->dev, "cannot get bt_gpio\n");
	else {
		ret = devm_gpio_request_one(pp->dev, bt_gpio, GPIOF_OUT_INIT_LOW, dev_name(pp->dev));
		if (ret)
			goto probe_fail;
	}
#endif

	exynos_pcie->d0uninit_cnt = 0;
	ret = exynos_pcie_clock_get(pp);
	if (ret)
		goto probe_fail;

	elbi_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	exynos_pcie->elbi_base = devm_ioremap_resource(&pdev->dev, elbi_base);
	if (IS_ERR(exynos_pcie->elbi_base)) {
		ret = PTR_ERR(exynos_pcie->elbi_base);
		goto probe_fail;
	}

	phy_base = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	exynos_pcie->phy_base = devm_ioremap_resource(&pdev->dev, phy_base);
	if (IS_ERR(exynos_pcie->phy_base)) {
		ret = PTR_ERR(exynos_pcie->phy_base);
		goto probe_fail;
	}

	block_base = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	exynos_pcie->block_base = devm_ioremap_resource(&pdev->dev, block_base);
	if (IS_ERR(exynos_pcie->block_base)) {
		ret = PTR_ERR(exynos_pcie->block_base);
		goto probe_fail;
	}

	pmu_base = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	exynos_pcie->pmu_base = devm_ioremap_resource(&pdev->dev, pmu_base);
	if (IS_ERR(exynos_pcie->pmu_base)) {
		ret = PTR_ERR(exynos_pcie->pmu_base);
		goto probe_fail;
	}

	if (soc_is_exynos7420()) {
		rc_dbi_base = platform_get_resource(pdev, IORESOURCE_MEM, 4);
		exynos_pcie->rc_dbi_base = devm_ioremap_resource(&pdev->dev, rc_dbi_base);
		if (IS_ERR(exynos_pcie->rc_dbi_base)) {
			ret = PTR_ERR(exynos_pcie->rc_dbi_base);
			goto probe_fail;
		}

		cmu_base = platform_get_resource(pdev, IORESOURCE_MEM, 5);
		exynos_pcie->cmu_base = devm_ioremap_resource(&pdev->dev, cmu_base);
		if (IS_ERR(exynos_pcie->cmu_base)) {
			ret = PTR_ERR(exynos_pcie->cmu_base);
			goto probe_fail;
		}

		phy_pcs_base = platform_get_resource(pdev, IORESOURCE_MEM, 6);
		exynos_pcie->phy_pcs_base = devm_ioremap_resource(&pdev->dev, phy_pcs_base);
		if (IS_ERR(exynos_pcie->phy_pcs_base)) {
			ret = PTR_ERR(exynos_pcie->phy_pcs_base);
			goto probe_fail;
		}
	}

	if (soc_is_exynos7420() && exynos_pcie->ch_num == 0) {
		/* IRQ request for cp2ap_wake */
		exynos_pcie->cp2ap_wake_gpio = of_get_named_gpio(root_node, "pcie,cp2ap_wake", 0);
		if (exynos_pcie->cp2ap_wake_gpio) {
			irq = gpio_to_irq(exynos_pcie->cp2ap_wake_gpio);
			ret = devm_request_irq(&pdev->dev, irq, handle_wake_irq,
				IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING,
				"ca2ap-wake", pp);
			if (ret) {
				dev_err(&pdev->dev, "failed to request irq\n");
				goto probe_fail;
			}
			INIT_WORK(&exynos_pcie->handle_wake_work, handle_wake_func);
			ret = enable_irq_wake(irq);
			if (ret) {
				dev_err(&pdev->dev, "failed to enable_irq_wake:%d\n", ret);
				goto probe_fail;
			}
		}
	}

	if (soc_is_exynos5433()) {
		exynos_pcie->gpio_base = devm_ioremap(&pdev->dev, 0x156900a0, 100);
		if (IS_ERR(exynos_pcie->gpio_base)) {
			ret = PTR_ERR(exynos_pcie->gpio_base);
			goto probe_fail;
		}

		exynos_pcie->cmu_base = devm_ioremap(&pdev->dev, 0x10030634, 100);
		if (IS_ERR(exynos_pcie->cmu_base)) {
			ret = PTR_ERR(exynos_pcie->cmu_base);
			goto probe_fail;
		}
	}

	exynos_pcie_resumed_phydown(pp);

	if (soc_is_exynos7420() && exynos_pcie->ch_num == 1) {
		exynos_pcie->eint_irq = gpio_to_irq(of_get_gpio(root_node, 1));
		ret = devm_request_irq(&pdev->dev, exynos_pcie->eint_irq, exynos_pcie_eint_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT, "exynos-pcie-eint", pp);
		if (ret) {
			dev_err(&pdev->dev, "failed to request eint irq\n");
			return ret;
		}
		disable_irq(exynos_pcie->eint_irq);
		exynos_pcie->eint_flag = 0;
		exynos_pcie->pcie_tpoweron_max = 0;
	}
	ret = add_pcie_port(pp, pdev);
	if (ret)
		goto probe_fail;

	platform_set_drvdata(pdev, exynos_pcie);

probe_fail:
	if (ret)
		dev_err(&pdev->dev, "%s: pcie probe failed\n", __func__);
	else
		dev_info(&pdev->dev, "%s: pcie probe success\n", __func__);

	return ret;
}

static int __exit exynos_pcie_remove(struct platform_device *pdev)
{
	struct exynos_pcie *exynos_pcie = platform_get_drvdata(pdev);
	struct pcie_port *pp = &exynos_pcie->pp;

#ifdef CONFIG_CPU_IDLE
	if (soc_is_exynos5433())
		raw_notifier_chain_unregister(&pci_lpa_nh, &exynos_pcie->lpa_nb);
	else if (soc_is_exynos7420())
		exynos_pm_unregister_notifier(&exynos_pcie->lpa_nb);
#endif

	if (exynos_pcie->state > STATE_LINK_DOWN) {
		/* phy all power down */
		if (soc_is_exynos5433()) {
			writel(readl(exynos_pcie->phy_base + 0x21*4) | (0x1 << 4), exynos_pcie->phy_base + 0x21*4);
			writel(readl(exynos_pcie->phy_base + 0x55*4) | (0x1 << 3), exynos_pcie->phy_base + 0x55*4);
		} else if (soc_is_exynos7420()) {
			writel(readl(exynos_pcie->phy_base + 0x15*4) | (0xf << 3), exynos_pcie->phy_base + 0x15*4);
			writel(0xff, exynos_pcie->phy_base + 0x4E*4);
			writel(0x3f, exynos_pcie->phy_base + 0x4F*4);
		}

#ifndef CONFIG_SOC_EXYNOS5433
		exynos_pcie_phy_clock_enable(pp, 0);
		exynos_pcie_ref_clock_enable(pp, 0);
#endif

		writel(0, exynos_pcie->pmu_base + PCIE_PHY_CONTROL);
		exynos_pcie_clock_enable(pp, 0);
	}

	remove_pcie_sys_file(&pdev->dev);
	mutex_destroy(&exynos_pcie->lock);
	destroy_workqueue(exynos_pcie->pcie_wq);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_pcie_of_match[] = {
	{ .compatible = "samsung,exynos5433-pcie", },
	{ .compatible = "samsung,exynos7420-pcie", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_pcie_of_match);
#endif

static void exynos_pcie_resumed_phydown(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;

	/* phy all power down on wifi off during suspend/resume */
	exynos_pcie_clock_enable(pp, 1);

	exynos_pcie_enable_interrupts(pp);
	writel(1, exynos_pcie->pmu_base + PCIE_PHY_CONTROL);

	if (soc_is_exynos5433()) {
		/* APP_REQ_EXIT_L1_MODE : BIT5 (0x0 : H/W mode, 0x1 : S/W mode) */
		val = readl(exynos_pcie->block_base + PCIE_PHY_GLOBAL_RESET);
		val &= ~(0x1 << 5);
		writel(val, exynos_pcie->block_base + PCIE_PHY_GLOBAL_RESET);

		/* PCIE_L1SUB_CM_CON : BIT0 (0x0 : REFCLK GATING DISABLE, 0x1 : REFCLK GATING ENABLE) */
		val = readl(exynos_pcie->block_base + PCIE_L1SUB_CM_CON);
		val |= 0x1;
		writel(val, exynos_pcie->block_base + PCIE_L1SUB_CM_CON);
	} else if (soc_is_exynos7420()) {
		/* APP_REQ_EXIT_L1_MODE : BIT0 (0x0 : S/W mode, 0x1 : H/W mode) */
		writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);
		writel(0x1, exynos_pcie->elbi_base + PCIE_CORE_RESETN_DISABLE);
		writel(readl(exynos_pcie->cmu_base) | 0x1, exynos_pcie->cmu_base);
	}

	exynos_pcie_assert_phy_reset(pp);

	/* phy all power down */
	if (soc_is_exynos5433()) {
		writel(readl(exynos_pcie->phy_base + 0x21*4) | (0x1 << 4), exynos_pcie->phy_base + 0x21*4);
		writel(readl(exynos_pcie->phy_base + 0x55*4) | (0x1 << 3), exynos_pcie->phy_base + 0x55*4);
	} else if (soc_is_exynos7420()) {
		writel(readl(exynos_pcie->phy_base + 0x15*4) | (0xf << 3), exynos_pcie->phy_base + 0x15*4);
		writel(0xff, exynos_pcie->phy_base + 0x4E*4);
		writel(0x3f, exynos_pcie->phy_base + 0x4F*4);
	}

#ifndef CONFIG_SOC_EXYNOS5433
	exynos_pcie_phy_clock_enable(pp, 0);
#endif

	writel(0, exynos_pcie->pmu_base + PCIE_PHY_CONTROL);
	exynos_pcie_clock_enable(pp, 0);
}

int exynos_pcie_poweron(int ch_num)
{
	struct pcie_port *pp = &g_pcie[ch_num].pp;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val, vendor_id, device_id;
	int ret;

	dev_info(pp->dev, "%s, start of poweron, pcie state: %d\n", __func__, exynos_pcie->state);
	if (exynos_pcie->state == STATE_LINK_DOWN) {
		exynos_pcie_clock_enable(pp, 1);
#ifndef CONFIG_SOC_EXYNOS5433
		exynos_pcie_ref_clock_enable(pp, 1);
#endif
		writel(1, exynos_pcie->pmu_base + PCIE_PHY_CONTROL);

		/* phy all power down clear */
		if (soc_is_exynos5433()) {
			writel(readl(exynos_pcie->phy_base + 0x55*4) & ~(0x1 << 3),
					exynos_pcie->phy_base + 0x55*4);
			udelay(100);
			writel(readl(exynos_pcie->phy_base + 0x21*4) & ~(0x1 << 4),
					exynos_pcie->phy_base + 0x21*4);
		} else if (soc_is_exynos7420()) {
			writel(readl(exynos_pcie->phy_base + 0x15*4) & ~(0xf << 3),
					exynos_pcie->phy_base + 0x15*4);
			writel(0x0, exynos_pcie->phy_base + 0x4E*4);
			writel(0x0, exynos_pcie->phy_base + 0x4F*4);
		}

		exynos_pcie->state = STATE_LINK_UP_TRY;
		if (!exynos_pcie->probe_ok) {
#ifdef CONFIG_CPU_IDLE
			exynos_pcie->lpa_nb.notifier_call = exynos_pci_lpa_event;
			exynos_pcie->lpa_nb.next = NULL;
			exynos_pcie->lpa_nb.priority = 0;

			if (soc_is_exynos5433())
				ret = raw_notifier_chain_register(&pci_lpa_nh, &exynos_pcie->lpa_nb);
			else if (soc_is_exynos7420())
				ret = exynos_pm_register_notifier(&exynos_pcie->lpa_nb);

			if (ret) {
				dev_err(pp->dev, "Failed to register lpa notifier\n");
				goto poweron_fail;
			}
#endif
			mutex_init(&exynos_pcie->lock);
			exynos_pcie->pcie_wq = create_freezable_workqueue("pcie_wq");
			if (IS_ERR(exynos_pcie->pcie_wq)) {
				dev_err(pp->dev, "couldn't create workqueue\n");
				goto mtx_fail;
			}

			INIT_DELAYED_WORK(&exynos_pcie->work, exynos_pcie_work);

			if (exynos_pcie_establish_link(pp)) {
				dev_err(pp->dev, "pcie link up fail\n");
				goto wq_fail;
			}
			exynos_pcie->state = STATE_LINK_UP;
			dw_pcie_scan(pp);

			exynos_pcie_rd_own_conf(pp, PCI_VENDOR_ID, 4, &val);
			vendor_id = val & ID_MASK;
			device_id = (val >> 16) & ID_MASK;

			exynos_pcie->pci_dev = pci_get_device(vendor_id, device_id, NULL);
			if (!exynos_pcie->pci_dev) {
				dev_err(pp->dev, "Failed to get pci device\n");
				goto wq_fail;
			}

#ifdef CONFIG_PCI_MSI
			exynos_pcie_msi_init(pp);
#endif

			pci_save_state(exynos_pcie->pci_dev);
			exynos_pcie->pci_saved_configs = pci_store_saved_state(exynos_pcie->pci_dev);
			exynos_pcie->probe_ok = 1;
		} else if (exynos_pcie->probe_ok) {
			if (exynos_pcie_reset(pp)) {
				dev_err(pp->dev, "pcie link up fail\n");
				goto poweron_fail;
			}
			exynos_pcie->state = STATE_LINK_UP;

#ifdef CONFIG_PCI_MSI
			exynos_pcie_msi_init(pp);
#endif

			pci_load_saved_state(exynos_pcie->pci_dev, exynos_pcie->pci_saved_configs);
			pci_restore_state(exynos_pcie->pci_dev);
		}
	}
	dev_info(pp->dev, "%s, end of poweron, pcie state: %d\n", __func__, exynos_pcie->state);

	return 0;

wq_fail:
	destroy_workqueue(exynos_pcie->pcie_wq);
mtx_fail:
	mutex_destroy(&exynos_pcie->lock);

#ifdef CONFIG_CPU_IDLE
	if (soc_is_exynos5433())
		raw_notifier_chain_unregister(&pci_lpa_nh, &exynos_pcie->lpa_nb);
	else if (soc_is_exynos7420())
		exynos_pm_unregister_notifier(&exynos_pcie->lpa_nb);
#endif

poweron_fail:
	exynos_pcie->state = STATE_LINK_UP;
	exynos_pcie_poweroff(exynos_pcie->ch_num);

	return -EPIPE;
}
EXPORT_SYMBOL(exynos_pcie_poweron);

void exynos_pcie_poweroff(int ch_num)
{
	struct pcie_port *pp = &g_pcie[ch_num].pp;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	unsigned long flags;

	dev_info(pp->dev, "%s, start of poweroff, pcie state: %d\n", __func__, exynos_pcie->state);
	if (exynos_pcie->state == STATE_LINK_UP || (exynos_pcie->state == STATE_LINK_DOWN_TRY)) {
		exynos_pcie->state = STATE_LINK_DOWN_TRY;
		while (exynos_pcie->lpc_checking)
			usleep_range(1000, 1100);

		if (ch_num == 1) {
			exynos_pcie->pcie_tpoweron_max = 0;
			if (exynos_pcie->eint_flag) {
				disable_irq_nosync(exynos_pcie->eint_irq);
				exynos_pcie->eint_flag = 0;
			}
		}

		if (soc_is_exynos5433())
			cancel_delayed_work(&exynos_pcie->work);
		else if (soc_is_exynos7420())
			writel(0x0, exynos_pcie->elbi_base + PCIE_IRQ_EN_LEVEL);

		spin_lock_irqsave(&pp->conf_lock, flags);
		exynos_pcie->state = STATE_LINK_DOWN;

		gpio_set_value(exynos_pcie->perst_gpio, 0);
		/* LTSSM disable */
		writel(PCIE_ELBI_LTSSM_DISABLE, exynos_pcie->elbi_base + PCIE_APP_LTSSM_ENABLE);

		/* phy all power down */
		if (soc_is_exynos5433()) {
			writel(readl(exynos_pcie->phy_base + 0x55*4) | (0x1 << 3),
					exynos_pcie->phy_base + 0x55*4);
			writel(readl(exynos_pcie->phy_base + 0x21*4) | (0x1 << 4),
					exynos_pcie->phy_base + 0x21*4);
		} else if (soc_is_exynos7420()) {
			writel(readl(exynos_pcie->phy_base + 0x15*4) | (0xf << 3),
					exynos_pcie->phy_base + 0x15*4);
			writel(0xff, exynos_pcie->phy_base + 0x4E*4);
			writel(0x3f, exynos_pcie->phy_base + 0x4F*4);
		}

		spin_unlock_irqrestore(&pp->conf_lock, flags);
#ifndef CONFIG_SOC_EXYNOS5433
		exynos_pcie_phy_clock_enable(pp, 0);
		exynos_pcie_ref_clock_enable(pp, 0);
#endif
		writel(0, exynos_pcie->pmu_base + PCIE_PHY_CONTROL);
		exynos_pcie_clock_enable(pp, 0);

	}
	dev_info(pp->dev, "%s, end of poweroff, pcie state: %d\n", __func__, exynos_pcie->state);
}
EXPORT_SYMBOL(exynos_pcie_poweroff);

void exynos_pcie_send_pme_turn_off(struct exynos_pcie *exynos_pcie)
{
	struct pcie_port *pp = &exynos_pcie->pp;
	struct device *dev = pp->dev;
	int __maybe_unused count = 0, i;
	u32 __maybe_unused val;
	u32 save_regs[6], save_address[6] = {0x5C, 0x4A, 0x3F, 0x15, 0x4E, 0x4F};

	writel(0x0, exynos_pcie->elbi_base + 0xa8);

	if (soc_is_exynos5433()) {
		val = readl(exynos_pcie->block_base + PCIE_PHY_GLOBAL_RESET);
		val |= 0x1 << 5;
		writel(val, exynos_pcie->block_base + PCIE_PHY_GLOBAL_RESET);
	} else if (soc_is_exynos7420()) {
		writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1_MODE);

		for (i = 0; i < 6; i++)
			save_regs[i] = readl(exynos_pcie->phy_base + (save_address[i] * 4));

		writel(0x41, exynos_pcie->phy_base + (save_address[3] * 4));
		writel(0xE0, exynos_pcie->phy_base + (save_address[4] * 4));
		writel(0x28, exynos_pcie->phy_base + (save_address[5] * 4));
		writel(0xCD, exynos_pcie->phy_base + (save_address[2] * 4));
		writel(0xDF, exynos_pcie->phy_base + (save_address[1] * 4));
		writel(0x54, exynos_pcie->phy_base + (save_address[0] * 4));
	}

	writel(0x1, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);
	writel(0x1, exynos_pcie->elbi_base + 0xac);
	writel(0x13, exynos_pcie->elbi_base + 0xb0);
	writel(0x19, exynos_pcie->elbi_base + 0xd0);
	writel(0x1, exynos_pcie->elbi_base + 0xa8);
	while (count < MAX_TIMEOUT) {
		if ((readl(exynos_pcie->elbi_base + PCIE_IRQ_PULSE) & IRQ_RADM_PM_TO_ACK)) {
			dev_info(dev, "ack message is ok\n");
			break;
		}

		udelay(10);
		count++;
	}

	if (count >= MAX_TIMEOUT)
		dev_err(dev, "cannot receive ack message from wifi\n");

	writel(0x0, exynos_pcie->elbi_base + PCIE_APP_REQ_EXIT_L1);

	do {
		val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP);
		val = val & 0x1f;
		if (val == 0x15) {
			dev_info(dev, "received Enter_L23_READY DLLP packet\n");
			break;
		}
		udelay(10);
		count++;
	} while (count < MAX_TIMEOUT);

	if (count >= MAX_TIMEOUT)
		dev_err(dev, "cannot receive L23_READY DLLP packet\n");

	if (soc_is_exynos7420()) {
		for (i = 0; i < 6; i++)
			writel(save_regs[i], exynos_pcie->phy_base + (save_address[i] * 4));
	}
}

int exynos_pcie_pm_suspend(int ch_num)
{
	struct pcie_port *pp = &g_pcie[ch_num].pp;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	unsigned long flags;

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(pp->dev, "RC%d already off\n", exynos_pcie->ch_num);
		return 0;
	}

	spin_lock_irqsave(&pp->conf_lock, flags);
	exynos_pcie->state = STATE_LINK_DOWN_TRY;
	spin_unlock_irqrestore(&pp->conf_lock, flags);

	exynos_pcie_send_pme_turn_off(exynos_pcie);
	exynos_pcie_poweroff(ch_num);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_pm_suspend);

int exynos_pcie_pm_resume(int ch_num)
{
	return exynos_pcie_poweron(ch_num);
}
EXPORT_SYMBOL(exynos_pcie_pm_resume);

#ifdef CONFIG_PM
static int exynos_pcie_suspend_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		dev_info(dev, "RC%d already off\n", exynos_pcie->ch_num);
		return 0;
	}
	exynos_pcie_send_pme_turn_off(exynos_pcie);
	gpio_set_value(exynos_pcie->perst_gpio, 0);

	return 0;
}

static int exynos_pcie_resume_noirq(struct device *dev)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pinctrl *pinctrl_reset;

	if (soc_is_exynos5433())
		writel(readl(exynos_pcie->cmu_base) | (0x7 << 12), exynos_pcie->cmu_base);

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		exynos_pcie_resumed_phydown(&exynos_pcie->pp);
		return 0;
	}

	/* link reset by using wake pin */
	if (soc_is_exynos5433()) {
		writel((readl(exynos_pcie->gpio_base + 0x8) & ~(0x3 << 12)) | (0x1 << 12),
				exynos_pcie->gpio_base + 0x8);
		udelay(20);
		writel(readl(exynos_pcie->gpio_base + 0x8) | (0x3 << 12), exynos_pcie->gpio_base + 0x8);
	} else if (soc_is_exynos7420()) {
		pinctrl_reset = devm_pinctrl_get_select(dev, "pcie_reset");
		if (IS_ERR(pinctrl_reset))
			dev_err(dev, "failed pcie link reset\n");

		udelay(20);

		pinctrl_reset = devm_pinctrl_get_select_default(dev);
		if (IS_ERR(pinctrl_reset))
			dev_err(dev, "failed pcie link reset\n");
	}

	exynos_pcie_enable_interrupts(&exynos_pcie->pp);

	writel(1, exynos_pcie->pmu_base + PCIE_PHY_CONTROL);
	exynos_pcie_establish_link(&exynos_pcie->pp);

	/* setup ATU for cfg/mem outbound */
	dw_pcie_prog_viewport_cfg0(&exynos_pcie->pp, 0x1000000);
	dw_pcie_prog_viewport_mem_outbound(&exynos_pcie->pp);

	/* L1.2 ASPM enable */
	dw_pcie_config_l1ss(&exynos_pcie->pp);

	return 0;
}

#else
#define exynos_pcie_suspend_noirq	NULL
#define exynos_pcie_resume_noirq	NULL
#endif

static const struct dev_pm_ops exynos_pcie_dev_pm_ops = {
	.suspend_noirq	= exynos_pcie_suspend_noirq,
	.resume_noirq	= exynos_pcie_resume_noirq,
};

static struct platform_driver exynos_pcie_driver = {
	.remove		= __exit_p(exynos_pcie_remove),
	.driver = {
		.name	= "exynos-pcie",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_pcie_of_match),
		.pm	= &exynos_pcie_dev_pm_ops,
	},
};

#ifdef CONFIG_CPU_IDLE
static int exynos_pci_lpa_event(struct notifier_block *nb, unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	struct exynos_pcie *exynos_pcie = container_of(nb,
			struct exynos_pcie, lpa_nb);
	struct pcie_port *pp = &exynos_pcie->pp;

	switch (event) {
	case LPA_PREPARE:
		if (exynos_pcie->state != STATE_LINK_DOWN)
			ret = -EBUSY;
		break;
	case LPC_PREPARE:
		if (exynos_pcie->state != STATE_LINK_DOWN) {
			exynos_pcie->lpc_checking = true;
			if (exynos_pcie->ch_num == 1 && exynos_pcie->state == STATE_LINK_UP) {
				if (!exynos_pcie->pcie_tpoweron_max) {
					dw_pcie_set_tpoweron(pp, 1);
					exynos_pcie->pcie_tpoweron_max = 1;

					udelay(40);
					if (readl(exynos_pcie->elbi_base + 0xEC) & 0x1) {

						if (!exynos_pcie->eint_flag) {
							enable_irq(exynos_pcie->eint_irq);
							exynos_pcie->eint_flag = 1;
						}

					} else {
						exynos_pcie->pcie_tpoweron_max = 0;
						dw_pcie_set_tpoweron(pp, 0);
						ret = -EBUSY;
					}
				}
			} else {
				ret = -EBUSY;
			}
			exynos_pcie->lpc_checking = false;
		}
		break;
	case LPA_EXIT:
		exynos_pcie_resumed_phydown(&exynos_pcie->pp);
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return notifier_from_errno(ret);
}
#endif

int check_pcie_link_status(int ch_num)
{
	struct pcie_port *pp = &g_pcie[ch_num].pp;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;
	int link_status, dev_state;

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15) {
		link_status = 1;
	} else {
		dev_info(pp->dev, "%s, Link is abnormal state(%x)\n", __func__, val);
		link_status = 0;
	}

	val = readl(exynos_pcie->elbi_base + PCIE_PM_DSTATE) & 0x7;
	if (val == PCIE_D0_UNINIT_STATE) {
		dev_info(pp->dev, "%s, Link is D0 uninit state(%x)\n", __func__, val);
		dev_state = 0;
	} else
		dev_state = 1;

	if (link_status && dev_state)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(check_pcie_link_status);

int check_wifi_op(int ch_num)
{
	return (g_pcie[ch_num].state == STATE_LINK_DOWN) ? 0 : 1;
}
EXPORT_SYMBOL(check_wifi_op);

#if defined(CONFIG_SOC_EXYNOS5433) && defined(CONFIG_CPU_IDLE)
void exynos_pci_lpa_resume(void)
{
	raw_notifier_call_chain(&pci_lpa_nh, LPA_EXIT, NULL);
}
EXPORT_SYMBOL_GPL(exynos_pci_lpa_resume);
#endif

void exynos_pcie_cfg_save(struct pcie_port *pp, bool rc)
{
	int i;
	u32 val = 0;
	u32 *shadow;
	void *cfg;
	unsigned long flags;
	struct exynos_pcie *exynos_pcie;
	exynos_pcie = to_exynos_pcie(pp);

	if (rc) {
		shadow = exynos_pcie->rc_shadow;
		cfg = pp->dbi_base;
	} else {
		shadow = exynos_pcie->ep_shadow;
		cfg = pp->va_cfg0_base;
	}

	spin_lock_irqsave(&pp->conf_lock, flags);
	if (rc) {
		/* save MSI Configuraiton */
		dw_pcie_rd_own_conf(pp, 0x820, 4, &val);
		shadow[0] = val;
		dw_pcie_rd_own_conf(pp, 0x824, 4, &val);
		shadow[1] = val;
		dw_pcie_rd_own_conf(pp, 0x828, 4, &val);
		shadow[2] = val;
	} else {
		for (i = PCIE_CONF_SPACE_DW - 1; i >= 0; i--) {
			val = readl_relaxed(cfg + i * 4);
			shadow[i] = val;
		}
	}
	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

void exynos_pcie_cfg_restore(struct pcie_port *pp, bool rc)
{
	int i;
	u32 val = 0;
	u32 *shadow;
	void *cfg;
	unsigned long flags;
	struct exynos_pcie *exynos_pcie;
	exynos_pcie = to_exynos_pcie(pp);

	if (rc) {
		shadow = exynos_pcie->rc_shadow;
		cfg = pp->dbi_base;
	} else {
		shadow = exynos_pcie->ep_shadow;
		cfg = pp->va_cfg0_base;
	}

	spin_lock_irqsave(&pp->conf_lock, flags);
	if (rc) {
		/* restore MSI Configuraiton */
		val = shadow[0];
		dw_pcie_wr_own_conf(pp, 0x820, 4, val);
		val = shadow[1];
		dw_pcie_wr_own_conf(pp, 0x824, 4, val);
		val = shadow[2];
		dw_pcie_wr_own_conf(pp, 0x828, 4, val);
	} else {
		for (i = PCIE_CONF_SPACE_DW - 1; i >= 0; i--) {
			val = shadow[i];
			writel_relaxed(val, cfg + i * 4);
		}
	}
	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

/* Exynos PCIe driver does not allow module unload */

static int __init pcie_init(void)
{
	return platform_driver_probe(&exynos_pcie_driver, exynos_pcie_probe);
}
device_initcall(pcie_init);

int exynos_pcie_register_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}
	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	exynos_pcie = to_exynos_pcie(pp);
	if (pp) {
		exynos_pcie->event_reg = reg;
		dev_info(pp->dev,
				"Event 0x%x is registered for RC %d\n",
				reg->events, exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_register_event);

int exynos_pcie_deregister_event(struct exynos_pcie_register_event *reg)
{
	int ret = 0;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;
	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}
	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}
	pp = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));
	exynos_pcie = to_exynos_pcie(pp);
	if (pp) {
		exynos_pcie->event_reg = NULL;
		dev_info(pp->dev, "Event is deregistered for RC %d\n",
				exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: did not find RC for pci endpoint device\n");
		ret = -ENODEV;
	}
	return ret;
}
EXPORT_SYMBOL(exynos_pcie_deregister_event);

int exynos_pcie_save_config(struct pci_dev *dev)
{
	int ret = 0;
	unsigned int val;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;

	if (dev) {
		pp = PCIE_BUS_PRIV_DATA(dev);
		if (!pp)
			return -ENODEV;
		exynos_pcie = to_exynos_pcie(pp);
		dev_info(pp->dev,
			"Save config space for the link of RC%d\n", exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15) {
		dev_info(pp->dev,
			"Save config space of RC%d and its EP\n",
			exynos_pcie->ch_num);
		dev_info(pp->dev, "Save RC%d\n", exynos_pcie->ch_num);
		exynos_pcie_cfg_save(pp, true);
		dev_info(pp->dev, "Save EP of RC%d\n", exynos_pcie->ch_num);
		exynos_pcie_cfg_save(pp, false);
	} else {
		dev_info(pp->dev,
			"PCIe: the link of RC%d is not up; can't save config space.\n",
				exynos_pcie->ch_num);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(exynos_pcie_save_config);

int exynos_pcie_restore_config(struct pci_dev *dev)
{
	int ret = 0;
	unsigned int val;
	struct pcie_port *pp;
	struct exynos_pcie *exynos_pcie;

	if (dev) {
		pp = PCIE_BUS_PRIV_DATA(dev);
		if (!pp)
			return -ENODEV;
		exynos_pcie = to_exynos_pcie(pp);
		dev_info(pp->dev,
			"Recovery for the link of RC%d\n", exynos_pcie->ch_num);
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	val = readl(exynos_pcie->elbi_base + PCIE_ELBI_RDLH_LINKUP) & 0x1f;
	if (val >= 0x0d && val <= 0x15) {
		dev_info(pp->dev,
			"Recover config space of RC%d and its EP\n",
			exynos_pcie->ch_num);
		dev_info(pp->dev, "Recover RC%d\n", exynos_pcie->ch_num);
		exynos_pcie_cfg_restore(pp, true);
#ifdef CONFIG_PCI_MSI
		exynos_pcie_msi_init(pp);
#endif
		dev_info(pp->dev, "Recover EP of RC%d\n", exynos_pcie->ch_num);
		exynos_pcie_cfg_restore(pp, false);
		dev_info(pp->dev,
			"Refreshing the saved config space in PCI framework for RC%d and its EP\n",
				exynos_pcie->ch_num);
		pci_save_state(exynos_pcie->pci_dev);
		pci_save_state(dev);
	} else {
		dev_info(pp->dev,
			"PCIe: the link of RC%d is not up yet; can't recover config space.\n",
			exynos_pcie->ch_num);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(exynos_pcie_restore_config);

int exynos_pcie_disable_irq(int ch_num)
{
	struct pcie_port *pp = &g_pcie[ch_num].pp;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	int irq;

	irq = gpio_to_irq(exynos_pcie->cp2ap_wake_gpio);
	disable_irq(irq);
	disable_irq(pp->irq);

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_disable_irq);

MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe host controller driver");
MODULE_LICENSE("GPL v2");
