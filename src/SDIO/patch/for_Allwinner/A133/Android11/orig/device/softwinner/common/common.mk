DEVICE_PACKAGE_OVERLAYS := \
    device/softwinner/common/overlay/base

PRODUCT_COPY_FILES += \
    device/softwinner/common/init.common.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.common.rc \

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.ipsec_tunnels.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.ipsec_tunnels.xml

#media
$(call inherit-product-if-exists, frameworks/av/media/libcedarc/libcdclist.mk)
$(call inherit-product-if-exists, frameworks/av/media/libcedarx/libcdxlist.mk)

# This package has no sense, just for remove pre-defined packages in other makefile.
# How to use: Add all packages which needed to remove to GLOBAL_REMOVED_PACKAGES.
PRODUCT_PACKAGES += PackageOverride

# tools
PRODUCT_PACKAGES += \
    mtop \
    preinstall \
    iperf3

PRODUCT_PACKAGES += misc.img

PRODUCT_PACKAGES += wireless-package

# buildinfo is a host package to speed up `pack` handling.
PRODUCT_PACKAGES += buildinfo

PRODUCT_CHECK_ELF_FILES := true

# Audio
PRODUCT_PACKAGES += \
    audio.a2dp.default \
    audio.usb.default \
    audio.r_submix.default

# f2fs format tool for recovery
PRODUCT_PACKAGES += mkfs.f2fs

USE_XML_AUDIO_POLICY_CONF := 1

PRODUCT_COPY_FILES += \
    frameworks/av/services/audiopolicy/config/a2dp_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/a2dp_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml

# gms express required property
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.base_build=noah

# scense_control
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.p_bootcomplete=true \
    persist.vendor.p_debug=false \
    persist.vendor.p_benchmark=true \
    persist.vendor.p_music=true

# enable charger suspend
PRODUCT_PRODUCT_PROPERTIES += \
    ro.charger.enable_suspend=true

# sf control
PRODUCT_PROPERTY_OVERRIDES += \
    debug.sf.disable_backpressure=1

TARGET_SYSTEM_PROP := $(TARGET_SYSTEM_PROP) $(LOCAL_PATH)/system.prop

PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.strictmode.disable=1

# for debug
PRODUCT_PACKAGES += kmsgd awlogd AwlogSettings


# DroidBoost config
PRODUCT_USE_DROIDBOOST := true

ifeq ($(PRODUCT_USE_DROIDBOOST),true)
# add treadahead
PRODUCT_PACKAGES += treadahead

#redefine preload classes
PRODUCT_COPY_FILES += \
    device/softwinner/common/config/preloaded-classes:system/etc/preloaded-classes

#Preopt SystemUI and Launcher3
PRODUCT_DEXPREOPT_SPEED_APPS += \
    SystemUI \
    Launcher3QuickStepGo
else
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.droidboost.disable=1 \
    persist.sys.without.treadahead=1
endif
# end DroidBoost


PRODUCT_PROPERTY_OVERRIDES += \
    ro.logd.size=524288 \
    ro.logd.size.main=4194304 \
    ro.logd.size.system=1048576 \
    ro.logd.size.crash=4194304 \

ifneq (,$(filter true,$(PRODUCT_DEBUG)))
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.debug.logpersistd=true \
    persist.debug.logcat.enable=true \
    persist.debug.kernel_log.enable=true \
    persist.debug.crashdump.enable=true
else
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.debug.logpersistd=false \
    persist.debug.logcat.enable=true \
    persist.debug.kernel_log.enable=true \
    persist.debug.crashdump.enable=false
endif

#Log for DroidBootstVerison
$(eval $(shell awk '{print $$1}' device/softwinner/common/config/droidboost.version | grep DroidBoostVerison))
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.build.DroidBoost.version=$(DroidBoostVerison)

# OEM Unlock reporting
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.oem_unlock_supported=1

# 64 bit build force cfi check, please ensure this config.
PRODUCT_CFI_INCLUDE_PATHS += \
    hardware/realtek/wlan/wpa_supplicant_8_lib \
    hardware/xradio/wlan/wpa_supplicant_8_lib \
    hardware/sprd/wlan/wpa_supplicant_8_lib \
    hardware/ssv/wlan/wpa_supplicant_8_lib \
    hardware/aw/wireless/wlan/wpa_supplicant_8_lib

# bin: busybox and cpu_monitor
$(call inherit-product-if-exists, vendor/aw/public/tool.mk)

#display service
#$(call inherit-product-if-exists, vendor/aw/public/package/display/display.mk)
$(call inherit-product-if-exists, vendor/aw/public/package/displayoutput/displayoutput.mk)

#display hal
$(call inherit-product-if-exists, hardware/aw/display/config.mk)
