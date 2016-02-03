/* drivers/input/touchscreen/melfas_ts.c
 *
 * Copyright (C) 2010 Melfas, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG_PRINT 0
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/notifier.h>//XF
#include <linux/fb.h>//XF
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/melfas_ts.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

#include <linux/input/mt.h> // slot
#include <plat/gpio-cfg.h>
#include <linux/of_gpio.h>
#include "../../video/exynos/decon/decon.h"

#define DYNAMIC_PRINT_INTERFACE
#ifdef  DYNAMIC_PRINT_INTERFACE
#define printktp  sscr_print
#else
#define printktp(fmt, args...) //printk(KERN_DEBUG "[MELFAS]" fmt, ##args)
#endif

#ifdef TEST_MELFAS_IIC
#define TEST_TICKS  2000// 2s
struct delayed_work melfas_delayed_work;
#endif

#ifdef TOUCH_BOOSTER
#include <linux/pm_qos.h>
#endif

 struct melfas_ts_data
{
#ifdef TOUCH_BOOSTER
    struct delayed_work  dvfs_work;
    struct mutex dvfs_lock;
    struct pm_qos_request tsp_cpu_qos;
    struct pm_qos_request tsp_mif_qos;
    struct pm_qos_request tsp_int_qos;
#endif
    uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct melfas_tsi_platform_data *pdata;
	struct work_struct	pen_event_work;
    struct workqueue_struct	*ts_workqueue;
	uint32_t flags;
	int (*power)(int onoff);
	bool debug_enabled;
};

#ifdef TOUCH_BOOSTER
#define TOUCH_BOOSTER_TIME      300 
static bool dvfs_lock_status = false;
static bool press_status = false;
struct melfas_ts_data *global_ts_data;

static void TSP_dvfs_suspend(struct melfas_ts_data *ts_data)
{
	int ret;
    
    mutex_lock(&ts_data->dvfs_lock);

    if (dvfs_lock_status) {
  
  if (pm_qos_request_active(&ts_data->tsp_cpu_qos))                        
      pm_qos_update_request(&ts_data->tsp_cpu_qos, 0);
  else
      pm_qos_add_request(&ts_data->tsp_cpu_qos, PM_QOS_CPU_FREQ_MIN, 0);
             
  if (pm_qos_request_active(&ts_data->tsp_mif_qos)){
      pm_qos_update_request(&ts_data->tsp_mif_qos, 0);
      }
  else{
      pm_qos_add_request(&ts_data->tsp_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
  }
             
  if (pm_qos_request_active(&ts_data->tsp_int_qos))
      pm_qos_update_request(&ts_data->tsp_int_qos, 0);
  else
      pm_qos_add_request(&ts_data->tsp_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0); 

       
         dvfs_lock_status = false;
         press_status = false;
#ifdef TOUCH_BOOSTER_PRINT
         pr_info("[TSP] %s : TSP DVFS mode exit\n", __func__);
#endif
   }
   mutex_unlock(&ts_data->dvfs_lock);  
}

static void TSP_dvfs_off(struct work_struct *work)
{
        pr_info("[TSP] %s\n",__func__);
        int ret;
        struct melfas_ts_data *ts_data =container_of(work,
                struct melfas_ts_data, dvfs_work.work);
        
        mutex_lock(&ts_data->dvfs_lock);
        
        if (dvfs_lock_status && !press_status) {

        if (pm_qos_request_active(&ts_data->tsp_cpu_qos))                        
            pm_qos_update_request(&ts_data->tsp_cpu_qos, 0);
        else
            pm_qos_add_request(&ts_data->tsp_cpu_qos, PM_QOS_CPU_FREQ_MIN, 0);
                   
        if (pm_qos_request_active(&ts_data->tsp_mif_qos)){
            pm_qos_update_request(&ts_data->tsp_mif_qos, 0);
            }
        else{
            pm_qos_add_request(&ts_data->tsp_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
        }
                   
        if (pm_qos_request_active(&ts_data->tsp_int_qos))
            pm_qos_update_request(&ts_data->tsp_int_qos, 0);
        else
            pm_qos_add_request(&ts_data->tsp_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);   

               
        dvfs_lock_status = false;
#ifdef TOUCH_BOOSTER_PRINT
        pr_info("[TSP] %s : TSP DVFS mode exit \n",__func__);
#endif
        }
       mutex_unlock(&ts_data->dvfs_lock);
}

static int TSP_lock_dvfs(bool touch_point,struct melfas_ts_data *ts_data)
{
#ifdef TOUCH_BOOSTER_PRINT
    pr_info("[TSP] %s\n",__func__);
#endif
    if (touch_point) 
    {
       press_status = true;
       cancel_delayed_work(&ts_data->dvfs_work);
       schedule_delayed_work(&ts_data->dvfs_work,msecs_to_jiffies(TOUCH_BOOSTER_TIME));
       mutex_lock(&ts_data->dvfs_lock);
       if (!dvfs_lock_status && press_status) {  
               if(pm_qos_request_active(&ts_data->tsp_cpu_qos)) 
               {  
                    pr_info("[TSP] Qos Update\n");
                    pm_qos_update_request(&ts_data->tsp_cpu_qos, 1000000);
               }
               else 
               {
                    pr_info("[TSP] Qos Add\n");
                    pm_qos_add_request(&ts_data->tsp_cpu_qos, PM_QOS_CPU_FREQ_MIN, 1000000);
               }
               if (pm_qos_request_active(&ts_data->tsp_mif_qos))
                    pm_qos_update_request(&ts_data->tsp_mif_qos, 266000);                  
               else
                    pm_qos_add_request(&ts_data->tsp_mif_qos, PM_QOS_BUS_THROUGHPUT, 266000);
                                    
               if (pm_qos_request_active(&ts_data->tsp_int_qos))
                    pm_qos_update_request(&ts_data->tsp_int_qos, 133000);
               else
                    pm_qos_add_request(&ts_data->tsp_int_qos, PM_QOS_DEVICE_THROUGHPUT, 133000);
               dvfs_lock_status = true;
#ifdef TOUCH_BOOSTER_PRINT
          pr_info("[TSP] TSP DVFS mode enter\n");
#endif
       }
       mutex_unlock(&ts_data->dvfs_lock);  
       return 1;
    } 
    else
    {
       press_status = false; 
       return 0;
    }
}
static int TSP_init_dvfs(struct melfas_ts_data *ts_data)
{
    mutex_init(&ts_data->dvfs_lock);

    INIT_DELAYED_WORK(&ts_data->dvfs_work, TSP_dvfs_off);

    dvfs_lock_status = false;
    return 0;
} 

#endif

static int (*touchscreen_power_on)(void);
static int (*touchscreen_power_off)(void);
static bool first_boot_flag;/*Added by Huangwj for Unbalanced enable for IRQ3 20141224*/
#ifdef TOUCH_DELAYED_OPEN
static struct delayed_work touch_open_work;
static int touch_power_on(void)
{
	if (touchscreen_power_on)
                touchscreen_power_on();
	return 0;
}
#endif
static int fb_state_change(struct notifier_block *nb,
                unsigned long val, void *data)
{
        struct fb_event *evdata = data;
        unsigned int blank;
		struct decon_win *win = evdata->info->par;
		struct decon_device *decon = win->decon;
		int id = decon->id;

        if (val != FB_EVENT_BLANK)
                return 0;
		if (id != 0)
			return 0;

        blank = *(int *)evdata->data;

        switch (blank) {
        case FB_BLANK_POWERDOWN:
		if (touchscreen_power_off)
                	touchscreen_power_off();
                break;
        case FB_BLANK_UNBLANK:
#ifdef TOUCH_DELAYED_OPEN
		schedule_delayed_work( &touch_open_work, msecs_to_jiffies(200)); //delay 200mS to let LCD resume faster
#else
		if (touchscreen_power_on)
                	touchscreen_power_on();
#endif
                break;
        default:
                break;
        }

        return NOTIFY_OK;
}

