/*
 *  stmvl6180.c - Linux kernel modules for STM VL6180 FlightSense Time-of-Flight sensor
 *
 *  Copyright (C) 2014 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

//API includes
#include "vl6180x_api.h"
#include "vl6180x_def.h"
#include "vl6180x_platform.h"
#include "stmvl6180.h"
#include "ranging_meizu.h"

stmvl6180x_dev vl6180x_dev;
//#define USE_INT
#define IRQ_NUM	   59
#define VL6180_I2C_ADDRESS  (0x52>>1)
/*
 * Global data
 */
//******************************** IOCTL definitions
#define VL6180_IOCTL_INIT 		_IO('p', 0x01)
#define VL6180_IOCTL_XTALKCALB		_IO('p', 0x02)
#define VL6180_IOCTL_OFFCALB		_IO('p', 0x03)
#define VL6180_IOCTL_STOP		_IO('p', 0x05)
#define VL6180_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL6180_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL6180_IOCTL_GETDATA 		_IOR('p', 0x0a, unsigned long)
#define VL6180_IOCTL_GETDATAS 		_IOR('p', 0x0b, VL6180x_RangeData_t)
struct mutex	  vl6180_mutex;
//#define CALIBRATION_FILE 1
#ifdef CALIBRATION_FILE
int8_t offset_calib=0;
int16_t xtalk_calib=0;
#endif

int offset_init = 0;

extern void i2c_setclient(struct i2c_client *client);
struct i2c_client* i2c_getclient(void);
static int stmvl6180_set_enable(struct i2c_client *client, unsigned int enable)
{

	return 0;

}

#ifdef MZ_LAER_FACTORY_TEST
#define VL6180_OFFSET_CALIB	0x02
#define VL6180_XTALK_CALIB		0x03

#define N_MEASURE_AVG   20
#define OFFSET_CALIB_TARGET_DISTANCE 100 // 100mm
#define XTALK_CALIB_TARGET_DISTANCE 400 // 400mm

static int g_Laser_OffsetCalib = 0xFFFFFFFF;
static int g_Laser_XTalkCalib = 0x2;

static int    g_s4Laser_Opened = 0;
static spinlock_t g_Laser_SpinLock;


#ifdef MZ_LASER_DEBUG
#define LOG_INF(format, args...) pr_info("[%s] " format, __FUNCTION__, ##args)
#else
#define LOG_INF(format, args...)
#endif

int VL6180x_GetRangeValue(VL6180x_RangeData_t *Range/*int *RangeValue, int *SingleRate*/)
{
	int Result = 1;

	int ParamVal = 765;

	#if 0
	int status;
	uint8_t range_start;
	uint8_t range_status;
	#endif
	
	u8 u8status=0;

	#if 0

	VL6180x_RangeGetInterruptStatus(vl6180x_dev, &u8status);
	
	if (u8status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY)
	{
		status = VL6180x_RdByte(vl6180x_dev, SYSRANGE_START, &range_start);
		status += VL6180x_RdByte(vl6180x_dev, RESULT_RANGE_STATUS, &range_status);
		if (!status && ((range_status&0x01) == 0x01) && (range_start == 0x0)) {
			VL6180x_RangeGetMeasurement(vl6180x_dev, Range);
			VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);
			ParamVal = Range->range_mm;
			last_dist = Range->range_mm;
		} else {
			/* return immediately with previous value when device is busy */
			Range->range_mm = last_dist;
		}
		
		/*if( ParamVal < 400 )
		{
			ParamVal = VL6180x_RangeCheckMeasurement(ParamVal);
		}*/
		//LOG_INF("Laser Range : %d / %d \n", Range->range_mm, Range->DMax); 
	}
	else
	{
		ParamVal = 765;
		LOG_INF("Laser Get Data Failed \n");
	}	

	#else

	int Count = 0;
	
	while(1)
	{
		VL6180x_RangeGetInterruptStatus(vl6180x_dev, &u8status);

		if ( u8status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY )
		{
			VL6180x_RangeGetMeasurement(vl6180x_dev, Range);

			ParamVal = Range->range_mm;

			VL6180x_RangeClearInterrupt(vl6180x_dev);
			
			break;
		}

		if( Count > 10 )
		{
			ParamVal = 765;
			Range->range_mm = 765;

			LOG_INF("Laser Get Data Failed \n");

			VL6180x_RangeClearInterrupt(vl6180x_dev);
			
			break;
		}

		msleep(3);
		
		Count++;
	}

	VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);
	
	#endif
	

	if( ParamVal == 765 )
	{
		Result = 0;
	}
	
	return Result;
}

