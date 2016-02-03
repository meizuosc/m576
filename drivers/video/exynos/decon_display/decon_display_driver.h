/* drivers/video/decon_display/decon_display_driver.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_DRIVER_HEADER__
#define __DECON_DISPLAY_DRIVER_HEADER__

#include <linux/kthread.h>
#include <linux/platform_device.h>

/* command mode is defined 0 in DT */
#define VIDEO_MODE	1
#define COMMAND_MODE	0

/* Display Driver Ic */
enum type_of_ddi {
	TYPE_OF_SM_DDI = 0,	/* Samsung Mobile: as a default */
	TYPE_OF_MAGNA_DDI,	/* MAGNA */
	TYPE_OF_NORMAL_DDI,	/* No Samsung and MAGNA */
};

struct decon_lcd {
	u32	mode;
	u32	vfp;
	u32	vbp;
	u32	hfp;
	u32	hbp;

	u32	vsa;
	u32	hsa;

	u32	xres;
	u32	yres;

	u32	width;
	u32	height;

	u32	hs_clk;
	u32	esc_clk;

	u32	fps;

	u32	mic;

	u32	hsync_2h_cycle;

	u32	mic_bitstream_swap;
	enum type_of_ddi ddi_type;
};

enum{
	DISP_STATUS_PM0 = 0,	/* initial status */
	DISP_STATUS_PM1,	/* platform started */
	DISP_STATUS_PM2,	/* platform suspended */
	DISP_STATUS_PM_MAX,
};

extern struct decon_lcd *decon_get_lcd_info(void);

struct display_driver;

#define MAX_GPIO	10

/* display_gpio - GPIOs resource for the display subsystem */
struct display_gpio {
	unsigned id[MAX_GPIO];
	int num;
};

#define MAX_SYSREG	2

struct display_sysreg {
	void __iomem *disp_sysreg[MAX_SYSREG];
};

/* pm_ops - HW IP or DISPLAY Block-specific callbacks */
struct pm_ops {
	void (*clk_on)(struct display_driver *dispdrv);
	void (*clk_off)(struct display_driver *dispdrv);
	int (*pwr_on)(struct display_driver *dispdrv);
	int (*pwr_off)(struct display_driver *dispdrv);
};

/* display_controller_ops - operations for controlling power of
 * dispay controller. */
struct display_controller_ops {
	int (*init_display_decon_clocks)(struct device *dev);
	int (*enable_display_decon_clocks)(struct device *dev);
	int (*disable_display_decon_clocks)(struct device *dev);
	int (*enable_display_decon_runtimepm)(struct device *dev);
	int (*disable_display_decon_runtimepm)(struct device *dev);
	int (*enable_display_dsd_clocks)(struct device *dev);
	int (*disable_display_dsd_clocks)(struct device *dev);
};

/* display_driverr_ops - operations for controlling power of
 * device */
struct display_driver_ops {
	int (*init_display_dsi_clocks)(struct device *dev);
	int (*enable_display_driver_power)(struct device *dev);
	int (*disable_display_driver_power)(struct device *dev);
	int (*reset_display_driver_panel)(struct device *dev);
};

/* display_dt_ops - operations for parsing device tree */
struct display_dt_ops {
	int (*parse_display_driver_dt)(struct platform_device *np,
		struct display_driver *ddp);
	struct s3c_fb_driverdata *(*get_display_drvdata)(void);
	struct s3c_fb_platdata *(*get_display_platdata)(void);
	struct mipi_dsim_config *(*get_display_dsi_drvdata)(void);
	struct mipi_dsim_lcd_config *(*get_display_lcd_drvdata)(void);
	struct display_gpio *(*get_display_dsi_reset_gpio)(void);
#ifdef CONFIG_DECON_MIC
	struct mic_config *(*get_display_mic_config)(void);
#endif
};

/* display_component_decon - This structure is abstraction of the
 * display controller device drvier. */
struct display_component_decon {
	struct resource *regs;
	int irq_no;
	int fifo_irq_no;
	int i80_irq_no;
	struct s3c_fb *sfb;
	struct display_controller_ops decon_ops;
	struct clk *clk;
	struct clk *dsd_clk;
	struct clk *gate_dsd_clk;
	struct pm_ops *ops;
};

