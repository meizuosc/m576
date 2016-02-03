/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Datasheets:
 * http://focus.ti.com/docs/prod/folders/print/bq27000.html
 * http://focus.ti.com/docs/prod/folders/print/bq27500.html
 * http://www.ti.com/product/bq27411-g1
 * http://www.ti.com/product/bq27421-g1
 * http://www.ti.com/product/bq27425-g1
 * http://www.ti.com/product/bq27441-g1
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/wakelock.h>


#include "bq2589x_reg.h"



#include <linux/power/bq27x00_battery.h>
#include <linux/kthread.h>

#define DRIVER_VERSION			"1.2.0"

#define INVALID_REG_ADDR		0xFF
#define TAG "[m86-battery]"
#define DEBUG_BATTERY 0

enum bq27xxx_reg_index {
	BQ27XXX_REG_CTRL = 0,
	BQ27XXX_REG_TEMP,
	BQ27XXX_REG_INT_TEMP,
	BQ27XXX_REG_VOLT,
	BQ27XXX_REG_AI,
	BQ27XXX_REG_FLAGS,
	BQ27XXX_REG_TTE,
	BQ27XXX_REG_TTF,
	BQ27XXX_REG_TTES,
	BQ27XXX_REG_TTECP,
	BQ27XXX_REG_NAC,
	BQ27XXX_REG_FCC,
	BQ27XXX_REG_CYCT,
	BQ27XXX_REG_AE,
	BQ27XXX_REG_SOC,
	BQ27XXX_REG_DCAP,
	BQ27XXX_POWER_AVG,
	NUM_REGS
};


/* bq27532 registers */
static __initdata u8 bq2753x_regs[] = {
	0x00,	/* CONTROL	*/
	0x06,	/* TEMP		*/
	0xFF,	/* INT TEMP - NA*/
	0x08,	/* VOLT		*/
	0x14,	/* AVG CURR	*/
	0x0A,	/* FLAGS	*/
	0x16,	/* TTE		*/
	0xFF,	/* TTF - NA	*/
	0xFF,	/* TTES - NA	*/
	0xFF,	/* TTECP - NA	*/
	0x0C,	/* NAC		*/
	0x12,	/* LMD(FCC)	*/
	0x1E,	/* CYCT		*/
	0xFF,	/* AE - NA	*/
	0x20,	/* SOC(RSOC)	*/
	0xFF,	/* DCAP(ILMD) - NA */
	0x24,	/* AP		*/
};

/*
 * SBS Commands for DF access - these are pretty standard
 * So, no need to go in the command array
 */
#define PACK_VER                       0x5F
#define BLOCK_DATA_CLASS		0x3E
#define DATA_BLOCK			0x3F
#define BLOCK_DATA			0x40
#define BLOCK_DATA_CHECKSUM		0x60
#define BLOCK_DATA_CONTROL		0x61

/* bq274xx/bq276xx specific command information */
#define BQ274XX_UNSEAL_KEY		0x36720414
#define BQ274XX_UNSEAL_KEY2		0xffffffff

#define BQ274XX_SOFT_RESET		0x0041

#define BQ274XX_FLAG_ITPOR				0x20
#define BQ274XX_CTRL_STATUS_INITCOMP	0x80

#define BQ27XXX_FLAG_DSC		BIT(0)
#define BQ27XXX_FLAG_SOCF		BIT(1) /* State-of-Charge threshold final */
#define BQ27XXX_FLAG_SOC1		BIT(2) /* State-of-Charge threshold 1 */
#define BQ27XXX_FLAG_FC			BIT(9)
#define BQ27XXX_FLAG_OTD		BIT(14)
#define BQ27XXX_FLAG_OTC		BIT(15)

/* BQ27000 has different layout for Flags register */
#define BQ27200_FLAG_EDVF		BIT(0) /* Final End-of-Discharge-Voltage flag */
#define BQ27200_FLAG_EDV1		BIT(1) /* First End-of-Discharge-Voltage flag */
#define BQ27200_FLAG_CI			BIT(4) /* Capacity Inaccurate flag */
#define BQ27200_FLAG_FC			BIT(5)
#define BQ27200_FLAG_CHGS		BIT(7) /* Charge state flag */

#define BQ27200_RS			20 /* Resistor sense */
#define BQ27200_POWER_CONSTANT		(256 * 29200 / 1000)

/* Subcommands of Control() */
#define CONTROL_STATUS_SUBCMD		0x0000
#define DEV_TYPE_SUBCMD			0x0001
#define FW_VER_SUBCMD			0x0002
#define CELL_VER_SUBCMD                        0x0008

#define DF_VER_SUBCMD			0x001F
#define SMOOTH_SYNC_SUBCMD		0x001E

#define RESET_SUBCMD			0x0041
#define SET_CFGUPDATE_SUBCMD		0x0061
#define SEAL_SUBCMD			0x0020

/* Location of SEAL enable bit in bq276xx DM */
#define BQ276XX_OP_CFG_B_SUBCLASS	 64
#define BQ276XX_OP_CFG_B_OFFSET		2
#define BQ276XX_OP_CFG_B_DEF_SEAL_BIT	(1 << 5)

struct bq27x00_device_info;
struct bq27x00_access_methods {
	int (*read)(struct bq27x00_device_info *di, u8 reg, bool single);
	int (*write)(struct bq27x00_device_info *di, u8 reg, int value,
			bool single);
	int (*blk_read)(struct bq27x00_device_info *di, u8 reg, u8 *data,
			u8 sz);
	int (*blk_write)(struct bq27x00_device_info *di, u8 reg, u8 *data,
			u8 sz);
};

