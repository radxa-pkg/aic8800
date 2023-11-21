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
include vendor/rockchip/common/BoardConfigVendor.mk
ifeq ($(strip $(TARGET_ARCH)), arm64)
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
endif

# Prebuild apps
#ifneq ($(strip $(TARGET_PRODUCT)), )
#    TARGET_DEVICE_DIR=$(shell test -d device && find device -maxdepth 4 -path '*/$(TARGET_PRODUCT)/BoardConfig.mk')
#    TARGET_DEVICE_DIR := $(patsubst %/,%,$(dir $(TARGET_DEVICE_DIR)))
#    $(info device-rockchip-common TARGET_DEVICE_DIR: $(TARGET_DEVICE_DIR))
#    $(shell python $(LOCAL_PATH)/auto_generator.py $(TARGET_DEVICE_DIR) preinstall bundled_persist-app)
#    $(shell python $(LOCAL_PATH)/auto_generator.py $(TARGET_DEVICE_DIR) preinstall_del bundled_uninstall_back-app)
#    $(shell python $(LOCAL_PATH)/auto_generator.py $(TARGET_DEVICE_DIR) preinstall_del_forever bundled_uninstall_gone-app)
#    -include $(TARGET_DEVICE_DIR)/preinstall/preinstall.mk
#    -include $(TARGET_DEVICE_DIR)/preinstall_del/preinstall.mk
#    -include $(TARGET_DEVICE_DIR)/preinstall_del_forever/preinstall.mk
#endif

# Inherit product config
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), atv)
  $(call inherit-product, device/google/atv/products/atv_base.mk)
  $(call inherit-product-if-exists, frameworks/base/data/sounds/AllAudio.mk)
  PRODUCT_PACKAGES += DocumentsUI \
                      PlayAutoInstallConfig \
                      ATVContentProvider \

else ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
  $(call inherit-product, device/rockchip/common/tv/tv_base.mk)
