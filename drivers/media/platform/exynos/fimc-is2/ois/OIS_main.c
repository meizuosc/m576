/////////////////////////////////////////////////////////////////////////////
// File Name	: OIS_main.c
// Function		: Main control function runnning on ISP.
//                ( But Just for example )
// Rule         : Use TAB 4
// 
// Copyright(c)	Rohm Co.,Ltd. All rights reserved 
// 
/***** ROHM Confidential ***************************************************/
#ifndef OIS_MAIN_C
#define OIS_MAIN_C
#endif

#ifndef __KERNEL__
#include <stdio.h>
#endif

#include "OIS_head.h"


// GLOBAL variable ( Upper Level Host Set this Global variables )
////////////////////////////////////////////////////////////////////////////////
OIS_UWORD	BOOT_MODE     = _FACTORY_;		// Execute Factory Adjust or not
											// This main routine ( below ) polling this variable.
	#define	AF_REQ			0x8000
	#define	SCENE_REQ_ON	0x4000
	#define	SCENE_REQ_OFF	0x2000
	#define	POWERDOWN		0x1000
	#define	INITIAL_VAL		0x0000
OIS_UWORD	OIS_REQUEST   = INITIAL_VAL;	// OIS control register.
	
OIS_UWORD	OIS_SCENE     = _SCENE_D_A_Y_1;	// OIS Scene parameter.
											// Upper Level Host can change the SCENE parameter.
// ==> RHM_HT 2013.03.04	Change type (OIS_UWORD -> double)
double		OIS_PIXEL[2];					// Just Only use for factory adjustment.
// <== RHM_HT 2013.03.04
ADJ_STS		OIS_MAIN_STS  = ADJ_ERR;		// Status register of this main routine.	RHM_HT 2013/04/15	Change "typedef" and initial value.


// MAIN
////////////////////////////////////////////////////////////////////////////////
int main( void ){

	_FACT_ADJ	fadj;										// Factory Adjustment data
	
	//------------------------------------------------------
	// Get Factory adjusted data
	//------------------------------------------------------
	fadj = get_FADJ_MEM_from_non_volatile_memory();		// Initialize by Factory adjusted value.

	//------------------------------------------------------
	// Enable Source Power and Input external clock to CLK/PS pin.
	//------------------------------------------------------
	/* Please write your source code here. */

	//------------------------------------------------------
	// PLL setting to use external CLK
	//------------------------------------------------------
	VCOSET0();
	
	//------------------------------------------------------
	// Download Program and Coefficient
	//------------------------------------------------------
	DEBUG_printf(("call func_PROGRAM_DOWNLOAD\n"));
	OIS_MAIN_STS = func_PROGRAM_DOWNLOAD( );				// Program Download
	if ( OIS_MAIN_STS <= ADJ_ERR ) return OIS_MAIN_STS;		// If success OIS_MAIN_STS is zero.		RHM_HT 2013/04/15	Change term and return value.

	DEBUG_printf(("call func_COEF_DOWNLOAD\n"));
	func_COEF_DOWNLOAD( 0 );								// Download Coefficient
	
	//------------------------------------------------------
	// Change Clock to external pin CLK_PS
	//------------------------------------------------------
	VCOSET1();

	//------------------------------------------------------
    // Issue DSP start command.
	//------------------------------------------------------
	I2C_OIS_spcl_cmnd( 1, _cmd_8C_EI );						// DSP calculation START
	
	//------------------------------------------------------
	// Set calibration data
	//------------------------------------------------------
	SET_FADJ_PARAM( &fadj );
	
	//------------------------------------------------------
	// Set default AF dac and scene parameter for OIS
	//------------------------------------------------------
	I2C_OIS_F0123_wr_( 0x90,0x00, 0x0130 );					// AF Control ( Value is example )
	func_SET_SCENE_PARAM_for_NewGYRO_Fil( _SCENE_SPORT_3, 1, 0, 0, &fadj );	// Set default SCENE ( Just example )
	
	DEBUG_printf(("gl_CURDAT = 0x%04X\n", fadj.gl_CURDAT));
	DEBUG_printf(("gl_HALOFS_X = 0x%04X\n", fadj.gl_HALOFS_X));
	DEBUG_printf(("gl_HALOFS_Y = 0x%04X\n", fadj.gl_HALOFS_Y));
	DEBUG_printf(("gl_PSTXOF = 0x%04X\n", fadj.gl_PSTXOF));
	DEBUG_printf(("gl_PSTYOF = 0x%04X\n", fadj.gl_PSTYOF));
	DEBUG_printf(("gl_HX_OFS = 0x%04X\n", fadj.gl_HX_OFS));
	DEBUG_printf(("gl_HY_OFS = 0x%04X\n", fadj.gl_HY_OFS));
	DEBUG_printf(("gl_GX_OFS = 0x%04X\n", fadj.gl_GX_OFS));
	DEBUG_printf(("gl_GY_OFS = 0x%04X\n", fadj.gl_GY_OFS));
	DEBUG_printf(("gl_KgxHG  = 0x%04X\n", fadj.gl_KgxHG));
	DEBUG_printf(("gl_KgyHG  = 0x%04X\n", fadj.gl_KgyHG));
	DEBUG_printf(("gl_KGXG   = 0x%04X\n", fadj.gl_KGXG));
	DEBUG_printf(("gl_KGYG   = 0x%04X\n", fadj.gl_KGYG));
	DEBUG_printf(("gl_SFTHAL_X = 0x%04X\n", fadj.gl_SFTHAL_X));
	DEBUG_printf(("gl_SFTHAL_Y = 0x%04X\n", fadj.gl_SFTHAL_Y));
	DEBUG_printf(("gl_TMP_X_ = 0x%04X\n", fadj.gl_TMP_X_));
	DEBUG_printf(("gl_TMP_Y_ = 0x%04X\n", fadj.gl_TMP_Y_));
	DEBUG_printf(("gl_KgxH0 = 0x%04X\n", fadj.gl_KgxH0));
	DEBUG_printf(("gl_KgyH0 = 0x%04X\n", fadj.gl_KgyH0));
	
	return ADJ_OK;												// RHM_HT 2013/04/15	Change return value
}