enum bq27x00_chip { BQ27200, BQ27500, BQ27520, BQ274XX, BQ276XX, BQ2753X,
	BQ27542, BQ27545};

struct bq27x00_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int energy;
	int flags;
	int power_avg;
	int health;
};

struct dm_reg {
	u8 subclass;
	u8 offset;
	u8 len;
	u32 data;
};

struct bq27x00_device_info {
	struct device		*dev;
	int			id;
	enum bq27x00_chip	chip;
	int irq_pin;

	struct bq27x00_reg_cache cache;
	int charge_design_full;

	unsigned long last_update;
	struct delayed_work work;
	struct delayed_work poll_work;
	struct delayed_work rom_update_work;

	struct power_supply	bat;

	struct bq27x00_access_methods bus;

	struct mutex lock;

	int fw_ver;
	int df_ver;
	u8 regs[NUM_REGS];
	struct dm_reg *dm_regs;
	u16 dm_regs_count;

	struct class *battery_class;
	struct device *battery_device;
	bool fake_temp_en;
	int fake_temp;
	bool present;
	struct wake_lock battery_i2c_wake;
	bool is_already_shutdown;
	bool is_updating;
};

static __initdata enum power_supply_property bq2753x_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
};
BLOCKING_NOTIFIER_HEAD(temp_notifier_list);
int register_temp_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&temp_notifier_list,n);
}
int unregister_temp_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_unregister(&temp_notifier_list,n);
}

static unsigned int poll_interval = 30;
module_param(poll_interval, uint, 0644);
MODULE_PARM_DESC(poll_interval, "battery poll interval in seconds - " \
		"0 disables polling");

/**
 * In order to over-discharge battery for test precharge function,
 * we provide sysfs interface to let battery driver report batterry
 * capacity not less than 1%.
 */
static bool fake_cap_enable = false;

/*
 * Forward Declarations
 */
static int read_dm_block(struct bq27x00_device_info *di, u8 subclass,
		u8 offset, u8 *data);


/*
 * Common code for BQ27x00 devices
 */

static inline int bq27xxx_read(struct bq27x00_device_info *di, int reg_index,
		bool single)
{
	int val;

	/* Reports 0 for invalid/missing registers */
	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return 0;

	val = di->bus.read(di, di->regs[reg_index], single);

	return val;
}

static inline int bq27xxx_write(struct bq27x00_device_info *di, int reg_index,
		int value, bool single)
{
	if (!di || di->regs[reg_index] == INVALID_REG_ADDR)
		return -1;

	return di->bus.write(di, di->regs[reg_index], value, single);
}

int control_cmd_wr(struct bq27x00_device_info *di, u16 cmd)
{
	dev_dbg(di->dev, "%s: cmd - %04x\n", __func__, cmd);

	return di->bus.write(di, BQ27XXX_REG_CTRL, cmd, false);
}

static int control_cmd_read(struct bq27x00_device_info *di, u16 cmd)
{
	dev_dbg(di->dev, "%s: cmd - %04x\n", __func__, cmd);

	di->bus.write(di, BQ27XXX_REG_CTRL, cmd, false);

	msleep(5);

	return di->bus.read(di, BQ27XXX_REG_CTRL, false);
}
/*
 * It is assumed that the gauge is in unsealed mode when this function
 * is called
 */
static int bq276xx_seal_enabled(struct bq27x00_device_info *di)
{
	u8 buf[32];
	u8 op_cfg_b;

	if (!read_dm_block(di, BQ276XX_OP_CFG_B_SUBCLASS,
				BQ276XX_OP_CFG_B_OFFSET, buf)) {
		return 1; /* Err on the side of caution and try to seal */
	}

	op_cfg_b = buf[BQ276XX_OP_CFG_B_OFFSET & 0x1F];

	if (op_cfg_b & BQ276XX_OP_CFG_B_DEF_SEAL_BIT)
		return 1;

	return 0;
}

#define SEAL_UNSEAL_POLLING_RETRY_LIMIT	1000

static inline int sealed(struct bq27x00_device_info *di)
{
	return control_cmd_read(di, CONTROL_STATUS_SUBCMD) & (3 << 13);
}

static int unseal(struct bq27x00_device_info *di, u32 key)
{
	dev_dbg(di->dev, "%s: key - %08x\n", __func__, key);
	di->bus.write(di, 0,0x0414, false);
	msleep(50);
	di->bus.write(di, 0,0x3672, false);
	msleep(50);
	di->bus.write(di, 0,0xffff, false);
	msleep(50);
	di->bus.write(di, 0, 0xffff, false);
	msleep(50);
	return 1;
}

static int seal(struct bq27x00_device_info *di)
{
	int i = 0;
	int is_sealed;

	dev_dbg(di->dev, "%s:\n", __func__);

	is_sealed = sealed(di);
	if (is_sealed)
		return is_sealed;

	if (di->chip == BQ276XX && !bq276xx_seal_enabled(di)) {
		dev_dbg(di->dev, "%s: sealing is not enabled\n", __func__);
		return is_sealed;
	}

	di->bus.write(di, BQ27XXX_REG_CTRL, SEAL_SUBCMD, false);

	while (i < SEAL_UNSEAL_POLLING_RETRY_LIMIT) {
		i++;
		is_sealed = sealed(di);
		if (is_sealed)
			break;
		msleep(10);
	}

	if (!is_sealed)
		dev_err(di->dev, "%s: failed\n", __func__);

	return is_sealed;
}
bool is_gauge_modified(struct bq27x00_device_info *di)
{//read block 28 status
	int ret;
	di->bus.write(di, 0x3f, 1, true);
	msleep(5);
	ret = di->bus.read(di, 0x5c,true);
	if(ret < 0){
		pr_err(TAG"%s i2c error\n",__func__);
		ret = 0;
	}
	pr_info(TAG"%s block 28 ret = %d\n",__func__,ret);
	return ret;
}
static int bq27x00_battery_reset(struct bq27x00_device_info *di);