else ifeq ($(strip $(BUILD_WITH_GO_OPT))|$(strip $(TARGET_ARCH)) ,true|arm)
  # For arm Go tablet.
  $(call inherit-product, $(SRC_TARGET_DIR)/product/generic_no_telephony.mk)
  $(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
  $(call inherit-product-if-exists, frameworks/base/data/sounds/AudioPackageGo.mk)
  PRODUCT_PACKAGES += Launcher3GoIconRecents
  ROCKCHIP_USE_LAZY_HAL := true
else ifeq ($(strip $(BUILD_WITH_GO_OPT))|$(strip $(TARGET_ARCH)) ,true|arm64)
  # For arm64 Go tablet
  $(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
else
# Normal tablet, add QuickStep for normal product only.
  $(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
  PRODUCT_PACKAGES += Launcher3QuickStep
endif

PRODUCT_AAPT_CONFIG ?= normal large xlarge hdpi xhdpi xxhdpi
PRODUCT_AAPT_PREF_CONFIG ?= xhdpi

PRODUCT_PACKAGES += \
    ExactCalculator

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

# librkskia
PRODUCT_PACKAGES += \
   librkskia

# librtxml librtopus
PRODUCT_PACKAGES += \
    librttinyxml \
    librtopus    \

# omx
PRODUCT_PACKAGES += \
    libomxvpu_enc \
    libomxvpu_dec \
    libRkOMX_Resourcemanager \
    libOMX_Core \

# For screen hwrotation
ifneq ($(filter 90 180 270, $(strip $(SF_PRIMARY_DISPLAY_ORIENTATION))), )
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	ro.surface_flinger.primary_display_orientation=ORIENTATION_$(SF_PRIMARY_DISPLAY_ORIENTATION)
endif

# build with go optimization
ifeq ($(strip $(BUILD_WITH_GO_OPT)),true)
ifeq ($(strip $(TARGET_ARCH)), arm64)
$(call inherit-product, build/target/product/go_defaults_512.mk)
$(call inherit-product, device/rockchip/common/build/rockchip/AndroidGo512.mk)
else
$(call inherit-product, build/target/product/go_defaults.mk)
endif
$(call inherit-product, device/rockchip/common/build/rockchip/AndroidGoCommon.mk)
PRODUCT_COPY_FILES += \
    device/rockchip/common/android.hardware.ram.low.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.ram.low.xml \
    frameworks/native/data/etc/android.software.app_widgets.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.app_widgets.xml
PRODUCT_PROPERTY_OVERRIDES += \
    config.disable_rtt=true \
    config.disable_consumerir=true
endif

ifeq ($(strip $(BOARD_AVB_ENABLE)),true)
$(call inherit-product, $(SRC_TARGET_DIR)/product/gsi_keys.mk)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.verified_boot.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.verified_boot.xml

# Build vbmeta with public_key_metadata
# when BOARD_AVB_METADATA_BIN_PATH is set
ifdef BOARD_AVB_METADATA_BIN_PATH
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS := \
    --public_key_metadata $(BOARD_AVB_METADATA_BIN_PATH)
endif # BOARD_AVB_METADATA_BIN_PATH
endif # BOARD_AVB_ENABLE

ifeq ($(strip $(BOARD_USE_LCDC_COMPOSER)), true)
# setup dalvik vm configs.
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

PRODUCT_PROPERTY_OVERRIDES += \
    ro.rk.screenshot_enable=true   \
    sys.status.hidebar_enable=false \
    persist.sys.ui.hw=true
endif

PRODUCT_COPY_FILES += \
    device/rockchip/common/init.rockchip.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rockchip.rc \
    device/rockchip/common/init.mount_all_early.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.mount_all.rc \
    $(LOCAL_PATH)/init.insmod.cfg:$(TARGET_COPY_OUT_VENDOR)/etc/init.insmod.cfg \
    $(LOCAL_PATH)/init.insmod.sh:$(TARGET_COPY_OUT_VENDOR)/bin/init.insmod.sh \
    device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).rc \
    device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).usb.rc \
    $(call add-to-product-copy-files-if-exists,device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).bootmode.emmc.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).bootmode.emmc.rc) \
    $(call add-to-product-copy-files-if-exists,device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).bootmode.unknown.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).bootmode.unknown.rc) \
    $(call add-to-product-copy-files-if-exists,device/rockchip/common/init.$(TARGET_BOARD_HARDWARE).bootmode.nvme.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).bootmode.nvme.rc) \
    device/rockchip/common/ueventd.rockchip.rc:$(TARGET_COPY_OUT_VENDOR)/ueventd.rc \
    device/rockchip/common/rk29-keypad.kl:system/usr/keylayout/rk29-keypad.kl \
    device/rockchip/common/ff680030_pwm.kl:system/usr/keylayout/ff680030_pwm.kl \
     device/rockchip/common/alarm_filter.xml:system/etc/alarm_filter.xml \
	device/rockchip/common/ff420030_pwm.kl:system/usr/keylayout/ff420030_pwm.kl

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/wpa_config.txt:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_config.txt \
    hardware/broadcom/wlan/bcmdhd/config/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf \
    hardware/broadcom/wlan/bcmdhd/config/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf \
    hardware/broadcom/wlan/bcmdhd/config/wpa_supplicant_bcm.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_bcm.conf

#for ssv6051
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/wifi/ssv6xxx/p2p_supplicant.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_ssv.conf \

PRODUCT_PACKAGES += \
    iperf \
    libiconv \
    libwpa_client \
    hostapd \
    wificond \
    wifilogd \
    wpa_supplicant \
    wpa_cli \
    wpa_supplicant.conf \
    dhcpcd.conf

ifeq ($(ROCKCHIP_USE_LAZY_HAL),true)
PRODUCT_PACKAGES += \
    android.hardware.wifi@1.0-service-lazy
else
PRODUCT_PACKAGES += \
    android.hardware.wifi@1.0-service
endif


ifeq ($(strip $(BOARD_HAS_RK_4G_MODEM)),true)
PRODUCT_PACKAGES += \
    CarrierDefaultApp \
    CarrierConfig \
    rild \
    librk-ril \
    dhcpcd

PRODUCT_PROPERTY_OVERRIDES += \
		ro.boot.noril=false \
		ro.telephony.default_network=9

ifeq ($(strip $(TARGET_ARCH)), arm64)
PRODUCT_PROPERTY_OVERRIDES += \
		vendor.rild.libpath=/vendor/lib64/librk-ril.so

PRODUCT_COPY_FILES += \
		$(LOCAL_PATH)/4g_modem/bin64/dhcpcd:$(TARGET_COPY_OUT_VENDOR)/bin/dhcpcd \
		$(LOCAL_PATH)/4g_modem/lib64/librk-ril.so:$(TARGET_COPY_OUT_VENDOR)/lib64/librk-ril.so
else
PRODUCT_PROPERTY_OVERRIDES += \
		vendor.rild.libpath=/vendor/lib/librk-ril.so

