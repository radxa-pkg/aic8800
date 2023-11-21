# Note: Only call this makefile under product path in BoardConfig.mk
# after Wi-Fi/Bluetooth configuration
#
# Need to add in BoardConfig.mk
#
# Wi-Fi:
# BOARD_WIFI_VENDOR := <wifi chip vendor>
# BOARD_USR_WIFI    := <wifi chip name>
# WIFI_DRIVER_MODULE_PATH := <full driver module path with mode name>
# WIFI_DRIVER_MODULE_NAME := <driver modle name in lsmod>
# WIFI_DRIVER_MODULE_ARG  := <args when insmod, optional>
#
# Bluetooth:
# BOARD_BLUETOOTH_VENDOR    := <bluetooth chip vendor>
# BOARD_HAVE_BLUETOOTH_NAME := <bluetooth chip name, optional>
#
# Main work:
# 1. Set BOARD_WIRELESS_FILES to generate copy-package
# 2. Set BOARD_WIRELESS_PACKAGES to requested for wireless-package
# 3. Set BOARD_WIRELESS_PROPERTIES to generate property-package
# 4. Set BOARD_WIRELESS_SYSTEM_PROPERTIES to generate property-system-package

BOARD_WIRELESS_FILES       :=
BOARD_WIRELESS_PACKAGES    :=
BOARD_WIRELESS_PROPERTIES  :=
BOARD_WIRELESS_SYSTEM_PROPERTIES :=

WIRELESS_CONFIG_PATH       := device/softwinner/common/config/wireless

SUPPORTED_WIFI_VENDOR      := broadcom realtek xradio sprd ssv aic common
SUPPORTED_BLUETOOTH_VENDOR := broadcom realtek xradio sprd aic common

SAVED_PRODUCT_COPY_FILES   := $(PRODUCT_COPY_FILES)
PRODUCT_COPY_FILES         :=

BOARD_WIRELESS_FILES += \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml

ifneq (,$(findstring $(BOARD_WIFI_VENDOR),$(SUPPORTED_WIFI_VENDOR)))
    BOARD_WIRELESS_PACKAGES += \
        regulatory-package \
        android.hardware.wifi@1.0-service-lazy \
        libwpa_client \
        wpa_supplicant \
        hostapd \
        wificond \
        wifilogd \
	wpa_cli  \
        wpa_supplicant.conf

    BOARD_WIRELESS_FILES += \
        frameworks/native/data/etc/android.hardware.wifi.passpoint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.passpoint.xml

    BOARD_WIRELESS_PROPERTIES += wifi.interface=wlan0
    DEVICE_MANIFEST_FILE += $(WIRELESS_CONFIG_PATH)/manifest/manifest_wifi.xml

    WPA_SUPPLICANT_VERSION      := VER_0_8_X
    BOARD_WPA_SUPPLICANT_DRIVER := NL80211
    BOARD_HOSTAPD_DRIVER        := NL80211

    ifneq ($(BOARD_WIFI_VENDOR),common)
        BOARD_WIRELESS_PROPERTIES += persist.vendor.wlan_vendor=$(BOARD_WIFI_VENDOR)
        BOARD_WIRELESS_FILES += $(WIRELESS_CONFIG_PATH)/initrc/init.wireless.wlan.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.wireless.wlan.rc
    endif

    ifeq ($(BOARD_WIFI_VENDOR),broadcom)
        BOARD_WLAN_DEVICE           := bcmdhd
        BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
        BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_bcmdhd
        BOARD_WIRELESS_PROPERTIES   += wifi.direct.interface=p2p-dev-wlan0
        -include hardware/aw/wireless/partner/ampak/firmware/device-wlan.mk
    else ifeq ($(BOARD_WIFI_VENDOR),realtek)
        BOARD_WLAN_DEVICE           := rtl
        BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_rtl
        BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_rtl
        BOARD_WIRELESS_FILES        += $(call find-copy-subdir-files,"wifi_efuse_*.map",$(TARGET_DEVICE_DIR)/configs,$(TARGET_COPY_OUT_VENDOR)/etc/wifi)
        BOARD_WIRELESS_PROPERTIES   += wifi.direct.interface=p2p0
        -include hardware/realtek/wlan/config/config.mk
    else ifeq ($(BOARD_WIFI_VENDOR),xradio)
        BOARD_WLAN_DEVICE           := xradio
        BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_xr
        BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_xr
        BOARD_WIRELESS_PROPERTIES   += wifi.direct.interface=p2p-dev-wlan0
        -include hardware/xradio/wlan/kernel-firmware/xradio-wlan.mk
    else ifeq ($(BOARD_WIFI_VENDOR),sprd)
        BOARD_WLAN_DEVICE           := sprd
        BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_sprd
        BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_sprd
        BOARD_WIRELESS_PROPERTIES   += wifi.direct.interface=p2p-dev-wlan0
        -include hardware/sprd/wlan/firmware/$(BOARD_USR_WIFI)/device-sprd.mk
    else ifeq ($(BOARD_WIFI_VENDOR),ssv)
        BOARD_WLAN_DEVICE           := ssv
        BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_ssv
        BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_ssv
        BOARD_WIRELESS_PROPERTIES   += wifi.direct.interface=p2p0
        -include hardware/ssv/wlan/firmware/$(BOARD_USR_WIFI)/device-ssv.mk
    else ifeq ($(BOARD_WIFI_VENDOR),aic) 
        BOARD_WLAN_DEVICE := aic 
        BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_aic 
        BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_aic 
        BOARD_WIRELESS_PROPERTIES += wifi.direct.interface=p2p-dev-wlan0
        -include hardware/aic/wlan/firmware/$(BOARD_USR_WIFI)/device-aic.mk
    else ifeq ($(BOARD_WIFI_VENDOR),common)
        BOARD_WLAN_DEVICE           := common
        BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_common
        BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_common
        -include hardware/aw/wireless/wlan/firmware/firmware.mk
    endif
