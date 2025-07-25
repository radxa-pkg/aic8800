EXTRA_CFLAGS += $(USER_EXTRA_CFLAGS)
EXTRA_CFLAGS += -Wno-implicit-fallthrough
EXTRA_CFLAGS += -Wno-int-to-pointer-cast
EXTRA_CFLAGS += -Wno-pointer-to-int-cast
#EXTRA_CFLAGS += -Wno-unused-function
#EXTRA_CFLAGS += -Wno-maybe-uninitialized
#EXTRA_CFLAGS += -Wno-unused-variable

RWNX_VERS_NUM := 6.4.3.0

CONFIG_COUNTRY_CODE = "00"


MODULE_NAME = aic8800D80_fdrv
CONFIG_AIC8800_WLAN_SUPPORT = m

# Support of bootrom start
CONFIG_START_FROM_BOOTROM = y

# Support of pmic setting, new version bootrom avaliable
CONFIG_PMIC_SETTING ?=n

# Select 8800DC/DW DCDC_VRF mode, check your board
CONFIG_VRF_DCDC_MODE = y

CONFIG_GKI_OPT_FEATURES ?= n

# ROM patch enabled option
CONFIG_ROM_PATCH_EN ?=n
# Support chip with mcu
CONFIG_MCU_INTEGRATED ?= n
CONFIG_MCU_MESSAGE ?= n
ifeq ($(CONFIG_MCU_INTEGRATED), y)
CONFIG_PMIC_SETTING = n
else
CONFIG_MCU_MESSAGE ?= n
endif

#
# WAITING FOR KCONFIG {
#
CONFIG_RWNX_FULLMAC ?= y
CONFIG_RWNX_FHOST ?= n

#
# DEBUG OPTIONS
CONFIG_RWNX_UM_HELPER_DFLT ?= "/dini/dini_bin/rwnx_umh.sh"

#
# FW ARCH:
CONFIG_RWNX_SDM ?= n
CONFIG_RWNX_TL4 ?= n

# IPC version
CONFIG_RWNX_OLD_IPC ?= n

# Support of P2P DebugFS for enabling/disabling NoA and OppPS
CONFIG_RWNX_P2P_DEBUGFS := n
#
# } // WAITING FOR KCONFIG
#

# Enable A-MSDU support (need FW support)
## Select this if FW is compiled with AMSDU support
CONFIG_RWNX_SPLIT_TX_BUF = y
## Select this TO send AMSDU
CONFIG_RWNX_AMSDUS_TX = n

# Enable BFMER support (need FW support)
CONFIG_RWNX_BFMER ?= n

