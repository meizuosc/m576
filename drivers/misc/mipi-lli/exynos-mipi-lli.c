/*
 * Exynos MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/mipi-lli.h>
#include <mach/exynos-pm.h>

#include "exynos-mipi-lli.h"
#include "exynos-mipi-lli-mphy.h"

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#include "exynos543x-mipi-lli.c"
#endif

#if defined(CONFIG_SOC_EXYNOS7420)
#include "exynos7420-mipi-lli.c"
#endif

#define EXYNOS_LLI_LINK_START		(0x4000)
/*
 * 5ns, Default System Clock 100MHz
 * SYSTEM_CLOCK_PERIOD = 1000MHz / System Clock
 */
#define SYSTEM_CLOCK_PERIOD		(10)
#define MPHY_OVTM_DUMP_MAX		(144)

static unsigned int credit_cnt = 10;
module_param(credit_cnt, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(credit_cnt, "credit waiting count");

static int roe_cnt;
static int mnt_cnt;
static int pa_err_cnt;
static int mnt_fail_cnt;

static const int lli_debug_info[] = {
	/* START OF LLI_PA_ATTRIBUTES */
	EXYNOS_PA_MK0_INSERTION_ENABLE,
	EXYNOS_PA_WT_START_VALUE,
	EXYNOS_PA_PHIT_ERROR_COUNTER,
	EXYNOS_PA_PHIT_RECEIVE_COUNTER_LSB,
	EXYNOS_PA_PHIT_RECEIVE_COUNTER_MSB,
	EXYNOS_PA_WORSTCASE_RTT,
	EXYNOS_PA_CSA_PA_STATUS,
	EXYNOS_PA_SYSTEM_CLK_PERIOD,
	EXYNOS_PA_MK0_CONTROL,
	EXYNOS_PA_NACK_RTT,
	EXYNOS_PA_SMART_BUFFER_ENABLE,
	EXYNOS_PA_MPHY_OV_TM_ENABLE,
	EXYNOS_PA_MPHY_CMN_ENABLE,
	EXYNOS_PA_USR_SCRAMBLER_ENABLE,
	EXYNOS_PA_DBG_TACTIVATE_TIMEOUT_CNT,
	EXYNOS_PA_DBG_PA_SIGNAL_STATUS,
	EXYNOS_PA_DBG_RETRY_BUFFER_STATUS0,
	EXYNOS_PA_DBG_RETRY_BUFFER_STATUS1,
	EXYNOS_PA_DBG_RW_PA_STATE,
	EXYNOS_PA_DBG_RW_MNT_STATE,
	EXYNOS_PA_DBG_RW_PLU_STATE,
	EXYNOS_PA_DBG_RW_ROE_STATE,
	EXYNOS_PA_DBG_RW_TX_STATE,
	EXYNOS_PA_PA_DBG_RX_LATCH_MODE,
	EXYNOS_PA_DBG_PHY_SIGNAL_STATUS,
	EXYNOS_PA_DBG_TX0_BURST_MK0_COUNT,
	EXYNOS_PA_DBG_TX0_NACK_MK2_COUNT,
	EXYNOS_PA_DBG_RX0_BURST_MK0_COUNT,
	EXYNOS_PA_DBG_RX0_NACK_MK2_COUNT,
	EXYNOS_PA_DBG_RX0_SYMERROR_COUNT,
	/* END OF LLI_PA_ATTRIBUTES */
	/* START OF LLI_NON_PA_ATTRIBUTES */
	EXYNOS_DME_CSA_SYSTEM_STATUS,
	EXYNOS_DME_LLI_INTR_ENABLE,
	EXYNOS_DME_LLI_INTR_STATUS,
	EXYNOS_DME_LLI_TL_INTR_REASON,
	EXYNOS_DME_LLI_DL_INTR_REASON,
	EXYNOS_DME_LLI_PA_INTR_REASON,
	EXYNOS_DME_LLI_IAL_INTR_REASON0,
	EXYNOS_DME_LLI_IAL_INTR_REASON1,
	EXYNOS_DME_DBG_DME_STATE,
	EXYNOS_DL_DBG_TX_CREDTIS,
	EXYNOS_DL_DBG_RX_BUFFER_SIZES,
	EXYNOS_DL_DBG_RX_BUFFER_VACANCIES,
	EXYNOS_DL_DBG_RX_RESIDUAL_CREDITS,
	EXYNOS_DL_DBG_SIGNAL_STATUS,
	/* END OF LLI_NON_PA_ATTRIBUTES */
};

static int poll_bit_set(void __iomem *ptr, u32 val, int timeout)
{
	u32 reg;

	do {
		reg = readl(ptr);
		if (reg & val)
			return 0;

		udelay(1);
	} while (timeout-- > 0);

	return -ETIME;
}

static u32 exynos_lli_cal_remap(phys_addr_t base_addr, unsigned long size)
{
	phys_addr_t remap_addr;
	unsigned long bit_pos;

	if (!size)
		return 0;

	bit_pos = find_first_bit(&size, 32);

	/* check size is pow of 2 */
	if (size != (1 << bit_pos))
		return 0;

	/* check base_address is aligned with size */
	if (base_addr & (size - 1))
		return 0;

	remap_addr = (base_addr >> bit_pos) << LLI_REMAP_BASE_ADDR_SHIFT;
	remap_addr |= bit_pos;
	remap_addr |= LLI_REMAP_ENABLE;

	return (u32)remap_addr;
}

static void exynos_lli_print_dump(struct work_struct *work)
{
	struct mipi_lli *lli = container_of(work, struct mipi_lli,
			wq_print_dump);
	struct mipi_lli_dump *dump = &lli->dump;
	struct exynos_mphy *phy = dev_get_drvdata(lli->mphy);
	int len = 0, i = 0;

	len = ARRAY_SIZE(lli_debug_clk_info);
	for (i = 0; i < len; i++)
		dev_err(lli->dev, "[LLI-CLK] 0x%p : 0x%08x\n",
				lli_debug_clk_info[i], dump->clk[i]);

	len = ARRAY_SIZE(lli_debug_info);
	for (i = 0; i < len; i++)
		dev_err(lli->dev, "[LLI] 0x%x : 0x%08x\n",
				0x10F24000 + (lli_debug_info[i]),
				dump->lli[i]);

	len = ARRAY_SIZE(phy_std_debug_info);
	for (i = 0; i < len; i++)
		dev_err(lli->dev, "[MPHY-STD] 0x%x : 0x%08x\n",
				0x10F20000 + (phy_std_debug_info[i] / 4),
				dump->mphy_std[i]);

	len = ARRAY_SIZE(dump->mphy_cmn) / 8;
	for (i = 0; i < len; i++)
		dev_err(lli->dev, "[MPHY-CMN] 0x%x : 0x%08x\n",
				0x10F20000 + i,
				dump->mphy_cmn[i]);

	len = MPHY_OVTM_DUMP_MAX;
	for (i = 0; i < len; i++)
		dev_err(lli->dev, "[MPHY-OVTM] 0x%x : 0x%08x\n",
				0x10F20000 + i,
				dump->mphy_ovtm[i]);

	if(phy && phy->pma_regs) {
		len = ARRAY_SIZE(dump->mphy_pma);
		for (i = 0; i < len; i++)
			dev_err(lli->dev, "[MPHY-PMA] 0x%x : 0x%08x\n",
					0x10ED0000 + i, dump->mphy_pma[i]);
	}

	memset(dump, 0, sizeof(struct mipi_lli_dump));
}

static int exynos_lli_reg_dump(struct mipi_lli *lli)
{
	struct exynos_mphy *phy = dev_get_drvdata(lli->mphy);
	struct mipi_lli_dump *dump = &lli->dump;
	int len = 0, i = 0;

	memset(dump, 0, sizeof(struct mipi_lli_dump));

	len = ARRAY_SIZE(lli_debug_clk_info);
	for (i = 0; i < len; i++)
		dump->clk[i] = readl(lli_debug_clk_info[i]);

	len = ARRAY_SIZE(lli_debug_info);
	for (i = 0; i < len; i++)
		dump->lli[i] = readl(lli->regs + lli_debug_info[i]);

	len = ARRAY_SIZE(phy_std_debug_info);
	for (i = 0; i < len; i++)
		dump->mphy_std[i] = readl(phy->loc_regs + phy_std_debug_info[i]);

	len = ARRAY_SIZE(dump->mphy_cmn) / 8;
	writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	for (i = 0; i < len; i++)
		dump->mphy_cmn[i] = readl(phy->loc_regs + i*4);
	writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);

	len = MPHY_OVTM_DUMP_MAX;
	writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	for (i = 0; i < len; i++)
		dump->mphy_ovtm[i] = readl(phy->loc_regs +i*4);
	writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);

	if(phy->pma_regs) {
		len = ARRAY_SIZE(dump->mphy_pma);
		for (i = 0; i < len; i++)
			dump->mphy_pma[i] = readl(phy->pma_regs + i*4);
	}

	return 0;
}

