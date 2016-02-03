/* linux/arch/arm/mach-exynos/apm-exynos7420.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS7420 - APM driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/err.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mailbox_client.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/mfd/samsung/core.h>
#include <asm/io.h>
#include <mach/apm-exynos.h>
#include <mach/asv-exynos.h>
#include <mach/regs-pmu-exynos7420.h>
#include "mailbox-exynos.h"

static int apm_wfi_prepare = 1;
static int cm3_status;
static unsigned int cl_mode_status = CL_ON;
static DEFINE_MUTEX(cl_mutex);
static DEFINE_MUTEX(cl_lock);
char* protocol_name;

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
u32 mif_in_voltage;
u32 atl_in_voltage;
u32 apo_in_voltage;
u32 g3d_in_voltage;
#endif

struct mbox_client cl;

void cl_dvfs_lock(void)
{
	mutex_lock(&cl_lock);
}
EXPORT_SYMBOL_GPL(cl_dvfs_lock);

void cl_dvfs_unlock(void)
{
	mutex_unlock(&cl_lock);
}
EXPORT_SYMBOL_GPL(cl_dvfs_unlock);

/* Routines for PM-transition notifications */
static BLOCKING_NOTIFIER_HEAD(apm_chain_head);

int register_apm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&apm_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_apm_notifier);

int unregister_apm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&apm_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_apm_notifier);

int apm_notifier_call_chain(unsigned long val)
{
	int ret;

	ret = blocking_notifier_call_chain(&apm_chain_head, val, NULL);

	return notifier_to_errno(ret);
}

