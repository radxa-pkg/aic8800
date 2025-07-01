define to-root-path
$(strip $(patsubst %/,%,$(shell echo\
 $(shell bash -c "\
 cd $(1);\
 while [[ ( ! ( -d device/amlogic/common ) ) && ( \`pwd\` != "/" ) ]]; do\
  cd ..;\
  echo \"../\";\
 done;"\
 )|sed 's/[[:space:]]//g'))\
)
endef

####################################################################################
DRIVER_DIR ?= vendor/wifi_driver
ANDROID_ROOT_DIR     ?= $(shell cd $(call to-root-path,.) && pwd)
KERNEL_TO_ROOT_PATH  ?= $(call to-root-path,$(KERNEL_SRC))
WIFI_SUPPORT_DRIVERS ?= $(EXTRA_WIFI_SUPPORT_DRIVERS)
####################################################################################

WIFI_SUPPORT_DRIVERS += dhd_sdio
dhd_sdio_build ?= true
dhd_sdio_modules ?= ap6181 ap6335 ap6234 ap6255 ap6256 ap6271 ap6212 ap6354 ap6356 ap6398s ap6275s bcm43751_s bcm43458_s bcm4358_s
dhd_sdio_src_path ?= $(DRIVER_DIR)/broadcom/ap6xxx/bcmdhd.101.10.361.x
dhd_sdio_copy_path ?= $(OUT_DIR)/$(KERNEL_TO_ROOT_PATH)/$(strip $(dhd_sdio_src_path))/dhd_sdio
dhd_sdio_build_path ?=
dhd_sdio_args ?= CONFIG_BCMDHD_SDIO=y
ifeq ($(dhd_sdio_build),true)
WIFI_BUILT_MODULES += $(dhd_sdio_modules)
endif

WIFI_SUPPORT_DRIVERS += dhd_usb
dhd_usb_build ?= true
dhd_usb_modules ?= ap6269 ap62x8
dhd_usb_src_path ?= $(DRIVER_DIR)/broadcom/ap6xxx/bcmdhd.101.10.361.x
dhd_usb_copy_path ?= $(OUT_DIR)/$(KERNEL_TO_ROOT_PATH)/$(strip $(dhd_usb_src_path))/dhd_usb
dhd_usb_build_path ?=
dhd_usb_args ?= CONFIG_BCMDHD_USB=y
ifeq ($(dhd_usb_build),true)
WIFI_BUILT_MODULES += $(dhd_usb_modules)
endif

WIFI_SUPPORT_DRIVERS += dhd_pcie
dhd_pcie_build ?= true
dhd_pcie_modules ?= ap6275p ap6275hh3
dhd_pcie_src_path ?= $(DRIVER_DIR)/broadcom/ap6xxx/bcmdhd.101.10.361.x
dhd_pcie_copy_path ?= $(OUT_DIR)/$(KERNEL_TO_ROOT_PATH)/$(strip $(dhd_pcie_src_path))/dhd_pcie
dhd_pcie_build_path ?=
dhd_pcie_args ?= CONFIG_BCMDHD_PCIE=y
ifeq ($(dhd_pcie_build),true)
WIFI_BUILT_MODULES += $(dhd_pcie_modules)
endif

WIFI_SUPPORT_DRIVERS += qca6174
qca6174_build ?= true
qca6174_modules ?= qca6174
qca6174_src_path ?= $(DRIVER_DIR)/qualcomm/qca6174
qca6174_copy_path ?=
qca6174_build_path ?= AIO/build
qca6174_args ?=
ifeq ($(qca6174_build),true)
WIFI_BUILT_MODULES += $(qca6174_modules)
endif

WIFI_SUPPORT_DRIVERS += w1
w1_build ?= true
w1_modules ?= w1
w1_src_path ?= $(DRIVER_DIR)/amlogic/w1/wifi
w1_copy_path ?=
w1_build_path ?= project_w1/vmac
w1_args ?=
ifeq ($(w1_build),true)
WIFI_BUILT_MODULES += $(w1_modules)
endif

WIFI_SUPPORT_DRIVERS += rtl8723du
rtl8723du_build ?= true
rtl8723du_modules ?= rtl8723du
rtl8723du_src_path ?= $(DRIVER_DIR)/realtek/8723du
rtl8723du_copy_path ?=
rtl8723du_build_path ?= rtl8723DU
rtl8723du_args ?=
ifeq ($(rtl8723du_build),true)
WIFI_BUILT_MODULES += $(rtl8723du_modules)
endif

WIFI_SUPPORT_DRIVERS += rtl8723bu
rtl8723bu_build ?= true
rtl8723bu_modules ?= rtl8723bu
rtl8723bu_src_path ?= $(DRIVER_DIR)/realtek/8723bu
rtl8723bu_copy_path ?=
rtl8723bu_build_path ?= rtl8723BU
rtl8723bu_args ?=
ifeq ($(rtl8723bu_build),true)
WIFI_BUILT_MODULES += $(rtl8723bu_modules)
endif