static int exynos_lli_debug_info(struct mipi_lli *lli)
{
	if(lli->is_debug_possible) {
		exynos_lli_reg_dump(lli);
		schedule_work(&lli->wq_print_dump);
	}

	return 0;
}

static void exynos_lli_sys_init(struct work_struct *work)
{
	struct mipi_lli *lli = container_of(work, struct mipi_lli, wq_sys_init);

	/* enable LLI_PHY_CONTROL */
	writel(1, lli->pmu_regs);

	exynos_lli_system_config(lli);
	/* init clock mux selection for LLI & M-PHY */
	exynos_lli_clock_init(lli);
	if (!lli->is_clk_enabled) {
		exynos_lli_clock_gating(lli, false);
		dev_dbg(lli->dev, "Clock is enabled\n");
		lli->is_clk_enabled = true;
	}
	/* Set clk divider to provide 100Mhz clk for cfg-clk of mipi-lli. */
	exynos_lli_clock_div(lli);

#if defined(CONFIG_SOC_EXYNOS7420)
	exynos_mphy_block_powerdown(lli->mphy, false);
#endif

	lli->is_suspended = false;
	/* LLI Interrupt enable */
	writel(0x3FFFF, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);
	writel(1, lli->regs + EXYNOS_DME_LLI_RESET);

	if(lli->event == LLI_EVENT_RESUME)
		mipi_lli_event_irq(lli, LLI_EVENT_RESUME);
}

