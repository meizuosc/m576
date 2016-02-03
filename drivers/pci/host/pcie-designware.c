/*
 * Synopsys Designware PCIe host controller driver
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

#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/types.h>
#include <asm/hardirq.h>
#include <plat/cpu.h>

#include "pcie-designware.h"
#include "pci-exynos.h"

/* Synopsis specific PCIE configuration registers */
#define PCIE_CAP_ID_NXT_PTR_REG		0x40
#define PCIE_CON_STATUS			0x44
#define PCIE_LINK_CAP			0x7C
#define PCIE_LINK_CTRSTS		0x80
#define PORT_LINK_PM_L1L2_ENABLE	(0x3 << 0)
#define PCIE_DEVICE_CTR2STS2		0x98
#define PCIE_CAP_LTR_ENABLE		(0x1 << 10)
#define PCIE_LINK_CTRSTS2		0xA0
#define PCIE_LINK_L1SS_CONTROL		0x158
#define PORT_LINK_TCOMMON_32US		(0x20 << 8)
#define PCIE_LINK_L1SS_CONTROL2		0x15C
#define PORT_LINK_L1SS_ENABLE		(0xf << 0)
#define PORT_LINK_TPOWERON_130US	(0x69 << 0)
//#define PORT_LINK_TPOWERON_3100US	(0xfa << 0)
#define PORT_LINK_TPOWERON_2500US   (0xca << 0)
#define PCIE_ACK_F_ASPM_CONTROL		0x70C
#define PCIE_MISC_CONTROL		0x8BC

#define PCIE_PORT_LINK_CONTROL		0x710
#define PORT_LINK_MODE_MASK		(0x3f << 16)
#define PORT_LINK_MODE_1_LANES		(0x1 << 16)
#define PORT_LINK_MODE_2_LANES		(0x3 << 16)
#define PORT_LINK_MODE_4_LANES		(0x7 << 16)

#define PCIE_LINK_WIDTH_SPEED_CONTROL	0x80C
#define PORT_LOGIC_SPEED_CHANGE		(0x1 << 17)
#define PORT_LOGIC_LINK_WIDTH_MASK	(0x1ff << 8)
#define PORT_LOGIC_LINK_WIDTH_1_LANES	(0x1 << 8)
#define PORT_LOGIC_LINK_WIDTH_2_LANES	(0x2 << 8)
#define PORT_LOGIC_LINK_WIDTH_4_LANES	(0x4 << 8)

#define PCIE_MSI_ADDR_LO		0x820
#define PCIE_MSI_ADDR_HI		0x824
#define PCIE_MSI_INTR0_ENABLE		0x828
#define PCIE_MSI_INTR0_MASK		0x82C
#define PCIE_MSI_INTR0_STATUS		0x830

#define PCIE_ATU_VIEWPORT		0x900
#define PCIE_ATU_REGION_INBOUND		(0x1 << 31)
#define PCIE_ATU_REGION_OUTBOUND	(0x0 << 31)
#define PCIE_ATU_REGION_INDEX2		(0x2 << 0)
#define PCIE_ATU_REGION_INDEX1		(0x1 << 0)
#define PCIE_ATU_REGION_INDEX0		(0x0 << 0)
#define PCIE_ATU_CR1			0x904
#define PCIE_ATU_TYPE_MEM		(0x0 << 0)
#define PCIE_ATU_TYPE_IO		(0x2 << 0)
#define PCIE_ATU_TYPE_CFG0		(0x4 << 0)
#define PCIE_ATU_TYPE_CFG1		(0x5 << 0)
#define PCIE_ATU_CR2			0x908
#define PCIE_ATU_ENABLE			(0x1 << 31)
#define PCIE_ATU_BAR_MODE_ENABLE	(0x1 << 30)
#define PCIE_ATU_LOWER_BASE		0x90C
#define PCIE_ATU_UPPER_BASE		0x910
#define PCIE_ATU_LIMIT			0x914
#define PCIE_ATU_LOWER_TARGET		0x918
#define PCIE_ATU_BUS(x)			(((x) & 0xff) << 24)
#define PCIE_ATU_DEV(x)			(((x) & 0x1f) << 19)
#define PCIE_ATU_FUNC(x)		(((x) & 0x7) << 16)
#define PCIE_ATU_UPPER_TARGET		0x91C

