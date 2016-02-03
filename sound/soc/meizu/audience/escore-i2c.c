/*
 * escore-i2c.c  --  I2C interface for Audience earSmart chips
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Greg Clemson <gclemson@audience.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "escore.h"
#include "escore-i2c.h"
#include <linux/kthread.h>
#include <linux/kmod.h>

static const struct i2c_client *escore_i2c;

static u32 escore_cpu_to_i2c(struct escore_priv *escore, u32 resp)
{
	return cpu_to_be32(resp);
}

static u32 escore_i2c_to_cpu(struct escore_priv *escore, u32 resp)
{
	return be32_to_cpu(resp);
}

int escore_i2c_read(struct escore_priv *escore, void *buf, int len)
{
	struct i2c_msg msg[] = {
		{
			.addr = escore_i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	int rc = 0;

	rc = i2c_transfer(escore_i2c->adapter, msg, 1);
	/*
	 * i2c_transfer returns number of messages executed. Since we
	 * are always sending only 1 msg, return value should be 1 for
	 * success case
	 */
	if (rc != 1) {
		pr_err("%s(): i2c_transfer() failed, rc = %d, msg_len = %d\n",
			__func__, rc, len);
		return -EIO;
	} else {
		return 0;
	}
}

int escore_i2c_write(struct escore_priv *escore, const void *buf, int len)
{
	struct i2c_msg msg;
	int max_xfer_len = ES_MAX_I2C_XFER_LEN;
	int rc = 0, written = 0, xfer_len;
	int retry = ES_MAX_RETRIES;

	msg.addr = escore_i2c->addr;
	msg.flags = 0;

	while (written < len) {
		xfer_len = min(len - written, max_xfer_len);

		msg.len = xfer_len;
		msg.buf = (void *)(buf + written);
		do {
			rc = i2c_transfer(escore_i2c->adapter, &msg, 1);
			if (rc == 1)
				break;
			else
				pr_err("%s(): failed, retrying, rc:%d\n",
						__func__, rc);
			usleep_range(10000, 10000);
			--retry;
		} while (retry != 0);
		retry = ES_MAX_RETRIES;
		written += xfer_len;
	}
	return 0;
}

int escore_i2c_cmd(struct escore_priv *escore, u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	u32 rv;
	int retry = ES_MAX_RETRIES + 1;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);

	*resp = 0;
	cmd = cpu_to_be32(cmd);
	err = escore_i2c_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		goto cmd_exit;

	do {
		if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
			pr_debug("%s(): Waiting for API interrupt. Jiffies:%lu",
					__func__, jiffies);
			err = wait_for_completion_timeout(&escore->rising_edge,
					msecs_to_jiffies(ES_RESP_TOUT_MSEC));
			if (!err) {
				pr_debug("%s(): API Interrupt wait timeout\n",
						__func__);
				err = -ETIMEDOUT;
				break;
			}
		} else {
			usleep_range(ES_RESP_POLL_TOUT,
					ES_RESP_POLL_TOUT + 500);
		}
		err = escore_i2c_read(escore, &rv, sizeof(rv));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = be32_to_cpu(rv);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: escore_i2c_read() failure\n", __func__);
		} else if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, cmd);
			err = -EINVAL;
			goto cmd_exit;
		} else if (*resp == ES_NOT_READY) {
			dev_dbg(escore->dev,
				"%s: escore_i2c_read() not ready\n", __func__);
			err = -ETIMEDOUT;
		} else {
			goto cmd_exit;
		}

		--retry;
	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);

cmd_exit:
	update_cmd_history(be32_to_cpu(cmd), *resp);
	return err;
}

int escore_i2c_boot_setup(struct escore_priv *escore)
{
	u16 boot_cmd = ES_I2C_BOOT_CMD;
	u16 boot_ack = 0;
	char msg[2];
	int rc;

	pr_info("%s()\n", __func__);
	pr_info("%s(): write ES_BOOT_CMD = 0x%04x\n", __func__, boot_cmd);
#ifdef CONFIG_MQ100_SENSOR_HUB
	mq100_indicate_state_change(MQ100_STATE_RESET);
#endif
	cpu_to_be16s(&boot_cmd);
	memcpy(msg, (char *)&boot_cmd, 2);
	rc = escore_i2c_write(escore, msg, 2);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot write\n", __func__);
		goto escore_bootup_failed;
	}
	usleep_range(1000, 1000);
	memset(msg, 0, 2);
	rc = escore_i2c_read(escore, msg, 2);
	if (rc < 0) {
		pr_err("%s(): firmware load failed boot ack\n", __func__);
		goto escore_bootup_failed;
	}
	memcpy((char *)&boot_ack, msg, 2);
	pr_info("%s(): boot_ack = 0x%04x\n", __func__, boot_ack);
	if (boot_ack != ES_I2C_BOOT_ACK) {
		pr_err("%s(): firmware load failed boot ack pattern", __func__);
		rc = -EIO;
		goto escore_bootup_failed;
	}

escore_bootup_failed:
	return rc;
}

