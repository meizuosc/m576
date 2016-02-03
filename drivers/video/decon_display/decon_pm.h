/* drivers/video/decon_display/decon_dt.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_HEADER__
#define __DECON_DISPLAY_HEADER__

#define S3CFB_PLATFORM_RESET (S3CFB_WIN_CONFIG + 100)

int init_display_pm_status(struct display_driver *dispdrv);
int init_display_pm(struct display_driver *dispdrv);
int disp_pm_set_plat_status(struct display_driver *dispdrv, bool platform_on);
int disp_pm_add_refcount(struct display_driver *dispdrv);
int disp_pm_dec_refcount(struct display_driver *dispdrv);
void disp_pm_te_triggered(struct display_driver *dispdrv);
int disp_pm_runtime_enable(struct display_driver *dispdrv);
int disp_pm_runtime_get_sync(struct display_driver *dispdrv);
int disp_pm_runtime_put_sync(struct display_driver *dispdrv);
void disp_pm_gate_lock(struct display_driver *dispdrv, bool increase);
int disp_pm_sched_power_on(struct display_driver *dispdrv, unsigned int cmd);
void disp_set_pm_status(int flag);
#endif
