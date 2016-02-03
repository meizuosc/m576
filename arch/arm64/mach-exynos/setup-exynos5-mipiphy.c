/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <mach/regs-clock.h>
#include <mach/exynos5-mipiphy.h>

#define MIPI_PHY_BIT0					(1 << 0)
#define MIPI_PHY_BIT1					(1 << 1)
#define MIPI_PHY_SRESETN				(1 << 1)
#define MIPI_PHY_MRESETN				(1 << 2)

#if defined(CONFIG_SOC_EXYNOS5433)
static __inline__ u32 exynos5_phy0_is_running(u32 reset)
{
	u32 ret = 0;

	/* When you try to disable DSI, CHECK CAM0 PD STATUS */
	if (reset == MIPI_PHY_MRESETN) {
		if (readl(EXYNOS_PMU_CAM0_STATUS) & 0x1)
			ret = __raw_readl(S5P_VA_SYSREG_CAM0 + 0x1014) & MIPI_PHY_BIT0;
	/* When you try to disable CSI, CHECK DISP PD STATUS */
	} else if (reset == MIPI_PHY_SRESETN) {
		if (readl(EXYNOS_PMU_DISP_STATUS) & 0x1)
			ret = __raw_readl(S5P_VA_SYSREG_DISP + 0x000C) & MIPI_PHY_BIT0;
	}

	return ret;
}

