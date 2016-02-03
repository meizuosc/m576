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


#include <linux/string.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/exynos_ion.h>
#include <linux/switch.h>

#include "mobicore_driver_api.h"
#include "tlcTui.h"
#include "dciTui.h"
#include "tui_ioctl.h"
#include "tui-hal.h"
#include "trustedui.h"

#define TUI_ALIGN_16MB_SZ	0x1000000	/* 16MB */
#define TUI_ALIGN_32MB_SZ	0x2000000	/* 32MB */
#define TUI_TOTAL_ALLOC_SZ	0x2000000	/* 32MB */
#define TUI_BUFFER_NUM		2		/* Framebuffer and Working buffer */

/* ------------------------------------------------------------- */
/* Externs */
extern struct completion ioComp;
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
extern void tui_i2c_reset(void);
#endif
extern struct switch_dev tui_switch;

/* ------------------------------------------------------------- */
/* Globals */
const struct mc_uuid_t drUuid = DR_TUI_UUID;
struct mc_session_handle drSessionHandle = {0, 0};
tuiDciMsg_ptr pDci;
DECLARE_COMPLETION(dciComp);
uint32_t gCmdId = TLC_TUI_CMD_NONE;
tlcTuiResponse_t gUserRsp = {TLC_TUI_CMD_NONE, TLC_TUI_ERR_UNKNOWN_CMD};

/* ------------------------------------------------------------- */
/* Static */
static const uint32_t DEVICE_ID = MC_DEVICE_ID_DEFAULT;

#ifdef CONFIG_SOC_EXYNOS5433
struct sec_tui_mempool {
	void * va;
	unsigned long pa;
	size_t size;
};

static struct sec_tui_mempool g_sec_tuiMemPool;
#else
struct tui_mempool {
	void * va;
	unsigned long pa;
	size_t size;
};

struct tui_mempool g_tuiMemPool;
#endif
/* Functions */
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
static int tsp_irq_num = 718; // default value

void trustedui_set_tsp_irq(int irq_num){
	tsp_irq_num = irq_num;
	pr_info("%s called![%d]\n",__func__, irq_num);
}
#endif

/* ------------------------------------------------------------- */
static bool tlcOpenDriver(void)
{
	bool ret = false;
	enum mc_result mcRet;

	/* Allocate WSM buffer for the DCI */
	mcRet = mc_malloc_wsm(DEVICE_ID, 0, sizeof(tuiDciMsg_t),
			(uint8_t **)&pDci, 0);
	if (MC_DRV_OK != mcRet) {
		pr_debug("ERROR tlcOpenDriver: Allocation of DCI WSM failed: %d\n",
				mcRet);
		return false;
	}

	/* Clear the session handle */
	memset(&drSessionHandle, 0, sizeof(drSessionHandle));
	/* The device ID (default device is used */
	drSessionHandle.device_id = DEVICE_ID;
	/* Open session with the Driver */
	mcRet = mc_open_session(&drSessionHandle, &drUuid, (uint8_t *)pDci,
			(uint32_t)sizeof(tuiDciMsg_t));
	if (MC_DRV_OK != mcRet) {
		printk(KERN_ERR "ERROR tlcOpenDriver: Open driver session failed: %d\n",
				mcRet);
		ret = false;
	} else {
		printk(KERN_ERR "Success tlcOpenDriver: open driver session success!\n");
		ret = true;
	}

	return ret;
}


/* ------------------------------------------------------------- */
static bool tlcOpen(void)
{
	bool ret = false;
	enum mc_result mcRet;

	/* Open the tbase device */
	pr_debug("tlcOpen: Opening tbase device\n");
	mcRet = mc_open_device(DEVICE_ID);

	/* In case the device is already open, mc_open_device will return an
	 * error (MC_DRV_ERR_INVALID_OPERATION).  But in this case, we can
	 * continue, even though mc_open_device returned an error.  Stop in all
	 * other case of error
	 */
	if (MC_DRV_OK != mcRet && MC_DRV_ERR_INVALID_OPERATION != mcRet) {
		pr_debug("ERROR tlcOpen: Error %d opening device\n", mcRet);
		return false;
	}

	pr_debug("tlcOpen: Opening driver session\n");
	ret = tlcOpenDriver();

	return ret;
}


/* ------------------------------------------------------------- */
static void tlcWaitCmdFromDriver(void)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;

	/* Wait for a command from secure driver */
	ret = mc_wait_notification(&drSessionHandle, -1);
	if (MC_DRV_OK == ret)
		pr_debug("tlcWaitCmdFromDriver: Got a command\n");
	else
		pr_debug("ERROR tlcWaitCmdFromDriver: mc_wait_notification() failed: %d\n",
			ret);
}


