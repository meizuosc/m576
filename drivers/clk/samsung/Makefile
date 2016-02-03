#
# Samsung Clock specific Makefile
#

ifeq ($(CONFIG_ARCH_EXYNOS7),y)
  obj-y				+= composite.o
  obj-$(CONFIG_SOC_EXYNOS7420)	+= clk-exynos7420.o
  obj-$(CONFIG_SOC_EXYNOS7580)	+= clk-exynos7580.o
else
	obj-$(CONFIG_COMMON_CLK)		+= clk.o clk-pll.o
	obj-$(CONFIG_ARCH_EXYNOS4)		+= clk-exynos4.o
	obj-$(CONFIG_SOC_EXYNOS5250)		+= clk-exynos5250.o
	obj-$(CONFIG_SOC_EXYNOS5430)		+= clk-exynos5430.o init-exynos5430.o
	obj-$(CONFIG_SOC_EXYNOS5433)		+= clk-exynos5433.o init-exynos5433.o
	obj-$(CONFIG_SOC_EXYNOS5422)		+= clk-exynos5422.o init-exynos5422.o
	obj-$(CONFIG_SOC_EXYNOS5440)		+= clk-exynos5440.o
endif
	obj-$(CONFIG_ARCH_EXYNOS)		+= clk-exynos-audss.o
	obj-$(CONFIG_PWM_SAMSUNG)		+= clk-exynos-pwm.o
