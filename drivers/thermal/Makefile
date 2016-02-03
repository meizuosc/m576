#
# Makefile for sensor chip drivers.
#

obj-$(CONFIG_THERMAL)		+= thermal_sys.o
ifeq ($(CONFIG_SOC_EXYNOS7580),y)
thermal_sys-y			+= exynos7580/thermal_core.o
else
thermal_sys-y			+= thermal_core.o
endif

# interface to/from other layers providing sensors
thermal_sys-$(CONFIG_THERMAL_HWMON)		+= thermal_hwmon.o
thermal_sys-$(CONFIG_THERMAL_OF)		+= of-thermal.o

# governors
thermal_sys-$(CONFIG_THERMAL_GOV_FAIR_SHARE)	+= fair_share.o
thermal_sys-$(CONFIG_THERMAL_GOV_STEP_WISE)	+= step_wise.o
thermal_sys-$(CONFIG_THERMAL_GOV_USER_SPACE)	+= user_space.o

# cpufreq cooling
ifeq ($(CONFIG_SOC_EXYNOS7580),y)
thermal_sys-$(CONFIG_CPU_THERMAL)	+= exynos7580/cpu_cooling.o
thermal_sys-$(CONFIG_GPU_THERMAL)	+= exynos7580/gpu_cooling.o
else
thermal_sys-$(CONFIG_CPU_THERMAL)	+= cpu_cooling.o
endif

# platform thermal drivers
obj-$(CONFIG_SPEAR_THERMAL)	+= spear_thermal.o
obj-$(CONFIG_RCAR_THERMAL)	+= rcar_thermal.o
obj-$(CONFIG_KIRKWOOD_THERMAL)  += kirkwood_thermal.o
ifeq ($(CONFIG_SOC_EXYNOS7580),y)
obj-y				+= exynos7580/
else
ifeq ($(CONFIG_SOC_EXYNOS7420),y)
obj-$(CONFIG_EXYNOS_THERMAL)	+= exynos7420_thermal.o cal_tmu7420.o
else
obj-$(CONFIG_EXYNOS_THERMAL)	+= exynos_thermal.o cal_tmu.o
endif
endif
obj-$(CONFIG_DOVE_THERMAL)  	+= dove_thermal.o
obj-$(CONFIG_DB8500_THERMAL)	+= db8500_thermal.o
obj-$(CONFIG_ARMADA_THERMAL)	+= armada_thermal.o
obj-$(CONFIG_DB8500_CPUFREQ_COOLING)	+= db8500_cpufreq_cooling.o
obj-$(CONFIG_INTEL_POWERCLAMP)	+= intel_powerclamp.o
obj-$(CONFIG_CPU_THERMAL_IPA)	+= ipa.o
