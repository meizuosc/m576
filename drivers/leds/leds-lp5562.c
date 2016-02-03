/*
 * LP5562 LED driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * Author: Milo(Woogyom) Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_data/leds-lp55xx.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>


#include "leds-lp55xx-common.h"

#define LP5562_PROGRAM_LENGTH		32
#define LP5562_MAX_LEDS			4

/* ENABLE Register 00h */
#define LP5562_REG_ENABLE		0x00
#define LP5562_EXEC_ENG1_M		0x30
#define LP5562_EXEC_ENG2_M		0x0C
#define LP5562_EXEC_ENG3_M		0x03
#define LP5562_EXEC_M			0x3F
#define LP5562_MASTER_ENABLE		0x40	/* Chip master enable */
#define LP5562_LOGARITHMIC_PWM		0x80	/* Logarithmic PWM adjustment */
#define LP5562_EXEC_RUN			0x2A
#define LP5562_ENABLE_DEFAULT	\
	(LP5562_MASTER_ENABLE | LP5562_LOGARITHMIC_PWM)
#define LP5562_ENABLE_RUN_PROGRAM	\
	(LP5562_ENABLE_DEFAULT | LP5562_EXEC_RUN)

/* OPMODE Register 01h */
#define LP5562_REG_OP_MODE		0x01
#define LP5562_MODE_ENG1_M		0x30
#define LP5562_MODE_ENG2_M		0x0C
#define LP5562_MODE_ENG3_M		0x03
#define LP5562_LOAD_ENG1		0x10
#define LP5562_LOAD_ENG2		0x04
#define LP5562_LOAD_ENG3		0x01
#define LP5562_RUN_ENG1			0x20
#define LP5562_RUN_ENG2			0x08
#define LP5562_RUN_ENG3			0x02
#define LP5562_ENG1_IS_LOADING(mode)	\
	((mode & LP5562_MODE_ENG1_M) == LP5562_LOAD_ENG1)
#define LP5562_ENG2_IS_LOADING(mode)	\
	((mode & LP5562_MODE_ENG2_M) == LP5562_LOAD_ENG2)
#define LP5562_ENG3_IS_LOADING(mode)	\
	((mode & LP5562_MODE_ENG3_M) == LP5562_LOAD_ENG3)

/* BRIGHTNESS Registers */
#define LP5562_REG_R_PWM		0x04
#define LP5562_REG_G_PWM		0x03
#define LP5562_REG_B_PWM		0x02
#define LP5562_REG_W_PWM		0x0E

/* CURRENT Registers */
#define LP5562_REG_R_CURRENT		0x07
#define LP5562_REG_G_CURRENT		0x06
#define LP5562_REG_B_CURRENT		0x05
#define LP5562_REG_W_CURRENT		0x0F

/* CONFIG Register 08h */
#define LP5562_REG_CONFIG		0x08
#define LP5562_PWM_HF			0x40
#define LP5562_PWRSAVE_EN		0x20
#define LP5562_CLK_INT			0x01	/* Internal clock */
#define LP5562_DEFAULT_CFG		(LP5562_PWM_HF | LP5562_PWRSAVE_EN)

/* RESET Register 0Dh */
#define LP5562_REG_RESET		0x0D
#define LP5562_RESET			0xFF

/* PROGRAM ENGINE Registers */
#define LP5562_REG_PROG_MEM_ENG1	0x10
#define LP5562_REG_PROG_MEM_ENG2	0x30
#define LP5562_REG_PROG_MEM_ENG3	0x50

/* LEDMAP Register 70h */
#define LP5562_REG_ENG_SEL		0x70
#define LP5562_ENG_SEL_PWM		0
#define LP5562_ENG_FOR_RGB_M		0x3F
#define LP5562_ENG_SEL_RGB		0x1B	/* R:ENG1, G:ENG2, B:ENG3 */
#define LP5562_ENG_FOR_W_M		0xC0
#define LP5562_ENG1_FOR_W		0x40	/* W:ENG1 */
#define LP5562_ENG2_FOR_W		0x80	/* W:ENG2 */
#define LP5562_ENG3_FOR_W		0xC0	/* W:ENG3 */

/* Program Commands */
#define LP5562_CMD_DISABLE		0x00
#define LP5562_CMD_LOAD			0x15
#define LP5562_CMD_RUN			0x2A
#define LP5562_CMD_DIRECT		0x3F
#define LP5562_PATTERN_OFF		0


