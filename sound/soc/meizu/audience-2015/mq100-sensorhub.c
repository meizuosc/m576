/*
 * Audience Sensorhub interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * Author: Pratik Khade <pkhade@audience.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "escore.h"
#include "escore-spi.h"
#include "escore-uart.h"
#include "escore-uart-common.h"
#include "escore-i2c.h"
#include "escore-cdev.h"
#if defined(CONFIG_ARCH_OMAP)
#define gpio_to_irq OMAP_GPIO_IRQ
#endif
#if defined(CONFIG_MQ100_DEMO)
#define SDATA_HDR (sizeof(u8) * 2)
#endif
/* local data structures */
enum mq100_power_state {
	MQ100_POWER_BOOT,
	MQ100_POWER_SLEEP,
	MQ100_POWER_SLEEP_PENDING,
	MQ100_POWER_AWAKE
};

struct sensorhub_data *sensor_priv;
/* codec private data TODO: move to runtime init */
struct escore_priv escore_priv = {
	.probe = mq100_core_probe,

	.flag.vs_enable = 0,
	.ap_tx1_ch_cnt = 2,

	.escore_power_state = ES_SET_POWER_STATE_NORMAL,
#if defined(CONFIG_SND_SOC_ES_I2C)
	.streamdev.intf = ES_I2C_INTF,
#elif defined(CONFIG_SND_SOC_ES_UART)
	.streamdev.intf = ES_UART_INTF,
#endif
	.flag.ns = 1,
	.flag.sleep_enable = 0,	/*Auto sleep disabled default */
	.sleep_delay = 3000,
	.wake_count = 0,
	.flag.sleep_abort = 0,
};

/* local function prototype */
#if defined(CONFIG_MQ100_DEMO)
static u8 sdata[512];
static void report_mag_data(struct sensorhub_data *data,
		struct osp_sh_motion_uncal_sensor_broadcast_node *mag);
static void report_gyro_data(struct sensorhub_data *data,
		struct osp_sh_motion_uncal_sensor_broadcast_node *gyr);
static void report_accel_data(struct sensorhub_data *data,
		struct osp_sh_motion_uncal_sensor_broadcast_node *acc);
static int sensorhub_init_input_dev(struct escore_priv *escore);

/*******************************************************************************
 * @fn		sensorhub_sensor_read_buf
 *			Helper function to read sensor data over I2C bus
 *
 ******************************************************************************/
static inline int sensorhub_read_buf(u8 *buffer, int length)
{
	return mq100_rdb(buffer, length, 0x0000);
}

/*******************************************************************************
 * @fn		osp_sensor_relay_data
 *			Sensor Hub interrupt thread worker
 *
 ******************************************************************************/
static void osp_relay_sensor_data(u8 *buf,
		u8 len,
		struct sensorhub_data *sensor)
{
	struct osp_sh_motion_uncal_sensor_broadcast_node *tmp_node = NULL;
	SensorType_t sensorId = 0;
	u8 *ptr = NULL;
	u8 *tmp = NULL;
	for (ptr = buf;
		ptr < (buf + len);
		ptr += SIZE_UNCAL_SBCAST_NODE + SDATA_HDR) {

		sensorId = (*ptr);
		tmp_node = (struct osp_sh_motion_uncal_sensor_broadcast_node *)\
				(ptr + 2);
		pr_debug("%s:%d ptr 0x%08X sensorId %d len %d\n",\
				__func__, __LINE__,\
				(unsigned int) (ptr), sensorId, len);

		switch (sensorId) {
		case SENSOR_ACCELEROMETER_UNCALIBRATED:
			report_accel_data(sensor_priv, tmp_node);
			break;

		case SENSOR_MAGNETIC_FIELD_UNCALIBRATED:
			report_mag_data(sensor_priv, tmp_node);
			break;

		case SENSOR_GYROSCOPE_UNCALIBRATED:
			report_gyro_data(sensor_priv, tmp_node);
			break;

		default:
#if defined(DEBUG)
			pr_debug("%s:%d sensor data ---->\n",
			__func__, __LINE__);
			for (; tmp < (ptr + SIZE_UNCAL_SBCAST_NODE);
				tmp++) {
				if (((tmp - sdata) % 0x10) == 0)
					printk(KERN_DEBUG "\n");
				printk(KERN_DEBUG "0x%02X ", *tmp);
			}
#endif
			break;
		}
	}
}

/******************************************************************************
 * @fn		osp_sh_work_func
 *			Sensor Hub interrupt thread worker
 *
 *****************************************************************************/
