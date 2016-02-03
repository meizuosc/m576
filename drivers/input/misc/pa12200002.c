#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/ctype.h>
#include <linux/hwmon-sysfs.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/input/pa12200002.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/meizu-sys.h>
#include "./../../base/base.h"


//#define	SUNLIGHT_ALS_USED

#define	LINK_KOBJ_NAME	"ps" // make soft link to /meizu/ps/

struct txc_data *txc_info = NULL;
static void txc_ps_handler(struct work_struct *work);

// I2C read one byte data from register 
static int i2c_read_reg(struct i2c_client *client,u8 reg,u8 *data)
{
  	u8 databuf[2]; 
	int res = 0;
	databuf[0]= reg;
	
	mutex_lock(&txc_info->i2c_lock);
	res = i2c_master_send(client,databuf,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err  reg = %d\n",reg);
		mutex_unlock(&txc_info->i2c_lock);
		return res;
	}
	res = i2c_master_recv(client,data,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_recv function err  reg = %d\n",reg);
		mutex_unlock(&txc_info->i2c_lock);
		return res;
	}

	mutex_unlock(&txc_info->i2c_lock);
	return 0;
}
// I2C Write one byte data to register
static int i2c_write_reg(struct i2c_client *client,u8 reg,u8 value)
{
	u8 databuf[2];    
	int res = 0;
	databuf[0] = reg;   
	databuf[1] = value;

	mutex_lock(&txc_info->i2c_lock);
	res = i2c_master_send(client,databuf,0x2);
	if (res < 0){
		APS_ERR("i2c_master_send function err  reg = %d, value = %d\n",reg,value);
		mutex_unlock(&txc_info->i2c_lock);
		return res;
	}
	mutex_unlock(&txc_info->i2c_lock);
	return 0;
}

static int pa122_init(struct txc_data *data)
{
	int ret = 0;
	u8 sendvalue=0;
	struct i2c_client *client = data->client;
	data->pa122_sys_run_cal = 1;
	data->ps_calibvalue = PA12_PS_OFFSET_EXTRA;//PA12_PS_OFFSET_DEFAULT + 
	data->fast_calib_flag = 0;
	data->ps_data = PS_UNKONW;
	data->ps_resumed = 1;
	data->resume_report = 0;
	data->mobile_wakeup = 0;
	data->ps_enable = 0;
	data->nontype_enable = 0;
	data->ps_shutdown = 0;

#ifdef	SUNLIGHT_ALS_USED
	/* Sun light */
	ret = i2c_write_reg(client,REG_CFG0, PA12_ALS_GAIN4000);
#endif
	/*  reg 11 */

	ret = i2c_write_reg(client,REG_PS_SET, 0x03); //PSET, Normal Mode
	if(ret < 0){	
		APS_ERR(" write ps set para,i2c_send function err\n");
		return ret;
	}

	/*Hysteresis type   &  ps sleep time 12.5ms*/
	sendvalue= PA12_PS_INT_HYSTERESIS | PA12_PS_PERIOD12;
	ret=i2c_write_reg(client,REG_CFG3,sendvalue);

	// Set PS threshold
	sendvalue=PA12_PS_FAR_TH_HIGH;	
	ret=i2c_write_reg(client,REG_PS_TH,sendvalue); //set TH threshold
	    
	sendvalue=PA12_PS_NEAR_TH_LOW;	
	ret=i2c_write_reg(client,REG_PS_TL,sendvalue); //set TL threshold
      
	ret = i2c_write_reg(client, REG_PS_OFFSET, PA12_PS_OFFSET_DEFAULT);

	return ret;
}

#ifdef	SUNLIGHT_ALS_USED
static int pa122_set_ps_mode(struct i2c_client *client)
{
	u8 sendvalue=0;
	int res = 0;
  
	if (txc_info->ps_enable) {
		sendvalue= PA12_LED_CURR100 | PA12_PS_PRST4 | PA12_ALS_PRST4;
		res=i2c_write_reg(client,REG_CFG1,sendvalue);

		// Interrupt Setting	 
		res=i2c_write_reg(client,REG_CFG2, (PA12_INT_ALS_PS_BOTH| PA12_PS_MODE_NORMAL
			| PA12_PS_INTF_INACTIVE | PA12_ALS_INTF_INACTIVE)); //set int mode

		/* Sun Light */
		res = i2c_write_reg(client, REG_ALS_TH_LSB, PA12_ALS_TH_LSB_SUN_LIGHT_ON);
		res = i2c_write_reg(client, REG_ALS_TH_MSB, PA12_ALS_TH_MSB_SUN_LIGHT_ON);
		res = i2c_write_reg(client, REG_ALS_TL_LSB, PA12_ALS_TL_LSB_SUN_LIGHT_ON);
		res = i2c_write_reg(client, REG_ALS_TL_MSB, PA12_ALS_TL_MSB_SUN_LIGHT_ON);
	} else {
		sendvalue= PA12_LED_CURR100 | PA12_PS_PRST4;
		res=i2c_write_reg(client,REG_CFG1,sendvalue);

		// Interrupt Setting	 
		res=i2c_write_reg(client,REG_CFG2, (PA12_INT_PS | PA12_PS_MODE_NORMAL
			| PA12_PS_INTF_INACTIVE)); //set int mode
	}
	return 0 ;
}


