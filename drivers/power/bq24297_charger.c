#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <plat/gpio-cfg.h>
//#include <mach/regs-gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
//#include <mach/gpio.h>
#include <mach/regs-pmu.h>
#include <asm/io.h>
#include <linux/wakelock.h>
#include <mach/hardware.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define BQ24297_DBG
static int bq24297_dbg = 0;
#ifdef BQ24297_DBG
//#define bq24297_print(fmt, args...) printk(fmt, ##args)
#define bq24297_print(fmt, ...) do { if (bq24297_dbg) printk("bq24297: "fmt, ##__VA_ARGS__); } while(0)
#else
//#define bq24297_print(fmt, args...)
#define bq24297_print(fmt, ...)
#endif

/* Register definitions */
#define INPUT_SRC_CTRL_REG              0X00
#define PWR_ON_CONF_REG                 0X01
#define CHARGE_CUR_CTRL_REG             0X02
#define PRE_CHARGE_TERM_CUR_CTRL_REG    0X03
#define CHARGE_VOLT_CTRL_REG            0X04
#define CHARGE_TERM_TIMER_CTRL_REG      0X05
#define BOOST_VOLT_THERM_CTRL_REG       0X06
#define MISC_OPERATION_CTRL_REG         0X07
#define SYSTEM_STATUS_REG               0X08
#define FAULT_REG                       0X09
#define VENDOR_PART_REV_STATUS_REG      0X0A

#define RESET_REGISTER_MASK             0x80
#define INPUT_VOLTAGE_LIMIT_MASK        0x78
#define SYSTEM_MIN_VOLTAGE_MASK         0x0E
#define PRECHG_CURRENT_MASK             0xF0
#define TERM_CURRENT_MASK               0x0F
#define CHG_VOLTAGE_LIMIT_MASK          0xFC
#define EN_CHG_TERM_MASK                0x80
#define PG_STAT_SHIFT                   (2)
#define PG_STAT_MASK                    (1<<PG_STAT_SHIFT)
#define CHRG_STAT_SHIFT                 (4)
#define CHRG_STAT_MASK                  (3<<CHRG_STAT_SHIFT)
#define VBUS_STAT_SHIFT                 (6)
#define VBUS_STAT_MASK                  (3<<VBUS_STAT_SHIFT)
#define BOOST_LIM_MASK                  0x01
#define ICHG_MASK                       0xfc

struct bq24297_chip {
	struct i2c_client  *client;
	struct power_supply *battery;
	struct power_supply usb;
	struct power_supply ac;
	struct delayed_work	work_status;
	int chg_int_gpio;
	int chg_int_irq;
	int usb_online;
	int ac_online;
	int vbus_stat;
	int chrg_stat;
	int pg_stat;
};

static struct bq24297_chip *this_chip = NULL;

static int bq24297_read_reg(int reg)
{
	struct i2c_client *client = this_chip->client;
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
	{
		dev_err(&client->dev, "Error read reg %d\n", reg);
	}

	return ret;
}

static int bq24297_write_reg(int reg, int value)
{
	struct i2c_client *client = this_chip->client;
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
	{
		dev_err(&client->dev, "Error write reg %d value %d\n", reg, value);
	}

	return ret;
}

static int bq24297_masked_write(int reg, u8 mask, u8 val)
{
	struct i2c_client *client = this_chip->client;
	int rc;
	int temp;

	rc = bq24297_read_reg(reg);
	if (rc < 0)
	{
		dev_err(&client->dev, "Error read reg %d in masked write\n", reg);
		return rc;
	}

	temp = rc;
	temp &= ~mask;
	temp |= (val & mask);

	rc = bq24297_write_reg(reg, temp);
	if (rc < 0)
	{
		dev_err(&client->dev, "Error write reg %d in masked write\n", reg);
		return rc;
	}

	return 0;
}

static char *supply_to_list[] = {
	"battery",
};

