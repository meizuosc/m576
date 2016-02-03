/*
 * Exynos specific support for Samsung pinctrl/gpiolib driver with eint support.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 * Copyright (c) 2012 Linaro Ltd
 *		http://www.linaro.org
 *
 * Author: Thomas Abraham <thomas.ab@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file contains the Samsung Exynos specific information required by the
 * the Samsung pinctrl/gpiolib driver. It also includes the implementation of
 * external gpio and wakeup interrupt support.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/pinctrl/pinctrl-samsung.h>

#include <plat/cpu.h>
#include "pinctrl-exynos.h"

/* bank type for non-alive type (DRV bit field: 2) */
static struct samsung_pin_bank_type bank_type_0  = {
	.fld_width = { 4, 1, 2, 2, 2, 2, },
	.reg_offset = { 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, },
};

/* bank type for alive type (DRV bit field: 2) */
static struct samsung_pin_bank_type bank_type_1 = {
	.fld_width = { 4, 1, 2, 2, },
	.reg_offset = { 0x00, 0x04, 0x08, 0x0c, },
};

/* bank type for non-alive type (DRV bit field: 4) */
static struct samsung_pin_bank_type bank_type_2  = {
	.fld_width = { 4, 1, 2, 4, 2, 2, },
	.reg_offset = { 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, },
};

/* bank type for alive type (DRV bit field: 4) */
static struct samsung_pin_bank_type bank_type_3 = {
	.fld_width = { 4, 1, 2, 4, },
	.reg_offset = { 0x00, 0x04, 0x08, 0x0c, },
};

/* list of external wakeup controllers supported */
static const struct of_device_id exynos_wkup_irq_ids[] = {
	{ .compatible = "samsung,exynos4210-wakeup-eint", },
	{ }
};

static void exynos_gpio_irq_mask(struct irq_data *irqd)
{
	struct samsung_pin_bank *bank = irq_data_get_irq_chip_data(irqd);
	struct samsung_pinctrl_drv_data *d = bank->drvdata;
	unsigned long reg_mask = d->ctrl->geint_mask + bank->eint_offset;
	void __iomem *reg_base = (bank->eint_ext_offset) ?
			(d->virt_base + bank->eint_ext_offset) : d->virt_base;
	unsigned long mask;
	unsigned long flags;

	spin_lock_irqsave(&bank->slock, flags);

	mask = readl(reg_base + reg_mask);
	mask |= 1 << irqd->hwirq;
	writel(mask, reg_base + reg_mask);

	spin_unlock_irqrestore(&bank->slock, flags);
}

static void exynos_gpio_irq_ack(struct irq_data *irqd)
{
	struct samsung_pin_bank *bank = irq_data_get_irq_chip_data(irqd);
	struct samsung_pinctrl_drv_data *d = bank->drvdata;
	unsigned long reg_pend = d->ctrl->geint_pend + bank->eint_offset;
	void __iomem *reg_base = (bank->eint_ext_offset) ?
			(d->virt_base + bank->eint_ext_offset) : d->virt_base;

	writel(1 << irqd->hwirq, reg_base + reg_pend);
}

static void exynos_gpio_irq_unmask(struct irq_data *irqd)
{
	struct samsung_pin_bank *bank = irq_data_get_irq_chip_data(irqd);
	struct samsung_pinctrl_drv_data *d = bank->drvdata;
	unsigned long reg_mask = d->ctrl->geint_mask + bank->eint_offset;
	void __iomem *reg_base = (bank->eint_ext_offset) ?
			(d->virt_base + bank->eint_ext_offset) : d->virt_base;
	unsigned long mask;
	unsigned long flags;

	/*
	 * Ack level interrupts right before unmask
	 *
	 * If we don't do this we'll get a double-interrupt.  Level triggered
	 * interrupts must not fire an interrupt if the level is not
	 * _currently_ active, even if it was active while the interrupt was
	 * masked.
	 */
	if (irqd_get_trigger_type(irqd) & IRQ_TYPE_LEVEL_MASK)
		exynos_gpio_irq_ack(irqd);

	spin_lock_irqsave(&bank->slock, flags);

	mask = readl(reg_base + reg_mask);
	mask &= ~(1 << irqd->hwirq);
	writel(mask, reg_base + reg_mask);

	spin_unlock_irqrestore(&bank->slock, flags);
}

static int exynos_gpio_irq_set_type(struct irq_data *irqd, unsigned int type)
{
	struct samsung_pin_bank *bank = irq_data_get_irq_chip_data(irqd);
	struct samsung_pin_bank_type *bank_type = bank->type;
	struct samsung_pinctrl_drv_data *d = bank->drvdata;
	struct samsung_pin_ctrl *ctrl = d->ctrl;
	unsigned int pin = irqd->hwirq;
	unsigned int shift = EXYNOS_EINT_CON_LEN * pin;
	unsigned int con, trig_type;
	unsigned long reg_con = ctrl->geint_con + bank->eint_offset;
	void __iomem *reg_base = (bank->eint_ext_offset) ?
			(d->virt_base + bank->eint_ext_offset) : d->virt_base;
	unsigned long flags;
	unsigned int mask;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		trig_type = EXYNOS_EINT_EDGE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		trig_type = EXYNOS_EINT_EDGE_FALLING;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		trig_type = EXYNOS_EINT_EDGE_BOTH;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		trig_type = EXYNOS_EINT_LEVEL_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		trig_type = EXYNOS_EINT_LEVEL_LOW;
		break;
	default:
		pr_err("unsupported external interrupt type\n");
		return -EINVAL;
	}

	if (type & IRQ_TYPE_EDGE_BOTH)
		__irq_set_handler_locked(irqd->irq, handle_edge_irq);
	else
		__irq_set_handler_locked(irqd->irq, handle_level_irq);

	con = readl(reg_base + reg_con);
	con &= ~(EXYNOS_EINT_CON_MASK << shift);
	con |= trig_type << shift;
	writel(con, reg_base + reg_con);

	reg_con = bank->pctl_offset + bank_type->reg_offset[PINCFG_TYPE_FUNC];
	shift = pin * bank_type->fld_width[PINCFG_TYPE_FUNC];
	mask = (1 << bank_type->fld_width[PINCFG_TYPE_FUNC]) - 1;

	spin_lock_irqsave(&bank->slock, flags);

	con = readl(reg_base + reg_con);
	con &= ~(mask << shift);
	con |= EXYNOS_EINT_FUNC << shift;
	writel(con, reg_base + reg_con);

	spin_unlock_irqrestore(&bank->slock, flags);

	return 0;
}

/*
 * irq_chip for gpio interrupts.
 */
static struct irq_chip exynos_gpio_irq_chip = {
	.name		= "exynos_gpio_irq_chip",
	.irq_unmask	= exynos_gpio_irq_unmask,
	.irq_mask	= exynos_gpio_irq_mask,
	.irq_ack		= exynos_gpio_irq_ack,
	.irq_set_type	= exynos_gpio_irq_set_type,
};

static int exynos_gpio_irq_map(struct irq_domain *h, unsigned int virq,
					irq_hw_number_t hw)
{
	struct samsung_pin_bank *b = h->host_data;

	irq_set_chip_data(virq, b);
	irq_set_chip_and_handler(virq, &exynos_gpio_irq_chip,
					handle_level_irq);
	set_irq_flags(virq, IRQF_VALID);
	return 0;
}

