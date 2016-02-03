#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

// DEBUG
#define TMD27713_DBG
static int tmd27713_dbg = 0;
#ifdef TMD27713_DBG
//#define tmd27713_print(fmt, args...) printk(fmt, ##args)
#define tmd27713_print(must_print, fmt, ...) do{if(must_print|tmd27713_dbg)printk("tmd27713: "fmt, ##__VA_ARGS__);}while(0)
#else
//#define tmd27713_print(fmt, args...)
#define tmd27713_print(must_print, fmt, ...)
#endif

// REG definition
#define ENABLE_REG     0x00
#define ATIME_REG      0x01
#define PTIME_REG      0x02
#define WTIME_REG      0x03
#define AILTL_REG      0x04
#define AILTH_REG      0x05
#define AIHTL_REG      0x06
#define AIHTH_REG      0x07
#define PILTL_REG      0x08
#define PILTH_REG      0x09
#define PIHTL_REG      0x0a
#define PIHTH_REG      0x0b
#define PERS_REG       0x0c
#define CONFIG_REG     0x0d
#define PPCOUNT_REG    0x0e
#define CTRL_REG       0x0f
#define ID_REG         0x12
#define STATUS_REG     0x13
#define C0DATA_REG     0x14
#define C0DATAH_REG    0x15
#define C1DATA_REG     0x16
#define C1DATAH_REG    0x17
#define PDATA_REG      0x18
#define PDATAH_REG     0x19

#define COMMAND_REG    0x80
#define CMD_P_INTCLR   0x05
#define CMD_L_INTCLR   0x06
#define CMD_INTCLR     0x07
#define CMD_BYTE_RW    0x00
#define CMD_AUTO_INC   0x20
#define CMD_SP_FUN     0x60

#define AGAIN_MASK     0x03

#define EN_PON         0x01
#define EN_AEN         0x02
#define EN_PEN         0x04
#define EN_WEN         0x08
#define EN_AIEN        0x10
#define EN_PIEN        0x20

#define STAT_AVALID    0x01
#define STAT_AINT      0x10
#define STAT_PINT      0x20

const static int again_table[4] = {1, 8, 16, 120};

struct tmd27713_chip {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct delayed_work dwork;
	int light_int_gpio;
	int light_int_irq;
	int als_on;
	int ps_on;
	int atime;
	int wtime;
	int ptime;
	int again;
	int apers;
	int ppers;
	int persistence;
	int pdiode;
	int als_threshold_lo_param;
	int als_threshold_hi_param;
	int ps_threshold_lo_param;
	int ps_threshold_hi_param;
};
static struct tmd27713_chip *this_chip = NULL;

static int tmd27713_read_byte(u8 reg)
{
	struct i2c_client *client = this_chip->client;
	s32 ret;

	reg &= ~CMD_SP_FUN;
	reg |= COMMAND_REG | CMD_BYTE_RW;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s err reg: 0x%x\n", __FUNCTION__, reg);
	}
	return ret;
}

static int tmd27713_write_byte(u8 reg, u8 data)
{
	struct i2c_client *client = this_chip->client;
	s32 ret;

	reg &= ~CMD_SP_FUN;
	reg |= COMMAND_REG | CMD_BYTE_RW;

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s err reg: 0x%x\n", __FUNCTION__, reg);
	}
	return (int)ret;
}

static int tmd27713_mask_write(u8 reg, u8 mask, u8 val)
{
	struct i2c_client *client = this_chip->client;
	s32 ret;
	s32 temp;

	ret = tmd27713_read_byte(reg);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s err read reg 0x%x\n", __FUNCTION__, reg);
		return ret;
	}

	temp = ret;
	temp &= ~mask;
	temp |= (val & mask);

	ret =  tmd27713_write_byte(reg, temp);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s err write reg 0x%x\n", __FUNCTION__, reg);
		return ret;
	}

	return 0;
}