static enum power_supply_property bq24297_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int bq24297_power_get_property(struct power_supply *ps, 
		enum power_supply_property psp, 
		union power_supply_propval *val)
{
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			if (ps->type == POWER_SUPPLY_TYPE_MAINS)
				val->intval = this_chip->ac_online;
			else if (ps->type == POWER_SUPPLY_TYPE_USB)
				val->intval = this_chip->usb_online;
			break;
		default:
			return -EINVAL;
	}
	bq24297_print("psp:%d, type:%d, value:%d\n", psp, ps->type, val->intval);

	return 0;
}

static ssize_t bq24297_show_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i,ret;

	for (i = 0; i <= 0x0a; i++)
	{
		ret = bq24297_read_reg(i);
		printk("%s, reg[%d]:0x%x\n", __FUNCTION__, i, ret);
	}
	return sprintf(buf, "\n");
}

static DEVICE_ATTR(reg, S_IRUGO, bq24297_show_reg, NULL);

static ssize_t bq24297_show_dbg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq24297_dbg);
}

static ssize_t bq24297_store_dbg(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	if (buf[0]!='0')
		bq24297_dbg = 1;
	else
		bq24297_dbg = 0;
	return count;
}

static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR, bq24297_show_dbg, bq24297_store_dbg);

static struct attribute *bq24297_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_debug.attr,
	NULL
};

static const struct attribute_group bq24297_attr_group = {
	.attrs = bq24297_attributes,
};

static int bq24297_read_version(void)
{
	int ret;
	ret = bq24297_read_reg(VENDOR_PART_REV_STATUS_REG);
	bq24297_print("BQ24297 Ver 0x%x\n", ret);
	return ret;
}