CONFIG_SDIO_SUPPORT =n
CONFIG_USB_SUPPORT =n
CONFIG_PCIE_SUPPORT =y
CONFIG_HSTMSI_SUPPORT =y
CONFIG_RX_REORDER ?=y
CONFIG_ARP_OFFLOAD =y
CONFIG_RADAR_OR_IR_DETECT =n
CONFIG_DOWNLOAD_FW =y
CONFIG_RFTEST=y
CONFIG_USB_BT = n
CONFIG_USE_BT = y
CONFIG_WS = n
CONFIG_USE_5G = y
CONFIG_SDIO_PWRCTRL ?= n
CONFIG_CREATE_TRACE_POINTS = n
CONFIG_TXRX_THREAD_PRIO = n
# CONFIG_COEX = n for BT_ONLY, CONFIG_COEX =y for combo and sw
CONFIG_COEX = y
CONFIG_RX_NETIF_RECV_SKB = y
CONFIG_GPIO_WAKEUP = n
CONFIG_SET_VENDOR_EXTENSION_IE = n
CONFIG_SUPPORT_REALTIME_CHANGE_MAC = y
CONFIG_WPA3_FOR_OLD_KERNEL ?= n
CONFIG_VHT_FOR_OLD_KERNEL ?= n
CONFIG_HE_FOR_OLD_KERNEL ?= n
CONFIG_PREALLOC_RX_SKB = n
CONFIG_WIFI_SUSPEND_FOR_LINUX = n
# Need to set fw path in BOARD_KERNEL_CMDLINE
CONFIG_USE_FW_REQUEST = n
CONFIG_USE_P2P0=n
CONFIG_BR_SUPPORT =n
BR_NAME = br0
CONFIG_FILTER_TCP_ACK =y
CONFIG_ONE_TXQ = y
CONFIG_PREALLOC_TXQ = n
CONFIG_RFKILL_POLL = n
CONFIG_TEMP_COMP = n
CONFIG_TEMP_CONTROL = n
CONFIG_DYNAMIC_PWR = n
CONFIG_POWER_LIMIT = n
CONFIG_FLASH_CALRES = n
CONFIG_RX_SKBLIST =y
CONFIG_FOR_IPCAM = n
CONFIG_LOWPOWER = n
CONFIG_NO_FIRMWARE_RELOAD = n
CONFIG_RF_CALIB_LATER = n
CONFIG_RX_TASKLET = y
CONFIG_FWLOG_REDIR = n
CONFIG_DELAYED_BA = n
CONFIG_DYNAMIC_PERPWR = n
CONFIG_BTDM_LOG = n
CONFIG_CACHE_GUARD = y
CONFIG_LOAD_BT_CONF = n
CONFIG_UPG_FLASH = n

# Support of MU-MIMO transmission (need FW support)
ifeq ($(CONFIG_RWNX_BFMER), y)
CONFIG_RWNX_MUMIMO_TX ?= n
else
CONFIG_RWNX_MUMIMO_TX = n
endif

# Enable handling of radar event
CONFIG_RWNX_RADAR ?= y

# Enable HW queue for Broadcast/Multicast traffic (need FW support)
CONFIG_RWNX_BCMC ?= y

# Enable Monitor+Data interface support (need FW support)
CONFIG_RWNX_MON_DATA =y

# extra DEBUG config
CONFIG_RWNX_SW_PROFILING ?= n
CONFIG_RWNX_DBG ?= y
CONFIG_DEBUG_FS ?= n

ifeq ($(CONFIG_USB_BT), y)
CONFIG_USE_BT = y
endif


obj-$(CONFIG_AIC8800_WLAN_SUPPORT) := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
	rwnx_gki.o             \
	rwnx_msg_tx.o          \
	rwnx_msg_rx.o          \
	rwnx_utils.o           \
	rwnx_cmds.o            \
	rwnx_irqs.o            \
	rwnx_cfgfile.o         \
	rwnx_strs.o            \
	rwnx_rx.o              \
	rwnx_tx.o              \
	rwnx_txq.o             \
	rwnx_main.o            \
	rwnx_mod_params.o      \
	rwnx_mesh.o            \
	rwnx_platform.o        \
	rwnx_pci.o             \
	rwnx_dini.o            \
	rwnx_v7.o              \
	ipc_host.o             \
	rwnx_tdls.o            \
	aic_vendor.o	       \
	md5.o                  \
	aicwf_compat_8800dc.o  \
	aicwf_compat_8800d80.o \
	regdb.o		\
	aicwf_rx_prealloc.o	\
	aic_priv_cmd.o

$(MODULE_NAME)-$(CONFIG_BR_SUPPORT)       += aic_br_ext.o
$(MODULE_NAME)-$(CONFIG_RWNX_RADAR)       += rwnx_radar.o
$(MODULE_NAME)-$(CONFIG_DEBUG_FS)         += rwnx_debugfs.o
$(MODULE_NAME)-$(CONFIG_DEBUG_FS)         += rwnx_fw_trace.o
$(MODULE_NAME)-$(CONFIG_NL80211_TESTMODE) += rwnx_testmode.o
$(MODULE_NAME)-$(CONFIG_RWNX_BFMER)       += rwnx_bfmer.o
$(MODULE_NAME)-$(CONFIG_RWNX_MUMIMO_TX)   += rwnx_mu_group.o
$(MODULE_NAME)-$(CONFIG_SDIO_SUPPORT)     += sdio_host.o
$(MODULE_NAME)-$(CONFIG_SDIO_SUPPORT)     += aicwf_txrxif.o
$(MODULE_NAME)-$(CONFIG_SDIO_SUPPORT)     += aicwf_sdio.o
$(MODULE_NAME)-$(CONFIG_FILTER_TCP_ACK)   += aicwf_tcp_ack.o

