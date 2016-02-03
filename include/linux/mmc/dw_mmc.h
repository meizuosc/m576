/*
 * Synopsys DesignWare Multimedia Card Interface driver
 *  (Based on NXP driver for lpc 31xx)
 *
 * Copyright (C) 2009 NXP Semiconductors
 * Copyright (C) 2009, 2010 Imagination Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef LINUX_MMC_DW_MMC_H
#define LINUX_MMC_DW_MMC_H

#include <linux/scatterlist.h>
#include <linux/mmc/core.h>
#include <linux/pm_qos.h>
#include <linux/wakelock.h>

#define MAX_MCI_SLOTS	2

enum dw_mci_state {
	STATE_IDLE = 0,
	STATE_SENDING_CMD,
	STATE_SENDING_DATA,
	STATE_DATA_BUSY,
	STATE_SENDING_STOP,
	STATE_DATA_ERROR,
};

enum {
	EVENT_CMD_COMPLETE = 0,
	EVENT_XFER_COMPLETE,
	EVENT_DATA_COMPLETE,
	EVENT_DATA_ERROR,
	EVENT_XFER_ERROR,
	EVENT_QUEUE_READY
};

struct mmc_data;

/**
 * struct dw_mci_slot - MMC slot state
 * @mmc: The mmc_host representing this slot.
 * @host: The MMC controller this slot is using.
 * @quirks: Slot-level quirks (DW_MCI_SLOT_QUIRK_XXX)
 * @wp_gpio: If gpio_is_valid() we'll use this to read write protect.
 * @ctype: Card type for this slot.
 * @mrq: mmc_request currently being processed or waiting to be
 *	processed, or NULL when the slot is idle.
 * @queue_node: List node for placing this node in the @queue list of
 *	&struct dw_mci.
 * @clock: Clock rate configured by set_ios(). Protected by host->lock.
 * @flags: Random state bits associated with the slot.
 * @id: Number of this slot.
 * @last_detect_state: Most recently observed card detect state.
 */
struct dw_mci_slot {
	struct mmc_host		*mmc;
	struct dw_mci		*host;

	int			quirks;
	int			wp_gpio;

	u32			ctype;

	struct mmc_request	*mrq;
	struct list_head	mrq_list;
	struct list_head	queue_node;

	unsigned int		clock;
	unsigned long		flags;
#define DW_MMC_CARD_PRESENT	0
#define DW_MMC_CARD_NEED_INIT	1
	int			id;
	int			last_detect_state;
};

/**
 * struct dw_mci_debug_data - DwMMC debugging infomation
 * @host_count: a number of all hosts
 * @info_count: a number of set of debugging information
 * @info_index: index of debugging information for each host
 * @host: pointer of each dw_mci structure
 * @debug_info: debugging information structure
 */

struct dw_mci_cmd_log {
	u64	send_time;
	u64	done_time;
	u8	cmd;
	u32	arg;
	u8	data_size;
	/* no data CMD = CD, data CMD = DTO */
	/*
	 * 0b1000 0000	: new_cmd with without send_cmd
	 * 0b0000 1000	: error occurs
	 * 0b0000 0100	: data_done : DTO(Data Transfer Over)
	 * 0b0000 0010	: resp : CD(Command Done)
	 * 0b0000 0001	: send_cmd : set 1 only start_command
	 */
	u8	seq_status;	/* 0bxxxx xxxx : error data_done resp send */
#define DW_MCI_FLAG_SEND_CMD	BIT(0)
#define DW_MCI_FLAG_CD		BIT(1)
#define DW_MCI_FLAG_DTO		BIT(2)
#define DW_MCI_FLAG_ERROR	BIT(3)
#define DW_MCI_FLAG_NEW_CMD_ERR	BIT(7)

	u16	rint_sts;	/* RINTSTS value in case of error */
	u8	status_count;	/* TBD : It can be changed */
};

enum dw_mci_req_log_state {
	STATE_REQ_START = 0,
	STATE_REQ_CMD_PROCESS,
	STATE_REQ_DATA_PROCESS,
	STATE_REQ_END,
};