static struct notifier_block fb_block = {
        .notifier_call = fb_state_change,
};
static struct i2c_client *this_client;

#if SLOT_TYPE
#define REPORT_MT(touch_number, x, y, area, pressure)			\
	do {								\
		input_mt_slot(ts->input_dev, touch_number);		\
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true); \
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);	\
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);	\
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, area); \
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, pressure); \
	} while (0)
#else
#define REPORT_MT(touch_number, x, y, area, pressure)			\
	do {								\
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, touch_number); \
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);	\
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);	\
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, area); \
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, pressure); \
		input_mt_sync(ts->input_dev);				\
	} while (0)
#endif


static struct muti_touch_info g_Mtouch_info[TS_MAX_TOUCH];

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif
extern int isc_fw_download(struct melfas_ts_data *info, const u8 *data, size_t len);
extern int isp_fw_download(const u8 *data, size_t len);

#if 1//copy from T11
static int melfas_i2c_read(struct i2c_client *client, u16 addr, u8 length, u8 *value)
{
	int ret;
	//u16 le_reg = cpu_to_le16(addr);
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = (u8 *)&addr,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = value,
		},
	};

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0){
		pr_err("[TSP] : read error \n");
		return ret;
	}
	if (ret != 2)
		pr_err("[TSP] : read length error \n");

	usleep_range(20, 40);
	//udelay(20);
	return ret;
	//return ret == 2 ? 0 : -EIO;
}