#define VIN_LIMIT_MIN_MV  3880
#define VIN_LIMIT_MAX_MV  5080
#define VIN_LIMIT_STEP_MV  80
static int bq24297_set_input_vin_limit(int mv)
{
	struct i2c_client *client = this_chip->client;
	u8 reg_val = 0;
	int set_vin = 0;

	if (mv < VIN_LIMIT_MIN_MV || mv > VIN_LIMIT_MAX_MV) {
		dev_err(&client->dev, "bad mV=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VIN_LIMIT_MIN_MV)/VIN_LIMIT_STEP_MV;
	set_vin = reg_val * VIN_LIMIT_STEP_MV + VIN_LIMIT_MIN_MV;
	reg_val = reg_val << 3;

	bq24297_print("req_vin = %d set_vin = %d reg_val = 0x%02x\n",
				mv, set_vin, reg_val);

	return bq24297_masked_write(INPUT_SRC_CTRL_REG, INPUT_VOLTAGE_LIMIT_MASK, reg_val);
}

#define SYSTEM_VMIN_LOW_MV  3000
#define SYSTEM_VMIN_HIGH_MV  3700
#define SYSTEM_VMIN_STEP_MV  100
static int bq24297_set_system_vmin(int mv)
{
	struct i2c_client *client = this_chip->client;
	u8 reg_val = 0;
	int set_vmin = 0;

	if (mv < SYSTEM_VMIN_LOW_MV || mv > SYSTEM_VMIN_HIGH_MV) {
		dev_err(&client->dev, "bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - SYSTEM_VMIN_LOW_MV)/SYSTEM_VMIN_STEP_MV;
	set_vmin = reg_val * SYSTEM_VMIN_STEP_MV + SYSTEM_VMIN_LOW_MV;
	reg_val = reg_val << 1;

	bq24297_print("req_vmin = %d set_vmin = %d reg_val = 0x%02x\n",
				mv, set_vmin, reg_val);

	return bq24297_masked_write(PWR_ON_CONF_REG, SYSTEM_MIN_VOLTAGE_MASK, reg_val);
}

#define IPRECHG_MIN_MA  128
#define IPRECHG_MAX_MA  2048
#define IPRECHG_STEP_MA  128
static int bq24297_set_prechg_i_limit(int ma)
{
	struct i2c_client *client = this_chip->client;
	u8 reg_val = 0;
	int set_ma = 0;

	if (ma < IPRECHG_MIN_MA || ma > IPRECHG_MAX_MA) {
		dev_err(&client->dev, "bad ma=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - IPRECHG_MIN_MA)/IPRECHG_STEP_MA;
	set_ma = reg_val * IPRECHG_STEP_MA + IPRECHG_MIN_MA;
	reg_val = reg_val << 4;

	bq24297_print("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24297_masked_write(PRE_CHARGE_TERM_CUR_CTRL_REG, PRECHG_CURRENT_MASK, reg_val);
}

#define ITERM_MIN_MA  128
#define ITERM_MAX_MA  2048
#define ITERM_STEP_MA  128
static int bq24297_set_term_current(int ma)
{
	struct i2c_client *client = this_chip->client;
	u8 reg_val = 0;
	int set_ma = 0;

	if (ma < ITERM_MIN_MA || ma > ITERM_MAX_MA) {
		dev_err(&client->dev, "bad mv=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - ITERM_MIN_MA)/ITERM_STEP_MA;
	set_ma = reg_val * ITERM_STEP_MA + ITERM_MIN_MA;

	bq24297_print("req_i = %d set_i = %d reg_val = 0x%02x\n",
				ma, set_ma, reg_val);

	return bq24297_masked_write(PRE_CHARGE_TERM_CUR_CTRL_REG, TERM_CURRENT_MASK, reg_val);
}

#define VBAT_MAX_MV  4400
#define VBAT_MIN_MV  3504
#define VBAT_STEP_MV  16
static int bq24297_set_vbat_max(int mv)
{
	struct i2c_client *client = this_chip->client;
	u8 reg_val = 0;
	int set_vbat = 0;

	if (mv < VBAT_MIN_MV || mv > VBAT_MAX_MV) {
		dev_err(&client->dev, "bad mv=%d asked to set\n", mv);
		return -EINVAL;
	}

	reg_val = (mv - VBAT_MIN_MV)/VBAT_STEP_MV;
	set_vbat = reg_val * VBAT_STEP_MV + VBAT_MIN_MV;
	reg_val = reg_val << 2;

	bq24297_print("req_vbat = %d set_vbat = %d reg_val = 0x%02x\n",
				mv, set_vbat, reg_val);

	return bq24297_masked_write(CHARGE_VOLT_CTRL_REG, CHG_VOLTAGE_LIMIT_MASK, reg_val);
}

static int bq24297_hw_init(void)
{
	struct i2c_client *client = this_chip->client;
	int ret;
	ret = bq24297_write_reg(PWR_ON_CONF_REG, RESET_REGISTER_MASK);
	if (ret < 0)
	{
		dev_err(&client->dev, "err hw init\n");
		return ret;
	}
	bq24297_set_input_vin_limit(4200);
	bq24297_set_system_vmin(3500);
	bq24297_set_prechg_i_limit(256);
	bq24297_set_term_current(128);
	bq24297_set_vbat_max(4200);
	bq24297_write_reg(CHARGE_TERM_TIMER_CTRL_REG, EN_CHG_TERM_MASK);
	bq24297_masked_write(PWR_ON_CONF_REG, BOOST_LIM_MASK, 0);
	return 0;
}

#define ICHG_MAX_MA 4544
#define ICHG_MIN_MA 512
#define ICHG_STEP_MA 64
static int bq24297_set_ichg_current(int ma)
{
	struct i2c_client *client = this_chip->client;
	u8 reg_val = 0;
	int set_ichg = 0;

	if (ma < ICHG_MIN_MA || ma > ICHG_MAX_MA) {
		dev_err(&client->dev, "bad ma=%d asked to set\n", ma);
		return -EINVAL;
	}

	reg_val = (ma - ICHG_MIN_MA)/ICHG_STEP_MA;
	set_ichg = reg_val * ICHG_STEP_MA + ICHG_MIN_MA;
	reg_val = reg_val << 2;

	bq24297_print("req_ichg = %d set_ichg = %d reg_val = 0x%02x\n",
				ma, set_ichg, reg_val);

	return bq24297_masked_write(CHARGE_CUR_CTRL_REG, ICHG_MASK, reg_val);
}
static void bq24297_set_ac_charge_current(void)
{
	bq24297_set_ichg_current(1024);
}

static void bq24297_set_usb_charge_current(void)
{
	bq24297_set_ichg_current(512);
}

static void bq24297_status_func(struct work_struct *work)
{
	int ret;

	ret = bq24297_read_reg(SYSTEM_STATUS_REG);

	this_chip->pg_stat = (ret & PG_STAT_MASK)>>PG_STAT_SHIFT;
	this_chip->chrg_stat = (ret & CHRG_STAT_MASK)>>CHRG_STAT_SHIFT;
	this_chip->vbus_stat = (ret & VBUS_STAT_MASK)>>VBUS_STAT_SHIFT;

	bq24297_print("%s(), reg[8] = 0x%x\n", __FUNCTION__, ret);

	if (this_chip->pg_stat == 0)
	{
		this_chip->usb_online = 0;
		this_chip->ac_online = 0;
	}
	else if (this_chip->pg_stat == 1 && this_chip->vbus_stat == 0x01)
	{
		this_chip->usb_online = 1;
		this_chip->ac_online = 0;
	}
	else if (this_chip->pg_stat == 1 && this_chip->vbus_stat == 0x02)
	{
		this_chip->usb_online = 0;
		this_chip->ac_online = 1;
	}
	else
	{
		return ;
	}

	if (this_chip->chrg_stat == 0x00)
	{
		this_chip->chrg_stat = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	else if (this_chip->chrg_stat == 0x01)
	{
		this_chip->chrg_stat = POWER_SUPPLY_STATUS_CHARGING;
	}
	else if (this_chip->chrg_stat == 0x02)
	{
		this_chip->chrg_stat = POWER_SUPPLY_STATUS_CHARGING;
	}
	else if (this_chip->chrg_stat == 0x03)
	{
		this_chip->chrg_stat = POWER_SUPPLY_STATUS_FULL;
	}

	if (this_chip->ac_online)
		bq24297_set_ac_charge_current();
	else if (this_chip->usb_online)
		bq24297_set_usb_charge_current();
	else
		bq24297_set_usb_charge_current();


	if (!this_chip->battery)
		this_chip->battery = power_supply_get_by_name("battery");
	if (this_chip->battery)
		power_supply_changed(this_chip->battery);
}

static irqreturn_t chg_int_func(int irqno, void *param)
{
	schedule_delayed_work(&this_chip->work_status, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static int bq24297_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq24297_chip *chip;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device_node *np = client->dev.of_node;
	struct pinctrl *pinctrl;
	int ret = 0;

	dev_info(&client->dev, "%s: addr=0x%x @ IIC%d, irq=%d\n",
			client->name,client->addr,client->adapter->nr,client->irq);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))//I2C_FUNC_SMBUS_BYTE
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	this_chip = chip;
	chip->client = client;
	i2c_set_clientdata(client, chip);

	of_property_read_u32(np, "debug", &bq24297_dbg);

	ret = bq24297_hw_init();
	if (ret < 0)
	{
		goto err_hw_init;
	}
	bq24297_read_version();

	chip->usb.name		= "usb";
	chip->usb.type		= POWER_SUPPLY_TYPE_USB;
	chip->usb.supplied_to	= supply_to_list;
	chip->usb.num_supplicants = ARRAY_SIZE(supply_to_list);
	chip->usb.get_property	= bq24297_power_get_property;
	chip->usb.properties	= bq24297_power_props;
	chip->usb.num_properties	= ARRAY_SIZE(bq24297_power_props);

	chip->ac.name		= "ac";
	chip->ac.type		= POWER_SUPPLY_TYPE_MAINS;
	chip->ac.supplied_to	= supply_to_list;
	chip->ac.num_supplicants = ARRAY_SIZE(supply_to_list);
	chip->ac.get_property	= bq24297_power_get_property;
	chip->ac.properties	= bq24297_power_props;
	chip->ac.num_properties	= ARRAY_SIZE(bq24297_power_props);

	power_supply_register(&client->dev, &chip->usb);
	power_supply_register(&client->dev, &chip->ac);
	chip->battery = power_supply_get_by_name("battery");

	ret = sysfs_create_group(&client->dev.kobj, &bq24297_attr_group);
	if (ret)
	{
		dev_err(&client->dev, "create sysfs error\n");
		goto err_create_sysfs;
	}

	pinctrl = devm_pinctrl_get_select_default(&client->dev);
	if (IS_ERR(pinctrl))
	{
		dev_err(&client->dev, "pinctrl error\n");
		goto err_pinctrl;
	}

	INIT_DELAYED_WORK(&chip->work_status, bq24297_status_func);

	chip->chg_int_gpio = of_get_named_gpio(np, "bq24297,chg_int", 0);
	chip->chg_int_irq = gpio_to_irq(chip->chg_int_gpio);
	//gpio_set_debounce(chip->chg_int_gpio, 1);// TODO
	ret = request_irq(chip->chg_int_irq, chg_int_func,
			IRQF_TRIGGER_RISING, "chg_int", NULL);
	if (ret)
	{
		dev_err(&client->dev, "request_irq error\n");
		goto err_request_irq;
	}

	return 0;

err_request_irq:
err_pinctrl:
	sysfs_remove_group(&client->dev.kobj, &bq24297_attr_group);
err_create_sysfs:
	power_supply_unregister(&chip->usb);
	power_supply_unregister(&chip->ac);
err_hw_init:
	kfree(chip);
	return ret;	
}
 
static int bq24297_remove(struct i2c_client *client)
{
	free_irq(this_chip->chg_int_irq, NULL);
	power_supply_unregister(&this_chip->usb);
	power_supply_unregister(&this_chip->ac);
	sysfs_remove_group(&client->dev.kobj, &bq24297_attr_group);
	kfree(this_chip);
	return 0;
}

#ifdef CONFIG_PM

static int bq24297_suspend(struct i2c_client *client,
		pm_message_t state)
{
	bq24297_print("%s()\n", __FUNCTION__);
	return 0;
}

static int bq24297_resume(struct i2c_client *client)
{
	bq24297_print("%s()\n", __FUNCTION__);
	return 0;
}

#else

#define bq24297_suspend NULL
#define bq24297_resume NULL

#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static struct of_device_id bq24297_of_match_table[] = {
	{
		.compatible = "ti,bq24297-chg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, bq24297_of_match_table);
#endif

#define BQ24297_I2C_NAME "bq24297_i2c"// TODO

static const struct i2c_device_id bq24297_id[] = {
	{ BQ24297_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17058_id);

static struct i2c_driver bq24297_i2c_driver = {
	.driver	= {
		.name	= BQ24297_I2C_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = bq24297_of_match_table,
#endif
	},
	.probe		= bq24297_probe,
	.remove		= bq24297_remove,
	.suspend	= bq24297_suspend,
	.resume		= bq24297_resume,
	.id_table	= bq24297_id,
};

static int __init bq24297_init(void)
{
	
	printk("BQ24297 charger driver: initialize\n");
	return i2c_add_driver(&bq24297_i2c_driver);
}
module_init(bq24297_init);

static void __exit bq24297_exit(void)
{
	i2c_del_driver(&bq24297_i2c_driver);
}
module_exit(bq24297_exit);

MODULE_AUTHOR("Sheng Liang <liang.sheng@samsung.com>");
MODULE_DESCRIPTION("BQ24297 charger");
MODULE_LICENSE("GPL");
