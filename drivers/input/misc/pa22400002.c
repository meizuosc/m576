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
#include <linux/input/pa22400002.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/meizu-sys.h>
#include "./../../base/base.h"


//#define	SUNLIGHT_ALS_USED

#define	LINK_KOBJ_NAME	"ps" // make soft link to /meizu/ps/

struct txc_data *txc_info224 = NULL;
static void txc_ps_handler(struct work_struct *work);

#define ps_ary_size 5
static const int ps_steady = ps_ary_size + 4;
static int far_ps_min = PA24_PS_OFFSET_MAX;
static int saturation_flag = 0;
static int oil_occurred = 0;
static int ps_seq_far[ps_ary_size] = {255, 255, 255, 255, 255};
static int ps_seq_oil[ps_ary_size] = {255, 255, 255, 255, 255};
static u8 ps_seq_near[ps_ary_size];

#define saturation_udelay 100000
#define sequence_udealy 15000

#define TXC_ABS(x) (x) >= 0 ? (x):(x)*(-1)
#define TXC_ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#define ARRAY_SUM(arr, sum) \
do { \
	int i = 0; \
	int size = TXC_ARRAY_SIZE(arr); \
	for (i=0; i<size; i++) \
		sum += arr[i]; \
} while (0)

#define ARRAY_ABS_SUM(arr, sum) \
do { \
	int i = 0; \
	int size = TXC_ARRAY_SIZE(arr); \
	for (i=0; i<size; i++) \
		sum += TXC_ABS(arr[i]); \
} while (0)

// I2C read one byte data from register 
static int i2c_read_reg(struct i2c_client *client,u8 reg,u8 *data)
{
	u8 databuf[2];
	int res = 0;
	databuf[0]= reg;
	
	mutex_lock(&txc_info224->i2c_lock);
	res = i2c_master_send(client,databuf,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_send function err  reg = %d\n",reg);
		mutex_unlock(&txc_info224->i2c_lock);
		return res;
	}
	res = i2c_master_recv(client,data,0x1);
	if(res <= 0)
	{
		APS_ERR("i2c_master_recv function err  reg = %d\n",reg);
		mutex_unlock(&txc_info224->i2c_lock);
		return res;
	}

	mutex_unlock(&txc_info224->i2c_lock);
	return 0;
}
// I2C Write one byte data to register
static int i2c_write_reg(struct i2c_client *client,u8 reg,u8 value)
{
	u8 databuf[2];
	int res = 0;
	databuf[0] = reg;
	databuf[1] = value;

	mutex_lock(&txc_info224->i2c_lock);
	res = i2c_master_send(client,databuf,0x2);
	if (res < 0){
		APS_ERR("i2c_master_send function err  reg = %d, value = %d\n",reg,value);
		mutex_unlock(&txc_info224->i2c_lock);
		return res;
	}
	mutex_unlock(&txc_info224->i2c_lock);
	return 0;
}