#else

static int pa122_set_ps_mode(struct i2c_client *client)
{
	u8 sendvalue=0, regdata = 0;
	int res = 0;
  
	sendvalue= PA12_LED_CURR100 | PA12_PS_PRST4;
	res=i2c_write_reg(client,REG_CFG1,sendvalue);

    /* Prevent interrupt form ALS, because ALS in on to prevent ps data being non-stable under specific angle in outdoor place */
    res = i2c_write_reg(client, REG_ALS_TH_LSB, 0xFF);
    res = i2c_write_reg(client, REG_ALS_TH_MSB, 0xFF);
    res = i2c_write_reg(client, REG_ALS_TL_LSB, 0x00);
    res = i2c_write_reg(client, REG_ALS_TL_MSB, 0x00);

	// Interrupt Setting	   
	res = i2c_read_reg(client, REG_CFG2, &regdata);
	sendvalue = (regdata & 0x03) | PA12_INT_PS | PA12_PS_MODE_NORMAL;
	res=i2c_write_reg(client,REG_CFG2, sendvalue); //set int mode

	return 0 ;
}

#endif


//PS enable function
int pa122_enable_ps(struct i2c_client *client, int enable)
{
	int res;
	u8 regdata=0;
	u8 sendvalue=0;

	if(enable == 1){ //PS ON
		ps_log("pa12201001 enable ps sensor\n");
		res=i2c_read_reg(client,REG_CFG0,&regdata); //Read Status
		if(res<0){
			APS_ERR("i2c_read function err\n");
			return res;
		}else{
			sendvalue=regdata & 0xFC; //clear bit-0 & bit-1
			sendvalue=sendvalue | 0x03; //0x02 PS On & ALS On to prevent strong IR causing ps data being non-stable under specific angle in outdoor place
		
			res=i2c_write_reg(client,REG_CFG0,sendvalue); //Write PS enable 
			if(res<0){
				APS_ERR("i2c_write function err\n");
				return res;
			}	  		 	
			res=i2c_read_reg(client,REG_CFG0,&regdata); //Read Status
			ps_log("CFG0 Status: %d\n",regdata);
		}
	}else{       //PS OFF	
		ps_log("pa12201001 disaple ps sensor\n");
		res=i2c_read_reg(client,REG_CFG0,&regdata); //Read Status
		if(res<0){
			APS_ERR("i2c_read function err\n");
			return res;
		}else{
			ps_log("CFG0 Status: %d\n",regdata);
			sendvalue=regdata & 0xFC; //clear bit-0 & bit-1
			res=i2c_write_reg(client,REG_CFG0,sendvalue); //Write PS disable 
			if(res<0){
				APS_ERR("i2c_write function err\n");
				return res;
			}	  	
			/*clear the ps interrupt statue*/
			res = i2c_read_reg(client, REG_CFG2, &regdata);
			if (res < 0) {
	    		pr_err("%s: txc_read error\n", __func__);
			}
			regdata &= 0xfc;
			res = i2c_write_reg(client, REG_CFG2, regdata);
			if (res < 0) {
	    		pr_err("%s: txc_write error\n", __func__);
			}
		}
	}
	return 0;
} 

//Read PS Count : 8 bit
int pa122_read_ps(struct i2c_client *client, u8 *data)
{
   int res;
	
   res = i2c_read_reg(client,REG_PS_DATA,data); //Read PS Data
   if(res < 0){
        APS_ERR("i2c_send function err\n");
   }
   return res;
}

//Read ALS Count : 16 bit
int pa122_read_als(struct i2c_client *client, u16 *data)
{
   int res;
   u8 LSB = 0;
   u8 MSB = 0;
	
    res = i2c_read_reg(client, REG_ALS_DATA_LSB, &LSB); //Read PS Data
    res = i2c_read_reg(client, REG_ALS_DATA_MSB, &MSB); //Read PS Data
    *data = (MSB << 8) | LSB; 
   if(res < 0){
        APS_ERR("i2c_send function err\n");
   }
   return res;
}

