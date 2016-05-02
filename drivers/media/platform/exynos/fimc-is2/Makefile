fimc-is-objs	:= fimc-is-core.o \
		fimc-is-debug.o \
		fimc-is-mem.o \
		fimc-is-framemgr.o \
		fimc-is-groupmgr.o \
		fimc-is-resourcemgr.o \
		fimc-is-video.o \
		fimc-is-video-sensor.o \
		fimc-is-video-3aa.o \
		fimc-is-video-3aac.o \
		fimc-is-video-3aap.o \
		fimc-is-video-isp.o \
		fimc-is-video-ispc.o \
		fimc-is-video-ispp.o \
		fimc-is-video-dis.o \
		fimc-is-video-scc.o \
		fimc-is-video-scp.o \
		fimc-is-subdev-ctrl.o \
		fimc-is-device-csi.o \
		fimc-is-device-flite.o \
		fimc-is-device-sensor.o \
		fimc-is-device-ischain.o \
		fimc-is-interface.o \
		fimc-is-time.o \
		fimc-is-dvfs.o \
		fimc-is-dt.o \
		fimc-is-clk-gate.o

obj-$(CONFIG_COMPANION_USE) += fimc-is-device-companion.o \
				fimc-is-video-companion.o

obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS) += fimc-is.o
obj-$(CONFIG_CAMERA_EEPROM_SUPPORT) += fimc-is-device-eeprom.o
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS) += sensor/
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS) += ischain/
obj-$(CONFIG_VIDEO_EXYNOS_FIMC_IS) += ois/
obj-$(CONFIG_MEIZU_CAMERA_SPECIAL) += meizu_camera_special.o
obj-$(CONFIG_MEIZU_CAMERA_SPECIAL) += lens/

EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2
EXTRA_CFLAGS += -Idrivers/media/platform/exynos/fimc-is2/include
