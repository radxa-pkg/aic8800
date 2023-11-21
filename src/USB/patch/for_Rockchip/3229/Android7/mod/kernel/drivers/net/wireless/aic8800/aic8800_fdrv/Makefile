RWNX_VERS_NUM := 6.4.3.0

CONFIG_AIC8800_WLAN_SUPPORT = m
MODULE_NAME = aic8800_fdrv
CONFIG_COUNTRY_CODE = "00"

# Support of bootrom start
CONFIG_START_FROM_BOOTROM = n

# Support of pmic setting, new version bootrom avaliable
CONFIG_PMIC_SETTING ?=n

# ROM patch enabled option
CONFIG_ROM_PATCH_EN ?=n

#
# WAITING FOR KCONFIG {
#
CONFIG_RWNX_SOFTMAC ?= n
CONFIG_RWNX_FULLMAC ?= y
CONFIG_RWNX_FHOST ?= n

#
# DEBUG OPTIONS
CONFIG_RWNX_UM_HELPER_DFLT ?= "/dini/dini_bin/rwnx_umh.sh"
CONFIG_AIC_FW_PATH = "/vendor/etc/firmware"
export CONFIG_AIC_FW_PATH

#
# FW ARCH:
CONFIG_RWNX_SDM ?= n
CONFIG_RWNX_TL4 ?= n

# IPC version
CONFIG_RWNX_OLD_IPC ?= n

# Support of P2P DebugFS for enabling/disabling NoA and OppPS
CONFIG_RWNX_P2P_DEBUGFS ?= y

#
# } // WAITING FOR KCONFIG
#

# Enable A-MSDU support (need FW support)
## Select this if FW is compiled with AMSDU support
CONFIG_RWNX_SPLIT_TX_BUF ?= n
## Select this TO send AMSDU
CONFIG_RWNX_AMSDUS_TX ?= n

# Enable BFMER support (need FW support)
CONFIG_RWNX_BFMER ?= n

CONFIG_SDIO_SUPPORT =n
CONFIG_USB_SUPPORT =y
CONFIG_RX_REORDER ?=y
CONFIG_ARP_OFFLOAD =y
CONFIG_USE_5G ?= n
CONFIG_RADAR_OR_IR_DETECT =n
CONFIG_DOWNLOAD_FW =n
CONFIG_RFTEST=y
CONFIG_USB_BT=y
CONFIG_MAC_RANDOM_IF_NO_MAC_IN_EFUSE = n
CONFIG_HE_FOR_OLD_KERNEL ?= y
CONFIG_COEX = y
CONFIG_ALIGN_8BYTES = y
CONFIG_TXRX_THREAD_PRIO = n
CONFIG_USB_ALIGN_DATA = y
CONFIG_RX_TASKLET = n
CONFIG_RX_NETIF_RECV_SKB = n


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

obj-$(CONFIG_AIC8800_WLAN_SUPPORT) := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
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
        regdb.o

$(MODULE_NAME)-$(CONFIG_RWNX_RADAR)       += rwnx_radar.o
$(MODULE_NAME)-$(CONFIG_DEBUG_FS)         += rwnx_debugfs.o
$(MODULE_NAME)-$(CONFIG_DEBUG_FS)         += rwnx_fw_dump.o
$(MODULE_NAME)-$(CONFIG_DEBUG_FS)         += rwnx_fw_trace.o
$(MODULE_NAME)-$(CONFIG_NL80211_TESTMODE) += rwnx_testmode.o
$(MODULE_NAME)-$(CONFIG_RWNX_BFMER)       += rwnx_bfmer.o
$(MODULE_NAME)-$(CONFIG_RWNX_MUMIMO_TX)   += rwnx_mu_group.o
$(MODULE_NAME)-$(CONFIG_SDIO_SUPPORT)     += sdio_host.o
$(MODULE_NAME)-$(CONFIG_SDIO_SUPPORT)     += aicwf_txrxif.o
$(MODULE_NAME)-$(CONFIG_SDIO_SUPPORT)     += aicwf_sdio.o

$(MODULE_NAME)-$(CONFIG_USB_SUPPORT)     += usb_host.o
$(MODULE_NAME)-$(CONFIG_USB_SUPPORT)     += aicwf_txrxif.o
$(MODULE_NAME)-$(CONFIG_USB_SUPPORT)     += aicwf_usb.o

ccflags-$(CONFIG_DEBUG_FS) += -DCONFIG_RWNX_DEBUGFS
ccflags-$(CONFIG_DEBUG_FS) += -DCONFIG_RWNX_UM_HELPER_DFLT=\"$(CONFIG_RWNX_UM_HELPER_DFLT)\"
ccflags-$(CONFIG_RWNX_P2P_DEBUGFS) += -DCONFIG_RWNX_P2P_DEBUGFS

# FW VARS
ccflags-y += -DNX_VIRT_DEV_MAX=4
ccflags-y += -DNX_REMOTE_STA_MAX=8
ccflags-y += -DNX_MU_GROUP_MAX=62
ccflags-y += -DNX_TXDESC_CNT=64
ccflags-y += -DNX_TX_MAX_RATES=4
ccflags-y += -DNX_CHAN_CTXT_CNT=3