#elif 0//copy from H03W
inline s32 ts_read_data(struct i2c_client *client, u16 reg, u8 *values, u16 length)
{
	s32 ret;
	if((ret = i2c_master_send(client , (u8*)&reg , 1)) < 0)	return ret;	// select register
	//udelay(50);		// for setup tx transaction.
	if((ret = i2c_master_recv(client , values , length)) < 0) return ret;
	udelay(20);
	return length;
}

static int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	int ret = -1;
	int i;
	ret= ts_read_data (client, addr, value, length);

	if (ret < 0)
	        pr_err("[TSP] : read error \n");

	if (ret != length)
		pr_err("[TSP] : read length error,ret = %d,length = %d\n",ret,length);
	/*
	  else{

	  for (i=0;i<length;i++,value++)
	  printk("value = %d\n",*value);
	  }*/
	return ret;

}
#else //melfas
static int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	int ret = -1;

	msg.addr = client->addr;
	msg.flags = 0x00;
	msg.len = 1;
	msg.buf = (u8 *) & addr;

	ret = i2c_transfer(adapter, &msg, 1);

	if (ret >= 0)
	{
		msg.addr = client->addr;
		msg.flags = I2C_M_RD;
		msg.len = length;
		msg.buf = (u8 *) value;

		ret = i2c_transfer(adapter, &msg, 1);
	}

	if (ret < 0)
	{
		pr_err("[TSP] : read error : [%d]", ret);
	}

	return ret;
}
#endif
#if 0
static int melfas_i2c_write(struct i2c_client *client, char *buf, int length)
{
	int i;
	char data[TS_WRITE_REGS_LEN];

	if (length > TS_WRITE_REGS_LEN)
	{
		pr_err("[TSP] %s :size error \n", __FUNCTION__);
		return -EINVAL;
	}

	for (i = 0; i < length; i++)
		data[i] = *buf++;

	i = i2c_master_send(client, (char *) data, length);

	if (i == length)
		return length;
	else
	{
		pr_err("[TSP] :write error : [%d]", i);
		return -EIO;
	}
}
#endif
#if SLOT_TYPE
static void melfas_ts_release_all_finger(struct melfas_ts_data *ts)
{
	int i;
#if DEBUG_PRINT
	pr_info("[TSP] %s\n", __func__);
#endif
	for (i = 0; i < TS_MAX_TOUCH; i++)
	{
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
	}
	input_sync(ts->input_dev);
}
#else
static void melfas_ts_release_all_finger(struct melfas_ts_data *ts)
{
#if DEBUG_PRINT
	pr_info("[TSP] %s\n", __func__);
#endif

	int i;
	for(i=0; i<TS_MAX_TOUCH; i++)
	{
		if(-1 == g_Mtouch_info[i].pressure)
			continue;

		if(g_Mtouch_info[i].pressure == 0)
			input_mt_sync(ts->input_dev);

		if(0 == g_Mtouch_info[i].pressure)
			g_Mtouch_info[i].pressure = -1;
	}
	input_sync(ts->input_dev);
}
#endif
#if 1
#if 1
static int check_firmware(struct melfas_ts_data *ts, u8 *val)
{
	int ret = 0;
	uint8_t i = 0;

	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		ret = melfas_i2c_read(ts->client, TS_READ_HW_VER_ADDR, 1, &val[0]);
		ret = melfas_i2c_read(ts->client, TS_READ_SW_VER_ADDR, 1, &val[1]);

		if (ret >= 0)
		{
			pr_info("[TSP] : HW Revision[0x%02x] SW Version[0x%02x] \n", val[0], val[1]);
			break; // i2c success
		}
	}

	if (ret < 0)
	{
		pr_info("[TSP] %s,%d: i2c read fail[%d] \n", __FUNCTION__, __LINE__, ret);
		return ret;
	}
	return ret;
}