#define	MODE_CURRENT			0x01//256
#define	MODE_SLOPE				0x02//512
#define	MODE_TBLINK				0x04//1024
#define MODE_TEST				0x03//768 -- //1280

#define	GET_MODE(x)	((x>>8)&0x0F)

static struct lp55xx_led_config lp5562_led_config[] = {
	/*{
		.name 		= "R",
		.chan_nr	= 0,
		.led_current	= 20,
		.max_current	= 40,
	},
	{
		.name 		= "G",
		.chan_nr	= 1,
		.led_current	= 20,
		.max_current	= 40,
	},
	{
		.name 		= "B",
		.chan_nr	= 2,
		.led_current	= 20,
		.max_current	= 40,
	},*/
	{
		.name 		= "m86_led",
		.chan_nr	= 0,
		.led_current	= 80,
		.max_current	= 80,
	},
};
/*
prescale: bit[14]
1: divides master clock by 512 = 64hz, 15.6 ms cycle time
0: divides master clock by 16 = 2048hz, 0.49 ms cycle time

step time: one ramp increment done in step time * clock after prescale

sign: bit[7]
0: increase pwm output
1: decrease pwm output

increment: the number of steps is increment + 1.
*/


/* mode_1: breath mode */
/*in this mode:      0--255   (6.37 * 255  = 1624.35ms)    
                               255      (13 * 15.6 = 202.8ms)   
                           255--0     (1624.35ms)
                                0         (63 * 15.6 = 982.8)*/
static const u8 mode_1[] = { 0x0d, 0x7F, 0x0d, 0x7F, 0x4d, 0x00, 0x0d, 0xFF, 0x0d, 0xFF, 0x7F, 0x00};

/* mode_2: blinking mode */
/* in this mode:     0--255   (0.98 * 255 = 249.9ms)
                                0        (24 * 15.6 = 374.4ms)
                            255--0   (249.9ms)
                                0        (982.8ms)
*/
 static const u8 mode_2[] = { 0x02, 0x7F, 0x02, 0x7F, 0x40, 0x00,0x58, 0x00, 0x40, 0xFF, 0x02, 0xFF, 0x02, 0xFF, 0x7F, 0x00};


/* mode_3:test */
static const u8 mode_3[] = { 0x42, 0x7F, 0x42, 0x7F, 0x42, 0xFF, 0x42, 0xFF, 0x7F, 0x00};

struct lp55xx_predef_pattern lp5562_led_patterns[] = {
	{
		.r = mode_1,
		.size_r = ARRAY_SIZE(mode_1),
	},
	{
		.r = mode_2,
		.size_r = ARRAY_SIZE(mode_2),
	},
	{
		.r = mode_3,
		.size_r = ARRAY_SIZE(mode_3),
	},
};

static struct lp55xx_platform_data lp5562_platform_data = {
        .led_config     = lp5562_led_config,
        .num_channels   = ARRAY_SIZE(lp5562_led_config),
        //.setup_resources   = lp5562_setup,
        //.release_resources = lp5562_release,
        //.enable            = lp5562_enable,
        .patterns		= lp5562_led_patterns,
        .num_patterns	= ARRAY_SIZE(lp5562_led_patterns),
};

static int lp5562_run_predef_led_pattern(struct lp55xx_chip *chip, int mode);

static inline void lp5562_wait_opmode_done(void)
{
	/* operation mode change needs to be longer than 153 us */
	usleep_range(200, 300);
}

static inline void lp5562_wait_enable_done(void)
{
	/* it takes more 488 us to update ENABLE register */
	usleep_range(500, 600);
}

static void lp5562_set_led_current(struct lp55xx_led *led, u8 led_current)
{
	u8 addr[] = {
		LP5562_REG_R_CURRENT,
		LP5562_REG_G_CURRENT,
		LP5562_REG_B_CURRENT,
		LP5562_REG_W_CURRENT,
	};

	led->led_current = led_current;
	lp55xx_write(led->chip, addr[led->chan_nr], led_current);
}

