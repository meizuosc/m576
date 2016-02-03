/*
 * MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __MIPI_LLI_H
#define __MIPI_LLI_H

#include <linux/miscdevice.h>
#include <linux/clk.h>

#ifdef CONFIG_LTE_MODEM_XMM7260
#define MIPI_LLI_RESERVE_SIZE SZ_8M
#else
#define MIPI_LLI_RESERVE_SIZE SZ_4M
#endif

extern phys_addr_t lli_phys_addr;

enum mipi_lli_event {
	LLI_EVENT_SIG		= 0x01,
	LLI_EVENT_RESET		= 0x02,
	LLI_EVENT_WAITFORMOUNT	= 0x04,
	LLI_EVENT_MOUNTED	= 0x08,
	LLI_EVENT_UNMOUNTED	= 0x10,
	LLI_EVENT_SUSPEND	= 0x20,
	LLI_EVENT_RESUME	= 0x40,
};

enum mipi_lli_link_status {
	LLI_UNMOUNTED,
	LLI_MOUNTED,
	LLI_WAITFORMOUNT,
};

struct mipi_lli_ipc_handler {
	void *data;
	void (*handler)(void *, enum mipi_lli_event, u32);
};

#if defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
struct mipi_lli_clks {
	/* for gate/ungate clock */
	struct clk	*aclk_cpif_200;
	struct clk	*gate_cpifnm_200;
	struct clk	*gate_mphy_pll;
	struct clk	*gate_lli_svc_loc;
	struct clk	*gate_lli_svc_rem;
	struct clk	*gate_lli_ll_init;
	struct clk	*gate_lli_be_init;
	struct clk	*gate_lli_ll_targ;
	struct clk	*gate_lli_be_targ;
	struct clk	*gate_lli_cmn_cfg;
	struct clk	*gate_lli_tx0_cfg;
	struct clk	*gate_lli_rx0_cfg;
	struct clk	*gate_lli_tx0_symbol;
	struct clk	*gate_lli_rx0_symbol;

	/* for clk_set_parents */
	struct clk	*mout_phyclk_lli_tx0_symbol_user;
	struct clk	*phyclk_lli_tx0_symbol;
	struct clk	*mout_phyclk_lli_rx0_symbol_user;
	struct clk	*phyclk_lli_rx0_symbol;
	struct clk	*mout_mphy_pll;
	struct clk	*fout_mphy_pll;

	/* for clk_set_rate */
	struct clk	*dout_aclk_cpif_200;
	struct clk	*dout_mif_pre;
};
#else /* CONFIG_SOC_EXYNOS7420 */
struct mipi_lli_clks {
	/* for div clock */
	struct clk	*dout_aclk_fsys0_200;

	/* for gate/ungate clock */
	struct clk	*aclk_xiu_llisfrx;
	struct clk	*aclk_axius_lli_be;
	struct clk	*aclk_axius_lli_ll;
	struct clk	*aclk_axius_llisfrx_llill;
	struct clk	*aclk_axius_llisfrx_llibe;

	struct clk	*aclk_lli_svc_loc;
	struct clk	*aclk_lli_svc_rem;
	struct clk	*aclk_lli_ll_init;
	struct clk	*aclk_lli_be_init;
	struct clk	*aclk_lli_ll_targ;
	struct clk	*aclk_lli_be_targ;
	struct clk	*user_phyclk_lli_tx0_symbol;
	struct clk	*user_phyclk_lli_rx0_symbol;

	struct clk	*aclk_xiu_modemx;
	struct clk	*aclk_combo_phy_modem_pcs_pclk;
	struct clk	*sclk_phy_fsys0;
	struct clk	*sclk_combo_phy_modem_26m;
	struct clk	*pclk_async_combo_phy_modem;
	/* for clk_set_parents */
	struct clk	*mout_phyclk_lli_tx0_symbol_user;
	struct clk	*phyclk_lli_tx0_symbol;
	struct clk	*mout_phyclk_lli_rx0_symbol_user;
	struct clk	*phyclk_lli_rx0_symbol;
};
#endif

