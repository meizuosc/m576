/* Fingerprint Cards, Hybrid Touch sensor driver
 *
 * Copyright (c) 2014,2015 Fingerprint Cards AB <tech@fingerprints.com>
 *
 *
 * Software license : "Dual BSD/GPL"
 * see <linux/module.h> and ./Documentation
 * for  details.
 *
*/

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/rcupdate.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/fb.h>
#include <linux/meizu-sys.h>
#include <linux/input.h>
#include <asm/siginfo.h>
#include <asm/uaccess.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>


#include "fpc_irq_common.h"
#include "fpc_irq_ctrl.h"
#include "fpc_irq_supply.h"
#include "fpc_irq_pm.h"

#ifndef CONFIG_OF
// #include <linux/xxxx/fpc_irq.h> // todo
#else
#include <linux/of.h>
#include "fpc_irq.h"
#endif

MODULE_AUTHOR("Fingerprint Cards AB <tech@fingerprints.com>");
MODULE_DESCRIPTION("FPC IRQ driver.");

MODULE_LICENSE("Dual BSD/GPL");

/* -------------------------------------------------------------------------- */
/* platform compatibility                                                     */
/* -------------------------------------------------------------------------- */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
	#include <linux/interrupt.h>
	#include <linux/irqreturn.h>
	#include <linux/of_gpio.h>
#endif

#define FPC_IRQ_KEY_ANDR_BACK     KEY_FINGERPRINT

/* -------------------------------------------------------------------------- */
/* global variables                                                           */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* fpc data types                                                             */
/* -------------------------------------------------------------------------- */
struct fpc_irq_attribute {
	struct device_attribute attr;
	size_t offset;
};

static struct pm_qos_request fp_cpu_num_min_qos;
static struct pm_qos_request fp_cpu_freq_qos;
static struct pm_qos_request fp_cpu_mif_qos;
static struct pm_qos_request fp_cpu_int_qos;
#define HMP_BOOST_TIMEOUT 	(500 * MSEC_PER_SEC)
#define BIG_CORE_NUM        (2)
#define HMP_BOOST_FREQ 		1500000
#define HMP_MIF_FREQ	    1552000
#define HMP_INT_FREQ		560000

/* -------------------------------------------------------------------------- */
/* fpc_irq driver constants                                                   */
/* -------------------------------------------------------------------------- */
#define FPC_IRQ_CLASS_NAME	"fpsensor_irq"
#define FPC_IRQ_WORKER_NAME	"fpc_irq_worker"
#define FPC_PM_WORKER_NAME  "fpc_pm_worker"


/* -------------------------------------------------------------------------- */
/* function prototypes                                                        */
/* -------------------------------------------------------------------------- */
static int fpc_irq_init(void);

static void fpc_irq_exit(void);

static int fpc_irq_probe(struct platform_device *plat_dev);

static int fpc_irq_remove(struct platform_device *plat_dev);

static int fpc_irq_get_of_pdata(struct platform_device *dev,
				fpc_irq_pdata_t *pdata);

static int fpc_irq_platform_init(fpc_irq_data_t *fpc_irq_data,
				fpc_irq_pdata_t *pdata);