static void osp_sh_work_func(struct work_struct *work)
{
	struct escore_priv *escore = (struct escore_priv *)&escore_priv;

	int rdblen = 0;
	int sensor_data_len = 0;
	u8 *ptr = NULL;

	/* Read interrupt reason and length */
	memset(sdata, 0, sizeof(sdata));
	rdblen = sensorhub_read_buf(sdata, sizeof(sdata));
	/* If valid, read specified length from slave memory */
	ptr = sdata;
	pr_debug("%s:%d rdblen = %d\n", __func__, __LINE__, rdblen);
	if ((rdblen == 0))
		goto exit;

	sensor_data_len = *ptr;
	pr_debug("%s:%d sensor-data-len %d data ---->",
			__func__, __LINE__, sensor_data_len);
#if defined(DEBUG)
	for (ptr = sdata; ptr < (sdata + rdblen); ptr++) {
		if (((ptr - sdata) % 0x10) == 0)
			pr_debug("\n");
		pr_debug("0x%02x ", *ptr);
	}
#endif
	osp_relay_sensor_data((u8 *)(sdata + sizeof(u8)),
			sensor_data_len, sensor_priv);
exit:
	enable_irq(gpio_to_irq(escore->pdata->gpiob_gpio));
}

static int sensorhub_probe(void)
{
	sensorhub_init_input_dev(&escore_priv);
	INIT_WORK(&sensor_priv->work, osp_sh_work_func);
	pr_debug("Sensor Hub driver init successful!");
	return 0;
}

static void report_accel_data(struct sensorhub_data *data,
		struct osp_sh_motion_uncal_sensor_broadcast_node *acc)
{
	/*
	 * Read Accel data or it is passed as argument
	 * Publish to input event
	 */
	input_report_abs(data->acc_input_dev, ABS_X, acc->Data[0]);
	input_report_abs(data->acc_input_dev, ABS_Y, acc->Data[1]);
	input_report_abs(data->acc_input_dev, ABS_Z, acc->Data[2]);
	input_sync(data->acc_input_dev);

}

static void report_gyro_data(struct sensorhub_data *data,
		struct osp_sh_motion_uncal_sensor_broadcast_node *gyr)
{
	/* Publish to input event */
	input_report_abs(data->gyro_input_dev, ABS_X, gyr->Data[0]);
	input_report_abs(data->gyro_input_dev, ABS_Y, gyr->Data[1]);
	input_report_abs(data->gyro_input_dev, ABS_Z, gyr->Data[2]);
	input_sync(data->gyro_input_dev);
}

static void report_mag_data(struct sensorhub_data *data,
		struct osp_sh_motion_uncal_sensor_broadcast_node *mag)
{
	/* Publish to input event */
	input_report_abs(data->mag_input_dev, ABS_X, mag->Data[0]);
	input_report_abs(data->mag_input_dev, ABS_Y, mag->Data[1]);
	input_report_abs(data->mag_input_dev, ABS_Z, mag->Data[2]);
	input_sync(data->mag_input_dev);
}

void sensorhub_remove_input_dev(void)
{
	struct sensorhub_data *data = sensor_priv;
	input_unregister_device(data->acc_input_dev);
	input_unregister_device(data->gyro_input_dev);
	input_unregister_device(data->mag_input_dev);
	kfree(data);
}

