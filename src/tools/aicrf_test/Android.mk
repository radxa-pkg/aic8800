LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := bt_test
LOCAL_SRC_FILES := bt_test.c
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := wifi_test
LOCAL_SRC_FILES := wifi_test.c
include $(BUILD_EXECUTABLE)