/*
 * irq domain callbacks for external gpio interrupt controller.
 */
static const struct irq_domain_ops exynos_gpio_irqd_ops = {
	.map	= exynos_gpio_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

static irqreturn_t exynos_eint_gpio_irq(int irq, void *data)
{
	struct samsung_pinctrl_drv_data *d = data;
	struct samsung_pin_ctrl *ctrl = d->ctrl;
	struct samsung_pin_bank *bank = ctrl->pin_banks;
	unsigned int svc, group, pin, virq;
	void __iomem *reg_base = (bank->eint_ext_offset) ?
			(d->virt_base + bank->eint_ext_offset) : d->virt_base;

	svc = readl(reg_base + ctrl->svc);
	group = EXYNOS_SVC_GROUP(svc);
	pin = svc & EXYNOS_SVC_NUM_MASK;

	if (!group)
		return IRQ_HANDLED;
	bank += (group - 1);

	virq = irq_linear_revmap(bank->irq_domain, pin);
	if (!virq)
		return IRQ_NONE;
	generic_handle_irq(virq);
	return IRQ_HANDLED;
}

struct exynos_eint_gpio_save {
	u32 eint_con;
	u32 eint_fltcon0;
	u32 eint_fltcon1;
};

/*
 * exynos_eint_gpio_init() - setup handling of external gpio interrupts.
 * @d: driver data of samsung pinctrl driver.
 */
static int exynos_eint_gpio_init(struct samsung_pinctrl_drv_data *d)
{
	struct samsung_pin_bank *bank;
	struct device *dev = d->dev;
	int ret;
	int i;

	if (!d->irq) {
		dev_err(dev, "irq number not available\n");
		return -EINVAL;
	}

	ret = devm_request_irq(dev, d->irq, exynos_eint_gpio_irq,
					0, dev_name(dev), d);
	if (ret) {
		dev_err(dev, "irq request failed\n");
		return -ENXIO;
	}

	bank = d->ctrl->pin_banks;
	for (i = 0; i < d->ctrl->nr_banks; ++i, ++bank) {
		if (bank->eint_type != EINT_TYPE_GPIO)
			continue;
		bank->irq_domain = irq_domain_add_linear(bank->of_node,
				bank->nr_pins, &exynos_gpio_irqd_ops, bank);
		if (!bank->irq_domain) {
			dev_err(dev, "gpio irq domain add failed\n");
			ret = -ENXIO;
			goto err_domains;
		}

		bank->soc_priv = devm_kzalloc(d->dev,
			sizeof(struct exynos_eint_gpio_save), GFP_KERNEL);
		if (!bank->soc_priv) {
			irq_domain_remove(bank->irq_domain);
			ret = -ENOMEM;
			goto err_domains;
		}
	}

	return 0;

err_domains:
	for (--i, --bank; i >= 0; --i, --bank) {
		if (bank->eint_type != EINT_TYPE_GPIO)
			continue;
		irq_domain_remove(bank->irq_domain);
	}

	return ret;
}

static void exynos_wkup_irq_mask(struct irq_data *irqd)
{
	struct samsung_pin_bank *b = irq_data_get_irq_chip_data(irqd);
	struct samsung_pinctrl_drv_data *d = b->drvdata;
	unsigned long reg_mask = d->ctrl->weint_mask + b->eint_offset;
	void __iomem *reg_base = (b->eint_ext_offset) ?
			(d->virt_base + b->eint_ext_offset) : d->virt_base;
	unsigned long mask;
	unsigned long flags;

	spin_lock_irqsave(&b->slock, flags);

	mask = readl(reg_base + reg_mask);
	mask |= 1 << irqd->hwirq;
	writel(mask, reg_base + reg_mask);
	spin_unlock_irqrestore(&b->slock, flags);
}

static void exynos_wkup_irq_ack(struct irq_data *irqd)
{
	struct samsung_pin_bank *b = irq_data_get_irq_chip_data(irqd);
	struct samsung_pinctrl_drv_data *d = b->drvdata;
	unsigned long pend = d->ctrl->weint_pend + b->eint_offset;
	void __iomem *reg_base = (b->eint_ext_offset) ?
			(d->virt_base + b->eint_ext_offset) : d->virt_base;

	writel(1 << irqd->hwirq, reg_base + pend);
}

static void exynos_wkup_irq_unmask(struct irq_data *irqd)
{
	struct samsung_pin_bank *b = irq_data_get_irq_chip_data(irqd);
	struct samsung_pinctrl_drv_data *d = b->drvdata;
	unsigned long reg_mask = d->ctrl->weint_mask + b->eint_offset;
	void __iomem *reg_base = (b->eint_ext_offset) ?
			(d->virt_base + b->eint_ext_offset) : d->virt_base;
	unsigned long mask;
	unsigned long flags;

	/*
	 * Ack level interrupts right before unmask
	 *
	 * If we don't do this we'll get a double-interrupt.  Level triggered
	 * interrupts must not fire an interrupt if the level is not
	 * _currently_ active, even if it was active while the interrupt was
	 * masked.
	 */
	if (irqd_get_trigger_type(irqd) & IRQ_TYPE_LEVEL_MASK)
		exynos_wkup_irq_ack(irqd);

	spin_lock_irqsave(&b->slock, flags);

	mask = readl(reg_base + reg_mask);
	mask &= ~(1 << irqd->hwirq);
	writel(mask, reg_base + reg_mask);

	spin_unlock_irqrestore(&b->slock, flags);
}

static int exynos_wkup_irq_set_type(struct irq_data *irqd, unsigned int type)
{
	struct samsung_pin_bank *bank = irq_data_get_irq_chip_data(irqd);
	struct samsung_pin_bank_type *bank_type = bank->type;
	struct samsung_pinctrl_drv_data *d = bank->drvdata;
	unsigned int pin = irqd->hwirq;
	unsigned long reg_con = d->ctrl->weint_con + bank->eint_offset;
	void __iomem *reg_base = (bank->eint_ext_offset) ?
			d->virt_base + bank->eint_ext_offset : d->virt_base;
	void __iomem *reg_ext_base = (bank->eint_ext_offset) ?
			d->virt_ext_base : d->virt_base;
	unsigned long shift = EXYNOS_EINT_CON_LEN * pin;
	unsigned long con, trig_type;
	unsigned long flags;
	unsigned int mask;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		trig_type = EXYNOS_EINT_EDGE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		trig_type = EXYNOS_EINT_EDGE_FALLING;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		trig_type = EXYNOS_EINT_EDGE_BOTH;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		trig_type = EXYNOS_EINT_LEVEL_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		trig_type = EXYNOS_EINT_LEVEL_LOW;
		break;
	default:
		pr_err("unsupported external interrupt type\n");
		return -EINVAL;
	}

	if (type & IRQ_TYPE_EDGE_BOTH)
		__irq_set_handler_locked(irqd->irq, handle_edge_irq);
	else
		__irq_set_handler_locked(irqd->irq, handle_level_irq);

	con = readl(reg_base + reg_con);
	con &= ~(EXYNOS_EINT_CON_MASK << shift);
	con |= trig_type << shift;
	writel(con, reg_base + reg_con);

	reg_con = bank->pctl_offset + bank_type->reg_offset[PINCFG_TYPE_FUNC];
	shift = pin * bank_type->fld_width[PINCFG_TYPE_FUNC];
	mask = (1 << bank_type->fld_width[PINCFG_TYPE_FUNC]) - 1;

	spin_lock_irqsave(&bank->slock, flags);

	con = readl(reg_ext_base + reg_con);
	con &= ~(mask << shift);
	con |= EXYNOS_EINT_FUNC << shift;
	writel(con, reg_ext_base + reg_con);

	spin_unlock_irqrestore(&bank->slock, flags);

	return 0;
}

