# RELEASE NAME: 20180702_BT_ANDROID_9.0
# RTKBT_API_VERSION=2.1.1.0

CUR_PATH := hardware/aic/aicbt

BOARD_HAVE_BLUETOOTH := true

PRODUCT_COPY_FILES += \
        $(CUR_PATH)/vendor/etc/bluetooth/aicbt.conf:vendor/etc/bluetooth/aicbt.conf


PRODUCT_PACKAGES += \
	libbt-vendor-aic

