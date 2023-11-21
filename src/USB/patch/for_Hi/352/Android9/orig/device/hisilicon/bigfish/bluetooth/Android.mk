include $(CLEAR_VARS)

ifeq ($(BOARD_BLUETOOTH_DEVICE_REALTEK),y)
$(warning --------------------bluetooth IN_RTKBT, SDK $(PLATFORM_SDK_VERSION))
ifeq (1,$(filter 1,$(shell echo "$$(( $(PLATFORM_SDK_VERSION) == 28 ))" )))
$(warning --------------------bluetooth IN_RTKBT P)
include $(HISI_PLATFORM_PATH)/bluetooth/rtkbt/Android.mk
include $(HISI_PLATFORM_PATH)/bluetooth/bluedroid_lowlatency/Android.mk
endif
endif

ifeq ($(BOARD_BLUETOOTH_DEVICE_MT7668U),y)
include $(HISI_PLATFORM_PATH)/bluetooth/mt7668u/Android.mk
endif