static int firmware_update(struct melfas_ts_data *ts)
{
	int ret = 0;
	uint8_t fw_ver[2] = {0, };

	//struct melfas_ts_data *info = ts;//context;
	//struct i2c_client *client = info->client;
	//struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);


	ret = check_firmware(ts, fw_ver);
    if(ret < 0) return ret;
	//mms100_download();
#if 1
	//mms100_ISC_download_mbinary(client);
	return 0;
#endif

	return ret;
}
#endif
#else
static int firmware_update(struct melfas_ts_data *ts)
{
	int ret = 0;
	uint8_t fw_ver[2] = {0, };

	struct melfas_ts_data *info = ts;//context;
	struct i2c_client *client = info->client;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	//ret = check_firmware(ts, fw_ver);
	if (0)//(ret < 0)
		pr_err("[TSP] check_firmware fail! [%d]", ret);
	else
	{
#if MELFAS_DOWNLOAD
		if (1)//(fw_ver[1] < MELFAS_FW_VERSION)
		{
			int ver;

			pr_info("[TSP] %s: \n", __func__);
			//ret = isc_fw_download(info, MELFAS_binary, MELFAS_binary_nLength);
			if (1)//(ret < 0)
			{
#if 1
				ret = isp_fw_download(MELFAS_MMS100_Initial_binary, MELFAS_MMS100_Initial_nLength);
				if (ret != 0)
				{
					pr_info("[TSP] error updating firmware to version 0x%02x \n", MELFAS_FW_VERSION);
					ret = -1;
				}
				else
				{
					ret = isc_fw_download(info, MELFAS_binary, MELFAS_binary_nLength);
				}
#else
				pr_info("[TSP] error updating firmware to version 0x%02x \n", MELFAS_FW_VERSION);
#endif
			}
		}
#endif
	}
	panic("isc_fw_download failed,system hang-up!");

	return ret;
}
#endif
static void melfas_ts_get_data(struct melfas_ts_data *ts)
{
    bool touch_status=false;
    int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN] = { 0, };
	int read_num, fingerID, Touch_Type = 0, touchState = 0;

	if (ts->debug_enabled)
		printk("[TSP] %s : \n", __FUNCTION__);

	if (ts == NULL)
		pr_info("[TSP] %s: ts data is NULL \n", __FUNCTION__);

	for (i = 0; i < I2C_RETRY_CNT; i++)
	{
		ret = melfas_i2c_read(ts->client, TS_READ_LEN_ADDR, 1, buf);

		if (ret >= 0)
		{
			if (ts->debug_enabled)
				printk("[TSP] : TS_READ_LEN_ADDR [%d] \n", ret);

			break; // i2c success
		}
	}

	if (ret < 0)
	{
		pr_info("[TSP] %s,%d: i2c read fail[%u] \n", __FUNCTION__, __LINE__, ret);
		return;
	}
	else
	{
		read_num = buf[0];
		if (ts->debug_enabled)
			printk("[TSP] %s,%d: read_num[%d] \n", __FUNCTION__, __LINE__, read_num);
	}
   
	if (read_num > 0)
	{
		for (i = 0; i < I2C_RETRY_CNT; i++)
		{
			ret = melfas_i2c_read(ts->client, TS_READ_START_ADDR, read_num, buf);
			if (ret >= 0)
			{
				if (ts->debug_enabled)
					printk("[TSP] melfas_ts_get_data : TS_READ_START_ADDR [%d] \n", ret);

				break; // i2c success
			}
		}
        
        if (ret < 0)
		{
			pr_info("[TSP] %s,%d: i2c read fail[%d] \n", __FUNCTION__, __LINE__, ret);
			return;
		}
		else
		{
			for (i = 0; i < read_num; i = i + 6)
			{
				Touch_Type = (buf[i] >> 5) & 0x03;

				/* touch type is panel */
				if (Touch_Type == TOUCH_SCREEN)
				{
					fingerID = (buf[i] & 0x0F) - 1;
					touchState = (buf[i] & 0x80);

					g_Mtouch_info[fingerID].posX = (uint16_t)(buf[i + 1] & 0x0F) << 8 | buf[i + 2];
					g_Mtouch_info[fingerID].posY = (uint16_t)(buf[i + 1] & 0xF0) << 4 | buf[i + 3];
					g_Mtouch_info[fingerID].area = buf[i + 4];

					if (touchState)
						g_Mtouch_info[fingerID].pressure = buf[i + 5];
					else
						g_Mtouch_info[fingerID].pressure = 0;
				}
			}


			for (i = 0; i < TS_MAX_TOUCH; i++)
			{
				if (g_Mtouch_info[i].pressure == -1)
					continue;

#if SLOT_TYPE
				if(g_Mtouch_info[i].pressure == 0)
				{
					// release event
					input_mt_slot(ts->input_dev, i);
					input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
				}
				else
				{
					REPORT_MT(i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].area, g_Mtouch_info[i].pressure);
				}
#else

                if(g_Mtouch_info[i].pressure == 0)
				{
					// release event
					input_mt_sync(ts->input_dev);
				}
				else
				{
                    touch_status=true;
                    //pr_info("i = %d, posX = %d, posY = %d, area = %d, pressure = %d\n",i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].area, g_Mtouch_info[i].pressure);
					REPORT_MT(i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].area, g_Mtouch_info[i].pressure);
				}
#endif
				if (ts->debug_enabled)
					printk("[TSP] %s: Touch ID: %d, State : %d, x: %d, y: %d, z: %d w: %d\n", __FUNCTION__,
					i, (g_Mtouch_info[i].pressure > 0), g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].pressure, g_Mtouch_info[i].area);

				if (g_Mtouch_info[i].pressure == 0)
     			g_Mtouch_info[i].pressure = -1;                           
			}
			input_sync(ts->input_dev);
		}
	}