/* display_component_dsi - This structure is abstraction of the
 * MIPI-DSI device drvier. */
struct display_component_dsi {
	struct resource *regs;
	int dsi_irq_no;
	struct display_driver_ops dsi_ops;
	struct mipi_dsim_device *dsim;
	struct clk *clk;
	struct pm_ops *ops;
};

struct display_component_mic {
	struct resource *regs;
	struct decon_mic *mic;
	struct clk *clk;
	struct pm_ops *ops;
};

/* display_pm_status - for representing the status of the display
 * PM component. In normal display, there are three main status.
 * one is the status getting set_win_config,
 * two is the status waiting fence from user,
 * the other is the status wating VSYNC. */
struct display_pm_status {
	spinlock_t slock;
	struct mutex pm_lock;
	struct mutex clk_lock;
	int clock_enabled;
	atomic_t lock_count;
	struct kthread_worker	control_clock_gating;
	struct task_struct	*control_clock_gating_thread;
	struct kthread_work	control_clock_gating_work;
	struct kthread_worker	control_power_gating;
	struct task_struct	*control_power_gating_thread;
	struct kthread_work	control_power_gating_work;
	const struct pm_ops *ops;
	bool clock_gating_on;
	bool power_gating_on;
	bool hotplug_gating_on;
	int clk_idle_count;
	int pwr_idle_count;
	int hotplug_delay_msec;
};

#define MAX_CLK_GATING_COUNT 0
#define MAX_PWR_GATING_COUNT 3
#define MAX_HOTPLUG_DELAY_MSEC 100

enum decon_ops_stamp {
	OPS_GET_WIN_CONFIG_IOCTL = 0,	/* 0 */
	OPS_RUN_FB_TEIRQ,
	OPS_RUN_UPDATE_REG_QUEUE_ENTER,
	OPS_RUN_UPDATE_REG_QUEUE_EXIT, 	/* 3 */
	OPS_CALL_RUNTIME_RESUME,
	OPS_CALL_RUNTIME_SUSPEND,
	OPS_CALL_DSIM_FRAME_DONE, 	/* 6 */
	OPS_CALL_DISPLAY_POWER_ON,
	OPS_CALL_DISPLAY_POWER_OFF,
	OPS_CALL_FB_BLANK_IN, 		/* 9 */
	OPS_CALL_FB_BLANK_OUT,
	//OPS_CALL_DISPLAY_CLOCK_ON, /* Disable */
	//OPS_CALL_DISPLAY_CLOCK_OFF,

	OPS_CALL_MAX, /* Last */
};

#ifdef CONFIG_DEBUG_FS
#define DISPLAY_DEBUG_FIFO_COUNT 16
struct decon_ops_timestamp {
	ktime_t		fifo_timestamps[DISPLAY_DEBUG_FIFO_COUNT];
	unsigned int	num_timestamps;
};
#endif

/* display_driver - Abstraction for display driver controlling
 * all display system in the system */
struct display_driver {
	/* platform driver for display system */
	struct device *display_driver;
	struct display_component_decon decon_driver;
	struct display_component_dsi dsi_driver;
	struct display_component_mic mic_driver;
	struct display_dt_ops dt_ops;
	unsigned int platform_status;
	struct display_pm_status pm_status;
#ifdef CONFIG_DEBUG_FS
	struct decon_ops_timestamp ops_timestamp[OPS_CALL_MAX];
#endif
};

#define GET_DISPDRV_OPS(p) (p)->dsi_driver.dsi_ops
#define GET_DISPCTL_OPS(p) (p)->decon_driver.decon_ops

struct platform_device;

struct display_driver *get_display_driver(void);
struct display_sysreg *decon_get_sysreg(void);
int create_decon_display_controller(struct platform_device *pdev);
int create_mipi_dsi_controller(struct platform_device *pdev);
#ifdef CONFIG_DECON_MIC
int create_decon_mic(struct platform_device *pdev);
#endif
int save_decon_operation_time(enum decon_ops_stamp ops);
int dump_decon_operation_time(void);

extern unsigned int *ref_power_status;

#endif
