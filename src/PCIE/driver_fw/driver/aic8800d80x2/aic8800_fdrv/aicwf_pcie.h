
#ifndef __AICWF_PCIE_H
#define __AICWF_PCIE_H

#include <linux/pci.h>
#include "aicwf_txrxif.h"
#ifdef AICWF_PCIE_SUPPORT
#include "aicwf_pcie_api.h"

#define AIC8800D80_PCI_VENDOR_ID	0xa69c
#define AIC8800D80_PCI_DEVICE_ID	0x8d80
#define AIC8800D80X2_PCI_VENDOR_ID	0xa69c
#define AIC8800D80X2_PCI_DEVICE_ID	0x8d90

enum AICWF_IC{
	PRODUCT_ID_AIC8801	=	0,
	PRODUCT_ID_AIC8800DC,
	PRODUCT_ID_AIC8800DW,
	PRODUCT_ID_AIC8800D80,
	PRODUCT_ID_AIC8800D80X2
};

struct rwnx_hw;

struct aic_pci_dev {
    struct rwnx_hw *rwnx_hw;
	struct pci_dev *pci_dev;
	struct aicwf_bus *bus_if;
	struct aicwf_rx_priv *rx_priv;
	struct aicwf_tx_priv *tx_priv;
	struct rwnx_cmd_mgr cmd_mgr;

	u8 *pci_bar0_vaddr;
	u8 *pci_bar1_vaddr;
	u8 *pci_bar2_vaddr;
	u8 *pci_bar3_vaddr;

	u32 fw_version_uint;

	spinlock_t txmsg_lock;
	spinlock_t ws_lock;

	u8 chip_id;

	//for 8800d80x2
    u8 *emb_hdma ;
    u8 *emb_tpci ;
    u8 *emb_mbox ;
    u8 *emb_sctl ;
    u8 *emb_shrm ;
    
    //> only used in aic_pci_api
    struct pci_dev *pdev;

	u32 bar0 ;
	u32 len0 ;
    u8 *map0 ;

    atomic_t cnt_msi ;
};

int aicwf_pcie_register_drv(void);
void aicwf_pcie_unregister_drv(void);
void aicwf_pcie_bus_init(struct aic_pci_dev *pciedev);
int rwnx_plat_bin_fw_upload_2(struct rwnx_hw *rwnx_hw, u32 fw_addr, char *filename);
int patch_config(struct rwnx_hw *rwnx_hw);
int pcie_reset_firmware(struct rwnx_hw *rwnx_hw, u32 fw_addr);
int pcie_rxbuf_rep_thread(void *data);

#ifdef CONFIG_WS
void rwnx_pm_stay_awake_pc(struct rwnx_hw *rwnx_hw);
void rwnx_pm_relax_pc(struct rwnx_hw *rwnx_hw);
#endif

#endif

#endif /* __AICWF_PCIE_H */