#define CFG_UPDATE_POLLING_RETRY_LIMIT 50
int enter_cfg_update_mode(struct bq27x00_device_info *di)
{
#if 0
	u8 old_chksum,new_chksum, old_byte,new_byte=0x88;
	u8 temp;
#endif
	pr_info("%s:\n", __func__);

	if (!unseal(di, BQ274XX_UNSEAL_KEY))
		return 0;
	msleep(10);
	return 1;
}

int exit_cfg_update_mode(struct bq27x00_device_info *di,int ret)
{
	pr_info("%s:\n", __func__);

	//control_cmd_wr(di, BQ274XX_SOFT_RESET);
	if(ret ==0)
		bq27x00_battery_reset(di);

	if (seal(di))
		return 1;
	else
		return 0;
}
u8 checksum(u8 *data)
{
	u16 sum = 0;
	int i;

	for (i = 0; i < 32; i++)
		sum += data[i];

	sum &= 0xFF;

	return 0xFF - sum;
}

#ifdef DEBUG
static void print_buf(const char *msg, u8 *buf)
{
	int i;

	printk("\nbq: %s buf: ", msg);
	for (i = 0; i < 32; i++)
		printk("%02x ", buf[i]);

	printk("\n");
}
#else
#define print_buf(a, b)
#endif
/*
 *return 1 when update has been done. 0 when in process
 */
int update_dm_block(struct bq27x00_device_info *di, u8 subclass,
		u8 offset, u8 data)
{
	u8 old_cksum,new_cksum,temp_sum,old_data;
	u8 blk_offset = offset >> 5;
	u8 real_offset = offset%32;
	int retry_count = 0;
retry:
	pr_info("%s: subclass %d offset %d blk_offset %d real_offset %d\n",
			__func__, subclass, offset,blk_offset,real_offset);
	retry_count++;
	di->bus.write(di, BLOCK_DATA_CONTROL, 0, true);
	msleep(10);
	di->bus.write(di, BLOCK_DATA_CLASS, subclass, true);
	msleep(10);

	di->bus.write(di, DATA_BLOCK, blk_offset, true);
	msleep(10);

	old_data = di->bus.read(di, BLOCK_DATA+real_offset,true);
	if(old_data == data){
		pr_info(TAG"%s already updated",__func__);
		return 1;
	}
	old_cksum = di->bus.read(di, BLOCK_DATA_CHECKSUM,true);
	pr_info(TAG"%s old_data = 0x%2x old_cksum = 0x%2x\n",__func__,old_data,old_cksum);
	di->bus.write(di, BLOCK_DATA+real_offset, data, true);
	msleep(10);
	//print_buf(__func__, data);

	temp_sum = (255-old_cksum-old_data)%256;
	new_cksum = 255 - (temp_sum + data)%256;
	pr_info(TAG"%s new_cksum=0x%02x\n",__func__,new_cksum);
	di->bus.write(di, BLOCK_DATA_CHECKSUM, new_cksum, true);
	msleep(200);
#if 1
	di->bus.write(di, BLOCK_DATA_CONTROL, 0, true);
	msleep(10);
	di->bus.write(di, BLOCK_DATA_CLASS, subclass, true);
	msleep(10);

	di->bus.write(di, DATA_BLOCK, blk_offset, true);
	msleep(10);

	old_data = di->bus.read(di, BLOCK_DATA+real_offset,true);
	if (old_data != data) {
		dev_err(di->dev, "%s: error updating subclass %d offset %d read_back data = %d\n",
				__func__, subclass, offset,old_data);
		if(retry_count < 5){
			pr_info(TAG"%s update fail, try again",__func__);
			goto retry;
		}
	}
	msleep(10);
	return 0;
#endif
	//return 1;
}

static int read_dm_block(struct bq27x00_device_info *di, u8 subclass,
		u8 offset, u8 *data)
{
	u8 cksum_calc, cksum;
	u8 blk_offset = offset >> 5;

	dev_dbg(di->dev, "%s: subclass %d offset %d\n",
			__func__, subclass, offset);

	di->bus.write(di, BLOCK_DATA_CONTROL, 0, true);
	msleep(5);

	di->bus.write(di, BLOCK_DATA_CLASS, subclass, true);
	msleep(5);

	di->bus.write(di, DATA_BLOCK, blk_offset, true);
	msleep(5);

	di->bus.blk_read(di, BLOCK_DATA, data, 32);

	cksum_calc = checksum(data);
	cksum = di->bus.read(di, BLOCK_DATA_CHECKSUM, true);
	if (cksum != cksum_calc) {
		dev_err(di->dev, "%s: error reading subclass %d offset %d\n",
				__func__, subclass, offset);
		return 0;
	}

	print_buf(__func__, data);

	return 1;
}
static int bq27x00_battery_read_cell_version(struct bq27x00_device_info *di)
{
	bq27xxx_write(di, BQ27XXX_REG_CTRL, CELL_VER_SUBCMD, false);

	msleep(10);

	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}
static int bq27x00_battery_read_pack_version(struct bq27x00_device_info *di)
{
	di->bus.write(di, DATA_BLOCK, 1, true);
	msleep(10);

	return di->bus.read(di, PACK_VER, true);
}

