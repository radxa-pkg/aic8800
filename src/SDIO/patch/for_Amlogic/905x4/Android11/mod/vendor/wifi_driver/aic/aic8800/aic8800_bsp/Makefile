CONFIG_SDIO_SUPPORT := y
CONFIG_SDIO_PWRCTRL := y
CONFIG_AIC_FW_PATH = "/vendor/etc/firmware"
export CONFIG_AIC_FW_PATH
ccflags-y += -DCONFIG_AIC_FW_PATH=\"$(CONFIG_AIC_FW_PATH)\"

MODULE_NAME := aic8800_bsp
ifeq ($(CONFIG_SDIO_SUPPORT), y)
ccflags-y += -DAICWF_SDIO_SUPPORT
ccflags-$(CONFIG_SDIO_PWRCTRL) += -DCONFIG_SDIO_PWRCTRL
endif

CONFIG_GPIO_WAKEUP = n
CONFIG_LINK_DET_5G = y
CONFIG_MCU_MESSAGE = n

ccflags-$(CONFIG_GPIO_WAKEUP) += -DCONFIG_GPIO_WAKEUP
ccflags-$(CONFIG_LINK_DET_5G) += -DCONFIG_LINK_DET_5G
ccflags-$(CONFIG_MCU_MESSAGE) += -DCONFIG_MCU_MESSAGE


obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
	aic_bsp_main.o \
	aic_bsp_driver.o \
	aicsdio.o \
	aicsdio_txrxif.o \
	md5.o


# Platform support list
CONFIG_PLATFORM_ROCKCHIP := n
CONFIG_PLATFORM_ALLWINNER := n
CONFIG_PLATFORM_INGENIC_T20 := n
CONFIG_PLATFORM_AMLOGIC := y
CONFIG_PLATFORM_UBUNTU := n

ifeq ($(CONFIG_PLATFORM_ROCKCHIP), y)
ccflags-$(CONFIG_PLATFORM_ROCKCHIP) += -DCONFIG_PLATFORM_ROCKCHIP
#KDIR  := /home/yaya/E/Rockchip/3399/rk3399-android-10/kernel
#ARCH := arm64
#CROSS_COMPILE := /home/yaya/E/Rockchip/3399/rk3399-android-10/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
#KDIR  := /home/yaya/E/Rockchip/3288/Android10/kernel/kernel/
#ARCH := arm
#CROSS_COMPILE := /home/yaya/E/Rockchip/3288/Android10/tool/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR := /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/kernel
#ARCH := arm
#CROSS_COMPILE := /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-
#KDIR := /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/kernel
#ARCH := arm
#CROSS_COMPILE := /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR := /home/yaya/E/Rockchip/3566/firefly/Android11.0/Firefly-RK356X_Android11.0_git_20210824/RK356X_Android11.0/kernel
#KDIR := /home/yaya/E/Rockchip/3566/oudu/kernel
#KDIR := /home/yaya/E/Rockchip/3566/shengteng/kernel
#ARCH := arm64
#CROSS_COMPILE := ~/E/Rockchip/3566/Android11/rk3566_rk3568_android11_oranth/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
#KDIR := /home/yaya/E/Rockchip/3328/Android9/SDK/kernel/
#ARCH := arm64
#CROSS_COMPILE := /home/yaya/E/Rockchip/3328/Android9/SDK/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
#KDIR := /home/yaya/E/Rockchip/3288/Android11/kernel
#ARCH := arm
#CROSS_COMPILE := /home/yaya/E/Rockchip/3288/Android11/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
KDIR ?= /home/yaya/E/Rockchip/3566/firefly/Android11.0/Firefly-RK356X_Android11.0_git_20210824/RK356X_Android11.0/kernel
ARCH ?= arm64
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3566/Android11/rk3566_rk3568_android11_oranth/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
#KDIR  ?= /home/yaya/E/Allwinner/A133/a133-sdk/android/longan/out/kernel/build/
#ARCH ?= arm64
#CROSS_COMPILE ?= /home/yaya/E/Allwinner/A133/a133-sdk/android/longan/out/gcc-linaro-5.3.1-2016.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
KDIR  ?= /home/yaya/E/Allwinner/r818/Android10/lichee/kernel/linux-4.9/
ARCH ?= arm64
CROSS_COMPILE ?= /home/yaya/E/Allwinner/r818/Android10/android/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

endif

ifeq ($(CONFIG_PLATFORM_INGENIC_T20), y)
KDIR ?= /home/yaya/E/Ingenic/T40/kernel
ARCH ?= mips
CROSS_COMPILE ?= /home/yaya/E/Ingenic/T40/mips-linux-gnu-ingenic-gcc7.2.0-glibc2.29-fp64/bin/mips-linux-gnu-
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
ccflags-$(CONFIG_PLATFORM_AMLOGIC) += -DCONFIG_PLATFORM_AMLOGIC
#ARCH = arm
#CROSS_COMPILE = /home/yaya/E/Amlogic/905x3/android9.0/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-
#KDIR = /home/yaya/E/Amlogic/905x3/android9.0/out/target/product/u202/obj/KERNEL_OBJ/
ARCH = arm64
CROSS_COMPILE = aarch64-linux-gnu-
KDIR = /home/yaya/E/Amlogic/905x4/android-R-aosp/out/android11-5.4/common
endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
ccflags-$(CONFIG_PLATFORM_UBUNTU) += -DCONFIG_PLATFORM_UBUNTU
KDIR  ?= /lib/modules/$(shell uname -r)/build
PWD   ?= $(shell pwd)
KVER ?= $(shell uname -r)
MODDESTDIR ?= /lib/modules/$(KVER)/kernel/drivers/net/wireless/
ARCH ?= x86_64
CROSS_COMPILE ?=
endif


all: modules
modules:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
clean:
	rm -rf *.o *.ko *.o.* *.mod.* modules.* Module.* .a* .o* .*.o.* *.mod .tmp* .cache.mk built-in.a

