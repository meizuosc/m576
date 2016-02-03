# Makefile of modem_if

EXTRA_CFLAGS += -Idrivers/misc/modem_v1
EXTRA_CFLAGS += -lnet/core/

obj-y += modem_main.o ipc_common.o modem_io_device.o
obj-y += modem_net_flowcontrol_device.o modem_utils.o

obj-$(CONFIG_UMTS_MODEM_SS333) += modem_modemctl_device_ss333.o boot_device_spi.o

obj-$(CONFIG_LINK_DEVICE_LLI) += modem_link_device_lli.o

obj-$(CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM) += modem_link_device_pm.o

# Check whether or not memory-type interface
ifeq ($(CONFIG_LINK_DEVICE_LLI),y)
LINK_DEVICE_MEMORY_INTERFACE=y
endif

ifdef LINK_DEVICE_MEMORY_INTERFACE
obj-y += modem_link_device_memory.o
endif
