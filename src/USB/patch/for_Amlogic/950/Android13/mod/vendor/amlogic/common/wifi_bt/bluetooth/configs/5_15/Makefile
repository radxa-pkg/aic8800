modules:
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_usb KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../mtk/mtkbt/bt_driver_usb O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_usb
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_sdio KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../mtk/mtkbt/bt_driver_sdio O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_sdio
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../realtek/rtk_btusb KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../realtek/rtk_btusb O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../realtek/rtk_btusb
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../aic/aic_btusb KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../aic/aic_btusb O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../aic/aic_btusb
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 MTK_CHIP_IF=usb O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 MTK_CHIP_IF=sdio O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 MTK_CHIP_IF=uart_tty O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../amlogic/sdio_driver_bt KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../amlogic/sdio_driver_bt O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../amlogic/sdio_driver_bt
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../amlogic/usb_driver_bt KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../amlogic/usb_driver_bt O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../amlogic/usb_driver_bt  
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../unisoc/tty-sdio KERNEL_SRC=$(KERNEL_SRC) M=$(M)/../../unisoc/tty-sdio O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) SUBDIRS=$(KERNEL_SRC)/$(M)/../../unisoc/tty-sdio

modules_install:
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_usb M=$(M)/../../mtk/mtkbt/bt_driver_usb KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_usb modules_install
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_sdio M=$(M)/../../mtk/mtkbt/bt_driver_sdio KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/bt_driver_sdio modules_install
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../realtek/rtk_btusb M=$(M)/../../realtek/rtk_btusb KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../realtek/rtk_btusb modules_install
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../aic/aic_btusb M=$(M)/../../aic/aic_btusb KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../aic/aic_btusb modules_install
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 M=$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 MTK_CHIP_IF=usb KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 modules_install
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 M=$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 MTK_CHIP_IF=sdio KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 modules_install
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 M=$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 MTK_CHIP_IF=uart_tty KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../mtk/mtkbt/mt7961/bt_driver/linux_v2 modules_install
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../amlogic/sdio_driver_bt M=$(M)/../../amlogic/sdio_driver_bt KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../amlogic/sdio_driver_bt modules_install
	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../amlogic/usb_driver_bt M=$(M)/../../amlogic/usb_driver_bt KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../amlogic/usb_driver_bt modules_install
#	$(MAKE) -C $(KERNEL_SRC)/$(M)/../../unisoc/tty-sdio M=$(M)/../../unisoc/tty-sdio KERNEL_SRC=$(KERNEL_SRC) O=$(O) CC=$(CC) HOSTCC=$(HOSTCC) LD=$(LD) NM=$(NV) OBJCOPY=$(OBJCOPY) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) SUBDIRS=$(KERNEL_SRC)/$(M)/../../unisoc/tty-sdio modules_install
	@order_file="";\
	modules_order_file=`find $(INSTALL_MOD_PATH)/lib/modules/*/$(INSTALL_MOD_DIR) -name "modules.order.*"`;\
	for f in $$modules_order_file; do\
	 if test -z "$$order_file";then\
	  order_file=$$f;\
	 else\
	  cat $$f >> $$order_file;\
	  rm -rf $$f;\
	 fi;\
	done