/*
 * Return the battery State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_soc(struct bq27x00_device_info *di)
{
	int soc;
	int volt;

	soc = bq27xxx_read(di, BQ27XXX_REG_SOC, false);

	if (soc < 0)
		dev_dbg(di->dev, "error reading relative State-of-Charge\n");

	volt = bq27xxx_read(di, BQ27XXX_REG_VOLT, false);
	if (volt < 0) {
		dev_err(di->dev, "error reading voltage\n");
	}else{
		if(volt < 3400)//shut down when voltage is lower than 3.4v
			soc = 0;
	}

	return soc;
}

static struct bq27x00_device_info *g_di=NULL;
int charger_get_manufacture(void)
{
	if(g_di){
		int cell;

		cell = bq27x00_battery_read_cell_version(g_di);
		if(DEBUG_BATTERY)
			pr_info(TAG"%s cell id = 0x%4x\n",__func__,cell);
		switch(cell){
			case 0x3481:
				return 0;//sony
			case 0x3602:
				return 0;//sony-mp
			case 0x3419:
				if(DEBUG_BATTERY)
					pr_info(TAG"%s old sony battery detected\n",__func__);
				return 0;//sony
			case 0x3460:
				if(DEBUG_BATTERY)
					pr_info(TAG"%s old atl battery detected\n",__func__);
				return 1;//atl
			case 0x3575:
				return 1;//atl-mp
			default:
				return  -1;//unknown
		}
	}else{
		return  -1;
	}
}

/*
 * Return the battery temperature in tenths of degree Kelvin
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_temperature(struct bq27x00_device_info *di)
{
	int temp;

	temp = bq27xxx_read(di, BQ27XXX_REG_TEMP, false);
	if(DEBUG_BATTERY){
		pr_info("%s temp = %d\n",__func__,temp);
	}
	if (temp < 0) {
		dev_err(di->dev, "error reading temperature\n");
		return -1000;
	}

	temp = temp -2731;
	if(di->fake_temp_en){
		temp = di->fake_temp;
	}

	if(charger_get_manufacture()==0){//sony battery
		if(temp > -200 && temp < 0){
			blocking_notifier_call_chain(&temp_notifier_list,SONY_FREEZE_TEMP,NULL);
		}else if(temp >= 0 && temp < 100){
			blocking_notifier_call_chain(&temp_notifier_list,SONY_COLD_TEMP,NULL);
		}else if(temp >= 100 && temp < 150){
			blocking_notifier_call_chain(&temp_notifier_list,SONY_ROOM_TEMP,NULL);
		}else if(temp >=150 && temp <= 450){
			blocking_notifier_call_chain(&temp_notifier_list,SONY_WARM_TEMP,NULL);
		}else if(temp >450 && temp <= 550){
			blocking_notifier_call_chain(&temp_notifier_list,SONY_HOT_TEMP,NULL);
		}else if(temp >550 && temp <= 600){
			blocking_notifier_call_chain(&temp_notifier_list,SONY_ALARM_TEMP,NULL);
		}
	}else{//atl battery
		if(temp > -200 && temp < 0){
			blocking_notifier_call_chain(&temp_notifier_list,ATL_FREEZE_TEMP,NULL);
		}else if(temp >= 0 && temp < 100){
			blocking_notifier_call_chain(&temp_notifier_list,ATL_COLD_TEMP,NULL);
		}else if(temp >= 100 && temp < 250){
			blocking_notifier_call_chain(&temp_notifier_list,ATL_ROOM_TEMP,NULL);
		}else if(temp >=250 && temp <= 450){
			blocking_notifier_call_chain(&temp_notifier_list,ATL_WARM_TEMP,NULL);
		}else if(temp >450 && temp <= 550){
			blocking_notifier_call_chain(&temp_notifier_list,ATL_HOT_TEMP,NULL);
		}else if(temp >550 && temp <= 600){
			blocking_notifier_call_chain(&temp_notifier_list,ATL_ALARM_TEMP,NULL);
		}
	}
#if 0
	if(board_temp >= 52){
		pr_info(TAG"%s board temp is higher than 52C\n",__func__);
		blocking_notifier_call_chain(&temp_notifier_list,BOARD_HOT_TEMP,NULL);
	}else if(board_temp <=42){
		blocking_notifier_call_chain(&temp_notifier_list,BOARD_COLD_TEMP,NULL);
	}
#endif
	return temp;
}

int charger_get_temperature(void)
{
	if(g_di){
		return bq27x00_battery_read_temperature(g_di);
	}else{
		return 300;
	}
}
/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_cyct(struct bq27x00_device_info *di)
{
	int cyct;

	cyct = bq27xxx_read(di, BQ27XXX_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");

	return cyct;
}

static int overtemperature(struct bq27x00_device_info *di, u16 flags)
{
	if (di->chip == BQ27520)
		return flags & (BQ27XXX_FLAG_OTC | BQ27XXX_FLAG_OTD);
	else
		return flags & BQ27XXX_FLAG_OTC;
}

/*
 * Read flag register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_read_health(struct bq27x00_device_info *di)
{
	s16 tval;

	tval = bq27xxx_read(di, BQ27XXX_REG_FLAGS, false);
	if (tval < 0) {
		dev_err(di->dev, "error reading flag register:%d\n", tval);
		return tval;
	}

	if ((di->chip == BQ27200)) {
		if (tval & BQ27200_FLAG_EDV1)
			tval = POWER_SUPPLY_HEALTH_DEAD;
		else
			tval = POWER_SUPPLY_HEALTH_GOOD;
		return tval;
	} else {
		if (tval & BQ27XXX_FLAG_SOCF)
			tval = POWER_SUPPLY_HEALTH_DEAD;
		else if (overtemperature(di, tval))
			tval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			tval = POWER_SUPPLY_HEALTH_GOOD;
		return tval;
	}

	return -1;
}

static void bq27x00_update(struct bq27x00_device_info *di)
{
	struct bq27x00_reg_cache cache = di->cache;
	int temp_soc,temp_health,temp_temp,temp_count;

	temp_soc = bq27x00_battery_read_soc(di);
	if(temp_soc >=0 && temp_soc <=100){
		cache.capacity = temp_soc;
	}
	pr_info(TAG"%s soc = %d\n",__func__,cache.capacity);
	temp_health = bq27x00_battery_read_health(di);
	if(temp_health >=0){
		cache.health = temp_health;
	}
	temp_temp = bq27x00_battery_read_temperature(di);
	if(temp_temp >-500 && temp_temp < 1000){
		cache.temperature = temp_temp;
	}
	temp_count = bq27x00_battery_read_cyct(di);
	if(temp_count >=0){
		cache.cycle_count = temp_count;
	}
	if (memcmp(&di->cache, &cache, sizeof(cache)) != 0) {
		di->cache = cache;
		power_supply_changed(&di->bat);
	}
}
void rom_mode_gauge_dm_init(struct bq27x00_device_info *di)
{
	u16 t_rise=12;
	u16 t_time_constant=355;
	u16 t_predict_time=800;
	u16 final_voltage =0;
	u8 op_config_d = 0x43;
	int ret =1;
	pr_info( "%s:\n", __func__);
	//pr_info(TAG"%s control status = %x\n",__func__,control_cmd_read(di,0));

	//if(is_gauge_modified(di)) return; // stop here if new battery is found
	if(charger_get_manufacture()==0){//sony battery
		t_rise = 34;
		t_time_constant = 2229;
	}else if(charger_get_manufacture()==1){//atl battery
		t_rise = 12;
		t_time_constant = 355;
	}else{
		return;
	}
	enter_cfg_update_mode(di);
	msleep(200);
	ret &= update_dm_block(di, 82,12,(t_rise&0xff00)>>8);
	ret &= update_dm_block(di, 82,13,t_rise&0x00ff);

	ret &= update_dm_block(di, 82,14,(t_time_constant&0xff00)>>8);
	ret &= update_dm_block(di, 82,15,t_time_constant&0x00ff);

	ret &= update_dm_block(di, 80,60,(t_predict_time&0xff00)>>8);
	ret &= update_dm_block(di, 80,61,t_predict_time&0x00ff);

	ret &= update_dm_block(di, 49,4,(final_voltage&0xff00)>>8);
	ret &= update_dm_block(di, 49,5,final_voltage&0x00ff);
	//byte operations
	ret &= update_dm_block(di, 64,6,op_config_d);
	ret &= update_dm_block(di, 57,28,1);
	exit_cfg_update_mode(di,ret);
	msleep(200);
	is_gauge_modified(di);
}
static void bq27x00_battery_isr(struct work_struct *work)
{
	struct bq27x00_device_info *di =
		container_of(work, struct bq27x00_device_info, work.work);
	pr_info(TAG"%s\n",__func__);

	bq27x00_update(di);
}
static void bq27x00_battery_poll(struct work_struct *work)
{
	struct bq27x00_device_info *di =
		container_of(work, struct bq27x00_device_info, poll_work.work);
	pr_info(TAG"%s\n",__func__);

	bq27x00_update(di);
	if(di->cache.capacity < 2){
		poll_interval = 2;
	}else if(di->cache.capacity<10){
		poll_interval = 10;
	}else{
		poll_interval = 60;
	}

	schedule_delayed_work(&di->poll_work, poll_interval*HZ);
}

/*
 * Return the battery average current in µA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27x00_battery_current(struct bq27x00_device_info *di,
		union power_supply_propval *val)
{
	int curr;

	curr = bq27xxx_read(di, BQ27XXX_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}

	val->intval = (int)((s16)curr) * 1000;

	return 0;
}

static int bq27x00_battery_status(struct bq27x00_device_info *di,
		union power_supply_propval *val)
{
	int status;
	struct power_supply *charger;
	union power_supply_propval charger_val;

	charger = power_supply_get_by_name("usb");
	if(charger){
		charger->get_property(charger, POWER_SUPPLY_PROP_CHARGE_TYPE,&charger_val);
		if(di->cache.capacity ==100){
			status = POWER_SUPPLY_STATUS_FULL;
		}else if(charger_val.intval == POWER_SUPPLY_CHARGE_TYPE_NONE){
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}else{
			status = POWER_SUPPLY_STATUS_CHARGING;
		}
	}else{
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	val->intval = status;

	return 0;
}

/*
 * Return the battery Voltage in millivolts
 * Or < 0 if something fails.
 */