void exynos7420_apm_power_up(void)
{
	u32 tmp;

	tmp = __raw_readl(EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
	tmp &= APM_LOCAL_PWR_CFG_RESET;
	tmp |= APM_LOCAL_PWR_CFG_RUN;
	__raw_writel(tmp, EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
}

void exynos7420_apm_power_down(void)
{
	u32 tmp;

	/* Reset CORTEX M3 */
	tmp = __raw_readl(EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
	tmp &= APM_LOCAL_PWR_CFG_RESET;
	__raw_writel(tmp, EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION);
}

/* check_rx_data function check return value.
 * CM3 send return value. 0xA is sucess, 0x0 is fail value.
 * check_rx_data function check this value. So sucess return 0.
 */
static int check_rx_data(void *msg)
{
	u8 i;
	u32 buf[5] = {0, 0, 0, 0, 0};

	for (i = 0; i < MBOX_LEN; i++)
		buf[i] = __raw_readl(EXYNOS_MAILBOX_RX(i));

	/* Check return command */
	buf[4] = __raw_readl(EXYNOS_MAILBOX_TX(0));

	/* Check apm device return value */
	if (buf[1] == APM_GPIO_ERR)
		return APM_GPIO_ERR;

	/* PMIC No ACK return value */
	if (buf[1] == PMIC_NO_ACK_ERR)
		return PMIC_NO_ACK_ERR;

	/* Multi byte condition */
	if ((buf[4] >> MULTI_BYTE_SHIFT) & MULTI_BYTE_MASK) {
		return 0;
	}

	/* Normal condition */
	if (((buf[4] >> COMMAND_SHIFT) & COMMAND_MASK) != READ_MODE) {
		if (buf[1] == APM_RET_SUCESS) {
			return 0;
		} else {
			pr_err("mailbox err : return incorrect\n");
			data_history();
			return -1;
		}
	} else if (((buf[4] >> COMMAND_SHIFT) & COMMAND_MASK) == READ_MODE) {
		return buf[1];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(check_rx_data);

/* Setting channel ack_mode condition */
static void channel_ack_mode(struct mbox_client *client)
{
	client->rx_callback = NULL;
	client->tx_done = NULL;
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	client->tx_block = true;
#endif
#ifdef CONFIG_EXYNOS_MBOX_POLLING
	client->tx_block = NULL;
#endif
	client->tx_tout = TIMEOUT;
	client->link_data = NULL;
	client->knows_txdone = false;
	client->chan_name = "samsung_mbox:exynos-apm";
}
EXPORT_SYMBOL_GPL(channel_ack_mode);

static int exynos_send_message(struct mbox_client *mbox_cl, void *msg)
{
	struct mbox_chan *chan;
	int ret;

	chan = mbox_request_channel(mbox_cl);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		return PTR_ERR(chan);
	}

	if (!mbox_send_message(chan, (void *)msg)) {
		ret = check_rx_data((void *)msg);
		if (ret == APM_GPIO_ERR) {
			pr_err("mailbox : gpio not set to gpio-i2c \n");
			apm_wfi_prepare = 1;
			mbox_free_channel(chan);
			return ERR_TIMEOUT;
		} else if (ret < 0) {
			pr_err("[%s] mailbox send error \n", __func__);
			mbox_free_channel(chan);
			return ERR_OUT;
		}
	} else {
		pr_err("%s : Mailbox timeout\n", __func__);
		apm_wfi_prepare = 1;
		mbox_free_channel(chan);
		return ERR_TIMEOUT;
	}
	mbox_free_channel(chan);

       return 0;
}

static int exynos_send_message_bulk_read(struct mbox_client *mbox_cl, void *msg)
{
	struct mbox_chan *chan;
	int ret;

	chan = mbox_request_channel(mbox_cl);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		return PTR_ERR(chan);
	}

	if (!mbox_send_message(chan, (void *)msg)) {
		ret = check_rx_data((void *)msg);
		if (ret < 0) {
			pr_err("[%s] mailbox send error \n", __func__);
			return ERR_RETRY;
		} else if (ret == APM_GPIO_ERR) {
			apm_wfi_prepare = 1;
			mbox_free_channel(chan);
			return ERR_TIMEOUT;
		}
	} else {
		pr_err("%s : Mailbox timeout error \n", __func__);
		apm_wfi_prepare = 1;
		mbox_free_channel(chan);
		return ERR_TIMEOUT;
	}
	mbox_free_channel(chan);

return 0;
}

static int exynos7420_do_cl_dvfs_setup(unsigned int atlas_cl_limit, unsigned int apollo_cl_limit,
					unsigned int g3d_cl_limit, unsigned int mif_cl_limit, unsigned int cl_period)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "setup";

	msg[0] = (NONE << COMMAND_SHIFT) | (INIT_SET << INIT_MODE_SHIFT);
	msg[1] = (atlas_cl_limit << ATLAS_SHIFT) | (apollo_cl_limit << APOLLO_SHIFT)
		| (g3d_cl_limit << G3D_SHIFT) | (mif_cl_limit << MIF_SHIFT) | (cl_period << PERIOD_SHIFT);
	msg[3] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
/* out means turn off apm device and then mode change */
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_do_cl_dvfs_setup);

/* exynos7420_cl_dvfs_setup()
 * exynos7420_cl_dvfs_setup set voltage margin limit and period.
 */
int exynos7420_cl_dvfs_setup(unsigned int atlas_cl_limit, unsigned int apollo_cl_limit,
					unsigned int g3d_cl_limit, unsigned int mif_cl_limit, unsigned int cl_period)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = exynos7420_do_cl_dvfs_setup(atlas_cl_limit, apollo_cl_limit,
					g3d_cl_limit, mif_cl_limit, cl_period);
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos7420_cl_dvfs_setup);

static int exynos7420_do_cl_dvfs_start(unsigned int cl_domain)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);

	/* CL-DVFS[29] start, command mode none */
	msg[0] = CL_DVFS | (NONE << COMMAND_SHIFT);
	msg[3] = ((cl_domain + 1) << CL_DOMAIN_SHIFT) | TX_INTERRUPT_ENABLE;

	if (cl_domain == ID_CL1)
		protocol_name = "cl_start(ATL)--";
	else if (cl_domain == ID_CL0)
		protocol_name = "cl_start(APO)--";
	else if (cl_domain == ID_MIF)
		protocol_name = "cl_start(MIF)--";
	else if (cl_domain == ID_G3D)
		protocol_name = "cl_start(G3D)--";

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
/* out means turn off apm device and then mode change */
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_do_cl_dvfs_start);

/* exynos7420_cl_dvfs_start()
 * cl_dvfs_start means os send cl_dvfs start command.
 * We change voltage and frequency, after than start cl-dvfs.
 */
int exynos7420_cl_dvfs_start(unsigned int cl_domain)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	if (cl_mode_status)
		ret = exynos7420_do_cl_dvfs_start(cl_domain);
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos7420_cl_dvfs_start);

static int exynos7420_do_cl_dvfs_mode_enable(void)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "cl_mode_enable";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = (1 << CL_ALL_START_SHIFT);
	msg[3] = (TX_INTERRUPT_ENABLE);

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
/* out means turn off apm device and then mode change */
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_do_cl_dvfs_mode_enable);