static int tmd27713_sp_func(u8 cmd)
{
	struct i2c_client *client = this_chip->client;
	s32 ret;

	cmd &= ~CMD_SP_FUN;
	cmd |= COMMAND_REG | CMD_SP_FUN;

	ret = i2c_smbus_write_byte(client, cmd);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s err cmd: 0x%x\n", __FUNCTION__, cmd);
	}
	return (int)ret;
}

static int tmd27713_als_threshold_set(void)
{
	u8 chdata[2];
	u8 als_buf[4];
	int ch0;
	int i;

	for (i = 0; i < 2; i++)
	{
		chdata[i] = tmd27713_read_byte(C0DATA_REG + i);
	}
	ch0 = chdata[0] + (chdata[1] << 8);
	// 0.8~1.2
	this_chip->als_threshold_hi_param = 12*ch0/10;
	if (this_chip->als_threshold_hi_param >= 65535)
		this_chip->als_threshold_hi_param = 65535;
	this_chip->als_threshold_lo_param = 8*ch0/10;
	als_buf[0] = this_chip->als_threshold_lo_param & 0x0ff;
	als_buf[1] = this_chip->als_threshold_lo_param >> 8;
	als_buf[2] = this_chip->als_threshold_hi_param & 0x0ff;
	als_buf[3] = this_chip->als_threshold_hi_param >> 8;
	for (i = 0; i < 4; i++)
	{
		tmd27713_write_byte(AILTL_REG + i, als_buf[i]);
	}
	return 0;
}

static int tmd27713_get_lux(void)
{
#define OPEN_AIR 1
	int GA = OPEN_AIR;
	int cpl, lux1, lux2, lux;
	u8 chdata[4];
	int c0data, c1data;
	int i;
	cpl = 272/100*(256-this_chip->atime)*again_table[this_chip->again]/(GA*24);
	for (i=0;i<4;i++)
	{
		chdata[i] = tmd27713_read_byte(C0DATA_REG + i);
	}
	c0data = (chdata[1]<<8) | chdata[0];
	c1data = (chdata[3]<<8) | chdata[2];
	lux1 = (c0data-2*c1data)/cpl;
	lux2 = (3*c0data-5*c1data)/5/cpl;
	lux = max(lux1, lux2);
	lux = max(0, lux);
	tmd27713_print(0, "lux:%d\n", lux);
	return lux;
}

static int tmd27713_als_get_data(void)
{
	int lux = tmd27713_get_lux();
	input_report_abs(this_chip->input_dev, ABS_MISC, lux);
	input_sync(this_chip->input_dev);
	return 0;
}

static int tmd27713_als_on(int on)
{
	if (on)
	{
		tmd27713_sp_func(CMD_L_INTCLR);
		tmd27713_write_byte(ATIME_REG, this_chip->atime);
		tmd27713_write_byte(PERS_REG, this_chip->persistence);
		tmd27713_mask_write(CTRL_REG, AGAIN_MASK, this_chip->again);
		tmd27713_mask_write(ENABLE_REG, EN_AEN | EN_AIEN | EN_PON, EN_AEN | EN_AIEN | EN_PON);
		tmd27713_als_threshold_set();
		input_report_abs(this_chip->input_dev, ABS_MISC, 0);
		input_sync(this_chip->input_dev);
		tmd27713_als_get_data();
		this_chip->als_on = 1;
	}
	else
	{
		tmd27713_mask_write(ENABLE_REG, EN_AEN | EN_AIEN, 0);
		this_chip->als_on = 0;
	}
	return 0;
}

static int tmd27713_sensor_on(int on)
{
	if (on)
	{
		tmd27713_sp_func(CMD_L_INTCLR);
		tmd27713_write_byte(ATIME_REG, this_chip->atime);
		tmd27713_write_byte(PTIME_REG, this_chip->ptime);
		tmd27713_write_byte(WTIME_REG, this_chip->wtime);
		tmd27713_write_byte(PERS_REG, this_chip->persistence);
		tmd27713_write_byte(CONFIG_REG, 0);// WLONG = 0
		tmd27713_write_byte(PPCOUNT_REG, 0x08);//TODO
		tmd27713_write_byte(CTRL_REG, 0x20);// P use both
		tmd27713_mask_write(ENABLE_REG, EN_AEN | EN_PEN | EN_WEN | EN_PON, EN_AEN | EN_PEN | EN_WEN | EN_PON);
	}
	else
	{
		tmd27713_mask_write(ENABLE_REG, EN_PEN | EN_WEN, 0);
	}
	return 0;
}