#define PCIE_AUX_CLK_FREQ_OFF		0xB40

#ifdef CONFIG_PCI_MSI
#define MAX_MSI_IRQS			32
#define MAX_MSI_CTRLS			8

static unsigned int msi_data;
static DECLARE_BITMAP(msi_irq_in_use, MAX_MSI_IRQS);
#endif

static struct hw_pci dw_pci[MAX_RC_NUM] = {
	{
		.domain		= 0,
		.setup		= dw_pcie_setup,
		.scan		= dw_pcie_scan_bus,
		.map_irq	= dw_pcie_map_irq,
	},
	{
		.domain		= 1,
		.setup		= dw_pcie_setup,
		.scan		= dw_pcie_scan_bus,
		.map_irq	= dw_pcie_map_irq,
	},
};

unsigned long global_io_offset;

static inline struct pcie_port *sys_to_pcie(struct pci_sys_data *sys)
{
	return sys->private_data;
}

int cfg_read(void *addr, int where, int size, u32 *val)
{
	*val = readl(addr);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;
	else if (size != 4)
		goto fail;

	return PCIBIOS_SUCCESSFUL;
fail:
	return PCIBIOS_BAD_REGISTER_NUMBER;
}

int cfg_write(void *addr, int where, int size, u32 val)
{
	if (size == 4)
		writel(val, addr);
	else if (size == 2)
		writew(val, addr + (where & 2));
	else if (size == 1)
		writeb(val, addr + (where & 3));
	else
		goto fail;

	return PCIBIOS_SUCCESSFUL;
fail:
	return PCIBIOS_BAD_REGISTER_NUMBER;

}

int dw_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
				u32 *val)
{
	int ret;

	if (pp->ops->rd_own_conf)
		ret = pp->ops->rd_own_conf(pp, where, size, val);
	else
		ret = cfg_read(pp->dbi_base + (where & ~0x3), where, size, val);

	return ret;
}

int dw_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
				u32 val)
{
	int ret;

	if (pp->ops->wr_own_conf)
		ret = pp->ops->wr_own_conf(pp, where, size, val);
	else
		ret = cfg_write(pp->dbi_base + (where & ~0x3), where, size,
				val);

	return ret;
}

#ifdef CONFIG_PCI_MSI
static struct irq_chip dw_msi_chip = {
	.name = "PCI-MSI",
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

/* MSI int handler */
void dw_handle_msi_irq(struct pcie_port *pp)
{
	unsigned long val;
	unsigned long flags;
	int i, pos;

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		spin_lock_irqsave(&pp->conf_lock, flags);
		dw_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_STATUS + i * 12, 4,
				(u32 *)&val);
		spin_unlock_irqrestore(&pp->conf_lock, flags);

		if (val) {
			pos = 0;
			while ((pos = find_next_bit(&val, 32, pos)) != 32) {
				generic_handle_irq(pp->msi_irq_start
					+ (i * 32) + pos);
				pos++;
			}
		}

		spin_lock_irqsave(&pp->conf_lock, flags);
		dw_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_STATUS + i * 12, 4, val);
		spin_unlock_irqrestore(&pp->conf_lock, flags);
	}
}

void dw_pcie_msi_init(struct pcie_port *pp)
{
	unsigned long flags;

	/* program the msi_data */
	spin_lock_irqsave(&pp->conf_lock, flags);
	dw_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
			__virt_to_phys((unsigned int *)(&msi_data)));
	dw_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4, 0);
	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

static int find_valid_pos0(int msgvec, int pos, int *pos0)
{
	int flag = 1;

	do {
		pos = find_next_zero_bit(msi_irq_in_use,
				MAX_MSI_IRQS, pos);
		/*if you have reached to the end then get out from here.*/
		if (pos == MAX_MSI_IRQS)
			return -ENOSPC;
		/*
		 * Check if this position is at correct offset.nvec is always a
		 * power of two. pos0 must be nvec bit alligned.
		 */
		if (pos % msgvec)
			pos += msgvec - (pos % msgvec);
		else
			flag = 0;
	} while (flag);

	*pos0 = pos;
	return 0;
}

