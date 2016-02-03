#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h> /* BUS_I2C */
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>

#include <linux/iio/triggered_buffer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/irq_work.h>


#include <linux/wakelock.h>
#include "./mx-hub.h"
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/meizu-sys.h>
#include <linux/muic_tsu6721.h>

#define SENSOR_HUB_TAG				  "[SensorHub] "
#define DEBUG						   1
#if defined(DEBUG)
#define SH_FUN(f)					   printk(KERN_INFO SENSOR_HUB_TAG"%s\n", __FUNCTION__)
#define SH_ERR(fmt, args...)			printk(KERN_ERR  SENSOR_HUB_TAG"%s %d ERROR: "fmt, __FUNCTION__, __LINE__, ##args)
#define SH_LOG(fmt, args...)			printk(KERN_ERR  SENSOR_HUB_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define SH_DBG(fmt, args...)			printk(KERN_INFO SENSOR_HUB_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#else
#define SH_FUN(f)					   printk(KERN_INFO SENSOR_HUB_TAG"%s\n", __FUNCTION__)
#define SH_ERR(fmt, args...)			printk(KERN_ERR  SENSOR_HUB_TAG"%s %d ERROR: "fmt, __FUNCTION__, __LINE__, ##args)
#define SH_LOG(fmt, args...)			printk(KERN_ERR  SENSOR_HUB_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define SH_DBG(fmt, args...)
#endif
#define CWMCU_MUTEX
/* GPIO for MCU control */
#define QueueSystemInfoMsgSize  30
#define QueueWarningMsgSize		  30

#define ACK					 0x79
#define NACK					0x1F

#define DPS_MAX		 (1 << (16 - 1))

/* Input poll interval in milliseconds */

#define CWMCU_POLL_INTERVAL 10
#define CWMCU_POLL_MAX	  2000
#define FT_VTG_MIN_UV	   2600000
#define FT_VTG_MAX_UV	   3300000
#define FT_I2C_VTG_MIN_UV   1800000
#define FT_I2C_VTG_MAX_UV   1800000



/* turn on gpio interrupt if defined */
#define CWMCU_INTERRUPT

#define CWMCU_CALIB_SAVE_IN_FLASH

#ifdef CWMCU_INTERRUPT
#define CWMCU_POLL_MIN	  50
#endif

struct CWMCU_T {
	struct i2c_client *client;
	struct regulator *sensor_pwr;
	//struct regulator *vcc_i2c;
	struct input_dev *input;
	struct workqueue_struct *driver_wq;
	struct work_struct work;
	struct delayed_work delay_work;
	struct CWMCU_SENSORS_INFO sensors_info[HANDLE_ID_END][SENSORS_ID_END];
	SensorsInit_T   hw_info[DRIVER_ID_END];
	RegInformation *pReadRegInfo;
	RegInformation *pWriteRegInfo;
	u8 m_cReadRegCount;
	u8 m_cWriteRegCount;
	uint8_t initial_hw_config;

	int mcu_mode;
	int sensor_debug;
	uint8_t kernel_status;

	/* enable & batch list */
	uint32_t enabled_list[HANDLE_ID_END];
	uint32_t interrupt_status;
	uint8_t calibratordata[DRIVER_ID_END][30];
	uint8_t calibratorUpdate[DRIVER_ID_END];
	/* Mcu site enable list*/

	/* power status */
	volatile uint32_t power_on_list;

	/* Calibrator status */
	int cal_cmd;
	int cal_type;
	int cal_id;
	int cal_status;

	/* gpio */
	int irq_gpio;
	int wakeup_gpio;
	int reset_gpio;
	int boot_gpio;
	int sleep_mcu_gpio;
	int mcu_busy_gpio;
	unsigned long irq_flags;
	const char *pwr_reg_name;
	const char *pwr18_reg_name;

	uint32_t debug_log;

	int cmd;
	uint32_t addr;
	int len;
	int mcu_slave_addr;
	int firmware_update_status;
	int cw_i2c_rw;  /* r = 0 , w = 1 */
	int cw_i2c_len;
	uint8_t cw_i2c_data[300];

	s32 iio_data[6];
	struct iio_dev *indio_dev;
	struct irq_work iio_irq_work;
	struct iio_trigger  *trig;
	atomic_t pseudo_irq_enable;

	struct class *sensor_class;
	struct device *sensor_dev;
	atomic_t delay;
	int supend_flag;

	int wq_polling_time;

#ifdef CWMCU_MUTEX
	struct mutex mutex_lock;
	struct mutex mutex_lock_i2c;
	struct mutex mutex_wakeup_gpio;
#endif

	unsigned char loge_buff[QueueSystemInfoMsgSize*2];
	unsigned char loge_buff_count;

	unsigned char logw_buff[QueueWarningMsgSize*2];
	unsigned char logw_buff_count;

	uint8_t logcat_cmd;


	int mcu_status;
	int mcu_init_count;
};
//for geater than 32 bytes read

static int CWMCU_Object_read(struct CWMCU_T *sensor, u8 reg_addr, u8 *data, u8 len)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr   = sensor->client->addr,
			.flags  = 0,
			.len	= 1,
			.buf	= &reg_addr,
		},
		{
			.addr   = sensor->client->addr,
			.flags  = I2C_M_RD,
			.len	= len,
			.buf	= data,
		},
	};

	ret = i2c_transfer(sensor->client->adapter, msgs, 2);
	return (ret == 2) ? len : ret;
}
static int CWMCU_reg_read(struct CWMCU_T *sensor, u8 reg_addr, u8 *data)
{
	RegInformation *pReadRegInfoInx = sensor->pReadRegInfo;
	int i;
	u8 cReadRegCount = sensor->m_cReadRegCount;
	int wRetVal = 0;

	if(pReadRegInfoInx == NULL || cReadRegCount == 0){
		wRetVal = -1;
		return wRetVal;
	}

	for(i = 0; i < cReadRegCount; i++){
		if(pReadRegInfoInx[i].cIndex == reg_addr)
			break;
	}

	if(i >= cReadRegCount){
		wRetVal = -1;
	}else{
		if(pReadRegInfoInx[i].cObjLen != 0)
			wRetVal = CWMCU_Object_read(sensor, pReadRegInfoInx[i].cIndex, data, pReadRegInfoInx[i].cObjLen);
	}
	return wRetVal;
}
// Returns the number of read bytes on success 

static int CWMCU_I2C_R(struct CWMCU_T *sensor, u8 reg_addr, u8 *data, u8 len)
{
	int rty = 0;
	int err = 0;
#ifdef CWMCU_MUTEX
	mutex_lock(&sensor->mutex_wakeup_gpio);
#endif
retry:
	err = i2c_smbus_read_i2c_block_data(sensor->client, reg_addr, len, data);
	if(err <0){
		if(rty<3){
			gpio_set_value(sensor->wakeup_gpio, 0);
			usleep_range(100,300);
			gpio_set_value(sensor->wakeup_gpio, 1);
			usleep_range(300, 500);
			rty++;
			goto retry;
		}else{
			pr_err("%s:%s:(i2c read error =%d)\n",LOG_TAG_KERNEL ,__FUNCTION__,err);
		}
	}
#ifdef CWMCU_MUTEX
	mutex_unlock(&sensor->mutex_wakeup_gpio);
#endif
	return err;
}

// write format	1.slave address  2.data[0]  3.data[1] 4.data[2]
static int CWMCU_I2C_W(struct CWMCU_T *sensor, u8 reg_addr, u8 *data, u8 len)
{
	int rty = 0;
	int err = 0;
#ifdef CWMCU_MUTEX
	mutex_lock(&sensor->mutex_wakeup_gpio);
#endif
 retry: 
  err = i2c_smbus_write_i2c_block_data(sensor->client, reg_addr, len, data);
	if(err <0){
		if(rty<3){
			gpio_set_value(sensor->wakeup_gpio, 0);
			usleep_range(100,300);
			gpio_set_value(sensor->wakeup_gpio, 1);
			usleep_range(300, 500);
			rty++;
			goto retry;
		}else{
			pr_err("%s:%s:(i2c write error =%d)\n",LOG_TAG_KERNEL ,__FUNCTION__,err);
		}
	}
#ifdef CWMCU_MUTEX
	mutex_unlock(&sensor->mutex_wakeup_gpio);
#endif
	return err;
}

static int CWMCU_I2C_W_SERIAL(struct CWMCU_T *sensor,u8 *data, int len)
{
	int dummy;
	dummy = i2c_master_send(sensor->client, data, len);
	if (dummy < 0) {
		pr_err("%s:%s:(i2c write error =%d)\n",LOG_TAG_KERNEL ,__FUNCTION__,dummy);
		return dummy;
	}
	return 0;
}

static int CWMCU_I2C_R_SERIAL(struct CWMCU_T *sensor,u8 *data, int len)
{
	int dummy;
	dummy = i2c_master_recv(sensor->client, data, len);
	if (dummy < 0) {
		pr_err("%s:%s:(i2c read error =%d)\n",LOG_TAG_KERNEL ,__FUNCTION__,dummy);
		return dummy;
	}
	return 0;
}

static int cw_send_event(struct CWMCU_T *sensor, u8 handle, u8 id, u8 *data)
{
	u8 event[21];/* Sensor HAL uses fixed 21 bytes */

	if (id == CWMCU_NODATA)
		return FAIL;

	event[0] = handle;
	event[1] = id;
	memcpy(&event[2], data, 19);

	if (sensor->debug_log & (1<<D_IIO_DATA))
		printk("%s: id%d,data:%d,%d,%d\n",
		  __func__,id,data[0],data[1],data[2]);
	if (sensor->indio_dev->active_scan_mask &&
		(!bitmap_empty(sensor->indio_dev->active_scan_mask,
			   sensor->indio_dev->masklength))) {
			iio_push_to_buffers(sensor->indio_dev, event);
		return 0;
	} else if (sensor->indio_dev->active_scan_mask == NULL)
		printk("%s: active_scan_mask = NULL, event might be missing\n",
		  __func__);

	if(sensor->sensor_debug)
		hub_log("%s: %d %d : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", __func__,
		 event[0], event[1],event[2],event[3],event[4],event[5],event[6],event[7],event[8],event[9],
		 event[10],event[11],event[12],event[13],event[14],event[15],event[16],event[17],event[18],event[19],event[20]);
	return -EIO;
}

