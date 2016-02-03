/*
 * fts.c
 *
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2012, 2013 STMicroelectronics Limited.
 * Authors: AMS(Analog Mems Sensor)
 *		: Victor Phay <victor.phay@st.com>
 *		: Li Wu <li.wu@st.com>
 *		: Giuseppe Di Giore <giuseppe.di-giore@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
#include <linux/device.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/completion.h>
#include <linux/wakelock.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include "fts.h"

#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/meizu-sys.h>

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#ifdef CONFIG_JANUARY_BOOSTER
#include <linux/input/janeps_booster.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
#include <linux/exynos_touch_daemon.h>
extern struct exynos_touch_daemon_data exynos_touch_daemon_data;
#endif

#define	LINK_KOBJ_NAME	"tp"

/*
 * Uncomment to use polling mode insead of interrupt mode.
 *
 */
//  #define FTS_USE_POLLING_MODE

static char *fts_fw_filename[] = {
	"st_fts.bin",
	"fts_fw.bin",
	"fts_config_fw.bin",
	"fts64_fw.bin"
};

/*
 * Event installer helpers
 */
#define event_id(_e)	 EVENTID_##_e
#define handler_name(_h) fts_##_h##_event_handler

#define install_handler(_i, _evt, _hnd) \
do { \
	_i->event_dispatch_table[event_id(_evt)] = handler_name(_hnd); \
} while (0)


/*
 * Asyncronouns command helper
 */
#define WAIT_WITH_TIMEOUT(_info, _timeout, _command) \
do { \
	if (wait_for_completion_timeout(&_info->cmd_done, _timeout) == 0) { \
		dev_warn(_info->dev, "Waiting for %s command: timeout\n", \
		#_command); \
	} \
} while (0)

#ifdef KERNEL_ABOVE_2_6_38
#define TYPE_B_PROTOCOL
#endif

unsigned char afe_version_same = 0x0;

/* forward declarations */
static void fts_interrupt_enable(struct fts_ts_info *info);
static int fts_fw_upgrade(struct fts_ts_info *info, int mode,int fw_forceupdate);
static int fts_init_hw(struct fts_ts_info *info);
static int fts_init_flash_reload(struct fts_ts_info *info);
static int fts_command(struct fts_ts_info *info, unsigned char cmd);
static void fts_interrupt_set(struct fts_ts_info *info, int enable);
static int fts_systemreset(struct fts_ts_info *info);
extern int hall_register_notifier_client(struct notifier_block * nb);
extern int hall_unregister_notifier_client(struct notifier_block * nb);
static int fts_get_init_status(struct fts_ts_info *info);
static int fts_get_fw_version(struct fts_ts_info *info);
static int fts_chip_powercycle(struct fts_ts_info *info);



static int fts_write_reg(struct fts_ts_info *info, unsigned char *reg,
						 unsigned short len)
{
	struct i2c_msg xfer_msg[1];

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = len;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	return (i2c_transfer(info->client->adapter, xfer_msg, 1) != 1);
}


static int fts_read_reg(struct fts_ts_info *info, unsigned char *reg, int cnum,
						unsigned char *buf, int num)
{
	struct i2c_msg xfer_msg[2];

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = buf;

	return (i2c_transfer(info->client->adapter, xfer_msg, 2) != 2);
}

static inline void fts_set_sensor_mode(struct fts_ts_info *info,int mode)
{
	if(!info)
		return ;
	mutex_lock(&info->fts_mode_mutex);
	info->mode = mode ;
	mutex_unlock(&info->fts_mode_mutex);
	return ;
}


static void swipe_gesture_control(char *data, char *reg)
{
	if (!reg)
		return;

	if(data[0]&0x01)
		reg[0] |= (1<<7);
	else
		reg[0] &= ~(1<<7);
	
	if(data[0]&0x02)
		reg[1] |= (1);
	else
		reg[1] &= ~(1);
	
	if(data[0]&0x04)
		reg[1] |= (1<<1);
	else
		reg[1] &= ~(1<<1);
	
	if(data[0]&0x08)
		reg[1] |= (1<<2);
	else
		reg[1] &= ~(1<<2);
}

static void unicode_gesture_control(char *data, char *reg)
{
	/*handler V*/
	if(data[0]&0x01)
		reg[1] |= (1<<5);
	else
		reg[1] &= ~(1<<5);
	/*handler C*/
	if(data[0]&0x02)
		reg[0] |= (1<<3);
	else
		reg[0] &= ~(1<<3);
	/*handler E*/
	if(data[0]&0x04)
		reg[0] |= (1<<6);
	else
		reg[0] &= ~(1<<6);
	/*handler W*/
	if(data[0]&0x08)
		reg[0] |= (1<<5);
	else
		reg[0] &= ~(1<<5);
	/*handler M*/
	if(data[0]&0x10)
		reg[0] |= (1<<4);
	else
		reg[0] &= ~(1<<4);
	/*handler S*/
	if(data[0]&0x20)
		reg[1] |= (1<<7);
	else
		reg[1] &= ~(1<<7);
	/*handler O*/
	if(data[0]&0x80)
		reg[0] |= (1<<2);
	else
		reg[0] &= ~(1<<2);
	/*handler Z*/
	if(data[0]&0x40)
		reg[2] |= (1);
	else
		reg[2] &= ~(1);
}
static void tap_gesture_control(char *data, char *reg)
{
	if(data[0])
		reg[0] |= (1<<1);
	else
		reg[0] &= ~(1<<1);
}

static int fts_set_gesture_reg(struct fts_ts_info *info, char *mode)
{
	int i;
 	unsigned char reg[6] = {0xC1, 0x06};
	unsigned char regcmd[6] = {0xC2, 0x06, 0xFF, 0xFF, 0xFF, 0xFF};

	for(i = 0; i < 4; i++){
		reg[i+2] = *(mode + i);
	}

	fts_write_reg(info, regcmd, sizeof(regcmd));
	usleep_range(5000,6000);
	fts_write_reg(info, reg, sizeof(reg));
	usleep_range(5000,6000);

 	tp_log(" set gesture mode %d %d %d\n", *mode, *(mode+1), *(mode+2));
	return 0;
}

static void fts_set_gesture(struct fts_ts_info *info)
{
	//int *p = (int *)info->gesture_mask;
	int all = (info->gesture_mask[ALL_INDEX] & 0xC0) >> 6;	
	char reg_data[4] = { 0 };

	if(all == 2){/*enable some gesture*/
		swipe_gesture_control(&info->gesture_mask[SWIPE_INDEX],(reg_data));
		unicode_gesture_control(&info->gesture_mask[UNICODE_INDEX],reg_data);
		tap_gesture_control(&info->gesture_mask[TAP_INDEX], reg_data);
		reg_data[3] = 0x0;
	}else if(all == 1){/*enable all gesture*/
		reg_data[0] |= 0xFE; 
		reg_data[1] |= 0xA7; // 
		reg_data[2] |= 0x1;
		reg_data[3] = 0x0;
	}

	fts_set_gesture_reg(info, reg_data);

}

#if 0
static void fts_cover_on(struct fts_ts_info *info)
{
	int ret =0;
	unsigned char regAdd[2] = {0xC1, 0x05};//cover on
	
	tp_log("fts cover on set begin.\n");

	ret = fts_write_reg(info, regAdd, sizeof(regAdd)); 
	usleep_range(5000,6000);
	if(ret){
		tp_log("fts cover on set failed.\n");
	}
}

static void fts_cover_off(struct fts_ts_info *info)
{
	int ret =0;
	unsigned char regAdd[2] = {0xC2, 0x05};//cover on
	tp_log("fts cover off set begin.\n");

	ret = fts_write_reg(info, regAdd, sizeof(regAdd)); 
	usleep_range(5000,6000);
	if(ret){
		tp_log("fts cover off set failed.\n");
	}
}
#endif

static ssize_t fts_gesture_data_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	
	int count = snprintf(buf, PAGE_SIZE, "%u\n",info->gesture_value);
	tp_log("gesture %x detect \n",info->gesture_value);
	info->gesture_value = GESTURE_ERROR ;
	return count ;
}

/* force update firmware*/
/*
static ssize_t fts_fw_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	return scnprintf(buf, PAGE_SIZE, "%04x\n", info->fw_version);
}
*/
static ssize_t fts_fw_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret, mode;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &mode);

	info->fw_force = 1;
	ret = fts_fw_upgrade(info, mode,0);
	info->fw_force = 0;
	if (ret)
		dev_err(dev, "Unable to upgrade firmware\n");
	return count;
}


static ssize_t fts_gesture_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	//return scnprintf(buf, PAGE_SIZE, "%04x\n", info->gesture_bit);
	int *p = (int *)info->gesture_mask;
	memcpy(buf, p, 4);
	tp_log("gesture mask %x %p\n", *buf, buf);
	return 4;

}

static ssize_t fts_gesture_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	
#if 1
	const char * data = buf ;
	tp_log("turn on/off [%x][%x][%x][%x] gesture control\n",
			  *(data+3),*(data+2),*(data+1),*(data));

#else
	int data[4];
	sscanf(buf, "%d %d %d %d", data+3, data+2, data+1, data);
		 tp_log("%s: turn on/off [%x][%x][%x][%x] gesture control\n",
		 __func__, *(data+3),*(data+2),*(data+1),*(data));

#endif

	if(data[2] == ALL_CTR) {
		info->gesture_disall = !data[0];
	}else if(data[2]==SWIPE_CTR){
		info->gesture_mask[SWIPE_INDEX] = 0x0F&data[0] ;
		info->gesture_mask[ALL_INDEX]   = 2<<6 ;
	}else if(data[2]==UNICODE_CTR){
		info->gesture_mask[UNICODE_INDEX] = 0xFF&data[0] ;
		info->gesture_mask[ALL_INDEX]	 = 2<<6 ;
	}else if(data[2]==TAP_CTR){
		info->gesture_mask[TAP_INDEX] = 0x01&data[0] ;
		info->gesture_mask[ALL_INDEX] = 2<<6 ;
	}else {
		tp_log("parse gesture type error\n");
		return -EIO ;
	}

	temp= ((info->gesture_mask[SWIPE_INDEX]==0x0F)&&
			(info->gesture_mask[UNICODE_INDEX]==0xFF)&&
			(info->gesture_mask[TAP_INDEX]==0x01));
	info->gesture_mask[ALL_INDEX] = (temp?1:2)<<6 ;

	return count;
}

static ssize_t fts_margin_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	int count;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[4] = { 0xB2, 0x03, 0x25, 0x04};
	unsigned char regAdd1 = READ_ONE_EVENT;
	int *p = (int *)data;
	int error;

	fts_interrupt_set(info, INT_DISABLE);
	msleep(10);
	fts_write_reg(info, regAdd, sizeof(regAdd));
	msleep(30);
	error = fts_read_reg(info, &regAdd1,sizeof(regAdd1), data, FTS_EVENT_SIZE);
	if (error) {
		tp_log("Cannot read device info\n");
		return -ENODEV;
	}
	fts_interrupt_set(info, INT_ENABLE);
	msleep(10);
	memcpy(buf,(data+3),4);
	//count = snprintf(buf, 256, "%d:%d:%d:%d\n",data[3],data[4],data[5],data[6]);
	tp_log("%s: left:[%d], right:[%d], top:[%d], bottom:[%d] margin data:[0x%x], return buffer data:[0x%x]\n",__func__,
		data[3], data[4], data[5],data[6],*p, *(int *)buf);
	return count ;

}

static ssize_t fts_margin_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	unsigned char regAdd[4] = {0xB0, 0x03, 0x00, 0x00};
#if 1
	const char * data1 = buf ;
	unsigned char data[2];
	data[0] = (unsigned char)data1[0];
	data[1] = (unsigned char)data1[1];
	printk("[%s]:margin control write [%x][%x]\n",
				  __func__,*(data+1),*(data)); 
