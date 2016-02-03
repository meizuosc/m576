/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
 /******************************************************************************
  *
  *  The original Work has been changed by NXP Semiconductors.
  *
  *  Copyright (C) 2015 NXP Semiconductors
  *
  *  Licensed under the Apache License, Version 2.0 (the "License");
  *  you may not use this file except in compliance with the License.
  *  You may obtain a copy of the License at
  *
  *  http://www.apache.org/licenses/LICENSE-2.0
  *
  *  Unless required by applicable law or agreed to in writing, software
  *  distributed under the License is distributed on an "AS IS" BASIS,
  *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  *  See the License for the specific language governing permissions and
  *  limitations under the License.
  *
  ******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <asm/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/regulator/consumer.h>

#define SIG_NFC 44
#define MAX_BUFFER_SIZE 512

#define PN544_MAGIC 0xE9
/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN544_SET_PWR    _IOW(PN544_MAGIC, 0x01, unsigned int)

/*
 * SPI Request NFCC to enable p61 power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define P61_SET_SPI_PWR    _IOW(PN544_MAGIC, 0x02, unsigned int)

/* SPI or DWP can call this ioctl to get the current
 * power state of P61
 *
*/
#define P61_GET_PWR_STATUS    _IOR(PN544_MAGIC, 0x03, unsigned int)

/* DWP side this ioctl will be called
 * level 1 = Wired access is enabled/ongoing
 * level 0 = Wired access is disalbed/stopped
*/
#define P61_SET_WIRED_ACCESS _IOW(PN544_MAGIC, 0x04, unsigned int)

/*
  NFC Init will call the ioctl to register the PID with the i2c driver
*/
#define P544_SET_NFC_SERVICE_PID _IOW(PN544_MAGIC, 0x05, long)

typedef enum p61_access_state {
	P61_STATE_INVALID = 0x0000,
	P61_STATE_IDLE = 0x0100,	/* p61 is free to use */
	P61_STATE_WIRED = 0x0200,	/* p61 is being accessed by DWP (NFCC) */
	P61_STATE_SPI = 0x0400,	/* P61 is being accessed by SPI */
	P61_STATE_DWNLD = 0x0800,	/* NFCC fw download is in progress */
	P61_STATE_SPI_PRIO = 0x1000,	/*Start of p61 access by SPI on priority */
	P61_STATE_SPI_PRIO_END = 0x2000,	/*End of p61 access by SPI on priority */
} p61_access_state_t;

struct pn544_dev {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct i2c_client *client;
	struct miscdevice pn544_device;
	unsigned int ven_gpio;
	unsigned int firm_gpio;
	unsigned int irq_gpio;
	//unsigned int ese_pwr_gpio;	/* gpio used by SPI to provide power to p61 via NFCC */
	struct mutex p61_state_mutex;	/* used to make p61_current_state flag secure */
	p61_access_state_t p61_current_state;	/* stores the current P61 state */
	bool nfc_ven_enabled;	/* stores the VEN pin state powered by Nfc */
	bool spi_ven_enabled;	/* stores the VEN pin state powered by Spi */
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	long nfc_service_pid;	/*used to signal the nfc the nfc service */
	struct wake_lock wake_lock;
	struct regulator *vdd18_nfc;
	struct notifier_block pm_notifier;
};