void VL6180x_SystemInit(int Scaling, int EnWAF, int CalibMode)
{
	//VL6180x_WaitDeviceBooted(vl6180x_dev);
	VL6180x_InitData(vl6180x_dev);
	
	VL6180x_Prepare(vl6180x_dev);
	VL6180x_UpscaleSetScaling(vl6180x_dev, Scaling);
	VL6180x_FilterSetState(vl6180x_dev, EnWAF); // turn on wrap around filter
	VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
	VL6180x_RangeClearInterrupt(vl6180x_dev);

	//Calibration Data
	if( CalibMode == VL6180_OFFSET_CALIB )
	{
		VL6180x_WrWord(vl6180x_dev, SYSRANGE_PART_TO_PART_RANGE_OFFSET, 0);
		VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);
		g_Laser_OffsetCalib = 0xFFFFFFFF;
		g_Laser_XTalkCalib = 0xFFFFFFFF;
	}
	else if( CalibMode == VL6180_XTALK_CALIB )
	{
		VL6180x_WrWord(vl6180x_dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, 0);
		g_Laser_XTalkCalib = 0xFFFFFFFF;
	}
	
	if( g_Laser_OffsetCalib != 0xFFFFFFFF )
	{
		VL6180x_SetOffsetCalibrationData(vl6180x_dev, g_Laser_OffsetCalib);
		LOG_INF("VL6180 Set Offset Calibration: Set the offset value as %d\n", g_Laser_OffsetCalib);
	}
	if( g_Laser_XTalkCalib != 0xFFFFFFFF )
	{
		VL6180x_SetXTalkCompensationRate(vl6180x_dev, g_Laser_XTalkCalib);
		LOG_INF("VL6180 Set XTalk Calibration: Set the XTalk value as %d\n", g_Laser_XTalkCalib);
	}
	
	VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);		
}

#endif

#ifdef CALIBRATION_FILE
static void stmvl6180_read_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;
	int i,is_sign=0;
	f = filp_open("/data/calibration/offset", O_RDONLY, 0);
	if (f!= NULL && !IS_ERR(f) && f->f_dentry!=NULL)
	{
		fs = get_fs();
		set_fs(get_ds());
		//init the buffer with 0
		for (i=0;i<8;i++)
			buf[i]=0;
		f->f_op->read(f, buf, 8, &f->f_pos);
		set_fs(fs);
		printk("offset as:%s, buf[0]:%c\n",buf, buf[0]);
		for (i=0;i<8;i++)
		{
			if (i==0 && buf[0]=='-')
				is_sign=1;
			else if (buf[i]>='0' && buf[i]<='9')
				offset_calib = offset_calib*10 + (buf[i]-'0');
			else
				break;
		}
		if (is_sign==1)
			offset_calib=-offset_calib;
		printk("offset_calib as %d\n", offset_calib);
		#ifdef CALIBRATE_CONFIG
		VL6180x_SetUserOffsetCalibration(vl6180x_dev, offset_calib);
		#endif
		filp_close(f, NULL);
	}
	else
		printk("no offset calibration file exist!\n");


	is_sign=0;
	f = filp_open("/data/calibration/xtalk", O_RDONLY, 0);
	if (f!= NULL && !IS_ERR(f) && f->f_dentry!=NULL)
	{
		fs = get_fs();
		set_fs(get_ds());
		//init the buffer with 0
		for (i=0;i<8;i++)
			buf[i]=0;
		f->f_op->read(f, buf, 8, &f->f_pos);
		set_fs(fs);
		printk("xtalk as:%s, buf[0]:%c\n",buf, buf[0]);
		for (i=0;i<8;i++)
		{
			if (i==0 && buf[0]=='-')
				is_sign=1;
			else if (buf[i]>='0' && buf[i]<='9')
				xtalk_calib = xtalk_calib*10 + (buf[i]-'0');
			else 
				break;
		}
		if (is_sign==1)
			xtalk_calib = -xtalk_calib;
		printk("xtalk_calib as %d\n", xtalk_calib);
		#ifdef CALIBRATE_CONFIG
		VL6180x_SetUserXTalkCompensationRate(vl6180x_dev, xtalk_calib);
		#endif
		filp_close(f, NULL);
	}
	else
		printk("no xtalk calibration file exist!\n");

	return;
}

#ifdef CALIBRATION_FILE
static void stmvl6180_write_offset_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;

	f = filp_open("/data/calibration/offset", O_WRONLY|O_CREAT, 0644);
	if (f!= NULL)
	{
		fs = get_fs();
		set_fs(get_ds());
		sprintf(buf,"%d",offset_calib);
		printk("write offset as:%s, buf[0]:%c\n",buf, buf[0]);
		f->f_op->write(f, buf, 8, &f->f_pos);
		set_fs(fs);
		#ifdef CALIBRATE_CONFIG
		VL6180x_SetUserOffsetCalibration(vl6180x_dev, offset_calib);
		#endif
	}
	filp_close(f, NULL);

	return;
}
static void stmvl6180_write_xtalk_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;

	f = filp_open("/data/calibration/xtalk", O_WRONLY|O_CREAT, 0644);
	if (f!= NULL)
	{
		fs = get_fs();
		set_fs(get_ds());
		sprintf(buf,"%d",xtalk_calib);
		printk("write xtalk as:%s, buf[0]:%c\n",buf, buf[0]);
		f->f_op->write(f, buf, 8, &f->f_pos);
		set_fs(fs);
		#ifdef CALIBRATE_CONFIG
		VL6180x_SetUserXTalkCompensationRate(vl6180x_dev, xtalk_calib);
		#endif
	}
	filp_close(f, NULL);

	return;
}
#endif