static int tmd27713_ps_poll(void)
{
	int i;
	u8 chdata[6];
	int ps_data;
	for (i = 0; i< 6; i++)
	{
		chdata[i] = tmd27713_read_byte(C0DATA_REG + i);
	}
	ps_data = chdata[4] + (chdata[5]<<8);
	tmd27713_print(0, "%s ps_data:0x%x\n", __FUNCTION__, ps_data);
	return ps_data;
}

#if 1
static int tmd27713_ps_calibrate(int start)
{
	u8 ps_buf[4];
	int i;
	int ps_val;
	ps_val = tmd27713_ps_poll();
	if (start)
	{
		if (ps_val < this_chip->ps_threshold_hi_param)
		{
			ps_buf[0] = 0;
			ps_buf[1] = 0;
			ps_buf[2] = this_chip->ps_threshold_hi_param & 0x0ff;
			ps_buf[3] = this_chip->ps_threshold_hi_param >> 8;
		}
		else
		{
			ps_buf[0] = this_chip->ps_threshold_lo_param & 0x0ff;
			ps_buf[1] = this_chip->ps_threshold_lo_param >> 8;
			ps_buf[2] = 0;
			ps_buf[3] = 0;
		}
	}
	else
	{
		if (ps_val < this_chip->ps_threshold_lo_param)
		{
			ps_buf[0] = 0;
			ps_buf[1] = 0;
			ps_buf[2] = this_chip->ps_threshold_hi_param & 0x0ff;
			ps_buf[3] = this_chip->ps_threshold_hi_param >> 8;
			input_report_abs(this_chip->input_dev, ABS_DISTANCE, 0);
			input_sync(this_chip->input_dev);
		}
		else if (ps_val > this_chip->ps_threshold_hi_param )
		{
			ps_buf[0] = this_chip->ps_threshold_lo_param & 0x0ff;
			ps_buf[1] = this_chip->ps_threshold_lo_param >> 8;
			ps_buf[2] = 0;
			ps_buf[3] = 0;
			input_report_abs(this_chip->input_dev, ABS_DISTANCE, 1);
			input_sync(this_chip->input_dev);
		}
		else
		{
			return 0;
		}
	}
	for (i = 0; i < 4; i++)
	{
		tmd27713_write_byte(PILTL_REG + i, ps_buf[i]);
	}
	return 0;
}
#endif

#if 1
void tmd27713_delayed_work_func(struct work_struct *work)
{
	tmd27713_print(0, "%s\n", __FUNCTION__);
	tmd27713_ps_poll();
	schedule_delayed_work(&this_chip->dwork, msecs_to_jiffies(300));
}
#endif

#if 1
static int tmd27713_ps_on(int on)
{
	if (on)
	{
		tmd27713_mask_write(ENABLE_REG, EN_PEN | EN_PIEN | EN_PON, EN_PEN | EN_PIEN | EN_PON);
		this_chip->ps_on = 1;
	}
	else
	{
		tmd27713_mask_write(ENABLE_REG, EN_PEN | EN_PIEN, 0);
		this_chip->ps_on = 0;
	}
	return 0;
}
#endif

static int tmd27713_get_data(void)
{
	int ret = 0;
	int status;

	status = tmd27713_read_byte(STATUS_REG);
	tmd27713_print(0, "%s, status:0x%x\n", __FUNCTION__, status);
	if ((status & 0x20) == 0x20)
	{
		tmd27713_ps_calibrate(0);
		//tmd27713_ps_poll();
		tmd27713_sp_func(CMD_P_INTCLR);
	}

	if ((status & 0x11) == 0x11)
	{
		tmd27713_als_threshold_set();
		tmd27713_als_get_data();
		tmd27713_sp_func(CMD_L_INTCLR);
	}

	return ret;
}
// ATTR
static ssize_t tmd27713_show_dbg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", tmd27713_dbg);
}

