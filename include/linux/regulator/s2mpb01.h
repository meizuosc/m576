/*
 * s2mpb01.h - Regulator driver for the S.LSI S2MPB01
 *
 * Copyright (C) 2014 Samsung Electronics
 * SangYoung Son <hello.son@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max77826.h
 */


#ifndef __LINUX_REGULATOR_S2MPB01_H
#define __LINUX_REGULATOR_S2MPB01_H

#include <linux/regulator/machine.h>

/* S2MPB01 regulator ids */
enum s2mpb01_regulators {
	S2MPB01_LDO1 = 0,
	S2MPB01_LDO2,
	S2MPB01_LDO3,
	S2MPB01_LDO4,
	S2MPB01_LDO5,
	S2MPB01_LDO6,
	S2MPB01_LDO7,
	S2MPB01_LDO8,
	S2MPB01_LDO9,
	S2MPB01_LDO10,
	S2MPB01_LDO11,
	S2MPB01_LDO12,
	S2MPB01_LDO13,
	S2MPB01_LDO14,
	S2MPB01_LDO15,
	S2MPB01_BUCK1,
	S2MPB01_BUCK2,

	S2MPB01_REG_MAX,
};

/* S2MPB01 PMIC Registers. */
enum s2mpb01_pmic_registers {
	S2MPB01_REG_PMIC_ID = 0x00,
	S2MPB01_REG_INT1,
	S2MPB01_REG_INT1M,
	S2MPB01_REG_STATUS1,
	S2MPB01_REG_CTRL,

	S2MPB01_REG_BUCK_CTRL = 0x0B,
	S2MPB01_REG_BUCK_OUT,
	S2MPB01_REG_BB_CTRL,
	S2MPB01_REG_BB_OUT,
	S2MPB01_REG_BUCK_RAMP,

	S2MPB01_REG_LDO1_CTRL = 0x10,
	S2MPB01_REG_LDO2_CTRL,
	S2MPB01_REG_LDO3_CTRL,
	S2MPB01_REG_LDO4_CTRL,
	S2MPB01_REG_LDO5_CTRL,
	S2MPB01_REG_LDO6_CTRL,
	S2MPB01_REG_LDO7_CTRL,
	S2MPB01_REG_LDO8_CTRL,
	S2MPB01_REG_LDO9_CTRL,
	S2MPB01_REG_LDO10_CTRL,
	S2MPB01_REG_LDO11_CTRL,
	S2MPB01_REG_LDO12_CTRL,
	S2MPB01_REG_LDO13_CTRL,
	S2MPB01_REG_LDO14_CTRL,
	S2MPB01_REG_LDO15_CTRL,

	S2MPB01_REG_LDO_DSCH1 = 0x1F,
	S2MPB01_REG_LDO_DSCH2,
};

/* LDO output, mode control */
#define LDO_CTRL_OFF	0x0
#define LDO_CTRL_PWREN	0x1
#define LDO_CTRL_MODE	0x2
#define LDO_CTRL_ON	0x3

struct s2mpb01_dev {
	struct device *dev;
	struct mutex io_lock;
	struct i2c_client *i2c;
	int num_regulators;
	struct regulator_dev **rdev;
};

struct s2mpb01_regulator_subdev {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct s2mpb01_platform_data {
	char *name;
	int num_regulators;
	struct s2mpb01_regulator_subdev *regulators;
};
#endif	/* __LINUX_REGULATOR_S2MPB01_H */