#endif
static void stmvl6180_ps_read_measurement(struct i2c_client *client)
{
	struct stmvl6180_data *data = i2c_get_clientdata(client);

	VL6180x_RangeGetMeasurement(vl6180x_dev, &(data->rangeData));


	data->ps_data = data->rangeData.range_mm;		
		
	input_report_abs(data->input_dev_ps, ABS_DISTANCE,(int)(data->ps_data+5)/10);
	input_report_abs(data->input_dev_ps, ABS_HAT0X,data->rangeData.range_mm);
	input_report_abs(data->input_dev_ps, ABS_X,data->rangeData.signalRate_mcps);
	input_sync(data->input_dev_ps);
	if (data->enableDebug)
		printk("range:%d, signalrate_mcps:%d, error:0x%x,rtnsgnrate:%u, rtnambrate:%u,rtnconvtime:%u\n", 
			data->rangeData.range_mm,
			data->rangeData.signalRate_mcps,
			data->rangeData.errorStatus,
			data->rangeData.rtnRate,
			data->rangeData.rtnAmbRate,
			data->rangeData.rtnConvTime);
	
}
/* interrupt work handler */
static void stmvl6180_work_handler(struct work_struct *work)
{
	struct stmvl6180_data *data = container_of(work, struct stmvl6180_data, dwork.work);
	struct i2c_client *client=data->client;
	uint8_t gpio_status=0;
	uint8_t to_startPS=0;


	mutex_lock(&data->work_mutex);
	
	VL6180x_RangeGetInterruptStatus(vl6180x_dev, &gpio_status);
	if (gpio_status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY)
	{
		if( data->enable_ps_sensor)
		{
			stmvl6180_ps_read_measurement(client);
			if (data->ps_is_singleshot)
				to_startPS = 1;
		}
		VL6180x_RangeClearInterrupt(vl6180x_dev);

	}
#if 0 //#for testing
	else
	{
		uint8_t data;
		VL6180x_RdByte(vl6180x_dev, RESULT_RANGE_STATUS, &data);
		printk("we get status as 0x%x\n",data);
	}
#endif
	if (to_startPS)
	{
		VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP | MODE_SINGLESHOT);
	}

	schedule_delayed_work(&data->dwork, msecs_to_jiffies((INT_POLLING_DELAY)));	// restart timer

   	mutex_unlock(&data->work_mutex);

	return;
}

#ifdef USE_INT
static irqreturn_t stmvl6180_interrupt_handler(int vec, void *info)
{

	struct i2c_client *client=(struct i2c_client *)info;
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	

	if (data->irq == vec)
	{
		//vl6180_dbgmsg("==>interrupt_handler\n");
		schedule_delayed_work(&data->dwork, 0);
	}
	return IRQ_HANDLED;
}
#endif

/*
 * SysFS support
 */
static ssize_t stmvl6180_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	
	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t stmvl6180_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);
 	unsigned long flags;
	
	vl6180_dbgmsg("enable ps senosr ( %ld),addr:0x%x\n", val,client->addr);
	
	if ((val != 0) && (val != 1)) {
		pr_err("%s:store unvalid value=%ld\n", __func__, val);
		return count;
	}
 	mutex_lock(&data->work_mutex);
	if(val == 1) {
		//turn on p sensor
		if (data->enable_ps_sensor==0) {

			stmvl6180_set_enable(client,0); /* Power Off */

			//re-init
			VL6180x_Prepare(vl6180x_dev);
			VL6180x_UpscaleSetScaling(vl6180x_dev, 3);

			//set parameters
			//VL6180x_RangeSetInterMeasPeriod(vl6180x_dev, 10); //10ms
			//set interrupt mode
			//VL6180x_RangeSetupGPIO1(vl6180x_dev, GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT, INTR_POL_HIGH);
			VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);

			//start
			VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
			data->ps_is_singleshot = 1;
			data->enable_ps_sensor= 1;

			/* we need this polling timer routine for house keeping*/
			spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			#ifndef COMPLY_WITH_MEIZU
			__cancel_delayed_work(&data->dwork);
			#else
			cancel_delayed_work(&data->dwork);
			#endif
			schedule_delayed_work(&data->dwork, msecs_to_jiffies(INT_POLLING_DELAY));	
			spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);	

			stmvl6180_set_enable(client, 1); /* Power On */	 
		}
	} 
	else {
		//turn off p sensor 
		data->enable_ps_sensor = 0;
		if (data->ps_is_singleshot == 0)
			VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP);
		VL6180x_RangeClearInterrupt(vl6180x_dev);

		stmvl6180_set_enable(client, 0);

		spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
		#ifndef COMPLY_WITH_MEIZU
		__cancel_delayed_work(&data->dwork);
		#else
		cancel_delayed_work(&data->dwork);
		#endif
		spin_unlock_irqrestore(&data->update_lock.wait_lock, flags); 

	}

	mutex_unlock(&data->work_mutex);
	return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IWUGO | S_IRUGO,
				   stmvl6180_show_enable_ps_sensor, stmvl6180_store_enable_ps_sensor);

static ssize_t stmvl6180_show_enable_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);

	
	return sprintf(buf, "%d\n", data->enableDebug);	
}