static int sensorhub_init_input_dev(struct escore_priv *escore)
{
	struct sensorhub_data *sensdata;
	int iRet = 0;
	struct input_dev *acc_input_dev, *gyro_input_dev, *mag_input_dev;

	/* Add initialization for sensor input devices */
	sensdata = kzalloc(sizeof(struct sensorhub_data), GFP_KERNEL);
	if (sensdata == NULL) {
		dev_err(escore->dev,
			"[SPI]: %s - failed to allocate memory for sensdata\n",
			__func__);
		return -ENOMEM;
	}
	sensor_priv = sensdata;

	/* allocate input_device */
	acc_input_dev = input_allocate_device();
	if (acc_input_dev == NULL)
		goto iRet_acc_input_free_device;

	gyro_input_dev = input_allocate_device();
	if (gyro_input_dev == NULL)
		goto iRet_gyro_input_free_device;

	mag_input_dev = input_allocate_device();
	if (mag_input_dev == NULL)
		goto iRet_mag_input_free_device;

	input_set_drvdata(acc_input_dev, sensdata);
	input_set_drvdata(gyro_input_dev, sensdata);
	input_set_drvdata(mag_input_dev, sensdata);

	acc_input_dev->name  = "accelerometer_sensor";
	gyro_input_dev->name = "gyro_sensor";
	mag_input_dev->name  = "magnetic_sensor";

	input_set_capability(acc_input_dev, EV_ABS, ABS_X);
	input_set_capability(acc_input_dev, EV_ABS, ABS_Y);
	input_set_capability(acc_input_dev, EV_ABS, ABS_Z);
	input_set_abs_params(acc_input_dev, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(acc_input_dev, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(acc_input_dev, ABS_Z, INT_MIN, INT_MAX, 0, 0);

	input_set_capability(gyro_input_dev, EV_ABS, ABS_X);
	input_set_capability(gyro_input_dev, EV_ABS, ABS_Y);
	input_set_capability(gyro_input_dev, EV_ABS, ABS_Z);
	input_set_abs_params(gyro_input_dev, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(gyro_input_dev, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(gyro_input_dev, ABS_Z, INT_MIN, INT_MAX, 0, 0);

	input_set_capability(mag_input_dev, EV_ABS, ABS_X);
	input_set_capability(mag_input_dev, EV_ABS, ABS_Y);
	input_set_capability(mag_input_dev, EV_ABS, ABS_Z);
	input_set_abs_params(mag_input_dev, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(mag_input_dev, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(mag_input_dev, ABS_Z, INT_MIN, INT_MAX, 0, 0);

	/* register input_device */
	iRet = input_register_device(acc_input_dev);
	if (iRet < 0)
		goto iRet_acc_input_unreg_device;

	iRet = input_register_device(gyro_input_dev);
	if (iRet < 0) {
		input_free_device(gyro_input_dev);
		input_free_device(mag_input_dev);
		goto iRet_gyro_input_unreg_device;
	}

	iRet = input_register_device(mag_input_dev);
	if (iRet < 0) {
		input_free_device(mag_input_dev);
		goto iRet_mag_input_unreg_device;
	}

	sensdata->acc_input_dev  = acc_input_dev;
	sensdata->gyro_input_dev = gyro_input_dev;
	sensdata->mag_input_dev  = mag_input_dev;

	dev_dbg(escore->dev,
	    "[SPI]: %s - Done creating input dev nodes for sensors",
	    __func__);

	return 1;

iRet_mag_input_unreg_device:
	input_unregister_device(gyro_input_dev);
iRet_gyro_input_unreg_device:
	input_unregister_device(acc_input_dev);
	return -EIO;
iRet_acc_input_unreg_device:
	pr_err("[SPI]: %s - could not register input device\n", __func__);
	input_free_device(mag_input_dev);
iRet_mag_input_free_device:
	input_free_device(gyro_input_dev);
iRet_gyro_input_free_device:
	input_free_device(acc_input_dev);
iRet_acc_input_free_device:
	pr_err("[SPI]: %s - could not allocate input device\n", __func__);
	kfree(sensdata);
	return -EIO;
}
#endif

/**
 * Register rdb callbacks
 * @param callbacks - structure containing
 * callbacks from mq100 rdb/wdb driver
 * @return
 */

int mq100_register_extensions(struct mq100_extension_cb *_callbacks, void *priv)
{
	int ret = -EINVAL;
	if (_callbacks == NULL)
		goto error;

	escore_priv.sensor_cb = _callbacks;
	/* Need to share more safe data structure */
	escore_priv.sensor_cb->priv = priv;
	ret = 0;
error:
	return ret;
}

EXPORT_SYMBOL_GPL(mq100_register_extensions);

void mq100_indicate_state_change(u8 val)
{
	if (escore_priv.sensor_cb != NULL && escore_priv.sensor_cb->status != 0)
		escore_priv.sensor_cb->status(escore_priv.sensor_cb->priv, val);

	dev_dbg(escore_priv.dev, "%s: State %d\n", __func__, val);
}

EXPORT_SYMBOL_GPL(mq100_indicate_state_change);

/* Max Size = PAGE_SIZE * 2 */
/*
 * Reads buf from mq100 using rdb
 * @param:	buf - rdb data Max Size supported - 2*PAGE_SIZE
 * Ensure buffer allocated
 * has enough space for rdb
 *
 * buf - buffer pointer
 * id - type specifier
 * len - max. buf size
 *
 * @return: no. of bytes read
 */
int mq100_rdb(void *buf, int len, int id)
{
	int rc;

	if (!buf)
		return 0;
	rc = escore_datablock_read(&escore_priv, buf, len, id);
	pr_debug("%s:%d: read returns %d", __func__, __LINE__, rc);

	return escore_priv.datablock_dev.rdb_read_count;
}

EXPORT_SYMBOL_GPL(mq100_rdb);

/*
 * Writes buf to mq100 using wdb,
 * this function will prepend 0x802F 0xffff
 * @param: buf - wdb data
 * @param: len - length
 * @return: no. of bytes written
 */
int mq100_wdb(const void *buf, int len)
{
	int rc;
	rc = escore_datablock_write(&escore_priv, buf, len);
	pr_debug("%s:%d: rc %d", __func__, __LINE__, rc);
	return 0;
}

EXPORT_SYMBOL_GPL(mq100_wdb);

static ssize_t mq100_get_pm_enable(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", escore_priv.pm_enable ?
			"on" : "off");
}

static ssize_t mq100_set_pm_enable(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(pm_enable, 0666, mq100_get_pm_enable, mq100_set_pm_enable);

#define SIZE_OF_VERBUF 256
/* TODO: fix for new read/write. use mq100_read() instead of BUS ops */
static ssize_t mq100_fw_version_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int idx = 0;
	unsigned int value;
	char versionbuffer[SIZE_OF_VERBUF];
	char *verbuf = versionbuffer;

	memset(verbuf, 0, SIZE_OF_VERBUF);

	value = escore_read(NULL, MQ100_FW_FIRST_CHAR);
	*verbuf++ = (value & 0x00ff);
	for (idx = 0; idx < (SIZE_OF_VERBUF - 2); idx++) {
		value = escore_read(NULL, MQ100_FW_NEXT_CHAR);
		*verbuf++ = (value & 0x00ff);
		if (!value)
			break;
	}
	/* Null terminate the string */
	*verbuf = '\0';
	dev_info(dev, "Audience fw ver %s\n", versionbuffer);
	return snprintf(buf, PAGE_SIZE, "FW Version = %s\n", versionbuffer);
}

static DEVICE_ATTR(fw_version, 0444, mq100_fw_version_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/fw_version */
#if defined(CONFIG_MQ100_DEMO)
static struct osp_pack osp_sh_sensor_broadcast_node temp_node;

static inline void fill_temp_node(void)
{
	static u16 x, y, z;
	static u32 timeStamp;
	temp_node.sensorId = 1;
	temp_node.data.uncal_sensorData.Data[0] = x++;
	temp_node.data.uncal_sensorData.Data[1] = y++;
	temp_node.data.uncal_sensorData.Data[2] = z++;
	temp_node.data.uncal_sensorData.timeStamp.timeStamp32 = timeStamp++;
	temp_node.data.uncal_sensorData.timeStamp.timeStamp40 = 0;
}

static ssize_t mq100_simulate_sensor_data(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret = 0;
	struct sensorhub_data *sensor = sensor_priv;
	fill_temp_node();
	osp_relay_sensor_data((uint8_t *)&temp_node,
			(sizeof(temp_node.data.uncal_sensorData) + SDATA_HDR),
			sensor);
	return ret;
}

static DEVICE_ATTR(simulate_sdata, 0444, mq100_simulate_sensor_data, NULL);
#endif
static ssize_t mq100_clock_on_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int ret = 0;
	return ret;
}

static DEVICE_ATTR(clock_on, 0444, mq100_clock_on_show, NULL);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/clock_on */

#if defined(CONFIG_MQ100_DEMO)
#define ES_RESET_CMD 0x9003
void mq100_reset_cmd(void)
{
	u32 cmd, rspn = 0;
	cmd = ES_RESET_CMD << 16;
	escore_cmd(&escore_priv, cmd, &rspn);
}
#endif

static ssize_t mq100_gpio_reset_set(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct escore_priv *mq100 = &escore_priv;
#if defined(CONFIG_MQ100_DEMO)
	mq100_reset_cmd();
#else
	escore_gpio_reset(mq100);
#endif
	dev_dbg(mq100->dev, "%s(): Ready for STANDARD download by proxy\n",
		__func__);
	return count;
}

static DEVICE_ATTR(gpio_reset, 0644, NULL, mq100_gpio_reset_set);
/* /sys/devices/platform/msm_slim_ctrl.1/earSmart-codec-gen0/gpio_reset */

static struct attribute *core_sysfs_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_clock_on.attr,
	&dev_attr_gpio_reset.attr,
	&dev_attr_pm_enable.attr,
#if defined(CONFIG_MQ100_DEMO)
	&dev_attr_simulate_sdata.attr,
#endif
	NULL
};

static struct attribute_group core_sysfs = {
	.attrs = core_sysfs_attrs
};

static int mq100_fw_download(struct escore_priv *mq100, int fw_type)
{
	int rc = 0;
	u32 cmd, resp;

	dev_info(mq100->dev, "%s(): firmware download type %d begin\n",
		 __func__, fw_type);
	if (fw_type != STANDARD) {
		dev_err(mq100->dev, "%s(): Unknown firmware type\n", __func__);
		goto fw_download_failed;
	}

	if (!mq100->boot_ops.setup || !mq100->boot_ops.finish) {
		dev_err(mq100->dev, "%s(): boot setup or finish func undef\n",
			__func__);
		goto fw_download_failed;
	}

	/* Reset Mode to Polling */
	mq100->cmd_compl_mode = ES_CMD_COMP_POLL;

#if defined(CONFIG_MQ100_DEMO)
	mq100_reset_cmd();
	msleep(200);
#endif
	rc = mq100->boot_ops.setup(mq100);
	if (rc) {
		dev_err(mq100->dev, "%s(): firmware download start error %d\n",
			__func__, rc);
		goto fw_download_failed;
	}

	rc = mq100->bus.ops.high_bw_write(mq100,
					  (char *)mq100->standard->data,
					  mq100->standard->size);
	if (rc) {
		dev_err(mq100->dev, "%s(): firmware write error %d\n",
			__func__, rc);
		rc = -EIO;
		goto fw_download_failed;
	}

	mq100->mode = fw_type;
	rc = mq100->boot_ops.finish(mq100);
	if (rc) {
		dev_err(mq100->dev, "%s() firmware download finish error %d\n",
			__func__, rc);
		goto fw_download_failed;
	}
	mq100_indicate_state_change(MQ100_STATE_NORMAL);
	dev_info(mq100->dev, "%s(): firmware download type %d done\n",
		 __func__, fw_type);

	if (mq100->pdata->gpioa_gpio != -1) {
		cmd = ((ES_SYNC_CMD | ES_SUPRESS_RESPONSE) << 16) |
					mq100->pdata->gpio_a_irq_type;
		rc = escore_cmd(mq100, cmd, &resp);
		if (rc < 0) {
			pr_err("%s(): API interrupt config failed:%d\n",
					__func__, rc);

			goto fw_download_failed;
		}
		/* Set Interrupt Mode */
		mq100->cmd_compl_mode = ES_CMD_COMP_INTR;
	}

fw_download_failed:
	return rc;
}

int mq100_bootup(struct escore_priv *mq100)
{
	int rc;
	BUG_ON(mq100->standard->size == 0);
	mutex_lock(&mq100->pm_mutex);
	mutex_unlock(&mq100->pm_mutex);

	escore_gpio_reset(mq100);

	rc = mq100_fw_download(mq100, STANDARD);

	if (rc) {
		dev_err(mq100->dev, "%s(): STANDARD fw download error %d\n",
			__func__, rc);
	}
	return rc;
}

irqreturn_t mq100_irq_work(int irq, void *data)
{
#if defined(CONFIG_MQ100_DEMO)
	disable_irq_nosync(irq);
	osp_sh_work_func(&sensor_priv->work);
	schedule_work(&sensor_priv->work);
	return IRQ_HANDLED;
#else
	int rc;
	struct escore_priv *escore = (struct escore_priv *)data;
	u32 event_type, cmd = 0;

	if (!escore) {
		pr_err("%s(): Invalid IRQ data\n", __func__);
		goto irq_exit;
	}

	cmd = ES_GET_EVENT << 16;

	rc = escore_cmd(escore, cmd, &event_type);
	if (rc < 0) {
		pr_err("%s(): Error reading IRQ event\n", __func__);
		goto irq_exit;
	}
	event_type &= ES_MASK_INTR_EVENT;
	escore->escore_event_type = event_type;

	if (event_type != ES_NO_EVENT) {
		pr_debug("%s(): Notify subscribers about 0x%04x event\n",
			 __func__, event_type);
		blocking_notifier_call_chain(escore->irq_notifier_list,
					     event_type, escore);
	}

	if (escore->sensor_cb && escore->sensor_cb->intr)
		escore->sensor_cb->intr(escore->sensor_cb->priv);

irq_exit:
	return IRQ_HANDLED;
#endif
}

static BLOCKING_NOTIFIER_HEAD(mq100_irq_notifier_list);

irqreturn_t mq100_cmd_completion_isr(int irq, void *data)
{
	struct escore_priv *escore = (struct escore_priv *)data;

	BUG_ON(!escore);

	pr_debug("%s(): API Rising edge received\n",
			__func__);
	/* Complete if expected */
	if (escore->wait_api_intr) {
		pr_debug("%s(): API Rising edge completion.\n",
				__func__);
		complete(&escore->cmd_compl);
		escore->wait_api_intr = 0;
	}
	return IRQ_HANDLED;
}

static struct esxxx_platform_data *mq100_populate_dt_pdata(struct device *dev)
{
	struct esxxx_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "%s(): platform data allocation failed\n",
			__func__);
		goto err;
	}
	pdata->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "%s(): get reset_gpio failed\n", __func__);
		goto dt_populate_err;
	}
	dev_dbg(dev, "%s(): reset gpio %d\n", __func__, pdata->reset_gpio);

#ifdef CONFIG_SND_SOC_ES705_EXTCLK_OVER_GPIO
	pdata->extclk_gpio = of_get_named_gpio(dev->of_node, "extclk-gpio", 0);
	if (pdata->extclk_gpio < 0) {
		dev_err(dev, "%s(): get extclk_gpio failed\n", __func__);
		goto dt_populate_err;
	}
	dev_dbg(dev, "%s(): extclk gpio %d\n", __func__, pdata->extclk_gpio);
#endif
	pdata->ext_clk_rate = ESXXX_CLK_19M2;
	pdata->esxxx_clk_cb = NULL;

/* API Interrupt registration */
#ifdef CONFIG_SND_SOC_ES_GPIO_A
	dev_dbg(dev, "%s(): gpioa configured\n", __func__);
	pdata->gpioa_gpio = of_get_named_gpio(dev->of_node, "gpioa-gpio", 0);
	if (pdata->gpioa_gpio < 0) {
		dev_err(dev, "%s(): get gpioa_gpio failed\n", __func__);
		goto dt_populate_err;
	}
#endif

	pdata->gpiob_gpio = of_get_named_gpio(dev->of_node, "gpiob-gpio", 0);
	if (pdata->gpiob_gpio < 0) {
		dev_err(dev, "%s(): get gpiob_gpio failed\n", __func__);
		goto dt_populate_err;
	}
	dev_dbg(dev, "%s(): gpiob gpio %d\n", __func__, pdata->gpiob_gpio);

	pdata->wakeup_gpio = of_get_named_gpio(dev->of_node, "wakeup-gpio", 0);
	if (pdata->wakeup_gpio < 0) {
		dev_err(dev, "%s(): get wakeup_gpio failed\n", __func__);
		pdata->wakeup_gpio = -1;
	}
	dev_dbg(dev, "%s(): wakeup gpio %d\n", __func__, pdata->wakeup_gpio);

	return pdata;

dt_populate_err:
	devm_kfree(dev, pdata);
err:
	return NULL;
}

