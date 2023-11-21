CONFIG_SDIO_SUPPORT := y
CONFIG_AIC_FW_PATH = "/vendor/etc/firmware"
export CONFIG_AIC_FW_PATH
ccflags-y += -DCONFIG_AIC_FW_PATH=\"$(CONFIG_AIC_FW_PATH)\"

MODULE_NAME := aic8800_bsp
ifeq ($(CONFIG_SDIO_SUPPORT), y)
ccflags-y += -DAICWF_SDIO_SUPPORT
endif

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
	aic_bsp_main.o \
	aic_bsp_driver.o \
	aicsdio.o \
	aicsdio_txrxif.o \
	md5.o


# Platform support list
CONFIG_PLATFORM_ROCHCHIP ?= n
CONFIG_PLATFORM_ALLWINNER ?= y
CONFIG_PLATFORM_AMLOGIC ?= n
CONFIG_PLATFORM_UBUNTU ?= n

ifeq ($(CONFIG_PLATFORM_ROCHCHIP), y)
ccflags-$(CONFIG_PLATFORM_ROCHCHIP) += -DCONFIG_PLATFORM_ROCHCHIP
KDIR  := /home/yaya/E/Rockchip/3399/Android10/rk3399-android-10/kernel
ARCH ?= arm64
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3399/Android10/rk3399-android-10/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
KDIR  := /home/yaya/E/Allwinner/A133/a133-sdk/android/longan/kernel/linux-5.4/
ARCH := arm64
CROSS_COMPILE ?= /home/yaya/E/Allwinner/A133/a133-sdk/android/longan/build/toolchain/gcc-linaro-5.3.1-2016.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
KDIR  := /lib/modules/$(shell uname -r)/build
PWD   := $(shell pwd)
KVER := $(shell uname -r)
MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/
ARCH := x86_64
CROSS_COMPILE :=
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
	rm -rf *.o *.ko *.o.* *.mod.* modules.* Module.* .a* .o* .*.o.* *.mod .tmp* .cache.mk built-in.a