void pa12_swap(u8 *x, u8 *y)
{
        u8 temp = *x;
        *x = *y;
        *y = temp;
}
static int pa122_run_fast_calibration(struct i2c_client *client)
{

	struct txc_data *data = i2c_get_clientdata(client);
	int i = 0;
	int j = 0;	
	u16 sum_of_pdata = 0;
	u8  xtalk_temp = 0;
    	u8 temp_pdata[4], cfg0data = 0,cfg2data = 0,cfg3data = 0;
   	unsigned int ArySize = 4;
	
   	ps_log("START proximity sensor calibration\n");

	i2c_write_reg(client, REG_PS_TH, 0xFF);
	i2c_write_reg(client, REG_PS_TL, 0);

	i2c_read_reg(client, REG_CFG0, &cfg0data);
	i2c_read_reg(client, REG_CFG2, &cfg2data);
	i2c_read_reg(client, REG_CFG3, &cfg3data);

	/*Offset mode & disable intr from ps*/
	i2c_write_reg(client, REG_CFG2, cfg2data & 0x33); 	
	
	/*PS sleep time 6.5ms */
	i2c_write_reg(client, REG_CFG3, cfg3data & 0xC7); 	

	/*Set crosstalk = 0*/
	i2c_write_reg(client, REG_PS_OFFSET, 0x00); 

	/*PS On*/
	i2c_write_reg(client, REG_CFG0, cfg0data | 0x03); 
	usleep_range(50 * 1000, 50 * 1000);

	for(i = 0; i < 4; i++)
	{
		usleep_range(7000, 8000);
		i2c_read_reg(client,REG_PS_DATA,temp_pdata+i);
		//ps_log("temp_data = %d\n", temp_pdata[i]);	
	}	
	
	/* pdata sorting */
	for (i = 0; i < ArySize - 1; i++)
		for (j = i+1; j < ArySize; j++)
			if (temp_pdata[i] > temp_pdata[j])
				pa12_swap(temp_pdata + i, temp_pdata + j);	
	
	/* calculate the cross-talk using central 2 data */
	for (i = 1; i < 3; i++) 
	{
		ps_log("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}

	xtalk_temp = sum_of_pdata/2;
   	ps_log("%s: sum_of_pdata = %d   calibvalue = %d\n",
                        __func__, sum_of_pdata, xtalk_temp);
	
	/* Restore Data */
	i2c_write_reg(client, REG_CFG0, cfg0data);
	i2c_write_reg(client, REG_CFG2, cfg2data | 0xC0); //make sure return normal mode
	i2c_write_reg(client, REG_CFG3, cfg3data);

	i2c_write_reg(client, REG_PS_TH, PA12_PS_FAR_TH_HIGH);
	i2c_write_reg(client, REG_PS_TL, PA12_PS_NEAR_TH_LOW);

	//if (((xtalk_temp + PA12_PS_OFFSET_EXTRA) > data->factory_calibvalue) && (xtalk_temp < PA12_PS_OFFSET_MAX))
	if (((xtalk_temp + PA12_PS_OFFSET_EXTRA) > data->factory_calibvalue) && (xtalk_temp < (data->factory_calibvalue + 10)))
	{ 	
		ps_log("Fast calibrated data=%d\n",xtalk_temp);
		data->fast_calib_flag = 1;
		data->fast_calibvalue = xtalk_temp + PA12_PS_OFFSET_EXTRA;
		/* Write offset value to 0x10 */
		i2c_write_reg(client, REG_PS_OFFSET, xtalk_temp + PA12_PS_OFFSET_EXTRA);
		return xtalk_temp + PA12_PS_OFFSET_EXTRA;
	}
	else
	{
		ps_log("Fast calibration fail, calibvalue=%d, data->factory_calibvalue = %d\n",xtalk_temp, data->factory_calibvalue);
		data->fast_calib_flag = 0;
		i2c_write_reg(client, REG_PS_OFFSET, data->factory_calibvalue);
		xtalk_temp = data->factory_calibvalue;
		
		return xtalk_temp;
	}
}
static int pa122_run_calibration(struct txc_data *data)
{
    	struct i2c_client *client = data->client;
	int i, j;	
	int ret;
	u16 sum_of_pdata = 0;
	u8 temp_pdata[20],cfg0data=0,cfg2data=0;
	unsigned int ArySize = 20;
	unsigned int cal_check_flag = 0;	
	//u8 value=0;
	int calibvalue;

	ps_log("%s: START proximity sensor calibration\n", __func__);

	i2c_write_reg(client, REG_PS_TH, 0xFF);
	i2c_write_reg(client, REG_PS_TL, 0);

RECALIBRATION:
	sum_of_pdata = 0;

	ret = i2c_read_reg(client, REG_CFG0, &cfg0data);
	ret = i2c_read_reg(client, REG_CFG2, &cfg2data);
	
	/*Set to offset mode & disable interrupt from ps*/
	ret = i2c_write_reg(client, REG_CFG2, cfg2data & 0x33); 

	/*Set crosstalk = 0*/
	ret = i2c_write_reg(client, REG_PS_OFFSET, 0x00);
	if (ret < 0) {
	    pr_err("%s: txc_write error\n", __func__);
	    /* Restore CFG2 (Normal mode) and Measure base x-talk */
	    ret = i2c_write_reg(client, REG_CFG0, cfg0data);
	    ret = i2c_write_reg(client, REG_CFG2, cfg2data | 0xC0); 
	    return ret;
	}

	/*PS On*/
	ret = i2c_write_reg(client, REG_CFG0, cfg0data | 0x02); 
	usleep_range(50000, 50000);

	for(i = 0; i < 20; i++)
	{
		usleep_range(15000, 15000);
		ret = i2c_read_reg(client,REG_PS_DATA,temp_pdata+i);
		//ps_log("temp_data = %d\n", temp_pdata[i]);	
	}	
	
	/* pdata sorting */
	for (i = 0; i < ArySize - 1; i++)
		for (j = i+1; j < ArySize; j++)
			if (temp_pdata[i] > temp_pdata[j])
				pa12_swap(temp_pdata + i, temp_pdata + j);	
	
	/* calculate the cross-talk using central 10 data */
	for (i = 5; i < 15; i++) 
	{
		//ps_log("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}
	calibvalue = sum_of_pdata/10;

	/* Restore CFG2 (Normal mode) and Measure base x-talk */
	ret = i2c_write_reg(client, REG_CFG0, cfg0data);
	ret = i2c_write_reg(client, REG_CFG2, cfg2data | 0xC0); 

	i2c_write_reg(client, REG_PS_TH, PA12_PS_FAR_TH_HIGH);
	i2c_write_reg(client, REG_PS_TL, PA12_PS_NEAR_TH_LOW);

	if ((calibvalue  >= 0) && (calibvalue < PA12_PS_OFFSET_MAX)) {
		data->ps_calibvalue = calibvalue + PA12_PS_OFFSET_EXTRA;
	} else {
		ps_log("%s: invalid calibrated data, calibvalue %d\n", __func__, calibvalue);

		if(cal_check_flag == 0)
		{
			ps_log("%s: RECALIBRATION start\n", __func__);
			cal_check_flag = 1;
			goto RECALIBRATION;
		}else{
			ps_log("%s: CALIBRATION FAIL -> cross_talk is set to DEFAULT\n", __func__);

			ret = i2c_write_reg(client, REG_PS_OFFSET, data->factory_calibvalue);
			data->pa122_sys_run_cal = 0;
			return -EINVAL;
		}

	}

CROSSTALKBASE_RECALIBRATION:
	
	/*PS On*/
	//ret = i2c_write_reg(client, REG_CFG0, cfg0data | 0x02); 
	/*PS On ALS On to prevent strong IR causing ps data being non-stable under specific angle in outdoor place*/
	ret = i2c_write_reg(client, REG_CFG0, cfg0data | 0x03); 

	/*Write offset value to register 0x10*/
	ret = i2c_write_reg(client, REG_PS_OFFSET, data->ps_calibvalue);
	
	for(i = 0; i < 10; i++)
	{
		usleep_range(15000, 15000);
		ret = i2c_read_reg(client,REG_PS_DATA,temp_pdata+i);
		ps_log("temp_data = %d\n", temp_pdata[i]);	
	}	
 
     	/* pdata sorting */
	for (i = 0; i < 9; i++)
	{
	    for (j = i+1; j < 10; j++)
	    {
		if (temp_pdata[i] > temp_pdata[j])
			pa12_swap(temp_pdata + i, temp_pdata + j);   
	    }
	}

	/* calculate the cross-talk_base using central 5 data */
	sum_of_pdata = 0;

	for (i = 3; i < 8; i++) 
	{
		ps_log("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}
	
	data->calibvalue_base = sum_of_pdata/5;
    	ps_log("%s: sum_of_pdata = %d   cross_talk_base = %d\n",
                        __func__, sum_of_pdata, data->calibvalue_base);

	if(data->calibvalue_base > 0) 
	{
		if (data->calibvalue_base >= 8)
			data->ps_calibvalue += data->calibvalue_base/8;
		else
			data->ps_calibvalue += 1;
		goto CROSSTALKBASE_RECALIBRATION;
	} else if (data->calibvalue_base == 0) {
		data->factory_calibvalue = data->ps_calibvalue;
		data->pa122_sys_run_cal = 1;
	}

  	 /* Restore CFG0  */
	ret = i2c_write_reg(client, REG_CFG0, cfg0data);

	ps_log("%s: FINISH proximity sensor calibration, %d\n", __func__, data->ps_calibvalue);
	return ret;
}

static void txc_set_enable(struct txc_data *txc, int enable)
{
    struct i2c_client *client = txc->client;
    int ret = 0;
    
    mutex_lock(&txc->enable_lock);

    if (enable) {
	    pa122_set_ps_mode(client);
		ret = pa122_run_fast_calibration(client);
		ps_log("%s: run fast calivalue = %d\n", __func__, ret);
	} else {
		disable_irq(txc_info->irq);
		input_report_abs(txc->input_dev, ABS_DISTANCE, PS_UNKONW);
		input_sync(txc->input_dev);
	}
	ret = pa122_enable_ps(client, enable);
	if(enable){
		usleep_range(50000, 50000); //Must wait 50ms for ps data to refresh.
	}
	if (ret < 0) {
		APS_ERR("pa12201001_enable_ps function err\n");
	}
	mutex_unlock(&txc->enable_lock);
}


static int txc_create_input(struct txc_data *txc)
{
	int ret;
	struct input_dev *dev;

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s()->%d:can not alloc memory to txc input device!\n",__func__, __LINE__);
		return -ENOMEM;
	}

	set_bit(EV_ABS, dev->evbit);
	input_set_capability(dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(dev, ABS_DISTANCE, 0, 100, 0, 0);  /*the max value 1bit*/
	dev->name = "pa122";
	dev->dev.parent = &txc->client->dev;

	ret = input_register_device(dev);
	if (ret < 0) {
		pr_err("%s()->%d:can not register txc input device!\n",
			__func__, __LINE__);
		input_free_device(dev);
		return ret;
	}

	txc->input_dev = dev;
	input_set_drvdata(txc->input_dev, txc);

	return 0;
}


//======================sys interface
#if 0
#endif 
static ssize_t txc_ps_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	int enabled;
	enabled = txc_info->ps_enable;

	return sprintf(buf, "%d, %d\n", enabled, txc_info->factory_calibvalue);
}

static ssize_t txc_ps_enable_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	int enable, handle;
	//int enable = simple_strtol(buf, NULL, 10);
	sscanf(buf, "%d %d\n", &handle, &enable);

	ps_log("%s :enable = %d ,handle = %d, waketype = %d, nontyep = %d, ps_enable = %d\n", 
		__func__, enable, handle, txc_info->mobile_wakeup, txc_info->nontype_enable, txc_info->ps_enable);

	if((handle != 27)&&(handle != 3))
		return count;

	if(handle == 27){
		txc_info->mobile_wakeup = enable;
	}else{
		txc_info->nontype_enable = enable;
	}

	if((txc_info->mobile_wakeup || txc_info->nontype_enable) == txc_info->ps_enable){
		if(txc_info->mobile_wakeup && txc_info->nontype_enable){
			txc_info->ps_data = PS_UNKONW;
			input_report_abs(txc_info->input_dev, ABS_DISTANCE, PS_UNKONW);
			input_sync(txc_info->input_dev);
			txc_ps_handler(&txc_info->ps_dwork.work);
		}
		return count;
	}

	txc_info->ps_enable = (txc_info->mobile_wakeup || txc_info->nontype_enable);
	
	txc_info->ps_data = PS_UNKONW;
 
	txc_set_enable(txc_info, enable);

	if(enable){
		enable_irq(txc_info->irq);
	//}else{
	//	disable_irq(txc_info->irq);
	}
	return count;
}

static ssize_t txc_ps_data_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u8 ps_data;
	    
	ret = i2c_read_reg(client, REG_PS_DATA, &ps_data); //Read PS Data

	return sprintf(buf, "%d\n", ps_data);
}

static ssize_t txc_als_data_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u8 msb, lsb;
	u16 als_data;
	    
	ret = i2c_read_reg(client, REG_ALS_DATA_LSB, &lsb); 
	ret = i2c_read_reg(client, REG_ALS_DATA_MSB, &msb);

	als_data = (msb << 8) | lsb;

	return sprintf(buf, "%d\n", als_data);
}


static ssize_t pa122_reg_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 regdata;
	int res=0;
	int count=0;
	int i;

	for(i = 0;i <17 ;i++){
		res=i2c_read_reg(client,0x00+i,&regdata);
		if(res<0)
			break;
		else
			count+=sprintf(buf+count,"[%x] = (%x)\n",0x00+i,regdata);
	}
	return count;
}

