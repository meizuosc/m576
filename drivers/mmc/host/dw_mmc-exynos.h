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

#ifndef _EXYNOS_DWMCI_H_
#define _EXYNOS_DWMCI_H_

#define NUM_PINS(x)			(x + 2)

#define EXYNOS_DEF_MMC_0_CAPS	(MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR | \
				MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23 | \
				MMC_CAP_ERASE)
//#define EXYNOS_DEF_MMC_1_CAPS	MMC_CAP_CMD23
#define EXYNOS_DEF_MMC_1_CAPS  (MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA/* | MMC_CAP_CMD23*/)
#define EXYNOS_DEF_MMC_2_CAPS	(MMC_CAP_CMD23 | MMC_CAP_UHS_SDR104 | \
				MMC_CAP_ERASE)

#define MAX_TUNING_RETRIES	6
#define MAX_TUNING_LOOP		(MAX_TUNING_RETRIES * 8 * 2)

/* Variations in Exynos specific dw-mshc controller */
enum dw_mci_exynos_type {
	DW_MCI_TYPE_EXYNOS4210,
	DW_MCI_TYPE_EXYNOS4412,
	DW_MCI_TYPE_EXYNOS5250,
	DW_MCI_TYPE_EXYNOS5422,
	DW_MCI_TYPE_EXYNOS5430,
};

/* Exynos implementation specific driver private data */
struct dw_mci_exynos_priv_data {
	u8			ciu_div;
	u32			sdr_timing;
	u32			sdr_hs_timing;
	u32			ddr_timing;
	u32			hs200_timing;
	u32			ddr200_timing;
	u32			ddr200_ulp_timing;
	u32			ddr200_tx_t_fastlimit;
	u32			ddr200_tx_t_initval;
	u32			*ref_clk;
	const char		*drv_str_pin;
	const char		*drv_str_addr;
	int			drv_str_val;
	u32			delay_line;
	u32			tx_delay_line;
	int			drv_str_base_val;
	u32			drv_str_num;
	int			cd_gpio;
	u32			caps;
	u32			ctrl_flag;
	u32			ctrl_windows;
	u32			ignore_phase;
	u32			selclk_drv;

#define DW_MMC_EXYNOS_USE_FINE_TUNING		BIT(0)
#define DW_MMC_EXYNOS_BYPASS_FOR_ALL_PASS	BIT(1)
#define DW_MMC_EXYNOS_ENABLE_SHIFT		BIT(2)
};

/*
 * Tunning patterns are from emmc4.5 spec section 6.6.7.1
 * Figure 27 (for 8-bit) and Figure 28 (for 4bit).
 */
static const u8 tuning_blk_pattern_4bit[] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

static const u8 tuning_blk_pattern_8bit[] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

extern int dw_mci_exynos_request_status(void);
extern void dw_mci_reg_dump(struct dw_mci *host);

/*****************/
/* SFR addresses */
/*****************/

#define SFR_OFFSET		0x0004

#if defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420) || \
	defined(CONFIG_SOC_EXYNOS7580)

#define SDMMC_DBADDRL 		 0x0088
#define SDMMC_DBADDRU		(SDMMC_DBADDRL + SFR_OFFSET)
#define SDMMC_IDSTS		(SDMMC_DBADDRU + SFR_OFFSET)
#define SDMMC_IDINTEN		(SDMMC_IDSTS + SFR_OFFSET)
#define SDMMC_DSCADDRL		(SDMMC_IDINTEN + SFR_OFFSET)
#define SDMMC_DSCADDRU		(SDMMC_DSCADDRL + SFR_OFFSET)
#define SDMMC_BUFADDR		(SDMMC_DSCADDRU + SFR_OFFSET)
#define SDMMC_BUFADDRU		(SDMMC_BUFADDR + SFR_OFFSET)
#define SDMMC_CLKSEL		(SDMMC_BUFADDRU + SFR_OFFSET) /* specific to Samsung Exynos */

#define SDMMC_AXI_BURST_LEN	0x00b4
#define SDMMC_SECTOR_NUM_INC	0x01F8

#else

#define SDMMC_DBADDR		 0x0088
#define SDMMC_IDSTS		(SDMMC_DBADDR + SFR_OFFSET)
#define SDMMC_IDINTEN		(SDMMC_IDSTS + SFR_OFFSET)
#define SDMMC_DSCADDR		(SDMMC_IDINTEN + SFR_OFFSET)
#define SDMMC_BUFADDR		(SDMMC_DSCADDR + SFR_OFFSET)
#define SDMMC_CLKSEL		(SDMMC_BUFADDR + SFR_OFFSET) /* specific to Samsung Exynos */

#define SDMMC_AXI_BURST_LEN	0xffff	/*not used*/
#define SDMMC_SECTOR_NUM_INC	0xffff	/*not used*/