PRODUCT_COPY_FILES += \
		$(LOCAL_PATH)/4g_modem/bin32/dhcpcd:$(TARGET_COPY_OUT_VENDOR)/bin/dhcpcd \
		$(LOCAL_PATH)/4g_modem/lib32/librk-ril.so:$(TARGET_COPY_OUT_VENDOR)/lib/librk-ril.so

endif
endif

ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
    PRODUCT_COPY_FILES += \
      $(LOCAL_PATH)/resolution_white.xml:/system/usr/share/resolution_white.xml \
      $(LOCAL_PATH)/tv/permissions/privapp-permissions-tv-common.xml:system/etc/permissions/privapp-permissions-tv-common.xml
endif

ifeq ($(filter MediaTek_mt7601 MediaTek RealTek Espressif, $(strip $(BOARD_CONNECTIVITY_VENDOR))), )
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.connectivity.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.connectivity.rc
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    $(LOCAL_PATH)/audio_policy_volumes_drc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes_drc.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/a2dp_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/a2dp_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/usb_audio_policy_configuration.xml \
    frameworks/av/media/libeffects/data/audio_effects.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_effects.xml

ifndef PRODUCT_FSTAB_TEMPLATE
$(warning Please add fstab.in with PRODUCT_FSTAB_TEMPLATE in your product.mk)
# To use fstab auto generator, define fstab.in in your product.mk,
# Then include the device/rockchip/common/build/rockchip/RebuildFstab.mk in your AndroidBoard.mk
PRODUCT_COPY_FILES += \
    $(TARGET_DEVICE_DIR)/fstab.rk30board:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.rk30board \
    $(TARGET_DEVICE_DIR)/fstab.rk30board:$(TARGET_COPY_OUT_RAMDISK)/fstab.rk30board
endif # Use PRODUCT_FSTAB_TEMPLATE

# For audio-recoard 
PRODUCT_PACKAGES += \
    libsrec_jni

# For tts test
PRODUCT_PACKAGES += \
    libwebrtc_audio_coding

#audio
$(call inherit-product-if-exists, hardware/rockchip/audio/tinyalsa_hal/codec_config/rk_audio.mk)

