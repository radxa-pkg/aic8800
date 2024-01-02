
ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk2928)
PRODUCT_COPY_FILES += \
	device/rockchip/common/wifi/lib/modules/wlan.ko:system/lib/modules/wlan.ko \
	device/rockchip/common/wifi/lib/modules/rkwifi.ko:system/lib/modules/rkwifi.ko \
	device/rockchip/common/wifi/lib/modules/rkwifi.oob.ko:system/lib/modules/rkwifi.oob.ko \
	device/rockchip/common/wifi/lib/modules/8188eu.ko:system/lib/modules/8188eu.ko \
	device/rockchip/common/wifi/lib/modules/8192cu.ko:system/lib/modules/8192cu.ko \
	device/rockchip/common/wifi/lib/modules/mt5931.ko:system/lib/modules/mt5931.ko \
	device/rockchip/common/wifi/lib/modules/8723au.ko:system/lib/modules/8723au.ko \
	device/rockchip/common/wifi/lib/modules/mt7601sta.ko:system/lib/modules/mt7601sta.ko \
	device/rockchip/common/wifi/lib/modules/mt7601ap.ko:system/lib/modules/mt7601ap.ko \
	device/rockchip/common/wifi/lib/modules/mtprealloc7601Usta.ko:system/lib/modules/mtprealloc7601Usta.ko \
	device/rockchip/common/wifi/lib/modules/esp8089.ko:system/lib/modules/esp8089.ko
else
PRODUCT_COPY_FILES += \
	device/rockchip/common/wifi/lib/modules_smp/wlan.ko:system/lib/modules/wlan.ko \
	device/rockchip/common/wifi/lib/modules_smp/rkwifi.ko:system/lib/modules/rkwifi.ko \
	device/rockchip/common/wifi/lib/modules_smp/rkwifi.oob.ko:system/lib/modules/rkwifi.oob.ko \
	device/rockchip/common/wifi/lib/modules_smp/8188eu.ko:system/lib/modules/8188eu.ko \
	device/rockchip/common/wifi/lib/modules_smp/8192cu.ko:system/lib/modules/8192cu.ko \
	device/rockchip/common/wifi/lib/modules_smp/mt5931.ko:system/lib/modules/mt5931.ko \
    device/rockchip/common/wifi/lib/modules_smp/8723as.ko:system/lib/modules/8723as.ko \
	device/rockchip/common/wifi/lib/modules_smp/8723au.ko:system/lib/modules/8723au.ko \
	device/rockchip/common/wifi/lib/modules_smp/8189es.ko:system/lib/modules/8189es.ko \
	device/rockchip/common/wifi/lib/modules_smp/mt7601sta.ko:system/lib/modules/mt7601sta.ko \
	device/rockchip/common/wifi/lib/modules_smp/mt7601ap.ko:system/lib/modules/mt7601ap.ko \
	device/rockchip/common/wifi/lib/modules_smp/mtprealloc7601Usta.ko:system/lib/modules/mtprealloc7601Usta.ko \
	device/rockchip/common/wifi/lib/modules_smp/esp8089.ko:system/lib/modules/esp8089.ko \
	device/rockchip/common/wifi/lib/modules_smp/aic8800_bsp.ko:system/lib/modules/aic8800_bsp.ko \
	device/rockchip/common/wifi/lib/modules_smp/aic8800_fdrv.ko:system/lib/modules/aic8800_fdrv.ko
endif

PRODUCT_COPY_FILES += \
	device/rockchip/common/wifi/lib/init_data.conf:system/lib/modules/init_data.conf \
	device/rockchip/common/wifi/lib/esp_init_data.bin:system/lib/modules/esp_init_data.bin \
	device/rockchip/common/wifi/lib/esp_supplicant:system/bin/esp_supplicant \
	device/rockchip/common/wifi/lib/esp_hostapd:system/bin/esp_hostapd \
	device/rockchip/common/wifi/lib/bcm_supplicant:system/bin/bcm_supplicant \
	device/rockchip/common/wifi/lib/rtl_supplicant:system/bin/rtl_supplicant \
	device/rockchip/common/wifi/lib/rtl_hostapd:system/bin/rtl_hostapd

#PRODUCT_COPY_FILES += \
    device/rockchip/common/wifi/mt5931/wpa_supplicant/wpa_supplicant:system/bin/wpa_supplicant_mt5931 \
    device/rockchip/common/wifi/mt5931/wpa_supplicant/p2p_supplicant.conf:system/etc/wifi/p2p_supplicant_mt5931.conf \
    device/rockchip/common/wifi/mt5931/wpa_supplicant/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant_mt5931.conf

ifeq ($(strip $(BOARD_CONNECTIVITY_VENDOR)), Espressif)
#add by th 
    PRODUCT_COPY_FILES += \
        device/rockchip/common/wifi/mt5931/wpa_supplicant/p2p_supplicant.conf:system/etc/wifi/p2p_supplicant.conf \
	device/rockchip/common/wifi/mt5931/wpa_supplicant/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf
endif