static u64 exynos_eint_wake_mask = 0xffffffffffffffff;

u64 exynos_get_eint_wake_mask(void)
{
	return exynos_eint_wake_mask;
}

static int exynos_wkup_irq_set_wake(struct irq_data *irqd, unsigned int on)
{
	struct samsung_pin_bank *bank = irq_data_get_irq_chip_data(irqd);
	/*
	 * eint_ext_val is for extended eint,
	 * if it is eint_ext, it is added 32 shifts on based.
	 */
	u32 shift = bank->eint_ext_offset ? bank->eint_num_base : 2 * bank->eint_offset;

	u64 bit;

	bit = (u64)(1ULL << (shift + irqd->hwirq));

	pr_info("wake %s for irq %d\n", on ? "enabled" : "disabled", irqd->irq);

	if (!on)
		exynos_eint_wake_mask |= bit;
	else
		exynos_eint_wake_mask &= ~bit;

	return 0;
}

/*
 * irq_chip for wakeup interrupts
 */
static struct irq_chip exynos_wkup_irq_chip = {
	.name	= "exynos_wkup_irq_chip",
	.irq_unmask	= exynos_wkup_irq_unmask,
	.irq_mask	= exynos_wkup_irq_mask,
	.irq_ack	= exynos_wkup_irq_ack,
	.irq_set_type	= exynos_wkup_irq_set_type,
	.irq_set_wake	= exynos_wkup_irq_set_wake,
};

/* interrupt handler for wakeup interrupts 0..15 */
static void exynos_irq_eint0_15(unsigned int irq, struct irq_desc *desc)
{
	struct exynos_weint_data *eintd = irq_get_handler_data(irq);
	struct samsung_pin_bank *bank = eintd->bank;
	struct irq_chip *chip = irq_get_chip(irq);
	int eint_irq;

	chained_irq_enter(chip, desc);
	chip->irq_mask(&desc->irq_data);

	if (chip->irq_ack)
		chip->irq_ack(&desc->irq_data);

	eint_irq = irq_linear_revmap(bank->irq_domain, eintd->irq);
	generic_handle_irq(eint_irq);
	chip->irq_unmask(&desc->irq_data);
	chained_irq_exit(chip, desc);
}

static inline void exynos_irq_demux_eint(unsigned long pend,
						struct irq_domain *domain)
{
	unsigned int irq;

	while (pend) {
		irq = fls(pend) - 1;
		generic_handle_irq(irq_find_mapping(domain, irq));
		pend &= ~(1 << irq);
	}
}

/* interrupt handler for wakeup interrupt 16 */
static void exynos_irq_demux_eint16_31(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_get_chip(irq);
	struct exynos_muxed_weint_data *eintd = irq_get_handler_data(irq);
	struct samsung_pinctrl_drv_data *d = eintd->banks[0]->drvdata;
	struct samsung_pin_ctrl *ctrl = d->ctrl;
	void __iomem *reg_base;
	unsigned long pend;
	unsigned long mask;
	int i;

	chained_irq_enter(chip, desc);

	for (i = 0; i < eintd->nr_banks; ++i) {
		struct samsung_pin_bank *b = eintd->banks[i];
		reg_base = (b->eint_ext_offset) ?
			(d->virt_base + b->eint_ext_offset) : d->virt_base;
		pend = readl(reg_base + ctrl->weint_pend + b->eint_offset);
		mask = readl(reg_base + ctrl->weint_mask + b->eint_offset);

		exynos_irq_demux_eint(pend & ~mask, b->irq_domain);
	}

	chained_irq_exit(chip, desc);
}

static int exynos_wkup_irq_map(struct irq_domain *h, unsigned int virq,
					irq_hw_number_t hw)
{
	irq_set_chip_and_handler(virq, &exynos_wkup_irq_chip, handle_level_irq);
	irq_set_chip_data(virq, h->host_data);
	set_irq_flags(virq, IRQF_VALID);
	return 0;
}

/*
 * irq domain callbacks for external wakeup interrupt controller.
 */
