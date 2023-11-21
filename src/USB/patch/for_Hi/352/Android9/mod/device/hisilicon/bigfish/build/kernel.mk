#----------------------------------------------------------------------
# Compile Linux Kernel
#----------------------------------------------------------------------
include $(CLEAR_VARS)
## Hisilicon add begin
ifeq ($(HIFAST),1)
-include $(SDK_DIR)/$(SDK_CFGFILE)
endif
## Hisilicon add end
KERNEL_OUT := $(HISI_BUILD_TOP)/$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
ATF_OUT := $(HISI_BUILD_TOP)/$(TARGET_OUT_INTERMEDIATES)/ATF_OBJ
KERNEL_IMAGE := $(KERNEL_OUT)/boot.img
KERNEL_CONFIG_DIR := $(KERNEL_OUT)/include/config
KERNEL_RELEASE := $(KERNEL_CONFIG_DIR)/kernel.release

TRUSTEDCORE_OUT := $(PWD)/$(TARGET_OUT_INTERMEDIATES)/TRUSTEDCORE_OBJ
TRUSTEDCORE_IMAGE := $(TRUSTEDCORE_OUT)/trustedcore.img

ifeq ($(HISILICON_TEE),true)
DISABLE_TEE =
else
DISABLE_TEE = 1
endif

BOARD_PREBUILT_DTBOIMAGE := $(KERNEL_OUT)/dtbo.img
ifeq ($(strip $(BOARD_AVB_ENABLE)),true)
AVB_FLAGS += CONFIG_AVB_SUPPORT=y
DTB_FILE_NAME = $(DTB_FILE_NAME_NO_SUFFIX)_avb.dtb
$(info DTB_FILE_NAME = $(DTB_FILE_NAME))
endif

#----------------------------------------------------------------------
# Update Wifi Drivers
#----------------------------------------------------------------------
CFG_HI_WIFI_SUPPORT=$(HI_WIFI_SUPPORT)
CFG_HI_BLUETOOTH_SUPPORT=$(HI_BLUETOOTH_SUPPORT)
WIFI_CFG := \
	CFG_HI_WIFI_DEVICE_MT7601U=$(BOARD_WIFI_MT7601U) \
	CFG_HI_WIFI_DEVICE_RTL8188FTV=$(BOARD_WIFI_RTL8188FTV) \
	CFG_HI_WIFI_DEVICE_RTL8822BEH=$(BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822BEH) \
	CFG_HI_WIFI_DEVICE_RTL8723DU=$(BOARD_WIFI_BLUETOOTH_DEVICE_RTL8723DU) \
	CFG_HI_WIFI_DEVICE_RTL8822CU=$(BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822CU) \
	CFG_HI_WIFI_DEVICE_MT7668U=$(BOARD_WIFI_BLUETOOTH_DEVICE_MT7668U) \
	CFG_HI_WIFI_DEVICE_ATBM6022=$(BOARD_WIFI_ATBM6022) \
	CFG_HI_WIFI_DEVICE_AIC8800D=$(BOARD_WIFI_BLUETOOTH_DEVICE_AIC8800D) \
	CFG_HI_WIFI_MODE_STA=y CFG_HI_WIFI_MODE_AP=y

BT_CFG := \
	CFG_HI_BT_DEVICE_REALTEK=$(BOARD_BLUETOOTH_DEVICE_REALTEK) \
	CFG_HI_BT_DEVICE_MT7668U=$(BOARD_BLUETOOTH_DEVICE_MT7668U) \
	CFG_HI_BT_DEVICE_AIC8800D=$(BOARD_BLUETOOTH_DEVICE_AIC8800D)

kernel_prepare:
	mkdir -p $(KERNEL_OUT)
	mkdir -p $(ATF_OUT)
	mkdir -p $(EMMC_PRODUCT_OUT)
	mkdir -p $(KERNEL_CONFIG_DIR)
	cd $(SDK_DIR);$(MAKE) msp_prepare SDK_CFGFILE=$(SDK_CFGFILE) PLATFORM_VERSION=$(PLATFORM_VERSION); cd -;