static int exynos_lli_init(struct mipi_lli *lli)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&lli->lock, irq_flags);
	queue_work(lli->wq, &lli->wq_sys_init);
	spin_unlock_irqrestore(&lli->lock, irq_flags);

	return 0;
}

static int exynos_lli_setting(struct mipi_lli *lli)
{
	struct exynos_mphy *phy;
	struct exynos_mphy_driver *driver;
	u32 remap_addr;

	phy = dev_get_drvdata(lli->mphy);
	if (!phy || !phy->driver) {
		dev_err(lli->dev, "mphy configuration driver isn't exist\n");
		return -EINVAL;
	}
	driver = phy->driver;

	/* update lli_link_state as reset */
	atomic_set(&lli->state, LLI_WAITFORMOUNT);

	/* set_system clk period */
	writel(SYSTEM_CLOCK_PERIOD, lli->regs + EXYNOS_PA_SYSTEM_CLK_PERIOD);
	/* Set DriveTactiveDuration */
	writel(0xf, lli->regs + EXYNOS_PA_DRIVE_TACTIVATE_DURATION);

	remap_addr = exynos_lli_cal_remap(lli->phys_addr, lli->shdmem_size);
	if (!remap_addr) {
		dev_err(lli->dev, "remap calculation error\n");
		return -EINVAL;
	}

	/* Un-set LL_INIT REMAP Address */
	writel(remap_addr, lli->regs + EXYNOS_IAL_LL_INIT_ADDR_REMAP);
	/* Set BE_INIT REMAP Address */
	writel(remap_addr, lli->regs + EXYNOS_IAL_BE_INIT_ADDR_REMAP);

	/* LLI Interrupt enable */
	writel(0x3FFFF, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);

	/* Set BE TC enable */
	writel(0, lli->regs + EXYNOS_DME_BE_TC_DISABLE);
	/* Set Breaking point enable */
	writel(0, lli->regs + EXYNOS_PA_DBG_PA_BREAK_POINT_ENABLE);
	/* Set Error count enable */
	writel(1, lli->regs + EXYNOS_PA_PHIT_ERR_COUNT_ENABLE);
	/* Set Receive count enable */
	writel(1, lli->regs + EXYNOS_PA_PHIT_RECEIVE_COUNT_ENABLE);

	/* Set Acti Tx/Rx count enable */
	writel(1, lli->regs + EXYNOS_PA_TX_COUNT);
	writel(1, lli->regs + EXYNOS_PA_RX_COUNT);

	/*
	 Set Rx Latch
	 Edge and number of latches from M-RX to PA.
	 [0:7] where,
	 [0] : Rising edge. 1 cycle.
	 [1] : Rising edge. 2 cycles.
	 [2] : No latch (Bypass)
	 [3-7] : Reserved
	 */
	writel(2, lli->regs + EXYNOS_PA_PA_DBG_RX_LATCH_MODE);

	writel(0x40, lli->regs + EXYNOS_PA_NACK_RTT);
	writel(0x0, lli->regs + EXYNOS_PA_MK0_INSERTION_ENABLE);
#if defined(CONFIG_UMTS_MODEM_SS300) || defined(CONFIG_UMTS_MODEM_SS303) || defined(CONFIG_UMTS_MODEM_SS333)
	writel((128<<0) | (15<<8) | (1<<12), lli->regs + EXYNOS_PA_MK0_CONTROL);
#else
	writel((128<<0) | (1<<12), lli->regs + EXYNOS_PA_MK0_CONTROL);
#endif
	/* Set Scrambler enable */
	if (lli->modem_info.scrambler)
		writel(1, lli->regs + EXYNOS_PA_USR_SCRAMBLER_ENABLE);

	/* MPHY configuration */
	if (driver->init)
		driver->init(phy);

	if (driver->cmn_init) {
		writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
		driver->cmn_init(phy);
		writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	}

	if (driver->ovtm_init) {
		writel(0x1, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
		driver->ovtm_init(phy);
		writel(0x0, lli->regs + EXYNOS_PA_MPHY_OV_TM_ENABLE);
	}

	if (driver->pma_init)
		driver->pma_init(phy);
	/* Update PA configuration for MPHY standard attributes */
	writel(0xFFFFFFFF, lli->regs + EXYNOS_PA_CONFIG_UPDATE);

	/* Set SNF FIFO for LL&BE */
	writel(((0x1F<<1) | 1), lli->regs + EXYNOS_IAL_LL_SNF_FIFO);
	writel(((0x1F<<1) | 1), lli->regs + EXYNOS_IAL_BE_SNF_FIFO);

	writel(0x1000, lli->regs + EXYNOS_PA_WORSTCASE_RTT);

	dev_dbg(lli->dev, "MIPI LLI is initialized\n");

	return 0;
}

static int exynos_lli_set_master(struct mipi_lli *lli, bool is_master)
{
	lli->is_master = is_master;

	writel((is_master << 1), lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);

	return 0;
}

static int exynos_lli_link_startup_mount(struct mipi_lli *lli)
{
	u32 regs;
	u32 ret = 0;

	if (lli->is_master) {
		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_STATUS);
		if (regs & LLI_MOUNTED) {
			pr_debug("LLI master already mounted\n");
			return ret;
		}

		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
		writel((0x1 << 2) | regs,
			lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	} else {
		ret = poll_bit_set(lli->regs + EXYNOS_DME_LLI_INTR_STATUS,
			     INTR_MPHY_HIBERN8_EXIT_DONE,
			     1000);
		if (ret) {
			dev_err(lli->dev, "HIBERN8 Exit Failed\n");
			return ret;
		}

		regs = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
		writel((0x1 << 2) | regs,
			lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	}

	return ret;
}

static int exynos_lli_get_status(struct mipi_lli *lli)
{
	return atomic_read(&lli->state);
}

static int exynos_lli_send_signal(struct mipi_lli *lli, u32 cmd)
{
	int is_mounted = 0;
	if (atomic_read(&lli->state) == LLI_MOUNTED) {
		/* For unmount failed issue, we should check the
		   LLI_MOUNT_CTRL is cleared or not */
		is_mounted = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_STATUS);
		if (is_mounted & LLI_MOUNT_CTRL) {
#if defined(CONFIG_SOC_EXYNOS5430) && defined(CONFIG_UMTS_MODEM_SS300)
			writel(0, lli->sys_regs + CPIF_LLI_SIG_IN0);
			udelay(10);
			writel(cmd, lli->sys_regs + CPIF_LLI_SIG_IN0);
#elif defined(CONFIG_LTE_MODEM_XMM7260)
			writel(cmd, lli->remote_regs + EXYNOS_TL_SIGNAL_SET_LSB
					+ 0x20C);
#else
			writel(cmd, lli->remote_regs +
					EXYNOS_TL_SIGNAL_SET_LSB);
#endif
			return 0;
		}
	}

	dev_err(lli->dev, "LLI not mounted !! mnt_reg = %x", is_mounted);

	return -EIO;
}

static int exynos_lli_reset_signal(struct mipi_lli *lli)
{
	writel(0xFFFFFFFF, lli->regs + EXYNOS_TL_SIGNAL_CLR_LSB);
	writel(0xFFFFFFFF, lli->regs + EXYNOS_TL_SIGNAL_CLR_MSB);

	return 0;
}

static int exynos_lli_read_signal(struct mipi_lli *lli)
{
	u32 intr_lsb, intr_msb;

	intr_lsb = readl(lli->regs + EXYNOS_TL_SIGNAL_STATUS_LSB);
	intr_msb = readl(lli->regs + EXYNOS_TL_SIGNAL_STATUS_MSB);

	if (intr_lsb)
		writel(intr_lsb, lli->regs + EXYNOS_TL_SIGNAL_CLR_LSB);
	if (intr_msb)
		writel(intr_msb, lli->regs + EXYNOS_TL_SIGNAL_CLR_MSB);

	/* TODO: change to dev_dbg */
	dev_dbg(lli->dev, "LSB = %x, MSB = %x\n", intr_lsb, intr_msb);

	return intr_lsb;
}

static int exynos_lli_intr_enable(struct mipi_lli *lli)
{
	u32 lli_intr = 0;

	if (mipi_lli_suspended())
		return -1;

	if((atomic_read(&lli->state) != LLI_UNMOUNTED) && lli->is_clk_enabled) {
		lli_intr = readl(lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);

		if (lli_intr != 0x3FFFF)
			writel(0x3FFFF, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);
	}

	return 0;
}

static int exynos_lli_intr_disable(struct mipi_lli *lli)
{
       if (mipi_lli_suspended())
               return -1;

       if((atomic_read(&lli->state) != LLI_UNMOUNTED) && lli->is_clk_enabled)
               writel(0x0, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);

       return 0;
}

#ifdef CONFIG_PM_SLEEP
/* exynos_lli_suspend must call by modem_if */
static int exynos_lli_suspend(struct mipi_lli *lli)
{
	spin_lock(&lli->lock);
	if (lli->is_suspended) {
		spin_unlock(&lli->lock);
		return 0;
	}

	/* masking all of lli interrupts */
	writel(0x0, lli->regs + EXYNOS_DME_LLI_INTR_ENABLE);
	/* clearing all of lli sideband signal */
	exynos_lli_reset_signal(lli);

#if defined(CONFIG_SOC_EXYNOS7420)
	exynos_mphy_block_powerdown(lli->mphy, true);
#endif

	if (lli->is_clk_enabled) {
		exynos_lli_clock_gating(lli, true);
		lli->is_clk_enabled = false;
	}
	/* disable LLI_PHY_CONTROL */
	writel(0, lli->pmu_regs);

	lli->is_suspended = true;
	lli->is_debug_possible = false;
	spin_unlock(&lli->lock);

	mipi_lli_event_irq(lli, LLI_EVENT_SUSPEND);

	return 0;
}

/* exynos_lli_resume must call by modem_if */
static int exynos_lli_resume(struct mipi_lli *lli)
{
	spin_lock(&lli->lock);

	if (!lli->is_suspended) {
		spin_unlock(&lli->lock);
		return 0;
	}

	/* re-init all of lli resource */
	lli->event = LLI_EVENT_RESUME;
	queue_work(lli->wq, &lli->wq_sys_init);

	spin_unlock(&lli->lock);

	return 0;
}

static int exynos_mipi_lli_suspend(struct device *dev)
{
	struct mipi_lli *lli = dev_get_drvdata(dev);

	if (!IS_ENABLED(CONFIG_LINK_DEVICE_LLI))
		exynos_lli_suspend(lli);

	return 0;
}

static int exynos_mipi_lli_resume(struct device *dev)
{
	return 0;
}
#else
#define exynos_lli_suspend NULL
#define exynos_lli_resume NULL
#define exynos_mipi_lli_suspend NULL
#define exynos_mipi_lli_resume NULL
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_RUNTIME
static int exynos_lli_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	lli->is_runtime_suspended = true;

	return 0;
}

