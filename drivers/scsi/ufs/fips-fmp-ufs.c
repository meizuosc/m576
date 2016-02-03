/*
 * Exynos FMP UFS driver for FIPS
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/buffer_head.h>
#include <linux/mtd/mtd.h>
#include <linux/kdev_t.h>
#include <fips-fmp.h>

#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include "ufshcd.h"
#include "../scsi_priv.h"

#define byte2word(b0, b1, b2, b3) 	\
		((unsigned int)(b0) << 24) | ((unsigned int)(b1) << 16) | ((unsigned int)(b2) << 8) | (b3)
#define get_word(x, c)	byte2word(((unsigned char *)(x) + 4 * (c))[0], ((unsigned char *)(x) + 4 * (c))[1], \
			((unsigned char *)(x) + 4 * (c))[2], ((unsigned char *)(x) + 4 * (c))[3])

#define SF_OFFSET	(20 * 1024)
#define MAX_SCAN_PART	(50)

struct ufs_fmp_work {
	struct Scsi_Host *host;
	struct scsi_device *sdev;
	struct block_device *bdev;
	sector_t sector;
	dev_t devt;
};

struct ufshcd_sg_entry *prd_table;
struct ufshcd_sg_entry *ucd_prdt_ptr_st;

static int ufs_fmp_init(struct platform_device *pdev, uint32_t mode)
{
	struct device *dev = &pdev->dev;
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;

	work = platform_get_drvdata(pdev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}
	host = work->host;
	hba = shost_priv(host);

	ucd_prdt_ptr_st = kmalloc(sizeof(struct ufshcd_sg_entry), GFP_KERNEL);
	if (!ucd_prdt_ptr_st) {
		dev_err(dev, "Fail to alloc prdt ptr for self test\n");
		return -ENOMEM;
	}
	hba->ucd_prdt_ptr_st = ucd_prdt_ptr_st;

	return 0;
}

static int ufs_fmp_set_key(struct platform_device *pdev, uint32_t mode, uint8_t *key, uint32_t key_len)
{
	struct device *dev = &pdev->dev;
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;
	struct ufshcd_sg_entry *prd_table;

	work = platform_get_drvdata(pdev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}

	host = work->host;
	hba = shost_priv(host);
	if (!hba->ucd_prdt_ptr_st) {
		dev_err(dev, "prdt ptr for fips is not allocated\n");
		return -ENOMEM;
	}

	prd_table = hba->ucd_prdt_ptr_st;

	if (mode == CBC_MODE) {
		SET_FAS(prd_table, CBC_MODE);

		switch (key_len) {
		case 32:
			prd_table->size = 32;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 7);
			prd_table->file_enckey1 = get_word(key, 6);
			prd_table->file_enckey2 = get_word(key, 5);
			prd_table->file_enckey3 = get_word(key, 4);
			prd_table->file_enckey4 = get_word(key, 3);
			prd_table->file_enckey5 = get_word(key, 2);
			prd_table->file_enckey6 = get_word(key, 1);
			prd_table->file_enckey7 = get_word(key, 0);
			break;
		case 16:
			prd_table->size = 16;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 3);
			prd_table->file_enckey1 = get_word(key, 2);
			prd_table->file_enckey2 = get_word(key, 1);
			prd_table->file_enckey3 = get_word(key, 0);
			prd_table->file_enckey4 = 0;
			prd_table->file_enckey5 = 0;
			prd_table->file_enckey6 = 0;
			prd_table->file_enckey7 = 0;
			break;
		default:
			dev_err(dev, "Invalid key length : %d\n", key_len);
			return -EINVAL;
		}
	} else if (mode == XTS_MODE) {
		SET_FAS(prd_table, XTS_MODE);

		switch (key_len) {
		case 64:
			prd_table->size = 32;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 7);
			prd_table->file_enckey1 = get_word(key, 6);
			prd_table->file_enckey2 = get_word(key, 5);
			prd_table->file_enckey3 = get_word(key, 4);
			prd_table->file_enckey4 = get_word(key, 3);
			prd_table->file_enckey5 = get_word(key, 2);
			prd_table->file_enckey6 = get_word(key, 1);
			prd_table->file_enckey7 = get_word(key, 0);

			/* tweak key */
			prd_table->file_twkey0 = get_word(key, 15);
			prd_table->file_twkey1 = get_word(key, 14);
			prd_table->file_twkey2 = get_word(key, 13);
			prd_table->file_twkey3 = get_word(key, 12);
			prd_table->file_twkey4 = get_word(key, 11);
			prd_table->file_twkey5 = get_word(key, 10);
			prd_table->file_twkey6 = get_word(key, 9);
			prd_table->file_twkey7 = get_word(key, 8);
			break;
		case 32:
			prd_table->size = 16;
			/* encrypt key */
			prd_table->file_enckey0 = get_word(key, 3);
			prd_table->file_enckey1 = get_word(key, 2);
			prd_table->file_enckey2 = get_word(key, 1);
			prd_table->file_enckey3 = get_word(key, 0);
			prd_table->file_enckey4 = 0;
			prd_table->file_enckey5 = 0;
			prd_table->file_enckey6 = 0;
			prd_table->file_enckey7 = 0;

			/* tweak key */
			prd_table->file_twkey0 = get_word(key, 7);
			prd_table->file_twkey1 = get_word(key, 6);
			prd_table->file_twkey2 = get_word(key, 5);
			prd_table->file_twkey3 = get_word(key, 4);
			prd_table->file_twkey4 = 0;
			prd_table->file_twkey5 = 0;
			prd_table->file_twkey6 = 0;
			prd_table->file_twkey7 = 0;
			break;
		default:
			dev_err(dev, "Invalid key length : %d\n", key_len);
			return -EINVAL;
		}
	} else if (mode == BYPASS_MODE) {
		SET_FAS(prd_table, BYPASS_MODE);

		/* enc key */
		prd_table->file_enckey0 = 0;
		prd_table->file_enckey1 = 0;
		prd_table->file_enckey2 = 0;
		prd_table->file_enckey3 = 0;
		prd_table->file_enckey4 = 0;
		prd_table->file_enckey5 = 0;
		prd_table->file_enckey6 = 0;
		prd_table->file_enckey7 = 0;

		/* tweak key */
		prd_table->file_twkey0 = 0;
		prd_table->file_twkey1 = 0;
		prd_table->file_twkey2 = 0;
		prd_table->file_twkey3 = 0;
		prd_table->file_twkey4 = 0;
		prd_table->file_twkey5 = 0;
		prd_table->file_twkey6 = 0;
		prd_table->file_twkey7 = 0;
	} else {
		dev_err(dev, "Invalid mode : %d\n", mode);
		return -EINVAL;
	}

	return 0;
}