static int fpc_irq_platform_destroy(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_create_class(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_init(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_init(fpc_irq_data_t *fpc_irq_data);
static int fpc_irq_input_init(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_goto_idle(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_enable(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_worker_destroy(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_manage_sysfs_setup(fpc_irq_data_t *fpc_irq_data,
					bool create);

static int fpc_irq_manage_sysfs_pm(fpc_irq_data_t *fpc_irq_data,
					bool create);

int fpc_irq_wait_for_interrupt(fpc_irq_data_t *fpc_irq_data, int timeout);

irqreturn_t fpc_irq_interrupt(int irq, void *_fpc_irq_data);

static ssize_t fpc_irq_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t fpc_irq_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count);

static ssize_t fpc_irq_show_attr_pm(struct device *dev,
				struct device_attribute *attr,
				char *buf);

static ssize_t fpc_irq_store_attr_pm(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count);

static int fpc_irq_worker_function(void *_fpc_irq_data);

static int fpc_irq_enable(fpc_irq_data_t *fpc_irq_data, int req_state);

static int fpc_irq_click_event(fpc_irq_data_t *fpc_irq_data, int val);

static int fpc_irq_lock_freq(void);

/* -------------------------------------------------------------------------- */
/* External interface                                                         */
/* -------------------------------------------------------------------------- */
module_init(fpc_irq_init);
module_exit(fpc_irq_exit);

#ifdef CONFIG_OF
static struct of_device_id fpc_irq_of_match[] __devinitdata = {
	{.compatible = "fpc,fpc_irq", },
	{}
};
MODULE_DEVICE_TABLE(of, fpc_irq_of_match);
#endif

static struct platform_device *fpc_irq_platform_device;

static struct platform_driver fpc_irq_driver = {
	.driver	 = {
		.name		= FPC_IRQ_DEV_NAME,
		.owner		= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= fpc_irq_of_match,
#endif
	},
	.probe   = fpc_irq_probe,
	.remove  = fpc_irq_remove,
	//.suspend = fpc_irq_pm_suspend,
	//.resume  = fpc_irq_pm_resume
};


/* -------------------------------------------------------------------------- */
/* devfs                                                                      */
/* -------------------------------------------------------------------------- */
#define FPC_IRQ_ATTR(__grp, __field, __mode)				\
{									\
	.attr = __ATTR(__field, (__mode),				\
	fpc_irq_show_attr_##__grp,					\
	fpc_irq_store_attr_##__grp),					\
	.offset = offsetof(struct fpc_irq_##__grp, __field)		\
}

#define FPC_IRQ_DEV_ATTR(_grp, _field, _mode)				\
struct fpc_irq_attribute fpc_irq_attr_##_field =			\
					FPC_IRQ_ATTR(_grp, _field, (_mode))

#define DEVFS_MODE_RW (S_IWUSR|S_IWGRP|S_IWOTH|S_IRUSR|S_IRGRP|S_IROTH)
#define DEVFS_MODE_WO (S_IWUSR|S_IWGRP|S_IWOTH)
#define DEVFS_MODE_RO (S_IRUSR|S_IRGRP|S_IROTH)

static FPC_IRQ_DEV_ATTR(setup, dst_pid,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, dst_signo,	    DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, enabled,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, click_event,   DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, lock_freq,   DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(setup, test_trigger,	DEVFS_MODE_WO);

static struct attribute *fpc_irq_setup_attrs[] = {
	&fpc_irq_attr_dst_pid.attr.attr,
	&fpc_irq_attr_dst_signo.attr.attr,
	&fpc_irq_attr_enabled.attr.attr,
	&fpc_irq_attr_click_event.attr.attr,
	&fpc_irq_attr_lock_freq.attr.attr,
	&fpc_irq_attr_test_trigger.attr.attr,
	NULL
};

static const struct attribute_group fpc_irq_setup_attr_group = {
	.attrs = fpc_irq_setup_attrs,
	.name = "setup"
};

static FPC_IRQ_DEV_ATTR(pm, state,		DEVFS_MODE_RO);
static FPC_IRQ_DEV_ATTR(pm, supply_on,		DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(pm, hw_reset,		DEVFS_MODE_WO);
static FPC_IRQ_DEV_ATTR(pm, notify_enabled,	DEVFS_MODE_RW);
static FPC_IRQ_DEV_ATTR(pm, notify_ack,		DEVFS_MODE_WO);
static FPC_IRQ_DEV_ATTR(pm, wakeup_req,		DEVFS_MODE_WO);

static struct attribute *fpc_irq_pm_attrs[] = {
	&fpc_irq_attr_state.attr.attr,
	&fpc_irq_attr_supply_on.attr.attr,
	&fpc_irq_attr_hw_reset.attr.attr,
	&fpc_irq_attr_notify_enabled.attr.attr,
	&fpc_irq_attr_notify_ack.attr.attr,
	&fpc_irq_attr_wakeup_req.attr.attr,
	NULL
};

static const struct attribute_group fpc_irq_pm_attr_group = {
	.attrs = fpc_irq_pm_attrs,
	.name = "pm"
};


/* -------------------------------------------------------------------------- */
/* function definitions                                                       */
/* -------------------------------------------------------------------------- */
static int fpc_irq_init(void)
{
	printk(KERN_INFO "%s\n", __func__);

	fpc_irq_platform_device = platform_device_register_simple(
							FPC_IRQ_DEV_NAME,
							0,
							NULL,
							0);

	if (IS_ERR(fpc_irq_platform_device))
		return PTR_ERR(fpc_irq_platform_device);

	return (platform_driver_register(&fpc_irq_driver) != 0)? EINVAL : 0;
}


/* -------------------------------------------------------------------------- */
static void fpc_irq_exit(void)
{
	printk(KERN_INFO "%s\n", __func__);

	platform_driver_unregister(&fpc_irq_driver);

	platform_device_unregister(fpc_irq_platform_device);
}
/* -------------------------------------------------------------------------- */
static int fpc_irq_probe(struct platform_device *plat_dev)
{
	int error = 0;
	fpc_irq_data_t *fpc_irq_data = NULL;

	fpc_irq_pdata_t *pdata_ptr;
	fpc_irq_pdata_t pdata_of;

	dev_info(&plat_dev->dev, "%s\n", __func__);

	if (fpc_irq_check_instance(plat_dev->name) < 0)
		return 0;

	fpc_irq_data = kzalloc(sizeof(*fpc_irq_data), GFP_KERNEL);

	if (!fpc_irq_data) {
		dev_err(&plat_dev->dev, "failed to allocate memory for struct fpc_irq_data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(plat_dev, fpc_irq_data);

	fpc_irq_data->plat_dev = plat_dev;
	fpc_irq_data->dev = &plat_dev->dev;
	fpc_irq_data->should_sleep  = 0;
	fpc_irq_data->should_wakeup = 0;

	fpc_irq_data->pdata.irq_gpio = -EINVAL;
	fpc_irq_data->pdata.irq_no   = -EINVAL;
	fpc_irq_data->pdata.rst_gpio = -EINVAL;

	init_waitqueue_head(&fpc_irq_data->wq_enable);
	init_waitqueue_head(&fpc_irq_data->wq_irq_return);

	pdata_ptr = plat_dev->dev.platform_data;

	if (!pdata_ptr) {
		error = fpc_irq_get_of_pdata(plat_dev, &pdata_of);
		pdata_ptr = (error) ? NULL : &pdata_of;
	}

	if (error)
		goto err_1;

	if (!pdata_ptr) {
		dev_err(fpc_irq_data->dev,
				"%s: dev.platform_data is NULL.\n", __func__);

		error = -EINVAL;
	}

	if (error)
		goto err_1;

	error = fpc_irq_platform_init(fpc_irq_data, pdata_ptr);
	if (error)
		goto err_1;

	error = fpc_irq_create_class(fpc_irq_data);
	if (error)
		goto err_2;

	error = fpc_irq_manage_sysfs_setup(fpc_irq_data, true);
	if (error)
		goto err_3;

	error = fpc_irq_manage_sysfs_pm(fpc_irq_data, true);
	if (error)
		goto err_4;

	error = fpc_irq_supply_init(fpc_irq_data);
	if (error)
		goto err_5;

	error = fpc_irq_ctrl_init(fpc_irq_data, pdata_ptr);
	if (error)
		goto err_6;

	error = fpc_irq_pm_init(fpc_irq_data);
	if (error)
		goto err_7;

	error = fpc_irq_worker_init(fpc_irq_data);
	if (error)
		goto err_8;

	error = fpc_irq_input_init(fpc_irq_data);
	if (error)
		goto err_9;

	pm_qos_add_request(&fp_cpu_num_min_qos, PM_QOS_CLUSTER0_NUM_MIN, 0); /*request new qos*/
	pm_qos_add_request(&fp_cpu_freq_qos, PM_QOS_CLUSTER0_FREQ_MIN, 0);	/*request new qos*/
	pm_qos_add_request(&fp_cpu_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&fp_cpu_int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
	sema_init(&fpc_irq_data->mutex, 0);

	fpc_irq_data->setup.dst_pid      = -EINVAL;
	fpc_irq_data->setup.dst_signo    = -EINVAL;
	fpc_irq_data->setup.enabled      = false;
	fpc_irq_data->setup.test_trigger = 0;

	up(&fpc_irq_data->mutex);

	return 0;

err_9:
	input_unregister_device(fpc_irq_data->key_input_dev);
err_8:
	fpc_irq_pm_destroy(fpc_irq_data);
err_7:
	fpc_irq_ctrl_destroy(fpc_irq_data);
err_6:
	fpc_irq_supply_destroy(fpc_irq_data);
err_5:
	fpc_irq_manage_sysfs_pm(fpc_irq_data, false);
err_4:
	fpc_irq_manage_sysfs_setup(fpc_irq_data, false);
err_3:
	class_destroy(fpc_irq_data->class);
err_2:
	fpc_irq_platform_destroy(fpc_irq_data);
err_1:
	platform_set_drvdata(plat_dev, NULL);

	kfree(fpc_irq_data);

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_remove(struct platform_device *plat_dev)
{
	fpc_irq_data_t *fpc_irq_data;

	if (fpc_irq_check_instance(plat_dev->name) < 0)
		return 0;

	fpc_irq_data = platform_get_drvdata(plat_dev);

	fpc_irq_worker_destroy(fpc_irq_data);

	meizu_sysfslink_unregister("mx_fpc");
	input_unregister_device(fpc_irq_data->key_input_dev);
//err_8:
	fpc_irq_pm_destroy(fpc_irq_data);
//err_7:
	fpc_irq_ctrl_destroy(fpc_irq_data);
//err_6:
	fpc_irq_supply_destroy(fpc_irq_data);
//err_5:
	fpc_irq_manage_sysfs_pm(fpc_irq_data, false);
//err_4:
	fpc_irq_manage_sysfs_setup(fpc_irq_data, false);
//err_3:
	class_destroy(fpc_irq_data->class);
//err_2:
	fpc_irq_platform_destroy(fpc_irq_data);
//err_1:
	platform_set_drvdata(plat_dev, NULL);

	kfree(fpc_irq_data);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_get_of_pdata(struct platform_device *dev,
				fpc_irq_pdata_t *pdata)
{
	struct device_node *node = dev->dev.of_node;

	if (node == NULL) {
		dev_err(&dev->dev, "%s: Could not find OF device node\n", __func__);
		goto of_err;
	}

	pdata->irq_gpio = of_get_named_gpio(node,"fpc,gpio_irq",0);
	if (!gpio_is_valid(pdata->irq_gpio))
	{
		dev_err(&dev->dev,	"[ERROR] irq request failed.\n");
		goto of_err;
	}
	pdata->rst_gpio = of_get_named_gpio(node, "fpc,gpio_rst", 0);
	if (!gpio_is_valid(pdata->rst_gpio))
	{
		dev_err(&dev->dev,	"[ERROR] reset request failed.\n");
		goto of_err;
	}

	return 0;

of_err:
	pdata->irq_gpio = -EINVAL;
	pdata->irq_no   = -EINVAL;
	pdata->rst_gpio = -EINVAL;

	return -ENODEV;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_platform_init(fpc_irq_data_t *fpc_irq_data,
				fpc_irq_pdata_t *pdata)
{
	int error = 0;

	if (gpio_is_valid(pdata->irq_gpio)) {

		dev_info(fpc_irq_data->dev,
			"Assign IRQ -> GPIO%d\n",
			pdata->irq_gpio);

		error = gpio_request(pdata->irq_gpio, "fpc_irq");

		if (error) {
			dev_err(fpc_irq_data->dev, "gpio_request failed. error = %d\n", error);
			return error;
		}

		fpc_irq_data->pdata.irq_gpio = pdata->irq_gpio;

		error = gpio_direction_input(fpc_irq_data->pdata.irq_gpio);

		if (error) {
			dev_err(fpc_irq_data->dev, "gpio_direction_input (irq) failed.\n");
			return error;
		}
	} else {
		dev_err(fpc_irq_data->dev, "IRQ gpio not valid.\n");
		return -EINVAL;
	}

	fpc_irq_data->pdata.irq_no = gpio_to_irq(fpc_irq_data->pdata.irq_gpio);

	if (fpc_irq_data->pdata.irq_no < 0) {
		dev_err(fpc_irq_data->dev, "gpio_to_irq failed.\n");
		error = fpc_irq_data->pdata.irq_no;
		return error;
	}

	error = request_irq(fpc_irq_data->pdata.irq_no, fpc_irq_interrupt,
			IRQF_TRIGGER_RISING, "fpc_irq", fpc_irq_data);

	disable_irq(fpc_irq_data->pdata.irq_no);

	if (error) {
		dev_err(fpc_irq_data->dev,
			"request_irq %i failed.\n",
			fpc_irq_data->pdata.irq_no);

		fpc_irq_data->pdata.irq_no = -EINVAL;
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_platform_destroy(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (fpc_irq_data->pdata.irq_no >= 0)
		free_irq(fpc_irq_data->pdata.irq_no, fpc_irq_data);

	if (gpio_is_valid(fpc_irq_data->pdata.irq_gpio))
		gpio_free(fpc_irq_data->pdata.irq_gpio);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_create_class(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	fpc_irq_data->class = class_create(THIS_MODULE, FPC_IRQ_CLASS_NAME);

	if (IS_ERR(fpc_irq_data->class)) {
		dev_err(fpc_irq_data->dev, "failed to create class.\n");
		error = PTR_ERR(fpc_irq_data->class);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_init(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	//dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	fpc_irq_data->idle_request = true;
	fpc_irq_data->term_request = false;

	//sema_init(&fpc_irq_data->sem_active, 0);

	fpc_irq_data->worker_thread = kthread_run(
						fpc_irq_worker_function,
						fpc_irq_data,
						"%s", FPC_IRQ_WORKER_NAME);

	if (IS_ERR(fpc_irq_data->worker_thread)) {
		dev_err(fpc_irq_data->dev, "%s irq worker kthread_run failed.\n", __func__);
		error = (int)PTR_ERR(fpc_irq_data->worker_thread);
	}

	if (IS_ERR(fpc_irq_data->pm_worker_thread)) {
		dev_err(fpc_irq_data->dev, "%s pm worker kthread_run failed.\n", __func__);
		error = (int)PTR_ERR(fpc_irq_data->pm_worker_thread);
	}

	return error;
}

static int fpc_irq_input_init(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	fpc_irq_data->key_input_dev = input_allocate_device();
	if(fpc_irq_data->key_input_dev == NULL)
		dev_err(fpc_irq_data->dev, "%s alloc input_dev fail\n", __func__);

	__set_bit(EV_KEY, fpc_irq_data->key_input_dev->evbit);
	__set_bit(FPC_IRQ_KEY_ANDR_BACK, fpc_irq_data->key_input_dev->keybit);

	fpc_irq_data->key_input_dev->name = "fpc-keys";
	error = input_register_device(fpc_irq_data->key_input_dev);
	if(error)
		dev_err(fpc_irq_data->dev, "%s register input_dev fail\n", __func__);

	return error;
}

/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_goto_idle(fpc_irq_data_t *fpc_irq_data)
{
	const int wait_idle_us = 100;

#if 0
	fpc_irq_data->idle_request = true;

	if (down_trylock(&fpc_irq_data->sem_active) == 0) {
		dev_dbg(fpc_irq_data->dev, "%s : already idle\n", __func__);
	} else {
		dev_dbg(fpc_irq_data->dev, "%s : idle_request\n", __func__);

		while (down_trylock(&fpc_irq_data->sem_active)) {

			fpc_irq_data->idle_request = true;
			wake_up_interruptible(&fpc_irq_data->wq_enable);
			SLEEP_US(wait_idle_us);
		}
		dev_dbg(fpc_irq_data->dev, "%s : is idle\n", __func__);
		up (&fpc_irq_data->sem_active);
	}
#endif
	disable_irq(fpc_irq_data->pdata.irq_no);
	SLEEP_US(wait_idle_us);
	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_enable(fpc_irq_data_t *fpc_irq_data)
{
	//dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	fpc_irq_data->idle_request = false;
	fpc_irq_data->interrupt_done = false;

	wake_up_interruptible(&fpc_irq_data->wq_enable);
	enable_irq(fpc_irq_data->pdata.irq_no);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_destroy(fpc_irq_data_t *fpc_irq_data)
{
	int error = 0;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (fpc_irq_data->worker_thread) {

		fpc_irq_worker_goto_idle(fpc_irq_data);

		fpc_irq_data->term_request = true;
		wake_up_interruptible(&fpc_irq_data->wq_enable);

		kthread_stop(fpc_irq_data->worker_thread);
		fpc_irq_data->worker_thread = NULL;
	}
	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_manage_sysfs_setup(fpc_irq_data_t *fpc_irq_data,
					bool create)
{
	int error = 0;

	if (create) {
		dev_dbg(fpc_irq_data->dev, "%s create\n", __func__);

		error = sysfs_create_group(&fpc_irq_data->dev->kobj,
					&fpc_irq_setup_attr_group);

		if (error) {
			dev_err(fpc_irq_data->dev,
				"sysf_create_group (setup) failed.\n");
			return error;
		}

	} else {
		dev_dbg(fpc_irq_data->dev, "%s remove\n", __func__);

		sysfs_remove_group(&fpc_irq_data->dev->kobj, &fpc_irq_setup_attr_group);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_manage_sysfs_pm(fpc_irq_data_t *fpc_irq_data,
					bool create)
{
	int error = 0;

	if (create) {
		dev_dbg(fpc_irq_data->dev, "%s create\n", __func__);

		error = sysfs_create_group(&fpc_irq_data->dev->kobj,
					&fpc_irq_pm_attr_group);

		if (error) {
			dev_err(fpc_irq_data->dev,
				"sysf_create_group (pm) failed.\n");
			return error;
		}

	} else {
		dev_dbg(fpc_irq_data->dev, "%s remove\n", __func__);

		sysfs_remove_group(&fpc_irq_data->dev->kobj, &fpc_irq_pm_attr_group);
	}

	return error;
}

/* -------------------------------------------------------------------------- */
int fpc_irq_wait_for_interrupt(fpc_irq_data_t *fpc_irq_data, int timeout)
{
	int result = 0;

	if (!timeout) {
		result = wait_event_interruptible(
				fpc_irq_data->wq_irq_return,
				fpc_irq_data->interrupt_done);
	} else {
		result = wait_event_interruptible_timeout(
				fpc_irq_data->wq_irq_return,
				fpc_irq_data->interrupt_done, timeout);
	}

	if (result < 0) {
		dev_err(fpc_irq_data->dev,
			 "wait_event_interruptible interrupted by signal (%d).\n", result);

		return result;
	}

	if (result || !timeout) {
		fpc_irq_data->interrupt_done = false;
		return 0;
	}

	return -ETIMEDOUT;
}


/* -------------------------------------------------------------------------- */
irqreturn_t fpc_irq_interrupt(int irq, void *_fpc_irq_data)
{
	fpc_irq_data_t *fpc_irq_data = _fpc_irq_data;

	if (gpio_get_value(fpc_irq_data->pdata.irq_gpio)) {
		fpc_irq_data->interrupt_done = true;
		wake_up_interruptible(&fpc_irq_data->wq_irq_return);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_show_attr_setup(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	fpc_irq_data_t *fpc_irq_data = dev_get_drvdata(dev);
	struct fpc_irq_attribute *fpc_attr;
	int val;

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_pid))
		val = fpc_irq_data->setup.dst_pid;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_signo))
		val = fpc_irq_data->setup.dst_signo;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, enabled))
		val = fpc_irq_data->setup.enabled;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, click_event))
		val = fpc_irq_data->setup.click_event;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, lock_freq))
		val = fpc_irq_data->setup.lock_freq;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, test_trigger))
		val = -EPERM;
	else
		return -ENOENT;

	return scnprintf(buf, PAGE_SIZE, "%i\n", val);

}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_store_attr_setup(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	int error = 0;
	fpc_irq_data_t *fpc_irq_data = dev_get_drvdata(dev);
	struct fpc_irq_attribute *fpc_attr;
	u64 val;

	error = kstrtou64(buf, 0, &val);

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (!error) {
		if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_pid))
			fpc_irq_data->setup.dst_pid = (pid_t)val;

		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, dst_signo))
			fpc_irq_data->setup.dst_signo = (int)val;

		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, enabled)) {
			fpc_irq_enable(fpc_irq_data, (int)val);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, click_event)) {
			fpc_irq_click_event(fpc_irq_data, (int)val);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, lock_freq)) {
			fpc_irq_lock_freq();
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_setup, test_trigger)) {

			fpc_irq_data->setup.test_trigger = (int)val;

			fpc_irq_send_signal(fpc_irq_data->dev,
						fpc_irq_data->setup.dst_pid,
						fpc_irq_data->setup.dst_signo,
						fpc_irq_data->setup.test_trigger
						);
		}
		else
			return -ENOENT;

		return strnlen(buf, count);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_show_attr_pm(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	fpc_irq_data_t *fpc_irq_data = dev_get_drvdata(dev);
	struct fpc_irq_attribute *fpc_attr;
	int val;

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (fpc_attr->offset == offsetof(struct fpc_irq_pm, state))
		val = fpc_irq_data->pm.state;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, supply_on))
		val = (fpc_irq_data->pm.supply_on) ? 1 : 0;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, hw_reset))
		val = -EPERM;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_enabled))
		val = (fpc_irq_data->pm.notify_enabled) ? 1 : 0;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_ack))
		val = -EPERM;
	else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, wakeup_req))
		val = -EPERM;
	else
		return -ENOENT;

	return scnprintf(buf, PAGE_SIZE, "%i\n", val);
}


