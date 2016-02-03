/******************************************************************************

	file		: isl29125.c

	Description	: Driver for ISL29125 RGB light sensor

	License		: GPLv2

	Copyright	: Intersil Corporation (c) 2013	
******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/gpio.h>			//vvdn change
#include <linux/idr.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <asm/io.h>
#include <linux/interrupt.h>		//vvdn change
#include <linux/kobject.h>		//vvdn change
#include <linux/sysfs.h>		//vvdn change
#include <linux/irq.h>			//vvdn change
#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/input/isl29125.h>		//vvdn change
#include <linux/meizu-sys.h>
#include <linux/notifier.h>
#include <linux/fb.h>
//#ifdef CONFIG_HAS_EARLYSUSPEND
//#include <linux/earlysuspend.h>
//#endif

#define LINK_KOBJ_NAME "als"

static atomic_t isl_als_start = ATOMIC_INIT(0);
static int isl_num = 0;

#if 0
static struct i2c_board_info i2c_devs_info[] = {
	{
	    I2C_BOARD_INFO("isl29125", ISL29125_I2C_ADDR),
	}
};
#endif

enum als_range { 
		    RangeLo = 0, 
		    RangeHi, 
		    RangeMax
		};
enum resolution { 
		    Bit16 = 0, 
		    Bit12, 
		    BitMax 
		 };

static s32 XYZCCM_RangeW[3][3] ={
	{	1816L,	17397L,	-8268L},// X col
	{	-2382L,	21845L,	-8342L },// Y col
	{	-10234L, 9842L, 18381L}, // Z col
};

static s32 XYZCCM_RangeB[3][3] ={
	{	4893L,	18197L,	-8839L},// X col
	{	645L,	21845L,	-7700L },// Y col
	{	-4919L, 5086L, 18041L}, // Z col
};

#define ALS_SAMPLE_COUNT	4
/*
#define record_light_values(arry) \
	do {\
	    int i = 0;\
	    for (i=0; i < ALS_SAMPLE_COUNT; i++){\
		arry[i] = -1;\
		}\
	} while (0)
*/

static struct isl29125_data_t *isl29125_info = NULL;
extern bool is_white_lcd(void);

/*
static short int set_config2(u8 reg)
{
	short int ret;
	struct i2c_client *client = isl29125_info->client_data;

	ret = i2c_smbus_write_byte_data(client, CONFIG2_REG, (u8)reg);
	if (ret < 0) {
		return -1;
	}
	return ret;
}
*/

static s32 cal_cct(struct isl29125_data_t *dat)
{
	int cct;
	signed long long X0, Y0, Z0, sum0;
	signed long long x,y,n, xe, ye;
	u8 bits;
	unsigned long als_r, als_g, als_b;
	signed long long tmp;

	als_r = dat->cache_red;
	als_g = dat->cache_green;
	als_b = dat->cache_blue;

	bits = 0;
	if(isl29125_info->tptype == 1) //BLACK
	{
		X0 = ( XYZCCM_RangeB[0][0]*als_r + XYZCCM_RangeB[0][1]*als_g + XYZCCM_RangeB[0][2] * als_b );
		Y0 = ( XYZCCM_RangeB[1][0]*als_r + XYZCCM_RangeB[1][1]*als_g + XYZCCM_RangeB[1][2] * als_b );
		Z0 = ( XYZCCM_RangeB[2][0]*als_r + XYZCCM_RangeB[2][1]*als_g + XYZCCM_RangeB[2][2] * als_b );
	}else{
		X0 = ( XYZCCM_RangeW[0][0]*als_r + XYZCCM_RangeW[0][1]*als_g + XYZCCM_RangeW[0][2] * als_b );
		Y0 = ( XYZCCM_RangeW[1][0]*als_r + XYZCCM_RangeW[1][1]*als_g + XYZCCM_RangeW[1][2] * als_b );
		Z0 = ( XYZCCM_RangeW[2][0]*als_r + XYZCCM_RangeW[2][1]*als_g + XYZCCM_RangeW[2][2] * als_b );
	}

//	isl_log("X=%lld, Y=%lld, Z=%lld,\n", X0, Y0, Z0);

	sum0 = X0 + Y0 + Z0;
	if (sum0 == 0)
	{
 		//isl_log("sum0 value is 0\n");
		return -1;
	}
	x = div64_s64(X0*10000, sum0);
	y = div64_s64(Y0*10000, sum0);
	xe=3320; // 0.3320
	ye=1858; // 0.1858
	dat->x = x;
	dat->y = y;
	
	// The x,y ranges are (0.25, 0.45) ~ (0.545, 0.245),If the current xy values are out of the #2 ranges, return 0 for CCT value
#if 0
	if(( x<2500 || x>5450 )||( y>4500 || y<2450)){
		cct = 0; // cct 0 means "cct is not valid"
		dat->cct = 0;
		isl_log("CCT is not valid\n");
		return cct;
	}
#endif
	n = div64_s64(( x - xe )*10000,( y - ye ));
        //cct = n * (n * ((-449 * n) / 1000 + 3525) / 1000 - 6823) / 1000 + 5520;
	tmp = div64_s64(-449*n, 10000);
	tmp = div64_s64((tmp+3525)*n, 10000);
	tmp = div64_s64((tmp-6823)*n, 10000);
	cct = tmp + 5520;
        //n = (X<<31 - 712964572L *sum ) / ( Y<<17 - 24354L * sum);
        //cct = n * (n * ((-449*n)/16384 + 3525)/16384 - 6823)/16384 + 5520;

	dat->X = X0;
 	dat->Y = Y0;
	dat->Z = Z0;

	if(cct < 0) cct = 0;
	dat->cct = cct;

	return dat->cct;
}

static unsigned long long cal_lux(struct isl29125_data_t *dat)
{
	dat->lux = ((dat->raw_green_ircomp * dat->lux_coef) >> 8) / 10;

	return dat->lux;
}

ssize_t raw_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//u32 lux;
	//struct i2c_client *client = to_i2c_client(dev);
	struct isl29125_data_t *dat=dev_get_drvdata(dev);

	mutex_lock(&dat->rwlock_mutex);
	sprintf(buf, "R0=%d, G0=%d, B0=%d, GIR=%ld\n", dat->cache_red, dat->cache_green, dat->cache_blue, dat->raw_green_ircomp);	
	mutex_unlock(&dat->rwlock_mutex);
	return strlen(buf);
}