static void lp5562_set_led_pwm(struct lp55xx_led *led, u8 led_current)
{
	//u8 enable;
	u8 cfg = LP5562_DEFAULT_CFG;
	struct lp55xx_chip *chip = led->chip;

	gpio_set_value(chip->pdata->en_gpio, 1);
	usleep_range(1000, 1100);

	//lp55xx_read(chip, LP5562_REG_ENABLE, &enable);
	//enable |= LP5562_MASTER_ENABLE;
	lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_DEFAULT);
	usleep_range(1000, 1100);

	if (!lp55xx_is_extclk_used(chip))
		cfg |= LP5562_CLK_INT;

	lp55xx_write(chip, LP5562_REG_CONFIG, cfg);

	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);
	
	
	//lp55xx_write(led->chip, LP5562_REG_W_CURRENT, 30);
	lp55xx_write(led->chip, LP5562_REG_W_PWM, led_current);

	if(0 == led_current){
		printk("%s: enter power save mode.\n", __func__);
		lp55xx_write(chip, LP5562_REG_ENABLE, 0x00);
		usleep_range(1000, 1100);
		lp55xx_write(chip, LP5562_REG_CONFIG, LP5562_DEFAULT_CFG);
		gpio_set_value(chip->pdata->en_gpio, 0);
	}
}


static void lp5562_load_engine(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 mask[] = {
		[LP55XX_ENGINE_1] = LP5562_MODE_ENG1_M,
		[LP55XX_ENGINE_2] = LP5562_MODE_ENG2_M,
		[LP55XX_ENGINE_3] = LP5562_MODE_ENG3_M,
	};

	u8 val[] = {
		[LP55XX_ENGINE_1] = LP5562_LOAD_ENG1,
		[LP55XX_ENGINE_2] = LP5562_LOAD_ENG2,
		[LP55XX_ENGINE_3] = LP5562_LOAD_ENG3,
	};

	lp55xx_update_bits(chip, LP5562_REG_OP_MODE, mask[idx], val[idx]);

	lp5562_wait_opmode_done();
}

static void lp5562_stop_engine(struct lp55xx_chip *chip)
{
	lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DISABLE);
	lp5562_wait_opmode_done();
}

static void lp5562_run_engine(struct lp55xx_chip *chip, bool start)
{
	int ret;
	u8 mode;
	u8 exec;

	/* stop engine */
	if (!start) {
		lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_DEFAULT);
		lp5562_wait_enable_done();
		lp5562_stop_engine(chip);
		//lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);
		lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DIRECT);
		lp5562_wait_opmode_done();
		lp55xx_write(chip, LP5562_REG_CONFIG, LP5562_DEFAULT_CFG);
		gpio_set_value(chip->pdata->en_gpio, 0);
		printk("%s: enter power save mode.\n", __func__);
		return;
	}

	/*
	 * To run the engine,
	 * operation mode and enable register should updated at the same time
	 */

	ret = lp55xx_read(chip, LP5562_REG_OP_MODE, &mode);
	if (ret)
		return;

	ret = lp55xx_read(chip, LP5562_REG_ENABLE, &exec);
	if (ret)
		return;

	/* change operation mode to RUN only when each engine is loading */
	if (LP5562_ENG1_IS_LOADING(mode)) {
		mode = (mode & ~LP5562_MODE_ENG1_M) | LP5562_RUN_ENG1;
		exec = (exec & ~LP5562_EXEC_ENG1_M) | LP5562_RUN_ENG1;
	}

	if (LP5562_ENG2_IS_LOADING(mode)) {
		mode = (mode & ~LP5562_MODE_ENG2_M) | LP5562_RUN_ENG2;
		exec = (exec & ~LP5562_EXEC_ENG2_M) | LP5562_RUN_ENG2;
	}

	if (LP5562_ENG3_IS_LOADING(mode)) {
		mode = (mode & ~LP5562_MODE_ENG3_M) | LP5562_RUN_ENG3;
		exec = (exec & ~LP5562_EXEC_ENG3_M) | LP5562_RUN_ENG3;
	}

	lp55xx_write(chip, LP5562_REG_OP_MODE, mode);
	lp5562_wait_opmode_done();

	lp55xx_update_bits(chip, LP5562_REG_ENABLE, LP5562_EXEC_M, exec);
	lp5562_wait_enable_done();
}

