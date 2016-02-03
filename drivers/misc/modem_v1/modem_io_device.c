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

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/skbuff.h>

#include "modem_prj.h"
#include "modem_utils.h"


static u16 exynos_build_fr_config(struct io_device *iod, struct link_device *ld,
		unsigned int count);

static void exynos_build_header(struct io_device *iod, struct link_device *ld,
		u8 *buff, u16 cfg, u8 ctl, size_t count);

static ssize_t show_waketime(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int msec;
	char *p = buf;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = container_of(miscdev, struct io_device,
			miscdev);

	msec = jiffies_to_msecs(iod->waketime);

	p += snprintf(buf, PAGE_SIZE, "raw waketime : %ums\n", msec);

	return p - buf;
}

static ssize_t store_waketime(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long msec;
	int ret;
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct io_device *iod = container_of(miscdev, struct io_device,
			miscdev);

	ret = strict_strtoul(buf, 10, &msec);
	if (ret)
		return count;

	iod->waketime = msecs_to_jiffies(msec);

	return count;
}

static struct device_attribute attr_waketime =
	__ATTR(waketime, S_IRUGO | S_IWUSR, show_waketime, store_waketime);

static ssize_t show_loopback(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;
	unsigned char *ip = (unsigned char *)&msd->loopback_ipaddr;
	char *p = buf;

	p += snprintf(buf, PAGE_SIZE, "%u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);

	return p - buf;
}

static ssize_t store_loopback(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;

	msd->loopback_ipaddr = ipv4str_to_be32(buf, count);

	return count;
}

static struct device_attribute attr_loopback =
	__ATTR(loopback, S_IRUGO | S_IWUSR, show_loopback, store_loopback);

static void iodev_showtxlink(struct io_device *iod, void *args)
{
	char **p = (char **)args;
	struct link_device *ld = get_current_link(iod);

	if (iod->io_typ == IODEV_NET && IS_CONNECTED(iod, ld))
		*p += snprintf(*p, PAGE_SIZE, "%s<->%s\n", iod->name, ld->name);
}

static ssize_t show_txlink(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct modem_shared *msd =
		container_of(miscdev, struct io_device, miscdev)->msd;
	char *p = buf;

	iodevs_for_each(msd, iodev_showtxlink, &p);

	return p - buf;
}

static ssize_t store_txlink(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	/* don't change without gpio dynamic switching */
	return -EINVAL;
}

static struct device_attribute attr_txlink =
	__ATTR(txlink, S_IRUGO | S_IWUSR, show_txlink, store_txlink);

static inline void iodev_lock_wlock(struct io_device *iod)
{
	if (iod->waketime > 0 && !wake_lock_active(&iod->wakelock))
		wake_lock_timeout(&iod->wakelock, iod->waketime);
}
static int netif_flow_ctrl(struct link_device *ld, struct sk_buff *skb)
{
	u8 cmd = skb->data[0];

	if (cmd == FLOW_CTRL_SUSPEND) {
		if (ld->suspend_netif_tx)
			goto exit;
		ld->suspend_netif_tx = true;
		mif_netif_stop(ld);
		mif_info("%s: FLOW_CTRL_SUSPEND\n", ld->name);
	} else if (cmd == FLOW_CTRL_RESUME) {
		if (!ld->suspend_netif_tx)
			goto exit;
		ld->suspend_netif_tx = false;
		mif_netif_wake(ld);
		mif_info("%s: FLOW_CTRL_RESUME\n", ld->name);
	} else {
		mif_info("%s: ERR! invalid command %02X\n", ld->name, cmd);
	}

exit:
	dev_kfree_skb_any(skb);
	return 0;
}

static inline int queue_skb_to_iod(struct sk_buff *skb, struct io_device *iod)
{
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	static u32 buf_full_err = 0;

	skb_queue_tail(rxq, skb);

	if (iod->format < IPC_MULTI_RAW && rxq->qlen > MAX_IOD_RXQ_LEN) {
		struct sk_buff *victim;

		if ((buf_full_err++ % MAX_BUFF_FULL_CNT) == 0)
			mif_err("%s: %s application may be dead (rxq->qlen %d > %d)\n",
				iod->name, iod->app ? iod->app : "corresponding",
				rxq->qlen, MAX_IOD_RXQ_LEN);
		victim = skb_dequeue(rxq);
		if (victim)
			dev_kfree_skb_any(victim);
		return -ENOSPC;
	} else {
		buf_full_err = 0;
		mif_debug("%s: rxq->qlen = %d\n", iod->name, rxq->qlen);
		wake_up(&iod->wq);
		return 0;
	}
}

static int rx_drain(struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
	return 0;
}

static int rx_loopback(struct sk_buff *skb)
{
	struct io_device *iod = skbpriv(skb)->iod;
	struct link_device *ld = skbpriv(skb)->ld;
	int ret;

	ret = ld->send(ld, iod, skb);
	if (ret < 0) {
		mif_err("%s->%s: ERR! ld->send fail (err %d)\n",
			iod->name, ld->name, ret);
	}

	return ret;
}

static int gather_multi_frame(struct exynos_link_header *hdr,
				struct sk_buff *skb)
{
	struct frag_config ctrl = hdr->cfg;
	struct io_device *iod = skbpriv(skb)->iod;
	struct modem_ctl *mc = iod->mc;
	struct sk_buff_head *multi_q = &iod->sk_multi_q[hdr->ch_id];
	int len = skb->len;

#ifdef DEBUG_MODEM_IF_LINK_RX
	/* If there has been no multiple frame with this ID, ... */
	if (skb_queue_empty(multi_q)) {
		mif_err("%s<-%s: start of multi-frame (pkt_index:%d fr_index:%d len:%d)\n",
			iod->name, mc->name, ctrl.packet_index, ctrl.frame_index, hdr->len);
	}
#endif
	skb_queue_tail(multi_q, skb);

	if (!ctrl.frame_last) {
		/* The last frame has not arrived yet. */
		mif_err("%s<-%s: recv multi-frame (CH_ID:%d rcvd:%d)\n",
			iod->name, mc->name, hdr->ch_id, skb->len);
	} else {
		struct sk_buff_head *rxq = &iod->sk_rx_q;
		unsigned long flags;

		/* It is the last frame because the "more" bit is 0. */
		mif_err("%s<-%s: end of multi-frame (CH_ID:%d rcvd:%d)\n",
			iod->name, mc->name, hdr->ch_id, skb->len);

		spin_lock_irqsave(&rxq->lock, flags);
		skb_queue_splice_tail_init(multi_q, rxq);
		spin_unlock_irqrestore(&rxq->lock, flags);

		wake_up(&iod->wq);
	}

	return len;
}

static inline int rx_frame_with_link_header(struct sk_buff *skb)
{
	struct exynos_link_header *hdr;
	bool single_frame = exynos_single_frame(skb->data);

	/* Remove EXYNOS link header */
	hdr = (struct exynos_link_header *)skb->data;
	skb_pull(skb, EXYNOS_HEADER_SIZE);

	if (single_frame)
		return queue_skb_to_iod(skb, skbpriv(skb)->iod);
	else
		return gather_multi_frame(hdr, skb);
}

static int rx_fmt_ipc(struct sk_buff *skb)
{
	if (skbpriv(skb)->lnk_hdr)
		return rx_frame_with_link_header(skb);
	else
		return queue_skb_to_iod(skb, skbpriv(skb)->iod);
}

static int rx_raw_misc(struct sk_buff *skb)
{
	struct io_device *iod = skbpriv(skb)->iod;

	if (skbpriv(skb)->lnk_hdr) {
		/* Remove the EXYNOS link header */
		skb_pull(skb, EXYNOS_HEADER_SIZE);
	}

	queue_skb_to_iod(skb, iod);
	wake_up(&iod->wq);

	return 0;
}

static int rx_multi_pdp(struct sk_buff *skb)
{
	struct link_device *ld = skbpriv(skb)->ld;
	struct io_device *iod = skbpriv(skb)->iod;
	struct net_device *ndev;
	struct iphdr *iphdr;
	int ret;

	ndev = iod->ndev;
	if (!ndev) {
		mif_info("%s: ERR! no iod->ndev\n", iod->name);
		return -ENODEV;
	}

	/* Remove the EXYNOS link header */
	skb_pull(skb, EXYNOS_HEADER_SIZE);

	skb->dev = ndev;
	ndev->stats.rx_packets++;
	ndev->stats.rx_bytes += skb->len;

	/* check the version of IP */
	iphdr = (struct iphdr *)skb->data;
	if (iphdr->version == IP6VERSION)
		skb->protocol = htons(ETH_P_IPV6);
	else
		skb->protocol = htons(ETH_P_IP);

	if (iod->use_handover) {
		struct ethhdr *ehdr;
		const char source[ETH_ALEN] = SOURCE_MAC_ADDR;

		ehdr = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
		memcpy(ehdr->h_dest, ndev->dev_addr, ETH_ALEN);
		memcpy(ehdr->h_source, source, ETH_ALEN);
		ehdr->h_proto = skb->protocol;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb_reset_mac_header(skb);
		skb_pull(skb, sizeof(struct ethhdr));
	}

	if (in_interrupt())
		ret = netif_rx(skb);
	else
		ret = netif_rx_ni(skb);

	if (ret != NET_RX_SUCCESS) {
		mif_err("%s->%s: ERR! netif_rx fail (err %d)\n",
			ld->name, iod->name, ret);
	}

	return ret;
}

static int rx_demux(struct link_device *ld, struct sk_buff *skb)
{
	struct io_device *iod;
	u8 ch = skbpriv(skb)->exynos_ch;

	if (unlikely(ch == 0)) {
		mif_err("%s: ERR! invalid ch# %d\n", ld->name, ch);
		return -ENODEV;
	}

	if (unlikely(ch == EXYNOS_CH_ID_FLOW_CTRL))
		return netif_flow_ctrl(ld, skb);

	/* IP loopback */
	if (ch == DATA_LOOPBACK_CHANNEL && ld->msd->loopback_ipaddr)
		ch = EXYNOS_CH_ID_PDP_0;

	iod = link_get_iod_with_channel(ld, ch);
	if (unlikely(!iod)) {
		mif_err("%s: ERR! no iod with ch# %d\n", ld->name, ch);
		return -ENODEV;
	}

	skbpriv(skb)->ld = ld;
	skbpriv(skb)->iod = iod;

	/* Don't care whether or not DATA_DRAIN_CHANNEL is opened */
	if (iod->id == DATA_DRAIN_CHANNEL)
		return rx_drain(skb);

	/* Don't care whether or not DATA_LOOPBACK_CHANNEL is opened */
	if (iod->id == DATA_LOOPBACK_CHANNEL)
		return rx_loopback(skb);

	/* Print recieved data from CP */
	log_ipc_pkt(skb, LINK, RX);

	if (atomic_read(&iod->opened) <= 0) {
		mif_err("%s: ERR! %s is not opened\n", ld->name, iod->name);
		return -ENODEV;
	}

	if (exynos_fmt_ch(ch))
		return rx_fmt_ipc(skb);
	else if (exynos_ps_ch(ch))
		return rx_multi_pdp(skb);
	else
		return rx_raw_misc(skb);
}

/**
 * rx_frame_config
 * @iod: pointer to an instance of io_device structure
 * @ld: pointer to an instance of link_device structure
 * @buff: pointer to a buffer in which incoming data is stored
 * @size: size of data in the buffer
 * @frm: pointer to an instance of exynos_frame_data structure
 *
 * 1) Checks a config field
 * 2) Calculates the length of link layer header in an incoming frame and stores
 *    the value to "frm->hdr_len"
 * 3) Stores the config field to "frm->hdr" and add the size of config field to
 *    "frm->hdr_rcvd"
 *
 * Returns the length of a config field that was copied to "frm"
 */
static int rx_frame_config(struct io_device *iod, struct link_device *ld,
		u8 *buff, int size, struct exynos_frame_data *frm)
{
	int rest;
	int rcvd;

	if (unlikely(!exynos_start_valid(buff))) {
		mif_err("%s->%s: ERR! INVALID config 0x%02x\n",
			ld->name, iod->name, buff[0]);
		return -EBADMSG;
	}

	frm->hdr_len = EXYNOS_HEADER_SIZE;

	/* Calculate the size of a segment that will be copied */
	rest = frm->hdr_len;
	rcvd = EXYNOS_FRAG_CONFIG_SIZE;
	mif_debug("%s->%s: hdr_len:%d hdr_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->hdr_len, frm->hdr_rcvd, rest, size,
		rcvd);

	/* Copy the config field of an EXYNOS link header to the header buffer */
	memcpy(frm->hdr, buff, rcvd);
	frm->hdr_rcvd += rcvd;

	return rcvd;
}

/**
 * rx_frame_prepare_skb
 * @iod: pointer to an instance of io_device structure
 * @ld: pointer to an instance of link_device structure
 * @frm: pointer to an instance of exynos_frame_data structure
 *
 * 1) Extracts the length of a link frame from the link header in "frm->hdr"
 * 2) Allocates an skb
 * 3) Calculates the payload size in the link frame
 * 4) Calculates the padding size in the link frame
 *
 * Returns the pointer to an skb
 */
static struct sk_buff *rx_frame_prepare_skb(struct io_device *iod,
		struct link_device *ld, struct exynos_frame_data *frm)
{
	struct sk_buff *skb;

	/* Get the frame length */
	frm->len = exynos_get_frame_len(frm->hdr);

	/* Allocate an skb */
	skb = rx_alloc_skb(frm->len, iod, ld);
	if (!skb) {
		mif_err("%s->%s: ERR! rx_alloc_skb fail (size %d)\n",
			ld->name, iod->name, frm->len);
		return NULL;
	}

	/* Calculates the payload size */
	frm->pay_len = frm->len - frm->hdr_len;

	/* Calculates the padding size */
	if (exynos_padding_exist(frm->hdr))
		frm->pad_len = exynos_calc_padding_size(frm->len);

	mif_debug("%s->%s: size %d (header:%d payload:%d padding:%d)\n",
		ld->name, iod->name, frm->len, frm->hdr_len, frm->pay_len,
		frm->pad_len);

	return skb;
}

/**
 * rx_frame_header
 * @iod: pointer to an instance of io_device structure
 * @ld: pointer to an instance of link_device structure
 * @buff: pointer to a buffer in which incoming data is stored
 * @size: size of data in the buffer
 * @frm: pointer to an instance of exynos_frame_data structure
 *
 * 1) Stores a link layer header to "frm->hdr" temporarily while "frm->hdr_rcvd"
 *    is less than "frm->hdr_len"
 * 2) Then,
 *      Allocates an skb
 *      Copies the link header from "frm" to "skb"
 *      Register the skb to receive payload
 *
 * Returns the size of a segment that was copied to "frm"
 */
static int rx_frame_header(struct io_device *iod, struct link_device *ld,
		u8 *buff, int size, struct exynos_frame_data *frm)
{
	struct sk_buff *skb;
	int rest;
	int rcvd;

	/* Calculate the size of a segment that will be copied */
	rest = frm->hdr_len - frm->hdr_rcvd;
	rcvd = min(rest, size);
	mif_debug("%s->%s: hdr_len:%d hdr_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->hdr_len, frm->hdr_rcvd, rest, size,
		rcvd);

	/* Copy a segment of an EXYNOS link header to "frm" */
	memcpy((frm->hdr + frm->hdr_rcvd), buff, rcvd);
	frm->hdr_rcvd += rcvd;

	if (frm->hdr_rcvd >= frm->hdr_len) {
		/* Prepare an skb with the information in {iod, ld, frm} */
		skb = rx_frame_prepare_skb(iod, ld, frm);
		if (!skb) {
			mif_err("%s->%s: ERR! rx_frame_prepare_skb fail\n",
				ld->name, iod->name);
			return -ENOMEM;
		}

		/* Copy an EXYNOS link header from "frm" to "skb" */
		memcpy(skb_put(skb, frm->hdr_len), frm->hdr, frm->hdr_len);

		/* Register the skb to receive payload */
		fragdata(iod, ld)->skb_recv = skb;
	}

	return rcvd;
}

/**
 * rx_frame_payload
 * @iod: pointer to an instance of io_device structure
 * @ld: pointer to an instance of link_device structure
 * @buff: pointer to a buffer in which incoming data is stored
 * @size: size of data in the buffer
 * @frm: pointer to an instance of exynos_frame_data structure
 *
 * Stores a link layer payload to "skb"
 *
 * Returns the size of a segment that was copied to "skb"
 */
static int rx_frame_payload(struct io_device *iod, struct link_device *ld,
		u8 *buff, int size, struct exynos_frame_data *frm)
{
	struct sk_buff *skb = fragdata(iod, ld)->skb_recv;
	int rest;
	int rcvd;

	/* Calculate the size of a segment that will be copied */
	rest = frm->pay_len - frm->pay_rcvd;
	rcvd = min(rest, size);
	mif_debug("%s->%s: pay_len:%d pay_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->pay_len, frm->pay_rcvd, rest, size,
		rcvd);

	/* Copy an EXYNOS link payload to "skb" */
	memcpy(skb_put(skb, rcvd), buff, rcvd);
	frm->pay_rcvd += rcvd;

	return rcvd;
}

static int rx_frame_padding(struct io_device *iod, struct link_device *ld,
		u8 *buff, int size, struct exynos_frame_data *frm)
{
	struct sk_buff *skb = fragdata(iod, ld)->skb_recv;
	int rest;
	int rcvd;

	/* Calculate the size of a segment that will be dropped as padding */
	rest = frm->pad_len - frm->pad_rcvd;
	rcvd = min(rest, size);
	mif_debug("%s->%s: pad_len:%d pad_rcvd:%d rest:%d size:%d rcvd:%d\n",
		ld->name, iod->name, frm->pad_len, frm->pad_rcvd, rest, size,
		rcvd);

	/* Copy an EXYNOS link padding to "skb" */
	memcpy(skb_put(skb, rcvd), buff, rcvd);
	frm->pad_rcvd += rcvd;

	return rcvd;
}

static int rx_frame_done(struct io_device *iod, struct link_device *ld,
		struct sk_buff *skb)
{
	/* Cut off the padding of the current frame */
	skb_trim(skb, exynos_get_frame_len(skb->data));
	mif_debug("%s->%s: frame length = %d\n", ld->name, iod->name, skb->len);

	return rx_demux(ld, skb);
}

static int recv_frame_from_buff(struct io_device *iod, struct link_device *ld,
		const char *data, unsigned size)
{
	struct exynos_frame_data *frm = &fragdata(iod, ld)->f_data;
	struct sk_buff *skb;
	u8 *buff = (u8 *)data;
	int rest = (int)size;
	int done = 0;
	int err = 0;

	mif_debug("%s->%s: size %d (RX state = %s)\n", ld->name, iod->name,
		size, get_rx_state_str(iod->curr_rx_state));

	while (rest > 0) {
		switch (iod->curr_rx_state) {
		case IOD_RX_ON_STANDBY:
			fragdata(iod, ld)->skb_recv = NULL;
			memset(frm, 0, sizeof(struct exynos_frame_data));

			done = rx_frame_config(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			iod->next_rx_state = IOD_RX_HEADER;

			break;

		case IOD_RX_HEADER:
			done = rx_frame_header(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			if (frm->hdr_rcvd >= frm->hdr_len)
				iod->next_rx_state = IOD_RX_PAYLOAD;
			else
				iod->next_rx_state = IOD_RX_HEADER;

			break;

		case IOD_RX_PAYLOAD:
			done = rx_frame_payload(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			if (frm->pay_rcvd >= frm->pay_len) {
				if (frm->pad_len > 0)
					iod->next_rx_state = IOD_RX_PADDING;
				else
					iod->next_rx_state = IOD_RX_ON_STANDBY;
			} else {
				iod->next_rx_state = IOD_RX_PAYLOAD;
			}

			break;

		case IOD_RX_PADDING:
			done = rx_frame_padding(iod, ld, buff, rest, frm);
			if (done < 0) {
				err = done;
				goto err_exit;
			}

			if (frm->pad_rcvd >= frm->pad_len)
				iod->next_rx_state = IOD_RX_ON_STANDBY;
			else
				iod->next_rx_state = IOD_RX_PADDING;

			break;

		default:
			mif_err("%s->%s: ERR! INVALID RX state %d\n",
				ld->name, iod->name, iod->curr_rx_state);
			err = -EINVAL;
			goto err_exit;
		}

		if (iod->next_rx_state == IOD_RX_ON_STANDBY) {
			/*
			** A complete frame is in fragdata(iod, ld)->skb_recv.
			*/
			skb = fragdata(iod, ld)->skb_recv;
			err = rx_frame_done(iod, ld, skb);
			if (err < 0)
				goto err_exit;
		}

		buff += done;
		rest -= done;
		if (rest < 0)
			goto err_range;

		iod->curr_rx_state = iod->next_rx_state;
	}

	return size;

err_exit:
	if (fragdata(iod, ld)->skb_recv) {
		mif_err("%s->%s: ERR! clear frag (size:%d done:%d rest:%d)\n",
			ld->name, iod->name, size, done, rest);
		dev_kfree_skb_any(fragdata(iod, ld)->skb_recv);
		fragdata(iod, ld)->skb_recv = NULL;
	}
	iod->curr_rx_state = IOD_RX_ON_STANDBY;
	return err;

err_range:
	mif_err("%s->%s: ERR! size:%d done:%d rest:%d\n",
		ld->name, iod->name, size, done, rest);
	iod->curr_rx_state = IOD_RX_ON_STANDBY;
	return size;
}

/* called from link device when a packet arrives for this io device */
static int io_dev_recv_data_from_link_dev(struct io_device *iod,
		struct link_device *ld, const char *data, unsigned int len)
{
	struct sk_buff *skb;
	int err;

	switch (iod->format) {
	case IPC_FMT:
	case IPC_RAW:
	case IPC_RFS:
	case IPC_MULTI_RAW:
		if (iod->waketime)
			wake_lock_timeout(&iod->wakelock, iod->waketime);

		err = recv_frame_from_buff(iod, ld, data, len);
		if (err < 0) {
			mif_err("%s->%s: ERR! recv_frame_from_buff fail "
				"(err %d)\n", ld->name, iod->name, err);
		}

		return err;

	default:
		mif_debug("%s->%s: len %d\n", ld->name, iod->name, len);

		/* save packet to sk_buff */
		skb = rx_alloc_skb(len, iod, ld);
		if (!skb) {
			mif_info("%s->%s: ERR! rx_alloc_skb fail\n",
				ld->name, iod->name);
			return -ENOMEM;
		}

		memcpy(skb_put(skb, len), data, len);

		queue_skb_to_iod(skb, iod);

		wake_up(&iod->wq);

		return len;
	}
}

static int recv_frame_from_skb(struct io_device *iod, struct link_device *ld,
		struct sk_buff *skb)
{
	struct sk_buff *clone;
	unsigned int rest;
	unsigned int rcvd;
	unsigned int tot;		/* total length including padding */
	int err = 0;

	/*
	** If there is only one EXYNOS frame in @skb, receive the EXYNOS frame and
	** return immediately. In this case, the frame verification must already
	** have been done at the link device.
	*/
	if (skbpriv(skb)->single_frame) {
		err = rx_frame_done(iod, ld, skb);
		if (err < 0)
			goto exit;
		return 0;
	}

	/*
	** The routine from here is used only if there may be multiple EXYNOS
	** frames in @skb.
	*/

	/* Check the config field of the first frame in @skb */
	if (!exynos_start_valid(skb->data)) {
		mif_err("%s->%s: ERR! INVALID config 0x%02X\n",
			ld->name, iod->name, skb->data[0]);
		err = -EINVAL;
		goto exit;
	}

	/* Get the total length of the frame with a padding */
	tot = exynos_get_total_len(skb->data);

	/* Verify the total length of the first frame */
	rest = skb->len;
	if (unlikely(tot > rest)) {
		mif_err("%s->%s: ERR! tot %d > skb->len %d)\n",
			ld->name, iod->name, tot, rest);
		err = -EINVAL;
		goto exit;
	}

	/* If there is only one EXYNOS frame in @skb, */
	if (likely(tot == rest)) {
		/* Receive the EXYNOS frame and return immediately */
		err = rx_frame_done(iod, ld, skb);
		if (err < 0)
			goto exit;
		return 0;
	}

	/*
	** This routine is used only if there are multiple EXYNOS frames in @skb.
	*/
	rcvd = 0;
	while (rest > 0) {
		clone = skb_clone(skb, GFP_ATOMIC);
		if (unlikely(!clone)) {
			mif_err("%s->%s: ERR! skb_clone fail\n",
				ld->name, iod->name);
			err = -ENOMEM;
			goto exit;
		}

		/* Get the start of an EXYNOS frame */
		skb_pull(clone, rcvd);
		if (!exynos_start_valid(clone->data)) {
			mif_err("%s->%s: ERR! INVALID config 0x%02X\n",
				ld->name, iod->name, clone->data[0]);
			dev_kfree_skb_any(clone);
			err = -EINVAL;
			goto exit;
		}

		/* Get the total length of the current frame with a padding */
		tot = exynos_get_total_len(clone->data);
		if (unlikely(tot > rest)) {
			mif_err("%s->%s: ERR! dirty frame (tot %d > rest %d)\n",
				ld->name, iod->name, tot, rest);
			dev_kfree_skb_any(clone);
			err = -EINVAL;
			goto exit;
		}

		/* Cut off the padding of the current frame */
		skb_trim(clone, exynos_get_frame_len(clone->data));

		/* Demux the frame */
		err = rx_demux(ld, clone);
		if (err < 0) {
			mif_err("%s->%s: ERR! rx_demux fail (err %d)\n",
				ld->name, iod->name, err);
			dev_kfree_skb_any(clone);
			goto exit;
		}

		/* Calculate the start of the next frame */
		rcvd += tot;

		/* Calculate the rest size of data in @skb */
		rest -= tot;
	}

exit:
	dev_kfree_skb_any(skb);
	return err;
}

/* called from link device when a packet arrives for this io device */
static int io_dev_recv_skb_from_link_dev(struct io_device *iod,
		struct link_device *ld, struct sk_buff *skb)
{
	enum dev_format dev = iod->format;
	int err;

	switch (dev) {
	case IPC_FMT:
	case IPC_RAW:
	case IPC_RFS:
	case IPC_MULTI_RAW:
		if (iod->waketime)
			wake_lock_timeout(&iod->wakelock, iod->waketime);

		err = recv_frame_from_skb(iod, ld, skb);
		if (err < 0) {
			mif_err("%s->%s: ERR! recv_frame_from_skb fail "
				"(err %d)\n", ld->name, iod->name, err);
		}

		return err;

	case IPC_BOOT:
	case IPC_DUMP:
		if (!iod->id) {
			mif_err("%s->%s: ERR! invalid iod\n",
				ld->name, iod->name);
			return -ENODEV;
		}

		if (iod->waketime)
			wake_lock_timeout(&iod->wakelock, iod->waketime);

		err = recv_frame_from_skb(iod, ld, skb);
		if (err < 0) {
			mif_err("%s->%s: ERR! recv_frame_from_skb fail "
				"(err %d)\n", ld->name, iod->name, err);
		}

		return err;

	default:
		mif_err("%s->%s: ERR! invalid iod\n", ld->name, iod->name);
		return -EINVAL;
	}
}

/* called from link device when a packet arrives fo this io device */
static int io_dev_recv_skb_single_from_link_dev(struct io_device *iod,
						struct link_device *ld, struct sk_buff *skb)
{
	int err;

	iodev_lock_wlock(iod);

	if (skbpriv(skb)->lnk_hdr && ld->aligned) {
		skb_trim(skb, exynos_get_frame_len(skb->data));
	}

	err = rx_demux(ld, skb);
	if (err < 0)
		mif_err_limited("%s<-%s: ERR! rx_demux fail (err %d)\n",
			iod->name, ld->name, err);

	    return err;
}
/* inform the IO device that the modem is now online or offline or
 * crashing or whatever...
 */
static void io_dev_modem_state_changed(struct io_device *iod,
			enum modem_state state)
{
	struct modem_ctl *mc = iod->mc;
	int old_state = mc->phone_state;

	if (old_state != state) {
		mc->phone_state = state;
		mif_err("%s state changed (%s -> %s)\n", mc->name,
			get_cp_state_str(old_state), get_cp_state_str(state));
	}

	if (state == STATE_CRASH_RESET || state == STATE_CRASH_EXIT ||
	    state == STATE_NV_REBUILDING)
		wake_up(&iod->wq);
}

/**
 * io_dev_sim_state_changed
 * @iod:	IPC's io_device
 * @sim_online: SIM is online?
 */
static void io_dev_sim_state_changed(struct io_device *iod, bool sim_online)
{
	if (atomic_read(&iod->opened) == 0) {
		mif_info("%s: ERR! not opened\n", iod->name);
	} else if (iod->mc->sim_state.online == sim_online) {
		mif_info("%s: SIM state not changed\n", iod->name);
	} else {
		iod->mc->sim_state.online = sim_online;
		iod->mc->sim_state.changed = true;
		mif_info("%s: SIM state changed {online %d, changed %d}\n",
			iod->name, iod->mc->sim_state.online,
			iod->mc->sim_state.changed);
		wake_up(&iod->wq);
	}
}

static void iodev_dump_status(struct io_device *iod, void *args)
{
	if (iod->format == IPC_RAW && iod->io_typ == IODEV_NET) {
		struct link_device *ld = get_current_link(iod);
		mif_com_log(iod->mc->msd, "%s: %s\n", iod->name, ld->name);
	}
}

static int misc_open(struct inode *inode, struct file *filp)
{
	struct io_device *iod = to_io_device(filp->private_data);
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;
	int ref_cnt;
	int ret;
	filp->private_data = (void *)iod;

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->init_comm) {
			ret = ld->init_comm(ld, iod);
			if (ret < 0) {
				mif_err("%s<->%s: ERR! init_comm fail(%d)\n",
					iod->name, ld->name, ret);
				return ret;
			}
		}
	}

	ref_cnt = atomic_inc_return(&iod->opened);

	mif_err("%s (opened %d) by %s\n", iod->name, ref_cnt, current->comm);

	return 0;
}

static int misc_release(struct inode *inode, struct file *filp)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct modem_shared *msd = iod->msd;
	struct link_device *ld;
	int ref_cnt;

	skb_queue_purge(&iod->sk_rx_q);

	list_for_each_entry(ld, &msd->link_dev_list, list) {
		if (IS_CONNECTED(iod, ld) && ld->terminate_comm)
			ld->terminate_comm(ld, iod);
	}

	ref_cnt = atomic_dec_return(&iod->opened);

	mif_err("%s (opened %d) by %s\n", iod->name, ref_cnt, current->comm);

	return 0;
}

#ifdef CONFIG_MZ_RSTINFO
extern void update_mdcrash_rstcnt(u32 crash_type);
#else
void update_mdcrash_rstcnt(u32 crash_type) {}
#endif

static unsigned int misc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct modem_ctl *mc = iod->mc;
	static int cnt;
	poll_wait(filp, &iod->wq, wait);

	if (!skb_queue_empty(&iod->sk_rx_q) && mc->phone_state != STATE_OFFLINE)
		return POLLIN | POLLRDNORM;

	if (mc->phone_state == STATE_CRASH_RESET
	    || mc->phone_state == STATE_CRASH_EXIT
	    || mc->phone_state == STATE_NV_REBUILDING
	    || mc->sim_state.changed) {
		if (iod->format == IPC_RAW) {
			msleep(20);
			return 0;
		}
		if (iod->format == IPC_DUMP)
			return 0;

		if (++cnt % 10000 == 0) {
			mif_err("Loading CP Dump File...\n");
			cnt = 0;
		}

		if (mc->phone_state == STATE_CRASH_EXIT)
			update_mdcrash_rstcnt(mc->phone_state);
//		mif_err("%s: state %s\n",
//			iod->name, get_cp_state_str(mc->phone_state));

		return POLLHUP;
	} else {
		return 0;
	}
}

