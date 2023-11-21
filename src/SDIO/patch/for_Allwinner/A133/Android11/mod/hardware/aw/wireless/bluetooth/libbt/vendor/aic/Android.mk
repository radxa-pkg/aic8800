LOCAL_PATH := hardware/aic/libbt

include $(CLEAR_VARS)

ifneq ($(BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR),)
  bdroid_C_INCLUDES := $(BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR)
  bdroid_CFLAGS += -DHAS_BDROID_BUILDCFG
else
  bdroid_C_INCLUDES :=
  bdroid_CFLAGS += -DHAS_NO_BDROID_BUILDCFG
endif

BDROID_DIR := $(TOP_DIR)system/bt

LOCAL_CFLAGS += -DUSE_AMPAK_BT_MODULE

LOCAL_CFLAGS += -DHAVE_BLUETOOTH_AIC

ifeq ($(strip $(USE_BLUETOOTH_AIC8800)),true)
LOCAL_CFLAGS += -DUSE_BLUETOOTH_AIC8800
endif

LOCAL_CFLAGS += \
        -Wall \
        -Werror \
        -Wno-switch \
        -Wno-unused-function \
        -Wno-unused-parameter \
        -Wno-unused-variable \

LOCAL_SRC_FILES := \
        src/bt_vendor_aicbt.c \
        src/aic_hardware.c \
        src/userial_vendor.c \
        src/upio.c \
        src/aic_conf.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
        $(BDROID_DIR)/hci/include \
        $(BDROID_DIR)/include \
        $(BDROID_DIR)/device/include \
        $(BDROID_DIR)

LOCAL_C_INCLUDES += $(bdroid_C_INCLUDES)
LOCAL_CFLAGS += $(bdroid_CFLAGS)

LOCAL_HEADER_LIBRARIES := libutils_headers

ifneq ($(BOARD_HAVE_BLUETOOTH_AIC_A2DP_OFFLOAD),)
  LOCAL_STATIC_LIBRARIES := libbt-aic_a2dp
endif

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        liblog

LOCAL_MODULE := libbt-aic
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := aic
LOCAL_PROPRIETARY_MODULE := true

include $(LOCAL_PATH)/vnd_buildcfg.mk

include $(BUILD_SHARED_LIBRARY)

