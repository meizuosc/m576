#ifndef	AUTO_SLEEP_WAKEUP_TEST_H
#define	AUTO_SLEEP_WAKEUP_TEST_H

#include <linux/types.h>

enum {
	AUTO_SLEEP_WAKEUP_TEST_PRESS_POWER_BUTTON	=  0,

};

extern int auto_sleep_wakeup_test_register_client(struct notifier_block *nb);
extern int auto_sleep_wakeup_test_unregister_client(struct notifier_block *nb);
extern int auto_sleep_wakeup_test_notifier_call_chain(unsigned long val, void *v);

#endif	/* AUTO_SLEEP_WAKEUP_TEST_H */
