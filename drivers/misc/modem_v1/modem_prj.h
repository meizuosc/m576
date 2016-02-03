/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MODEM_PRJ_H__
#define __MODEM_PRJ_H__

#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/mipi-lli.h>

#include "include/modem_v1.h"
#include "include/exynos_ipc.h"
#include "modem_pm.h"

//#define DEBUG_MODEM_IF
//#define REPORT_CRASHDMP
//#define LLI_SHMEM_DUMP

#define CALLER	(__builtin_return_address(0))

#define MAX_CPINFO_SIZE		512

#define MAX_LINK_DEVTYPE	3

#define MAX_FMT_DEVS	10
#define MAX_RAW_DEVS	32
#define MAX_RFS_DEVS	10
#define MAX_BOOT_DEVS	10
#define MAX_DUMP_DEVS	10

#define MAX_IOD_RXQ_LEN	2048

#define MAX_BUFF_FULL_CNT	50000

#define IOCTL_MODEM_ON			_IO('o', 0x19)
#define IOCTL_MODEM_OFF			_IO('o', 0x20)
#define IOCTL_MODEM_RESET		_IO('o', 0x21)
#define IOCTL_MODEM_BOOT_ON		_IO('o', 0x22)
#define IOCTL_MODEM_BOOT_OFF		_IO('o', 0x23)
#define IOCTL_MODEM_BOOT_DONE		_IO('o', 0x24)

#define IOCTL_MODEM_PROTOCOL_SUSPEND	_IO('o', 0x25)
#define IOCTL_MODEM_PROTOCOL_RESUME	_IO('o', 0x26)

#define IOCTL_MODEM_STATUS		_IO('o', 0x27)
#define IOCTL_MODEM_DL_START		_IO('o', 0x28)
#define IOCTL_MODEM_FW_UPDATE		_IO('o', 0x29)

#define IOCTL_MODEM_NET_SUSPEND		_IO('o', 0x30)
#define IOCTL_MODEM_NET_RESUME		_IO('o', 0x31)

#define IOCTL_MODEM_DUMP_START		_IO('o', 0x32)
#define IOCTL_MODEM_DUMP_UPDATE		_IO('o', 0x33)
#define IOCTL_MODEM_FORCE_CRASH_EXIT	_IO('o', 0x34)
#define IOCTL_MODEM_CP_UPLOAD		_IO('o', 0x35)
#define IOCTL_MODEM_DUMP_RESET		_IO('o', 0x36)

#if defined(CONFIG_SEC_DUAL_MODEM_MODE)
#define IOCTL_MODEM_SWITCH_MODEM	_IO('o', 0x37)
#endif

#define IOCTL_MODEM_RAMDUMP_START	_IO('o', 0xCE)
#define IOCTL_MODEM_RAMDUMP_STOP	_IO('o', 0xCF)

#define IOCTL_MODEM_XMIT_BOOT		_IO('o', 0x40)
#define IOCTL_MODEM_GET_SHMEM_INFO	_IO('o', 0x41)
#define IOCTL_DPRAM_INIT_STATUS		_IO('o', 0x43)

/* ioctl command for IPC Logger */
#define IOCTL_MIF_LOG_DUMP		_IO('o', 0x51)
#define IOCTL_MIF_DPRAM_DUMP		_IO('o', 0x52)

/* ioctl command definitions. */
#define IOCTL_DPRAM_PHONE_POWON		_IO('o', 0xD0)
#define IOCTL_DPRAM_PHONEIMG_LOAD	_IO('o', 0xD1)
#define IOCTL_DPRAM_NVDATA_LOAD		_IO('o', 0xD2)
#define IOCTL_DPRAM_PHONE_BOOTSTART	_IO('o', 0xD3)

#define IOCTL_DPRAM_PHONE_UPLOAD_STEP1	_IO('o', 0xDE)
#define IOCTL_DPRAM_PHONE_UPLOAD_STEP2	_IO('o', 0xDF)

/* ioctcl command for fast CP Boot */
#define IOCTL_SEC_CP_INIT		_IO('o', 0x61)
#define IOCTL_CHECK_SECURITY	_IO('o', 0x62)
#define IOCTL_XMIT_BIN			_IO('o', 0x63)