static int bq27x00_battery_voltage(struct bq27x00_device_info *di,
		union power_supply_propval *val)
{
	int volt;

	volt = bq27xxx_read(di, BQ27XXX_REG_VOLT, false);
	if (volt < 0) {
		dev_err(di->dev, "error reading voltage\n");
		return volt;
	}

	val->intval = volt * 1000;

	return 0;
}

static int bq27x00_simple_value(int value,
		union power_supply_propval *val)
{
	if (value < 0)
		return value;

	if (fake_cap_enable) {
		value = max(1, value);
	}

	val->intval = value;

	return 0;
}

#define to_bq27x00_device_info(x) container_of((x), \
		struct bq27x00_device_info, bat);

static int bq27x00_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);
	if(di->is_updating)
		return -EINVAL;
	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = bq27x00_battery_status(di, val);
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			ret = bq27x00_battery_voltage(di, val);
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = di->present;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			ret = bq27x00_battery_current(di, val);
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			ret = bq27x00_simple_value(di->cache.capacity, val);
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = di->cache.temperature ;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;
		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			ret = bq27x00_simple_value(di->cache.cycle_count, val);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			ret = bq27x00_simple_value(di->cache.health, val);
			break;
		default:
			return -EINVAL;
	}

	return ret;
}
static int bq27x00_fake_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = 4200000;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval =  0;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval =  21;
			break;
		case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
			val->intval =  POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = 160;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;
		case POWER_SUPPLY_PROP_CHARGE_NOW:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_CYCLE_COUNT:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_ENERGY_NOW:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_POWER_AVG:
			val->intval = 10;
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
			break;
		default:
			return -EINVAL;
	}

	return ret;
}


