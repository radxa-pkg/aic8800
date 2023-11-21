CONFIG_AIC8800_BTLPM_SUPPORT = m

obj-$(CONFIG_AIC8800_BTLPM_SUPPORT) := aic8800_btlpm.o

ccflags-y += -I$(srctree)/$(src)/../aic8800_bsp

aic8800_btlpm-y := \
	aic_bluetooth_main.o \
	rfkill.o \
	lpm.o

# Platform support list
# CONFIG_PLATFORM_ROCHCHIP ?= n
# CONFIG_PLATFORM_ALLWINNER ?= y
# CONFIG_PLATFORM_AMLOGIC ?= n
# CONFIG_PLATFORM_UBUNTU ?= n

ccflags-y += -DAIC_TRACE_INCLUDE_PATH=$(src)

ifeq ($(CONFIG_PLATFORM_ROCHCHIP), y)
KDIR  := /home/yaya/E/Rockchip/3229/Android10/SDK/kernel/
ARCH ?= arm
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android10/SDK/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
ccflags-$(CONFIG_PLATFORM_ROCHCHIP) += -DCONFIG_PLATFORM_ROCHCHIP
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
ccflags-$(CONFIG_PLATFORM_NANOPI) += -DCONFIG_PLATFORM_NANOPI
ccflags-y += -DANDROID_PLATFORM
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