static ssize_t tmd27713_store_dbg(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	if (buf[0]!='0')
		tmd27713_dbg = 1;
	else
		tmd27713_dbg = 0;
	return count;
}

static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR, tmd27713_show_dbg, tmd27713_store_dbg);

static ssize_t tmd27713_show_enable_als(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", this_chip->als_on);
}

static ssize_t tmd27713_store_enable_als(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	if (buf[0]!='0')
	{
		tmd27713_als_on(1);
	}
	else
	{
		tmd27713_als_on(0);
	}
	return count;
}

static DEVICE_ATTR(enable_als, S_IRUGO|S_IWUSR, tmd27713_show_enable_als, tmd27713_store_enable_als);

static ssize_t tmd27713_show_enable_ps(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", this_chip->ps_on);
}

static ssize_t tmd27713_store_enable_ps(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	if (buf[0]!='0')
	{
		tmd27713_sensor_on(1);
		tmd27713_ps_calibrate(1);
		tmd27713_ps_on(1);
		//schedule_delayed_work(&this_chip->dwork, msecs_to_jiffies(300));
	}
	else
	{
		//cancel_delayed_work(&this_chip->dwork);
		tmd27713_ps_on(0);
		tmd27713_sensor_on(0);
	}
	return count;
}

static DEVICE_ATTR(enable_ps, S_IRUGO|S_IWUSR, tmd27713_show_enable_ps, tmd27713_store_enable_ps);

static struct attribute *tmd27713_attributes[] = {
	&dev_attr_enable_als.attr,
	&dev_attr_enable_ps.attr,
	&dev_attr_debug.attr,
	NULL
};

static const struct attribute_group tmd27713_attr_group = {
	.attrs = tmd27713_attributes,
};

static int tmd27713_get_id(void)
{
	int ret;
	ret = tmd27713_read_byte(ID_REG);
	tmd27713_print(0, "id 0x%x\n", ret);
	return ret;
}

static irqreturn_t tmd27713_thread_irq(int irq, void *data)
{
	tmd27713_print(0, "%s\n", __FUNCTION__);
	tmd27713_get_data();

	return IRQ_HANDLED;
}