#if defined(CONFIG_SND_SOC_ES_UART) ||\
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
static struct esxxx_platform_data _platform_data = {
	.reset_gpio = 78,
	.gpioa_gpio = 82,
	.gpiob_gpio = 67,
	.wakeup_gpio = 68,
	.ext_clk_rate = ESXXX_CLK_19M2,
};
#endif
int mq100_core_probe(struct device *dev)
{
	struct esxxx_platform_data *pdata = NULL;
	int rc = 0;
#if defined(CONFIG_SND_SOC_ES_I2C)
	const char *fw_filename = "audience/mq100/audience-mq100-i2c-fw.bin";
#elif defined(CONFIG_SND_SOC_ES_UART)
	const char *fw_filename = "audience/mq100/audience-mq100-uart-fw.bin";
#else
	const char *fw_filename = NULL;
#endif

	if (fw_filename == NULL) {
		dev_err(dev, "%s: Firmware not available for selected config\n",
			__func__);
		goto fw_undef_err;
	}

	if (dev->of_node) {
		dev_info(dev, "Platform data from device tree\n");
		pdata = mq100_populate_dt_pdata(dev);
		dev->platform_data = pdata;
	} else {
		dev_info(dev, "Platform data from board file\n");
#if defined(CONFIG_SND_SOC_ES_UART) ||\
	defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
		pdata = &_platform_data;
#else
		pdata = dev->platform_data;
#endif
	}

	if (pdata == NULL) {
		dev_err(dev, "%s(): pdata is NULL", __func__);
		rc = -EIO;
		goto pdata_error;
	}

	escore_priv.dev = dev;
	escore_priv.pdata = pdata;
	escore_priv.fw_requested = 0;
	if (pdata->esxxx_clk_cb)
		pdata->esxxx_clk_cb(1);

#if defined(CONFIG_SND_SOC_ES_UART)
	pdata->enable_hs_uart_intf = true;
#endif

	escore_priv.boot_ops.bootup = mq100_bootup;
	escore_priv.irq_notifier_list = &mq100_irq_notifier_list;

	escore_priv.flag.is_codec = 0;
	escore_priv.cmd_compl_mode = ES_CMD_COMP_POLL;
	escore_priv.wait_api_intr = 0;
#if defined(CONFIG_SND_SOC_ES_UART_STREAMDEV)
	escore_priv.streamdev = es_uart_streamdev;
#elif defined(CONFIG_SND_SOC_ES_SPI)
	escore_priv.streamdev = es_spi_streamdev;
#endif

	mutex_init(&escore_priv.pm_mutex);
	mutex_init(&escore_priv.wake_mutex);
	mutex_init(&escore_priv.abort_mutex);
	mutex_init(&escore_priv.datablock_dev.datablock_mutex);
	mutex_init(&escore_priv.datablock_dev.datablock_read_mutex);

	init_completion(&escore_priv.cmd_compl);

	if (escore_priv.sensor_cb && escore_priv.sensor_cb->probe != 0) {
		escore_priv.sensor_cb->probe(escore_priv.sensor_cb->priv);
		if (!escore_priv.sensor_cb->priv)
			dev_err(escore_priv.dev,
				"[MQ100]: %s no cookie shared\n", __func__);
	}
	rc = sysfs_create_group(&escore_priv.dev->kobj, &core_sysfs);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): failed to create core sysfs entries: %d\n",
			__func__, rc);
	}
