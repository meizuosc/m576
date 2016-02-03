/*
 * TDK tvclb820lba voice coil motor driver IC LC898212XD.
 *
 * Modified to comply with SS architecture by qudao@meizu.com.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include "../meizu_camera_special.h"
#include "AfInit.h"
#include "AfSTMV.h"

#define LENS_I2C_BUSNUM 0
#define E2PROM_WRITE_ID 0xA0
#define E2PROM_SHARP_WRITE_ID 0xA0
#define E2PROM_SENSOR_WRITE_ID 0x34

#define REAR_SHARP_MODULE  1
#define REAR_PRIMAX_MODULE 2

#define Min_Pos		0
#define Max_Pos		1023

#define MAX_INFI	0x6400
#define MAX_MACRO	0x9C00

signed short Hall_Max = 0x0000; // Please read INF position from EEPROM or OTP
signed short Hall_Min = 0x0000; // Please read MACRO position from EEPROM or OTP
signed short	AF_mid = 0x0000; // Please read 50cm DAC position from EEPROM or OTP
signed short	AF_inf = 0x0000; // Please read inf DAC position from EEPROM or OTP
signed short	AF_macro = 0x0000; // Please read macro DAC position from EEPROM or OTP

static bool af_active_progress = false;
static char af_active_result[1024];
#define LC898212XD_DRVNAME "LC898212XD"

#define LC898212XD_DEBUG
#ifdef LC898212XD_DEBUG
#define LC898212XDDB printk
#else
#define LC898212XDDB(x,...)
#endif

static spinlock_t g_LC898212XD_SpinLock;
static struct i2c_client * g_pstLC898212XD_I2Cclient = NULL;
static unsigned long g_u4LC898212XD_INF = 0;
static unsigned long g_u4LC898212XD_MACRO = 1023;
static unsigned long g_u4CurrPosition   = 0;

extern void RamReadA(unsigned short addr, unsigned short *data);

/*******************************************************************************
* WriteRegI2C
********************************************************************************/
int LC898212XD_WriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId)
{
	int  i4RetValue = 0;
	int retry = 3;
	u16 i2c_origi_addr;

	spin_lock(&g_LC898212XD_SpinLock);
	i2c_origi_addr = g_pstLC898212XD_I2Cclient->addr;
	g_pstLC898212XD_I2Cclient->addr = (i2cId >> 1);
	spin_unlock(&g_LC898212XD_SpinLock);

	do {
		i4RetValue = i2c_master_send(g_pstLC898212XD_I2Cclient,
			a_pSendData, a_sizeSendData);
		if (i4RetValue != a_sizeSendData) {
			LC898212XDDB("[LC898212XD] I2C send failed!!, Addr = 0x%x, Data = 0x%x \n",
				a_pSendData[0], a_pSendData[1] );
		} else {
			break;
		}
		udelay(50);
	} while ((retry--) > 0);

	spin_lock(&g_LC898212XD_SpinLock);
	g_pstLC898212XD_I2Cclient->addr = i2c_origi_addr;
	spin_unlock(&g_LC898212XD_SpinLock);
	return 0;
}

/*******************************************************************************
* ReadRegI2C
********************************************************************************/
int LC898212XD_ReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId)
{
	int  i4RetValue = 0;
	u16 i2c_origi_addr;

	spin_lock(&g_LC898212XD_SpinLock);
	i2c_origi_addr = g_pstLC898212XD_I2Cclient->addr;
	g_pstLC898212XD_I2Cclient->addr = (i2cId >> 1);
	spin_unlock(&g_LC898212XD_SpinLock);

	i4RetValue = i2c_master_send(g_pstLC898212XD_I2Cclient,
		a_pSendData, a_sizeSendData);
	if (i4RetValue != a_sizeSendData) {
		LC898212XDDB("[LC898212XD] I2C send failed!!, Slave Addr = 0x%x\n",
			g_pstLC898212XD_I2Cclient->addr);
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstLC898212XD_I2Cclient, (u8 *)a_pRecvData, a_sizeRecvData);
	if (i4RetValue != a_sizeRecvData) {
		LC898212XDDB("[LC898212XD] I2C read failed!! \n");
		return -1;
	}

	spin_lock(&g_LC898212XD_SpinLock);
	g_pstLC898212XD_I2Cclient->addr = i2c_origi_addr;
	spin_unlock(&g_LC898212XD_SpinLock);

	return 0;
}