static void power_pin_sw(struct CWMCU_T *sensor,SWITCH_POWER_ID id, int onoff)
{
    int rty = 3;
    int value = 0;
    int value_mcu = 0;
    uint32_t current_power_on_list = 0;

#ifdef CWMCU_MUTEX
	mutex_lock(&sensor->mutex_wakeup_gpio);
#endif
    value = gpio_get_value(sensor->wakeup_gpio);
    value_mcu = gpio_get_value(sensor->sleep_mcu_gpio);
    if (onoff) {
        current_power_on_list = sensor->power_on_list;
        sensor->power_on_list |= ((uint32_t)(1) << id);
        if(current_power_on_list == 0 ||value ==0 || value_mcu==0){
            do{
                gpio_set_value(sensor->wakeup_gpio, onoff);
                usleep_range(200, 200);
                value_mcu = gpio_get_value(sensor->sleep_mcu_gpio);
                if(value_mcu==0){
                    gpio_set_value(sensor->wakeup_gpio, 0);
                    usleep_range(100, 100);
                }
                rty --;
            }while(rty>0&&value_mcu ==0);
            if(rty <=0){
                SH_LOG("Pin switch fail: MCU still in sleep mode\n");
            }
        } 
    } else {
        sensor->power_on_list &= ~(1 << id);
        if (sensor->power_on_list == 0 && value == 1) {
            gpio_set_value(sensor->wakeup_gpio, onoff);
            usleep_range(100, 100);
        }
    }
#ifdef CWMCU_MUTEX
	mutex_unlock(&sensor->mutex_wakeup_gpio);
#endif
}

static void cwmcu_kernel_status(struct CWMCU_T *sensor,uint8_t status)
{
	if (sensor->mcu_mode == CW_BOOT) {
		return ;
	}

	sensor->kernel_status = status;
	if(CWMCU_I2C_W(sensor, RegMapW_SetHostStatus, &sensor->kernel_status, 1) < 0){
		printk("Write SetHostStatus Fail [I2C], func: %s ,li: %d\n",__func__,__LINE__);
	}
}

static int check_enable_list(struct CWMCU_T *sensor){
	int i = 0,j=0;
	int count = 0;
	int handle = 0;
	uint8_t data[10];
	int error_msg = 0;
	uint32_t enabled_list[HANDLE_ID_END] = {0};
	uint32_t enabled_list_temp = 0;

	if (sensor->mcu_mode == CW_BOOT) {
		SH_LOG("mcu_mode = boot, func:%s, line:%d\n", __func__, __LINE__);
		return 0;
	}

    if(CWMCU_I2C_R(sensor, RegMapR_GetHostEnableList, data, 8)>= 0) {
		enabled_list[NonWakeUpHandle] = (uint32_t)data[3]<<24 |(uint32_t)data[2]<<16 |(uint32_t)data[1]<<8 |(uint32_t)data[0];
		enabled_list[WakeUpHandle] = (uint32_t)data[7]<<24 |(uint32_t)data[6]<<16 |(uint32_t)data[5]<<8 |(uint32_t)data[4];
		enabled_list[InternalHandle] = 0;

		if((enabled_list[NonWakeUpHandle] != sensor->enabled_list[NonWakeUpHandle]) 
			|| (enabled_list[WakeUpHandle] != sensor->enabled_list[WakeUpHandle]))
		{
			SH_LOG("Enable List Check AP0:%d,MCU0:%d;AP1:%d,MCU1:%d\n",
			sensor->enabled_list[NonWakeUpHandle],enabled_list[NonWakeUpHandle],
			sensor->enabled_list[WakeUpHandle],enabled_list[WakeUpHandle]);

			for(j = 0; j < InternalHandle; j++)
			{
				handle = j;
				enabled_list_temp = sensor->enabled_list[handle]^enabled_list[handle];
				for(i = 0;i <SENSORS_ID_END;i++){
					if (enabled_list_temp& (1<<i)){
						data[0] = handle;
						data[1] = i;
						if(sensor->sensors_info[handle][i].en){
										  sensor->sensors_info[handle][i].rate = (sensor->sensors_info[handle][i].rate ==0)?200:sensor->sensors_info[handle][i].rate;
							data[2] = sensor->sensors_info[handle][i].rate;
							data[3] = (uint8_t)sensor->sensors_info[handle][i].timeout;
							data[4] = (uint8_t)(sensor->sensors_info[handle][i].timeout >>8);
							error_msg = CWMCU_I2C_W(sensor, RegMapW_SetEnable, data, 5);
						}else{
							data[2] = 0;
							data[3] = 0;
							data[4] = 0;
							error_msg = CWMCU_I2C_W(sensor, RegMapW_SetDisable, data, 5);
						}
						if (error_msg < 0)
							SH_ERR("I2c Write Fail;%d,%d\n", handle, i);
						count++;
						if(count >15)
						{
							count = 0;
							msleep(20);
						}
					}
				}
			}
		}
	}
	return 0;
}

static int cwmcu_read_buff(struct CWMCU_T *sensor , u8 handle)
{
	uint8_t count_reg;
	uint8_t data_reg;
	uint8_t data[24] = {0};
	uint16_t count = 0;
	int i = 0;

	if (sensor->mcu_mode == CW_BOOT) {
		SH_LOG("-CWMCU- mcu_mode = boot, func:%s, line:%d\n", __func__, __LINE__);
		return -1;
	}

	if(handle == NonWakeUpHandle){
		count_reg = RegMapR_StreamCount;
		data_reg = RegMapR_StreamEvent;
	}else if(handle == WakeUpHandle){
		count_reg = RegMapR_BatchCount;
		data_reg = RegMapR_BatchEvent;
	}else{
		return FAIL;
	}

	if (CWMCU_I2C_R(sensor, count_reg, data, 2) >= 0) {
		count = ((uint16_t)data[1] << 8) | (uint16_t)data[0];
	} else {
		hub_log("%s:%s:(check count failed)\n",LOG_TAG_KERNEL ,__FUNCTION__);
		return FAIL;
	}
	if((data[0] ==0xFF) && (data[1] ==0xFF))
		return NO_ERROR;

	for (i = 0; i < count; i++) {
		if (CWMCU_I2C_R(sensor, data_reg, data, 9) >= 0) {
			cw_send_event(sensor,handle,data[0],&data[1]);
		}
	}
		return NO_ERROR;
}
static int cwmcu_read_gesture(struct CWMCU_T *sensor )
{
	uint8_t data[24] = {0};
	uint8_t count = 0;
	int i = 0;

	if (sensor->mcu_mode == CW_BOOT) {
		SH_LOG("mcu_mode = boot, func:%s, line:%d\n", __func__, __LINE__);
		return 0;
	}

	if (CWMCU_I2C_R(sensor, RegMapR_GestureCount, &count, 1) < 0) {
		hub_log("%s:%s:(check count failed)\n",LOG_TAG_KERNEL ,__FUNCTION__);
		return FAIL;
	}
	if(count ==0xFF)
		return NO_ERROR;

	for (i = 0; i < count; i++) {
		if (CWMCU_I2C_R(sensor, RegMapR_GestureEvent, data, 9) >= 0) {
			cw_send_event(sensor,NonWakeUpHandle,data[0],&data[1]);
		}
	}
	return NO_ERROR;
}

#define QueueSystemInfoMsgBuffSize	  QueueSystemInfoMsgSize*5
static void parser_mcu_info(char *data){
	static unsigned char loge_bufftemp[QueueSystemInfoMsgBuffSize];
	static int buff_counttemp = 0;
	int i;

	for(i=0;i<QueueSystemInfoMsgSize;i++){
		loge_bufftemp[buff_counttemp] = (unsigned char)data[i];
		buff_counttemp++;
		if(data[i] == '\n'|| (buff_counttemp >=QueueSystemInfoMsgBuffSize)){
			hub_log("%s:%s","MSG",loge_bufftemp);
			memset(loge_bufftemp,0x00,QueueSystemInfoMsgBuffSize);
			buff_counttemp = 0;
		}
	}
}

static void read_mcu_info(struct CWMCU_T *sensor)
{
	uint8_t data[40];
	uint16_t count = 0;
	int i = 0;

	SH_FUN();
	if (sensor->mcu_mode == CW_BOOT) {
		SH_LOG("-CWMCU- mcu_mode = boot, func:%s, line:%d\n", __func__, __LINE__);
		return ;
	}

	if (CWMCU_I2C_R(sensor, RegMapR_SystemInfoMsgCount, data, 1) >= 0){
		count = (uint16_t)data[0];
	} else {
		hub_log("%s:%s:(check count fail)\n",LOG_TAG_KERNEL ,__FUNCTION__);
		return;
	}

	if(count ==0xFF)
		return ;

	for (i = 0; i < count; i++) {
		if (CWMCU_I2C_R(sensor, RegMapR_SystemInfoMsgEvent, data, 30) >= 0) {
			parser_mcu_info(data);
		}
	}

}

static int CWMCU_POLLING(struct CWMCU_T *sensor)
{
	//if (sensor->debug_log & (1<<D_DELAY_WQ)) 
		//printk("--CWMCU-- Polling: debug_log =>0x%d\n", sensor->enabled_list);

	if (sensor->enabled_list[NonWakeUpHandle])
	{
		power_pin_sw(sensor,SWITCH_POWER_POLLING, 1);
		cwmcu_read_buff(sensor, NonWakeUpHandle);
		cwmcu_read_buff(sensor, WakeUpHandle);
		power_pin_sw(sensor,SWITCH_POWER_POLLING, 0);
	}
    return 0;
}

/*==========sysfs node=====================*/
static int cwmcu_find_mindelay(struct CWMCU_T *sensor, int handle)
{
	int i;
	int min_delay = 30;
	for (i = 0; i < SENSORS_ID_END; i++) {	
		if(sensor->sensors_info[handle][i].en
				&& (sensor->sensors_info[handle][i].rate >= 10)
				&& (sensor->sensors_info[handle][i].rate < min_delay)
		  )
		{
			min_delay = sensor->sensors_info[handle][i].rate;
		}
	}
	min_delay = (min_delay<=10)? 10: min_delay;
	return min_delay;
}

static ssize_t active_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	int enabled = 0;
	int id = 0;
	int handle = 0;
	int error_msg = 0;
	uint8_t data[10];

	if (sensor->mcu_mode == CW_BOOT) {
		return count;
	}
	sscanf(buf, "%d %d %d\n", &handle, &id, &enabled);
	power_pin_sw(sensor,SWITCH_POWER_ENABLE, 1);

	sensor->sensors_info[handle][id].en = enabled;
	data[0] = handle;
	data[1] = id;
	if(enabled){
		sensor->enabled_list[handle] |= 1<<id;
		data[2] = (sensor->sensors_info[handle][id].rate ==0)?200:sensor->sensors_info[handle][id].rate;
		data[3] = (uint8_t)sensor->sensors_info[handle][id].timeout;
		data[4] = (uint8_t)(sensor->sensors_info[handle][id].timeout >>8);
			error_msg = CWMCU_I2C_W(sensor, RegMapW_SetEnable, data, 5);
	}else{
		sensor->enabled_list[handle] &= ~(1<<id);
		sensor->sensors_info[handle][id].rate = 0;
		sensor->sensors_info[handle][id].timeout = 0;
		data[2] = 0;
		data[3] = 0;
		data[4] = 0;
			error_msg = CWMCU_I2C_W(sensor, RegMapW_SetDisable, data, 5);
	}
	if (error_msg < 0)
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
	usleep_range(5000, 5000);
	check_enable_list(sensor);
	   power_pin_sw(sensor,SWITCH_POWER_ENABLE, 0);
	if(handle == NonWakeUpHandle){
        sensor->wq_polling_time = cwmcu_find_mindelay(sensor,NonWakeUpHandle);
        if(sensor->wq_polling_time != atomic_read(&sensor->delay)){
            cancel_delayed_work_sync(&sensor->delay_work);
            if(sensor->enabled_list[NonWakeUpHandle]){
                atomic_set(&sensor->delay, sensor->wq_polling_time);
                queue_delayed_work(sensor->driver_wq, &sensor->delay_work,
                msecs_to_jiffies(atomic_read(&sensor->delay)));
	   }else{
                atomic_set(&sensor->delay, CWMCU_POLL_MAX);
	}
	}
	}

	hub_log("%s:(%d,%d,%d,%d,%d)\n",__FUNCTION__ ,handle,id,enabled,(int)sensor->sensors_info[handle][id].rate,(int)sensor->sensors_info[handle][id].timeout);

	return count;
}