struct dw_mci_req_log {
	u64				timestamp;
	u32				info0;
	u32				info1;
	u32				info2;
	u32				info3;
	u32				pending_events;
	u32				completed_events;
	enum dw_mci_state		state_cmd;
	enum dw_mci_state		state_dat;
	enum dw_mci_req_log_state	log_state;
};

#define DWMCI_LOG_MAX		0x80
#define DWMCI_REQ_LOG_MAX	0x40
struct dw_mci_debug_info {
	struct dw_mci_cmd_log		cmd_log[DWMCI_LOG_MAX];
	atomic_t			cmd_log_count;
	struct dw_mci_req_log		req_log[DWMCI_REQ_LOG_MAX];
	atomic_t			req_log_count;
	unsigned char			en_logging;
#define DW_MCI_DEBUG_ON_CMD	BIT(0)
#define DW_MCI_DEBUG_ON_REQ	BIT(1)
};

#define DWMCI_DBG_NUM_HOST	3

#define DWMCI_DBG_NUM_INFO	2			/* configurable */
#define DWMCI_DBG_MASK_INFO	(BIT(0) | BIT(2))	/* configurable */
#define DWMCI_DBG_BIT_HOST(x)	BIT(x)

struct dw_mci_debug_data {
	unsigned char			host_count;
	unsigned char			info_count;
	unsigned char			info_index[DWMCI_DBG_NUM_HOST];
	struct dw_mci			*host[DWMCI_DBG_NUM_HOST];
	struct dw_mci_debug_info	debug_info[DWMCI_DBG_NUM_INFO];
};

/**
 * struct dw_mci - MMC controller state shared between all slots
 * @lock: Spinlock protecting the queue and associated data.
 * @regs: Pointer to MMIO registers.
 * @sg: Scatterlist entry currently being processed by PIO code, if any.
 * @sg_miter: PIO mapping scatterlist iterator.
 * @cur_slot: The slot which is currently using the controller.
 * @mrq: The request currently being processed on @cur_slot,
 *	or NULL if the controller is idle.
 * @cmd: The command currently being sent to the card, or NULL.
 * @data: The data currently being transferred, or NULL if no data
 *	transfer is in progress.
 * @use_dma: Whether DMA channel is initialized or not.
 * @using_dma: Whether DMA is in use for the current transfer.
 * @sg_dma: Bus address of DMA buffer.
 * @sg_cpu: Virtual address of DMA buffer.
 * @dma_ops: Pointer to platform-specific DMA callbacks.
 * @cmd_status: Snapshot of SR taken upon completion of the current
 *	command. Only valid when EVENT_CMD_COMPLETE is pending.
 * @data_status: Snapshot of SR taken upon completion of the current
 *	data transfer. Only valid when EVENT_DATA_COMPLETE or
 *	EVENT_DATA_ERROR is pending.
 * @stop_cmdr: Value to be loaded into CMDR when the stop command is
 *	to be sent.
 * @dir_status: Direction of current transfer.
 * @tasklet: Tasklet running the request state machine.
 * @card_tasklet: Tasklet handling card detect.
 * @pending_events: Bitmask of events flagged by the interrupt handler
 *	to be processed by the tasklet.
 * @completed_events: Bitmask of events which the state machine has
 *	processed.
 * @state: Tasklet state.
 * @queue: List of slots waiting for access to the controller.
 * @bus_hz: The rate of @mck in Hz. This forms the basis for MMC bus
 *	rate and timeout calculations.
 * @current_speed: Configured rate of the controller.
 * @num_slots: Number of slots available.
 * @verid: Denote Version ID.
 * @data_offset: Set the offset of DATA register according to VERID.
 * @dev: Device associated with the MMC controller.
 * @pdata: Platform data associated with the MMC controller.
 * @drv_data: Driver specific data for identified variant of the controller
 * @priv: Implementation defined private data.
 * @biu_clk: Pointer to bus interface unit clock instance.
 * @ciu_clk: Pointer to card interface unit clock instance.
 * @slot: Slots sharing this MMC controller.
 * @fifo_depth: depth of FIFO.
 * @data_shift: log2 of FIFO item size.
 * @part_buf_start: Start index in part_buf.
 * @part_buf_count: Bytes of partial data in part_buf.
 * @part_buf: Simple buffer for partial fifo reads/writes.
 * @push_data: Pointer to FIFO push function.
 * @pull_data: Pointer to FIFO pull function.
 * @quirks: Set of quirks that apply to specific versions of the IP.
 * @irq_flags: The flags to be passed to request_irq.
 * @irq: The irq value to be passed to request_irq.
 *
 * Locking
 * =======
 *
 * @lock is a softirq-safe spinlock protecting @queue as well as
 * @cur_slot, @mrq and @state. These must always be updated
 * at the same time while holding @lock.
 *
 * The @mrq field of struct dw_mci_slot is also protected by @lock,
 * and must always be written at the same time as the slot is added to
 * @queue.
 *
 * @pending_events and @completed_events are accessed using atomic bit
 * operations, so they don't need any locking.
 *
 * None of the fields touched by the interrupt handler need any
 * locking. However, ordering is important: Before EVENT_DATA_ERROR or
 * EVENT_DATA_COMPLETE is set in @pending_events, all data-related
 * interrupts must be disabled and @data_status updated with a
 * snapshot of SR. Similarly, before EVENT_CMD_COMPLETE is set, the
 * CMDRDY interrupt must be disabled and @cmd_status updated with a
 * snapshot of SR, and before EVENT_XFER_COMPLETE can be set, the
 * bytes_xfered field of @data must be written. This is ensured by
 * using barriers.
 */
