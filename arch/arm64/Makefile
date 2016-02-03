#
# arch/arm64/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995-2001 by Russell King

dtb-$(CONFIG_MACH_ESPRESSO7420) += exynos7420-espresso7420.dtb
ifeq ($(CONFIG_SUPPORT_CODEGEN),y)
dtb-$(CONFIG_MACH_AVL7420) += exynos7420-avl7420-codegen.dtb
dtb-$(CONFIG_MACH_M86) += exynos7420-m86-codegen.dtb
else
dtb-$(CONFIG_MACH_AVL7420) += exynos7420-avl7420.dtb
dtb-$(CONFIG_MACH_M86) += exynos7420-m86.dtb
endif
dtb-$(CONFIG_MACH_SMDK7580) += exynos7580-smdk7580.dtb

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

LDFLAGS_vmlinux	:=-p --no-undefined -X
CPPFLAGS_vmlinux.lds = -DTEXT_OFFSET=$(TEXT_OFFSET)
OBJCOPYFLAGS	:=-O binary -R .note -R .note.gnu.build-id -R .comment -S
GZFLAGS		:=-9

LIBGCC 		:= $(shell $(CC) $(KBUILD_CFLAGS) -print-libgcc-file-name)

KBUILD_DEFCONFIG := defconfig

KBUILD_CFLAGS	+= -mgeneral-regs-only
KBUILD_CFLAGS	+= -fno-pic
ifeq ($(CONFIG_CPU_BIG_ENDIAN), y)
KBUILD_CPPFLAGS	+= -mbig-endian
AS		+= -EB
LD		+= -EB
else
KBUILD_CPPFLAGS	+= -mlittle-endian
AS		+= -EL
LD		+= -EL
endif

comma = ,

CHECKFLAGS	+= -D__aarch64__

# exynos
machine-$(CONFIG_ARCH_EXYNOS)		+= exynos
plat-y		+= samsung
ifneq ($(machine-y),)
MACHINE  := arch/arm64/mach-$(word 1,$(machine-y))/
else
MACHINE  :=
endif
machdirs := $(patsubst %,arch/arm64/mach-%/,$(machine-y))
platdirs := $(patsubst %,arch/arm64/plat-%/,$(plat-y))
KBUILD_CPPFLAGS += $(patsubst %, -I%include,$(machdirs) $(platdirs))

# Defaul value
head-y		:= arch/arm64/kernel/head.o

# The byte offset of the kernel image in RAM from the start of RAM.
TEXT_OFFSET := 0x00080000

export	TEXT_OFFSET GZFLAGS

core-y		+= arch/arm64/kernel/ arch/arm64/mm/
core-$(CONFIG_CRYPTO) += arch/arm64/crypto/
core-y		+= $(machdirs) $(platdirs)
libs-y		:= arch/arm64/lib/ $(libs-y)
libs-y		+= $(LIBGCC)

# Default target when executing plain make
ifeq ($(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE),y)
KBUILD_IMAGE	:= Image.gz-dtb
else
KBUILD_IMAGE	:= Image.gz
endif

KBUILD_DTBS	:= dtbs

all:	$(KBUILD_IMAGE) $(KBUILD_DTBS)

boot := arch/arm64/boot

Image Image.gz: vmlinux scripts dtbs
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

zinstall install: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $@

%.dtb: scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts $(boot)/dts/$@

dtbs: scripts
	$(Q)$(MAKE) $(build)=$(boot)/dts dtbs
	$(Q)cp $(boot)/dts/$(DTB_LIST) $(boot)

PHONY += vdso_install
vdso_install:
	$(Q)$(MAKE) $(build)=arch/arm64/kernel/vdso $@

Image.gz-dtb: vmlinux scripts dtbs
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

# We use MRPROPER_FILES and CLEAN_FILES now
archclean:
	$(Q)$(MAKE) $(clean)=$(boot)

define archhelp
  echo  '* Image.gz      - Compressed kernel image (arch/$(ARCH)/boot/Image.gz)'
  echo  '  Image         - Uncompressed kernel image (arch/$(ARCH)/boot/Image)'
  echo  '* dtbs          - Build device tree blobs for enabled boards'
  echo  '  install       - Install uncompressed kernel'
  echo  '  zinstall      - Install compressed kernel'
  echo  '                  Install using (your) ~/bin/installkernel or'
  echo  '                  (distribution) /sbin/installkernel or'
  echo  '                  install to $$(INSTALL_PATH) and run lilo'
endef