static ssize_t pa122_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int addr, cmd;

	if(2 != sscanf(buf, "%x %x", &addr, &cmd)){
		APS_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	i2c_write_reg(client, addr, cmd);
	return count;
}

static ssize_t txc_calibration_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	int calibration = simple_strtol(buf, NULL, 10);
    
	if (calibration) {
		pa122_run_calibration(txc_info);
	}

	return count;
}

static ssize_t txc_calibvalue_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 regdata;
	int res = 0;
	if (!txc_info->pa122_sys_run_cal){
		res = -1;
	}else{
		i2c_read_reg(client, REG_PS_OFFSET, &regdata);
		res = regdata;
	}
	return sprintf(buf, "%d\n", res);
}

static ssize_t txc_calibvalue_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);

	txc_info->factory_calibvalue = simple_strtol(buf, NULL, 10);
	i2c_write_reg(client, REG_PS_OFFSET, txc_info->factory_calibvalue);
	ps_log("=============================the factory calibvalue init is %d\n",txc_info->factory_calibvalue);

	return count;
}

static ssize_t txc_batch_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
/*	
	int ps_batch = simple_strtol(buf, NULL, 10);

	if (ps_batch == 1) {
	    input_report_abs(txc_info->input_dev, ABS_DISTANCE, PS_UNKONW);
	    input_sync(txc_info->input_dev);
	}
*/
	return count;
}

