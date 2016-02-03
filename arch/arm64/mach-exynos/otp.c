/* linux/arch/arm64/mach-exynos/include/mach/otp.c
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS5433 - OTP Source File
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
 */

#include <mach/otp.h>

u8 otp_temp25_atlas0;
u8 otp_temp85_atlas0;
u8 otp_vptat_atlas0;
u8 otp_temp25_atlas1;
u8 otp_temp85_atlas1;
u8 otp_vptat_atlas1;
u8 otp_temp25_apolo;
u8 otp_temp85_apolo;
u8 otp_vptat_apolo;
u8 otp_temp25_gpu;
u8 otp_temp85_gpu;
u8 otp_vptat_gpu;
u8 otp_temp25_isp;
u8 otp_temp85_isp;
u8 otp_vptat_isp;
u8 otp_cal_type;

bool otp_cmd_init(void)
{
	u32 ulreg;
	u32 uErrorCnt = 0;
	bool bResult = false;

	Outp32(rOTP_CON_CONRTOL, 0x01);

	while (true) {
		if (Inp32(rOTP_INT_STATUS) & 0x01) {
			bResult = true;
			break;
		}
		uErrorCnt++;
		if (uErrorCnt > 0xffffff) {
			bResult = false;
			break;
		}
	}

	ulreg = Inp32(rOTP_INT_STATUS);
	Outp32(rOTP_INT_STATUS, (ulreg | 0x01));

	return bResult;
}

bool otp_cmd_standby(void)
{
	u32 ulreg;
	u32 uErrorCnt = 0;
	bool bResult = false;

	/* 1. set standby command */
	ulreg = Inp32(rOTP_CON_CONRTOL);
	Outp32(rOTP_CON_CONRTOL, (ulreg & 0xfffffff7) | 0x08);

	while (true) {
		if (Inp32(rOTP_INT_STATUS) & 0x08) {
			bResult = true;
			break;
		}
		uErrorCnt++;
		if (uErrorCnt > 0xffffff) {
			bResult = false;
			break;
		}
	}

	ulreg = Inp32(rOTP_INT_STATUS);
	Outp32(rOTP_INT_STATUS, (ulreg | 0x08));

	return bResult;
}

u32 otp_cmd_read(u32 ulAddress, u32 *pulError)
{
	u32 ulreg;
	u32 ulErrorCnt = 0;
	u32 ulReadData = 0;

	pr_debug("ulAddress: 0x%X\n", ulAddress);
	*pulError = NO_FAIL;

	/* 1. set address */
	/* OTP_IF: program data[31],address [14:0] */
	ulreg = Inp32(rOTP_IF);
	ulreg = (ulreg & 0xFFFF0000) | (ulAddress & 0x7FFF);
	Outp32(rOTP_IF, ulreg);

	/* 2. set read command */
	ulreg = Inp32(rOTP_CON_CONRTOL);
	Outp32(rOTP_CON_CONRTOL, (ulreg & 0xFFFFFFFD) | 0x02);

	/* 3. check read status */
	while (true) {
		ulreg = Inp32(rOTP_INT_STATUS);

		/* check read done */
		if (ulreg & 0x02) {
			pr_debug("pulError = NO_FAIL (0x%x)\n", *pulError);
			break;
		}

		/* Check secure fail */
		if (ulreg & 0x80) {
			*pulError = SECURE_FAIL;
			pr_err("pulError = SECURE_FAIL (0x%x)\n", *pulError);
			break;
		}

		ulErrorCnt++;
		if (ulErrorCnt > 0xffffff) {
			*pulError = TIME_OUT;
			pr_err("pulError = SECURE_FAIL (0x%x)\n", *pulError);
			break;
		}
	}

	if (*pulError == NO_FAIL) {
		/* checking bit [14:13] */
		ulreg = (Inp32(rOTP_IF) & 0x6000) >> 13;

		/* 4-1 read SECURE DATA [bit [14:13]= 1:0 or 1:1] */
		if (ulreg & 0x2) {
			ulReadData = Inp32(rOTP_SECURE_READ_DATA);
			pr_debug("read SECURE DATA= 0x%x\n", ulReadData);
		} else if (ulreg & 0x1) {
			/* 4-2 Hardware only accessible [bit [14:13]= 0:1] */
			*pulError = UNACCESSIBLE_REGION;
			pr_err("UNACCESSIBLE_REGION\n");
		} else if (!(ulreg & 0x3)) {
			/* 4-3 read NON SECURE DATA [bit [14:13]= 0:0] */
			ulReadData = Inp32(rOTP_NONSECURE_READ_DATA);
			pr_debug("read NON SECURE DATA= 0x%x\n", ulReadData);
		} else if (ulreg == 1) {
			/* 4-3 Hardware only accessible [bit [14:13]= 0:1] */
			*pulError = UNACCESSIBLE_REGION;
		}
	}

	if (*pulError == NO_FAIL) {
		ulreg = Inp32(rOTP_INT_STATUS);
		Outp32(rOTP_INT_STATUS, (ulreg | 0x02));
	}

	return ulReadData;
}

