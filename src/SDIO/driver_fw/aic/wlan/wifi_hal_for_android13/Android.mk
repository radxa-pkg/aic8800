# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

ifeq ($(WPA_SUPPLICANT_VERSION),VER_0_8_X)

ifneq ($(BOARD_WPA_SUPPLICANT_DRIVER),)
  CONFIG_DRIVER_$(BOARD_WPA_SUPPLICANT_DRIVER) := y
endif

WPA_SUPPL_DIR = external/wpa_supplicant_8
WPA_SRC_FILE :=

include $(WPA_SUPPL_DIR)/wpa_supplicant/android.config

WPA_SUPPL_DIR_INCLUDE = $(WPA_SUPPL_DIR)/src \
	$(WPA_SUPPL_DIR)/src/common \
	$(WPA_SUPPL_DIR)/src/drivers \
	$(WPA_SUPPL_DIR)/src/l2_packet \
	$(WPA_SUPPL_DIR)/src/utils \
	$(WPA_SUPPL_DIR)/src/wps \
	$(WPA_SUPPL_DIR)/wpa_supplicant

ifdef CONFIG_DRIVER_NL80211
WPA_SUPPL_DIR_INCLUDE += external/libnl/include
WPA_SRC_FILE += driver_cmd_nl80211.c
endif

ifdef CONFIG_DRIVER_WEXT
WPA_SRC_FILE += driver_cmd_wext.c
endif

ifeq ($(TARGET_ARCH),arm)
# To force sizeof(enum) = 4
L_CFLAGS += -mabi=aapcs-linux
endif

ifdef CONFIG_ANDROID_LOG
L_CFLAGS += -DCONFIG_ANDROID_LOG
endif

ifdef CONFIG_P2P
L_CFLAGS += -DCONFIG_P2P
endif

L_CFLAGS += -Wall -Werror -Wno-unused-parameter -Wno-macro-redefined

########################

include $(CLEAR_VARS)
LOCAL_MODULE := lib_driver_cmd_aic
LOCAL_SHARED_LIBRARIES := libc libcutils
LOCAL_CFLAGS := $(L_CFLAGS)
LOCAL_SRC_FILES := $(WPA_SRC_FILE)
LOCAL_C_INCLUDES := $(WPA_SUPPL_DIR_INCLUDE)
LOCAL_VENDOR_MODULE := true
include $(BUILD_STATIC_LIBRARY)

########################

endif

# Make the HAL library
# ============================================================
include $(CLEAR_VARS)

LOCAL_CFLAGS := \
    -Wall \
    -Werror \
    -Wno-format \
    -Wno-reorder \
    -Wno-unused-function \
    -Wno-unused-parameter \
    -Wno-unused-private-field \
    -Wno-unused-variable \
    -include unistd.h

LOCAL_C_INCLUDES += \
	external/libnl/include \
	$(call include-path-for, libhardware_legacy)/hardware_legacy \
	external/wpa_supplicant_8/src/drivers

LOCAL_HEADER_LIBRARIES := libutils_headers liblog_headers

LOCAL_SRC_FILES := \
	wifi_hal.cpp \
	rtt.cpp \
	common.cpp \
	cpp_bindings.cpp \
	gscan.cpp \
	link_layer_stats.cpp \
	wifi_logger.cpp \
	wifi_offload.cpp

LOCAL_SHARED_LIBRARIES := \
	libnl \
	libutils \
	liblog

LOCAL_MODULE := libwifi-hal-aic
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)

