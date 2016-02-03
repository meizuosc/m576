 /*
  * Copyright (C) 2015 NXP Semiconductors
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *      http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */

 /**
 * \addtogroup spi_driver
 *
 * @{ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>

#include <linux/of.h>
#include <linux/of_gpio.h>

/* Device driver's configuration macro */
/* Macro to configure poll/interrupt based req*/
//#undef P61_IRQ_ENABLE
#define P61_IRQ_ENABLE

/* Macro to configure Hard/Soft reset to P61 */
//#define P61_HARD_RESET
//#undef P61_HARD_RESET

#ifdef P61_HARD_RESET
static struct regulator *p61_regulator = NULL;
#endif

/* Macro to define SPI clock frequency */

//#define P61_SPI_CLOCK_7Mzh
#undef P61_SPI_CLOCK_7Mzh

#ifdef P61_SPI_CLOCK_7Mzh
#define P61_SPI_CLOCK     7000000L;
#else
#define P61_SPI_CLOCK     4000000L;
#endif

/* size of maximum read/write buffer supported by driver */
#define MAX_BUFFER_SIZE   258U

/* Different driver debug lever */
enum P61_DEBUG_LEVEL {
	P61_DEBUG_OFF,
	P61_FULL_DEBUG
};

#define P61_MAGIC 0xEA
#define P61_SET_PWR _IOW(P61_MAGIC, 0x01, unsigned int)
#define P61_SET_DBG _IOW(P61_MAGIC, 0x02, unsigned int)
#define P61_SET_POLL _IOW(P61_MAGIC, 0x03, unsigned int)

/* Variable to store current debug level request by ioctl */
static unsigned char debug_level = P61_FULL_DEBUG;

#define P61_DBG_MSG(msg...)  \
        switch(debug_level)      \
        {                        \
        case P61_DEBUG_OFF:      \
        break;                 \
        case P61_FULL_DEBUG:     \
        printk(KERN_INFO "[NXP-P61] :  " msg); \
        break; \
        default:                 \
        printk(KERN_ERR "[NXP-P61] :  Wrong debug level %d", debug_level); \
        break; \
        } \

#define P61_ERR_MSG(msg...) printk(KERN_ERR "[NFC-P61] : " msg );


/* Device specific macro and structure */
struct p61_dev {
	wait_queue_head_t read_wq;	/* wait queue for read interrupt */
	struct mutex read_mutex;	/* read mutex */
	struct mutex write_mutex;	/* write mutex */
	struct spi_device *spi;	/* spi device structure */
	struct miscdevice p61_device;	/* char device as misc driver */
	unsigned int rst_gpio;	/* SW Reset gpio */
	unsigned int irq_gpio;	/* P61 will interrupt DH for any ntf */
	bool irq_enabled;	/* flag to indicate irq is used */
	unsigned char enable_poll_mode;	/* enable the poll mode */
	spinlock_t irq_enabled_lock;	/*spin lock for read irq */
};

/* T==1 protocol specific global data */
const unsigned char SOF = 0xA5u;

/**
 * \ingroup spi_driver
 * \brief Called from SPI LibEse to initilaize the P61 device
 *
 * \param[in]       struct inode *
 * \param[in]       struct file *
 *
 * \retval 0 if ok.
 *
*/

static int p61_dev_open(struct inode *inode, struct file *filp)
{

	struct p61_dev
	*p61_dev = container_of(filp->private_data,
				struct p61_dev,
				p61_device);

	filp->private_data = p61_dev;
	P61_DBG_MSG("%s : Major No: %d, Minor No: %d\n", __func__,
		    imajor(inode), iminor(inode));

	return 0;
}

/**
 * \ingroup spi_driver
 * \brief To configure the P61_SET_PWR/P61_SET_DBG/P61_SET_POLL
 * \n         P61_SET_PWR - hard reset (arg=2), soft reset (arg=1)
 * \n         P61_SET_DBG - Enable/Disable (based on arg value) the driver logs
 * \n         P61_SET_POLL - Configure the driver in poll (arg = 1), interrupt (arg = 0) based read operation
 * \param[in]       struct file *
 * \param[in]       unsigned int
 * \param[in]       unsigned long
 *
 * \retval 0 if ok.
 *
*/

