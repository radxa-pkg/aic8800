#
# Copyright (C) 2012 The Android Open Source Project
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

#Support modules:
#   bcm40183, AP6210, AP6476, AP6330, AP62x2,AP6335,mt5931 & mt6622

-include device/amlogic/$(PRODUCT_DIR)/wifibt.build.config.trunk.mk
BLUETOOTH_MODULE := $(CONFIG_BLUETOOTH_MODULES)
ifeq ($(BLUETOOTH_MODULE), )
BLUETOOTH_MODULE := multibt
endif

$(warning BLUETOOTH_MODULE is $(BLUETOOTH_MODULE))
ifneq ($(BLUETOOTH_INF),)
$(warning BLUETOOTH_INF is $(BLUETOOTH_INF))
else
$(warning BLUETOOTH_INF is not set)
endif

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
    PRODUCT_PROPERTY_OVERRIDES += config.disable_bluetooth=false \
    ro.vendor.autoconnectbt.isneed=false \
    ro.vendor.autoconnectbt.macprefix=00:CD:FF \
    ro.vendor.autoconnectbt.btclass=50c \
    ro.vendor.autoconnectbt.nameprefix?=Amlogic_RC \
    ro.vendor.autoconnectbt.rssilimit=70 \
    persist.bluetooth.bluetooth_audio_hal.disabled = false

else
    PRODUCT_PROPERTY_OVERRIDES += config.disable_bluetooth=true

endif

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
PRODUCT_PACKAGES += Bluetooth \
    bt_vendor.conf \
    bt_stack.conf \
    bt_did.conf \
    auto_pair_devlist.conf \
    libbt-hci \
    bluetooth.default \
    audio.a2dp.default \
    libbt-client-api \
    com.broadcom.bt \
    com.broadcom.bt.xml \
    android.hardware.bluetooth@1.0-impl-droidlogic \
    android.hardware.bluetooth@1.0-service-droidlogic

PRODUCT_COPY_FILES += \
    vendor/amlogic/common/wifi_bt/bluetooth/broadcom/vendor/data/auto_pairing.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/auto_pairing.conf \
    vendor/amlogic/common/wifi_bt/bluetooth/broadcom/vendor/data/blacklist.conf:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth/blacklist.conf


endif


##################################################################################amlbt
ifeq ($(BLUETOOTH_MODULE), AMLBT)

#load aml mk
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/amlogic/amlbt.mk )

PRODUCT_COPY_FILES += vendor/amlogic/common/wifi_bt/bluetooth/configs/init_rc/init.amlogic.amlbt.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.amlogic.bluetooth.rc

PRODUCT_PACKAGES += libbt-vendor
endif
################################################################################## RTKBT
ifeq ($(BLUETOOTH_MODULE),RTKBT)
#ifneq ($(filter rtl8761 rtl8723bs rtl8723bu rtl8821 rtl8822bu rtl8822bs, $(BLUETOOTH_MODULE)),)


#Realtek add start
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/realtek/rtkbt/rtkbt.mk )
#realtek add end
PRODUCT_PACKAGES += libbt-vendor

#Realtek add start
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml
#realtek add end
endif

################################################################################## qcabt
ifeq ($(BLUETOOTH_MODULE),QCABT)

#qca add start
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/qualcomm/qcabt.mk )
#qca add end
ifeq ($(BLUETOOTH_INF), USB)
PRODUCT_COPY_FILES += vendor/amlogic/common/wifi_bt/bluetooth/configs/init_rc/init.amlogic.bluetooth_qca.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.amlogic.bluetooth.rc
endif

PRODUCT_PACKAGES += libbt-vendor

endif

##################################################################################bcmbt
ifeq ($(BLUETOOTH_MODULE), BCMBT)

#load bcm mk
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/broadcom/bcmbt.mk )
#load bcm mk end
ifeq ($(BLUETOOTH_INF), USB)
PRODUCT_COPY_FILES += vendor/amlogic/common/wifi_bt/bluetooth/configs/init_rc/init.amlogic.bluetooth.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.amlogic.bluetooth.rc
endif


PRODUCT_PACKAGES += libbt-vendor

endif

##################################################################################mtkbt
ifeq ($(BLUETOOTH_MODULE), MTKBT)

#load mtk mk
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mtkbt.mk )
#load mtk mk end


PRODUCT_PACKAGES += libbt-vendor \
                    libbluetooth_mtkbt

endif

##################################################################################nxpbt
ifeq ($(BLUETOOTH_MODULE), NXPBT)

#load nxp mk
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/nxp/nxpbt.mk )
#load nxp mk end

ifeq ($(BLUETOOTH_INF), UART)
PRODUCT_COPY_FILES += vendor/amlogic/common/wifi_bt/bluetooth/configs/init_rc/init.amlogic.bluetooth_nxp_uart.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.amlogic.bluetooth.rc
endif

PRODUCT_PACKAGES += libbt-vendor

endif

##################################################################################multibt
ifeq ($(BLUETOOTH_MODULE), multibt)

BOARD_HAVE_BLUETOOTH_MULTIBT := true
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.btmodule = multibt

$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/realtek/rtkbt/rtkbt.mk )
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mtkbt.mk )
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/broadcom/bcmbt.mk )
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/qualcomm/qcabt.mk )
$(call inherit-product, vendor/amlogic/common/wifi_bt/bluetooth/amlogic/amlbt.mk )

PRODUCT_COPY_FILES += vendor/amlogic/common/wifi_bt/bluetooth/configs/init_rc/init.amlogic.amlbt.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.amlogic.bluetooth.rc


PRODUCT_PACKAGES += \
	libbt-vendor_bcm \
    libbt-vendor_rtl \
	libbt-vendor_mtk \
	libbt-vendor_qca \
    libbluetooth_mtkbt \
    libbt-vendor_aml
endif