static int pa224_init(struct txc_data *data)
{
	int ret = 0;
	u8 sendvalue=0;
	struct i2c_client *client = data->client;
	data->pa224_sys_run_cal = 1;
	//data->ps_calibvalue = PA24_PS_OFFSET_EXTRA;//PA12_PS_OFFSET_DEFAULT +
	//data->fast_calib_flag = 0;
	data->ps_data = PS_UNKONW;
	data->ps_resumed = 1;
	//data->resume_report = 0;
	data->mobile_wakeup = 0;
	data->ps_enable = 0;
	data->nontype_enable = 0;
	data->ps_shutdown = 0;
	data->ps_fb_irq= 0;
	data->ps_fb_mode = 0;

	/*  reg 11 */
	ret = i2c_write_reg(client,REG_PS_SET,0x82); //PSET
	if(ret < 0){	
		APS_ERR(" write ps set para,i2c_send function err\n");
		return ret;
	}

	/* IR pulses setting and PD setting */
	sendvalue = PA24_PS_FILTER_0 | PA24_PS_PD | PA24_PS_IRLP_32;
	ret=i2c_write_reg(client,REG_CFG4,sendvalue);

	/* VCSEL current setting */
	sendvalue= PA24_VCSEL_CURR10| PA24_PS_PRST8;
	ret=i2c_write_reg(client,REG_CFG1,sendvalue);	

	/*Window type  &  ps sleep time 12.5ms*/
	sendvalue= PA24_PS_INT_WINDOW| PA24_PS_PERIOD12;
	ret=i2c_write_reg(client,REG_CFG3,sendvalue);

	ret=i2c_write_reg(client,REG_PS_TH,0xFF); //set TH threshold	
	ret=i2c_write_reg(client,REG_PS_TL,0); //set TL threshold

	ret = i2c_write_reg(client, REG_PS_OFFSET, 0x00);

	return ret;
}
static int pa224_set_ps_mode(struct i2c_client *client, bool flag)
{
	struct txc_data *data = i2c_get_clientdata(client);
	u8 sendvalue=0;
	int res = 0, i = 0;

	if (flag)
	{
		saturation_flag = 0;
		//oil_occurred = 0;
		//far_ps_min = PA24_PS_OFFSET_MAX;

		for (i=0;i<ps_ary_size;i++){
			ps_seq_far[i] = 255;
			ps_seq_oil[i] = 255;
		}

		sendvalue= PA24_VCSEL_CURR10| PA24_PS_PRST8;
		res=i2c_write_reg(client,REG_CFG1,sendvalue);

		/*Window type  &  ps sleep time 12.5ms*/
		sendvalue = PA24_PS_INT_WINDOW| PA24_PS_PERIOD12;
		res = i2c_write_reg(client,REG_CFG3,sendvalue);

		/*Set PS thresholds, deliberately let high threshold < low threshld to cause first interruption*/
		if (oil_occurred && far_ps_min < PA24_PS_OFFSET_MAX){
			data->ps_near_threshold = far_ps_min + 20;
			data->ps_far_threshold = far_ps_min + 19;
		}else if (!oil_occurred && far_ps_min < PA24_PS_OFFSET_MAX){
			data->ps_near_threshold = far_ps_min + 15;
			data->ps_far_threshold = far_ps_min + 14;
		}else if (far_ps_min >= PA24_PS_OFFSET_MAX){
			data->ps_near_threshold = 150;
			data->ps_far_threshold = 149;
		}

		res = i2c_write_reg(client,REG_PS_TH,data->ps_far_threshold);
		res = i2c_write_reg(client,REG_PS_TL,data->ps_near_threshold);

		// Interrupt Setting	   
		//res = i2c_read_reg(client, REG_CFG2, &regdata);
		sendvalue = PA24_INT_PS | PA24_PS_MODE_OFFSET;
		res=i2c_write_reg(client,REG_CFG2, sendvalue); //set int mode
	}else{
		/*clear the ps interrupt flag*/
		res = i2c_read_reg(client, REG_CFG2, &sendvalue);
		if (res < 0) {
			pr_err("%s: txc_read error\n", __func__);
		}
		sendvalue &= 0xfc;
		res = i2c_write_reg(client, REG_CFG2, sendvalue);
		if (res < 0) {
			pr_err("%s: txc_write error\n", __func__);
		}
		/*Hysteresis type  &  ps sleep time 12.5ms*/
		sendvalue = PA24_PS_INT_HYSTERESIS| PA24_PS_PERIOD12;
		res = i2c_write_reg(client,REG_CFG3,sendvalue);	
		/*Set PS thresholds*/
		if(far_ps_min < PA24_PS_OFFSET_MAX){
			data->fb_near_threshold = far_ps_min + 35;
			data->fb_far_threshold = far_ps_min + 25;
		}else{
			data->fb_near_threshold = 200;
			data->fb_far_threshold = far_ps_min + 15;
		}
		res = i2c_write_reg(client,REG_PS_TH,data->fb_near_threshold);
		res = i2c_write_reg(client,REG_PS_TL,data->fb_far_threshold);
		ps_log("*****Hysteresis type******* min = %d, near = %d, far = %d\n",
			 far_ps_min, data->fb_near_threshold, data->fb_far_threshold);
	}
	return 0 ;
}


//Read PS Count : 8 bit
int pa224_read_ps(struct i2c_client *client, u8 *data)
{
	int res;

	res = i2c_read_reg(client,REG_PS_DATA,data); //Read PS Data
	if(res < 0){
		APS_ERR("i2c_send function err\n");
	}
	return res;
}

void pa224_swap(u8 *x, u8 *y)
{
		u8 temp = *x;
		*x = *y;
		*y = temp;
}
/*
 * Calibration for PA224 is simply read crosstalk value, no need to store in file or write in register 0x10. 
 */
