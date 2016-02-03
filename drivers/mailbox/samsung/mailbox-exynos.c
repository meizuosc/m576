/*
 * Copyright 2013 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/syscore_ops.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/reboot.h>
#include <linux/fb.h>
#include <linux/mfd/samsung/core.h>
#include <linux/exynos-ss.h>
#include <asm-generic/checksum.h>

#include <mach/asv-exynos.h>
#include <mach/regs-pmu-exynos7420.h>
#include <mach/exynos-pm.h>
#include <mach/apm-exynos.h>
#include <mach/regs-clock-exynos7420.h>
#include "mailbox-exynos.h"

/* firmware file information */
#define fw_checksum	1326

char *firmware_file = "apm_0611_fw_CH5.h";

extern char* protocol_name;

int mbox_irq;
unsigned int sram_status = 0;
unsigned int dram_checksum = 0;
static struct workqueue_struct *mailbox_wq;
static struct cl_init_data cl_init;
static struct class *mailbox_class;
static struct debug_data tx, rx;

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
u32 atl_voltage;
u32 apo_voltage;
u32 g3d_voltage;
u32 mif_voltage;

extern u32 mif_in_voltage;
extern u32 atl_in_voltage;
extern u32 apo_in_voltage;
extern u32 g3d_in_voltage;
#else
u32 default_vol[4] = {100000, 100000, 100000, 100000};
#endif

static bool apm_power_down = false;

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
void samsung_mbox_enable_irq(void)
{
	enable_irq(mbox_irq);
}

void samsung_mbox_disable_irq(void)
{
	disable_irq(mbox_irq);
}
#endif

static inline struct samsung_mlink *to_samsung_mlink(struct mbox_link *plink)
{
	if (!plink)
		return NULL;

	return container_of(plink, struct samsung_mlink, link);
}

static inline struct samsung_mbox *to_samsung_mbox(struct mbox_link *plink)
{
	if (!plink)
		return NULL;

	return to_samsung_mlink(plink)->smc;
}

static void firmware_load(const char *firmware, int size)
{
	dram_checksum = csum_partial(firmware, size, 0);

	memcpy(EXYNOS_MAILBOX_SRAM, firmware, size);

	pr_info("OK \n");
}

static int firmware_update(struct device *dev)
{
	const struct firmware *fw_entry = NULL;
	int err;

	dev_info(dev, "Loading APM firmware ... ");
	err = request_firmware(&fw_entry, firmware_file, dev);
	if (err) {
		dev_err(dev, "FAIL \n");
		return err;
	}

	firmware_load(fw_entry->data, fw_entry->size);

	release_firmware(fw_entry);

	return 0;
}

static void exynos7420_apm_reset_release(void)
{
	unsigned int tmp;

	/* Cortex M3 Interrupt bit clear */
	__raw_writel(0x0, EXYNOS_MAILBOX_TX_INT);
	__raw_writel(0x0, EXYNOS_MAILBOX_RX_INT);

	/* Set APM device enable */
	tmp = __raw_readl(EXYNOS_PMU_CORTEXM3_APM_OPTION);
	tmp &= ~ENABLE_APM;
	tmp |= ENABLE_APM;
	__raw_writel(tmp, EXYNOS_PMU_CORTEXM3_APM_OPTION);

	tmp = __raw_readl(EXYNOS_MAILBOX_MRX_SEM);
	tmp &= ~MRX_SEM_ENABLE;
	tmp |= MRX_SEM_ENABLE;
	__raw_writel(tmp, EXYNOS_MAILBOX_MRX_SEM);
}

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
static irqreturn_t samsung_ipc_handler(int irq, void *p)
{
	struct samsung_mlink *plink = p;
	u32 tmp;
	u32 i;

	mbox_link_txdone(&plink->link, MBOX_OK);

	/* Acknowledge the interrupt by clearing the interrupt register */
	tmp = __raw_readl(EXYNOS_MAILBOX_RX_INT);
	tmp &= ~RX_INT_CLEAR;
	__raw_writel(tmp, EXYNOS_MAILBOX_RX_INT);

	/* Debug information */
	rx.buf[rx.cnt][G3D_STATUS] = (__raw_readl(EXYNOS_PMU_G3D_STATUS) & G3D_STATUS_MASK);
	rx.name[rx.cnt] = protocol_name;
	for (i = 0; i < MAILBOX_REG_CNT; i++) {
		rx.time[rx.cnt] = ktime_to_ms(ktime_get());
		rx.buf[rx.cnt][i] = __raw_readl(EXYNOS_MAILBOX_RX(i));
	}
#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	/* Debug register clear */
	__raw_writel(0x0, EXYNOS_MAILBOX_RX(0));

	/* Check firmware setting voltage */
	rx.atl_value = ((rx.buf[rx.cnt][0] >> ATL_VOL_SHIFT) & VOL_MASK);
	rx.apo_value = ((rx.buf[rx.cnt][0] >> APO_VOL_SHIFT) & VOL_MASK);
	rx.g3d_value = ((rx.buf[rx.cnt][0] >> G3D_VOL_SHIFT) & VOL_MASK);
	rx.mif_value = ((rx.buf[rx.cnt][0] >> MIF_VOL_SHIFT) & VOL_MASK);

	atl_voltage = ((rx.atl_value * (u32)PMIC_STEP) + MIN_VOL);
	apo_voltage = ((rx.apo_value * (u32)PMIC_STEP) + MIN_VOL);
	g3d_voltage = ((rx.g3d_value * (u32)PMIC_STEP) + MIN_VOL);
	mif_voltage = ((rx.mif_value * (u32)PMIC_STEP) + MIN_VOL);

	rx.vol[rx.cnt][0] = atl_voltage;
	rx.vol[rx.cnt][1] = apo_voltage;
	rx.vol[rx.cnt][2] = g3d_voltage;
	rx.vol[rx.cnt][3] = mif_voltage;
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, rx.vol[rx.cnt]);
#else
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, default_vol);
#endif

	rx.cnt++;
	if (rx.cnt == DEBUG_COUNT) rx.cnt = 0;

	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
