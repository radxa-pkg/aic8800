####################################################################################
DRIVER_DIR ?= vendor/wifi_driver
K_REL_DIR  ?= \
$(patsubst %/,%,\
 $(shell echo \
  $(foreach word,\
   $(shell echo \
    $(subst $(ROOT_DIR),,$(KERNEL_SRC))\
   | tr '/' ' '),\
  ../)\
 |sed 's/[[:space:]]//g')\
)
WIFI_SUPPORT_DRIVERS ?= $(EXTRA_WIFI_SUPPORT_DRIVERS)
####################################################################################

WIFI_SUPPORT_DRIVERS += dhd_sdio
dhd_sdio_build ?= true
dhd_sdio_modules ?= ap6181 ap6335 ap6234 ap6255 ap6271 ap6212 ap6354 ap6356 ap6398s ap6275s bcm43751_s bcm43458_s bcm4358_s
dhd_sdio_src_path ?= $(DRIVER_DIR)/broadcom/ap6xxx/bcmdhd.101.10.361.x
dhd_sdio_copy_path ?= $(OUT_DIR)/$(K_REL_DIR)/$(strip $(dhd_sdio_src_path))/dhd_sdio
dhd_sdio_build_path ?=
dhd_sdio_args ?= CONFIG_BCMDHD_SDIO=y

WIFI_SUPPORT_DRIVERS += dhd_usb
dhd_usb_build ?= true
dhd_usb_modules ?= ap6269 ap62x8
dhd_usb_src_path ?= $(DRIVER_DIR)/broadcom/ap6xxx/bcmdhd.101.10.361.x
dhd_usb_copy_path ?= $(OUT_DIR)/$(K_REL_DIR)/$(strip $(dhd_usb_src_path))/dhd_usb
dhd_usb_build_path ?=
dhd_usb_args ?= CONFIG_BCMDHD_USB=y

WIFI_SUPPORT_DRIVERS += dhd_pcie
dhd_pcie_build ?= true
dhd_pcie_modules ?= ap6275p ap6275hh3
dhd_pcie_src_path ?= $(DRIVER_DIR)/broadcom/ap6xxx/bcmdhd.101.10.361.x
dhd_pcie_copy_path ?= $(OUT_DIR)/$(K_REL_DIR)/$(strip $(dhd_pcie_src_path))/dhd_pcie
dhd_pcie_build_path ?=
dhd_pcie_args ?= CONFIG_BCMDHD_PCIE=y

WIFI_SUPPORT_DRIVERS += qca6174
qca6174_build ?= true
qca6174_modules ?= qca6174
qca6174_src_path ?= $(DRIVER_DIR)/qualcomm/qca6174
qca6174_copy_path ?=
qca6174_build_path ?= AIO/build
qca6174_args ?=

WIFI_SUPPORT_DRIVERS += w1
w1_build ?= true
w1_modules ?= w1
w1_src_path ?= $(DRIVER_DIR)/amlogic/w1/wifi
w1_copy_path ?=
w1_build_path ?= project_w1/vmac
w1_args ?=

WIFI_SUPPORT_DRIVERS += rtl8723du
rtl8723du_build ?= true
rtl8723du_modules ?= rtl8723du
rtl8723du_src_path ?= $(DRIVER_DIR)/realtek/8723du
rtl8723du_copy_path ?=
rtl8723du_build_path ?= rtl8723DU
rtl8723du_args ?=

WIFI_SUPPORT_DRIVERS += rtl8723bu
rtl8723bu_build ?= true
rtl8723bu_modules ?= rtl8723bu
rtl8723bu_src_path ?= $(DRIVER_DIR)/realtek/8723bu
rtl8723bu_copy_path ?=
rtl8723bu_build_path ?= rtl8723BU
rtl8723bu_args ?=

WIFI_SUPPORT_DRIVERS += rtl8821cu
rtl8821cu_build ?= true
rtl8821cu_modules ?= rtl8821cu
rtl8821cu_src_path ?= $(DRIVER_DIR)/realtek/8821cu
rtl8821cu_copy_path ?=
rtl8821cu_build_path ?= rtl8821CU
rtl8821cu_args ?=

WIFI_SUPPORT_DRIVERS += rtl8822cu
rtl8822cu_build ?= true
rtl8822cu_modules ?= rtl8822cu
rtl8822cu_src_path ?= $(DRIVER_DIR)/realtek/8822cu
rtl8822cu_copy_path ?=
rtl8822cu_build_path ?= rtl88x2CU
rtl8822cu_args ?=

WIFI_SUPPORT_DRIVERS += rtl8822cs
rtl8822cs_build ?= true
rtl8822cs_modules ?= rtl8822cs
rtl8822cs_src_path ?= $(DRIVER_DIR)/realtek/8822cs
rtl8822cs_copy_path ?=
rtl8822cs_build_path ?= rtl88x2CS
rtl8822cs_args ?=

WIFI_SUPPORT_DRIVERS += sd8987
sd8987_build ?= true
sd8987_modules ?= sd8987
sd8987_src_path ?= $(DRIVER_DIR)/marvell/sd8987
sd8987_copy_path ?=
sd8987_build_path ?=
sd8987_args ?=

WIFI_SUPPORT_DRIVERS += mt7661
mt7661_build ?= true
mt7661_modules ?= mt7661
mt7661_src_path ?= $(DRIVER_DIR)/mtk/drivers/mt7661
mt7661_copy_path ?=
mt7661_build_path ?= wlan_driver/gen4m
mt7661_args ?=

WIFI_SUPPORT_DRIVERS += mt7668u
mt7668u_build ?= true
mt7668u_modules ?= mt7668u
mt7668u_src_path ?= $(DRIVER_DIR)/mtk/drivers/mt7668u
mt7668u_copy_path ?=
mt7668u_build_path ?=
mt7668u_args ?=

