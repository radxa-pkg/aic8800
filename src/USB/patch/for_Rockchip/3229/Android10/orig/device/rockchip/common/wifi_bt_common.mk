
ifneq ($(strip $(TARGET_BOARD_PLATFORM)), sofia3gr)
BOARD_CONNECTIVITY_VENDOR := Broadcom
BOARD_CONNECTIVITY_MODULE := ap6xxx
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), Broadcom)
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_bcmdhd
BOARD_WLAN_DEVICE           := bcmdhd
WIFI_DRIVER_FW_PATH_PARAM   := "/sys/module/bcmdhd/parameters/firmware_path"
WIFI_DRIVER_FW_PATH_STA     := "/vendor/etc/firmware/fw_bcm4329.bin"
WIFI_DRIVER_FW_PATH_P2P     := "/vendor/etc/firmware/fw_bcm4329_p2p.bin"
WIFI_DRIVER_FW_PATH_AP      := "/vendor/etc/firmware/fw_bcm4329_apsta.bin"
endif

# bluetooth support
ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), Broadcom)
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR ?= device/rockchip/$(TARGET_BOARD_PLATFORM)/bluetooth

ifeq ($(strip $(PRODUCT_BUILD_MODULE)), px5car)
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR ?= device/rockchip/px5/bluetooth
endif

ifeq ($(strip $(PRODUCT_BUILD_MODULE)), px3car)
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR ?= device/rockchip/px3/bluetooth
endif

ifeq ($(strip $(BOARD_CONNECTIVITY_MODULE)), ap6xxx_gps)
BLUETOOTH_USE_BPLUS := true
BLUETOOTH_ENABLE_FM := false
endif
endif

BOARD_HAVE_BLUETOOTH_RTK := false 

