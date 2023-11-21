CONFIG_USB_SUPPORT =y
CONFIG_RFTEST =y

# Platform support list
CONFIG_PLATFORM_NANOPI_M4 = y
CONFIG_PLATFORM_ROCHCHIP ?= y
CONFIG_PLATFORM_ALLWINNER ?= n
CONFIG_PLATFORM_AMLOGIC ?= n
CONFIG_PLATFORM_UBUNTU ?= n

CONFIG_AIC_LOADFW_SUPPORT = m
MODULE_NAME = aic_load_fw
CONFIG_AIC_FW_PATH = "/vendor/etc/firmware"
export CONFIG_AIC_FW_PATH

ifeq ($(CONFIG_USB_SUPPORT), y)
ccflags-y += -DAICWF_USB_SUPPORT
endif
ccflags-$(CONFIG_RFTEST) += -DCONFIG_RFTEST
ccflags-$(CONFIG_PLATFORM_NANOPI_M4) += -DCONFIG_PLATFORM_NANOPI_M4
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
ccflags-$(CONFIG_PLATFORM_AMLOGIC) += -DCONFIG_PLATFORM_AMLOGIC
ccflags-$(CONFIG_PLATFORM_UBUNTU) += -DCONFIG_PLATFORM_UBUNTU

obj-$(CONFIG_AIC_LOADFW_SUPPORT) := $(MODULE_NAME).o
$(MODULE_NAME)-y := 	aic_bluetooth_main.o \
					aicbluetooth.o \
					aicwf_usb.o \
					aic_txrxif.o \
					aicbluetooth_cmds.o



ifeq ($(CONFIG_PLATFORM_ROCHCHIP), y)
KDIR  := /home/yaya/E/Rockchip/3229/Android10/SDK/kernel/
ARCH ?= arm
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android10/SDK/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)
KVER := $(shell uname -r)
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/
ARCH ?= x86_64
CROSS_COMPILE ?=
endif


all: modules
modules:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

install:
	mkdir -p $(MODDESTDIR)
	install -p -m 644 $(MODULE_NAME).ko  $(MODDESTDIR)
	/sbin/depmod -a ${KVER}

uninstall:
	rm -rfv $(MODDESTDIR)/$(MODULE_NAME).ko
	/sbin/depmod -a ${KVER}

clean:
	rm -rf *.o *.ko *.o.* *.mod.* modules.* Module.* .a* .o* .*.o.* *.mod .tmp* .cache.mk
