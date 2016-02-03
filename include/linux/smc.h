/*
 *  Copyright (c) 2012 Samsung Electronics.
 *
 * EXYNOS - SMC Call
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_EXYNOS_SMC_H
#define __ASM_ARCH_EXYNOS_SMC_H

#define SMC_CMD_INIT		(-1)
#define SMC_CMD_INFO		(-2)
/* For Power Management */
#define SMC_CMD_SLEEP		(-3)
#define SMC_CMD_CPU1BOOT	(-4)
#define SMC_CMD_CPU0AFTR	(-5)
#define SMC_CMD_SAVE		(-6)
#define SMC_CMD_SHUTDOWN	(-7)
/* For CP15 Access */
#define SMC_CMD_C15RESUME	(-11)
/* For L2 Cache Access */
#define SMC_CMD_L2X0CTRL	(-21)
#define SMC_CMD_L2X0SETUP1	(-22)
#define SMC_CMD_L2X0SETUP2	(-23)
#define SMC_CMD_L2X0INVALL	(-24)
#define SMC_CMD_L2X0DEBUG	(-25)

/* For Accessing CP15/SFR (General) */
#define SMC_CMD_REG		(-101)

/* For setting memory for debug */
#define SMC_CMD_SET_DEBUG_MEM  (-120)

/* Command ID for smc */
#define SMC_PROTECTION_SET	0x81000000
#define SMC_DRM_FW_LOADING	0x81000001
#define SMC_DCPP_SUPPORT	0x81000002
#define SMC_DRM_MAKE_PGTABLE	0x81000003
#define SMC_DRM_CLEAR_PGTABLE	0x81000004
#define SMC_MEM_PROT_SET	0x81000005
#define SMC_DRM_SECMEM_INFO	0x81000006
#define SMC_DRM_VIDEO_PROC	0x81000007
#define SMC_DRM_SECMEM_REGION_INFO	0x8100000A
#define SMC_DRM_SECMEM_REGION_PROT	0x8100000B
#define MC_FC_SET_CFW_PROT	0xC20018A0
#define MC_FC_DRM_SET_CFW_PROT	0x10000000

/* Parameter for smc */
#define SMC_PROTECTION_ENABLE	1
#define SMC_PROTECTION_DISABLE	0

/* For FMP Ctrl */
#if defined(CONFIG_SOC_EXYNOS7420)
#define SMC_CMD_FMP		(0xC2001810)
#else
#define SMC_CMD_FMP		(0x81000020)
#endif

/* For DTRNG Access */
#ifdef CONFIG_EXYRNG_USE_CRYPTOMANAGER
#define SMC_CMD_RANDOM		(0x82001012)
#else
#define SMC_CMD_RANDOM		(0x81000030)
#endif

/* MACRO for SMC_CMD_REG */
#define SMC_REG_CLASS_CP15	(0x0 << 30)
#define SMC_REG_CLASS_SFR_W	(0x1 << 30)
#define SMC_REG_CLASS_SFR_R	(0x3 << 30)
#define SMC_REG_CLASS_MASK	(0x3 << 30)
#define SMC_REG_ID_CP15(CRn, Op1, CRm, Op2) \
	   (SMC_REG_CLASS_CP15 | (CRn << 10) | (Op1 << 7) | (CRm << 3) | (Op2))
#define SMC_REG_ID_SFR_W(ADDR)	(SMC_REG_CLASS_SFR_W | ((ADDR) >> 2))
#define SMC_REG_ID_SFR_R(ADDR)	(SMC_REG_CLASS_SFR_R | ((ADDR) >> 2))

/* op type for SMC_CMD_SAVE and SMC_CMD_SHUTDOWN */
#define OP_TYPE_CORE            0x0
#define OP_TYPE_CLUSTER         0x1

/* Power State required for SMC_CMD_SAVE and SMC_CMD_SHUTDOWN */
#define SMC_POWERSTATE_SLEEP    0x0
#define SMC_POWERSTATE_IDLE     0x1
#define SMC_POWERSTATE_SWITCH   0x2

/* For FMP Ctrl */
#if defined(CONFIG_SOC_EXYNOS7420)
#define FMP_KEY_STORE		0x0
#define FMP_KEY_SET		0x1
#define FMP_KEY_RESUME		0x2
#else
#define FMP_MMC_KEY_SET		0x0
#define FMP_MMC_SUSPEND		0x1
#define FMP_MMC_RESUME		0x2
#endif

#if defined(CONFIG_SOC_EXYNOS7420)
#define UFS_FMP			0x15572000
#define EMMC0_FMP		0x15741000
#endif

/* For DTRNG Access */
#define HWRNG_INIT		0x0
#define HWRNG_EXIT		0x1
#define HWRNG_GET_DATA		0x2
#define HWRNG_RESUME		0x3

/* For CFW group */
#define CFW_DISP_RW	3
#define CFW_VPP0	5
#define CFW_VPP1	6

#ifndef __ASSEMBLY__
extern int exynos_smc(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3);
extern int exynos_smc_readsfr(unsigned long addr, unsigned long* val);
#endif

#endif