#define CPBOOT_DIR_MASK		0xF000
#define CPBOOT_STAGE_MASK	0x0F00
#define CPBOOT_CMD_MASK		0x000F
#define CPBOOT_REQ_RESP_MASK	0x0FFF

#define CPBOOT_DIR_AP2CP	0x9000
#define CPBOOT_DIR_CP2AP	0xA000

#define CPBOOT_STAGE_SHIFT	8

#define CPBOOT_STAGE_START	0x0000
#define CPBOOT_CRC_SEND		0x000C
#define CPBOOT_STAGE_DONE	0x000D
#define CPBOOT_STAGE_FAIL	0x000F

/* modem status */
#define MODEM_OFF		0
#define MODEM_CRASHED		1
#define MODEM_RAMDUMP		2
#define MODEM_POWER_ON		3
#define MODEM_BOOTING_NORMAL	4
#define MODEM_BOOTING_RAMDUMP	5
#define MODEM_DUMPING		6
#define MODEM_RUNNING		7

#define HDLC_HEADER_MAX_SIZE	6 /* fmt 3, raw 6, rfs 6 */

#define PSD_DATA_CHID_BEGIN	0x2A
#define PSD_DATA_CHID_END	0x38

#define PS_DATA_CH_0		10
#define PS_DATA_CH_LAST		24
#define RMNET0_CH_ID		PS_DATA_CH_0

#define IP6VERSION		6

#define SOURCE_MAC_ADDR		{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}

/* IP loopback */
#define DATA_DRAIN_CHANNEL	30	/* Drain channel to drop RX packets */
#define DATA_LOOPBACK_CHANNEL	31

/* Debugging features */
#define MIF_LOG_DIR		"/data/log"
#define MIF_MAX_PATH_LEN	256
#define MIF_MAX_NAME_LEN	64
#define MIF_MAX_STR_LEN		32

#define MAX_HEX_LEN			16
#define MAX_NAME_LEN		64
#define MAX_PREFIX_LEN		128
#define MAX_STR_LEN			256

#define CP_CRASH_TAG		"CP Crash "

#define NO_WAKEUP_LOCK

static const char const *dev_format_str[] = {
	[IPC_FMT]	= "FMT",
	[IPC_RAW]	= "RAW",
	[IPC_RFS]	= "RFS",
	[IPC_MULTI_RAW]	= "MULTI_RAW",
	[IPC_BOOT]	= "BOOT",
	[IPC_DUMP]	= "RAMDUMP",
	[IPC_CMD]	= "CMD",
	[IPC_DEBUG]	= "DEBUG",
};

/**
 * get_dev_name
 * @dev: IPC device (enum dev_format)
 *
 * Returns IPC device name as a string.
 */
static const inline char *get_dev_name(unsigned int dev)
{
	if (unlikely(dev >= MAX_DEV_FORMAT))
		return "INVALID";
	else
		return dev_format_str[dev];
}

/* Does modem ctl structure will use state ? or status defined below ?*/
enum modem_state {
	STATE_OFFLINE,
	STATE_CRASH_RESET, /* silent reset */
	STATE_CRASH_EXIT, /* cp ramdump */
	STATE_BOOTING,
	STATE_ONLINE,
	STATE_NV_REBUILDING, /* <= rebuilding start */
	STATE_LOADER_DONE,
	STATE_SIM_ATTACH,
	STATE_SIM_DETACH,
#if defined(CONFIG_SEC_DUAL_MODEM_MODE)
	STATE_MODEM_SWITCH,
#endif
};

static const char const *cp_state_str[] = {
	[STATE_OFFLINE]		= "OFFLINE",
	[STATE_CRASH_RESET]	= "CRASH_RESET",
	[STATE_CRASH_EXIT]	= "CRASH_EXIT",
	[STATE_BOOTING]		= "BOOTING",
	[STATE_ONLINE]		= "ONLINE",
	[STATE_NV_REBUILDING]	= "NV_REBUILDING",
	[STATE_LOADER_DONE]	= "LOADER_DONE",
	[STATE_SIM_ATTACH]	= "SIM_ATTACH",
	[STATE_SIM_DETACH]	= "SIM_DETACH",
#if defined(CONFIG_SEC_DUAL_MODEM_MODE)
	[STATE_MODEM_SWITCH]	= "MODEM_SWITCH",
#endif
};