static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	static DEFINE_SPINLOCK(lock);
	void __iomem *addr_phy;
	void __iomem *addr_reset;
	unsigned long flags;
	u32 cfg;

	addr_phy = EXYNOS_PMU_MIPI_PHY_CONTROL(id);

	spin_lock_irqsave(&lock, flags);

	/* PHY reset */
	switch(id) {
	case 0:
		if (reset == MIPI_PHY_SRESETN) {
			if (readl(EXYNOS_PMU_CAM0_STATUS) & 0x1) {
				addr_reset = S5P_VA_SYSREG_CAM0 + 0x1014;
				cfg = __raw_readl(addr_reset);
				cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		} else {
			if (readl(EXYNOS_PMU_DISP_STATUS) & 0x1) {
				addr_reset = S5P_VA_SYSREG_DISP + 0x000c;
				cfg = __raw_readl(addr_reset);
				cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		}
		break;
	case 1:
		if (readl(EXYNOS_PMU_CAM0_STATUS) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM0 + 0x1014;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT1) : (cfg & ~MIPI_PHY_BIT1);
			__raw_writel(cfg, addr_reset);
		}
		break;
	case 2:
		if (readl(EXYNOS_PMU_CAM1_STATUS) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM1 + 0x1020;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
			__raw_writel(cfg, addr_reset);
		}
		break;
	default:
		pr_err("id(%d) is invalid", id);
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	/* PHY PMU enable */
	cfg = __raw_readl(addr_phy);

	if (on)
		cfg |= MIPI_PHY_ENABLE;
	else {
		if (id == 0) {
			if (!exynos5_phy0_is_running(reset))
				cfg &= ~MIPI_PHY_ENABLE;
		} else {
			cfg &= ~MIPI_PHY_ENABLE;
		}
	}

	__raw_writel(cfg, addr_phy);
	spin_unlock_irqrestore(&lock, flags);

	return 0;
}
#else

static int dphy_m4s4_status = 0;

static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	int ret = 0;
	static DEFINE_SPINLOCK(lock);
	void __iomem *pmu_addr;
	void __iomem *cmu_addr;
	unsigned long flags;
	u32 cfg;

	spin_lock_irqsave(&lock, flags);

	if (reset == MIPI_PHY_SRESETN) {
		if (on) {
			switch(id) {
			case 0:
				++dphy_m4s4_status;
				if ((dphy_m4s4_status == 1) ||
					(!(__raw_readl(S7P_MIPI_DPHY_CONTROL(0)) & 0x1))) {
					if (dphy_m4s4_status != 1) {
						pr_info("%s: reset %d: dphy_m4s4_ref_cnt %d is wrong",
							 __func__, reset, dphy_m4s4_status);
						BUG();
					}
					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(MIPI_PHY_ENABLE, pmu_addr);

					/*
					 * enable reset -> release reset
					 * M4S4 could not be reset exclusive of first power on
					 * cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
					 * cfg = __raw_readl(cmu_addr);
					 * cfg &= ~(1 << 0);
					 * __raw_writel(cfg, cmu_addr);
					 * cfg |= (1 << 0);
					 * __raw_writel(cfg, cmu_addr);
					 */
				}
				break;
			case 1:
				pmu_addr = S7P_MIPI_DPHY_CONTROL(2);
				__raw_writel(MIPI_PHY_ENABLE, pmu_addr);

				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 8);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 8);
				__raw_writel(cfg, cmu_addr);

				break;
			case 2:
				pmu_addr = S7P_MIPI_DPHY_CONTROL(3);
				__raw_writel(MIPI_PHY_ENABLE, pmu_addr);

				cmu_addr = EXYNOS7420_VA_SYSREG + 0x2930;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 12);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 12);
				__raw_writel(cfg, cmu_addr);

				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		} else {
			switch(id) {
			case 0:
				--dphy_m4s4_status;
				if (dphy_m4s4_status == 0) {
					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(0, pmu_addr);
				}
				break;
			case 1:
				pmu_addr = S7P_MIPI_DPHY_CONTROL(2);
				__raw_writel(0, pmu_addr);
				break;
			case 2:
				pmu_addr = S7P_MIPI_DPHY_CONTROL(3);
				__raw_writel(0, pmu_addr);
				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		}
	} else { /* reset ==  S5P_MIPI_DPHY_MRESETN */
		if (on) {
			switch(id) {
			case 0:
				cfg = __raw_readl(S7P_MIPI_DPHY_CONTROL(0));
				++dphy_m4s4_status;
				/* it is a UBOOT_LCD case */
				if ((dphy_m4s4_status == 1) && (cfg & 0x1))
					goto p_err;

				if ((dphy_m4s4_status == 1) || (!(cfg & 0x1))) {
					if (dphy_m4s4_status != 1) {
						pr_info("%s: reset %d: dphy_m4s4_ref_cnt %d is wrong",
							 __func__, reset, dphy_m4s4_status);
						BUG();
					}
					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(MIPI_PHY_ENABLE, pmu_addr);

					cmu_addr = S7P_MIPI_DPHY_SYSREG;
					cfg = __raw_readl(cmu_addr);

					cfg &= ~(1 << 0);
					__raw_writel(cfg, cmu_addr);
					cfg |= (1 << 0);
					__raw_writel(cfg, cmu_addr);
				}
				break;
			case 1:
				cfg = __raw_readl(S7P_MIPI_DPHY_CONTROL(1));
				/* it is a UBOOT_LCD case */
				if (cfg & 0x1)
					goto p_err;

				pmu_addr = S7P_MIPI_DPHY_CONTROL(1);
				__raw_writel(MIPI_PHY_ENABLE, pmu_addr);

				cmu_addr = S7P_MIPI_DPHY_SYSREG;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 4);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 4);
				__raw_writel(cfg, cmu_addr);
				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		} else { /* off */
			switch(id) {
			case 0:
				--dphy_m4s4_status;
				if (dphy_m4s4_status == 0) {
					cmu_addr = S7P_MIPI_DPHY_SYSREG;
					cfg = __raw_readl(cmu_addr);

					cfg &= ~(1 << 0);
					__raw_writel(cfg, cmu_addr);
					cfg |= (1 << 0);
					__raw_writel(cfg, cmu_addr);

					pmu_addr = S7P_MIPI_DPHY_CONTROL(0);
					__raw_writel(0, pmu_addr);
				}
				break;
			case 1:
				cmu_addr = S7P_MIPI_DPHY_SYSREG;
				cfg = __raw_readl(cmu_addr);

				cfg &= ~(1 << 4);
				__raw_writel(cfg, cmu_addr);
				cfg |= (1 << 4);
				__raw_writel(cfg, cmu_addr);

				pmu_addr = S7P_MIPI_DPHY_CONTROL(1);
				__raw_writel(0, pmu_addr);
				break;
			default:
				pr_err("id(%d) is invalid", id);
				ret =  -EINVAL;
				goto p_err;
				break;
			}
		}
	}

p_err:
	spin_unlock_irqrestore(&lock, flags);
	return ret;
}
#endif

int exynos5_csis_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, MIPI_PHY_SRESETN);
}
EXPORT_SYMBOL(exynos5_csis_phy_enable);

int exynos5_dism_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, MIPI_PHY_MRESETN);
}
EXPORT_SYMBOL(exynos5_dism_phy_enable);
