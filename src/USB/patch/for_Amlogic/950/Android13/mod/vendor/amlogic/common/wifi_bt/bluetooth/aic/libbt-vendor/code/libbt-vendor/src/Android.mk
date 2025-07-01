LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

BDROID_DIR := $(TOP_DIR)packages/modules/Bluetooth/system


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
        aic_btsnoop_net.c \
	FallthroughBTA.cpp

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../include \
        $(LOCAL_PATH)/../codec/sbc \
        $(LOCAL_PATH)/../codec/plc \
	$(TOP_DIR)vendor/amlogic/common/wifi_bt/bluetooth/common/include \
        $(BDROID_DIR)/hci/include

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        liblog

LOCAL_WHOLE_STATIC_LIBRARIES := \
        libbt-codec-aic
ifeq ($(BOARD_HAVE_BLUETOOTH_MULTIBT),true)
        LOCAL_MODULE := libbt-vendor_aic
        LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
        LOCAL_LICENSE_CONDITIONS := notice
else
        LOCAL_MODULE := libbt-vendor
        LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
        LOCAL_LICENSE_CONDITIONS := notice
endif

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SHARED_LIBRARY)
