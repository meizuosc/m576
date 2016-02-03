/* drivers/devfreq/FW_exynos_ppmu.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - PPMU controling firmware code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "FW_exynos_ppmu.h"

/*
	Function For Version Number
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
*/

#ifdef CONFIG_PM_DEVFREQ
static s32 ppmu_version;
#endif

s32 PPMU_GetVersion(addr_u32 BaseAddr)
{
	u32 uVersion;

#ifdef CONFIG_PM_DEVFREQ
	if (ppmu_version != 0)
		return ppmu_version;
#endif

	uVersion = (Inp32(BaseAddr) & 0x000f0000) >> 16;
	if (uVersion == 2)
		uVersion = 2;
	else
		uVersion = 1;

#ifdef CONFIG_PM_DEVFREQ
	ppmu_version = uVersion;
#endif

	return uVersion;
}

/*
	Function For Enable Interrupt
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
*/
void PPMU_EnableOverflowInterrupt(addr_u32 BaseAddr)
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue = 0x1<<31 | 0xf; /* CCNT and All PMCNTx */
		Outp32(BaseAddr+rV2_INTENS, uRegValue);
	} else { /* must be v1.1 and not support this function. */
		uRegValue = 0x1<<31 | 0xf; /* CCNT and All PMCNTx */
		Outp32(BaseAddr+rV1_INTENS, uRegValue);
	}
}


/*
	Function for initiating PPMU defualt setting
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
*/
void PPMU_Init(addr_u32 BaseAddr)
{
	u32 uRegValue;
	u32  bIsCounting;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		/* reset to default */
		uRegValue = Inp32(BaseAddr+rV2_PMNC);

		bIsCounting = uRegValue & 0x1;
		if (bIsCounting) {
			uRegValue &= ~(1<<0); /* counting disable */
			Outp32(BaseAddr+rV2_PMNC, uRegValue);
		}
		uRegValue = uRegValue & ~(0x3<<20);

		uRegValue &= ~(0x1<<16);/* 0x0: Configure start mode as APB interface */
		uRegValue &= ~(0x1<<3); /* 0x0: Disable clock dividing */
		uRegValue |= (0x1<<2); /* 0x1: Reset CCNT */
		uRegValue |= (0x1<<1); /* 0x1: Reset PMCNTs */
		Outp32(BaseAddr+rV2_PMNC, uRegValue);

		/* Count Enable CCNT, PMCNTTx */
		Outp32(BaseAddr+rV2_CNTENS, 0x8000000f);

		PPMU_EnableOverflowInterrupt(BaseAddr);
	} else { /* must be v1.1 */
		uRegValue = Inp32(BaseAddr+rV1_PMNC);

		bIsCounting = uRegValue & 0x1;
		if (bIsCounting) {
			uRegValue &= ~(1<<0);
			Outp32(BaseAddr+rV1_PMNC, uRegValue);
		}

		uRegValue &= ~(0x1<<3); /* NO CC Divider */
		uRegValue |= 0x1<<2; /* Reset CCNT */
		uRegValue |= 0x1<<1; /* Reset All PMCNTs */
		Outp32(BaseAddr+rV1_PMNC, uRegValue);

		Outp32(BaseAddr+rV1_CNTENS, 0x8000000f);

		PPMU_EnableOverflowInterrupt(BaseAddr);
	}
}

/*
	Function for event setting for CCNT, PMCNT0, PMCNT1, PMCNT2, PMCNT3
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		eEventx = event number (Ref. ppmu.h)
*/
void PPMU_SetEvent(addr_u32 BaseAddr, PPMU_EVENT012 eEvent0, PPMU_EVENT012 eEvent1, PPMU_EVENT012 eEvent2, PPMU_EVENT3 eEvent3)
{
	u32 uRegValue = 0;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent0;
		Outp32(BaseAddr+rV2_CH_EV0_TYPE, uRegValue);

		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent1;
		Outp32(BaseAddr+rV2_CH_EV1_TYPE, uRegValue);

		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent2;
		Outp32(BaseAddr+rV2_CH_EV2_TYPE, uRegValue);

		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent3;
		Outp32(BaseAddr+rV2_CH_EV3_TYPE, uRegValue);
	} else { /* must be v1.1 */
		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent0;
		Outp32(BaseAddr+rV1_BEVT0SEL, uRegValue);

		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent1;
		Outp32(BaseAddr+rV1_BEVT1SEL, uRegValue);

		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent2;
		Outp32(BaseAddr+rV1_BEVT2SEL, uRegValue);

		uRegValue &= ~(0xff);
		uRegValue = (u32)eEvent3;
		Outp32(BaseAddr+rV1_BEVT3SEL, uRegValue);
	}
}