static int exynos_lli_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	lli->is_runtime_suspended = false;

	return 0;
}
#else
#define exynos_lli_runtime_suspend NULL
#define exynos_lli_runtime_resume NULL
#endif /* CONFIG_PM_RUNTIME */

#ifdef CONFIG_CPU_IDLE
static int exynos_lli_lpa_event(struct notifier_block *notifier,
		unsigned long pm_event, void *v)
{
	struct mipi_lli *lli = container_of(notifier, struct mipi_lli, lpa_nb);
	int err = NOTIFY_DONE;

	switch (pm_event) {
	case LPA_ENTER:
		break;
	case LPA_PREPARE:
	case LPC_PREPARE:
		if (atomic_read(&lli->state) != LLI_UNMOUNTED)
			err = -EBUSY;
		break;
	case LPA_ENTER_FAIL:
	case LPA_EXIT:
		break;
	}

	return notifier_from_errno(err);
}
#endif

static void exynos_mipi_lli_set_automode(struct mipi_lli *lli, bool is_auto)
{
	if (is_auto)
		writel(CSA_AUTO_MODE, lli->regs + EXYNOS_PA_CSA_PA_SET);
	else
		writel(CSA_AUTO_MODE, lli->regs + EXYNOS_PA_CSA_PA_CLR);
}