static const struct irq_domain_ops exynos_wkup_irqd_ops = {
	.map	= exynos_wkup_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

static void exynos_eint_get_fltcon(struct samsung_pinctrl_drv_data *d,
				   struct samsung_pin_bank *bank,
				   unsigned int *fltcon0, unsigned int *fltcon1)
{
	if (bank->eint_fltcon_type == EINT_FLTCON_NORMAL) {
		*fltcon0 = d->ctrl->weint_fltcon + 2 * bank->eint_offset;
		*fltcon1 = *fltcon0 + 0x4;
	} else {
		*fltcon0 = d->ctrl->weint_fltcon + bank->eint_fltcon0_offset;
		*fltcon1 = d->ctrl->weint_fltcon + bank->eint_fltcon1_offset;
	}
}

static void exynos_eint_flt_config(int en, int sel, int width,
				   struct samsung_pinctrl_drv_data *d,
				   struct samsung_pin_bank *bank)
{
	unsigned int fltcon0_reg = 0;
	unsigned int fltcon1_reg = 0;
	unsigned int flt_con;
	unsigned int val, shift;
	int i;
	void __iomem *reg_base = (bank->eint_ext_offset) ?
			(d->virt_base + bank->eint_ext_offset) : d->virt_base;

	flt_con = 0;

	if (en)
		flt_con |= EXYNOS_EINT_FLTCON_EN;

	if (sel)
		flt_con |= EXYNOS_EINT_FLTCON_SEL;

	flt_con |= EXYNOS_EINT_FLTCON_WIDTH(width);

	exynos_eint_get_fltcon(d, bank, &fltcon0_reg, &fltcon1_reg);
	for (i = 0; i < EXYNOS_EINT_FLTCON_LEN >> 1; i++) {
		shift = i * EXYNOS_EINT_FLTCON_LEN;
		val = readl(reg_base + fltcon0_reg);
		val &= ~(EXYNOS_EINT_FLTCON_MASK << shift);
		val |= (flt_con << shift);
		writel(val, reg_base + fltcon0_reg);
		if (bank->nr_pins > 4)
			writel(val, reg_base + fltcon1_reg);
	}
};

/*
 * exynos_eint_wkup_init() - setup handling of external wakeup interrupts.
 * @d: driver data of samsung pinctrl driver.
 */
static int exynos_eint_wkup_init(struct samsung_pinctrl_drv_data *d)
{
	struct device *dev = d->dev;
	struct device_node *wkup_np = NULL;
	struct device_node *np;
	struct samsung_pin_bank *bank;
	struct exynos_weint_data *weint_data;
	struct exynos_muxed_weint_data *muxed_data;
	unsigned int muxed_banks = 0;
	unsigned int i;
	int idx, irq;

	for_each_child_of_node(dev->of_node, np) {
		if (of_match_node(exynos_wkup_irq_ids, np)) {
			wkup_np = np;
			break;
		}
	}
	if (!wkup_np)
		return -ENODEV;

	if (of_property_read_bool(wkup_np, "samsung,eint-flt-conf")) {
		pr_info("%s: Need to configure eint filter\n", __func__);
		d->eint_flt_config = true;
	}

	bank = d->ctrl->pin_banks;
	for (i = 0; i < d->ctrl->nr_banks; ++i, ++bank) {
		if (bank->eint_type != EINT_TYPE_WKUP)
			continue;

		bank->irq_domain = irq_domain_add_linear(bank->of_node,
				bank->nr_pins, &exynos_wkup_irqd_ops, bank);
		if (!bank->irq_domain) {
			dev_err(dev, "wkup irq domain add failed\n");
			return -ENXIO;
		}

		bank->soc_priv = devm_kzalloc(d->dev,
			sizeof(struct exynos_eint_gpio_save), GFP_KERNEL);
		if (!bank->soc_priv) {
			irq_domain_remove(bank->irq_domain);
			return -ENOMEM;
		}

		if (!of_find_property(bank->of_node, "interrupts", NULL)) {
			bank->eint_type = EINT_TYPE_WKUP_MUX;
			++muxed_banks;
			continue;
		}

		weint_data = devm_kzalloc(dev, bank->nr_pins
					* sizeof(*weint_data), GFP_KERNEL);
		if (!weint_data) {
			dev_err(dev, "could not allocate memory for weint_data\n");
			return -ENOMEM;
		}

		for (idx = 0; idx < bank->nr_pins; ++idx) {
			irq = irq_of_parse_and_map(bank->of_node, idx);
			if (!irq) {
				dev_err(dev, "irq number for eint-%s-%d not found\n",
							bank->name, idx);
				continue;
			}
			weint_data[idx].irq = idx;
			weint_data[idx].bank = bank;
			irq_set_handler_data(irq, &weint_data[idx]);
			irq_set_chained_handler(irq, exynos_irq_eint0_15);
		}
	}

	if (!muxed_banks)
		return 0;

	irq = irq_of_parse_and_map(wkup_np, 0);
	if (!irq) {
		dev_err(dev, "irq number for muxed EINTs not found\n");
		return 0;
	}

	muxed_data = devm_kzalloc(dev, sizeof(*muxed_data)
		+ muxed_banks*sizeof(struct samsung_pin_bank *), GFP_KERNEL);
	if (!muxed_data) {
		dev_err(dev, "could not allocate memory for muxed_data\n");
		return -ENOMEM;
	}

	irq_set_chained_handler(irq, exynos_irq_demux_eint16_31);
	irq_set_handler_data(irq, muxed_data);

	bank = d->ctrl->pin_banks;
	idx = 0;
	for (i = 0; i < d->ctrl->nr_banks; ++i, ++bank) {
		if (bank->eint_type != EINT_TYPE_WKUP_MUX)
			continue;

		muxed_data->banks[idx++] = bank;
	}
	muxed_data->nr_banks = muxed_banks;

	if (d->eint_flt_config) {
		bank = d->ctrl->pin_banks;
		for (i = 0; i < d->ctrl->nr_banks; ++i, ++bank)
			exynos_eint_flt_config(EXYNOS_EINT_FLTCON_EN,
				 EXYNOS_EINT_FLTCON_SEL, 0, d, bank);
	}

	return 0;
}

static void exynos_pinctrl_suspend_bank(
				struct samsung_pinctrl_drv_data *drvdata,
				struct samsung_pin_bank *bank)
{
	struct exynos_eint_gpio_save *save = bank->soc_priv;
	unsigned int geint_con = drvdata->ctrl->geint_con;
	unsigned int fltcon0_reg = 0;
	unsigned int fltcon1_reg = 0;
	void __iomem *regs = (bank->eint_ext_offset) ?
		(drvdata->virt_base + bank->eint_ext_offset) : drvdata->virt_base;

	exynos_eint_get_fltcon(drvdata, bank, &fltcon0_reg, &fltcon1_reg);

	save->eint_con = readl(regs + geint_con + bank->eint_offset);
	pr_debug("%s: save     con %#010x\n", bank->name, save->eint_con);

	save->eint_fltcon0 = readl(regs + fltcon0_reg);
	pr_debug("%s: save fltcon0 %#010x\n", bank->name, save->eint_fltcon0);

	if (bank->nr_pins > 4) {
		save->eint_fltcon1 = readl(regs + fltcon1_reg);
		pr_debug("%s: save fltcon1 %#010x\n", bank->name, save->eint_fltcon1);
	}
}

static void exynos5430_pinctrl_suspend(struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	struct samsung_pin_bank *bank = ctrl->pin_banks;
	struct samsung_pinctrl_drv_data *d = bank->drvdata;

	int i;
	for (i = 0; i < ctrl->nr_banks; ++i, ++bank) {
		if (bank->eint_type == EINT_TYPE_GPIO
			|| bank->eint_type == EINT_TYPE_WKUP
			|| bank->eint_type == EINT_TYPE_WKUP_MUX) {
			exynos_pinctrl_suspend_bank(drvdata, bank);
			if (d->eint_flt_config)
				exynos_eint_flt_config(EXYNOS_EINT_FLTCON_EN, 0,
					       0, drvdata, bank);
		}
	}
}

static void exynos_pinctrl_suspend(struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	struct samsung_pin_bank *bank = ctrl->pin_banks;
	int i;
	for (i = 0; i < ctrl->nr_banks; ++i, ++bank)
		if (bank->eint_type == EINT_TYPE_GPIO)
			exynos_pinctrl_suspend_bank(drvdata, bank);
}

static void exynos_pinctrl_resume_bank(
				struct samsung_pinctrl_drv_data *drvdata,
				struct samsung_pin_bank *bank)
{
	struct exynos_eint_gpio_save *save = bank->soc_priv;
	unsigned int geint_con = drvdata->ctrl->geint_con;
	unsigned int fltcon0_reg = 0;
	unsigned int fltcon1_reg = 0;
	void __iomem *regs = (bank->eint_ext_offset) ?
		(drvdata->virt_base + bank->eint_ext_offset) : drvdata->virt_base;

	exynos_eint_get_fltcon(drvdata, bank, &fltcon0_reg, &fltcon1_reg);

	pr_debug("%s:     con %#010x => %#010x\n", bank->name,
			readl(regs + geint_con + bank->eint_offset),
			save->eint_con);
	writel(save->eint_con, regs + geint_con + bank->eint_offset);

	pr_debug("%s: fltcon0 %#010x => %#010x\n", bank->name,
			readl(regs + fltcon0_reg), save->eint_fltcon0);
	writel(save->eint_fltcon0, regs + fltcon0_reg);

	if (bank->nr_pins > 4) {
		pr_debug("%s: fltcon1 %#010x => %#010x\n", bank->name,
			readl(regs + fltcon1_reg), save->eint_fltcon1);
		writel(save->eint_fltcon1, regs + fltcon1_reg);
	}
}

static void exynos5430_pinctrl_resume(struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	struct samsung_pin_bank *bank = ctrl->pin_banks;
	int i;

	for (i = 0; i < ctrl->nr_banks; ++i, ++bank)
		exynos_pinctrl_resume_bank(drvdata, bank);
}

static void exynos_pinctrl_resume(struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	struct samsung_pin_bank *bank = ctrl->pin_banks;
	int i;

	for (i = 0; i < ctrl->nr_banks; ++i, ++bank)
		if (bank->eint_type == EINT_TYPE_GPIO)
			exynos_pinctrl_resume_bank(drvdata, bank);
}

/* pin banks of exynos4210 pin-controller 0 */
static struct samsung_pin_bank exynos4210_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x040, "gpb", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x060, "gpc0", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x080, "gpc1", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0A0, "gpd0", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0C0, "gpd1", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x0E0, "gpe0", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x100, "gpe1", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x120, "gpe2", 0x24),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x140, "gpe3", 0x28),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x160, "gpe4", 0x2c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x180, "gpf0", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x1A0, "gpf1", 0x34),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x1C0, "gpf2", 0x38),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x1E0, "gpf3", 0x3c),
};

