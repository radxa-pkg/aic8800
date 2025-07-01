####################################################################USB INF KO

ifeq ($(BLUETOOTH_INF), USB)

ifeq ($(BLUETOOTH_MODULE), RTKBT)
	BOARD_VENDOR_KERNEL_MODULES += $(PRODUCT_OUT)/obj/lib_vendor/rtk_btusb.ko
endif

ifeq ($(BLUETOOTH_MODULE), BCMBT)
	BOARD_VENDOR_KERNEL_MODULES += $(PRODUCT_OUT)/obj/lib_vendor/btusb.ko
endif

ifeq ($(BLUETOOTH_MODULE), MTKBT)
	BOARD_VENDOR_KERNEL_MODULES += $(PRODUCT_OUT)/obj/lib_vendor/btmtk_usb.ko
endif

####################################################################OTHER INF KO
else

ifeq ($(BLUETOOTH_MODULE), MTKBT)
	BOARD_VENDOR_KERNEL_MODULES += $(PRODUCT_OUT)/obj/lib_vendor/btmtksdio.ko
endif

endif

####################################################################multi bt
ifeq ($(BLUETOOTH_MODULE), multibt)
	BOARD_VENDOR_KERNEL_MODULES += \
		$(PRODUCT_OUT)/obj/lib_vendor/rtk_btusb.ko \
		$(PRODUCT_OUT)/obj/lib_vendor/btusb.ko \
		$(PRODUCT_OUT)/obj/lib_vendor/btmtk_usb.ko \
		$(PRODUCT_OUT)/obj/lib_vendor/btmtksdio.ko \
		$(PRODUCT_OUT)/obj/lib_vendor/btmtk_uart_unify.ko \
		$(PRODUCT_OUT)/obj/lib_vendor/btmtk_usb_unify.ko \
		$(PRODUCT_OUT)/obj/lib_vendor/btmtk_sdio_unify.ko \
		$(PRODUCT_OUT)/obj/lib_vendor/aic_btusb.ko
endif