void E2PROMReadA_sensor(unsigned short addr, u8 *data)
{
	int ret = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};
	ret = LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), data, 1, E2PROM_SENSOR_WRITE_ID);
	if (ret < 0)
		LC898212XDDB("[LC898212XD] I2C read e2prom failed!! \n");

	return;
}

void E2PROMReadA_sharp(unsigned short addr, u8 *data)
{
	int ret = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};
	ret = LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), data, 1, E2PROM_SHARP_WRITE_ID);     
	if (ret < 0)
		LC898212XDDB("[LC898212XD] I2C read e2prom failed!! \n");

	return;
}

void E2PROMReadA(unsigned short addr, u8 *data)
{
	int ret = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};
	ret = LC898212XD_ReadRegI2C(puSendCmd , sizeof(puSendCmd), data, 1, E2PROM_WRITE_ID);     
	if (ret < 0)
		LC898212XDDB("[LC898212XD] I2C read e2prom failed!! \n");

	return;
}

int AF_reverse_convert(signed short position)
{
	pr_info("%s(), input value: 0x%x(%d)\n",
		__func__, position, position);
	return (unsigned short)((((position)+32768)>>6) & 0x3ff);
}

unsigned short AF_convert(int position)
{
	return (((position)<<6)-32768) & 0xffff;
}

inline static int moveLC898212XD(unsigned long a_u4Position)
{
	StmvTo( AF_convert(a_u4Position) ) ;	// Move to Target Position

	spin_lock(&g_LC898212XD_SpinLock);
	g_u4CurrPosition = (unsigned long)a_u4Position;
	spin_unlock(&g_LC898212XD_SpinLock);
	return 0;
}

inline static int setLC898212XDInf(unsigned long a_u4Position)
{
    spin_lock(&g_LC898212XD_SpinLock);
    g_u4LC898212XD_INF = a_u4Position;
    spin_unlock(&g_LC898212XD_SpinLock);	
    return 0;
}

inline static int setLC898212XDMacro(unsigned long a_u4Position)
{
    spin_lock(&g_LC898212XD_SpinLock);
    g_u4LC898212XD_MACRO = a_u4Position;
    spin_unlock(&g_LC898212XD_SpinLock);	
    return 0;	
}