ssize_t show_lux(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned long long lux;
	//struct i2c_client *client = to_i2c_client(dev);
	struct isl29125_data_t *dat=dev_get_drvdata(dev);

	mutex_lock(&dat->rwlock_mutex);
	lux = cal_lux(dat);
	sprintf(buf, "%llu\n", lux);	
	mutex_unlock(&dat->rwlock_mutex);
	return strlen(buf);
}

static ssize_t show_cct(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
	s32 cct;

	struct isl29125_data_t *isl29125=dev_get_drvdata(dev);
	//struct i2c_client *client = to_i2c_client(dev);

	mutex_lock(&isl29125->rwlock_mutex);
    cct = cal_cct(isl29125);
    mutex_unlock(&isl29125->rwlock_mutex);
	if(cct<0)
	{
		return sprintf(buf, "cct_err : high IR light source\n");
	}
	else if(cct==0)
	{
		return sprintf(buf, "cct_err : out of range\n");
	}
	else
	{
		return sprintf(buf,"%d\n", cct);
	}
}

int set_optical_range(int *range)
{
        int ret;

        ret = i2c_smbus_read_byte_data(isl29125_info->client_data, CONFIG1_REG);
        if (ret < 0) {
                isl_log(KERN_ERR "%s: Failed to get data\n", __FUNCTION__);
                return -1;
        }

        if(*range == 10000)
                ret |= RGB_SENSE_RANGE_10000_SET;
        else if (*range == 375)
                ret &= RGB_SENSE_RANGE_375_SET;
        else
                return -1;

        ret = i2c_smbus_write_byte_data(isl29125_info->client_data, CONFIG1_REG, ret);
        if (ret < 0) {
                isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
                return -1;
        }

        return 0;
}

static int get_optical_range(int *range)
{
        int ret;

        ret = i2c_smbus_read_byte_data(isl29125_info->client_data, CONFIG1_REG);
        if (ret < 0) {
                isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
                return -1;
        }
	
        *range = (ret & RGB_SENSE_RANGE_10000_SET)?10000:375;

        return 0;

}

#define ADC_BIT_RESOLUTION_POS 4
static int get_adc_resolution_bits(int *res)
{
	int ret;

	ret = i2c_smbus_read_byte_data(isl29125_info->client_data, CONFIG1_REG); 
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;	
	}

	*res = (ret & (1 << ADC_BIT_RESOLUTION_POS ))?12:16; 

	return 0;
}

int set_adc_resolution_bits(u8 *res)
{
	int ret;
	int reg;

	reg = i2c_smbus_read_byte_data(isl29125_info->client_data, CONFIG1_REG);
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}	

	if(*res)
		reg |= ADC_RESOLUTION_12BIT_SET;
	else 
		reg &= ADC_RESOLUTION_16BIT_SET;
	ret = i2c_smbus_write_byte_data(isl29125_info->client_data, CONFIG1_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int set_mode(int mode)
{
	int ret;
	short int reg;

	reg = i2c_smbus_read_byte_data(isl29125_info->client_data, CONFIG1_REG);
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		return -1;
	}

	reg &= RGB_OP_MODE_CLEAR;
	reg |= mode;

	ret = i2c_smbus_write_byte_data(isl29125_info->client_data, CONFIG1_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__); 
		return -1;
	}

	return 0;
}

static ssize_t show_xy_value(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
	struct isl29125_data_t *dat=dev_get_drvdata(dev);
	//cal_cct(dat);
	return sprintf(buf,"x= %ld y= %ld\n",dat->x,dat->y);
}

void autorange(unsigned long long green)
{
	int ret;
	unsigned int adc_resolution, optical_range;		

	ret = get_adc_resolution_bits(&adc_resolution);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to get adc resolution\n", __FUNCTION__);
		return;
	}

	ret = get_optical_range(&optical_range);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to get optical range\n", __FUNCTION__);
		return;
	}

	switch (adc_resolution) {
		case 12:
			switch(optical_range) {
				case 375:
					/* Switch to 10000 lux */
					if(green > 0xCCC) {
						optical_range = 10000;
						if (isl29125_info->tptype == 1)
						    isl29125_info->lux_coef = BLACK_HIGH_LUX_COEF;
						else
						    isl29125_info->lux_coef = WHITE_HIGH_LUX_COEF;
						set_optical_range(&optical_range);
					}
					break;
				case 10000:
					/* Switch to 375 lux */
					if(green < 0xCC) {
						optical_range = 375;
						if (isl29125_info->tptype == 1)
						    isl29125_info->lux_coef = BLACK_LOW_LUX_COEF;
						else
						    isl29125_info->lux_coef = WHITE_LOW_LUX_COEF;
						set_optical_range(&optical_range);
					}
					break;
			}
			break;
		case 16:
			//isl_log("%s: the optical_range is %d\n", __func__, optical_range);
			switch(optical_range) {
				case 375:
					
					/* Switch to 10000 lux */
					if(green > 0xCCCC) {
						
						optical_range = 10000;
						//isl_log(":next range is 10000\n");
						if (isl29125_info->tptype == 1)
						    isl29125_info->lux_coef = BLACK_HIGH_LUX_COEF;
						else
						    isl29125_info->lux_coef = WHITE_HIGH_LUX_COEF;
						set_optical_range(&optical_range);
					}

					break;
				case 10000:
					/* Switch to 375 lux */
					if(green < 0xCCC) {
						optical_range = 375;
						//isl_log(":next range is 375\n");
						if (isl29125_info->tptype == 1)
						    isl29125_info->lux_coef = BLACK_LOW_LUX_COEF;
						else
						    isl29125_info->lux_coef = WHITE_LOW_LUX_COEF;
						set_optical_range(&optical_range);
					} else {
						if (isl29125_info->tptype == 1)
						    isl29125_info->lux_coef = BLACK_HIGH_LUX_COEF;
						else
						    isl29125_info->lux_coef = WHITE_HIGH_LUX_COEF;
					}
					break;
			}
			break;
	}
}

