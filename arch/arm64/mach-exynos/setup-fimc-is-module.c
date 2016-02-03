/* linux/arch/arm/mach-exynos/setup-fimc-sensor.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/exynos-fimc-is-module.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_SOC_EXYNOS5433)
#include <mach/regs-clock-exynos5433.h>
#elif defined(CONFIG_SOC_EXYNOS7420)
#include <mach/regs-clock-exynos7420.h>
#endif

int pinctrl_free_state(struct pinctrl *p);

static int exynos_fimc_is_module_pin_control(struct platform_device *pdev,
	struct pinctrl *pinctrl, struct exynos_sensor_pin *pin_ctrls)
{
	char* name = pin_ctrls->name;
	ulong pin = pin_ctrls->pin;
	u32 delay = pin_ctrls->delay;
	u32 value = pin_ctrls->value;
	u32 voltage = pin_ctrls->voltage;
	enum pin_act act = pin_ctrls->act;
	int ret = 0;

	switch (act) {
	case PIN_NONE:
		usleep_range(delay, delay);
		break;
	case PIN_OUTPUT:
		if (gpio_is_valid(pin)) {
			if (value)
				ret = gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_OUTPUT_HIGH");
			else
				ret = gpio_request_one(pin, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");

			if (unlikely(ret)) {
				pr_err("%s(), request gpio:%ld fail:%d\n", __func__, pin, ret);
			}

			usleep_range(delay, delay);
			gpio_free(pin);
		} else {
			pr_warn("%s(), do nothing for PIN_OUTPUT due to invalid gpio:%ld\n",
				__func__, pin);
		}
		break;
	case PIN_INPUT:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_IN, "CAM_GPIO_INPUT");
			gpio_free(pin);
		} else {
			pr_warn("%s(), do nothing for PIN_INPUT due to invalid gpio:%ld\n",
				__func__, pin);
		}
		break;
	case PIN_RESET:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_RESET");
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 0);
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 1);
			usleep_range(1000, 1000);
			gpio_free(pin);
		} else {
			pr_warn("%s(), do nothing for PIN_RESET due to invalid gpio:%ld\n",
				__func__, pin);
		}
		break;
	case PIN_FUNCTION:
		{
			struct pinctrl_state *s = (struct pinctrl_state *)pin;

			if (value) {
				ret = pinctrl_select_state(pinctrl, s);
				if (ret < 0) {
					pr_err("pinctrl_select_state(%s) is fail(%d)\n", name, ret);
					return ret;
				}
			} else {
				ret = pinctrl_free_state(pinctrl);
				if (ret < 0) {
					pr_err("pinctrl_select_free(%s) is fail(%d)\n", name, ret);
					return ret;
				}
			}
			usleep_range(delay, delay);
		}
		break;
	case PIN_REGULATOR:
		{
			struct regulator *regulator = NULL;

			regulator = regulator_get(&pdev->dev, name);
			if (IS_ERR_OR_NULL(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				return PTR_ERR(regulator);
			}

			if (value) {
				if(voltage > 0) {
					pr_debug("%s : regulator_set_voltage(%d)\n",__func__, voltage);
					ret = regulator_set_voltage(regulator, voltage, voltage);
					if(ret) {
						pr_err("%s : regulator_set_voltage(%d) fail\n", __func__, ret);
					}
				}

				if (regulator_is_enabled(regulator)) {
					pr_warning("%s regulator is already enabled\n", name);
					regulator_put(regulator);
					return 0;
				}

				ret = regulator_enable(regulator);
				if (ret) {
					pr_err("%s : regulator_enable(%s) fail\n", __func__, name);
					regulator_put(regulator);
					return ret;
				}
			} else {
				if (!regulator_is_enabled(regulator)) {
					pr_warning("%s regulator is already disabled\n", name);
					regulator_put(regulator);
					return 0;
				}

				ret = regulator_disable(regulator);
				if (ret) {
					pr_err("%s : regulator_disable(%s) fail\n", __func__, name);
					regulator_put(regulator);
					return ret;
				}
			}

			usleep_range(delay, delay);
			regulator_put(regulator);
		}
		break;
	default:
		pr_err("unknown act for pin\n");
		break;
	}

	return ret;
}

int exynos_fimc_is_module_pins_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 enable)
{
	int ret = 0;
	u32 idx_max, idx;
	struct pinctrl *pinctrl;
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	struct exynos_platform_fimc_is_module *pdata;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.platform_data);
	BUG_ON(enable > 1);
	BUG_ON(scenario > SENSOR_SCENARIO_MAX);

	pdata = dev_get_platdata(&pdev->dev);
	pinctrl = pdata->pinctrl;
	pin_ctrls = pdata->pin_ctrls;
	idx_max = pdata->pinctrl_index[scenario][enable];

	pr_info("%s(), power %s sensor %d\n", __func__,
		(enable == GPIO_SCENARIO_ON) ? "on": "off",
		pdata->id);

	/* print configs */
	for (idx = 0; idx < idx_max; ++idx) {
		pr_debug("[@] pin_ctrl(act(%d), pin(%ld), val(%d), nm(%s)\n",
			pin_ctrls[scenario][enable][idx].act,
			(pin_ctrls[scenario][enable][idx].act == PIN_FUNCTION) ? 0 : pin_ctrls[scenario][enable][idx].pin,
			pin_ctrls[scenario][enable][idx].value,
			pin_ctrls[scenario][enable][idx].name);
	}

	/* do configs */
	for (idx = 0; idx < idx_max; ++idx) {
		ret = exynos_fimc_is_module_pin_control(pdev, pinctrl, &pin_ctrls[scenario][enable][idx]);
		if (ret) {
			pr_err("[@] exynos_fimc_is_sensor_gpio(%d) is fail(%d)", idx, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int exynos_fimc_is_module_pin_debug(struct platform_device *pdev,
	struct pinctrl *pinctrl, struct exynos_sensor_pin *pin_ctrls)
{
	int ret = 0;
	ulong pin = pin_ctrls->pin;
	char* name = pin_ctrls->name;
	enum pin_act act = pin_ctrls->act;

	switch (act) {
	case PIN_NONE:
		break;
	case PIN_OUTPUT:
	case PIN_INPUT:
	case PIN_RESET:
		if (gpio_is_valid(pin))
			/*pr_info*/printk("[@] pin %s : %d\n", name, gpio_get_value(pin));
		break;
	case PIN_FUNCTION:
		{
			/* there is no way to get pin by name after probe */
			ulong base, pin;
			u32 index;

			base = (ulong)ioremap_nocache(0x13470000, 0x10000);

			index = 0x60; /* GPC2 */
			pin = base + index;
			/*pr_info*/printk("[@] CON[0x%X] : 0x%X\n", index, readl((void *)pin));
			/*pr_info*/printk("[@] DAT[0x%X] : 0x%X\n", index, readl((void *)(pin + 4)));

			index = 0x160; /* GPD7 */
			pin = base + index;
			/*pr_info*/printk("[@] CON[0x%X] : 0x%X\n", index, readl((void *)pin));
			/*pr_info*/printk("[@] DAT[0x%X] : 0x%X\n", index, readl((void *)(pin + 4)));

			iounmap((void *)base);
		}
		break;
	case PIN_REGULATOR:
		{
			struct regulator *regulator;
			int voltage;

			regulator = regulator_get(&pdev->dev, name);
			if (IS_ERR(regulator)) {
				printk("%s : regulator_get(%s) fail\n", __func__, name);
				return PTR_ERR(regulator);
			}

			if (regulator_is_enabled(regulator))
				voltage = regulator_get_voltage(regulator);
			else
				voltage = 0;

			regulator_put(regulator);

			printk("[@] %s LDO : %d\n", name, voltage);
		}
		break;
	default:
		/*pr_err*/printk("unknown act for pin\n");
		break;
	}

	return ret;
}

int exynos_fimc_is_module_pins_dbg(struct platform_device *pdev,
	u32 scenario,
	u32 enable)
{
	int ret = 0;
	u32 idx_max, idx;
	struct pinctrl *pinctrl;
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	struct exynos_platform_fimc_is_module *pdata;

	printk("test 000\n");

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.platform_data);
	BUG_ON(enable > 1);
	BUG_ON(scenario > SENSOR_SCENARIO_MAX);

	pdata = dev_get_platdata(&pdev->dev);
	pinctrl = pdata->pinctrl;
	pin_ctrls = pdata->pin_ctrls;
	idx_max = pdata->pinctrl_index[scenario][enable];

	/* print configs */
	for (idx = 0; idx < idx_max; ++idx) {
		printk(KERN_DEBUG "[@] pin_ctrl(act(%d), pin(%ld), val(%d), nm(%s)\n",
			pin_ctrls[scenario][enable][idx].act,
			(pin_ctrls[scenario][enable][idx].act == PIN_FUNCTION) ? 0 : pin_ctrls[scenario][enable][idx].pin,
			pin_ctrls[scenario][enable][idx].value,
			pin_ctrls[scenario][enable][idx].name);
	}

	/* do configs */
	for (idx = 0; idx < idx_max; ++idx) {
		ret = exynos_fimc_is_module_pin_debug(pdev, pinctrl, &pin_ctrls[scenario][enable][idx]);
		if (ret) {
			pr_err("[@] exynos_fimc_is_sensor_gpio(%d) is fail(%d)", idx, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}