ifeq ($(CFG_HI_WIFI_SUPPORT),y)
	rm -rf $(KERNEL_OUT)/drivers/wifi
	cd $(HISI_KERNEL_SOURCE)/drivers && rm -rf wifi \
		&& ln -s $(HISI_BUILD_TOP)/$(SDK_DIR)/source/component/wifi/drv ./wifi && cd -;
endif

ifeq ($(CFG_HI_BLUETOOTH_SUPPORT),y)
	rm -rf $(KERNEL_OUT)/drivers/bluetooth_usb
	cd $(HISI_KERNEL_SOURCE)/drivers && rm -rf bluetooth_usb \
		&& ln -s $(HISI_BUILD_TOP)/$(SDK_DIR)/../bluetooth/ ./bluetooth_usb && cd -;
endif

	if [ -d $(HISI_KERNEL_SOURCE) ]; then \
	$(MAKE) -C $(HISI_KERNEL_SOURCE) distclean; \
	fi
	echo "build kernel config: KERNEL_CONFIG=$(ANDROID_KERNEL_CONFIG)"

kernel_config: kernel_prepare
	$(MAKE) -C $(HISI_KERNEL_SOURCE) mrproper
	$(MAKE) -C $(HISI_KERNEL_SOURCE) \
	SDK_CFGFILE=$(SDK_CFGFILE) \
	O=$(KERNEL_OUT) \
	ARCH=$(TARGET_ARCH) CROSS_COMPILE=$(CROSS_COMPILE_TOOLCHAIN) \
	$(ANDROID_KERNEL_CONFIG)

kernel_menuconfig: kernel_config
	$(MAKE) -C $(HISI_KERNEL_SOURCE) O=$(KERNEL_OUT) \
		SDK_CFGFILE=$(SDK_CFGFILE) \
		ARCH=$(TARGET_ARCH) CROSS_COMPILE=$(CROSS_COMPILE_TOOLCHAIN) menuconfig

ifeq ($(TARGET_ARCH), arm)
$(KERNEL_IMAGE): kernel_config $(INSTALLED_RAMDISK_TARGET) | $(ACP)
	echo "Android make kernel" > $(KERNEL_RELEASE);
	cp -avf $(PRODUCT_OUT)/ramdisk.img $(PRODUCT_OUT)/ramdisk.cpio.gz
	$(MAKE) -C $(HISI_KERNEL_SOURCE) O=$(KERNEL_OUT) \
		SDK_CFGFILE=$(SDK_CFGFILE) \
		ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE_TOOLCHAIN) \
		LOCALVERSION= \
		HI_CONFIG_WIFI=$(CFG_HI_WIFI_SUPPORT) $(WIFI_CFG) \
		HI_CONFIG_BLUETOOTH=$(CFG_HI_BLUETOOTH_SUPPORT) $(BT_CFG) \
		CONFIG_MSP=y $(AVB_FLAGS) \
		CONFIG_INITRAMFS_SOURCE=$(HISI_BUILD_TOP)/$(PRODUCT_OUT)/ramdisk.cpio.gz \
		-j 4;
	$(MAKE) -C $(HISI_KERNEL_SOURCE) O=$(KERNEL_OUT) \
		SDK_CFGFILE=$(SDK_CFGFILE) \
		ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE_TOOLCHAIN) \
		LOCALVERSION= \
		CONFIG_MSP=y \
		CONFIG_INITRAMFS_SOURCE=$(HISI_BUILD_TOP)/$(PRODUCT_OUT)/ramdisk.cpio.gz \
		-j 128 uImage;
	if [ -d $(HISI_ATF_SOURCE) ]; then \
	$(MAKE) -C $(HISI_ATF_SOURCE)	\
		BUILD_BASE=$(ATF_OUT)		\
		CROSS_COMPILE=aarch64-hi100-linux- \
		CONFIG_BL31_BASE=$(CFG_HI_ATF_BASE)             \
		PLAT=$(SDK_HI_CHIP_TYPE)			\
		$(if $(DISABLE_TEE),DISABLE_TEE=$(DISABLE_TEE),)  \
		$(if $(DISABLE_TEE), SPD=none,) \
		bl31 -j20; \
		cp -avf $(ATF_OUT)/$(SDK_HI_CHIP_TYPE)/release/bl31.bin $(EMMC_PRODUCT_OUT)/atf.bin; \
	else \
		cp -avf $(SDK_DIR)/pub/image/$(SDK_HI_CHIP_TYPE)/$(ATF_FILE_NAME) $(EMMC_PRODUCT_OUT)/atf.bin; \
	fi
	cp -avf $(KERNEL_OUT)/arch/arm/boot/uImage $(PRODUCT_OUT)/kernel
	$(hide) cp -avf $(KERNEL_OUT)/arch/arm/boot/uImage $@ ;
