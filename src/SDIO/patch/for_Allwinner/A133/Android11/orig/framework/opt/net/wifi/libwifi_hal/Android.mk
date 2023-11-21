# Copyright (C) 2016 The Android Open Source Project
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

wifi_hal_cflags := \
    -Wall \
    -Werror \
    -Wextra \
    -Winit-self \
    -Wno-unused-function \
    -Wno-unused-parameter \
    -Wshadow \
    -Wunused-variable \
    -Wwrite-strings
ifdef WIFI_DRIVER_MODULE_PATH
wifi_hal_cflags += -DWIFI_DRIVER_MODULE_PATH=\"$(WIFI_DRIVER_MODULE_PATH)\"
endif
ifdef WIFI_DRIVER_MODULE_ARG
wifi_hal_cflags += -DWIFI_DRIVER_MODULE_ARG=\"$(WIFI_DRIVER_MODULE_ARG)\"
endif
ifdef WIFI_DRIVER_MODULE_NAME
wifi_hal_cflags += -DWIFI_DRIVER_MODULE_NAME=\"$(WIFI_DRIVER_MODULE_NAME)\"
endif
ifdef WIFI_DRIVER_FW_PATH_STA
wifi_hal_cflags += -DWIFI_DRIVER_FW_PATH_STA=\"$(WIFI_DRIVER_FW_PATH_STA)\"
endif
ifdef WIFI_DRIVER_FW_PATH_AP
wifi_hal_cflags += -DWIFI_DRIVER_FW_PATH_AP=\"$(WIFI_DRIVER_FW_PATH_AP)\"
endif
ifdef WIFI_DRIVER_FW_PATH_P2P
wifi_hal_cflags += -DWIFI_DRIVER_FW_PATH_P2P=\"$(WIFI_DRIVER_FW_PATH_P2P)\"
endif

# Some devices use a different path (e.g. devices with broadcom WiFi parts).
ifdef WIFI_DRIVER_FW_PATH_PARAM
wifi_hal_cflags += -DWIFI_DRIVER_FW_PATH_PARAM=\"$(WIFI_DRIVER_FW_PATH_PARAM)\"
else
wifi_hal_cflags += -DWIFI_DRIVER_FW_PATH_PARAM=\"/sys/module/wlan/parameters/fwpath\"
endif

ifdef WIFI_DRIVER_STATE_CTRL_PARAM
wifi_hal_cflags += -DWIFI_DRIVER_STATE_CTRL_PARAM=\"$(WIFI_DRIVER_STATE_CTRL_PARAM)\"
endif
ifdef WIFI_DRIVER_STATE_ON
wifi_hal_cflags += -DWIFI_DRIVER_STATE_ON=\"$(WIFI_DRIVER_STATE_ON)\"
endif
ifdef WIFI_DRIVER_STATE_OFF
wifi_hal_cflags += -DWIFI_DRIVER_STATE_OFF=\"$(WIFI_DRIVER_STATE_OFF)\"
endif

# Common code shared between the HALs.
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libwifi-hal-common
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS := $(wifi_hal_cflags)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := libbase
LOCAL_HEADER_LIBRARIES := libcutils_headers
ifeq ($(BOARD_WIFI_VENDOR),common)
LOCAL_CPPFLAGS   += -DBOARD_WIFI_VENDOR_COMMON
LOCAL_C_INCLUDES += hardware/aw/wireless/hwinfo
endif
LOCAL_SRC_FILES := wifi_hal_common.cpp
include $(BUILD_STATIC_LIBRARY)

# A fallback "vendor" HAL library.
# Don't link this, link libwifi-hal.
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libwifi-hal-fallback
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS := $(wifi_hal_cflags)
LOCAL_SRC_FILES := wifi_hal_fallback.cpp
LOCAL_HEADER_LIBRARIES := libhardware_legacy_headers
include $(BUILD_STATIC_LIBRARY)

# Pick a vendor provided HAL implementation library.
# ============================================================
LIB_WIFI_HAL := libwifi-hal-fallback
VENDOR_LOCAL_SHARED_LIBRARIES :=
ifeq ($(BOARD_WLAN_DEVICE), bcmdhd)
  LIB_WIFI_HAL := libwifi-hal-bcm
  VENDOR_LOCAL_SHARED_LIBRARIES := libcrypto
else ifeq ($(BOARD_WLAN_DEVICE), qcwcn)
  LIB_WIFI_HAL := libwifi-hal-qcom
  VENDOR_LOCAL_SHARED_LIBRARIES := libcld80211
else ifeq ($(BOARD_WLAN_DEVICE), mrvl)
  # this is commented because none of the nexus devices
  # that sport Marvell's wifi have support for HAL
  # LIB_WIFI_HAL := libwifi-hal-mrvl
else ifeq ($(BOARD_WLAN_DEVICE), MediaTek)
  # support MTK WIFI HAL
  LIB_WIFI_HAL := libwifi-hal-mt66xx
else ifeq ($(BOARD_WLAN_DEVICE), realtek)
  # support Realtek WIFI HAL
  LIB_WIFI_HAL := libwifi-hal-rtk
else ifeq ($(BOARD_WLAN_DEVICE), xradio)
  # support XRadio WIFI HAL
  LIB_WIFI_HAL := libwifi-hal-xradio
else ifeq ($(BOARD_WLAN_DEVICE), ssv)
  # support ssv WIFI HAL
  LIB_WIFI_HAL := libwifi-hal-ssv
else ifeq ($(BOARD_WLAN_DEVICE), sprd)
  # support sprd WIFI HAL
  LIB_WIFI_HAL := libwifi-hal-sprd
else ifeq ($(BOARD_WLAN_DEVICE), common)
  # support aw autodetct WIFI HAL
  LIB_WIFI_HAL := libwifi-hal-autodetect
else ifeq ($(BOARD_WLAN_DEVICE), emulator)
  LIB_WIFI_HAL := libwifi-hal-emu
endif

# The WiFi HAL that you should be linking.
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libwifi-hal
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS := $(wifi_hal_cflags)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include
LOCAL_EXPORT_HEADER_LIBRARY_HEADERS := libhardware_legacy_headers
LOCAL_HEADER_LIBRARIES := libhardware_legacy_headers
LOCAL_SHARED_LIBRARIES := \
    libbase \
    libcutils \
    liblog \
    libnl \
    libutils \
    $(VENDOR_LOCAL_SHARED_LIBRARIES)
LOCAL_SRC_FILES := \
    driver_tool.cpp \
    hal_tool.cpp
LOCAL_WHOLE_STATIC_LIBRARIES := $(LIB_WIFI_HAL) libwifi-hal-common
include $(BUILD_SHARED_LIBRARY)

# Test utilities (e.g. mock classes) for libwifi-hal
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libwifi-hal-test
LOCAL_CFLAGS := $(wifi_hal_cflags)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/testlib/include
LOCAL_STATIC_LIBRARIES := libgmock
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/testlib/include
include $(BUILD_STATIC_LIBRARY)