#ifdef CONFIG_SND_SOC_ES705_EXTCLK_OVER_GPIO
	dev_dbg(mq100_priv.dev, "%s(): extclk_gpio = %d\n",
		__func__, pdata->extclk_gpio);
	rc = gpio_request(pdata->extclk_gpio, "mq100_extclk");
	if (rc < 0) {
		dev_warn(mq100_priv.dev, "%s(): mq100_extclk already requested",
			 __func__);
	} else {
		rc = gpio_direction_output(pdata->extclk_gpio, 1);
		if (rc < 0) {
			dev_err(mq100_priv.dev,
				"%s(): mq100_extclk direction failed",
				__func__);
			goto extclk_gpio_direction_error;
		}
		pdata->esxxx_clk_cb = mq100_enable_ext_clk;
		pdata->esxxx_clk_cb(1);
	}
#endif

	dev_dbg(escore_priv.dev, "%s(): reset_gpio = %d\n", __func__,
		pdata->reset_gpio);
	if (pdata->reset_gpio != -1) {
		rc = gpio_request(pdata->reset_gpio, "mq100_reset");
		if (rc < 0) {
			dev_warn(escore_priv.dev,
				 "%s(): mq100_reset already requested",
				 __func__);
		} else {
			rc = gpio_direction_output(pdata->reset_gpio, 0);
			if (rc < 0) {
				dev_err(escore_priv.dev,
					"%s(): mq100_reset direction failed",
					__func__);
				goto reset_gpio_direction_error;
			}
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): mq100_reset undefined\n",
			 __func__);
	}

	dev_dbg(escore_priv.dev, "%s(): wakeup_gpio = %d\n", __func__,
		pdata->wakeup_gpio);

	if (pdata->wakeup_gpio != -1) {
		rc = gpio_request(pdata->wakeup_gpio, "mq100_wakeup");
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): mq100_wakeup request failed", __func__);
			goto wakeup_gpio_request_error;
		}
		rc = gpio_direction_output(pdata->wakeup_gpio, 0);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): mq100_wakeup direction failed",
				__func__);
			goto wakeup_gpio_direction_error;
		}
	} else {
		dev_warn(escore_priv.dev, "%s(): mq100_wakeup undefined\n",
			 __func__);
	}

	rc = request_firmware((const struct firmware **)&escore_priv.standard,
			      fw_filename, escore_priv.dev);
	if (rc) {
		dev_err(escore_priv.dev,
			"%s(): request_firmware(%s) failed %d\n", __func__,
			fw_filename, rc);
		goto request_firmware_error;
	}

	escore_priv.fw_requested = 1;

	if (pdata->gpiob_gpio != -1) {
		rc = request_threaded_irq(gpio_to_irq(pdata->gpiob_gpio),
					  NULL,
					  mq100_irq_work, IRQF_TRIGGER_RISING,
					  "mq100_irq_work", &escore_priv);
		if (rc) {
			dev_err(escore_priv.dev,
				"%s(): event request_irq() failed\n", __func__);
			goto event_irq_request_error;
		}
		rc = irq_set_irq_wake(gpio_to_irq(pdata->gpiob_gpio), 1);
		if (rc < 0) {
			dev_err(escore_priv.dev,
				"%s(): set event irq wake failed\n", __func__);
			disable_irq(gpio_to_irq(pdata->gpiob_gpio));
			free_irq(gpio_to_irq(pdata->gpiob_gpio), &escore_priv);
			goto event_irq_wake_error;
		}

		/* Disable the interrupt till needed */
		if (pdata->gpiob_gpio != -1)
			disable_irq(gpio_to_irq(pdata->gpiob_gpio));

	} else {
		dev_warn(escore_priv.dev, "%s(): mq100_gpiob undefined\n",
			 __func__);
	}