//for als integration time setup
static ssize_t stmvl6180_store_enable_debug(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	long on = simple_strtol(buf, NULL, 10);
	if ((on !=0) &&  (on !=1))
	{
		pr_err("%s: err! invalide input: %s\n", __func__, buf);
		return count;
	}
	data->enableDebug=on;

	return count;
}

//DEVICE_ATTR(name,mode,show,store)
static DEVICE_ATTR(enable_debug, S_IWUSR | S_IRUGO,
				   stmvl6180_show_enable_debug, stmvl6180_store_enable_debug);


static struct attribute *stmvl6180_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
	NULL
};


static const struct attribute_group stmvl6180_attr_group = {
	.attrs = stmvl6180_attributes,
};
/*
 * misc device file operation functions
 */
static int stmvl6180_ioctl_handler(struct file *file, 
				unsigned int cmd, unsigned long arg, void __user *p)
{

	int rc=0;
 	unsigned long flags;
	unsigned long distance=0;
	struct i2c_client *client;
	switch (cmd) {
	case VL6180_IOCTL_INIT:	   /* init.  */
	{
		client = i2c_getclient();
		if (client)
		{
			struct stmvl6180_data *data = i2c_get_clientdata(client);
			//turn on p sensor only if it's not enabled by other client
			if (data->enable_ps_sensor==0) {
				printk("ioclt INIT to enable PS sensor=====\n");
				stmvl6180_set_enable(client,0); /* Power Off */
				//re-init
				VL6180x_Prepare(vl6180x_dev);
				VL6180x_UpscaleSetScaling(vl6180x_dev, 3);
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
				VL6180x_FilterSetState(vl6180x_dev, 1); // turn on wrap around filter
#endif
				//set parameters
				//VL6180x_RangeSetInterMeasPeriod(vl6180x_dev, 10); //10ms
				//set interrupt mode
				//VL6180x_RangeSetupGPIO1(vl6180x_dev, GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT, INTR_POL_HIGH);
				VL6180x_RangeConfigInterrupt(vl6180x_dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
				VL6180x_RangeClearInterrupt(vl6180x_dev);
	
				//start
				//range_set_systemMode(client->addr, RANGE_START_SINGLESHOT);
				//data->ps_is_singleshot = 1;
				VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
				data->ps_is_singleshot = 1;
				data->enable_ps_sensor= 1;

				/* we need this polling timer routine for house keeping*/
				spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				#ifndef COMPLY_WITH_MEIZU
				__cancel_delayed_work(&data->dwork);
				#else
				cancel_delayed_work(&data->dwork);
				#endif
				schedule_delayed_work(&data->dwork, msecs_to_jiffies(INT_POLLING_DELAY));	
				spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);	
	
				stmvl6180_set_enable(client, 1); /* Power On */	 
			} else {
				pr_warn("%s(), ioctl INIT, do nothing\n", __func__);
			}
		

		}
		return 0;
	}
	case VL6180_IOCTL_XTALKCALB: 	/*crosstalk calibration*/
	/* This is adapt to meizu factory */
	{
		void __user *p_u4Param = (void __user *)p;

		int i=0;
		int RangeSum =0;
		int RateSum = 0;
		int XtalkInt =0;

		VL6180x_RangeData_t Range;

		spin_lock(&g_Laser_SpinLock);
		g_s4Laser_Opened = 3;
		spin_unlock(&g_Laser_SpinLock);

		VL6180x_SystemInit(3, 1, VL6180_XTALK_CALIB); 
		usleep_range(10000, 11000);

		for ( i = 0; i < N_MEASURE_AVG; ) {
			if ( VL6180x_GetRangeValue(&Range) ) {
				RangeSum += Range.range_mm;
				RateSum += Range.signalRate_mcps;
				LOG_INF("VL6180 XTalk Calibration: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
				i++;
			}
			usleep_range(10000, 11000);
		}

		XtalkInt = ( RateSum * ( N_MEASURE_AVG * XTALK_CALIB_TARGET_DISTANCE - RangeSum ) ) /( N_MEASURE_AVG * XTALK_CALIB_TARGET_DISTANCE * N_MEASURE_AVG) ;

		g_Laser_XTalkCalib = XtalkInt;

		// TODO:	If g_Laser_XTalkCalib is negative,	laser don't get range.
		if( g_Laser_XTalkCalib < 0 ) {
			rc = -1;
			g_Laser_XTalkCalib = 0xFFFFFFFF;
		}
		LOG_INF("stmvl6180 XTalk calibration data:%d.\n", g_Laser_XTalkCalib);
		#ifdef MZ_LASER_SAVE_CALI
		laser_cal_data[1] = g_Laser_XTalkCalib;
		laser_data_write_emmc(laser_cal_data, sizeof(laser_cal_data));
		#endif

		//VL6180x_SetXTalkCompensationRate(vl6180x_dev, g_Laser_XTalkCalib);
		VL6180x_SystemInit(3, 1, 0);

		LOG_INF("VL6180 XTalk Calibration: End\n");

		spin_lock(&g_Laser_SpinLock);
		g_s4Laser_Opened = 2;
		spin_unlock(&g_Laser_SpinLock);

		if(copy_to_user(p_u4Param , &XtalkInt , sizeof(int))) {
			LOG_INF("copy to user failed when getting VL6180_IOCTL_GETOFFCALB \n");
		}
	}
	break;

	case VL6180_IOCTL_SETXTALK:
	{
		#ifndef MZ_LAER_FACTORY_TEST
		client = i2c_getclient();
		if (client)
		{
			unsigned int xtalkint=0;
			#ifndef COMPLY_WITH_MEIZU
			struct stmvl6180_data *data = i2c_get_clientdata(client);
			#endif
			if (copy_from_user(&xtalkint, (unsigned int *)p, sizeof(unsigned int))) {
				rc = -EFAULT;
			}
			printk("ioctl SETXTALK as 0x%x\n", xtalkint);
#ifdef CALIBRATION_FILE
			xtalk_calib = xtalkint;
			stmvl6180_write_xtalk_calibration_file();
#endif
			VL6180x_SetXTalkCompensationRate(vl6180x_dev, xtalkint);

		}
		#else
		pr_info("%s(), do nothing, see ioctl get xtal\n", __func__);
		#endif
		return 0;
	}
	case VL6180_IOCTL_OFFCALB: 	/*offset calibration*/
	{
		/* This is adapt to meizu factory */
		void __user *p_u4Param = (void __user *)p;

		int i = 0;
		int RangeSum =0,RangeAvg=0;
		int OffsetInt =0;
		VL6180x_RangeData_t Range;

		spin_lock(&g_Laser_SpinLock);
		g_s4Laser_Opened = 3;
		spin_unlock(&g_Laser_SpinLock);

		VL6180x_SystemInit(3, 1, VL6180_OFFSET_CALIB);	
		usleep_range(10000, 11000);

		for ( i = 0; i < N_MEASURE_AVG; ) {
			if ( VL6180x_GetRangeValue(&Range) ) {
				LOG_INF("VL6180 Offset Calibration Orignal: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
				i++;
				RangeSum += Range.range_mm;
			}
			usleep_range(10000, 11000);
		}

		RangeAvg = RangeSum / N_MEASURE_AVG;

		if ( RangeAvg >= ( OFFSET_CALIB_TARGET_DISTANCE - 3 ) && RangeAvg <= ( OFFSET_CALIB_TARGET_DISTANCE + 3) ) {
			LOG_INF("VL6180 Offset Calibration: Original offset is OK, finish offset calibration\n");
		} else {	
			LOG_INF("VL6180 Offset Calibration: Start offset calibration\n");

			VL6180x_SystemInit(1, 0, VL6180_OFFSET_CALIB);	
			usleep_range(10000, 11000);

			RangeSum = 0;

			for ( i = 0; i < N_MEASURE_AVG; ) {
				if ( VL6180x_GetRangeValue(&Range) ) {
					LOG_INF("VL6180 Offset Calibration: %d - RV[%d] - SR[%d]\n", i, Range.range_mm, Range.signalRate_mcps);
					i++;
					RangeSum += Range.range_mm;
				}
				usleep_range(10000, 11000);
			}

			RangeAvg = RangeSum / N_MEASURE_AVG;

			LOG_INF("VL6180 Offset Calibration: Get the average Range as %d\n", RangeAvg);

			OffsetInt = OFFSET_CALIB_TARGET_DISTANCE - RangeAvg;

			LOG_INF("VL6180 Offset Calibration: Set the offset value(pre-scaling) as %d\n", OffsetInt);

			if( ABS(OffsetInt) > 127 ) // offset value : ~128 ~ 127
			{
				OffsetInt = 0xFFFFFFFF;
				rc = -1;
			}

			g_Laser_OffsetCalib = OffsetInt;
			//VL6180x_SetOffset(vl6180x_dev, OffsetInt);	
			VL6180x_SystemInit(3, 1, 0);

			LOG_INF("VL6180 Offset Calibration: End\n");
		}	

		LOG_INF("stmvl6180 offset calibration data:%d.\n", g_Laser_OffsetCalib);
		#ifdef MZ_LASER_SAVE_CALI
		laser_cal_data[0] = g_Laser_OffsetCalib;
		laser_data_write_emmc(laser_cal_data, sizeof(laser_cal_data));
		#endif

		spin_lock(&g_Laser_SpinLock);
		g_s4Laser_Opened = 2;
		spin_unlock(&g_Laser_SpinLock);

		if(copy_to_user(p_u4Param , &OffsetInt , sizeof(int))) {
			LOG_INF("copy to user failed when getting VL6180_IOCTL_GETOFFCALB \n");
		}
	}
	break;

	case VL6180_IOCTL_SETOFFSET:
	{
		#ifndef MZ_LAER_FACTORY_TEST
		client = i2c_getclient();
		if (client)
		{
			int8_t offsetint=0;
			#ifndef COMPLY_WITH_MEIZU
			int8_t scaling;
			struct stmvl6180_data *data = i2c_get_clientdata(client);
			#endif
			if (copy_from_user(&offsetint, (int8_t *)p, sizeof(int8_t))) {
				rc = -EFAULT;
			}
			printk("ioctl SETOFFSET as %d\n", offsetint);
#ifdef CALIBRATION_FILE
			offset_calib = offsetint;
			stmvl6180_write_offset_calibration_file();
#endif
			#ifdef CALIBRATE_CONFIG
			VL6180x_SetOffset(vl6180x_dev,offsetint);
			#endif
		}
		#else
		pr_info("%s(), do nothing, see ioctl get offset\n", __func__);
		#endif
		return 0;
	}
	case VL6180_IOCTL_STOP:
	{
		client = i2c_getclient();
		if (client)
		{
			struct stmvl6180_data *data = i2c_get_clientdata(client);
			//turn off p sensor only if it's enabled by other client
			if (data->enable_ps_sensor==1) {

				//turn off p sensor 
				data->enable_ps_sensor = 0;
				if (data->ps_is_singleshot == 0)
					VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP);
				VL6180x_RangeClearInterrupt(vl6180x_dev);

				stmvl6180_set_enable(client, 0);

				spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
				/*
		 		* If work is already scheduled then subsequent schedules will not
		 		* change the scheduled time that's why we have to cancel it first.
		 		*/
				#ifndef COMPLY_WITH_MEIZU
				__cancel_delayed_work(&data->dwork);
				#else
				cancel_delayed_work(&data->dwork);
				#endif
				spin_unlock_irqrestore(&data->update_lock.wait_lock, flags); 
			} else {
				pr_warn("%s(), ioctl STOP, do nothing\n", __func__);
			}
		}
		return 0;
	}
	case VL6180_IOCTL_GETDATA:	  /* Get proximity value only */
	{
		client = i2c_getclient();
		if (client)
		{
			struct stmvl6180_data *data = i2c_get_clientdata(client);
			distance = data->rangeData.FilteredData.range_mm;	
		}
		//printk("vl6180_getDistance return %ld\n",distance);
		return put_user(distance, (unsigned long *)p);
		
	}
	case VL6180_IOCTL_GETDATAS:	 /* Get all range data */
	{
		client = i2c_getclient();
		if (client)
		{
			struct stmvl6180_data *data = i2c_get_clientdata(client);
			//printk("IOCTL_GETDATAS, m_range_mm:%d===\n",data->rangeData.m_range_mm);
			if (copy_to_user((VL6180x_RangeData_t *)p, &(data->rangeData), sizeof(VL6180x_RangeData_t))) {
				rc = -EFAULT;
			}	 
		}
		else
			return -EFAULT;

		return rc;   
	}
	default:
		return -EINVAL;
	}
	return rc;
}

static int stmvl6180_open(struct inode *inode, struct file *file)
{

	/* power on*/
	struct i2c_client *client;
	client = i2c_getclient();

	if (IS_ERR_OR_NULL(client))	{
		pr_err("%s(), err, client is invalid: %p\n",
			__func__, client);
		return -ENODEV;
	}

	if( g_s4Laser_Opened )
	{
		LOG_INF("The device is opened \n");
		return -EBUSY;
	}

	spin_lock(&g_Laser_SpinLock);
	g_s4Laser_Opened = 1;
	offset_init = 0;
	spin_unlock(&g_Laser_SpinLock);

	mz_ranging_power_enable(client, true);

	return 0;
}

#ifndef COMPLY_WITH_MEIZU
static int stmvl6180_flush(struct inode *inode, struct file *file)
#else
static int stmvl6180_flush(struct file *file, fl_owner_t id)
#endif
{
 	unsigned long flags;
	struct i2c_client *client;
	client = i2c_getclient();

	if (client)
	{
		struct stmvl6180_data *data = i2c_get_clientdata(client);
		if (data->enable_ps_sensor==1) 
		{
			//turn off p sensor if it's enabled
			data->enable_ps_sensor = 0;
			VL6180x_RangeClearInterrupt(vl6180x_dev);
			
			stmvl6180_set_enable(client, 0);

			spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
			/*
		 	* If work is already scheduled then subsequent schedules will not
		 	* change the scheduled time that's why we have to cancel it first.
		 	*/
			#ifndef COMPLY_WITH_MEIZU
			__cancel_delayed_work(&data->dwork);
			#else
			cancel_delayed_work(&data->dwork);
			#endif
			spin_unlock_irqrestore(&data->update_lock.wait_lock, flags); 

		}

		mz_ranging_power_enable(client, false);

		if (g_s4Laser_Opened)
		{
			LOG_INF("Free \n");

			spin_lock(&g_Laser_SpinLock);
			g_s4Laser_Opened = 0;
			offset_init = 0;
			spin_unlock(&g_Laser_SpinLock);
		}
	}

	return 0;
}
static long stmvl6180_ioctl(struct file *file, 
				unsigned int cmd, unsigned long arg)
{
	int ret;
	mutex_lock(&vl6180_mutex);
	ret = stmvl6180_ioctl_handler(file, cmd, arg, (void __user *)arg);
	mutex_unlock(&vl6180_mutex);

	return ret;
}


/*
 * Initialization function
 */
static int stmvl6180_init_client(struct i2c_client *client)
{
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	uint8_t id=0,module_major=0,module_minor=0;
	uint8_t model_major=0,model_minor=0;
	uint8_t i=0,val;

	// Read Model ID
	VL6180x_RdByte(vl6180x_dev, VL6180_MODEL_ID_REG, &id);
	printk("read MODLE_ID: 0x%x, i2cAddr:0x%x\n",id,client->addr);
	if (id == 0xb4) {
		printk("STM VL6180 Found\n");
	}
	else if (id==0){
		printk("Not found STM VL6180\n");
		return -EIO;
	}

	// Read Model Version
	VL6180x_RdByte(vl6180x_dev, VL6180_MODEL_REV_MAJOR_REG, &model_major);
	model_major &= 0x07;
	VL6180x_RdByte(vl6180x_dev, VL6180_MODEL_REV_MINOR_REG, &model_minor);
	model_minor &= 0x07;
	printk("STM VL6180 Model Version : %d.%d\n", model_major,model_minor);

	// Read Module Version
	VL6180x_RdByte(vl6180x_dev, VL6180_MODULE_REV_MAJOR_REG, &module_major);
	VL6180x_RdByte(vl6180x_dev, VL6180_MODULE_REV_MINOR_REG, &module_minor);
	printk("STM VL6180 Module Version : %d.%d\n",module_major,module_minor);
	
	// Read Identification 
	printk("STM VL6180 Serial Numbe: ");
	for (i=0; i<=(VL6180_FIRMWARE_REVISION_ID_REG-VL6180_REVISION_ID_REG);i++)
	{
		VL6180x_RdByte(vl6180x_dev, (VL6180_REVISION_ID_REG+i), &val);
		printk("0x%x-",val);
	}
	printk("\n");
	

	data->ps_data=0;			
	data->enableDebug=0;
#ifdef CALIBRATION_FILE
	stmvl6180_read_calibration_file();
#endif
	
	//VL6180 Initialization
	VL6180x_WaitDeviceBooted(vl6180x_dev);
	VL6180x_InitData(vl6180x_dev);
	//VL6180x_FilterSetState(vl6180x_dev, 1); /* activate wrap around filter */
	//VL6180x_DisableGPIOxOut(vl6180x_dev, 1); /* diable gpio 1 output, not needed when polling */

	return 0;
}

#ifdef MZ_LAER_FACTORY_TEST
static u8 flight_mode = 0;

static ssize_t flight_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	char *p = buf;
	VL6180x_RangeData_t Range;

	switch (flight_mode) {
	case 1:
		VL6180x_GetRangeValue(&Range);
		p += sprintf(p, "[RAW]test get RAW distance(mm): %d\n", Range.FilteredData.rawRange_mm);
		p += sprintf(p, "test get distance(mm): %d\n", Range.range_mm);
		p += sprintf(p, "return signal rate: %d\n", Range.signalRate_mcps);
		if (Range.signalRate_mcps < 26)
			p += sprintf(p, "[PASS]Laser assemble ok!\n");
		else
			p += sprintf(p, "[NO PASS]Laser assemble not ok!\n");

		msleep(50);

		return (p - buf);
	case 2:
		while(1) {
			VL6180x_GetRangeValue(&Range);
			if (flight_mode == 5)
				break;
		};
		break;
	default:
		break;
	}
	return sprintf(buf, "error flight mode!\n");
}

