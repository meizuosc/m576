/////////////////////////////////////////////////////////////////////////////
// File Name	: OIS_user.c
// Function		: User defined function.
// 				  These functions depend on user's circumstance.
// 				  
// Rule         : Use TAB 4
// 
// Copyright(c)	Rohm Co.,Ltd. All rights reserved 
// 
/***** ROHM Confidential ***************************************************/
#ifndef OIS_USER_C
#define OIS_USER_C
#endif

#ifndef __KERNEL__
#include <stdio.h>
#endif
#include "OIS_head.h"

// Following Variables that depend on user's environment			RHM_HT 2013.03.13	add
OIS_UWORD			FOCUS_VAL	= 0x0122;				// Focus Value

// <== RHM_HT 2013/07/10	Added new user definition variables

// /////////////////////////////////////////////////////////
// VCOSET function
// ---------------------------------------------------------
// <Function>
//		To use external clock at CLK/PS, it need to set PLL.
//		After enabling PLL, more than 30ms wait time is required to change clock source.
//		So the below sequence has to be used:
// 		Input CLK/PS --> Call VCOSET0 --> Download Program/Coed --> Call VCOSET1
//
// <Input>
//		none
//
// <Output>
//		none
//
// =========================================================
void	VCOSET0( void )
{

    OIS_UWORD 	CLK_PS = 24000;            						// Input Frequency [kHz] of CLK/PS terminal (Depend on your system)
    OIS_UWORD 	FVCO_1 = 36000;                					// Target Frequency [kHz]
																// 27000 for 63163
																// 36000 for 63165
    OIS_UWORD 	FREF   = 25;             						// Reference Clock Frequency [kHz]
 
    OIS_UWORD	DIV_N  = CLK_PS / FREF - 1;         			// calc DIV_N
    OIS_UWORD	DIV_M  = FVCO_1 / FREF - 1;         			// calc DIV_M
 
    I2C_OIS_per_write( 0x62, DIV_N  ); 							// Divider for internal reference clock
    I2C_OIS_per_write( 0x63, DIV_M  ); 							// Divider for internal PLL clock
    I2C_OIS_per_write( 0x64, 0x4060 ); 							// Loop Filter

    I2C_OIS_per_write( 0x60, 0x3011 ); 							// PLL
    I2C_OIS_per_write( 0x65, 0x0080 ); 							// 
    I2C_OIS_per_write( 0x61, 0x8002 ); 							// VCOON 
    I2C_OIS_per_write( 0x61, 0x8003 ); 							// Circuit ON 
    I2C_OIS_per_write( 0x61, 0x8809 ); 							// PLL ON
}


void	VCOSET1( void )
{
//     Wait( 30 );                  							// Wait for PLL lock

    I2C_OIS_per_write( 0x05, 0x000C ); 							// Prepare for PLL clock as master clock
    I2C_OIS_per_write( 0x05, 0x000D ); 							// Change to PLL clock
}


// /////////////////////////////////////////////////////////
// Write Data to Slave device via I2C master device
// ---------------------------------------------------------
// <Function>
//		I2C master send these data to the I2C slave device.
//		This function relate to your own circuit.
//
// <Input>
//		OIS_UBYTE	slvadr	I2C slave adr
//		OIS_UBYTE	size	Transfer Size
//		OIS_UBYTE	*dat	data matrix
//
// <Output>
//		none
//
// <Description>
//		[S][SlaveAdr][W]+[dat[0]]+...+[dat[size-1]][P]
//	
// =========================================================
void	WR_I2C( OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat )
{
	/* Please write your source code here. */
	OIS_WORD ret;
	struct mz_ois_i2c_data	mz_ois_i2c_data;
	/* Maximum bytes of one I2C transaction is 64K */
	BUG_ON(size > (1UL << 16));
	mz_ois_i2c_data.rw = 1;
	mz_ois_i2c_data.slavaddr = slvadr;
	mz_ois_i2c_data.data_buf = dat;
	mz_ois_i2c_data.data_len = size;

	ret = mz_ois_i2c_write(&mz_ois_i2c_data);
	if (ret < 0) {
		pr_err("%s(), write err:%d\n", __func__, ret);
	}
}