int escore_i2c_boot_finish(struct escore_priv *escore)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int rc = 0;
	int sync_retry = ES_SYNC_MAX_RETRY;

	/* Utilize gpio-a for boot finish */
	if (escore->pdata->gpioa_gpio != -1) {
		rc = wait_for_completion_timeout(&escore->falling_edge,
				msecs_to_jiffies(ES_SBL_RESP_TOUT));
		if (!rc) {
			pr_err("%s(): Boot Finish response timed out\n",
				__func__);
			rc = -ETIMEDOUT;
		} else {
			pr_info("%s(): firmware load success\n", __func__);
			rc = 0;
		}
		return rc;
	}

	/* Use Polling method if gpio-a is not defined */

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);
		rc = escore_i2c_cmd(escore, sync_cmd, &sync_ack);
		if (rc < 0) {
			pr_err("%s(): firmware load failed sync write\n",
					__func__);
			continue;
		}
		pr_debug("%s(): sync_ack = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): firmware load failed sync ack pattern",
					__func__);
			rc = -EIO;
		} else {
#ifdef CONFIG_MQ100_SENSOR_HUB
			mq100_indicate_state_change(MQ100_STATE_NORMAL);
#endif
			pr_info("%s(): firmware load success", __func__);
			break;
		}
	} while (sync_retry--);

	return rc;
}

static void escore_i2c_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_i2c_read;
	escore->bus.ops.write = escore_i2c_write;
	escore->bus.ops.cmd = escore_i2c_cmd;
	escore->streamdev = es_i2c_streamdev;
}

static int escore_i2c_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->bus.ops.high_bw_write = escore_i2c_write;
	escore->bus.ops.high_bw_read = escore_i2c_read;
	escore->bus.ops.high_bw_cmd = escore_i2c_cmd;
	escore->boot_ops.setup = escore_i2c_boot_setup;
	escore->boot_ops.finish = escore_i2c_boot_finish;
	escore->bus.ops.cpu_to_bus = escore_cpu_to_i2c;
	escore->bus.ops.bus_to_cpu = escore_i2c_to_cpu;
	rc = escore->probe(escore->dev);
	if (rc)
		goto out;

	rc = escore->boot_ops.bootup(escore);
	if (rc)
		goto out;

out:
	return rc;
}

int escore_i2c_probe_thread(void *ptr)
{
	int rc = 0;
	struct device *dev = (struct device *) ptr;
	struct escore_priv *escore = &escore_priv;

	rc = escore_probe(escore, dev, ES_I2C_INTF, ES_CONTEXT_THREAD);
	if (rc)
		dev_err(dev, "%s(): i2c probe failed\n", __func__);
	return rc;

}

static int escore_i2c_probe(struct i2c_client *i2c,
			   const struct i2c_device_id *id)
{
	struct escore_priv *escore = &escore_priv;
	struct task_struct *i2c_probe_thread;
	int rc = 0;

	pr_debug("%s:%d", __func__, __LINE__);

	escore_i2c = i2c;

	if (escore->pri_intf == ES_I2C_INTF) {
		escore->bus.setup_prim_intf = escore_i2c_setup_pri_intf;
		escore->dev = &i2c->dev;
	}
	if (escore->high_bw_intf == ES_I2C_INTF)
		escore->bus.setup_high_bw_intf = escore_i2c_setup_high_bw_intf;

	i2c_set_clientdata(i2c, escore);

	if (escore->high_bw_intf == ES_I2C_INTF) {
		/* create thread to bootup es705 when using i2c as high speed interface,
		 * considering that request firmware from filesystem. */
		i2c_probe_thread = kthread_run(escore_i2c_probe_thread,
						(void *) &i2c->dev,
						"escore i2c thread");
		if (IS_ERR_OR_NULL(i2c_probe_thread)) {
			pr_err("%s(): can't create escore i2c probe thread = %p\n",
				__func__, i2c_probe_thread);
			rc = -ENOMEM;
		}
	}
	else {
		rc = escore_probe(escore, &i2c->dev, ES_I2C_INTF, ES_CONTEXT_PROBE);
	}

	return rc;
}

static int escore_i2c_remove(struct i2c_client *i2c)
{
	struct esxxx_platform_data *pdata = i2c->dev.platform_data;
	if (pdata->reset_gpio != -1)
		gpio_free(pdata->reset_gpio);
	if (pdata->wakeup_gpio != -1)
		gpio_free(pdata->wakeup_gpio);
	if (pdata->gpioa_gpio != -1)
		gpio_free(pdata->gpioa_gpio);
	if (pdata->gpiob_gpio != -1)
		gpio_free(pdata->gpiob_gpio);

	snd_soc_unregister_codec(&i2c->dev);

	kfree(i2c_get_clientdata(i2c));

	return 0;
}

struct es_stream_device es_i2c_streamdev = {
	.read = escore_i2c_read,
	.intf = ES_I2C_INTF,
};

int escore_i2c_init(void)
{
	int rc;
	pr_debug("%s: adding i2c driver\n", __func__);

	rc = i2c_add_driver(&escore_i2c_driver);
	if (!rc)
		pr_info("%s() registered as I2C", __func__);

	else
		pr_err("%s(): i2c_add_driver failed, rc = %d", __func__, rc);

	return rc;
}

static const struct i2c_device_id escore_i2c_id[] = {
	{ "earSmart", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, escore_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id escore_i2c_dt_ids[] = {
	{ .compatible = "audience,escore"},
	{ }
};
#endif

struct i2c_driver escore_i2c_driver = {
	.driver = {
		.name = "earSmart-codec",
		.owner = THIS_MODULE,
		.pm = &escore_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(escore_i2c_dt_ids),
#endif
	},
	.probe = escore_i2c_probe,
	.remove = escore_i2c_remove,
	.id_table = escore_i2c_id,
};

MODULE_DESCRIPTION("Audience earSmart I2C core driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:earSmart-codec");
