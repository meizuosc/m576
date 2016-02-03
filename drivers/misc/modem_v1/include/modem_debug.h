#ifndef __MODEM_DEBUG_H__
#define __MODEM_DEBUG_H__

#include <linux/types.h>
#include <linux/time.h>
#include <linux/spinlock.h>

#define MAX_LOG_LEN	(256 - sizeof(struct timespec) - sizeof(int))
struct log_buff {
	struct timespec ts;
	int level;
	char data[MAX_LOG_LEN];
};

#define MAX_PM_TRACE_SIZE		4096
struct log_buff_circ_queue {
	spinlock_t lock;

	struct log_buff logb[MAX_PM_TRACE_SIZE];
	u32 in;
	u32 out;

	/*
	This flag is set when a log_buff_circ_queue instance becomes full for
	the first time, and must be cleared only after all logs are received by
	the bebugfs.
	*/
	bool full;
};

enum modemctl_event {
	MDM_EVENT_CP_FORCE_RESET,
	MDM_EVENT_CP_FORCE_CRASH,
	MDM_EVENT_CP_ABNORMAL_RX,
	MDM_CRASH_PM_FAIL,
	MDM_CRASH_PM_CP_FAIL,
	MDM_CRASH_INVALID_RB,
	MDM_CRASH_INVALID_IOD,
	MDM_CRASH_INVALID_SKBCB,
	MDM_CRASH_INVALID_SKBIOD,
	MDM_CRASH_NO_MEM,
	MDM_CRASH_CMD_RESET = 90,
	MDM_CRASH_CMD_EXIT,
};

int register_cp_crash_notifier(struct notifier_block *nb);
void modemctl_notify_event(enum modemctl_event evt);

extern void evt_log(int level, const char *fmt, ...);

#endif