#else
	int data1[2];
	unsigned char data[2];
	sscanf(buf, "%d %d",data1+1, data1);
	data[0] = (unsigned char)data1[0];
	data[1] = (unsigned char)data1[1];
	printk("[%s]:margin control write [%x][%x] \n",
				  __func__,*(data+1),*(data));

#endif
	//regAdd[0] = 0xB0;
	//regAdd[1] = 0x03
	regAdd[3] = data[0];
	switch(data[1]){
		case MAR_LEFT:
			regAdd[2] = 0x25;
			tp_log("set left margin addr to %d\n", data[0]);
			break;
		case MAR_RIGHT:
			regAdd[2] = 0x26;
			tp_log("set right margin addr to %d\n", data[0]);
			break;
		case MAR_TOP:
			regAdd[2] = 0x27;
			tp_log("set top margin addr to %d\n", data[0]);
			break;
		case MAR_BOTTOM:
			regAdd[2] = 0x28;
			tp_log("set bottom margin addr to %d\n", data[0]);
			break;
		default:
			tp_log("write margin mode data[ %x ] is invalid\n", data[0]);
			break;
	}
	temp = fts_write_reg(info,regAdd,sizeof(regAdd));
	if(temp)
		tp_log("%s: set margin contorl is failed!\n",__func__);

	return count;
}

static ssize_t fts_margin_control_test_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	int count;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[4] = { 0xB2, 0x03, 0x25, 0x04};
	unsigned char regAdd1 = READ_ONE_EVENT;
	int *p = (int *)data;
	int error;

	fts_interrupt_set(info, INT_DISABLE);
	msleep(10);
	fts_write_reg(info, regAdd, sizeof(regAdd));
	msleep(30);
	error = fts_read_reg(info, &regAdd1,sizeof(regAdd1), data, FTS_EVENT_SIZE);
	if (error) {
		tp_log("Cannot read device info\n");
		return -ENODEV;
	}
	fts_interrupt_set(info, INT_ENABLE);
	msleep(10);
	//memcpy(buf,(data+3),4);
	count = snprintf(buf, 256, "%d:%d:%d:%d\n",data[3],data[4],data[5],data[6]);
	tp_log("%s: left:[%d], right:[%d], top:[%d], bottom:[%d] margin data:[0x%x]\n",__func__,
		data[3], data[4], data[5],data[6],*p);
	return count ;

}


static ssize_t fts_margin_control_test_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	unsigned char regAdd[4] = {0xB0, 0x03, 0x00, 0x00};
#if 0
	const char * data1 = buf ;
	unsigned char data[2];
	data[0] = (unsigned char)data1[0];
	data[1] = (unsigned char)data1[1];
	printk("[%s]:margin control write [%x][%x]\n",
				  __func__,*(data+1),*(data)); 
#else
	int data1[2];
	unsigned char data[2];
	sscanf(buf, "%d %d",data1+1, data1);
	data[0] = (unsigned char)data1[0];
	data[1] = (unsigned char)data1[1];
	printk("[%s]:margin control write [%x][%x] \n",
				  __func__,*(data+1),*(data));

#endif
	//regAdd[0] = 0xB0;
	//regAdd[1] = 0x03
	regAdd[3] = data[0];
	switch(data[1]){
		case MAR_LEFT:
			regAdd[2] = 0x25;
			tp_log("set left margin addr to %d\n", data[0]);
			break;
		case MAR_RIGHT:
			regAdd[2] = 0x26;
			tp_log("set right margin addr to %d\n", data[0]);
			break;
		case MAR_TOP:
			regAdd[2] = 0x27;
			tp_log("set top margin addr to %d\n", data[0]);
			break;
		case MAR_BOTTOM:
			regAdd[2] = 0x28;
			tp_log("set bottom margin addr to %d\n", data[0]);
			break;
		default:
			tp_log("write margin mode data[ %x ] is invalid\n", data[0]);
			break;
	}
	temp = fts_write_reg(info,regAdd,sizeof(regAdd));
	if(temp)
		tp_log("%s: set margin contorl is failed!\n",__func__);

	return count;
}

static ssize_t fts_glove_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	return scnprintf(buf, PAGE_SIZE, "%04x\n", info->glove_bit);
}

static ssize_t fts_glove_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &info->glove_bit);
	tp_log("%s: glove mode: %d\n",__func__, info->glove_bit);
	if(info->glove_bit) {
		if(info->hover_bit){
			info->hover_bit = 0;
			fts_command(info, HOVER_OFF);
		}
		ret = fts_command(info, GLOVE_ON);
		fts_set_sensor_mode(info, MODE_GLOVE);
	}else {
		ret = fts_command(info, GLOVE_OFF);
		fts_set_sensor_mode(info, MODE_NORMAL);
	}
	if (ret)
		tp_log("%s: glove mode: %d : failed !\n",__func__, info->glove_bit);
	return count;
}

static ssize_t fts_hover_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	return scnprintf(buf, PAGE_SIZE, "%04x\n", info->hover_bit);
}

static ssize_t fts_hover_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &info->hover_bit);
	tp_log("%s: hover mode: %d\n",__func__, info->hover_bit);
	if(info->hover_bit) {
		if(info->glove_bit){
			info->glove_bit = 0;
			fts_command(info, GLOVE_OFF);
		}
		ret = fts_command(info, HOVER_ON);
		fts_set_sensor_mode(info, MODE_HOVER);
	}else {
		ret = fts_command(info, HOVER_OFF);
		fts_set_sensor_mode(info, MODE_NORMAL);
	}
	if (ret)
		tp_log("%s: hover mode: %d : failed !\n",__func__, info->hover_bit);
	return count;
}

static ssize_t fts_cover_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	return scnprintf(buf, 16, "%04x\n", info->cover_bit);
}

static ssize_t fts_cover_control_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* reading out firmware upgrade mode */
	sscanf(buf, "%d", &info->cover_bit);
	tp_log("%s: cover mode: %d",__func__, info->cover_bit);
	
	if(info->cover_bit) {
		fts_set_sensor_mode(info, MODE_COVER);
	}else {
		fts_set_sensor_mode(info, MODE_NORMAL);
	}
	return count;
}
#if 0
static int fts_cover_notity_func(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct fts_ts_info * info =
				container_of(nb, struct fts_ts_info,cover_notifier);

	tp_log("hall_notify action[%ld]mode[%d]\n",action,info->mode);

	if(HALL_COVER==action){

		if(info->resume_bit){
			fts_command(info, GLOVE_OFF);
			fts_command(info, HOVER_OFF);
			fts_cover_on(info);
			mdelay(5);
			fts_command(info, FORCECALIBRATION);
		}
		
		fts_set_sensor_mode(info, MODE_COVER);
	}else {	
		 if(info->resume_bit){
 			   fts_cover_off(info);
 			   mdelay(5);
 			   fts_command(info, FORCECALIBRATION);
 			}
 			fts_set_sensor_mode(info, MODE_NORMAL);
 
 			if(info->glove_bit){
 				if(info->resume_bit)
 				fts_command(info, GLOVE_ON);
 				fts_set_sensor_mode(info, MODE_GLOVE);
 			}
 
 			if(info->hover_bit){
 				if(info->resume_bit)
 				fts_command(info, HOVER_ON);
 				fts_set_sensor_mode(info, MODE_HOVER);
 			}
	}
	
   return 0 ;
}

#else

static int fts_cover_notity_func(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct fts_ts_info * info =
				container_of(nb, struct fts_ts_info,cover_notifier);

	tp_log("hall_notify action[%ld]mode[%d]\n",action,info->mode);

	if(HALL_COVER==action){
		fts_set_sensor_mode(info, MODE_COVER);
	}else {	
 		fts_set_sensor_mode(info, MODE_NORMAL);
	}

   return 0 ;
}

#endif
static void fts_register_cover_notify(struct fts_ts_info *info)
{
	info->cover_notifier.notifier_call = fts_cover_notity_func ;
	hall_register_notifier_client(&(info->cover_notifier));
}

static ssize_t fts_sysfs_config_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int error;		

	error = fts_get_fw_version(info);
	if(error) {
		tp_log("%s: can not get fw version!\n", __func__);
	}

	error = snprintf(buf, PAGE_SIZE, "%s:%x:%s:%x,,\n","86FTS",info->config_id,"FTM3AD54",info->fw_version);
	return error;
}
static ssize_t fts_sysfs_fwupdate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	//fwupdate_stat: 2:  sucess,;  1: failed; 0: No firmware update.
	return sprintf(buf, "%d\n", info->fwupdate_stat);
}

static unsigned int le_to_uint(const unsigned char *ptr)
{
	if (NULL == ptr) {
		return 0;
	}
	return (unsigned int)ptr[0] + (unsigned int)ptr[1] * 0x100;
}
static unsigned int be_to_uint(const unsigned char *ptr)
{
	if (NULL == ptr) {
		return 0;
	}
	return (unsigned int)ptr[1] + (unsigned int)ptr[0] * 0x100;
}
static ssize_t fts_fw_test_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	const struct firmware *fw = NULL;
	unsigned char *data;
	unsigned int size;
	char *firmware_name = "st_fts.bin";
	int fw_version;
	int config_version;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int retval;
	
	retval = request_firmware(&fw,firmware_name,info->dev);
	if(retval){
		tp_log("%s: request_firmware failed!\n",__func__);
	}
	
	data =(unsigned char *)fw->data;
	size = fw->size;

	fw_version = le_to_uint(&data[4]);	
//	config_version = be_to_uint(&data[13]);
	config_version = be_to_uint(&data[63521]);  // read correct address to get config ID in file

	tp_log("%s: fw_version = %x, config_version = %x, size = %d\n", __func__, fw_version, config_version, size);

	return 0;
}

static ssize_t fts_irq_gpio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	return sprintf(buf, "%d\n", gpio_get_value(info->bdata->irq_gpio));
}

static ssize_t fts_touch_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	sscanf(buf,"%d", &info->touch_debug);
	return count;
}