endif

ifneq (,$(findstring $(BOARD_BLUETOOTH_VENDOR),$(SUPPORTED_BLUETOOTH_VENDOR)))
    BOARD_WIRELESS_PACKAGES += \
        android.hardware.bluetooth@1.0-impl \
        android.hardware.bluetooth@1.0-service \
        android.hardware.bluetooth@1.0-service.rc \
        android.hidl.memory@1.0-impl \
        Bluetooth \
        libbt-vendor \
        audio.a2dp.default

    BOARD_WIRELESS_FILES += \
        frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
        frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml

    BOARD_BLUETOOTH_TTY  ?= /dev/ttyS1
    BOARD_WIRELESS_SYSTEM_PROPERTIES += bluetooth.enable_timeout_ms=8000
    BOARD_WIRELESS_PROPERTIES += persist.vendor.bluetooth_port=$(BOARD_BLUETOOTH_TTY)
    BOARD_WIRELESS_PROPERTIES += ro.bt.bdaddr_path=/sys/class/addr_mgt/addr_bt

    DEVICE_MANIFEST_FILE += $(WIRELESS_CONFIG_PATH)/manifest/manifest_bluetooth.xml

    BOARD_HAVE_BLUETOOTH := true
    BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(TARGET_DEVICE_DIR)/configs/bluetooth/

    ifneq ($(BOARD_BLUETOOTH_VENDOR),common)
        BOARD_WIRELESS_PROPERTIES += persist.vendor.bluetooth_vendor=$(BOARD_BLUETOOTH_VENDOR)
        BOARD_WIRELESS_FILES      += $(WIRELESS_CONFIG_PATH)/initrc/init.wireless.bluetooth.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.wireless.bluetooth.rc
    endif

    ifeq ($(BOARD_BLUETOOTH_VENDOR),broadcom)
        BOARD_HAVE_BLUETOOTH_BCM := true
        BOARD_CUSTOM_BT_CONFIG   := $(TARGET_DEVICE_DIR)/configs/bluetooth/vnd_$(TARGET_DEVICE).txt
        BOARD_WIRELESS_FILES     += $(TARGET_DEVICE_DIR)/configs/bluetooth/bt_vendor.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/bt_vendor.conf
        -include hardware/aw/wireless/partner/ampak/firmware/device-bt.mk
    else ifeq ($(BOARD_BLUETOOTH_VENDOR),realtek)
        BOARD_HAVE_BLUETOOTH_RTK := true
        PRODUCT_COPY_FILES += $(TARGET_DEVICE_DIR)/configs/bluetooth/rtkbt.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/rtkbt.conf
        -include hardware/realtek/bluetooth/firmware/rtlbtfw_cfg.mk
        -include hardware/realtek/bluetooth/rtlbtmp/rtlbtmp.mk
    else ifeq ($(BOARD_BLUETOOTH_VENDOR),xradio)
        BOARD_HAVE_BLUETOOTH_XRADIO := true
        BOARD_CUSTOM_BT_CONFIG      := $(TARGET_DEVICE_DIR)/configs/bluetooth/vnd_$(TARGET_DEVICE).txt
        BOARD_WIRELESS_FILES        += $(TARGET_DEVICE_DIR)/configs/bluetooth/bt_vendor.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/bt_vendor.conf
        -include hardware/xradio/bt/firmware/xradio-bt.mk
    else ifeq ($(BOARD_BLUETOOTH_VENDOR),sprd)
        BOARD_HAVE_BLUETOOTH_SPRD := true
        BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(TOP_DIR)hardware/sprd/libbt/conf/sprd/marlin3/include
        -include hardware/sprd/libbt/conf/sprd/runtime/bt_copy_file.mk
    else ifeq ($(BOARD_BLUETOOTH_VENDOR),aic) 
        BOARD_HAVE_BLUETOOTH_AIC := true 
        BOARD_CUSTOM_BT_CONFIG := $(BOARD_CUSTOM_BT_CONFIG_TMP) 
#        -include hardware/aic/libbt/firmware/$(BOARD_HAVE_BLUETOOTH_NAME)/aic-bt.mk
    else ifeq ($(BOARD_BLUETOOTH_VENDOR),common)
        BOARD_HAVE_BLUETOOTH_COMMON := true
        BOARD_CUSTOM_BT_CONFIG  := $(TARGET_DEVICE_DIR)/configs/bluetooth/vnd_$(TARGET_DEVICE).txt
        BOARD_WIRELESS_FILES    += $(TARGET_DEVICE_DIR)/configs/bluetooth/bt_vendor.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/bt_vendor.conf
        BOARD_WIRELESS_FILES    += $(TARGET_DEVICE_DIR)/configs/bluetooth/rtkbt.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/rtkbt.conf
        BOARD_WIRELESS_PACKAGES += libbt-xradio libbt-broadcom libbt-realtek libbt-sprd libbt-aic wireless_hwinfo
        -include hardware/aw/wireless/bluetooth/firmware/firmware.mk
    endif
    $(call soong_config_add,vendor,board_bluetooth_vendor,$(BOARD_BLUETOOTH_VENDOR))
else
    GLOBAL_REMOVED_PACKAGES += Bluetooth
endif

BOARD_WIRELESS_FILES += $(PRODUCT_COPY_FILES)
PRODUCT_COPY_FILES   := $(SAVED_PRODUCT_COPY_FILES)