static void pn544_disable_irq(struct pn544_dev *pn544_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn544_dev->irq_enabled_lock, flags);
	if (pn544_dev->irq_enabled) {
		disable_irq_nosync(pn544_dev->client->irq);
		pn544_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn544_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn544_dev_irq_handler(int irq, void *dev_id)
{
	struct pn544_dev *pn544_dev = dev_id;

	pn544_disable_irq(pn544_dev);
	if (!gpio_get_value(pn544_dev->irq_gpio))
		return IRQ_HANDLED;

	/* Wake up waiting readers */
	wake_up(&pn544_dev->read_wq);

	return IRQ_HANDLED;
}

static ssize_t pn544_dev_read(struct file *filp, char __user * buf,
			      size_t count, loff_t * offset)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	pr_debug("%s : reading   %zu bytes.\n", __func__, count);

	mutex_lock(&pn544_dev->read_mutex);

	if (!gpio_get_value(pn544_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}
		while (1) {
			pn544_dev->irq_enabled = true;
			enable_irq(pn544_dev->client->irq);
			ret = wait_event_interruptible(pn544_dev->read_wq,
						       !pn544_dev->irq_enabled);
			pn544_disable_irq(pn544_dev);
			if (ret)
				goto fail;
			if (gpio_get_value(pn544_dev->irq_gpio))
				break;
			pr_warning("%s: spurious interrupt detected\n",
				   __func__);
		}
	}

	/* Read data */
	ret = i2c_master_recv(pn544_dev->client, tmp, count);

	mutex_unlock(&pn544_dev->read_mutex);

	/* pn544 seems to be slow in handling I2C read requests
	 * so add 1ms delay after recv operation */
	udelay(1000);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
		       __func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn544_dev->read_mutex);
	return ret;
}

static ssize_t pn544_dev_write(struct file *filp, const char __user * buf,
			       size_t count, loff_t * offset)
{
	struct pn544_dev *pn544_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	pn544_dev = filp->private_data;
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}
	pr_debug("%s : writing %zu bytes.\n", __func__, count);

	/* Write data */
	ret = i2c_master_send(pn544_dev->client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	/* pn544 seems to be slow in handling I2C write requests
	 * so add 1ms delay after I2C send oparation */
	udelay(1000);

	return ret;
}

static void p61_update_access_state(struct pn544_dev *pn544_dev,
				    p61_access_state_t current_state, bool set)
{
	pr_debug("%s: Enter current_state = %x\n", __func__,
		pn544_dev->p61_current_state);
	if (current_state) {
		if (set) {
			if (pn544_dev->p61_current_state == P61_STATE_IDLE)
				pn544_dev->p61_current_state =
				    P61_STATE_INVALID;
			pn544_dev->p61_current_state |= current_state;
		} else {
			pn544_dev->p61_current_state ^= current_state;
			if (!pn544_dev->p61_current_state)
				pn544_dev->p61_current_state = P61_STATE_IDLE;
		}
	}
	pr_debug("%s: Exit current_state = %x\n", __func__,
		pn544_dev->p61_current_state);
}

static void p61_get_access_state(struct pn544_dev *pn544_dev,
				 p61_access_state_t * current_state)
{

	if (current_state == NULL) {
		//*current_state = P61_STATE_INVALID;
		pr_err("%s : invalid state of p61_access_state_t current state  \n",
		     __func__);
	} else
		*current_state = pn544_dev->p61_current_state;
}

static void p61_access_lock(struct pn544_dev *pn544_dev)
{
	pr_debug("%s: Enter\n", __func__);
	mutex_lock(&pn544_dev->p61_state_mutex);
	pr_debug("%s: Exit\n", __func__);
}

static void p61_access_unlock(struct pn544_dev *pn544_dev)
{
	pr_debug("%s: Enter\n", __func__);
	mutex_unlock(&pn544_dev->p61_state_mutex);
	pr_debug("%s: Exit\n", __func__);
}

static void signal_handler(p61_access_state_t state, long nfc_pid)
{
	struct siginfo sinfo;
	pid_t pid;
	struct task_struct *task;
	int sigret = 0;

	pr_debug("%s: Enter\n", __func__);

	memset(&sinfo, 0, sizeof(struct siginfo));
	sinfo.si_signo = SIG_NFC;
	sinfo.si_code = SI_QUEUE;
	sinfo.si_int = state;
	pid = nfc_pid;

	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (task) {
		pr_debug("%s.\n", task->comm);
		sigret = send_sig_info(SIG_NFC, &sinfo, task);
		if (sigret < 0) {
			pr_info("send_sig_info failed..... sigret %d.\n",
				sigret);
			//msleep(60);
		}
	} else
		pr_info("finding task from PID failed\r\n");

	pr_debug("%s: Exit\n", __func__);
}

