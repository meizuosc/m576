/*
 * Copyright (C) 2014 Linaro Ltd.
 * Author: Jassi Brar <jassisinghbrar@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAILBOX_CLIENT_H
#define __MAILBOX_CLIENT_H

#include <linux/mailbox.h>

#define MBOX_TX_QUEUE_LEN	20

struct mbox_chan;

/**
 * struct mbox_client - User of a mailbox
 * @chan_name:		The "controller:channel" this client wants
 * @rx_callback:	Atomic callback to provide client the data received
 * @tx_done:		Atomic callback to tell client of data transmission
 * @tx_block:		If the mbox_send_message should block until data is
 *			transmitted.
 * @tx_tout:		Max block period in ms before TX is assumed failure
 * @knows_txdone:	if the client could run the TX state machine. Usually
 *			if the client receives some ACK packet for transmission.
 *			Unused if the controller already has TX_Done/RTR IRQ.
 * @link_data:		Optional controller specific parameters during channel
 *			request.
 */
struct mbox_client {
	char *chan_name;
	void (*rx_callback)(struct mbox_client *cl, void *mssg);
	void (*tx_done)(struct mbox_client *cl, void *mssg, enum mbox_result r);
	bool tx_block;
	unsigned long tx_tout;
	bool knows_txdone;
	void *link_data;
};

struct mbox_chan {
	char name[16]; /* Physical link's name */
	struct mbox_con *con; /* Parent Controller */
	unsigned txdone_method;

	/* Physical links */
	struct mbox_link *link;
	struct mbox_link_ops *link_ops;

	/* client */
	struct mbox_client *cl;
	struct completion tx_complete;

	void *active_req;
	unsigned msg_count, msg_free;
	void *msg_data[MBOX_TX_QUEUE_LEN];
	/* Access to the channel */
	spinlock_t lock;
	/* Hook to add to the controller's list of channels */
	struct list_head node;
	/* Notifier to all clients waiting on aquiring this channel */
	struct blocking_notifier_head avail;
} __aligned(32);

/* Internal representation of a controller */
struct mbox_con {
	struct device *dev;
	char name[16]; /* controller_name */
	struct list_head channels;
	/*
	 * If the controller supports only TXDONE_BY_POLL,
	 * this timer polls all the links for txdone.
	 */
	struct timer_list poll;
	unsigned period;
	/* Hook to add to the global controller list */
	struct list_head node;
} __aligned(32);

struct mbox_chan *mbox_request_channel(struct mbox_client *cl);
int mbox_send_message(struct mbox_chan *chan, void *mssg);
void mbox_client_txdone(struct mbox_chan *chan, enum mbox_result r);
void mbox_free_channel(struct mbox_chan *chan);
int mbox_notify_chan_register(const char *name, struct notifier_block *nb);
void mbox_notify_chan_unregister(const char *name, struct notifier_block *nb);

#endif /* __MAILBOX_CLIENT_H */