#ifndef CONFIG_SND_SOC_ES_GPIO_A
	dev_dbg(escore_priv.dev, "%s(): gpioa not configured\n", __func__);
	pdata->gpioa_gpio = -1;
#endif
	/* API Interrupt registration */
	if (pdata->gpioa_gpio != -1) {
		rc = request_threaded_irq(gpio_to_irq(pdata->gpioa_gpio),
					  NULL,
					  mq100_cmd_completion_isr,
					  IRQF_TRIGGER_RISING,
					  "mq100_cmd_completion_isr",
					  &escore_priv);
		if (rc) {
			dev_err(escore_priv.dev,
				"%s(): event request_irq() failed\n", __func__);
			goto api_init_err;
		}
		/* Fix value to Rising Edge */
		pdata->gpio_a_irq_type = ES_RISING_EDGE;
	} else {
		dev_warn(escore_priv.dev, "%s(): mq100_gpioa undefined\n",
			 __func__);
	}
	dev_dbg(dev, "%s(): gpioa gpio %d\n", __func__, pdata->gpioa_gpio);
#if defined(CONFIG_MQ100_DEMO)
	sensorhub_probe();
#endif
	return rc;

api_init_err:
	gpio_free(pdata->gpiob_gpio);
event_irq_wake_error:
event_irq_request_error:
/* request_vs_firmware_error: */
	release_firmware(escore_priv.standard);