$(MODULE_NAME)-$(CONFIG_USB_SUPPORT)     += usb_host.o
$(MODULE_NAME)-$(CONFIG_USB_SUPPORT)     += aicwf_txrxif.o
$(MODULE_NAME)-$(CONFIG_USB_SUPPORT)     += aicwf_usb.o

$(MODULE_NAME)-$(CONFIG_PCIE_SUPPORT)     += aicwf_pcie.o
$(MODULE_NAME)-$(CONFIG_PCIE_SUPPORT)     += aicwf_txrxif.o
$(MODULE_NAME)-$(CONFIG_PCIE_SUPPORT)     += pcie_host.o
$(MODULE_NAME)-$(CONFIG_PCIE_SUPPORT)     += aicwf_pcie_api.o
$(MODULE_NAME)-$(CONFIG_USE_BT)           += aicwf_bt_init.o

ccflags-$(CONFIG_DEBUG_FS) += -DCONFIG_RWNX_DEBUGFS
ccflags-$(CONFIG_DEBUG_FS) += -DCONFIG_RWNX_UM_HELPER_DFLT=\"$(CONFIG_RWNX_UM_HELPER_DFLT)\"
ccflags-$(CONFIG_RWNX_P2P_DEBUGFS) += -DCONFIG_RWNX_P2P_DEBUGFS

# FW VARS
ccflags-y += -DNX_VIRT_DEV_MAX=4

#for 8800D and DCDW u01
#ccflags-y += -DNX_REMOTE_STA_MAX=10

#for 8800DCDW u02
ccflags-y += -DNX_REMOTE_STA_MAX_FOR_OLD_IC=32
ccflags-y += -DNX_REMOTE_STA_MAX=32

ccflags-y += -DNX_MU_GROUP_MAX=62
ccflags-y += -DNX_TXDESC_CNT=64
ccflags-y += -DNX_TX_MAX_RATES=4
ccflags-y += -DNX_CHAN_CTXT_CNT=3

ccflags-y += -DFLASH_SET_GEN2=0

# FW ARCH:
ccflags-$(CONFIG_RWNX_SDM) += -DCONFIG_RWNX_SDM
ccflags-$(CONFIG_RWNX_TL4) += -DCONFIG_RWNX_TL4
ccflags-$(CONFIG_RWNX_OLD_IPC) += -DCONFIG_RWNX_OLD_IPC
ccflags-$(CONFIG_PLATFORM_NANOPI_M4) += -DCONFIG_NANOPI_M4
ccflags-$(CONFIG_PLATFORM_INGENIC_T20) += -DCONFIG_INGENIC_T20
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
ccflags-$(CONFIG_START_FROM_BOOTROM) += -DCONFIG_START_FROM_BOOTROM
ccflags-$(CONFIG_PMIC_SETTING) += -DCONFIG_PMIC_SETTING
ccflags-$(CONFIG_VRF_DCDC_MODE) += -DCONFIG_VRF_DCDC_MODE
ccflags-$(CONFIG_ROM_PATCH_EN) += -DCONFIG_ROM_PATCH_EN
ccflags-$(CONFIG_HE_FOR_OLD_KERNEL) += -DCONFIG_HE_FOR_OLD_KERNEL
ccflags-$(CONFIG_MCU_INTEGRATED) += -DCONFIG_MCU_INTEGRATED
ccflags-$(CONFIG_MCU_MESSAGE) += -DCONFIG_MCU_MESSAGE
ccflags-$(CONFIG_COEX) += -DCONFIG_COEX
ccflags-$(CONFIG_RX_SKBLIST) += -DCONFIG_RX_SKBLIST
ccflags-$(CONFIG_RX_TASKLET) += -DCONFIG_RX_TASKLET

