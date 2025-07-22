
#ifndef _PCIE_HOST_H_
#define _PCIE_HOST_H_

#include "ipc_host.h"
#include "rwnx_tx.h"

#define PCIE_TXQUEUE_CNT     NX_TXQ_CNT
#define PCIE_TXDESC_CNT      NX_TXDESC_CNT

#define PCIE_RX_MSG_BIT      CO_BIT(0)
#define PCIE_RX_DATA_BIT     CO_BIT(1)
#define PCIE_TXC_DATA_BIT    CO_BIT(2)
#define PCIE_FW_ERR_BIT      CO_BIT(3)

#define AIC8800D80_PCIE_IRQ_STATUS_OFFSET  0x3521c
#define AIC8800D80_PCIE_IRQ_ACK_OFFSET     0x35208
#define AIC8800D80X2_PCIE_IRQ_STATUS_OFFSET  0x021c
#define AIC8800D80X2_PCIE_IRQ_ACK_OFFSET     0x0208

#define PCIE_RXDATA_COMP_PATTERN 0xFFFFFFFF
#define PCIE_TXDATA_COMP_PATTERN 0xEEEEEEEE

/// Definition of the IPC Host environment structure.
struct pcie_host_env_tag
{
    // Index used that points to the first free TX desc
    uint32_t txdesc_free_idx[PCIE_TXQUEUE_CNT];
    // Index used that points to the first used TX desc
    uint32_t txdesc_used_idx[PCIE_TXQUEUE_CNT];
    // Array storing the currently pushed host ids, per IPC queue
    unsigned long tx_host_id[PCIE_TXQUEUE_CNT][PCIE_TXDESC_CNT];

    /// Pointer to the attached object (used in callbacks and register accesses)
    void *pthis;
};

extern int aicwf_pcie_platform_init(struct aic_pci_dev *pcidev);
extern void aicwf_hostif_ready(void);
extern int pcie_host_msg_push(struct ipc_host_env_tag *env, void *msg_buf, uint16_t len);
extern void pcie_txdesc_push(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
                          struct sk_buff *skb, int hw_queue);
extern void aicwf_pcie_host_txdesc_push(struct ipc_host_env_tag *env, const int queue_idx, const uint64_t host_id);
extern void aicwf_pcie_host_tx_cfm_handler(struct ipc_host_env_tag *env, u32 *data, u8 free);
void aicwf_pcie_host_init(struct ipc_host_env_tag *env, void *cb,  struct ipc_shared_env_tag *shared_env_ptr, void *pthis);

#endif