static int ufs_fmp_set_iv(struct platform_device *pdev, uint32_t mode, uint8_t *iv, uint32_t iv_len)
{
	struct device *dev = &pdev->dev;
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;
	struct ufshcd_sg_entry *prd_table;

	work = platform_get_drvdata(pdev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}

	host = work->host;
	hba = shost_priv(host);
	if (!hba->ucd_prdt_ptr_st) {
		dev_err(dev, "prdt ptr for fips is not allocated\n");
		return -ENOMEM;
	}

	prd_table = hba->ucd_prdt_ptr_st;

	if (mode == CBC_MODE || mode == XTS_MODE) {
		prd_table->file_iv0 = get_word(iv, 3);
		prd_table->file_iv1 = get_word(iv, 2);
		prd_table->file_iv2 = get_word(iv, 1);
		prd_table->file_iv3 = get_word(iv, 0);
	} else if (mode == BYPASS_MODE) {
		prd_table->file_iv0 = 0;
		prd_table->file_iv1 = 0;
		prd_table->file_iv2 = 0;
		prd_table->file_iv3 = 0;
	} else {
		dev_err(dev, "Invalid mode : %d\n", mode);
		return -EINVAL;
	}

	return 0;
}

static dev_t find_devt_for_selftest(void)
{
	int i, idx = 0;
	uint64_t size;
	uint64_t size_list[MAX_SCAN_PART];
	dev_t devt_list[MAX_SCAN_PART];
	dev_t devt_scan, devt;
	struct block_device *bdev;
	fmode_t fmode = FMODE_WRITE | FMODE_READ;

	for (i = 0; i < MAX_SCAN_PART; i++) {
		devt_scan = blk_lookup_devt("sda", i);
		bdev = blkdev_get_by_dev(devt_scan, fmode, NULL);
		if (IS_ERR(bdev))
			continue;
		else {
			size_list[idx++] = (uint64_t)i_size_read(bdev->bd_inode);
			devt_list[idx++] = devt_scan;
		}
	}

	if (!idx)
		goto err;

	for (i = 0; i < idx; i++) {
		if (i == 0) {
			size = size_list[i];
			devt = devt_list[i];
		} else {
			if (size < size_list[i])
				devt = devt_list[i];
		}
	}

	return devt;

err:
	return (dev_t)0;
}

