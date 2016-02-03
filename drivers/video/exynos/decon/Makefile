#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_VIDEO_EXYNOS_HDMI) += s5p-hdmi.o
s5p-hdmi-y += hdmi_drv.o hdmi_edid.o hdmi_reg.o hdmiphy_conf.o hdcp_drv.o
obj-$(CONFIG_EXYNOS_VPP) += vpp/
obj-$(CONFIG_EXYNOS_MIPI_DSI) += dsim.o
dsim-y += dsim_drv.o dsim_reg_7420.o
obj-$(CONFIG_EXYNOS_DECON) += decon.o
decon-y += decon_core.o decon-int_drv.o decon-ext_drv.o decon_helper.o
obj-$(CONFIG_SOC_EXYNOS7420) += decon_reg_7420.o
obj-$(CONFIG_SOC_EXYNOS7580) += decon_reg_7580.o
obj-$(CONFIG_EXYNOS_DECON_DPU) += dpu_reg_7420.o
obj-$(CONFIG_VIDEO_EXYNOS_HDMI_CEC) += s5p-hdmi_cec.o
s5p-hdmi_cec-y += hdmi_cec.o hdmi_cec_ctrl.o
obj-y += panels/