static void bq27x00_external_power_changed(struct power_supply *psy)
{
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

static void __init set_properties_array(struct bq27x00_device_info *di,
		enum power_supply_property *props, int num_props)
{
	int tot_sz = num_props * sizeof(enum power_supply_property);

	di->bat.properties = devm_kzalloc(di->dev, tot_sz, GFP_KERNEL);

	if (di->bat.properties) {
		memcpy(di->bat.properties, props, tot_sz);
		di->bat.num_properties = num_props;
	} else {
		di->bat.num_properties = 0;
	}
}

static int __init bq27x00_powersupply_init(struct bq27x00_device_info *di)
{
	int ret;

	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	if (di->chip == BQ2753X) {
		set_properties_array(di, bq2753x_battery_props,
				ARRAY_SIZE(bq2753x_battery_props));
	} else {
		set_properties_array(di, bq2753x_battery_props,
				ARRAY_SIZE(bq2753x_battery_props));
	}
	di->bat.get_property = bq27x00_battery_get_property;
	di->bat.external_power_changed = bq27x00_external_power_changed;

	INIT_DELAYED_WORK(&di->work, bq27x00_battery_isr);
	INIT_DELAYED_WORK(&di->poll_work, bq27x00_battery_poll);
	mutex_init(&di->lock);

	ret = power_supply_register(di->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	bq27x00_update(di);

	return 0;
}
static int __init bq27x00_fake_init(struct bq27x00_device_info *di)
{
	int ret;

	di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
	set_properties_array(di, bq2753x_battery_props,	ARRAY_SIZE(bq2753x_battery_props));
	di->bat.get_property = bq27x00_fake_get_property;

	ret = power_supply_register(di->dev, &di->bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		return ret;
	}

	dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);

	return 0;
}


static void bq27x00_powersupply_unregister(struct bq27x00_device_info *di)
{
	/*
	 * power_supply_unregister call bq27x00_battery_get_property which
	 * call bq27x00_battery_poll.
	 * Make sure that bq27x00_battery_poll will not call
	 * schedule_delayed_work again after unregister (which cause OOPS).
	 */
	poll_interval = 0;

	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(&di->bat);

	mutex_destroy(&di->lock);
}


/* i2c specific code */

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_MUTEX(battery_mutex);

static int bq27xxx_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	int ret;
	int retry_count = 5;

	wake_lock(&di->battery_i2c_wake);
	do{
		ret = single?i2c_smbus_read_byte_data(client,reg):i2c_smbus_read_word_data(client,reg);
		if(ret < 0){
			msleep(50);
			pr_err(TAG"%s read i2c fail\n",__func__);
		}
	}while(ret<0 && retry_count-->0);
	wake_unlock(&di->battery_i2c_wake);

	return ret;
}

static int bq27xxx_write_i2c(struct bq27x00_device_info *di, u8 reg, int value, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	int ret;
	int retry_count = 5;

	wake_lock(&di->battery_i2c_wake);
	do{
		ret = single?i2c_smbus_write_byte_data(client,reg,value):i2c_smbus_write_word_data(client,reg,value);
		if(ret < 0){
			msleep(50);
			pr_err(TAG"%s write i2c fail\n",__func__);
		}
	}while(ret<0 && retry_count-->0);
	wake_unlock(&di->battery_i2c_wake);

	return ret;
}

static int bq27xxx_read_i2c_blk(struct bq27x00_device_info *di, u8 reg,
		u8 *data, u8 len)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	int ret;
	int retry_count = 5;

	wake_lock(&di->battery_i2c_wake);
	do{
		ret = i2c_smbus_read_i2c_block_data( client,reg,len,data);
		if(ret < 0){
			msleep(50);
			pr_err(TAG"%s read i2c blk fail\n",__func__);
		}
	}while(ret<0 && retry_count-->0);
	wake_unlock(&di->battery_i2c_wake);

	return ret;
}

static int bq27xxx_write_i2c_blk(struct bq27x00_device_info *di, u8 reg,
		u8 *data, u8 sz)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	int ret;
	int retry_count = 5;

	wake_lock(&di->battery_i2c_wake);
	do{
		ret = i2c_smbus_write_i2c_block_data( client,reg,sz,data);
		if(ret < 0){
			msleep(50);
			pr_err(TAG"%s write i2c blk fail\n",__func__);
		}
	}while(ret<0 && retry_count-->0);
	wake_unlock(&di->battery_i2c_wake);

	return 0;
}