const struct lli_driver exynos_lli_driver = {
	.init = exynos_lli_init,
	.set_master = exynos_lli_set_master,
	.link_startup_mount = exynos_lli_link_startup_mount,
	.get_status = exynos_lli_get_status,
	.send_signal = exynos_lli_send_signal,
	.reset_signal = exynos_lli_reset_signal,
	.read_signal = exynos_lli_read_signal,
	.loopback_test = exynos_lli_loopback_test,
	.debug_info = exynos_lli_debug_info,
	.intr_enable = exynos_lli_intr_enable,
	.intr_disable = exynos_lli_intr_disable,
	.suspend = exynos_lli_suspend,
	.resume = exynos_lli_resume,
};

static irqreturn_t exynos_mipi_lli_thread(int irq, void *_dev)
{
	struct device *dev = _dev;
	struct mipi_lli *lli = dev_get_drvdata(dev);
	struct exynos_mphy *phy;
	static bool is_first = true;
	int rx_fsm_state, tx_fsm_state, afc_val, csa_status, credit = 0;

	phy = dev_get_drvdata(lli->mphy);

	if(lli->event == LLI_EVENT_UNMOUNTED) {
		if (lli->is_clk_enabled) {
			exynos_lli_clock_gating(lli, true);
			dev_err(dev, "Clock is gated\n");
			lli->is_clk_enabled = false;
		}

		mipi_lli_event_irq(lli, LLI_EVENT_UNMOUNTED);
		dev_err(dev, "Unmounted\n");

		return IRQ_HANDLED;
	}

	rx_fsm_state = readl(phy->loc_regs + PHY_RX_FSM_STATE(0));
	tx_fsm_state = readl(phy->loc_regs + PHY_TX_FSM_STATE(0));
	csa_status = readl(lli->regs + EXYNOS_DME_CSA_SYSTEM_STATUS);
	credit = readl(lli->regs + EXYNOS_DL_DBG_TX_CREDTIS);

#if defined(CONFIG_SOC_EXYNOS7420)
	afc_val = readl(phy->pma_regs + (0x1F*4));
#else
	writel(0x1, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
	afc_val = readl(phy->loc_regs + (0x27*4));
	writel(0x0, lli->regs + EXYNOS_PA_MPHY_CMN_ENABLE);
#endif

	if (is_first) {
		phy->afc_val = afc_val;
		is_first = false;
	}

	dev_err(dev, "rx=%x, tx=%x, afc=%x, status=%x, pa_err=%x\n",
		rx_fsm_state, tx_fsm_state, afc_val, csa_status, pa_err_cnt);

	if (!credit) {
		u32 i = 0;

		dev_err(dev, "no credit\n");
		for (i = 0; i < credit_cnt; i++) {
			usleep_range(1000, 1100);
			credit = readl(lli->regs + EXYNOS_DL_DBG_TX_CREDTIS);
			if (credit)
				break;
		}
		dev_err(dev, "waited for %dms for CREDITS: tx=%x, rx=%x\n", i,
				readl(phy->loc_regs + PHY_RX_FSM_STATE(0)),
				readl(phy->loc_regs + PHY_TX_FSM_STATE(0)));
		if (!credit) {
			dev_err(dev, "ERR: Mount failed: %d\n", ++mnt_fail_cnt);
			mipi_lli_debug_info();
			dev_err(dev, "DUMP: ok:%d fail:%d roe:%d",
					mnt_cnt, mnt_fail_cnt, roe_cnt);
			return IRQ_HANDLED;
		}
	}

	atomic_set(&lli->state, LLI_MOUNTED);
	mipi_lli_event_irq(lli, LLI_EVENT_MOUNTED);

	dev_err(dev, "Mount ok:%d fail:%d roe:%d pa_err:%d\n",
			++mnt_cnt, mnt_fail_cnt, roe_cnt, pa_err_cnt);

	return IRQ_HANDLED;
}

static irqreturn_t exynos_mipi_lli_irq(int irq, void *_dev)
{
	struct device *dev = _dev;
	struct mipi_lli *lli = dev_get_drvdata(dev);
	int status;
	struct exynos_mphy *phy;

	phy = dev_get_drvdata(lli->mphy);
	status = readl(lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	if (status & INTR_SW_RESET_DONE) {
		dev_err(dev, "SW_RESET_DONE ++\n");
		exynos_lli_setting(lli);
		lli->event = LLI_EVENT_RESET;
		lli->is_debug_possible = true;
		mipi_lli_event_irq(lli, LLI_EVENT_RESET);

		if (status & INTR_RESET_ON_ERROR_DETECTED) {
			dev_err(dev, "LLI is wating for mount "
					"after LINE-RESET..\n");
			atomic_set(&lli->state, LLI_WAITFORMOUNT);
			mipi_lli_event_irq(lli, LLI_EVENT_WAITFORMOUNT);
		}
	}

	if (status & INTR_MPHY_HIBERN8_EXIT_DONE) {
		dev_err(dev, "HIBERN8_EXIT_DONE: rx=%x, tx=%x\n",
				readl(phy->loc_regs + PHY_RX_FSM_STATE(0)),
				readl(phy->loc_regs + PHY_TX_FSM_STATE(0)));
		mdelay(1);
		writel(LLI_MOUNT_CTRL, lli->regs + EXYNOS_DME_CSA_SYSTEM_SET);
	}

	if (status & INTR_MPHY_HIBERN8_ENTER_DONE)
		dev_err(dev, "HIBERN8_ENTER_DONE\n");

	if (status & INTR_PA_PLU_DETECTED)
		dev_err(dev, "PLU_DETECT\n");

	if (status & INTR_PA_PLU_DONE) {
		dev_err(dev, "PLU_DONE\n");

		if (lli->modem_info.automode)
			exynos_mipi_lli_set_automode(lli, true);
	}

	if ((status & INTR_RESET_ON_ERROR_DETECTED)) {
		dev_err(dev, "Error detected ++ roe_cnt = %d\n", ++roe_cnt);
	}

	if (status & INTR_RESET_ON_ERROR_SENT) {
		dev_err(dev, "Error sent ++ roe_cnt = %d\n", ++roe_cnt);
		writel(1, lli->regs + EXYNOS_DME_LLI_RESET);

		return IRQ_HANDLED;
	}

	if (status & INTR_PA_ERROR_INDICATION) {
		dev_err_ratelimited(dev, "PA_REASON %x\n",
				readl(lli->regs + EXYNOS_DME_LLI_PA_INTR_REASON)
				);
		pa_err_cnt++;
	}

	if (status & INTR_DL_ERROR_INDICATION) {
		dev_err(dev, "DL_REASON %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_DL_INTR_REASON));
	}

	if (status & INTR_TL_ERROR_INDICATION) {
		dev_err(dev, "TL_REASON %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_TL_INTR_REASON));
	}

	if (status & INTR_IAL_ERROR_INDICATION) {
		dev_err(dev, "IAL_REASON0 %x, REASON1 %x\n",
			readl(lli->regs + EXYNOS_DME_LLI_IAL_INTR_REASON0),
			readl(lli->regs + EXYNOS_DME_LLI_IAL_INTR_REASON1));
	}

	if (status & INTR_LLI_MOUNT_DONE) {
		writel(status, lli->regs + EXYNOS_DME_LLI_INTR_STATUS);
		dev_err(dev, "Mount intr\n");
		lli->event = LLI_EVENT_MOUNTED;

		return IRQ_WAKE_THREAD;
	}

	if (status & INTR_LLI_UNMOUNT_DONE) {
		atomic_set(&lli->state, LLI_UNMOUNTED);
		writel(status, lli->regs + EXYNOS_DME_LLI_INTR_STATUS);
		dev_err(dev, "Unmount intr\n");
		lli->is_debug_possible = false;
		lli->event = LLI_EVENT_UNMOUNTED;

		return IRQ_WAKE_THREAD;
	}

	writel(status, lli->regs + EXYNOS_DME_LLI_INTR_STATUS);

	return IRQ_HANDLED;
}

int mipi_lli_get_setting(struct mipi_lli *lli)
{
	struct device_node *lli_node = lli->dev->of_node;
	struct device_node *modem_node;
	const char *modem_name;
	const __be32 *prop;

	modem_name = of_get_property(lli_node, "modem-name", NULL);
	if (!modem_name) {
		dev_err(lli->dev, "parsing err : modem-name node\n");
		goto parsing_err;
	}
	modem_node = of_get_child_by_name(lli_node, "modems");
	if (!modem_node) {
		dev_err(lli->dev, "parsing err : modems node\n");
		goto parsing_err;
	}
	modem_node = of_get_child_by_name(modem_node, modem_name);
	if (!modem_node) {
		dev_err(lli->dev, "parsing err : modem node\n");
		goto parsing_err;
	}

	lli->modem_info.name = devm_kzalloc(lli->dev, strlen(modem_name),
			GFP_KERNEL);
	strncpy(lli->modem_info.name, modem_name, strlen(modem_name));

	prop = of_get_property(modem_node, "scrambler", NULL);
	if (prop)
		lli->modem_info.scrambler = be32_to_cpup(prop) ? true : false;
	else
		lli->modem_info.scrambler = false;

	prop = of_get_property(modem_node, "automode", NULL);
	if (prop)
		lli->modem_info.automode = be32_to_cpup(prop) ? true : false;
	else
		lli->modem_info.automode = false;

parsing_err:
	dev_err(lli->dev, "modem_name:%s, scrambler:%d, automode:%d\n",
			modem_name,
			lli->modem_info.scrambler,
			lli->modem_info.automode);
	return 0;
}

static int exynos_mipi_lli_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct mipi_lli *lli;
	struct resource *res;
	void __iomem *regs, *remote_regs, *sysregs, *pmuregs;
	int irq, irq_sig;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot find register resource 0\n");
		return -ENXIO;
	}

	regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "cannot find register resource 1\n");
		return -ENXIO;
	}

	remote_regs = devm_request_and_ioremap(dev, res);
	if (!remote_regs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dev_err(dev, "cannot find register resource 2\n");
		return -ENXIO;
	}

	sysregs = devm_request_and_ioremap(dev, res);
	if (!sysregs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dev_err(dev, "cannot find register resource 3\n");
		return -ENXIO;
	}

	pmuregs = devm_request_and_ioremap(dev, res);
	if (!pmuregs) {
		dev_err(dev, "cannot request_and_map registers\n");
		return -EADDRNOTAVAIL;
	}

	/* Request LLI IRQ */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no irq specified\n");
		return -EBUSY;
	}

	/* Request Signal IRQ */
	irq_sig = platform_get_irq(pdev, 1);
	if (irq_sig < 0) {
		dev_err(dev, "no irq_sig specified\n");
		return -EBUSY;
	}

	ret = mipi_lli_add_driver(dev, &exynos_lli_driver, irq_sig);
	if (ret < 0)
		return ret;

	lli = dev_get_drvdata(dev);

	lli->regs = regs;
	lli->remote_regs = remote_regs;
	lli->sys_regs = sysregs;
	lli->pmu_regs = pmuregs;
	lli->is_master = false;
