#
# Makefile for the exynos video drivers.
#

obj-$(CONFIG_EXYNOS_MIPI_DSI)		+= exynos_mipi_dsi.o exynos_mipi_dsi_common.o \
				     	exynos_mipi_dsi_lowlevel.o
obj-$(CONFIG_EXYNOS_LCD_S6E8AX0)	+= s6e8ax0.o
obj-$(CONFIG_EXYNOS_DP)			+= exynos_dp_core.o exynos_dp_reg.o
obj-$(CONFIG_EXYNOS_DECON_DISPLAY)	+= decon_display/
obj-$(CONFIG_EXYNOS_DECON_TV_DISPLAY)	+= decon_tv_display/
obj-y					+= panels/
obj-$(CONFIG_EXYNOS_DECON)		+= decon/
obj-$(CONFIG_EXYNOS_DECON_EXYNOS7580)	+= decon_7580/