static ssize_t txc_flush_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0; 
}

static ssize_t txc_flush_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
/*
	int sensors_id = 0;

	sscanf(buf, "%d\n", &sensors_id);
	ps_log("****txc_flush_set: buf = %d\n",sensors_id);

	input_report_abs(txc_info->input_dev, ABS_DISTANCE, sensors_id);
	input_sync(txc_info->input_dev);
*/
	return count;
}

/* the mobile_wakeup is use for enable the pa sensor irq can wakeup the system */
static ssize_t mobile_wakeup_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", txc_info->mobile_wakeup);
}

static ssize_t mobile_wakeup_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	txc_info->mobile_wakeup = simple_strtol(buf, NULL, 10);
	ps_log("%s: mobile_wakeup = %d\n", __func__, txc_info->mobile_wakeup);
	return count;
}

static ssize_t gpio_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gpio_get_value(txc_info->irq_gpio));
}

static ssize_t id_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 regdata;
	char *id = "no this ic";

	i2c_read_reg(client,0x7F,&regdata);
	if(0 == regdata){
		id = "pa122";
	}else if (0x11 == regdata){
		id = "pa224";
	}
	return snprintf(buf, 16, "%s\n",id);
}

/* sysfs attributes operation function*/
static DEVICE_ATTR(ps_enable, 0664, txc_ps_enable_show, txc_ps_enable_store);
static DEVICE_ATTR(ps_data, 0664, txc_ps_data_show, NULL);
static DEVICE_ATTR(als_data, 0664, txc_als_data_show, NULL);
static DEVICE_ATTR(ps_reg, 0664, pa122_reg_show, pa122_reg_store);
static DEVICE_ATTR(ps_calibration, 0664, NULL, txc_calibration_store);
static DEVICE_ATTR(ps_calibbias, 0664, txc_calibvalue_show, NULL);
static DEVICE_ATTR(ps_offset, 0664, NULL, txc_calibvalue_store);
static DEVICE_ATTR(ps_batch, 0664, NULL, txc_batch_store);
static DEVICE_ATTR(ps_flush, 0664, txc_flush_show, txc_flush_store);
static DEVICE_ATTR(ps_wakeup_enable, 0664, mobile_wakeup_show, mobile_wakeup_store);
static DEVICE_ATTR(ps_gpio, 0664, gpio_status_show, NULL);
static DEVICE_ATTR(ps_id, 0664, id_status_show, NULL);