ifeq ($(BOARD_NFC_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hce.xml
endif

ifeq ($(BOARD_BLUETOOTH_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml
ifeq ($(BOARD_BLUETOOTH_LE_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml
endif
endif

ifeq ($(BOARD_WIFI_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.passpoint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.passpoint.xml \
    frameworks/native/data/etc/android.software.ipsec_tunnels.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.ipsec_tunnels.xml
endif

ifeq ($(BOARD_HAS_GPS),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml
endif

ifeq ($(BOARD_COMPASS_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.compass.xml
endif

ifeq ($(BOARD_USER_FAKETOUCH),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.faketouch.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.faketouch.xml
endif

ifeq ($(BOARD_GYROSCOPE_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml
endif

ifeq ($(BOARD_PROXIMITY_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml
endif

ifeq ($(BOARD_LIGHT_SENSOR_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml
endif

# opengl aep feature
ifeq ($(BOARD_OPENGL_AEP),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml
endif

# CAMERA
ifeq ($(BOARD_CAMERA_SUPPORT),true)
ifeq ($(BOARD_CAMERA_SUPPORT_EXT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.external.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.external.xml

PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4-external-service
else
PRODUCT_PACKAGES += \
    librkisp_aec \
    librkisp_awb \
    librkisp_af

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml
endif

PRODUCT_PACKAGES += \
    camera.$(TARGET_BOARD_HARDWARE) \
    Camera

# Camera HAL
PRODUCT_PACKAGES += \
    camera.device@1.0-impl \
    camera.device@3.2-impl \
    android.hardware.camera.provider@2.4-impl \
    android.hardware.camera.metadata@3.2

ifeq ($(ROCKCHIP_USE_LAZY_HAL),true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.camera.enableLazyHal=true
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4-service-lazy
else
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4-service
endif

$(call inherit-product-if-exists, hardware/rockchip/camera/Config/rk32xx_camera.mk)
$(call inherit-product-if-exists, hardware/rockchip/camera/Config/user.mk)
$(call inherit-product-if-exists, hardware/rockchip/camera/etc/camera_etc.mk)
endif

# Camera Autofocus
ifeq ($(CAMERA_SUPPORT_AUTOFOCUS),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.autofocus.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.autofocus.xml \

endif

# USB HOST
ifeq ($(BOARD_USB_HOST_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml
endif

# USB ACCESSORY
ifeq ($(BOARD_USB_ACCESSORY_SUPPORT),true)
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
    PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/vr_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/vr_core_hardware.xml
else ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), laptop)
    PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/laptop_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/laptop_core_hardware.xml
else ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), tablet)
ifneq ($(strip $(BUILD_WITH_GO_OPT)),true)
    PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/tablet_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/tablet_core_hardware.xml
endif
# add this prop to skip vr test for cts-on-gsi in vts
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.boot.vr=0
endif

# Live Wallpapers
PRODUCT_PACKAGES += \
    NoiseField \
    PhaseBeam \
    librs_jni \
    libjni_pinyinime

ifneq ($(TARGET_BOARD_PLATFORM_PRODUCT), atv)
# Sensor HAL
PRODUCT_PACKAGES += \
    android.hardware.sensors@1.0-service \
    android.hardware.sensors@1.0-impl \
    sensors.$(TARGET_BOARD_HARDWARE)

endif

# Power HAL
PRODUCT_PACKAGES += \
    android.hardware.power@1.0-service \
    android.hardware.power@1.0-impl \
    power.$(TARGET_BOARD_PLATFORM)

# Camera omx-plugin vpu akmd libion_rockchip_ext
PRODUCT_PACKAGES += \
    libvpu \
    libstagefrighthw \
    libgralloc_priv_omx \
    akmd \
    libion_ext


# Light HAL
ifneq ($(TARGET_BOARD_PLATFORM_PRODUCT), atv)
PRODUCT_PACKAGES += \
    lights.$(TARGET_BOARD_PLATFORM) \
    android.hardware.light@2.0-impl
ifeq ($(ROCKCHIP_USE_LAZY_HAL),true)
PRODUCT_PACKAGES += \
    android.hardware.light@2.0-service-lazy
else
PRODUCT_PACKAGES += \
    android.hardware.light@2.0-service
endif
endif

# Keymaster HAL
PRODUCT_PACKAGES += \
    android.hardware.keymaster@3.0-impl \
    android.hardware.keymaster@3.0-service

# new gatekeeper HAL
# PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0-impl \
    android.hardware.gatekeeper@1.0-service

ifeq ($(strip $(BOARD_SUPER_PARTITION_GROUPS)),rockchip_dynamic_partitions)
# Weaver HAL
PRODUCT_PACKAGES += \
    android.hardware.weaver@1.0-impl \
    android.hardware.weaver@1.0-service

# Fastbootd HAL
# TODO: develop a hal for GMS...
PRODUCT_PACKAGES += \
    android.hardware.fastboot@1.0-impl-rockchip \
    fastbootd
endif # BOARD_USE_DYNAMIC_PARTITIONS

# Dumpstate HAL
PRODUCT_PACKAGES += \
    android.hardware.dumpstate@1.0-service.dragon

# Gralloc HAL
PRODUCT_PACKAGES += \
    gralloc.$(TARGET_BOARD_HARDWARE) \
    android.hardware.graphics.mapper@2.0-impl-2.1 \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service

# HW Composer
PRODUCT_PACKAGES += \
    hwcomposer.$(TARGET_BOARD_HARDWARE) \
    android.hardware.graphics.composer@2.1-impl \
    android.hardware.graphics.composer@2.1-service

# iep
ifneq ($(filter rk3188 rk3190 rk3026 rk3288 rk312x rk3126c rk3128 px3se rk3368 rk3326 rk3328 rk3366 rk3399, $(strip $(TARGET_BOARD_PLATFORM))), )
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
    ro.board.platform=$(strip $(TARGET_BOARD_PLATFORM)) \
    ro.target.product=$(strip $(TARGET_BOARD_PLATFORM_PRODUCT))

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

PRODUCT_PACKAGES += \
    android.hardware.audio@2.0-service \
    android.hardware.audio@5.0-impl \
    android.hardware.audio.effect@5.0-impl \
    android.hardware.soundtrigger@2.0-impl

PRODUCT_PACKAGES += \
    libclearkeycasplugin \
    android.hardware.drm@1.0-impl

ifeq ($(ROCKCHIP_USE_LAZY_HAL),true)
PRODUCT_PACKAGES += \
    android.hardware.cas@1.1-service-lazy \
    android.hardware.drm@1.0-service-lazy \
    android.hardware.drm@1.2-service-lazy.clearkey \
    rockchip.hardware.rockit@1.0-service
else
PRODUCT_PACKAGES += \
    android.hardware.cas@1.1-service \
    android.hardware.drm@1.0-service \
    android.hardware.drm@1.2-service.clearkey \
    rockchip.hardware.rockit@1.0-service
endif
#Health hardware
PRODUCT_PACKAGES += \
    android.hardware.health@2.0-service \
    android.hardware.health@2.0-impl
ifneq ($(filter true yes, $(BUILD_WITH_GOOGLE_MARKET) $(PRODUCT_USE_PREBUILT_GTVS)),)
  ifeq ($(strip $(TARGET_ARCH)), arm64)
    ifneq ($(strip $(BUILD_WITH_GO_OPT)), true)
      # for swiftshader, vulkan v1.1 test.
      PRODUCT_PACKAGES += \
        vulkan.rk30board

      PRODUCT_PROPERTY_OVERRIDES += \
        ro.cpuvulkan.version=4198400

      PRODUCT_COPY_FILES += \
        frameworks/native/data/etc/android.hardware.vulkan.version-1_1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version-1_1.xml \
        frameworks/native/data/etc/android.hardware.vulkan.level-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level-0.xml \
        frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.compute-0.xml
    endif
  endif
endif

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

PRODUCT_PROPERTY_OVERRIDES += \
    media.stagefright.thumbnail.prefer_hw_codecs=true

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
    $(LOCAL_PATH)/init.rockchip.hasUMS.true.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).environment.rc
else
ifeq ($(strip $(BUILD_WITH_CDROM)),true)
PRODUCT_PROPERTY_OVERRIDES +=                 \
    ro.factory.hasUMS=cdrom                   \
    ro.factory.cdrom=$(BUILD_WITH_CDROM_PATH) \
    persist.sys.usb.config=mass_storage,adb 

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rockchip.hasCDROM.true.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).environment.rc
else
PRODUCT_PROPERTY_OVERRIDES +=       \
    ro.factory.hasUMS=false         \
    persist.sys.usb.config=adb  \
    testing.mediascanner.skiplist = /mnt/shell/emulated/Android/

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rockchip.hasUMS.false.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.$(TARGET_BOARD_HARDWARE).environment.rc
endif
endif


########################################################
# build with drmservice
########################################################
ifeq ($(strip $(BUILD_WITH_DRMSERVICE)),true)
PRODUCT_PACKAGES += rockchip.drmservice
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

ifeq ($(strip $(BOARD_BOOT_READAHEAD)), true)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/proprietary/readahead/readahead:$(TARGET_COPY_OUT_VENDOR)/sbin/readahead \
    $(LOCAL_PATH)/proprietary/readahead/readahead_list.txt:$(TARGET_COPY_OUT_VENDOR)/readahead_list.txt
endif

# Copy manifest to vendor/
ifeq ($(strip $(BOARD_RECORD_COMMIT_ID)),true)
PRODUCT_COPY_FILES += \
    $(OUT_DIR)/commit_id.xml:$(TARGET_COPY_OUT_VENDOR)/commit_id.xml
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
    frameworks/native/data/etc/com.android.nfc_extras.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.android.nfc_extras.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hce.xml \
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

# Bluetooth HAL
PRODUCT_PACKAGES += \
    libbt-vendor \
    android.hardware.bluetooth@1.0-impl \
    android.hardware.bluetooth@1.0-service \
    android.hardware.bluetooth@1.0-service.rc

ifeq ($(strip $(BOARD_HAVE_BLUETOOTH_RTK)), true)
include hardware/realtek/rtkbt/rtkbt.mk
endif


ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
    include device/rockchip/common/samba/rk31_samba.mk
    PRODUCT_COPY_FILES += \
      $(LOCAL_PATH)/init.box.samba.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.box.samba.rc \
      device/rockchip/common/cifsmanager.sh:system/bin/cifsmanager.sh

    PRODUCT_PROPERTY_OVERRIDES += \
      ro.rk.screenoff_time=2147483647
else
PRODUCT_PROPERTY_OVERRIDES += \
    ro.rk.screenoff_time=60000
endif

# Flash Lock Status reporting,
# GTS: com.google.android.gts.persistentdata.
# PersistentDataHostTest#testTestGetFlashLockState
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.oem_unlock_supported=1

# Add for function frp
ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET)), true)
  ifneq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
  # Add for Mainline GMS Modules.
  $(call inherit-product-if-exists, vendor/partner_modules/rockchip/modules-mandatory.mk)
  endif
  ifeq ($(strip $(BUILD_WITH_GOOGLE_FRP)), true)
    PRODUCT_PROPERTY_OVERRIDES += \
      ro.frp.pst=/dev/block/by-name/frp
  endif
  ifeq ($(strip $(TARGET_BUILD_VARIANT)), user)
    # Enforce privapp-permissions whitelist only for user build.
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.control_privapp_permissions=enforce
    ifneq ($(strip $(BUILD_WITH_GOOGLE_GMS_EXPRESS)),true)
      $(warning ****************************************)
      $(error You are building GMS with a user variant without GMS-Express!)
      $(warning Please note that all your apps MUST be able to get permissions, Otherwise android cannot boot!)
      $(warning After confirming your apps, please remove the above error line!)
      $(warning ****************************************)
    endif
  endif
endif

ifeq ($(strip $(PRODUCT_USE_PREBUILT_GTVS)), yes)
  $(call inherit-product-if-exists, vendor/google_gtvs/gms.mk.sample)
  $(call inherit-product-if-exists, vendor/widevine/widevine.mk)
endif

ifeq ($(strip $(BUILD_WITH_GO_OPT)),true)
  ifeq ($(strip $(BUILD_WITH_EEA)),true)
    $(call inherit-product-if-exists, vendor/partner_gms/products/gms_go_eea_$(BUILD_WITH_EEA_TYPE).mk)
  else
    ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET)), true)
      ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET_ALL)), true)
        $(call inherit-product-if-exists, vendor/partner_gms/products/gms_go.mk)
      else
        $(call inherit-product-if-exists, vendor/partner_gms/products/gms_go-mandatory.mk)
      endif
    endif
  endif
else
  ifeq ($(strip $(BUILD_WITH_EEA)), true)
    $(call inherit-product-if-exists, vendor/partner_gms/products/gms_eea_$(BUILD_WITH_EEA_TYPE).mk)
  else
    ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET)), true)
      ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT), box)
        $(call inherit-product-if-exists, vendor/partner_gms/products/gms-mini-box.mk)
      else
        ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET_ALL)), true)
          $(call inherit-product-if-exists, vendor/partner_gms/products/gms.mk)
        else
          $(call inherit-product-if-exists, vendor/partner_gms/products/gms-mandatory.mk)
        endif
      endif
    endif
  endif
endif

ifneq ($(strip $(BOARD_WIDEVINE_OEMCRYPTO_LEVEL)), )
$(call inherit-product-if-exists, vendor/widevine/widevine.mk)
endif

ifeq ($(strip $(BUILD_WITH_MICROSOFT_PLAYREADY)), true)
$(call inherit-product-if-exists, vendor/microsoft/playready.mk)
endif

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

# rktoolbox
ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
ifeq ($(strip $(BOARD_WITH_RKTOOLBOX)),true)
$(call inherit-product-if-exists, external/rktoolbox/rktoolbox.mk)
endif
endif

# hdmi cec
ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.hdmi.cec.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.hdmi.cec.xml
PRODUCT_PROPERTY_OVERRIDES += ro.hdmi.device_type=4
PRODUCT_PACKAGES += \
	hdmi_cec.$(TARGET_BOARD_PLATFORM)

# HDMI CEC HAL
PRODUCT_PACKAGES += \
    android.hardware.tv.cec@1.0-impl \
    android.hardware.tv.cec@1.0-service

# Hw Output HAL
PRODUCT_PACKAGES += \
    rockchip.hardware.outputmanager@1.0-impl \
    rockchip.hardware.outputmanager@1.0-service

PRODUCT_PACKAGES += hw_output.default
endif

# mid used hdmi
ifeq ($(strip $(BOARD_SHOW_HDMI_SETTING)), true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.rk.hdmisetting=true

PRODUCT_COPY_FILES += \
      $(LOCAL_PATH)/resolution_white.xml:/system/usr/share/resolution_white.xml

PRODUCT_PACKAGES += \
    rockchip.hardware.outputmanager@1.0-impl \
    rockchip.hardware.outputmanager@1.0-service

PRODUCT_PACKAGES += hw_output.default
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

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
PRODUCT_COPY_FILES += \
       device/rockchip/common/lowmem_package_filter.xml:system/etc/lowmem_package_filter.xml 
endif

# neon transform library by djw
PRODUCT_COPY_FILES += \
	device/rockchip/common/neon_transform/lib/librockchipxxx.so:system/lib/librockchipxxx.so \
	device/rockchip/common/neon_transform/lib64/librockchipxxx.so:system/lib64/librockchipxxx.so

# support eecolor hdr api
PRODUCT_COPY_FILES += \
        device/rockchip/common/eecolorapi/lib/libeecolorapi.so:system/lib/libeecolorapi.so \
        device/rockchip/common/eecolorapi/lib64/libeecolorapi.so:system/lib64/libeecolorapi.so

#if force app can see udisk
ifeq ($(strip $(BOARD_FORCE_UDISK_VISIBLE)),true)
PRODUCT_PROPERTY_OVERRIDES += \
	ro.vendor.udisk.visible=true
endif

#if disable safe mode to speed up booting time
ifeq ($(strip $(BOARD_DISABLE_SAFE_MODE)),true)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.safemode.disabled=true
endif

#boot and shutdown animation, ringing
ifeq ($(strip $(BOOT_SHUTDOWN_ANIMATION_RINGING)),true)
include device/rockchip/common/bootshutdown/bootshutdown.mk
endif

# For oem preset
ifeq ($(strip $(OEM_PRESET)),true)
include device/rockchip/common/oem_preset/oem_preset.mk
endif

#boot video enable 
ifeq ($(strip $(BOOT_VIDEO_ENABLE)),true)
include device/rockchip/common/bootvideo/bootvideo.mk
endif

ifeq ($(strip $(BOARD_ENABLE_PMS_MULTI_THREAD_SCAN)), true)
PRODUCT_PROPERTY_OVERRIDES += \
	ro.pms.multithreadscan=true		
endif

#add for hwui property
PRODUCT_PROPERTY_OVERRIDES += \
    ro.rk.screenshot_enable=true   \
    ro.rk.hdmi_enable=true   \
    sys.status.hidebar_enable=false

PRODUCT_FULL_TREBLE_OVERRIDE := true
#PRODUCT_COMPATIBILITY_MATRIX_LEVEL_OVERRIDE := 27

# Add runtime resource overlay for framework-res
# TODO disable for box
ifeq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
PRODUCT_ENFORCE_RRO_TARGETS := \
    framework-res
endif

#The module which belong to vndk-sp is defined by google
PRODUCT_PACKAGES += \
    android.hardware.renderscript@1.0.vndk-sp\
    android.hardware.graphics.allocator@2.0.vndk-sp\
    android.hardware.graphics.mapper@2.0.vndk-sp\
    android.hardware.graphics.common@1.0.vndk-sp\
    libhwbinder.vndk-sp\
    libbase.vndk-sp\
    libcutils.vndk-sp\
    libhardware.vndk-sp\
    libhidlbase.vndk-sp\
    libhidltransport.vndk-sp\
    libutils.vndk-sp\
    libc++.vndk-sp\
    libRS_internal.vndk-sp\
    libRSDriver.vndk-sp\
    libRSCpuRef.vndk-sp\
    libbcinfo.vndk-sp\
    libblas.vndk-sp\
    libft2.vndk-sp\
    libpng.vndk-sp\
    libcompiler_rt.vndk-sp\
    libbacktrace.vndk-sp\
    libunwind.vndk-sp\
    liblzma.vndk-sp\

#######for target product ########
ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT),box)
  DEVICE_PACKAGE_OVERLAYS += device/rockchip/common/overlay_screenoff
  PRODUCT_PROPERTY_OVERRIDES += \
       ro.target.product=box \
       media.stagefright.extractremote=false

else ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT),atv)
  PRODUCT_PROPERTY_OVERRIDES += \
       ro.target.product=atv \
       ro.com.google.clientidbase=android-rockchip-tv
  PRODUCT_COPY_FILES += \
       $(LOCAL_PATH)/bootanimation.zip:/system/media/bootanimation.zip

else ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT),vr)
  PRODUCT_PROPERTY_OVERRIDES += \
        ro.target.product=vr
else ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT),laptop)
  PRODUCT_PROPERTY_OVERRIDES += \
        ro.target.product=laptop
else # tablet
  PRODUCT_PROPERTY_OVERRIDES += \
        ro.target.product=tablet

  PRODUCT_PACKAGES += \
        SoundRecorder
ifneq ($(strip $(BUILD_WITH_GOOGLE_GMS_EXPRESS)),true)
PRODUCT_PACKAGES += \
    Music \
    WallpaperPicker
endif # tablet without GMS-Express
endif

# By default, enable zram; experiment can toggle the flag,
# which takes effect on boot
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.zram_enabled=1

### fix adb-device cannot be identified  ###
### in AOSP-system image (user firmware) ###
ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.logd.kernel=1
PRODUCT_COPY_FILES += \
    device/rockchip/common/zmodem/rz:$(TARGET_COPY_OUT_VENDOR)/bin/rz \
    device/rockchip/common/zmodem/sz:$(TARGET_COPY_OUT_VENDOR)/bin/sz \
    device/rockchip/common/picocom/bin/picocom:$(TARGET_COPY_OUT_VENDOR)/bin/picocom
PRODUCT_PACKAGES += io
endif

