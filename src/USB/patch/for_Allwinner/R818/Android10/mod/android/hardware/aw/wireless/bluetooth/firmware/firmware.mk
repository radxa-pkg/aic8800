-include hardware/aw/wireless/bluetooth/firmware/xradio/xradio-bt.mk
-include hardware/aw/wireless/bluetooth/firmware/realtek/realtek-bt.mk
-include hardware/aw/wireless/bluetooth/firmware/broadcom/broadcom-bt.mk
-include hardware/aw/wireless/bluetooth/firmware/sprd/sprd-bt.mk
-include hardware/aw/wireless/bluetooth/firmware/aic/aic-bt.mk

# initrc for bluetooth
PRODUCT_COPY_FILES += \
    hardware/aw/wireless/bluetooth/config/init.bluetooth.common.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.bluetooth.common.rc
