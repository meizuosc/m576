/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#ifdef CONFIG_LINK_DEVICE_LLI
#include <linux/mipi-lli.h>
#endif

#include "modem_prj.h"

#define SPI_XMIT_DELEY	100

static int check_cp_status(unsigned int gpio_cp_status, unsigned int count)
{
	int ret = 0;
	int cnt = 0;

	while (1) {
		if (gpio_get_value(gpio_cp_status) != 0) {
			ret = 0;
			break;
		}

		cnt++;
		if (cnt >= count) {
			mif_err("ERR! gpio_cp_status == 0 (cnt %d)\n", cnt);
			ret = -EFAULT;
			break;
		}

		msleep(20);
	}

	return ret;
}

static inline int spi_send(struct modem_boot_spi *boot, const char *buff, unsigned len)
{
	int ret;
	struct spi_message msg;
	struct spi_transfer xfer = {
		.len = len,
		.tx_buf = buff,
	};

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(boot->spi_dev, &msg);
	if (ret < 0)
		mif_err("ERR! spi_sync fail (err %d)\n", ret);

	return ret;
}

static int spi_boot_write(struct modem_boot_spi *boot, const char *addr,
			  const long len)
{
	int ret = 0;
	char *buff = NULL;
	mif_err("+++\n");

	buff = kzalloc(len, GFP_KERNEL);
	if (!buff) {
		mif_err("ERR! kzalloc(%ld) fail\n", len);
		ret = -ENOMEM;
		goto exit;
	}

	ret = copy_from_user(buff, (const void __user *)addr, len);
	if (ret) {
		mif_err("ERR! copy_from_user fail (err %d)\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	ret = spi_send(boot, buff, len);
	if (ret < 0) {
		mif_err("ERR! spi_send fail (err %d)\n", ret);
		goto exit;
	}

exit:
	if (buff)
		kfree(buff);

	mif_err("---\n");
	return ret;
}

static int spi_boot_open(struct inode *inode, struct file *filp)
{
	struct modem_boot_spi *boot = to_modem_boot_spi(filp->private_data);
	filp->private_data = boot;
	return 0;
}

static long spi_boot_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	int ret = 0;
	struct modem_firmware img;
	struct modem_boot_spi *boot = filp->private_data;
#ifdef CONFIG_LINK_DEVICE_LLI // Insert workaround temporary
	int cnt = 0;
#endif


	mutex_lock(&boot->lock);
	switch (cmd) {
	case IOCTL_MODEM_XMIT_BOOT:

		memset(&img, 0, sizeof(struct modem_firmware));
		ret = copy_from_user(&img, (const void __user *)arg,
					sizeof(struct modem_firmware));
		if (ret) {
			mif_err("ERR! copy_from_user fail (err %d)\n", ret);
			ret = -EFAULT;
			goto exit_err;
		}
		mif_info("IOCTL_MODEM_XMIT_BOOT (size %d)\n", img.size);

		if (img.size > SZ_16K) {
			mif_err("Unexpected cp bootloader size : 0x%x\n", img.size);
			goto exit_err;
		}

		ret = spi_boot_write(boot, img.binary, img.size);
		if (ret < 0) {
			mif_err("ERR! spi_boot_write fail (err %d)\n", ret);
			break;
		}

		if (!boot->gpio_cp_status)
			break;

		ret = check_cp_status(boot->gpio_cp_status, 100);
		if (ret < 0) {
			mif_err("ERR! check_cp_status fail (err %d)\n", ret);
			break;
		}

#ifdef CONFIG_LINK_DEVICE_LLI // Insert workaround temporary
		cnt = 100;
		while (mipi_lli_get_link_status() != LLI_MOUNTED) {
			if (cnt <= 0) {
				mif_err("ERR! LLI not setup !!!\n");
				ret = -EIO;
				break;
			}
			cnt--;
			msleep(20);
		}
#endif

		break;

	default:
		mif_err("ioctl cmd error\n");
		ret = -ENOIOCTLCMD;

		break;
	}
	mutex_unlock(&boot->lock);

exit_err:
	return ret;
}

static const struct file_operations modem_spi_boot_fops = {
	.owner = THIS_MODULE,
	.open = spi_boot_open,
	.unlocked_ioctl = spi_boot_ioctl,
};

static int modem_spi_boot_probe(struct spi_device *spi)
{
	int ret;
	struct device *dev = &spi->dev;
	struct modem_boot_spi *boot;
	mif_info("%s:+++\n", __func__);

	boot = devm_kzalloc(dev, sizeof(struct modem_boot_spi), GFP_KERNEL);
	if (!boot) {
		mif_err("failed to allocate for modem_boot_spi\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	mutex_init(&boot->lock);

	spi->bits_per_word = 8;

	if (spi_setup(spi)) {
		mif_err("ERR! spi_setup fail\n");
		ret = -EINVAL;
		goto err_setup;
	}
	boot->spi_dev = spi;

	if (dev->of_node) {
		struct device_node *np;
		unsigned gpio_cp_status;

		np = of_find_compatible_node(NULL, NULL,
					     "sec_modem,modem_pdata");
		if (!np) {
			mif_err("DT, failed to get node\n");
			ret = -EINVAL;
			goto err_setup;
		}

		gpio_cp_status = of_get_named_gpio(np, "mif,gpio_cp_status", 0);
		if (!gpio_is_valid(gpio_cp_status)) {
			mif_err("gpio_cp_status: Invalied gpio pins\n");
			ret = -EINVAL;
			goto err_setup;
		}
		mif_err("gpio_cp_status: %d\n", gpio_cp_status);

		boot->gpio_cp_status = gpio_cp_status;
	} else {
		struct modem_boot_spi_platform_data *pdata;

		pdata = dev->platform_data;
		if (!pdata) {
			mif_err("Non-DT, incorrect pdata!\n");
			ret = -EINVAL;
			goto err_setup;
		}

		boot->gpio_cp_status = pdata->gpio_cp_status;
	}

	spi_set_drvdata(spi, boot);

	boot->misc_dev.minor = MISC_DYNAMIC_MINOR;
	boot->misc_dev.name = MODEM_BOOT_DEV_SPI;
	boot->misc_dev.fops = &modem_spi_boot_fops;
	ret = misc_register(&boot->misc_dev);
	if (ret) {
		mif_err("ERR! misc_register fail (err %d)\n", ret);
		goto err_setup;
	}

	mif_err("%s:---\n",__func__);
	return 0;

err_setup:
	mutex_destroy(&boot->lock);
	devm_kfree(dev, boot);

err_alloc:
	mif_err("xxx\n");
	return ret;
}

static int modem_spi_boot_remove(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct modem_boot_spi *boot = spi_get_drvdata(spi);

	misc_deregister(&boot->misc_dev);
	mutex_destroy(&boot->lock);
	devm_kfree(dev, boot);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id modem_boot_spi_match[] = {
	{ .compatible = MODEM_BOOT_DEV_SPI },
	{},
};
MODULE_DEVICE_TABLE(of, modem_boot_spi_match);
#endif

static struct spi_driver modem_boot_device_spi_driver = {
	.probe = modem_spi_boot_probe,
	.remove = modem_spi_boot_remove,
	.driver = {
		.name = MODEM_BOOT_DEV_SPI,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(modem_boot_spi_match),
#endif
	},
};

static int __init modem_boot_device_spi_init(void)
{
	int err;

	err = spi_register_driver(&modem_boot_device_spi_driver);
	if (err) {
		mif_err("spi_register_driver fail (err %d)\n", err);
		return err;
	}

	return 0;
}

static void __exit modem_boot_device_spi_exit(void)
{
	spi_unregister_driver(&modem_boot_device_spi_driver);
}

module_init(modem_boot_device_spi_init);
module_exit(modem_boot_device_spi_exit);

MODULE_DESCRIPTION("SPI Driver for Downloading Modem Bootloader");
MODULE_LICENSE("GPL");