enum direction {
	TX = 0,
	UL = 0,
	AP2CP = 0,
	RX = 1,
	DL = 1,
	CP2AP = 1,
	TXRX = 2,
	RXTX = 2,
	MAX_DIR = 2
};

/**
  @brief      return the phone_state string
  @param state    the state of a phone (CP)
 */
static const inline char *get_cp_state_str(int state)
{
	return cp_state_str[state];
}
#if 0
/**
  @brief      return the phone_state string
 */
static const inline char *mc_state(struct modem_ctl *mc)
{
	return get_cp_state_str((int)mc->phone_state);
}
#endif
enum com_state {
	COM_NONE,
	COM_ONLINE,
	COM_HANDSHAKE,
	COM_BOOT,
	COM_CRASH,
};

enum link_mode {
	LINK_MODE_OFFLINE = 0,
	LINK_MODE_BOOT,
	LINK_MODE_IPC,
	LINK_MODE_DLOAD,
	LINK_MODE_ULOAD,
};

struct sim_state {
	bool online;	/* SIM is online? */
	bool changed;	/* online is changed? */
};

enum cp_boot_mode {
	CP_BOOT_MODE_NORMAL,
	CP_BOOT_MODE_DUMP,
	CP_BOOT_RE_INIT,
	MAX_CP_BOOT_MODE
};

struct modem_firmware {
	char *binary;
	u32 size;
};

enum cp_copy_stage {
	BOOT=0,
	MAIN=2,
	NV=3,
	NV_PROT=4
};
struct sec_info {
	enum cp_boot_mode mode;
	u32 boot_size;
	u32 main_size;
};

struct data_info {
	enum cp_copy_stage stage;
	u32 m_offset;
	u8 *buff;
	u32 total_size;
	u32 offset;
	u32 len;
};

#define SMC_ID	0x82000700
#define SMC_ID_CLK	0xC2001011

#define SSS_CLK_ENABLE		0
#define SSS_CLK_DISABLE		1

#define HDLC_START		0x7F
#define HDLC_END		0x7E
#define SIZE_OF_HDLC_START	1
#define SIZE_OF_HDLC_END	1
#define MAX_LINK_PADDING_SIZE	3

#define EXYNOS_MULTI_FRAME_ID_BITS	7
#define NUM_EXYNOS_MULTI_FRAME_IDS		(2 ^ EXYNOS_MULTI_FRAME_ID_BITS)
#define MAX_EXYNOS_MULTI_FRAME_ID		(NUM_EXYNOS_MULTI_FRAME_IDS - 1)

struct header_data {
	char hdr[HDLC_HEADER_MAX_SIZE];
	u32 len;
	u32 frag_len;
	char start; /*hdlc start header 0x7F*/
};

struct fmt_hdr {
	u16 len;
	u8 control;
} __packed;

struct raw_hdr {
	u32 len;
	u8 channel;
	u8 control;
} __packed;

struct rfs_hdr {
	u32 len;
	u8 cmd;
	u8 id;
} __packed;

struct sipc_fmt_hdr {
	u16 len;
	u8  msg_seq;
	u8  ack_seq;
	u8  main_cmd;
	u8  sub_cmd;
	u8  cmd_type;
} __packed;

#define SIPC5_START_MASK	0b11111000
#define SIPC5_CONFIG_MASK	0b00000111
#define SIPC5_EXT_FIELD_MASK	0b00000011

#define SIPC5_PADDING_EXIST	0b00000100
#define SIPC5_EXT_FIELD_EXIST	0b00000010
#define SIPC5_CTL_FIELD_EXIST	0b00000001

#define SIPC5_EXT_LENGTH_MASK	SIPC5_EXT_FIELD_EXIST
#define SIPC5_CTL_FIELD_MASK	(SIPC5_EXT_FIELD_EXIST | SIPC5_CTL_FIELD_EXIST)

#define SIPC5_MIN_HEADER_SIZE		4
#define SIPC5_HEADER_SIZE_WITH_CTL_FLD	5
#define SIPC5_HEADER_SIZE_WITH_EXT_LEN	6
#define SIPC5_MAX_HEADER_SIZE		SIPC5_HEADER_SIZE_WITH_EXT_LEN