/* Dynamic irq allocate and deallocation */
static int assign_irq(int no_irqs, struct msi_desc *desc, int *pos)
{
	int res, bit, irq, pos0, pos1, i;
	u32 val;
	unsigned long flags;
	struct pcie_port *pp = sys_to_pcie(desc->dev->bus->sysdata);

	if (!pp) {
		BUG();
		return -EINVAL;
	}

	pos0 = find_first_zero_bit(msi_irq_in_use,
			MAX_MSI_IRQS);
	if (pos0 % no_irqs) {
		if (find_valid_pos0(no_irqs, pos0, &pos0))
			goto no_valid_irq;
	}
	if (no_irqs > 1) {
		pos1 = find_next_bit(msi_irq_in_use,
				MAX_MSI_IRQS, pos0);
		/* there must be nvec number of consecutive free bits */
		while ((pos1 - pos0) < no_irqs) {
			if (find_valid_pos0(no_irqs, pos1, &pos0))
				goto no_valid_irq;
			pos1 = find_next_bit(msi_irq_in_use,
					MAX_MSI_IRQS, pos0);
		}
	}

	irq = (pp->msi_irq_start + pos0);

	i = 0;
	while (i < no_irqs) {
		set_bit(pos0 + i, msi_irq_in_use);
		irq_alloc_descs((irq + i), (irq + i), 1, 0);
		irq_set_msi_desc(irq + i, desc);
		irq_set_chip_and_handler(irq + i, &dw_msi_chip,
					handle_simple_irq);
		set_irq_flags(irq + i, IRQF_VALID);
		/*Enable corresponding interrupt in MSI interrupt controller */
		res = ((pos0 + i) / 32) * 12;
		bit = (pos0 + i) % 32;

		spin_lock_irqsave(&pp->conf_lock, flags);
		dw_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, &val);
		val |= 1 << bit;
		dw_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, val);
		spin_unlock_irqrestore(&pp->conf_lock, flags);

		i++;
	}

	*pos = pos0;
	return irq;

no_valid_irq:
	*pos = pos0;
	return -ENOSPC;
}

static void clear_irq(unsigned int irq)
{
	int res, bit, val, pos;
	unsigned long flags;
	struct irq_desc *desc;
	struct msi_desc *msi;
	struct pcie_port *pp;

	/* get the port structure */
	desc = irq_to_desc(irq);
	msi = irq_desc_get_msi_desc(desc);
	pp = sys_to_pcie(msi->dev->bus->sysdata);
	if (!pp) {
		BUG();
		return;
	}

	pos = irq - pp->msi_irq_start;

	irq_free_desc(irq);

	clear_bit(pos, msi_irq_in_use);

	/* Disable corresponding interrupt on MSI interrupt controller */
	res = (pos / 32) * 12;
	bit = pos % 32;

	spin_lock_irqsave(&pp->conf_lock, flags);
	dw_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, &val);
	val &= ~(1 << bit);
	dw_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, val);
	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	int irq, pos, msgvec;
	u16 msg_ctr;
	struct msi_msg msg;
	struct pcie_port *pp = sys_to_pcie(pdev->bus->sysdata);

	if (!pp) {
		BUG();
		return -EINVAL;
	}

	pci_read_config_word(pdev, desc->msi_attrib.pos+PCI_MSI_FLAGS,
				&msg_ctr);
	msgvec = (msg_ctr&PCI_MSI_FLAGS_QSIZE) >> 4;
	if (msgvec == 0)
		msgvec = (msg_ctr & PCI_MSI_FLAGS_QMASK) >> 1;
	if (msgvec > 5)
		msgvec = 0;

	irq = assign_irq((1 << msgvec), desc, &pos);
	if (irq < 0)
		return irq;

	msg_ctr &= ~PCI_MSI_FLAGS_QSIZE;
	msg_ctr |= msgvec << 4;
	pci_write_config_word(pdev, desc->msi_attrib.pos + PCI_MSI_FLAGS,
				msg_ctr);
	desc->msi_attrib.multiple = msgvec;

	msg.address_hi = 0x0;
	msg.address_lo = __virt_to_phys((unsigned int *)(&msi_data));
	msg.data = pos;
	write_msi_msg(irq, &msg);

	return 0;
}

