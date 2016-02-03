/*
 * escore-spi.c  --  SPI interface for Audience earSmart chips
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
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "escore.h"
#include "escore-spi.h"
static struct spi_device *escore_spi;

static u32 escore_cpu_to_spi(struct escore_priv *escore, u32 resp)
{
	return cpu_to_be32(resp);
}

static u32 escore_spi_to_cpu(struct escore_priv *escore, u32 resp)
{
	return be32_to_cpu(resp);
}

static int escore_spi_read(struct escore_priv *escore, void *buf, int len)
{
	int rc;

	rc = spi_read(escore_spi, buf, len);
	if (rc < 0) {
		dev_err(&escore_spi->dev, "%s(): error %d reading SR\n",
			__func__, rc);
		return rc;
	}
	return rc;
}

static int escore_spi_read_streaming(struct escore_priv *escore,
				void *buf, int len)
{
	int rc;
	int rdcnt, rd_len, rem_len, pk_cnt;
	u16 data;

	pk_cnt = len/ESCORE_SPI_PACKET_LEN;
	rd_len = ESCORE_SPI_PACKET_LEN;
	rem_len = len % ESCORE_SPI_PACKET_LEN;

	for (rdcnt = 0; rdcnt < pk_cnt; rdcnt++) {
		rc = escore_spi_read(escore, (char *)(buf + (rdcnt * rd_len)),
				rd_len);
		if (rc < 0) {
			dev_err(escore->dev,
				"%s(): Read Data Block error %d\n",
				__func__, rc);
			return rc;
		}
		//usleep_range(60, 60);
	}

	if (rem_len) {
		rc = escore_spi_read(escore, (char *) (buf + (rdcnt * rd_len)),
				rem_len);
	}

	for (rdcnt = 0; rdcnt < len; rdcnt += 2) {
		data = *((u16 *)buf);
		data = be16_to_cpu(data);
		memcpy(buf, (char *)&data, sizeof(data));
		buf += 2;
	}

	return rdcnt;
}

static int escore_spi_write(struct escore_priv *escore,
			    const void *buf, int len)
{
	int rc;
	int rem = 0;
	char align[4] = {0};

	/* Check if length is 4 byte aligned or not
	   If not aligned send extra bytes after write */
	if ((len > 4) && (len % 4 != 0)) {
		rem = len % 4;
		dev_info(escore->dev,
			"Alignment required 0x%x bytes\n", rem);
	}

	rc = spi_write(escore_spi, buf, len);

	if (rem != 0)
		rc = spi_write(escore_spi, align, 4 - rem);

	return rc;
}

static int  escore_spi_high_bw_cmd(struct escore_priv *escore,
			  u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	int retry = ES_SPI_MAX_RETRIES;
	u16 resp16;

	*resp = 0;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);
	cmd = cpu_to_be32(cmd);
	err = escore_spi_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		goto cmd_exit;
 

	usleep_range(1000, 1050);

	do {
		--retry;
		if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
			pr_debug("%s(): Waiting for API interrupt. Jiffies:%lu",
					__func__, jiffies);
			err = wait_for_completion_timeout(&escore->cmd_compl,
					msecs_to_jiffies(ES_RESP_TOUT_MSEC));
			if (!err) {
				pr_err("%s(): API Interrupt wait timeout\n",
						__func__);
				err = -ETIMEDOUT;
				break;
			}
		} else {
			if (retry % ES_SPI_CONT_RETRY == 0) {
				usleep_range(1000, 1200);
			}
		}

		err = escore_spi_read(escore, &resp16, sizeof(resp16));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = (be16_to_cpu(resp16) << 16);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: escore_spi_read() failure, err=%d\n",
				__func__, err);
		}
		if (resp16 != 0) {
			err = escore_spi_read(escore, &resp16, sizeof(resp16));
			(*resp) |= be16_to_cpu(resp16);
			dev_dbg(escore->dev, "%s: *resp=0x%08x\n",
					__func__, *resp);
			if (err) {
				dev_dbg(escore->dev,
					"%s: escore_spi_read() failure, err=%d\n",
					__func__, err);
			}
		} else {
			if (retry == 0) {
				err = -ETIMEDOUT;
				dev_err(escore->dev,
					"%s: response retry timeout, err=%d\n",
					__func__, err);
				break;
			} else {
				continue;
			}
		}

		if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, cmd);
			err = -EINVAL;
			goto cmd_exit;
		} else {
			goto cmd_exit;
		}

	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);