static int isl29125_i2c_read_word16(struct i2c_client *client, unsigned char reg_addr, unsigned short *buf)
{
	u8 dat[4];
	int ret;
	
	ret = i2c_smbus_read_i2c_block_data(client, reg_addr, 2, dat);
	if(ret != 2)
	{
		isl_log(KERN_ERR "%s: Failed to read block data\n", __FUNCTION__);
		return -1;
	}
	*buf = ((u16)dat[1] << 8) | (u16)dat[0];

	return 0;
}  

int isl29125_i2c_write_word16(struct i2c_client *client, unsigned char reg_addr, unsigned short *buf)
{
	int ret;
	unsigned char reg_h;
	unsigned char reg_l;

	/* Extract LSB and MSB bytes from data */
	reg_l = *buf & 0xFF;
	reg_h = (*buf & 0xFF00) >> 8;

	ret = i2c_smbus_write_byte_data(client, reg_addr, reg_l); 
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;	
	}

	ret = i2c_smbus_write_byte_data(client, reg_addr + 1, reg_h); 
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		return -1;	
	}

	return 0;
}

void initialize_isl29125(struct i2c_client *client)
{
	unsigned char reg;

	/* Set device mode to RGB ,RGB Data sensing range 10000 Lux(High Range),
	   ADC resolution 16-bit, ADC start at intb start(is SYNC set 0, 
	   ADC start by i2c write 1)*/
	i2c_smbus_write_byte_data(client, CONFIG1_REG, 0x0D);

	/* Default IR Active compenstation,
	   Disable IR compensation control */
	i2c_smbus_write_byte_data(client, CONFIG2_REG, isl29125_info->ir_comp);

	/* Interrupt threshold assignment for Green,G:01/R:10/B:11;
	   Interrupt persistency as 8 conversion data out of windows */
	i2c_smbus_write_byte_data(client, CONFIG3_REG, 0x0C);

	/* Writing interrupt low threshold as 0xCCC (5% of max range)  0x0CCC*/
	i2c_smbus_write_byte_data(client, LOW_THRESHOLD_LBYTE_REG, 0x00);
	i2c_smbus_write_byte_data(client, LOW_THRESHOLD_HBYTE_REG, 0x00);

	/* Writing interrupt high threshold as 0xF333 (80% of max range)  0xCCCC*/
	i2c_smbus_write_byte_data(client, HIGH_THRESHOLD_LBYTE_REG, 0xFF);
	i2c_smbus_write_byte_data(client, HIGH_THRESHOLD_HBYTE_REG, 0xFF);

	/* Clear the brownout status flag */
	reg = i2c_smbus_read_byte_data(client, STATUS_FLAGS_REG);
	reg &= ~(1 << BOUTF_FLAG_POS);
	i2c_smbus_write_byte_data(client, STATUS_FLAGS_REG, reg);
}

static ssize_t show_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);

	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG); 
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;	
	}
	isl_log("reg is %d\n",reg);
	switch (reg & 0x7) {
		case 0:
			sprintf(buf,"%s\n","pwdn"); 
			break;
		case 1:
			sprintf(buf,"%s\n","green"); 
			break;
		case 2:
			sprintf(buf, "%s\n","red"); 
			break;
		case 3:
			sprintf(buf, "%s\n","blue"); 
			break;
		case 4:
			sprintf(buf, "%s\n","standby"); 
			break;
		case 5:
			sprintf(buf, "%s\n","green.red.blue"); 
			break;
		case 6:
			sprintf(buf, "%s\n","green.red"); 
			break;
		case 7:
			sprintf(buf, "%s\n","green.blue"); 
			break;
	}

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t store_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        int ret;
        int mode;
        int val;

	mutex_lock(&isl29125_info->rwlock_mutex);

        val = simple_strtoul(buf, NULL, 10);
        if(val == 0) {
                mode = RGB_OP_PWDN_MODE_SET;
        } else if(val == 1) {
                mode = RGB_OP_GREEN_MODE_SET;
        } else if(val == 2) {
                mode = RGB_OP_RED_MODE_SET;
        } else if(val == 3) {
                mode = RGB_OP_BLUE_MODE_SET;
        } else if(val == 4) {
                mode = RGB_OP_STANDBY_MODE_SET;
        } else if(val == 5) {
                mode = RGB_OP_GRB_MODE_SET;
        } else if(val == 6) {
                mode = RGB_OP_GR_MODE_SET;
        } else if(val == 7) {
                mode = RGB_OP_GB_MODE_SET;
        } else {
                isl_log(KERN_ERR "%s: Invalid value\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
                return -1;
        }

        ret = set_mode(mode);
        if (ret < 0) {
                isl_log(KERN_ERR "%s: Failed to set operating mode\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
                return -1;
        }

	mutex_unlock(&isl29125_info->rwlock_mutex);
        return strlen(buf);

}

static ssize_t show_optical_range(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	int reg;

	mutex_lock(&isl29125_info->rwlock_mutex);

	ret = get_optical_range(&reg);
	if(ret < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	sprintf(buf, "%d", reg); 

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t show_adc_resolution_bits(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	int reg;

	mutex_lock(&isl29125_info->rwlock_mutex);
	ret = get_adc_resolution_bits(&reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__); 
		return -1;
	}

	sprintf(buf, "%d", reg); 
	mutex_unlock(&isl29125_info->rwlock_mutex);

	return strlen(buf);
}

static ssize_t store_adc_resolution_bits(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	u8 reg;
	int val;

	mutex_lock(&isl29125_info->rwlock_mutex);
	val = simple_strtoul(buf, NULL, 10);
	if(val == 0)
		reg = 0;
	else if(val == 1)
		reg = 1;
	else {
		isl_log(KERN_ERR "%s: Invalid input\n",__FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return count;
	}
	ret = set_adc_resolution_bits(&reg);	
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to set adc resolution\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return count;

	}
	mutex_unlock(&isl29125_info->rwlock_mutex);

	return strlen(buf);	
}

static ssize_t show_intr_threshold_high(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = isl29125_i2c_read_word16(client, HIGH_THRESHOLD_LBYTE_REG, &reg); 
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;	
	}

	sprintf(buf, "%d", reg);

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}


static ssize_t store_intr_threshold_high(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned short reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = simple_strtoul(buf, NULL, 10);
	if (reg == 0 || reg > 65535) {
		isl_log(KERN_ERR "%s: Invalid input value\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}
	ret = isl29125_i2c_write_word16(client, HIGH_THRESHOLD_LBYTE_REG, &reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write word\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}	

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);	

}

static ssize_t show_intr_threshold_low(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = isl29125_i2c_read_word16(client, LOW_THRESHOLD_LBYTE_REG, &reg); 
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;	
	}

	sprintf(buf, "%d", reg); 

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t store_intr_threshold_low(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned short reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = simple_strtoul(buf, NULL, 10);
	if (reg == 0 || reg > 65536) {
		mutex_unlock(&isl29125_info->rwlock_mutex);
		isl_log(KERN_ERR "%s: Invalid input value\n", __FUNCTION__);
		return -1;
	}

	ret = isl29125_i2c_write_word16(client, LOW_THRESHOLD_LBYTE_REG, &reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write word\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}	
	mutex_unlock(&isl29125_info->rwlock_mutex);

	return strlen(buf);	
}

static ssize_t show_intr_threshold_assign(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG); 
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;	
	}

	/* Extract interrupt threshold assign value */
	reg = (reg & ((0x3) << INTR_THRESHOLD_ASSIGN_POS)) >> INTR_THRESHOLD_ASSIGN_POS;	

	switch(reg) {
		case 0:
			sprintf(buf, "%s", "none"); 
			break;
		case 1:
			sprintf(buf, "%s", "green"); 
			break;
		case 2:
			sprintf(buf, "%s", "red"); 
			break;
		case 3:
			sprintf(buf, "%s", "blue"); 
			break;
	}

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);

}