void arch_teardown_msi_irq(unsigned int irq)
{
	clear_irq(irq);
}
#endif

int dw_pcie_link_up(struct pcie_port *pp)
{
	if (pp == NULL)
		return 0;
	if (pp->ops == NULL)
		return 0;
	if (pp->ops->link_up)
		return pp->ops->link_up(pp);
	else
		return 0;
}

void dw_pcie_set_tpoweron(struct pcie_port *pp, int max)
{
	void __iomem *ep_dbi_base = pp->va_cfg0_base;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	unsigned long flags;
	u32 val;

	if (exynos_pcie->state != STATE_LINK_UP)
		return;

	spin_lock_irqsave(&pp->conf_lock, flags);

	/* Disable ASPM */
	val = readl(ep_dbi_base + 0xBC);
	val &= ~0x3;
	writel(val, ep_dbi_base + 0xBC);

	val = readl(ep_dbi_base + 0x248);
	val &= ~0xF;
	writel(val, ep_dbi_base + 0x248);

	if (max) {
		writel(PORT_LINK_TPOWERON_2500US, ep_dbi_base + 0x24C);
		dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4, PORT_LINK_TPOWERON_2500US);
	} else {
		writel(PORT_LINK_TPOWERON_130US, ep_dbi_base + 0x24C);
		dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4, PORT_LINK_TPOWERON_130US);
	}

	/* Enable L1ss */
	val = readl(ep_dbi_base + 0xBC);
	val |= 0x2;
	writel(val, ep_dbi_base + 0xBC);

	val = readl(ep_dbi_base + 0x248);
	val |= PORT_LINK_L1SS_ENABLE;
	writel(val, ep_dbi_base + 0x248);

	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

void dw_pcie_config_l1ss(struct pcie_port *pp)
{
	u32 val;
	void __iomem *ep_dbi_base = pp->va_cfg0_base;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (soc_is_exynos7420() && (exynos_pcie->ch_num == 0)) {
		/* set RC config */
		/* PCIE20_CAP_LINKCTRLSTATUS */
		dw_pcie_rd_own_conf(pp, PCIE_LINK_CTRSTS, 4, &val);
		val &= ~0x3 << 0;
		val |= 0x2 << 0;
		dw_pcie_wr_own_conf(pp, PCIE_LINK_CTRSTS, 4, val);

		/* PCIE20_L1SUB_CONTROL1 */
		dw_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
		val |= PORT_LINK_L1SS_ENABLE - 1;
		dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);

		/* PCIE20_DEVICE_CONTROL2_STATUS2 */
		dw_pcie_rd_own_conf(pp, PCIE_DEVICE_CTR2STS2, 4, &val);
		val |= PCIE_CAP_LTR_ENABLE;
		dw_pcie_wr_own_conf(pp, PCIE_DEVICE_CTR2STS2, 4, val);

		/* set EP config */
		/* PCIE20_CAP_LINKCTRLSTATUS */
		val = readl(ep_dbi_base + PCIE_LINK_CTRSTS);
		val &= ~0x3;
		val |= 0x2 << 0;
		writel(val, ep_dbi_base + PCIE_LINK_CTRSTS);

		/* PCIE20_L1SUB_CONTROL1 */
		val = readl(ep_dbi_base + PCIE_LINK_L1SS_CONTROL);
		val |= PORT_LINK_L1SS_ENABLE - 1;
		writel(val, ep_dbi_base + PCIE_LINK_L1SS_CONTROL);

		/* PCIE20_DEVICE_CONTROL2_STATUS2 */
		val = readl(ep_dbi_base + PCIE_DEVICE_CTR2STS2);
		val |= PCIE_CAP_LTR_ENABLE;
		writel(val, ep_dbi_base + PCIE_DEVICE_CTR2STS2);
	} else {
		val = readl(ep_dbi_base + 0xbc);
		val &= ~0x3;
		val |= 0x142;
		writel(val, ep_dbi_base + 0xBC);
		val = readl(ep_dbi_base + 0x248);
		writel(val | 0xa0f, ep_dbi_base + 0x248);
		writel(PORT_LINK_TPOWERON_130US, ep_dbi_base + 0x24C);
		writel(0x10031003, ep_dbi_base + 0x1B4);
		val = readl(ep_dbi_base + 0xD4);
		writel(val | (1 << 10), ep_dbi_base + 0xD4);
		dw_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
		val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
		dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
		dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4, PORT_LINK_TPOWERON_130US);
		dw_pcie_wr_own_conf(pp, 0xB44, 4, 0xD2);

		dw_pcie_rd_own_conf(pp, PCIE_LINK_CTRSTS, 4, &val);
		val &= ~0x3 << 0;
		val |= 0x42 << 0;
		dw_pcie_wr_own_conf(pp, PCIE_LINK_CTRSTS, 4, val);
		dw_pcie_wr_own_conf(pp, PCIE_DEVICE_CTR2STS2, 4, PCIE_CAP_LTR_ENABLE);
	}
}

