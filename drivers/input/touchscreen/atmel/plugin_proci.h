
#ifndef __LINUX_ATMEL_MXT_PLUG_PI
#define __LINUX_ATMEL_MXT_PLUG_PI

#include <linux/types.h>

extern struct plugin_proci mxt_plugin_pi;

#define MAX_GESTURE_SUPPORTED_IN_T116 MAX_KEYS_SUPPORTED_IN_DRIVER
#define MAX_GESTURE_TRACE_STROKE 15

#define CHARACTER_ASCII_BEGIN 0x21

enum{
	UNLOCK_0 = 0,
	UNLOCK_1,
	UNLOCK_NUM,
};

enum{
	SLIDING_LEFT = 0,
	SLIDING_RIGHT,
	SLIDING_UP,
	SLIDING_DOWN,
	SLIDING_NUM
};

enum{
	SLIDING_AND_CHARACTER = SLIDING_NUM,
};

//must less than CHARACTER_ASCII_BEGIN

#define SUBNAME_GES(x)	(char)((x) & 0x7F)

enum{
	GES_CTRL_EN = 7,
	GES_SWITCH = 8,
};

ssize_t plugin_proci_pi_gesture_show(struct plugin_proci *p, char *buf, size_t count);
int plugin_proci_pi_gesture_store(struct plugin_proci *p, const char *buf, size_t count);
ssize_t plugin_proci_pi_trace_show(struct plugin_proci *p, char *buf, size_t count);

#endif