static uint32_t sendCmdToUser(uint32_t commandId)
{
	dciReturnCode_t ret = TUI_DCI_ERR_NO_RESPONSE;

	/* Init shared variables */
	gCmdId = commandId;
	gUserRsp.id = TLC_TUI_CMD_NONE;
	gUserRsp.returnCode = TLC_TUI_ERR_UNKNOWN_CMD;

	/* Give way to ioctl thread */
	complete(&dciComp);
	pr_debug("sendCmdToUser: give way to ioctl thread\n");

	/* Wait for ioctl thread to complete */
	wait_for_completion_interruptible(&ioComp);
	pr_debug("sendCmdToUser: Got an answer from ioctl thread.\n");
	INIT_COMPLETION(ioComp);

	/* Check id of the cmd processed by ioctl thread (paranoïa) */
	if (gUserRsp.id != commandId) {
		pr_debug("sendCmdToUser ERROR: Wrong response id 0x%08x iso 0x%08x\n",
				pDci->nwdRsp.id, RSP_ID(commandId));
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
	} else {
		/* retrieve return code */
		switch (gUserRsp.returnCode) {
		case TLC_TUI_OK:
			ret = TUI_DCI_OK;
			break;
		case TLC_TUI_ERROR:
			ret = TUI_DCI_ERR_INTERNAL_ERROR;
			break;
		case TLC_TUI_ERR_UNKNOWN_CMD:
			ret = TUI_DCI_ERR_UNKNOWN_CMD;
			break;
		}
	}

	return ret;
}

phys_addr_t hal_tui_video_space_alloc(void)
{
	phys_addr_t base;
	size_t size;

	ion_exynos_contig_heap_info(ION_EXYNOS_ID_VIDEO, &base, &size);

	if (size > TUI_TOTAL_ALLOC_SZ)
#ifdef CONFIG_SOC_EXYNOS5433
		g_sec_tuiMemPool.size = TUI_TOTAL_ALLOC_SZ;
#else
		g_tuiMemPool.size = TUI_TOTAL_ALLOC_SZ;
#endif
	else
#ifdef CONFIG_SOC_EXYNOS5433
		g_sec_tuiMemPool.size = size;
#else
		g_tuiMemPool.size = size;
#endif

	/* Align base address by 16MB */

	base = ((base + (TUI_ALIGN_16MB_SZ-1))/TUI_ALIGN_16MB_SZ * TUI_ALIGN_16MB_SZ);
#ifdef CONFIG_SOC_EXYNOS5433
	g_sec_tuiMemPool.pa = base;
#else
	g_tuiMemPool.pa = base;
#endif

	return base;
}

#define FRAMEBUFFER_SIZE 0x1000000
#define FRAMEBUFFER_COUNT 2