static int lp5562_update_firmware(struct lp55xx_chip *chip,
					const u8 *data, size_t size)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 pattern[LP5562_PROGRAM_LENGTH] = {0};
	u8 addr[] = {
		[LP55XX_ENGINE_1] = LP5562_REG_PROG_MEM_ENG1,
		[LP55XX_ENGINE_2] = LP5562_REG_PROG_MEM_ENG2,
		[LP55XX_ENGINE_3] = LP5562_REG_PROG_MEM_ENG3,
	};
	unsigned cmd;
	char c[3];
	int program_size;
	int nrchars;
	int offset = 0;
	int ret;
	int i;

	/* clear program memory before updating */
	for (i = 0; i < LP5562_PROGRAM_LENGTH; i++)
		lp55xx_write(chip, addr[idx] + i, 0);

	i = 0;
	while ((offset < size - 1) && (i < LP5562_PROGRAM_LENGTH)) {
		/* separate sscanfs because length is working only for %s */
		ret = sscanf(data + offset, "%2s%n ", c, &nrchars);
		if (ret != 1)
			goto err;

		ret = sscanf(c, "%2x", &cmd);
		if (ret != 1)
			goto err;

		pattern[i] = (u8)cmd;
		offset += nrchars;
		i++;
	}

	/* Each instruction is 16bit long. Check that length is even */
	if (i % 2)
		goto err;

	program_size = i;
	for (i = 0; i < program_size; i++)
		lp55xx_write(chip, addr[idx] + i, pattern[i]);

	return 0;

err:
	dev_err(&chip->cl->dev, "wrong pattern format\n");
	return -EINVAL;
}

static void lp5562_firmware_loaded(struct lp55xx_chip *chip)
{
	const struct firmware *fw = chip->fw;

	if (fw->size > LP5562_PROGRAM_LENGTH) {
		dev_err(&chip->cl->dev, "firmware data size overflow: %zu\n",
			fw->size);
		return;
	}

	/*
	 * Program momery sequence
	 *  1) set engine mode to "LOAD"
	 *  2) write firmware data into program memory
	 */

	lp5562_load_engine(chip);
	lp5562_update_firmware(chip, fw->data, fw->size);
}

static int lp5562_post_init_device(struct lp55xx_chip *chip)
{
	int ret;
	u8 cfg = LP5562_DEFAULT_CFG;

	/* Set all PWMs to direct control mode */
	ret = lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DIRECT);
	if (ret)
		return ret;

	lp5562_wait_opmode_done();

	/* Update configuration for the clock setting */
	if (!lp55xx_is_extclk_used(chip))
		cfg |= LP5562_CLK_INT;

	ret = lp55xx_write(chip, LP5562_REG_CONFIG, cfg);
	if (ret)
		return ret;

	/* Initialize all channels PWM to zero -> leds off */
	lp55xx_write(chip, LP5562_REG_R_PWM, 0);
	lp55xx_write(chip, LP5562_REG_G_PWM, 0);
	lp55xx_write(chip, LP5562_REG_B_PWM, 0);
	lp55xx_write(chip, LP5562_REG_R_CURRENT, 0);
	lp55xx_write(chip, LP5562_REG_G_CURRENT, 0);
	lp55xx_write(chip, LP5562_REG_B_CURRENT, 0);
	lp55xx_write(chip, LP5562_REG_W_PWM, 0);

	lp55xx_write(chip, LP5562_REG_W_CURRENT, chip->pdata->led_config[0].led_current);
	/* Set LED map as register PWM by default */
	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);

	/*load the pattern*/
	lp55xx_write(chip, LP5562_REG_CONFIG, LP5562_DEFAULT_CFG);
	return 0;
}

static void lp5562_led_brightness_work(struct work_struct *work)
{
	struct lp55xx_led *led = container_of(work, struct lp55xx_led, brightness_work);
	struct lp55xx_chip *chip = led->chip;
	int mode;
	u8 data = (led->brightness & 0x0FF);

	mutex_lock(&chip->lock);
	if (0 == data) {
		mode = MODE_CURRENT;
	}else {
		mode = GET_MODE(led->brightness);
	}
	printk("%s :value = 0x%.4X, mode = %d, data = %d \n",__func__, led->brightness,mode, data);
	
	switch( mode )
	{
	case MODE_CURRENT:
		lp5562_set_led_pwm(led, data);
		break;
	case MODE_SLOPE:
		lp5562_run_predef_led_pattern(chip,1);
		printk("+++++++++++++++++slope mode!\n");
		break;
	case MODE_TBLINK:
		lp5562_run_predef_led_pattern(chip,2);
		printk("+++++++++++++++++tblink mode!\n");
		break;
	case MODE_TEST:
		lp5562_run_predef_led_pattern(chip,3);
		printk("+++++++++++++++++test mode!\n");
		break;
	default:
		lp5562_run_predef_led_pattern(chip, 0);
		printk(" this is default mode! mode = %d\n", mode);
		//dev_err(led_cdev->dev, "mode  %d is valite \n",mode);
		//ret = -EINVAL;			
		break;
	}
	mutex_unlock(&chip->lock);

	
}