cmd_exit:
	update_cmd_history(be32_to_cpu(cmd), *resp);
	return err;
}


static int escore_spi_cmd(struct escore_priv *escore,
			  u32 cmd, u32 *resp)
{
	int err = 0;
	int sr = cmd & BIT(28);
	int retry = ES_SPI_MAX_RETRIES;
	u16 resp16;

	dev_dbg(escore->dev,
			"%s: cmd=0x%08x  sr=0x%08x\n", __func__, cmd, sr);
	cmd = cpu_to_be32(cmd);
	err = escore_spi_write(escore, &cmd, sizeof(cmd));
	if (err || sr)
		goto cmd_exit;

	usleep_range(ES_SPI_5MS_DELAY, ES_SPI_5MS_DELAY + 50);

	do {
		--retry;
		if (escore->cmd_compl_mode == ES_CMD_COMP_INTR) {
			pr_debug("%s(): Waiting for API interrupt. Jiffies:%lu",
					__func__, jiffies);
			err = wait_for_completion_timeout(&escore->cmd_compl,
					msecs_to_jiffies(ES_RESP_TOUT_MSEC));
			if (!err) {
				pr_err("%s(): API Interrupt wait timeout\n",
						__func__);
				err = -ETIMEDOUT;
				break;
			}
		} else {
			usleep_range(ES_SPI_RETRY_DELAY, ES_SPI_RETRY_DELAY + 200);
		}

		err = escore_spi_read(escore, &resp16, sizeof(resp16));
		dev_dbg(escore->dev, "%s: err=%d\n", __func__, err);
		*resp = (be16_to_cpu(resp16) << 16);
		dev_dbg(escore->dev, "%s: *resp=0x%08x\n", __func__, *resp);
		if (err) {
			dev_dbg(escore->dev,
				"%s: escore_spi_read() failure, err=%d\n",
				__func__, err);
		}
		if (resp16 != 0) {
			err = escore_spi_read(escore, &resp16, sizeof(resp16));
			(*resp) |= be16_to_cpu(resp16);
			dev_dbg(escore->dev, "%s: *resp=0x%08x\n",
					__func__, *resp);
			if (err) {
				dev_dbg(escore->dev,
					"%s: escore_spi_read() failure, err=%d\n",
					__func__, err);
			}
		} else {
			if (retry == 0) {
				err = -ETIMEDOUT;
				dev_err(escore->dev,
					"%s: response retry timeout, err=%d\n",
					__func__, err);
				break;
			} else {
				continue;
			}
		}

		if ((*resp & ES_ILLEGAL_CMD) == ES_ILLEGAL_CMD) {
			dev_err(escore->dev, "%s: illegal command 0x%08x\n",
				__func__, cmd);
			err = -EINVAL;
			goto cmd_exit;
		} else {
			goto cmd_exit;
		}

	} while (retry != 0 && escore->cmd_compl_mode != ES_CMD_COMP_INTR);

cmd_exit:
	update_cmd_history(be32_to_cpu(cmd), *resp);
	return err;
}

int escore_spi_setup(u32 speed)
{
	int status;

	/* set SPI speed to
	 *	- 12MHz for fw download
	 *		- <= 4MHz for rest operations */
	escore_spi->max_speed_hz = speed;
	escore_spi->mode = SPI_MODE_1;
	status = spi_setup(escore_spi);
	if (status < 0) {
		pr_err("%s : can't setup %s, status %d\n", __func__,
				(char *)dev_name(&escore_spi->dev), status);
	} else {
		pr_info("\nSPI speed changed to %dHz\n",
				escore_spi->max_speed_hz);
		usleep_range(1000, 1000);
	}
	return status;
}