static ssize_t active_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[10] = {0};
	uint32_t enabled_list[2] ={0};
	if (sensor->mcu_mode == CW_BOOT) {
		hub_log("%s:%s:(mcu_mode = CW_BOOT)\n",LOG_TAG_KERNEL ,__FUNCTION__);
		return snprintf(buf, 32, "In Boot Mode\n");
	}
	power_pin_sw(sensor,SWITCH_POWER_ENABLE, 1);
	if (CWMCU_I2C_R(sensor, RegMapR_GetHostEnableList, data, 8) >= 0) {
		enabled_list[NonWakeUpHandle] = (uint32_t)data[3]<<24 |(uint32_t)data[2]<<16 |(uint32_t)data[1]<<8 |(uint32_t)data[0];
		enabled_list[WakeUpHandle] = (uint32_t)data[7]<<24 |(uint32_t)data[6]<<16 |(uint32_t)data[5]<<8 |(uint32_t)data[4];
		if (sensor->debug_log & (1<<D_EN))
			hub_log("%s:%s:(MCU En Status:%d,%d)\n",LOG_TAG_KERNEL ,__FUNCTION__, enabled_list[NonWakeUpHandle], enabled_list[WakeUpHandle]);
	} else {
		hub_log("%s:%s:(check MCU En Status failed)\n",LOG_TAG_KERNEL ,__FUNCTION__);
	}
	power_pin_sw(sensor,SWITCH_POWER_ENABLE, 0);
	return snprintf(buf, 256, "%d %d %d %d\n", sensor->enabled_list[NonWakeUpHandle], sensor->enabled_list[WakeUpHandle],enabled_list[NonWakeUpHandle],enabled_list[WakeUpHandle]);
}

static ssize_t batch_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint32_t id = 0;
	uint32_t handle = 0;	
	uint32_t mode = -1;
	uint32_t rate = 0;
	uint32_t timeout = 0;
	uint8_t data[5];
	int err = 0;

	if (sensor->mcu_mode == CW_BOOT) {
		return count;
	}
	sscanf(buf, "%d %d %d %d %d\n", &handle, &id, &mode, &rate, &timeout);
	if(mode ==0)
	{
	sensor->sensors_info[handle][id].rate = (uint8_t)rate;
	sensor->sensors_info[handle][id].timeout = (uint16_t)timeout;
		data[0] = handle;
		data[1] = id;
		data[2] = sensor->sensors_info[handle][id].rate;
		data[3] = (uint8_t)sensor->sensors_info[handle][id].timeout;
		data[4] = (uint8_t)(sensor->sensors_info[handle][id].timeout >>8);
		if(sensor->sensors_info[handle][id].en){
			power_pin_sw(sensor,SWITCH_POWER_BATCH, 1);
			err = CWMCU_I2C_W(sensor, RegMapW_SetEnable, data, 5);
			power_pin_sw(sensor,SWITCH_POWER_BATCH, 0);
			if(err < 0){
				hub_log("%s:(Write Fail:id:%d, mode:%d, rate:%d, timeout:%d)\n",__FUNCTION__, id,mode, rate, timeout);
			}
		}
		
	}
		if (sensor->debug_log & (1<<D_EN))
			hub_log("%s:(id:%d, mode:%d, rate:%d, timeout:%d)\n",__FUNCTION__, id,mode, rate, timeout);
	return count;
}

static ssize_t flush_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	int id = 0;
	int handle = 0;
	uint8_t data[2] = {0};
	int err = 0;

	if (sensor->mcu_mode == CW_BOOT){
		return count;
	}

	sscanf(buf, "%d %d\n", &handle, &id);
	data[0] = (uint8_t)handle;
	data[1] = (uint8_t)id;

	hub_log("%s:(flush:id:%d)\n",__FUNCTION__, id);
	power_pin_sw(sensor,SWITCH_POWER_FLUSH, 1);
	err = CWMCU_I2C_W(sensor, RegMapW_SetFlush, data, 2);
	power_pin_sw(sensor,SWITCH_POWER_FLUSH, 0);
	if(err < 0){
		SH_ERR("H:%d,Id:%d\n",handle, id);
   	 }

	return count;
}

static int CWMCU_Write_Mcu_Memory(struct CWMCU_T *sensor,const char *buf)
{
	uint8_t WriteMemoryCommand[2];
	uint8_t data[300];
	uint8_t received[10];
	uint8_t XOR = 0;
	uint16_t i = 0;
	WriteMemoryCommand[0] = 0x31;
	WriteMemoryCommand[1] = 0xCE;
	if (CWMCU_I2C_W_SERIAL(sensor,(uint8_t *)WriteMemoryCommand, 2) < 0) {
		return -EAGAIN;
	}
	if (CWMCU_I2C_R_SERIAL(sensor,(uint8_t *)received, 1) < 0) {
		return -EAGAIN;
	}
	if (received[0] != ACK) {
		return -EAGAIN;
	}
 
	data[0] = (uint8_t) (sensor->addr >> 24);
	data[1] = (uint8_t) (sensor->addr >> 16);
	data[2] = (uint8_t) (sensor->addr >> 8);
	data[3] = (uint8_t) sensor->addr;
	data[4] = data[0] ^ data[1] ^ data[2] ^ data[3];
	if (CWMCU_I2C_W_SERIAL(sensor,(uint8_t *)data, 5) < 0) {
		return -EAGAIN;
	}
	if (CWMCU_I2C_R_SERIAL(sensor,(uint8_t *)received, 1) < 0) {
		return -EAGAIN;
	}
	if (received[0] != ACK) {
		return -EAGAIN;
	}

	data[0] = sensor->len - 1;
	XOR = sensor->len - 1;
	for (i = 0; i < sensor->len; i++) {
		data[i+1] = buf[i];
		XOR ^= buf[i];
	}
	data[sensor->len+1] = XOR;

	if (CWMCU_I2C_W_SERIAL(sensor,(uint8_t *)data, (sensor->len + 2)) < 0) {
		return -EAGAIN;
	}
	return 0;
}


static int set_calib_cmd(struct CWMCU_T *sensor, uint8_t cmd, uint8_t id, uint8_t type)
{
	uint8_t data[4];
	int err;
	if (sensor->mcu_mode == CW_BOOT)
		return -1;

	data[0] = cmd;
	data[1] = id;
	data[2] = type;
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_W(sensor, RegMapW_CalibratorCmd, data, 4);
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	if(err)
		SH_LOG("Write CalibratorCmd Fail [I2C], func: %s ,li: %d\n",__func__,__LINE__);
	return err;
}

/*
	sensors default calibrator flow:
		sensors_calib_start(sensors, id);
		do{
			sensors_calib_status(sensors, id,&status);
		}while(status ==CALIB_STATUS_INPROCESS)
		if(status ==CALIB_STATUS_PASS)
			sensors_calib_data_read(sensors, id,data);
		save data
*/
static int sensors_calib_start(struct CWMCU_T *sensor, uint8_t id)
{
	int err;
	err = set_calib_cmd(sensor, CALIB_EN, id, CALIB_TYPE_DEFAULT);
	if (err < 0){
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		return err;
	}
	err = set_calib_cmd(sensor, CALIB_CHECK_STATUS, id, CALIB_TYPE_NON);
	if (err < 0){
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		return err;
	}
	return err;
}
static int sensors_calib_status(struct CWMCU_T *sensor, uint8_t id, int *status)
{
	int err;
	uint8_t i2c_data[31] = {0};
	if (sensor->mcu_mode == CW_BOOT)
		return FAIL;
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_CalibratorData, i2c_data, 30);
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	if (err < 0){
		hub_log("%s:(I2c Read Fail)\n" ,__FUNCTION__);
		return I2C_FAIL;
	}
	status[0] = (int)((int8_t)i2c_data[0]);
	return NO_ERROR;
}
static int sensors_calib_data_read(struct CWMCU_T *sensor, uint8_t id, uint8_t *data)
{
	int err;
	if (sensor->mcu_mode == CW_BOOT)
		return FAIL;
	err = set_calib_cmd(sensor, CALIB_DATA_READ, id, CALIB_TYPE_NON);
	if (err < 0){
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		return err;
	}
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_CalibratorData, data, 30);
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	if (err < 0){
		hub_log("%s:(I2c Read Fail)\n" ,__FUNCTION__);
		return err;
	}
	return NO_ERROR;
}
static int sensors_calib_data_write(struct CWMCU_T *sensor, uint8_t id, uint8_t *data)
{
	int err;
	if (sensor->mcu_mode == CW_BOOT)
		return FAIL;
	err = set_calib_cmd(sensor, CALIB_DATA_WRITE, id, CALIB_TYPE_NON);
	if (err < 0){
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		return err;
	}
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_W(sensor, RegMapW_CalibratorData, data, 30);
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	if (err < 0){
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		return err;
	}
	return NO_ERROR;
}

static int proximity_calib_en(struct CWMCU_T *sensor, int en)
{
	int err;
	if(en)
		err = set_calib_cmd(sensor, CALIB_EN, PROXIMITY, CALIB_TYPE_SENSORS_ENABLE);
	else
		err = set_calib_cmd(sensor, CALIB_EN, PROXIMITY, CALIB_TYPE_SENSORS_DISABLE);
	if (err < 0){
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		return err;
	}

	return NO_ERROR;
}

/*
	FUN: proximity_calib_data
	|data[0]: Proximity sensors raw data
	|data[1] is Hight threshold to check sensors is near
	|data[2] is low threshold to check sensors is far
*/
static int proximity_calib_data(struct CWMCU_T *sensor, int *data)
{
	int err;
	uint8_t i2c_data[31] = {0};
	int *ptr;
	ptr = (int *)i2c_data;

	SH_FUN();
	if (sensor->mcu_mode == CW_BOOT)
		return FAIL;
	err = set_calib_cmd(sensor, CALIB_DATA_READ, PROXIMITY, CALIB_TYPE_NON);
	if (err < 0){
		hub_log("%s:(set_calib_cmd Fail)\n" ,__FUNCTION__);
		return err;
	}
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_CalibratorData, i2c_data, 30);
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	if (err < 0){
		hub_log("%s:(I2c Read Fail)\n" ,__FUNCTION__);
		return I2C_FAIL;
	}
	data[0] = ptr[3];
	data[1] = ptr[1];
	data[2] = ptr[2];

	SH_LOG("raw:%d, close:%d, far:%d\n", data[0], data[1], data[2]);
	return NO_ERROR;
}

