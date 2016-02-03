/* arch/arm64/mach-exynos/include/mach/apm-exynos.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *						  http://www.samsung.com
 *
 * APM register definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_REGS_APM_H
#define __ASM_ARCH_REGS_APM_H __FILE__

/* Margin related variables */
#define MARGIN_0MV				(0)
#define MARGIN_6_25MV				(1)
#define MARGIN_12_5MV				(2)
#define MARGIN_18_75MV				(3)
#define MARGIN_25MV				(4)
#define MARGIN_31_25MV				(5)
#define MARGIN_37_5MV				(6)
#define MARGIN_43_75MV				(7)
#define MARGIN_50MV				(8)
#define MARGIN_56_25MV				(9)
#define MARGIN_62_5MV				(0xA)
#define MARGIN_68_75MV				(0xB)
#define MARGIN_75MV				(0xC)
#define MARGIN_81_25MV				(0xD)
#define MARGIN_87_5MV				(0xE)
#define MARGIN_93_75MV				(0xF)

/* PERIOD related variables */
#define PERIOD_1MS				(0)
#define PERIOD_5MS				(1)

/* APM Protocol related variables */
/* Notifier variables */
#define APM_READY				(0x0001)
#define APM_SLEEP				(0x0002)
#define APM_TIMEOUT				(0x0003)
#define CL_ENABLE				(0x0004)
#define CL_DISABLE				(0x0005)

/* Shift variables */
#define CL_DVFS_SHIFT				(29)
#define COMMAND_SHIFT				(27)
#define PM_SECTION_SHIFT			(26)
#define MASK_SHIFT				(25)
#define INIT_MODE_SHIFT				(22)
#define ASV_MODE_SHIFT				(21)
#define CL_ALL_STOP_SHIFT			(30)
#define CL_ALL_START_SHIFT			(31)
#define MULTI_BYTE_SHIFT			(16)
#define CL_DOMAIN_SHIFT				(14)

#define MULTI_BYTE_CNT_SHIFT			(16)
#define ATLAS_SHIFT				(0)
#define APOLLO_SHIFT				(4)
#define G3D_SHIFT				(8)
#define MIF_SHIFT				(12)
#define PERIOD_SHIFT				(16)
#define BYTE_SHIFT				(8)
#define WRITE_MODE				(0)
#define READ_MODE				(1)
#define NONE					(2)
#define TX_INTERRUPT_ENABLE			(1)
#define MASK					(1)
#define BYTE_4					(4)
#define INIT_SET				(1)
#define ASV_SET					(1)
#define DEBUG_COUNT				(10)

/* Mask variables */
#define CL_DVFS_MASK				(1)
#define COMMAND_MASK				(0x3)
#define MULTI_BYTE_MASK				(0xF)
#define CL_DVFS					(CL_DVFS_MASK << CL_DVFS_SHIFT)
#define CL_DVFS_OFF				(0)
#define COMMAND					(COMMAND_MASK << COMMAND_SHIFT)
#define MULTI_BYTE				(MULTI_BYTE_MASK << MULTI_BYTE_SHIFT)

/* Error variable */
#define APM_RET_SUCESS				(0xa)
#define APM_GPIO_ERR				(0xFFFFFFFF)
#define PMIC_NO_ACK_ERR				(0xEEEEEEEE)
#define ERR_TIMEOUT				(1)
#define ERR_RETRY				(2)
#define ERR_OUT					(3)
#define RETRY_ERR				(-0xFF)

/* apm related variables */
#define MBOX_LEN				(4)
#define TIMEOUT					(500)		/* timeout 500 msec */
#define TX					(0)
#define RX					(1)
#define HSI2C_MODE				(0)
#define APM_MODE				(1)
#define APM_TIMOUT				(2)

#define CL_ON					(1)
#define CL_OFF					(0)

#define APM_OFF					(0)
#define APM_ON					(1)

struct apm_ops {
	int (*apm_update_bits) (unsigned int type, unsigned int reg,
					unsigned int mask, unsigned int value);
	int (*apm_write) (unsigned int type, unsigned int reg, unsigned int value);
	int (*apm_bulk_write) (unsigned int type, unsigned char reg,
					unsigned char *buf, unsigned int count);
	int (*apm_read) (unsigned int type, unsigned int reg, unsigned int *val);
	int (*apm_bulk_read) (unsigned int type, unsigned char reg,
				unsigned char *buf, unsigned int count);
};

struct debug_data {
	u32 buf[DEBUG_COUNT][5];
	s64 time[DEBUG_COUNT];
	char* name[DEBUG_COUNT];
	unsigned int cnt;
#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	u32 vol[DEBUG_COUNT][4];
	u32 atl_value;
	u32 apo_value;
	u32 g3d_value;
	u32 mif_value;
#endif
};

struct cl_init_data {
	u32 atlas_margin;
	u32 apollo_margin;
	u32 g3d_margin;
	u32 mif_margin;
	u32 period;
	u32 cl_status;
	u32 apm_status;
};

extern void cl_dvfs_lock(void);
extern void cl_dvfs_unlock(void);
extern int cm3_status_open(struct inode *inode, struct file *file);
extern struct apm_ops exynos_apm_function_ops;
extern int register_apm_notifier(struct notifier_block *nb);
extern int unregister_apm_notifier(struct notifier_block *nb);
extern int apm_notifier_call_chain(unsigned long val);
extern void exynos7420_apm_power_up(void);
extern void exynos7420_apm_power_down(void);
extern int exynos7420_cl_dvfs_setup(unsigned int atlas_cl_limit, unsigned int apollo_cl_limit, unsigned int g3d_cl_limit,
									unsigned int mif_cl_limit, unsigned int cl_period);
extern int exynos7420_cl_dvfs_start(unsigned int cl_domain);
extern int exynos7420_cl_dvfs_stop(unsigned int cl_domain, unsigned int level);
extern int exynos7420_cl_dvfs_mode_enable(void);
extern int exynos7420_cl_dvfs_mode_disable(void);
extern int exynos7420_g3d_power_on_noti_apm(void);
extern int exynos7420_g3d_power_down_noti_apm(void);
extern int exynos7420_apm_enter_wfi(void);
extern int exynos7420_apm_update_bits(unsigned int type, unsigned int reg, unsigned int mask, unsigned int value);
extern int exynos7420_apm_write(unsigned int type, unsigned int address, unsigned int value);
extern int exynos7420_apm_bulk_write(unsigned int type, unsigned char reg, unsigned char *buf, unsigned int count);
extern int exynos7420_apm_read(unsigned int type, unsigned int reg, unsigned int *val);
extern int exynos7420_apm_bulk_read(unsigned int type, unsigned char reg, unsigned char *buf, unsigned int count);
#endif
