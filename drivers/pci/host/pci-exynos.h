/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PCIE_EXYNOS_H
#define __PCIE_EXYNOS_H

#define MAX_TIMEOUT		1000
#define ID_MASK			0xffff
#define TPUT_THRESHOLD  50
#define MAX_RC_NUM		2

#define PCIE_CONF_SPACE_DW	1024
#define EXYNOS7420_PCIE1_DEV_ID	0xA575

#define to_exynos_pcie(x)	container_of(x, struct exynos_pcie, pp)

#if defined(CONFIG_SOC_EXYNOS5433)
struct exynos_pcie_clks {
	struct clk	*clk;
	struct clk	*phy_clk;
};
#elif defined(CONFIG_SOC_EXYNOS7420)
struct exynos_pcie_clks {
	struct clk	*pcie_clks[10];
	struct clk	*phy_clks[2];
};
#endif

#define PCIE_BUS_PRIV_DATA(pdev) \
	(((struct pci_sys_data *)pdev->bus->sysdata)->private_data)

enum exynos_pcie_state {
	STATE_LINK_DOWN = 0,
	STATE_LINK_UP_TRY,
	STATE_LINK_DOWN_TRY,
	STATE_LINK_UP,
};

struct exynos_pcie {
	void __iomem		*elbi_base;
	void __iomem		*phy_base;
	void __iomem		*block_base;
	void __iomem		*pmu_base;
	void __iomem		*rc_dbi_base;
	void __iomem		*gpio_base;
	void __iomem		*cmu_base;
	void __iomem		*phy_pcs_base;
	int			perst_gpio;
	int			eint_irq;
	int			eint_flag;
	int			pcie_tpoweron_max;
	int			cp2ap_wake_gpio;
	int			ch_num;
	int			pcie_clk_num;
	int			phy_clk_num;
	enum exynos_pcie_state	state;
	int			probe_ok;
	int			l1ss_enable;
	int			d0uninit_cnt;
	bool			use_msi;
	bool			lpc_checking;
	struct workqueue_struct *pcie_wq;
	struct exynos_pcie_clks	clks;
	struct pcie_port	pp;
	struct pci_dev		*pci_dev;
	struct pci_saved_state	*pci_saved_configs;
	struct notifier_block	lpa_nb;
	struct mutex		lock;
	struct delayed_work	work;
	struct work_struct      handle_wake_work;
	struct exynos_pcie_register_event *event_reg;
	u32			ep_shadow[PCIE_CONF_SPACE_DW];
	u32			rc_shadow[3];
};

/* PCIe ELBI registers */
#define PCIE_IRQ_PULSE			0x000
#define IRQ_INTA_ASSERT			(0x1 << 0)
#define IRQ_INTB_ASSERT			(0x1 << 2)
#define IRQ_INTC_ASSERT			(0x1 << 4)
#define IRQ_INTD_ASSERT			(0x1 << 6)
#define IRQ_RADM_PM_TO_ACK		(0x1 << 18)
#define PCIE_IRQ_LEVEL			0x004
#define PCIE_IRQ_SPECIAL		0x008
#define PCIE_IRQ_EN_PULSE		0x00c
#define PCIE_IRQ_EN_LEVEL		0x010
#define IRQ_MSI_ENABLE			(0x1 << 1)
#define IRQ_LINKDOWN_ENABLE		(0x1 << 4)
#define PCIE_IRQ_EN_SPECIAL		0x014
#define PCIE_SW_WAKE			0x018
#define PCIE_BUS_NUM			0x01c
#define PCIE_RADM_MSG_LTR		0x020
#define PCIE_APP_LTR_LATENCY		0x024
#define PCIE_APP_INIT_RESET		0x028
#define PCIE_APP_LTSSM_ENABLE		0x02c
#define PCIE_L1_BUG_FIX_ENABLE		0x038
#define PCIE_APP_REQ_EXIT_L1		0x040
#define PCIE_APPS_PM_XMT_TURNOFF	0x04c
#define PCIE_ELBI_RDLH_LINKUP		0x074
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_PM_DSTATE			0x88
#define PCIE_D0_UNINIT_STATE		0x4
#define PCIE_CORE_RESETN_DISABLE	0xF0
#define PCIE_APP_REQ_EXIT_L1_MODE	0xF4
#define PCIE_ELBI_SLV_AWMISC		0x11c
#define PCIE_ELBI_SLV_ARMISC		0x120
#define PCIE_ELBI_SLV_DBI_ENABLE	(0x1 << 21)

/* PCIe Purple registers */
#define PCIE_L1SUB_CM_CON		0x1010
#define PCIE_PHY_GLOBAL_RESET		0x1040
#define PCIE_PHY_COMMON_RESET		0x1020
#define PCIE_PHY_CMN_REG		0x008
#define PCIE_PHY_MAC_RESET		0x208
#define PCIE_PHY_PLL_LOCKED		0x010
#define PCIE_PHY_TRSVREG_RESET		0x020
#define PCIE_PHY_TRSV_RESET		0x024

/* PCIe PHY registers */
#define PCIE_PHY_IMPEDANCE		0x004
#define PCIE_PHY_PLL_DIV_0		0x008
#define PCIE_PHY_PLL_BIAS		0x00c
#define PCIE_PHY_DCC_FEEDBACK		0x014
#define PCIE_PHY_PLL_DIV_1		0x05c
#define PCIE_PHY_TRSV0_EMP_LVL		0x084
#define PCIE_PHY_TRSV0_DRV_LVL		0x088
#define PCIE_PHY_TRSV0_RXCDR		0x0ac
#define PCIE_PHY_TRSV0_LVCC		0x0dc
#define PCIE_PHY_TRSV1_EMP_LVL		0x144
#define PCIE_PHY_TRSV1_RXCDR		0x16c
#define PCIE_PHY_TRSV1_LVCC		0x19c
#define PCIE_PHY_TRSV2_EMP_LVL		0x204
#define PCIE_PHY_TRSV2_RXCDR		0x22c
#define PCIE_PHY_TRSV2_LVCC		0x25c
#define PCIE_PHY_TRSV3_EMP_LVL		0x2c4
#define PCIE_PHY_TRSV3_RXCDR		0x2ec
#define PCIE_PHY_TRSV3_LVCC		0x31c

/* PCIe PMU registers */
#if defined(CONFIG_SOC_EXYNOS5433)
#define PCIE_PHY_CONTROL		0x730
#elif defined(CONFIG_SOC_EXYNOS7420)
#define PCIE_PHY_CONTROL		0x0
#endif

#endif
