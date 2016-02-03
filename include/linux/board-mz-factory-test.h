/*
 * Copyright (c) 2014 MEIZU Technology Co., Ltd.
 *		http://www.meizu.com
 *
 * Author: QuDao <qudao@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __MZ_FACTORY_TEST_H__
#define __MZ_FACTORY_TEST_H__

#define MX_FACTORY_TEST_BT	0
#define MX_FACTORY_TEST_CAMERA	1
#define MX_FACTORY_TEST_ALL	2
//#define MX_FACTORY_HAS_LED
extern int (*mx_is_factory_test_mode)(int type);
extern int (*mx_set_factory_test_led)(int on);

#endif