static int bq27x00_battery_reset(struct bq27x00_device_info *di)
{
	dev_info(di->dev, "Gas Gauge Reset\n");

	bq27xxx_write(di, BQ27XXX_REG_CTRL, RESET_SUBCMD, false);

	msleep(10);

	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static int bq27x00_battery_read_fw_version(struct bq27x00_device_info *di)
{
	bq27xxx_write(di, BQ27XXX_REG_CTRL, FW_VER_SUBCMD, false);

	msleep(10);

	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static int bq27x00_battery_read_device_type(struct bq27x00_device_info *di)
{
	bq27xxx_write(di, BQ27XXX_REG_CTRL, DEV_TYPE_SUBCMD, false);

	msleep(10);

	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static int bq27x00_battery_read_dataflash_version(struct bq27x00_device_info *di)
{
	bq27xxx_write(di, BQ27XXX_REG_CTRL, DF_VER_SUBCMD, false);

	msleep(10);

	return bq27xxx_read(di, BQ27XXX_REG_CTRL, false);
}

static ssize_t show_firmware_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	int ver;

	ver = bq27x00_battery_read_fw_version(di);

	return sprintf(buf, "%d\n", ver);
}

static ssize_t show_dataflash_version(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	int ver;

	ver = bq27x00_battery_read_dataflash_version(di);

	return sprintf(buf, "%d\n", ver);
}

static ssize_t show_device_type(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	int dev_type;

	dev_type = bq27x00_battery_read_device_type(di);

	return sprintf(buf, "%d\n", dev_type);
}

static ssize_t show_reset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);

	bq27x00_battery_reset(di);

	return sprintf(buf, "okay\n");
}
static ssize_t fake_temp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int fake_en=0;
	struct bq27x00_device_info *di = dev_get_drvdata(dev);

	sscanf(buf,"%d",&fake_en);
	if(fake_en == 1022){
		di->fake_temp_en = true;
		pr_info("enable fake temperature\n");
	}else{
		di->fake_temp_en = false;
		pr_info("disable fake temperature\n");
	}
	return size;
}
static ssize_t fake_temp_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);

	sprintf(buf,"%d\n",di->fake_temp_en);
	return 2;
}

static ssize_t fake_cap_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int enable = 0;

	if (!sscanf(buf, "%d", &enable))
		return -EINVAL;

	fake_cap_enable = enable ? true : false;
	return size;
}

static ssize_t fake_cap_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", fake_cap_enable);
}

static ssize_t temp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);

	sscanf(buf,"%d",&di->fake_temp);
	if(di->fake_temp_en ){
		bq27x00_update(di);
	}else{
		pr_info("fake temperature is not enabled, turn it first please\n");
	}
	return size;
}
static ssize_t temp_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);

	sprintf(buf,"%d",di->fake_temp);
	return strlen(buf);
}
static ssize_t manufacture_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);
	int pack;
	int cell;
	static char pack_str[10];
	static char cell_str[10];

	pack = bq27x00_battery_read_pack_version(di);
	cell = bq27x00_battery_read_cell_version(di);
	switch(pack){
		case 1:
			strncpy(pack_str,"NVT",10);
			break;
		case 2:
			strncpy(pack_str,"SCUD",10);
			break;
		case 3:
			strncpy(pack_str,"SUNWODA",10);
			break;
		default:
			strncpy(pack_str,"unknown",10);
			break;
	}
	if(charger_get_manufacture()==0){//sony battery
		strncpy(cell_str,"SONY",10);
	}else if(charger_get_manufacture()==1){//atl battery
		strncpy(cell_str,"ATL",10);
	}else{
		strncpy(cell_str,"unknown",10);
	}

	sprintf(buf,"%s-%s\n",pack_str,cell_str);
	return strlen(buf);
}

static ssize_t attr_dump_reg(struct device *dev,
		struct device_attribute *attr,char *buf)
{
	struct bq27x00_device_info *di = dev_get_drvdata(dev);

	int i=0,offset=0;
	int val;
	static char buf_bak[20];
	for(i=0x0;i<=0x27;i+=2){
		val = bq27xxx_read_i2c(di, i, false);
		sprintf(buf_bak,"[0x%2x]=0x%04x\n",i,val);
		strcat(buf,buf_bak);
		offset +=strlen(buf_bak);
	}
	for(i=0x6c;i<=0x75;i+=2){
		val = bq27xxx_read_i2c(di, i, false);
		sprintf(buf_bak,"[0x%2x]=0x%04x\n",i,val);
		strcat(buf,buf_bak);
		offset +=strlen(buf_bak);
	}

	return strlen(buf);
}


static struct device_attribute attributes[] = {
	__ATTR(fw_version, S_IRUGO, show_firmware_version, NULL),
	__ATTR(df_version, S_IRUGO, show_dataflash_version, NULL),
	__ATTR(device_type, S_IRUGO, show_device_type, NULL),
	__ATTR(reset, S_IRUGO, show_reset, NULL),
	__ATTR(fake_temp_enable, 0660, fake_temp_show, fake_temp_store),
	__ATTR(fake_temp, 0660, temp_show, temp_store),
	__ATTR(fake_cap_enable, 0666, fake_cap_enable_show, fake_cap_enable_store),
	__ATTR(manufacture, 0444, manufacture_show, NULL),
	__ATTR(dump_reg, 0444, attr_dump_reg,NULL ),
};
static int create_sysfs_files(struct device *dev)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(attributes); i++)
		if(device_create_file(dev, attributes + i))
			goto err;
	return 0;
err:
	for(; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	pr_err(TAG"unable to create sysfs interface\n");
	return -1;
}

static irqreturn_t battery_interrupt(int irq, void *data)
{
	struct bq27x00_device_info *di = data;
	pr_info(TAG"%s battery irq triggered\n",__func__);

	schedule_delayed_work(&di->work, 0);
	return IRQ_HANDLED;
}