#ifdef TOUCH_BOOSTER
#ifdef TOUCH_BOOSTER_PRINT
           pr_info("[TSP] touch_status:%s\n",(touch_status==true)?"1" :"0");
#endif
           TSP_lock_dvfs(touch_status,ts);
#endif   
}
#include <plat/gpio-cfg.h>
/*
static struct workqueue_struct *melfas_wq;
static void melfas_ts_work_func(struct work_struct *work)
{
	struct melfas_ts_data *ts = NULL;
	ts = container_of(work, struct melfas_ts_data, work);

    melfas_ts_get_data(ts);
}
*/
static void melfas_ts_pen_irq_work(struct work_struct *work)
{  
    struct melfas_ts_data *ts = i2c_get_clientdata(this_client);
	//do {
		melfas_ts_get_data(ts);
	//} while(gpio_get_value(GPIO_CTP_EINT) == 0);
    
    enable_irq(this_client->irq);
	if (ts->debug_enabled)
    	printk("---[TSP] %s\n", __FUNCTION__); 
}

static irqreturn_t melfas_ts_irq_handler(int irq, void *handle)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *) handle;

	if (ts->debug_enabled)
  		printk("+++[TSP] %s\n", __FUNCTION__);  
	disable_irq_nosync(this_client->irq);
	/*do {
		melfas_ts_get_data(ts);
	} while(gpio_get_value(GPIO_CTP_EINT) == 0);
    
    enable_irq(this_client->irq);*/
	if (!work_pending(&ts->pen_event_work)) {
		queue_work(ts->ts_workqueue, &ts->pen_event_work);
	}    
	return IRQ_HANDLED;
}

static void t50_touch_init(void)
{
#if 0
	int err;

	printk("%s++\n", __func__);

	/*gpio_request(TSP_EN, "TP_EN");
	gpio_direction_output(TSP_EN, 1 );
	gpio_free(TSP_EN);*/

	mdelay(10);
	err = gpio_request(GPIO_CTP_RST,"TP_RST");
	if (err) {
		pr_err("failed to request GPK0 for touch reset control\n");
		return;
	}
	gpio_direction_output(GPIO_CTP_RST,0);
	samsung_gpio_setpull(GPIO_CTP_RST,SAMSUNG_GPIO_PULL_NONE);
	mdelay(100);
	gpio_direction_output(GPIO_CTP_RST,1);

	mdelay(10);
	gpio_free(GPIO_CTP_RST);
#endif	
}