#define SIPC5_CONFIG_SIZE	1
#define SIPC5_CH_ID_SIZE	1

#define SIPC5_CONFIG_OFFSET	0
#define SIPC5_CH_ID_OFFSET	1
#define SIPC5_LEN_OFFSET	2
#define SIPC5_CTL_OFFSET	4

#define SIPC5_CH_ID_PDP_0	10
#define SIPC5_CH_ID_PDP_LAST	24
#define SIPC5_CH_ID_BOOT0	215
#define SIPC5_CH_ID_DUMP0	225
#define SIPC5_CH_ID_FMT_0	235
#define SIPC5_CH_ID_RFS_0	245
#define SIPC5_CH_ID_MAX		255

#define SIPC5_CH_ID_FLOW_CTRL	255
#define FLOW_CTRL_SUSPEND	((u8)(0xCA))
#define FLOW_CTRL_RESUME	((u8)(0xCB))

/* If iod->id is 0, do not need to store to `iodevs_tree_fmt' in SIPC4 */
#define sipc4_is_not_reserved_channel(ch) ((ch) != 0)

/* Channel 0, 5, 6, 27, 255 are reserved in SIPC5.
 * see SIPC5 spec: 2.2.2 Channel Identification (Ch ID) Field.
 * They do not need to store in `iodevs_tree_fmt'
 */
#define exynos_is_not_reserved_channel(ch) \
	((ch) != 0 && (ch) != 27 && (ch) != 255)

struct sipc5_link_hdr {
	u8 cfg;
	u8 ch;
	u16 len;
	union {
		u8 ctl;
		u16 ext_len;
	};
} __packed;

struct sipc5_frame_data {
	/* Frame length calculated from the length fields */
	unsigned len;

	/* The length of link layer header */
	unsigned hdr_len;

	/* The length of received header */
	unsigned hdr_rcvd;

	/* The length of link layer payload */
	unsigned pay_len;

	/* The length of received data */
	unsigned pay_rcvd;

	/* The length of link layer padding */
	unsigned pad_len;

	/* The length of received padding */
	unsigned pad_rcvd;

	/* Header buffer */
	u8 hdr[SIPC5_MAX_HEADER_SIZE];
};

struct vnet {
	struct io_device *iod;
};

/* for fragmented data from link devices */
struct fragmented_data {
	struct sk_buff *skb_recv;
	struct header_data h_data;
	struct exynos_frame_data f_data;
	/* page alloc fail retry*/
	unsigned realloc_offset;
};
#define fragdata(iod, ld) (&(iod)->fragments[(ld)->link_type])

/** struct skbuff_priv - private data of struct sk_buff
 * this is matched to char cb[48] of struct sk_buff
 */
struct skbuff_private {
	struct io_device *iod;
	struct link_device *ld;
	struct io_device *real_iod; /* for rx multipdp */

	/* for time-stamping */
	struct timespec ts;

	u32 lnk_hdr:1,
		reserved:15,
		exynos_ch:8,
		frm_ctrl:8;

	/* for indicating that thers is only one IPC frame in an skb */
	bool single_frame;
} __packed;

static inline struct skbuff_private *skbpriv(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct skbuff_private) > sizeof(skb->cb));
	return (struct skbuff_private *)&skb->cb;
}

enum iod_rx_state {
	IOD_RX_ON_STANDBY = 0,
	IOD_RX_HEADER,
	IOD_RX_PAYLOAD,
	IOD_RX_PADDING,
	MAX_IOD_RX_STATE
};

static const char const *rx_state_str[] = {
	[IOD_RX_ON_STANDBY]	= "RX_ON_STANDBY",
	[IOD_RX_HEADER]		= "RX_HEADER",
	[IOD_RX_PAYLOAD]	= "RX_PAYLOAD",
	[IOD_RX_PADDING]	= "RX_PADDING",
};

/**
 * get_dev_name
 * @dev: IPC device (enum dev_format)
 *
 * Returns IPC device name as a string.
 */
static const inline char *get_rx_state_str(unsigned int state)
{
	if (unlikely(state >= MAX_IOD_RX_STATE))
		return "INVALID_STATE";
	else
		return rx_state_str[state];
}