static int pa224_run_calibration(struct txc_data *data)
{
	struct i2c_client *client = data->client;
	int i, j;	
	int ret;
	u16 sum_of_pdata = 0;
	u8 temp_pdata[20],cfg0data=0;
	//u8 temp_pdata[20],cfg0data=0,cfg2data=0;
	unsigned int ArySize = 20;
	unsigned int cal_check_flag = 0;	
	//u8 value=0;
	int calibvalue;

	ps_log("%s: START proximity sensor calibration\n", __func__);

	data->pa224_sys_run_cal = 1;

RECALIBRATION:
	sum_of_pdata = 0;

	ret = i2c_read_reg(client, REG_CFG0, &cfg0data);

	/*Set crosstalk = 0*/
	ret = i2c_write_reg(client, REG_PS_OFFSET, 0x00);
	if (ret < 0) {
		pr_err("%s: txc_write error\n", __func__);
		/* Restore CFG2 (Normal mode) and Measure base x-talk */
		ret = i2c_write_reg(client, REG_CFG0, cfg0data); 
		return ret;
	}

	/*PS On*/
	ret = i2c_write_reg(client, REG_CFG0, cfg0data | 0x02); 
	usleep_range(50000, 50000);

	for(i = 0; i < 20; i++){
		usleep_range(15000, 15000);
		ret = i2c_read_reg(client,REG_PS_DATA,temp_pdata+i);
		//ps_log("temp_data = %d\n", temp_pdata[i]);	
	}	
	
	/* pdata sorting */
	for (i = 0; i < ArySize - 1; i++)
		for (j = i+1; j < ArySize; j++)
			if (temp_pdata[i] > temp_pdata[j])
				pa224_swap(temp_pdata + i, temp_pdata + j);	
	
	/* calculate the cross-talk using central 10 data */
	for (i = 5; i < 15; i++) {
		//ps_log("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}
	calibvalue = sum_of_pdata/10;

	/* Restore CFG2 (Normal mode) and Measure base x-talk */
	ret = i2c_write_reg(client, REG_CFG0, cfg0data);

	if ((calibvalue >= 0) && (calibvalue < PA24_PS_OFFSET_MAX)) {
		txc_info224->ps_data = PS_UNKONW;
		far_ps_min = calibvalue;
		data->ps_near_threshold = calibvalue+15;
		data->ps_far_threshold = calibvalue+14;
		ret = i2c_write_reg(client,REG_PS_TL,calibvalue+14);
		ret = i2c_write_reg(client,REG_PS_TH,calibvalue+15);
		ps_log("%s: FINISH proximity sensor calibration, %d\n", __func__, calibvalue);
		//data->ps_calibvalue = calibvalue + PA24_PS_OFFSET_EXTRA;
	} else {
		ps_log("%s: invalid calibrated data, calibvalue %d\n", __func__, calibvalue);

		if(cal_check_flag == 0)
		{
			ps_log("%s: RECALIBRATION start\n", __func__);
			cal_check_flag = 1;
			goto RECALIBRATION;
		}else{
			ps_log("%s: CALIBRATION FAIL -> cross_talk is set to DEFAULT\n", __func__);
			data->pa224_sys_run_cal = 0;
			return -EINVAL;
		}
	}

	return ret;
}

static void txc_set_enable(struct txc_data *txc, int enable)
{
	struct i2c_client *client = txc->client;
	int ret = 0;

	mutex_lock(&txc->enable_lock);

	if (enable) { //ps on
		ps_log("pa22401001 enable ps sensor\n");
		pa224_set_ps_mode(client, 1);

		ret=i2c_write_reg(client,REG_CFG0,0x02); //Write PS enable
		if(ret<0){
			APS_ERR("pa22401001_enable_ps function err\n");
		}

		usleep_range(50000, 50000); //Must wait 50ms for ps data to refresh.
	} else { //ps off
		ps_log("pa22401001 disaple ps sensor\n");
		input_report_abs(txc->input_dev, ABS_DISTANCE, PS_UNKONW);
		input_sync(txc->input_dev);
		ret=i2c_write_reg(client,REG_CFG0,0x00); //Write PS disable
		if(ret<0){
			APS_ERR("pa22401001_enable_ps function err\n");
		}
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
static ssize_t txc_ps_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	int enabled;
	enabled = txc_info224->ps_enable;

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t txc_ps_enable_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	int enable, handle;
	//int enable = simple_strtol(buf, NULL, 10);
	sscanf(buf, "%d %d\n", &handle, &enable);

	ps_log("%s :enable = %d ,handle = %d, waketype = %d, nontyep = %d, ps_enable = %d\n", 
		__func__, enable, handle, txc_info224->mobile_wakeup, txc_info224->nontype_enable, txc_info224->ps_enable);

	if((handle != 27)&&(handle != 3))
		return count;

	if(handle == 27){
		txc_info224->mobile_wakeup = enable;
	}else{
		txc_info224->nontype_enable = enable;
	}

	if((txc_info224->mobile_wakeup || txc_info224->nontype_enable) == txc_info224->ps_enable){
		if(txc_info224->mobile_wakeup && txc_info224->nontype_enable){
			input_report_abs(txc_info224->input_dev, ABS_DISTANCE, PS_UNKONW);
			//input_sync(txc_info224->input_dev);
			if(handle == 3){
				input_report_abs(txc_info224->input_dev, ABS_DISTANCE, txc_info224->ps_data);
				input_sync(txc_info224->input_dev);
				if(txc_info224->ps_data == PS_NEAR){
					ps_log("***********near*********** enable ps\n");
				}else if(txc_info224->ps_data == PS_FAR){
					ps_log("***********far*********** enable ps\n");
				}
			}else{
				txc_info224->ps_data = PS_UNKONW;
				txc_set_enable(txc_info224, enable);
			}
		}
		if(txc_info224->ps_enable && !txc_info224->mobile_wakeup && !enable && txc_info224->ps_fb_mode){
			pa224_set_ps_mode(txc_info224->client, 0);
		}
		return count;
	}

	txc_info224->ps_enable = (txc_info224->mobile_wakeup || txc_info224->nontype_enable);
	
	txc_info224->ps_data = PS_UNKONW;
 
	txc_set_enable(txc_info224, enable);

	if(enable){
		enable_irq(txc_info224->irq);
	}else{
		disable_irq(txc_info224->irq);
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
static ssize_t txc_ps_near_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u8 ps_data;

	ret = i2c_read_reg(client, REG_PS_TH, &ps_data); //Read PS Data

	return sprintf(buf, "%d\n", ps_data);
}

static ssize_t txc_ps_far_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u8 ps_data;

	ret = i2c_read_reg(client, REG_PS_TL, &ps_data); //Read PS Data

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


static ssize_t pa224_reg_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	u8 regdata;
	int res=0;
	int count=0;
	int i;

	for(i = 0;i <19 ;i++){
		res=i2c_read_reg(client,0x00+i,&regdata);
		if(res<0)
			break;
		else
			count+=sprintf(buf+count,"[%x] = (%x)\n",0x00+i,regdata);
	}
	return count;
}

static ssize_t pa224_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
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
		pa224_run_calibration(txc_info224);
	}

	return count;
}