USE_XML_AUDIO_POLICY_CONF := 1

ifeq ($(strip $(BOARD_USE_DRM)),true)
PRODUCT_PACKAGES += \
    modetest
endif

ifeq ($(strip $(BOARD_USB_ALLOW_DEFAULT_MTP)), true)
PRODUCT_PROPERTY_OVERRIDES += \
       ro.usb.default_mtp=true
endif

#GOOGLE EXPRESS PLUS CONFIGURATION
ifeq ($(strip $(BUILD_WITH_GOOGLE_GMS_EXPRESS)),true)
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/gms-express.xml:system/etc/sysconfig/gms-express.xml

# Imporve the tracking of GMS Express base build.
PRODUCT_PROPERTY_OVERRIDES += \
    ro.base_build=noah
endif

PRODUCT_PACKAGES += libstdc++.vendor

#only box and atv using our audio policy(write by rockchip)
ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
USE_CUSTOM_AUDIO_POLICY := 1
endif

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
PRODUCT_PACKAGES += \
    update_engine \
    update_verifier	\
    cppreopts.sh

PRODUCT_STATIC_BOOT_CONTROL_HAL := \
    bootctrl.rk30board	\
    libavbuser	\
    libfstab	\
    libcutils

PRODUCT_PACKAGES += \
    update_engine_sideload \
    sg_write_buffer \
    f2fs_io \
    check_f2fs

