#===============================================================================
# export variables
#===============================================================================
ifeq ($(CFG_HI_EXPORT_FLAG),)
    ifneq ($(srctree),)
    KERNEL_DIR := $(srctree)

    SDK_DIR := $(shell cd $(KERNEL_DIR)/../../.. && /bin/pwd)
    else
    SDK_DIR := $(shell cd $(CURDIR)/../../../.. && /bin/pwd)
    endif
    include $(SDK_DIR)/base.mak
endif

#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#   variable
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
ifneq ($(ANDROID_BUILD),y)
    MODULE_BIN_DIR := $(HI_ROOTFS_DIR)/lib/firmware
    MODULE_CFG_DIR := $(HI_ROOTFS_DIR)/etc/Wireless/RT2870STA
else
ifneq ($(CFG_WIFI_FIRMWARE_IPTV),y)
    MODULE_BIN_DIR := $(ANDROID_PRODUCT_OUT)/vendor/firmware
else
    MODULE_BIN_DIR := $(ANDROID_PRODUCT_OUT)/system/lib/firmware
endif
    MODULE_CFG_DIR := $(ANDROID_PRODUCT_OUT)/system/etc/Wireless/RT2870STA
endif

objects :=

ifeq ($(CFG_HI_WIFI_DEVICE_MT7668U),y)
objects += usb_mt7668u
CFG_SRC_MT7668U := $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/wlan_cfg/mt7668/wifi.cfg
BIN_DIR_MT7668U := $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/bin/mt7668/EEPROM_MT7668.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/bin/mt7668/EEPROM_MT7668_e1.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/bin/mt7668/mt7668_patch_e1_hdr.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/bin/mt7668/mt7668_patch_e2_hdr.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/bin/mt7668/WIFI_RAM_CODE2_USB_MT7668.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/bin/mt7668/WIFI_RAM_CODE_MT7668.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt7668u/bin/mt7668/woble_setting_7668.bin
endif

ifeq ($(CFG_HI_WIFI_DEVICE_RTL8822BEH),y)
objects += pcie_rtl8822beh
endif

ifeq ($(CFG_HI_WIFI_DEVICE_RTL8822CU),y)
objects += usb_rtl8822cu
endif
ifeq ($(CFG_HI_WIFI_DEVICE_RTL8188FTV),y)
objects += usb_rtl8188ftv
endif

ifeq ($(CFG_HI_WIFI_DEVICE_MT7601U),y)
objects += usb_mt7601u
CFG_SRC_MT7601U := $(COMPONENT_DIR)/wifi/drv/usb_mt7601u/JEDI.MP1.mt7601u.v1.12/MT7601USTA.dat
ifneq ($(CFG_WIFI_FIRMWARE_IPTV),y)
BIN_DIR_MT7601U := $(ANDROID_PRODUCT_OUT)/vendor/etc
else
BIN_DIR_MT7601U := $(ANDROID_PRODUCT_OUT)/system/etc
endif
BIN_SRC_MT7601U := $(COMPONENT_DIR)/wifi/drv/usb_mt7601u/JEDI.MP1.mt7601u.v1.12/MT7601EEPROM.bin
endif

ifeq ($(CFG_HI_WIFI_DEVICE_RTL8723DU),y)
objects += usb_rtl8723du
endif

ifeq ($(CFG_HI_WIFI_DEVICE_ATBM6022),y)
objects += usb_atbm6022
endif

ifeq ($(CFG_HI_WIFI_DEVICE_MT76X2U),y)
objects += usb_mt76x2u
BIN_SRC_MT76X2U := $(COMPONENT_DIR)/wifi/drv/usb_mt76x2u/JEDI.MP2.mt76x2u.wifi.v3.3.2/conf/MT76x2STA.dat
FW_SRC_MT76X2U := $(COMPONENT_DIR)/wifi/drv/usb_mt76x2u/JEDI.MP2.mt76x2u.wifi.v3.3.2/mcu/bin/mt7662_firmware_e3_tvbox.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt76x2u/JEDI.MP2.mt76x2u.wifi.v3.3.2/mcu/bin/mt7662_patch_e3_hdr.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt76x2u/JEDI.MP2.mt76x2u.wifi.v3.3.2/mcu/bin/mt7662t_firmware_e1.bin \
                  $(COMPONENT_DIR)/wifi/drv/usb_mt76x2u/JEDI.MP2.mt76x2u.wifi.v3.3.2/mcu/bin/mt7662t_patch_e1_hdr.bin
endif

ifneq ($(objects),)
obj-y += $(addsuffix /,$(objects))
else
$(error "If wifi support, you must choose one wifi device at least!!")
endif

#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#   rules
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#==================================================================
#                          all
#==================================================================
.PHONY: all clean install

install:
ifeq ($(CFG_HI_WIFI_DEVICE_MT7668U),y)
	@test -d $(MODULE_BIN_DIR) || mkdir -p $(MODULE_BIN_DIR)
	@cp -v $(BIN_DIR_MT7668U) $(MODULE_BIN_DIR)/
	@cp -v $(CFG_SRC_MT7668U) $(MODULE_BIN_DIR)/
endif

ifeq ($(CFG_HI_WIFI_DEVICE_MT7601U),y)
	@test -d $(MODULE_CFG_DIR) || mkdir -p $(MODULE_CFG_DIR)
	@cp -v $(CFG_SRC_MT7601U) $(MODULE_CFG_DIR)/
	@test -d $(BIN_DIR_MT7601U) || mkdir -p $(BIN_DIR_MT7601U)
	@test -d $(BIN_SRC_MT7601U) || cp -v $(BIN_SRC_MT7601U) $(BIN_DIR_MT7601U)/
endif


ifeq ($(CFG_HI_WIFI_DEVICE_MT76X2U),y)
	@test -d $(MODULE_CFG_DIR) || mkdir -p $(MODULE_CFG_DIR)
	@cp -v $(BIN_SRC_MT76X2U) $(MODULE_CFG_DIR)/
	@test -d $(MODULE_BIN_DIR) || mkdir -p $(MODULE_BIN_DIR)
	@cp -v $(FW_SRC_MT76X2U) $(MODULE_BIN_DIR)/
endif

clean:
