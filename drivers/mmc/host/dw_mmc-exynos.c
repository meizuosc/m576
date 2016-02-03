/*
 * Exynos Specific Extensions for Synopsys DW Multimedia Card Interface driver
 *
 * Copyright (C) 2012, Samsung Electronics Co., Ltd.
 * Copyright (C) 2013, The Chromium OS Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>	/* for kmalloc/kfree prototype */
#include <linux/interrupt.h>
#include <linux/pinctrl/pinctrl-samsung.h>

#include <mach/exynos-pm.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"
#include "dw_mmc-exynos.h"

/* SFR save/restore for LPA */
struct dw_mci *dw_mci_lpa_host[3] = {0, 0, 0};
unsigned int dw_mci_host_count;
unsigned int dw_mci_save_sfr[3][30];

extern void dw_mci_ciu_reset(struct device *dev, struct dw_mci *host);
extern bool dw_mci_fifo_reset(struct device *dev, struct dw_mci *host);

/*
 * MSHC SFR save/restore
 */
static void dw_mci_exynos_save_host_base(struct dw_mci *host)
{
	dw_mci_lpa_host[dw_mci_host_count] = host;
	dw_mci_host_count++;
}

int dw_mci_exynos_get_host_id(struct dw_mci *host)
{
	int i;

	for (i=0 ; i<3; i++)
		if (host == dw_mci_lpa_host[i])
			return i;

	BUG_ON(1);
}

static int dw_mci_exynos_priv_init(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(host->dev, "mem alloc failed for private data\n");
		return -ENOMEM;
	}

	host->priv = priv;
	dw_mci_exynos_save_host_base(host);

	return 0;
}

void dw_mci_reg_dump(struct dw_mci *host)
{
	const struct dw_mci_drv_data *drv_data = host->drv_data;

	dev_err(host->dev, ": ============== REGISTER DUMP ==============\n");
	dev_err(host->dev, ": CTRL:	0x%08x\n", mci_readl(host, CTRL));
	dev_err(host->dev, ": PWREN:	0x%08x\n", mci_readl(host, PWREN));
	dev_err(host->dev, ": CLKDIV:	0x%08x\n", mci_readl(host, CLKDIV));
	dev_err(host->dev, ": CLKSRC:	0x%08x\n", mci_readl(host, CLKSRC));
	dev_err(host->dev, ": CLKENA:	0x%08x\n", mci_readl(host, CLKENA));
	dev_err(host->dev, ": TMOUT:	0x%08x\n", mci_readl(host, TMOUT));
	dev_err(host->dev, ": CTYPE:	0x%08x\n", mci_readl(host, CTYPE));
	dev_err(host->dev, ": BLKSIZ:	0x%08x\n", mci_readl(host, BLKSIZ));
	dev_err(host->dev, ": BYTCNT:	0x%08x\n", mci_readl(host, BYTCNT));
	dev_err(host->dev, ": INTMSK:	0x%08x\n", mci_readl(host, INTMASK));
	dev_err(host->dev, ": CMDARG:	0x%08x\n", mci_readl(host, CMDARG));
	dev_err(host->dev, ": CMD:	0x%08x\n", mci_readl(host, CMD));
	dev_err(host->dev, ": RESP0:	0x%08x\n", mci_readl(host, RESP0));
	dev_err(host->dev, ": RESP1:	0x%08x\n", mci_readl(host, RESP1));
	dev_err(host->dev, ": RESP2:	0x%08x\n", mci_readl(host, RESP2));
	dev_err(host->dev, ": RESP3:	0x%08x\n", mci_readl(host, RESP3));
	dev_err(host->dev, ": MINTSTS:	0x%08x\n", mci_readl(host, MINTSTS));
	dev_err(host->dev, ": RINTSTS:	0x%08x\n", mci_readl(host, RINTSTS));
	dev_err(host->dev, ": STATUS:	0x%08x\n", mci_readl(host, STATUS));
	dev_err(host->dev, ": FIFOTH:	0x%08x\n", mci_readl(host, FIFOTH));
	dev_err(host->dev, ": CDETECT:	0x%08x\n", mci_readl(host, CDETECT));
	dev_err(host->dev, ": WRTPRT:	0x%08x\n", mci_readl(host, WRTPRT));
	dev_err(host->dev, ": GPIO:	0x%08x\n", mci_readl(host, GPIO));
	dev_err(host->dev, ": TCBCNT:	0x%08x\n", mci_readl(host, TCBCNT));
	dev_err(host->dev, ": TBBCNT:	0x%08x\n", mci_readl(host, TBBCNT));
	dev_err(host->dev, ": DEBNCE:	0x%08x\n", mci_readl(host, DEBNCE));
	dev_err(host->dev, ": USRID:	0x%08x\n", mci_readl(host, USRID));
	dev_err(host->dev, ": VERID:	0x%08x\n", mci_readl(host, VERID));
	dev_err(host->dev, ": HCON:	0x%08x\n", mci_readl(host, HCON));
	dev_err(host->dev, ": UHS_REG:	0x%08x\n", mci_readl(host, UHS_REG));
	dev_err(host->dev, ": BMOD:	0x%08x\n", mci_readl(host, BMOD));
	dev_err(host->dev, ": PLDMND:	0x%08x\n", mci_readl(host, PLDMND));
#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS5433) || \
	defined(CONFIG_SOC_EXYNOS7580)
	dev_err(host->dev, ": DBADDRL:	0x%08x\n", mci_readl(host, DBADDRL));
	dev_err(host->dev, ": DBADDRU:	0x%08x\n", mci_readl(host, DBADDRU));
	dev_err(host->dev, ": DSCADDRL:	0x%08x\n", mci_readl(host, DSCADDRL));
	dev_err(host->dev, ": DSCADDRU:	0x%08x\n", mci_readl(host, DSCADDRU));
	dev_err(host->dev, ": BUFADDR:	0x%08x\n", mci_readl(host, BUFADDR));
	dev_err(host->dev, ": BUFADDRU:	0x%08x\n", mci_readl(host, BUFADDRU));
#else
	dev_err(host->dev, ": DBADDR:	0x%08x\n", mci_readl(host, DBADDR));
	dev_err(host->dev, ": DSCADDR:	0x%08x\n", mci_readl(host, DSCADDR));
	dev_err(host->dev, ": BUFADDR:	0x%08x\n", mci_readl(host, BUFADDR));
#endif

	dev_err(host->dev, ": IDSTS:	0x%08x\n", mci_readl(host, IDSTS));
	dev_err(host->dev, ": IDINTEN:	0x%08x\n", mci_readl(host, IDINTEN));
	dev_err(host->dev, ": SHA_CMD_IS:	0x%08x\n", mci_readl(host, SHA_CMD_IS));
	if (drv_data && drv_data->register_dump)
		drv_data->register_dump(host);
	dw_mci_cmd_reg_summary(host);
	dw_mci_status_reg_summary(host);
	dev_err(host->dev, ": ============== STATUS DUMP ================\n");
	dev_err(host->dev, ": cmd_status:      0x%08x\n", host->cmd_status);
	dev_err(host->dev, ": data_status:     0x%08x\n", host->data_status);
	dev_err(host->dev, ": pending_events:  0x%08lx\n", host->pending_events);
	dev_err(host->dev, ": completed_events:0x%08lx\n", host->completed_events);
	dev_err(host->dev, ": state:           %d\n", host->state_cmd);
	dev_err(host->dev, ": gate-clk:            %s\n",
			      atomic_read(&host->ciu_clk_cnt) ?
			      "enable" : "disable");
	dev_err(host->dev, ": ciu_en_win:           %d\n",
			atomic_read(&host->ciu_en_win));
	dev_err(host->dev, ": ===========================================\n");
	if ((mci_readl(host, IDSTS) == 0x4000) && (mci_readl(host, MPSTAT) & 0x1))
		panic("eMMC DMA BUS hang.\n");
}

/*
 * Configuration of Security Management Unit
 */
void dw_mci_exynos_cfg_smu(struct dw_mci *host)
{
	volatile unsigned int reg;

#if defined(CONFIG_MMC_DW_FMP_DM_CRYPT) || defined(CONFIG_MMC_DW_FMP_ECRYPT_FS)
	if (!((host->pdata->quirks & DW_MCI_QUIRK_USE_SMU) ||
		(host->pdata->quirks & DW_MCI_QUIRK_BYPASS_SMU)))
		return;
#else
	if (!(host->pdata->quirks & DW_MCI_QUIRK_BYPASS_SMU))
		return;
#endif

	reg = __raw_readl(host->regs + SDMMC_MPSECURITY);

	/* Extended Descriptor On */
#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430) || \
	defined(CONFIG_SOC_EXYNOS5433)
	mci_writel(host, MPSECURITY, reg | DWMCI_MPSECURITY_PROTBYTZPC);
#else
	reg = reg & ~DWMCI_MPSECURITY_MMC_SFR_PROT_ON;
	mci_writel(host, MPSECURITY, (reg | DWMCI_MPSECURITY_PROTBYTZPC) | \
						DWMCI_MPSECURITY_FMP_ENC_ON | DWMCI_MPSECURITY_DESCTYPE(3));
	mci_writel(host, MPSLUN0, DWMCI_SET_LUN(DW_MMC_LUN_BEGIN, DW_MMC_LUN_END));
#endif

	mci_writel(host, MPSBEGIN0, 0);
	mci_writel(host, MPSEND0, DWMCI_BLOCK_NUM);
	mci_writel(host, MPSCTRL0, DWMCI_MPSCTRL_BYPASS);
}

#ifdef CONFIG_CPU_IDLE
#if defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420) || \
	defined(CONFIG_SOC_EXYNOS7580)
static void dw_mci_exynos_smu_reset(struct dw_mci *host)
{
	u32 i;
	bool is_smu;

	is_smu = (host->pdata->quirks & DW_MCI_QUIRK_BYPASS_SMU) ?
				true : false;
#if defined(CONFIG_MMC_DW_FMP_DM_CRYPT) || defined(CONFIG_MMC_DW_FMP_ECRYPT_FS)
	is_smu = is_smu || ((host->pdata->quirks & DW_MCI_QUIRK_USE_SMU) ?
				true : false);
#endif
	if (is_smu) {
		for (i = 1; i < 8; i++) {
			__raw_writel(0x0, host->regs + SDMMC_MPSBEGIN0 + (0x10 * i));
			__raw_writel(0x0, host->regs + SDMMC_MPSEND0 + (0x10 * i));
			__raw_writel(0x0, host->regs + SDMMC_MPSCTRL0 + (0x10 * i));
		}
	}
}
#endif

static void exynos_sfr_save(unsigned int i)
{
	struct dw_mci *host = dw_mci_lpa_host[i];

	atomic_inc_return(&host->biu_en_win);
	dw_mci_biu_clk_en(host, false);

	dw_mci_save_sfr[i][0] = mci_readl(host, CTRL);
	dw_mci_save_sfr[i][1] = mci_readl(host, PWREN);
	dw_mci_save_sfr[i][2] = mci_readl(host, CLKDIV);
	dw_mci_save_sfr[i][3] = mci_readl(host, CLKSRC);
	dw_mci_save_sfr[i][4] = mci_readl(host, CLKENA);
	dw_mci_save_sfr[i][5] = mci_readl(host, TMOUT);
	dw_mci_save_sfr[i][6] = mci_readl(host, CTYPE);
	dw_mci_save_sfr[i][7] = mci_readl(host, INTMASK);
	dw_mci_save_sfr[i][8] = mci_readl(host, FIFOTH);
	dw_mci_save_sfr[i][9] = mci_readl(host, UHS_REG);
	dw_mci_save_sfr[i][10] = mci_readl(host, BMOD);
	dw_mci_save_sfr[i][11] = mci_readl(host, PLDMND);
	dw_mci_save_sfr[i][12] = mci_readl(host, IDINTEN);
	dw_mci_save_sfr[i][13] = mci_readl(host, CLKSEL);
	dw_mci_save_sfr[i][14] = mci_readl(host, CDTHRCTL);
#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS5433) || \
	defined(CONFIG_SOC_EXYNOS7580)
	dw_mci_save_sfr[i][15] = mci_readl(host, DBADDRL);
	dw_mci_save_sfr[i][16] = mci_readl(host, DBADDRU);
	dw_mci_save_sfr[i][20] = mci_readl(host, MPSECURITY);
#else
	dw_mci_save_sfr[i][15] = mci_readl(host, DBADDR);
#endif
	dw_mci_save_sfr[i][17] = mci_readl(host, DDR200_RDDQS_EN);
	dw_mci_save_sfr[i][18] = mci_readl(host, DDR200_DLINE_CTRL);
#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430) || \
	defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420) || \
	defined(CONFIG_SOC_EXYNOS7580)
	dw_mci_save_sfr[i][19] = mci_readl(host, DDR200_ENABLE_SHIFT);
#endif
	/* For LPA */
	atomic_inc_return(&host->ciu_en_win);
	if (host->pdata->enable_cclk_on_suspend) {
		host->pdata->on_suspend = true;
		if(dw_mci_ciu_clk_en(host, false))
			dev_info(host->dev, "ciu_clk_enable_fail!\n");
	}
	if (atomic_dec_return(&host->ciu_en_win) != 0)
		dev_warn(host->dev, "%s ciu_en_win_dec failed\n", __func__);
	if (atomic_dec_return(&host->biu_en_win) != 0)
		dev_warn(host->dev, "%s biu_en_win_dec failed\n", __func__);

}

static void exynos_sfr_restore(unsigned int i)
{
	struct dw_mci *host = dw_mci_lpa_host[i];
	const struct dw_mci_drv_data *drv_data;

	int startbit_clear = false;
	unsigned int cmd_status = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(500);

	drv_data = host->drv_data;

	atomic_inc_return(&host->biu_en_win);
	dw_mci_biu_clk_en(host, false);

	mci_writel(host, CTRL , dw_mci_save_sfr[i][0]);
	mci_writel(host, PWREN, dw_mci_save_sfr[i][1]);
	mci_writel(host, CLKDIV, dw_mci_save_sfr[i][2]);
	mci_writel(host, CLKSRC, dw_mci_save_sfr[i][3]);
	mci_writel(host, CLKENA, dw_mci_save_sfr[i][4]);
	mci_writel(host, TMOUT, dw_mci_save_sfr[i][5]);
	mci_writel(host, CTYPE, dw_mci_save_sfr[i][6]);
	mci_writel(host, INTMASK, dw_mci_save_sfr[i][7]);
	mci_writel(host, FIFOTH, dw_mci_save_sfr[i][8]);
	mci_writel(host, UHS_REG, dw_mci_save_sfr[i][9]);
	mci_writel(host, BMOD  , dw_mci_save_sfr[i][10]);
	mci_writel(host, PLDMND, dw_mci_save_sfr[i][11]);
	mci_writel(host, IDINTEN, dw_mci_save_sfr[i][12]);
	mci_writel(host, CLKSEL, dw_mci_save_sfr[i][13]);
	mci_writel(host, CDTHRCTL, dw_mci_save_sfr[i][14]);
#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS5433) || \
	defined(CONFIG_SOC_EXYNOS7580)
	mci_writel(host, DBADDRL, dw_mci_save_sfr[i][15]);
	mci_writel(host, DBADDRU, dw_mci_save_sfr[i][16]);
	if (drv_data && drv_data->cfg_smu) {
		mci_writel(host, MPSECURITY, dw_mci_save_sfr[i][20]);
		dw_mci_exynos_smu_reset(host);
		drv_data->cfg_smu(host);
	}
#else
	mci_writel(host, DBADDR, dw_mci_save_sfr[i][15]);
#endif
	mci_writel(host, DDR200_RDDQS_EN, dw_mci_save_sfr[i][17]);
	mci_writel(host, DDR200_DLINE_CTRL, dw_mci_save_sfr[i][18]);
#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430) || \
	defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420) || \
	defined(CONFIG_SOC_EXYNOS7580)
	mci_writel(host, DDR200_ENABLE_SHIFT, dw_mci_save_sfr[i][19]);