static void lp5562_write_program_memory(struct lp55xx_chip *chip,
					u8 base, const u8 *rgb, int size)
{
	int i;

	if (!rgb || size <= 0)
		return;

	for (i = 0; i < size; i++)
		lp55xx_write(chip, base + i, *(rgb + i));

	lp55xx_write(chip, base + i, 0);
	lp55xx_write(chip, base + i + 1, 0);
}

/* check the size of program count */
static inline bool _is_pc_overflow(struct lp55xx_predef_pattern *ptn)
{
	return (ptn->size_r >= LP5562_PROGRAM_LENGTH ||
		ptn->size_g >= LP5562_PROGRAM_LENGTH ||
		ptn->size_b >= LP5562_PROGRAM_LENGTH);
}

static int lp5562_run_predef_led_pattern(struct lp55xx_chip *chip, int mode)
{
	struct lp55xx_predef_pattern *ptn;
	int i;

	if (mode == LP5562_PATTERN_OFF) {
		lp5562_run_engine(chip, false);
		return 0;
	}

	ptn = chip->pdata->patterns + (mode - 1);
	if (!ptn || _is_pc_overflow(ptn)) {
		dev_err(&chip->cl->dev, "invalid pattern data\n");
		return -EINVAL;
	}

	gpio_set_value(chip->pdata->en_gpio, 1);
	usleep_range(1000, 1100);

	lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_DEFAULT);
	usleep_range(1000, 1100);

	lp55xx_write(chip, LP5562_REG_CONFIG, LP5562_DEFAULT_CFG | LP5562_CLK_INT);

	lp5562_stop_engine(chip);

	/* Set LED map as RGB */
	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG1_FOR_W);

	/* Load engines */
	for (i = LP55XX_ENGINE_1; i <= LP55XX_ENGINE_3; i++) {
		chip->engine_idx = i;
		lp5562_load_engine(chip);
	}

	/* Clear program registers */
	lp55xx_write(chip, LP5562_REG_PROG_MEM_ENG1, 0);
	lp55xx_write(chip, LP5562_REG_PROG_MEM_ENG1 + 1, 0);
/*
	lp55xx_write(chip, LP5562_REG_PROG_MEM_ENG2, 0);
	lp55xx_write(chip, LP5562_REG_PROG_MEM_ENG2 + 1, 0);
	lp55xx_write(chip, LP5562_REG_PROG_MEM_ENG3, 0);
	lp55xx_write(chip, LP5562_REG_PROG_MEM_ENG3 + 1, 0);
*/

	/* Program engines */
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG1,
				ptn->r, ptn->size_r);
/*
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG2,
				ptn->g, ptn->size_g);
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG3,
				ptn->b, ptn->size_b);
*/
	/* Run engines */
	lp5562_run_engine(chip, true);

	return 0;
}

static ssize_t lp5562_store_pattern(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_predef_pattern *ptn = chip->pdata->patterns;
	int num_patterns = chip->pdata->num_patterns;
	unsigned long mode;
	int ret;

	ret = kstrtoul(buf, 0, &mode);
	if (ret)
		return ret;

	if (mode > num_patterns || !ptn)
		return -EINVAL;

	mutex_lock(&chip->lock);
	ret = lp5562_run_predef_led_pattern(chip, mode);
	mutex_unlock(&chip->lock);

	if (ret)
		return ret;

	return len;
}