int dw_pcie_host_init(struct pcie_port *pp)
{
	struct device_node *np = pp->dev->of_node;
	struct of_pci_range range;
	struct of_pci_range_parser parser;

	if (of_pci_range_parser_init(&parser, np)) {
		dev_err(pp->dev, "missing ranges property\n");
		return -EINVAL;
	}

	/* Get the I/O and memory ranges from DT */
	for_each_of_pci_range(&parser, &range) {
		unsigned long restype = range.flags & IORESOURCE_TYPE_BITS;
		if (restype == IORESOURCE_IO) {
			of_pci_range_to_resource(&range, np, &pp->io);
			pp->io.name = "I/O";
			pp->io.start = max_t(resource_size_t,
					     PCIBIOS_MIN_IO,
					     range.pci_addr + global_io_offset);
			pp->io.end = min_t(resource_size_t,
					   IO_SPACE_LIMIT,
					   range.pci_addr + range.size
					   + global_io_offset);
			pp->config.io_size = resource_size(&pp->io);
			pp->config.io_bus_addr = range.pci_addr;
		}
		if (restype == IORESOURCE_MEM) {
			of_pci_range_to_resource(&range, np, &pp->mem);
			pp->mem.name = "MEM";
			pp->config.mem_size = resource_size(&pp->mem);
			pp->config.mem_bus_addr = range.pci_addr;
		}
		if (restype == 0) {
			of_pci_range_to_resource(&range, np, &pp->cfg);
			pp->config.cfg0_size = resource_size(&pp->cfg)/2;
			pp->config.cfg1_size = resource_size(&pp->cfg)/2;
		}
	}

	if (!pp->dbi_base) {
		pp->dbi_base = devm_ioremap(pp->dev, pp->cfg.start,
					resource_size(&pp->cfg));
		if (!pp->dbi_base) {
			dev_err(pp->dev, "error with ioremap\n");
			return -ENOMEM;
		}
	}

	pp->cfg0_base = pp->cfg.start;
	pp->cfg1_base = pp->cfg.start + pp->config.cfg0_size;
	pp->io_base = pp->io.start;
	pp->mem_base = pp->mem.start;

	pp->va_cfg0_base = devm_ioremap(pp->dev, pp->cfg0_base,
					pp->config.cfg0_size);
	if (!pp->va_cfg0_base) {
		dev_err(pp->dev, "error with ioremap in function\n");
		return -ENOMEM;
	}
	pp->va_cfg1_base = devm_ioremap(pp->dev, pp->cfg1_base,
					pp->config.cfg1_size);
	if (!pp->va_cfg1_base) {
		dev_err(pp->dev, "error with ioremap\n");
		return -ENOMEM;
	}

	if (of_property_read_u32(np, "num-lanes", &pp->lanes)) {
		dev_err(pp->dev, "Failed to parse the number of lanes\n");
		return -EINVAL;
	}

#ifdef CONFIG_PCI_MSI
	if (of_property_read_u32(np, "msi-base", &pp->msi_irq_start)) {
		dev_err(pp->dev, "Failed to parse msi-base\n");
		return -EINVAL;
	}
#endif

	return 0;
}

