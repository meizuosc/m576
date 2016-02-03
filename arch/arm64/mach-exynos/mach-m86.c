/*
 * SAMSUNG EXYNOS5430 Flattened Device Tree enabled machine
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/exynos_ion.h>
#include <linux/power_supply.h>
#include <linux/delay.h>

#include <asm/mach/arch.h>
#include <mach/regs-pmu.h>
#include <mach/resetreason.h>

#include <plat/cpu.h>

#include "common.h"

#define REBOOT_MODE_NORMAL			0x00
#define REBOOT_MODE_CHARGE			0x0A
/* Reboot into fastboot mode */
#define REBOOT_MODE_FASTBOOT		0xFC
/* Auto enter bootloader command line */
#define REBOOT_MODE_BOOTLOADER		0xFE
/* Reboot into recovery */
#define REBOOT_MODE_RECOVERY		0xFF

static void m86_restart(char mode, const char *cmd)
{
	void __iomem *addr = EXYNOS_PMU_SYSIP_DAT0;

	record_normal_reboot_reason(cmd);

	if (cmd) {
		if (!strcmp(cmd, "charge")) {
			__raw_writel(REBOOT_MODE_CHARGE, addr);
		} else if (!strcmp(cmd, "fastboot") || !strcmp(cmd, "fb")) {
			__raw_writel(REBOOT_MODE_FASTBOOT, addr);
		} else if (!strcmp(cmd, "bootloader") || !strcmp(cmd, "bl")) {
			__raw_writel(REBOOT_MODE_BOOTLOADER, addr);
		} else if (!strcmp(cmd, "recovery")) {
			__raw_writel(REBOOT_MODE_RECOVERY, addr);
		}
	}

	__raw_writel(0x1, EXYNOS_PMU_SWRESET);
}

static int m86_charger_online(void)
{
	struct power_supply *wall, *usb;
	int wall_online = 0;
	int usb_online = 0;

	/* get wall power supply */
	wall = power_supply_get_by_name("Wall");
	if (wall) {
		/* get wall's online property */
		if (wall->get_property) {
			wall->get_property(wall, POWER_SUPPLY_PROP_ONLINE,
					(union power_supply_propval *)&wall_online);
		}
	}

	/* get usb power supply */
	usb = power_supply_get_by_name("usb");
	if (usb) {
		/* get usb's online property */
		if (usb->get_property) {
			usb->get_property(usb, POWER_SUPPLY_PROP_ONLINE,
					(union power_supply_propval *)&usb_online);
		}
	}

	return wall_online | usb_online;
}

static void m86_power_off(void)
{
	unsigned int reg=0;
	printk("power off the device....\n");

	if (m86_charger_online()) {
		m86_restart(0, "charge");
	} else {
		reg = readl(EXYNOS_PMU_PS_HOLD_CONTROL);
		reg &= ~(0x1<<8);
		writel(reg, EXYNOS_PMU_PS_HOLD_CONTROL);

		mdelay(200);

		printk("Power off failed, maybe RTC alarm occur or Power Key pressed.\n");
		m86_restart(0, "power off");
	}
}
static void m86_power_off_prepare(void)
{
	printk("power off prepare the deivce....\n");
}
static void __init m86_power_off_init(void)
{
	//register power off callback
	pm_power_off = m86_power_off;
	pm_power_off_prepare = m86_power_off_prepare;
}

const struct of_device_id of_iommu_bus_match_table[] = {
	{ .compatible = "samsung,exynos-iommu-bus", },
	{} /* Empty terminated list */
};

static void __init m86_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static void __init m86_dt_machine_init(void)
{
	of_platform_bus_probe(NULL, of_iommu_bus_match_table, NULL);
	exynos_pmu_init();
	m86_power_off_init();
}

static char const *m86_dt_compat[] __initdata = {
	"Meizu, M86",
	NULL
};

DT_MACHINE_START(M86, "M86")
	.map_io		= m86_dt_map_io,
	.init_machine	= m86_dt_machine_init,
	.dt_compat	= m86_dt_compat,
	.restart        = m86_restart,
MACHINE_END