static int proximity_set_threshold(struct CWMCU_T *sensor, int near_th, int far_th)
{
	int err;
	uint8_t i2c_data[31] = {0};
	int *ptr;
	ptr = (int *)i2c_data;

   SH_FUN();
	if (sensor->mcu_mode == CW_BOOT)
		return FAIL;
	err = set_calib_cmd(sensor, CALIB_DATA_WRITE, PROXIMITY, CALIB_TYPE_NON);
	if (err < 0){
		hub_log("%s:(set_calib_cmd Fail)\n" ,__FUNCTION__);
		return err;
	}
	ptr[0] = 0;
	ptr[1] = near_th;
	ptr[2] = far_th;

	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_W(sensor, RegMapW_CalibratorData, i2c_data, 30);
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	if (err < 0)
	{
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		return -1;
	}
	return NO_ERROR;
}

static ssize_t set_firmware_update_cmd(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	u8 data[300] = {0};
	int i = 0;
	int status = 0;
	int proximity_data[3] = {0};
	u8 cReadRegCount = sensor->m_cReadRegCount;
	u8 cWriteRegCount = sensor->m_cWriteRegCount;
	RegInformation *pReadRegInfoInx = sensor->pReadRegInfo;
	RegInformation *pWriteRegInfoInx = sensor->pWriteRegInfo;

	sscanf(buf, "%d %d %d\n", &sensor->cmd, &sensor->addr, &sensor->len);

	hub_log("%s:%s:(cmd=%d addr=%d len=%d)\n",LOG_TAG_KERNEL ,__FUNCTION__, sensor->cmd, sensor->addr, sensor->len);

	power_pin_sw(sensor,SWITCH_POWER_FIRMWARE_COMMAND, 1);

	switch (sensor->cmd) {
	case CHANGE_TO_BOOTLOADER_MODE:
		hub_log("%s:%s:(CHANGE_TO_BOOTLOADER_MODE)\n",LOG_TAG_KERNEL ,__FUNCTION__);

		sensor->mcu_mode = CW_BOOT;
		sensor->mcu_slave_addr = sensor->client->addr;
		sensor->client->addr = 0x72 >> 1;

		if(gpio_is_valid(sensor->reset_gpio))
			gpio_request(sensor->reset_gpio, "MCU_RESET");
		gpio_direction_output(sensor->reset_gpio, 1);
		gpio_set_value(sensor->boot_gpio, 1);
		usleep_range(5000, 6000);
		gpio_set_value(sensor->reset_gpio, 0);
		//usleep_range(10000, 11000);
		msleep(500);
		gpio_set_value(sensor->reset_gpio, 1);
		msleep(500);
		break;

	case CHANGE_TO_NORMAL_MODE:
		hub_log("%s:%s:(CHANGE_TO_NORMAL_MODE)\n",LOG_TAG_KERNEL ,__FUNCTION__);

		sensor->firmware_update_status = 1;
		sensor->client->addr = 0x74 >> 1;

		//if(gpio_is_valid(sensor->reset_gpio))
			//gpio_request(sensor->reset_gpio, "MCU_RESET");
		//gpio_direction_output(sensor->reset_gpio, 1);
		gpio_set_value(sensor->boot_gpio, 0);
		gpio_set_value(sensor->reset_gpio, 1);
		usleep_range(5000, 6000);
		//msleep(2);
		gpio_set_value(sensor->reset_gpio, 0);
		//usleep_range(10000, 11000);
		msleep(500);
		gpio_set_value(sensor->reset_gpio, 1);
		msleep(500);

		gpio_direction_input(sensor->reset_gpio);
   		sensor->mcu_mode = CW_NORMAL;
   		
		power_pin_sw(sensor,SWITCH_POWER_SHUTDOWN, 1);
		cwmcu_kernel_status(sensor,KERNEL_SHUTDOWN);
		power_pin_sw(sensor,SWITCH_POWER_SHUTDOWN, 0);
		break;

	case CHECK_FIRMWAVE_VERSION:
		if (CWMCU_I2C_R(sensor, RegMapR_GetFWVersion, data, 4) >= 0) {
			hub_log("%s:%s:(CHECK_FIRMWAVE_VERSION:%u,%u,%u,%u)\n",LOG_TAG_KERNEL ,__FUNCTION__, data[0],data[1],data[2],data[3]);
		}
		break;

	case GET_FWPROJECT_ID:
		if (CWMCU_reg_read(sensor, RegMapR_GetProjectID, data) >= 0) {
			hub_log("%s:%s:(PROJECT ID:%s) \n",LOG_TAG_KERNEL ,__FUNCTION__, data);
		}
		break;

	case SHOW_THE_REG_INFO:
		if (pWriteRegInfoInx != NULL && pReadRegInfoInx != NULL) {
			hub_log("(number of read reg:%u number of write reg:%u) \n",cReadRegCount ,cWriteRegCount);
			hub_log("--------------------READ REGISTER INFORMATION------------------------\n");
			for(i =0; i < cReadRegCount; i++){
				hub_log("(read tag number:%u and lengh:%u) \n",pReadRegInfoInx->cIndex,pReadRegInfoInx->cObjLen);
				pReadRegInfoInx++;
			}

			hub_log("--------------------WRITE REGISTER INFORMATION-----------------------\n");
			for(i =0; i < cWriteRegCount; i++){
				hub_log("(write tag number:%u and lengh:%u) \n",pWriteRegInfoInx->cIndex ,pWriteRegInfoInx->cObjLen);
				pWriteRegInfoInx++;
			}
		}
		break;

	case SET_DEBUG_LOG:
		if(sensor->len)
			sensor->debug_log  |= (1<< sensor->addr);
		else
			sensor->debug_log  &= ~(1<< sensor->addr);
		hub_log("%s:%s:(SET_DEBUG_LOG%u)\n",LOG_TAG_KERNEL ,__FUNCTION__,sensor->debug_log);
		break;
	case SET_SYSTEM_COMMAND:
		data[0] = sensor->addr;
		data[1] = sensor->len;
		CWMCU_I2C_W(sensor, RegMapW_SetSystemCommand, data, 2);
		hub_log("%s:%s:(SET_SYSTEM_COMMAND)\n",LOG_TAG_KERNEL ,__FUNCTION__);
		break;
	case GET_SYSTEM_TIMESTAMP:
		if (CWMCU_I2C_R(sensor, RegMapR_GetSystemTimestamp, data, 4) >= 0) {
			hub_log("%s:%s:(Timestamp:%u)\n",LOG_TAG_KERNEL ,__FUNCTION__,(((uint32_t)data[3])<<24) |(((uint32_t)data[2])<<16) |(((uint32_t)data[1])<<8) | ((uint32_t)data[0]));
		}
		break;
	case SET_HW_INITIAL_CONFIG_FLAG:
		sensor->initial_hw_config = sensor->addr;
		break;
	case SET_SENSORS_POSITION:
		data[0] = sensor->addr;
		data[1] = sensor->len;
		CWMCU_I2C_W(sensor, RegMapW_SetSensorAxisReference, data, 2);
	 	break;
	case CMD_CALIBRATOR_START:
		sensors_calib_start(sensor,sensor->addr);
		break;
	case CMD_CALIBRATOR_STATUS:
		sensors_calib_status(sensor,sensor->addr,&status);
		break;
	case CMD_CALIBRATOR_READ:
		sensors_calib_data_read(sensor,sensor->addr,sensor->cw_i2c_data);
		break;
	case CMD_CALIBRATOR_WRITE:
		sensors_calib_data_write(sensor,sensor->addr,sensor->cw_i2c_data);
		break;
	case CMD_PROXIMITY_EN:
		proximity_calib_en(sensor,sensor->addr);
		break;
	case CMD_PROXIMITY_DATA:
		proximity_calib_data(sensor,proximity_data);
			hub_log("%s:%s:(Proximity data:%d,%d,%d)\n",LOG_TAG_KERNEL ,__FUNCTION__,proximity_data[0],proximity_data[1],proximity_data[2]);
		break;
	case CMD_PROXIMITY_TH:
		proximity_set_threshold(sensor,sensor->addr,sensor->len);
		hub_log("%s:%s:(Proximity th:%d,%d)\n",LOG_TAG_KERNEL ,__FUNCTION__,sensor->addr,sensor->len);
			break;
	//case MCU_RESET: //20 0 0 mcu_reset
		//sensor->firmware_update_status = 0;
		//cwmcu_reset_mcu();
		//break;
	}
	power_pin_sw(sensor,SWITCH_POWER_FIRMWARE_COMMAND, 0);
	return count;
}

static ssize_t set_firmware_update_data(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	hub_log("%s:%s:()\n",LOG_TAG_KERNEL ,__FUNCTION__);
	sensor->firmware_update_status = 1;
	sensor->firmware_update_status = CWMCU_Write_Mcu_Memory(sensor,buf);
	return count;
}

static ssize_t get_firmware_update_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	hub_log("%s:%s:(firmware_update_status = %d)\n",LOG_TAG_KERNEL ,__FUNCTION__,sensor->firmware_update_status);
	return snprintf(buf, sizeof(sensor->firmware_update_status), "%d\n", sensor->firmware_update_status);
}

static ssize_t set_firmware_update_i2(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	int intsize = sizeof(int);

	if(sensor->sensor_debug)
		SH_FUN();
	memcpy(&sensor->cw_i2c_rw, buf, intsize);
	memcpy(&sensor->cw_i2c_len, &buf[4], intsize);
	memcpy(sensor->cw_i2c_data, &buf[8], sensor->cw_i2c_len);
	return count;
}

static ssize_t get_firmware_update_i2(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	int status = 0;

	if(sensor->sensor_debug)
		SH_FUN();
	if (sensor->cw_i2c_rw) {
		if (CWMCU_I2C_W_SERIAL(sensor,sensor->cw_i2c_data, sensor->cw_i2c_len) < 0) {
			status = -1;
		}
		memcpy(buf, &status, sizeof(int));
		return 4;
	} else {
		if (CWMCU_I2C_R_SERIAL(sensor,sensor->cw_i2c_data, sensor->cw_i2c_len) < 0) {
			status = -1;
			memcpy(buf, &status, sizeof(int));
			return 4;
		}
		memcpy(buf, &status, sizeof(int));
		memcpy(&buf[4], sensor->cw_i2c_data, sensor->cw_i2c_len);
		return 4+sensor->cw_i2c_len;
	}
	return  0;
}

static ssize_t mcu_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sensor->mcu_mode);
}

static ssize_t mcu_model_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	int mode = 0;
	sscanf(buf, "%d\n", &mode);
	sensor->mcu_mode = mode;
	return count;
}

