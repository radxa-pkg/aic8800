# RELEASE NAME: 20180702_BT_ANDROID_9.0
# RTKBT_API_VERSION=2.1.1.0

CUR_PATH := hardware/aic/aicbt

BOARD_HAVE_BLUETOOTH := true

PRODUCT_PACKAGES += \
	libbt-vendor-aic

PRODUCT_PROPERTY_OVERRIDES += \
	persist.service.bdroid.bdaddr=22:23:67:c6:69:73
