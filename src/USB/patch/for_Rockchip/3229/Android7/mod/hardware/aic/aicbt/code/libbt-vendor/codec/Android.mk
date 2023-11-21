LOCAL_PATH := $(call my-dir)

# libbt-codec.a
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        ./plc/sbcplc.c \
        ./sbc/sbc.c \
        ./sbc/sbc_primitives.c \
        ./sbc/sbc_primitives_mmx.c \
        ./sbc/sbc_primitives_neon.c

LOCAL_C_INCLUDES :=
LOCAL_CFLAGS := \
        -Wno-error=unused-parameter \
        -Wno-ignored-attributes

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := libcutils libc
LOCAL_MODULE := libbt-codec
#LOCAL_MULTILIB := 32

include $(BUILD_STATIC_LIBRARY)

