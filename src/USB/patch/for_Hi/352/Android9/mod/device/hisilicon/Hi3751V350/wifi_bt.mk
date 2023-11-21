# Bluetooth and WIFI configuration

# Enable WiFi or Bluetooth or WiFi+Bluetooth module used in the board.
# Supported modules:
#   RealTek RTl8822BU (11ac 2x2 2.4G+5G)(Supported mode: STA, AP, WiFi Direct And BT)
# Set to 'y', enable the WiFi module, the driver will be compiled.
# Set to 'n', disable the WiFi module, the driver won't be compiled.
# Attention : Can set only one module to 'y'.
# Example:
#   enable RTL8188EUS : BOARD_WLAN_DEVICE_RTL8188EUS = y
#   disable RTL8188EUS: BOARD_WLAN_DEVICE_RTL8188EUS = n

# ------> Wifi Only <------------
# RTL8188EUS
#BOARD_WIFI_RTL8188EUS := n
# RTL8188FTV
BOARD_WIFI_RTL8188FTV := n
BOARD_WIFI_ATBM6022 := n
# MT7601U
BOARD_WIFI_MT7601U := n

# ------> BT Only   <-------------
# CSR8510
#BOARD_BLUETOOTH_CSR8510 := n

# ------> Wifi + BT <------------
# RTL8822BEH
BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822BEH := n
# RTL8723DU
BOARD_WIFI_BLUETOOTH_DEVICE_RTL8723DU := n
# RTL8822BU
BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822BU := n

# RTL8822CU
BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822CU := n
# MT7668U
BOARD_WIFI_BLUETOOTH_DEVICE_MT7668U   := n

# AIC8800D
BOARD_WIFI_BLUETOOTH_DEVICE_AIC8800D := y

##  Stable configuration definitions
ifeq ($(BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822BEH),y)
HI_WIFI_SUPPORT := y
# PCIE device, bluetooth can not use realtek driver
HI_BLUETOOTH_SUPPORT := n
BOARD_WIFI_VENDOR := realtek
BOARD_BLUETOOTH_DEVICE_REALTEK := y
BOARD_HAVE_BLUETOOTH_RTK := true
BOARD_HAVE_BLUETOOTH_RTK_IF := uart
BOARD_HAVE_BLUETOOTH_RTK_COEX := true
$(call inherit-product, device/hisilicon/bigfish/bluetooth/rtkbt/rtkbt.mk)
endif
ifeq ($(BOARD_WIFI_BLUETOOTH_DEVICE_RTL8723DU),y)
HI_WIFI_SUPPORT := y
HI_BLUETOOTH_SUPPORT := y
BOARD_WIFI_VENDOR := realtek
BOARD_BLUETOOTH_DEVICE_REALTEK := y
BOARD_HAVE_BLUETOOTH_RTK := true
BOARD_HAVE_BLUETOOTH_RTK_IF := usb
BOARD_HAVE_BLUETOOTH_RTK_COEX := true
$(call inherit-product, device/hisilicon/bigfish/bluetooth/rtkbt/rtkbt.mk)
endif
ifeq ($(BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822BU),y)
HI_WIFI_SUPPORT := y
HI_BLUETOOTH_SUPPORT := y
BOARD_WIFI_VENDOR := realtek
BOARD_BLUETOOTH_DEVICE_REALTEK := y
BOARD_HAVE_BLUETOOTH_RTK := true
BOARD_HAVE_BLUETOOTH_RTK_IF := usb
BOARD_HAVE_BLUETOOTH_RTK_COEX := true
$(call inherit-product, device/hisilicon/bigfish/bluetooth/rtkbt/rtkbt.mk)
endif

ifeq ($(BOARD_WIFI_BLUETOOTH_DEVICE_RTL8822CU),y)
HI_WIFI_SUPPORT := y
HI_BLUETOOTH_SUPPORT := y
BOARD_WIFI_VENDOR := realtek
BOARD_BLUETOOTH_DEVICE_REALTEK := y
BOARD_HAVE_BLUETOOTH_RTK := true
BOARD_HAVE_BLUETOOTH_RTK_IF := usb
BOARD_HAVE_BLUETOOTH_RTK_COEX := true
$(call inherit-product, device/hisilicon/bigfish/bluetooth/rtkbt/rtkbt.mk)
endif
ifeq ($(BOARD_WIFI_BLUETOOTH_DEVICE_MT7668U), y)
HI_WIFI_SUPPORT := y
HI_BLUETOOTH_SUPPORT := y
BOARD_WIFI_VENDOR := mediatek
BOARD_BLUETOOTH_DEVICE_MT7668U := y
endif
ifeq ($(BOARD_WIFI_ATBM6022), y)
HI_WIFI_SUPPORT := y
BOARD_WIFI_VENDOR := atbm6022
endif
ifeq ($(BOARD_WIFI_RTL8188FTV), y)
HI_WIFI_SUPPORT := y
BOARD_WIFI_VENDOR := realtek
endif
ifeq ($(BOARD_WIFI_MT7601U), y)
HI_WIFI_SUPPORT := y
BOARD_WIFI_VENDOR := mediatek
endif
ifeq ($(BOARD_WIFI_BLUETOOTH_DEVICE_AIC8800D),y)
HI_WIFI_SUPPORT := y
HI_BLUETOOTH_SUPPORT := y
BOARD_BLUETOOTH_DEVICE_AIC8800D := y
BOARD_WIFI_VENDOR := aic
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_AIC := true
BOARD_HAVE_BLUETOOTH_AIC_COEX := true
$(call inherit-product, device/hisilicon/bigfish/bluetooth/aicbt/aicbt.mk)
endif
# Stable configuration definitions

# wpa_supplicant and hostapd build configuration
# wpa_supplicant is used for WiFi STA, hostapd is used for WiFi SoftAP
ifeq ($(BOARD_WIFI_VENDOR), realtek)
PRODUCT_PROPERTY_OVERRIDES += wifi.driver.supplicant=wpa_supplicant
WPA_SUPPLICANT_VERSION := VER_0_8_X_REALTEK
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_rtl
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_rtl

BOARD_WLAN_DEVICE := realtek
endif
ifeq ($(BOARD_WIFI_VENDOR), mediatek)
PRODUCT_PROPERTY_OVERRIDES += wifi.driver.supplicant=mtk_supplicant
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_bcmdhd

BOARD_WLAN_DEVICE := bcmdhd
endif
ifeq ($(BOARD_WIFI_VENDOR), )
PRODUCT_PROPERTY_OVERRIDES += wifi.driver.supplicant=wpa_supplicant
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_bcmdhd

BOARD_WLAN_DEVICE := bcmdhd
endif
ifeq ($(BOARD_WIFI_VENDOR), atbm6022)
PRODUCT_PROPERTY_OVERRIDES += wifi.driver.supplicant=wpa_supplicant
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_bcmdhd

BOARD_WLAN_DEVICE := bcmdhd
endif

ifeq ($(BOARD_WIFI_VENDOR), aic)
PRODUCT_PROPERTY_OVERRIDES += wifi.driver.supplicant=aic_supplicant
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_aic
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_aic

BOARD_WLAN_DEVICE := aic
endif