/* pin banks of exynos4210 pin-controller 1 */
static struct samsung_pin_bank exynos4210_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpj0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x020, "gpj1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x040, "gpk0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x060, "gpk1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x080, "gpk2", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x0A0, "gpk3", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0C0, "gpl0", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 3, 0x0E0, "gpl1", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x100, "gpl2", 0x20),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x120, "gpy0"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 4, 0x140, "gpy1"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x160, "gpy2"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x180, "gpy3"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x1A0, "gpy4"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x1C0, "gpy5"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x1E0, "gpy6"),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC00, "gpx0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC20, "gpx1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC40, "gpx2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC60, "gpx3", 0x0c),
};

/* pin banks of exynos4210 pin-controller 2 */
static struct samsung_pin_bank exynos4210_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 7, 0x000, "gpz"),
};

/*
 * Samsung pinctrl driver data for Exynos4210 SoC. Exynos4210 SoC includes
 * three gpio/pin-mux/pinconfig controllers.
 */
struct samsung_pin_ctrl exynos4210_pin_ctrl[] = {
	{
		/* pin-controller instance 0 data */
		.pin_banks	= exynos4210_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos4210_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos4210-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 data */
		.pin_banks	= exynos4210_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos4210_pin_banks1),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS_WKUP_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos4210-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 data */
		.pin_banks	= exynos4210_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos4210_pin_banks2),
		.label		= "exynos4210-gpio-ctrl2",
	},
};

/* pin banks of exynos4x12 pin-controller 0 */
static struct samsung_pin_bank exynos4x12_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x040, "gpb", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x060, "gpc0", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x080, "gpc1", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0A0, "gpd0", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0C0, "gpd1", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x180, "gpf0", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x1A0, "gpf1", 0x34),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x1C0, "gpf2", 0x38),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x1E0, "gpf3", 0x3c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x240, "gpj0", 0x40),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x260, "gpj1", 0x44),
};

/* pin banks of exynos4x12 pin-controller 1 */
static struct samsung_pin_bank exynos4x12_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x040, "gpk0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x060, "gpk1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x080, "gpk2", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x0A0, "gpk3", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x0C0, "gpl0", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x0E0, "gpl1", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x100, "gpl2", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x260, "gpm0", 0x24),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x280, "gpm1", 0x28),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x2A0, "gpm2", 0x2c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x2C0, "gpm3", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x2E0, "gpm4", 0x34),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x120, "gpy0"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 4, 0x140, "gpy1"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x160, "gpy2"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x180, "gpy3"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x1A0, "gpy4"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x1C0, "gpy5"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x1E0, "gpy6"),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC00, "gpx0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC20, "gpx1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC40, "gpx2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC60, "gpx3", 0x0c),
};

/* pin banks of exynos4x12 pin-controller 2 */
static struct samsung_pin_bank exynos4x12_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x000, "gpz", 0x00),
};

/* pin banks of exynos4x12 pin-controller 3 */
static struct samsung_pin_bank exynos4x12_pin_banks3[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpv0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x020, "gpv1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x040, "gpv2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x060, "gpv3", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x080, "gpv4", 0x10),
};

/*
 * Samsung pinctrl driver data for Exynos4x12 SoC. Exynos4x12 SoC includes
 * four gpio/pin-mux/pinconfig controllers.
 */
struct samsung_pin_ctrl exynos4x12_pin_ctrl[] = {
	{
		/* pin-controller instance 0 data */
		.pin_banks	= exynos4x12_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos4x12_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos4x12-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 data */
		.pin_banks	= exynos4x12_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos4x12_pin_banks1),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS_WKUP_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos4x12-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 data */
		.pin_banks	= exynos4x12_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos4x12_pin_banks2),
		.label		= "exynos4x12-gpio-ctrl2",
	}, {
		/* pin-controller instance 3 data */
		.pin_banks	= exynos4x12_pin_banks3,
		.nr_banks	= ARRAY_SIZE(exynos4x12_pin_banks3),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos4x12-gpio-ctrl3",
	},
};

/* pin banks of exynos5250 pin-controller 0 */
static struct samsung_pin_bank exynos5250_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x040, "gpa2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x060, "gpb0", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x080, "gpb1", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0A0, "gpb2", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0C0, "gpb3", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x0E0, "gpc0", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x100, "gpc1", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x120, "gpc2", 0x24),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x140, "gpc3", 0x28),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x160, "gpd0", 0x2c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x180, "gpd1", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x2E0, "gpc4", 0x34),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x1A0, "gpy0"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 4, 0x1C0, "gpy1"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x1E0, "gpy2"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x200, "gpy3"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x220, "gpy4"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x240, "gpy5"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x260, "gpy6"),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC00, "gpx0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC20, "gpx1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC40, "gpx2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xC60, "gpx3", 0x0c),
};

/* pin banks of exynos5250 pin-controller 1 */
static struct samsung_pin_bank exynos5250_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpe0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x020, "gpe1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x040, "gpf0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x060, "gpf1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x080, "gpg0", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0A0, "gpg1", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x0C0, "gpg2", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0E0, "gph0", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x100, "gph1", 0x20),
};

/* pin banks of exynos5250 pin-controller 2 */
static struct samsung_pin_bank exynos5250_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpv0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x020, "gpv1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x060, "gpv2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x080, "gpv3", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x0C0, "gpv4", 0x10),
};

/* pin banks of exynos5250 pin-controller 3 */
static struct samsung_pin_bank exynos5250_pin_banks3[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x000, "gpz", 0x00),
};

/*
 * Samsung pinctrl driver data for Exynos5250 SoC. Exynos5250 SoC includes
 * four gpio/pin-mux/pinconfig controllers.
 */
struct samsung_pin_ctrl exynos5250_pin_ctrl[] = {
	{
		/* pin-controller instance 0 data */
		.pin_banks	= exynos5250_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos5250_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS_WKUP_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos5250-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 data */
		.pin_banks	= exynos5250_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos5250_pin_banks1),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos5250-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 data */
		.pin_banks	= exynos5250_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos5250_pin_banks2),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos5250-gpio-ctrl2",
	}, {
		/* pin-controller instance 3 data */
		.pin_banks	= exynos5250_pin_banks3,
		.nr_banks	= ARRAY_SIZE(exynos5250_pin_banks3),
		.label		= "exynos5250-gpio-ctrl3",
	},
};

/* pin banks of exynos5422 pin-controller 0 (Right) */
static struct samsung_pin_bank exynos5422_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpy7", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xc00, "gpx0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xc20, "gpx1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xc40, "gpx2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0xc60, "gpx3", 0x0c),
};