struct mipi_lli_dump {
	u32	clk[32];
	u32	lli[1024];
	u32	mphy_std[1024];
	u32	mphy_cmn[1024];
	u32	mphy_ovtm[1024];
	u32	mphy_pma[128];
};

struct mipi_lli_modem {
	char	*name;
	bool	scrambler;
	bool	automode;
};

struct mipi_lli {
	const struct lli_driver *driver;	/* hw-specific hooks */

	struct miscdevice misc_dev;

	/* Flags that need to be manipulated atomically because they can
	 * change while the host controller is running.  Always use
	 * set_bit() or clear_bit() to change their values.
	 */
	unsigned long		flags;

	struct mipi_lli_modem	modem_info;
	struct mipi_lli_clks	clks;
	struct mipi_lli_dump	dump;
	struct work_struct	wq_print_dump;
	struct work_struct	wq_sys_init;
	struct workqueue_struct *wq;

	unsigned int		irq;		/* irq allocated */
	unsigned int		irq_sig;	/* irq_sig allocated */
	void __iomem		*regs;		/* device memory/io */
	void __iomem		*remote_regs;	/* device memory/io */
	void __iomem		*sys_regs;	/* device memory/io */
	void __iomem		*pmu_regs;	/* device memory/io */

	struct mutex		lli_mutex;
	struct spinlock		lock;
	struct device		*dev;
	struct device		*mphy;

	void __iomem		*shdmem_addr;
	phys_addr_t		shdmem_size;
	phys_addr_t		phys_addr;
	atomic_t		state;
	bool			is_master;
	bool			is_suspended;
	bool			is_debug_possible;
	bool			is_clk_enabled;
	bool			is_runtime_suspended;

	enum mipi_lli_event	event;

	struct notifier_block	lpa_nb;
	struct mipi_lli_ipc_handler hd;
};

struct lli_driver {
	int	flags;

	int	(*init)(struct mipi_lli *lli);
	int	(*exit)(struct mipi_lli *lli);
	int	(*set_master)(struct mipi_lli *lli, bool is_master);
	int	(*link_startup_mount)(struct mipi_lli *lli);
	int	(*get_status)(struct mipi_lli *lli);

	int	(*send_signal)(struct mipi_lli *lli, u32 cmd);
	int	(*reset_signal)(struct mipi_lli *lli);
	int	(*read_signal)(struct mipi_lli *lli);
	int	(*loopback_test)(struct mipi_lli *lli);
	int	(*debug_info)(struct mipi_lli *lli);

	int	(*intr_enable)(struct mipi_lli *lli);
	int     (*intr_disable)(struct mipi_lli *lli);

	int	(*suspend)(struct mipi_lli *lli);
	int	(*resume)(struct mipi_lli *lli);
};

int mipi_lli_event_irq(struct mipi_lli *lli, enum mipi_lli_event event);
extern int mipi_lli_add_driver(struct device *dev,
			       const struct lli_driver *lli_driver,
			       int irq);
extern void mipi_lli_remove_driver(struct mipi_lli *lli);

extern void __iomem *mipi_lli_request_sh_region(unsigned long sh_addr,
						unsigned long size);
extern void mipi_lli_release_sh_region(void *rgn);
extern phys_addr_t mipi_lli_get_phys_base(void);
extern unsigned long mipi_lli_get_phys_size(void);
extern int mipi_lli_get_link_status(void);
extern int mipi_lli_set_link_status(int status);
extern int mipi_lli_suspended(void);
extern int mipi_lli_register_handler(void (*handler)(void *,
			enum mipi_lli_event, u32), void *data);
extern int mipi_lli_unregister_handler(void (*handler)(void *,
			enum mipi_lli_event, u32));
extern void mipi_lli_send_interrupt(u32 cmd);
extern void mipi_lli_reset_interrupt(void);
extern u32 mipi_lli_read_interrupt(void);
extern void mipi_lli_debug_info(void);
extern void mipi_lli_intr_enable(void);
extern void mipi_lli_intr_disable(void);
extern void mipi_lli_reset(void);
extern void mipi_lli_reload(void);
extern void mipi_lli_suspend(void);
extern void mipi_lli_resume(void);

#endif /* __MIPI_LLI_H */