static ssize_t sensor_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sensor->sensor_debug);
}

static ssize_t sensor_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	int mode = 0;
	sscanf(buf, "%d\n", &mode);
	sensor->sensor_debug = mode;
	return count;
}

static ssize_t set_calibrator_cmd(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	int err;
	if (sensor->mcu_mode == CW_BOOT)
		return count;
	sscanf(buf, "%d %d %d\n", &sensor->cal_cmd, &sensor->cal_id, &sensor->cal_type);
	sensor->cal_status = CALIB_STATUS_INPROCESS;
	err = set_calib_cmd(sensor, sensor->cal_cmd, sensor->cal_id, sensor->cal_type);

	if (sensor->debug_log & (1<<D_CALIB))
		hub_log("%s:%s:(cmd:%d,id:%d,type:%d)\n",LOG_TAG_KERNEL ,__FUNCTION__, sensor->cal_cmd, sensor->cal_id, sensor->cal_type);
	if (err < 0){
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
		sensor->cal_status = CALIB_STATUS_FAIL;
	}
	return count;
}
static ssize_t get_calibrator_cmd(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	return sprintf(buf, "Cmd:%d,Id:%d,Type:%d\n", sensor->cal_cmd, sensor->cal_id, sensor->cal_type);
}

static ssize_t get_calibrator_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	return sprintf(buf, "%d %d\n",sensor->cal_id, sensor->cal_status);
}

static ssize_t get_calibrator_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t Cal_data[31] = {0};
	int err;

	if (sensor->mcu_mode == CW_BOOT)
		return 0;

	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_CalibratorData, Cal_data, 30);
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	if(sensor->cal_cmd == CALIB_DATA_READ && err >=0){
		memcpy(sensor->calibratordata[sensor->cal_id],Cal_data,30);
		sensor->calibratorUpdate[sensor->cal_id] = 1;
    }
	return sprintf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		err,
		Cal_data[0], Cal_data[1], Cal_data[2],
		Cal_data[3], Cal_data[4], Cal_data[5], Cal_data[6], Cal_data[7], Cal_data[8], Cal_data[9], Cal_data[10], Cal_data[11], Cal_data[12],
		Cal_data[13], Cal_data[14], Cal_data[15], Cal_data[16], Cal_data[17], Cal_data[18], Cal_data[19], Cal_data[20], Cal_data[21], Cal_data[22],
		Cal_data[23], Cal_data[24], Cal_data[25], Cal_data[26], Cal_data[27], Cal_data[28], Cal_data[29]);
}

static ssize_t set_calibrator_data(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[30];
	int temp[33] = {0};
	int i,err;
	if (sensor->mcu_mode == CW_BOOT)
		return count;
	sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		&temp[0], &temp[1], &temp[2],
		&temp[3], &temp[4], &temp[5], &temp[6], &temp[7], &temp[8], &temp[9], &temp[10], &temp[11], &temp[12],
		&temp[13], &temp[14], &temp[15], &temp[16], &temp[17], &temp[18], &temp[19], &temp[20], &temp[21], &temp[22],
		&temp[23], &temp[24], &temp[25], &temp[26], &temp[27], &temp[28], &temp[29]);
	for (i = 0 ; i < 30; i++)
		data[i] = (uint8_t)temp[i];

	if(sensor->cal_cmd == CALIB_DATA_WRITE){
		memcpy(sensor->calibratordata[sensor->cal_id],data,30);
		sensor->calibratorUpdate[sensor->cal_id] = 1;
	}
	power_pin_sw(sensor,SWITCH_POWER_CALIB, 1);
	err = CWMCU_I2C_W(sensor, RegMapW_CalibratorData, data, 30);
	if (err < 0)
		hub_log("%s:(I2c Write Fail)\n" ,__FUNCTION__);
	 power_pin_sw(sensor,SWITCH_POWER_CALIB, 0);
	return count;
}

static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[4];
	int16_t version = -1;

	if (sensor->mcu_mode == CW_BOOT) {
		return 0;
	}
	power_pin_sw(sensor,SWITCH_POWER_VERSION, 1);
	if (CWMCU_I2C_R(sensor, RegMapR_GetFWVersion, data, 4) >= 0) {
		version = (int16_t)( ((uint16_t)data[1])<<8 | (uint16_t)data[0]);
		hub_log("%s:%s:(CHECK_FIRMWAVE_VERSION : M:%u,D:%u,V:%u,SV:%u)\n",LOG_TAG_HAL ,__FUNCTION__, data[3], data[2], data[1], data[0]);
	}else{
		hub_log("%s:%s:(i2c read fail)\n",LOG_TAG_HAL ,__FUNCTION__);
	}
	power_pin_sw(sensor,SWITCH_POWER_VERSION, 0);
	return sprintf(buf, "%d\n", version);
}

static ssize_t library_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct CWMCU_T *sensor = dev_get_drvdata(dev);
    uint8_t data[4];

    SH_FUN();
    if (CW_BOOT == sensor->mcu_mode){
        SH_LOG("mcu_mode == CW_BOOT!\n");
        return 0;
    }

    power_pin_sw(sensor,SWITCH_POWER_VERSION, 1);
    if (CWMCU_I2C_R(sensor, RegMapR_GetLibVersion, data, 4) >= 0){
        SH_LOG("check_library_version:%u,%u,%u,%u\n", data[3], data[2], data[1], data[0]);
    }else{
        SH_ERR("i2c read fail)\n");
    }
    power_pin_sw(sensor,SWITCH_POWER_VERSION, 0);
    return sprintf(buf, "%d %d %d %d\n", data[3], data[2], data[1], data[0]);
}

static ssize_t timestamp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[4];
	uint32_t *ptr;
	int err;
	ptr = (uint32_t *)data;

	if (CW_BOOT == sensor->mcu_mode){
		SH_LOG("mcu_mode == CW_BOOT!\n");
		return 0;
	}

	power_pin_sw(sensor,SWITCH_POWER_TIME, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_GetSystemTimestamp, data, 4);
	power_pin_sw(sensor,SWITCH_POWER_TIME, 0);
	if(err < 0){
		printk(" Read GetSystemTimestamp Fail [I2C], func: %s ,li: %d\n",__func__,__LINE__);
		return err;
	}
	return sprintf(buf, "%d %u\n", err, ptr[0]);
}


static ssize_t set_sys_cmd(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[8];
	int temp[8] = {0};
	int i,err;

	if (sensor->mcu_mode == CW_BOOT){
		SH_ERR("mcu_mode == CW_BOOT!\n");
		return count;
	}

	sscanf(buf, "%d %d %d %d %d %d %d %d\n",
		&temp[0], &temp[1], &temp[2],
		&temp[3], &temp[4], &temp[5], &temp[6], &temp[7]);

	for (i = 0 ; i < 8; i++)
		data[i] = (uint8_t)temp[i];

	power_pin_sw(sensor,SWITCH_POWER_SYS, 1);
	err = CWMCU_I2C_W(sensor, RegMapW_SetSystemCommand, data, 8);
	if (err < 0)
		SH_ERR("I2c Write Fail!\n");
	power_pin_sw(sensor,SWITCH_POWER_SYS, 0);
	return count;
}

static void read_calib_info(struct CWMCU_T *sensor)
{
	uint8_t data[24] = {0};
	int status = 0;
	uint16_t *ptr;
	ptr = (uint16_t *)data;

	if (sensor->mcu_mode == CW_BOOT) {
		SH_LOG("mcu_mode = boot, func:%s, line:%d\n", __func__, __LINE__);
                return;
	}

	if(set_calib_cmd(sensor, CALIB_CHECK_STATUS, sensor->cal_id, sensor->cal_type)){
		SH_ERR("I2c Write Fail!\n");
		return;
	}

	if(sensors_calib_status(sensor,  sensor->cal_id, &status) >=0){
		SH_LOG("Calib id:%d:status:%d\n", sensor->cal_id , status);
		sensor->cal_status = status;
		if(status ==CALIB_STATUS_PASS){
			ptr[0] =  (uint16_t)sensor->cal_id;
			cw_send_event(sensor,NonWakeUpHandle,CALIBRATOR_UPDATE,data);
		}
	}
    return ;

}

static void read_error_code(struct CWMCU_T *sensor)
{
    uint8_t data[4] = {0};
    int8_t *ptr;
    int err;
    ptr = (int8_t *)data;
    err = CWMCU_I2C_R(sensor, RegMapR_ErrorCode, data, 4);
    if (err < 0)
        SH_ERR("I2c Write Fail!\n");
    if(ptr[0] ==ERR_TASK_BLOCK){
        SH_LOG("ERR_TASK_BLOCK\n");
    }
    SH_LOG("%s:%d,%d,%d,%d)\n",__FUNCTION__ , ptr[0], ptr[1], ptr[2], ptr[3]);
    
}