PRODUCT_PACKAGES += \
    update_engine_client

AB_OTA_PARTITIONS += \
    boot \
    system	\
    uboot	\
    trust	\
    vendor	\
    odm	\
    dtbo
ifeq ($(strip $(BOARD_AVB_ENABLE)),true)
AB_OTA_PARTITIONS += \
    vbmeta
endif
ifndef BOARD_USES_AB_LEGACY_RETROFIT
AB_OTA_PARTITIONS += \
    product
endif

# Boot control HAL
PRODUCT_PACKAGES += \
    android.hardware.boot@1.0-impl \
    android.hardware.boot@1.0-impl.recovery \
    android.hardware.boot@1.0-service \

PRODUCT_PACKAGES += \
  bootctrl.rk30board \
  bootctrl.rk30board.recovery

PRODUCT_PACKAGES_DEBUG += \
    bootctl

# A/B OTA dexopt package
PRODUCT_PACKAGES += otapreopt_script

# A/B OTA dexopt update_engine hookup
AB_OTA_POSTINSTALL_CONFIG += \
    RUN_POSTINSTALL_system=true \
    POSTINSTALL_PATH_system=system/bin/otapreopt_script \
    FILESYSTEM_TYPE_system=ext4 \
    POSTINSTALL_OPTIONAL_system=true