/* *******************************************Production test****************************** */
static ssize_t fts_autotune_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char event_id = 0;
	unsigned char tune_flag = 0;
	unsigned int  retry = 0;
	unsigned char error ;
	char mutual_check_error = 2;
	char self_check_error = 2;
	unsigned char regAdd =0;
	unsigned char Tuneretry =0;

	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	fts_systemreset(info);
	msleep(200);
	fts_command(info, SLEEPOUT);
	msleep(10);
	fts_interrupt_set(info, INT_DISABLE);
	msleep(10);
	fts_command(info, FLUSHBUFFER);
	msleep(5);
		
	for(Tuneretry = 0; Tuneretry < 2; Tuneretry++){	
		fts_command(info, CX_TUNING);
		msleep(800);

		for (retry = 0; retry < 220; retry++) {
			regAdd = READ_ONE_EVENT;
			error = fts_read_reg(info, &regAdd,sizeof(regAdd), data, FTS_EVENT_SIZE);
			if (error) {
				dev_err(info->dev, "Cannot read device info\n");
				return -ENODEV;
			}
			tp_log("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);

			event_id = data[0];
			tune_flag = data[1];

			if ((event_id == 0x16) && (tune_flag == 0x01)) {
				if((data[3] == 0x00) && (data[4] == 0x00)){
					mutual_check_error = 0;
					tp_log("fts initialization check mutual ok \n");
				}else{
					mutual_check_error = 1;
					tp_log("fts initialization check mutual fail \n");
				}
				break;
			}else{
				msleep(10);
			}
		}
		if(mutual_check_error == 0)
			break;
		printk("fts initialization check mutual fail,retry \n");
	}

	for(Tuneretry = 0; Tuneretry < 2; Tuneretry++){
		fts_command(info, SELF_TUNING);
		msleep(300);
	
		for (retry = 0; retry < 270; retry++) {
			regAdd = READ_ONE_EVENT;
			error = fts_read_reg(info, &regAdd,sizeof(regAdd), data, FTS_EVENT_SIZE);
			if (error) {
				dev_err(info->dev, "Cannot read device info\n");
				return -ENODEV;
			}
			tp_log("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

			event_id = data[0];
			tune_flag = data[1];

			if ((event_id == 0x16) && (tune_flag == 0x02)) {
				if((data[3] == 0x00) && (data[4] == 0x00)){
					self_check_error = 0;
					tp_log("fts initialization check self ok \n");
				}else{
					self_check_error = 1;
					tp_log("fts initialization check self fail \n");
				}
				break;
			}else{
				msleep(10);
			}
		}
		if(self_check_error == 0)break;
		printk("fts initialization check self fail,retry \n");			
	}		

	tp_log("fts restart TP \n");
	fts_systemreset(info);
	msleep(200);
	fts_command(info, SLEEPOUT);
	usleep_range(10000, 10000);
	fts_interrupt_set(info, INT_ENABLE);
	usleep_range(10000, 10000);
	fts_command(info, SENSEON);
	fts_command(info, FORCECALIBRATION);

	return sprintf(buf, "%d\n", (self_check_error+mutual_check_error));

}

static ssize_t fts_ito_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char event_id = 0;
	unsigned char tune_flag = 0;
	unsigned char retry = 0;
	unsigned char i = 0;
	unsigned char error ;
	unsigned char regAdd = 0;
	unsigned int ito_check_status[11]={0,0,0,0,0,0,0,0,0,0,0};

	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);

	fts_systemreset(info);
	msleep(200);
	fts_command(info, SLEEPOUT);
	usleep_range(10000, 10000);
	fts_interrupt_set(info, INT_DISABLE);
	usleep_range(10000, 10000);
	fts_command(info, FLUSHBUFFER);
	usleep_range(5000, 5000);
	fts_command(info, ITO_CHECK);
	msleep(200);
	tp_log("fts ITO Check Start \n");

	for (retry = 0; retry < 40; retry++) {
		regAdd = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd,sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -ENODEV;
		}
		tp_log("FTS ITO event : %02X %02X %02X %02X %02X %02X %02X %02X\n",
			data[0], data[1], data[2], data[3],data[4], data[5], data[6], data[7]);

		event_id = data[0];
		tune_flag = data[1];

		if ((event_id == 0x0F) && (tune_flag == 0x05)) {
			if((data[2] == 0x00) && (data[3] == 0x00)){
				ito_check_status[0] = 0;
				tp_log("fts ITO check ok \n");
				break;
			}else{
				ito_check_status[0] = 1;
				tp_log("fts ITO check fail \n");
				for(i = 0;i<10;i++){
					if(ito_check_status[i+1] == 0){
						ito_check_status[i+1] = (data[2] << 8) | data[3];
						break;
					}
				}
			}
		}else{
			usleep_range(5000, 5000);
		}
	}

	memcpy(buf,ito_check_status,sizeof(ito_check_status));
	tp_log("the ito test data is: ");
	for(i = 0; i < 11; i++){
		printk("%d ",*((int*)(buf+i*4)));
	}
	tp_log("fts restart TP ++++++++++++ito_check_status = %d\n", ito_check_status[0]);
	fts_systemreset(info);
	msleep(200);
	fts_command(info, SLEEPOUT);
	usleep_range(10000, 10000);
	fts_interrupt_set(info, INT_ENABLE);
	usleep_range(10000, 10000);
	fts_command(info, SENSEON);
	fts_command(info, FORCECALIBRATION);

	return 44;
}

static DEVICE_ATTR(fwupdate, (S_IRUGO|S_IWUSR|S_IWGRP), fts_sysfs_fwupdate_show, fts_fw_control_store);
static DEVICE_ATTR(gesture_control, (S_IRUGO|S_IWUSR|S_IWGRP), fts_gesture_control_show, fts_gesture_control_store);
static DEVICE_ATTR(margin_control, (S_IRUGO|S_IWUSR|S_IWGRP), fts_margin_control_show, fts_margin_control_store);
static DEVICE_ATTR(margin_control_test, (S_IRUGO|S_IWUSR|S_IWGRP), fts_margin_control_test_show, fts_margin_control_test_store);
static DEVICE_ATTR(gesture_data, S_IRUGO, fts_gesture_data_read, NULL);
static DEVICE_ATTR(glove_control, (S_IRUGO|S_IWUSR|S_IWGRP), fts_glove_control_show, fts_glove_control_store);
static DEVICE_ATTR(appid, (S_IRUGO), fts_sysfs_config_id_show, NULL);
//static DEVICE_ATTR(fwupdate, (S_IRUGO), fts_sysfs_fwupdate_show, NULL);
static DEVICE_ATTR(update_test, (S_IRUGO),fts_fw_test_show, NULL);
static DEVICE_ATTR(cover_control, (S_IRUGO|S_IWUSR|S_IWGRP),fts_cover_control_show, fts_cover_control_store);
static DEVICE_ATTR(hover_control, (S_IRUGO|S_IWUSR|S_IWGRP), fts_hover_control_show, fts_hover_control_store);
static DEVICE_ATTR(irq_gpio,(S_IRUGO|S_IWUSR|S_IWGRP), fts_irq_gpio_show, NULL);
/** factory test */
static DEVICE_ATTR(autotune_test,(S_IRUGO), fts_autotune_test_show, NULL);
static DEVICE_ATTR(ito_test,(S_IRUGO), fts_ito_test_show, NULL);
static DEVICE_ATTR(touch_debug,(S_IWUSR|S_IWGRP), NULL, fts_touch_debug_store);

static struct attribute *fts_attr_group[] = {
	&dev_attr_fwupdate.attr,
	&dev_attr_gesture_control.attr,
	&dev_attr_margin_control.attr,
	&dev_attr_margin_control_test.attr,
	&dev_attr_gesture_data.attr,
	&dev_attr_glove_control.attr,
	&dev_attr_appid.attr,
	&dev_attr_update_test.attr,
	&dev_attr_cover_control.attr,
	&dev_attr_hover_control.attr,
	&dev_attr_irq_gpio.attr,
	&dev_attr_autotune_test.attr,
	&dev_attr_ito_test.attr,
	&dev_attr_touch_debug.attr,
	NULL,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void fts_early_suspend(struct early_suspend *h)
{
	struct fts_ts_info *info;
	struct device *dev;

	info = container_of(h, struct fts_ts_info, early_suspend);
	dev = &info->client->dev;
	dev_info(dev, "FTS Early Suspend entered\n");
	if (fts_suspend(info->client, PMSG_SUSPEND))
		dev_err(&info->client->dev, "Early suspend failed\n");
	dev_info(dev, "FTS Early Suspended\n");
}

static void fts_late_resume(struct early_suspend *h)
{
	struct fts_ts_info *info;
	struct device *dev;

	info = container_of(h, struct fts_ts_info, early_suspend);
	dev = &info->client->dev;
	dev_info(dev, "FTS Early Resume entered\n");
	if (fts_resume(info->client))
		dev_err(&info->client->dev, "Late resume failed\n");
	dev_info(dev, "FTS Early Resumed\n");
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd;
	int ret;

	regAdd = cmd;
	ret = fts_write_reg(info, &regAdd, sizeof(regAdd));
	tp_log( " fts:Issued command 0x%02x, return value %d\n", cmd, ret);
	return ret;
}


static int fts_systemreset(struct fts_ts_info *info)
{
	int ret;
	unsigned char regAdd[4] = { 0xB6, 0x00, 0x23, 0x01 };

	dev_dbg(info->dev, "Doing a system reset\n");

	ret = fts_write_reg(info, regAdd, sizeof(regAdd));

	usleep_range(5000, 6000);

	return ret;
}

#if 0
static int fts_get_mode(struct fts_ts_info *info)
{
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[4] = { 0xB2, 0x00, 0x02, 0x01 };

	fts_write_reg(info, regAdd, sizeof(regAdd));

	msleep(20);

	regAdd[0] = READ_ONE_EVENT;
	info->mode = fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE) ?
			MODE_NORMAL : data[3];

	return 0;
}
#endif

static int fts_get_fw_version(struct fts_ts_info *info)
{
	unsigned char val[8];
	unsigned char regAdd[3] = {0xB6, 0x00, 0x07};
	unsigned char regAdd1[1];
	int error;
	
	error = fts_read_reg(info, regAdd, sizeof(regAdd), val, sizeof(val));
	/*check for chip id*/
	if ((val[1] != FTS_ID0) || (val[2] != FTS_ID1)) {
		dev_err(info->dev,
			"Wrong version id (read 0x%02x%02x, expected 0x%02x%02x)\n",
				val[1], val[2], FTS_ID0, FTS_ID1);
		return -ENODEV;
	}else {
		info->fw_version = (val[5] << 8) | val[4];
	}

	if(info->fw_version > 0){
		regAdd1[0] = 0x80;
		error = fts_read_reg(info, regAdd1, sizeof(regAdd1), val, sizeof(val));
		if (error) {
			dev_err(info->dev, "Cannot read config id\n");
			return -ENODEV;
		}else{
			info->config_id = (val[4] << 8) | val[5];
		}

	}else{
		info->config_id = 0;
	}

	return 0;
}

static int fts_flash_status(struct fts_ts_info *info,
				unsigned int timeout, unsigned int steps)
{
	int ret, status;
	unsigned char data;
	unsigned char regAdd[2];

	do {
		regAdd[0] = FLASH_READ_STATUS;
		regAdd[1] = 0;
		
		usleep_range(20000, 20000);

		ret = fts_read_reg(info, regAdd, sizeof(regAdd), &data, sizeof(data));
		if (ret)
			status = FLASH_STATUS_UNKNOWN;
		else
			status = (data & 0x01) ? FLASH_STATUS_BUSY : FLASH_STATUS_READY;

		if (status == FLASH_STATUS_BUSY) {
			timeout -= steps;
			msleep(steps);
		}

	} while ((status == FLASH_STATUS_BUSY) && (timeout));

	return status;
}


static int fts_flash_unlock(struct fts_ts_info *info)
{
	int ret;
	unsigned char regAdd[4] = { FLASH_UNLOCK,
				FLASH_UNLOCK_CODE_0,
				FLASH_UNLOCK_CODE_1,
				0x00 };

	ret = fts_write_reg(info, regAdd, sizeof(regAdd));

	if (ret)
		dev_err(info->dev, "Cannot unlock flash\n");
	else
		dev_dbg(info->dev, "Flash unlocked\n");

	return ret;
}

static int fts_flash_load(struct fts_ts_info *info,
			int cmd, int address, const char *data, int size)
{
	int ret;
	unsigned char *cmd_buf;
	unsigned int loaded;

	cmd_buf = kmalloc(FLASH_LOAD_COMMAND_SIZE, GFP_KERNEL);
	if (cmd_buf == NULL) {
		dev_err(info->dev, "Out of memory when programming flash\n");
		return -ENOMEM;
	}

	loaded = 0;
	while (loaded < size) {
		cmd_buf[0] = cmd;
		cmd_buf[1] = (address >> 8) & 0xFF;
		cmd_buf[2] = (address) & 0xFF;

		memcpy(&cmd_buf[3], data, FLASH_LOAD_CHUNK_SIZE);
		ret = fts_write_reg(info, cmd_buf, FLASH_LOAD_COMMAND_SIZE);
		if (ret) {
			dev_err(info->dev, "Cannot load firmware in RAM\n");
			break;
		}

		data += FLASH_LOAD_CHUNK_SIZE;
		loaded += FLASH_LOAD_CHUNK_SIZE;
		address += FLASH_LOAD_CHUNK_SIZE;
	}

	kfree(cmd_buf);

	return (loaded == size) ? 0 : -1;
}