static int melfas_power_off(void)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(this_client);

	pr_info("[TSP] %s\n", __func__);
    cancel_work_sync(&ts->pen_event_work);
	melfas_ts_release_all_finger(ts);
	pr_info("[TSP] disable irq\n");
	disable_irq(this_client->irq);
	if (ts->pdata->power_enable){
		ts->power = ts->pdata->power_enable;
		ts->power(DISABLE);
	}
	return 0;
}
static int melfas_power_on(void)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(this_client); 
	pr_info("[TSP] %s\n", __func__);
    /*2014-01-16 xufei:powering up is set in file: board-p632x-display.c -> static int reset_lcd_s6e8aa0(struct lcd_device *ld)*/
	if (ts->pdata->power_enable)
		ts->pdata->power_enable(ENABLE);
   	//mdelay(50);    
	pr_info("[TSP] enable irq\n");
	if (first_boot_flag == true)
		first_boot_flag = false;
	else
		enable_irq(this_client->irq); // scl wave
	return 0;
}
#ifdef TEST_MELFAS_IIC
static unsigned int melfas_test(struct work_struct *work)
{
  int ret = 0; 
  uint8_t buf[5/*TS_READ_REGS_LEN*/] = { 0, };
  printktp("\n%s is called\n",__func__);
  ret = melfas_i2c_read(this_client, TS_READ_LEN_ADDR, 1, buf);
  if (ret >= 0)
  {
	printktp("[TSP] : TS_READ_LEN_ADDR [%d] \n\n", ret);
  }
  else
    printktp("[TSP] : Melfas i2c test failed!\n\n");
  schedule_delayed_work(&melfas_delayed_work, TEST_TICKS);
  return 0;  
}
#endif
int melfas_touch_power_enable(int on)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(this_client);
	struct device *dev = &ts->client->dev;
	struct regulator *regulator_dvdd;
	struct regulator *regulator_avdd;
	struct melfas_tsi_platform_data  *pData;
	static bool enabled;
	int retval = 0;
	struct pinctrl *pinctrl_irq;

	
	if (enabled == on)
		return retval;

	pData = ts->pdata;
	regulator_dvdd = regulator_get(NULL, pData->regulator_dvdd);
	if (IS_ERR(regulator_dvdd)) {
		tsp_debug_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, pData->regulator_dvdd);
		return PTR_ERR(regulator_dvdd);
	}

	regulator_avdd = regulator_get(NULL, pData->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		tsp_debug_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, pData->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}

	tsp_debug_info(true, dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on) {
		pinctrl_irq = devm_pinctrl_get_select(dev, "on_state");
		if (IS_ERR(pinctrl_irq))
			dev_err(dev, "%s: Failed to configure tsp_attn pin\n", __func__);
		regulator_set_voltage(regulator_dvdd, 1800000, 1800000);
		retval = regulator_enable(regulator_dvdd);
		if (retval) {
			tsp_debug_err(true, dev, "%s: Failed to enable vdd: %d\n", __func__, retval);
			return retval;
		}
		tsp_debug_info(true, dev, "%s\n", __func__);
		//regulator_set_voltage(regulator_avdd, 2800000, 2800000);
		//retval = regulator_enable(regulator_avdd);
		if (retval) {
			tsp_debug_err(true, dev, "%s: Failed to enable avdd: %d\n", __func__, retval);
			return retval;
		}

	} else {
		pinctrl_irq = devm_pinctrl_get_select(dev, "off_state");
		if (IS_ERR(pinctrl_irq))
			dev_err(dev, "%s: Failed to configure tsp_attn pin\n", __func__);
		if (regulator_is_enabled(regulator_dvdd))
			regulator_disable(regulator_dvdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);
	}

	enabled = on;
	regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return retval;
}
static int melfas_parse_dt(struct i2c_client *client, struct melfas_tsi_platform_data *pData)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	u32 coords[2], lines[2];
	int retval = 0;

	pData->gpio = of_get_named_gpio(np, "melfas,irq_gpio", 0);
	if (gpio_is_valid(pData->gpio)) {
		retval = gpio_request_one(pData->gpio, GPIOF_DIR_IN, "melfas,tsp_int");
		if (retval) {
			tsp_debug_err(true, dev, "Unable to request tsp_int [%d]\n", pData->gpio);
			return -EINVAL;
		}
	} else {
		tsp_debug_err(true, dev, "Failed to get irq gpio\n");
		return -EINVAL;
	}
	client->irq = gpio_to_irq(pData->gpio);

	if (of_property_read_u32(np, "melfas,irq_type", &pData->irq_type)) {
		tsp_debug_err(true, dev, "Failed to get irq_type property\n");
		return -EINVAL;
	}
	if (of_property_read_u32_array(np, "melfas,max_coords", coords, 2)) {
		tsp_debug_err(true, dev, "Failed to get max_coords property\n");
		return -EINVAL;
	}
	pData->max_x = coords[0];
	pData->max_y = coords[1];

	if (of_property_read_u32_array(np, "melfas,num_lines", lines, 2)) {
		tsp_debug_err(true, dev, "Failed to get num_liness property\n");
		return -EINVAL;
	}
	pData->max_area= lines[0];
	pData->max_pressure = lines[1];

	if (of_property_read_string(np, "melfas,regulator_dvdd", &pData->regulator_dvdd)) {
		tsp_debug_err(true, dev, "Failed to get regulator_dvdd name property\n");
		return -EINVAL;
	}
	if (of_property_read_string(np, "melfas,regulator_avdd", &pData->regulator_avdd)) {
		tsp_debug_err(true, dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}
	return retval;
}

