# drivers/staging/samsung/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

obj-$(CONFIG_BATTERY_SAMSUNG)	+= sec_batt.o
# Samsung Debug Feature
obj-$(CONFIG_SEC_DEBUG)		+= sec_debug.o sec_getlog.o sec_gaf.o
obj-$(CONFIG_SEC_LOG)		+= sec_log.o

obj-$(CONFIG_SEC_MEM)		+= sec_memory.o
# Samsung Reset Feature
obj-$(CONFIG_SEC_REBOOT)	+= sec_reboot.o
# Samsung Misc Feature
obj-$(CONFIG_SEC_MISC)		+= sec_misc.o
# Samsung sec sysfs Feature
obj-$(CONFIG_SEC_SYSFS)		+= sec_sysfs.o