#endif
	atomic_inc_return(&host->ciu_en_win);
	dw_mci_ciu_clk_en(host, false);

	mci_writel(host, CMDARG, 0);
	wmb();
	mci_writel(host, CMD, (SDMMC_CMD_START |
				SDMMC_CMD_UPD_CLK |
				SDMMC_CMD_PRV_DAT_WAIT));

	while (time_before(jiffies, timeout)) {
		cmd_status = mci_readl(host, CMD);
		if (!(cmd_status & SDMMC_CMD_START)) {
			startbit_clear = true;
			break;
		}
	}

	/* For eMMC HS400 mode */
	if (i == 0)
		mci_writel(host, DDR200_ASYNC_FIFO_CTRL, 0x1);

	if (atomic_dec_return(&host->ciu_en_win) != 0)
		dev_warn(host->dev, "%s ciu_en_win dec failed\n", __func__);
	if (atomic_dec_return(&host->biu_en_win) != 0)
	        dev_warn(host->dev, "%s biu_en_win dec failed\n", __func__);

	/* For unuse clock gating */
	if (host->pdata->enable_cclk_on_suspend) {
		host->pdata->on_suspend = false;
		dw_mci_ciu_clk_dis(host);
		dw_mci_biu_clk_dis(host);
	}

	if (startbit_clear == false)
		dev_err(host->dev, "CMD start bit stuck %02d\n", i);
}

static int dw_mmc_exynos_notifier0(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	int ret = NOTIFY_DONE;
	struct dw_mci *host = dw_mci_lpa_host[0];

	switch (cmd) {
	case LPA_PREPARE:
	case LPC_PREPARE:
		if (host->req_state)
			ret = -EBUSY;
		break;
	case LPA_ENTER:
		exynos_sfr_save(0);
		break;
	case LPA_ENTER_FAIL:
		break;
	case LPA_EXIT:
		exynos_sfr_restore(0);
		break;
	}

	return notifier_from_errno(ret);
}

static int dw_mmc_exynos_notifier1(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	int ret = NOTIFY_DONE;
	struct dw_mci *host = dw_mci_lpa_host[1];

	switch (cmd) {
	case LPA_PREPARE:
	case LPC_PREPARE:
		if (host->req_state)
			ret = -EBUSY;
		break;
	case LPA_ENTER:
		exynos_sfr_save(1);
		break;
	case LPA_ENTER_FAIL:
		break;
	case LPA_EXIT:
		exynos_sfr_restore(1);
		break;
	}

	return notifier_from_errno(ret);
}

static int dw_mmc_exynos_notifier2(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	int ret = NOTIFY_DONE;
	struct dw_mci *host = dw_mci_lpa_host[2];

	switch (cmd) {
	case LPA_PREPARE:
	case LPC_PREPARE:
		if (host->req_state)
			ret = -EBUSY;
		break;
	case LPA_ENTER:
		exynos_sfr_save(2);
		break;
	case LPA_ENTER_FAIL:
		break;
	case LPA_EXIT:
		exynos_sfr_restore(2);
		break;
	}

	return notifier_from_errno(ret);
}

static struct notifier_block dw_mmc_exynos_notifier_block[3] = {
	[0] = {
		.notifier_call = dw_mmc_exynos_notifier0,
		.priority = 2,
	},
	[1] = {
		.notifier_call = dw_mmc_exynos_notifier1,
		.priority = 2,
	},
	[2] = {
		.notifier_call = dw_mmc_exynos_notifier2,
		.priority = 2,
	},
};

void dw_mci_exynos_register_notifier(struct dw_mci *host)
{
	/*
	 * Should be called sequentially
	 */
	BUG_ON(!dw_mci_host_count);
	exynos_pm_register_notifier(
			&(dw_mmc_exynos_notifier_block[dw_mci_host_count-1]));
}

void dw_mci_exynos_unregister_notifier(struct dw_mci *host)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (host == dw_mci_lpa_host[i])
			exynos_pm_unregister_notifier(&(dw_mmc_exynos_notifier_block[i]));
	}
}
#endif

static int dw_mci_exynos_setup_clock(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

#if defined(CONFIG_SOC_EXYNOS4412)
	host->bus_hz /= EXYNOS4412_FIXED_CIU_CLK_DIV;
#elif defined(CONFIG_SOC_EXYNOS4210)
	host->bus_hz /= EXYNOS4210_FIXED_CIU_CLK_DIV;
#else
	host->bus_hz /= (priv->ciu_div + 1);
#endif

	return 0;
}

static void dw_mci_exynos_register_dump(struct dw_mci *host)
{
	u32 i;
	bool is_smu;

	is_smu = (host->pdata->quirks & DW_MCI_QUIRK_BYPASS_SMU) ?
				true : false;
#if defined(CONFIG_MMC_DW_FMP_DM_CRYPT) || defined(CONFIG_MMC_DW_FMP_ECRYPT_FS)
	is_smu = is_smu || ((host->pdata->quirks & DW_MCI_QUIRK_USE_SMU) ?
				true : false);
#endif
	dev_err(host->dev, ": CLKSEL:	0x%08x\n", mci_readl(host, CLKSEL));
	if (is_smu) {
		dev_err(host->dev, ": EMMCP_BASE:	0x%08x\n",
			mci_readl(host, EMMCP_BASE));
		dev_err(host->dev, ": MPSECURITY:	0x%08x\n",
			mci_readl(host, MPSECURITY));
		dev_err(host->dev, ": MPSTAT:	0x%08x\n",
			mci_readl(host, MPSTAT));
		for (i = 0; i < 8; i++) {
			dev_err(host->dev, ": MPSBEGIN%d:	0x%08x\n", i,
				__raw_readl(host->regs + SDMMC_MPSBEGIN0 + (0x10 * i)));
			dev_err(host->dev, ": MPSEND%d:	0x%08x\n", i,
				__raw_readl(host->regs + SDMMC_MPSEND0 + (0x10 * i)));
			dev_err(host->dev, ": MPSCTRL%d:	0x%08x\n", i,
				__raw_readl(host->regs + SDMMC_MPSCTRL0 + (0x10 * i)));
#if defined(CONFIG_SOC_EXYNOS7420)
			dev_err(host->dev, ": MPSLUN%d:	0x%08x\n", i,
				__raw_readl(host->regs + SDMMC_MPSLUN0 + (0x10 * i)));
#endif
		}
	}
	dev_err(host->dev, ": DDR200_RDDQS_EN:	0x%08x\n",
		mci_readl(host, DDR200_RDDQS_EN));
	dev_err(host->dev, ": DDR200_ASYNC_FIFO_CTRL:	0x%08x\n",
		mci_readl(host, DDR200_ASYNC_FIFO_CTRL));
	dev_err(host->dev, ": DDR200_DLINE_CTRL:	0x%08x\n",
		mci_readl(host, DDR200_DLINE_CTRL));
}

static void dw_mci_exynos_prepare_command(struct dw_mci *host, u32 *cmdr)
{
	/*
	 * Exynos4412 and Exynos5250 extends the use of CMD register with the
	 * use of bit 29 (which is reserved on standard MSHC controllers) for
	 * optionally bypassing the HOLD register for command and data. The
	 * HOLD register should be bypassed in case there is no phase shift
	 * applied on CMD/DATA that is sent to the card.
	 */
	if (SDMMC_CLKSEL_GET_DRV_WD3(mci_readl(host, CLKSEL)))
		*cmdr |= SDMMC_CMD_USE_HOLD_REG;
}

