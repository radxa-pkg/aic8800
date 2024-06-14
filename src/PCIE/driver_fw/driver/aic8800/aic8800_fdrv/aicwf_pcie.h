
#ifndef __AICWF_PCIE_H
#define __AICWF_PCIE_H

#include <linux/pci.h>
#include "aicwf_txrxif.h"
#ifdef AICWF_PCIE_SUPPORT

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