/* -------------------------------------------------------------------------- */
static ssize_t fpc_irq_store_attr_pm(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	int error = 0;
	fpc_irq_data_t *fpc_irq_data = dev_get_drvdata(dev);
	struct fpc_irq_attribute *fpc_attr;
	u64 val;

	error = kstrtou64(buf, 0, &val);

	fpc_attr = container_of(attr, struct fpc_irq_attribute, attr);

	if (!error) {
		if (fpc_attr->offset == offsetof(struct fpc_irq_pm, state))
			return -EPERM;

		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, supply_on)) {
			error = fpc_irq_supply_set(fpc_irq_data, (val != 0));
			if (error < 0)
				return -EIO;
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, hw_reset)) {
			error = fpc_irq_ctrl_hw_reset(fpc_irq_data);
			if (error < 0)
				return -EIO;
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_enabled)) {
			fpc_irq_pm_notify_enable(fpc_irq_data, (int)val);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, notify_ack)) {
			fpc_irq_pm_notify_ack(fpc_irq_data, (int)val);
		}
		else if (fpc_attr->offset == offsetof(struct fpc_irq_pm, wakeup_req)) {
			error = fpc_irq_pm_wakeup_req(fpc_irq_data);
			if (error < 0)
				return -EIO;
		}
		else
			return -ENOENT;

		return strnlen(buf, count);
	}

	return error;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_worker_function(void *_fpc_irq_data)
{
	int status;
	const int irq_timeout_ms = 100;
	fpc_irq_data_t *fpc_irq_data = _fpc_irq_data;

	while (!kthread_should_stop()) {

		//up(&fpc_irq_data->sem_active);

		dev_dbg(fpc_irq_data->dev, "%s : waiting\n", __func__);

		wait_event_interruptible(fpc_irq_data->wq_enable,
				!fpc_irq_data->idle_request || fpc_irq_data->term_request);

		if (fpc_irq_data->term_request)
			continue;

		//down(&fpc_irq_data->sem_active);

		if  (!fpc_irq_data->idle_request)
			dev_dbg(fpc_irq_data->dev, "%s : running\n", __func__);

		//enable_irq(fpc_irq_data->pdata.irq_no);
		while (!fpc_irq_data->idle_request) {

			status = fpc_irq_wait_for_interrupt(fpc_irq_data, irq_timeout_ms);

			if ((status >= 0) && (status != -ETIMEDOUT)) {

				fpc_irq_send_signal(
						fpc_irq_data->dev,
						fpc_irq_data->setup.dst_pid,
						fpc_irq_data->setup.dst_signo,
						FPC_IRQ_SIGNAL_TEST);
			}
		}
		//disable_irq(fpc_irq_data->pdata.irq_no);
	}

	dev_dbg(fpc_irq_data->dev, "%s : exit\n", __func__);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_enable(fpc_irq_data_t *fpc_irq_data, int req_state)
{
	//dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (req_state == 0) {
		if (fpc_irq_data->setup.enabled) {
			fpc_irq_worker_goto_idle(fpc_irq_data);
			fpc_irq_data->setup.enabled = 0;
		}
	} else {
		if (fpc_irq_data->setup.enabled == 0) {
			fpc_irq_worker_enable(fpc_irq_data);
			fpc_irq_data->setup.enabled = 1;
		}
	}
	return 0;
}

