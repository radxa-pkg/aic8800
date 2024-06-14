LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := stack_example
LOCAL_SRC_FILES := stack_example.c
include $(BUILD_EXECUTABLE)

