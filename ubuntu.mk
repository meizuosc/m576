CURRENT_DIR = $(shell pwd)
UBUNTU_OUT ?= $(CURRENT_DIR)/ubuntu_out
UBUNTU_INITRD=$(UBUNTU_OUT)/initrd.img-touch
PREBUILT_KERNEL_IMAGE=$(UBUNTU_OUT)/arch/arm64/boot/Image
UBUNTU_BOOTIMG=$(UBUNTU_OUT)/boot.img
ifeq ($(USE_CCACHE),1)
CCACHE=ccache
else
CCACHE=
endif
TURBO_CROSS_COMPILE ?=$(CCACHE) $(CURRENT_DIR)/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
MKIMG=./prebuilts/mkbootimg
CORE_NAME=vivid_overlay
$(UBUNTU_OUT):
	mkdir -p $@

$(PREBUILT_KERNEL_IMAGE): $(UBUNTU_OUT)
	make CROSS_COMPILE="$(TURBO_CROSS_COMPILE)" O=$(UBUNTU_OUT) ARCH=arm64 VARIANT_DEFCONFIG= SELINUX_DEFCONFIG= m86_user_defconfig
	make CROSS_COMPILE="$(TURBO_CROSS_COMPILE)" O=$(UBUNTU_OUT) ARCH=arm64 headers_install;
	make CROSS_COMPILE="$(TURBO_CROSS_COMPILE)" O=$(UBUNTU_OUT) CFLAGS_MODULE="-fno-pic" ARCH=arm64 Image

$(UBUNTU_INITRD): $(UBUNTU_OUT)
	dpkg -x prebuilts/initrd/$(CORE_NAME)/armhf/* $(UBUNTU_OUT); \
	cp $(UBUNTU_OUT)/usr/lib/ubuntu-touch-generic-initrd/initrd.img-touch $(UBUNTU_OUT); \

PHONY += bootimage
bootimage: $(PREBUILT_KERNEL_IMAGE) $(UBUNTU_INITRD)
	$(MKIMG) --kernel $(PREBUILT_KERNEL_IMAGE) --ramdisk $(UBUNTU_INITRD) --cmdline "console=ttyFIQ2,115200n8 no_console_suspend systempart=/dev/disk/by-partlabel/system datapart=/dev/disk/by-partlabel/userdata" --base 0x40000000 --pagesize 4096 --kernel_offset 0x80000 --ramdisk_offset 0x2000000 --output $(UBUNTU_BOOTIMG)