/* pin banks of exynos5422 pin-controller 1 (Top) */
static struct samsung_pin_bank exynos5422_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpc0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x020, "gpc1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x040, "gpc2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x060, "gpc3", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x080, "gpc4", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0A0, "gpd1", 0x14),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x0C0, "gpy0"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 4, 0x0E0, "gpy1"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 6, 0x100, "gpy2"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x120, "gpy3"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x140, "gpy4"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x160, "gpy5"),
	EXYNOS_PIN_BANK_EINTN(bank_type_0, 8, 0x180, "gpy6"),
};

/* pin banks of exynos5422 pin-controller 2 (Left) */
static struct samsung_pin_bank exynos5422_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpe0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x020, "gpe1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x040, "gpf0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x060, "gpf1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x080, "gpg0", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0A0, "gpg1", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x0C0, "gpg2", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0E0, "gpj4", 0x1c),
};

/* pin banks of exynos5422 pin-controller 3 (Bottom) */
static struct samsung_pin_bank exynos5422_pin_banks3[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x040, "gpa2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x060, "gpb0", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x080, "gpb1", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0A0, "gpb2", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0C0, "gpb3", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x0E0, "gpb4", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x100, "gph0", 0x20),
};

/* pin banks of exynos5422 pin-controller 4 (Audio) */
static struct samsung_pin_bank exynos5422_pin_banks4[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x000, "gpz", 0x00),
};

/*
 * Samsung pinctrl driver data for Exynos5422 SoC. Exynos5422 SoC includes
 * four gpio/pin-mux/pinconfig controllers.
 */
struct samsung_pin_ctrl exynos5422_pin_ctrl[] = {
	{
		/* pin-controller instance 0 data */
		.pin_banks	= exynos5422_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos5422_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS_WKUP_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos5422-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 data */
		.pin_banks	= exynos5422_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos5422_pin_banks1),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos5422-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 data */
		.pin_banks	= exynos5422_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos5422_pin_banks2),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos5422-gpio-ctrl2",
	}, {
		/* pin-controller instance 3 data */
		.pin_banks	= exynos5422_pin_banks3,
		.nr_banks	= ARRAY_SIZE(exynos5422_pin_banks3),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
		.label		= "exynos5422-gpio-ctrl3",
	}, {
		/* pin-controller instance 4 data */
		.pin_banks	= exynos5422_pin_banks4,
		.nr_banks	= ARRAY_SIZE(exynos5422_pin_banks4),
		.label		= "exynos5422-gpio-ctrl4",
	},
};

/* pin banks of exynos5430 pin-controller 0 (ALIVE) */
static struct samsung_pin_bank exynos5430_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x040, "gpa2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x060, "gpa3", 0x0c),
};

/* pin banks of exynos5430 pin-controller 1 (AUD) */
static struct samsung_pin_bank exynos5430_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 7, 0x000, "gpz0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x020, "gpz1", 0x04),
};

/* pin banks of exynos5430 pin-controller 2 (CPIF) */
static struct samsung_pin_bank exynos5430_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpv6", 0x00),
};

/* pin banks of exynos5430 pin-controller 3 (FSYS) */
static struct samsung_pin_bank exynos5430_pin_banks3[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x000, "gph1", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 7, 0x020, "gpr4", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x040, "gpr0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x060, "gpr1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x080, "gpr2", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x0a0, "gpr3", 0x14),
};

/* pin banks of exynos5430 pin-controller 4 (NFC) */
static struct samsung_pin_bank exynos5430_pin_banks4[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpj0", 0x00),
};

/* pin banks of exynos5430 pin-controller 5 (PERIC) */
static struct samsung_pin_bank exynos5430_pin_banks5[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x000, "gpv7", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x020, "gpb0", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x040, "gpc0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x060, "gpc1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x080, "gpc2", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x0a0, "gpc3", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x0c0, "gpg0", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x0e0, "gpd0", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x100, "gpd1", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x120, "gpd2", 0x24),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x140, "gpd4", 0x28),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x160, "gpd5", 0x2c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x180, "gpd8", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x1a0, "gpd6", 0x34),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x1c0, "gpd7", 0x38),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x1e0, "gpf0", 0x3c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x200, "gpf1", 0x40),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x220, "gpf2", 0x44),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x240, "gpf3", 0x48),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x260, "gpf4", 0x4c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x280, "gpf5", 0x50),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x2c0, "gpg1", 0x54),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x2e0, "gpg2", 0x58),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x300, "gpg3", 0x5c),
};

/* pin banks of exynos5430 pin-controller 6 (TOUCH) */
static struct samsung_pin_bank exynos5430_pin_banks6[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpj1", 0x00),
};

/* pin banks of exynos7420 pin-controller 0 (ALIVE) */
static struct samsung_pin_bank exynos7420_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0x040, "gpa2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_1, 8, 0x060, "gpa3", 0x0c),
};

/* pin banks of exynos7420 pin-controller 1 (AUD) */
static struct samsung_pin_bank exynos7420_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x000, "gpz0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x020, "gpz1", 0x04),
};

/* pin banks of exynos7420 pin-controller 2 (BUS0) */
static struct samsung_pin_bank exynos7420_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x000, "gpb0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x020, "gpc0", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x040, "gpc1", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x060, "gpc2", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x080, "gpc3", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x0a0, "gpd0", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x0c0, "gpd1", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0e0, "gpd2", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x100, "gpd4", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x120, "gpd5", 0x24),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x140, "gpd6", 0x28),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 3, 0x160, "gpd7", 0x2c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x180, "gpd8", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 2, 0x1a0, "gpg0", 0x34),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x1c0, "gpg3", 0x38),
};

/* pin banks of exynos7420 pin-controller 3 (BUS1) */
static struct samsung_pin_bank exynos7420_pin_banks3[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x020, "gpf0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x040, "gpf1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x060, "gpf2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x080, "gpf3", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0a0, "gpf4", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x0c0, "gpf5", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x0e0, "gpg1", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x100, "gpg2", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 6, 0x120, "gph1", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 3, 0x140, "gpv6", 0x24),
};

/* pin banks of exynos7420 pin-controller 4 (NFC) */
static struct samsung_pin_bank exynos7420_pin_banks4[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 3, 0x000, "gpj0", 0x00),
};

/* pin banks of exynos7420 pin-controller 5 (TOUCH) */
static struct samsung_pin_bank exynos7420_pin_banks5[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 3, 0x000, "gpj1", 0x00),
};

/* pin banks of exynos7420 pin-controller 6 (FF) */
static struct samsung_pin_bank exynos7420_pin_banks6[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x000, "gpg4", 0x00),
};

/* pin banks of exynos7420 pin-controller 7 (ESE) */
static struct samsung_pin_bank exynos7420_pin_banks7[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x000, "gpv7", 0x00),
};

/* pin banks of exynos7420 pin-controller 8 (FSYS0) */
static struct samsung_pin_bank exynos7420_pin_banks8[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 7, 0x000, "gpr4", 0x00),
};

/* pin banks of exynos7420 pin-controller 9 (FSYS1) */
static struct samsung_pin_bank exynos7420_pin_banks9[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 4, 0x000, "gpr0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x020, "gpr1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 5, 0x040, "gpr2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_0, 8, 0x060, "gpr3", 0x0c),
};