WIFI_SUPPORT_DRIVERS += rtl8821cu
rtl8821cu_build ?= true
rtl8821cu_modules ?= rtl8821cu
rtl8821cu_src_path ?= $(DRIVER_DIR)/realtek/8821cu
rtl8821cu_copy_path ?=
rtl8821cu_build_path ?= rtl8821CU
rtl8821cu_args ?=
ifeq ($(rtl8821cu_build),true)
WIFI_BUILT_MODULES += $(rtl8821cu_modules)
endif

WIFI_SUPPORT_DRIVERS += rtl8822cu
rtl8822cu_build ?= true
rtl8822cu_modules ?= rtl8822cu
rtl8822cu_src_path ?= $(DRIVER_DIR)/realtek/8822cu
rtl8822cu_copy_path ?=
rtl8822cu_build_path ?= rtl88x2CU
rtl8822cu_args ?=
ifeq ($(rtl8822cu_build),true)
WIFI_BUILT_MODULES += $(rtl8822cu_modules)
endif

WIFI_SUPPORT_DRIVERS += rtl8822cs
rtl8822cs_build ?= true
rtl8822cs_modules ?= rtl8822cs
rtl8822cs_src_path ?= $(DRIVER_DIR)/realtek/8822cs
rtl8822cs_copy_path ?=
rtl8822cs_build_path ?= rtl88x2CS
rtl8822cs_args ?=
ifeq ($(rtl8822cs_build),true)
WIFI_BUILT_MODULES += $(rtl8822cs_modules)
endif

WIFI_SUPPORT_DRIVERS += rtl8821cs
rtl8821cs_build ?= true
rtl8821cs_modules ?= rtl8821cs
rtl8821cs_src_path ?= $(DRIVER_DIR)/realtek/8821cs
rtl8821cs_copy_path ?=
rtl8821cs_build_path ?= rtl8821CS
rtl8821cs_args ?=
ifeq ($(rtl8821cs_build),true)
WIFI_BUILT_MODULES += $(rtl8821cs_modules)
endif

WIFI_SUPPORT_DRIVERS += sd8987
sd8987_build ?= true
sd8987_modules ?= sd8987
sd8987_src_path ?= $(DRIVER_DIR)/marvell/sd8987
sd8987_copy_path ?=
sd8987_build_path ?=
sd8987_args ?= KERNELDIR=$(KERNEL_SRC)
ifeq ($(sd8987_build),true)
WIFI_BUILT_MODULES += $(sd8987_modules)
endif

########################################################
#mtk wifi driver start
########################################################
WIFI_SUPPORT_DRIVERS += mt7661
mt7661_build ?= true
mt7661_modules ?= mt7661
mt7661_src_path ?= $(DRIVER_DIR)/mtk/drivers/mt7661
mt7661_copy_path ?=
mt7661_build_path ?= wlan_driver/gen4m
mt7661_args ?=
ifeq ($(mt7661_build),true)
WIFI_BUILT_MODULES += $(mt7661_modules)
endif

WIFI_SUPPORT_DRIVERS += mt7668u
mt7668u_build ?= true
mt7668u_modules ?= mt7668u
mt7668u_src_path ?= $(DRIVER_DIR)/mtk/drivers/mt7668u
mt7668u_copy_path ?=
mt7668u_build_path ?=
mt7668u_args ?=
ifeq ($(mt7668u_build),true)
WIFI_BUILT_MODULES += $(mt7668u_modules)
endif

WIFI_SUPPORT_DRIVERS += mt7961_pcie
mt7961_pcie_build ?= true
mt7961_pcie_modules ?= mt7961_pcie
mt7961_pcie_src_path ?= $(DRIVER_DIR)/mtk/drivers/mt7961
mt7961_pcie_copy_path ?= $(OUT_DIR)/$(KERNEL_TO_ROOT_PATH)/$(strip $(mt7961_pcie_src_path))/mt7961_pcie
mt7961_pcie_build_path ?= wlan_driver/gen4m
mt7961_pcie_args ?= HIF=pcie

WIFI_SUPPORT_DRIVERS += mt7961_usb
mt7961_usb_build ?= true
mt7961_usb_modules ?= mt7961_usb
mt7961_usb_src_path ?= $(DRIVER_DIR)/mtk/drivers/mt7961
mt7961_usb_copy_path ?= $(OUT_DIR)/$(KERNEL_TO_ROOT_PATH)/$(strip $(mt7961_pcie_src_path))/mt7961_usb
mt7961_usb_build_path ?= wlan_driver/gen4m
mt7961_usb_args ?= HIF=usb

########################################################
#mtk wifi driver end
########################################################

WIFI_SUPPORT_DRIVERS += uwe5621ds
uwe5621ds_build ?= true
uwe5621ds_modules ?= uwe5621ds
uwe5621ds_src_path ?= $(DRIVER_DIR)/unisoc/uwe5621
uwe5621ds_copy_path ?=
uwe5621ds_build_path ?=
uwe5621ds_args ?=
ifeq ($(uwe5621ds_build),true)
WIFI_BUILT_MODULES += $(uwe5621ds_modules)
endif