/* exynos7420_cl_dvfs_mode_disable()
 * cl_dvfs_stop means os send cl_dvfs stop command to CM3.
 * We need change voltage and frequency. first, we stop cl-dvfs.
 */
int exynos7420_cl_dvfs_mode_enable(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = exynos7420_do_cl_dvfs_mode_enable();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos7420_cl_dvfs_mode_enable);

static int exynos7420_do_cl_dvfs_mode_disable(void)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "cl_mode_disable";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = (1 << CL_ALL_STOP_SHIFT);
	msg[3] = (TX_INTERRUPT_ENABLE);

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
/* out means turn off apm device and then mode change */
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_do_cl_dvfs_mode_disable);

/* exynos7420_cl_dvfs_mode_disable()
 * cl_dvfs_stop means os send cl_dvfs stop command to CM3.
 * We need change voltage and frequency. first, we stop cl-dvfs.
 */
int exynos7420_cl_dvfs_mode_disable(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = exynos7420_do_cl_dvfs_mode_disable();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos7420_cl_dvfs_mode_disable);

static int exynos7420_do_cl_dvfs_stop(unsigned int cl_domain, unsigned int level)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	if (cl_domain == ID_G3D) {
		/* G3D driver not use level 0, 1 */
		level = level + 1;
	}

	channel_ack_mode(&cl);
	protocol_name = "cl_stop";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = (CL_DVFS_OFF << CL_DVFS_SHIFT) | (NONE << COMMAND_SHIFT);
	msg[1] = level;
	msg[3] = ((cl_domain + 1) << CL_DOMAIN_SHIFT) | (TX_INTERRUPT_ENABLE);

	if (cl_domain == ID_CL1)
		protocol_name = "cl_stop(ATL)++";
	else if (cl_domain == ID_CL0)
		protocol_name = "cl_stop(APO)++";
	else if (cl_domain == ID_MIF)
		protocol_name = "cl_stop(MIF)++";
	else if (cl_domain == ID_G3D)
		protocol_name = "cl_stop(G3D)++";

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
/* out means turn off apm device and then mode change */
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_do_cl_dvfs_stop);

/* exynos7420_cl_dvfs_stop()
 * cl_dvfs_stop means os send cl_dvfs stop command to CM3.
 * We need change voltage and frequency. first, we stop cl-dvfs.
 */
int exynos7420_cl_dvfs_stop(unsigned int cl_domain, unsigned int level)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	if (cl_mode_status)
		ret = exynos7420_do_cl_dvfs_stop(cl_domain, level);
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos7420_cl_dvfs_stop);

static int exynos7420_do_g3d_power_on_noti_apm(void)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "g3d_power_on";

	/* CL-DVFS[29] stop, command mode none */
	msg[3] = TX_INTERRUPT_ENABLE | (1 << 13);

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_do_g3d_power_on_noti_apm);

/* exynos7420_g3d_power_on_noti_apm()
 * APM driver notice g3d power on status to CM3.
 */
int exynos7420_g3d_power_on_noti_apm(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = exynos7420_do_g3d_power_on_noti_apm();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos7420_g3d_power_on_noti_apm);

static int exynos7420_do_g3d_power_down_noti_apm(void)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret = 0;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "g3d_power_off";

	/* CL-DVFS[29] stop, command mode none */
	msg[3] = TX_INTERRUPT_ENABLE | (1 << 12);

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return 0;
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_do_g3d_power_down_noti_apm);

/* exynos7420_g3d_power_on_noti_apm()
 * APM driver notice g3d power off status to CM3.
 */
int exynos7420_g3d_power_down_noti_apm(void)
{
	int ret = 0;

	sec_core_lock();
	cl_dvfs_lock();
	ret = exynos7420_do_g3d_power_down_noti_apm();
	cl_dvfs_unlock();
	sec_core_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(exynos7420_g3d_power_down_noti_apm);

/* exynos7420_apm_enter_wfi();
 * This function send CM3 go to WFI message to CM3.
 */
int exynos7420_apm_enter_wfi(void)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	struct mbox_chan *chan;

	mutex_lock(&cl_mutex);

	if (apm_wfi_prepare) {
		mutex_unlock(&cl_mutex);
		return 0;
	}

	channel_ack_mode(&cl);
	protocol_name = "enter_wfi";

	/* CL-DVFS[29] stop, command mode none */
	msg[0] = 1 << 23;
	msg[3] = TX_INTERRUPT_ENABLE;

	chan = mbox_request_channel(&cl);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		mutex_unlock(&cl_mutex);
		return PTR_ERR(chan);
	}
	if (!mbox_send_message(chan, (void *)msg)) {
	}
	mbox_free_channel(chan);
	mutex_unlock(&cl_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos7420_apm_enter_wfi);

