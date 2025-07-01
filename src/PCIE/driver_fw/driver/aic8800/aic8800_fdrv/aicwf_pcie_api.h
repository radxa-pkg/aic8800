#ifndef AIC_PCIE_API_H
#define AIC_PCIE_API_H

#include <linux/types.h>
#include "aicwf_pcie.h"

//#define AIC_PCIE_API_DELAY      1

#define AIC_MAP_HDMA_BASE_AIC8800D80         0x0000
#define AIC_MAP_HDMA_LIMT_AIC8800D80         0x0fff
#define AIC_MAP_HDMA_ADDR_AIC8800D80         0x40242000
#define AIC_MAP_TPCI_BASE_AIC8800D80         0x1000
#define AIC_MAP_TPCI_LIMT_AIC8800D80         0x1fff
#define AIC_MAP_TPCI_ADDR_AIC8800D80         0x40780000
#define AIC_MAP_MBOX_BASE_AIC8800D80         0x2000
#define AIC_MAP_MBOX_LIMT_AIC8800D80         0x2fff
#define AIC_MAP_MBOX_ADDR_AIC8800D80         0x40035000
#define AIC_MAP_SCTL_BASE_AIC8800D80         0x3000
#define AIC_MAP_SCTL_LIMT_AIC8800D80         0x3fff
#define AIC_MAP_SCTL_ADDR_AIC8800D80         0x40500000
#define AIC_MAP_SHRM_BASE_AIC8800D80         0x4000
#define AIC_MAP_SHRM_LIMT_AIC8800D80         0x7fff
#define AIC_MAP_SHRM_ADDR_AIC8800D80         0x001DC000

#define AIC_MAP_HDMA_BASE_AIC8800D80X2       0x0000
#define AIC_MAP_HDMA_LIMT_AIC8800D80X2       0x0fff
#define AIC_MAP_HDMA_ADDR_AIC8800D80X2       0x40242000
#define AIC_MAP_TPCI_BASE_AIC8800D80X2       0x1000
#define AIC_MAP_TPCI_LIMT_AIC8800D80X2       0x1fff
#define AIC_MAP_TPCI_ADDR_AIC8800D80X2       0x40780000
#define AIC_MAP_MBOX_BASE_AIC8800D80X2       0x2000
#define AIC_MAP_MBOX_LIMT_AIC8800D80X2       0x2fff
#define AIC_MAP_MBOX_ADDR_AIC8800D80X2       0x40035000
#define AIC_MAP_SCTL_BASE_AIC8800D80X2       0x3000
#define AIC_MAP_SCTL_LIMT_AIC8800D80X2       0x3fff
#define AIC_MAP_SCTL_ADDR_AIC8800D80X2       0x40500000
#define AIC_MAP_SHRM_BASE_AIC8800D80X2       0x8000
#define AIC_MAP_SHRM_LIMT_AIC8800D80X2       0xffff
#define AIC_MAP_SHRM_ADDR_AIC8800D80X2       0x00180000

//# for aic dma descriptor
#define AIC_MAP_SHRM_DMA_OFF_AIC8800D80      0x3C00
#define AIC_DMA_MPS_AIC8800D80               0x8000
#define AIC_MAP_SHRM_DMA_OFF_AIC8800D80X2    0x4000
#define AIC_DMA_MPS_AIC8800D80X2             0x8000
//# 
#define AIC_TRAN_DRV2EMB        0
#define AIC_TRAN_EMB2DRV        1
//# for aic test trans: share mem offset
#define AIC_TEST_TRAN_WLEN      0x0100
#define AIC_TEST_TRAN_OFF0      0x0000
#define AIC_TEST_TRAN_OFF1      0x0100

#ifndef PCI_L1SS_CTL1
#define PCI_L1SS_CTL1		0x08	/* Control 1 Register */
#endif

#ifndef PCI_L1SS_CTL1_ASPM_L1_1
#define  PCI_L1SS_CTL1_ASPM_L1_1	0x00000008  /* ASPM L1.1 Enable */
#endif

#ifndef PCI_L1SS_CTL1_ASPM_L1_2
#define  PCI_L1SS_CTL1_ASPM_L1_2	0x00000004  /* ASPM L1.2 Enable */
#endif

#if 0
struct aicdev {
    struct pci_dev *pdev;

	u32 bar0 ;
	u32 len0 ;
    u8 *map0 ;

    u8 *emb_hdma ;
    u8 *emb_tpci ;
    u8 *emb_mbox ;
    u8 *emb_sctl ;
    u8 *emb_shrm ;

    atomic_t cnt_msi ;
};
#endif

struct aicdma {
    u32 src ;
    u32 dst ;
    u32 misc;   // {ctrl, len}
    u32 next;
};

#ifndef LOG_INFO
#define LOG_INFO(fmt, args...) \
		printk("aicpcie: " fmt, ##args)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(fmt, args...) \
		printk("aicpcie: " fmt, ##args)
#endif

#ifndef LOG_WARN
#define LOG_WARN(fmt, args...) \
		printk("aicpcie: " fmt, ##args)
#endif

void aicwf_pcie_cfg_x(struct aic_pci_dev *adev, u32 addr, u32 wr);
void aicwf_pcie_map_set(struct aic_pci_dev *adev, u8 idx, u32 base, u32 limt, u32 addr);
void aicwf_pcie_cfg(struct aic_pci_dev *adev);
int  aicwf_pcie_dma(struct aic_pci_dev *adev, void *dst, void *src, u32 blen, u8 dir);
int  aicwf_pcie_tran(struct aic_pci_dev *adev, void *addr_emb, void *buf, u32 blen, u8 dir, u8 rem);
int  aicwf_pcie_test_dma(struct aic_pci_dev *adev, u32 off, u16 wlen, u8 dir);
int  aicwf_pcie_test_accs_emb(struct aic_pci_dev *adev);
int  aicwf_pcie_test_msi(struct aic_pci_dev *adev);
int  aicwf_pcie_setst(struct aic_pci_dev *adev);

#endif

