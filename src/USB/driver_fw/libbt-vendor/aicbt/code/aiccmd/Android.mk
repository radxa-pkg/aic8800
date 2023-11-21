LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES += \
                    aiccmd.c


LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := aiccmd
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)

