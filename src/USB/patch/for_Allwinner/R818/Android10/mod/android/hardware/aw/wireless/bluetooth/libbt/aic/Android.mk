LOCAL_PATH := hardware/aic/aicbt/code/libbt-vendor/src

include $(CLEAR_VARS)

BDROID_DIR := $(TOP_DIR)system/bt

LOCAL_SRC_FILES := \
        aic_socket.c \
        bt_vendor_aic.c \
        hardware.c \
        userial_vendor.c \
        upio.c \
        bt_list.c \
        bt_skbuff.c \
        hci_h5.c \
        aic_parse.c \
        aic_btservice.c \
        hardware_uart.c \
        hardware_usb.c \
        aic_heartbeat.c \
        aic_poll.c \
        aic_btsnoop_net.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../include \
        $(LOCAL_PATH)/../codec/sbc \
        $(LOCAL_PATH)/../codec/plc \
        $(BDROID_DIR)/hci/include

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        liblog

LOCAL_WHOLE_STATIC_LIBRARIES := \
        libbt-codec

LOCAL_MODULE := libbt-aic
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
