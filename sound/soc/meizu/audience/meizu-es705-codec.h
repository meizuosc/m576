/*
 * meizu-es705-codec.h  --  Audience eScore I2S interface
 *
 * Copyright 2011 Audience, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MEIZU_ES705_CODEC_H
#define _MEIZU_ES705_CODEC_H

#define ES705_PORT_GET_PARAM (0x800B << 16)
#define ES705_PORT_PARAM_ID  (0x800C << 16)
#define ES705_PORT_SET_PARAM (0x800D << 16)
#define ES705_PORT_A_ID (0x0A << 8)
#define ES705_PORT_B_ID (0x0B << 8)
#define ES705_PORT_C_ID (0x0C << 8)
#define ES705_PORT_D_ID (0x0D << 8)
#define ES705_PORT_WORDLENGHT    0x00
#define ES705_PORT_LATCHEDGE     0x04
#define ES705_PORT_ENDIANNESS    0x05
#define ES705_PORT_MODE          0x07
#define ES705_PORT_CLOCK         0x09
#define ES705_PORT_JUSTIFICATION 0x0A

#define ES705_DHWPT_COMMAND (0x8052 << 16)
#define ES705_DHWPT_UNIDIRECTIONAL (0x01 << 8)
#define ES705_DHWPT_RST 0x00
#define ES705_DHWPT_A_B 0xC4
#define ES705_DHWPT_A_C 0xC8
#define ES705_DHWPT_A_D 0xCC
#define ES705_DHWPT_B_A 0xD1
#define ES705_DHWPT_B_C 0xD9
#define ES705_DHWPT_B_D 0xDD
#define ES705_DHWPT_C_A 0xE2
#define ES705_DHWPT_C_B 0xE6
#define ES705_DHWPT_C_D 0xEE
#define ES705_DHWPT_D_A 0xF3
#define ES705_DHWPT_D_B 0xF7
#define ES705_DHWPT_D_C 0xFB

int es705_codec_add_dev(void);

#endif