static int samsung_mbox_startup(struct mbox_link *link, void *ignored)
{
	return 0;
}
#else
static int samsung_mbox_startup(struct mbox_link *link, void *ignored) { return 0;}
#endif

static int samsung_mbox_send_data(struct mbox_link *link, void *msg)
{
	struct samsung_mlink *samsung_link = to_samsung_mlink(link);
	u32 *msg_data = (u32 *)msg;
	u32 i, status, limit_cnt = 0, tmp;

	samsung_link->data = msg_data;

	/* Check cortex m3 hardware status */
	tmp = __raw_readl(EXYNOS_PMU_CORTEXM3_APM_STATUS);
	status = (((tmp >> STANDBYWFI) & STANDBYWFI_MASK) & (tmp & APM_STATUS_MASK));
	if (status)
		return -1;

	/* Check rx interrupt status */
	do {
		status = __raw_readl(EXYNOS_MAILBOX_RX(3)) & CM3_INTERRPUT_SHIFT;
		limit_cnt++;
		if (limit_cnt > CM3_COUNT_MAX) return -1;
	} while (status);

	tx.buf[tx.cnt][G3D_STATUS] = (__raw_readl(EXYNOS_PMU_G3D_STATUS) & G3D_STATUS_MASK);
	tx.name[tx.cnt] = protocol_name;

	limit_cnt = 0;
	/* Check Tx interrupt status */
	do {
		status = __raw_readl(EXYNOS_MAILBOX_TX(3)) & CM3_INTERRPUT_SHIFT;
		limit_cnt++;
		if (limit_cnt > CM3_COUNT_MAX) return -1;
	} while (status);

	/* Save information and data to mailbox SFR */
	for (i = 0; i < MAILBOX_REG_CNT; i++) {
		writel(msg_data[i], EXYNOS_MAILBOX_TX(i));
		tx.buf[tx.cnt][i] = readl(EXYNOS_MAILBOX_TX(i));
		/* Save a time and message information */
		tx.time[tx.cnt] = ktime_to_ms(ktime_get());
	}
#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	tx.vol[rx.cnt][0] = atl_in_voltage;
	tx.vol[rx.cnt][1] = apo_in_voltage;
	tx.vol[rx.cnt][2] = g3d_in_voltage;
	tx.vol[rx.cnt][3] = mif_in_voltage;
	exynos_ss_mailbox(msg_data, 0, protocol_name, tx.vol[rx.cnt]);
#else
	exynos_ss_mailbox(msg_data, 0, protocol_name, default_vol);
#endif

	tx.cnt++;
	if (tx.cnt == DEBUG_COUNT) tx.cnt = 0;

	return 0;
}

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
static void samsung_mbox_shutdown(struct mbox_link *link)
{
}
#else
static void samsung_mbox_shutdown(struct mbox_link *link) {}
#endif

