/*
 * Copyright (c) 2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/switch.h>

#include "tui_ioctl.h"
#include "tlcTui.h"
#include "mobicore_driver_api.h"
#include "dciTui.h"
#include "tui-hal.h"

/* extern */
extern struct mc_session_handle drSessionHandle;
extern struct completion dciComp;
extern uint32_t gCmdId;
extern tlcTuiResponse_t gUserRsp;

/* Global variables */
DECLARE_COMPLETION(ioComp);

/* Static variables */
static struct cdev tui_cdev;
static struct task_struct *threadId;

struct switch_dev tui_switch;

static long tui_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;
	int __user *uarg = (int __user *)arg;

	if (_IOC_TYPE(cmd) != TUI_IO_MAGIC)
		return -EINVAL;

	pr_err("t-base-tui module: ioctl 0x%x ", cmd);

	switch (cmd) {

	case TUI_IO_NOTIFY:
		pr_err("TUI_IO_NOTIFY\n");

		if (tlcNotifyEvent(arg))
			ret = 0;
		else
			ret = -EFAULT;
		break;

	case TUI_IO_WAITCMD:
		pr_err("TUI_IO_WAITCMD\n");

		/* Create the TlcTui Main thread and start secure driver (only
		   1st time) */
		if (drSessionHandle.session_id == 0) {
			threadId = kthread_run(mainThread, NULL, "dci_thread");
			if (!threadId) {
				pr_debug(KERN_ERR "Unable to start Trusted UI main thread\n");
				return -EFAULT;
			}
		}

		/* Wait for signal from DCI handler */
		wait_for_completion_interruptible(&dciComp);
		INIT_COMPLETION(dciComp);

		/* Write command id to user */
		pr_debug("IOCTL: sending command %d to user.\n", gCmdId);

		if (copy_to_user(uarg, &gCmdId, sizeof(gCmdId)))
			ret = -EFAULT;
		else
			ret = 0;
		break;

	case TUI_IO_ACK:
		pr_err("TUI_IO_ACK\n");

		/* Read user response */
		if (copy_from_user(&gUserRsp, uarg, sizeof(gUserRsp)))
			ret = -EFAULT;
		else
			ret = 0;

		/* Send signal to DCI */
		pr_debug("IOCTL: User completed command %d.\n", gUserRsp.id);
		complete(&ioComp);
		break;

	default:
		pr_info("undefined!\n");
		return -ENOTTY;
	}

	return ret;
}

static struct file_operations tui_fops = {
	.owner = THIS_MODULE,
	.compat_ioctl = tui_ioctl,
};

/*--------------------------------------------------------------------------- */
static int __init tlcTui_init(void)
{
	pr_err("Loading t-base-tui module.\n");
	pr_debug("\n=============== Running TUI Kernel TLC ===============\n");

	dev_t devno;
	int err;
	static struct class *tui_class;

	err = alloc_chrdev_region(&devno, 0, 1, TUI_DEV_NAME);
	if (err) {
		pr_debug(KERN_ERR "Unable to allocate Trusted UI device number\n");
		return err;
	}

	cdev_init(&tui_cdev, &tui_fops);
	tui_cdev.owner = THIS_MODULE;
	/*    tui_cdev.ops = &tui_fops; */

	err = cdev_add(&tui_cdev, devno, 1);
	if (err) {
		pr_debug(KERN_ERR "Unable to add Trusted UI char device\n");
		unregister_chrdev_region(devno, 1);
		return err;
	}

	tui_class = class_create(THIS_MODULE, "tui_cls");
	if (IS_ERR(tui_class)) {
		pr_debug(KERN_ERR "Failed to create tui class.\n");
		unregister_chrdev_region(devno, 1);
		cdev_del(&tui_cdev);
		return -1;
	}

	device_create(tui_class, NULL, devno, NULL, TUI_DEV_NAME);

	if (!hal_tui_init()) {
		pr_debug(KERN_ERR "Failed to initialize tui hal\n");
		unregister_chrdev_region(devno, 1);
		cdev_del(&tui_cdev);
		return -1;
	}

	/* register the switch device for tui */
	tui_switch.name = "tui";
	err = switch_dev_register(&tui_switch);
	if (err)
		pr_debug(KERN_ERR "Failed to register tui_switch.\n");

	return 0;
}

static void __exit tlcTui_exit(void)
{
	pr_info("Unloading t-base-tui module.\n");

	switch_dev_unregister(&tui_switch);
	unregister_chrdev_region(tui_cdev.dev, 1);
	cdev_del(&tui_cdev);

	hal_tui_exit();
}

module_init(tlcTui_init);
module_exit(tlcTui_exit);

MODULE_AUTHOR("Trustonic");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("TUI Kernel TLC");