static unsigned int melfas_check_i2cbus(struct i2c_client *client)
{
	int ret = 0; 
	uint8_t buf[5] = {0};
	int error = 0;
      
	ret = melfas_i2c_read(client, TS_READ_LEN_ADDR, 1, buf);
	if (ret >= 0){
		printk("[TSP] : melfas_check_i2cbus success\n");
		error = 0;
	}
	else{
		dev_err(&client->dev, "Failed to mxt_check_i2cbus\n");
		error = ret;
	}
	return error;  
}

static ssize_t melfas_debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	bool c;

	c = data->debug_enabled ? 1 : 0;
	return scnprintf(buf, PAGE_SIZE, "%d\n", c);
}


static ssize_t melfas_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		dev_dbg(dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static DEVICE_ATTR(debug_enable, S_IWUSR | S_IRUSR, melfas_debug_enable_show,
			melfas_debug_enable_store);


static struct attribute *melfas_attrs[] = {

	&dev_attr_debug_enable.attr,
	NULL
};

static const struct attribute_group melfas_attr_group = {
	.attrs = melfas_attrs,
};

static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data *ts;
	struct melfas_tsi_platform_data  *pData;
	int ret = 0, i;
	int err;

	pr_info("[TSP] %s\n", __FUNCTION__);
    
	t50_touch_init();
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR"WRONG --%s\n",__func__); 
		printk("[TSP] melfas_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	if (melfas_check_i2cbus(client)){
		dev_err(&client->dev, "melfas_check_i2cbus failed!\n");
		goto err_check_functionality_failed;
	}

	pData = kmalloc(sizeof(struct melfas_tsi_platform_data), GFP_KERNEL);
	ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
	if (ts == NULL)
	{
		printk("[TSP] %s: failed to create a state of melfas-ts\n", __FUNCTION__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	ts->debug_enabled = 0;
	i2c_set_clientdata(client, ts);
	this_client = client;
	ret = firmware_update(ts);
    //while(ret<0)  ret = firmware_update(ts);//xufei
	printk("melfas_ts_probe 1");
#ifdef CONFIG_OF
    melfas_parse_dt(client, pData);
	pData->power_enable = melfas_touch_power_enable;
	ts->pdata = pData;
#else
	ts->pdata = client->dev.platform_data;
#endif

	err = sysfs_create_group(&client->dev.kobj, &melfas_attr_group);
	if (err) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			err);
		goto err_free_object;
	}


    /*2014-01-16 xufei:powering up is set in file: board-p632x-display.c -> static int reset_lcd_s6e8aa0(struct lcd_device *ld)*/
	if (ts->pdata->power_enable)
		ts->pdata->power_enable(ENABLE);

	if (ret < 0)
	{
		printk(KERN_ERR"WRONG --%s\n",__func__);
		printk("[TSP] %s: firmware update fail\n", __FUNCTION__);
		goto err_detect_failed;
	}
	ts->input_dev = input_allocate_device();
	if (!ts->input_dev)
	{
		printk("[TSP] %s: Not enough memory\n", __FUNCTION__);
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "melfas-ts";

	__set_bit(EV_ABS,  ts->input_dev->evbit);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->pdata->max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->pdata->max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, ts->pdata->max_area, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, ts->pdata->max_pressure, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, P5_MAX_TOUCH-1, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret)
	{
		printk("[TSP] %s: Failed to register device\n", __FUNCTION__);
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}

    INIT_WORK(&ts->pen_event_work, melfas_ts_pen_irq_work);
	ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
    
	if (ts->client->irq)
	{
       // ret = request_threaded_irq(client->irq,NULL,
	   //			  melfas_ts_irq_handler, IRQF_TRIGGER_FALLING /*| IRQF_ONESHOT*/, ts->client->name,ts);  
        
        ret = request_irq(client->irq,melfas_ts_irq_handler,
					      IRQF_TRIGGER_FALLING , ts->client->name, ts);
		if (ret)
		{
			printk(KERN_ERR"WRONG --%s\n",__func__);
			printk("[TSP] %s: Can't allocate irq %d, ret %d\n", __FUNCTION__, ts->client->irq, ret);
			ret = -EBUSY;
			goto err_request_irq;
		}
        else
        {
            printk("request_threaded_irq sucessfully\n");
        }
		disable_irq(client->irq);
	}

	for (i = 0; i < TS_MAX_TOUCH; i++) /* _SUPPORT_MULTITOUCH_ */
		g_Mtouch_info[i].pressure = -1;

 	input_sync(ts->input_dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = melfas_ts_early_suspend;
	ts->early_suspend.resume = melfas_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	//ly add to fix lcd issue
	melfas_ts_release_all_finger(ts);
	enable_irq(client->irq);
	first_boot_flag = true;
#ifdef TOUCH_DELAYED_OPEN
	INIT_DELAYED_WORK(&touch_open_work, touch_power_on);
#endif
    fb_register_client(&fb_block);
	touchscreen_power_on = melfas_power_on; 
	touchscreen_power_off = melfas_power_off; 
	pr_info("[TSP] %s: Start touchscreen. name: %s, irq: %d\n", __FUNCTION__, ts->client->name, ts->client->irq);
#ifdef TEST_MELFAS_IIC    
    INIT_DELAYED_WORK(&melfas_delayed_work, melfas_test);
    schedule_delayed_work(&melfas_delayed_work, TEST_TICKS); 
#endif

#ifdef TOUCH_BOOSTER  
        if(!TSP_init_dvfs(ts))
           printk("initial TSP DVFS OK!\n"); 
#endif
    //pm_qos_add_request(&ts->tsp_cpu_qos, PM_QOS_CPU_FREQ_MIN, 1000000);          

	return 0;

err_request_irq:
	pr_info("[TSP] %s: err_request_irq failed\n", __func__);
	free_irq(client->irq, ts);
exit_create_singlethread:
    printk("[TSP] %s: err_create_singlethread failed_\n", __func__);	
err_input_register_device_failed:
	pr_info("[TSP] %s: err_input_register_device failed\n", __func__);
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	pr_info("[TSP] %s: err_input_dev_alloc failed\n", __func__);

err_detect_failed:
	pr_info("[TSP] %s: err_after_get_regulator failed_\n", __func__);
	kfree(ts);

err_free_object:
	sysfs_remove_group(&client->dev.kobj, &melfas_attr_group);

err_alloc_data_failed:
	pr_info("[TSP] %s: err_after_get_regulator failed_\n", __func__);	

err_check_functionality_failed:
	printk("[TSP] %s: err_check_functionality failed_\n", __func__);

	return ret;
}