ifeq ($(PRODUCT_SUPPORT_ANDROIDTV_TYPE),full)
	$(MKDTBOIMG) create $(BOARD_PREBUILT_DTBOIMAGE) $(KERNEL_OUT)/arch/arm/boot/dts/$(DTB_FILE_NAME)
else
	$(hide) cp -avf $(KERNEL_OUT)/arch/arm/boot/dts/$(DTB_FILE_NAME) $(BOARD_PREBUILT_DTBOIMAGE)
endif
	if [ -e $(SDK_DIR)/tools/linux/utils/image_mate ]; then \
		$(SDK_DIR)/tools/linux/utils/image_mate $(BOARD_PREBUILT_DTBOIMAGE) $(BOARD_PREBUILT_DTBOIMAGE); \
	fi
ifneq ($(strip $(BOARD_AVB_ENABLE)),true)
	$(hide) cp -avf $(KERNEL_OUT)/arch/arm/boot/dts/$(DTB_FILE_NAME) $(EMMC_PRODUCT_OUT)/dtbo.img;
	$(hide) chmod a+r $(KERNEL_IMAGE);
	cp -avf $@ $(EMMC_PRODUCT_OUT)
endif
else
$(KERNEL_IMAGE): kernel_config $(INSTALLED_RAMDISK_TARGET) | $(ACP)
	echo "Android make kernel" > $(KERNEL_RELEASE);
	cp -avf $(PRODUCT_OUT)/ramdisk.img $(PRODUCT_OUT)/ramdisk.cpio.gz
	$(MAKE) -C $(HISI_KERNEL_SOURCE) O=$(KERNEL_OUT) \
		SDK_CFGFILE=$(SDK_CFGFILE) \
		ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE_TOOLCHAIN) \
		LOCALVERSION= \
		HI_CONFIG_WIFI=$(CFG_HI_WIFI_SUPPORT) $(WIFI_CFG) \
		HI_CONFIG_BLUETOOTH=$(CFG_HI_BLUETOOTH_SUPPORT) $(BT_CFG) \
		CONFIG_MSP=y $(AVB_FLAGS) \
		CONFIG_INITRAMFS_SOURCE=$(HISI_BUILD_TOP)/$(PRODUCT_OUT)/ramdisk.cpio.gz \
		-j 4;
	if [ -d $(HISI_ATF_SOURCE) ]; then \
	$(MAKE) -C $(HISI_ATF_SOURCE)	\
		BUILD_BASE=$(ATF_OUT)		\
		CROSS_COMPILE=$(CROSS_COMPILE_TOOLCHAIN) \
		CONFIG_BL31_BASE=$(CFG_HI_ATF_BASE)             \
		PLAT=$(SDK_HI_CHIP_TYPE)			\
		$(if $(DISABLE_TEE),DISABLE_TEE=$(DISABLE_TEE),)  \
		$(if $(DISABLE_TEE), SPD=none,) \
		bl31 -j20; \
		cp -avf $(ATF_OUT)/$(SDK_HI_CHIP_TYPE)/release/bl31.bin $(EMMC_PRODUCT_OUT)/atf.bin;\
	else \
		cp -avf $(SDK_DIR)/pub/image/$(SDK_HI_CHIP_TYPE)/$(ATF_FILE_NAME) $(EMMC_PRODUCT_OUT)/atf.bin;\
	fi
	cp -avf $(KERNEL_OUT)/arch/$(CFG_HI_CPU_ARCH)/boot/Image $(PRODUCT_OUT)/kernel
	$(hide) cp -avf $(KERNEL_OUT)/arch/arm64/boot/Image $@ ;
