/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * This software is proprietary of Samsung Electronics.
 * No part of this software, either material or conceptual may be copied or distributed, transmitted,
 * transcribed, stored in a retrieval system or translated into any human or computer language in any form by any means,
 * electronic, mechanical, manual or otherwise, or disclosed
 * to third parties without the express written permission of Samsung Electronics.

 * Alternatively, this program is free software in case of open source projec;
 * you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 */

#ifndef __LCD_CTRL_H__
#define __LCD_CTRL_H__

#include "decon_lcd.h"

void lcd_init(int id, struct decon_lcd *lcd);
void lcd_enable(int id);
void lcd_disable(int id);
int lcd_gamma_ctrl(int id, unsigned int backlightlevel);
int lcd_gamma_update(int id);

#endif /* __LCD_CTRL_H__ */
