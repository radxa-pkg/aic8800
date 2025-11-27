
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

#ifdef CONFIG_TEMP_CONTROL
#define TEMP_GET_INTERVAL                (10 * 1000)
#define TEMP_THD_1                       65  //temperature 1
#define TEMP_THD_2                       85 //temperature 2
#define BUFFERING_V1                     3
#define BUFFERING_V2                	 5
#define TMR_INTERVAL_1                   3	//timer_1 3ms
#define TMR_INTERVAL_2                   5 	//timer_2 5ms
#endif

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
    spinlock_t irq_lock;

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

    uint32_t txc_cnt;
    uint32_t rxc_cnt;

#ifdef CONFIG_TEMP_CONTROL
	spinlock_t tx_flow_lock;
	struct timer_list netif_timer;
	struct timer_list tp_ctrl_timer;
	struct work_struct tp_ctrl_work;
	struct work_struct netif_work;
	spinlock_t tm_lock;
	s8_l cur_temp;
	bool net_stop;
	bool on_off;	  //for command, 0 - off, 1 - on
	int8_t get_level; //for command, 0 - 100%, 1 - 12%, 2 - 3%
	int8_t set_level; //for command, 0 - driver auto, 1 - 12%, 2 - 3%
	int interval_t1;
	int interval_t2;
	u8_l cur_stat;	  //0--normal temp, 1/2--buffering temp
	s8_l tp_thd_1; // temperature threshold 1
	s8_l tp_thd_2; // temperature threshold 2
	int8_t tm_start; //timer start flag
#endif
};

#ifdef CONFIG_TEMP_CONTROL
void aicwf_netif_worker(struct work_struct *work);
void aicwf_temp_ctrl_worker(struct work_struct *work);
void aicwf_temp_ctrl(struct aic_pci_dev *pcidev);
void aicwf_netif_ctrl(struct aic_pci_dev *pcidev, int val);
#endif

int aicwf_pcie_register_drv(void);
void aicwf_pcie_unregister_drv(void);
int aicwf_pcie_bus_init(struct aic_pci_dev *pciedev);
int rwnx_plat_bin_fw_upload_2(struct rwnx_hw *rwnx_hw, u32 fw_addr, char *filename);
int patch_config(struct rwnx_hw *rwnx_hw);
int pcie_reset_firmware(struct rwnx_hw *rwnx_hw, u32 fw_addr);
int pcie_rxbuf_rep_thread(void *data);
int pcie_rxbuf_process_thread(void *data);
int pcie_irq_process_thread(void *data);
#ifdef CONFIG_TX_THREAD
int pcie_txbuf_process_thread(void *data);
#endif

#ifdef CONFIG_WS
void rwnx_pm_stay_awake_pc(struct rwnx_hw *rwnx_hw);
void rwnx_pm_relax_pc(struct rwnx_hw *rwnx_hw);
#endif

#endif
void aicwf_pcie_vif_down_db(struct aic_pci_dev *pciedev);
irqreturn_t aicwf_pcie_irq_hdlr(int irq, void *dev_id);

#endif /* __AICWF_PCIE_H */