/**
 * dw_mci_exynos_set_bus_hz - try to set the ciu clock; update host->bus_hz.
 *
 * @want_bus_hz: bus_hz the eMMC device wants
 */
static void dw_mci_exynos_set_bus_hz(struct dw_mci *host, u32 want_bus_hz,
					unsigned char timing)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 ciu_rate = clk_get_rate(host->ciu_clk);
	u32 ciu_div;
	u32 tmp_reg;
	int clkerr = 0;
	struct clk *adiv = clk_get(host->dev, "dout_mmc_a");
	struct clk *bdiv = clk_get(host->dev, "dout_mmc_b");

	if (timing == MMC_TIMING_MMC_HS200_DDR)
		tmp_reg = priv->ddr200_timing;
	else if (timing == MMC_TIMING_MMC_HS200_DDR_ULP)
		tmp_reg = priv->ddr200_ulp_timing;
	else if (timing == MMC_TIMING_UHS_DDR50)
		tmp_reg = priv->ddr_timing;
	else
		tmp_reg = priv->sdr_timing;
	ciu_div = SDMMC_CLKSEL_GET_DIVRATIO(tmp_reg);

	/* Try to set the upstream clock rate */
	if (ciu_rate != (want_bus_hz * ciu_div)) {
		ciu_rate = want_bus_hz * ciu_div;

		/*
		 * Some SoCs have more than one clock divider
		 * because expected frequency can't be generated
		 * for too high frequency of muxed PLL.
		 */
		clkerr = clk_set_rate(adiv, ciu_rate);
		if (clkerr)
			dev_warn(host->dev, "Couldn't set rate to %u\n",
				 ciu_rate);

		if (!IS_ERR(bdiv)) {
			clkerr = clk_set_rate(bdiv, ciu_rate);
			if (clkerr)
				dev_warn(host->dev, "Couldn't set rate to %u\n",
					 ciu_rate);
			ciu_rate = clk_get_rate(host->ciu_clk);
		}
	}

	host->bus_hz = ciu_rate / ciu_div;
}

static void dw_mci_exynos_set_ios(struct dw_mci *host, unsigned int tuning, struct mmc_ios *ios)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	struct dw_mci_board *pdata = host->pdata;
	u32 *clk_tbl = priv->ref_clk;
	u32 clksel, rddqs, dline;
	u32 cclkin;
	unsigned char timing = ios->timing;
	unsigned char timing_org = timing;

	if (timing == MMC_TIMING_MMC_HS200_DDR_ES)
		timing = MMC_TIMING_MMC_HS200_DDR;

	if (timing > MMC_TIMING_MMC_HS200_DDR) {
		pr_err("%s: timing(%d): not suppored\n", __func__, timing);
		return;
	}

	if ((host->pdata->quirks & DW_MCI_QUIRK_ENABLE_ULP) &&
			(timing == MMC_TIMING_MMC_HS200_DDR))
		timing = MMC_TIMING_MMC_HS200_DDR_ULP;

	cclkin = clk_tbl[timing];
	rddqs = DWMCI_DDR200_RDDQS_EN_DEF;
	dline = DWMCI_DDR200_DLINE_CTRL_DEF;
	clksel = mci_readl(host, CLKSEL);
	clksel &= ~(BIT(19));

	if (host->bus_hz != cclkin) {
		dw_mci_exynos_set_bus_hz(host, cclkin, timing);
		host->bus_hz = cclkin;
		host->current_speed = 0;
	}

	host->pdata->io_mode = timing;

	if (timing == MMC_TIMING_MMC_HS200_DDR) {
		clksel = ((priv->ddr200_timing & 0xfffffff8) | pdata->clk_smpl);
		if (pdata->is_fine_tuned)
			clksel |= BIT(6);

		if (!tuning) {
			rddqs |= (DWMCI_RDDQS_EN | DWMCI_AXI_NON_BLOCKING_WRITE);
			if (timing_org == MMC_TIMING_MMC_HS200_DDR_ES)
				rddqs |= DWMCI_RESP_RCLK_MODE;
			if (priv->delay_line)
				dline = DWMCI_FIFO_CLK_DELAY_CTRL(0x2) |
				DWMCI_RD_DQS_DELAY_CTRL(priv->delay_line);
			else
				dline = DWMCI_FIFO_CLK_DELAY_CTRL(0x2) |
				DWMCI_RD_DQS_DELAY_CTRL(90);
			host->quirks &= ~DW_MCI_QUIRK_NO_DETECT_EBIT;
		}
	} else if (timing == MMC_TIMING_MMC_HS200_DDR_ULP) {
		clksel = ((priv->ddr200_ulp_timing & 0xfffffff8) | pdata->clk_smpl);
		if (pdata->is_fine_tuned)
			clksel |= BIT(6);

		if (!tuning) {
			rddqs &= ~((0xFF << 16) | (0xFF << 8));
			rddqs |= (DWMCI_TXDT_CRC_TIMER_SET(priv->ddr200_tx_t_fastlimit,
						priv->ddr200_tx_t_initval) |
					DWMCI_RDDQS_EN | DWMCI_AXI_NON_BLOCKING_WRITE);
			if (timing_org == MMC_TIMING_MMC_HS200_DDR_ES)
				rddqs |= DWMCI_RESP_RCLK_MODE;
			if (priv->delay_line || priv->tx_delay_line)
				dline = DWMCI_WD_DQS_DELAY_CTRL(priv->tx_delay_line) |
				DWMCI_FIFO_CLK_DELAY_CTRL(0x2) |
				DWMCI_RD_DQS_DELAY_CTRL(priv->delay_line);
			else
				dline = DWMCI_FIFO_CLK_DELAY_CTRL(0x2) |
				DWMCI_RD_DQS_DELAY_CTRL(90);
			host->quirks &= ~DW_MCI_QUIRK_NO_DETECT_EBIT;
			clksel |= BIT(19);
		}
	} else if (timing == MMC_TIMING_MMC_HS200 ||
			timing == MMC_TIMING_UHS_SDR104) {
		clksel = (clksel & 0xfff8ffff) | (priv->selclk_drv << 16);
	} else if (timing == MMC_TIMING_UHS_SDR50) {
		clksel = (clksel & 0xfff8ffff) | (priv->selclk_drv << 16);
	} else if (timing == MMC_TIMING_UHS_DDR50) {
		clksel = priv->ddr_timing;
	} else if (timing == MMC_TIMING_MMC_HS || timing == MMC_TIMING_SD_HS) {
		clksel = priv->sdr_hs_timing;
	} else {
		clksel = priv->sdr_timing;
	}

	mci_writel(host, CLKSEL, clksel);
	mci_writel(host, DDR200_RDDQS_EN, rddqs);
	mci_writel(host, DDR200_DLINE_CTRL, dline);
	if (timing == MMC_TIMING_MMC_HS200_DDR)
		mci_writel(host, DDR200_ASYNC_FIFO_CTRL, 0x1);
}