static ssize_t lp5562_store_engine_mux(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	u8 mask;
	u8 val;

	/* LED map
	 * R ... Engine 1 (fixed)
	 * G ... Engine 2 (fixed)
	 * B ... Engine 3 (fixed)
	 * W ... Engine 1 or 2 or 3
	 */

	if (sysfs_streq(buf, "RGB")) {
		mask = LP5562_ENG_FOR_RGB_M;
		val = LP5562_ENG_SEL_RGB;
	} else if (sysfs_streq(buf, "W")) {
		enum lp55xx_engine_index idx = chip->engine_idx;

		mask = LP5562_ENG_FOR_W_M;
		switch (idx) {
		case LP55XX_ENGINE_1:
			val = LP5562_ENG1_FOR_W;
			break;
		case LP55XX_ENGINE_2:
			val = LP5562_ENG2_FOR_W;
			break;
		case LP55XX_ENGINE_3:
			val = LP5562_ENG3_FOR_W;
			break;
		default:
			return -EINVAL;
		}

	} else {
		dev_err(dev, "choose RGB or W\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	lp55xx_update_bits(chip, LP5562_REG_ENG_SEL, mask, val);
	mutex_unlock(&chip->lock);

	return len;
}
static ssize_t lp5562_reg_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;

	u8 regdata;
	int res=0;
	int count=0;
	int i;

	for(i = 0;i <16 ;i++){
		lp55xx_read(chip, 0x00+i, &regdata);
		if(res<0)
			break;
		else
			count+=sprintf(buf+count,"[%x] = (%x)\n",0x00+i,regdata);
	}
	return count;
}


static DEVICE_ATTR(led_pattern, S_IWUSR, NULL, lp5562_store_pattern);
static DEVICE_ATTR(engine_mux, S_IWUSR, NULL, lp5562_store_engine_mux);
static DEVICE_ATTR(reg, 0444, lp5562_reg_show, NULL);


static struct attribute *lp5562_attributes[] = {
	&dev_attr_led_pattern.attr,
	&dev_attr_engine_mux.attr,
	&dev_attr_reg.attr,
	NULL,
};

static const struct attribute_group lp5562_group = {
	.attrs = lp5562_attributes,
};

/* Chip specific configurations */
static struct lp55xx_device_config lp5562_cfg = {
	.max_channel  = LP5562_MAX_LEDS,
	.reset = {
		.addr = LP5562_REG_RESET,
		.val  = LP5562_RESET,
	},
	.enable = {
		.addr = LP5562_REG_ENABLE,
		.val  = LP5562_ENABLE_DEFAULT,
	},
	.post_init_device   = lp5562_post_init_device,
	.set_led_current    = lp5562_set_led_current,
	.brightness_work_fn = lp5562_led_brightness_work,
	.run_engine         = lp5562_run_engine,
	.firmware_cb        = lp5562_firmware_loaded,
	.dev_attr_group     = &lp5562_group,
};

static int lp5562_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct lp55xx_chip *chip;
	struct lp55xx_led *led;
	struct lp55xx_platform_data *pdata = &lp5562_platform_data;
	struct device_node *dp = client->dev.of_node;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	led = devm_kzalloc(&client->dev,sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;
	
	if(of_property_read_bool(dp, "led,en_gpio")) {
		pdata->en_gpio = of_get_named_gpio_flags(dp, "led,en_gpio", 0, NULL);
	}else {
		pdata->en_gpio = -1;
	}

	chip->cl = client;
	chip->pdata = pdata;
	chip->cfg = &lp5562_cfg;

	mutex_init(&chip->lock);

	i2c_set_clientdata(client, led);

	ret = lp55xx_init_device(chip);
	if (ret)
		goto err_init;

	ret = lp55xx_register_leds(led, chip);
	if (ret)
		goto err_register_leds;

	ret = lp55xx_register_sysfs(chip);
	if (ret) {
		dev_err(&client->dev, "registering sysfs failed\n");
		goto err_register_sysfs;
	}

	return 0;

err_register_sysfs:
	lp55xx_unregister_leds(led, chip);
err_register_leds:
	lp55xx_deinit_device(chip);
err_init:
	return ret;
}

static int lp5562_remove(struct i2c_client *client)
{
	struct lp55xx_led *led = i2c_get_clientdata(client);
	struct lp55xx_chip *chip = led->chip;

	lp5562_stop_engine(chip);

	lp55xx_unregister_sysfs(chip);
	lp55xx_unregister_leds(led, chip);
	lp55xx_deinit_device(chip);

	return 0;
}

static void lp5562_shutdown(struct i2c_client *client)

{
	struct lp55xx_led *led = i2c_get_clientdata(client);
	struct lp55xx_chip *chip = led->chip;

	lp5562_stop_engine(chip);

}

static struct of_device_id mx_led_of_match_table[] = {
	{
		.compatible = "ti,lp5562",
	},
	{},
};


static const struct i2c_device_id lp5562_id[] = {
	{ "lp5562", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp5562_id);

static struct i2c_driver lp5562_driver = {
	.driver = {
		.name	= "lp5562",
		.owner 	= THIS_MODULE,
		.of_match_table = mx_led_of_match_table,
	},
	.probe		= lp5562_probe,
	.remove		= lp5562_remove,
	.shutdown 	= lp5562_shutdown,
	.id_table	= lp5562_id,
};

module_i2c_driver(lp5562_driver);

MODULE_DESCRIPTION("Texas Instruments LP5562 LED Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