static int ufs_fmp_run(struct platform_device *pdev, uint32_t mode, uint8_t *data, uint32_t len)
{
	int write;
	struct ufs_hba *hba;
	struct ufs_fmp_work *work;
	struct Scsi_Host *host;
	struct device *dev = &pdev->dev;
	static struct buffer_head *bh;

	work = platform_get_drvdata(pdev);
	if (!work) {
		dev_err(dev, "Fail to get work from platform device\n");
		return -ENODEV;
	}
	host = work->host;
	hba = shost_priv(host);

	if ((mode == XTS_MODE) || (mode == CBC_MODE))
		write = 1;
	else if (mode == BYPASS_MODE)
		write = 0;
	else {
		dev_err(dev, "Fail to run FMP due to abnormal mode = %d\n", mode);
		return -EINVAL;
	}
	hba->self_test = mode;

	bh = __getblk(work->bdev, work->sector, FMP_BLK_SIZE);
	if (!bh) {
		dev_err(dev, "Fail to get block from bdev\n");
		hba->self_test = 0;
		return -ENODEV;
	}

	get_bh(bh);
	if (write) {
		memcpy(bh->b_data, data, len);
		bh->b_state &= ~(1 << BH_Uptodate);
		bh->b_state &= ~(1 << BH_Lock);
		ll_rw_block(WRITE_FLUSH_FUA, 1, &bh);
		wait_on_buffer(bh);

		memset(bh->b_data, 0, FMP_BLK_SIZE);
	} else {
		bh->b_state &= ~(1 << BH_Uptodate);
		bh->b_state &= ~(1 << BH_Lock);
		ll_rw_block(READ_SYNC, 1, &bh);
		wait_on_buffer(bh);

		memcpy(data, bh->b_data, len);
	}
	hba->self_test = 0;
	put_bh(bh);

	return 0;
}

static int ufs_fmp_exit(void)
{
	if (ucd_prdt_ptr_st)
		kfree(ucd_prdt_ptr_st);

	return 0;
}

struct fips_fmp_ops fips_fmp_fops = {
	.init = ufs_fmp_init,
	.set_key = ufs_fmp_set_key,
	.set_iv = ufs_fmp_set_iv,
	.run = ufs_fmp_run,
	.exit = ufs_fmp_exit,
};

int fips_fmp_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ufs_fmp_work *work;
	struct device_node *dev_node;
	struct platform_device *pdev_ufs;
	struct ufs_hba *hba;
	struct Scsi_Host *host;
	struct scsi_device *sdev;
	fmode_t fmode = FMODE_WRITE | FMODE_READ;

	work = kmalloc(sizeof(*work), GFP_KERNEL);
	if (!work) {
		dev_err(dev, "Fail to alloc fmp work buffer\n");
		return -ENOMEM;
	}

	dev_node = of_find_compatible_node(NULL, NULL, "samsung,exynos-ufs");
	if (!dev_node) {
		dev_err(dev, "Fail to find exynos ufs device node\n");
		goto out;
	}

	pdev_ufs = of_find_device_by_node(dev_node);
	if (!pdev_ufs) {
		dev_err(dev, "Fail to find exynos ufs pdev\n");
		goto out;
	}

	hba = platform_get_drvdata(pdev_ufs);
	if (!hba) {
		dev_err(dev, "Fail to find hba from pdev,\n");
		goto out;
	}

	host = hba->host;
	sdev = to_scsi_device(&pdev_ufs->dev);
	work->host = host;
	work->sdev = sdev;

	work->devt = find_devt_for_selftest();
	if (!work->devt) {
		dev_err(dev, "Fail to find devt for self test\n");
		return -ENODEV;
	}

	work->bdev = blkdev_get_by_dev(work->devt, fmode, NULL);
	if (IS_ERR(work->bdev)) {
		dev_err(dev, "Fail to open block device\n");
		return -ENODEV;
	}
	work->sector = (sector_t)((i_size_read(work->bdev->bd_inode) - SF_OFFSET) / FMP_BLK_SIZE);

	platform_set_drvdata(pdev, work);

	return 0;

out:
	if (work)
		kfree(work);

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(fips_fmp_init);
