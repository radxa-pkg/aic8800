CONFIG_USB_SUPPORT =y
CONFIG_RFTEST =y
CONFIG_USB_BT =y

# Platform support list
CONFIG_PLATFORM_ROCHCHIP ?= y
CONFIG_PLATFORM_ALLWINNER ?= n
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
ccflags-$(CONFIG_PLATFORM_ROCHCHIP) += -DCONFIG_PLATFORM_ROCHCHIP
KDIR := /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/kernel
ARCH ?= arm
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
#KDIR  := /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/kernel
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR  := /home/yaya/E/Rockchip/3399/rk3399-android-10/kernel
#ARCH ?= arm64
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3399/rk3399-android-10/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
KDIR  := /home/yaya/E/Allwinner/R818/R818/AndroidQ/lichee/kernel/linux-4.9
ARCH ?= arm64
CROSS_COMPILE ?= /home/yaya/E/Allwinner/R818/R818/AndroidQ/lichee/out/gcc-linaro-5.3.1-2016.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
ccflags-$(CONFIG_PLATFORM_AMLOGIC) += -DCONFIG_PLATFORM_AMLOGIC
ARCH := arm
CROSS_COMPILE := /home/yaya/D/Workspace/CyberQuantum/JinHaoYue/amls905x3/SDK/20191101-0tt-asop/android9.0/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-
KDIR := /home/yaya/D/Workspace/CyberQuantum/JinHaoYue/amls905x3/SDK/20191101-0tt-asop/android9.0/out/target/product/u202/obj/KERNEL_OBJ/

endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
ccflags-$(CONFIG_PLATFORM_UBUNTU) += -DCONFIG_PLATFORM_UBUNTU
#KDIR  := /lib/modules/$(shell uname -r)/build
KDIR  := /home/yaya/D/Workspace/CyberQuantum/AIC/Linux/linux-3.14.29/
#PWD   := $(shell pwd)
#KVER := $(shell uname -r)
#MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/aic8800
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