endif

#TWRP
BOARD_TWRP_ENABLE ?= false

#Build with UiMode Config
PRODUCT_COPY_FILES += \
    device/rockchip/common/uimode/package_uimode_config.xml:vendor/etc/package_uimode_config.xml

# Zoom out recovery ui of box by two percent.
ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
    TARGET_RECOVERY_OVERSCAN_PERCENT := 2
    TARGET_BASE_PARAMETER_IMAGE ?= device/rockchip/common/baseparameter/baseparameter_fb1080.img
    # savBaseParameter tool
    ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
        PRODUCT_PACKAGES += saveBaseParameter
    endif
    DEVICE_FRAMEWORK_MANIFEST_FILE := device/rockchip/common/manifest_framework_override.xml
endif

# add AudioSetting
ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
PRODUCT_PACKAGES += \
    rockchip.hardware.rkaudiosetting@1.0-service \
    rockchip.hardware.rkaudiosetting@1.0-impl \
    rockchip.hardware.rkaudiosetting@1.0

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/rt_audio_config.xml:/vendor/etc/rt_audio_config.xml
endif

#Build with Flash IMG
BOARD_FLASH_IMG_ENABLE ?= false
ifeq ($(TARGET_BOARD_PLATFORM_PRODUCT),box)
    BOARD_FLASH_IMG_ENABLE := true
endif
#FLASH_IMG
ifeq ($(strip $(BOARD_FLASH_IMG_ENABLE)), true)
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
        ro.flash_img.enable = true
else
    PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
        ro.flash_img.enable = false
endif
PRODUCT_COPY_FILES += \
    device/rockchip/common/flash_img/flash_img.sh:system/bin/flash_img.sh

# Vendor seccomp policy files for media components:
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/seccomp_policy/mediacodec.policy:$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy/mediacodec.policy
