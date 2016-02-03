/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_OTP_H__
#define __EXYNOS_OTP_H__

/* Bank0 magic code */
#define OTP_MAGIC_MIPI_DSI0_M0		0x4430		/* ascii: D0 */
#define OTP_MAGIC_MIPI_DSI0_M1		0x4431		/* D1 */
#define OTP_MAGIC_MIPI_CSI0		0x4330		/* C0 */
#define OTP_MAGIC_MIPI_CSI1		0x4331		/* C1 */
#define OTP_MAGIC_MIPI_CSI2		0x4332		/* C2 */
#define OTP_MAGIC_MIPI_CSI3		0x4333		/* C3 */
#define OTP_MAGIC_MPHY_LLI		0x4C4C		/* LL */
/* Bank1 magic code */
#define OTP_MAGIC_PCIE_WIFI		0x5057		/* ascii: PW */
#define OTP_MAGIC_PCIE_MODEM		0x504D		/* PM */
#define OTP_MAGIC_HDMI			0x4844		/* HD */
#define OTP_MAGIC_EDP			0x4450		/* DP */

#define OTP_BANK0_SIZE			128
#define OTP_BANK1_SIZE			128

/* OTP tune bits structure of each IP block */
#define OTP_OFFSET_MAGIC	0	/* bit0 */
#define OTP_OFFSET_TYPE		2	/* bit16 */
#define OTP_OFFSET_COUNT	3	/* bit24 */
#define OTP_OFFSET_INDEX	4	/* bit32 */
#define OTP_OFFSET_DATA		5	/* bit40 */

#define OTP_TYPE_BYTE		0
#define OTP_TYPE_WORD		1

/* Parsed OTP tune bits structure */
struct tune_bits {
	u8	index;		/* SFR addr = base addr + (index * 4) */
	u32	value;		/* if OTP_TYPE is BYTE, only LSB 8bit is valid */
};

#ifdef CONFIG_EXYNOS_OTP
int otp_tune_bits_offset(u16 magic, unsigned long *addr);
int otp_tune_bits_parsed(u16 magic, u8 *dt_type, u8 *nr_data, struct tune_bits **data);
#else
static inline int otp_tune_bits_offset(u16 magic, unsigned long *addr)
{
	return -1;
}
static inline int otp_tune_bits_parsed(u16 magic, u8 *dt_type, u8 *nr_data, struct tune_bits **data)
{
	return -1;
}
#endif

#endif /* __EXYNOS_OTP_H__ */