struct meminfo {
	unsigned int base_addr;
	unsigned int size;
};


struct io_device {
	/* rb_tree node for an io device */
	struct rb_node node_chan;
	struct rb_node node_fmt;

	/* Name of the IO device */
	char *name;

	/* Reference count */
	atomic_t opened;

	/* Wait queue for the IO device */
	wait_queue_head_t wq;

	/* Misc and net device structures for the IO device */
	struct miscdevice  miscdev;
	struct net_device *ndev;

	/* ID and Format for channel on the link */
	unsigned id;
	enum modem_link link_types;
	enum dev_format format;
	enum modem_io io_typ;
	enum modem_network net_typ;

	/* Attributes of an IO device */
	u32 attrs;

	/* The name of the application that will use this IO device */
	char *app;

	/* Whether or not handover among 2+ link devices */
	bool use_handover;

	/* SIPC version */
	enum sipc_ver ipc_version;

	bool link_header;

	/* Rx queue of sk_buff */
	struct sk_buff_head sk_rx_q;

	/* RX state used in RX FSM */
	enum iod_rx_state curr_rx_state;
	enum iod_rx_state next_rx_state;

	/*
	** work for each io device, when delayed work needed
	** use this for private io device rx action
	*/
	struct delayed_work rx_work;

	struct fragmented_data fragments[LINKDEV_MAX];

	/* for keeping multi-frame packets temporarily */
	struct sk_buff_head sk_multi_q[NUM_EXYNOS_MULTI_FRAME_IDS];
	struct sk_buff *skb[128];

	/* called from linkdevice when a packet arrives for this iodevice */
	int (*recv)(struct io_device *iod, struct link_device *ld,
					const char *data, unsigned int len);
	int (*recv_skb)(struct io_device *iod, struct link_device *ld,
					struct sk_buff *skb);

	int (*recv_skb_single)(struct io_device *iod, struct link_device *ld,
					struct sk_buff *skb);

	/* inform the IO device that the modem is now online or offline or
	 * crashing or whatever...
	 */
	void (*modem_state_changed)(struct io_device *iod, enum modem_state);

	/* inform the IO device that the SIM is not inserting or removing */
	void (*sim_state_changed)(struct io_device *iod, bool sim_online);

	struct modem_ctl *mc;
	struct modem_shared *msd;

	struct wake_lock wakelock;
	long waketime;

	struct exynos_seq_num seq_num;

	/* DO NOT use __current_link directly
	 * you MUST use skbpriv(skb)->ld in mc, link, etc..
	 */
	struct link_device *__current_link;
};
#define to_io_device(misc) container_of(misc, struct io_device, miscdev)

/* get_current_link, set_current_link don't need to use locks.
 * In ARM, set_current_link and get_current_link are compiled to
 * each one instruction (str, ldr) as atomic_set, atomic_read.
 * And, the order of set_current_link and get_current_link is not important.
 */
#define get_current_link(iod) ((iod)->__current_link)
#define set_current_link(iod, ld) ((iod)->__current_link = (ld))

struct link_device {
	struct list_head  list;
	char *name;

	enum modem_link link_type;
	unsigned aligned;

	/* SIPC version */
	enum sipc_ver ipc_version;

	/* Maximum IPC device = the last IPC device (e.g. IPC_RFS) + 1 */
	int max_ipc_dev;

	/* Modem data */
	struct modem_data *mdm_data;

	/* Power management */
	struct modem_link_pm pm;

	/* Modem control */
	struct modem_ctl *mc;

	/* Modem shared data */
	struct modem_shared *msd;

	/* Operation mode of the link device */
	enum link_mode mode;

	/* completion for waiting for link initialization */
	struct completion init_cmpl;

	/* completion for waiting for PIF initialization in a CP */
	struct completion pif_cmpl;

	struct io_device *fmt_iods[4];

	/* TX queue of socket buffers */
	struct sk_buff_head sk_fmt_tx_q;
	struct sk_buff_head sk_raw_tx_q;
	struct sk_buff_head sk_rfs_tx_q;

	struct sk_buff_head *skb_txq[MAX_IPC_DEV];

