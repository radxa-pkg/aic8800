# RELEASE NAME: 20210202_BT_ANDROID_9.0
# AICBT_API_VERSION=1.0.0.0

CUR_PATH := $(LOCAL_PATH)

#BOARD_HAVE_BLUETOOTH := true
#BOARD_HAVE_BLUETOOTH_AIC := true
#BOARD_HAVE_BLUETOOTH_AIC_COEX := true

#ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
#BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(CUR_PATH)/bluetooth
#endif

PRODUCT_COPY_FILES += \
	$(CUR_PATH)/vendor/etc/bluetooth/aicbt.conf:vendor/etc/bluetooth/aicbt.conf

PRODUCT_PROPERTY_OVERRIDES += \
	persist.bluetooth.btsnoopenable=false \
	persist.bluetooth.btsnooppath=/sdcard/btsnoop_hci.cfa \
	persist.bluetooth.btsnoopsize=0xffff \
	persist.bluetooth.aiccoex=true \
	bluetooth.enable_timeout_ms=11000