static ssize_t get_raw_data0(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[6];
	uint16_t *ptr;
	int err;
	ptr = (uint16_t *)data;
	SH_FUN();
	if (CW_BOOT == sensor->mcu_mode)
	{
		SH_LOG("mcu_mode == CW_BOOT!\n");
		return 0;
	}
	power_pin_sw(sensor,SWITCH_POWER_SYS, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_GetAccelerationRawData, data, 6);
	power_pin_sw(sensor,SWITCH_POWER_SYS, 0);
	if (err < 0){
		SH_ERR("read RegMapR_GetAccelerationRawData failed!\n");
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}
static ssize_t get_raw_data1(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[6];
	uint16_t *ptr;
	int err;
	ptr = (uint16_t *)data;
	SH_FUN();
	if (CW_BOOT == sensor->mcu_mode)
	{
		SH_LOG("mcu_mode == CW_BOOT!\n");
		return 0;
	}
	power_pin_sw(sensor,SWITCH_POWER_SYS, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_GetMagneticRawData, data, 6);
	power_pin_sw(sensor,SWITCH_POWER_SYS, 0);
	if (err < 0){
		SH_LOG("RawData1:%u,%u,%u)\n", ptr[0], ptr[1], ptr[2]);
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}
static ssize_t get_raw_data2(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[6];
	uint16_t *ptr;
	int err;
	ptr = (uint16_t *)data;
	SH_FUN();
	if (CW_BOOT == sensor->mcu_mode)
	{
		SH_LOG("mcu_mode == CW_BOOT!\n");
		return 0;
	}
	power_pin_sw(sensor,SWITCH_POWER_SYS, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_GetGyroRawData, data, 6);
	power_pin_sw(sensor,SWITCH_POWER_SYS, 0);
	if (err < 0){
		SH_LOG("RawData2:%u,%u,%u)\n", ptr[0], ptr[1], ptr[2]);
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}

static ssize_t get_raw_data3(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[6];
	uint16_t *ptr;
	int err;
	ptr = (uint16_t *)data;
	SH_FUN();
	if (CW_BOOT == sensor->mcu_mode)
	{
		SH_LOG("mcu_mode == CW_BOOT!\n");
		return 0;
	}
	power_pin_sw(sensor,SWITCH_POWER_SYS, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_GetLightRawData, data, 6);
	power_pin_sw(sensor,SWITCH_POWER_SYS, 0);
	if (err < 0){
		SH_LOG("RawData3:%u,%u,%u)\n", ptr[0], ptr[1], ptr[2]);
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}

static ssize_t get_raw_data4(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[6];
	uint16_t *ptr;
	int err;
	ptr = (uint16_t *)data;
	SH_FUN();
	if (CW_BOOT == sensor->mcu_mode)
	{
		SH_LOG("mcu_mode == CW_BOOT!\n");
		return 0;
	}
	power_pin_sw(sensor,SWITCH_POWER_SYS, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_GetProximityRawData, data, 6);
	power_pin_sw(sensor,SWITCH_POWER_SYS, 0);
	if(err < 0){
		SH_LOG("RawData4:%u,%u,%u)\n", ptr[0], ptr[1], ptr[2]);
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}

static ssize_t get_mag_special_data(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct CWMCU_T *sensor = dev_get_drvdata(dev);
    uint8_t data[64];
    uint16_t *ptr;
    int err;
    ptr = (uint16_t *)data;
    SH_FUN();
    if (CW_BOOT == sensor->mcu_mode)
    {
        SH_LOG("mcu_mode == CW_BOOT!\n");
        return 0;
    }
    power_pin_sw(sensor,SWITCH_POWER_SYS, 1);
    err = CWMCU_I2C_R(sensor, RegMapR_MagSpecialData, data, 64);
    power_pin_sw(sensor,SWITCH_POWER_SYS, 0);
    memcpy(buf,data,64);
    return 64;
}

#ifndef CWMCU_CALIB_SAVE_IN_FLASH
static void reload_calib_data(struct CWMCU_T *sensor)
{
    int i;
    for(i = 0;i < DRIVER_ID_END ; i ++)
    {
        if(sensor->calibratorUpdate[i])
        {
            sensors_calib_data_write(sensor, i, sensor->calibratordata[i]);
            usleep_range(10000, 10000);
        }
    }
}
#endif

static void cwmcu_reinit(struct CWMCU_T *sensor)
{
#ifndef CWMCU_CALIB_SAVE_IN_FLASH
    reload_calib_data(sensor);
#endif
    check_enable_list(sensor);
    cwmcu_kernel_status(sensor,KERNEL_RESUME);
}
/*set the ID_EN gpio status
*  A: the gpio id
*  B: the value of gpio.
*/
static ssize_t mcu_gpio_set(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[2];
	int temp[2] = {0};
	int i,err;

	if (sensor->mcu_mode == CW_BOOT){
		SH_ERR("mcu_mode == CW_BOOT!\n");
		return count;
	}

	sscanf(buf, "%d %d\n",&temp[0], &temp[1]);

	for (i = 0 ; i < 2; i++)
		data[i] = (uint8_t)temp[i];

	power_pin_sw(sensor,SWITCH_POWER_MCU_GPIO, 1);
	err = CWMCU_I2C_W(sensor, RegMapW_GPIO_IDEN, data, 2);
	if (err < 0)
		SH_ERR("I2c Write Fail!\n");
	power_pin_sw(sensor,SWITCH_POWER_MCU_GPIO, 0);
	return count;
}
/* show the ID_EN gpio status and serialport status
*  A:gpio id
*  B:the status of gpio
*  C:the serialport status
*/
static ssize_t mcu_gpio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
	uint8_t data[2];
	int err;
	SH_FUN();
	if (CW_BOOT == sensor->mcu_mode)
	{
		SH_LOG("mcu_mode == CW_BOOT!\n");
		return 0;
	}
	power_pin_sw(sensor,SWITCH_POWER_MCU_GPIO, 1);
	err = CWMCU_I2C_R(sensor, RegMapR_GPIO_IDEN, data, 2);
	SH_LOG("GPIO:%u,Status:%u,Err:%d)\n", data[0],data[1],err);
	power_pin_sw(sensor,SWITCH_POWER_MCU_GPIO, 0);
	return sprintf(buf, "%u %u  %d \n", data[0], data[1],check_cable_status());
}

static struct device_attribute attributes[] = {
	__ATTR(enable, 0664,  active_show, active_set),
	__ATTR(batch, 0220, NULL, batch_set),
	__ATTR(flush, 0220, NULL, flush_set),
	__ATTR(mcu_mode, 0664, mcu_mode_show, mcu_model_set),
	__ATTR(calibrator_cmd, 0664,  get_calibrator_cmd, set_calibrator_cmd),
	__ATTR(calibrator_data, 0664, get_calibrator_data, set_calibrator_data),
	__ATTR(calibrator_status, 0444,  get_calibrator_status, NULL),
	__ATTR(firmware_update_i2c, 0664, get_firmware_update_i2, set_firmware_update_i2),
	__ATTR(firmware_update_cmd, 0220, NULL, set_firmware_update_cmd),
	__ATTR(firmware_update_data, 0220, NULL, set_firmware_update_data),
	__ATTR(firmware_update_status, 0444, get_firmware_update_status, NULL),
	__ATTR(version, 0444,  version_show, NULL),
	__ATTR(library_version, 0444,  library_version_show, NULL),
	__ATTR(timestamp, 0444, timestamp_show, NULL),
	__ATTR(sys_cmd, 0220,  NULL, set_sys_cmd),
	__ATTR(raw_data0, 0444, get_raw_data0, NULL),
	__ATTR(raw_data1, 0444, get_raw_data1, NULL),
	__ATTR(raw_data2, 0444, get_raw_data2, NULL),
	__ATTR(raw_data3, 0444, get_raw_data3, NULL),
	__ATTR(raw_data4, 0444, get_raw_data4, NULL),
	__ATTR(mag_special_data, 0444, get_mag_special_data, NULL),
	__ATTR(sensor_debug,0664, sensor_debug_show,sensor_debug_store),
	__ATTR(mcu_gpio,0664, mcu_gpio_show,mcu_gpio_set),
};

static void CWMCU_IRQ(struct CWMCU_T *sensor)
{
	uint8_t temp[2] = {0};
	uint8_t data_event[24] = {0};

	if (sensor->mcu_mode == CW_BOOT) {
		SH_LOG("mcu_mode = boot, func:%s, line:%d\n", __func__, __LINE__);
                return;
	}

	sensor->interrupt_status = 0;
	if (CWMCU_I2C_R(sensor, RegMapR_InterruptStatus, temp, 2) >= 0)
	{
		sensor->interrupt_status = (u32)temp[1] << 8 | (u32)temp[0];

		if (sensor->debug_log & (1<<D_IRQ))
			SH_LOG("interrupt_status:%d\n", sensor->interrupt_status);

		if( sensor->interrupt_status >= (1<<IRQ_MAX_SIZE)){
			sensor->interrupt_status = 0;
			SH_LOG("interrupt_status > IRQ_MAX_SIZE func:%s, line:%d\n", __func__, __LINE__);
		}
	}
	else
	{
		SH_ERR("check interrupt_status failed\n");
		return;
	}

	if (sensor->interrupt_status & (1<<IRQ_INIT))
	{
		cwmcu_reinit(sensor);
		cw_send_event(sensor, NonWakeUpHandle, MCU_REINITIAL, data_event);
	}

	if (sensor->interrupt_status & (1<<IRQ_GESTURE))
	{
		cwmcu_read_gesture(sensor);
	}

	if ((sensor->interrupt_status & (1<<IRQ_BATCH_TIMEOUT)) ||(sensor->interrupt_status & (1<<IRQ_BATCH_FULL)) )
	{
		cwmcu_read_buff(sensor,WakeUpHandle);
	}

	if (sensor->interrupt_status & (1<<IRQ_INFO))
	{
		read_mcu_info(sensor);
	}
	if (sensor->interrupt_status & (1<<IRQ_CALIB))
	{
		read_calib_info(sensor);
	}
    if (sensor->interrupt_status & (1<<IRQ_ERROR)) 
    {
        read_error_code(sensor);
    }
}

static int CWMCU_suspend(struct device *dev)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&sensor->delay_work);
	power_pin_sw(sensor,SWITCH_POWER_PROBE, 1);
	cwmcu_kernel_status(sensor,KERNEL_SUSPEND);
	power_pin_sw(sensor,SWITCH_POWER_PROBE, 0);
	return 0;
}

static int CWMCU_resume(struct device *dev)
{
	struct CWMCU_T *sensor = dev_get_drvdata(dev);
    
	power_pin_sw(sensor,SWITCH_POWER_PROBE, 1);
	cwmcu_kernel_status(sensor,KERNEL_RESUME);
	power_pin_sw(sensor,SWITCH_POWER_PROBE, 0);
	queue_delayed_work(sensor->driver_wq, &sensor->delay_work,
	msecs_to_jiffies(atomic_read(&sensor->delay)));
	return 0;
}

/*=======iio device reg=========*/
static void iio_trigger_work(struct irq_work *work)
{
	struct CWMCU_T *mcu_data = container_of((struct irq_work *)work, struct CWMCU_T, iio_irq_work);

	iio_trigger_poll(mcu_data->trig, iio_get_time_ns());
}

static irqreturn_t cw_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct CWMCU_T *mcu_data = iio_priv(indio_dev);

#ifdef CWMCU_MUTEX
	mutex_lock(&mcu_data->mutex_lock);
#endif
	iio_trigger_notify_done(mcu_data->indio_dev->trig);
#ifdef CWMCU_MUTEX
	mutex_unlock(&mcu_data->mutex_lock);
#endif

	return IRQ_HANDLED;
}

static const struct iio_buffer_setup_ops cw_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static int cw_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct CWMCU_T *mcu_data = iio_priv(indio_dev);

	if (!atomic_cmpxchg(&mcu_data->pseudo_irq_enable, 0, 1)) {
		SH_FUN();
		cancel_work_sync(&mcu_data->work);
		queue_work(mcu_data->driver_wq, &mcu_data->work);
		cancel_delayed_work_sync(&mcu_data->delay_work);
		queue_delayed_work(mcu_data->driver_wq, &mcu_data->delay_work, 0);
	}

	return 0;
}

static int cw_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct CWMCU_T *mcu_data = iio_priv(indio_dev);

	if (atomic_cmpxchg(&mcu_data->pseudo_irq_enable, 1, 0)) {
		cancel_work_sync(&mcu_data->work);
		cancel_delayed_work_sync(&mcu_data->delay_work);
		SH_FUN();
	}
	return 0;
}

static int cw_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		cw_pseudo_irq_enable(indio_dev);
	else
		cw_pseudo_irq_disable(indio_dev);
	return 0;
}

static int cw_data_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = (struct iio_dev *)iio_trigger_get_drvdata(trig);
#ifdef CWMCU_MUTEX
	struct CWMCU_T *mcu_data = iio_priv(indio_dev);
	mutex_lock(&mcu_data->mutex_lock);
