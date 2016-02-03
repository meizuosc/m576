/* linux/arch/arm64/mach-exynos/include/mach/otp.h
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS5433 - OTP Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef __OTP_H__
#define __OTP_H__

#include <linux/io.h>
#include <mach/map.h>

#define	OTP_BASE	S5P_VA_CHIPID6

#define	rOTP_LOCK0				(OTP_BASE+0x000)
#define	rOTP_LOCK1				(OTP_BASE+0x004)
#define	rOTP_SECURE_READ_DATA			(OTP_BASE+0x008)
#define	rOTP_NONSECURE_READ_DATA		(OTP_BASE+0x00C)
#define	rOTP_CON_CONRTOL			(OTP_BASE+0x010)
#define	rOTP_CON_CONFIG				(OTP_BASE+0x014)
#define	rOTP_IF					(OTP_BASE+0x018)
#define	rOTP_INT_STATUS				(OTP_BASE+0x01C)
#define	rOTP_INT_EN				(OTP_BASE+0x020)
#define	rOTP_CON_TIME_PARA_0			(OTP_BASE+0x024)
#define	rOTP_CON_TIME_PARA_1			(OTP_BASE+0x028)
#define	rOTP_CON_TIME_PARA_2			(OTP_BASE+0x02C)
#define	rOTP_CON_TIME_PARA_3			(OTP_BASE+0x030)
#define	rOTP_CON_TIME_PARA_4			(OTP_BASE+0x034)
#define	rOTP_CON_TIME_PARA_5			(OTP_BASE+0x038)
#define	rOTP_CON_TIME_PARA_6			(OTP_BASE+0x03C)
#define	rOTP_CON_TIME_PARA_7			(OTP_BASE+0x040)
#define	rOTP_ADD_LOCK				(OTP_BASE+0x044)
#define	rOTP_CUSTOM_LOCK0			(OTP_BASE+0x048)
#define	rOTP_CUSTOM_LOCK01			(OTP_BASE+0x04C)

#define	OTP_LOCK_KEY_BASE			0x7C00
#define	OTP_LOCK_KEY20_AREA_BASE		64
#define BANK_OFFSET	0x400

#define Outp32(addr, data)			(__raw_writel(data, addr))
#define Inp32(addr)				(__raw_readl(addr))

#define EXYNOS5433_OTP_TEMP25_ATLAS0		7
#define EXYNOS5433_OTP_TEMP85_ATLAS0		8
#define EXYNOS5433_OTP_VPTAT_ATLAS0		10
#define EXYNOS5433_OTP_TEMP25_ATLAS1		12
#define EXYNOS5433_OTP_TEMP85_ATLAS1		13
#define EXYNOS5433_OTP_VPTAT_ATLAS1		15
#define EXYNOS5433_OTP_TEMP25_APOLO		17
#define EXYNOS5433_OTP_TEMP85_APOLO		18
#define EXYNOS5433_OTP_VPTAT_APOLO		20
#define EXYNOS5433_OTP_TEMP25_GPU		22
#define EXYNOS5433_OTP_TEMP85_GPU		23
#define EXYNOS5433_OTP_VPTAT_GPU		25
#define EXYNOS5433_OTP_TEMP25_ISP		27
#define EXYNOS5433_OTP_TEMP85_ISP		28
#define EXYNOS5433_OTP_VPTAT_ISP		30
#define EXYNOS5433_OTP_CALTYPE			11

enum {
	NO_FAIL = 0,
	INIT_FAIL,
	STANDBY_FAIL,
	PROGRAM_FAIL,
	LOCK_PROGRAM_FAIL,
	SECURE_FAIL,
	PROGRAM_LOCK,
	UNACCESSIBLE_REGION,
	TIME_OUT
};

extern u8 otp_temp25_atlas0;
extern u8 otp_temp85_atlas0;
extern u8 otp_vptat_atlas0;
extern u8 otp_temp25_atlas1;
extern u8 otp_temp85_atlas1;
extern u8 otp_vptat_atlas1;
extern u8 otp_temp25_apolo;
extern u8 otp_temp85_apolo;
extern u8 otp_vptat_apolo;
extern u8 otp_temp25_gpu;
extern u8 otp_temp85_gpu;
extern u8 otp_vptat_gpu;
extern u8 otp_temp25_isp;
extern u8 otp_temp85_isp;
extern u8 otp_vptat_isp;
extern u8 otp_cal_type;

bool otp_cmd_init(void);
bool otp_cmd_standby(void);
u32 otp_cmd_read(u32 ulAddress, u32 *pulError);
bool otp_read(u32 ulAddress, u8 *pucData, u32 ulSize, u32 *pulError);
u8 otp_parse(u32 ulOffset);
void otp_tmu_read(void);
void otp_tmu_print_info(void);

#endif /* __OTP_H__ */