static int fts_flash_erase(struct fts_ts_info *info, int cmd)
{
	int ret;
	unsigned char regAdd = cmd;

	ret = fts_write_reg(info, &regAdd, sizeof(regAdd));

	if (ret)
		dev_err(info->dev, "Cannot erase flash\n");
	else
		dev_dbg(info->dev, "Flash erased\n");

	return ret;
}


static int fts_flash_program(struct fts_ts_info *info, int cmd)
{
	int ret;
	unsigned char regAdd = cmd;

	ret = fts_write_reg(info, &regAdd, sizeof(regAdd));

	if (ret)
		dev_err(info->dev, "Cannot program flash\n");
	else
		dev_dbg(info->dev, "Flash programmed\n");

	return ret;
}


static int fts_fw_upgrade(struct fts_ts_info *info, int mode,int fw_forceupdate)
{
	int ret;
	const struct firmware *fw = NULL;
	unsigned char *data;
	unsigned int size;
	int updata_loop = 0;
	int status, fw_ver = 0, config_ver = 0;
	int program_command, erase_command, load_command, load_address = 0;

	tp_log(" TouchScreen firmware upgrade...\n");

	info->fwupdate_stat = 1;
	if(!info->fw_force){
		ret = request_firmware(&fw, fts_fw_filename[0], info->dev);
	}else{
		ret = request_firmware(&fw, fts_fw_filename[1], info->dev);
	}
	if (ret) {
		tp_log("Unable to open firmware file '%s'\n",fts_fw_filename[0]);
		return ret;
	}

	if ((fw->size != 65568) /*|| (fw->size != fts_fw_size[mode])*/) {
		tp_log("Wrong firmware file for the size is smaller than 65568\n");
		goto fw_done;
	}
	tp_log("tp firmware data address: data[0] = %p, data[63521] = %p\n,", &fw->data[0], &fw->data[63521]);

	data = (unsigned char *)fw->data;
	size = fw->size;
	fw_ver = le_to_uint(&data[4]);	
	config_ver = be_to_uint(&data[63521]);

	if(!info->fw_force){
		//tp_log("%s: fw update probe begin!\n", __func__);
		ret = fts_get_fw_version(info);
		if(ret) {
			tp_log("%s: can not get fw version!\n", __func__);
		}
		
		tp_log("%s: tp:fw_version = %x, config_id = %x. bin: fw_ver = %x, config_ver = %x\n", __func__,
			info->fw_version, info->config_id, fw_ver, config_ver);
		
		if(fw_ver > info->fw_version || config_ver > info->config_id || fw_forceupdate == 1)	
		{
			mode = 2;
			tp_log("%s: mode = %d",__func__, mode);
		}else{
			info->fwupdate_stat = 0;
			tp_log("%s: no need to update",__func__);
			return 0;
		}
	}

fts_updat:
	dev_dbg(info->dev, "Flash programming...\n");
	ret = fts_systemreset(info);
	if (ret) {
		dev_warn(info->dev, "Cannot reset the device 00\n");
		goto fw_done;
	}
	msleep(150);

	switch (mode) {
	case MODE_CONFIG_ONLY:
		program_command = FLASH_PROGRAM;
		erase_command = FLASH_ERASE;
		load_command = FLASH_LOAD_FIRMWARE;
		load_address = FLASH_LOAD_INFO_BLOCK_OFFSET;
		break;
	case MODE_RELEASE_AND_CONFIG_64:
		/* skip 32 bytes header */
		data += 32;
		size = size - 32;
		/* fall throug */
	case MODE_RELEASE_ONLY:
		program_command = FLASH_PROGRAM;
		erase_command = FLASH_ERASE;
		load_command = FLASH_LOAD_FIRMWARE;
		load_address = FLASH_LOAD_FIRMWARE_OFFSET;
		break;
	default:
		/* should never be here, already checked mode value before */
		break;
	}

	dev_info(info->dev, "1) checking for status.\n");
	status = fts_flash_status(info, 1000, 100);
	if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
		dev_err(info->dev, "Wrong flash status\n");
		goto fw_done;
	}

	dev_info(info->dev, "2) unlock the flash.\n");
	ret = fts_flash_unlock(info);
	if (ret) {
		dev_err(info->dev, "Cannot unlock the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	msleep(FTS_FLASH_COMMAND_DELAY);

	dev_info(info->dev, "3) load the program.\n");
	ret = fts_flash_load(info, load_command, load_address, data, size);
	if (ret) {
		dev_err(info->dev,
			"Cannot load program to for the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	msleep(FTS_FLASH_COMMAND_DELAY);

	dev_info(info->dev, "4) erase the flash.\n");
	ret = fts_flash_erase(info, erase_command);
	if (ret) {
		dev_err(info->dev, "Cannot erase the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	msleep(FTS_FLASH_COMMAND_DELAY);

	dev_info(info->dev, "5) checking for status.\n");
	status = fts_flash_status(info, 1000, 100);
	if ((status == FLASH_STATUS_UNKNOWN) || (status == FLASH_STATUS_BUSY)) {
		dev_err(info->dev, "Wrong flash status\n");
		goto fw_done;
	}

	/* wait for a while */
	msleep(FTS_FLASH_COMMAND_DELAY);

	dev_info(info->dev, "6) program the flash.\n");
	ret = fts_flash_program(info, program_command);
	if (ret) {
		dev_err(info->dev, "Cannot program the flash device\n");
		goto fw_done;
	}

	/* wait for a while */
	msleep(FTS_FLASH_COMMAND_DELAY);

	dev_info(info->dev, "Flash programming: done and then perform a system reset\n");
	ret = fts_systemreset(info);
	if (ret) {
		dev_warn(info->dev, "Cannot reset the device\n");
		goto fw_done;
	}

	ret = fts_get_fw_version(info);
	if (ret) {
		dev_warn(info->dev, "Cannot retrieve firmware version\n");
		goto fw_done;
	}

	tp_log("%s: tp:fw_version = %x, config_id = %x. bin: fw_ver = %x, config_ver = %x\n", __func__, 
			info->fw_version, info->config_id, fw_ver, config_ver);
	if(fw_ver == info->fw_version && config_ver == info->config_id)	
	{
		info->fwupdate_stat = 2;
		tp_log("%s: firmware update OK!", __func__);
	}else{
		if (updata_loop < 3){
			updata_loop++;
			tp_log("%s: firmware updata failed, update again %d********************************************************\n", __func__, updata_loop);
			goto fts_updat;
		}
		tp_log("%s: firmware update failed!", __func__);
	}

	dev_info(info->dev,"New firmware version 0x%04x installed\n",info->fw_version);

fw_done:
	release_firmware(fw);

	return ret;
}


static void fts_interrupt_set(struct fts_ts_info *info, int enable)
{
	unsigned char regAdd[4] = { 0xB6, 0x00, 0x1C, enable };

	fts_write_reg(info, &regAdd[0], 4);
}


/*
 * New Interrupt handle implementation
 */
static inline unsigned char *fts_next_event(unsigned char *evt)
{
	/* Nothing to do with this event, moving to the next one */
	evt += FTS_EVENT_SIZE;

	/* the previous one was the last event ?  */
	return (evt[-1] & 0x1F) ? evt : NULL;
}


/* EventId : 0x00 */
static unsigned char *fts_nop_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	// GIUSEPPE dev_dbg(info->dev, "Doing nothing for event 0x%02x\n", *event);
	return fts_next_event(event);
}


/* EventId : 0x03 */
static unsigned char *fts_enter_pointer_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	unsigned char touchId,touchcount;
	int x, y, z;

	if(!info->resume_bit)
		goto no_report;

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;

	__set_bit(touchId, &info->touch_id);

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
	z = (event[5] & 0x3F);

	if (x == X_AXIS_MAX)
		x--;

	if (y == Y_AXIS_MAX)
		y--;

#if 0
	/* for LCD */
	x = 1080 - x;
	y = 1920 - y;
	////////////
#endif
	//input_mt_slot(info->input_dev, finger);

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
	// Record Touch Point Coordinates
	if(exynos_touch_daemon_data.record == 1) {
		if(exynos_touch_daemon_data.tp.count < TOUCHPOINT) {
			exynos_touch_daemon_data.tp.x[exynos_touch_daemon_data.tp.count] = x;
			exynos_touch_daemon_data.tp.y[exynos_touch_daemon_data.tp.count] = y;
			exynos_touch_daemon_data.tp.wx[exynos_touch_daemon_data.tp.count] = z;
			exynos_touch_daemon_data.tp.wy[exynos_touch_daemon_data.tp.count] = z;
			exynos_touch_daemon_data.tp.count++;
		} else {
			printk("%s: Recordable touch point exceeds %d\n", __func__, TOUCHPOINT);
			exynos_touch_daemon_data.record = 0;
		}
	}
#endif
	
	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev,MT_TOOL_FINGER, 1);

#ifdef CONFIG_JANUARY_BOOSTER
	if(touchcount >= 1) {
		janeps_input_report(PRESS, x, y);
	}
#endif

	if(touchcount == 1){
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
	}

	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);

no_report:
	return fts_next_event(event);
}


/* EventId : 0x04 */
static unsigned char *fts_leave_pointer_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	unsigned char touchId, touchcount;
#ifdef CONFIG_JANUARY_BOOSTER
	int x, y;

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);

	if (x == X_AXIS_MAX)
		x--;
	if (y == Y_AXIS_MAX)
		y--;

#if 0
	/* for LCD */
	x = 1080 - x;
	y = 1920 - y;
#endif
#endif

	touchId = event[1] & 0x0F;
	touchcount = (event[1] & 0xF0) >> 4;

	__clear_bit(touchId, &info->touch_id);

	input_mt_slot(info->input_dev, touchId);
	input_mt_report_slot_state(info->input_dev,MT_TOOL_FINGER, 0);
	if (touchcount == 0){
		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
		if (exynos_touch_daemon_data.record == 1) {
			printk("%s Touch point recording is completed ad %d points\n", __func__, exynos_touch_daemon_data.tp.count);
			exynos_touch_daemon_data.record = 0;
		}
#endif
#ifdef CONFIG_JANUARY_BOOSTER
		janeps_input_report(RELEASE, x, y);
#endif
	}
	//input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);

	return fts_next_event(event);
}

/* EventId : 0x05 */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler


/* EventId : 0x07 */
static unsigned char *fts_hover_enter_pointer_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;
	int x, y, z;


	touchId = event[1] & 0x0F;

	__set_bit(touchId, &info->touch_id);

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
#define HOVER_ENTER_Z_VALUE 0
	z = HOVER_ENTER_Z_VALUE;

	if (x == X_AXIS_MAX)
		x--;

	if (y == Y_AXIS_MAX)
		y--;

	input_mt_slot(info->input_dev, touchId);
	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId);
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	//input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);


	return fts_next_event(event);
}


/* EventId : 0x08 */
#define fts_hover_leave_pointer_event_handler fts_enter_pointer_event_handler


/* EventId : 0x09 */
#define fts_hover_motion_pointer_event_handler fts_leave_pointer_event_handler


/* EventId : 0x0B */
static unsigned char *fts_proximity_enter_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;
	int x, y, z;

	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	touchId = event[1] & 0x0F;

	__set_bit(touchId, &info->touch_id);

	x = X_AXIS_MAX / 2;
	y = Y_AXIS_MAX / 2;
#define PROXIMITY_ENTER_Z_VALUE 0
	z = PROXIMITY_ENTER_Z_VALUE;

	input_mt_slot(info->input_dev, touchId);
	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId);
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	//input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);


	return fts_next_event(event);
}


/* EventId : 0x0C */
#define fts_proximity_leave_event_handler fts_leave_pointer_event_handler