#ifndef MHZ
#define MHZ (1000*1000)
#endif
static int dw_mci_exynos_parse_dt(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 timing[4];
	u32 div = 0;

	struct device_node *np = host->dev->of_node;
	int ref_clk_size;
	u32 *ref_clk;
	u32 *ciu_clkin_values = NULL;
	int idx_ref;
	int ret = 0;
	int id = 0;

	/*
	 * Reference clock values for speed mode change are extracted from DT.
	 * Now, a number of reference clock values should be defined in DT
	 * We will be able to get reference clock values by using just one function
	 * when DT support to get tables with many columns.
	*/
	if (of_property_read_u32(np, "num-ref-clks", &ref_clk_size)) {
		dev_err(host->dev, "Getting a number of referece clock failed\n");
		ret = -ENODEV;
		goto err_ref_clk;
	}

	ref_clk = devm_kzalloc(host->dev, ref_clk_size * sizeof(*ref_clk),
					GFP_KERNEL);
	if (!ref_clk) {
		dev_err(host->dev, "Mem alloc failed for reference clock table\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}

	ciu_clkin_values = devm_kzalloc(host->dev,
			ref_clk_size * sizeof(*ciu_clkin_values), GFP_KERNEL);

	if (!ciu_clkin_values) {
		dev_err(host->dev, "Mem alloc failed for temporary clock values\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}
	if (of_property_read_u32_array(np, "ciu_clkin", ciu_clkin_values, ref_clk_size)) {
		dev_err(host->dev, "Getting ciu_clkin values faild\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}

	for (idx_ref = 0; idx_ref < ref_clk_size; idx_ref++, ref_clk++, ciu_clkin_values++) {
		if (*ciu_clkin_values > MHZ)
			*(ref_clk) = (*ciu_clkin_values);
		else
			*(ref_clk) = (*ciu_clkin_values) * MHZ;
	}

	ref_clk -= ref_clk_size;
	ciu_clkin_values -= ref_clk_size;
	priv->ref_clk = ref_clk;

	of_property_read_u32(np, "samsung,dw-mshc-ciu-div", &div);
	priv->ciu_div = div;

	if (of_get_property(np, "cd-gpio", NULL))
		priv->cd_gpio = of_get_named_gpio(np, "cd-gpio", 0);
	else
		priv->cd_gpio = -1;

	if (of_property_read_u32(np, "selclk_drv", &priv->selclk_drv))
		priv->selclk_drv = 3;

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-sdr-timing", timing, 4);
	if (ret)
		return ret;

	priv->sdr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-sdr-hs-timing", timing, 4);
	if (ret)
		priv->sdr_hs_timing = priv->sdr_timing;
	else
		priv->sdr_hs_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-ddr-timing", timing, 4);
	if (ret)
		goto err_ref_clk;

	priv->ddr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);

	priv->drv_str_pin = of_get_property(np, "clk_pin", NULL);
	priv->drv_str_addr = of_get_property(np, "clk_addr", NULL);
	of_property_read_u32(np, "ignore-phase", &priv->ignore_phase);
	of_property_read_u32(np, "clk_val", &priv->drv_str_val);
	priv->drv_str_base_val = priv->drv_str_val;
	of_property_read_u32(np, "clk_str_num", &priv->drv_str_num);
	dev_info(host->dev, "Clock pin control Pin:%s Addr:%s Val:%d Num:%d",
		priv->drv_str_pin, priv->drv_str_addr,
		priv->drv_str_val, priv->drv_str_num);

	/*
	 * Exynos-dependent generic control flag
	 */
	if (of_find_property(np, "use-fine-tuning", NULL))
		priv->ctrl_flag |= DW_MMC_EXYNOS_USE_FINE_TUNING;
	if (of_find_property(np, "bypass-for-allpass", NULL))
		priv->ctrl_flag |= DW_MMC_EXYNOS_BYPASS_FOR_ALL_PASS;
	if (of_find_property(np, "use-enable-shift", NULL))
		priv->ctrl_flag |= DW_MMC_EXYNOS_ENABLE_SHIFT;

	id = of_alias_get_id(host->dev->of_node, "mshc");
	switch (id) {
	/* dwmmc0 : eMMC    */
	case 0:
		ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-hs200-timing", timing, 4);
		if (ret)
			goto err_ref_clk;
		priv->hs200_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);

		ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-ddr200-timing", timing, 4);
		if (ret)
			goto err_ref_clk;

		priv->ddr200_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);

		ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-ddr200-ulp-timing", timing, 4);
		if (!ret)
			priv->ddr200_ulp_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);
		else
			ret = 0;

		/* Rx Delay Line */
		of_property_read_u32(np,
			"samsung,dw-mshc-ddr200-delay-line", &priv->delay_line);

		/* Tx Delay Line */
		of_property_read_u32(np,
			"samsung,dw-mshc-ddr200-tx-delay-line", &priv->tx_delay_line);

		/* The fast RXCRC packet arrival time */
		of_property_read_u32(np,
			"samsung,dw-mshc-txdt-crc-timer-fastlimit", &priv->ddr200_tx_t_fastlimit);

		/* Initial value of the timeout down counter for RXCRC packet */
		of_property_read_u32(np,
			"samsung,dw-mshc-txdt-crc-timer-initval", &priv->ddr200_tx_t_initval);
		break;
	/* dwmmc1 : SDIO    */
	case 1:
		break;
	/* dwmmc2 : SD Card */
	case 2:
		break;
	default:
		ret = -ENODEV;
	}

err_ref_clk:

	return ret;
}

static void dw_mci_set_quirk_endbit(struct dw_mci *host, s8 mid)
{
	u32 clksel, phase;
	u32 shift;

	clksel = mci_readl(host, CLKSEL);
	phase = (((clksel >> 24) & 0x7) + 1) << 1;
	shift = 360 / phase;

	if (host->verid < DW_MMC_260A && (shift * mid) % 360 >= 225)
		host->quirks |= DW_MCI_QUIRK_NO_DETECT_EBIT;
	else
		host->quirks &= ~DW_MCI_QUIRK_NO_DETECT_EBIT;
}

static void dw_mci_exynos_set_enable_shift(struct dw_mci *host, u32 sample, bool fine_tune)
{
	u32 i, j, en_shift, en_shift_phase[3][4] = {{0, 0, 1, 0},
						{1, 2, 3, 3},
						{2, 4, 5, 5}};

	en_shift = mci_readl(host, DDR200_ENABLE_SHIFT)
		& ~(DWMCI_ENABLE_SHIFT_MASK);

	for (i = 0; i < 3; i++) {
		for (j = 1; j < 4; j++) {
			if (sample == en_shift_phase[i][j]) {
				en_shift |= DWMCI_ENABLE_SHIFT(en_shift_phase[i][0]);
				break;
			}
		}
	}
	if ((en_shift < 2) && fine_tune)
		en_shift += 1;
	mci_writel(host, DDR200_ENABLE_SHIFT, en_shift);
}
static u8 dw_mci_tuning_sampling(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 clksel, i;
	u8 sample;

	clksel = mci_readl(host, CLKSEL);
	sample = (clksel + 1) & 0x7;

	if (priv->ignore_phase) {
		for (i = 0; i < 8; i++) {
			if (priv->ignore_phase & (0x1 << sample))
				sample = (sample + 1) & 0x7;
			else
				break;
		}
	}
	clksel = (clksel & 0xfffffff8) | sample;
	mci_writel(host, CLKSEL, clksel);
	if (priv->ctrl_flag & DW_MMC_EXYNOS_ENABLE_SHIFT)
		dw_mci_exynos_set_enable_shift(host, sample, false);

	return sample;
}

/* initialize the clock sample to given value */
static void dw_mci_exynos_set_sample(struct dw_mci *host, u32 sample, bool tuning)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 clksel;

	clksel = mci_readl(host, CLKSEL);
	clksel = (clksel & ~0x7) | SDMMC_CLKSEL_CCLK_SAMPLE(sample);
	mci_writel(host, CLKSEL, clksel);
	if (priv->ctrl_flag & DW_MMC_EXYNOS_ENABLE_SHIFT)
		dw_mci_exynos_set_enable_shift(host, sample, false);
	if (!tuning)
		dw_mci_set_quirk_endbit(host, clksel);
}

static void dw_mci_set_fine_tuning_bit(struct dw_mci *host,
		bool is_fine_tuning)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 clksel, sample;

	clksel = mci_readl(host, CLKSEL);
	clksel = (clksel & ~BIT(6));
	sample = (clksel & 0x7);

	if (is_fine_tuning) {
		host->pdata->is_fine_tuned = true;
		clksel |= BIT(6);
	} else
		host->pdata->is_fine_tuned = false;
	mci_writel(host, CLKSEL, clksel);
	if (priv->ctrl_flag & DW_MMC_EXYNOS_ENABLE_SHIFT) {
		if (((sample % 2) == 1) && is_fine_tuning && sample != 0x7)
			dw_mci_exynos_set_enable_shift(host, sample, true);
		else
			dw_mci_exynos_set_enable_shift(host, sample, false);
	}
}

