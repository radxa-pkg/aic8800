CONFIG_AIC8800_BTLPM_SUPPORT = m

obj-$(CONFIG_AIC8800_BTLPM_SUPPORT) := aic8800_btlpm.o

ccflags-y += -I$(srctree)/$(src)/../aic8800_bsp

aic8800_btlpm-y := \
	aic_bluetooth_main.o \
	rfkill.o \
	lpm.o

# Platform support list
CONFIG_PLATFORM_ROCKCHIP ?= n
CONFIG_PLATFORM_ALLWINNER ?= n
CONFIG_PLATFORM_AMLOGIC ?= y
CONFIG_PLATFORM_UBUNTU ?= n

ccflags-y += -DAIC_TRACE_INCLUDE_PATH=$(src)

ifeq ($(CONFIG_PLATFORM_ROCKCHIP), y)
KDIR  ?= /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/kernel
ARCH ?= arm
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR  ?= /home/yaya/E/Rockchip/3229/Android10/SDK/kernel/
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android10/SDK/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR  ?= /home/yaya/E/Rockchip/3288/Android10/kernel/kernel/
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3288/Android10/tool/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux#-gnueabihf-
#KDIR ?= /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/kernel
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-
#KDIR ?= /home/yaya/E/Rockchip/3328/Android9/SDK/SDK/kernel
#ARCH ?= arm64
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3328/Android9/SDK/SDK/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-

ccflags-$(CONFIG_PLATFORM_ROCKCHIP) += -DCONFIG_PLATFORM_ROCKCHIP
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
KDIR  ?= /home/yaya/E/Allwinner/R818/R818/AndroidQ/lichee/kernel/linux-4.9/
ARCH ?= arm64
CROSS_COMPILE ?= /home/yaya/E/Allwinner/R818/R818/AndroidQ/android/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
ccflags-$(CONFIG_PLATFORM_AMLOGIC) += -DCONFIG_PLATFORM_AMLOGIC
#ARCH = arm
#CROSS_COMPILE = /home/yaya/E/Amlogic/905x3/android9.0/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-
#KDIR = /home/yaya/E/Amlogic/905x3/android9.0/out/target/product/u202/obj/KERNEL_OBJ/
ARCH = arm64
CROSS_COMPILE = aarch64-linux-gnu-
KDIR = /home/yaya/E/Amlogic/905x4/android-R-aosp/out/android11-5.4/common
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
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
	rm -rf *.o *.ko *.o.* *.mod.* modules.* Module.* .a* .o* .*.o.* *.mod .tmp* .cache.mk


