# RELEASE NAME: 20210202_BT_ANDROID_9.0
# AICBT_API_VERSION=1.0.0.0

CUR_PATH := hardware/aic/aicbt

BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_AIC := true
BOARD_HAVE_BLUETOOTH_AIC_COEX := true

ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(CUR_PATH)/bluetooth
endif
ifneq ($(filter rk3328, $(strip $(TARGET_BOARD_PLATFORM))), )
PRODUCT_COPY_FILES += \
	$(CUR_PATH)/vendor/etc/bluetooth/aicbt_S0.conf:vendor/etc/bluetooth/aicbt.conf
else
PRODUCT_COPY_FILES += \
	$(CUR_PATH)/vendor/etc/bluetooth/aicbt.conf:vendor/etc/bluetooth/aicbt.conf
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), tablet)
BT_FIRMWARE_FILES := $(shell ls $(CUR_PATH)/vendor/firmware)
PRODUCT_COPY_FILES += \
	$(foreach file, $(BT_FIRMWARE_FILES), $(CUR_PATH)/vendor/firmware/$(file):$(TARGET_COPY_OUT_VENDOR)/etc/firmware/$(file))
else
BT_FIRMWARE_FILES := $(shell ls $(CUR_PATH)/vendor/firmware_box)
PRODUCT_COPY_FILES += \
	$(foreach file, $(BT_FIRMWARE_FILES), $(CUR_PATH)/vendor/firmware_box/$(file):$(TARGET_COPY_OUT_VENDOR)/etc/firmware/$(file))
endif

PRODUCT_PACKAGES += \
	libbt-vendor-aic

PRODUCT_PROPERTY_OVERRIDES += \
	persist.bluetooth.btsnoopenable=false \
	persist.bluetooth.btsnooppath=/sdcard/btsnoop_hci.cfa \
	persist.bluetooth.btsnoopsize=0xffff \
	persist.bluetooth.aiccoex=true \
	bluetooth.enable_timeout_ms=11000