ccflags-y += -DCONFIG_RWNX_FULLMAC
ccflags-y += -I$(srctree)
ccflags-y += -I$(srctree)/$(src)
ccflags-y += -I$(srctree)/$(src)/../aic8800_bsp
ccflags-y += -DCONFIG_AIC_FW_PATH=\"$(CONFIG_AIC_FW_PATH)\"
ccflags-$(CONFIG_RWNX_RADAR) += -DCONFIG_RWNX_RADAR
ccflags-$(CONFIG_RWNX_MON_DATA) += -DCONFIG_RWNX_MON_DATA
ccflags-$(CONFIG_RWNX_BFMER) += -DCONFIG_RWNX_BFMER
ccflags-$(CONFIG_RWNX_SPLIT_TX_BUF) += -DCONFIG_RWNX_SPLIT_TX_BUF
ifeq ($(CONFIG_RWNX_SPLIT_TX_BUF), y)
ccflags-$(CONFIG_RWNX_AMSDUS_TX) += -DCONFIG_RWNX_AMSDUS_TX
endif
ccflags-$(CONFIG_RWNX_DBG) += -DCONFIG_RWNX_DBG
ccflags-$(CONFIG_RWNX_SW_PROFILING) += -DCONFIG_RWNX_SW_PROFILING
ccflags-$(CONFIG_RWNX_MUMIMO_TX) += -DCONFIG_RWNX_MUMIMO_TX
ccflags-$(CONFIG_RFTEST) += -DCONFIG_RFTEST
ccflags-y += -DDEFAULT_COUNTRY_CODE=""\$(CONFIG_COUNTRY_CODE)"\"
ccflags-$(CONFIG_USE_5G) += -DUSE_5G
ccflags-$(CONFIG_WS) += -DCONFIG_WS
ccflags-$(CONFIG_CREATE_TRACE_POINTS) += -DCREATE_TRACE_POINTS
ccflags-$(CONFIG_TXRX_THREAD_PRIO) += -DCONFIG_TXRX_THREAD_PRIO
ccflags-$(CONFIG_GPIO_WAKEUP) += -DCONFIG_GPIO_WAKEUP
ccflags-$(CONFIG_SET_VENDOR_EXTENSION_IE) += -DCONFIG_SET_VENDOR_EXTENSION_IE
ccflags-$(CONFIG_SUPPORT_REALTIME_CHANGE_MAC) += -DCONFIG_SUPPORT_REALTIME_CHANGE_MAC
ccflags-$(CONFIG_WPA3_FOR_OLD_KERNEL) += -DCONFIG_WPA3_FOR_OLD_KERNEL
ccflags-$(CONFIG_VHT_FOR_OLD_KERNEL) += -DCONFIG_VHT_FOR_OLD_KERNEL
ccflags-$(CONFIG_PREALLOC_RX_SKB) += -DCONFIG_PREALLOC_RX_SKB
ccflags-$(CONFIG_WIFI_SUSPEND_FOR_LINUX) += -DCONFIG_WIFI_SUSPEND_FOR_LINUX
ccflags-$(CONFIG_USE_FW_REQUEST) += -DCONFIG_USE_FW_REQUEST
ccflags-$(CONFIG_USE_P2P0) += -DCONFIG_USE_P2P0
ccflags-$(CONFIG_FILTER_TCP_ACK) += -DCONFIG_FILTER_TCP_ACK
ccflags-$(CONFIG_ONE_TXQ)  += -DCONFIG_ONE_TXQ
ccflags-$(CONFIG_PREALLOC_TXQ) += -DCONFIG_PREALLOC_TXQ
ccflags-$(CONFIG_RFKILL_POLL)  += -DCONFIG_RFKILL_POLL