	/* RX queue of socket buffers */
	struct sk_buff_head sk_fmt_rx_q;
	struct sk_buff_head sk_raw_rx_q;
	struct sk_buff_head sk_rfs_rx_q;

	struct sk_buff_head *skb_rxq[MAX_IPC_DEV];

	bool raw_tx_suspended; /* for misc dev */
	struct completion raw_tx_resumed_by_cp;

	/**
	 * This flag is for TX flow control on network interface.
	 * This must be set and clear only by a flow control command from CP.
	 */
	bool suspend_netif_tx;

	struct workqueue_struct *tx_wq;
	struct work_struct tx_work;
	struct delayed_work tx_delayed_work;

	struct delayed_work *tx_dwork[MAX_IPC_DEV];
	struct delayed_work fmt_tx_dwork;
	struct delayed_work raw_tx_dwork;
	struct delayed_work rfs_tx_dwork;

	struct workqueue_struct *rx_wq;
	struct work_struct rx_work;
	struct delayed_work rx_delayed_work;

	enum com_state com_state;

	/* init communication - setting link driver */
	int (*init_comm)(struct link_device *ld, struct io_device *iod);

	/* terminate communication */
	void (*terminate_comm)(struct link_device *ld, struct io_device *iod);

	/* called by an io_device when it has a packet to send over link
	 * - the io device is passed so the link device can look at id and
	 *   format fields to determine how to route/format the packet
	 */
	int (*send)(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb);

	/* method for CP fast booting */
	int (*xmit_bin)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* method for CP booting */
	int (*xmit_boot)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* methods for CP firmware upgrade */
	int (*dload_start)(struct link_device *ld, struct io_device *iod);
	int (*firm_update)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* methods for CP crash dump */
	int (*force_dump)(struct link_device *ld, struct io_device *iod);
	int (*dump_start)(struct link_device *ld, struct io_device *iod);
	int (*dump_update)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);
	int (*dump_finish)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* IOCTL extension */
	int (*ioctl)(struct link_device *ld, struct io_device *iod,
			unsigned cmd, unsigned long arg);

	/* Send thermal for CP TMU */
	void (*send_tmu)(struct link_device *ld, struct io_device *iod,
			int arg);

	/* for cp security */
	int (*check_security)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);
	int (*sec_init)(struct link_device *ld, struct io_device *iod,
			unsigned long arg);

	/* ready a link_device instance */
	void (*ready)(struct link_device *ld);

	/* reset physical link */
	void (*reset)(struct link_device *ld);

	/* reload physical link set-up */
	void (*reload)(struct link_device *ld);

	/* turn off physical link */
	void (*off)(struct link_device *ld);

	/* Whether the physical link is unmounted or not */
	bool (*unmounted)(struct link_device *ld);

	/* Whether the physical link is suspended or not */
	bool (*suspended)(struct link_device *ld);

};

/** rx_alloc_skb - allocate an skbuff and set skb's iod, ld
 * @length:	length to allocate
 * @iod:	struct io_device *
 * @ld:		struct link_device *
 *
 * %NULL is returned if there is no free memory.
 */
static inline struct sk_buff *rx_alloc_skb(unsigned int length,
		struct io_device *iod, struct link_device *ld)
{
	struct sk_buff *skb;

	if (iod->format == IPC_MULTI_RAW || iod->format == IPC_RAW)
		skb = dev_alloc_skb(length);
	else
		skb = alloc_skb(length, GFP_ATOMIC);

	if (likely(skb)) {
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;
	}
	return skb;
}

struct modemctl_ops {
	int (*modem_on)(struct modem_ctl *);
	int (*modem_off)(struct modem_ctl *);
	int (*modem_reset)(struct modem_ctl *);
	int (*modem_boot_on)(struct modem_ctl *);
	int (*modem_boot_off)(struct modem_ctl *);
	int (*modem_boot_done)(struct modem_ctl *);
	int (*modem_force_crash_exit)(struct modem_ctl *);
	int (*modem_dump_reset)(struct modem_ctl *);
	int (*modem_dump_start)(struct modem_ctl *);
#if defined(CONFIG_LTE_MODEM_S5E4270) || defined(CONFIG_LTE_MODEM_S5E7580)
	int (*suspend_modem_ctrl)(struct modem_ctl *);
	int (*resume_modem_ctrl)(struct modem_ctl *);
	int (*modem_get_meminfo)(struct modem_ctl *, unsigned long arg);
#endif
};