static struct attribute *txc_attributes[] = {
	&dev_attr_ps_enable.attr,
	&dev_attr_ps_data.attr,
	&dev_attr_als_data.attr,
	&dev_attr_ps_reg.attr,
	&dev_attr_ps_calibration.attr,
	&dev_attr_ps_calibbias.attr,
	&dev_attr_ps_offset.attr,
	&dev_attr_ps_batch.attr,
	&dev_attr_ps_flush.attr,
	&dev_attr_ps_wakeup_enable.attr,
	&dev_attr_ps_gpio.attr,
	&dev_attr_ps_id.attr,
	NULL,
};

static struct attribute_group txc_attribute_group = {
	.attrs = txc_attributes,
};

static void txc_ps_handler(struct work_struct *work)
{
    struct txc_data *txc = container_of(work, struct txc_data, ps_dwork.work);
    struct i2c_client *client = txc_info->client;
    u8 psdata=0;
    //u16 alsdata = 0;
    int ps_data = PS_UNKONW;
    u8 sendvalue;
    int res;
    u8 data;
    int ret;
	
    wake_lock_timeout(&txc_info->pa_wakelock, 2*HZ);
	
    ret = pa122_read_ps(client,&psdata);
#ifdef	SUNLIGHT_ALS_USED
    ret = pa122_read_als(client, &alsdata);
#endif
    if (ret < 0) {
		pr_err("%s: txc_write error\n", __func__);
    }
#ifdef	SUNLIGHT_ALS_USED
	if(alsdata > PA12_SUN_LIGHT_ON){
		i2c_write_reg(client, REG_ALS_TH_LSB, PA12_ALS_TH_LSB_SUN_LIGHT_OFF);
		i2c_write_reg(client, REG_ALS_TH_MSB, PA12_ALS_TH_MSB_SUN_LIGHT_OFF);
		i2c_write_reg(client, REG_ALS_TL_LSB, PA12_ALS_TL_LSB_SUN_LIGHT_OFF);
		i2c_write_reg(client, REG_ALS_TL_MSB, PA12_ALS_TL_MSB_SUN_LIGHT_OFF);
		i2c_write_reg(client, REG_PS_OFFSET, 0);
	}else if(alsdata < PA12_SUN_LIGHT_OFF){
		i2c_write_reg(client, REG_ALS_TH_LSB, PA12_ALS_TH_LSB_SUN_LIGHT_ON);
		i2c_write_reg(client, REG_ALS_TH_MSB, PA12_ALS_TH_MSB_SUN_LIGHT_ON);
		i2c_write_reg(client, REG_ALS_TL_LSB, PA12_ALS_TL_LSB_SUN_LIGHT_ON);
		i2c_write_reg(client, REG_ALS_TL_MSB, PA12_ALS_TL_MSB_SUN_LIGHT_ON);
		if (txc->fast_calib_flag)
			i2c_write_reg(client, REG_PS_OFFSET, txc->fast_calibvalue);
		else
			i2c_write_reg(client, REG_PS_OFFSET, txc->ps_calibvalue);
	}
#endif	

	//ps_log("ps_data = %d, psdata = %d*************************************************\n", ps_data, psdata);
	if (txc->ps_data == PS_UNKONW || txc->ps_data == PS_FAR) {
		if(psdata > PA12_PS_FAR_TH_HIGH){
		    ps_data = PS_NEAR;
		    sendvalue= PA12_LED_CURR100 | PA12_PS_PRST2 | PA12_ALS_PRST4;
		    res=i2c_write_reg(client,REG_CFG1,sendvalue);
		    if (res < 0) {
			    pr_err("%s: txc_write error\n", __func__);
		    }

		    sendvalue= PA12_PS_INT_HYSTERESIS | PA12_PS_PERIOD6;
		    res=i2c_write_reg(client,REG_CFG3,sendvalue);
		    if (res < 0) {
			    pr_err("%s: txc_write error\n", __func__);
		    }
		} else if (psdata < PA12_PS_NEAR_TH_LOW) {
			ps_data= PS_FAR;
		} 
	} else if (txc->ps_data == PS_NEAR) {
		if(psdata < PA12_PS_NEAR_TH_LOW){
			ps_data = PS_FAR;
			
#ifdef	SUNLIGHT_ALS_USED
			sendvalue= PA12_LED_CURR100 | PA12_PS_PRST4 | PA12_ALS_PRST4;
#else
			sendvalue= PA12_LED_CURR100 | PA12_PS_PRST4;
#endif
			res=i2c_write_reg(client,REG_CFG1,sendvalue);
			if (res < 0) {
				pr_err("%s: txc_write error\n", __func__);
			}
			sendvalue= PA12_PS_INT_HYSTERESIS | PA12_PS_PERIOD12;
		    	res=i2c_write_reg(client,REG_CFG3,sendvalue);
		    	if (res < 0) {
					pr_err("%s: txc_write error\n", __func__);
				}
		}
	}

	if (txc->ps_data != ps_data) {
	    txc->ps_data = ps_data;
		    input_report_abs(txc->input_dev, ABS_DISTANCE, ps_data);
		    input_sync(txc->input_dev);
		    if (ps_data == PS_NEAR) {
			ps_log("***********near*********** pdata = %d\n", psdata);	
		    } else if (ps_data == PS_FAR) {
			ps_log("****************far***************pdata = %d\n", psdata);		
	    }
	}
	ret = i2c_read_reg(txc->client, REG_CFG2, &data);
	if (ret < 0) {
	    pr_err("%s: txc_read error\n", __func__);
	}
	data &= 0xfe;
	ret = i2c_write_reg(txc->client, REG_CFG2, data);
	if (ret < 0) {
	    pr_err("%s: txc_write error\n", __func__);
	}
}

