#ifndef _LINUX_MUIC_TSU6721_H
#define _LINUX_MUIC_TSU6721_H

#include <linux/notifier.h>

#define USB_HOST_ATTACH	0x0001	/* Notify of system down */
#define USB_HOST_DETACH	0x0002
#define USB_OTG_ATTACH       0x0003
#define USB_OTG_DETACH     0x0004
#define ADAPTER_ATTACH        0x0005
#define ADAPTER_DETACH      0x0006
#define MEIZU_M76_AC_ATTACH	0x0007
#define MEIZU_M76_AC_DETACH	0x0008
#define NON_STANDARD_ATTACH  0x0009
#define NON_STANDARD_DETACH  0x000A
#define APPLE_ATTACH			0x000B
#define APPLE_DETACH			0x000C
#define USB_PORT_ATTACH       0x000D
#define USB_PORT_DETACH     0x000E
#define UART_ATTACH       0x000F
#define UART_DETACH     0x0010

extern int register_muic_notifier(struct notifier_block *);
extern int unregister_muic_notifier(struct notifier_block *);
extern bool check_cable_status(void);


#endif /* _LINUX_MUIC_TSU6721_H */