static long misc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct modem_ctl *mc = iod->mc;
	int p_state;
	char *buff;
	void __user *user_buff;
	unsigned long size;

	switch (cmd) {
	case IOCTL_MODEM_ON:
		if (mc->ops.modem_on) {
			mif_err("%s: IOCTL_MODEM_ON\n", iod->name);
			return mc->ops.modem_on(mc);
		}
		mif_err("%s: !mc->ops.modem_on\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_OFF:
		if (mc->ops.modem_off) {
			mif_err("%s: IOCTL_MODEM_OFF\n", iod->name);
			return mc->ops.modem_off(mc);
		}
		mif_err("%s: !mc->ops.modem_off\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_RESET:
		if (mc->ops.modem_reset) {
			mif_err("%s: IOCTL_MODEM_RESET\n", iod->name);
			return mc->ops.modem_reset(mc);
		}
		mif_err("%s: !mc->ops.modem_reset\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_BOOT_ON:
		if (mc->ops.modem_boot_on) {
			mif_err("%s: IOCTL_MODEM_BOOT_ON\n", iod->name);
			return mc->ops.modem_boot_on(mc);
		}
		mif_err("%s: !mc->ops.modem_boot_on\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_BOOT_OFF:
		if (mc->ops.modem_boot_off) {
			mif_err("%s: IOCTL_MODEM_BOOT_OFF\n", iod->name);
			return mc->ops.modem_boot_off(mc);
		}
		mif_err("%s: !mc->ops.modem_boot_off\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_BOOT_DONE:
		mif_err("%s: IOCTL_MODEM_BOOT_DONE\n", iod->name);
		if (mc->ops.modem_boot_done)
			return mc->ops.modem_boot_done(mc);
		return 0;

	case IOCTL_MODEM_STATUS:
		mif_debug("%s: IOCTL_MODEM_STATUS\n", iod->name);

		p_state = mc->phone_state;

		if (p_state != STATE_ONLINE) {
			mif_err("%s: IOCTL_MODEM_STATUS (state %s)\n",
				iod->name, get_cp_state_str(p_state));
		}

		if (mc->sim_state.changed) {
			int s_state = mc->sim_state.online ?
					STATE_SIM_ATTACH : STATE_SIM_DETACH;
			mc->sim_state.changed = false;
			return s_state;
		}

		if (p_state == STATE_NV_REBUILDING)
			mc->phone_state = STATE_ONLINE;

		return p_state;

	case IOCTL_MODEM_XMIT_BOOT:
		if (ld->xmit_boot) {
			mif_info("%s: IOCTL_MODEM_XMIT_BOOT\n", iod->name);
			return ld->xmit_boot(ld, iod, arg);
		}
		mif_err("%s: !ld->xmit_boot\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DL_START:
		if (ld->dload_start) {
			mif_info("%s: IOCTL_MODEM_DL_START\n", iod->name);
			return ld->dload_start(ld, iod);
		}
		mif_err("%s: !ld->dload_start\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_FW_UPDATE:
		if (ld->firm_update) {
			mif_info("%s: IOCTL_MODEM_FW_UPDATE\n", iod->name);
			return ld->firm_update(ld, iod, arg);
		}
		mif_err("%s: !ld->firm_update\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_FORCE_CRASH_EXIT:
		if (mc->ops.modem_force_crash_exit) {
			mif_err("%s: IOCTL_MODEM_FORCE_CRASH_EXIT\n",
				iod->name);
			return mc->ops.modem_force_crash_exit(mc);
		}
		mif_err("%s: !mc->ops.modem_force_crash_exit\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DUMP_RESET:
		if (mc->ops.modem_dump_reset) {
			mif_info("%s: IOCTL_MODEM_DUMP_RESET\n", iod->name);
			return mc->ops.modem_dump_reset(mc);
		}
		mif_err("%s: !mc->ops.modem_dump_reset\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DUMP_START:
		if (mc->ops.modem_dump_start) {
			mif_err("%s: IOCTL_MODEM_DUMP_START\n", iod->name);
			return mc->ops.modem_dump_start(mc);
		} else if (ld->dump_start) {
			mif_err("%s: IOCTL_MODEM_DUMP_START\n", iod->name);
			return ld->dump_start(ld, iod);
		}
		mif_err("%s: !dump_start\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_RAMDUMP_START:
		if (ld->dump_start) {
			mif_info("%s: IOCTL_MODEM_RAMDUMP_START\n", iod->name);
			return ld->dump_start(ld, iod);
		}
		mif_err("%s: !ld->dump_start\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_DUMP_UPDATE:
		if (ld->dump_update) {
			mif_info("%s: IOCTL_MODEM_DUMP_UPDATE\n", iod->name);
			return ld->dump_update(ld, iod, arg);
		}
		mif_err("%s: !ld->dump_update\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_RAMDUMP_STOP:
		if (ld->dump_finish) {
			mif_info("%s: IOCTL_MODEM_RAMDUMP_STOP\n", iod->name);
			return ld->dump_finish(ld, iod, arg);
		}
		mif_err("%s: !ld->dump_finish\n", iod->name);
		return -EINVAL;

	case IOCTL_MODEM_CP_UPLOAD:
		buff = iod->msd->cp_crash_info + strlen(CP_CRASH_TAG);
		user_buff = (void __user *)arg;

		mif_info("%s: IOCTL_MODEM_CP_UPLOAD\n", iod->name);
		memmove(iod->msd->cp_crash_info, CP_CRASH_TAG, strlen(CP_CRASH_TAG));
		if (arg) {
			if (copy_from_user(buff, user_buff, MAX_CPINFO_SIZE))
				return -EFAULT;
		}
		WARN(true, "++++%s+++++\n", iod->msd->cp_crash_info);
		//panic(iod->msd->cp_crash_info);
		return 0;

	case IOCTL_MODEM_PROTOCOL_SUSPEND:
		mif_info("%s: IOCTL_MODEM_PROTOCOL_SUSPEND\n", iod->name);
		if (iod->format == IPC_MULTI_RAW) {
			iodevs_for_each(iod->msd, iodev_netif_stop, 0);
			return 0;
		}
		return -EINVAL;

	case IOCTL_MODEM_PROTOCOL_RESUME:
		mif_info("%s: IOCTL_MODEM_PROTOCOL_RESUME\n", iod->name);
		if (iod->format != IPC_MULTI_RAW) {
			iodevs_for_each(iod->msd, iodev_netif_wake, 0);
			return 0;
		}
		return -EINVAL;

	case IOCTL_MIF_LOG_DUMP:
		user_buff = (void __user *)arg;

		iodevs_for_each(iod->msd, iodev_dump_status, 0);
		size = MAX_MIF_BUFF_SIZE;
		if (copy_to_user(user_buff, &size, sizeof(unsigned long)))
			return -EFAULT;
		mif_dump_log(mc->msd, iod);
		return 0;

	case IOCTL_MODEM_GET_SHMEM_INFO:
		mif_debug("<%s> IOCTL_MODEM_GET_SHMEM_INFO\n", iod->name);
		return 0;
// hack
//		return iod->mc->ops.modem_get_meminfo(iod->mc, arg);

	case IOCTL_SEC_CP_INIT:
		if (ld->sec_init) {
			mif_info("%s: IOCTL_MODEM_SEC_INIT\n", iod->name);
			return ld->sec_init(ld, iod, arg);
		}
		return -EINVAL;

	case IOCTL_CHECK_SECURITY:
		if (ld->check_security) {
			mif_info("%s: IOCTL_MODEM_CHECK_SECURITY\n", iod->name);
			return ld->check_security(ld, iod, arg);
		}
		mif_err("%s: !ld->check_security\n", iod->name);
		return -EINVAL;

	case IOCTL_XMIT_BIN:
		if (ld->xmit_bin) {
			return ld->xmit_bin(ld, iod, arg);
		}
		return -EINVAL;
	default:
		 /* If you need to handle the ioctl for specific link device,
		  * then assign the link ioctl handler to ld->ioctl
		  * It will be call for specific link ioctl */
		if (ld->ioctl)
			return ld->ioctl(ld, iod, cmd, arg);

		mif_info("%s: ERR! undefined cmd 0x%X\n", iod->name, cmd);
		return -EINVAL;
	}

	return 0;
}

static ssize_t misc_write(struct file *filp, const char __user *data,
			size_t count, loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = get_current_link(iod);
	struct modem_ctl *mc = iod->mc;
	struct sk_buff *skb;
	u8 *buff;
	int ret;
	size_t headroom;
	size_t tailroom;
	size_t tx_bytes;
	u16 fr_cfg;

	if (iod->format <= IPC_RFS && iod->id == 0)
		return -EINVAL;

	if (unlikely(!cp_online(mc)) && exynos_ipc_ch(iod->id)) {
		mif_debug("%s: ERR! %s->state == %s\n",
			iod->name, mc->name, get_cp_state_str(mc->phone_state));
		return -EPERM;
	}

	if (iod->link_header) {
		fr_cfg = exynos_build_fr_config(iod, ld, count);
		headroom = EXYNOS_HEADER_SIZE;
		if (ld->aligned)
			tailroom = exynos_calc_padding_size(EXYNOS_HEADER_SIZE + count);
		else
			tailroom = 0;
	} else {
		fr_cfg = 0;
		headroom = 0;
		tailroom = 0;
	}

	tx_bytes = headroom + count + tailroom;

	skb = alloc_skb(tx_bytes, GFP_KERNEL);
	if (!skb) {
		mif_info("%s: ERR! alloc_skb fail (tx_bytes:%ld)\n",
			iod->name, tx_bytes);
		return -ENOMEM;
	}

	/* Store the IO device, the link device, etc. */
	skbpriv(skb)->iod = iod;
	skbpriv(skb)->ld = ld;

	skbpriv(skb)->lnk_hdr = iod->link_header;
	skbpriv(skb)->exynos_ch = iod->id;

	/* Build EXYNOS link header*/
	if (fr_cfg) {
		buff = skb_put(skb, headroom);
		exynos_build_header(iod, ld, buff, fr_cfg, 0, count);
	}

	/* Store IPC message */
	buff = skb_put(skb, count);
	if (copy_from_user(buff, data, count)) {
		mif_err("%s->%s: ERR! copy_from_user fail (count %ld)\n",
			iod->name, ld->name, count);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	/* Apply padding */
	if (tailroom)
		skb_put(skb, tailroom);

	log_ipc_pkt(skb, IODEV, TX);

	ret = ld->send(ld, iod, skb);
	if (ret < 0) {
		mif_info("%s->%s: ERR! ld->send fail (err %d, tx_bytes %ld)\n",
			iod->name, ld->name, ret, tx_bytes);
		return ret;
	}

	if (ret != tx_bytes) {
		mif_info("%s->%s: WARNING! ret %d != tx_bytes %ld (count %ld)\n",
			iod->name, ld->name, ret, tx_bytes, count);
	}

	return count;
}

static ssize_t misc_read(struct file *filp, char *buf, size_t count,
			loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	struct sk_buff *skb;
	int copied = 0;

	if (skb_queue_empty(rxq)) {
		mif_info("%s: ERR! no data in rxq\n", iod->name);
		return 0;
	}

	skb = skb_dequeue(rxq);
	if (unlikely(!skb)) {
		mif_info("%s: No data in RXQ\n", iod->name);
		return 0;
	}

	log_ipc_pkt(skb, IODEV, RX);

	copied = skb->len > count ? count : skb->len;

	if (copy_to_user(buf, skb->data, copied)) {
		mif_err("%s: ERR! copy_to_user fail\n", iod->name);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	mif_debug("%s: data:%d copied:%d qlen:%d\n",
		iod->name, skb->len, copied, rxq->qlen);

	if (skb->len > count) {
		skb_pull(skb, count);
		skb_queue_head(rxq, skb);
	} else {
		dev_kfree_skb_any(skb);
	}

	return copied;
}

static const struct file_operations misc_io_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.poll = misc_poll,
	.unlocked_ioctl = misc_ioctl,
	.write = misc_write,
	.read = misc_read,
};

static int vnet_open(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);

	mif_err("%s\n", vnet->iod->name);

	netif_start_queue(ndev);
	atomic_inc(&vnet->iod->opened);
	return 0;
}

static int vnet_stop(struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);

	mif_err("%s\n", vnet->iod->name);

	atomic_dec(&vnet->iod->opened);
	netif_stop_queue(ndev);
	skb_queue_purge(&vnet->iod->sk_rx_q);
	return 0;
}

static int vnet_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct vnet *vnet = netdev_priv(ndev);
	struct io_device *iod = vnet->iod;
	struct link_device *ld = get_current_link(iod);
	struct sk_buff *skb_new = skb;
	struct iphdr *ip_header = (struct iphdr *)skb->data;
	struct modem_ctl *mc = iod->mc;
	int ret;
	unsigned headroom;
	unsigned tailroom;
	size_t count = skb->len;
	size_t tx_bytes;
	u8 *buff;
	u16 fr_cfg;

	if (unlikely(!cp_online(mc))) {
		if (!netif_queue_stopped(ndev))
			netif_stop_queue(ndev);
		/* just drop the tx packet and return tx_ok */
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* When use `handover' with Network Bridge,
	 * user -> bridge device(rmnet0) -> real rmnet(xxxx_rmnet0) -> here.
	 * bridge device is ethernet device unlike xxxx_rmnet(net device).
	 * We remove the an ethernet header of skb before using skb->len,
	 * because bridge device added an ethernet header to skb.
	 */
	if (iod->use_handover) {
		if (iod->id >= EXYNOS_CH_ID_PDP_0 && iod->id <= EXYNOS_CH_ID_PDP_9)
			skb_pull(skb, sizeof(struct ethhdr));
	}

	if (iod->link_header) {
		fr_cfg = exynos_build_fr_config(iod, ld, count);
		headroom = EXYNOS_HEADER_SIZE;
		if (ld->aligned)
			tailroom = exynos_calc_padding_size(EXYNOS_HEADER_SIZE + count);
		else
			tailroom = 0;
	} else {
		fr_cfg = 0;
		headroom = 0;
		tailroom = 0;
	}

	tx_bytes = headroom + count + tailroom;

	if (skb_headroom(skb) < headroom || skb_tailroom(skb) < tailroom) {
		mif_debug("%s: skb_copy_expand needed\n", iod->name);
		skb_new = skb_copy_expand(skb, headroom, tailroom, GFP_ATOMIC);
		if (!skb_new) {
			mif_info("%s: ERR! skb_copy_expand fail\n", iod->name);
			return NETDEV_TX_BUSY;
		}
		dev_kfree_skb_any(skb);
	}

	/* Build EXYNOS link header*/
	buff = skb_push(skb_new, headroom);
	if (fr_cfg)
		exynos_build_header(iod, ld, buff, fr_cfg, 0, count);

	/* IP loop-back */
	ip_header = (struct iphdr *)skb->data;
	if (iod->msd->loopback_ipaddr &&
		ip_header->daddr == iod->msd->loopback_ipaddr) {
		swap(ip_header->saddr, ip_header->daddr);
		buff[EXYNOS_CH_ID_OFFSET] = DATA_LOOPBACK_CHANNEL;
	}

	if (tailroom)
		skb_put(skb_new, tailroom);

	skbpriv(skb_new)->iod = iod;
	skbpriv(skb_new)->ld = ld;

	skbpriv(skb_new)->lnk_hdr = iod->link_header;
	skbpriv(skb_new)->exynos_ch = iod->id;

	log_ipc_pkt(skb_new, IODEV, TX);

	ret = ld->send(ld, iod, skb_new);
	if (ret < 0) {
		netif_stop_queue(ndev);
		mif_info("%s->%s: ERR! ld->send fail (err %d, tx_bytes %ld)\n",
			iod->name, ld->name, ret, tx_bytes);
		return NETDEV_TX_BUSY;
	}

	if (ret != tx_bytes) {
		mif_info("%s->%s: WARNING! ret %d != tx_bytes %ld (count %ld)\n",
			iod->name, ld->name, ret, tx_bytes, count);
	}

	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += count;

	return NETDEV_TX_OK;
}

static struct net_device_ops vnet_ops = {
	.ndo_open = vnet_open,
	.ndo_stop = vnet_stop,
	.ndo_start_xmit = vnet_xmit,
};

static void vnet_setup(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_PPP;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	ndev->addr_len = 0;
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

static void vnet_setup_ether(struct net_device *ndev)
{
	ndev->netdev_ops = &vnet_ops;
	ndev->type = ARPHRD_ETHER;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST | IFF_SLAVE;
	ndev->addr_len = ETH_ALEN;
	random_ether_addr(ndev->dev_addr);
	ndev->hard_header_len = 0;
	ndev->tx_queue_len = 1000;
	ndev->mtu = ETH_DATA_LEN;
	ndev->watchdog_timeo = 5 * HZ;
}

static u16 exynos_build_fr_config(struct io_device *iod, struct link_device *ld,
				unsigned int count)
{
	u16 fr_cfg = 0;

	if (iod->format > IPC_DUMP)
		return 0;

	if ((count + EXYNOS_HEADER_SIZE) <= 0xFFFF)
		fr_cfg |= (EXYNOS_SINGLE_MASK << 8);

	return fr_cfg;
}

static void exynos_build_header(struct io_device *iod, struct link_device *ld,
				u8 *buff, u16 cfg, u8 ctl, size_t count)
{
	u16 *exynos_header = (u16 *)(buff + EXYNOS_START_OFFSET);
	u16 *frame_seq = (u16 *)(buff + EXYNOS_FRAME_SEQ_OFFSET);
	u16 *frag_cfg = (u16 *)(buff + EXYNOS_FRAG_CONFIG_OFFSET);
	u16 *size = (u16 *)(buff + EXYNOS_LEN_OFFSET);
	struct exynos_seq_num *seq_num = &(iod->seq_num);

	*exynos_header = EXYNOS_START_MASK;
	*frame_seq = ++seq_num->frame_cnt;
	*frag_cfg = cfg;
	*size = (u16)(EXYNOS_HEADER_SIZE + count);
	buff[EXYNOS_CH_ID_OFFSET] = iod->id;

	if (cfg == EXYNOS_SINGLE_MASK)
		*frag_cfg = cfg;

	buff[EXYNOS_CH_SEQ_OFFSET] = ++seq_num->ch_cnt[iod->id];
}

int exynos_init_io_device(struct io_device *iod)
{
	int ret = 0;
	struct vnet *vnet;

	if (iod->attrs & IODEV_ATTR(ATTR_NO_LINK_HEADER))
		iod->link_header = false;
	else
		iod->link_header = true;

	/* Get modem state from modem control device */
	iod->modem_state_changed = io_dev_modem_state_changed;
	iod->sim_state_changed = io_dev_sim_state_changed;

	/* Get data from link device */
	mif_debug("%s: EXYNOS version = %d\n", iod->name, iod->ipc_version);
	iod->recv = io_dev_recv_data_from_link_dev;
	iod->recv_skb = io_dev_recv_skb_from_link_dev;
	iod->recv_skb_single = io_dev_recv_skb_single_from_link_dev;
	/* Register misc or net device */
	switch (iod->io_typ) {
	case IODEV_MISC:
		init_waitqueue_head(&iod->wq);
		skb_queue_head_init(&iod->sk_rx_q);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register failed\n", iod->name);

		break;

	case IODEV_NET:
		skb_queue_head_init(&iod->sk_rx_q);
		if (iod->use_handover)
			iod->ndev = alloc_netdev(0, iod->name,
						vnet_setup_ether);
		else
			iod->ndev = alloc_netdev(0, iod->name, vnet_setup);

		if (!iod->ndev) {
			mif_info("%s: ERR! alloc_netdev fail\n", iod->name);
			return -ENOMEM;
		}

		ret = register_netdev(iod->ndev);
		if (ret) {
			mif_info("%s: ERR! register_netdev fail\n", iod->name);
			free_netdev(iod->ndev);
		}

		mif_debug("iod 0x%p\n", iod);
		vnet = netdev_priv(iod->ndev);
		mif_debug("vnet 0x%p\n", vnet);
		vnet->iod = iod;

		break;

	case IODEV_DUMMY:
		skb_queue_head_init(&iod->sk_rx_q);

		iod->miscdev.minor = MISC_DYNAMIC_MINOR;
		iod->miscdev.name = iod->name;
		iod->miscdev.fops = &misc_io_fops;

		ret = misc_register(&iod->miscdev);
		if (ret)
			mif_info("%s: ERR! misc_register fail\n", iod->name);

		ret = device_create_file(iod->miscdev.this_device,
					&attr_waketime);
		if (ret)
			mif_info("%s: ERR! device_create_file fail\n",
				iod->name);

		ret = device_create_file(iod->miscdev.this_device,
				&attr_loopback);
		if (ret)
			mif_err("failed to create `loopback file' : %s\n",
					iod->name);

		ret = device_create_file(iod->miscdev.this_device,
				&attr_txlink);
		if (ret)
			mif_err("failed to create `txlink file' : %s\n",
					iod->name);
		break;

	default:
		mif_info("%s: ERR! wrong io_type %d\n", iod->name, iod->io_typ);
		return -EINVAL;
	}

	return ret;
}