/* for IPC Logger */
struct mif_storage {
	char *addr;
	unsigned int cnt;
};

/* modem_shared - shared data for all io/link devices and a modem ctl
 * msd : mc : iod : ld = 1 : 1 : M : N
 */
struct modem_shared {
	/* list of link devices */
	struct list_head link_dev_list;

	/* rb_tree root of io devices. */
	struct rb_root iodevs_tree_chan; /* group by channel */
	struct rb_root iodevs_tree_fmt; /* group by dev_format */

	/* for IPC Logger */
	struct mif_storage storage;
	spinlock_t lock;

	/* CP crash information */
	char cp_crash_info[530];

	/* loopbacked IP address
	 * default is 0.0.0.0 (disabled)
	 * after you setted this, you can use IP packet loopback using this IP.
	 * exam: echo 1.2.3.4 > /sys/devices/virtual/misc/umts_multipdp/loopback
	 */
	__be32 loopback_ipaddr;
};

struct modem_ctl {
	struct device *dev;
	char *name;
	struct modem_data *mdm_data;

	struct modem_shared *msd;

	enum modem_state phone_state;
	struct sim_state sim_state;

	unsigned gpio_cp_on;
	unsigned gpio_cp_off;
	unsigned gpio_reset_req_n;
	unsigned gpio_cp_reset;

	unsigned gpio_dump_noti;

	/* for broadcasting AP's PM state (active or sleep) */
	unsigned gpio_pda_active;
	unsigned mbx_pda_active;
	int int_pda_active;

	/* for checking aliveness of CP */
	unsigned gpio_phone_active;
	unsigned mbx_phone_active;
	int irq_phone_active;
	struct modem_irq irq_cp_wdt;		/* watchdog timer */
	struct modem_irq irq_cp_fail;

	/* for AP-CP power management (PM) handshaking */
	unsigned gpio_ap_wakeup;
	unsigned mbx_ap_wakeup;
	int irq_ap_wakeup;

	unsigned gpio_ap_status;
	unsigned mbx_ap_status;
	int int_ap_status;

	unsigned gpio_cp_wakeup;
	unsigned mbx_cp_wakeup;
	int int_cp_wakeup;

	unsigned gpio_cp_status;
	unsigned mbx_cp_status;
	int irq_cp_status;

	/* for performance tuning */
	unsigned gpio_perf_req;
	unsigned mbx_perf_req;
	int irq_perf_req;

	/* for tmu request */
	unsigned gpio_tmu_req;
	unsigned mbx_tmu_req;
	int irq_tmu_req;

	/* for system revision information */
	unsigned mbx_sys_rev;
	unsigned mbx_pmic_rev;
	unsigned mbx_pkg_id;
	unsigned mbx_lock_value;

	/* for USB/HSIC PM */
	unsigned gpio_host_wakeup;
	int irq_host_wakeup;
	unsigned gpio_host_active;
	unsigned gpio_slave_wakeup;

	unsigned gpio_cp_dump_int;
	unsigned gpio_ap_dump_int;
	unsigned gpio_flm_uart_sel;
	unsigned gpio_cp_warm_reset;
#if defined(CONFIG_MACH_M0_CTC)
	unsigned gpio_flm_uart_sel_rev06;
#endif

	unsigned gpio_sim_detect;
	int irq_sim_detect;

#ifdef CONFIG_LINK_DEVICE_SHMEM
	struct modem_pmu *pmu;
#endif

#ifdef CONFIG_LINK_DEVICE_PLD
	unsigned gpio_fpga_cs_n;
#endif

#if defined(CONFIG_MACH_U1_KOR_LGT)
	unsigned gpio_cp_reset_msm;
	unsigned gpio_boot_sw_sel;
	void (*vbus_on)(void);
	void (*vbus_off)(void);
	bool usb_boot;
#endif

#ifdef CONFIG_TDSCDMA_MODEM_SPRD8803
	unsigned gpio_ap_cp_int1;
	unsigned gpio_ap_cp_int2;
#endif