#endif
	cw_set_pseudo_irq(indio_dev, state);
#ifdef CWMCU_MUTEX
	mutex_unlock(&mcu_data->mutex_lock);
#endif

	return 0;
}

static const struct iio_trigger_ops cw_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &cw_data_rdy_trigger_set_state,
};

static int cw_probe_trigger(struct iio_dev *iio_dev)
{
	struct CWMCU_T *mcu_data = iio_priv(iio_dev);
	int ret;

	iio_dev->pollfunc = iio_alloc_pollfunc(&iio_pollfunc_store_time,
			&cw_trigger_handler, IRQF_ONESHOT, iio_dev,
			"%s_consumer%d", iio_dev->name, iio_dev->id);
	if (iio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	mcu_data->trig = iio_trigger_alloc("%s-dev%d", iio_dev->name, iio_dev->id);
	if (!mcu_data->trig)
	{
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	mcu_data->trig->dev.parent = &mcu_data->client->dev;
	mcu_data->trig->ops = &cw_trigger_ops;
	iio_trigger_set_drvdata(mcu_data->trig, iio_dev);

	ret = iio_trigger_register(mcu_data->trig);
	if (ret)
		goto error_free_trig;

	return 0;

error_free_trig:
	iio_trigger_free(mcu_data->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(iio_dev->pollfunc);
error_ret:
	return ret;
}

static int cw_probe_buffer(struct iio_dev *iio_dev)
{
	int ret;
	struct iio_buffer *buffer;

	buffer = iio_kfifo_allocate(iio_dev);
	if (!buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}

	buffer->scan_timestamp = true;
	iio_dev->buffer = buffer;
	iio_dev->setup_ops = &cw_buffer_setup_ops;
	iio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	ret = iio_buffer_register(iio_dev, iio_dev->channels,
				  iio_dev->num_channels);
	if (ret)
		goto error_free_buf;

	iio_scan_mask_set(iio_dev, iio_dev->buffer, CW_SCAN_ID);
	iio_scan_mask_set(iio_dev, iio_dev->buffer, CW_SCAN_X);
	iio_scan_mask_set(iio_dev, iio_dev->buffer, CW_SCAN_Y);
	iio_scan_mask_set(iio_dev, iio_dev->buffer, CW_SCAN_Z);
	return 0;

error_free_buf:
	iio_kfifo_free(iio_dev->buffer);
error_ret:
	return ret;
}

static int cw_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
							int *val, int *val2, long mask)
{
	struct CWMCU_T *mcu_data = iio_priv(indio_dev);
	int ret = -EINVAL;

	if (chan->type != IIO_ACCEL)
		return ret;

#ifdef CWMCU_MUTEX
	mutex_lock(&mcu_data->mutex_lock);
#endif
	switch (mask) {
	case 0:
		*val = mcu_data->iio_data[chan->channel2 - IIO_MOD_X];
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		/* Gain : counts / uT = 1000 [nT] */
		/* Scaling factor : 1000000 / Gain = 1000 */
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;

		default:
			break;
	}
#ifdef CWMCU_MUTEX
	mutex_unlock(&mcu_data->mutex_lock);
#endif
	return ret;
}


#define CW_CHANNEL(axis)					\
{										   \
	.type = IIO_ACCEL,					  \
	.modified = 1,						  \
	.channel2 = axis+1,					 \
	.info_mask = BIT(IIO_CHAN_INFO_SCALE),  \
	.scan_index = axis,					 \
	.scan_type = IIO_ST('u', 32, 32, 0)	 \
}

static const struct iio_chan_spec cw_channels[] = {
	CW_CHANNEL(CW_SCAN_ID),
	CW_CHANNEL(CW_SCAN_X),
	CW_CHANNEL(CW_SCAN_Y),
	CW_CHANNEL(CW_SCAN_Z),
	IIO_CHAN_SOFT_TIMESTAMP(CW_SCAN_TIMESTAMP)
};

static const struct iio_info cw_info = {
	.read_raw = &cw_read_raw,
	.driver_module = THIS_MODULE,
};

static void cwmcu_delwork_report(struct work_struct *work)
{
    struct CWMCU_T *sensor = container_of((struct delayed_work *)work,
        struct CWMCU_T, delay_work);

    if (atomic_read(&sensor->pseudo_irq_enable)) {
        if (sensor->mcu_mode == CW_BOOT) {
            //printk("%s:%s:(sensor->mcu_mode = CW_BOOT)\n",LOG_TAG_KERNEL ,__FUNCTION__);
        }else{
            //power_pin_sw(sensor,SWITCH_POWER_POLLING, 1);
            CWMCU_POLLING(sensor);
           // power_pin_sw(sensor,SWITCH_POWER_POLLING, 0);

        }
	queue_delayed_work(sensor->driver_wq, &sensor->delay_work,
	msecs_to_jiffies(atomic_read(&sensor->delay)));
    }
}

static int create_sysfs_interfaces(struct CWMCU_T *mcu_data)
{
	int i;
	int res;

/*
	SH_FUN();
	mcu_data->sensor_class = class_create(THIS_MODULE, "cywee_sensorhub");
	if (IS_ERR(mcu_data->sensor_class))
		return PTR_ERR(mcu_data->sensor_class);
*/
	mcu_data->sensor_dev = device_create(meizu_class, NULL, 0,
						 "%s", "mx_hub");
	if (IS_ERR(mcu_data->sensor_dev)) {
		res = PTR_ERR(mcu_data->sensor_dev);
		goto err_device_create;
	}

	res = dev_set_drvdata(mcu_data->sensor_dev, mcu_data);
	if (res)
		goto err_set_drvdata;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(mcu_data->sensor_dev, attributes + i))
			goto error;

	res = sysfs_create_link(&mcu_data->sensor_dev->kobj,
				&mcu_data->indio_dev->dev.kobj, "iio");
	if (res < 0)
		goto error;

	return 0;

error:
	while (--i >= 0)
		device_remove_file(mcu_data->sensor_dev, attributes + i);
err_set_drvdata:
	put_device(mcu_data->sensor_dev);
	device_unregister(mcu_data->sensor_dev);
err_device_create:
	//class_destroy(mcu_data->sensor_class);
	return res;
}

#ifdef CWMCU_INTERRUPT
static irqreturn_t CWMCU_interrupt_thread(int irq, void *data)
{
	struct CWMCU_T *sensor = data;
	 //hub_log(KERN_DEBUG "CwMcu:%s in\n", __func__);
	
	if (sensor->mcu_mode == CW_BOOT) {
		//hub_log("%s:%s:(sensor->mcu_mode = CW_BOOT)\n",LOG_TAG_KERNEL ,__FUNCTION__);
		return IRQ_HANDLED;
	}
	queue_work(sensor->driver_wq, &sensor->work);

	return IRQ_HANDLED;
}

static void cwmcu_work_report(struct work_struct *work)
{
  
   struct CWMCU_T *sensor = container_of((struct work_struct *)work,
			struct CWMCU_T, work);

	if (sensor->mcu_mode == CW_BOOT) {
		printk("%s:%s:(sensor->mcu_mode = CW_BOOT)\n",LOG_TAG_KERNEL ,__FUNCTION__);
		return;
	}

	power_pin_sw(sensor,SWITCH_POWER_INTERRUPT, 1);
	CWMCU_IRQ(sensor);
	power_pin_sw(sensor,SWITCH_POWER_INTERRUPT, 0);
}
#endif

static int cwstm_parse_dt(struct device *dev, struct CWMCU_T *sensor)
{

	int retval;
	u32 value;
	const char *name;
	struct device_node *np = dev->of_node;
	struct pinctrl *pinctrl;

	sensor->irq_gpio = of_get_named_gpio_flags(np,
			"mx-hub,irq-gpio", 0, NULL);
	
	retval = of_property_read_u32(np, "mx-hub,irq-flags", &value);
	if(retval < 0)
		return retval;
	else
		sensor->irq_flags = value;

	pinctrl = devm_pinctrl_get_select(dev, "sensorhub_irq");
	if(IS_ERR(pinctrl)) 
	     printk( "failed to get tp irq pinctrl - ON");

	retval = of_property_read_string(np, "mx-hub,pwr28-reg-name", &name);
	if(retval == -EINVAL)
		sensor->pwr_reg_name = NULL;
	else
		sensor->pwr_reg_name = name;

	retval = of_property_read_string(np, "mx-hub,pwr18-reg-name", &name);
	if(retval == -EINVAL)
		sensor->pwr18_reg_name = NULL;
	else
		sensor->pwr18_reg_name = name;

	if(of_property_read_bool(np, "mx-hub,wkup-gpio")) {
		sensor->wakeup_gpio = of_get_named_gpio_flags(np,
				"mx-hub,wkup-gpio", 0, NULL);
	}else {
		sensor->wakeup_gpio = -1;
	}

	if(of_property_read_bool(np, "mx-hub,reset-gpio")) {
		sensor->reset_gpio = of_get_named_gpio_flags(np,
				"mx-hub,reset-gpio", 0, NULL);
	}else {
		sensor->reset_gpio = -1;
	}

	if(of_property_read_bool(np, "mx-hub,boot-gpio")) {
		sensor->boot_gpio = of_get_named_gpio_flags(np,
				"mx-hub,boot-gpio", 0, NULL);
	}else {
		sensor->boot_gpio = -1;
	}

	if(of_property_read_bool(np, "mx-hub,sleep-gpio")) {
		sensor->sleep_mcu_gpio = of_get_named_gpio_flags(np,
				"mx-hub,sleep-gpio", 0, NULL);
	}else {
		sensor->sleep_mcu_gpio = -1;
	}

	if(of_property_read_bool(np, "mx-hub,busy-gpio")) {
		sensor->mcu_busy_gpio = of_get_named_gpio_flags(np,
				"mx-hub,busy-gpio", 0, NULL);
	}else {
		sensor->mcu_busy_gpio = -1;
	}
	return 0;

}

static int mx_hub_gpio_setup(int gpio, unsigned char *buf, bool config, int dir, int state)
{
	int retval = 0;

	if(config) {
		retval = gpio_request(gpio, buf);
		if(retval) {
			pr_err("%s: Failed to get gpio %d (code: %d)",
				__func__, gpio, retval);
			return retval;
			} 
		if(dir == 0)
			retval = gpio_direction_input(gpio);
		else 
			retval = gpio_direction_output(gpio, state);
		if(retval) {
			pr_err("%s: Failed to set gpio %d direction",
				__func__, gpio);
			return retval;
			}
	}else {
				gpio_free(gpio);
	}
	
	return 0;

}

static int mx_hub_set_gpio(struct CWMCU_T *sensor )
{
	int retval;

	retval = mx_hub_gpio_setup(sensor->irq_gpio, "MCU_IRQ", 1, 0, 0);
	if (retval < 0) {
		hub_log("%s: Failed to configure attention GPIO	1 %d\n",
				__func__ , sensor->irq_gpio);
		goto err_gpio_irq;
	}

	if(sensor->wakeup_gpio >=0) {
		retval = mx_hub_gpio_setup(sensor->wakeup_gpio, "MCU_WAKEUP", 1, 1, 0);
		if (retval < 0) {
			hub_log("%s: Failed to configure attention GPIO	2 %d\n",
					__func__ , sensor->wakeup_gpio);
			goto err_gpio_wakeup;
		}
	}

	if(sensor->reset_gpio >=0) {
		retval = mx_hub_gpio_setup(sensor->reset_gpio, "MCU_RESET", 1, 1, 1);
		if (retval < 0) {
			hub_log("%s: Failed to configure attention GPIO 	3 %d\n",
					__func__ , sensor->reset_gpio);
			goto err_gpio_reset;
		}
	}

	if(sensor->boot_gpio >=0) {
		retval = mx_hub_gpio_setup(sensor->boot_gpio, "MCU_BOOT", 1, 1, 1);
		if (retval < 0) {
			hub_log("%s: Failed to configure attention GPIO  4 %d\n",
					__func__ , sensor->boot_gpio);
			goto err_gpio_boot;
		}
	}

	if(sensor->sleep_mcu_gpio >= 0) {
	    retval = mx_hub_gpio_setup(sensor->sleep_mcu_gpio, "MCU_SLEEP", 1, 0, 0);
		if (retval < 0) {
			hub_log("%s: Failed to configure attention GPIO  sleep %d\n",
					__func__ , sensor->sleep_mcu_gpio);
			goto err_gpio_sleep;
		}
	}
	//hub_log("boot_gpio value is %d\n", gpio_get_value(sensor->boot_gpio));
	usleep_range(500, 1000);
	if(sensor->boot_gpio >= 0) 
		gpio_set_value(sensor->boot_gpio, 0);

	usleep_range(10000, 15000);
	if(sensor->reset_gpio >= 0) {
		gpio_set_value(sensor->reset_gpio, 0);
		usleep_range(10000, 11000);
		gpio_set_value(sensor->reset_gpio, 1);
		usleep_range(20000, 20000);
		gpio_direction_input(sensor->reset_gpio);
	}

err_gpio_sleep:
	if(sensor->boot_gpio >= 0)
		gpio_free(sensor->boot_gpio);

err_gpio_boot:
	if(sensor->reset_gpio >= 0)
		gpio_free(sensor->reset_gpio);

err_gpio_reset:
	if(sensor->wakeup_gpio >= 0)
		gpio_free(sensor->wakeup_gpio);

err_gpio_wakeup:
	gpio_free(sensor->irq_gpio);

err_gpio_irq:
	return retval;
}



static void cwmcu_remove_trigger(struct iio_dev *indio_dev)
{
	struct CWMCU_T *mcu_data = iio_priv(indio_dev);

	iio_trigger_unregister(mcu_data->trig);
	iio_trigger_free(mcu_data->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static void cwmcu_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}


static void cwmcu_hw_config_init(struct CWMCU_T *sensor)
{
	int i = 0;
	int j = 0;

	sensor->initial_hw_config = 0;
	for(i = 0; i < HANDLE_ID_END; i++)
	{
		sensor->enabled_list[i] = 0;
		for(j = 0;j<SENSORS_ID_END;j++)
		{
			sensor->sensors_info[i][j].en = 0;
			sensor->sensors_info[i][j].mode= 0;
			sensor->sensors_info[i][j].rate = 0;
			sensor->sensors_info[i][j].timeout= 0;
		}
	}
	sensor->interrupt_status = 0;
	sensor->power_on_list = 0;
	sensor->cal_cmd = 0;
	sensor->cal_type = 0;
	sensor->cal_id = 0;
	sensor->debug_log = 0;
	for(i = 0;i<DRIVER_ID_END;i++){
		sensor->hw_info[i].hw_id=0;

		sensor->calibratorUpdate[i]=0;
		for(j = 0;j<30;j++){
			sensor->calibratordata[i][j]=0;
		}
	}
}

static int CWMCU_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct CWMCU_T *mcu;
	struct iio_dev *indio_dev;
	int error;
	uint8_t data[2] = {3, 1};

	//hub_log("%s:%s:(sensor->mcu_mode = CW_BOOT)\n",LOG_TAG_KERNEL ,__FUNCTION__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "CwMcu: i2c_check_functionality error\n");
		return -EIO;
	}

	indio_dev = iio_device_alloc(sizeof(*mcu));
	if (!indio_dev) {
		hub_log("%s: iio_device_alloc failed\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, indio_dev);

	indio_dev->name = CWMCU_I2C_NAME;
	indio_dev->dev.parent = &client->dev;
	indio_dev->info = &cw_info;
	indio_dev->channels = cw_channels;
	indio_dev->num_channels = ARRAY_SIZE(cw_channels);
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	mcu = iio_priv(indio_dev);
	mcu->client = client;
	mcu->indio_dev = indio_dev;

	atomic_set(&mcu->delay, 20);
	INIT_DELAYED_WORK(&mcu->delay_work, cwmcu_delwork_report);
#ifdef CWMCU_MUTEX
	mutex_init(&mcu->mutex_lock);
	mutex_init(&mcu->mutex_wakeup_gpio);
	mcu->supend_flag = 1;
#endif

	// parse the dts information
	error = cwstm_parse_dt(&client->dev, mcu);
	if (error < 0) {
		pr_err("failed to parse device tree: %d\n", error);
		goto err_parse_dt;
	}

	if((mcu->pwr_reg_name != NULL) && (mcu->pwr_reg_name != 0)) {
		mcu->sensor_pwr = regulator_get(&client->dev, mcu->pwr_reg_name);
		if(!IS_ERR(mcu->sensor_pwr)) {
			error = regulator_enable(mcu->sensor_pwr);
		}else{
			dev_err(&client->dev, "failed to get regulator (0x%p).\n",mcu->sensor_pwr);
		}
	}
	usleep_range(2000, 3000);
		
	/* mcu reset */
	error = mx_hub_set_gpio(mcu);	
	if(error < 0) {
		hub_log(" Failed to set up GPIO's \n");
	};

	mcu->mcu_mode = CW_NORMAL;

	error = cw_probe_buffer(indio_dev);
	if (error) {
		hub_log("%s: iio yas_probe_buffer failed\n", __func__);
		goto error_free_dev;
	}
	error = cw_probe_trigger(indio_dev);
	if (error) {
		hub_log("%s: iio yas_probe_trigger failed\n", __func__);
		goto error_remove_buffer;
	}
	error = iio_device_register(indio_dev);
	if (error) {
		hub_log("%s: iio iio_device_register failed\n", __func__);
		goto error_remove_trigger;
	}

	init_irq_work(&mcu->iio_irq_work, iio_trigger_work);

	cwmcu_hw_config_init(mcu);

	mcu->driver_wq = create_singlethread_workqueue("cywee_mcu");
	i2c_set_clientdata(client, mcu);
	pm_runtime_enable(&client->dev);
	INIT_WORK(&mcu->work, cwmcu_work_report);

#ifdef CWMCU_INTERRUPT
	mcu->client->irq = gpio_to_irq(mcu->irq_gpio);

	if (mcu->client->irq > 0) {
		error = request_threaded_irq(mcu->client->irq, NULL,
						   CWMCU_interrupt_thread,
						   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						   "cwmcu", mcu);
		if (error < 0) {
				pr_err("request irq %d failed\n", mcu->client->irq);
				goto exit_destroy_mutex;
		}

		error = enable_irq_wake(mcu->client->irq);
		if (error < 0)
		    hub_log("[CWMCU] could not enable irq as wakeup source %d\n", error);
	} else {
		 hub_log("[CWMCU] set irq source  failed \n");
	}
#endif

	/*check the serialport status, if serialport isn't on, then set the ID_EN gpio high*/
	if(!check_cable_status()){
		power_pin_sw(mcu,SWITCH_POWER_MCU_GPIO, 1);
		error = CWMCU_I2C_W(mcu, RegMapW_GPIO_IDEN, data, 2);
		if (error < 0)
			SH_ERR("I2c Write Fail!\n");
		power_pin_sw(mcu,SWITCH_POWER_MCU_GPIO, 0);
	}

	error = create_sysfs_interfaces(mcu);
	if (error)
		goto err_free_mem;

	return 0;

err_free_mem:
	iio_device_unregister(indio_dev);
error_remove_trigger:
	cwmcu_remove_trigger(indio_dev);
error_remove_buffer:
	cwmcu_remove_buffer(indio_dev);
error_free_dev:
err_parse_dt:
#ifdef CWMCU_INTERRUPT
exit_destroy_mutex:
#endif
	iio_device_free(indio_dev);
	i2c_set_clientdata(client, NULL);
	return error;
}
/*set the ID_EN gpio low when shutdown*/
static void CWMCU_shutdown(struct i2c_client *client)
{
	struct CWMCU_T *sensor = i2c_get_clientdata(client);
	uint8_t data[2] = {3, 0};
	power_pin_sw(sensor,SWITCH_POWER_SHUTDOWN, 1);
	cwmcu_kernel_status(sensor,KERNEL_SHUTDOWN);
	power_pin_sw(sensor,SWITCH_POWER_SHUTDOWN, 0);
	
	power_pin_sw(sensor,SWITCH_POWER_MCU_GPIO, 1);
	if(CWMCU_I2C_W(sensor, RegMapW_GPIO_IDEN, data, 2))
		SH_ERR("I2c Write Fail!\n");
	power_pin_sw(sensor,SWITCH_POWER_MCU_GPIO, 0);
}

static int CWMCU_i2c_remove(struct i2c_client *client)
{
	struct CWMCU_T *sensor = i2c_get_clientdata(client);
	kfree(sensor);
	return 0;
}

static struct of_device_id cwstm_match_table[] = {
	{ .compatible = "st,sensor_hub",},
	//{ .compatible = "cywee,sensor_hub",},	
	{ },
};

static const struct dev_pm_ops CWMCU_pm_ops = {
	.suspend = CWMCU_suspend,
	.resume = CWMCU_resume
};

static const struct i2c_device_id CWMCU_id[] = {
	{ CWMCU_I2C_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, CWMCU_id);

static struct i2c_driver CWMCU_driver = {
	.driver = {
		.name = CWMCU_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &CWMCU_pm_ops,
		.of_match_table = cwstm_match_table,
	},
	.probe	= CWMCU_i2c_probe,
	.shutdown = CWMCU_shutdown,
	.remove   = CWMCU_i2c_remove,
	.id_table = CWMCU_id,
};

static int __init CWMCU_i2c_init(void){
	return i2c_add_driver(&CWMCU_driver);
}

static void __exit CWMCU_i2c_exit(void){
	i2c_del_driver(&CWMCU_driver);
}

module_init(CWMCU_i2c_init);
module_exit(CWMCU_i2c_exit);

MODULE_DESCRIPTION("CWMCU I2C Bus Driver");
MODULE_AUTHOR("CyWee Group Ltd.");
MODULE_LICENSE("GPL");