/* read current clock sample offset */
static u32 dw_mci_exynos_get_sample(struct dw_mci *host)
{
	u32 clksel = mci_readl(host, CLKSEL);
	return SDMMC_CLKSEL_CCLK_SAMPLE(clksel);
}

static void exynos_dwmci_restore_drv_st(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	pin_config_set(priv->drv_str_addr, priv->drv_str_pin,
		PINCFG_PACK(PINCFG_TYPE_DRV, priv->drv_str_base_val));
}

#define DRV_STR_LV1 0x0
#define DRV_STR_LV2 0x1
#define DRV_STR_LV3 0x2
#define DRV_STR_LV4 0x3
#define DRV_STR_LV5 0x4
#define DRV_STR_LV6 0x5

static void exynos_dwmci_tuning_drv_st(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 drv_str[] = {DRV_STR_LV1,   /* LV1 -> LV2 */
		DRV_STR_LV2,
		DRV_STR_LV3,
		DRV_STR_LV4,
		DRV_STR_LV5,
		DRV_STR_LV6
	};

	if (priv->drv_str_val == DRV_STR_LV1)
		priv->drv_str_val = priv->drv_str_num;

	priv->drv_str_val--;

	dev_info(host->dev, "Clock GPIO Drive Strength Value: 0x%x\n",
			priv->drv_str_val);

	pin_config_set(priv->drv_str_addr, priv->drv_str_pin,
			PINCFG_PACK(PINCFG_TYPE_DRV, drv_str[priv->drv_str_val]));
}

static s8 exynos_dwmci_extra_tuning(u8 map)
{
	s8 sel = -1;

	if ((map & 0x03) == 0x03)
		sel = 0;
	else if ((map & 0x0c) == 0x0c)
		sel = 3;
	else if ((map & 0x06) == 0x06)
		sel = 2;

	return sel;
}

/*
 * After testing all (8) possible clock sample values and using one bit for
 * each value that works, return the "middle" bit position of any sequential
 * bits.
 */
static int find_median_of_bits(struct dw_mci *host, unsigned int map, bool force)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	unsigned int i, testbits, orig_bits;
	u8 divratio;
	int sel = -1;

	/* replicate the map so "arithimetic shift right" shifts in
	 * the same bits "again". e.g. portable "Rotate Right" bit operation.
	 */
	if (map == 0xFF && force == false)
		return sel;

	testbits = orig_bits = map | (map << 8);
	divratio = (mci_readl(host, CLKSEL) >> 24) & 0x7;

	if (divratio == 1) {
		if (!(priv->ctrl_flag & DW_MMC_EXYNOS_ENABLE_SHIFT))
			testbits = orig_bits = map & (map >> 4);
		dev_info(host->dev, "divratio: %d map: 0x %08x\n",
					divratio, testbits);
#define THREEBITS 0x7
		/* Middle is bit 1. */
		for (i = 1; i < (8 + 1); i++, testbits >>= 1) {
			if ((testbits & THREEBITS) == THREEBITS)
				return SDMMC_CLKSEL_CCLK_SAMPLE(i);
		}

		/* Select one of two. */
		if (host->pdata->extra_tuning) {
			testbits = orig_bits;
			sel = exynos_dwmci_extra_tuning(testbits);
			dev_info(host->dev, "Extra Tuning %d\n", sel);
			if (sel >= 0)
				return SDMMC_CLKSEL_CCLK_SAMPLE(sel);
		}
	} else {
#define SEVENBITS 0x7f
		/* Middle is bit 3 */
		for (i = 3; i < (8 + 3); i++, testbits >>= 1) {
			if ((testbits & SEVENBITS) == SEVENBITS)
				return SDMMC_CLKSEL_CCLK_SAMPLE(i);
		}

#define FIVEBITS 0x1f
		/* Middle is bit 2. */
		testbits = orig_bits;
		for (i = 2; i < (8 + 2); i++, testbits >>= 1) {
			if ((testbits & FIVEBITS) == FIVEBITS)
				return SDMMC_CLKSEL_CCLK_SAMPLE(i);
		}

#define THREEBITS 0x7
		/* Middle is bit 1. */
		testbits = orig_bits;
		for (i = 1; i < (8 + 1); i++, testbits >>= 1) {
			if ((testbits & THREEBITS) == THREEBITS)
				return SDMMC_CLKSEL_CCLK_SAMPLE(i);
		}
	}

	return sel;
}

static int __find_median_of_16bits(u32 orig_bits, u16 mask, u8 startbit)
{
	u32 i, testbits;

	testbits = orig_bits;
	for (i = startbit; i < (16 + startbit); i++, testbits >>= 1)
		if ((testbits & mask) == mask)
			return SDMMC_CLKSEL_CCLK_FINE_SAMPLE(i);
	return -1;
}

#define NUM_OF_MASK	7
static int find_median_of_16bits(struct dw_mci *host, unsigned int map, bool force)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 orig_bits;
	u8 i, divratio;
	int sel = -1;
	u16 mask[NUM_OF_MASK] = {0x1fff, 0x7ff, 0x1ff, 0x7f, 0x1f, 0xf, 0x7};

	/* replicate the map so "arithimetic shift right" shifts in
	 * the same bits "again". e.g. portable "Rotate Right" bit operation.
	 */
	if (map == 0xFFFF && force == false)
		return sel;

	divratio = (mci_readl(host, CLKSEL) >> 24) & 0x7;
	dev_info(host->dev, "divratio: %d map: 0x %08x\n", divratio, map);

	orig_bits = map | (map << 16);

	if (divratio == 1) {
		if (!(priv->ctrl_flag & DW_MMC_EXYNOS_ENABLE_SHIFT))
			orig_bits = orig_bits & (orig_bits >> 8);
		i = 3;
	}

	for (i = 0; i < NUM_OF_MASK; i++) {
		sel = __find_median_of_16bits(orig_bits, mask[i], NUM_OF_MASK-i);
		if (-1 != sel)
			break;
	}

	return sel;
}

/*
 * Test all 8 possible "Clock in" Sample timings.
 * Create a bitmap of which CLock sample values work and find the "median"
 * value. Apply it and remember that we found the best value.
 */