void dw_pcie_scan(struct pcie_port *pp)
{
	u32 val;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	dw_pcie_wr_own_conf(pp, PCI_BASE_ADDRESS_0, 4, 0);

	/* program correct class for RC */
	dw_pcie_wr_own_conf(pp, PCI_CLASS_DEVICE, 2, PCI_CLASS_BRIDGE_PCI);

	dw_pcie_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val |= PORT_LOGIC_SPEED_CHANGE;
	dw_pcie_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);

	dw_pci[exynos_pcie->ch_num].nr_controllers = 1;
	dw_pci[exynos_pcie->ch_num].private_data = (void **)&pp;

	pci_common_init(&dw_pci[exynos_pcie->ch_num]);
	pci_assign_unassigned_resources();
	dw_pcie_config_l1ss(pp);

	return;
}

void dw_pcie_prog_viewport_cfg0(struct pcie_port *pp, u32 busdev)
{
	u32 val;

	/* Program viewport 0 : OUTBOUND : CFG0 */
	dw_pcie_rd_own_conf(pp, PCIE_ATU_VIEWPORT, 4, &val);
	val = PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX0;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4, val);
	dw_pcie_wr_own_conf(pp,  PCIE_ATU_LOWER_BASE, 4, pp->cfg0_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->cfg0_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4,  pp->cfg0_base + pp->config.cfg0_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, busdev);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, 0);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_CFG0);
	val = PCIE_ATU_ENABLE;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, val);
}

static void dw_pcie_prog_viewport_cfg1(struct pcie_port *pp, u32 busdev)
{
	u32 val;

	/* Program viewport 1 : OUTBOUND : CFG1 */
	val = PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX2;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4, val);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_CFG1);
	val = PCIE_ATU_ENABLE;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, val);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->cfg1_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->cfg1_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, pp->cfg1_base + pp->config.cfg1_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4,  busdev);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, 0);
}

void dw_pcie_prog_viewport_mem_outbound(struct pcie_port *pp)
{
	u32 val;

	/* Program viewport 0 : OUTBOUND : MEM */
	val = PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX1;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4, val);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_MEM);
	val = PCIE_ATU_ENABLE;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, val);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->mem_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->mem_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, pp->mem_base + pp->config.mem_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, pp->config.mem_bus_addr);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, upper_32_bits(pp->config.mem_bus_addr));
}

static void dw_pcie_prog_viewport_io_outbound(struct pcie_port *pp)
{
	u32 val;

	/* Program viewport 1 : OUTBOUND : IO */
	val = PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX2;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4, val);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_IO);
	val = PCIE_ATU_ENABLE;
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, val);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->io_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->io_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, pp->io_base + pp->config.io_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, pp->config.io_bus_addr);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, upper_32_bits(pp->config.io_bus_addr));
}

static int dw_pcie_rd_other_conf(struct pcie_port *pp, struct pci_bus *bus,
		u32 devfn, int where, int size, u32 *val)
{
	int ret = PCIBIOS_SUCCESSFUL;
	u32 address, busdev;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));
	address = where & ~0x3;

	if (bus->parent->number == pp->root_bus_nr) {
		dw_pcie_prog_viewport_cfg0(pp, busdev);
		ret = cfg_read(pp->va_cfg0_base + address, where, size, val);
		dw_pcie_prog_viewport_mem_outbound(pp);
	} else {
		dw_pcie_prog_viewport_cfg1(pp, busdev);
		ret = cfg_read(pp->va_cfg1_base + address, where, size, val);
		dw_pcie_prog_viewport_io_outbound(pp);
	}

	return ret;
}

