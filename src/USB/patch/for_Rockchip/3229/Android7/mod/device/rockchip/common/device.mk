#
# Copyright 2014 Rockchip Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Prebuild apps
ifneq ($(strip $(TARGET_PRODUCT)), )
    TARGET_DEVICE_DIR=$(shell test -d device && find device -maxdepth 4 -path '*/$(TARGET_PRODUCT)/BoardConfig.mk')
    TARGET_DEVICE_DIR := $(patsubst %/,%,$(dir $(TARGET_DEVICE_DIR)))
#    $(info device-rockchip-common TARGET_DEVICE_DIR: $(TARGET_DEVICE_DIR))
    $(shell python $(LOCAL_PATH)/auto_generator.py $(TARGET_DEVICE_DIR) preinstall bundled_persist-app)
    $(shell python $(LOCAL_PATH)/auto_generator.py $(TARGET_DEVICE_DIR) preinstall_del bundled_uninstall_back-app)
    $(shell python $(LOCAL_PATH)/auto_generator.py $(TARGET_DEVICE_DIR) preinstall_del_forever bundled_uninstall_gone-app)
    -include $(TARGET_DEVICE_DIR)/preinstall/preinstall.mk
    -include $(TARGET_DEVICE_DIR)/preinstall_del/preinstall.mk
    -include $(TARGET_DEVICE_DIR)/preinstall_del_forever/preinstall.mk
endif

#add for Nougat Bring Up
#$(call inherit-product, device/rockchip/common/copy.mk)

# Box product use device/rockchip/common/tv/tv_base.mk instead
ifneq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
endif

PRODUCT_AAPT_CONFIG ?= normal large xlarge hdpi xhdpi xxhdpi
PRODUCT_AAPT_PREF_CONFIG ?= xhdpi

########################################################
# Kernel
########################################################
PRODUCT_COPY_FILES += \
    $(TARGET_PREBUILT_KERNEL):kernel

#SDK Version
PRODUCT_PROPERTY_OVERRIDES += \
    ro.rksdk.version=RK30_ANDROID$(PLATFORM_VERSION)-SDK-v1.00.00

# Filesystem management tools
PRODUCT_PACKAGES += \
    fsck.f2fs \
    mkfs.f2fs \
    fsck_f2fs

# PCBA tools
ifeq ($(strip $(TARGET_ROCKCHIP_PCBATEST)), true)
PRODUCT_PACKAGES += \
    pcba_core \
    bdt
endif

ifeq ($(strip $(BOARD_USE_LCDC_COMPOSER)), true)
# setup dalvik vm configs.
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

PRODUCT_PROPERTY_OVERRIDES += \
    ro.hwui.texture_cache_size=72 \
    ro.hwui.layer_cache_size=48 \
    ro.hwui.r_buffer_cache_size=8 \
    ro.hwui.path_cache_size=32 \
    ro.hwui.gradient_cache_size=1 \
    ro.hwui.drop_shadow_cache_size=6 \
    ro.hwui.texture_cache_flushrate=0.4 \
    ro.hwui.text_small_cache_width=1024 \
    ro.hwui.text_small_cache_height=1024 \
    ro.hwui.text_large_cache_width=2048 \
    ro.hwui.text_large_cache_height=1024 \
    ro.hwui.disable_scissor_opt=true \
    ro.rk.screenshot_enable=true   \
    sys.status.hidebar_enable=false   \
    persist.sys.ui.hw=true

else
ifeq ($(strip $(BOARD_USE_LOW_MEM)), true)
include frameworks/native/build/tablet-dalvik-heap.mk
else
include frameworks/native/build/tablet-7in-hdpi-1024-dalvik-heap.mk
endif
endif