static int dw_mci_exynos_execute_tuning(struct dw_mci *host, u32 opcode)
{
	struct dw_mci_slot *slot = host->cur_slot;
	struct dw_mci_exynos_priv_data *priv = host->priv;
	struct mmc_host *mmc = slot->mmc;
	struct mmc_ios *ios = &(mmc->ios);
	unsigned int tuning_loop = MAX_TUNING_LOOP;
	unsigned int drv_str_retries = MAX_TUNING_RETRIES - 1;
	const u8 *tuning_blk_pattern;	/* data pattern we expect */
	bool tuned = 0;
	int ret = 0;
	u8 *tuning_blk;			/* data read from device */
	unsigned int blksz;

	unsigned int sample_good = 0;	/* bit map of clock sample (0-7) */
	u32 test_sample = -1;
	u32 orig_sample;
	int best_sample = 0, best_sample_ori = 0;
	u8 pass_index;
	bool en_fine_tuning = false;
	bool is_fine_tuning = false;
	unsigned int abnormal_result = 0xFF;
	u8 all_pass_count = 0;
	bool bypass = false;

	if (priv->ctrl_flag & DW_MMC_EXYNOS_USE_FINE_TUNING) {
		en_fine_tuning = true;
		abnormal_result = 0xFFFF;
	}

	if (opcode == MMC_SEND_TUNING_BLOCK_HS200) {
		if (ios->bus_width == MMC_BUS_WIDTH_8) {
			tuning_blk_pattern = tuning_blk_pattern_8bit;
			blksz = sizeof(tuning_blk_pattern_8bit);
		} else if (ios->bus_width == MMC_BUS_WIDTH_4) {
			tuning_blk_pattern = tuning_blk_pattern_4bit;
			blksz = sizeof(tuning_blk_pattern_4bit);
		} else {
			return -EINVAL;
		}
	} else if (opcode == MMC_SEND_TUNING_BLOCK) {
		tuning_blk_pattern = tuning_blk_pattern_4bit;
		blksz = sizeof(tuning_blk_pattern_4bit);
	} else {
		dev_err(mmc_classdev(host->cur_slot->mmc),
			"Undefined command(%d) for tuning\n",
			opcode);
		return -EINVAL;
	}

	/* Short circuit: don't tune again if we already did. */
	if (host->pdata->tuned) {
		host->drv_data->misc_control(host, CTRL_RESTORE_CLKSEL, NULL);
		mci_writel(host, CDTHRCTL, host->cd_rd_thr << 16 | 1);
		dev_info(host->dev, "EN_SHIFT 0x %08x CLKSEL 0x %08x\n",
			mci_readl(host, DDR200_ENABLE_SHIFT),
			mci_readl(host, CLKSEL));
		return 0;
	}

	tuning_blk = kmalloc(2 * blksz, GFP_KERNEL);
	if (!tuning_blk)
		return -ENOMEM;

	test_sample = orig_sample = dw_mci_exynos_get_sample(host);
	host->cd_rd_thr = 512;
	mci_writel(host, CDTHRCTL, host->cd_rd_thr << 16 | 1);

	/* Restore Base Drive Strength */
	if (priv->drv_str_pin) {
		exynos_dwmci_restore_drv_st(host);
		priv->drv_str_val = priv->drv_str_base_val;
	}

	/*
	 * eMMC 4.5 spec section 6.6.7.1 says the device is guaranteed to
	 * complete 40 iteration of CMD21 in 150ms. So this shouldn't take
	 * longer than about 30ms or so....at least assuming most values
	 * work and don't time out.
	 */

	if (host->pdata->io_mode == MMC_TIMING_MMC_HS200_DDR)
		host->quirks |= DW_MCI_QUIRK_NO_DETECT_EBIT;

	do {
		struct mmc_request mrq;
		struct mmc_command cmd;
		struct mmc_command stop;
		struct mmc_data data;
		struct scatterlist sg;

		if (!tuning_loop)
			break;

		memset(&cmd, 0, sizeof(cmd));
		cmd.opcode = opcode;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		cmd.error = 0;
		cmd.cmd_timeout_ms = 10; /* 2x * (150ms/40 + setup overhead) */

		memset(&stop, 0, sizeof(stop));
		stop.opcode = MMC_STOP_TRANSMISSION;
		stop.arg = 0;
		stop.flags = MMC_RSP_R1B | MMC_CMD_AC;
		stop.error = 0;

		memset(&data, 0, sizeof(data));
		data.blksz = blksz;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.sg = &sg;
		data.sg_len = 1;
		data.error = 0;

		memset(tuning_blk, ~0U, blksz);
		sg_init_one(&sg, tuning_blk, blksz);

		memset(&mrq, 0, sizeof(mrq));
		mrq.cmd = &cmd;
		mrq.stop = &stop;
		mrq.data = &data;
		host->mrq_cmd = &mrq;

		/*
		 * DDR200 tuning Sequence with fine tuning setup
		 *
		 * 0. phase 0 (0 degree) + no fine tuning setup
		 * - pass_index = 0
		 * 1. phase 0 + fine tuning setup
		 * - pass_index = 1
		 * 2. phase 1 (90 degree) + no fine tuning setup
		 * - pass_index = 2
		 * ..
		 * 15. phase 7 + fine tuning setup
		 * - pass_index = 15
		 *
		 */
		if (en_fine_tuning)
			dw_mci_set_fine_tuning_bit(host, is_fine_tuning);
		else
			test_sample = dw_mci_tuning_sampling(host);

		dw_mci_set_timeout(host);
		dev_info(host->dev, "EN_SHIFT 0x %08x CLKSEL 0x %08x\n",
			mci_readl(host, DDR200_ENABLE_SHIFT),
			mci_readl(host, CLKSEL));
		mmc_wait_for_req(mmc, &mrq);

		pass_index = (u8)test_sample;
		if (en_fine_tuning) {
			pass_index *= 2;
			if (is_fine_tuning) {
				pass_index++;
				test_sample = dw_mci_tuning_sampling(host);
			}
			is_fine_tuning = !is_fine_tuning;
		}

		if (!cmd.error && !data.error) {
			/*
			 * Verify the "tuning block" arrived (to host) intact.
			 * If yes, remember this sample value works.
			 */
			if (host->use_dma == 1) {
				sample_good |= (1 << pass_index);
			} else {
				if (!memcmp(tuning_blk_pattern, tuning_blk, blksz))
					sample_good |= (1 << pass_index);
			}
		} else {
			dev_info(&mmc->class_dev,
				"Tuning error: cmd.error:%d, data.error:%d\n",
				cmd.error, data.error);
		}

		if (orig_sample == test_sample && !is_fine_tuning) {

			/*
			 * Get at middle clock sample values.
			 */
			if (priv->ctrl_flag & DW_MMC_EXYNOS_BYPASS_FOR_ALL_PASS)
				bypass = (all_pass_count >= 2) ? true : false;
			if (en_fine_tuning)
				best_sample = find_median_of_16bits(host,
						sample_good, bypass);
			else
				best_sample = find_median_of_bits(host,
						sample_good, bypass);

			if (sample_good == abnormal_result)
				all_pass_count++;
			if (bypass) {
				dev_info(host->dev, "Bypassed for all pass at 3 times\n");
				if (en_fine_tuning) {
					best_sample = 4;
					sample_good = 0x7FFF;
				} else {
					best_sample = 4;
					sample_good = 0x7F;
				}
			}

			dev_info(host->dev, "sample_good: 0x %02x best_sample: 0x %02x\n",
					sample_good, best_sample);

			if (best_sample >= 0) {
				if (sample_good != abnormal_result) {
					tuned = true;
					break;
				}
			}

			if (drv_str_retries) {
				drv_str_retries--;
				if (priv->drv_str_pin)
					exynos_dwmci_tuning_drv_st(host);
				sample_good = 0;
			} else
				break;
		}
		tuning_loop--;
	} while (!tuned);

	if (priv->drv_str_pin && (priv->drv_str_val == DRV_STR_LV1))
		exynos_dwmci_restore_drv_st(host);

	if (bypass)
		exynos_dwmci_restore_drv_st(host);

	/*
	 * To set sample value with mid, the value should be divided by 2,
	 * because mid represents index in pass map extended.(8 -> 16 bits)
	 * And that mid is odd number, means the selected case includes
	 * using fine tuning.
	 */

	best_sample_ori = best_sample;
	best_sample /= 2;

	if (host->pdata->io_mode == MMC_TIMING_MMC_HS200_DDR)
		host->quirks &= ~DW_MCI_QUIRK_NO_DETECT_EBIT;

	if (tuned) {
		host->pdata->clk_smpl = best_sample;
		if (host->pdata->only_once_tune)
			host->pdata->tuned = true;
		dw_mci_exynos_set_sample(host, best_sample, false);
		if (en_fine_tuning) {
			if (best_sample_ori % 2)
				dw_mci_set_fine_tuning_bit(host, true);
			else
				dw_mci_set_fine_tuning_bit(host, false);
		}
	} else {
		/* Failed. Just restore and return error */
		dev_err(host->dev, "tuning err\n");
		mci_writel(host, CDTHRCTL, 0 << 16 | 0);
		dw_mci_exynos_set_sample(host, orig_sample, false);
		ret = -EIO;
	}

	kfree(tuning_blk);

	return ret;
}