# FW ARCH:
ccflags-$(CONFIG_RWNX_SDM) += -DCONFIG_RWNX_SDM
ccflags-$(CONFIG_RWNX_TL4) += -DCONFIG_RWNX_TL4
ccflags-$(CONFIG_RWNX_OLD_IPC) += -DCONFIG_RWNX_OLD_IPC
ccflags-$(CONFIG_START_FROM_BOOTROM) += -DCONFIG_START_FROM_BOOTROM
ccflags-$(CONFIG_PMIC_SETTING) += -DCONFIG_PMIC_SETTING
ccflags-$(CONFIG_ROM_PATCH_EN) += -DCONFIG_ROM_PATCH_EN
ccflags-$(CONFIG_HE_FOR_OLD_KERNEL) += -DCONFIG_HE_FOR_OLD_KERNEL
ccflags-$(CONFIG_COEX) += -DCONFIG_COEX
ccflags-$(CONFIG_RX_TASKLET) += -DCONFIG_RX_TASKLET

ccflags-y += -DCONFIG_RWNX_FULLMAC
ccflags-y += -I$(src)/.
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

ifeq ($(CONFIG_SDIO_SUPPORT), y)
ccflags-y += -DAICWF_SDIO_SUPPORT
endif

ifeq ($(CONFIG_USB_SUPPORT), y)
ccflags-y += -DAICWF_USB_SUPPORT
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
ccflags-$(CONFIG_USE_5G) += -DUSE_5G
ccflags-$(CONFIG_RADAR_OR_IR_DETECT) += -DRADAR_OR_IR_DETECT
ccflags-$(CONFIG_DOWNLOAD_FW)  += -DCONFIG_DOWNLOAD_FW
ccflags-$(CONFIG_USB_BT)  += -DCONFIG_USB_BT
ccflags-$(CONFIG_ALIGN_8BYTES)  += -DCONFIG_ALIGN_8BYTES
ccflags-$(CONFIG_TXRX_THREAD_PRIO) += -DCONFIG_TXRX_THREAD_PRIO
ccflags-$(CONFIG_USB_ALIGN_DATA) += -DCONFIG_USB_ALIGN_DATA
ccflags-$(CONFIG_MAC_RANDOM_IF_NO_MAC_IN_EFUSE) += -DCONFIG_MAC_RANDOM_IF_NO_MAC_IN_EFUSE
ccflags-y += -DDEFAULT_COUNTRY_CODE=""\$(CONFIG_COUNTRY_CODE)"\"
ccflags-$(CONFIG_RX_NETIF_RECV_SKB) += -DCONFIG_RX_NETIF_RECV_SKB


# Platform support list
CONFIG_PLATFORM_ROCHCHIP ?= y
CONFIG_PLATFORM_ALLWINNER ?= n
CONFIG_PLATFORM_AMLOGIC ?= n
CONFIG_PLATFORM_UBUNTU ?= n


ifeq ($(CONFIG_PLATFORM_ROCHCHIP), y)
KDIR := /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/kernel
ARCH ?= arm
CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android7/RK3229_ANDROID7.1_v1.01_20170914/rk3229_Android7.1_v1.01_xml0914/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
#KDIR  := /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/kernel
#ARCH ?= arm
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3229/Android9/rk3229_android9.0_box/prebuilts/gcc/linux-x86/arm/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
#KDIR  := /home/yaya/E/Rockchip/3399/rk3399-android-10/kernel
#ARCH ?= arm64
#CROSS_COMPILE ?= /home/yaya/E/Rockchip/3399/rk3399-android-10/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_ALLWINNER), y)
KDIR  := /home/yaya/E/Allwinner/R818/R818/AndroidQ/lichee/kernel/linux-4.9
ARCH ?= arm64
CROSS_COMPILE ?= /home/yaya/E/Allwinner/R818/R818/AndroidQ/lichee/out/gcc-linaro-5.3.1-2016.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
ccflags-y += -DANDROID_PLATFORM
endif

ifeq ($(CONFIG_PLATFORM_AMLOGIC), y)
ccflags-y += -DANDROID_PLATFORM
ARCH := arm
CROSS_COMPILE := /home/yaya/D/Workspace/CyberQuantum/JinHaoYue/amls905x3/SDK/20191101-0tt-asop/android9.0/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androidkernel-
KDIR := /home/yaya/D/Workspace/CyberQuantum/JinHaoYue/amls905x3/SDK/20191101-0tt-asop/android9.0/out/target/product/u202/obj/KERNEL_OBJ/

endif

ifeq ($(CONFIG_PLATFORM_UBUNTU), y)
#KDIR  := /lib/modules/$(shell uname -r)/build
KDIR  := /home/yaya/D/Workspace/CyberQuantum/AIC/Linux/linux-3.10/
#PWD   := $(shell pwd)
#KVER := $(shell uname -r)
#MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/aic8800
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

uninstall:
	rm -rfv $(MODDESTDIR)/$(MODULE_NAME).ko
	/sbin/depmod -a ${KVER}

clean:
	rm -rf *.o *.ko *.o.* *.mod.* modules.* Module.* .a* .o* .*.o.* *.mod .tmp* .cache.mk