PRODUCT_COPY_FILES += \
	device/rockchip/common/init.rockchip.rc:root/init.rockchip.rc \
    device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).rc:root/init.$(TARGET_BOARD_HARDWARE).rc \
    device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).usb.rc:root/init.$(TARGET_BOARD_HARDWARE).usb.rc \
    $(call add-to-product-copy-files-if-exists,device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).bootmode.emmc.rc:root/init.$(TARGET_BOARD_HARDWARE).bootmode.emmc.rc) \
    $(call add-to-product-copy-files-if-exists,device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).bootmode.unknown.rc:root/init.$(TARGET_BOARD_HARDWARE).bootmode.unknown.rc) \
    $(call add-to-product-copy-files-if-exists,device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).bootmode.nvme.rc:root/init.$(TARGET_BOARD_HARDWARE).bootmode.nvme.rc) \
    device/rockchip/common/ueventd.rockchip.rc:root/ueventd.$(TARGET_BOARD_HARDWARE).rc \
    device/rockchip/common/media_profiles_default.xml:system/etc/media_profiles_default.xml \
    device/rockchip/common/rk29-keypad.kl:system/usr/keylayout/rk29-keypad.kl \
    device/rockchip/common/ff680030_pwm.kl:system/usr/keylayout/ff680030_pwm.kl \
     device/rockchip/common/alarm_filter.xml:system/etc/alarm_filter.xml \
	device/rockchip/common/ff420030_pwm.kl:system/usr/keylayout/ff420030_pwm.kl

PRODUCT_COPY_FILES += \
    hardware/broadcom/wlan/bcmdhd/config/wpa_supplicant_overlay.conf:system/etc/wifi/wpa_supplicant_overlay.conf \
    hardware/broadcom/wlan/bcmdhd/config/p2p_supplicant_overlay.conf:system/etc/wifi/p2p_supplicant_overlay.conf

#for ssv6051
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/wifi/ssv6xxx/p2p_supplicant.conf:system/etc/wifi/p2p_supplicant.conf \

PRODUCT_PACKAGES += \
    libiconv \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf \
    dhcpcd.conf

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_PACKAGES += \
    libpppoe-jni \
    pppoe-service \
    librp-pppoe

PRODUCT_SYSTEM_SERVER_JARS += \
    pppoe-service
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.box.samba.rc:root/init.box.samba.rc
endif

ifeq ($(filter MediaTek_mt7601 MediaTek RealTek Espressif, $(strip $(BOARD_CONNECTIVITY_VENDOR))), )
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.connectivity.rc:root/init.connectivity.rc
endif

ifeq ($(findstring car,$(PRODUCT_BUILD_MODULE)),car)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio_policy_$(PRODUCT_BUILD_MODULE).conf:system/etc/audio_policy.conf
else
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio_policy_$(TARGET_BOARD_HARDWARE).conf:system/etc/audio_policy.conf
endif


PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/fstab.rk30board.bootmode.unknown:root/fstab.rk30board.bootmode.unknown \
    $(LOCAL_PATH)/fstab.rk30board.bootmode.emmc:root/fstab.rk30board.bootmode.emmc

# For audio-recoard 
PRODUCT_PACKAGES += \
    libsrec_jni

# For tts test
PRODUCT_PACKAGES += \
    libwebrtc_audio_coding

#camera
$(call inherit-product-if-exists, hardware/rockchip/camera/Config/rk32xx_camera.mk)
$(call inherit-product-if-exists, hardware/rockchip/camera/Config/user.mk)

#audio
$(call inherit-product-if-exists, hardware/rockchip/audio/tinyalsa_hal/codec_config/rk_audio.mk)