static ssize_t store_intr_threshold_assign(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	short int reg;
	short int threshold_assign;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	if(!strcmp(buf, "none")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_CLEAR;				  	
	} else if(!strcmp(buf, "green")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_GREEN;
	} else if(!strcmp(buf, "red")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_RED;
	} else if(!strcmp(buf, "blue")) {
		threshold_assign = INTR_THRESHOLD_ASSIGN_BLUE;
	} else {
		isl_log(KERN_ERR "%s: Invalid value\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG);
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	reg &= INTR_THRESHOLD_ASSIGN_CLEAR;
	reg |= threshold_assign;	

	ret = i2c_smbus_write_byte_data(client, CONFIG3_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t show_intr_persistency(struct device *dev, struct device_attribute *attr, char *buf)
{

	short int reg;
	short int intr_persist;

	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG); 
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;	
	}

	reg = (reg & (0x3 << INTR_PERSIST_CTRL_POS)) >> INTR_PERSIST_CTRL_POS;

	switch(reg) {
		case 0:
			intr_persist = 1;
			break;
		case 1:
			intr_persist = 2;
			break;
		case 2:
			intr_persist = 4;
			break;
		case 3:
			intr_persist = 8;
			break; 	

	}

	sprintf(buf, "%d", intr_persist); 
	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t store_intr_persistency(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	short int reg;
	short int intr_persist;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);

	intr_persist = simple_strtoul(buf, NULL, 10);
	if (intr_persist == 8)
		intr_persist = INTR_PERSIST_SET_8;
	else if (intr_persist == 4)
		intr_persist = INTR_PERSIST_SET_4;
	else if (intr_persist == 2)
		intr_persist = INTR_PERSIST_SET_2;
	else if (intr_persist == 1)
		intr_persist = INTR_PERSIST_SET_1;
	else {
		isl_log(KERN_ERR "%s: Invalid value\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG);
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	reg &= INTR_PERSIST_CTRL_CLEAR;
	reg |= intr_persist << INTR_PERSIST_CTRL_POS; 	

	ret = i2c_smbus_write_byte_data(client, CONFIG3_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t show_rgb_conv_intr(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG); 
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;	
	}

	reg = (reg & (1 << RGB_CONV_TO_INTB_CTRL_POS)) >> RGB_CONV_TO_INTB_CTRL_POS;

	sprintf(buf, "%s", reg?"disable":"enable"); 

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t store_rgb_conv_intr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, rgb_conv_intr;
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	ret = simple_strtoul(buf, NULL, 10);
	if(ret == 1)
		rgb_conv_intr = 0;
	else if(ret == 0)
		rgb_conv_intr = 1;
	else {
		isl_log(KERN_ERR "%s: Invalid input for rgb conversion interrupt [0-1]\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	reg = i2c_smbus_read_byte_data(client, CONFIG3_REG);
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}	

	reg &= RGB_CONV_TO_INTB_CLEAR;
	reg |= rgb_conv_intr;

	ret = i2c_smbus_write_byte_data(client, CONFIG3_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}

	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);	
}

static ssize_t show_adc_start_sync(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG); 
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;	
	}

	reg = (reg & (1 << RGB_START_SYNC_AT_INTB_POS)) >> RGB_START_SYNC_AT_INTB_POS;

	sprintf(buf, "%s", reg?"risingIntb":"i2cwrite"); 
	mutex_unlock(&isl29125_info->rwlock_mutex);

	return strlen(buf);
}

static ssize_t store_adc_start_sync(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	short int reg, adc_start_sync;

	struct i2c_client *client = to_i2c_client(dev);                             
	ret = simple_strtoul(buf, NULL, 10);
	if(ret == 0)
		adc_start_sync = 0;
	else if(ret == 1)
		adc_start_sync = 1;
	else {
		isl_log(KERN_ERR "%s: Invalid value for adc start sync\n", __FUNCTION__);
		return -1;
	}

	mutex_lock(&isl29125_info->rwlock_mutex);
	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG);
	if (reg < 0) {
		isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}	

	if(adc_start_sync)
		reg |= ADC_START_AT_RISING_INTB;
	else 
		reg &= ADC_START_AT_I2C_WRITE;


	ret = i2c_smbus_write_byte_data(client, CONFIG1_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ERR "%s: Failed to write data\n", __FUNCTION__);
		mutex_unlock(&isl29125_info->rwlock_mutex);
		return -1;
	}
	mutex_unlock(&isl29125_info->rwlock_mutex);

	return strlen(buf);	
}