ccflags-$(CONFIG_TEMP_COMP) += -DCONFIG_TEMP_COMP
ccflags-$(CONFIG_TEMP_CONTROL) += -DCONFIG_TEMP_CONTROL
ccflags-$(CONFIG_POWER_LIMIT)  += -DCONFIG_POWER_LIMIT
ccflags-$(CONFIG_FLASH_CALRES) += -DCONFIG_FLASH_CALRES
ccflags-$(CONFIG_FOR_IPCAM)  += -DCONFIG_FOR_IPCAM
ccflags-$(CONFIG_LOWPOWER) += -DCONFIG_LOWPOWER
ccflags-$(CONFIG_NO_FIRMWARE_RELOAD) += -DCONFIG_NO_FIRMWARE_RELOAD
ccflags-$(CONFIG_RF_CALIB_LATER) += -DCONFIG_RF_CALIB_LATER
ccflags-$(CONFIG_FWLOG_REDIR) += -DCONFIG_FWLOG_REDIR
ccflags-$(CONFIG_DELAYED_BA) += -DCONFIG_DELAYED_BA
ccflags-$(CONFIG_DYNAMIC_PERPWR) += -DCONFIG_DYNAMIC_PERPWR

ccflags-$(CONFIG_GKI_OPT_FEATURES) += -DCONFIG_GKI_OPT_FEATURES

ccflags-$(CONFIG_DYNAMIC_PWR) += -DCONFIG_DYNAMIC_PWR
ccflags-$(CONFIG_BTDM_LOG) += -DCONFIG_BTDM_LOG
ccflags-$(CONFIG_CACHE_GUARD) += -DCONFIG_CACHE_GUARD
ccflags-$(CONFIG_LOAD_BT_CONF) += -DCONFIG_LOAD_BT_CONF
ccflags-$(CONFIG_UPG_FLASH) += -DCONFIG_UPG_FLASH

ifeq ($(CONFIG_SDIO_SUPPORT), y)
ccflags-y += -DAICWF_SDIO_SUPPORT
ccflags-$(CONFIG_SDIO_PWRCTRL) += -DCONFIG_SDIO_PWRCTRL
endif

ifeq ($(CONFIG_USB_SUPPORT), y)
ccflags-y += -DAICWF_USB_SUPPORT
endif

ifeq ($(CONFIG_PCIE_SUPPORT), y)
ccflags-y += -DAICWF_PCIE_SUPPORT
endif

ifeq ($(CONFIG_HSTMSI_SUPPORT), y)
ccflags-y += -DCONFIG_HSTMSI_SUPPORT
endif

ifeq ($(CONFIG_BR_SUPPORT), y)
ccflags-y += -DCONFIG_BR_SUPPORT
ccflags-y += '-DCONFIG_BR_SUPPORT_BRNAME="'$(BR_NAME)'"'
endif

ifeq ($(CONFIG_RWNX_MUMIMO_TX), y)
ccflags-y += -DCONFIG_USER_MAX=2
else
ccflags-y += -DCONFIG_USER_MAX=1
endif

ifeq ($(CONFIG_RWNX_BCMC), y)
ccflags-y += -DNX_TXQ_CNT=5
else
ccflags-y += -DNX_TXQ_CNT=4
endif

# For old kernel (<=3.19)
ifeq ($(shell test $(VERSION) -lt 4 -a "$(CONFIG_VENDOR_RWNX)" = y ; echo $$?),0)
ccflags-y += -DCONFIG_VENDOR_RWNX_VHT_NO80
endif

ccflags-$(CONFIG_RX_REORDER) += -DAICWF_RX_REORDER
ccflags-$(CONFIG_ARP_OFFLOAD) += -DAICWF_ARP_OFFLOAD
ccflags-$(CONFIG_RADAR_OR_IR_DETECT) += -DCONFIG_RADAR_OR_IR_DETECT
ccflags-$(CONFIG_DOWNLOAD_FW)  += -DCONFIG_DOWNLOAD_FW
ccflags-$(CONFIG_RX_NETIF_RECV_SKB) += -DCONFIG_RX_NETIF_RECV_SKB
ccflags-$(CONFIG_USB_BT) += -DCONFIG_USB_BT
ccflags-$(CONFIG_USE_BT) += -DCONFIG_USE_BT

