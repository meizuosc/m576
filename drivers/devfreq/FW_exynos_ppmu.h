/* drivers/devfreq/FW_exynos_ppmu.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - PPMU controling firmware header.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PPMUx_H__
#define __PPMUx_H__

#ifdef CONFIG_PM_DEVFREQ
#include <linux/io.h>

#define Outp32(addr, data)      (__raw_writel(data, addr))
#define Inp32(addr)             (__raw_readl(addr))

typedef void __iomem *addr_u32;
#else
#define Outp32(addr, data) (*(volatile u32 *)(addr) = (data))
#define Inp32(addr) (*(volatile u32 *)(addr))

typedef unsigned long u32;
typedef unsigned long long u64;
typedef signed long s32;

typedef unsigned long addr_u32;
#endif	/* CONFIG_PM_DEVFREQ */


/* for PPMU V2.0 Register */
#define rV2_VERSION				0x0
#define rV2_PMNC				0x4
#define rV2_CNTENS				0x8
#define rV2_CNTENC				0xC
#define rV2_INTENS				0x10
#define rV2_INTENC				0x14
#define rV2_FLAG				0x18
#define rV2_CIG_CFG0				0x1c
#define rV2_CIG_CFG1				0x20
#define rV2_CIG_CFG2				0x24
#define rV2_CIG_RESULT				0x28
#define rV2_CNT_RESET				0x2c
#define rV2_CNT_AUTO				0x30
#define rV2_PMCNT0				0x34
#define rV2_PMCNT1				0x38
#define rV2_PMCNT2				0x3c
#define rV2_PMCNT3_LOW				0x40
#define rV2_PMCNT3_HIGH				0x44
#define rV2_CCNT				0x48
#define rV2_CH_EV0_TYPE				0x200
#define rV2_CH_EV1_TYPE				0x204
#define rV2_CH_EV2_TYPE				0x208
#define rV2_CH_EV3_TYPE				0x20c
#define rV2_SM_ID_V				0x220
#define rV2_SM_ID_A				0x224
#define rV2_SM_OTHERS_V				0x228
#define rV2_SM_OTHERS_A				0x22c
#define rV2_INTERRUPT_TEST			0x260

/* for PPMU V1.1 Register */
#define rV1_PMNC				0x0
#define rV1_CNTENS				0x10
#define rV1_CNTENC				0x20
#define rV1_INTENS				0x30
#define rV1_INTENC				0x40
#define rV1_FLAG				0x50
#define rV1_CCNT				0x100
#define rV1_PMCNT0				0x110
#define rV1_PMCNT1				0x120
#define rV1_PMCNT2				0x130
#define rV1_PMCNT3_HIGH				0x140
#define rV1_PMCNT3_LOW				0x150
#define rV1_BEVT0SEL				0x1000
#define rV1_BEVT1SEL				0x1100
#define rV1_BEVT2SEL				0x1200
#define rV1_BEVT3SEL				0x1300
#define rV1_COUNTER_RESET			0x1800
#define rV1_READ_OVERFLOW_CNT			0x1810
#define rV1_READ_UNDERFLOW_CNT			0x1814
#define rV1_WRITE_OVERFLOW_CNT			0x1850
#define rV1_WRITE_UNDERFLOW_CNT			0x1854
#define rV1_READ_MAX_PENDING_CNT		0x1880
#define rV1_WRITE_MAX_PENDING_CNT		0x1884


/* time-sampling based measurement */
typedef enum {			/* AUTO and CIG only in 2.0 */
	PPMU_MODE_MANUAL,
	PPMU_MODE_AUTO,
	PPMU_MODE_CIG		/* Conditional Interrupt Generation */
} PPMU_MODE;

typedef enum {
	AxLEN,
	AxSIZE,
	AxBURST,
	AxLOCK,
	AxCACHE,
	AxPROT,
	RRESP,
	BRESP
} SMByOthersType;

typedef enum {
	PPMU1_EVENT_RD_DATA = 0x5,
	PPMU1_EVENT_WR_DATA = 0x6,
	PPMU2_EVENT_RD_DATA = 0x4,
	PPMU2_EVENT_WR_DATA = 0x5,
} PPMU_EVENT012;

typedef enum {
	PPMU1_EVENT3_RD_DATA = 0x5,
	PPMU1_EVENT3_WR_DATA = 0x6,
	PPMU1_EVENT3_RW_DATA = 0x7,
	PPMU2_EVENT3_RD_DATA = 0x4,
	PPMU2_EVENT3_WR_DATA = 0x5,
	PPMU2_EVENT3_RW_DATA = 0x22,
} PPMU_EVENT3;

s32  PPMU_GetVersion(addr_u32 BaseAddr);
void PPMU_EnableOverflowInterrupt(addr_u32 BaseAddr);
void PPMU_Init(addr_u32 BaseAddr);
void PPMU_SetEvent(addr_u32 BaseAddr, PPMU_EVENT012 eEvent0, PPMU_EVENT012 eEvent1, PPMU_EVENT012 eEvent2, PPMU_EVENT3 eEvent3);
void PPMU_Start(addr_u32 BaseAddr, PPMU_MODE eMode);/* for individual control use CNTENS/C Reg */
void PPMU_Stop(addr_u32 BaseAddr);
void PPMU_GetResult(addr_u32 BaseAddr, u32 *uClockCount, u32 *uEvent0Count, u32 *uEvent1Count, u32 *uEvent2Count, u64 *uEvent3Count);
void PPMU_Reset(addr_u32 BaseAddr);
void PPMU_ControlByExtTrigger(addr_u32 BaseAddr);/* start/stop by external trigger(or SYSCON) */
s32 PPMU_StartByExtTrigger(addr_u32 BaseAddr, addr_u32 controlAddr, PPMU_MODE eMode);
void PPMU_StopByExtTrigger(addr_u32 controlAddr);
s32 PPMU_EnableCIGInterrupt(addr_u32 BaseAddr, u32  PMCNT0en, u32  PMCNT1en, u32  PMCNT2en);
s32 PPMU_SetCIGCondition(addr_u32 BaseAddr, u32  eventIdx, u32  UpperThreshold, u32  UpperRepeat, u32  LowerThreshold, u32  LowerRepeat);
s32 PPMU_GetCIGResult(addr_u32 BaseAddr, u32  *uClockCount, u32  *uEvent0Count, u32  *uEvent1Count, u32  *uEvent2Count, u64  *uEvent3Count);
void PPMU_DeInit(addr_u32 BaseAddr);
s32 PPMU_Filter(addr_u32 BaseAddr , u32  SM_ID_V, u32  SM_ID_A, u32  SM_Other_V, u32  SM_Other_A);/* Just for Debugging */

#endif