static ssize_t isl29125_enable_sensor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",isl29125_info->sensor_enable);
}

static ssize_t isl29125_enable_sensor_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = simple_strtoul(buf, NULL, 10);
	short int reg;

	if ((val != 0) && (val != 1)){
		return count;
	}
	isl_log("isl enable -- sensor enable = %ld\n", val);

	if(val == 1) {
		reg = i2c_smbus_read_byte_data(isl29125_info->client_data, CONFIG1_REG);
		if (reg == 0) {//default value is 0x05 or ox0d
			initialize_isl29125(isl29125_info->client_data);
		}

	    mutex_lock(&isl29125_info->rwlock_mutex);
	    atomic_set(&isl_als_start, 1);
	    isl29125_info->prev_lux = 0;
	    isl_num = 0;
	    //isl29125_info->record_count = 0;
	    //record_light_values(isl29125_info->record_arry);
	    queue_delayed_work(isl29125_info->event_wq, &isl29125_info->sensor_dwork, msecs_to_jiffies(POLL_DELAY));	// 0ms
	    mutex_unlock(&isl29125_info->rwlock_mutex);
	} else {
	    mutex_lock(&isl29125_info->rwlock_mutex);
	    cancel_delayed_work_sync(&isl29125_info->sensor_dwork);
		//isl29125_info->record_count = 0;
	    //record_light_values(isl29125_info->record_arry);
	    mutex_unlock(&isl29125_info->rwlock_mutex);
	}
	isl29125_info->sensor_enable = val;

	return count;
}

static ssize_t isl29125_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	
	return sprintf(buf, "%d\n",POLL_DELAY);
}

static ssize_t reg_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	short int reg;
	int i;
	struct i2c_client *client = to_i2c_client(dev);                             

	mutex_lock(&isl29125_info->rwlock_mutex);
	*buf = 0;
	for(i=0; i<15 ; i++)
	{
		reg = i2c_smbus_read_byte_data(client, (u8)i); 
		if (reg < 0) {
			isl_log(KERN_ERR "%s: Failed to read data\n", __FUNCTION__);
			mutex_unlock(&isl29125_info->rwlock_mutex);
			return -1;	
		}

		sprintf(buf, "%sreg%02x(%02x)\n", buf, i, reg); 
	}
	mutex_unlock(&isl29125_info->rwlock_mutex);
	return strlen(buf);
}

static ssize_t reg_dump_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	//unsigned long val = simple_strtoul(buf, NULL, 10);
	unsigned int reg, dat;
	int ret;	
	struct i2c_client *client = to_i2c_client(dev);                             

	sscanf(buf,"%02x %02x", &reg, &dat);

	mutex_lock(&isl29125_info->rwlock_mutex);
 
	ret = i2c_smbus_write_byte_data(client, (u8)reg, (u8)dat);

	mutex_unlock(&isl29125_info->rwlock_mutex);

	return count;
}

static ssize_t tptype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", isl29125_info->tptype? "B":"W");
}

/*
type: 1  --> black
type: 0  --> white
*/
static ssize_t tptype_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int type;

	sscanf(buf,"%d", &type);

	if (1 == type){
		isl29125_info->tptype = 1;
		isl29125_info->lux_coef = BLACK_HIGH_LUX_COEF;
	}else if (0 == type){
		isl29125_info->tptype = 0;
		isl29125_info->lux_coef = WHITE_HIGH_LUX_COEF;
	}

	return count;
}
static ssize_t isl29125_batch_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t isl_flush_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t isl_flush_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int sensors_id = 0;

	sscanf(buf, "%d\n", &sensors_id);
	isl_log("%s : buf = %d\n", __func__, sensors_id);

	//input_report_abs(isl29125_info->sensor_input, ABS_FLUSH_ISL, sensors_id);
	input_report_abs(isl29125_info->sensor_input, ABS_MISC, -1);
	input_sync(isl29125_info->sensor_input);

	return count;
}
static ssize_t isl_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	return sprintf(buf, "%d\n", isl29125_info->isl_debug);
}

static ssize_t isl_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int boost_debug = 0;

	sscanf(buf, "%u", &boost_debug);
	isl29125_info->isl_debug = !!boost_debug;

	return count;
}


// debugging and testing
static DEVICE_ATTR(raw_adc, ISL29125_SYSFS_PERMISSIONS, raw_adc_show, NULL); 
static DEVICE_ATTR(als_reg, ISL29125_SYSFS_PERMISSIONS, reg_dump_show, reg_dump_store); 
/* Attributes of ISL29125 RGB light sensor */

// main sysfs
static DEVICE_ATTR(cct, ISL29125_SYSFS_PERMISSIONS , show_cct, NULL);
static DEVICE_ATTR(lux, ISL29125_SYSFS_PERMISSIONS , show_lux, NULL);
static DEVICE_ATTR(xy_value, ISL29125_SYSFS_PERMISSIONS , show_xy_value, NULL);
// optional
static DEVICE_ATTR(mode, ISL29125_SYSFS_PERMISSIONS , show_mode, store_mode);
static DEVICE_ATTR(optical_range, ISL29125_SYSFS_PERMISSIONS , show_optical_range, NULL);
static DEVICE_ATTR(adc_resolution_bits, ISL29125_SYSFS_PERMISSIONS , show_adc_resolution_bits, store_adc_resolution_bits);
static DEVICE_ATTR(intr_threshold_high , ISL29125_SYSFS_PERMISSIONS , show_intr_threshold_high, store_intr_threshold_high);
static DEVICE_ATTR(intr_threshold_low , ISL29125_SYSFS_PERMISSIONS , show_intr_threshold_low, store_intr_threshold_low);
static DEVICE_ATTR(intr_threshold_assign , ISL29125_SYSFS_PERMISSIONS , show_intr_threshold_assign, store_intr_threshold_assign);
static DEVICE_ATTR(intr_persistency, ISL29125_SYSFS_PERMISSIONS , show_intr_persistency, store_intr_persistency);
static DEVICE_ATTR(rgb_conv_intr, ISL29125_SYSFS_PERMISSIONS , show_rgb_conv_intr, store_rgb_conv_intr);
static DEVICE_ATTR(adc_start_sync, ISL29125_SYSFS_PERMISSIONS , show_adc_start_sync, store_adc_start_sync);
// mandatory for android
static DEVICE_ATTR(als_enable, ISL29125_SYSFS_PERMISSIONS ,isl29125_enable_sensor_show, isl29125_enable_sensor_store);
static DEVICE_ATTR(poll_delay, ISL29125_SYSFS_PERMISSIONS ,isl29125_delay_show, NULL);
static DEVICE_ATTR(tptype, ISL29125_SYSFS_PERMISSIONS ,tptype_show, tptype_store);
static DEVICE_ATTR(als_batch, ISL29125_SYSFS_PERMISSIONS ,NULL, isl29125_batch_store);
static DEVICE_ATTR(als_flush, ISL29125_SYSFS_PERMISSIONS ,isl_flush_show, isl_flush_store);
static DEVICE_ATTR(debug, ISL29125_SYSFS_PERMISSIONS ,isl_debug_show, isl_debug_store);