/* EventId : 0x0E */
static unsigned char *fts_button_status_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	int i;
	unsigned int buttons, changed;

	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	/* get current buttons status */
	buttons = event[1] | (event[2] << 8);

	/* check what is changed */
	changed = buttons ^ info->buttons;

	for (i = 0; i < 16; i++)
		if (changed & (1 << i))
			input_report_key(info->input_dev,
				BTN_0 + i,
				(!(info->buttons & (1 << i))));

	/* save current button status */
	info->buttons = buttons;

	dev_dbg(info->dev, "Event 0x%02x -  SS = 0x%02x, MS = 0x%02x\n",
				event[0], event[1], event[2]);

	return fts_next_event(event);
}


/* EventId : 0x0F */
static unsigned char *fts_error_event_handler(struct fts_ts_info *info,
			unsigned char *event)
{
	int error,i;
	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	tp_log("FTS fts error event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			event[0], event[1], event[2], event[3],
			event[4], event[5], event[6], event[7]);

	if(event[1] == 0x0a){
		if (info->bus_reg) {
			error = regulator_disable(info->bus_reg);
			if (error < 0) {
				dev_err(info->dev,"%s: Failed to enable bus pullup regulator\n",__func__);
			}
		}

		if (info->pwr_reg) {
			error = regulator_disable(info->pwr_reg);
			if (error < 0) {
				dev_err(info->dev,
					"%s: Failed to enable power regulator\n",__func__);
			}
		}
		msleep(300);
		if (info->pwr_reg)
			error = regulator_enable(info->pwr_reg);

		if (info->bus_reg)
			error = regulator_enable(info->bus_reg);
		msleep(300);

		gpio_set_value(info->bdata->reset_gpio, 0);
		usleep_range(10000, 11000);
		gpio_set_value(info->bdata->reset_gpio, 1);
		msleep(200);

		for (i = 0; i < TOUCH_ID_MAX; i++){//before reset clear all slot
				input_mt_slot(info->input_dev, i);
				input_mt_report_slot_state(info->input_dev,
				(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
		}
		input_sync(info->input_dev);

		error = fts_systemreset(info);
		msleep(150);
		error += fts_command(info, SLEEPOUT);
		usleep_range(10000,11000);
		error += fts_command(info, FORCECALIBRATION);
		error += fts_command(info, SENSEON);
		error += fts_command(info, FLUSHBUFFER);
		if (error) {
			tp_log("%s: Cannot reset the device----------\n", __func__);
		}
		if(event[6] >= 0x80)
			{ printk("ESD or Low battery at gesture mode recovery \n");
				fts_set_gesture(info);
				fts_command(info, ENTER_GESTURE_MODE);
				//enable_irq_wake(info->client->irq);
				fts_set_sensor_mode(info, MODE_GESTURE);
				info->gesture_enable = 1;				
			}			
	}

	return fts_next_event(event);
}


/* EventId : 0x10 */
static unsigned char *fts_controller_ready_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);
	info->touch_id = 0;
	info->buttons = 0;
	input_sync(info->input_dev);
	return fts_next_event(event);
}


/* EventId : 0x11 */
static unsigned char *fts_sleepout_controller_ready_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);
	complete(&info->cmd_done);
	return fts_next_event(event);
}


/* EventId : 0x16 */
static unsigned char *fts_status_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	switch (event[1]) {
	case FTS_STATUS_MUTUAL_TUNE:
	case FTS_STATUS_SELF_TUNE:
	case FTS_FORCE_CAL_SELF_MUTUAL:
		complete(&info->cmd_done);
		break;

	case FTS_FLASH_WRITE_CONFIG:
	case FTS_FLASH_WRITE_COMP_MEMORY:
	case FTS_FORCE_CAL_SELF:
	case FTS_WATER_MODE_ON:
	case FTS_WATER_MODE_OFF:
	default:
		dev_dbg(info->dev,
			"Received unhandled status event = 0x%02x\n", event[1]);
		break;
	}

	return fts_next_event(event);
}


/* EventId : 0x05 */
#define fts_motion_pointer_event_handler fts_enter_pointer_event_handler


/* EventId : 0x20 */
static unsigned char *fts_gesture_event_handler(
		struct fts_ts_info *info, unsigned char *event)

{

	unsigned char touchId;
	//static int x_off, y_off;

	//unsigned char gesture_direction;
	dev_dbg(info->dev, "%s Received event 0x%02x  event[1]  = %d\n", __func__,event[0], event[1] );

	/* always use touchId zero */
	touchId = 0;
	__set_bit(touchId, &info->touch_id);


	switch(event[1]){
	case 0x02:/*add 02-->O*/
		info->gesture_value = UNICODE_O;
		break;
	case 0x03:/*add 03-->C*/
		info->gesture_value = UNICODE_C;
		break;
	case 0x04:/*add 04-->M*/
		info->gesture_value = UNICODE_M;
		break;
	case 0x05:/*add 05-->W*/
		info->gesture_value = UNICODE_W;
		break;
	case 0x06:/*add 06-->E*/
		info->gesture_value = UNICODE_E;
		break;
	case 0x0a:/*add 0a-->UP*/
		info->gesture_value = SWIPE_Y_UP;
		break;
	case 0x09:/*add 09-->down*/
		info->gesture_value = SWIPE_Y_DOWN;
		break;
	case 0x07:/*add 07-->left*/
		info->gesture_value = SWIPE_X_RIGHT;
		break;
	case 0x08:/*add 08-->right*/
		info->gesture_value = SWIPE_X_LEFT;
		break;
	case 0x01:/*add 01-->double click*/
		tp_log("%s: case 0x01--> double click\n", __func__);
		info->gesture_value = DOUBLE_TAP;
		break;
	case 0x0D:/*add 06-->V*/
		info->gesture_value = UNICODE_V_DOWN;
		break;
	case 0x0F:/*add 06-->S*/
		info->gesture_value = UNICODE_S;
		break;
	case 0x10:/*add 06-->Z*/
		info->gesture_value = UNICODE_Z;
		break;

	default:
		//info->gesture_value = GESTURE_ERROR;
		return 0;
	}
	input_report_key(info->input_dev, KEY_GESTURE, 1);
	input_report_key(info->input_dev, KEY_GESTURE, 0);
	//input_sync(info->input_dev);

	/*
	* Done with gesture event, clear bit.
	*/
	__clear_bit(touchId, &info->touch_id);

	tp_log("fts gesture:Event 0x%02x Event1 = 0x%02x- ID[%d], \n",
		event[0], event[1],touchId);

	return fts_next_event(event);

}


/* EventId : 0x23 */
static unsigned char *fts_pen_enter_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;
	int x, y, z;
	int eraser, barrel;

	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	/* always use last position as touchId */
	touchId = TOUCH_ID_MAX;

	__set_bit(touchId, &info->touch_id);

	x = (event[2] << 4) | (event[4] & 0xF0) >> 4;
	y = (event[3] << 4) | (event[4] & 0x0F);
	z = (event[5] & 0xFF);

	eraser = (event[1] * 0x80) >> 7;
	barrel = (event[1] * 0x40) >> 6;

	if (x == X_AXIS_MAX)
		x--;

	if (y == Y_AXIS_MAX)
		y--;

	input_mt_slot(info->input_dev, touchId);
	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, touchId);
	input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, z);
	input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, z);
	//input_report_abs(info->input_dev, ABS_MT_PRESSURE, z);

	input_report_key(info->input_dev, BTN_STYLUS, eraser);
	input_report_key(info->input_dev, BTN_STYLUS2, barrel);
	input_mt_report_slot_state(info->input_dev, MT_TOOL_PEN, 1);


	return fts_next_event(event);
}


/* EventId : 0x24 */
static unsigned char *fts_pen_leave_event_handler(
			struct fts_ts_info *info, unsigned char *event)
{
	unsigned char touchId;

	dev_dbg(info->dev, "Received event 0x%02x\n", event[0]);

	/* always use last position as touchId */
	touchId = TOUCH_ID_MAX;

	__clear_bit(touchId, &info->touch_id);

	input_report_key(info->input_dev, BTN_STYLUS, 0);
	input_report_key(info->input_dev, BTN_STYLUS2, 0);

	input_mt_slot(info->input_dev, touchId);
	input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, -1);

	dev_dbg(info->dev,
		"Event 0x%02x - release ID[%d]\n",
		event[0], touchId);

	return fts_next_event(event);
}


/* EventId : 0x25 */
#define fts_pen_motion_event_handler fts_pen_enter_event_handler


/*
 * This handler is called each time there is at least
 * one new event in the FIFO
 */
static void fts_event_handler(struct work_struct *work)
{
	struct fts_ts_info *info;
	int error, error1;
	int left_events;
	unsigned char regAdd;
	unsigned char data[FTS_EVENT_SIZE * (FTS_FIFO_MAX)] = {0};
	unsigned char *event = NULL;
	unsigned char eventId;
	event_dispatch_handler_t event_handler;

	info = container_of(work, struct fts_ts_info, work);
	/*
	 * to avoid reading all FIFO, we read the first event and
	 * then check how many events left in the FIFO
	 */

	wake_lock_timeout(&info->wakelock, HZ);
	
	regAdd = READ_ONE_EVENT;
	error = fts_read_reg(info, &regAdd,
			sizeof(regAdd), data, FTS_EVENT_SIZE);

	if (!error) {

		left_events = data[7] & 0x1F;
		if ((left_events > 0) && (left_events < FTS_FIFO_MAX)) {
			/*
			 * Read remaining events.
			 */
			regAdd = READ_ALL_EVENT;
			error1 = fts_read_reg(info, &regAdd, sizeof(regAdd),
						&data[FTS_EVENT_SIZE],
						left_events * FTS_EVENT_SIZE);

			/*
			 * Got an error reading remining events,
			 * process at least * the first one that was
			 * raeding fine.
			 */
			if (error1)
				data[7] &= 0xE0;
		}

		/* At least one event is available */
		event = data;
		do {
			eventId = *event;
			event_handler = info->event_dispatch_table[eventId];
			#if 0
			event = event_handler ?
					event_handler(info, event) :
					fts_next_event(event);
			#else
			if(eventId < EVENTID_LAST) {
				event = event_handler(info, (event));
			}else {
				event = fts_next_event(event);
			}
			#endif
			input_sync(info->input_dev);
		} while (event);
	}

	/*
	 * re-enable interrupts
	 */
	fts_interrupt_enable(info);
}

static void fts_fw_update_auto(struct work_struct *work)
{
	int retval, retval1, ret, ret1, ret2, retry, error, initretrycnt;
	struct fts_ts_info *info;
	struct delayed_work *fwu_work = container_of(work,struct delayed_work, work);
	info = container_of(fwu_work, struct fts_ts_info, fwu_work);
	/*check firmware*/
	info->fw_force = 0;
	retval = fts_fw_upgrade(info, 0,0);
	if(retval){
		tp_log("%s: firwmare update failed and retry!\n", __func__);	
		//fts_chip_powercycle(info);   // do powercycle befor upgrade
		retval1 =fts_fw_upgrade(info, 0,1);
		if(retval1){
			tp_log("%s: firwmare update failed again!\n", __func__);	
			return;
		}
	}

	if(info->fwupdate_stat == 0){// FW is not updated
		ret = fts_get_init_status(info); // 0 = initialization status correct
	}else{
		ret = -2; //  FW is updated, we want to force initialization
	}

	if(ret != 0){ 	// initialization status not correct or after FW update, do initialization.		
		ret1 = fts_init_flash_reload(info); // 0 = initialization done correctly
		if(ret1 != 0 ) {	
			////initialization error, retry initialization
			for(retry = 0; retry < 12; retry++){
				fts_chip_powercycle(info); 
				ret2 = fts_init_flash_reload(info);
				if(ret2 == 0)
					break;					    	
				initretrycnt++;
				tp_log("initialization cycle count = %02d\n", initretrycnt);
			}

			if(ret2 != 0){//initialization error
				error = fts_systemreset(info);
				msleep(150);
				error += fts_command(info, SLEEPOUT);
				usleep_range(10000,11000);
				error += fts_command(info, FORCECALIBRATION);
				error += fts_command(info, SENSEON);
				error += fts_command(info, FLUSHBUFFER);
				if (error) {
					tp_log("%s: Cannot reset the device----------\n", __func__);
				}
			}
		}
	}
}