request_firmware_error:
wakeup_gpio_direction_error:
	gpio_free(pdata->wakeup_gpio);
wakeup_gpio_request_error:
reset_gpio_direction_error:
	gpio_free(pdata->reset_gpio);

pdata_error:
fw_undef_err:
	dev_dbg(escore_priv.dev, "%s(): exit with error\n", __func__);
	return rc;
}

static __init int mq100_init(void)
{
	int rc = 0;

	pr_debug("%s()", __func__);
#if defined(CONFIG_SND_SOC_ES_SLIM)
	escore_priv.device_name = "elemental-addr";
#endif
	mutex_init(&escore_priv.access_lock);
	mutex_init(&escore_priv.intf_probed_mutex);
	init_completion(&escore_priv.fw_download);
	escore_platform_init();
#if defined(CONFIG_SND_SOC_ES_I2C)
	escore_priv.pri_intf = ES_I2C_INTF;
#elif defined(CONFIG_SND_SOC_ES_SLIM)
	escore_priv.pri_intf = ES_SLIM_INTF;
#elif defined(CONFIG_SND_SOC_ES_UART)
	escore_priv.pri_intf = ES_UART_INTF;
#elif defined(CONFIG_SND_SOC_ES_SPI)
	escore_priv.pri_intf = ES_SPI_INTF;
#endif

#if defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C)
	escore_priv.high_bw_intf = ES_I2C_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM)
	escore_priv.high_bw_intf = ES_SLIM_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
	escore_priv.high_bw_intf = ES_UART_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	escore_priv.high_bw_intf = ES_SPI_INTF;
