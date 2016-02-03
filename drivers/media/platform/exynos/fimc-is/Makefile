fimc-is-objs	:= fimc-is-core.o \
		fimc-is-mem.o \
		fimc-is-framemgr.o \
		fimc-is-groupmgr.o \
		fimc-is-resourcemgr.o \
		fimc-is-video.o \
		fimc-is-video-sensor.o \
		fimc-is-video-3aa.o \
		fimc-is-video-3aac.o \
		fimc-is-video-isp.o \
		fimc-is-video-scc.o \
		fimc-is-video-scp.o \
		fimc-is-video-vdisc.o \
		fimc-is-video-vdiso.o \
		fimc-is-hw-csi.o \
		fimc-is-hw-ischain.o \
		fimc-is-subdev-ctrl.o \
		fimc-is-device-csi.o \
		fimc-is-device-flite.o \
		fimc-is-device-sensor.o \
		fimc-is-device-ischain.o \
		fimc-is-interface.o \
		fimc-is-time.o \
		fimc-is-dvfs.o \
		fimc-is-dt.o \
		fimc-is-clk-gate.o \
		sensor/fimc-is-device-6b2.o \
		sensor/fimc-is-device-8b1.o \
		sensor/fimc-is-device-6d1.o \
		sensor/fimc-is-device-imx135.o \
		sensor/fimc-is-device-3l2.o \
		sensor/fimc-is-device-2p2.o \
		sensor/fimc-is-device-2p2_12m.o \
		sensor/fimc-is-device-3h5.o \
		sensor/fimc-is-device-3h7.o \
		sensor/fimc-is-device-3h7_sunny.o \
		sensor/fimc-is-device-4e5.o \
		sensor/fimc-is-device-6a3.o \
		sensor/fimc-is-device-imx175.o \
		sensor/fimc-is-device-imx240.o \
		sensor/fimc-is-device-4h5.o \
		sensor/fimc-is-device-imx134.o

obj-$(CONFIG_USE_VENDER_FEATURE) := fimc-is-spi.o \
				fimc-is-sec-define.o \
				crc32.o

obj-$(CONFIG_COMPANION_USE)	+= fimc-is-companion.o \
				fimc-is-fan53555.o \
				fimc-is-ncp6335b.o \
				fimc-is-companion-dt.o \
				fimc-is-device-companion.o \
				fimc-is-video-companion.o

obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS) += fimc-is.o
obj-$(CONFIG_OIS_USE)	+= fimc-is-device-ois.o
obj-$(CONFIG_AF_HOST_CONTROL)	+= fimc-is-device-af.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT)	+= fimc-is-device-eeprom.o