static int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	free_irq(client->irq, ts);
	ts->power(0);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	sysfs_remove_group(&client->dev.kobj, &melfas_attr_group);
#ifdef TOUCH_DELAYED_OPEN
	cancel_delayed_work_sync(&touch_open_work);
#endif
#ifdef TOUCH_BOOSTER
if (pm_qos_request_active(&ts->tsp_cpu_qos))
    pm_qos_remove_request(&ts->tsp_cpu_qos);                   
if (pm_qos_request_active(&ts->tsp_mif_qos))
    pm_qos_remove_request(&ts->tsp_mif_qos);                   
if (pm_qos_request_active(&ts->tsp_int_qos))
    pm_qos_remove_request(&ts->tsp_int_qos);
#endif
	return 0;
}

static int melfas_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	pr_info("[TSP] %s\n", __func__);
	melfas_ts_release_all_finger(ts);
	pr_info("[TSP] disable irq\n");
	disable_irq(client->irq);
	///   ts->power(0);
	return 0;
}

static int melfas_ts_resume(struct i2c_client *client)
{
	pr_info("[TSP] %s\n", __func__);
	enable_irq(client->irq); // scl wave
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
    printk("%s is called!\n",__func__);
    struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
    printk("%s is called!\n",__func__);
    struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id melfas_ts_id[] =
{
	{ "melfas_mms100_MIP", 0},
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id melfas_dt_ids[] = {
	{ .compatible = "melfas_mms100_MIP" },
	{ }
};
#endif

static struct i2c_driver melfas_ts_driver = {
	.driver = {
		.name = "melfas_mms100_MIP",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(melfas_dt_ids),
#endif
	},
	.probe = melfas_ts_probe,
	.remove = __devexit_p(melfas_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = melfas_ts_suspend,
	.resume = melfas_ts_resume,
#endif

	.id_table = melfas_ts_id,
};

static int __devinit melfas_ts_init(void)
{
	printk("%s\n", __func__);
	return i2c_add_driver(&melfas_ts_driver);
}

static void __exit melfas_ts_exit(void)
{
	i2c_del_driver(&melfas_ts_driver);
}

MODULE_DESCRIPTION("Driver for Melfas MIP Touchscreen Controller");
MODULE_AUTHOR("MinSang, Kim <kimms@melfas.com>");
MODULE_VERSION("0.2");
MODULE_LICENSE("GPL");

module_init(melfas_ts_init);
module_exit(melfas_ts_exit);