/*
 * Samsung pinctrl driver data for Exynos5430 SoC. Exynos5430 SoC includes
 * four gpio/pin-mux/pinconfig controllers.
 */
struct samsung_pin_ctrl exynos5430_pin_ctrl[] = {
	{
		/* pin-controller instance 0 data */
		.pin_banks	= exynos5430_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos5430_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS5430_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS5430_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS5430_WKUP_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DAT_CLEAR,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5430-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 data */
		.pin_banks	= exynos5430_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos5430_pin_banks1),
		.gpio_type	= EXYNOS_GPIO_TYPE_DAT_CLEAR,
		.label		= "exynos5430-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 data */
		.pin_banks	= exynos5430_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos5430_pin_banks2),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DAT_CLEAR,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5430-gpio-ctrl2",
	}, {
		/* pin-controller instance 3 data */
		.pin_banks	= exynos5430_pin_banks3,
		.nr_banks	= ARRAY_SIZE(exynos5430_pin_banks3),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DAT_CLEAR,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5430-gpio-ctrl3",
	}, {
		/* pin-controller instance 4 data */
		.pin_banks	= exynos5430_pin_banks4,
		.nr_banks	= ARRAY_SIZE(exynos5430_pin_banks4),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DAT_CLEAR,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5430-gpio-ctrl4",
	}, {
		/* pin-controller instance 5 data */
		.pin_banks	= exynos5430_pin_banks5,
		.nr_banks	= ARRAY_SIZE(exynos5430_pin_banks5),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DAT_CLEAR,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5430-gpio-ctrl5",
	}, {
		/* pin-controller instance 6 data */
		.pin_banks	= exynos5430_pin_banks6,
		.nr_banks	= ARRAY_SIZE(exynos5430_pin_banks6),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DAT_CLEAR,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5430-gpio-ctrl6",
	},
};

struct samsung_pin_ctrl exynos7420_pin_ctrl[] = {
	{
		/* pin-controller instance 0 Alive data */
		.pin_banks	= exynos7420_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS5430_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS5430_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS5430_WKUP_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 AUD data */
		.pin_banks	= exynos7420_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks1),
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.label		= "exynos7420-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 BUS0 data */
		.pin_banks	= exynos7420_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks2),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl2",
	}, {
		/* pin-controller instance 3 BUS1 data */
		.pin_banks	= exynos7420_pin_banks3,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks3),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl3",
	}, {
		/* pin-controller instance 4 NFC data */
		.pin_banks	= exynos7420_pin_banks4,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks4),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl4",
	}, {
		/* pin-controller instance 5 TOUCH data */
		.pin_banks	= exynos7420_pin_banks5,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks5),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl5",
	}, {
		/* pin-controller instance 6 FF data */
		.pin_banks	= exynos7420_pin_banks6,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks6),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl6",
	}, {
		/* pin-controller instance 7 ESE data */
		.pin_banks	= exynos7420_pin_banks7,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks7),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl7",
	}, {
		/* pin-controller instance 8 FSYS0 data */
		.pin_banks	= exynos7420_pin_banks8,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks8),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl8",
	}, {
		/* pin-controller instance 9 FSYS1 data */
		.pin_banks	= exynos7420_pin_banks9,
		.nr_banks	= ARRAY_SIZE(exynos7420_pin_banks9),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.gpio_type	= EXYNOS_GPIO_TYPE_DRV_SEPARATE_3BIT,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos7420-gpio-ctrl9",
	},
};

/* pin banks of exynos7580 pin-controller 0 (ALIVE) */
static struct samsung_pin_bank exynos7580_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x040, "gpa2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x060, "gpa3", 0x0c),
};

/* pin banks of exynos7580 pin-controller 1 (AUD) */
static struct samsung_pin_bank exynos7580_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x000, "gpz0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x020, "gpz1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x040, "gpz2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x060, "gpz3", 0x0c),
};

/* pin banks of exynos7580 pin-controller 2 (ESE) */
static struct samsung_pin_bank exynos7580_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x000, "gpc0", 0x00),
};

/* pin banks of exynos7580 pin-controller 3 (FSYS) */
static struct samsung_pin_bank exynos7580_pin_banks3[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpr0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x020, "gpr1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x040, "gpr2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x060, "gpr3", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 7, 0x080, "gpr4", 0x10),
};

/* pin banks of exynos7580 pin-controller 4 (MIF) */
static struct samsung_pin_bank exynos7580_pin_banks4[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x000, "gpm0", 0x00),
};

/* pin banks of exynos7580 pin-controller 5 (NFC) */
static struct samsung_pin_bank exynos7580_pin_banks5[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x000, "gpc3", 0x00),
};

/* pin banks of exynos7580 pin-controller 6 (TOP) */
static struct samsung_pin_bank exynos7580_pin_banks6[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x000, "gpb0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x020, "gpb1", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x040, "gpb2", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x060, "gpc1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x080, "gpc2", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x0a0, "gpc5", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x0c0, "gpd0", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 7, 0x0e0, "gpd1", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x100, "gpd2", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x120, "gpd3", 0x24),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x140, "gpe0", 0x28),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x160, "gpe1", 0x2c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x180, "gpf0", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x1a0, "gpf1", 0x34),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x1c0, "gpf2", 0x38),
};

/* pin banks of exynos7580 pin-controller 7 (TOUCH) */
static struct samsung_pin_bank exynos7580_pin_banks7[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpc4", 0x00),
};

struct samsung_pin_ctrl exynos7580_pin_ctrl[] = {
	{
		/* pin-controller instance 0 Alive data */
		.pin_banks	= exynos7580_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS5430_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS5430_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS5430_WKUP_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume	= exynos5430_pinctrl_resume,
		.label		= "exynos7580-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 AUD data */
		.pin_banks	= exynos7580_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks1),
		.label		= "exynos7580-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 ESE data */
		.pin_banks	= exynos7580_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks2),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume	= exynos5430_pinctrl_resume,
		.label		= "exynos7580-gpio-ctrl2",
	}, {
		/* pin-controller instance 3 FSYS data */
		.pin_banks	= exynos7580_pin_banks3,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks3),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume	= exynos5430_pinctrl_resume,
		.label		= "exynos7580-gpio-ctrl3",
	}, {
		/* pin-controller instance 4 MIF data */
		.pin_banks	= exynos7580_pin_banks4,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks4),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume	= exynos5430_pinctrl_resume,
		.label		= "exynos7580-gpio-ctrl4",
	}, {
		/* pin-controller instance 5 NFC data */
		.pin_banks	= exynos7580_pin_banks5,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks5),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume	= exynos5430_pinctrl_resume,
		.label		= "exynos7580-gpio-ctrl5",
	}, {
		/* pin-controller instance 6 TOP data */
		.pin_banks	= exynos7580_pin_banks6,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks6),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume	= exynos5430_pinctrl_resume,
		.label		= "exynos7580-gpio-ctrl6",
	}, {
		/* pin-controller instance 7 TOUCH data */
		.pin_banks	= exynos7580_pin_banks7,
		.nr_banks	= ARRAY_SIZE(exynos7580_pin_banks7),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume	= exynos5430_pinctrl_resume,
		.label		= "exynos7580-gpio-ctrl7",
	},
};