static ssize_t flight_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int val;
	struct i2c_client *client = to_i2c_client(dev);

	sscanf(buf, "%d", &val);
	switch (val) {
	case 1:
		mz_ranging_power_enable(client, true);
		break;
	case 2:
		VL6180x_SystemInit(3, 1, 0);
		flight_mode = 1;
		msleep(50);
		break;
	case 3:
		VL6180x_SystemInit(3, 1, 0);
		flight_mode = 2;
		msleep(50);
		break;
	case 4:
		mz_ranging_power_enable(client, false);
		flight_mode = 0;
		break;
	default:
		flight_mode = 5;
		break;
	}

	return count;
}

static struct device_attribute dev_attr_ctrl = {
	.attr = {.name = "flight_ctrl", .mode = 0644},
	.show = flight_show,
	.store = flight_store,
};
#endif

/*
 * I2C init/probing/exit functions
 */

static struct i2c_driver stmvl6180_driver;
static int __devinit stmvl6180_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct stmvl6180_data *data;
	int err = 0;
#ifdef USE_INT
	int irq = 0;
#endif
	printk("stmvl6180_probe==========\n");
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct stmvl6180_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	data->client = client;
	i2c_set_clientdata(client, data);

	data->enable = 0;		/* default mode is standard */
	printk("enable = %x\n", data->enable);

	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);
	mutex_init(&vl6180_mutex);

	// setup platform i2c client
	i2c_setclient(client);
	//vl6180x_dev.I2cAddress = client->addr;

	err = mz_ranging_parse_dt(client);
	if (err) {
		dev_err(&client->dev, "%s(), parse dt failed:%d\n",
			__func__, err);
		goto exit_kfree;
	}
	//interrupt set up
