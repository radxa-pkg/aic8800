CONFIG_USB_SUPPORT =y
CONFIG_RFTEST =y

# Platform support list
CONFIG_PLATFORM_ROCHCHIP ?= n
CONFIG_PLATFORM_ALLWINNER ?= y
CONFIG_PLATFORM_AMLOGIC ?= n
CONFIG_PLATFORM_UBUNTU ?= n

CONFIG_AIC_LOADFW_SUPPORT = m
MODULE_NAME = aic_load_fw
# CONFIG_AIC_FW_PATH = "/vendor/etc/firmware"
# export CONFIG_AIC_FW_PATH

ifeq ($(CONFIG_USB_SUPPORT), y)
ccflags-y += -DAICWF_USB_SUPPORT
endif
ccflags-$(CONFIG_RFTEST) += -DCONFIG_RFTEST
ccflags-$(CONFIG_USB_BT) += -DCONFIG_USB_BT

obj-$(CONFIG_AIC_LOADFW_SUPPORT) := $(MODULE_NAME).o
$(MODULE_NAME)-y := 	aic_bluetooth_main.o \
					aicbluetooth.o \
					aicwf_usb.o \
					aic_txrxif.o \
					aicbluetooth_cmds.o \
					md5.o


ifeq ($(CONFIG_PLATFORM_ROCHCHIP), y)
KDIR  := /home/yaya/E/Rockchip/3229/Android10/SDK/kernel/
ARCH ?= arm
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android10/SDK/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
	ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
	ccflags-$(CONFIG_PLATFORM_AMLOGIC) += -DCONFIG_PLATFORM_AMLOGIC
endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
ccflags-$(CONFIG_PLATFORM_UBUNTU) += -DCONFIG_PLATFORM_UBUNTU
KDIR  := /lib/modules/$(shell uname -r)/build
#KDIR := /home/yaya/D/Workspace/CyberQuantum/Linux/linux-5.7.7/
PWD   := $(shell pwd)
KVER := $(shell uname -r)
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/aic8800
ARCH := x86_64
CROSS_COMPILE :=
endif


all: modules
modules:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

install:
	mkdir -p $(MODDESTDIR)
	install -p -m 644 $(MODULE_NAME).ko  $(MODDESTDIR)/
	/sbin/depmod -a ${KVER}

uninstall:
	rm -rfv $(MODDESTDIR)/$(MODULE_NAME).ko
	/sbin/depmod -a ${KVER}

clean:
	rm -rf *.o *.ko *.o.* *.mod.* modules.* Module.* .a* .o* .*.o.* *.mod .tmp* .cache.mk