static int fpc_irq_click_event(fpc_irq_data_t *fpc_irq_data, int val)
{
	//dev_dbg(fpc_irq_data->dev, "%s\n", __func__);
	if (fpc_irq_data->input_dev == NULL) {
		dev_err(fpc_irq_data->dev, "%s - input_dev == NULL !\n", __func__);
		return -ENODEV;
	}
	input_report_key(fpc_irq_data->key_input_dev, FPC_IRQ_KEY_ANDR_BACK, 1);
	input_report_key(fpc_irq_data->key_input_dev, FPC_IRQ_KEY_ANDR_BACK, 0);
	input_sync(fpc_irq_data->key_input_dev);
	return 0;
}

static int fpc_irq_lock_freq(void)
{
	pm_qos_update_request_timeout(&fp_cpu_num_min_qos, BIG_CORE_NUM, HMP_BOOST_TIMEOUT);
	//set_hmp_boostpulse(HMP_BOOST_TIMEOUT);
	pm_qos_update_request_timeout(&fp_cpu_freq_qos, HMP_BOOST_FREQ, HMP_BOOST_TIMEOUT);
	pm_qos_update_request_timeout(&fp_cpu_mif_qos, HMP_MIF_FREQ, HMP_BOOST_TIMEOUT);
	pm_qos_update_request_timeout(&fp_cpu_int_qos, HMP_INT_FREQ, HMP_BOOST_TIMEOUT);
	return 0;
}
/* -------------------------------------------------------------------------- */
