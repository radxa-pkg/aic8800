KERNEL_ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-
PRODUCT_OUT=out/target/product/$(TARGET_PRODUCT)
TARGET_OUT=$(PRODUCT_OUT)/obj/lib_vendor

################################################################################USB INF DRV
ifeq ($(BLUETOOTH_INF), USB)

define rtk-usb-bt
	@echo "interface is usb"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/realtek/rtk_btusb ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_BT_RTKBTUSB=m modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/realtek/rtk_btusb/rtk_btusb.ko $(TARGET_OUT)/

endef

define bcm-usb-bt
	@echo "interface is usb"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/broadcom/btusb/btusb_1_6_29_1/ ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/broadcom/btusb/btusb_1_6_29_1/btusb.ko $(TARGET_OUT)/
endef

define mtk-usb-bt
	@echo "interface is usb"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_usb/ ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_usb/btmtk_usb.ko $(TARGET_OUT)/
endef

################################################################################OTHER INF DRV
else

define mtk-sdio-bt
	@echo "interface is sdio"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_sdio/ ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_sdio/btmtksdio.ko $(TARGET_OUT)/
endef

define multi-bt
	@echo "interface is multibt"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/realtek/rtk_btusb ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CONFIG_BT_RTKBTUSB=m modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/realtek/rtk_btusb/rtk_btusb.ko $(TARGET_OUT)/

	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/broadcom/btusb/btusb_1_6_29_1/ ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/broadcom/btusb/btusb_1_6_29_1/btusb.ko $(TARGET_OUT)/

	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_usb/ ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_usb/btmtk_usb.ko $(TARGET_OUT)/

	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_sdio/ ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/bt_driver_sdio/btmtksdio.ko $(TARGET_OUT)/

	@echo "building mt7961_usb_bt"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mt7961/bt_driver/linux_v2/ MTK_CHIP_IF=usb ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mt7961/bt_driver/linux_v2/btmtk_usb_unify.ko $(TARGET_OUT)/

	@echo "building mt7961_sdio_bt"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mt7961/bt_driver/linux_v2/ MTK_CHIP_IF=sdio ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mt7961/bt_driver/linux_v2/btmtk_sdio_unify.ko $(TARGET_OUT)/
	
	@echo "building aic8800_usb_bt"
	$(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/aic/aic_btusb ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/aic/aic_btusb/aic_btusb.ko $(TARGET_OUT)/
endef
#	@echo "building mt7961_uart_tty_bt"
#   $(MAKE) -C $(shell pwd)/$(PRODUCT_OUT)/obj/KERNEL_OBJ M=$(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mt7961/bt_driver/linux_v2/ MTK_CHIP_IF=uart_tty ARCH=$(KERNEL_ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
#   cp $(shell pwd)/vendor/amlogic/common/wifi_bt/bluetooth/mtk/mtkbt/mt7961/bt_driver/linux_v2/btmtk_uart_tty_unify.ko $(TARGET_OUT)/

endif


default:
	@echo "no bt configured"

QCABT:
	@echo "qca bt configured"

RTKBT:
	@echo "rtk bt configured"
	$(rtk-usb-bt)

BCMBT:
	@echo "bcm bt configured"
	$(bcm-usb-bt)

MTKBT:
	@echo "mtk bt configured"
	$(mtk-sdio-bt)
	$(mtk-usb-bt)

multibt:
	@echo "multi bt configured"
	$(multi-bt)