static ssize_t txc_calibvalue_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", txc_info224->pa224_sys_run_cal - 1);
}

static ssize_t txc_calibvalue_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	//struct i2c_client *client = to_i2c_client(dev);

	//txc_info224->factory_calibvalue = simple_strtol(buf, NULL, 10);
	//i2c_write_reg(client, REG_PS_OFFSET, txc_info224->factory_calibvalue);
	//ps_log("=============================the factory calibvalue init is %d\n",txc_info224->factory_calibvalue);

	return count;
}

static ssize_t txc_batch_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
/*	
	int ps_batch = simple_strtol(buf, NULL, 10);

	if (ps_batch == 1) {
		input_report_abs(txc_info224->input_dev, ABS_DISTANCE, PS_UNKONW);
		input_sync(txc_info224->input_dev);
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
	int sensors_id = 0;

	sscanf(buf, "%d\n", &sensors_id);
	ps_log("****txc_flush_set: buf = %d\n",sensors_id);

	input_report_abs(txc_info224->input_dev, ABS_DISTANCE, sensors_id);
	input_sync(txc_info224->input_dev);
	input_report_abs(txc_info224->input_dev, ABS_DISTANCE, PS_UNKONW);
	return count;
}

/* the mobile_wakeup is use for enable the pa sensor irq can wakeup the system */
static ssize_t mobile_wakeup_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", txc_info224->mobile_wakeup);
}

static ssize_t mobile_wakeup_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	txc_info224->mobile_wakeup = simple_strtol(buf, NULL, 10);
	ps_log("%s: mobile_wakeup = %d\n", __func__, txc_info224->mobile_wakeup);
	return count;
}