/* ------------------------------------------------------------- */
static void tlcProcessCmd(void)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;
	uint32_t commandId = CMD_TUI_SW_NONE;

	pDci->cmdNwd.payload.allocData.numOfBuff = TUI_BUFFER_NUM;

	if (NULL == pDci) {
		pr_debug("ERROR tlcProcessCmd: DCI has not been set up properly - exiting\n");
		return;
	} else {
		commandId = pDci->cmdNwd.id;
	}

	/* Warn if previous response was not acknowledged */
	if (CMD_TUI_SW_NONE == commandId) {
		pr_debug("ERROR tlcProcessCmd: Notified without command\n");
		return;
	} else {
		if (pDci->nwdRsp.id != CMD_TUI_SW_NONE)
			pr_debug("tlcProcessCmd: Warning, previous response not ack\n");
	}

	/* Handle command */
	switch (commandId) {
	case CMD_TUI_SW_OPEN_SESSION:
		printk(KERN_ERR "tlcProcessCmd: CMD_TUI_SW_OPEN_SESSION.\n");

		/* Start android TUI activity */
		ret = sendCmdToUser(TLC_TUI_CMD_START_ACTIVITY);
		if (TUI_DCI_OK == ret) {

#ifdef CONFIG_SOC_EXYNOS5433
			/* allocate TUI frame buffer */
			ret = hal_tui_alloc(&pDci->nwdRsp.allocBuffer[MAX_DCI_BUFFER_NUMBER],
					pDci->cmdNwd.payload.allocData.allocSize,
					pDci->cmdNwd.payload.allocData.numOfBuff);
#else
			/* allocate TUI frame buffer */
			ret = hal_tui_alloc(pDci->nwdRsp.allocBuffer,
					FRAMEBUFFER_SIZE,
					FRAMEBUFFER_COUNT);
#endif

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
			printk(KERN_ERR "tlcProcessCmd: CMD_TUI_SW_OPEN_SESSION & disable_irq(%d);.\n",tsp_irq_num);
			disable_irq(tsp_irq_num);
#endif
			msleep(100); // temp code
		}
#ifdef CONFIG_SOC_EXYNOS5433
		hal_tui_video_space_alloc();

		pDci->nwdRsp.allocBuffer[0].pa = g_sec_tuiMemPool.pa;
		pDci->nwdRsp.allocBuffer[1].pa = (g_sec_tuiMemPool.pa + g_sec_tuiMemPool.size/2);

		pDci->cmdNwd.payload.allocData.allocSize = g_sec_tuiMemPool.size;
#else
		pDci->cmdNwd.payload.allocData.allocSize = g_tuiMemPool.size;
#endif
		break;

	case CMD_TUI_SW_STOP_DISPLAY:
		printk(KERN_ERR "tlcProcessCmd: CMD_TUI_SW_STOP_DISPLAY.\n");
		switch_set_state(&tui_switch, TRUSTEDUI_MODE_VIDEO_SECURED);

		/* Deactivate linux UI drivers */
		ret = hal_tui_deactivate();

		break;

	case CMD_TUI_SW_CLOSE_SESSION:
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
		printk(KERN_ERR "tlcProcessCmd: CMD_TUI_SW_CLOSE_SESSION & enable_irq(%d).\n", tsp_irq_num);
#endif

		/* Activate linux UI drivers */
		ret = hal_tui_activate();
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
		tui_i2c_reset();
		enable_irq(tsp_irq_num);
#endif
		hal_tui_free();
		switch_set_state(&tui_switch, TRUSTEDUI_MODE_OFF);

		/* Stop android TUI activity */
		ret = sendCmdToUser(TLC_TUI_CMD_STOP_ACTIVITY);
		break;

	default:
		pr_debug("ERROR tlcProcessCmd: Unknown command %d\n",
				commandId);
		break;
	}

	/* Fill in response to SWd, fill ID LAST */
	pr_debug("tlcProcessCmd: return 0x%08x to cmd 0x%08x\n",
			ret, commandId);
	pDci->nwdRsp.returnCode = ret;
	pDci->nwdRsp.id = RSP_ID(commandId);

	/* Acknowledge command */
	pDci->cmdNwd.id = CMD_TUI_SW_NONE;

	/* Notify SWd */
	pr_debug("DCI RSP NOTIFY CORE\n");
	ret = mc_notify(&drSessionHandle);
	if (MC_DRV_OK != ret)
		pr_debug("ERROR tlcProcessCmd: Notify failed: %d\n", ret);
}


/* ------------------------------------------------------------- */
static void tlcCloseDriver(void)
{
	enum mc_result ret;

	/* Close session with the Driver */
	ret = mc_close_session(&drSessionHandle);
	if (MC_DRV_OK != ret) {
		pr_debug("ERROR tlcCloseDriver: Closing driver session failed: %d\n",
				ret);
	}
}


/* ------------------------------------------------------------- */
static void tlcClose(void)
{
	enum mc_result ret;

	pr_debug("tlcClose: Closing driver session\n");
	tlcCloseDriver();

	pr_debug("tlcClose: Closing tbase\n");
	/* Close the tbase device */
	ret = mc_close_device(DEVICE_ID);
	if (MC_DRV_OK != ret) {
		pr_debug("ERROR tlcClose: Closing tbase device failed: %d\n",
			ret);
	}
}

/* ------------------------------------------------------------- */
bool tlcNotifyEvent(uint32_t eventType)
{
	bool ret = false;
	enum mc_result result;

	if (NULL == pDci) {
		pr_debug("ERROR tlcNotifyEvent: DCI has not been set up properly - exiting\n");
		return false;
	}

	/* Wait for previous notification to be acknowledged */
	while (pDci->nwdNotif != NOT_TUI_NONE) {
		pr_debug("TLC waiting for previous notification ack\n");
		usleep_range(10000, 10000);
	};

	/* Prepare notification message in DCI */
	pr_debug("tlcNotifyEvent: eventType = %d\n", eventType);
	pDci->nwdNotif = eventType;

	/* Signal the Driver */
	pr_debug("DCI EVENT NOTIFY CORE\n");
	result = mc_notify(&drSessionHandle);
	if (MC_DRV_OK != result) {
		pr_debug("ERROR tlcNotifyEvent: mcNotify failed: %d\n", result);
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}

/* ------------------------------------------------------------- */
/**
 */
int mainThread(void *uarg)
{

	pr_debug("mainThread: TlcTui start!\n");

	/* Open session on the driver */
	if (!tlcOpen()) {
		pr_debug("ERROR mainThread: open driver failed!\n");
		return 1;
	}

	/* TlcTui main thread loop */
	for (;;) {
		/* Wait for a command from the DrTui on DCI*/
		tlcWaitCmdFromDriver();
		/* Something has been received, process it. */
		tlcProcessCmd();
	}

	/* Close tlc. Note that this frees the DCI pointer.
	 * Do not use this pointer after tlcClose().*/
	tlcClose();

	return 0;
}

/** @} */