# Platform support list
CONFIG_PLATFORM_ROCKCHIP ?= n
CONFIG_PLATFORM_ALLWINNER ?= n
CONFIG_PLATFORM_INGENIC_T20 ?= n
CONFIG_PLATFORM_AMLOGIC ?= n
CONFIG_PLATFORM_UBUNTU ?= y

ifeq ($(CONFIG_PLATFORM_ROCKCHIP), y)
#KDIR ?= /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/kernel
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-
#KDIR  ?= /home/yaya/E/Rockchip/3399/rk3399-android-10/kernel
#ARCH ?= arm64
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3399/rk3399-android-10/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
#KDIR  ?= /home/yaya/E/Rockchip/3288/Android10/kernel/kernel/
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3288/Android10/tool/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR  ?= /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/kernel
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR ?= /home/yaya/E/Rockchip/3566/Android/kernel
#ARCH ?= arm64
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3566/Android/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
#KDIR ?= /home/yaya/E/Rockchip/3328/Android9/SDK/kernel/
#ARCH ?= arm64
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3328/Android9/SDK/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
#KDIR ?= /home/yaya/E/Rockchip/3566/oudu/kernel
KDIR ?= ~/E/Rockchip/3566/Android11/rk3566_rk3568_android11_oranth/kernel
#KDIR ?= /home/yaya/E/Rockchip/3566/shengteng/kernel
ARCH ?= arm64
CROSS_COMPILE ?= ~/E/Rockchip/3566/Android11/rk3566_rk3568_android11_oranth/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
ccflags-$(CONFIG_PLATFORM_ROCKCHIP) += -DCONFIG_PLATFORM_ROCKCHIP
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
ccflags-$(CONFIG_PLATFORM_ALLWINNER) += -DCONFIG_PLATFORM_ALLWINNER
ccflags-y += -DANDROID_PLATFORM
KDIR  ?= /home/yaya/E/Allwinner/r818/Android10/lichee/kernel/linux-4.9/
ARCH ?= arm64
CROSS_COMPILE ?= /home/yaya/E/Allwinner/r818/Android10/android/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
endif

ifeq ($(CONFIG_PLATFORM_INGENIC_T20), y)
KDIR ?= /home/yaya/E/T40/kernel
ARCH ?= mips
CROSS_COMPILE ?= /home/yaya/E/T40/mips-linux-gnu-ingenic-gcc7.2.0-glibc2.29-fp64/bin/mips-linux-gnu-
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
ccflags-$(CONFIG_PLATFORM_AMLOGIC) += -DCONFIG_PLATFORM_AMLOGIC
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
KDIR  ?= /lib/modules/$(shell uname -r)/build
PWD   ?= $(shell pwd)
KVER ?= $(shell uname -r)
MODDESTDIR ?= /lib/modules/$(KVER)/kernel/drivers/net/wireless/aic8800D80/
ARCH ?= x86_64
CROSS_COMPILE ?=
endif


all: modules
modules:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

install:
	mkdir -p $(MODDESTDIR)
	install -p -m 644 $(MODULE_NAME).ko  $(MODDESTDIR)
	/sbin/depmod -a ${KVER}
	echo $(MODULE_NAME) >> /etc/modules-load.d/aic.conf

uninstall:
	rm -rfv $(MODDESTDIR)/$(MODULE_NAME).ko
	/sbin/depmod -a ${KVER}
	rm -f /etc/modules-load.d/aic.conf

clean:
	rm -rf *.o *.ko *.o.* *.mod.* modules.* Module.* .a* .o* .*.o.* *.mod .tmp* .cache.mk


