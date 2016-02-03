/*
 * fts.c
 *
 * FTS driver support to tunning touch (FingerTipS)
 *
 * Copyright (C) MEIZU Limited.
 * Authors:robin 
 *        
 *         
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/vmalloc.h>
#include "fts.h"
#include "tpd.h"

#define FTS_W_CMD (0xAA)
#define FTS_R_CMD (0xBB)
#define FTS_WR_SIZE 1080*1920*2 
unsigned char * fts_wr_buf = NULL ;

struct fts_data {
	unsigned char * reg ;
	unsigned int  reg_len ;

	/*only for read  */
	unsigned char *buf ;
	unsigned int  read_len ;
};

extern int fts_read_reg(struct fts_ts_info *info, unsigned char *reg, int cnum,
						unsigned char *pbuf, int num);	
extern int fts_write_reg(struct fts_ts_info *info, unsigned char *reg,
						 unsigned short len);


static char *fts_char_devnode(struct device *dev, mode_t *mode)
{
	if (!mode)
		return NULL;

	*mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	return kasprintf(GFP_KERNEL, "rmi/%s", dev_name(dev));
}

static int ftsdev_create_device_class(struct fts_ts_info *info)
{
	info->main_class = class_create(THIS_MODULE, "fts");

	if (!info->main_class) {
		pr_err("%s: Failed to create /dev/%s\n",
				__func__, "fts");
		return -ENODEV;
	}

	info->main_class->devnode = fts_char_devnode;

	return 0;
}

static loff_t ftsdev_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	struct fts_ts_info *dev_data = filp->private_data;

	if (IS_ERR(dev_data)) {
		pr_err("%s: Pointer of char device data is invalid", __func__);
		return -EBADF;
	}

	mutex_lock(&(dev_data->file_mutex));

	switch (whence) {
	case SEEK_SET:
		newpos = off;
		break;
	case SEEK_CUR:
		newpos = filp->f_pos + off;
		break;
	case SEEK_END:
		newpos = filp->f_pos + off;
		break;
	default:
		newpos = -EINVAL;
		goto clean_up;
	}

	if (newpos < 0) {
		dev_err(&dev_data->client->dev,
				"%s: New position 0x%04x is invalid\n",
				__func__, (unsigned int)newpos);
		newpos = -EINVAL;
		goto clean_up;
	}

	filp->f_pos = newpos;

clean_up:
	mutex_unlock(&(dev_data->file_mutex));

	return newpos;
}

static ssize_t ftsdev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos){

		return -EIO ;
}

static ssize_t ftsdev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	return -EIO ;
}

static int ftsdev_open(struct inode *inp, struct file *filp)
{
	int retval = 0;
	int retry = 3 ;
	struct fts_ts_info *dev_data =
			container_of(inp->i_cdev, struct fts_ts_info, main_cdev);

	info_printk("Enter(%p)\n",dev_data);

	if (!dev_data)
		return -EACCES;

	filp->private_data = dev_data;

	mutex_lock(&(dev_data->file_mutex));


	if(!fts_wr_buf){
RETRY:
	 fts_wr_buf = vzalloc(FTS_WR_SIZE);
	if(!fts_wr_buf){
		if(retry--)
			goto RETRY ;
	info_printk("alloc buf (%d) error\n",FTS_WR_SIZE);	
	retval = -ENODEV ;
	}
}
exit:
	mutex_unlock(&(dev_data->file_mutex));
	return retval;
}


static int ftsdev_release(struct inode *inp, struct file *filp)
{
	
	struct fts_ts_info *dev_data =
			container_of(inp->i_cdev, struct fts_ts_info, main_cdev);
	if (!dev_data)
		return -EACCES;

	mutex_lock(&(dev_data->file_mutex));
    if(fts_wr_buf){
		vfree(fts_wr_buf);
		fts_wr_buf = NULL ;
    }
	mutex_unlock(&(dev_data->file_mutex));

	return 0;
}


static long ftsdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	struct fts_data  ftsdata ;
	struct fts_ts_info * info = (struct fts_ts_info*)file->private_data ;
	unsigned char buf[64] ;
	int retval = 0 ;
	info_printk("cmd(%x)\n",cmd);
	
	mutex_lock(&info->file_mutex);
	switch(cmd){
	case  FTS_W_CMD :
		  if(copy_from_user(&ftsdata,(struct fts_data __user *)arg,sizeof(struct fts_data))){
				info_printk("copy ftsdata from user error\n");
			  retval= -EIO ;
			  goto OUT ;
		  }
		  if(copy_from_user(fts_wr_buf,(unsigned char __user *)ftsdata.reg,ftsdata.reg_len)){
		    	info_printk("copy write data from user error\n");
			    retval = -EIO ;
		  }
		  fts_write_reg(info,fts_wr_buf,ftsdata.reg_len);
	break ; 
		
	case FTS_R_CMD :
		if(copy_from_user(&ftsdata,(struct fts_data __user*)arg,sizeof(struct fts_data))){
			retval = -EIO ;
			goto OUT ;
		}
		if(copy_from_user(buf,(unsigned char __user*)ftsdata.reg,ftsdata.reg_len)){
			retval = -EIO ;
			goto OUT ;
		}
		info_printk("read len(%d)\n",ftsdata.read_len);
#ifdef FTS_DEBUG
		int i ;
		for(i=0;i<ftsdata.reg_len;i++)
			info_printk("reg data(%d)(%x)\n",i,buf[i]);
#endif
		retval = fts_read_reg(info,buf,ftsdata.reg_len,fts_wr_buf,ftsdata.read_len);
		if(!retval){
				retval= copy_to_user((unsigned char __user*)ftsdata.buf,fts_wr_buf,ftsdata.read_len);
		}else 
		   info_printk("read error \n");
	break ;
		
	default :
	 retval= -EIO ;
	}
	
OUT:
	mutex_unlock(&info->file_mutex);
	return retval;
}



static const struct file_operations ftsdev_fops = {
	.owner = THIS_MODULE,
	.llseek = ftsdev_llseek,
	.read = ftsdev_read,
	.write = ftsdev_write,
	.unlocked_ioctl	= ftsdev_ioctl,
	.open = ftsdev_open,
	.release = ftsdev_release,
};

 int fts_create_dev(struct fts_ts_info *info)
{
	dev_t dev_no;
	int retval = 0 ;
    struct device * device_ptr ;

	mutex_init(&info->file_mutex);
	retval = alloc_chrdev_region(&dev_no, 0, 1, "fts");
		if (retval < 0) {
			dev_err(&info->client->dev,
					"%s: Failed to allocate char device region\n",
					__func__);
			return 0;
		}
	cdev_init(&info->main_cdev, &ftsdev_fops);
	retval = cdev_add(&info->main_cdev, dev_no, 1);
	if (retval < 0) {
		dev_err(&info->client->dev,
				"%s: Failed to add rmi char device\n",
				__func__);
		return 0;
	}
	
	ftsdev_create_device_class(info);
	device_ptr = device_create(info->main_class, NULL, dev_no,
			NULL, "fts""%d", MINOR(dev_no));
	if (IS_ERR(device_ptr)) {
		dev_err(&info->client->dev,
				"%s: Failed to create rmi char device\n",
				__func__);
		return  -ENODEV;
	}
	
	return 0 ;
}