int samsung_mbox_last_tx_done(struct mbox_link *link)
{
	unsigned int status, limit_cnt = 0, tmp, i;

	do {
		if (limit_cnt) {
			cpu_relax();
			usleep_range(28, 30);
		}

		status = __raw_readl(EXYNOS_MAILBOX_RX_INT) & CM3_POLLING_SHIFT;

		limit_cnt++;
		if (limit_cnt > 100) return -EIO;
	} while(!status);

	/* Acknowledge the interrupt by clearing the interrupt register */
	tmp = __raw_readl(EXYNOS_MAILBOX_RX_INT);
	tmp &= ~RX_INT_CLEAR;
	__raw_writel(tmp, EXYNOS_MAILBOX_RX_INT);

	/* Debug information */
	rx.buf[rx.cnt][G3D_STATUS] = (__raw_readl(EXYNOS_PMU_G3D_STATUS) & G3D_STATUS_MASK);
	rx.name[rx.cnt] = protocol_name;
	for (i = 0; i < MAILBOX_REG_CNT; i++) {
		rx.time[rx.cnt] = ktime_to_ms(ktime_get());
		rx.buf[rx.cnt][i] = __raw_readl(EXYNOS_MAILBOX_RX(i));
	}
#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	/* Debug register clear */
	__raw_writel(0x0, EXYNOS_MAILBOX_RX(0));

	/* Check firmware setting voltage */
	rx.atl_value = ((rx.buf[rx.cnt][0] >> ATL_VOL_SHIFT) & VOL_MASK);
	rx.apo_value = ((rx.buf[rx.cnt][0] >> APO_VOL_SHIFT) & VOL_MASK);
	rx.g3d_value = ((rx.buf[rx.cnt][0] >> G3D_VOL_SHIFT) & VOL_MASK);
	rx.mif_value = ((rx.buf[rx.cnt][0] >> MIF_VOL_SHIFT) & VOL_MASK);

	atl_voltage = ((rx.atl_value * (u32)PMIC_STEP) + MIN_VOL);
	apo_voltage = ((rx.apo_value * (u32)PMIC_STEP) + MIN_VOL);
	g3d_voltage = ((rx.g3d_value * (u32)PMIC_STEP) + MIN_VOL);
	mif_voltage = ((rx.mif_value * (u32)PMIC_STEP) + MIN_VOL);

	rx.vol[rx.cnt][0] = atl_voltage;
	rx.vol[rx.cnt][1] = apo_voltage;
	rx.vol[rx.cnt][2] = g3d_voltage;
	rx.vol[rx.cnt][3] = mif_voltage;
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, rx.vol[rx.cnt]);
#else
	exynos_ss_mailbox(rx.buf[rx.cnt], 1, protocol_name, default_vol);
#endif

	rx.cnt++;
	if (rx.cnt == DEBUG_COUNT) rx.cnt = 0;

	return 0;
}

static struct mbox_link_ops samsung_mbox_ops = {
	.send_data = samsung_mbox_send_data,
	.startup = samsung_mbox_startup,
	.shutdown = samsung_mbox_shutdown,
	.last_tx_done = samsung_mbox_last_tx_done,
};

/*
 * [DEBUG] Check mailbox related register and TX/RX data history
 */
