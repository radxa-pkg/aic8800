CONFIG_AIC8800_BTLPM_SUPPORT := m
CONFIG_AIC8800_WLAN_SUPPORT := y
CONFIG_AIC_WLAN_SUPPORT := y

obj-$(CONFIG_AIC8800_BTLPM_SUPPORT) += aic8800_btlpm/
obj-$(CONFIG_AIC8800_WLAN_SUPPORT) += aic8800_fdrv/
obj-$(CONFIG_AIC_WLAN_SUPPORT) += aic8800_bsp/

# Platform support list
CONFIG_PLATFORM_ROCKCHIP = n
CONFIG_PLATFORM_ALLWINNER = n
CONFIG_PLATFORM_AMLOGIC = y
CONFIG_PLATFORM_UBUNTU = n

ifeq ($(CONFIG_PLATFORM_ROCKCHIP), y)
#KDIR = /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/kernel
#ARCH = arm
#CROSS_COMPILE = /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-
KDIR  = /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/kernel
ARCH = arm
CROSS_COMPILE = /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR  = /home/yaya/E/Rockchip/3399/rk3399-android-10/kernel
#ARCH = arm64
#CROSS_COMPILE = /home/yaya/E/Rockchip/3399/rk3399-android-10/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
ccflags-y += -DANDROID_PLATFORM
ccflags-y += -DCONFIG_PLATFORM_ROCKCHIP
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
KDIR  = /home/yaya/E/Allwinner/R818/R818/AndroidQ/lichee/kernel/linux-4.9
ARCH = arm64
CROSS_COMPILE = /home/yaya/E/Allwinner/R818/R818/AndroidQ/lichee/out/gcc-linaro-5.3.1-2016.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
ccflags-y += -DANDROID_PLATFORM
#ARCH = arm
#CROSS_COMPILE = /home/yaya/E/Amlogic/905x3/android9.0/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-
#KDIR = /home/yaya/E/Amlogic/905x3/android9.0/out/target/product/u202/obj/KERNEL_OBJ/
#ARCH = arm64
#CROSS_COMPILE = aarch64-linux-gnu-
#KDIR = /home/yaya/E/Amlogic/905x4/android-R-aosp/out/android11-5.4/common
endif


$(warning =============================================)
$(warning aic wifi driver LINUX_SRC=$(KERNEL_SRC))
$(warning CROSS_COMPILE=$(CROSS_COMPILE))
$(warning =============================================)


all: modules
modules_install:
	@$(MAKE) INSTALL_MOD_STRIP=1 M=$(M) -C $(KERNEL_SRC) modules_install
	mkdir -p ${OUT_DIR}/../vendor_lib/modules
	cd ${OUT_DIR}/$(M)/; find -name "*.ko" -exec cp {} ${OUT_DIR}/../vendor_lib/modules/ \;

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules


clean:
	cd aic8800_bsp/;make clean;cd ..
	cd aic8800_fdrv/;make clean;cd ..
	cd aic8800_btlpm/;make clean;cd ..
	rm -rf modules.order Module.symvers 