#ifdef USE_INT
	gpio_request(IRQ_NUM,"vl6180_gpio_int");
	gpio_direction_input(IRQ_NUM);
	irq = gpio_to_irq(IRQ_NUM);
	if (irq < 0)
	{
		pr_err("filed to map GPIO :%d to interrupt:%d\n",IRQ_NUM,irq);
	}
	else
	{
		int result;
		vl6180_dbgmsg("register_irq:%d\n",irq);
		if ((result = request_threaded_irq(irq, NULL, stmvl6180_interrupt_handler, IRQF_TRIGGER_RISING, //IRQF_TRIGGER_FALLING- poliarity:0 IRQF_TRIGGER_RISNG - poliarty:1
			"vl6180_lb_gpio_int", (void *)client))) 
		{
			pr_err("%s Could not allocate STMVL6180_INT ! result:%d\n", __func__,result);
	
			goto exit_kfree;
		}
	}	
	//disable_irq(irq);
	data->irq = irq;
	vl6180_dbgmsg("%s interrupt is hooked\n", __func__);
#endif
	
	INIT_DELAYED_WORK(&data->dwork, stmvl6180_work_handler);

	mz_ranging_power_enable(client, true);

	/* Initialize the STM VL6180 chip */
	err = stmvl6180_init_client(client);
	if (err)
		goto exit_kfree;

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		pr_err("%s Failed to allocate input device ps\n",__func__);
		goto exit_free_dev_ps;
	}
	
	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 76, 0, 0); //range in cm 
	input_set_abs_params(data->input_dev_ps, ABS_HAT0X, 0, 765, 0, 0); //range in_mm
	input_set_abs_params(data->input_dev_ps, ABS_X, 0, 65535, 0, 0); //rtnRate

	data->input_dev_ps->name = "STM VL6180 proximity sensor";


	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		pr_err("%sUnable to register input device ps: %s\n",__func__, data->input_dev_ps->name);
		goto exit_unregister_dev_ps;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &stmvl6180_attr_group);
	if (err)
	{
		pr_err("%sUnable to create sysfs group\n",__func__);
		goto exit_unregister_dev_ps;
	}

	mz_ranging_power_enable(client, false);
	#ifdef MZ_LAER_FACTORY_TEST
	spin_lock_init(&g_Laser_SpinLock);
	device_create_file(&client->dev, &dev_attr_ctrl);
	meizu_sysfslink_register(&client->dev, "laser");
	#endif
	printk("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);

	return 0;

exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);	
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
#ifndef COMPLY_WITH_MEIZU
exit_free_irq:
#endif
#ifdef USE_INT
	free_irq(irq, client);