	unsigned int hw_revision;
	unsigned int package_id;
	unsigned int lock_value;

#ifdef CONFIG_SEC_DUAL_MODEM_MODE
	unsigned gpio_sim_io_sel;
	unsigned gpio_cp_ctrl1;
	unsigned gpio_cp_ctrl2;
#endif

	struct work_struct pm_qos_work;

	struct work_struct pm_tmu_work;

	/* completion for waiting for CP power-off */
	struct completion off_cmpl;

	/* Switch with 2 links in a modem */
	unsigned gpio_link_switch;

	const struct attribute_group *group;

	struct delayed_work dwork;
	struct work_struct work;

	struct modemctl_ops ops;
	struct io_device *iod;
	struct io_device *bootd;

	/* Wakelock for modem_ctl */
	struct wake_lock mc_wake_lock;

	/* spin lock for each modem_ctl instance */
	spinlock_t lock;

	struct notifier_block event_nfb;

	void (*gpio_revers_bias_clear)(void);
	void (*gpio_revers_bias_restore)(void);

	bool need_switch_to_usb;
	bool sim_polarity;

	bool sim_shutdown_req;
	void (*modem_complete)(struct modem_ctl *mc);
};

static inline bool cp_offline(struct modem_ctl *mc)
{
	if (!mc)
		return true;
	return (mc->phone_state == STATE_OFFLINE);
}

static inline bool cp_online(struct modem_ctl *mc)
{
	if (!mc)
		return false;
	return (mc->phone_state == STATE_ONLINE);
}

static inline bool cp_booting(struct modem_ctl *mc)
{
	if (!mc)
		return false;
	return (mc->phone_state == STATE_BOOTING);
}

static inline bool cp_crashed(struct modem_ctl *mc)
{
	if (!mc)
		return false;
	return (mc->phone_state == STATE_CRASH_EXIT);
}

static inline bool rx_possible(struct modem_ctl *mc)
{
	if (likely(cp_online(mc)))
		return true;

	if (cp_booting(mc) || cp_crashed(mc))
		return true;

	return false;
}

unsigned long shm_get_phys_base(void);
unsigned long shm_get_phys_size(void);
unsigned long shm_get_ipc_rgn_size(void);
unsigned long shm_get_ipc_rgn_offset(void);

int sipc4_init_io_device(struct io_device *iod);
int sipc5_init_io_device(struct io_device *iod);
int exynos_init_io_device(struct io_device *iod);

#ifdef TEST_WITHOUT_CP
ssize_t misc_write(struct file *filp, const char __user *data,
			size_t count, loff_t *fpos);
#endif
bool sipc5_start_valid(u8 *frm);
bool sipc5_padding_exist(u8 *frm);
bool sipc5_multi_frame(u8 *frm);
bool sipc5_ext_len(u8 *frm);
int sipc5_get_hdr_len(u8 *frm);
u8 sipc5_get_ch_id(u8 *frm);
u8 sipc5_get_ctrl_field(u8 *frm);
int sipc5_get_frame_len(u8 *frm);
int sipc5_calc_padding_size(int len);
int sipc5_get_total_len(u8 *frm);

u8 sipc5_build_config(struct io_device *iod, struct link_device *ld, u32 count);
void sipc5_build_header(struct io_device *iod, struct link_device *ld,
			u8 *buff, u8 cfg, u8 ctrl, u32 count);

#if defined(CONFIG_TDSCDMA_MODEM_SPRD8803) && defined(CONFIG_LINK_DEVICE_SPI)
extern int spi_sema_init(void);
extern int sprd_boot_done;
#endif

#define STD_UDL_STEP_MASK	0x0000000F
#define STD_UDL_SEND		0x1
#define STD_UDL_CRC		0xC

struct std_dload_info {
	u32 size;
	u32 mtu;
	u32 num_frames;
} __packed;

u32 std_udl_get_cmd(u8 *frm);
bool std_udl_with_payload(u32 cmd);

void print_status(struct modem_ctl *mc);
void check_lli_irq(struct modem_link_pm *pm, enum mipi_lli_event event);
int init_link_device_pm(struct link_device *ld,	struct modem_link_pm *pm,
			struct link_pm_svc *pm_svc,	void (*fail_fn)(struct modem_link_pm *),
			void (*cp_fail_fn)(struct modem_link_pm *));
#endif