static struct attribute *isl29125_attributes[] = {
	/* read RGB value attributes */
	&dev_attr_cct.attr,
	&dev_attr_lux.attr,
	&dev_attr_xy_value.attr,
	/* Device operating mode */ 
	&dev_attr_mode.attr,

	/* Current optical sensing range */
	&dev_attr_optical_range.attr,

	/* Current adc resolution */
	&dev_attr_adc_resolution_bits.attr,
	/* Interrupt related attributes */
	&dev_attr_intr_threshold_high.attr,
	&dev_attr_intr_threshold_low.attr,
	&dev_attr_intr_threshold_assign.attr,
	&dev_attr_intr_persistency.attr,
	&dev_attr_rgb_conv_intr.attr,
	&dev_attr_adc_start_sync.attr,

	&dev_attr_als_enable.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_als_reg.attr,
	&dev_attr_raw_adc.attr,
	&dev_attr_tptype.attr,
	&dev_attr_als_batch.attr,
	&dev_attr_als_flush.attr,	
	&dev_attr_debug.attr,
	NULL
};

static struct attribute_group isl29125_attr_group = {
	.attrs = isl29125_attributes
};

#if 0
static void als_swap(unsigned long long *x, unsigned long long *y)
{
        unsigned long long temp = *x;
        *x = *y;
        *y = temp;
}

static void get_max_lux(unsigned long long *arry)
{
	//unsigned long long temp, max_lux;
	int i,j;

	for (i = 0; i < ALS_SAMPLE_COUNT -1; i++) {
		for (j = i+1; j < ALS_SAMPLE_COUNT; j++) {
			if (arry[i] > arry[j])
				als_swap(arry+i, arry+j);
		}
	}
}
#endif