#ifdef FTS_USE_POLLING_MODE
static enum hrtimer_restart fts_timer_func(struct hrtimer *timer)
{
	struct fts_ts_info *info =
		container_of(timer, struct fts_ts_info, timer);

	queue_work(info->event_wq, &info->work);
	return HRTIMER_NORESTART;
}
#else
static irqreturn_t fts_interrupt_handler(int irq, void *handle)
{
	struct fts_ts_info *info = handle;

	disable_irq_nosync(info->client->irq);
	queue_work(info->event_wq, &info->work);
	return IRQ_HANDLED;
}
#endif


static int fts_interrupt_install(struct fts_ts_info *info)
{
	int i, error = 0;

	info->event_dispatch_table = kzalloc(
		sizeof(event_dispatch_handler_t) * EVENTID_LAST, GFP_KERNEL);

	if (!info->event_dispatch_table) {
		dev_err(info->dev, "OOM allocating event dispatch table\n");
		return -ENOMEM;
	}

	for (i = 0; i < EVENTID_LAST; i++)
		info->event_dispatch_table[i] = fts_nop_event_handler;

	install_handler(info, ENTER_POINTER, enter_pointer);

	install_handler(info, LEAVE_POINTER, leave_pointer);
	install_handler(info, MOTION_POINTER, motion_pointer);

	install_handler(info, BUTTON_STATUS, button_status);

	install_handler(info, HOVER_ENTER_POINTER, hover_enter_pointer);
	install_handler(info, HOVER_LEAVE_POINTER, hover_leave_pointer);
	install_handler(info, HOVER_MOTION_POINTER, hover_motion_pointer);

	install_handler(info, PROXIMITY_ENTER, proximity_enter);
	install_handler(info, PROXIMITY_LEAVE, proximity_leave);

	install_handler(info, ERROR, error);
	install_handler(info, CONTROLLER_READY, controller_ready);
	install_handler(info, SLEEPOUT_CONTROLLER_READY,
					sleepout_controller_ready);
	install_handler(info, STATUS, status);

	install_handler(info, GESTURE, gesture);

	install_handler(info, PEN_ENTER, pen_enter);
	install_handler(info, PEN_LEAVE, pen_leave);
	install_handler(info, PEN_MOTION, pen_motion);

	/* disable interrupts in any case */
	fts_interrupt_set(info, INT_DISABLE);

#ifdef FTS_USE_POLLING_MODE
	dev_dbg(info->dev, "Polling Mode\n");
	hrtimer_init(&info->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->timer.function = fts_timer_func;
	hrtimer_start(&info->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#else
	dev_dbg(info->dev, "Interrupt Mode\n");
	if (request_irq(info->client->irq, fts_interrupt_handler,
			IRQF_TRIGGER_LOW, info->client->name,
			info)) {
		dev_err(info->dev, "Request irq failed\n");
		kfree(info->event_dispatch_table);
		error = -EBUSY;
	} else{
		fts_interrupt_set(info, INT_ENABLE);
		//enable_irq_wake(info->client->irq);
	}
#endif

	return error;
}


static void fts_interrupt_uninstall(struct fts_ts_info *info)
{
	fts_interrupt_set(info, INT_DISABLE);

	kfree(info->event_dispatch_table);

#ifdef FTS_USE_POLLING_MODE
	hrtimer_cancel(&info->timer);
#else
	free_irq(info->client->irq, info);
#endif
}

static void fts_interrupt_enable(struct fts_ts_info *info)
{
#ifdef FTS_USE_POLLING_MODE
	hrtimer_start(&info->timer,
			ktime_set(0, 10000000), HRTIMER_MODE_REL);
#else
	enable_irq(info->client->irq);
#endif
}

#if 0
static void fts_interrupt_disable(struct fts_ts_info *info)
{
#ifdef FTS_USE_POLLING_MODE
	hrtimer_cancel(&info->timer);
#else
	disable_irq(info->client->irq);
#endif
}

#endif

static int fts_init(struct fts_ts_info *info)
{
	int error;

	error = fts_systemreset(info);
	if (error) {
		dev_err(info->dev,
			"Cannot reset the device\n");
		return -ENODEV;
	}

	/* check for chip id */
	error = fts_get_fw_version(info);
	if (error) {
		dev_err(info->dev,
			"Cannot initiliaze, wrong device id\n");
		return -ENODEV;
	}

	error = fts_interrupt_install(info);

	if (error)
		dev_err(info->dev, "Init (1) error (#errors = %d)\n", error);

	return error ? -ENODEV : 0;
}

// add Power Cycle function  2015/11/3 20:06

static int fts_chip_powercycle(struct fts_ts_info *info)
{
	int error,i;

	if (info->bus_reg) {
		error = regulator_disable(info->bus_reg);
		if (error < 0) {
			dev_err(info->dev,"%s: Failed to enable bus pullup regulator\n",__func__);
		}
	}

	if (info->pwr_reg) {
		error = regulator_disable(info->pwr_reg);
		if (error < 0) {
			dev_err(info->dev,"%s: Failed to enable power regulator\n",__func__);
		}
	}
	msleep(300);
	if (info->pwr_reg)
		error = regulator_enable(info->pwr_reg);

	if (info->bus_reg)
		error = regulator_enable(info->bus_reg);
	msleep(300);

	gpio_set_value(info->bdata->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpio_set_value(info->bdata->reset_gpio, 1);
	msleep(500);

	for (i = 0; i < TOUCH_ID_MAX; i++){//before reset clear all slot
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev,
			(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
	}
	input_sync(info->input_dev);

	return error;
}


static int fts_save_tuning_value(struct fts_ts_info *info)
{
	//add crc check after save tuning value by harry
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char event_id = 0;
	unsigned char tune_flag= 0;
	unsigned char error ;
	unsigned char retry ;
	unsigned char regAdd =0;
	unsigned char crc_check_error=0;
	int backup_error = 2;
	unsigned char regAdd1[3] = {0xB6, 0x00, 0x86};
	unsigned char val[4];
	unsigned char crc_status;

	fts_interrupt_set(info, INT_DISABLE);
	usleep_range(10000, 10000);
	fts_command(info, FLUSHBUFFER);
	usleep_range(5000, 5000);
	fts_command(info, TUNING_BACKUP);
	msleep(100);

	for (retry = 0; retry < 200; retry++) 
	{
		regAdd = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd,sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -1;
		}
		tp_log("FTS fts statu event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			data[0], data[1], data[2], data[3],data[4], data[5], data[6], data[7]);

		event_id = data[0];
		tune_flag = data[1];

		if ((event_id == 0x16) && (tune_flag == 0x04))
		{
			tp_log("fts initialization save to flash ok \n");
			backup_error = 0;
			break;
		}
		else
		{
			usleep_range(10000, 10000);
		}
	}

	if(backup_error != 0)
	{
		tp_log("fts initialization save to flash timeout \n");
		return -1;
	}
	

	fts_command(info, FLUSHBUFFER);
	usleep_range(10000, 10000);
	printk("fts CRC check \n");
	fts_systemreset(info);
	printk("fts restart TP \n");
	msleep(200);

	// check CRC status
	error = fts_read_reg(info, regAdd1, sizeof(regAdd1), val, sizeof(val));
	if (error) 
	{
		dev_err(info->dev, "Cannot read crc status\n");
		return -1;
	}

	crc_status = val[1] & 0x06;
	if(crc_status != 0) // CRC error
	{
		printk("fts CRC status = %d \n", crc_status);
		crc_check_error = -1;
	}	

	fts_interrupt_set(info, INT_ENABLE);

	return crc_check_error;
}

static int fts_get_init_status(struct fts_ts_info *info)
{
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[4] = {0xB2, 0x07, 0xFB, 0x04};
	unsigned char regAdd1 =0;
	unsigned char event_id = 0;
	unsigned char chip_afe0_version = 0x56;
	unsigned char chip_afe1_version = 0x78;
	unsigned char error ;
	unsigned char retry ;
	unsigned char val[4];
	unsigned char crc_status;

	fts_interrupt_set(info, INT_DISABLE);
	usleep_range(10000, 10000);
	fts_command(info, FLUSHBUFFER);

	//regAdd[4] = { 0xB2, 0x07, 0xFB, 0x04 };
	fts_write_reg(info, regAdd, sizeof(regAdd)); //READ Config AFE version
	msleep(30);

	for(retry = 0; retry < 40; retry++) {
		regAdd1 = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd1,sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -1;
		}
		tp_log("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			data[0], data[1], data[2], data[3],data[4], data[5], data[6], data[7]);

		event_id = data[0];
		if (event_id == 0x12){
			chip_afe0_version = data[3];
			break;
		}else{
			usleep_range(10000, 10000);
		}
	}

	//regAdd[4] = { 0xB2, 0x17, 0xFB, 0x04};
	regAdd[1] = 0x17;
	fts_write_reg(info, regAdd, sizeof(regAdd)); //READ CX Memery AFE version
	msleep(30);

	for(retry = 0; retry < 40; retry++) {
		regAdd1 = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd1,sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -1;
		}
		tp_log("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			data[0], data[1], data[2], data[3],data[4], data[5], data[6], data[7]);

		event_id = data[0];
		if (event_id == 0x12){
			chip_afe1_version = data[3];
			break;
		}else{
			usleep_range(10000, 10000);
		}
	}

	if(chip_afe1_version == chip_afe0_version )
	{
		afe_version_same = 0x1;
		tp_log("fts AFE version is same\n");
	}else{
		afe_version_same = 0x0;
		tp_log("fts AFE version not the same\n");
	}

	fts_interrupt_set(info, INT_ENABLE);

	regAdd[0] = 0xB6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x86;
	error = fts_read_reg(info, regAdd, 3, val, sizeof(val));
	if (error) {
		dev_err(info->dev, "Cannot read crc status\n");
		return -1;
	}

	crc_status = val[1] & 0x06;

	if((crc_status != 0)||(afe_version_same == 0)){	
		tp_log("fts initialization status error\n");
		return -1;
	}else{
		tp_log("fts initialization status OK\n");
		return 0;
	}

}

static int  fts_init_flash_reload(struct fts_ts_info *info)
{
	unsigned char data[FTS_EVENT_SIZE];
	int retry, error = 0;
	unsigned char event_id = 0;
	unsigned char tune_flag = 0;
	char mutual_check_error = 2;
	char self_check_error = 2;
	unsigned char regAdd =0;
	unsigned char crc_check =0;
	
	fts_systemreset(info);
	printk("fts restart TP \n");
	msleep(500);

	init_completion(&info->cmd_done);
	error += fts_command(info, SLEEPOUT);
	WAIT_WITH_TIMEOUT(info, HZ, SLEEPOUT);

	fts_interrupt_set(info, INT_DISABLE);
	usleep_range(10000, 10000);
	fts_command(info, FLUSHBUFFER);
	usleep_range(5000, 5000);

	// mutual initialization
	fts_command(info, CX_TUNING);
	msleep(800);

	for (retry = 0; retry < 220; retry++){ // poll with timeout 2200ms
		regAdd = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd,sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -ENODEV;
		}
		printk("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]);

			event_id = data[0];
			tune_flag = data[1];

		if ((event_id == 0x16) && (tune_flag == 0x01)) {
			if((data[3] == 0x00) && (data[4] == 0x00)){
				mutual_check_error = 0;
				printk("fts mutual initialization ok \n");
			}else{
				mutual_check_error = 1;
				printk("fts mutual initialization fail \n");
			}
			break;
		}else{
			usleep_range(10000, 10000);
		}
	}

	if(mutual_check_error != 0){
		if(mutual_check_error == 2){
			printk("fts mutual initialization timeout \n");
		}
		return -1;	
	}		

	// self initialization
	fts_command(info, SELF_TUNING);
	msleep(300);

	for (retry = 0; retry < 270; retry++){ //poll with timeout
		regAdd = READ_ONE_EVENT;
		error = fts_read_reg(info, &regAdd,sizeof(regAdd), data, FTS_EVENT_SIZE);
		if (error) {
			dev_err(info->dev, "Cannot read device info\n");
			return -ENODEV;
		}
		printk("FTS fts status event: %02X %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[6], data[7]);

		event_id = data[0];
		tune_flag = data[1];

		if ((event_id == 0x16) && (tune_flag == 0x02)) {
			if((data[3] == 0x00) && (data[4] == 0x00)){
				self_check_error = 0;
				printk("fts self initialization ok \n");
			}else{
				self_check_error = 1;
				printk("fts self initialization fail \n");
			}
			break;	
		}else{
			usleep_range(10000, 10000);
		}
	}

	if(self_check_error != 0){
		if(self_check_error == 2){
			printk("fts self initialization timeout \n");
		}
		return -1;		
	}

	crc_check = fts_save_tuning_value(info);

	error += fts_command(info, SLEEPOUT);
	msleep(10);
	error += fts_command(info, SENSEON);
	fts_interrupt_set(info, INT_ENABLE);
	error += fts_command(info, FLUSHBUFFER);
	error += fts_command(info, FORCECALIBRATION);

	if (error != 0){
		dev_err(info->dev, "Init (2) error (#errors = %d)\n", error);
		return -ENODEV ;
	}else if (crc_check != 0){ 
		return -1;
	}else{ 
		return 0;
	}

}

static int fts_init_hw(struct fts_ts_info *info)
{
	int error = 0;

	init_completion(&info->cmd_done);
	error += fts_command(info, SLEEPOUT);
	WAIT_WITH_TIMEOUT(info, HZ, SLEEPOUT);

	error += fts_command(info, SENSEON);
	error += fts_command(info, FLUSHBUFFER);

	if (error)
		dev_err(info->dev, "Init (2) error (#errors = %d)\n", error);

	return error ? -ENODEV : 0;
}

static int fts_get_reg(struct fts_ts_info *rmi4_data,
		bool get)
{
	int retval;
	const struct fts_i2c_platform_data *bdata =
			rmi4_data->bdata;

	if (!get) {
		retval = 0;
		goto regulator_put;
	}

	if ((bdata->pwr_reg_name != NULL) && (*bdata->pwr_reg_name != 0)) {
		rmi4_data->pwr_reg = regulator_get(rmi4_data->dev,
				bdata->pwr_reg_name);
		if (IS_ERR(rmi4_data->pwr_reg)) {
			dev_err(rmi4_data->dev,
					"%s: Failed to get power regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->pwr_reg);
			goto regulator_put;
		}
	}

	if ((bdata->bus_reg_name != NULL) && (*bdata->bus_reg_name != 0)) {
		rmi4_data->bus_reg = regulator_get(rmi4_data->dev,
				bdata->bus_reg_name);
		if (IS_ERR(rmi4_data->bus_reg)) {
			dev_err(rmi4_data->dev,
					"%s: Failed to get bus pullup regulator\n",
					__func__);
			retval = PTR_ERR(rmi4_data->bus_reg);
			goto regulator_put;
		}
	}

	return 0;

regulator_put:
	if (rmi4_data->pwr_reg) {
		regulator_put(rmi4_data->pwr_reg);
		rmi4_data->pwr_reg = NULL;
	}

	if (rmi4_data->bus_reg) {
		regulator_put(rmi4_data->bus_reg);
		rmi4_data->bus_reg = NULL;
	}

	return retval;
}

static int fts_enable_reg(struct fts_ts_info *rmi4_data,
		bool enable)
{
	int retval;

	if (!enable) {
		retval = 0;
		goto disable_pwr_reg;
	}

	if (rmi4_data->bus_reg) {
		retval = regulator_enable(rmi4_data->bus_reg);
		if (retval < 0) {
			dev_err(rmi4_data->dev,
					"%s: Failed to enable bus pullup regulator\n",
					__func__);
			goto exit;
		}
	}

	if (rmi4_data->pwr_reg) {
		retval = regulator_enable(rmi4_data->pwr_reg);
		if (retval < 0) {
			dev_err(rmi4_data->dev,
					"%s: Failed to enable power regulator\n",
					__func__);
			goto disable_bus_reg;
		}
	}

	return 0;

disable_pwr_reg:
	if (rmi4_data->pwr_reg)
		regulator_disable(rmi4_data->pwr_reg);

disable_bus_reg:
	if (rmi4_data->bus_reg)
		regulator_disable(rmi4_data->bus_reg);

exit:
	return retval;
}
static int fts_gpio_setup(int gpio, bool config, int dir, int state)
{
	int retval = 0; 
		unsigned char buf[16];

		if (config) {
				snprintf(buf, 16, "fts_gpio_%u\n", gpio);

				retval = gpio_request(gpio, buf);
				if (retval) {
						pr_err("%s: Failed to get gpio %d (code: %d)",
										__func__, gpio, retval);
						return retval;
				}

				if (dir == 0)
						retval = gpio_direction_input(gpio);
				else 
						retval = gpio_direction_output(gpio, state);
				if (retval) {
						pr_err("%s: Failed to set gpio %d direction",
										__func__, gpio);
						return retval;
				}
		} else {
				gpio_free(gpio);
		}

		return retval;
}


static int fts_set_gpio(struct fts_ts_info*rmi4_data)
{
	int retval;
	const struct fts_i2c_platform_data *bdata =
			rmi4_data->bdata;

	retval = fts_gpio_setup(bdata->irq_gpio,true, 0, 0);
	if (retval < 0) {
		tp_log("%s: Failed to configure attention GPIO\n",__func__);
		goto err_gpio_irq;
	}

	if (bdata->reset_gpio >= 0) {
		retval = fts_gpio_setup(bdata->reset_gpio,true, 1, 0);
		if (retval < 0) {
			tp_log("%s: Failed to configure reset GPIO\n",__func__);
			goto err_gpio_reset;
		}
	}
	if (bdata->reset_gpio >= 0) {
		gpio_set_value(bdata->reset_gpio, 0);
		usleep_range(10000, 11000);
		gpio_set_value(bdata->reset_gpio, 1);
		msleep(70);
	}

	return 0;

err_gpio_reset:
	fts_gpio_setup(bdata->irq_gpio, false, 0, 0);
err_gpio_irq:
	return retval;
}

static int parse_dt(struct device *dev, struct fts_i2c_platform_data *bdata)
{
	int retval;
	const char *name;
	struct device_node *np = dev->of_node;
	struct pinctrl *pinctrl;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
			"touch,irq-gpio", 0, NULL);
	
	pinctrl = devm_pinctrl_get_select(dev, "touch_irq");
	if(IS_ERR(pinctrl)) 
		 tp_log( "failed to get tp irq pinctrl - ON");
#if 0
	retval = of_property_read_u32(np, "synaptics,irq-flags", &value);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = value;
#endif
	retval = of_property_read_string(np, "touch,regulator_vddio", &name);
	if (retval == -EINVAL)
		bdata->pwr_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "touch,regulator_vdd", &name);
	if (retval == -EINVAL)
		bdata->bus_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->bus_reg_name = name;

	if (of_property_read_bool(np, "touch,reset-gpio")) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
				"touch,reset-gpio", 0, NULL);
	} else {
		bdata->reset_gpio = -1;
	}

	return 0;
}

static int fts_fb_state_chg_callback(struct notifier_block *nb, 
		unsigned long val, void *data)
{
	struct fts_ts_info *info = container_of(nb, struct fts_ts_info, notifier);
	struct fb_event *evdata = data;
	int i;
	unsigned int blank;


	if(val != FB_EVENT_BLANK)
		return 0;
	//tp_log("%s: fts notifier begin!\n", __func__);

	if(evdata && evdata->data && val == FB_EVENT_BLANK && info) {

		blank = *(int *)(evdata->data);
	
		switch(blank) {
		case FB_BLANK_POWERDOWN:
			if(info->sensor_sleep)
				break;
			/* Release all buttons */
			info->buttons = 0;
			info->resume_bit = 0;

			/* No need ot check for error code */
			//cancel_work_sync(&info->work);
			tp_log("%s: gesture_disall = %d, info->mode = %d\n", __func__, info->gesture_disall, info->mode);
			if( (!info->gesture_disall) &&(info->mode != MODE_COVER)) {
				fts_set_gesture(info);
				fts_command(info, ENTER_GESTURE_MODE);
				enable_irq_wake(info->client->irq);
				fts_set_sensor_mode(info, MODE_GESTURE);
				info->gesture_enable = 1;
			} else {
				/* Read out device mode, used when resuming device */

				/* suspend the device and flush the event FIFO */
				tp_log("%s: suspend send command sleep \n", __func__);
				fts_command(info, SLEEPIN);
				fts_command(info, FLUSHBUFFER);
			}

			/* Release all slots */
			for (i = 0; i < TOUCH_ID_MAX; i++){
					input_mt_slot(info->input_dev, i);
					input_mt_report_slot_state(info->input_dev,
					(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
			}
			input_sync(info->input_dev);
	
			info->sensor_sleep = true;
			break;

		case FB_BLANK_UNBLANK:
			if(!info->sensor_sleep)
				break;
			tp_log("%s: FB_BLANK_UNBLANK\n", __func__);
			
			for (i = 0; i < TOUCH_ID_MAX; i++){
					input_mt_slot(info->input_dev, i);
					input_mt_report_slot_state(info->input_dev,
					(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
			}
			input_sync(info->input_dev);

			
			if(info->gesture_enable == 1) {
				fts_command(info, SLEEPIN);
				msleep(200);
				disable_irq_wake(info->client->irq);
				info->gesture_enable = 0;
				fts_set_sensor_mode(info, MODE_NORMAL);
				if(info->glove_bit)
					fts_set_sensor_mode(info, MODE_GLOVE);
				if(info->hover_bit)
					fts_set_sensor_mode(info, MODE_HOVER);
			}

			/* wake-up the device */
			init_completion(&info->cmd_done);
			fts_command(info, SLEEPOUT);
			WAIT_WITH_TIMEOUT(info, HZ, SLEEPOUT);

#if 0
			if(info->gesture_enable == 1) {
				/* enable sense */
				fts_command(info, 0xC5);
				info->gesture_enable = 0;
			}else{
				/* enable sense */
				fts_command(info, SENSEON);
			}
#else
			/* enable sense */
			fts_command(info, SENSEON);
#endif
			/* put back the device in the original mode (see fts_suspend()) 
			switch (info->mode) {
			case MODE_HOVER:
				fts_command(info, HOVER_ON);
				break;

			case MODE_GLOVE:
				fts_command(info, GLOVE_ON);
				break;

			case MODE_COVER:
				fts_cover_on(info);
				usleep_range(5000,6000);
				fts_command(info, FORCECALIBRATION);
				break;
				
			default:
				dev_warn(info->dev, "Invalid device mode - 0x%02x\n",
				info->mode);
			break;
			}*/

			info->resume_bit = 1;
			info->sensor_sleep = false;
			break;
		default:
			break;

		}
	}
	return NOTIFY_OK;
	
}
static struct notifier_block fts_noti_block = {
	.notifier_call = fts_fb_state_chg_callback,
};

static int fts_probe(struct i2c_client *client,
				const struct i2c_device_id *idp)
{
	struct fts_ts_info *info = NULL;
	char fts_ts_phys[64];
	int error = 0;
	struct device_node *dp = client->dev.of_node;
	int retval;

	//tp_log("%s: fts tp driver probe begin.\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Unsupported I2C functionality\n");
		error = -EIO;
		goto ProbeErrorExit_0;
	}

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "Out of memory\n");
		error = -ENOMEM;
		goto ProbeErrorExit_0;
	}

	if(dp) {
		info->bdata = devm_kzalloc(&client->dev, sizeof(struct fts_i2c_platform_data), GFP_KERNEL);		
		if(!info->bdata){
			tp_log("info.bdate kzalloc failed \n");
			goto ProbeErrorExit_1;
		}
		parse_dt(&client->dev, info->bdata);
	}

	retval = fts_get_reg(info, true);
	if (retval < 0) {
		tp_log("%s: Failed to get regulators\n",__func__);
		goto ProbeErrorExit_1;
	}

	retval = fts_enable_reg(info, true);
	if (retval < 0) {
		tp_log("%s: Failed to enable regulators\n",__func__);
		goto ProbeErrorExit_2;
	}

	retval = fts_set_gpio(info);
	if (retval < 0) {
		tp_log("%s: Failed to set up GPIO's\n",__func__);
		goto ProbeErrorExit_2;
	}

	info->client = client;
	i2c_set_clientdata(client, info);

	info->fwu_workqueue = create_singlethread_workqueue("fts-fwu-queue");
	if(!info->fwu_workqueue){
		tp_log("cannot create fwu work thread\n");
		goto ProbeErrorExit_3;
	}
	INIT_DELAYED_WORK(&info->fwu_work, fts_fw_update_auto);

	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND,"fts_tp");
	info->event_wq = create_singlethread_workqueue("fts-event-queue");
	if (!info->event_wq) {
		dev_err(&client->dev, "Cannot create work thread\n");
		error = -ENOMEM;
		goto ProbeErrorExit_31;
	}

	INIT_WORK(&info->work, fts_event_handler);

	info->client->irq = gpio_to_irq(info->bdata->irq_gpio);
	//tp_log("%s: bdata->irq_gpio = %d, to irq :client->irq = %d\n",__func__, info->bdata->irq_gpio, info->client->irq);


	info->dev = &info->client->dev;
	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		dev_err(info->dev, "No such device\n");
		error = -ENODEV;
		goto ProbeErrorExit_4;
	}
	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = FTS_TS_DRV_NAME;
	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input0",
			 info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->id.vendor = 0x0001;
	info->input_dev->id.product = 0x0002;
	info->input_dev->id.version = 0x0100;

	__set_bit(EV_SYN, info->input_dev->evbit);
	__set_bit(EV_KEY, info->input_dev->evbit);
	__set_bit(EV_ABS, info->input_dev->evbit);
	__set_bit(BTN_TOUCH, info->input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, info->input_dev->keybit);

	input_mt_init_slots(info->input_dev, TOUCH_ID_MAX, INPUT_MT_DIRECT);

	//input_mt_init_slots(info->input_dev, TOUCH_ID_MAX,0);

	input_set_abs_params(info->input_dev, ABS_MT_TRACKING_ID,
					 0, FINGER_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
					 X_AXIS_MIN, X_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
					 Y_AXIS_MIN, Y_AXIS_MAX, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
					 AREA_MIN, AREA_MAX, 0, 0);
	input_set_abs_params(info->input_dev,ABS_MT_TOUCH_MINOR,
					AREA_MIN, AREA_MAX, 0, 0);
	//input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
					// PRESSURE_MIN, PRESSURE_MAX, 0, 0);

	input_set_capability(info->input_dev, EV_KEY, KEY_GESTURE);

	/* register the multi-touch input device */
	error = input_register_device(info->input_dev);
	if (error) {
		dev_err(info->dev, "No such device\n");
		error = -ENODEV;
		goto ProbeErrorExit_4;
	}

	/* track slots */
	info->touch_id = 0;

	/* track buttons */
	info->buttons = 0;
	/* init hardware device */
	error = fts_init(info);
	if (error) {
		dev_err(info->dev, "Cannot initialize the device\n");
		error = -ENODEV;
		goto ProbeErrorExit_5;
	}

	error = fts_init_hw(info);
	if (error) {
		dev_err(info->dev, "Cannot initialize the hardware device\n");
		error = -ENODEV;
		goto ProbeErrorExit_5;
	}
	info->resume_bit = 1;
	info->gesture_disall = 1 ;
	info->gesture_value  = 0 ;

	
	mutex_init(&info->fts_mode_mutex);
	info->notifier = fts_noti_block;
	error = fb_register_client(&info->notifier);
	if(error) {
		tp_log("fts register notifier failed.\n");
		goto ProbeErrorExit_5;
	} 

	fts_register_cover_notify(info);


#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = fts_early_suspend;
	info->early_suspend.resume = fts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

#ifdef CONFIG_EXYNOS_TOUCH_DAEMON
#ifdef CONFIG_TOUCHSCREEN_FTS
	exynos_touch_daemon_data.touchdata = info;
#endif
#endif

	/* sysfs stuff */
	info->attrs.attrs = fts_attr_group;
	error = sysfs_create_group(&client->dev.kobj, &info->attrs);
	if (error) {
		dev_err(info->dev, "Cannot create sysfs structure\n");
		error = -ENODEV;
		goto ProbeErrorExit_6;
	}

	if( meizu_sysfslink_register(&info->client->dev, LINK_KOBJ_NAME) < 0)
		tp_log("sysfs_create_link failed.\n");

	queue_delayed_work(info->fwu_workqueue, &info->fwu_work, msecs_to_jiffies(EXP_FN_WORK_DELAY_MS));
	return 0;

/* error exit path */

ProbeErrorExit_6:
	fb_unregister_client(&info->notifier);
	hall_unregister_notifier_client(&info->cover_notifier);
ProbeErrorExit_5:
	input_unregister_device(info->input_dev);

ProbeErrorExit_4:
	destroy_workqueue(info->event_wq);

ProbeErrorExit_31:
	destroy_workqueue(info->fwu_workqueue);
	wake_lock_destroy(&info->wakelock);
	
ProbeErrorExit_3:
	fts_enable_reg(info, false);
ProbeErrorExit_2:
	fts_get_reg(info, false);	
ProbeErrorExit_1:
	kfree(info);

ProbeErrorExit_0:
	dev_err(&client->dev, "Probe failed.\n");

	return error;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	/* sysfs stuff */
	sysfs_remove_group(&client->dev.kobj, &info->attrs);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

	/* remove interrupt and event handlers */
	fts_interrupt_uninstall(info);

	/* Empty the FIFO buffer */
	fts_command(info, FLUSHBUFFER);

	/* Remove the work thread */
	destroy_workqueue(info->event_wq);
	destroy_workqueue(info->fwu_workqueue);
	
	//mx_remove_link(LINK_KOBJ_NAME);
	meizu_sysfslink_unregister(LINK_KOBJ_NAME);

	/* unregister the device */
	input_unregister_device(info->input_dev);

	fts_enable_reg(info, false);
	fts_get_reg(info, false);

	/* free all */
	kfree(info);

	return 0;
}
#if 0
static int fts_suspend(struct i2c_client *client, pm_message_t mesg)
{

	struct fts_ts_info *info = i2c_get_clientdata(client);
	int i;


	if(!st_tp || info->sensor_sleep)
		return 0;
	tp_log("%s: suspend begin!\n", __func__);
	/* Release all buttons */
	info->buttons = 0;

	/* Release all slots */
	for (i = 0; i < TOUCH_ID_MAX; i++)
		if (__test_and_clear_bit(i, &info->touch_id)) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev,
			(i < FINGER_MAX) ? MT_TOOL_FINGER : MT_TOOL_PEN, 0);
		}
	input_sync(info->input_dev);

	/* No need ot check for error code */
	cancel_work_sync(&info->work);

	if(info->gesture_enable) {
		fts_gesture_control(info);
		fts_command(info, ENTER_GESTURE_MODE);
		enable_irq_wake(info->client->irq);
	} else {
		fts_interrupt_disable(info);
		/* Read out device mode, used when resuming device */
		//fts_get_mode(info);

		/* suspend the device and flush the event FIFO */
		tp_log("%s: suspend send command sleep \n", __func__);
		fts_command(info, SLEEPIN);
		fts_command(info, FLUSHBUFFER);
	}
	info->sensor_sleep = true;