// *********************************************************
// Read Data from Slave device via I2C master device
// ---------------------------------------------------------
// <Function>
//		I2C master read data from the I2C slave device.
//		This function relate to your own circuit.
//
// <Input>
//		OIS_UBYTE	slvadr	I2C slave adr
//		OIS_UBYTE	size	Transfer Size
//		OIS_UBYTE	*dat	data matrix
//
// <Output>
//		OIS_UWORD	16bit data read from I2C Slave device
//
// <Description>
//	if size == 1
//		[S][SlaveAdr][W]+[dat[0]]+         [RS][SlaveAdr][R]+[RD_DAT0]+[RD_DAT1][P]
//	if size == 2
//		[S][SlaveAdr][W]+[dat[0]]+[dat[1]]+[RS][SlaveAdr][R]+[RD_DAT0]+[RD_DAT1][P]
//
// *********************************************************
OIS_UWORD	RD_I2C( OIS_UBYTE slvadr, OIS_UBYTE size, OIS_UBYTE *dat )
{
	OIS_UWORD	read_data = 0;
	
	/* Please write your source code here. */
	OIS_WORD ret;
	OIS_UBYTE rd_buf[2];
	struct mz_ois_i2c_data	mz_ois_i2c_data;
	BUG_ON(size > MZ_OIS_MAX_ADDR_LEN);
	mz_ois_i2c_data.rw = 0;
	mz_ois_i2c_data.slavaddr = slvadr;

	mz_ois_i2c_data.addr_len = size;
	mz_ois_i2c_data.addr_buf = dat;

	/* We only read 16bit data */
	mz_ois_i2c_data.data_len = sizeof(rd_buf);
	mz_ois_i2c_data.data_buf = rd_buf;

	ret = mz_ois_i2c_read(&mz_ois_i2c_data);
	if (ret < 0) {
		pr_err("%s(), read err:%d\n", __func__, ret);
		return ret;
	}

	read_data = (rd_buf[0] << 8) | rd_buf[1];
	return read_data;
}


// *********************************************************
// Write Factory Adjusted data to the non-volatile memory
// ---------------------------------------------------------
// <Function>
//		Factory adjusted data are sotred somewhere
//		non-volatile memory.
//
// <Input>
//		_FACT_ADJ	Factory Adjusted data
//
// <Output>
//		none
//
// <Description>
//		You have to port your own system.
//
// *********************************************************
void	store_FADJ_MEM_to_non_volatile_memory( _FACT_ADJ param )
{
	/* 	Write to the non-vollatile memory such as EEPROM or internal of the CMOS sensor... */	
}

// *********************************************************
// Read Factory Adjusted data from the non-volatile memory
// ---------------------------------------------------------
// <Function>
//		Factory adjusted data are sotred somewhere
//		non-volatile memory.  I2C master has to read these
//		data and store the data to the OIS controller.
//
// <Input>
//		none
//
// <Output>
//		_FACT_ADJ	Factory Adjusted data
//
// <Description>
//		You have to port your own system.
//
// *********************************************************
_FACT_ADJ	get_FADJ_MEM_from_non_volatile_memory( void )
{
	/* 	Read from the non-vollatile memory such as EEPROM or internal of the CMOS sensor... */	
	_FACT_ADJ *p_FACT_ADJ = NULL;

	if (mz_ois_get_fadj_data(&p_FACT_ADJ)) {
		pr_info("%s(), get meizu's FADJ data failed, use default\n", __func__);
		p_FACT_ADJ = &FADJ_MEM;
	} else {
		pr_info("%s(), get meizu's FADJ data success\n", __func__);
	}

	return *p_FACT_ADJ;
}


// *********************************************************
// Wait
// ---------------------------------------------------------
// <Function>
//
// <Input>
//		OIS_ULONG	time	on the micro second time scale
//
// <Output>
//		none
//
// <Description>
//
// *********************************************************
void	Wait_usec( OIS_ULONG time )
{
	/* Please write your source code here. */
	
	{
		// Sample Code
		
		// Argument of Sleep() is on the msec scale.
// 		DWORD msec = time / 1000;
// 		
// 		if (msec == 0){
// 			msec = 1;
// 		}
// 		Sleep(msec);
	}
}


#ifdef	DEBUG_FADJ
#ifndef __KERNEL__
// *********************************************************
// Printf for DEBUG
// ---------------------------------------------------------
// <Function>
//
// <Input>
//		const char *format, ...	
// 				Same as printf
//
// <Output>
//		none
//
// <Description>
//
// *********************************************************
int debug_print(const char *format, ...)
{
	char str[512];
	int r;
	va_list va;

	int length = (int)strlen(format);

	if(	length >= 512 ){
		printf("length of %s: %d\n", format, length);
		return -1;
	}

	va_start(va, format);
	r = vsprintf(str, format, va);
	va_end(va);
	
	printf(str);

	
	return r;
}
#endif
#endif
