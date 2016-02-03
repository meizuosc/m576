/*
 * Copyright (c) 2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TBASE_TUI_H__
#define __TBASE_TUI_H__

#define TRUSTEDUI_MODE_OFF                0x00
#define TRUSTEDUI_MODE_ALL                0xff
#define TRUSTEDUI_MODE_TUI_SESSION    0x01
#define TRUSTEDUI_MODE_VIDEO_SECURED  0x02
#define TRUSTEDUI_MODE_INPUT_SECURED  0x04

int trustedui_blank_inc(void);
int trustedui_blank_dec(void);
int trustedui_blank_get_counter(void);
void trustedui_blank_set_counter(int counter);

int trustedui_get_current_mode(void);
void trustedui_set_mode(int mode);
int trustedui_set_mask(int mask);
int trustedui_clear_mask(int mask);

/* Use the arch_extension sec pseudo op before switching to secure world */
#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#define MC_ARCH_EXTENSION_SEC
#endif

/*
 * MobiCore SMCs
 */
#define MC_SMC_N_YIELD		0x3 /* Yield to switch from NWd to SWd. */
#define MC_SMC_N_SIQ		0x4  /* SIQ to switch from NWd to SWd. */

/*
 * MobiCore fast calls. See MCI documentation
 */
#define MC_FC_INIT		-1
#define MC_FC_INFO		-2
#define MC_FC_POWER		-3
#define MC_FC_DUMP		-4
#define MC_FC_NWD_TRACE		-31 /* Mem trace setup fastcall */


/*
 * return code for fast calls
 */
#define MC_FC_RET_OK				0
#define MC_FC_RET_ERR_INVALID			1
#define MC_FC_RET_ERR_ALREADY_INITIALIZED	5


/* structure wrappers for specific fastcalls */

/* generic fast call parameters */
union fc_generic {
	struct {
		uint32_t cmd;
		uint32_t param[3];
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t param[2];
	} as_out;
};

/* fast call init */
union mc_fc_init {
	union fc_generic as_generic;
	struct {
		uint32_t cmd;
		uint32_t base;
		uint32_t nq_info;
		uint32_t mcp_info;
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t rfu[2];
	} as_out;
};

/* fast call info parameters */
union mc_fc_info {
	union fc_generic as_generic;
	struct {
		uint32_t cmd;
		uint32_t ext_info_id;
		uint32_t rfu[2];
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t state;
		uint32_t ext_info;
	} as_out;
};

/*
 * _smc() - fast call to MobiCore
 *
 * @data: pointer to fast call data
 */
static inline long _smc(void *data)
{
	int ret = 0;

	if (data == NULL)
		return -EPERM;

#ifdef MC_SMC_FASTCALL
	{
		ret = smc_fastcall(data, sizeof(union fc_generic));
	}
#else
	{
		union fc_generic *fc_generic = data;
		/* SMC expect values in r0-r3 */
		register u32 reg0 __asm__("r0") = fc_generic->as_in.cmd;
		register u32 reg1 __asm__("r1") = fc_generic->as_in.param[0];
		register u32 reg2 __asm__("r2") = fc_generic->as_in.param[1];
		register u32 reg3 __asm__("r3") = fc_generic->as_in.param[2];

		__asm__ volatile (
#ifdef MC_ARCH_EXTENSION_SEC
			/* This pseudo op is supported and required from
			 * binutils 2.21 on */
			".arch_extension sec\n"
#endif
			"smc 0\n"
			: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
		);
#ifdef __ARM_VE_A9X4_QEMU__
		__asm__ volatile (
		    "nop\n"
		    "nop\n"
		    "nop\n"
		    "nop"
		 );
#endif

		/* set response */
		fc_generic->as_out.resp     = reg0;
		fc_generic->as_out.ret      = reg1;
		fc_generic->as_out.param[0] = reg2;
		fc_generic->as_out.param[1] = reg3;
	}
#endif
	return ret;
}

/*
 * convert fast call return code to linux driver module error code
 */
static inline int convert_fc_ret(uint32_t sret)
{
	int ret = -EFAULT;

	switch (sret) {
	case MC_FC_RET_OK:
		ret = 0;
		break;
	case MC_FC_RET_ERR_INVALID:
		ret = -EINVAL;
		break;
	case MC_FC_RET_ERR_ALREADY_INITIALIZED:
		ret = -EBUSY;
		break;
	default:
		break;
	}
	return ret;
}

#endif