static int pn544_dev_open(struct inode *inode, struct file *filp)
{
	struct pn544_dev *pn544_dev =
	    container_of(filp->private_data, struct pn544_dev, pn544_device);

	filp->private_data = pn544_dev;
	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));
	return 0;
}

static long pn544_dev_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	p61_access_state_t current_state = P61_STATE_INVALID;

	pr_debug("%s : cmd = %d, arg = %ld\n", __func__, cmd, arg);
	p61_access_lock(pn544_dev);

	switch (cmd) {
	case PN544_SET_PWR:
		current_state = P61_STATE_INVALID;
		p61_get_access_state(pn544_dev, &current_state);
		if (arg == 2) {
			if (current_state & (P61_STATE_SPI | P61_STATE_SPI_PRIO)) {
				/* NFCC fw/download should not be allowed if p61 is used by SPI */
				pr_info("%s NFCC should not be allowed to reset/FW download \n", __func__);
				p61_access_unlock(pn544_dev);
				return -EBUSY;	/* Device or resource busy */
			}
			pn544_dev->nfc_ven_enabled = true;
			if (pn544_dev->spi_ven_enabled == false) {
				/* power on with firmware download (requires hw reset) */
				pr_info("%s power on with firmware\n", __func__);
				gpio_set_value(pn544_dev->ven_gpio, 1);
				msleep(20);
				p61_update_access_state(pn544_dev, P61_STATE_DWNLD, true);
				gpio_set_value(pn544_dev->firm_gpio, 1);
				msleep(20);
				gpio_set_value(pn544_dev->ven_gpio, 0);
				msleep(100);
				gpio_set_value(pn544_dev->ven_gpio, 1);
				msleep(20);
			}
			enable_irq_wake(pn544_dev->client->irq);
		} else if (arg == 1) {
			/* power on */
			pr_info("%s power on\n", __func__);
			if ((current_state & (P61_STATE_WIRED | P61_STATE_SPI | P61_STATE_SPI_PRIO)) == 0)
				p61_update_access_state(pn544_dev, P61_STATE_IDLE, true);
			gpio_set_value(pn544_dev->firm_gpio, 0);

			pn544_dev->nfc_ven_enabled = true;
			if (pn544_dev->spi_ven_enabled == false) {
				gpio_set_value(pn544_dev->ven_gpio, 1);
				msleep(100);
			}
			enable_irq_wake(pn544_dev->client->irq);
		} else if (arg == 0) {
			/* power off */
			pr_info("%s power off\n", __func__);
			if ((current_state & (P61_STATE_WIRED | P61_STATE_SPI | P61_STATE_SPI_PRIO)) == 0)
				p61_update_access_state(pn544_dev, P61_STATE_IDLE, true);
			gpio_set_value(pn544_dev->firm_gpio, 0);

			pn544_dev->nfc_ven_enabled = false;
			/* Don't change Ven state if spi made it high */
			if (pn544_dev->spi_ven_enabled == false) {
				gpio_set_value(pn544_dev->ven_gpio, 0);
				msleep(100);
			}
			disable_irq_wake(pn544_dev->client->irq);
		} else {
			pr_err("%s bad arg %lu\n", __func__, arg);
			/* changed the p61 state to idle */
			p61_access_unlock(pn544_dev);
			return -EINVAL;
		}
		break;
	case P61_SET_SPI_PWR:
		current_state = P61_STATE_INVALID;
		p61_get_access_state(pn544_dev, &current_state);
		if (arg == 1) {
			pr_info("%s : PN61_SET_SPI_PWR - power on ese\n", __func__);
			if ((current_state & (P61_STATE_SPI | P61_STATE_SPI_PRIO)) == 0) {
				p61_update_access_state(pn544_dev, P61_STATE_SPI, true);
				pn544_dev->spi_ven_enabled = true;
				if (pn544_dev->nfc_ven_enabled == false) {
					/* provide power to NFCC if, NFC service not provided */
					gpio_set_value(pn544_dev->ven_gpio, 1);
					msleep(10);
				}
				/* pull the gpio to high once NFCC is power on */
				//gpio_set_value(pn544_dev->ese_pwr_gpio, 1);
			} else {
				pr_info("%s : PN61_SET_SPI_PWR -  power on ese failed \n", __func__);
				p61_access_unlock(pn544_dev);
				return -EBUSY;	/* Device or resource busy */
			}
		} else if (arg == 0) {
			pr_info("%s : PN61_SET_SPI_PWR - power off ese\n", __func__);
			if (current_state & P61_STATE_SPI_PRIO) {
				p61_update_access_state(pn544_dev, P61_STATE_SPI_PRIO, false);
				if (current_state & P61_STATE_WIRED) {
					if (pn544_dev->nfc_service_pid) {
						pr_info("nfc service pid %s   ---- %ld", __func__, pn544_dev->nfc_service_pid);
						signal_handler(P61_STATE_SPI_PRIO_END, pn544_dev->nfc_service_pid);
					} else
						pr_info(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__,
						     pn544_dev->nfc_service_pid);
				}
				//if (!(current_state & P61_STATE_WIRED))
				//	gpio_set_value(pn544_dev->ese_pwr_gpio, 0);
				pn544_dev->spi_ven_enabled = false;
				if (pn544_dev->nfc_ven_enabled == false) {
					gpio_set_value(pn544_dev->ven_gpio, 0);
					msleep(10);
				}
			} else if (current_state & P61_STATE_SPI) {
				p61_update_access_state(pn544_dev, P61_STATE_SPI, false);
				//if (!(current_state & P61_STATE_WIRED))
				//	gpio_set_value(pn544_dev->ese_pwr_gpio, 0);
				pn544_dev->spi_ven_enabled = false;
				if (pn544_dev->nfc_ven_enabled == false) {
					gpio_set_value(pn544_dev->ven_gpio, 0);
					msleep(10);
				}
			} else {
				pr_err("%s : PN61_SET_SPI_PWR - failed, current_state = %x \n",
				     __func__, pn544_dev->p61_current_state);
				p61_access_unlock(pn544_dev);
				return -EPERM;	/* Operation not permitted */
			}
		} else if (arg == 2) {
			pr_info("%s : PN61_SET_SPI_PWR - reset\n", __func__);
			if (current_state & (P61_STATE_IDLE | P61_STATE_SPI | P61_STATE_SPI_PRIO)) {
				if (pn544_dev->spi_ven_enabled == false) {
					pn544_dev->spi_ven_enabled = true;
					if (pn544_dev->nfc_ven_enabled == false) {
						/* provide power to NFCC if, NFC service not provided */
						gpio_set_value(pn544_dev->ven_gpio, 1);
						msleep(10);
					}
				}
				//gpio_set_value(pn544_dev->ese_pwr_gpio, 0);
				msleep(10);
				//gpio_set_value(pn544_dev->ese_pwr_gpio, 1);
				msleep(10);
			} else {
				pr_info("%s : PN61_SET_SPI_PWR - reset  failed \n", __func__);
				p61_access_unlock(pn544_dev);
				return -EBUSY;	/* Device or resource busy */
			}
		} else if (arg == 3) {
			pr_info("%s : PN61_SET_SPI_PWR - Prio Session Start power on ese\n", __func__);
			if ((current_state & (P61_STATE_SPI | P61_STATE_SPI_PRIO)) == 0) {
				p61_update_access_state(pn544_dev, P61_STATE_SPI_PRIO, true);
				if (current_state & P61_STATE_WIRED) {
					if (pn544_dev->nfc_service_pid) {
						pr_info("nfc service pid %s   ---- %ld", __func__, pn544_dev->nfc_service_pid);
						signal_handler(P61_STATE_SPI_PRIO, pn544_dev->nfc_service_pid);
					} else
						pr_info(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__,
						     pn544_dev->nfc_service_pid);
				}
				pn544_dev->spi_ven_enabled = true;
				if (pn544_dev->nfc_ven_enabled == false) {
					/* provide power to NFCC if, NFC service not provided */
					gpio_set_value(pn544_dev->ven_gpio, 1);
					msleep(10);
				}
				/* pull the gpio to high once NFCC is power on */
				//gpio_set_value(pn544_dev->ese_pwr_gpio, 1);
			} else {
				pr_info("%s : Prio Session Start power on ese failed \n", __func__);
				p61_access_unlock(pn544_dev);
				return -EBUSY;	/* Device or resource busy */
			}
		} else if (arg == 4) {
			if (current_state & P61_STATE_SPI_PRIO) {
				pr_info("%s : PN61_SET_SPI_PWR - Prio Session Ending...\n", __func__);
				p61_update_access_state(pn544_dev, P61_STATE_SPI_PRIO, false);
				/*after SPI prio timeout, the state is changing from SPI prio to SPI */
				p61_update_access_state(pn544_dev, P61_STATE_SPI, true);
				if (current_state & P61_STATE_WIRED) {
					if (pn544_dev->nfc_service_pid) {
						pr_info("nfc service pid %s   ---- %ld", __func__, pn544_dev->nfc_service_pid);
						signal_handler(P61_STATE_SPI_PRIO_END, pn544_dev->nfc_service_pid);
					} else
						pr_info(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__,
						     pn544_dev->nfc_service_pid);
				}
			} else {
				pr_info("%s : PN61_SET_SPI_PWR -  Prio Session End failed \n", __func__);
				p61_access_unlock(pn544_dev);
				return -EBADRQC;	/* Device or resource busy */
			}
		} else {
			pr_info("%s bad ese pwr arg %lu\n", __func__, arg);
			p61_access_unlock(pn544_dev);
			return -EBADRQC;	/* Invalid request code */
		}
		break;

	case P61_GET_PWR_STATUS:
		current_state = P61_STATE_INVALID;
		p61_get_access_state(pn544_dev, &current_state);
		pr_info("%s: P61_GET_PWR_STATUS  = %x", __func__, current_state);
		put_user(current_state, (int __user *)arg);
		break;

	case P61_SET_WIRED_ACCESS:
		current_state = P61_STATE_INVALID;
		p61_get_access_state(pn544_dev, &current_state);
		if (arg == 1) {
			if (current_state) {
				pr_info("%s : P61_SET_WIRED_ACCESS - enabling\n", __func__);
				p61_update_access_state(pn544_dev, P61_STATE_WIRED, true);
				if (current_state & P61_STATE_SPI_PRIO) {
					if (pn544_dev->nfc_service_pid) {
						pr_info("nfc service pid %s   ---- %ld", __func__, pn544_dev->nfc_service_pid);
						signal_handler(P61_STATE_SPI_PRIO, pn544_dev->nfc_service_pid);
					} else
						pr_info(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__,
						     pn544_dev->nfc_service_pid);
				}
				//if ((current_state & (P61_STATE_SPI | P61_STATE_SPI_PRIO)) == 0)
				//	gpio_set_value(pn544_dev->ese_pwr_gpio, 1);
			} else {
				pr_info("%s : P61_SET_WIRED_ACCESS -  enabling failed \n", __func__);
				p61_access_unlock(pn544_dev);
				return -EBUSY;	/* Device or resource busy */
			}

		} else if (arg == 0) {
			pr_info("%s : P61_SET_WIRED_ACCESS - disabling \n", __func__);
			if (current_state & P61_STATE_WIRED) {
				p61_update_access_state(pn544_dev, P61_STATE_WIRED, false);
				//if ((current_state & (P61_STATE_SPI | P61_STATE_SPI_PRIO)) == 0)
				//	gpio_set_value(pn544_dev->ese_pwr_gpio, 0);
			} else {
				pr_err("%s : P61_SET_WIRED_ACCESS - failed, current_state = %x \n",
				     __func__, pn544_dev->p61_current_state);
				p61_access_unlock(pn544_dev);
				return -EPERM;	/* Operation not permitted */
			}

		} else {
			pr_info("%s P61_SET_WIRED_ACCESS - bad arg %lu\n", __func__, arg);
			p61_access_unlock(pn544_dev);
			return -EBADRQC;	/* Invalid request code */
		}
		break;
	case P544_SET_NFC_SERVICE_PID:
		pr_info("%s : The NFC Service PID is %ld\n", __func__, arg);
		pn544_dev->nfc_service_pid = arg;
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		p61_access_unlock(pn544_dev);
		return -EINVAL;
	}
	p61_access_unlock(pn544_dev);
	return 0;
}