#endif

#define SDMMC_CDTHRCTL		0x100
#define SDMMC_DATA(x)		(x)

#if defined(CONFIG_SOC_EXYNOS5422) || defined(CONFIG_SOC_EXYNOS5430) || \
	defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7420) || \
	defined(CONFIG_SOC_EXYNOS7580)
#define SDMMC_DDR200_ENABLE_SHIFT	0x110
#define SDMMC_DDR200_RDDQS_EN		0x180
#define SDMMC_DDR200_ASYNC_FIFO_CTRL	0x184
#define SDMMC_DDR200_DLINE_CTRL		0x188
#else
#define SDMMC_DDR200_RDDQS_EN		0x110
#define SDMMC_DDR200_ASYNC_FIFO_CTRL	0x114
#define SDMMC_DDR200_DLINE_CTRL		0x118
#endif

#define SDMMC_EMMCP_BASE		0x1000
#define SDMMC_MPSTAT			(SDMMC_EMMCP_BASE + 0x0008)
#define SDMMC_MPSECURITY		(SDMMC_EMMCP_BASE + 0x0010)
#define SDMMC_MPENCKEY			(SDMMC_EMMCP_BASE + 0x0020)
#define SDMMC_MPSBEGIN0			(SDMMC_EMMCP_BASE + 0x0200)
#define SDMMC_MPSEND0			(SDMMC_EMMCP_BASE + 0x0204)
#define SDMMC_MPSLUN0			(SDMMC_EMMCP_BASE + 0x0208)
#define SDMMC_MPSCTRL0			(SDMMC_EMMCP_BASE + 0x020C)
#define SDMMC_MPSBEGIN1			(SDMMC_EMMCP_BASE + 0x0210)
#define SDMMC_MPSEND1			(SDMMC_EMMCP_BASE + 0x0214)
#define SDMMC_MPSCTRL1			(SDMMC_EMMCP_BASE + 0x021C)

/******************/
/* Derived macros */
/******************/
/* SDMMC_CLKSEL */
#define SDMMC_CLKSEL_CCLK_SAMPLE(x)	(((x) & 7) << 0)
#define SDMMC_CLKSEL_CCLK_FINE_SAMPLE(x)	(((x) & 0xF) << 0)
#define SDMMC_CLKSEL_CCLK_DRIVE(x)	(((x) & 7) << 16)
#define SDMMC_CLKSEL_CCLK_FINE_DRIVE(x)	(((x) & 3) << 22)
#define SDMMC_CLKSEL_CCLK_DIVIDER(x)	(((x) & 7) << 24)
#define SDMMC_CLKSEL_GET_DRV_WD3(x)	(((x) >> 16) & 0x7)
#define SDMMC_CLKSEL_GET_DIVRATIO(x)	((((x) >> 24) & 0x7) + 1)
#define SDMMC_CLKSEL_TIMING(div, f_drv, drv, sample) \
	(SDMMC_CLKSEL_CCLK_DIVIDER(div) |	\
	 SDMMC_CLKSEL_CCLK_FINE_DRIVE(f_drv) |	\
	 SDMMC_CLKSEL_CCLK_DRIVE(drv) |		\
	 SDMMC_CLKSEL_CCLK_SAMPLE(sample))

/* SDMMC_DDR200_RDDQS_EN */
#define DWMCI_TXDT_CRC_TIMER_FASTLIMIT(x)	(((x) & 0xFF) << 16)
#define DWMCI_TXDT_CRC_TIMER_INITVAL(x)		(((x) & 0xFF) << 8)
#define DWMCI_TXDT_CRC_TIMER_SET(x, y)	(DWMCI_TXDT_CRC_TIMER_FASTLIMIT(x) | \
					DWMCI_TXDT_CRC_TIMER_INITVAL(y))
#define DWMCI_AXI_NON_BLOCKING_WRITE		BIT(7)
#define DWMCI_RESP_RCLK_MODE			BIT(5)
#define DWMCI_BUSY_CHK_CLK_STOP_EN		BIT(2)
#define DWMCI_RXDATA_START_BIT_SEL		BIT(1)
#define DWMCI_RDDQS_EN				BIT(0)
#define DWMCI_DDR200_RDDQS_EN_DEF	(DWMCI_TXDT_CRC_TIMER_FASTLIMIT(0x13) | \
					DWMCI_TXDT_CRC_TIMER_INITVAL(0x15))

/* SDMMC_DDR200_ASYNC_FIFO_CTRL */
#define DWMCI_ASYNC_FIFO_RESET		BIT(0)