struct dw_mci {
	spinlock_t		lock;
	void __iomem		*regs;

	struct scatterlist	*sg;
	struct sg_mapping_iter	sg_miter;

	struct dw_mci_slot	*cur_slot;
	struct mmc_request	*mrq;
	struct mmc_request	*mrq_cmd;
	struct mmc_request	*mrq_dat;
	struct mmc_command	*cmd;
	struct mmc_data		*data;
	struct mmc_data		*data_cmd;
	struct mmc_data		*data_dat;
	struct mmc_command	stop;
	bool			stop_snd;
	struct workqueue_struct	*card_workqueue;

	/* DMA interface members*/
	int			use_dma;
	int			using_dma;

	dma_addr_t		sg_dma;
	void			*sg_cpu;
	const struct dw_mci_dma_ops	*dma_ops;
#ifdef CONFIG_MMC_DW_IDMAC
	unsigned int		ring_size;
#else
	struct dw_mci_dma_data	*dma_data;
#endif
	unsigned int		desc_sz;
	unsigned int		align_size;

	struct pm_qos_request	pm_qos_int;

	u32			cmd_status;
	u32			data_status;
	u32			stop_cmdr;
	u32			dir_status;
	struct tasklet_struct	tasklet;
	u32			tasklet_state;
	struct delayed_work	card_work;
	unsigned long		pending_events;
	unsigned long		completed_events;
	enum dw_mci_state	state;
	enum dw_mci_state	state_cmd;
	enum dw_mci_state	state_dat;
	struct list_head	queue;

	u32			bus_hz;
	u32			current_speed;
	u32			num_slots;
	u32			fifoth_val;
	u32			cd_rd_thr;
	u16			verid;
	u16			data_offset;
	struct device		*dev;
	struct dw_mci_board	*pdata;
	const struct dw_mci_drv_data	*drv_data;
	void			*priv;
	struct clk		*biu_clk;
	struct clk		*ciu_clk;
	struct clk		*gate_clk;
	atomic_t		biu_clk_cnt;
	atomic_t		ciu_clk_cnt;
	atomic_t		biu_en_win;
	atomic_t		ciu_en_win;
	struct dw_mci_slot	*slot[MAX_MCI_SLOTS];