static int dw_mci_exynos_turn_on_2_8v(struct dw_mci *host)
{
	int ret = 0;
	struct device_node *np = host->dev->of_node;
	int dev_pwr = 0;

	/*
	 * This GPIOs is for eMMC device 2.8 volt supply control
	*/
	if (of_get_property(np, "gpios", NULL) != NULL) {
		dev_pwr = of_get_gpio(np, 0);
		if (dev_pwr < 0)
			ret = -ENODEV;
		else if (gpio_request(dev_pwr, "dev-pwr"))
			ret = -ENODEV;
		else
			gpio_direction_output(dev_pwr, 1);
	}

	return ret;
}

static int dw_mci_exynos_request_ext_irq(struct dw_mci *host,
					irq_handler_t func)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	int ext_cd_irq = 0;

	if (gpio_is_valid(priv->cd_gpio) &&
			!gpio_request(priv->cd_gpio, "DWMCI_EXT_CD")) {
		ext_cd_irq = gpio_to_irq(priv->cd_gpio);
		if (ext_cd_irq &&
				devm_request_irq(host->dev, ext_cd_irq, func,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT,
					"tflash_det", host) == 0) {
			dev_warn(host->dev, "success to request irq for card detect.\n");
			enable_irq_wake(ext_cd_irq);
		} else
			dev_warn(host->dev, "cannot request irq for card detect.\n");

	}

	return 0;
}

/* reverse sd present detect due to TF socket irq */
static int dw_mci_exynos_check_cd(struct dw_mci *host)
{
	int ret = -1;
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (gpio_is_valid(priv->cd_gpio))
		ret = gpio_get_value(priv->cd_gpio);

	if (priv->ctrl_windows & DW_MCI_TOGGLE_BLOCK_CD)
		ret = 0;

	return ret;
}

static int dw_mci_exynos_check_socket(struct dw_mci *host)
{
	int ret = -1;
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (gpio_is_valid(priv->cd_gpio))
		ret = gpio_get_value(priv->cd_gpio);

	return ret;
}

static int dw_mci_exynos_set_def_caps(struct dw_mci *host)
{
	int id;
	int ret;

	id = of_alias_get_id(host->dev->of_node, "mshc");
	switch (id) {
	/* dwmmc0 : eMMC    */
	case 0:
		ret = EXYNOS_DEF_MMC_0_CAPS;
		break;
	case 1:
		ret = EXYNOS_DEF_MMC_1_CAPS;
		break;
	case 2:
		ret = EXYNOS_DEF_MMC_2_CAPS;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static int dw_mci_exynos_init_clock(struct dw_mci *host)
{
#if defined(CONFIG_SOC_EXYNOS7420)
	int ret = 0;

	struct clk *dout_sclk_mmc0;
	struct clk *biu;
	struct clk *ciu;

	ciu = devm_clk_get(host->dev, "gate_ciu");
	biu = devm_clk_get(host->dev, "biu");
	dout_sclk_mmc0 = devm_clk_get(host->dev, "dout_mmc_a");

	if (IS_ERR(dout_sclk_mmc0))
		dev_info(host->dev, "Failed to get dout_sclk_mmc0\n");

	dev_info(host->dev, "CLOCK dout_sclk_mmc0 %ld\n", clk_get_rate(dout_sclk_mmc0));
	ret = clk_set_rate(dout_sclk_mmc0, 800 * 1000000);
	if (ret)
		dev_info(host->dev, "Failed to set aclk/sclk rate\n");

	dev_info(host->dev, "CLOCK dout_sclk_mmc0 %ld\n\n", clk_get_rate(dout_sclk_mmc0));
	dev_info(host->dev, "CLOCK biu %ld\n\n", clk_get_rate(biu));
	dev_info(host->dev, "CLOCK ciu %ld\n\n", clk_get_rate(ciu));

	dev_info(host->dev, "MMC init clock completed\n");

#endif
	return 0;
}

static int dw_mci_exynos_toggle_windows(struct dw_mci *host, u32 flag)
{
	int ret = 0;
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (!(flag & DW_MCI_TOGGLE_MASK))
		ret = -1;
	else if (flag & DW_MCI_TOGGLE_ENABLE)
		priv->ctrl_windows |= (flag & DW_MCI_TOGGLE_MASK);
	else
		priv->ctrl_windows &= ~(flag & DW_MCI_TOGGLE_MASK);

	return ret;
}

static int dw_mci_exynos_misc_control(struct dw_mci *host,
		enum dw_mci_misc_control control, void *priv)
{
	int ret = 0;

	switch (control) {
	case CTRL_RESTORE_CLKSEL:
		dw_mci_exynos_set_sample(host, host->pdata->clk_smpl, false);
		dw_mci_set_fine_tuning_bit(host, host->pdata->is_fine_tuned);
		break;
	case CTRL_TURN_ON_2_8V:
		ret = dw_mci_exynos_turn_on_2_8v(host);
		break;
	case CTRL_REQUEST_EXT_IRQ:
		ret = dw_mci_exynos_request_ext_irq(host, (irq_handler_t)priv);
		break;
	case CTRL_CHECK_CD:
		ret = dw_mci_exynos_check_cd(host);
		break;
	case CTRL_CHECK_SOCKET:
		ret = dw_mci_exynos_check_socket(host);
		break;
	case CTRL_SET_DEF_CAPS:
		ret = dw_mci_exynos_set_def_caps(host);
		break;
	case CTRL_TOGGLE_WINDOWS:
		ret = dw_mci_exynos_toggle_windows(host, *((u32*)priv));
		break;
	case CTRL_INIT_CLOCK:
		ret = dw_mci_exynos_init_clock(host);
		break;
	default:
		dev_err(host->dev, "dw_mmc exynos: wrong case\n");
		return -ENODEV;
	}

	return ret;
}

static const struct dw_mci_drv_data exynos_drv_data = {
	.init			= dw_mci_exynos_priv_init,
	.setup_clock		= dw_mci_exynos_setup_clock,
	.prepare_command	= dw_mci_exynos_prepare_command,
	.register_dump		= dw_mci_exynos_register_dump,
	.set_ios		= dw_mci_exynos_set_ios,
	.parse_dt		= dw_mci_exynos_parse_dt,
	.cfg_smu		= dw_mci_exynos_cfg_smu,
	.execute_tuning		= dw_mci_exynos_execute_tuning,
	.misc_control		= dw_mci_exynos_misc_control,
#ifdef	CONFIG_CPU_IDLE
	.register_notifier	= dw_mci_exynos_register_notifier,
	.unregister_notifier	= dw_mci_exynos_unregister_notifier,
#endif
};

static const struct of_device_id dw_mci_exynos_match[] = {
	{ .compatible = "samsung,exynos4412-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5250-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5422-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5430-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5433-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos7420-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos7580-dw-mshc",
			.data = &exynos_drv_data, },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_exynos_match);

static u64 exynos_dwmci_dmamask = DMA_BIT_MASK(32);

static int dw_mci_exynos_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data;
	const struct of_device_id *match;

	match = of_match_node(dw_mci_exynos_match, pdev->dev.of_node);
	pdev->dev.dma_mask = &exynos_dwmci_dmamask;
	drv_data = match->data;
	return dw_mci_pltfm_register(pdev, drv_data);
}

static struct platform_driver dw_mci_exynos_pltfm_driver = {
	.probe		= dw_mci_exynos_probe,
	.remove		= __exit_p(dw_mci_pltfm_remove),
	.driver		= {
		.name		= "dwmmc_exynos",
		.of_match_table	= dw_mci_exynos_match,
		.pm		= &dw_mci_pltfm_pmops,
	},
};

module_platform_driver(dw_mci_exynos_pltfm_driver);

MODULE_DESCRIPTION("Samsung Specific DW-MSHC Driver Extension");
MODULE_AUTHOR("Thomas Abraham <thomas.ab@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dwmmc-exynos");