#ifdef CONFIG_CPU_IDLE
	lli->lpa_nb.notifier_call = exynos_lli_lpa_event;
	exynos_pm_register_notifier(&lli->lpa_nb);
#endif

	INIT_WORK(&lli->wq_print_dump, exynos_lli_print_dump);
	lli->wq = alloc_workqueue("lli_wq", WQ_NON_REENTRANT | WQ_UNBOUND | WQ_HIGHPRI, 1);

	if (!lli->wq) {
		dev_err(dev, "ERR! fail to create lli workqueue!! \n");
		return -EFAULT;
	}

	INIT_WORK(&lli->wq_sys_init, exynos_lli_sys_init);

	mipi_lli_get_setting(lli);

	ret = request_threaded_irq(irq, exynos_mipi_lli_irq,
			exynos_mipi_lli_thread, 0, dev_name(dev), dev);
	if (ret < 0)
		return ret;

	if (node) {
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret)
			dev_err(dev, "failed to add mphy\n");
	} else {
		dev_err(dev, "no device node, failed to add mphy\n");
		ret = -ENODEV;
	}

	lli->mphy = exynos_get_mphy();
	if (!lli->mphy) {
		dev_err(dev, "failed get mphy\n");
		return -ENODEV;
	}

	exynos_lli_get_clk_info(lli);

	exynos_lli_system_config(lli);

	spin_lock_init(&lli->lock);
	lli->is_suspended = true;
	lli->is_debug_possible = false;
	lli->event = LLI_EVENT_UNMOUNTED;

	dev_info(dev, "Registered MIPI-LLI interface\n");

	return ret;
}