static irqreturn_t txc_irq_handler(int irq, void *data)
{
	if(txc_info->ps_shutdown)
		return IRQ_HANDLED;
	if(txc_info->ps_resumed){
		schedule_delayed_work(&txc_info->ps_dwork, 0);
	}else{
		txc_info->resume_report = 1;
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void txc_early_suspend(struct early_suspend *h)
{
	if (txc_info->ps_enable) {
		mt_eint_unmask(CUST_EINT_INTI_INT_NUM);
	} else if ((txc_info->gesture) && (txc_info->mobile_leather)){
	    /* when enter earlysuspend, open it gesture handler*/
	    txc_info->mcu_enable = true;
	    txc_set_enable(txc_info, txc_info->mcu_enable);
	}
	txc_info->ps_resumed = 0;
}

static void txc_late_resume(struct early_suspend *h)
{
	if (txc_info->mcu_enable) {
		txc_info->mcu_enable = false;
		if (!txc_info->ps_syscall) {
			schedule_delayed_work(&txc_info->ioctl_enable_work, 0);
		}
	}
	txc_info->ps_resumed = 1;
}
#endif

#if 0
static int txc_fb_state_chg_callback(struct notifier_block *nb, 
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if(val != FB_EVENT_BLANK)
		return 0;

	if(evdata && evdata->data && val == FB_EVENT_BLANK) {
		blank = *(int *)(evdata->data);

		switch(blank) {
		case FB_BLANK_POWERDOWN:
			if (txc_info->ps_enable) {
			//	mt_eint_unmask(CUST_EINT_INTI_INT_NUM);
			} else if ((txc_info->gesture) && (txc_info->mobile_leather)){
	    		/* when enter earlysuspend, open it gesture handler*/
	    		txc_info->mcu_enable = true;

				//disable_irq(txc_info->irq);
	    		txc_set_enable(txc_info, txc_info->mcu_enable);
				ps_log("%s: suspend enable the txc ps sensor!\n", __func__);
	    		//schedule_delayed_work(&txc_info->ioctl_enable_work, 0);
			}
			break;
		case FB_BLANK_UNBLANK:
			if (txc_info->mcu_enable) {
				//enable_irq(txc_info->irq);
				txc_info->mcu_enable = false;
				if (!txc_info->ps_syscall)
					schedule_delayed_work(&txc_info->ioctl_enable_work, 0);
			}
			break;
		default:
			break;
		}
	}

	return NOTIFY_OK;
}


static struct notifier_block txc_noti_block = {
	.notifier_call = txc_fb_state_chg_callback,
};

#endif

static int txc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct txc_data *txc;
	struct device_node *dp = client->dev.of_node;
	struct pinctrl *pinctrl;
	const char *name;
	u8 iddata;
	//struct regulator *regulator = NULL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s()->%d:i2c adapter don't support i2c operation!\n",
			__func__, __LINE__);
		return -ENODEV;
	}
	/*request private data*/
	txc = kzalloc(sizeof(struct txc_data), GFP_KERNEL);
	if (!txc) {
		pr_err("%s()->%d:can not alloc memory to private data !\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	/*set client private data*/
	txc->client = client;
	i2c_set_clientdata(client, txc);
	txc_info = txc;

	mutex_init(&txc->enable_lock);
	mutex_init(&txc->i2c_lock);

	i2c_read_reg(client,0x7F,&iddata);
	if(0 != iddata){
		printk("the ic id is not pa122: %d\n", iddata);
		goto error1;
	}

	if(dp) {
		txc->irq_gpio = of_get_named_gpio_flags(dp, "txc,irq-gpio", 0, NULL);

		pinctrl = devm_pinctrl_get_select(&client->dev, "txc_ps");
		if(IS_ERR(pinctrl))
			ps_log("failed to get txc ps irq pinctrl !!!\n");

		ret = of_property_read_string(dp, "txc,power_reg", &name);
		if(ret== -EINVAL)
			txc->power_name = NULL;
		else {
			txc->power_name = name;
			txc->pa_pwr = regulator_get(&client->dev, txc->power_name);
			if(!IS_ERR(txc->pa_pwr)) {
				ret = regulator_enable(txc->pa_pwr);
				dev_info(&client->dev, "regulator_enable: %d\n", ret);
			}else{
				dev_err(&client->dev, "failed to get regulator (0x%p).\n",txc->pa_pwr);
			}
		}
	}

	ret = pa122_init(txc);
	if (ret < 0) {
		pr_err("%s()->%d:pa12201001_init failed the first time!\n", __func__, __LINE__);
	}	

	/*create input device for reporting data*/
	ret = txc_create_input(txc);
	if (ret < 0) {
		pr_err("%s()->%d:can not create input device!\n",
			__func__, __LINE__);
		return ret;
	}

	ret = sysfs_create_group(&client->dev.kobj, &txc_attribute_group);
	if (ret < 0) {
		pr_err("%s()->%d:can not create sysfs group attributes!\n",
			__func__, __LINE__);
		return ret;
	}

	wake_lock_init(&txc->pa_wakelock, WAKE_LOCK_SUSPEND,"ps_wake_lock");

	txc_info->irq = gpio_to_irq(txc->irq_gpio);
	if(txc_info->irq > 0){
		INIT_DELAYED_WORK(&txc->ps_dwork, txc_ps_handler);
			
		ret = request_threaded_irq(txc_info->irq, NULL, txc_irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT, "txc_ps", txc_info);
		if(ret < 0) {
			ps_log("%s request irq %d failed\n", __func__, txc_info->irq);	
		}
		disable_irq(txc_info->irq);
	}

	if(meizu_sysfslink_register(&client->dev, LINK_KOBJ_NAME) < 0)
		ps_log("sysfs_create_link failed.\n");

	//fb_register_client(&txc_info->notifier);
#ifdef CONFIG_HAS_EARLYSUSPEND
	txc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	txc->early_suspend.suspend = txc_early_suspend;
	txc->early_suspend.resume = txc_late_resume;
	register_early_suspend(&txc->early_suspend);
#endif

	return 0;
error1:
	kfree(txc_info);
	printk("pa122 id is not correct, probe is over\n");
	return -1;
}

static const struct i2c_device_id txc_id[] = {
	{ "PA122", 0 },
	{},
};

static int txc_remove(struct i2c_client *client)
{
	struct txc_data *txc = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&txc->early_suspend);
#endif
	sysfs_remove_group(&client->dev.kobj, &txc_attribute_group);
	input_free_device(txc->input_dev);
	meizu_sysfslink_unregister(LINK_KOBJ_NAME);
	kfree(txc);

	return 0;
}
static int ps_suspend(struct device *dev)
{
	ps_log("%s: begin\n",__func__);
	if((txc_info->ps_enable) && (txc_info->mobile_wakeup)){
		enable_irq_wake(txc_info->irq);
	}
	txc_info->ps_resumed = 0;
	txc_info->resume_report = 0;
	return 0;
}
static int ps_resume(struct device *dev)
{
	ps_log("%s: begin\n",__func__);
	if((txc_info->ps_enable) && (txc_info->mobile_wakeup)){
		disable_irq_wake(txc_info->irq);
	}
	if(txc_info->resume_report){
		txc_ps_handler(&txc_info->ps_dwork.work);
	}
	txc_info->ps_resumed = 1;
	return 0;
}