static int dw_pcie_wr_other_conf(struct pcie_port *pp, struct pci_bus *bus,
		u32 devfn, int where, int size, u32 val)
{
	int ret = PCIBIOS_SUCCESSFUL;
	u32 address, busdev;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));
	address = where & ~0x3;

	if (bus->parent->number == pp->root_bus_nr) {
		dw_pcie_prog_viewport_cfg0(pp, busdev);
		ret = cfg_write(pp->va_cfg0_base + address, where, size, val);
		dw_pcie_prog_viewport_mem_outbound(pp);
	} else {
		dw_pcie_prog_viewport_cfg1(pp, busdev);
		ret = cfg_write(pp->va_cfg1_base + address, where, size, val);
		dw_pcie_prog_viewport_io_outbound(pp);
	}

	return ret;
}

static int dw_pcie_valid_config(struct pcie_port *pp,
				struct pci_bus *bus, int dev)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (exynos_pcie->state != STATE_LINK_UP)
		return 0;

	/* If there is no link, then there is no device */
	if (bus->number != pp->root_bus_nr) {
		if (!dw_pcie_link_up(pp))
			return 0;
	}

	/* access only one slot on each root port */
	if (bus->number == pp->root_bus_nr && dev > 0)
		return 0;

	/*
	 * do not read more than one device on the bus directly attached
	 * to RC's (Virtual Bridge's) DS side.
	 */
	if (bus->primary == pp->root_bus_nr && dev > 0)
		return 0;

	return 1;
}

static int dw_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			int size, u32 *val)
{
	struct pcie_port *pp = sys_to_pcie(bus->sysdata);
	unsigned long flags;
	int ret;

	if (!pp) {
		BUG();
		return -EINVAL;
	}

	spin_lock_irqsave(&pp->conf_lock, flags);
	if (dw_pcie_valid_config(pp, bus, PCI_SLOT(devfn)) == 0) {
		spin_unlock_irqrestore(&pp->conf_lock, flags);
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (bus->number != pp->root_bus_nr)
		ret = dw_pcie_rd_other_conf(pp, bus, devfn,
						where, size, val);
	else
		ret = dw_pcie_rd_own_conf(pp, where, size, val);
	spin_unlock_irqrestore(&pp->conf_lock, flags);

	return ret;
}

static int dw_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			int where, int size, u32 val)
{
	struct pcie_port *pp = sys_to_pcie(bus->sysdata);
	unsigned long flags;
	int ret;

	if (!pp) {
		BUG();
		return -EINVAL;
	}

	spin_lock_irqsave(&pp->conf_lock, flags);
	if (dw_pcie_valid_config(pp, bus, PCI_SLOT(devfn)) == 0) {
		spin_unlock_irqrestore(&pp->conf_lock, flags);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (bus->number != pp->root_bus_nr)
		ret = dw_pcie_wr_other_conf(pp, bus, devfn,
						where, size, val);
	else
		ret = dw_pcie_wr_own_conf(pp, where, size, val);
	spin_unlock_irqrestore(&pp->conf_lock, flags);

	return ret;
}

static struct pci_ops dw_pcie_ops = {
	.read = dw_pcie_rd_conf,
	.write = dw_pcie_wr_conf,
};

int dw_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct pcie_port *pp;

	pp = sys_to_pcie(sys);

	if (!pp)
		return 0;

	if (global_io_offset < SZ_1M && pp->config.io_size > 0) {
		sys->io_offset = global_io_offset - pp->config.io_bus_addr;
		pci_ioremap_io(sys->io_offset, pp->io.start);
		global_io_offset += SZ_64K;
		pci_add_resource_offset(&sys->resources, &pp->io,
					sys->io_offset);
	}

	sys->mem_offset = pp->mem.start - pp->config.mem_bus_addr;
	pci_add_resource_offset(&sys->resources, &pp->mem, sys->mem_offset);

	return 1;
}

struct pci_bus *dw_pcie_scan_bus(int nr, struct pci_sys_data *sys)
{
	struct pci_bus *bus;
	struct pcie_port *pp = sys_to_pcie(sys);

	if (pp) {
		pp->root_bus_nr = sys->busnr;
		bus = pci_scan_root_bus(NULL, sys->busnr, &dw_pcie_ops,
					sys, &sys->resources);
	} else {
		bus = NULL;
		BUG();
	}

	return bus;
}

int dw_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct pcie_port *pp = sys_to_pcie(dev->bus->sysdata);

	return pp->irq;
}