static const struct file_operations pn544_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = pn544_dev_read,
	.write = pn544_dev_write,
	.open = pn544_dev_open,
	.unlocked_ioctl = pn544_dev_ioctl,
};

static int pn544_pm_callback(struct notifier_block *this, unsigned long event,
			     void *v)
{
	struct pn544_dev *pn544_dev =
	    container_of(this, struct pn544_dev, pm_notifier);

	switch (event) {
	case PM_SUSPEND_PREPARE:	/* try to suspending */
	case PM_POST_SUSPEND:	/* back from suspended */
		if (gpio_get_value(pn544_dev->irq_gpio)) {
			pr_info("########nfc irq trigered########\n");
			wake_lock_timeout(&pn544_dev->wake_lock, 5 * HZ);
		}
		break;
	}
	return NOTIFY_OK;
}

static int pn544_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	int ret, gpio;
	struct pn544_dev *pn544_dev;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;

	dev_info(dev, "nxp i2c device probe...\n");

	if (!np) {
		dev_err(dev, "no DT node for nfc\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}

	pn544_dev = kzalloc(sizeof(*pn544_dev), GFP_KERNEL);
	if (pn544_dev == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		return -ENOMEM;
	}
	//IRQ 
	gpio = of_get_gpio(np, 0);
	ret = gpio_request(gpio, "nfc_int");
	if (ret) {
		pr_err("gpio_nfc_int request error\n");
		ret = -ENODEV;
		goto gpio_req_fail;
	}
	pn544_dev->irq_gpio = gpio;

	//FIRM
	gpio = of_get_gpio(np, 1);
	ret = gpio_request(gpio, "nfc_firm");
	if (ret) {
		pr_err("gpio_nfc_firm request error\n");
		ret = -ENODEV;
		goto gpio_req_fail;
	}
	gpio_direction_output(gpio, 0);
	pn544_dev->firm_gpio = gpio;

	//VEN
	gpio = of_get_gpio(np, 2);
	ret = gpio_request(gpio, "nfc_ven");
	if (ret) {
		pr_err("gpio_nfc_ven request error\n");
		ret = -ENODEV;
		goto gpio_req_fail;
	}
	gpio_direction_output(gpio, 0);
	pn544_dev->ven_gpio = gpio;

	//TODO: check ese_pwr pin
	//pn544_dev->ese_pwr_gpio = platform_data->ese_pwr_gpio;

	pn544_dev->p61_current_state = P61_STATE_IDLE;
	pn544_dev->nfc_ven_enabled = false;
	pn544_dev->spi_ven_enabled = false;
	pn544_dev->client = client;
	client->irq = gpio_to_irq(pn544_dev->irq_gpio);

	//ret = gpio_direction_output(pn544_dev->ese_pwr_gpio, 0);
	//if (ret < 0) {
	//      pr_err("%s : not able to set ese_pwr gpio as output\n",
	//              __func__);
	//      goto err_ese_pwr;
	//}

	/* init mutex and queues */
	init_waitqueue_head(&pn544_dev->read_wq);
	mutex_init(&pn544_dev->read_mutex);
	mutex_init(&pn544_dev->p61_state_mutex);
	spin_lock_init(&pn544_dev->irq_enabled_lock);
	pn544_dev->pn544_device.minor = MISC_DYNAMIC_MINOR;
	pn544_dev->pn544_device.name = "pn544";
	pn544_dev->pn544_device.fops = &pn544_dev_fops;

	ret = misc_register(&pn544_dev->pn544_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	pn544_dev->irq_enabled = true;
	ret =
	    request_irq(client->irq, pn544_dev_irq_handler, IRQF_TRIGGER_HIGH,
			client->name, pn544_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}

	pn544_dev->vdd18_nfc = devm_regulator_get(dev, "vdd18_nfc");
	if (IS_ERR(pn544_dev->vdd18_nfc))
		dev_err(dev, "vdd18_nfc is not available\n");
	else
		ret = regulator_enable(pn544_dev->vdd18_nfc);

	wake_lock_init(&pn544_dev->wake_lock, WAKE_LOCK_SUSPEND, "nfc_wake");
	pn544_dev->pm_notifier.notifier_call = pn544_pm_callback;
	register_pm_notifier(&pn544_dev->pm_notifier);

	pn544_disable_irq(pn544_dev);
	i2c_set_clientdata(client, pn544_dev);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn544_dev->pn544_device);
err_misc_register:
	mutex_destroy(&pn544_dev->read_mutex);
	mutex_destroy(&pn544_dev->p61_state_mutex);
gpio_req_fail:
	if (pn544_dev->firm_gpio)
		gpio_free(pn544_dev->firm_gpio);
	if (pn544_dev->ven_gpio)
		gpio_free(pn544_dev->ven_gpio);
	if (pn544_dev->irq_gpio)
		gpio_free(pn544_dev->irq_gpio);
	kfree(pn544_dev);
	return ret;
}

static int pn544_remove(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev;

	pn544_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn544_dev);
	misc_deregister(&pn544_dev->pn544_device);
	unregister_pm_notifier(&pn544_dev->pm_notifier);
	wake_lock_destroy(&pn544_dev->wake_lock);
	mutex_destroy(&pn544_dev->read_mutex);
	mutex_destroy(&pn544_dev->p61_state_mutex);
	gpio_free(pn544_dev->irq_gpio);
	gpio_free(pn544_dev->ven_gpio);
	//gpio_free(pn544_dev->ese_pwr_gpio);
	pn544_dev->p61_current_state = P61_STATE_INVALID;
	pn544_dev->nfc_ven_enabled = false;
	pn544_dev->spi_ven_enabled = false;

	if (pn544_dev->firm_gpio)
		gpio_free(pn544_dev->firm_gpio);
	kfree(pn544_dev);

	return 0;
}

static void pn544_shutdown(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev = i2c_get_clientdata(client);

	gpio_direction_output(pn544_dev->ven_gpio, 0);
	regulator_disable(pn544_dev->vdd18_nfc);

	pr_info("[NFC]disable vdd18_nfc output\n");
}

static struct of_device_id pn547_dt_id[] = {
	{.compatible = "nxp,pn547_nfc"},
	{}
};

static const struct i2c_device_id pn544_id[] = {
	{"nxp,pn547_nfc", 0},
	{}
};

static struct i2c_driver pn544_driver = {
	.id_table = pn544_id,
	.probe = pn544_probe,
	.remove = pn544_remove,
	.shutdown = pn544_shutdown,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "pn547-nfc",
		   .of_match_table = of_match_ptr(pn547_dt_id),
		   },
};

module_i2c_driver(pn544_driver);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN544 driver");
MODULE_LICENSE("GPL");