static int escore_spi_datablock_read(struct escore_priv *escore, void *buf,
		size_t len, int id)
{
	int rc;
	int size;
	u32 cmd;
	int rdcnt = 0;
	u32 resp;
	u8 flush_extra_blk = 0;
	u32 flush_buf;
	u16 temp;

	/* Reset read data block size */
	escore->datablock_dev.rdb_read_count = 0;

	cmd = (ES_READ_DATA_BLOCK << 16) | (id & 0xFFFF);

	rc = escore_spi_cmd(escore, cmd, &resp);
	if (rc < 0) {
		dev_err(escore->dev, "%s(): escore_spi_cmd() failed rc = %d\n",
			 __func__, rc);
		goto out;
	}
	if ((resp >> 16) != ES_READ_DATA_BLOCK) {
		dev_err(escore->dev, "%s(): Invalid respn received: 0x%08x\n",
				__func__, resp);
		rc = -EINVAL;
		goto out;
	}

	size = resp & 0xFFFF;
	dev_dbg(escore->dev, "%s(): RDB size = %d\n", __func__, size);
	if (size == 0 || size % 4 != 0) {
		dev_err(escore->dev,
			"%s(): Read Data Block with invalid size:%d\n",
			__func__, size);
		rc = -EINVAL;
		goto out;
	}

	if (len != size) {
		dev_dbg(escore->dev, "%s(): Requested:%ld Received:%d\n",
			 __func__, len, size);
		if (len < size)
			flush_extra_blk = (size - len) % 4;
		else
			len = size;
	}

	for (rdcnt = 0; rdcnt < len;) {
		rc = escore_spi_read(escore, (char *)&temp, sizeof(temp));
		if (rc < 0) {
			dev_err(escore->dev, "%s(): Read Data Block error %d\n",
					__func__, rc);
			goto out;
		}
		temp = be16_to_cpu(temp);
		memcpy(buf, (char *)&temp, 2);
		buf += 2;
		rdcnt += 2;
	}

	/* Store read data block size */
	escore->datablock_dev.rdb_read_count = size;

	/* No need to read in case of no extra bytes */
	if (flush_extra_blk) {
		/* Discard the extra bytes */
		rc = escore_spi_read(escore, &flush_buf, flush_extra_blk);
		if (rc < 0) {
			dev_err(escore->dev, "%s(): Read Data Block error in flushing %d\n",
					__func__, rc);
			goto out;
		}
	}
out:
	return rc;
}

static int escore_spi_boot_setup(struct escore_priv *escore)
{
	u16 boot_cmd = ES_SPI_BOOT_CMD;
	u16 boot_ack;
	u16 sbl_sync_cmd = ES_SPI_SYNC_CMD;
	u16 sbl_sync_ack;
	int rc;
	u8 cmd_compl_mode;
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int sync_retry = ES_SYNC_MAX_RETRY;
	int retry = 5;

	msleep(20);
	pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n", __func__, sync_cmd);
	sync_cmd = cpu_to_be32(sync_cmd);
	rc = escore_spi_write(escore, &sync_cmd, sizeof(sync_cmd));
	if (rc < 0) {
		pr_err("%s(): failed sync write\n", __func__);
		goto escore_spi_boot_setup_failed;
	}
	do {
	    rc = escore_spi_read(escore, &sync_ack, sizeof(sync_ack));
        if (rc < 0) {
            pr_err("%s(): failed sync read\n", __func__);
            continue;
        }

	    sync_ack = be32_to_cpu(sync_ack);
	    pr_debug("%s(): SYNC ACK = 0x%08x\n", __func__, sync_ack);
		if (sync_ack != ES_SPI_SBL_SYNC_ACK) {
			pr_err("%s():  failed sync ack pattern 0x%08x", __func__, sync_ack);
			rc = -EIO;
            msleep(10);
		} else {
			pr_debug("%s(): SPI sync success", __func__);
			break;
		}
	} while (sync_retry--);

	pr_debug("%s(): prepare for fw download\n", __func__);

	/* GPIO_A (INTR_API) transits from low to high once
	 * SBL SYNC command response is ready. So, for the command completion
	 * mode to "Interrupt" to read the SBL SYNC response.
	 */
	cmd_compl_mode = escore->cmd_compl_mode;
	if (escore->pdata->gpioa_gpio != -1)
		escore->cmd_compl_mode = ES_CMD_COMP_INTR;

	sbl_sync_cmd = cpu_to_be16(sbl_sync_cmd);
	rc = escore_spi_write(escore, &sbl_sync_cmd, sizeof(sbl_sync_cmd));
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync write\n", __func__);
		goto escore_spi_boot_setup_failed;
	}

	if (escore->pdata->gpioa_gpio != -1) {
		rc = wait_for_completion_timeout(&escore->cmd_compl,
				msecs_to_jiffies(ES_SBL_RESP_TOUT));
		if (!rc) {
			pr_err("%s(): SBL SYNC response timed out\n", __func__);
			rc = -ETIMEDOUT;
			goto escore_spi_boot_setup_failed;
		}
	} else {
		//usleep_range(10000, 10500);
		msleep (4);
	}

	rc = escore_spi_read(escore, &sbl_sync_ack, sizeof(sbl_sync_ack));
	escore->cmd_compl_mode = cmd_compl_mode;
	if (rc < 0) {
		pr_err("%s(): firmware load failed sync ack\n",
			__func__);
		goto escore_spi_boot_setup_failed;
	}

	sbl_sync_ack = be16_to_cpu(sbl_sync_ack);
	pr_debug("%s(): SBL SYNC ACK = 0x%08x\n", __func__, sbl_sync_ack);
	if (sbl_sync_ack != ES_SPI_SYNC_ACK) {
		pr_err("%s(): sync ack pattern fail 0x%x\n", __func__,
				sbl_sync_ack);
		rc = -EIO;
		goto escore_spi_boot_setup_failed;
	}

	pr_debug("%s(): write ES_BOOT_CMD = 0x%04x\n", __func__, boot_cmd);

	do {
		boot_cmd = cpu_to_be16(boot_cmd);
		rc = escore_spi_write(escore, &boot_cmd, sizeof(boot_cmd));
		if (rc < 0) {
			pr_err("%s(): firmware load failed boot write\n",
				__func__);
			goto escore_spi_boot_setup_failed;
		}

		usleep_range(ES_SPI_1MS_DELAY, ES_SPI_1MS_DELAY + 5);
		rc = escore_spi_read(escore, &boot_ack, sizeof(boot_ack));
		if (rc < 0) {
			pr_err("%s(): firmware load failed boot ack\n",
				__func__);
			//goto escore_spi_boot_setup_failed;
		}

		boot_ack = be16_to_cpu(boot_ack);
		pr_debug("%s(): BOOT ACK = 0x%08x\n", __func__, boot_ack);

		if (boot_ack != ES_SPI_BOOT_ACK) {
			pr_err("%s():boot ack pattern fail 0x%08x", __func__, boot_ack);
			rc = -EIO;
			//goto escore_spi_boot_setup_failed;
		}
	} while (rc && retry--);

	escore_spi->max_speed_hz = ES_SPI_FW_SPEED;
	rc = escore_spi_setup(escore_spi->max_speed_hz);