void dw_pcie_setup_rc(struct pcie_port *pp)
{
	struct pcie_port_info *config = &pp->config;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;
	u32 membase;
	u32 memlimit;

	/* DBI_RO_WR_EN */
	dw_pcie_wr_own_conf(pp, PCIE_MISC_CONTROL, 4,  1);

	/* Change Device ID for PCIe1 */
	if (soc_is_exynos7420() && (exynos_pcie->ch_num == 1))
		dw_pcie_wr_own_conf(pp, PCI_DEVICE_ID, 2, EXYNOS7420_PCIE1_DEV_ID);

	/* set the number of lines as 4 */
	dw_pcie_rd_own_conf(pp, PCIE_PORT_LINK_CONTROL, 4, &val);
	val &= ~PORT_LINK_MODE_MASK;
	switch (pp->lanes) {
	case 1:
		val |= PORT_LINK_MODE_1_LANES;
		break;
	case 2:
		val |= PORT_LINK_MODE_2_LANES;
		break;
	case 4:
		val |= PORT_LINK_MODE_4_LANES;
		break;
	}
	dw_pcie_wr_own_conf(pp, PCIE_PORT_LINK_CONTROL, 4, val);

	/* set link width speed control register */
	dw_pcie_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val &= ~PORT_LOGIC_LINK_WIDTH_MASK;
	switch (pp->lanes) {
	case 1:
		val |= PORT_LOGIC_LINK_WIDTH_1_LANES;
		break;
	case 2:
		val |= PORT_LOGIC_LINK_WIDTH_2_LANES;
		break;
	case 4:
		val |= PORT_LOGIC_LINK_WIDTH_4_LANES;
		break;
	}
	dw_pcie_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);

	/* set max link width & speed : Gen1, Lane1 */
	dw_pcie_rd_own_conf(pp, PCIE_LINK_CAP, 4, &val);
	val &= 0xfffffc00;
	dw_pcie_wr_own_conf(pp, PCIE_LINK_CAP, 4, val | 0x7 << 15 | 0x1 << 4 | 0x2 << 0);

	dw_pcie_wr_own_conf(pp, PCIE_AUX_CLK_FREQ_OFF, 4, 0x18);
	dw_pcie_wr_own_conf(pp, PCIE_CON_STATUS, 4, 0x0);

	/* setup RC BARs */
	dw_pcie_wr_own_conf(pp, PCI_BASE_ADDRESS_0, 4, 0x00000004);
	dw_pcie_wr_own_conf(pp, PCI_BASE_ADDRESS_1, 4, 0x00000004);

	/* setup bus numbers */
	dw_pcie_rd_own_conf(pp, PCI_PRIMARY_BUS, 4, &val);
	val &= 0xff000000;
	val |= 0x00010100;
	dw_pcie_wr_own_conf(pp, PCI_PRIMARY_BUS, 4, val);

	/* setup memory base, memory limit */
	membase = ((u32)pp->mem_base & 0xfff00000) >> 16;
	memlimit = (config->mem_size + (u32)pp->mem_base) & 0xfff00000;
	val = memlimit | membase;
	dw_pcie_wr_own_conf(pp, PCI_MEMORY_BASE, 4, val);

	/* setup command register */
	dw_pcie_rd_own_conf(pp, PCI_COMMAND, 4, &val);
	val &= 0xffff0000;
	val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER | PCI_COMMAND_SERR;
	dw_pcie_wr_own_conf(pp, PCI_COMMAND, 4, val);

	dw_pcie_rd_own_conf(pp, PCIE_LINK_CTRSTS, 4, &val);
	val |= (0x1 << 5);
	dw_pcie_wr_own_conf(pp, PCIE_LINK_CTRSTS, 4, val);

	/* set target speed to GEN1 only */
	dw_pcie_rd_own_conf(pp, PCIE_LINK_CTRSTS2, 4, &val);
	val &= ~0xf;
	val |= 0x1;
	dw_pcie_wr_own_conf(pp, PCIE_LINK_CTRSTS2, 4, val);
}

MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
MODULE_DESCRIPTION("Designware PCIe host controller driver");
MODULE_LICENSE("GPL v2");