static int exynos_mipi_lli_remove(struct platform_device *pdev)
{
	struct mipi_lli *lli = platform_get_drvdata(pdev);

	mipi_lli_remove_driver(lli);

	return 0;
}

static const struct dev_pm_ops exynos_mipi_lli_pm = {
	/* suspend, resume functions must call by modem_if. */
	SET_SYSTEM_SLEEP_PM_OPS(exynos_mipi_lli_suspend, exynos_mipi_lli_resume)
	SET_RUNTIME_PM_OPS(exynos_lli_runtime_suspend,
			exynos_lli_runtime_resume, NULL)
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_mipi_lli_dt_match[] = {
	{
		.compatible = "samsung,exynos-mipi-lli"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mipi_lli_dt_match);
#endif

static struct platform_driver exynos_mipi_lli_driver = {
	.probe = exynos_mipi_lli_probe,
	.remove = exynos_mipi_lli_remove,
	.driver = {
		.name = "exynos-mipi-lli",
		.owner = THIS_MODULE,
		.pm = &exynos_mipi_lli_pm,
		.of_match_table = of_match_ptr(exynos_mipi_lli_dt_match),
	},
};

module_platform_driver(exynos_mipi_lli_driver);

MODULE_DESCRIPTION("Exynos MIPI LLI driver");
MODULE_AUTHOR("Yulgon Kim <yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