escore_spi_boot_setup_failed:
	return rc;
}

int escore_spi_boot_finish(struct escore_priv *escore)
{
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;
	int rc = 0;
	int sync_retry = ES_SYNC_MAX_RETRY;

	escore_spi->max_speed_hz = ES_SPI_OPERATION_SPEED;
	rc = escore_spi_setup(escore_spi->max_speed_hz);

	msleep(50);

	/* sometimes earSmart chip sends success in second sync command */
	do {
		pr_debug("%s(): write ES_SYNC_CMD = 0x%08x\n",
				__func__, sync_cmd);
		rc = escore_spi_cmd(escore, sync_cmd, &sync_ack);
		pr_debug("%s(): rc=%d, sync_ack = 0x%08x\n", __func__, rc, sync_ack);
		if (rc < 0) {
			pr_err("%s(): firmware load failed sync write\n",
					__func__);
			continue;
		}
		if (sync_ack != ES_SYNC_ACK) {
			pr_err("%s(): firmware load failed sync ack pattern",
					__func__);
			rc = -EIO;
		} else {
			pr_info("%s(): firmware load success", __func__);
			break;
		}
	} while (sync_retry--);
	return rc;
}

static void escore_spi_setup_pri_intf(struct escore_priv *escore)
{
	escore->bus.ops.read = escore_spi_read;
	escore->bus.ops.write = escore_spi_write;
	escore->bus.ops.cmd = escore_spi_cmd;
	escore->streamdev = es_spi_streamdev;
}

static int escore_spi_setup_high_bw_intf(struct escore_priv *escore)
{
	int rc;

	escore->boot_ops.setup = escore_spi_boot_setup;
	escore->boot_ops.finish = escore_spi_boot_finish;
	escore->bus.ops.high_bw_write = escore_spi_write;
	escore->bus.ops.high_bw_read = escore_spi_read;
	escore->bus.ops.high_bw_cmd = escore_spi_high_bw_cmd;
	escore->bus.ops.rdb = escore_spi_datablock_read;
	escore->bus.ops.cpu_to_bus = escore_cpu_to_spi;
	escore->bus.ops.bus_to_cpu = escore_spi_to_cpu;

	rc = escore->probe(escore->dev);
	if (rc)
		goto out;

	rc = escore->boot_ops.bootup(escore);
	if (rc)
		goto out;

out:
	return rc;
}

