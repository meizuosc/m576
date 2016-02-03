#ifndef	EXYNOS_TOUCH_DAEMON_H
#define	EXYNOS_TOUCH_DAEMON_H

#include <linux/types.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/wakelock.h>

#define TOUCHPOINT	1000

enum {
	V_SCROLL = 0,
	H_SCROLL,
	FLICKING_TO_LEFT,
	FLICKING_TO_UP,
};

struct touch_point {
	int x[TOUCHPOINT];
	int y[TOUCHPOINT];
	int wx[TOUCHPOINT];
	int wy[TOUCHPOINT];
	int count;
};

struct exynos_touch_daemon_data {
	struct work_struct work;
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_DSX
	struct synaptics_rmi4_data *touchdata;
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS
	struct melfas_ts_data *touchdata;
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS
	struct fts_ts_info *touchdata;
#endif
	struct timespec ts;
	int enable;
	int record;
	int mode;
	int scenario;
	int start;
	int frame;
	int powerlog;
	s64 start_time;
	unsigned int response_time;
	unsigned int finish_time;
	int booster;
	struct touch_point tp;
};
#endif	/* EXYNOS_TOUCH_DAEMON_H */