/**
 * exynos7420_apm_update_bits(): Mask a value, after then write value.
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @mask : masking value
 * @value : Pointer to store write value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos7420_apm_update_bits(unsigned int type, unsigned int reg,
					unsigned int mask, unsigned int value)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "update_bits";

	/* CL-DVFS[29] stop, command mode write(0x0), mask mode enable */
	msg[0] = ((type << PM_SECTION_SHIFT) | (MASK << MASK_SHIFT) | (mask));
	msg[1] = reg;
	msg[2] = value;
	msg[3] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_apm_update_bits);

/**
 * exynos7420_apm_write()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @value : Pointer to store write value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos7420_apm_write(unsigned int type, unsigned int reg, unsigned int value)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	int ret;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "write";

	/* CL-DVFS[29] stop, command mode write(0x0) */
	msg[0] = (type << PM_SECTION_SHIFT);
	msg[1] = reg;
	msg[2] = value;
	msg[3] = TX_INTERRUPT_ENABLE;

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	if (reg == 0x1a) {
		mif_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	} else if (reg == 0x1c) {
		atl_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	} else if (reg == 0x1E) {
		apo_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	} else if (reg == 0x24) {
		g3d_in_voltage = ((value * (u32)PMIC_STEP) + MIN_VOL);
	}
#endif

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_apm_write);

/**
 * exynos7420_apm_bulk_write()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @*buf: write buffer section
 * @value : Pointer to store write value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos7420_apm_bulk_write(unsigned int type, unsigned char reg, unsigned char *buf, unsigned int count)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	unsigned int i, shift;
	int ret;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "bulk_write";

	msg[0] = (type << PM_SECTION_SHIFT) | ((count-1) << MULTI_BYTE_CNT_SHIFT) | reg;

	for (i = 0; i < count; i++) {
		shift = ((count-1)-i) * BYTE_SHIFT;
		if (shift > 31)  {
			msg[1] |= buf[i] << (shift-32);
		} else {
			msg[2] |= buf[i] << shift;
		}
	}
	msg[3] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message(&cl, msg);
	if (ret == ERR_TIMEOUT || ret == ERR_OUT) {
		data_history();
		goto timeout;
	} else if (ret) {
		goto error;
	}

	mutex_unlock(&cl_mutex);

	return ret;
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
error :
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_apm_bulk_write);

/**
 * exynos7420_apm_read()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @*val: store read value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos7420_apm_read(unsigned int type, unsigned int reg, unsigned int *val)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	struct mbox_chan *chan;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "read";

	/* CL-DVFS[29] stop, command mode read(0x1) */
	msg[0] = (READ_MODE << COMMAND_SHIFT) | (type << PM_SECTION_SHIFT);
	msg[1] = reg;
	msg[3] = TX_INTERRUPT_ENABLE;

	chan = mbox_request_channel(&cl);
	if (IS_ERR(chan)) {
		pr_err("mailbox : Did not make a mailbox channel\n");
		mutex_unlock(&cl_mutex);
		return PTR_ERR(chan);
	}

	if (!mbox_send_message(chan, (void *)msg)) {
		*val = check_rx_data((void *)msg);
		if (*val == APM_GPIO_ERR) {
			pr_err("%s, gpio error\n", __func__);
			mbox_free_channel(chan);
			data_history();
			goto timeout;
		}
	} else {
		pr_err("%s : Mailbox timeout error \n", __func__);
		apm_wfi_prepare = 1;
		mbox_free_channel(chan);
		data_history();
		goto timeout;
	}

	mbox_free_channel(chan);
	mutex_unlock(&cl_mutex);
	return 0;
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_apm_read);