//W1_GRBG_INIT, W1_GREEN, W1_RED, W1_BLUE, W1_GREEN_IRCOMP, W1_GOTO_GRBG_INIT, 
static void isl29125_work_handler(struct work_struct *work)
{
	struct isl29125_data_t *isl29125 =
	   	 container_of(work, struct isl29125_data_t, sensor_dwork.work);
	struct input_dev *sensor_input = isl29125->sensor_input;
	int ret;
	//int i;
	//short int reg;
	//u8 dbg;
	//int cct, res, range;
	unsigned long long lux;
	int report_lux = -1;

	if(isl29125->isl_shutdn)
		return;

	ret = isl29125_i2c_read_word16(isl29125->client_data, GREEN_DATA_LBYTE_REG, &(isl29125->cache_green));
	if (ret < 0) {
	    isl_log(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
	    return;
	}
	ret = isl29125_i2c_read_word16(isl29125->client_data, RED_DATA_LBYTE_REG, &(isl29125->cache_red));
	if (ret < 0) {
	    isl_log(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
	    return;
	}
	ret = isl29125_i2c_read_word16(isl29125->client_data, BLUE_DATA_LBYTE_REG, &(isl29125->cache_blue));
	if (ret < 0) {
	    isl_log(KERN_ERR "%s: Failed to read word\n", __FUNCTION__);
	    return;
	}
		
	isl29125->raw_green_ircomp = isl29125->cache_green;

	lux = cal_lux(isl29125);
	/* LOW and HIGH range switch, add by txy*/
	autorange(isl29125->raw_green_ircomp);
	isl29125->cct = cal_cct(isl29125);
	//printk("[1]  lux = %llu,  prev lux = %llu, \n", lux, isl29125->prev_lux);
	/* debounce the zero*/
	if ((isl29125->prev_lux != 0) && (lux == 0)) {
		report_lux = lux;
		lux = isl29125->prev_lux;	
	}

	/*debounce processing : eg. 0,1,0,1
	if ((isl29125->record_arry[0] == -1) || (isl29125->record_arry[1] == -1)
		|| (isl29125->record_arry[2] == -1) || (isl29125->record_arry[3] == -1))
	{
	    isl29125->record_arry[isl29125->record_count] = lux;
	    isl29125->record_count++;
	} else {
		if (isl29125->prev_lux != lux) {
		    isl29125->record_count = ALS_SAMPLE_COUNT - 3;
		    do {
			isl29125->record_arry[isl29125->record_count-1] = isl29125->record_arry[isl29125->record_count];
			isl29125->record_count++;
		    } while (isl29125->record_count < ALS_SAMPLE_COUNT);

		    isl29125->record_arry[isl29125->record_count-1] = lux;
		}

		if ((isl29125->record_arry[0] == isl29125->record_arry[2])
			&& (isl29125->record_arry[1] == isl29125->record_arry[3])
			&& (isl29125->record_arry[2] != isl29125->record_arry[3]))
		{
		    lux = max(isl29125->record_arry[2], isl29125->record_arry[3]);
		}
	}
*/
	if (atomic_read(&isl_als_start)) {
		lux += 1;
		atomic_set(&isl_als_start, 0);
	}
	
	isl29125->lux = lux;
	
	input_report_abs(sensor_input, ABS_LUX, isl29125->lux);
	input_report_abs(sensor_input, ABS_GREEN, isl29125->cache_green);
	input_report_abs(sensor_input, ABS_GREENIR, isl29125->cct);
	input_sync(sensor_input);
		
	if (report_lux == 0)
	    isl29125->prev_lux = report_lux;
	else
	    isl29125->prev_lux = lux;
	
	if (isl_num < 4){
		isl_num++;
		queue_delayed_work(isl29125_info->event_wq,&isl29125->sensor_dwork, msecs_to_jiffies(POLL_DELAY));	// restart timer
	}else{
		queue_delayed_work(isl29125_info->event_wq,&isl29125->sensor_dwork, msecs_to_jiffies(DEFAULT_CONVERSION_TIME));	// restart timer
	}

	if (isl29125->isl_debug)
		isl_log("R=%u, G=%u, B=%u, GIR=%lu, CCT=%d, LUX=%llu, cofe %ld\n",
		isl29125->cache_red, isl29125->cache_green, isl29125->cache_blue, 
		isl29125->raw_green_ircomp, isl29125->cct, isl29125->lux, isl29125->lux_coef);
		
}


static int isl29125_input_create(struct isl29125_data_t *isl29125)
{
    	int ret;

	isl29125->sensor_input = input_allocate_device();
	if (!isl29125->sensor_input) {
		isl_log("%s: Failed to allocate input device als\n", __func__);
		return -ENOMEM;
	}
	set_bit(EV_ABS, isl29125->sensor_input->evbit);
	input_set_capability(isl29125->sensor_input, EV_ABS, ABS_LUX);
	input_set_capability(isl29125->sensor_input, EV_ABS, ABS_GREEN);
	input_set_capability(isl29125->sensor_input, EV_ABS, ABS_GREENIR);
	input_set_capability(isl29125->sensor_input, EV_ABS, ABS_FLUSH_ISL);
	input_set_abs_params(isl29125->sensor_input, ABS_LUX, -1, 0xFFFF, 0, 0);
	input_set_abs_params(isl29125->sensor_input, ABS_GREEN, 0, 0xFFFF, 0, 0);
	input_set_abs_params(isl29125->sensor_input, ABS_GREENIR, 0, 0xFFFF, 0, 0);
	input_set_abs_params(isl29125->sensor_input, ABS_FLUSH_ISL, 0, 100, 0, 0);

	isl29125->sensor_input->name = "isl29125";
	isl29125->sensor_input->dev.parent = &isl29125->client_data->dev;

	ret = input_register_device(isl29125->sensor_input);
	if (ret) {
		isl_log("%s: Unable to register input device als: %s\n",
		       __func__,isl29125->sensor_input->name);
		return -1;
	}
	input_set_drvdata(isl29125->sensor_input, isl29125);

	return 0;
}

#if 0
static int isl29125_open(struct inode *inode, struct file *file)
{
	file->private_data = isl29125_info->client_data;

	if (!file->private_data)
	{
		isl_log("null pointer!!\n");
		return -EINVAL;
	}
	
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int isl29125_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long isl29125_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	switch (cmd){
	//case ALSPS_SET_ALS_MODE:
	//case ALSPS_GET_ALS_RAW_DATA:
	//	break;

	default:
		isl_log("%s not supported = 0x%04x", __FUNCTION__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}
	return err;    
}

static struct file_operations isl29125_fops = {
	.open = isl29125_open,
	.release = isl29125_release,
	.unlocked_ioctl = isl29125_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice isl29125_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "isl29125",
	.fops = &isl29125_fops,
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static int isl_early_suspend(struct early_suspend *h)
{
	int ret;
	short int reg;
	struct i2c_client *client = isl29125_info->client_data;

	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG);
	if(reg < 0) {
		isl_log(KERN_ALERT "%s: Failed to read CONFIG1_REG\n", __FUNCTION__); 
		goto err;
	}		

	reg &= RGB_OP_MODE_CLEAR;
	reg |= RGB_OP_STANDBY_MODE_SET; 

	/* Put the sensor device in standby mode */
	ret = i2c_smbus_write_byte_data(client, CONFIG1_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ALERT "%s: Failed to write to CONFIG1_REG\n", __FUNCTION__);
		goto err;
	}	 	

	if (isl29125_info->sensor_enable) {
	    //__cancel_delayed_work(&isl29125_info->sensor_dwork);
	    cancel_delayed_work(&isl29125_info->sensor_dwork);
	    isl29125_info->record_count = 0;
	    record_light_values(isl29125_info->record_arry);
    	}

	return 0;
err:
	return -1;
}

static int isl_late_resume(struct early_suspend *h)
{

	int ret;
	short int reg;
	struct i2c_client *client = isl29125_info->client_data;

	reg = i2c_smbus_read_byte_data(client, CONFIG1_REG);
	if(reg < 0) {
		isl_log(KERN_ALERT "%s: Failed to read CONFIG1_REG\n", __FUNCTION__); 
		goto err;
	}		

	reg &= RGB_OP_MODE_CLEAR;
	reg |= RGB_OP_GRB_MODE_SET; 

	/* Put the sensor device in active conversion mode */
	ret = i2c_smbus_write_byte_data(client, CONFIG1_REG, reg);
	if (ret < 0) {
		isl_log(KERN_ALERT "%s: Failed to write to CONFIG1_REG\n", __FUNCTION__);
		goto err;
	}	 	

	if (isl29125_info->sensor_enable)
	    schedule_delayed_work(&isl29125_info->sensor_dwork, msecs_to_jiffies(POLL_DELAY));

	return 0;
err:
	return -1;
}
#endif

static int isl_fb_state_chg_callback(struct notifier_block *nb, 
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if(val != FB_EVENT_BLANK)
		return 0;

	if(evdata && evdata->data && val == FB_EVENT_BLANK){
		blank = *(int *)(evdata->data);

		switch(blank){
		case FB_BLANK_POWERDOWN:
			isl_log("isl suspend -- sensor enable = %d\n", isl29125_info->sensor_enable);
			if(isl29125_info->sensor_enable){
				cancel_delayed_work_sync(&isl29125_info->sensor_dwork);
				isl29125_info->isl_fb = 1;
			}
			break;

		case FB_BLANK_UNBLANK:
			isl_log("isl resume -- sensor enable = %d, sensor fb = %d\n", isl29125_info->sensor_enable, isl29125_info->isl_fb);
			if(isl29125_info->isl_fb){
				isl29125_info->prev_lux = 0;
				isl29125_info->isl_fb = 0;
				queue_delayed_work(isl29125_info->event_wq, &isl29125_info->sensor_dwork, msecs_to_jiffies(POLL_DELAY));
			}
			break;

		default:
			break;

		}

	}
	return NOTIFY_OK;
}

static struct notifier_block isl_noti_block = {
	.notifier_call = isl_fb_state_chg_callback,
};

static int isl_sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	char reg;
	int ret;
	struct isl29125_data_t *isl29125; 
	//struct device_node *dp = client->dev.of_node;
	struct pinctrl *pinctrl;

	isl29125 = kzalloc(sizeof(struct isl29125_data_t), GFP_KERNEL);
	if(!isl29125)
	{
		isl_log(KERN_ERR "%s: failed to alloc memory for module data\n", __FUNCTION__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, isl29125);
	isl29125->client_data = client;
	isl29125_info = isl29125;
	isl29125_info->ir_comp = DEFAULT_IR_COMP;
	isl29125_info->isl_fb = 0;

	/*distinguish the white or black tp*/
	if(is_white_lcd()){
		//printk("***************** this is white lcd ! *************************\n");
		isl29125_info->tptype = 0;
		isl29125_info->lux_coef = WHITE_HIGH_LUX_COEF;
	}else{
		//printk("***************** this is black lcd ! *************************\n");
		isl29125_info->tptype = 1;
		isl29125_info->lux_coef = BLACK_HIGH_LUX_COEF;
	}
 
	/* Initialize a mutex for synchronization in sysfs file access */
	mutex_init(&isl29125->rwlock_mutex);

	/* Read the device id register from isl29125 sensor device */
	usleep_range(10000, 11000);;
	//for(i = 0;i<10;i++)
	reg = i2c_smbus_read_byte_data(client, DEVICE_ID_REG);
	isl_log("%s,i2c client address is 0x%x,chip id is 0x%x",__func__,client->addr,reg);
	/* Verify whether we have a valid sensor */
	if( reg != ISL29125_DEV_ID) {
		isl_log(KERN_ERR "%s: Invalid device id for isl29125 sensor device\n", __FUNCTION__);  
		goto err_id;
	}

	/* Initialize the sensor interrupt thread that would be scheduled by sensor
	   interrupt handler */

	pinctrl = devm_pinctrl_get_select(&client->dev, "intersil_light");
	if(IS_ERR(pinctrl)) 
		printk( "failed to get tp irq pinctrl - ON");

	ret = isl29125_input_create(isl29125);
	if(ret < 0){
		isl_log("%s: isl29125 sensor device create input device failed.\n", __FUNCTION__);
		goto err_id;
	}
	isl29125->event_wq = create_singlethread_workqueue("isl-event-queue");
	if (!isl29125->event_wq) {
		dev_err(&client->dev, "Cannot create work thread\n");
		goto err;
	}
	INIT_DELAYED_WORK(&isl29125->sensor_dwork, isl29125_work_handler); 

	/* Initialize the default configurations for isl29125 sensor device */ 
	initialize_isl29125(client);

	/* Register sysfs hooks */                                                  
	ret = sysfs_create_group(&client->dev.kobj, &isl29125_attr_group);          
	if(ret) {                                                                   
		isl_log(KERN_ERR "%s: Failed to create sysfs\n", __FUNCTION__);                    
		goto err;                                                           
	}                                                                           

	isl29125->fb_notifier = isl_noti_block;
	ret = fb_register_client(&isl29125->fb_notifier);
	if(ret){
		isl_log("isl register notifier failed.\n");
		goto err;
	}
	if(meizu_sysfslink_register(&client->dev, LINK_KOBJ_NAME) < 0)
		isl_log("als create meizu sys link failed!\n");
#ifdef CONFIG_HAS_EARLYSUSPEND
	isl29125->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	isl29125->early_suspend.suspend = isl_early_suspend;
	isl29125->early_suspend.resume = isl_late_resume;
	register_early_suspend(&isl29125->early_suspend);
#endif

	isl29125->isl_shutdn = 0;

	return 0;
err: 
	destroy_workqueue(isl29125->event_wq);
err_id:
	input_free_device(isl29125->sensor_input);
	kfree(isl29125);
	return -1;
}

static int isl_sensor_remove(struct i2c_client *client)
{
	struct isl29125_data_t *isl29125 = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&isl29125->early_suspend);
#endif
	//misc_deregister(&isl29125_device);
	sysfs_remove_group(&client->dev.kobj, &isl29125_attr_group);
	meizu_sysfslink_unregister(LINK_KOBJ_NAME);
	destroy_workqueue(isl29125->event_wq);
	input_free_device(isl29125->sensor_input);
	kfree(isl29125);	
	return 0;
}

static void isl_shutdown(struct i2c_client *client)
{
	struct isl29125_data_t *isl29125 = i2c_get_clientdata(client);
	isl29125->isl_shutdn = 1;

}

struct i2c_device_id isl_sensor_device_table[] = {
	{"isl29125", 0},
	{},
};

static struct of_device_id isl_of_match_table[] = {
	{
		.compatible = "intersil,isl29125",
	},
	{},
};

/* i2c device driver information */
static struct i2c_driver isl_sensor_driver = {
	.driver = {
		.name = "isl29125",
		.owner = THIS_MODULE,
		.of_match_table = isl_of_match_table,
		//.pm = &isl_pm_ops,
	},
	.probe    = isl_sensor_probe	   ,
	.remove   = isl_sensor_remove	   ,
	.shutdown = isl_shutdown,
	.id_table = isl_sensor_device_table,
};

static int __init isl29125_init(void)
{
	//i2c_register_board_info(3, i2c_devs_info, 1);
	/* Register i2c driver with i2c core */	
	return i2c_add_driver(&isl_sensor_driver);

}

static void __exit isl29125_exit(void)
{
	/* Unregister i2c driver with i2c core */
	i2c_del_driver(&isl_sensor_driver);
}

module_init(isl29125_init);
module_exit(isl29125_exit);

MODULE_AUTHOR("Intersil Corporation; Meizu");
MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("Driver for ISL29125 RGB light sensor");
