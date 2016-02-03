# arch/arm/mach-exynos/Makefile
#
# Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Core

obj-$(CONFIG_ARCH_EXYNOS)	+= common.o
obj-$(CONFIG_SOC_EXYNOS7420)	+= pm_domains-exynos7420.o
obj-$(CONFIG_SOC_EXYNOS7580)	+= pm_domains-exynos7580.o
obj-$(CONFIG_SOC_EXYNOS5433)	+= pm_domains-exynos5433.o

obj-$(CONFIG_CPU_IDLE)		+= exynos-pm.o
obj-$(CONFIG_PM_RUNTIME)	+= pm_domains.o
obj-$(CONFIG_PM_RUNTIME_TEST_SYSFS)	+= pm_domains_sysfs.o
obj-$(CONFIG_ARCH_EXYNOS)	+= pmu.o pmu_cal_sys.o
ifneq ($(CONFIG_SOC_EXYNOS7580),y)
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode.o
else
obj-$(CONFIG_ARCH_EXYNOS)	+= exynos-powermode-smp.o
endif
obj-$(CONFIG_SUSPEND)		+= pm.o

ifeq ($(CONFIG_CAL_SYS_PWRDOWN),y)
obj-$(CONFIG_SOC_EXYNOS5433)	+= pmu_cal_sys_exynos5433.o
obj-$(CONFIG_SOC_EXYNOS7420)	+= pmu_cal_sys_exynos7420.o
obj-$(CONFIG_SOC_EXYNOS7580)	+= pmu_cal_sys_exynos7580.o
endif
obj-$(CONFIG_SOC_EXYNOS7580)   += pmu-cp.o

obj-$(CONFIG_RESET_REASON_EXYNOS7420)	+= reset-reason-exynos7420.o

# machine support
obj-$(CONFIG_EXYNOS_ASV)	+= asv-exynos.o asv-exynos_cal.o

obj-$(CONFIG_MACH_ESPRESSO7420)		+= mach-espresso7420.o asv-exynos7420_cal.o
obj-$(CONFIG_MACH_AVL7420)		+= mach-avl7420.o asv-exynos7420_cal.o
obj-$(CONFIG_MACH_M86)			+= mach-m86.o asv-exynos7420_cal.o
obj-$(CONFIG_MACH_M86)			+= board-mz-factory-test.o
obj-$(CONFIG_MACH_M86)			+= board-m86-gps.o
obj-$(CONFIG_MACH_M86)                  += hardware.o

obj-$(CONFIG_MACH_SMDK7580)		+= mach-smdk7580.o

obj-$(CONFIG_MACH_SMDK7580)		+= mach-smdk7580.o asv-exynos7580_cal.o

obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS)	+= setup-fimc-is.o
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS)	+= setup-fimc-is-sensor.o
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS)	+= setup-fimc-is-module.o
obj-$(CONFIG_COMPANION_USE)		+= setup-fimc-is-companion.o

# device support

obj-$(CONFIG_EXYNOS5_SETUP_MIPIPHY)	+= setup-exynos5-mipiphy.o

ifneq ($(CONFIG_SOC_EXYNOS7580),y)
obj-$(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION) += secmem.o
endif
