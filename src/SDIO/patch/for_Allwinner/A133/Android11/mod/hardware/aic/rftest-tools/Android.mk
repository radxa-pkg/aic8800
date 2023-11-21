###############################################################################
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := aic-rftest-tools
LOCAL_MODULE_TAGS := optional
LOCAL_CERTIFICATE := platform
LOCAL_DEX_PREOPT := false
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_SRC_FILES := app-debug.apk
include $(BUILD_PREBUILT)