static void txc_shutdown(struct i2c_client *client)
{
	int ret = 0;

	struct txc_data *txc = i2c_get_clientdata(client);
	if((txc->power_name != NULL) && (regulator_is_enabled(txc->pa_pwr))){
		ret = regulator_disable(txc->pa_pwr);
		dev_info(&client->dev, "regulator_disable: %d\n", ret);
	}else{
		dev_err(&client->dev, "failed to get regulator (0x%p).\n",txc->pa_pwr);
	}
	txc->ps_shutdown = 1;
}

static struct of_device_id txc_ps_of_match_table[] = {
	{
		.compatible = "txc,pa122",
	},
	{},
};

static const struct dev_pm_ops ps_pm_ops = {
	.suspend = ps_suspend,
	.resume  = ps_resume,
};

static struct i2c_driver txc_driver = {
	.driver = {
		.name	= TXC_DEV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = txc_ps_of_match_table,
		.pm	= &ps_pm_ops,
	},
	.probe	= txc_probe,
	.remove = txc_remove,
	.shutdown = txc_shutdown,
	.id_table = txc_id,
};

static int __init txc_init(void)
{
	//i2c_register_board_info(3, i2c_txc, 1);
	return i2c_add_driver(&txc_driver);
}

static void __exit txc_exit(void)
{
	i2c_del_driver(&txc_driver);
}

module_init(txc_init);
module_exit(txc_exit);

