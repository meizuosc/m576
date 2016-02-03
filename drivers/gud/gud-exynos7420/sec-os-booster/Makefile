#
# Makefile for Secure OS booster API
#
obj-$(CONFIG_SECURE_OS_BOOSTER_API) += sec_os_booster.o

# MobiCore kernel driver path
GUD_ROOT_FOLDER := drivers/gud/gud-exynos7420

# Choose one platform from the folder
MOBICORE_PLATFORM := $(shell (ls -1 $(GUD_ROOT_FOLDER)/MobiCoreDriver/platforms | tail -1) )
# Use the available platform folder
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver/platforms/$(MOBICORE_PLATFORM)

# Includes MobiCore kernel driver
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver/public
