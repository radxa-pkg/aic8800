#
# Copyright (C) 2008 The Android Open Source Project
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
#
#$(warning "aic firmware copy") 

FW_BASE_PATH := hardware/aic/aicbt/vendor/firmware
SEARCH_PATH := $(shell find $(FW_BASE_PATH)/ -name "*.bin" | sed 's/\/[^/]\+\/*$$//g')
$(foreach p,$(SEARCH_PATH), $(eval FW_BIN_FILES += $(call find-copy-subdir-files,"*.bin",$(p),$(TARGET_COPY_OUT_VENDOR)/etc/firmware)))


FW_BIN_FILES += hardware/aic/aicbt/vendor/firmware/aic_userconfig.txt:$(TARGET_COPY_OUT_VENDOR)/etc/firmware/aic_userconfig.txt
FW_BIN_FILES += hardware/aic/aicbt/vendor/etc/bluetooth/aicbt.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/aicbt.conf

PRODUCT_COPY_FILES += $(FW_BIN_FILES)