	/* FIFO push and pull */
	int			fifo_depth;
	int			data_shift;
	u8			part_buf_start;
	u8			part_buf_count;
	union {
		u16		part_buf16;
		u32		part_buf32;
		u64		part_buf;
	};
	void (*push_data)(struct dw_mci *host, void *buf, int cnt);
	void (*pull_data)(struct dw_mci *host, void *buf, int cnt);

	/* Workaround flags */
	u32			quirks;

	/* S/W reset timer */
	struct timer_list       timer;

	/* Data timeout timer */
	struct timer_list       dto_timer;
	unsigned int		dto_cnt;

	struct delayed_work	tp_mon;
	u32			transferred_cnt;
	u32			cmd_cnt;
	struct pm_qos_request	pm_qos_mif;
	struct pm_qos_request	pm_qos_cluster1;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	struct pm_qos_request   pm_qos_cluster0;
#endif

	struct regulator	*vmmc;	/* Power regulator */
	struct regulator	*vqmmc;
	struct regulator	*vdd29_tf;
	unsigned long		irq_flags; /* IRQ flags */
	int			irq;

	struct wake_lock	wakelock;

	/* Save request status */
#define DW_MMC_REQ_IDLE		0
#define DW_MMC_REQ_BUSY		1
	unsigned int		req_state;
	struct dw_mci_debug_info	*debug_info;	/* debug info */
	unsigned int		cp_reseting;
	unsigned int		sdcard_cd;
};

/* DMA ops for Internal/External DMAC interface */
struct dw_mci_dma_ops {
	/* DMA Ops */
	int (*init)(struct dw_mci *host);
	void (*start)(struct dw_mci *host, unsigned int sg_len);
	void (*complete)(struct dw_mci *host);
	void (*stop)(struct dw_mci *host);
	void (*reset)(struct dw_mci *host);
	void (*cleanup)(struct dw_mci *host);
	void (*exit)(struct dw_mci *host);
};

/* IP Quirks/flags. */
/* DTO fix for command transmission with IDMAC configured */
#define DW_MCI_QUIRK_IDMAC_DTO			BIT(0)
/* delay needed between retries on some 2.11a implementations */
#define DW_MCI_QUIRK_RETRY_DELAY		BIT(1)
/* High Speed Capable - Supports HS cards (up to 50MHz) */
#define DW_MCI_QUIRK_HIGHSPEED			BIT(2)
/* Unreliable card detection */
#define DW_MCI_QUIRK_BROKEN_CARD_DETECTION	BIT(3)
/* Bypass the security management unit */
#define DW_MCI_QUIRK_BYPASS_SMU			BIT(4)
/* No detect end bit during read */
#define DW_MCI_QUIRK_NO_DETECT_EBIT		BIT(5)
/* Use fixed IO voltage */
#define DW_MMC_QUIRK_FIXED_VOLTAGE		BIT(7)
/* Use S/W data timeout */
#define DW_MMC_QUIRK_SW_DATA_TIMEOUT		BIT(8)
/* Retry CRC error */
#define DW_MMC_QUIRK_RETRY_ERROR		BIT(9)
/* Use CPU mode for tuning */
#define DW_MMC_QUIRK_USE_CPU_MODE_TUNING	BIT(10)
/* W/A to fix AXI hang-up for mismatch of sector size*/
#define DW_MMC_QUIRK_FMP_SIZE_MISMATCH		BIT(11)
/* Not allow DMA single transfer */
#define DW_MMC_QUIRK_NOT_ALLOW_SINGLE_DMA	BIT(12)
/* Use the security management unit */
#define DW_MCI_QUIRK_USE_SMU			BIT(13)
/* Enables ultra low power mode */
#define DW_MCI_QUIRK_ENABLE_ULP			BIT(14)

/* Slot level quirks */
/* This slot has no write protect */
#define DW_MCI_SLOT_QUIRK_NO_WRITE_PROTECT	BIT(0)