#endif
exit_kfree:
	kfree(data);
exit:
	return err;
}

static int __devexit stmvl6180_remove(struct i2c_client *client)
{
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	
	//input_unregister_device(data->input_dev_als);
	input_unregister_device(data->input_dev_ps);
	
	//input_free_device(data->input_dev_als);
	input_free_device(data->input_dev_ps);

#ifdef  USE_INT
	free_irq(data->irq, client);
#endif

	sysfs_remove_group(&client->dev.kobj, &stmvl6180_attr_group);

	/* Power down the device */
	stmvl6180_set_enable(client, 0);

	kfree(data);

	return 0;
}

static const struct i2c_device_id stmvl6180_id[] = {
	{ STMVL6180_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, stmvl6180_id);

static const struct file_operations stmvl6180_ranging_fops = {
		.owner =			THIS_MODULE,
		.unlocked_ioctl =	stmvl6180_ioctl,
		.open =				stmvl6180_open,
		.flush = 			stmvl6180_flush,
};

static struct miscdevice stmvl6180_ranging_dev = {
		.minor =	MISC_DYNAMIC_MINOR,
		.name =		"stmvl6180_ranging",
		.fops =		&stmvl6180_ranging_fops
};

#ifdef CONFIG_OF
static struct of_device_id mz_ranging_of_match_table[] = {
	{
		.compatible = "meizu,st-vl6180",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mz_ranging_of_match_table);
#else
#define mz_ranging_of_match_table NULL
#endif

static const struct dev_pm_ops mz_ranging_pmic_pm = {
	.suspend = mz_ranging_suspend,
	.resume = mz_ranging_resume,
};

static struct i2c_driver stmvl6180_driver = {
	.driver = {
		.name	= STMVL6180_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = mz_ranging_of_match_table,
		.pm = &mz_ranging_pmic_pm,
	},
	.probe	= stmvl6180_probe,
	.remove	= __devexit_p(stmvl6180_remove),
	.id_table = stmvl6180_id,

};

static int __init stmvl6180_init(void)
{	
	int ret=0;
	printk("stmvl6180_init===\n");
	//to register as a misc device
	ret = misc_register(&stmvl6180_ranging_dev);
	if (ret) {
		pr_err("%s(), misc_register() failed:%d\n",
			__func__, ret);
		return ret;
	}

	ret = i2c_add_driver(&stmvl6180_driver);
	if (ret) {
		pr_err("%s(), i2c_add_driver() failed:%d\n",
			__func__, ret);
		return ret;
	}

	return 0;	
}

static void __exit stmvl6180_exit(void)
{
	printk("stmvl6180_exit===\n");
	i2c_del_driver(&stmvl6180_driver);
	misc_deregister(&stmvl6180_ranging_dev);
}

MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(stmvl6180_init);
module_exit(stmvl6180_exit);