#if 0
	if ((info->power) && (info->power(FTS_POWER_OFF)))
		dev_warn(info->dev, "Cannot power-off device\n");
#endif
	/* ignore errors */
	return 0;
}


static int fts_resume(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int error;
#if 0
	/* Power-on the device */
	if ((info->power) && (info->power(FTS_POWER_ON))) {
		dev_err(&client->dev, "Cannot power-on device\n");
		return -ENODEV;
	}
#endif

	if(!st_tp || !info->sensor_sleep)
		return 0;
	tp_log("%s: start\n", __func__);

	error = fts_systemreset(info);
	if (error) {
		dev_err(info->dev,
			"Cannot reset the device\n");
		return -ENODEV;
	}
	msleep(100);
	
	if(info->gesture_enable){
		disable_irq_wake(info->client->irq);
	}
	else {
		fts_interrupt_enable(info);
	/* enable interrupts */
	}

	/* wake-up the device */
	init_completion(&info->cmd_done);
	fts_command(info, SLEEPOUT);
	WAIT_WITH_TIMEOUT(info, HZ, SLEEPOUT);

	/* enable sense */
	fts_command(info, SENSEON);

	/* put back the device in the original mode (see fts_suspend()) */
	switch (info->mode) {
	case MODE_PROXIMITY:
		fts_command(info, PROXIMITY_ON);
		break;

	case MODE_HOVER:
		fts_command(info, HOVER_ON);
		break;

	case MODE_GESTURE:
		fts_command(info, GESTURE_ON);
		break;

	case MODE_HOVER_N_PROXIMITY:
		fts_command(info, HOVER_ON);
		fts_command(info, PROXIMITY_ON);
		break;

	case MODE_GESTURE_N_PROXIMITY:
		fts_command(info, GESTURE_ON);
		fts_command(info, PROXIMITY_ON);
		break;

	case MODE_GESTURE_N_PROXIMITY_N_HOVER:
		fts_command(info, HOVER_ON);
		fts_command(info, GESTURE_ON);
		fts_command(info, PROXIMITY_ON);
		break;

	default:
		dev_warn(info->dev, "Invalid device mode - 0x%02x\n",
				info->mode);
		break;
	}

	if(info->glove_bit) {
		error = fts_command(info, GLOVE_ON);
		if(error) {
			tp_log("%s: glove mode open failed!\n", __func__);
		}
	}
	info->sensor_sleep = false;
	return 0;
}
#endif

static struct of_device_id fts_of_match_table[] = {
	{
		.compatible = "st,fts",
	},
	{},
};
static const struct i2c_device_id fts_device_id[] = {
	{FTS_TS_DRV_NAME, 0},
	{}
};

static struct i2c_driver fts_i2c_driver = {
	.driver   = {
		.name = FTS_TS_DRV_NAME,
		.of_match_table = fts_of_match_table,
	},
	.probe	= fts_probe,
	.remove   = fts_remove,
	.id_table = fts_device_id,
};

static int __init fts_driver_init(void)
{
	return i2c_add_driver(&fts_i2c_driver);
}


static void __exit fts_driver_exit(void)
{
	i2c_del_driver(&fts_i2c_driver);
}


MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("JHJANG");
MODULE_AUTHOR("Giuseppe Di Giore <giuseppe.di-giore@st.com");
MODULE_LICENSE("GPL");

late_initcall(fts_driver_init);
module_exit(fts_driver_exit);
