/*
 * Flash IC driver for MEIZU M76. This is based on TI's sample code.
 * Modified and maintained by QuDao, qudao@meizu.com
 *
 * Copyright (C) 2014 MEIZU
 * Copyright (C) 2014 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LM3644_H
#define __LM3644_H

#define LM3644_NAME "leds-lm3644"
#define LM3644_ADDR 0x63
//#define USE_NTC

/* uA */
#define LM3644_MIN_FLASH_CURRENT (11310)
#define LM3644_MAX_FLASH_CURRENT (1499750)
#define LM3644_FLASH_CURRENT_STEP (11720)
#define LM3644_MIN_TORCH_CURRENT (1060)
#define LM3644_MAX_TORCH_CURRENT (186480)
#define LM3644_TORCH_CURRENT_STEP (1460)

/*
* reg 0x1
*/
enum lm3644_reg_enable {
	REG_LED0_ENALBE_MASK = (1 << 0),
	REG_LED0_ENABLE = (1 << 0),
	REG_LED1_ENALBE_MASK = (1 << 1),
	REG_LED1_ENABLE = (1 << 1),
	REG_MODE_MASK = (3 << 2),
	REG_MODE_STANDBY = (0 << 2),
	REG_MODE_IR = (1 << 2),
	REG_MODE_TORCH = (2 << 2),
	REG_MODE_FLASH = (3 << 2),
	REG_TORCH_NTC_ENABLE_MASK = (1 << 4),
	REG_TORCH_NTC_ENABLE = (1 << 4),
	REG_STROBE_ENABLE_MASK = (1 << 5),
	REG_STROBE_ENABLE = (1 << 5),
	REG_TX_ENABLE_MASK = (1 << 7),
	REG_TX_ENABLE = (1 << 7),
};

/*
* reg 0x2
*/
enum lm3644_reg_ivfm {
	REG_UVLO_MASK = (1 << 6),
	REG_UVLO_ENABLE = (1 << 6),
};

/*
* reg 0x3
*/
enum lm3644_reg_flash0_br {
	REG_FLASH0_BR_MASK = (0x7f << 0),
};

/*
* reg 0x4
*/
enum lm3644_reg_flash1_br {
	REG_FLASH1_BR_MASK = (0x7f << 0),
};

/*
* reg 0x5
*/
enum lm3644_reg_torch0_br {
	REG_TORCH0_BR_MASK = (0x7f << 0),
};

/*
* reg 0x6
*/
enum lm3644_reg_torch1_br {
	REG_TORCH1_BR_MASK = (0x7f << 0),
};

/*
* reg 0x8
*/
enum lm3644_reg_flash_tout {
	REG_FLASH_TOUT_MASK = (0xf << 0),
	REG_FLASH_TOUT_MAX = (0xf << 0),
};

/*
* reg 0x9
*/
enum lm3644_reg_temp {
	REG_TORCH_TEMP_FUNC_MASK = (1 << 0),
	REG_TEMP_FUNC = (1 << 0),
	REG_TORCH_FUNC = (0 << 0),
	REG_TEMP_DETECT_VOL_MASK = (0x7 << 1),
	REG_NTC_SHORT_FAULT_MASK = (1 << 4),
	REG_NTC_SHORT_FAULT_ENABLE = (1 << 4),
	REG_NTC_OPEN_FAULT_MASK = (1 << 5),
	REG_NTC_OPEN_FAULT_ENABLE = (1 << 5),
};
#endif /* __LM3644_H */