/*
	Function for setting measurement period
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		clk_count = for caculation
*/
void PPMU_SetGICMeasurementPeriod(addr_u32 BaseAddr, u32 clk_count)
{
	u32 uRegValue = 0xFFFFFFFF - (clk_count*1024*1024-1);
	Outp32(BaseAddr+rV2_CCNT, uRegValue); /* 1000ms interval */
}


/*
	Function for start ppmu counting by APB signal used in Auto or Manual mode
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		eMode = Auto, Manual, CIG (Ref. ppmu.h)

*/
void PPMU_Start(addr_u32 BaseAddr, PPMU_MODE eMode)/* for individual control use CNTENS/C Reg */
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue = Inp32(BaseAddr+rV2_PMNC);
		uRegValue = uRegValue & ~(0x3<<20);/* clear Start Mode */
		uRegValue =
			(eMode == PPMU_MODE_MANUAL)	? uRegValue | (0x0<<20) :
			(eMode == PPMU_MODE_AUTO)	? uRegValue | (0x1<<20) :
			(eMode == PPMU_MODE_CIG)	? uRegValue | (0x2<<20) : 0;
		uRegValue |= 0x1;/* Start counting throung APB interface */

		if (eMode == PPMU_MODE_CIG)
			PPMU_SetGICMeasurementPeriod(BaseAddr, 400); /* 1000ms interval */

		Outp32(BaseAddr+rV2_PMNC, uRegValue);
	} else { /* must be v1.1 and only 'manual mode' exist. */
		uRegValue = Inp32(BaseAddr+rV1_PMNC);
		uRegValue &= ~0x1;
		uRegValue |= 0x1;
		Outp32(BaseAddr+rV1_PMNC, uRegValue);
	}
}

/*
	Function for stop ppmu counting used in Auto, Manual and CIG mode
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
*/
void PPMU_Stop(addr_u32 BaseAddr)
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue = Inp32(BaseAddr+rV2_PMNC);
		uRegValue &= ~0x1;	/* stop signal */
		Outp32(BaseAddr+rV2_PMNC, uRegValue);
	} else { /* must be v1.1 */
		uRegValue = Inp32(BaseAddr+rV1_PMNC);
		uRegValue &= ~0x1;
		Outp32(BaseAddr+rV1_PMNC, uRegValue);
	}
}

/*
	Function for getting result count
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		uClockcount = for clock count
		uEventxCount = for PMCNTx count
*/
void PPMU_GetResult(addr_u32 BaseAddr, u32 *uClockCount, u32 *uEvent0Count, u32 *uEvent1Count, u32 *uEvent2Count, u64  *uEvent3Count)
{
	if (PPMU_GetVersion(BaseAddr) == 2) {
		*uClockCount = Inp32(BaseAddr+rV2_CCNT);
		*uEvent0Count = Inp32(BaseAddr+rV2_PMCNT0);
		*uEvent1Count = Inp32(BaseAddr+rV2_PMCNT1);
		*uEvent2Count = Inp32(BaseAddr+rV2_PMCNT2);
		*uEvent3Count = (u64)(Inp32(BaseAddr+rV2_PMCNT3_LOW)) + (((u64)(Inp32(BaseAddr+rV2_PMCNT3_HIGH)&0xff))<<32);
	} else { /* must be v1.1 */
		*uClockCount = Inp32(BaseAddr+rV1_CCNT);
		*uEvent0Count = Inp32(BaseAddr+rV1_PMCNT0);
		*uEvent1Count = Inp32(BaseAddr+rV1_PMCNT1);
		*uEvent2Count = Inp32(BaseAddr+rV1_PMCNT2);
		*uEvent3Count = (u64)(Inp32(BaseAddr+rV1_PMCNT3_HIGH)<<8) + (Inp32(BaseAddr+rV1_PMCNT3_LOW) & 0xf);
	}
}

/*
	Function for changing start method to external trigger(sysreg)
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
*/
void PPMU_ControlByExtTrigger(addr_u32 BaseAddr)/* start/stop by external trigger(or SYSCON) */
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue = Inp32(BaseAddr+rV2_PMNC);
		uRegValue |= 0x1<<16;
		Outp32(BaseAddr+rV2_PMNC, uRegValue);
	} else { /* must be v1.1 */
		uRegValue = Inp32(BaseAddr+rV1_PMNC);
		uRegValue |= 0x1<<16;
		Outp32(BaseAddr+rV1_PMNC, uRegValue);
	}
}