int data_history(void) {
#if 0
	int count;
#endif
	pr_info("EXYNOS_MAILBOX_TX_CON : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_TX_CON));
	pr_info("EXYNOS_MAILBOX_TX_ADDR : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_TX_ADDR));
	pr_info("EXYNOS_MAILBOX_TX_DATA : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_TX_DATA));
	pr_info("EXYNOS_MAILBOX_TX_INT : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_TX_INT));
	pr_info("EXYNOS_MAILBOX_RX(0) : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_RX(0)));
	pr_info("EXYNOS_MAILBOX_RX(1) : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_RX(1)));
	pr_info("EXYNOS_MAILBOX_RX(2) : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_RX(2)));
	pr_info("EXYNOS_MAILBOX_RX(3) : %8.0x \n", __raw_readl(EXYNOS_MAILBOX_RX(3)));
	pr_info("EXYNOS7420_CORTEXM3_APM_CONFIGURATION : %8.0x \n", __raw_readl(EXYNOS_PMU_CORTEXM3_APM_CONFIGURATION));
	pr_info("EXYNOS7420_CORTEXM3_APM_STATUS : %8.0x \n", __raw_readl(EXYNOS_PMU_CORTEXM3_APM_STATUS));
	pr_info("EXYNOS7420_CORTEXM3_APM_OPTION : %8.0x \n", __raw_readl(EXYNOS_PMU_CORTEXM3_APM_OPTION));

#if 0
	for (count = 0; count < DEBUG_COUNT; count++) {
		pr_info("[%lld ms]TX data : %8.x %8.x %8.x %8.x %8.x\n", tx.time[count], tx.buf[count][0],
				tx.buf[count][1], tx.buf[count][2], tx.buf[count][3], tx.buf[count][4]);
	}
	for (count = 0; count < DEBUG_COUNT; count++) {
		pr_info("[%lld ms]RX data : %8.x %8.x %8.x %8.x %8.x\n", rx.time[count], rx.buf[count][0],
				rx.buf[count][1], rx.buf[count][2], rx.buf[count][3], rx.buf[count][4]);
	}
#endif
	return 0;
}

static void thread_mailbox_work(struct work_struct *work)
{
	const struct firmware *fw_entry = NULL;
	unsigned int tmp, status, limit_cnt = 0;
	int ret, err;
	unsigned int sram_checksum = 0;

	if (!apm_power_down) {
		err = request_firmware(&fw_entry, firmware_file, NULL);
		if (err) {
			pr_err("mailbox : request firmware fail \n");
		}

		sram_checksum = csum_partial(EXYNOS_MAILBOX_SRAM, fw_entry->size, 0);
		ret = memcmp(EXYNOS_MAILBOX_SRAM, fw_entry->data, fw_entry->size);
		if (ret) {
			/* Cortex M3 compare error case */
			sram_status = SRAM_UNSTABLE;
			pr_info("mailbox : APM SRAM compare error\n");
		} else {
			/* Cortex M3 compare sucess case */
			sram_status = SRAM_STABLE;
			pr_info("mailbox : APM SRAM stable \n");
		}
		pr_info("mailbox : fw_checksum [%d], DRAM checksum [%d], sram checksum [%d] \n",
								fw_checksum, dram_checksum, sram_checksum);

		release_firmware(fw_entry);

		/* This condition is lcd on */
		/* Local power up to cortex M3 */
		if (sram_status == SRAM_STABLE) {
			exynos7420_apm_power_up();

			/* Enable CORTEX M3 */
			exynos7420_apm_reset_release();

			cl_init.apm_status = APM_ON;
			/* Call apm notifier */
			sec_core_lock();
			cl_dvfs_lock();
			apm_notifier_call_chain(APM_READY);
			cl_dvfs_unlock();
			sec_core_unlock();

			/* Set CL-DVFS voltage margin limit and CL-DVFS period */
			ret = exynos7420_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
							cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
			if (ret)
				pr_warn("mailbox : Do not set margin and period information\n");
		}
	} else {
		/* This condition is lcd off */
		if (sram_status == SRAM_STABLE) {
#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
			exynos7420_cl_dvfs_mode_disable();
			cl_init.apm_status = APM_OFF;
#endif
			sec_core_lock();
			cl_dvfs_lock();
			apm_notifier_call_chain(APM_SLEEP);
			cl_dvfs_unlock();
			sec_core_unlock();

			/* send message go to wfi */
			exynos7420_apm_enter_wfi();

			/* Check cortex M3 core enter wfi mode */
			do {
				tmp = __raw_readl(EXYNOS_PMU_CORTEXM3_APM_STATUS);
				status = (tmp >> STANDBYWFI) & STANDBYWFI_MASK;
				limit_cnt++;
				if (limit_cnt > CM3_COUNT_MAX) break;
			} while (!status);

			/* local power down(reset) to CORTEX M3 */
			exynos7420_apm_power_down();
		}
	}
}
static DECLARE_WORK(mailbox_work, thread_mailbox_work);

static int exynos7420_mailbox_notifier(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	int ret = NOTIFY_DONE;
	int tmp;

	switch (cmd) {
	case LPA_PREPARE:
		tmp = __raw_readl(EXYNOS_PMU_CORTEXM3_APM_STATUS);
		if (tmp & APM_LOCAL_PWR_CFG_RUN)
			ret = -EBUSY;
		break;
	default:
		break;
	}

	return notifier_from_errno(ret);
}

static struct notifier_block exynos_mailbox_notifier_block = {
	.notifier_call = exynos7420_mailbox_notifier,
};

static int exynos7420_mailbox_fb_notifier(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	unsigned int blank;

	if (val != FB_EVENT_BLANK &&
		val != FB_R_EARLY_EVENT_BLANK)
		return 0;
	/*
	 * If FBNODE is not zero, it is not primary display(LCD)
	 * and don't need to process these scheduling.
	 */
	if (info->node)
		return NOTIFY_OK;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		if (mailbox_wq) {
			if (work_pending(&mailbox_work))
				flush_work(&mailbox_work);
			apm_power_down = true;
			queue_work(mailbox_wq, &mailbox_work);
		}
		break;
	case FB_BLANK_UNBLANK:
		if (mailbox_wq) {
			if (work_pending(&mailbox_work))
				flush_work(&mailbox_work);
			apm_power_down = false;
			queue_work(mailbox_wq, &mailbox_work);
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_mailbox_fb_notifier_block = {
	.notifier_call = exynos7420_mailbox_fb_notifier,
};

static int exynos_mailbox_reboot_notifier_call(struct notifier_block *this,
				   unsigned long code, void *_cmd)
{
	/* This condition is lcd off */
#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
	exynos7420_cl_dvfs_mode_disable();
#endif
	sec_core_lock();
	cl_dvfs_lock();
	apm_notifier_call_chain(APM_SLEEP);
	cl_dvfs_unlock();
	sec_core_unlock();
	pr_info("APM device go to wfi, change to hsi2c \n");

	/* Reset CORTEX M3 */
	exynos7420_apm_power_down();

	return NOTIFY_DONE;
}

static struct notifier_block exynos_mailbox_reboot_notifier = {
	.notifier_call = exynos_mailbox_reboot_notifier_call,
	.priority = (INT_MIN + 1),
};

static int samsung_mbox_probe(struct platform_device *pdev)
{
	struct samsung_mbox *samsung_mbox;
	struct device_node *node = pdev->dev.of_node;
	struct samsung_mlink *mbox_link;
	struct mbox_link **link;
	int loop, count, ret = 0;

	if (!node) {
		dev_err(&pdev->dev, "driver doesnt support"
				"non-dt devices\n");
		return -ENODEV;
	}

	/* read sub link count */
	count = of_property_count_strings(node,
				"samsung,mbox-names");
	if (count <= 0) {
		dev_err(&pdev->dev, "no mbox devices found\n");
		return -ENODEV;
	}

	samsung_mbox = devm_kzalloc(&pdev->dev,
			sizeof(struct samsung_mbox), GFP_KERNEL);

	if (IS_ERR(samsung_mbox))
		return PTR_ERR(samsung_mbox);

	link = devm_kzalloc(&pdev->dev, (count + 1) * sizeof(*link),
			GFP_KERNEL);
	if (IS_ERR(link))
		return PTR_ERR(link);

	/* copy dev information */
	samsung_mbox->dev = &pdev->dev;

	/* Update firmware */
	ret = firmware_update(samsung_mbox->dev);
	if (ret < 0) {
		dev_err(samsung_mbox->dev, "failed update firmware\n");
		return -ENODEV;
	}

	for (loop = 0; loop < count; loop++) {
		mbox_link = &samsung_mbox->samsung_link[loop];

		/* save interrupt information */
		mbox_irq = irq_of_parse_and_map(node, loop);
		if (mbox_irq < 0) {
			dev_err(&pdev->dev, "Failed get irq map \n");
			return -ENODEV;
		}

		mbox_link->smc = samsung_mbox;
		link[loop] = &mbox_link->link;
		if (of_property_read_string_index(node, "samsung,mbox-names",
						  loop, &mbox_link->name)) {
			dev_err(&pdev->dev,
				"mbox_name [%d] read failed\n", loop);
			return -ENODEV;
		}

		snprintf(link[loop]->link_name, 16, mbox_link->name);

		/* Get of cl dvfs related information to DT */
		switch (exynos_get_table_ver()) {
		case 0 :
		case 1 :
		case 4 :
		case 5 :
			ret = of_property_read_u32_index(node, "asv_v0_atlas_margin", 0, &cl_init.atlas_margin);
			if (ret) {
				dev_err(&pdev->dev, "atlas_margin do not set, Set to default 0mV Value\n");
				cl_init.atlas_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v0_apollo_margin", 0, &cl_init.apollo_margin);
			if (ret) {
				dev_err(&pdev->dev, "apollo_margin do not set, Set to default 0mV Value\n");
				cl_init.apollo_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v0_g3d_margin", 0, &cl_init.g3d_margin);
			if (ret) {
				dev_err(&pdev->dev, "g3d_margin do not set, Set to default 0mV Value\n");
				cl_init.g3d_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v0_mif_margin", 0, &cl_init.mif_margin);
			if (ret) {
				dev_err(&pdev->dev, "mif_margin do not set, Set to default 0mV Value\n");
				cl_init.mif_margin = MARGIN_0MV;
			}
			break;
		case 2 :
		case 3 :
		case 6 :
			ret = of_property_read_u32_index(node, "asv_v1_atlas_margin", 0, &cl_init.atlas_margin);
			if (ret) {
				dev_err(&pdev->dev, "atlas_margin do not set, Set to default 0mV Value\n");
				cl_init.atlas_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v1_apollo_margin", 0, &cl_init.apollo_margin);
			if (ret) {
				dev_err(&pdev->dev, "apollo_margin do not set, Set to default 0mV Value\n");
				cl_init.apollo_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v1_g3d_margin", 0, &cl_init.g3d_margin);
			if (ret) {
				dev_err(&pdev->dev, "g3d_margin do not set, Set to default 0mV Value\n");
				cl_init.g3d_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v1_mif_margin", 0, &cl_init.mif_margin);
			if (ret) {
				dev_err(&pdev->dev, "mif_margin do not set, Set to default 0mV Value\n");
				cl_init.mif_margin = MARGIN_0MV;
			}
			break;
		case 7 :
			ret = of_property_read_u32_index(node, "asv_v2_atlas_margin", 0, &cl_init.atlas_margin);
			if (ret) {
				dev_err(&pdev->dev, "atlas_margin do not set, Set to default 0mV Value\n");
				cl_init.atlas_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v2_apollo_margin", 0, &cl_init.apollo_margin);
			if (ret) {
				dev_err(&pdev->dev, "apollo_margin do not set, Set to default 0mV Value\n");
				cl_init.apollo_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v2_g3d_margin", 0, &cl_init.g3d_margin);
			if (ret) {
				dev_err(&pdev->dev, "g3d_margin do not set, Set to default 0mV Value\n");
				cl_init.g3d_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v2_mif_margin", 0, &cl_init.mif_margin);
			if (ret) {
				dev_err(&pdev->dev, "mif_margin do not set, Set to default 0mV Value\n");
				cl_init.mif_margin = MARGIN_0MV;
			}
			break;
		case 8 :
		case 9 :
		case 10 :
		case 11 :
		case 15 :
			ret = of_property_read_u32_index(node, "asv_v3_atlas_margin", 0, &cl_init.atlas_margin);
			if (ret) {
				dev_err(&pdev->dev, "atlas_margin do not set, Set to default 0mV Value\n");
				cl_init.atlas_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v3_apollo_margin", 0, &cl_init.apollo_margin);
			if (ret) {
				dev_err(&pdev->dev, "apollo_margin do not set, Set to default 0mV Value\n");
				cl_init.apollo_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v3_g3d_margin", 0, &cl_init.g3d_margin);
			if (ret) {
				dev_err(&pdev->dev, "g3d_margin do not set, Set to default 0mV Value\n");
				cl_init.g3d_margin = MARGIN_0MV;
			}

			ret = of_property_read_u32_index(node, "asv_v3_mif_margin", 0, &cl_init.mif_margin);
			if (ret) {
				dev_err(&pdev->dev, "mif_margin do not set, Set to default 0mV Value\n");
				cl_init.mif_margin = MARGIN_0MV;
			}
			break;
		}

		ret = of_property_read_u32_index(node, "cl_period", 0, &cl_init.period);
		if (ret) {
			dev_err(&pdev->dev, "cl period do not set, Set to default 1ms\n");
			cl_init.period = PERIOD_1MS;
		}

		dev_info(&pdev->dev, "atlas:%d step, apollo:%d step, g3d:%d step, mif:%d step, period:<%d>\n",
			cl_init.atlas_margin, cl_init.apollo_margin, cl_init.g3d_margin,
			cl_init.mif_margin, cl_init.period);

		dev_info(&pdev->dev, "Initialize <%s> mail box \n", link[loop]->link_name);
	}

	link[loop] = NULL; /* Terminating link */

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	/* Request interrupt */
	ret = request_irq(mbox_irq, samsung_ipc_handler, IRQF_SHARED, mbox_link->name,
				mbox_link);
	if (ret) {
		dev_err(&pdev->dev, "failed to register mailbox interrupt:%d\n", ret);
		return ret;
	}

	disable_irq(mbox_irq);
#endif

	mutex_init(&samsung_mbox->lock);
	samsung_mbox->mbox_con.links = link;
#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	samsung_mbox->mbox_con.txdone_irq = true;
#endif
#ifdef CONFIG_EXYNOS_MBOX_POLLING
	samsung_mbox->mbox_con.txdone_irq = false;
	samsung_mbox->mbox_con.txdone_poll = true;
	samsung_mbox->mbox_con.txpoll_period = POLL_PERIOD;
#endif
	samsung_mbox->mbox_con.ops = &samsung_mbox_ops;
	snprintf(samsung_mbox->mbox_con.controller_name, 16, "samsung_mbox");

	ret = mbox_controller_register(&samsung_mbox->mbox_con);
	if (ret) {
		dev_err(&pdev->dev, "%s: MBOX Link register failed\n", __func__);
		return ret;
	}

	platform_set_drvdata(pdev, samsung_mbox);

	exynos_pm_register_notifier(&exynos_mailbox_notifier_block);
	register_reboot_notifier(&exynos_mailbox_reboot_notifier);

	mailbox_wq = create_singlethread_workqueue("thred-mailbox");
	if (!mailbox_wq) {
		return -ENOMEM;
	}

	fb_register_client(&exynos_mailbox_fb_notifier_block);

	/* Write rcc table to apm sram area */
	set_rcc_info();

#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
	cl_init.cl_status = CL_ON;
#endif
	return ret;
}

static int samsung_mbox_remove(struct platform_device *pdev)
{
	struct samsung_mbox *samsung_mbox = platform_get_drvdata(pdev);
	struct samsung_mlink *mbox_link;

	mbox_link = &samsung_mbox->samsung_link[0];

#ifdef CONFIG_EXYNOS_MBOX_INTERRUPT
	free_irq(mbox_irq, mbox_link);
#endif

	mbox_controller_unregister(&samsung_mbox->mbox_con);
	exynos_pm_unregister_notifier(&exynos_mailbox_notifier_block);
	unregister_reboot_notifier(&exynos_mailbox_reboot_notifier);
	fb_unregister_client(&exynos_mailbox_fb_notifier_block);

	return 0;
}

static int samsung_mailbox_pm_resume_early(struct device *dev)
{
	int ret;

	ret = firmware_update(dev);
	if (ret < 0) {
		dev_err(dev, "failed update firmware\n");
		return -ENODEV;
	}

	/* Write rcc table to apm sram area */
	set_rcc_info();

	return 0;
}

static struct dev_pm_ops samsung_mailbox_pm = {
	.resume_early	= samsung_mailbox_pm_resume_early,
};

/* Debug FS */
static int mailbox_message_open_show(struct seq_file *buf, void *d)
{
	int count;

	/* Print tx message history */
	for (count = 0; count < DEBUG_COUNT; count++) {
		seq_printf(buf, "[TX] [%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", tx.time[count], tx.buf[count][0],
				tx.buf[count][1], tx.buf[count][2], tx.buf[count][3], tx.buf[count][4], tx.name[count]);
		seq_printf(buf, "[RX] [%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", rx.time[count], rx.buf[count][0],
				rx.buf[count][1], rx.buf[count][2], rx.buf[count][3], rx.buf[count][4], rx.name[count]);
	}

	return 0;
}

static int mailbox_send_data_open_show(struct seq_file *buf, void *d)
{
	int count;

	/* Print tx message history */
	for (count = 0; count < DEBUG_COUNT; count++) {
		seq_printf(buf, "[%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", tx.time[count], tx.buf[count][0],
				tx.buf[count][1], tx.buf[count][2], tx.buf[count][3], tx.buf[count][4], tx.name[count]);
	}

	return 0;
}

static int mailbox_receive_data_open_show(struct seq_file *buf, void *d)
{
	int count;

	/* Print tx message history */
	for (count = 0; count < DEBUG_COUNT; count++) {
		seq_printf(buf, "[%lld ms]data : %8.x %8.x %8.x %8.x %8.x %s\n", rx.time[count], rx.buf[count][0],
				rx.buf[count][1], rx.buf[count][2], rx.buf[count][3], rx.buf[count][4], rx.name[count]);
	}

	return 0;
}

static int cm3_margin_open_show(struct seq_file *buf, void *d)
{
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
	seq_printf(buf, "ATLAS  Limit margin : %d uV \n", cl_init.atlas_margin * PMIC_STEP);
	seq_printf(buf, "APOLLO Limit margin : %d uV \n", cl_init.apollo_margin * PMIC_STEP);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	seq_printf(buf, "G3D    Limit margin : %d uV \n", cl_init.g3d_margin * PMIC_STEP);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	seq_printf(buf, "MIF    Limit margin : %d uV \n", cl_init.mif_margin * PMIC_STEP);
#endif

	return 0;
}

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
static int cl_voltage_open_show(struct seq_file *buf, void *d)
{
	seq_printf(buf, "=========== [input]==[cl_volt]==============\n");
	seq_printf(buf, "atl_voltage : %d %d\n", atl_in_voltage, atl_voltage);
	seq_printf(buf, "apo_voltage : %d %d\n", apo_in_voltage, apo_voltage);
	seq_printf(buf, "g3d_voltage : %d %d\n", g3d_in_voltage, g3d_voltage);
	seq_printf(buf, "mif_voltage : %d %d\n", mif_in_voltage, mif_voltage);

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
static int cpu_margin_get(void *data, u64 *val)
{
	pr_info("ATLAS  Limit margin : %d uV \n", cl_init.atlas_margin * PMIC_STEP);
	pr_info("APOLLO Limit margin : %d uV \n", cl_init.apollo_margin * PMIC_STEP);

	return 0;
}

static int cpu_margin_set(void *data, u64 val)
{
	int ret;

	cl_init.atlas_margin = val;
	cl_init.apollo_margin = val;

	ret = exynos7420_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
					cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
	if (ret)
		pr_warn("mailbox : Do not set margin and period information\n");

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
static int g3d_margin_get(void *data, u64 *val)
{
	pr_info("G3D    Limit margin : %d uV \n", cl_init.g3d_margin * PMIC_STEP);

	return 0;
}

static int g3d_margin_set(void *data, u64 val)
{
	int ret;

	cl_init.g3d_margin = val;

	ret = exynos7420_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
					cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
	if (ret)
		pr_warn("mailbox : Do not set margin and period information\n");

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
static int mif_margin_get(void *data, u64 *val)
{
	pr_info("MIF    Limit margin : %d uV \n", cl_init.mif_margin * PMIC_STEP);

	return 0;
}

static int mif_margin_set(void *data, u64 val)
{
	int ret;

	cl_init.mif_margin = val;

	ret = exynos7420_cl_dvfs_setup(cl_init.atlas_margin, cl_init.apollo_margin,
					cl_init.g3d_margin, cl_init.mif_margin, cl_init.period);
	if (ret)
		pr_warn("mailbox : Do not set margin and period information\n");

	return 0;
}
#endif

#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
static int cl_enable_get(void *data, u64 *val)
{
	if (cl_init.cl_status == CL_OFF && cl_init.apm_status == APM_ON)
		pr_info("CL STATUS : Disable\n");
	else if (cl_init.cl_status == CL_ON && cl_init.apm_status == APM_ON)
		pr_info("CL STATUS : Enable\n");
	else if (cl_init.apm_status == APM_OFF)
		pr_info("CL STATUS : APM Power Off\n");

	return 0;
}

static int cl_enable_set(void *data, u64 val)
{
	if (val) {
		if (cl_init.apm_status == APM_ON) {
			if (cl_init.cl_status == CL_OFF) {
				exynos7420_cl_dvfs_mode_enable();

				sec_core_lock();
				cl_dvfs_lock();
				apm_notifier_call_chain(CL_ENABLE);
				cl_dvfs_unlock();
				sec_core_unlock();

				cl_init.cl_status = CL_ON;
				pr_info("CL_ENABLE \n");
			} else if (cl_init.cl_status == CL_ON) {
				pr_info("Already turn on CL-DVFS\n");
			}
		} else if (cl_init.apm_status == APM_OFF) {
			pr_info("APM OFF Status \n");
		}
	} else {
		if (cl_init.apm_status == APM_ON) {
			if (cl_init.cl_status == CL_OFF) {
				pr_info("Already turn off CL-DVFS\n");
			} else if (cl_init.cl_status == CL_ON) {
				exynos7420_cl_dvfs_mode_disable();

				sec_core_lock();
				cl_dvfs_lock();
				apm_notifier_call_chain(CL_DISABLE);
				cl_dvfs_unlock();
				sec_core_unlock();

				cl_init.cl_status = CL_OFF;
				pr_info("CL_DISABLE \n");
			}
		} else if (cl_init.apm_status == APM_OFF) {
			pr_info("APM OFF Status \n");
		}
	}

	return 0;
}
#endif
static int mailbox_message_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, mailbox_message_open_show, inode->i_private);
}

static int mailbox_send_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, mailbox_send_data_open_show, inode->i_private);
}

static int mailbox_receive_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, mailbox_receive_data_open_show, inode->i_private);
}

static int cm3_margin_open(struct inode *inode, struct file *file)
{
	return single_open(file, cm3_margin_open_show, inode->i_private);
}

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
static int cl_voltage_open(struct inode *inode, struct file *file)
{
	return single_open(file, cl_voltage_open_show, inode->i_private);
}
#endif

static const struct file_operations message_status_fops = {
	.open		= mailbox_message_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations send_status_fops = {
	.open		= mailbox_send_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations receive_status_fops = {
	.open		= mailbox_receive_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations mode_status_fops = {
	.open		= cm3_status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations mode_margin_fops = {
	.open		= cm3_margin_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
static const struct file_operations cl_voltage_fops = {
	.open		= cl_voltage_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
DEFINE_SIMPLE_ATTRIBUTE(cpu_margin_fops, cpu_margin_get, cpu_margin_set, "%llx\n");
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
DEFINE_SIMPLE_ATTRIBUTE(g3d_margin_fops, g3d_margin_get, g3d_margin_set, "%llx\n");
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
DEFINE_SIMPLE_ATTRIBUTE(mif_margin_fops, mif_margin_get, mif_margin_set, "%llx\n");
#endif
#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
DEFINE_SIMPLE_ATTRIBUTE(cl_enable_fops, cl_enable_get, cl_enable_set, "%llx\n");
#endif

void mailbox_debugfs(void)
{
	struct dentry *den;

	den = debugfs_create_dir("mailbox", NULL);
	debugfs_create_file("message", 0644, den, NULL, &message_status_fops);
	debugfs_create_file("send_data", 0644, den, NULL, &send_status_fops);
	debugfs_create_file("receive_data", 0644, den, NULL, &receive_status_fops);
	debugfs_create_file("mode", 0644, den, NULL, &mode_status_fops);
	debugfs_create_file("cl_dvs_margin", 0644, den, NULL, &mode_margin_fops);
#ifdef CONFIG_EXYNOS_APM_VOLTAGE_DEBUG
	debugfs_create_file("cl_voltage", 0644, den, NULL, &cl_voltage_fops);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
	debugfs_create_file("cpu_cl_margin", 0644, den, NULL, &cpu_margin_fops);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	debugfs_create_file("g3d_cl_margin", 0644, den, NULL, &g3d_margin_fops);
#endif
#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	debugfs_create_file("mif_cl_margin", 0644, den, NULL, &mif_margin_fops);
#endif
#if (defined(CONFIG_EXYNOS_CL_DVFS_CPU) || defined(CONFIG_EXYNOS_CL_DVFS_G3D) || defined(CONFIG_EXYNOS_CL_DVFS_MIF))
	debugfs_create_file("cl_control", 0644, den, NULL, &cl_enable_fops);
#endif
}

static const struct of_device_id mailbox_smc_match[] = {
	{ .compatible = "samsung,exynos-mailbox" },
	{},
};

static struct platform_driver samsung_mbox_driver = {
	.probe	= samsung_mbox_probe,
	.remove	= samsung_mbox_remove,
	.driver	= {
		.name = "exynos-mailbox",
		.owner	= THIS_MODULE,
		.of_match_table	= mailbox_smc_match,
		.pm	= &samsung_mailbox_pm,
	},
};

static int __init exynos_mailbox_init(void)
{
	mailbox_debugfs();

	return platform_driver_register(&samsung_mbox_driver);
}
fs_initcall(exynos_mailbox_init);

static void __exit exynos_mailbox_exit(void)
{
	class_destroy(mailbox_class);
}
module_exit(exynos_mailbox_exit);