ifeq ($(PRODUCT_SUPPORT_ANDROIDTV_TYPE),full)
		$(MKDTBOIMG) create $(BOARD_PREBUILT_DTBOIMAGE) $(KERNEL_OUT)/arch/arm/boot/dts/$(DTB_FILE_NAME)
else
		$(hide) cp -avf $(KERNEL_OUT)/arch/arm64/boot/dts/hisilicon/$(DTB_FILE_NAME) $(BOARD_PREBUILT_DTBOIMAGE)
endif
	if [ -e $(SDK_DIR)/tools/linux/utils/image_mate ]; then \
		$(SDK_DIR)/tools/linux/utils/image_mate $(BOARD_PREBUILT_DTBOIMAGE) $(BOARD_PREBUILT_DTBOIMAGE); \
	fi
ifneq ($(strip $(BOARD_AVB_ENABLE)),true)
	$(hide) cp -avf $(KERNEL_OUT)/arch/arm64/boot/dts/hisilicon/$(DTB_FILE_NAME) $(EMMC_PRODUCT_OUT)/dtbo.img;
	$(hide) chmod a+r $(KERNEL_IMAGE);
	cp -avf $@ $(EMMC_PRODUCT_OUT)
endif
endif

.PHONY: kernel $(KERNEL_IMAGE) $(INSTALLED_BOOTIMAGE_TARGET) $(INSTALLED_DTBOIMAGE_TARGET)
$(INSTALLED_KERNEL_TARGET): $(KERNEL_IMAGE)
$(BOARD_PREBUILT_DTBOIMAGE): $(KERNEL_IMAGE)
kernel: $(INSTALLED_BOOTIMAGE_TARGET) $(INSTALLED_DTBOIMAGE_TARGET)
	cp -avf $(INSTALLED_BOOTIMAGE_TARGET) $(EMMC_PRODUCT_OUT)/boot.img
	cp -avf $(INSTALLED_DTBOIMAGE_TARGET) $(EMMC_PRODUCT_OUT)/dtbo.img
	if [ -e $(SDK_DIR)/tools/linux/utils/image_mate ]; then \
	$(SDK_DIR)/tools/linux/utils/image_mate $(EMMC_PRODUCT_OUT)/atf.bin $(EMMC_PRODUCT_OUT)/atf.bin;\
	fi

ifeq ($(CFG_HI_WIFI_SUPPORT),y)
	mkdir -p  $(HISI_BUILD_TOP)/$(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/modules
	$(hide) (find $(KERNEL_OUT)/drivers/wifi -name "*.ko" | xargs cp -avf -t $(HISI_BUILD_TOP)/$(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/modules)
endif

ifeq ($(CFG_HI_BLUETOOTH_SUPPORT),y)
	mkdir -p  $(HISI_BUILD_TOP)/$(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/modules
	$(hide) (find $(KERNEL_OUT)/drivers/bluetooth_usb -name "*.ko" | xargs cp -avf -t $(HISI_BUILD_TOP)/$(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/modules)
endif

ifeq ($(strip $(BOARD_AVB_ENABLE)),true)
	echo "atf common sign"
	$(hide) mkdir -p $(SECURE_OBJ_DIR)
	$(hide) $(SDK_LINUX_SIGN_TOOL_DIR)/CASignTool 2 $(SECURE_CONFIG_DIR)/common_atf_config.cfg -k $(SECURE_RSA_KEY_DIR) -r $(EMMC_PRODUCT_OUT) -o $(SECURE_OBJ_DIR)
	$(hide) cp -arv $(SECURE_OBJ_DIR)/atf.bin_Sign.img $(HISI_BUILD_TOP)/$(EMMC_PRODUCT_OUT)/atf.bin

	echo "atf avb sign ..."
	$(AVB_TOOL) add_hash_footer --hash_algorithm sha256 --image $(EMMC_PRODUCT_OUT)/atf.bin --partition_size $(BOARD_ATF_PARTITION_SIZE) --partition_name atf
endif
#----------------------------------------------------------------------
# Linux Kernel End
#----------------------------------------------------------------------
