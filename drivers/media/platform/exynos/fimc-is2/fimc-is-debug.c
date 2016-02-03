/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>

#include "fimc-is-config.h"
#include "fimc-is-debug.h"
#include "fimc-is-device-ischain.h"

struct fimc_is_debug fimc_is_debug;

#define DEBUG_FS_ROOT_NAME	"fimc-is"
#define DEBUG_FS_LOGFILE_NAME	"isfw-msg"
#define DEBUG_FS_IMGFILE_NAME	"dump-img"

static const struct file_operations debug_log_fops;
static const struct file_operations debug_img_fops;

void fimc_is_dmsg_init(void)
{
	fimc_is_debug.dsentence_pos = 0;
	memset(fimc_is_debug.dsentence, 0x0, DEBUG_SENTENCE_MAX);
}

void fimc_is_dmsg_concate(const char *fmt, ...)
{
	va_list ap;
	char term[50];
	u32 copy_len;

	va_start(ap, fmt);
	vsnprintf(term, sizeof(term), fmt, ap);
	va_end(ap);

	copy_len = min((DEBUG_SENTENCE_MAX - fimc_is_debug.dsentence_pos), strlen(term));
	strncpy(fimc_is_debug.dsentence + fimc_is_debug.dsentence_pos, term, copy_len);
	fimc_is_debug.dsentence_pos += copy_len;
}

char *fimc_is_dmsg_print(void)
{
	return fimc_is_debug.dsentence;
}

int fimc_is_debug_probe(void)
{
	fimc_is_debug.debug_cnt = 0;
	fimc_is_debug.minfo = NULL;

	fimc_is_debug.dump_count = DBG_IMAGE_DUMP_COUNT;
	fimc_is_debug.img_kvaddr = 0;
	fimc_is_debug.img_cookie = 0;
	fimc_is_debug.size = 0;
	fimc_is_debug.dsentence_pos = 0;
	memset(fimc_is_debug.dsentence, 0x0, DEBUG_SENTENCE_MAX);

#ifdef ENABLE_DBG_FS
	fimc_is_debug.root = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);
	if (fimc_is_debug.root)
		probe_info("%s is created\n", DEBUG_FS_ROOT_NAME);

	fimc_is_debug.logfile = debugfs_create_file(DEBUG_FS_LOGFILE_NAME, S_IRUSR,
		fimc_is_debug.root, &fimc_is_debug, &debug_log_fops);
	if (fimc_is_debug.logfile)
		probe_info("%s is created\n", DEBUG_FS_LOGFILE_NAME);

#ifdef DBG_IMAGE_DUMP
	fimc_is_debug.imgfile = debugfs_create_file(DEBUG_FS_IMGFILE_NAME, S_IRUSR,
		fimc_is_debug.root, &fimc_is_debug, &debug_img_fops);
	if (fimc_is_debug.imgfile)
		probe_info("%s is created\n", DEBUG_FS_IMGFILE_NAME);
#endif
#endif

	return 0;
}

int fimc_is_debug_open(struct fimc_is_minfo *minfo)
{
	fimc_is_debug.debug_cnt = 0;
	fimc_is_debug.minfo = minfo;

	set_bit(FIMC_IS_DEBUG_OPEN, &fimc_is_debug.state);

	return 0;
}

int fimc_is_debug_close(void)
{
	clear_bit(FIMC_IS_DEBUG_OPEN, &fimc_is_debug.state);

	return 0;
}

static int isfw_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static ssize_t isfw_debug_read(struct file *file, char __user *user_buf,
	size_t buf_len, loff_t *ppos)
{
	char *debug;
	size_t debug_cnt, backup_cnt;
	size_t count1, count2;
	size_t buf_count;
	size_t writes;
	struct fimc_is_minfo *minfo;

	count1 = 0;
	count2 = 0;
	debug_cnt = 0;
	minfo = fimc_is_debug.minfo;

retry:
	if (!test_bit(FIMC_IS_DEBUG_OPEN, &fimc_is_debug.state))
		return 0;

	vb2_ion_sync_for_device(minfo->fw_cookie, DEBUG_OFFSET, DEBUG_CNT, DMA_FROM_DEVICE);

	debug_cnt = *((int *)(minfo->kvaddr + DEBUGCTL_OFFSET)) - DEBUG_OFFSET;
	backup_cnt = fimc_is_debug.debug_cnt;

	if (fimc_is_debug.debug_cnt > debug_cnt) {
		count1 = DEBUG_CNT - fimc_is_debug.debug_cnt;
		count2 = debug_cnt;
	} else {
		count1 = debug_cnt - fimc_is_debug.debug_cnt;
		count2 = 0;
	}

	buf_count = buf_len;

	if (buf_count && count1) {
		debug = (char *)(minfo->kvaddr + DEBUG_OFFSET + fimc_is_debug.debug_cnt);

		if (count1 > buf_count)
			count1 = buf_count;

		buf_count -= count1;

		memcpy(user_buf, debug, count1);
		fimc_is_debug.debug_cnt += count1;
	}

	if (buf_count && count2) {
		debug = (char *)(minfo->kvaddr + DEBUG_OFFSET);

		if (count2 > buf_count)
			count2 = buf_count;

		buf_count -= count2;

		memcpy(user_buf, debug, count2);
		fimc_is_debug.debug_cnt = count2;
	}

	info("FW_READ : Origin(%zd), New(%zd) - Length(%zd)\n",
		backup_cnt,
		fimc_is_debug.debug_cnt,
		(buf_len - buf_count));

	writes = buf_len - buf_count;
	if (writes == 0) {
		msleep(500);
		goto retry;
	}

	return writes;
}

int imgdump_request(ulong cookie, ulong kvaddr, size_t size)
{
	if (fimc_is_debug.dump_count && (fimc_is_debug.size == 0) && (fimc_is_debug.img_kvaddr == 0)) {
		fimc_is_debug.dump_count--;
		fimc_is_debug.img_cookie = cookie;
		fimc_is_debug.img_kvaddr = kvaddr;
		fimc_is_debug.size = size;
	}

	return 0;
}

static int imgdump_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static ssize_t imgdump_debug_read(struct file *file, char __user *user_buf,
	size_t buf_len, loff_t *ppos)
{
	size_t size = 0;

	if (buf_len <= fimc_is_debug.size)
		size = buf_len;
	else
		size = fimc_is_debug.size;

	if (size) {
		vb2_ion_sync_for_device((void *)fimc_is_debug.img_cookie, 0, size, DMA_FROM_DEVICE);
		memcpy(user_buf, (void *)fimc_is_debug.img_kvaddr, size);
		info("DUMP : %p, SIZE : %zd\n", (void *)fimc_is_debug.img_kvaddr, size);
	}

	fimc_is_debug.img_cookie += size;
	fimc_is_debug.img_kvaddr += size;
	fimc_is_debug.size -= size;

	if (size == 0) {
		fimc_is_debug.img_cookie = 0;
		fimc_is_debug.img_kvaddr = 0;
	}

	return size;
}

static const struct file_operations debug_log_fops = {
	.open	= isfw_debug_open,
	.read	= isfw_debug_read,
	.llseek	= default_llseek
};

static const struct file_operations debug_img_fops = {
	.open	= imgdump_debug_open,
	.read	= imgdump_debug_read,
	.llseek	= default_llseek
};