/**
 * exynos7420_apm_bulk_read()
 * @type: Register pmic section (pm_section(0), rtc_section(1))
 * @reg: register address
 * @*buf: read buffer section
 * @*count : read count
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int exynos7420_apm_bulk_read(unsigned int type, unsigned char reg, unsigned char *buf, unsigned int count)
{
	u32 msg[MBOX_LEN] = {0, 0, 0, 0};
	u32 result[2] = {0, 0};
	unsigned int ret, i, shift;

	mutex_lock(&cl_mutex);
	channel_ack_mode(&cl);
	protocol_name = "bulk_read";

	msg[0] = (READ_MODE << COMMAND_SHIFT) | (type << PM_SECTION_SHIFT)
			| ((count-1) << MULTI_BYTE_CNT_SHIFT) | (reg);
	msg[3] = TX_INTERRUPT_ENABLE;

	ret = exynos_send_message_bulk_read(&cl, msg);
	if (ret == ERR_TIMEOUT) {
		data_history();
		goto timeout;
	}

	result[0] = __raw_readl(EXYNOS_MAILBOX_RX(1));
	result[1] = __raw_readl(EXYNOS_MAILBOX_RX(2));

	for (i = 0; i < count; i++) {
		shift = ((count-1)-i) * BYTE_SHIFT;
		if (shift > 31)  {
			buf[i] = result[0] >> (shift-32);
		} else {
			buf[i] = result[1] >> shift;
		}
	}

	mutex_unlock(&cl_mutex);

	return 0;
timeout :
	exynos7420_apm_power_down();
	apm_notifier_call_chain(APM_TIMEOUT);
	mutex_unlock(&cl_mutex);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(exynos7420_apm_bulk_read);

struct apm_ops exynos_apm_function_ops = {
	.apm_update_bits	= exynos7420_apm_update_bits,
	.apm_write		= exynos7420_apm_write,
	.apm_bulk_write		= exynos7420_apm_bulk_write,
	.apm_read		= exynos7420_apm_read,
	.apm_bulk_read		= exynos7420_apm_bulk_read,
};

static int exynos7420_cm3_status_show(struct seq_file *buf, void *d)
{
	/* Show pmic communcation mode */
	if (cm3_status == HSI2C_MODE) seq_printf(buf, "mode : HSI2C \n");
	else if (cm3_status == APM_MODE) seq_printf(buf, "mode : APM \n");
	else if (cm3_status == APM_TIMOUT) seq_printf(buf, "mode : HSI2C (CM3 timeout) \n");

	return 0;
}

int cm3_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos7420_cm3_status_show, inode->i_private);
}

#ifdef CONFIG_EXYNOS_MBOX
static int exynos7420_apm_function_notifier(struct notifier_block *notifier,
						unsigned long pm_event, void *v)
{
	switch (pm_event) {
		case APM_READY:
			cm3_status = APM_MODE;
			apm_wfi_prepare = 0;
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
			samsung_mbox_enable_irq();
#endif
			pr_info("mailbox: hsi2c -> apm mode \n");
			break;
		case APM_SLEEP:
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
			if (cm3_status != APM_TIMOUT)
				samsung_mbox_disable_irq();
#endif
			if (cm3_status == APM_MODE)
				pr_info("mailbox: apm -> hsi2c mode \n");
			cm3_status = HSI2C_MODE;
			apm_wfi_prepare = 1;
			break;
		case APM_TIMEOUT:
			cm3_status = APM_TIMOUT;
			apm_wfi_prepare = 1;
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
			samsung_mbox_disable_irq();
#endif
			pr_info("mailbox: apm -> hsi2c mode(timeout) \n");
			break;
		case CL_ENABLE:
			cl_mode_status = CL_ON;
			break;
		case CL_DISABLE:
			cl_mode_status = CL_OFF;
			break;
		default:
			break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_apm_notifier = {
	.notifier_call = exynos7420_apm_function_notifier,
};
#endif

static int exynos7420_apm_probe(struct platform_device *pdev)
{
#ifdef CONFIG_EXYNOS_MBOX
	register_apm_notifier(&exynos_apm_notifier);
#endif
	return 0;
}

static int exynos7420_apm_remove(struct platform_device *pdev)
{
#ifdef CONFIG_EXYNOS_MBOX
	unregister_apm_notifier(&exynos_apm_notifier);
#endif
	return 0;
}

static const struct of_device_id apm_smc_match[] = {
	{ .compatible = "samsung,exynos-apm" },
	{},
};

static struct platform_driver exynos7420_apm_driver = {
	.probe	= exynos7420_apm_probe,
	.remove = exynos7420_apm_remove,
	.driver	= {
		.name = "exynos-apm-driver",
		.owner	= THIS_MODULE,
		.of_match_table	= apm_smc_match,
	},
};

static int __init exynos7420_apm_init(void)
{
	return platform_driver_register(&exynos7420_apm_driver);
}
late_initcall(exynos7420_apm_init);

static void __exit exynos7420_apm_exit(void)
{
	platform_driver_unregister(&exynos7420_apm_driver);
}
module_exit(exynos7420_apm_exit);