/*
	Function for starting with external trigger(sysreg)
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		controlAddr = SYSREG_PERIS ALL_PPMU_CON BaseAddr
		eMode = Auto, Menaul, CIG (Ref. ppmu.h)
*/
s32 PPMU_StartByExtTrigger(addr_u32 BaseAddr, addr_u32 controlAddr, PPMU_MODE eMode)
{

	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		if (eMode != PPMU_MODE_CIG) {
			uRegValue = Inp32(BaseAddr+rV2_PMNC);
			uRegValue = uRegValue & ~(0x3<<20);/* clear Start Mode */
			uRegValue =
				(eMode == PPMU_MODE_MANUAL)	? uRegValue | (0x0<<20) :
				(eMode == PPMU_MODE_AUTO)	? uRegValue | (0x1<<20) : 0;
			Outp32(BaseAddr+rV2_PMNC, uRegValue);

			Outp32(controlAddr, 0xFFFFFFFF); /* SYSREF PPMU External Trigger */
		} else {
			return 1;
		}
	} else { /* must be v1.1 and only 'manual mode' exist. */
		Outp32(controlAddr, 0xFFFFFFFF); /* SYSREF PPMU External Trigger */
	}

	return 0;
}

/*
	Function for stopping with external trigger(sysreg)
	Parameter :
		controlAddr = SYSREG_PERIS ALL_PPMU_CON BaseAddr
*/
void PPMU_StopByExtTrigger(addr_u32 controlAddr)
{
	Outp32(controlAddr, 0x0);
}

/*
	Function for setting CIG Interrupt enable
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		PMCNTxen = enable interrupt signal for PMCNTx
*/
s32 PPMU_EnableCIGInterrupt(addr_u32 BaseAddr, u32 PMCNT0en, u32 PMCNT1en, u32 PMCNT2en)
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue = Inp32(BaseAddr+rV2_PMNC);
		uRegValue = Inp32(BaseAddr + rV2_CIG_CFG0);
		uRegValue = uRegValue | PMCNT0en | (PMCNT1en<<1) | (PMCNT2en<<2);
		Outp32(BaseAddr + rV2_CIG_CFG0, uRegValue);
	} else {
		return 1;
	}
	return 0;
}

/*
	Function for setting measurement period
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		clk_count = for caculation
*/
s32 PPMU_SetCIGCondition(addr_u32 BaseAddr, u32 eventIdx, u32 UpperThreshold, u32 UpperRepeat, u32 LowerThreshold, u32 LowerRepeat)
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue = Inp32(BaseAddr+rV2_PMNC);
		uRegValue = Inp32(BaseAddr + rV2_CIG_CFG0);

		uRegValue = uRegValue & ~(0xf<<4);
		uRegValue = uRegValue | ((LowerRepeat & 0xf)<<4);

		uRegValue = uRegValue & ~(0xf<<8);
		uRegValue = uRegValue | ((UpperRepeat & 0xf)<<8);

		uRegValue = uRegValue & ~(0x1<<16); /* Lower bound relationship : Any */
		uRegValue = uRegValue | (0x1<<20); /* Upper bound relationship : All */

		Outp32(BaseAddr + rV2_CIG_CFG0, uRegValue);

		Outp32(BaseAddr + rV2_CIG_CFG1, LowerThreshold);
		Outp32(BaseAddr + rV2_CIG_CFG2, UpperThreshold);
	} else {
		return 1;
	}
	return 0;
}

/*
	Function for getting result count for CIG mode
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
		uClockcount = for clock count
		uEventxCount = for PMCNTx count
*/
s32 PPMU_GetCIGResult(addr_u32 BaseAddr, u32 *uClockCount, u32 *uEvent0Count, u32 *uEvent1Count, u32 *uEvent2Count, u64  *uEvent3Count)
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		if (PPMU_GetVersion(BaseAddr) == 2) {
			/* Clear Interrupt */
			uRegValue = Inp32(BaseAddr + rV2_CIG_RESULT);

			uRegValue = uRegValue | (1<<4) | (1<<0);
			Outp32(BaseAddr + rV2_CIG_RESULT, uRegValue);

			*uClockCount = Inp32(BaseAddr+rV2_CCNT);
			*uEvent0Count = Inp32(BaseAddr+rV2_PMCNT0);
			*uEvent1Count = Inp32(BaseAddr+rV2_PMCNT1);
			*uEvent2Count = Inp32(BaseAddr+rV2_PMCNT2);
			*uEvent3Count = (u64)(Inp32(BaseAddr+rV2_PMCNT3_LOW)) + (((u64)(Inp32(BaseAddr+rV2_PMCNT3_HIGH)&0xff))<<32);
		}
	} else {
		return 1;
	}
	return 0;
}