#elif defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_DEFAULT)
	escore_priv.high_bw_intf = escore_priv.pri_intf;
#endif

#if defined(CONFIG_SND_SOC_ES_I2C) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C)
	rc = escore_i2c_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SPI) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI)
	rc = escore_spi_init();
#endif
#if defined(CONFIG_SND_SOC_ES_SLIM) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM)
	rc = escore_slimbus_init();
#endif
#if defined(CONFIG_SND_SOC_ES_UART) || \
		defined(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART)
	rc = escore_uart_bus_init(&escore_priv);
#endif
	if (rc) {
		pr_debug("Error registering Audience driver: %d\n", rc);
		goto INIT_ERR;
	}
#if defined(CONFIG_SND_SOC_ES_CDEV)
	rc = escore_cdev_init(&escore_priv);
	if (rc) {
		pr_debug("Error enabling CDEV interface: %d\n", rc);
		goto INIT_ERR;
	}
#endif
INIT_ERR:
	return rc;
}

module_init(mq100_init);

static __exit void mq100_exit(void)
{
	if (escore_priv.fw_requested)
		release_firmware(escore_priv.standard);

#if defined(CONFIG_MQ100_DEMO)
	sensorhub_remove_input_dev();
#endif
#if defined(CONFIG_SND_SOC_ES_UART_STREAMDEV)
	escore_cdev_cleanup(&escore_priv);
#endif

#if defined(CONFIG_SND_SOC_ES_I2C)
	i2c_del_driver(&escore_i2c_driver);
#else
	/* no support from QCOM to unregister
	 * slim_driver_unregister(&mq100_slim_driver);
	 */
#endif

}

module_exit(mq100_exit);

MODULE_DESCRIPTION("Audience Sensorhub Interface Driver");
MODULE_AUTHOR("Pratik Khade <pkhade@audience.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:motionq");
MODULE_FIRMWARE("audience-mq100-i2c-fw.bin");