bool otp_read(u32 ulAddress, u8 *pucData, u32 ulSize, u32 *pulError)
{
	u32 ulCount;
	u32 ulDataShiftCount;
	u32 ulDataCount = 0;
	u32 ulData;
	*pulError = NO_FAIL;

	if (otp_cmd_init() == false)
		*pulError = INIT_FAIL;

	if (*pulError != INIT_FAIL) {
		for (ulCount = 0; ulCount < (ulSize / 32); ulCount++) {
			ulData = otp_cmd_read(ulAddress + (ulCount*32), pulError);
			if (*pulError != NO_FAIL)
				break;

			for (ulDataShiftCount = 0; ulDataShiftCount < 32; ulDataShiftCount++)
				pucData[ulDataCount++] = (unsigned char)((ulData >> ulDataShiftCount) & 0x1);
		}
	}

	if (otp_cmd_standby() == false)
		*pulError = STANDBY_FAIL;

	if (*pulError != NO_FAIL)
		return false;
	else
		return true;
}

u8 otp_parse(u32 ulOffset)
{
	int ulSize = 32;
	u8 ret = 0;
	u32 ErrorCnt = 0;
	u8 pucData[32] = {0,};
	u32 i;

	if (otp_read((BANK_OFFSET * 3 + (32 * ulOffset)), pucData, ulSize, &ErrorCnt)) {
		for (i = 0; i < 8; i++) {
			if (pucData[i*2] == 1)
				ret = ret | (0x1 << i);
		}
	}

	return ret;
}

void otp_tmu_print_info(void)
{
	pr_info("otp_temp25_atlas0=%d\n", otp_temp25_atlas0);
	pr_info("otp_temp85_atlas0=%d\n", otp_temp85_atlas0);
	pr_info("otp_vptat_atlas0=%d\n", otp_vptat_atlas0);
	pr_info("otp_temp25_atlas1=%d\n", otp_temp25_atlas1);
	pr_info("otp_temp85_atlas1=%d\n", otp_temp85_atlas1);
	pr_info("otp_vptat_atlas1=%d\n", otp_vptat_atlas1);
	pr_info("otp_temp25_apolo=%d\n", otp_temp25_apolo);
	pr_info("otp_temp85_apolo=%d\n", otp_temp85_apolo);
	pr_info("otp_vptat_apolo=%d\n",  otp_vptat_apolo);
	pr_info("otp_temp25_gpu=%d\n",   otp_temp25_gpu);
	pr_info("otp_temp85_gpu=%d\n",   otp_temp85_gpu);
	pr_info("otp_vptat_gpu=%d\n",    otp_vptat_gpu);
	pr_info("otp_temp25_isp=%d\n",   otp_temp25_isp);
	pr_info("otp_temp85_isp=%d\n",   otp_temp85_isp);
	pr_info("otp_vptat_isp=%d\n",    otp_vptat_isp);
	pr_info("otp_cal_type=%d\n",     otp_cal_type);
}

void otp_tmu_read(void)
{
	otp_temp25_atlas0 = otp_parse(EXYNOS5433_OTP_TEMP25_ATLAS0);
	otp_temp85_atlas0 = otp_parse(EXYNOS5433_OTP_TEMP85_ATLAS0);
	otp_vptat_atlas0 = otp_parse(EXYNOS5433_OTP_VPTAT_ATLAS0);
	otp_temp25_atlas1 = otp_parse(EXYNOS5433_OTP_TEMP25_ATLAS1);
	otp_temp85_atlas1 = otp_parse(EXYNOS5433_OTP_TEMP85_ATLAS1);
	otp_vptat_atlas1 = otp_parse(EXYNOS5433_OTP_VPTAT_ATLAS1);
	otp_temp25_apolo = otp_parse(EXYNOS5433_OTP_TEMP25_APOLO);
	otp_temp85_apolo = otp_parse(EXYNOS5433_OTP_TEMP85_APOLO);
	otp_vptat_apolo = otp_parse(EXYNOS5433_OTP_VPTAT_APOLO);
	otp_temp25_gpu = otp_parse(EXYNOS5433_OTP_TEMP25_GPU);
	otp_temp85_gpu = otp_parse(EXYNOS5433_OTP_TEMP85_GPU);
	otp_vptat_gpu = otp_parse(EXYNOS5433_OTP_VPTAT_GPU);
	otp_temp25_isp = otp_parse(EXYNOS5433_OTP_TEMP25_ISP);
	otp_temp85_isp = otp_parse(EXYNOS5433_OTP_TEMP85_ISP);
	otp_vptat_isp = otp_parse(EXYNOS5433_OTP_VPTAT_ISP);
	otp_cal_type = otp_parse(EXYNOS5433_OTP_CALTYPE);
}