/* SDMMC_DDR200_DLINE_CTRL */
#define DWMCI_WD_DQS_DELAY_CTRL(x)		(((x) & 0x3FF) << 20)
#define DWMCI_FIFO_CLK_DELAY_CTRL(x)		(((x) & 0x3) << 16)
#define DWMCI_RD_DQS_DELAY_CTRL(x)		((x) & 0x3FF)
#define DWMCI_DDR200_DLINE_CTRL_SET(x, y, z)	(DWMCI_WD_DQS_DELAY_CTRL(x) | \
						DWMCI_FIFO_CLK_DELAY_CTRL(y) | \
						DWMCI_RD_DQS_DELAY_CTRL(z))
#define DWMCI_DDR200_DLINE_CTRL_DEF	(DWMCI_FIFO_CLK_DELAY_CTRL(0x2) | \
					DWMCI_RD_DQS_DELAY_CTRL(0x40))

/* SDMMC_SECTOR_NUM_INC */
#define DWMCI_BURST_LENGTH_MASK		(0xF)
#define DWMCI_BURST_LENGTH_CTRL(x)	(((x)&DWMCI_BURST_LENGTH_MASK) | \
					(((x)&DWMCI_BURST_LENGTH_MASK)<<16))

/* SDMMC_SECTOR_NUM_INC */
#define DWMCI_SECTOR_SIZE_MASK		(0x1FFF)
#define DWMCI_SECTOR_SIZE_CTRL(x)	((x)&DWMCI_SECTOR_SIZE_MASK)

/* SDMMC_DDR200_ENABLE_SHIFT */
#define DWMCI_ENABLE_SHIFT_MASK			(0x3)
#define DWMCI_ENABLE_SHIFT(x)			((x) & DWMCI_ENABLE_SHIFT_MASK)

/* Block number in eMMC */
#define DWMCI_BLOCK_NUM			0xFFFFFFFF

/* SMU control bits */
#define DWMCI_MPSCTRL_SECURE_READ_BIT		BIT(7)
#define DWMCI_MPSCTRL_SECURE_WRITE_BIT		BIT(6)
#define DWMCI_MPSCTRL_NON_SECURE_READ_BIT	BIT(5)
#define DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT	BIT(4)
#define DWMCI_MPSCTRL_USE_FUSE_KEY		BIT(3)
#define DWMCI_MPSCTRL_ECB_MODE			BIT(2)
#define DWMCI_MPSCTRL_ENCRYPTION		BIT(1)
#define DWMCI_MPSCTRL_VALID			BIT(0)
#define DWMCI_MPSCTRL_BYPASS			(DWMCI_MPSCTRL_SECURE_READ_BIT |\
						DWMCI_MPSCTRL_SECURE_WRITE_BIT |\
						DWMCI_MPSCTRL_NON_SECURE_READ_BIT |\
						DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT |\
						DWMCI_MPSCTRL_VALID)

#define EXYNOS4210_FIXED_CIU_CLK_DIV	2
#define EXYNOS4412_FIXED_CIU_CLK_DIV	4

/* FMP SECURITY bits */
#define DWMCI_MPSECURITY_PROTBYTZPC		BIT(31)
#define DWMCI_MPSECURITY_MMC_SFR_PROT_ON	BIT(29)
#define DWMCI_MPSECURITY_FMP_ENC_ON		BIT(28)
#define DWMCI_MPSECURITY_DESCTYPE(type) 	((type & 0x3) << 19)

/* FMP configuration */
#if defined(CONFIG_SOC_EXYNOS5433)
#define DW_MMC_BYPASS_SECTOR_BEGIN		0x80000000
#define DW_MMC_BYPASS_SECTOR_END		0xFFFFFFFF
#define DW_MMC_ENCRYPTION_SECTOR_BEGIN		0
#define DW_MMC_ENCRYPTION_SECTOR_END		0x7FFFFFFF
#elif defined(CONFIG_SOC_EXYNOS7420)
#define DW_MMC_BYPASS_SECTOR_BEGIN		0x0
#define DW_MMC_ENCRYPTION_SECTOR_BEGIN		0x0000FFFF
#define DW_MMC_FILE_ENCRYPTION_SECTOR_BEGIN	0xFFFF0000
#else
#define DW_MMC_BYPASS_SECTOR_BEGIN		0
#define DW_MMC_BYPASS_SECTOR_END		0
#define DW_MMC_ENCRYPTION_SECTOR_BEGIN		1
#define DW_MMC_ENCRYPTION_SECTOR_END		0xFFFFFFFF
#endif

/* FMP logical unit number */
#define DW_MMC_LUN_BEGIN			0
#define DW_MMC_LUN_END				0xff
#define DWMCI_SET_LUN(s, e) \
	(((s & 0xff) << 16) | (e & 0xff))

#endif /* _EXYNOS_DWMCI_H_ */