ifeq ($(BOARD_NFC_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:system/etc/permissions/android.hardware.nfc.hce.xml
endif

ifeq ($(BOARD_HAS_GPS),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml
endif

ifeq ($(BOARD_COMPASS_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml
endif

ifeq ($(BOARD_GYROSCOPE_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml
endif

ifeq ($(BOARD_PROXIMITY_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml
endif

ifeq ($(BOARD_LIGHT_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml
endif

# opengl aep feature
ifeq ($(BOARD_OPENGL_AEP),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:system/etc/permissions/android.hardware.opengles.aep.xml
endif

# CAMERA
ifeq ($(BOARD_CAMERA_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml
endif

# USB HOST
ifeq ($(BOARD_USB_HOST_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml
endif

# USB ACCESSORY
ifeq ($(BOARD_USB_ACCESSORY_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
    PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/box_core_hardware.xml:system/etc/permissions/box_core_hardware.xml 
else ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
    PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/vr_core_hardware.xml:system/etc/permissions/vr_core_hardware.xml 
else ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), laptop)
    PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/laptop_core_hardware.xml:system/etc/permissions/laptop_core_hardware.xml
else # tablet
    PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml 
endif

# Live Wallpapers
PRODUCT_PACKAGES += \
    LiveWallpapersPicker \
    NoiseField \
    PhaseBeam \
    librs_jni \
    libjni_pinyinime

# HAL
ifneq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
PRODUCT_PACKAGES += \
    power.$(TARGET_BOARD_PLATFORM) 
endif
PRODUCT_PACKAGES += \
    sensors.$(TARGET_BOARD_HARDWARE) \
    gralloc.$(TARGET_BOARD_HARDWARE) \
    hwcomposer.$(TARGET_BOARD_HARDWARE) \
    lights.$(TARGET_BOARD_PLATFORM) \
    camera.$(TARGET_BOARD_HARDWARE) \
    Camera \
    libvpu \
    libstagefrighthw \
    libgralloc_priv_omx \
    akmd 

# iep
ifneq ($(filter rk3188 rk3190 rk3026 rk3288 rk312x rk3126c rk3128 px3se rk3368 rk3328 rk3366 rk3399, $(strip $(TARGET_BOARD_PLATFORM))), )
BUILD_IEP := true
PRODUCT_PACKAGES += \
    libiep
else
BUILD_IEP := false
endif

# charge
PRODUCT_PACKAGES += \
    charger \
    charger_res_images

# Allows healthd to boot directly from charger mode rather than initiating a reboot.
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.enable_boot_charger_mode=0

# Add board.platform default property to parsing related rc
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.board.platform=$(strip $(TARGET_BOARD_PLATFORM))

PRODUCT_CHARACTERISTICS := tablet

# audio lib
PRODUCT_PACKAGES += \
    audio_policy.$(TARGET_BOARD_HARDWARE) \
    audio.primary.$(TARGET_BOARD_HARDWARE) \
    audio.alsa_usb.$(TARGET_BOARD_HARDWARE) \
    audio.a2dp.default\
    audio.r_submix.default\
    libaudioroute\
    audio.usb.default

# Filesystem management tools
# EXT3/4 support
PRODUCT_PACKAGES += \
    mke2fs \
    e2fsck \
    tune2fs \
    resize2fs

# audio lib
PRODUCT_PACKAGES += \
    libasound \
    alsa.default \
    acoustics.default \
    libtinyalsa \
    tinymix \
    tinyplay \
    tinypcminfo

PRODUCT_PACKAGES += \
	alsa.audio.primary.$(TARGET_BOARD_HARDWARE)\
	alsa.audio_policy.$(TARGET_BOARD_HARDWARE)

$(call inherit-product-if-exists, external/alsa-lib/copy.mk)
$(call inherit-product-if-exists, external/alsa-utils/copy.mk)


PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.strictmode.visual=false 

ifeq ($(strip $(BOARD_HAVE_BLUETOOTH)),true)
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.bt_enable=true
else
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.bt_enable=false
endif

ifeq ($(strip $(MT6622_BT_SUPPORT)),true)
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.btchip=mt6622
endif

ifeq ($(strip $(BLUETOOTH_USE_BPLUS)),true)
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.btchip=broadcom.bplus
endif

ifeq ($(strip $(BOARD_HAVE_FLASH)), true)
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.flash_enable=true
else
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.flash_enable=false
endif

ifeq ($(strip $(BOARD_SUPPORT_HDMI)), true)
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.hdmi_enable=true
else
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.hdmi_enable=false
endif

ifeq ($(strip $(MT7601U_WIFI_SUPPORT)),true)
    PRODUCT_PROPERTY_OVERRIDES += ro.rk.wifichip=mt7601u
endif


PRODUCT_TAGS += dalvik.gc.type-precise


########################################################
# build with UMS? CDROM?
########################################################
ifeq ($(strip $(BUILD_WITH_UMS)),true)
PRODUCT_PROPERTY_OVERRIDES +=               \
    ro.factory.hasUMS=true                  \
    persist.sys.usb.config=mass_storage,adb

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rockchip.hasUMS.true.rc:root/init.$(TARGET_BOARD_HARDWARE).environment.rc
else
ifeq ($(strip $(BUILD_WITH_CDROM)),true)
PRODUCT_PROPERTY_OVERRIDES +=                 \
    ro.factory.hasUMS=cdrom                   \
    ro.factory.cdrom=$(BUILD_WITH_CDROM_PATH) \
    persist.sys.usb.config=mass_storage,adb 

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rockchip.hasCDROM.true.rc:root/init.$(TARGET_BOARD_HARDWARE).environment.rc
else
PRODUCT_PROPERTY_OVERRIDES +=       \
    ro.factory.hasUMS=false         \
    persist.sys.usb.config=mtp,adb  \
    testing.mediascanner.skiplist = /mnt/shell/emulated/Android/

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rockchip.hasUMS.false.rc:root/init.$(TARGET_BOARD_HARDWARE).environment.rc
endif
endif

#$_rbox_$_modify_$_zhengyang: add displayd
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_PACKAGES += \
    displayd
endif

########################################################
# build with drmservice
########################################################
ifeq ($(strip $(BUILD_WITH_DRMSERVICE)),true)
PRODUCT_PACKAGES += drmservice
endif

########################################################
# this product has GPS or not
########################################################
ifeq ($(strip $(BOARD_HAS_GPS)),true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.factory.hasGPS=true
else
PRODUCT_PROPERTY_OVERRIDES += \
    ro.factory.hasGPS=false
endif
########################################################
# this product has Ethernet or not
########################################################
ifneq ($(strip $(BOARD_HS_ETHERNET)),true)
PRODUCT_PROPERTY_OVERRIDES += ro.rk.ethernet_enable=false
endif

#######################################################
#build system support ntfs?
########################################################
ifeq ($(strip $(BOARD_IS_SUPPORT_NTFS)),true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.factory.storage_suppntfs=true

PRODUCT_PACKAGES += \
   ntfs-3g \
   ntfsfix \
   mkntfs
else
PRODUCT_PROPERTY_OVERRIDES += \
    ro.factory.storage_suppntfs=false
endif

########################################################
# build without barrery
########################################################
ifeq ($(strip $(BUILD_WITHOUT_BATTERY)), true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.factory.without_battery=true
else
PRODUCT_PROPERTY_OVERRIDES += \
    ro.factory.without_battery=false
endif
 
PRODUCT_PACKAGES += \
    com.android.future.usb.accessory

#device recovery ui
#PRODUCT_PACKAGES += \
    librecovery_ui_$(TARGET_PRODUCT)

# for bugreport
ifneq ($(TARGET_BUILD_VARIANT), user)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/bugreport.sh:system/bin/bugreport.sh
endif

ifeq ($(strip $(BOARD_BOOT_READAHEAD)), true)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/proprietary/readahead/readahead:root/sbin/readahead \
    $(LOCAL_PATH)/proprietary/readahead/readahead_list.txt:root/readahead_list.txt
endif

#whtest for bin
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/whtest.sh:system/bin/whtest.sh

# for preinstall
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/preinstall_cleanup.sh:system/bin/preinstall_cleanup.sh
    
$(call inherit-product-if-exists, external/wlan_loader/wifi-firmware.mk)

# Copy manifest to system/
ifeq ($(strip $(SYSTEM_WITH_MANIFEST)),true)
PRODUCT_COPY_FILES += \
    manifest.xml:system/manifest.xml
endif

# Copy init.usbstorage.rc to root
#ifeq ($(strip $(BUILD_WITH_MULTI_USB_PARTITIONS)),true)
#PRODUCT_COPY_FILES += \
#    $(LOCAL_PATH)/init.usbstorage.rc:root/init.usbstorage.rc
#endif

ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), ap6xxx_nfc)
#NFC packages
PRODUCT_PACKAGES += \
    nfc_nci.$(TARGET_BOARD_HARDWARE) \
    NfcNci \
    Tag \
    com.android.nfc_extras

# NFCEE access control
ifeq ($(TARGET_BUILD_VARIANT),user)
NFCEE_ACCESS_PATH := $(LOCAL_PATH)/nfc/nfcee_access.xml
else
NFCEE_ACCESS_PATH := $(LOCAL_PATH)/nfc/nfcee_access_debug.xml
endif

copyNfcFirmware = $(subst XXXX,$(strip $(1)),hardware/broadcom/nfc/firmware/XXXX:/system/vendor/firmware/XXXX)
# NFC access control + feature files + configuration
PRODUCT_COPY_FILES += \
    $(NFCEE_ACCESS_PATH):system/etc/nfcee_access.xml \
    frameworks/native/data/etc/com.android.nfc_extras.xml:system/etc/permissions/com.android.nfc_extras.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:system/etc/permissions/android.hardware.nfc.hce.xml \
    $(LOCAL_PATH)/nfc/libnfc-brcm.conf:system/etc/libnfc-brcm.conf \
    $(LOCAL_PATH)/nfc/libnfc-brcm-20791b03.conf:system/etc/libnfc-brcm-20791b03.conf \
    $(LOCAL_PATH)/nfc/libnfc-brcm-20791b04.conf:system/etc/libnfc-brcm-20791b04.conf \
    $(LOCAL_PATH)/nfc/libnfc-brcm-20791b05.conf:system/etc/libnfc-brcm-20791b05.conf \
    $(LOCAL_PATH)/nfc/libnfc-brcm-43341b00.conf:system/etc/libnfc-brcm-43341b00.conf \
    $(call copyNfcFirmware, BCM20791B3_002.004.010.0161.0000_Generic_I2CLite_NCD_Signed_configdata.ncd) \
    $(call copyNfcFirmware, BCM20791B3_002.004.010.0161.0000_Generic_PreI2C_NCD_Signed_configdata.ncd) \
    $(call copyNfcFirmware, BCM20791B5_002.006.013.0011.0000_Generic_I2C_NCD_Unsigned_configdata.ncd) \
    $(call copyNfcFirmware, BCM43341NFCB0_002.001.009.0021.0000_Generic_I2C_NCD_Signed_configdata.ncd) \
    $(call copyNfcFirmware, BCM43341NFCB0_002.001.009.0021.0000_Generic_PreI2C_NCD_Signed_configdata.ncd)
endif

# for realtek bluetooth
PRODUCT_PACKAGES += \
    bluetooth_rtk.default \
    libbt-vendor.so \
    libbt-vendor-aic.so \
    libbt-vendor_uart.so \
    libbt-vendor_usb.so \
    bt_vendor.conf
#include hardware/realtek/rtkbt/rtkbt.mk
$(call inherit-product, hardware/realtek/rtkbt/rtkbt.mk)

include hardware/aic/aicbt/aicbt.mk

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
include device/rockchip/common/samba/rk31_samba.mk
PRODUCT_PROPERTY_OVERRIDES += \
    ro.rk.screenoff_time=2147483647
else 
PRODUCT_PROPERTY_OVERRIDES += \
    ro.rk.screenoff_time=60000
endif

# setup dm-verity configs.
# uncomment the two lines if use verity
#PRODUCT_SYSTEM_VERITY_PARTITION := /dev/block/platform/ff0f0000.rksdmmc/by-name/system
#$(call inherit-product, build/target/product/verity.mk)

ifeq ($(strip $(BUILD_BOX_WITH_GOOGLE_MARKET)), true)
$(call inherit-product-if-exists, vendor/partner_gms/products/gms-mini-box.mk)
$(call inherit-product-if-exists, vendor/widevine/widevine.mk)
endif

ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET)), true)
ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET_ALL)), true)
$(call inherit-product-if-exists, vendor/partner_gms/products/gms.mk)
else
$(call inherit-product-if-exists, vendor/partner_gms/products/gms-mandatory.mk)
endif
endif

ifeq ($(BUILD_WITH_WIDEVINE),true)
$(call inherit-product-if-exists, vendor/widevine/widevine.mk)
endif

#ro.product.first_api_level indicates the first api level, device has been commercially launced on.
PRODUCT_PROPERTY_OVERRIDES += \
    ro.product.first_api_level=23

$(call inherit-product-if-exists, vendor/rockchip/common/device-vendor.mk)

########################################################
# this product has support remotecontrol or not
########################################################
ifeq ($(strip $(BOARD_HAS_REMOTECONTROL)),true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.config.enable.remotecontrol=true
else
PRODUCT_PROPERTY_OVERRIDES += \
    ro.config.enable.remotecontrol=false
endif

ifeq ($(strip $(BUILD_WITH_SKIPVERIFY)),true)
PRODUCT_PROPERTY_OVERRIDES +=               \
    ro.config.enable.skipverify=true
endif

ifneq ($(filter rk%, $(TARGET_BOARD_PLATFORM)), )
PRODUCT_COPY_FILES += \
        device/rockchip/common/performance_info.xml:system/etc/performance_info.xml
endif

# hdmi cec
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.hdmi.cec.xml:system/etc/permissions/android.hardware.hdmi.cec.xml
PRODUCT_PROPERTY_OVERRIDES += ro.hdmi.device_type=4
PRODUCT_PACKAGES += \
	hdmi_cec.$(TARGET_BOARD_HARDWARE)
endif
PRODUCT_PACKAGES += \
	abc
# boot optimization
PRODUCT_COPY_FILES += \
        device/rockchip/common/boot_boost/libboot_optimization.so:system/lib/libboot_optimization.so
ifeq ($(strip $(BOARD_WITH_BOOT_BOOST)),true)
PRODUCT_COPY_FILES += \
        device/rockchip/common/boot_boost/prescan_packages.xml:system/etc/prescan_packages.xml
PRODUCT_PROPERTY_OVERRIDES += \
        ro.boot_boost.enable=true
endif

# mem optimization
ifeq ($(strip $(BOARD_WITH_MEM_OPTIMISE)),true)
PRODUCT_COPY_FILES += \
	device/rockchip/common/lowmem_package_filter.xml:system/etc/lowmem_package_filter.xml 
PRODUCT_PROPERTY_OVERRIDES += \
	ro.mem_optimise.enable=true
endif


# lowmem mode
ifeq ($(strip $(BOARD_USE_LOW_MEM)),true)
PRODUCT_COPY_FILES += \
	device/rockchip/common/lowmem_package_filter.xml:system/etc/lowmem_package_filter.xml
PRODUCT_PROPERTY_OVERRIDES += \
	ro.config.low_ram=true
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
PRODUCT_COPY_FILES += \
       device/rockchip/common/lowmem_package_filter.xml:system/etc/lowmem_package_filter.xml 
endif

# neon transform library by djw
PRODUCT_COPY_FILES += \
	device/rockchip/common/neon_transform/lib/librockchipxxx.so:system/lib/librockchipxxx.so \
	device/rockchip/common/neon_transform/lib64/librockchipxxx.so:system/lib64/librockchipxxx.so

#if force app can see udisk
ifeq ($(strip $(BOARD_FORCE_UDISK_VISIBLE)),true)
PRODUCT_PROPERTY_OVERRIDES += \
	ro.udisk.visible=true
endif

#if box platform force app can see udisk
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.udisk.visible=true
endif

#if disable safe mode to speed up booting time
ifeq ($(strip $(BOARD_DISABLE_SAFE_MODE)),true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.safemode.disabled=true
endif

ifeq ($(strip $(BOARD_ENABLE_3G_DONGLE)),true)
ifeq ($(strip $(PRODUCT_BUILD_MODULE)), px3car)
PRODUCT_PROPERTY_OVERRIDES += \
    persist.enable.3g.dongle=true \
    rild.libpath=/system/lib/libril-rk29-dataonly.so
else
PRODUCT_PROPERTY_OVERRIDES += \
    persist.enable.3g.dongle=true \
    rild.libpath=/system/lib64/libril-rk29-dataonly.so
endif
endif

#boot and shutdown animation, ringing
ifeq ($(strip $(BOOT_SHUTDOWN_ANIMATION_RINGING)),true)
include device/rockchip/common/bootshutdown/bootshutdown.mk
endif

#boot video enable 
ifeq ($(strip $(BOOT_VIDEO_ENABLE)),true)
include device/rockchip/common/bootvideo/bootvideo.mk
endif

ifeq ($(strip $(BOARD_ENABLE_PMS_MULTI_THREAD_SCAN)), true)
PRODUCT_PROPERTY_OVERRIDES += \
	ro.pms.multithreadscan=true		
endif

ifeq ($(strip $(BOARD_USB_ALLOW_DEFAULT_MTP)), true)
PRODUCT_PROPERTY_OVERRIDES += \
	ro.usb.default_mtp=true
endif

#add for hwui property
PRODUCT_PROPERTY_OVERRIDES += \
    ro.hwui.texture_cache_size=72 \
    ro.hwui.layer_cache_size=48 \
    ro.hwui.r_buffer_cache_size=8 \
    ro.hwui.path_cache_size=32 \
    ro.hwui.gradient_cache_size=1 \
    ro.hwui.drop_shadow_cache_size=6 \
    ro.hwui.texture_cache_flushrate=0.4 \
    ro.hwui.text_small_cache_width=1024 \
    ro.hwui.text_small_cache_height=1024 \
    ro.hwui.text_large_cache_width=2048 \
    ro.hwui.text_large_cache_height=1024 \
    ro.hwui.disable_scissor_opt=true \
    ro.rk.screenshot_enable=true   \
    ro.rk.hdmi_enable=true   \
    sys.status.hidebar_enable=false   \
    persist.sys.ui.hw=true