static ssize_t gpio_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gpio_get_value(txc_info224->irq_gpio));
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
static DEVICE_ATTR(ps_near, 0664, txc_ps_near_show, NULL);
static DEVICE_ATTR(ps_far, 0664, txc_ps_far_show, NULL);
static DEVICE_ATTR(als_data, 0664, txc_als_data_show, NULL);
static DEVICE_ATTR(ps_reg, 0664, pa224_reg_show, pa224_reg_store);
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
	&dev_attr_ps_near.attr,
	&dev_attr_ps_far.attr,
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
static void pa224_get_ps_slope_array(int *ps_seq, int *slope, u8 ps, int arysize)
{
	int i;

	for (i=0; i<arysize-1; i++)
	{
		ps_seq[arysize-1-i] = ps_seq[arysize-1-i-1];
		if (ps_seq[arysize-1-i] == 0)
			ps_seq[arysize-1-i] = ps;
	}
	ps_seq[0] = (int)ps;

	for (i=0; i<arysize-1; i++)
	{
		slope[i] = ps_seq[i] - ps_seq[i+1];
	}
	return;
}

static void txc_ps_handler(struct work_struct *work)
{
	struct txc_data *txc = container_of(work, struct txc_data, ps_dwork.work);
	struct i2c_client *client = txc_info224->client;
	u8 psdata=0, prst = PA24_PS_PRST8, data;
	int ret, ps_data = PS_UNKONW, i =0;
	static int far_loop = 0;
	int slope[ps_ary_size-1];
	int sum = 0, abs_sum = 0, ps_sum = 0;

	wake_lock_timeout(&txc_info224->pa_wakelock, 2*HZ);

	ret = pa224_read_ps(client,&psdata);
	if (ret < 0) {
		pr_err("%s: txc_write error\n", __func__);
	}
	//SUNLIGHT
	if(!txc_info224->ps_fb_mode){
	if (psdata == 0) {
		saturation_flag = 1;
		if (oil_occurred && far_ps_min < PA24_PS_OFFSET_MAX){
			txc->ps_near_threshold = far_ps_min + OIL_EFFECT + 15;
			txc->ps_far_threshold = far_ps_min + OIL_EFFECT;
		}else if (!oil_occurred && far_ps_min < PA24_PS_OFFSET_MAX){
			txc->ps_near_threshold = far_ps_min + 15;
			txc->ps_far_threshold = far_ps_min + 7;
		}else if (far_ps_min >= PA24_PS_OFFSET_MAX){
			txc->ps_near_threshold = 150;
			txc->ps_far_threshold = 149;
		}
		usleep_range(saturation_udelay, saturation_udelay);
		//APS_ERR("Sun light!!, ht=%d, lt=%d, far_ps_min=%d\n", txc->ps_near_threshold, txc->ps_far_threshold, far_ps_min);
		ps_data = PS_FAR;
		goto exit_ps_handler;
	}

	//FARTHER AWAY
	if (psdata < txc->ps_far_threshold && (txc->ps_data == PS_UNKONW || txc->ps_data == PS_FAR)){
		pa224_get_ps_slope_array(ps_seq_far, slope, psdata, ps_ary_size);
		ARRAY_SUM(ps_seq_far, ps_sum);
		ARRAY_SUM(slope, sum);
		ARRAY_ABS_SUM(slope, abs_sum);
		//APS_ERR("slope : %d %d %d %d\n", slope[3], slope[2], slope[1], slope[0]);
		//APS_ERR("value : %d %d %d %d %d\n", ps_seq_far[4], ps_seq_far[3], ps_seq_far[2], ps_seq_far[1], ps_seq_far[0]);	
		//APS_ERR("saturation_flag=%d\n", saturation_flag);
		//If saturation happened, the average ps value must be greater than (far_ps_min-10) and also  steady
		if ( (saturation_flag && ps_sum/ps_ary_size >= ( far_ps_min>10 ? (far_ps_min-10) : far_ps_min )) 
			  || !saturation_flag || (saturation_flag && far_ps_min >= PA24_PS_OFFSET_MAX)) 
		{
			//STEADY
			if (abs_sum < ps_steady) {
				if (saturation_flag)
					saturation_flag = 0;

				ps_data = PS_FAR;
				oil_occurred = 0;
				far_ps_min = ps_sum/ps_ary_size;
				if (far_ps_min > PS_FAR_MIN_MAX)
					far_ps_min = PS_FAR_MIN_MAX;
				txc->ps_near_threshold = far_ps_min+15;
				txc->ps_far_threshold = far_ps_min>15? (far_ps_min-5):15;
				i2c_write_reg(client, REG_CFG3, PA24_PS_INT_WINDOW | PA24_PS_PERIOD12);
				//APS_ERR("FAR, far_ps_min %3d high low : %3d %3d\n", far_ps_min, txc->ps_near_threshold, txc->ps_far_threshold);
			}
		}
		usleep_range(sequence_udealy, sequence_udealy);
	}
	//NEAR
	else if (psdata > txc->ps_near_threshold){
		for (i = 0; i < ps_ary_size; i++){
			i2c_read_reg(client, REG_PS_DATA, &ps_seq_near[i]);
			if (i > 0)
				slope[i-1] = ps_seq_near[i]-ps_seq_near[i-1];
			usleep_range(5000, 5000);
		}

		ARRAY_ABS_SUM(slope, abs_sum);
		if (abs_sum < ps_steady){
			ps_data = PS_NEAR;
			if (psdata >= 200){
				oil_occurred = 1;
				txc->ps_far_threshold = far_ps_min + OIL_EFFECT;
				txc->ps_near_threshold = 0xFF;
			}else{
				txc->ps_far_threshold = far_ps_min+7;
				txc->ps_near_threshold = 200;
			}
		//APS_ERR("NER, ps %3d high low : %3d %3d\n", psdata, txc->ps_near_threshold, txc->ps_far_threshold);
		}else if (abs_sum > 20){
			APS_ERR("Flicker light!!!!\n");
			i2c_write_reg(client, REG_CFG3, PA24_PS_INT_WINDOW | PA24_PS_PERIOD6);
			usleep_range(saturation_udelay,saturation_udelay);
		}
	}
	//NEAR to FAR
	if (psdata < txc->ps_far_threshold && txc->ps_data == PS_NEAR)
	{
		if (oil_occurred){
			far_loop++;
			pa224_get_ps_slope_array(ps_seq_oil, slope, psdata, ps_ary_size);
			ARRAY_SUM(ps_seq_oil, ps_sum);
			ARRAY_SUM(slope, sum);
			ARRAY_ABS_SUM(slope, abs_sum);
			//APS_ERR("slope : %d %d %d %d\n", slope[3], slope[2], slope[1], slope[0]);
			//APS_ERR("value : %d %d %d %d %d\n", ps_seq_oil[4], ps_seq_oil[3], ps_seq_oil[2], ps_seq_oil[1], ps_seq_oil[0]);
			//STEADY
			if ( abs_sum < ps_steady || far_loop > 10) {
				ps_data = PS_FAR;
				oil_occurred = 0;
				if (far_loop <= 10){
					far_ps_min = ps_sum/ps_ary_size;
					if (far_ps_min > PS_FAR_MIN_MAX)
						far_ps_min = PS_FAR_MIN_MAX;
					txc->ps_near_threshold = far_ps_min+15;
					txc->ps_far_threshold = far_ps_min>5? (far_ps_min-5):5;
				}else{
					far_ps_min = far_ps_min + 15;
					if (far_ps_min > PS_FAR_MIN_MAX)
						far_ps_min = PS_FAR_MIN_MAX;
					txc->ps_near_threshold = far_ps_min+15;
					txc->ps_far_threshold = far_ps_min+7;
				}	
				i2c_write_reg(client, REG_CFG3, PA24_PS_INT_WINDOW | PA24_PS_PERIOD12);
				//APS_ERR("OIL to FAR, far_ps_min %3d high low : %3d %3d\n", far_ps_min, txc->ps_near_threshold, txc->ps_far_threshold);
			}
			usleep_range(sequence_udealy, sequence_udealy);
		}else{
			ps_data = PS_FAR;
			txc->ps_near_threshold = far_ps_min+15;
			txc->ps_far_threshold = far_ps_min+7;
			//APS_ERR("FAR, far_ps_min %3d high low : %3d %3d\n", far_ps_min, txc->ps_near_threshold, txc->ps_far_threshold);
		}

	}
	}else{
	//ps_log("ps_data = %d, psdata = %d, near =%d, far= %d***************************************test**********\n", ps_data, psdata, txc->fb_near_threshold, txc->fb_far_threshold);
	if (txc->ps_data == PS_UNKONW || txc->ps_data == PS_FAR) {
		if(psdata > txc->fb_near_threshold){
			ps_data = PS_NEAR;
		} else if (psdata < txc->fb_far_threshold) {
			ps_data= PS_FAR;
		} 
	} else if (txc->ps_data == PS_NEAR) {
		if(psdata < txc->fb_far_threshold){
			ps_data = PS_FAR;
		}
	}
	}

exit_ps_handler:
	if(!txc_info224->ps_fb_mode){
		/*Set thresholds*/
		i2c_write_reg(client, REG_CFG1, PA24_VCSEL_CURR10 | prst);
		i2c_write_reg(client, REG_PS_TH, txc->ps_near_threshold);
		i2c_write_reg(client, REG_PS_TL, txc->ps_far_threshold);
	}

	if ((txc->ps_data != ps_data) &&(ps_data != PS_UNKONW)) {
		txc->ps_data = ps_data;
		input_report_abs(txc->input_dev, ABS_DISTANCE, ps_data);
		input_sync(txc->input_dev);
		if (ps_data == PS_NEAR) {
			ps_log("***near*********** pdata = %d, min = %d, near = %d, far = %d\n",
				psdata, far_ps_min, txc->ps_near_threshold, txc->ps_far_threshold);
		} else if (ps_data == PS_FAR) {
			ps_log("********far******* pdata = %d, min = %d, near = %d, far = %d\n",
				psdata, far_ps_min, txc->ps_near_threshold, txc->ps_far_threshold);
		}
	}
	if(!txc_info224->ps_fb_mode){
		ret = i2c_read_reg(txc->client, REG_CFG2, &data);
		if (ret < 0) {
			pr_err("%s: txc_read error\n", __func__);
		}
		data &= 0xfc;
		ret = i2c_write_reg(txc->client, REG_CFG2, data);
		if (ret < 0) {
			pr_err("%s: txc_write error\n", __func__);
		}
	}
}