int escore_spi_probe_thread(void *ptr)
{
	int rc = 0;
	struct device *dev = (struct device *) ptr;
	struct escore_priv *escore = &escore_priv;

	rc = escore_probe(escore, dev, ES_SPI_INTF, ES_CONTEXT_THREAD);
	if (rc)
		dev_err(dev, "%s(): spi probe failed\n", __func__);
	return rc;

}

static int escore_spi_probe(struct spi_device *spi)
{
	int rc = 0;
	struct escore_priv *escore = &escore_priv;
	struct task_struct *spi_probe_thread;

	pr_info("called: %s\n", __func__);

	dev_set_drvdata(&spi->dev, &escore_priv);

	escore_spi = spi;

	/* setup spi at startup, maybe just using spi as command/streaming interface */
	escore_spi->max_speed_hz = ES_SPI_OPERATION_SPEED;
	rc = escore_spi_setup(escore_spi->max_speed_hz);
	if (rc < 0)
		return rc;

	if (escore->pri_intf == ES_SPI_INTF) {
		escore->bus.setup_prim_intf = escore_spi_setup_pri_intf;
		escore->dev = &spi->dev;
	}
	if (escore->high_bw_intf == ES_SPI_INTF)
		escore->bus.setup_high_bw_intf = escore_spi_setup_high_bw_intf;

	if (escore->high_bw_intf == ES_SPI_INTF) {
		/* create thread to bootup es705 when using spi as high speed interface,
		 * considering that request firmware from filesystem. */
		spi_probe_thread = kthread_run(escore_spi_probe_thread,
									   (void *) &spi->dev,
									   "escore spi thread");
		if (IS_ERR_OR_NULL(spi_probe_thread)) {
			pr_err("%s(): can't create escore spi probe thread = %p\n",
				__func__, spi_probe_thread);
			rc = -ENOMEM;
		}
	}
	else {
		rc = escore_probe(escore, &spi->dev, ES_SPI_INTF, ES_CONTEXT_PROBE);
	}

	return rc;
}

int escore_spi_wait(struct escore_priv *escore)
{
	/* For SPI there is no wait function available. It should always return
	 * true so that next read request is made in streaming case.
	 */
	return 1;
}

static int escore_spi_close(struct escore_priv *escore)
{
	int rc;
	char buf[64] = {0};
	u32 sync_cmd = (ES_SYNC_CMD << 16) | ES_SYNC_POLLING;
	u32 sync_ack;

	rc = escore_spi_write(escore, buf, sizeof(buf));
	if (rc) {
		dev_err(escore->dev, "%s:spi write error %d\n",
				__func__, rc);
		return rc;
	}

	/* Sending SYNC command */
	rc = escore_spi_cmd(escore, sync_cmd, &sync_ack);
	if (rc) {
		dev_err(escore->dev, "%s: Failed to send SYNC cmd, error %d\n",
				__func__, rc);
		return rc;
	}

	if (sync_ack != ES_SYNC_ACK) {
		dev_warn(escore->dev, "%s:Invalis SYNC_ACK received %x\n",
				__func__, sync_ack);
		rc = -EIO;
	}
	return rc;
}

struct es_stream_device es_spi_streamdev = {
	.read = escore_spi_read_streaming,
	.wait = escore_spi_wait,
	.close = escore_spi_close,
	.intf = ES_SPI_INTF,
};

static int escore_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}

int __init escore_spi_init(void)
{
	return spi_register_driver(&escore_spi_driver);
}

void __exit escore_spi_exit(void)
{
	spi_unregister_driver(&escore_spi_driver);
}

#ifdef CONFIG_OF
static struct of_device_id escore_spi_dt_ids[] = {
	{ .compatible = "audience,escore"},
	{ }
};
#endif

struct spi_driver escore_spi_driver = {
	.driver = {
		.name   = "earSmart-codec",
		.bus    = &spi_bus_type,
		.owner  = THIS_MODULE,
		.pm = &escore_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(escore_spi_dt_ids),
#endif
	},
	.probe  = escore_spi_probe,
	.remove = escore_spi_remove,
};

MODULE_DESCRIPTION("Audience earSmart SPI core driver");
MODULE_AUTHOR("Greg Clemson <gclemson@audience.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:earSmart-codec");
