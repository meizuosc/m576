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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>

#include "fpc_irq_common.h"
#include "fpc_irq_ctrl.h"

/* -------------------------------------------------------------------------- */
/* function prototypes                                                        */
/* -------------------------------------------------------------------------- */
static void fpc_irq_ctrl_reset_out(fpc_irq_data_t *fpc_irq_data);


/* -------------------------------------------------------------------------- */
/* fpc_irq driver constants                                                   */
/* -------------------------------------------------------------------------- */
#define FPC1020_RESET_RETRIES	2
#define FPC1020_RESET_RETRY_US	1250

#define FPC1020_RESET_LOW_US	1000
#define FPC1020_RESET_HIGH1_US	100
#define FPC1020_RESET_HIGH2_US	1250


/* -------------------------------------------------------------------------- */
/* function definitions                                                       */
/* -------------------------------------------------------------------------- */
int fpc_irq_ctrl_init(fpc_irq_data_t *fpc_irq_data, fpc_irq_pdata_t *pdata)
{
	int ret = 0;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (!gpio_is_valid(pdata->rst_gpio)) {

		dev_info(fpc_irq_data->dev,
			"%s rst_gpio invalid or not set\n",
			__func__);
	}

	ret = gpio_request(pdata->rst_gpio, "fpc_rst");

	if (ret) {
		dev_err(fpc_irq_data->dev,
			"%s rst_gpio request failed (%d)\n",
		       __func__,
		       ret);

		return ret;

	} else {
		fpc_irq_data->pdata.rst_gpio = pdata->rst_gpio;

		dev_dbg(fpc_irq_data->dev,
			"%s assign HW reset -> GPIO%d\n",
			__func__,
			fpc_irq_data->pdata.rst_gpio);
	}

	ret = gpio_direction_output(fpc_irq_data->pdata.rst_gpio, 1);

	if (ret < 0) {
		dev_err(fpc_irq_data->dev,
			"gpio_direction_input (irq) failed (%d).\n",
			ret);
	}

	return ret;
}


/* -------------------------------------------------------------------------- */
int fpc_irq_ctrl_destroy(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (gpio_is_valid(fpc_irq_data->pdata.rst_gpio)) {
		gpio_free(fpc_irq_data->pdata.rst_gpio);
	}

	return 0;
}


/* -------------------------------------------------------------------------- */
int fpc_irq_ctrl_hw_reset(fpc_irq_data_t *fpc_irq_data)
{
	int ret = 0;
	int counter = FPC1020_RESET_RETRIES;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (!gpio_is_valid(fpc_irq_data->pdata.rst_gpio) ||
		!gpio_is_valid(fpc_irq_data->pdata.irq_gpio)) {
		dev_err(fpc_irq_data->dev, "%s error, GPIO invalid\n", __func__);
		return -EIO;
	}

	while (counter) {

		--counter;

		fpc_irq_ctrl_reset_out(fpc_irq_data);

		ret = gpio_get_value(fpc_irq_data->pdata.irq_gpio) ? 0 : -EIO;

		if (!ret) {
			dev_dbg(fpc_irq_data->dev, "%s OK\n", __func__);
			return 0;
		} else {
			dev_err(fpc_irq_data->dev,
				"%s timed out, retrying\n",
				__func__);

			udelay(FPC1020_RESET_RETRY_US);
		}
	}
	return ret;
}


/* -------------------------------------------------------------------------- */
static void fpc_irq_ctrl_reset_out(fpc_irq_data_t *fpc_irq_data)
{
	gpio_set_value(fpc_irq_data->pdata.rst_gpio, 1);
	udelay(FPC1020_RESET_HIGH1_US);

	gpio_set_value(fpc_irq_data->pdata.rst_gpio, 0);
	udelay(FPC1020_RESET_LOW_US);

	gpio_set_value(fpc_irq_data->pdata.rst_gpio, 1);
	udelay(FPC1020_RESET_HIGH2_US);
}


/* -------------------------------------------------------------------------- */