static long p61_dev_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	int ret = 0;
	struct p61_dev *p61_dev = NULL;

	unsigned char buf[100];

	P61_DBG_MSG("p61_dev_ioctl-Enter %u arg = %ld\n", cmd, arg);
	p61_dev = filp->private_data;

	switch (cmd) {
	case P61_SET_PWR:
		if (arg == 2) {
#ifdef P61_HARD_RESET
			P61_DBG_MSG(" Disabling p61_regulator");
			if (p61_regulator != NULL) {
				regulator_disable(p61_regulator);
				msleep(50);
				regulator_enable(p61_regulator);
				P61_DBG_MSG(" Enabling p61_regulator");
			} else {
				P61_ERR_MSG(" ERROR : p61_regulator is not enabled");
			}
#else
			P61_DBG_MSG(" Soft Reset");
			gpio_set_value(p61_dev->rst_gpio, 1);
			P61_DBG_MSG("p61_dev_ioctl-1\n");
			msleep(20);
			gpio_set_value(p61_dev->rst_gpio, 0);
			P61_DBG_MSG("p61_dev_ioctl-0\n");
			msleep(50);
			ret = spi_read(p61_dev->spi, (void *)buf, sizeof(buf));
			msleep(50);
			gpio_set_value(p61_dev->rst_gpio, 1);
			P61_DBG_MSG("p61_dev_ioctl-1 \n");
			msleep(20);
#endif

		} else if (arg == 1) {
			P61_DBG_MSG(" Soft Reset");
			//gpio_set_value(p61_dev->rst_gpio, 1);
			//msleep(20);
			gpio_set_value(p61_dev->rst_gpio, 0);
			msleep(50);
			ret = spi_read(p61_dev->spi, (void *)buf, sizeof(buf));
			msleep(50);
			gpio_set_value(p61_dev->rst_gpio, 1);
			msleep(20);

		}
		break;

	case P61_SET_DBG:
		debug_level = (unsigned char)arg;
		P61_DBG_MSG("[NXP-P61] -  Debug level %d", debug_level);
		break;

	case P61_SET_POLL:

		p61_dev->enable_poll_mode = (unsigned char)arg;
		if (p61_dev->enable_poll_mode == 0) {
			P61_DBG_MSG("[NXP-P61] - IRQ Mode is set \n");
		} else {
			P61_DBG_MSG("[NXP-P61] - Poll Mode is set \n");
			p61_dev->enable_poll_mode = 1;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Write data to P61 on SPI
 *
 * \param[in]       struct file *
 * \param[in]       const char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval data size
 *
*/

static ssize_t p61_dev_write(struct file *filp, const char *buf, size_t count,
			     loff_t * offset)
{

	int ret = -1;
	struct p61_dev *p61_dev;
	unsigned char tx_buffer[MAX_BUFFER_SIZE];

	P61_DBG_MSG("p61_dev_write -Enter count %lu\n", count);

	p61_dev = filp->private_data;

	mutex_lock(&p61_dev->write_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(&tx_buffer[0], 0, sizeof(tx_buffer));
	if (copy_from_user(&tx_buffer[0], &buf[0], count)) {
		P61_ERR_MSG("%s : failed to copy from user space\n", __func__);
		mutex_unlock(&p61_dev->write_mutex);
		return -EFAULT;
	}

	/* Write data */
	ret = spi_write(p61_dev->spi, &tx_buffer[0], count);
	if (ret < 0) {
		ret = -EIO;
	} else {
		ret = count;
	}

	mutex_unlock(&p61_dev->write_mutex);
	P61_DBG_MSG("p61_dev_write ret %d- Exit \n", ret);
	return ret;
}

#ifdef P61_IRQ_ENABLE

/**
 * \ingroup spi_driver
 * \brief To disable IRQ
 *
 * \param[in]       struct p61_dev *
 *
 * \retval void
 *
*/

static void p61_disable_irq(struct p61_dev *p61_dev)
{
	unsigned long flags;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

	spin_lock_irqsave(&p61_dev->irq_enabled_lock, flags);
	if (p61_dev->irq_enabled) {
		disable_irq_nosync(p61_dev->spi->irq);
		p61_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&p61_dev->irq_enabled_lock, flags);

	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
}

/**
 * \ingroup spi_driver
 * \brief Will get called when interrupt line asserted from P61
 *
 * \param[in]       int
 * \param[in]       void *
 *
 * \retval IRQ handle
 *
*/

static irqreturn_t p61_dev_irq_handler(int irq, void *dev_id)
{
	struct p61_dev *p61_dev = dev_id;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
	p61_disable_irq(p61_dev);

	/* Wake up waiting readers */
	wake_up(&p61_dev->read_wq);

	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return IRQ_HANDLED;
}
#endif

/**
 * \ingroup spi_driver
 * \brief Used to read data from P61 in Poll/interrupt mode configured using ioctl call
 *
 * \param[in]       struct file *
 * \param[in]       char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval read size
 *
*/

static ssize_t p61_dev_read(struct file *filp, char *buf, size_t count,
			    loff_t * offset)
{
	int ret = -EIO, i;
	struct p61_dev *p61_dev = filp->private_data;
	unsigned char sof = 0x00;
	int total_count = 0;
	unsigned char rx_buffer[MAX_BUFFER_SIZE];

	P61_DBG_MSG("p61_dev_read count %lu - Enter \n", count);

	if (count < MAX_BUFFER_SIZE) {
		P61_ERR_MSG("Invalid length (min : 258) [%lu] \n",
			    count);
		return -EINVAL;
	}

	mutex_lock(&p61_dev->read_mutex);
	if (count > MAX_BUFFER_SIZE) {
		count = MAX_BUFFER_SIZE;
	}

	memset(&rx_buffer[0], 0x00, sizeof(rx_buffer));

	if (p61_dev->enable_poll_mode) {
		P61_DBG_MSG(" %s Poll Mode Enabled \n", __FUNCTION__);

		do {
			sof = 0x00;
			P61_DBG_MSG("SPI_READ returned 0x%x", sof);
			ret = spi_read(p61_dev->spi, (void *)&sof, 1);
			if (0 > ret) {
				P61_ERR_MSG("spi_read failed:%d [SOF] \n", ret);
				goto fail;
			}
			P61_DBG_MSG("SPI_READ returned 0x%x", sof);
			/* if SOF not received, give some time to P61 */
			/* RC put the conditional delay only if SOF not received */
			if (sof != SOF)
				usleep_range(1000, 1500);	//msleep(5);
		} while (sof != SOF);
	} else {
#ifdef P61_IRQ_ENABLE
		P61_DBG_MSG(" %s Interrrupt Mode Enabled \n", __FUNCTION__);
		if (!gpio_get_value(p61_dev->irq_gpio)) {
			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				goto fail;
			}
			while (1) {
				P61_DBG_MSG(" %s waiting for interrupt \n",
					    __FUNCTION__);
				p61_dev->irq_enabled = true;
				enable_irq(p61_dev->spi->irq);
				ret =
				    wait_event_interruptible(p61_dev->read_wq,
							     !p61_dev->
							     irq_enabled);
				p61_disable_irq(p61_dev);
				if (ret) {
					P61_ERR_MSG
					    ("wait_event_interruptible() : Failed\n");
					goto fail;
				}

				if (gpio_get_value(p61_dev->irq_gpio))
					break;

				P61_ERR_MSG("%s: spurious interrupt detected\n",
					    __func__);
			}
		}
#else
		P61_ERR_MSG(" %s P61_IRQ_ENABLE not Enabled \n", __FUNCTION__);
#endif
		i = 0;
		do {
		i ++;
		/* read the SOF */
		sof = 0x00;
		ret = spi_read(p61_dev->spi, (void *)&sof, 1);
		if ((0 > ret) || (sof != SOF)) {
			P61_DBG_MSG("SPI_READ returned 0x%x", sof);
			P61_ERR_MSG("spi_read failed [SOF] 0x%x\n",
				    ret);
			ret = -EIO;
			//goto fail;
		}
		msleep(1);
		} while (!sof && (i < 5));

		if (sof != SOF)
			goto fail;
	}

	total_count = 1;
	rx_buffer[0] = sof;
	/* Read the HEADR of Two bytes */
	ret = spi_read(p61_dev->spi, (void *)&rx_buffer[1], 2);
	if (ret < 0) {
		P61_ERR_MSG("spi_read fails after [PCB] \n");
		ret = -EIO;
		goto fail;
	}

	total_count += 2;
	/* Get the data length */
	count = rx_buffer[2];
	P61_DBG_MSG("Data Lenth = %lu", count);
	/* Read the availabe data along with one byte LRC */
	ret = spi_read(p61_dev->spi, (void *)&rx_buffer[3], (count + 1));
	if (ret < 0) {
		P61_ERR_MSG("spi_read failed \n");
		ret = -EIO;
		goto fail;
	}
	total_count = (total_count + (count + 1));
	P61_DBG_MSG("total_count = %d", total_count);

	if (copy_to_user(buf, &rx_buffer[0], total_count)) {
		P61_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	ret = total_count;
	P61_DBG_MSG("p61_dev_read ret %d Exit\n", ret);

	mutex_unlock(&p61_dev->read_mutex);

	return ret;

fail:
	P61_ERR_MSG("Error p61_dev_read ret %d Exit\n", ret);
	mutex_unlock(&p61_dev->read_mutex);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief It will configure the GPIOs required for soft reset, read interrupt & regulated power supply to P61.
 *
 * \param[in]       struct p61_spi_platform_data *
 * \param[in]       struct p61_dev *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/

static int p61_hw_setup(struct p61_dev *p61_dev, struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	int ret = -1;
	int gpio;

	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);
	if (!dev->of_node) {
		P61_ERR_MSG("Err : dts %s\n", __FUNCTION__);
		return ret;
	}

#ifdef P61_IRQ_ENABLE
	gpio = of_get_named_gpio(dev->of_node, "nxp,spi-irq-gpio", 0);
	ret = gpio_request(gpio, "p61_irq");
	if (ret < 0) {
		P61_ERR_MSG("request failed gpio = 0x%x\n", gpio);
		goto fail;
	}
	p61_dev->irq_gpio = gpio;
#endif

#ifdef P61_HARD_RESET
	/* RC : platform specific settings need to be declare */
	p61_regulator = regulator_get(&spi->dev, "vaux3");
	if (IS_ERR(p61_regulator)) {
		ret = PTR_ERR(p61_regulator);
		P61_ERR_MSG(" Error to get vaux3 (error code) = %d\n", ret);
		return -ENODEV;
	} else {
		P61_DBG_MSG("successfully got regulator\n");
	}

	ret = regulator_set_voltage(p61_regulator, 1800000, 1800000);
	if (ret != 0) {
		P61_ERR_MSG("Error setting the regulator voltage %d\n", ret);
		regulator_put(p61_regulator);
		return ret;
	} else {
		regulator_enable(p61_regulator);
		P61_DBG_MSG("successfully set regulator voltage\n");

	}
#endif
	gpio = of_get_named_gpio(dev->of_node, "nxp,spi-reset-gpio", 0);
	ret = gpio_request(gpio, "p61_reset");
	if (ret < 0) {
		P61_ERR_MSG("gpio request failed = 0x%x\n", gpio);
		goto fail_gpio;
	}
	ret = gpio_direction_output(gpio, 0);
	if (ret < 0) {
		P61_ERR_MSG("gpio request failed gpio = 0x%x\n", gpio);
		goto fail_gpio;
	}
	p61_dev->rst_gpio = gpio;

	ret = 0;
	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return ret;

fail_gpio:
#ifdef P61_IRQ_ENABLE
fail:
	P61_ERR_MSG("p61_hw_setup failed\n");
#endif
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Set the P61 device specific context for future use.
 *
 * \param[in]       struct spi_device *
 * \param[in]       void *
 *
 * \retval void
 *
*/

static inline void p61_set_data(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

/**
 * \ingroup spi_driver
 * \brief Get the P61 device specific context.
 *
 * \param[in]       const struct spi_device *
 *
 * \retval Device Parameters
 *
*/

static inline void *p61_get_data(const struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

/* possible fops on the p61 device */
static const struct file_operations p61_dev_fops = {
	.owner = THIS_MODULE,
	.read = p61_dev_read,
	.write = p61_dev_write,
	.open = p61_dev_open,
	.unlocked_ioctl = p61_dev_ioctl,
};

/**
 * \ingroup spi_driver
 * \brief To probe for P61 SPI interface. If found initialize the SPI clock, bit rate & SPI mode.
          It will create the dev entry (P61) for user space.
 *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
*/

static int p61_probe(struct spi_device *spi)
{
	int ret = -1;
	struct p61_dev *p61_dev = NULL;
#ifdef P61_IRQ_ENABLE
	unsigned int irq_flags;
#endif
	P61_DBG_MSG("%s chip select : %d , bus number = %d \n",
		    __FUNCTION__, spi->chip_select, spi->master->bus_num);

	p61_dev = kzalloc(sizeof(*p61_dev), GFP_KERNEL);
	if (p61_dev == NULL) {
		P61_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	ret = p61_hw_setup(p61_dev, spi);
	if (ret < 0) {
		P61_ERR_MSG("Failed to p61_enable_P61_IRQ_ENABLE\n");
		goto err_exit0;
	}

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = P61_SPI_CLOCK;
	//spi->chip_select = SPI_NO_CS;
	ret = spi_setup(spi);
	if (ret < 0) {
		P61_ERR_MSG("failed to do spi_setup()\n");
		goto err_exit0;
	}

	p61_dev->spi = spi;
	p61_dev->p61_device.minor = MISC_DYNAMIC_MINOR;
	p61_dev->p61_device.name = "p61";
	p61_dev->p61_device.fops = &p61_dev_fops;
	p61_dev->p61_device.parent = &spi->dev;

	dev_set_drvdata(&spi->dev, p61_dev);

	/* init mutex and queues */
	init_waitqueue_head(&p61_dev->read_wq);
	mutex_init(&p61_dev->read_mutex);
	mutex_init(&p61_dev->write_mutex);
#ifdef P61_IRQ_ENABLE
	spin_lock_init(&p61_dev->irq_enabled_lock);
#endif

	ret = misc_register(&p61_dev->p61_device);
	if (ret < 0) {
		P61_ERR_MSG("misc_register failed! %d\n", ret);
		goto err_exit0;
	}
#ifdef P61_IRQ_ENABLE
	p61_dev->spi->irq = gpio_to_irq(p61_dev->irq_gpio);
	if (p61_dev->spi->irq < 0) {
		P61_ERR_MSG("gpio_to_irq request failed gpio = 0x%x\n",
			    p61_dev->irq_gpio);
		goto err_exit1;
	}
	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	p61_dev->irq_enabled = true;
	irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

	ret = request_irq(p61_dev->spi->irq, p61_dev_irq_handler,
			  irq_flags, p61_dev->p61_device.name, p61_dev);
	if (ret) {
		P61_ERR_MSG("request_irq failed\n");
		goto err_exit1;
	}
	p61_disable_irq(p61_dev);
#endif
	p61_dev->enable_poll_mode = 0;	/* Default IRQ read mode */
	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return ret;
err_exit1:
	misc_deregister(&p61_dev->p61_device);
err_exit0:
	mutex_destroy(&p61_dev->read_mutex);
	mutex_destroy(&p61_dev->write_mutex);
	if (p61_dev != NULL)
		kfree(p61_dev);
err_exit:
	P61_DBG_MSG("ERROR: Exit : %s ret %d\n", __FUNCTION__, ret);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Will get called when the device is removed to release the resources.
 *
 * \param[in]       struct spi_device
 *
 * \retval 0 if ok.
 *
*/

static int p61_remove(struct spi_device *spi)
{
	struct p61_dev *p61_dev = p61_get_data(spi);
	P61_DBG_MSG("Entry : %s\n", __FUNCTION__);

#ifdef P61_HARD_RESET
	if (p61_regulator != NULL) {
		regulator_disable(p61_regulator);
		regulator_put(p61_regulator);
	} else {
		P61_ERR_MSG("ERROR %s p61_regulator not enabled \n",
			    __FUNCTION__);
	}
#endif
	gpio_free(p61_dev->rst_gpio);

#ifdef P61_IRQ_ENABLE
	free_irq(p61_dev->spi->irq, p61_dev);
	gpio_free(p61_dev->irq_gpio);
#endif

	mutex_destroy(&p61_dev->read_mutex);
	misc_deregister(&p61_dev->p61_device);

	if (p61_dev != NULL)
		kfree(p61_dev);
	P61_DBG_MSG("Exit : %s\n", __FUNCTION__);
	return 0;
}

static struct of_device_id p61_of_match_table[] = {
	{.compatible = "nxp_ese_spi",},
	{},
};

MODULE_DEVICE_TABLE(of, p61_of_match_table);

static struct spi_driver p61_driver = {
	.driver = {
		   .name = "nxp_ese_spi",
		   .owner = THIS_MODULE,
		   .of_match_table = p61_of_match_table,
		   },
	.probe = p61_probe,
	.remove = p61_remove,
};

module_spi_driver(p61_driver);

MODULE_AUTHOR("BHUPENDRA PAWAR");
MODULE_DESCRIPTION("NXP P61 SPI driver");
MODULE_LICENSE("GPL");