enum dw_mci_cd_types {
	DW_MCI_CD_INTERNAL = 1,	/* use mmc internal CD line */
	DW_MCI_CD_EXTERNAL,	/* use external callback */
	DW_MCI_CD_GPIO,		/* use external gpio pin for CD line */
	DW_MCI_CD_NONE,		/* no CD line, use polling to detect card */
	DW_MCI_CD_PERMANENT,	/* no CD line, card permanently wired to host */
};

struct dma_pdata;

struct block_settings {
	unsigned short	max_segs;	/* see blk_queue_max_segments */
	unsigned int	max_blk_size;	/* maximum size of one mmc block */
	unsigned int	max_blk_count;	/* maximum number of blocks in one req*/
	unsigned int	max_req_size;	/* maximum number of bytes in one req*/
	unsigned int	max_seg_size;	/* see blk_queue_max_segment_size */
};

struct dw_mci_mon_table {
	u32	range;
	s32	mif_lock_value;
	s32	cpu_lock_value;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
	s32     kfc_lock_value;
#endif
};

/* Board platform data */
struct dw_mci_board {
	u32 num_slots;

	u32 quirks; /* Workaround / Quirk flags */
	unsigned int bus_hz; /* Clock speed at the cclk_in pad */

	u32 caps;	/* Capabilities */
	u32 caps2;	/* More capabilities */
	u32 pm_caps;	/* PM capabilities */
	/*
	 * Override fifo depth. If 0, autodetect it from the FIFOTH register,
	 * but note that this may not be reliable after a bootloader has used
	 * it.
	 */
	unsigned int fifo_depth;

	/*
	 * clk_smpl is "clock-in sample phase shift" value that is
	 * determined by execute_tuning() support.
	 * "tuned" says we've already called execute_tuning() and don't
	 * need to retest all the possible values again.
	 */
	u8 clk_smpl;
	bool is_fine_tuned;
	bool tuned;
	bool extra_tuning;
	bool only_once_tune;
	bool register_notifier;

	/* delay in mS before detecting cards after interrupt */
	u32 detect_delay_ms;

	int (*init)(u32 slot_id, irq_handler_t , void *);
	int (*get_ro)(u32 slot_id);
	int (*get_cd)(u32 slot_id);
	int (*get_ocr)(u32 slot_id);
	int (*get_bus_wd)(u32 slot_id);
	void (*hw_reset)(u32 slot_id);

	/* INT QOS khz */
	unsigned int qos_int_level;

	unsigned char io_mode;

	u32 error_retry_cnt;
	/* cd_type: Type of Card Detection method (see cd_types enum above) */
	enum dw_mci_cd_types cd_type;

	/* Number of descriptors */
	unsigned int desc_sz;

	/* ext_cd_cleanup: Cleanup external card detect subsystem.
	* ext_cd_init: Initialize external card detect subsystem.
	*       notify_func argument is a callback to the dwmci driver
	*       that triggers the card detection event. Callback arguments:
	*       dev is pointer to platform device of the host controller,
	*       state is new state of the card (0 - removed, 1 - inserted).
	*/

	int (*ext_cd_init)(void (*notify_func)
			(struct platform_device *, int state));
	int (*ext_cd_cleanup)(void (*notify_func)
			(struct platform_device *, int state));

	/*
	 * Enable power to selected slot and set voltage to desired level.
	 * Voltage levels are specified using MMC_VDD_xxx defines defined
	 * in linux/mmc/host.h file.
	 */
	void (*setpower)(u32 slot_id, u32 volt);
	void (*exit)(u32 slot_id);
	void (*select_slot)(u32 slot_id);

	struct dw_mci_dma_ops *dma_ops;
	struct dma_pdata *data;
	struct block_settings *blk_settings;
	struct dw_mci_mon_table *tp_mon_tbl;
	unsigned int sw_timeout;
	bool use_gate_clock;
	bool use_biu_gate_clock;
	bool enable_cclk_on_suspend;
	bool on_suspend;
	unsigned int dev_drv_str;
};

#endif /* LINUX_MMC_DW_MMC_H */