static int lc898212xd_init(void)
{
	stSmvPar StSmvPar;
	u8 buf[4];
	int module_id = 0;

	unsigned int HallOff = 0x00;	 	// Please Read Offset from EEPROM or OTP
	unsigned int HallBias = 0x00;   // Please Read Bias from EEPROM or OTP
    
	LC898212XDDB("[LC898212XD] LC898212XD_init - Start\n");

	module_id = mz_get_module_id(SENSOR_POSITION_REAR);
	pr_info("%s(), module_id:%d\n", __func__, module_id);
	if (module_id < 0) {
		pr_err("%s(), err! invalid module_id:%d\n",
			__func__, module_id);
		return -EINVAL;
	}

	if (module_id == REAR_PRIMAX_MODULE) {
		E2PROMReadA(0x00a4, &buf[0]);
		E2PROMReadA(0x00a5, &buf[1]);
		AF_inf = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA(0x00ac, &buf[0]);
		E2PROMReadA(0x00ad, &buf[1]);
		AF_macro = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA(0x00b8, &buf[0]);
		E2PROMReadA(0x00b9, &buf[1]);
		AF_mid = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA(0x00c8, &buf[0]);
		E2PROMReadA(0x00c9, &buf[1]);
		Hall_Max = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA(0x00cc, &buf[0]);
		E2PROMReadA(0x00cd, &buf[1]);
		Hall_Min = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA(0x00d0, &buf[0]);
		E2PROMReadA(0x00d1, &buf[1]);
		HallBias= (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA(0x00d4, &buf[0]);
		E2PROMReadA(0x00d5, &buf[1]);
		HallOff= (buf[0] | (buf[1] << 8)) & 0xFFFF;
	} else if (module_id == REAR_SHARP_MODULE) {
		E2PROMReadA_sharp(0x00a4, &buf[0]);
		E2PROMReadA_sharp(0x00a5, &buf[1]);
		AF_inf = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA_sharp(0x00ac, &buf[0]);
		E2PROMReadA_sharp(0x00ad, &buf[1]);
		AF_macro = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA_sharp(0x00b8, &buf[0]);
		E2PROMReadA_sharp(0x00b9, &buf[1]);
		AF_mid = (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA_sharp(0x00c8, &buf[0]);
		E2PROMReadA_sharp(0x00c9, &buf[1]);
		Hall_Max =  (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA_sharp(0x00cc, &buf[0]);
		E2PROMReadA_sharp(0x00cd, &buf[1]);
		Hall_Min =  (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA_sharp(0x00d0, &buf[0]);
		E2PROMReadA_sharp(0x00d1, &buf[1]);
		HallBias= (buf[0] | (buf[1] << 8)) & 0xFFFF;

		E2PROMReadA_sharp(0x00d4, &buf[0]);
		E2PROMReadA_sharp(0x00d5, &buf[1]);
		HallOff= (buf[0] | (buf[1] << 8)) & 0xFFFF;
	}

	pr_info("%s(), Hall_Max:0x%x, Hall_Min:0x%x, HallOff:0x%x, HallBias: 0x%x\n"
		"\tAf_inf:0x%x, Af_macro:0x%x, AF_mid:0x%x\n", __func__,
		Hall_Max, Hall_Min, HallOff, HallBias,
		AF_inf, AF_macro, AF_mid);

	AfInit( HallOff,  HallBias);	// Initialize driver IC

	// Step move parameter set
	StSmvPar.UsSmvSiz	= STMV_SIZE ;
	StSmvPar.UcSmvItv	= STMV_INTERVAL ;
	StSmvPar.UcSmvEnb	= STMCHTG_SET | STMSV_SET | STMLFF_SET ;
	StmvSet( StSmvPar ) ;
	
	ServoOn();	// Close loop ON

	LC898212XDDB("[LC898212XD] LC898212XD_init - End\n");

	return 0;
}

static ssize_t af_range_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	char *p = buf;
	int ret;
	int min_pos, max_pos;

	ret = camera_module_active(true);
	#ifndef IGNORE_POWER_STATUS
	if (ret) {
		pr_err("%s(), active camera module failed:%d\n",
			__func__, ret);
		return ret;
	}
	#endif

	/* initialize camera af */
	ret = lc898212xd_init();
	if (ret) {
		pr_info("%s(), lc898212xd_init() failed:%d\n",
			__func__, ret);
		camera_module_active(false);
		return ret;
	}

	min_pos = AF_reverse_convert(Hall_Min);
	max_pos = AF_reverse_convert(Hall_Max);
	p += sprintf(p, "Min AF position: %d\n", min_pos);
	p += sprintf(p, "Max AF position: %d\n", max_pos);

	pr_info("Hall_min: 0x%x ->%d, AF_inf: 0x%x ->%d, AF_mid: 0x%x ->%d\n"
		"\t AF_macro: 0x%x ->%d, Hall_Max: 0x%x ->%d\n",
		Hall_Min, AF_reverse_convert(Hall_Min),
		AF_inf, AF_reverse_convert(AF_inf),
		AF_mid, AF_reverse_convert(AF_mid),
		AF_macro, AF_reverse_convert(AF_macro),
		Hall_Max, AF_reverse_convert(Hall_Max));

	camera_module_active(false);

	return (p - buf);
}

static ssize_t af_range_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

static ssize_t af_active_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	char *p = buf;

	if (!af_active_progress) {
		pr_err("%s(), focus not triggerred yet\n", __func__);
		return -EPERM;
	}

	p += scnprintf(p, sizeof(af_active_result), af_active_result);
	af_active_progress = false;
	return (p - buf);
}

static ssize_t af_active_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int val = -1;
	int ret;
	unsigned short pos;
	int real_pos_10;
	signed short	test_pos_16 = 0x0000; /* 16bit vcm moveto set, just for test */
	char *p = af_active_result;

	if (af_active_progress) {
		pr_err("%s(), already triggered focus\n", __func__);
		return -EBUSY;
	}

	sscanf(buf, "%d", &val);
	if (val < 0 || val > 1023) {
		LC898212XDDB("[LC898212XD] invalid test moveto position\n");
		snprintf(af_active_result, sizeof(af_active_result), "Invalid AF move position!\n");
		/*
		* To comply with show method
		*/
		af_active_progress = true;
		return -EINVAL;
	}

	ret = camera_module_active(true);
	#ifndef IGNORE_POWER_STATUS
	if (ret) {
		pr_err("%s(), active camera module failed:%d\n",
		__func__, ret);
		return ret;
	}
	#endif

	/* initialize camera af */
	ret = lc898212xd_init();
	if (ret) {
		pr_info("%s(), lc898212xd_init() failed:%d\n",
			__func__, ret);
		camera_module_active(false);
		return ret;
	}

	moveLC898212XD(val);
	/* enough settle time for vcm moveto */
	msleep(1000);

	test_pos_16 = (signed short)AF_convert(val);	/* 10bit ADC exchange to 16bit ADC value */

	RamReadA(0x3C,	&pos);	/* Get Position */
	real_pos_10 = AF_reverse_convert(pos);

	camera_module_active(false);
	p += sprintf(p, "current AF position: %d\n", real_pos_10);

	pr_info("%s(), convert user's input to reg: %d -> %d, reverse convert %d -> %d\n",
		__func__, val, test_pos_16, test_pos_16, AF_reverse_convert(test_pos_16));
	pr_info("read from reg:%d, convers actual reg's value to user side: %d -> %d,",
		(signed short)pos, (signed short)pos, real_pos_10);

	/* check whether VCM moveto is blocked */
	pr_info("%s(), compare as signed, pos:%d, test_pos_16:%d\n",
		__func__, (signed short)pos, test_pos_16);

	if (((signed short)pos >= (test_pos_16 - 0x200))
		&& ((signed short)pos <= (test_pos_16 + 0x200))) {
		p += sprintf(p, "PASS:AF move is OK.\n");
	} else {
		p += sprintf(p, "NO PASS:AF move is not OK, maybe blocked!\n");
	}

	af_active_progress = true;
	return count;
}

static ssize_t lens_pos_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	char *p = buf;
	unsigned short pos;
	int real_pos_10;

	msleep(1000);
	RamReadA(0x3C,	&pos);	/* Get Position */
	real_pos_10 = AF_reverse_convert(pos);

	p += sprintf(p, "reg's value:0x%x, that's %d in 10bit\n", pos, real_pos_10);

	return (p - buf);
}

static struct device_attribute dev_attr_af_active = {
	.attr = {.name = "camera_af_active", .mode = 0644},
	.show = af_active_show,
	.store = af_active_store,
};

static struct device_attribute dev_attr_af_range = {
	.attr = {.name = "camera_af_range", .mode = 0444},
	.show = af_range_show,
	.store = af_range_store,
};

static struct device_attribute dev_attr_lens_pos = {
	.attr = {.name = "lens_pos", .mode = 0444},
	.show = lens_pos_show,
	.store = NULL,
};

int LC898212XD_probe(struct i2c_client *client)
{

	g_pstLC898212XD_I2Cclient = client;

	spin_lock_init(&g_LC898212XD_SpinLock);

	device_create_file(&client->dev, &dev_attr_af_active);
	device_create_file(&client->dev, &dev_attr_af_range);
	device_create_file(&client->dev, &dev_attr_lens_pos);

	dev_info(&client->dev, "%s() success, i2c client name && addr: %s, 0x%x\n", __func__,
		client->name, client->addr);
    return 0;
}