static irqreturn_t txc_irq_handler(int irq, void *data)
{
	if(txc_info224->ps_shutdown)
		return IRQ_HANDLED;
	//if(txc_info224->ps_resumed){
		schedule_delayed_work(&txc_info224->ps_dwork, 0);
	//}else{
		//txc_info224->resume_report = 1;
	//}

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void txc_early_suspend(struct early_suspend *h)
{
	if (txc_info224->ps_enable) {
		mt_eint_unmask(CUST_EINT_INTI_INT_NUM);
	} else if ((txc_info224->gesture) && (txc_info224->mobile_leather)){
		/* when enter earlysuspend, open it gesture handler*/
		txc_info224->mcu_enable = true;
		txc_set_enable(txc_info224, txc_info224->mcu_enable);
	}
	txc_info224->ps_resumed = 0;
}

static void txc_late_resume(struct early_suspend *h)
{
	if (txc_info224->mcu_enable) {
		txc_info224->mcu_enable = false;
		if (!txc_info224->ps_syscall) {
			schedule_delayed_work(&txc_info224->ioctl_enable_work, 0);
		}
	}
	txc_info224->ps_resumed = 1;
}
#endif

static int ps_fb_state_chg_callback(struct notifier_block *nb, 
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
			ps_log("ps suspend--- enable = %d, wakeup = %d\n", txc_info224->ps_enable, txc_info224->mobile_wakeup);
			if(txc_info224->ps_enable){
				if(txc_info224->mobile_wakeup){
					enable_irq_wake(txc_info224->irq);
					txc_info224->ps_fb_irq = 1;
				}else{
					/*Change to hysteresis type*/
					cancel_delayed_work(&txc_info224->ps_dwork);
					disable_irq(txc_info224->irq);
					pa224_set_ps_mode(txc_info224->client, 0);
					txc_info224->ps_fb_mode = 1;
					enable_irq(txc_info224->irq);
				}
			}
			txc_info224->ps_resumed = 0;
			//txc_info224->resume_report = 0;
			break;
		case FB_BLANK_UNBLANK:
			ps_log("ps resume--- enable = %d, wakeup = %d, fb_irq = %d, fb_mode = %d\n", txc_info224->ps_enable, txc_info224->mobile_wakeup, txc_info224->ps_fb_irq, txc_info224->ps_fb_mode);
			if(txc_info224->ps_fb_irq){
				disable_irq_wake(txc_info224->irq);
				txc_info224->ps_fb_irq = 0;
			}else if(txc_info224->ps_fb_mode){
				/*Change back to window type*/
				pa224_set_ps_mode(txc_info224->client, 1);
				txc_info224->ps_fb_mode = 0;
			}
			txc_info224->ps_resumed = 1;

			break;
		default:
			break;
		}
	}

	return NOTIFY_OK;
}