static int __init battery_parse_dt(struct bq27x00_device_info *di)
{
	struct device *dev = di->dev;
	struct device_node *np = dev->of_node;
	int gpio;
	int ret;

	if(!np){
		pr_info(TAG"%s(),  parse device tree error", __func__);
		return -EINVAL;
	}
	gpio = of_get_named_gpio(np, "int-gauge", 0);
	if(gpio < 0){
		return -EINVAL;
	}
	ret = gpio_request(gpio, "int-gauge");
	if(ret){
		pr_err(TAG"int-gauge request error\n");
		return ret;
	}
	di->irq_pin = gpio_to_irq(gpio);

	return 0;
}
static int bq27532_rom_update_thread(void *data)
{
	struct bq27x00_device_info *di=data;
	int ret;

	di->is_updating = true;
#ifndef CONFIG_RECOVERY_KERNEL
	rom_mode_gauge_dm_init(di);
#endif
	di->is_updating = false;
	ret = request_threaded_irq(di->irq_pin,NULL,battery_interrupt,IRQF_TRIGGER_FALLING| IRQF_ONESHOT,"battery",di);
	enable_irq_wake(di->irq_pin);
	schedule_delayed_work(&di->poll_work,0);
	return 0;
}

static  dev_t  battery_device_dev_t ;

static int __init bq27x00_battery_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	char *name;
	struct bq27x00_device_info *di;
	int num = 0;
	int retval = 0;
	u8 *regs;

	name = kasprintf(GFP_KERNEL, "%s-%d", id->name, num);
	if (!name) {
		dev_err(&client->dev, "failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}
	wake_lock_init(&di->battery_i2c_wake,WAKE_LOCK_SUSPEND,"battery_i2c_wake");

	di->id = num;
	di->dev = &client->dev;
	di->chip = id->driver_data;
	di->bat.name = name;
	di->bus.read = &bq27xxx_read_i2c;
	di->bus.write = &bq27xxx_write_i2c;
	di->bus.blk_read = bq27xxx_read_i2c_blk;
	di->bus.blk_write = bq27xxx_write_i2c_blk;
	di->dm_regs = NULL;
	di->dm_regs_count = 0;

	battery_parse_dt(di);

	if (di->chip == BQ2753X){
		regs = bq2753x_regs;
	} else {
		dev_err(&client->dev,
				"Unexpected gas gague: %d\n", di->chip);
		regs = bq2753x_regs;
	}

	memcpy(di->regs, regs, NUM_REGS);

	di->present = true;
	if(bq27xxx_read(di, BQ27XXX_REG_CTRL, false) < 0){
		pr_info("battery doesn't exist, maybe it's a development board!!\n");
		bq27x00_fake_init(di);
		di->present = false;
		goto batt_failed_2;
	}
	g_di = di;
	di->fw_ver = bq27x00_battery_read_fw_version(di);
	dev_info(&client->dev, "Gas Guage fw version is 0x%04x\n", di->fw_ver);

	retval = bq27x00_powersupply_init(di);
	if (retval)
		goto batt_failed_3;

	i2c_set_clientdata(client, di);
	di->battery_class = class_create(THIS_MODULE,"battery_class");
	if(IS_ERR(di->battery_class)){
		retval = PTR_ERR(di->battery_class);
		pr_err(TAG"%s battery class create fail\n", __func__);
		goto err_class_create;
	}

	alloc_chrdev_region(&battery_device_dev_t,0,1,"battery_device");
	di->battery_device = device_create(di->battery_class,
			NULL,battery_device_dev_t,di,"battery_device");
	if(IS_ERR(di->battery_device)){
		retval = PTR_ERR(di->battery_device);
		pr_err(TAG"%s charger device create fail\n", __func__);
		goto err_device_create;
	}

	retval = create_sysfs_files(di->battery_device);
	if(retval < 0){
		pr_err(TAG"%s sysfiles create fail\n", __func__);
		goto err_create_sys;
	}
	if(di->present){
		kthread_run(bq27532_rom_update_thread,di,"BQ27532 rom update thread");
	}
	return 0;
err_create_sys:
	device_destroy(di->battery_class,battery_device_dev_t);
err_device_create:
	class_destroy(di->battery_class);
err_class_create:

batt_failed_3:
	kfree(di);
batt_failed_2:
	kfree(name);
batt_failed_1:
	mutex_lock(&battery_mutex);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27x00_battery_remove(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);

	bq27x00_powersupply_unregister(di);

	kfree(di->bat.name);

	mutex_lock(&battery_mutex);
	mutex_unlock(&battery_mutex);

	kfree(di);

	return 0;
}
static void bq27532_shutdown(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);
	di->is_already_shutdown = true;
	cancel_delayed_work_sync(&di->poll_work);
}

static const struct i2c_device_id bq27x00_id[] = {
	{ "bq2753x", BQ2753X },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq27x00_id);

static struct i2c_driver __refdata bq27x00_battery_driver = {
	.driver = {
		.name = "bq27x00-battery",
	},
	.probe = bq27x00_battery_probe,
	.remove = bq27x00_battery_remove,
	.id_table = bq27x00_id,
	.shutdown = bq27532_shutdown,
};

static inline int __init bq27x00_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq27x00_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27x00 i2c driver\n");

	return ret;
}

static inline void __exit bq27x00_battery_i2c_exit(void)
{
	i2c_del_driver(&bq27x00_battery_driver);
}

static int __init bq27x00_battery_init(void)
{
	int ret;

	ret = bq27x00_battery_i2c_init();
	return ret;
}
module_init(bq27x00_battery_init);

static void __exit bq27x00_battery_exit(void)
{
	bq27x00_battery_i2c_exit();
}
module_exit(bq27x00_battery_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
MODULE_LICENSE("GPL");
