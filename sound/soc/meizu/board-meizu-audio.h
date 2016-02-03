/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

void meizu_enable_mclk(bool on);
void meizu_audio_clock_init(void);
int meizu_audio_regulator_init(struct device *dev);
int set_aud_pll_rate(struct device *dev, unsigned long rate);