/*
	Function for reset counter CCNT, PMCNTx
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
*/
void PPMU_Reset(addr_u32 BaseAddr)
{
	u32 uRegValue;

	if (PPMU_GetVersion(BaseAddr) == 2) {
		uRegValue = Inp32(BaseAddr+rV2_PMNC);
		uRegValue |= (0x1<<2); /* 0x1: Reset CCNT */
		uRegValue |= (0x1<<1); /* 0x1: Reset PMCNTs */
		Outp32(BaseAddr+rV2_PMNC, uRegValue);

	} else { /* must be v1.1 */
		uRegValue = Inp32(BaseAddr+rV1_PMNC);
		uRegValue |= (0x1<<2); /* 0x1: Reset CCNT */
		uRegValue |= (0x1<<1); /* 0x1: Reset PMCNTs */
		Outp32(BaseAddr+rV1_PMNC, uRegValue);

		uRegValue &= ~(0x00000006);
		Outp32(BaseAddr+rV1_PMNC, uRegValue);
	}
}



/*
	Function for deinitializing ppmu
	Parameter :
		BaseAddr = PPMU IP BASE ADDRESS
*/
void PPMU_DeInit(addr_u32 BaseAddr)
{
	if (PPMU_GetVersion(BaseAddr) == 2) {
		/* clear */
		Outp32(BaseAddr+rV2_FLAG, (0x1<<31 | 0xf));
		Outp32(BaseAddr+rV2_INTENC, (0x1<<31 | 0xf));
		Outp32(BaseAddr+rV2_CNTENC, (0x1<<31 | 0xf));
		Outp32(BaseAddr+rV2_CNT_RESET, (1<<31)|0xf);

		/* reset to default */
		Outp32(BaseAddr+rV2_PMNC, 0x0);
		Outp32(BaseAddr+rV2_CIG_CFG0, 0x0);
		Outp32(BaseAddr+rV2_CIG_CFG1, 0x0);
		Outp32(BaseAddr+rV2_CIG_CFG2, 0x0);
		Outp32(BaseAddr+rV2_CNT_AUTO, 0x0);
		Outp32(BaseAddr+rV2_PMCNT0, 0x0);
		Outp32(BaseAddr+rV2_PMCNT1, 0x0);
		Outp32(BaseAddr+rV2_PMCNT2, 0x0);
		Outp32(BaseAddr+rV2_PMCNT3_LOW, 0x0);
		Outp32(BaseAddr+rV2_PMCNT3_HIGH, 0x0);
		Outp32(BaseAddr+rV2_CCNT, 0x0);
		Outp32(BaseAddr+rV2_CH_EV0_TYPE, 0x0);
		Outp32(BaseAddr+rV2_CH_EV1_TYPE, 0x0);
		Outp32(BaseAddr+rV2_CH_EV2_TYPE, 0x0);
		Outp32(BaseAddr+rV2_CH_EV3_TYPE, 0x0);
		Outp32(BaseAddr+rV2_SM_ID_V, 0x0);
		Outp32(BaseAddr+rV2_SM_ID_A, 0x0);
		Outp32(BaseAddr+rV2_SM_OTHERS_V, 0x0);
		Outp32(BaseAddr+rV2_SM_OTHERS_A, 0x0);
		Outp32(BaseAddr+rV2_INTERRUPT_TEST, 0x0);

	} else { /* must be v1.1 */
		Outp32(BaseAddr+rV1_INTENC, (0x1<<31 | 0xf));
		Outp32(BaseAddr+rV1_CNTENC, (0x1<<31 | 0xf));
		Outp32(BaseAddr+rV1_COUNTER_RESET, (0x3<<16) | (0xf<<0));
		Outp32(BaseAddr+rV1_PMNC, 0x0);
		Outp32(BaseAddr+rV1_CNTENS, 0x0);
	}
}

/*
	Function for filtering
	Parameter :
		SM_ID_V : sfr value
		SM_ID_A : sfr value
		SM_Other_V : sfr value
		SM_Other_A : sfr value
*/
s32 PPMU_Filter(addr_u32 BaseAddr, u32 SM_ID_V, u32 SM_ID_A, u32 SM_Other_V, u32 SM_Other_A)/* Just for Debugging */
{
	if (PPMU_GetVersion(BaseAddr) == 2) {
		Outp32(BaseAddr+rV2_SM_ID_V, SM_ID_V);
		Outp32(BaseAddr+rV2_SM_ID_A, SM_ID_A);
		Outp32(BaseAddr+rV2_SM_OTHERS_V, SM_ID_V);
		Outp32(BaseAddr+rV2_SM_OTHERS_A, SM_ID_A);
	} else {
		return 1;
	}
	return 0;
}
