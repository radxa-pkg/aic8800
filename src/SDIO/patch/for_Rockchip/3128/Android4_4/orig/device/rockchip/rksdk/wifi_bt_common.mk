
# Wifi related defines
ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), MediaTek)
FORCE_WIFI_WORK_AS_ANDROID4_2 := false
BUILD_MEDIATEK_RFTEST_TOOL := false
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), combo_mt66xx)
BOARD_MEDIATEK_USES_GPS := true
combo_config := hardware/mediatek/config/$(strip $(BOARD_CONNECTIVITY_MODULE))/board_config.mk
include $(combo_config)
endif
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), mt5931_6622)
combo_config := hardware/mediatek/config/$(strip $(BOARD_CONNECTIVITY_MODULE))/board_config.mk
include $(combo_config)
endif
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), Broadcom)
FORCE_WIFI_WORK_AS_ANDROID4_2 := false
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_bcmdhd
BOARD_WLAN_DEVICE           := bcmdhd
WIFI_DRIVER_FW_PATH_PARAM   := "/sys/module/bcmdhd/parameters/firmware_path"
WIFI_DRIVER_FW_PATH_STA     := "/system/etc/firmware/fw_bcm4329.bin"
WIFI_DRIVER_FW_PATH_P2P     := "/system/etc/firmware/fw_bcm4329_p2p.bin"
WIFI_DRIVER_FW_PATH_AP      := "/system/etc/firmware/fw_bcm4329_apsta.bin"
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), RealTek)
FORCE_WIFI_WORK_AS_ANDROID4_2 := false
BOARD_WIFI_VENDOR := realtek
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_rtl
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_rtl
BOARD_WLAN_DEVICE           := realtek
WIFI_DRIVER_FW_PATH_STA     := ""
WIFI_DRIVER_FW_PATH_P2P     := ""
WIFI_DRIVER_FW_PATH_AP      := ""
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), Espressif)
FORCE_WIFI_WORK_AS_ANDROID4_2 := false
BOARD_WIFI_VENDOR := Espressif
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_esp
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_esp
BOARD_WLAN_DEVICE           := esp8089
WIFI_DRIVER_FW_PATH_STA     := ""
WIFI_DRIVER_FW_PATH_P2P     := ""
WIFI_DRIVER_FW_PATH_AP      := ""
endif

# bluetooth support
BLUETOOTH_USE_BPLUS := false
ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), Broadcom)
BLUETOOTH_USE_BPLUS := false
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/rockchip/$(TARGET_PRODUCT)/bluetooth
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), ap6xxx_gps)
BLUETOOTH_USE_BPLUS := true
BLUETOOTH_ENABLE_FM := false
endif
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), RealTek)
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), rtl8723as)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/rockchip/$(TARGET_PRODUCT)/bluetooth
BLUETOOTH_HCI_USE_RTK_H5 := true
endif
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), rtl8723bs)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BLUETOOTH_HCI_USE_RTK_H5 := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/common/bluetooth/libbt_rtk8723bs/
endif
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), rtl8723au)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/rockchip/$(TARGET_PRODUCT)/bluetooth
endif
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), rtl8723bu)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
#BLUETOOTH_HCI_USE_RTK_H5 := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/rockchip/$(TARGET_PRODUCT)/bluetooth
endif
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), MediaTek)
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), combo_mt66xx)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := hardware/mediatek/bt/combo_mt66xx/
endif
ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), mt5931_6622)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := hardware/mediatek/bt/mt5931_6622/
endif
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), esp8089_bk3515)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/rockchip/$(TARGET_PRODUCT)/bluetooth
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), mt6622)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := hardware/mediatek/bt/mt5931_6622/
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), rda587x)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := false
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/rockchip/$(TARGET_PRODUCT)/bluetooth
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), MediaTek_mt7601)
mt7601_config := hardware/mediatek/config/mt7601/board_config.mk
include $(mt7601_config)
BOARD_HAVE_BLUETOOTH := false
BOARD_HAVE_BLUETOOTH_BCM := false
FORCE_WIFI_WORK_AS_ANDROID4_2 := false
BUILD_MEDIATEK_RFTEST_TOOL := false
endif

# bluetooth end