static int tmd27713_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tmd27713_chip *chip;
	struct device_node *np = client->dev.of_node;
	struct pinctrl *pinctrl;
	int ret;
	int val;
	int i;

	dev_info(&client->dev, "%s: addr=0x%x @ IIC%d, irq=%d\n",
			client->name,client->addr,client->adapter->nr,client->irq);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
	{
		dev_err(&client->dev, "i2c smbus byte data unsupported\n");
		return -EIO;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
	{
		dev_err(&client->dev, "no mem\n");
		return -ENOMEM;
	}
	this_chip = chip;
	chip->client = client;
	i2c_set_clientdata(client, chip);

	of_property_read_u32(np, "debug", &tmd27713_dbg);
	of_property_read_u32(np, "tmd27713,atime", &val);
	chip->atime = (256*272-val*100)/272;
	of_property_read_u32(np, "tmd27713,again", &val);
	for (i = 0; i < 4; i++)
	{
		if (again_table[i] > val)
		{
			chip->again = max(i-1, 0);
			break;
		}
	}
	of_property_read_u32(np, "tmd27713,ptime", &val);
	chip->ptime = (256*272-val*100)/272;
	of_property_read_u32(np, "tmd27713,wtime", &val);
	chip->wtime = (256*272-val*100)/272;
	of_property_read_u32(np, "tmd27713,apers", &val);
	chip->persistence = val&0x0f;
	of_property_read_u32(np, "tmd27713,ppers", &val);
	chip->persistence |= ((val&0x0f)<<4);
	of_property_read_u32(np, "tmd27713,pdiode", &val);
	if (val >= 0 && val <= 2)
	{
		chip->pdiode = val + 1;
	}
	else
	{
		chip->pdiode = 3;
	}
	tmd27713_print(0, "atime:0x%x, ptime:0x%x, wtime:0x%x\n", chip->atime, chip->ptime, chip->wtime);
	tmd27713_print(0, "again:0x%x, pers:0x%x, pdiode:0x%x\n", chip->again, chip->persistence, chip->pdiode);
	chip->ps_threshold_lo_param = 200;
	chip->ps_threshold_hi_param = 1000;

	tmd27713_get_id();

	chip->input_dev = devm_input_allocate_device(&client->dev);
	if (chip->input_dev == NULL)
	{
		dev_err(&client->dev, "input_allocate_device error\n");
		goto err_alloc_input;
	}
	chip->input_dev->name = "tmd27713";
	chip->input_dev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->input_dev->evbit);
	input_set_capability(chip->input_dev, EV_ABS, ABS_MISC);
	input_set_capability(chip->input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(chip->input_dev, ABS_MISC, 0, 65535, 0, 0);
	input_set_abs_params(chip->input_dev, ABS_DISTANCE, 0, 1, 0, 0);
	ret = input_register_device(chip->input_dev);
	if (ret < 0)
	{
		dev_err(&client->dev, "input_register_device error\n");
		goto err_register_input;
	}

	ret = sysfs_create_group(&client->dev.kobj, &tmd27713_attr_group);
	if (ret < 0)
	{
		dev_err(&client->dev, "create sysfs error\n");
		goto err_create_sysfs;
	}

	INIT_DELAYED_WORK(&chip->dwork, tmd27713_delayed_work_func);

	pinctrl = devm_pinctrl_get_select_default(&client->dev);
	if (IS_ERR(pinctrl))
	{
		dev_err(&client->dev, "pinctrl error\n");
		goto err_pinctrl;
	}

	chip->light_int_gpio = of_get_named_gpio(np, "tmd27713,lp_int", 0);
	chip->light_int_irq = gpio_to_irq(chip->light_int_gpio);
	ret = request_threaded_irq(chip->light_int_irq, NULL, tmd27713_thread_irq,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			"tmd27713interrupt", chip);
	if (ret < 0)
	{
		dev_err(&client->dev, "request_threaded_irq error\n");
		goto err_req_irq;
	}

	return 0;

err_req_irq:
err_pinctrl:
	sysfs_remove_group(&client->dev.kobj, &tmd27713_attr_group);
err_create_sysfs:
	//input_unregister_device(chip->input_dev);
err_register_input:
	//input_free_device(chip->input_dev);
err_alloc_input:
	kfree(chip);
	return -EINVAL;
}

static int tmd27713_remove(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_PM
static int tmd27713_suspend(struct i2c_client *client,
		pm_message_t state)
{
	return 0;
}

static int tmd27713_resume(struct i2c_client *client)
{
	return 0;
}
#else
 #define tmd27713_suspend NULL
 #define tmd27713_resume NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static struct of_device_id tmd27713_of_match_table[] = {
	{
		.compatible = "taos,tmd27713",
	},
	{},
};
MODULE_DEVICE_TABLE(of, tmd27713_of_match_table);
#endif

#define TMD27713_I2C_NAME "tmd27713_i2c"//TODO

static const struct i2c_device_id tmd27713_id[] = {
	{ TMD27713_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmd27713_id);

static struct i2c_driver tmd27713_i2c_driver = {
	.driver	= {
		.name	= TMD27713_I2C_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tmd27713_of_match_table,
#endif
	},
	.probe		= tmd27713_probe,
	.remove		= tmd27713_remove,
	.suspend	= tmd27713_suspend,
	.resume		= tmd27713_resume,
	.id_table	= tmd27713_id,
};

module_i2c_driver(tmd27713_i2c_driver);

MODULE_AUTHOR("Sheng Liang <liang.sheng@samsung.com>");
MODULE_DESCRIPTION("TMD27713");
MODULE_LICENSE("GPL");