#if defined(CONFIG_SOC_EXYNOS5430)
u32 exynos_eint_to_pin_num(int eint)
{
	return exynos5430_pin_ctrl[0].base + eint;
}
#elif defined(CONFIG_SOC_EXYNOS5422)
u32 exynos_eint_to_pin_num(int eint)
{
	return exynos5422_pin_ctrl[0].base + eint + 8;
}
#elif defined(CONFIG_SOC_EXYNOS7420)
u32 exynos_eint_to_pin_num(int eint)
{
	return exynos7420_pin_ctrl[0].base + eint;
}
#endif

/* pin banks of exynos5433 pin-controller 0 (ALIVE) */
static struct samsung_pin_bank exynos5433_pin_banks0[] = {
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x000, "gpa0", 0x00),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x020, "gpa1", 0x04),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x040, "gpa2", 0x08),
	EXYNOS_PIN_BANK_EINTW(bank_type_3, 8, 0x060, "gpa3", 0x0c),

	/* GPF1~5 group is special group for extended EINT(32~63),
	 * so it needs to care. xxxx group is dummy for align insted of GPF0.
	 * Because GPF0 is not Alive block */
	EXYNOS_PIN_BANK_EINTW_EXT(bank_type_2, 8, 0x000, "xxxx", 0x00, 0x1000, EINT_FLTCON_PRESET01,  0x0,  0x4, 0),
	EXYNOS_PIN_BANK_EINTW_EXT(bank_type_2, 8, 0x020, "gpf1", 0x04, 0x1000, EINT_FLTCON_PRESET01,  0x8,  0xc, 32),
	EXYNOS_PIN_BANK_EINTW_EXT(bank_type_2, 4, 0x040, "gpf2", 0x08, 0x1000, EINT_FLTCON_PRESET0,  0x10,  0x0, 40),
	EXYNOS_PIN_BANK_EINTW_EXT(bank_type_2, 4, 0x060, "gpf3", 0x0c, 0x1000, EINT_FLTCON_PRESET0,  0x14,  0x0, 44),
	EXYNOS_PIN_BANK_EINTW_EXT(bank_type_2, 8, 0x080, "gpf4", 0x10, 0x1000, EINT_FLTCON_PRESET01, 0x18, 0x1c, 48),
	EXYNOS_PIN_BANK_EINTW_EXT(bank_type_2, 8, 0x0a0, "gpf5", 0x14, 0x1000, EINT_FLTCON_PRESET01, 0x20, 0x24, 56),
};

/* pin banks of exynos5433 pin-controller 1 (AUD) */
static struct samsung_pin_bank exynos5433_pin_banks1[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 7, 0x000, "gpz0", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x020, "gpz1", 0x04),
};

/* pin banks of exynos5433 pin-controller 2 (CPIF) */
static struct samsung_pin_bank exynos5433_pin_banks2[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x000, "gpv6", 0x00),
};

/* pin banks of exynos5433 pin-controller 3 (eSE) */
static struct samsung_pin_bank exynos5433_pin_banks3[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpj2", 0x00),
};

/* pin banks of exynos5433 pin-controller 4 (FINGER) */
static struct samsung_pin_bank exynos5433_pin_banks4[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x000, "gpd5", 0x00),
};

/* pin banks of exynos5433 pin-controller 5 (FSYS) */
static struct samsung_pin_bank exynos5433_pin_banks5[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x000, "gph1", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 7, 0x020, "gpr4", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x040, "gpr0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x060, "gpr1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x080, "gpr2", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x0a0, "gpr3", 0x14),
};

/* pin banks of exynos5433 pin-controller 6 (IMEM) */
static struct samsung_pin_bank exynos5433_pin_banks6[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x000, "gpf0", 0x00),
};

/* pin banks of exynos5433 pin-controller 7 (NFC) */
static struct samsung_pin_bank exynos5433_pin_banks7[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpj0", 0x00),
};

/* pin banks of exynos5433 pin-controller 8 (PERIC) */
static struct samsung_pin_bank exynos5433_pin_banks8[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x000, "gpv7", 0x00),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x020, "gpb0", 0x04),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x040, "gpc0", 0x08),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x060, "gpc1", 0x0c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x080, "gpc2", 0x10),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x0a0, "gpc3", 0x14),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x0c0, "gpg0", 0x18),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 4, 0x0e0, "gpd0", 0x1c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 6, 0x100, "gpd1", 0x20),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x120, "gpd2", 0x24),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x140, "gpd4", 0x28),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x160, "gpd8", 0x2c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 7, 0x180, "gpd6", 0x30),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x1a0, "gpd7", 0x34),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 5, 0x1c0, "gpg1", 0x38),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 2, 0x1e0, "gpg2", 0x3c),
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 8, 0x200, "gpg3", 0x40),
};

/* pin banks of exynos5433 pin-controller 9 (TOUCH) */
static struct samsung_pin_bank exynos5433_pin_banks9[] = {
	EXYNOS_PIN_BANK_EINTG(bank_type_2, 3, 0x000, "gpj1", 0x00),
};

/*
 * Samsung pinctrl driver data for Exynos5433 evt0 SoC. Exynos5433 SoC includes
 * four gpio/pin-mux/pinconfig controllers.
 */
struct samsung_pin_ctrl exynos5433_pin_ctrl[] = {
	{
		/* pin-controller instance 0 data */
		.pin_banks	= exynos5433_pin_banks0,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks0),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.weint_con	= EXYNOS5430_WKUP_ECON_OFFSET,
		.weint_mask	= EXYNOS5430_WKUP_EMASK_OFFSET,
		.weint_pend	= EXYNOS5430_WKUP_EPEND_OFFSET,
		.weint_fltcon	= EXYNOS5430_WKUP_EFLTCON_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl0",
	}, {
		/* pin-controller instance 1 data */
		.pin_banks	= exynos5433_pin_banks1,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks1),
		.label		= "exynos5433-gpio-ctrl1",
	}, {
		/* pin-controller instance 2 data */
		.pin_banks	= exynos5433_pin_banks2,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks2),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl2",
	}, {
		/* pin-controller instance 3 data */
		.pin_banks	= exynos5433_pin_banks3,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks3),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl3",
	}, {
		/* pin-controller instance 4 data */
		.pin_banks	= exynos5433_pin_banks4,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks4),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl4",
	}, {
		/* pin-controller instance 5 data */
		.pin_banks	= exynos5433_pin_banks5,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks5),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl5",
	}, {
		/* pin-controller instance 6 data */
		.pin_banks	= exynos5433_pin_banks6,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks6),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl6",
	}, {
		/* pin-controller instance 7 data */
		.pin_banks	= exynos5433_pin_banks7,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks7),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl7",
	}, {
		/* pin-controller instance 8 data */
		.pin_banks	= exynos5433_pin_banks8,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks8),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl8",
	}, {
		/* pin-controller instance 9 data */
		.pin_banks	= exynos5433_pin_banks9,
		.nr_banks	= ARRAY_SIZE(exynos5433_pin_banks9),
		.geint_con	= EXYNOS_GPIO_ECON_OFFSET,
		.geint_mask	= EXYNOS_GPIO_EMASK_OFFSET,
		.geint_pend	= EXYNOS_GPIO_EPEND_OFFSET,
		.svc		= EXYNOS_SVC_OFFSET,
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos5430_pinctrl_suspend,
		.resume		= exynos5430_pinctrl_resume,
		.label		= "exynos5433-gpio-ctrl9",
	},
};