static struct notifier_block ps_noti_block = {
	.notifier_call = ps_fb_state_chg_callback,
};

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
	txc_info224 = txc;
	mutex_init(&txc->enable_lock);
	mutex_init(&txc->i2c_lock);

	i2c_read_reg(client,0x7F,&iddata);
	if(0x11 != iddata)
	{
		printk("the ic id is not pa224: %d\n", iddata);
		goto error_id;
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

	ret = pa224_init(txc);
	if (ret < 0) {
		pr_err("%s()->%d:pa22401001_init failed the first time!\n", __func__, __LINE__);
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

	txc_info224->irq = gpio_to_irq(txc->irq_gpio);
	if(txc_info224->irq > 0){
		INIT_DELAYED_WORK(&txc->ps_dwork, txc_ps_handler);
			
		ret = request_threaded_irq(txc_info224->irq, NULL, txc_irq_handler, IRQF_TRIGGER_FALLING |  IRQF_ONESHOT, "txc_ps", txc_info224);
		if(ret < 0) {
			ps_log("%s request irq %d failed\n", __func__, txc_info224->irq);	
		}
		disable_irq(txc_info224->irq);
	}

	txc_info224->fb_notifier = ps_noti_block;
	ret = fb_register_client(&txc_info224->fb_notifier);
	if(ret)
		ps_log("ps register notifier failed.\n");
	if(meizu_sysfslink_register(&client->dev, LINK_KOBJ_NAME) < 0)
		ps_log("sysfs_create_link failed.\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	txc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	txc->early_suspend.suspend = txc_early_suspend;
	txc->early_suspend.resume = txc_late_resume;
	register_early_suspend(&txc->early_suspend);
#endif
	return 0;
error_id:
	kfree(txc_info224);
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
#if 1
static int ps_suspend(struct device *dev)
{
	return 0;
}
static int ps_resume(struct device *dev)
{
	u8 regdata;
	int res=0;
	int i;

	for(i = 0;i <13 ;i++){
		res=i2c_read_reg(txc_info224->client,0x00+i,&regdata);
		if(res<0)
			break;
		else
			ps_log("[%x] = (%x)\n",0x00+i,regdata);
	}
	ps_log("%s: begin, ps irq gpio = %d\n",__func__, gpio_get_value(txc_info224->irq_gpio));
	return 0;
}
#else
static int ps_suspend(struct device *dev)
{
	ps_log("%s: begin, enable = %d, wakeup = %d\n",__func__, txc_info224->ps_enable, txc_info224->mobile_wakeup);
	if(txc_info224->ps_enable){
		if(txc_info224->mobile_wakeup){
			enable_irq_wake(txc_info224->irq);
		}else{
			/*Change to hysteresis type*/
			pa224_set_ps_mode(txc_info224->client, 0);
		}
	}
	txc_info224->ps_resumed = 0;
	//txc_info224->resume_report = 0;

	return 0;
}
static int ps_resume(struct device *dev)
{
	ps_log("%s: begin, enable = %d, wakeup = %d\n",__func__, txc_info224->ps_enable, txc_info224->mobile_wakeup);
	if(txc_info224->ps_enable){
		if(txc_info224->mobile_wakeup){
			disable_irq_wake(txc_info224->irq);
		}else{
			/*Change back to window type*/
			pa224_set_ps_mode(txc_info224->client, 1);
		}
	}

	//if(txc_info224->resume_report){
	//	txc_ps_handler(&txc_info224->ps_dwork.work);
	//}
	txc_info224->ps_resumed = 1;

	return 0;
}


#endif

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
		.compatible = "txc,pa224",
	},
	{},
};

#if 1
static const struct dev_pm_ops ps_pm_ops = {
	.suspend = ps_suspend,
	.resume  = ps_resume,
};
#endif

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

static int __init pa224_driver_init(void)
{
	//i2c_register_board_info(3, i2c_txc, 1);
	return i2c_add_driver(&txc_driver);
}

static void __exit pa224_driver_exit(void)
{
	i2c_del_driver(&txc_driver);
}

module_init(pa224_driver_init);
module_exit(pa224_driver_exit);

