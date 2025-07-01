/**
 ******************************************************************************
 *
 * @file rwnx_irqs.c
 *
 * Copyright (C) RivieraWaves 2012-2019
 *
 ******************************************************************************
 */
#include <linux/interrupt.h>
#include <linux/jiffies.h>

#include "rwnx_defs.h"
#include "ipc_host.h"
#include "rwnx_prof.h"
#include "ipc_shared.h"
#include "pcie_host.h"

extern void rwnx_data_dump(char* tag, void* data, unsigned long len);
extern void rwnx_rx_handle_msg(struct rwnx_hw *rwnx_hw, struct ipc_e2a_msg *msg);

u8 data_cnt = 0;
u8 debug_print = 1;

/**
 * rwnx_irq_hdlr - IRQ handler
 *
 * Handler registerd by the platform driver
 */
irqreturn_t rwnx_irq_hdlr(int irq, void *dev_id)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)dev_id;
    disable_irq_nosync(irq);
    tasklet_schedule(&rwnx_hw->task);
    return IRQ_HANDLED;
}

/**
 * rwnx_task - Bottom half for IRQ handler
 *
 * Read irq status and process accordingly
 */
extern u32 total;

void rwnx_task(unsigned long data)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)data;
    struct aic_pci_dev *adev = rwnx_hw->pcidev;

#ifdef AICWF_PCIE_SUPPORT
    //struct rwnx_plat *rwnx_plat = rwnx_hw->plat;

    uint32_t status;
    bool rxdata_pause = false;
    uint32_t rxdata_successive_cnt = 0;
    bool txdata_pause = false;
    uint32_t txdata_successive_cnt = 0;

    if(rwnx_hw->pci_suspending) {
        printk("%s pci_suspending return\n", __func__);
        if(rwnx_hw->is_irq_disable) {
            rwnx_hw->is_irq_disable = 0;
            enable_irq(rwnx_hw->pcidev->pci_dev->irq);
        }
        return;
    }

    if(rwnx_hw->pcidev->chip_id == PRODUCT_ID_AIC8800D80)
        status = *(volatile unsigned int *)(rwnx_hw->pcidev->pci_bar1_vaddr + AIC8800D80_PCIE_IRQ_STATUS_OFFSET);
    else
        status = readl(adev->emb_mbox + AIC8800D80X2_PCIE_IRQ_STATUS_OFFSET);

    ///printk("s=%x \n",status);
    while(status) {
        //printk(" status = %x \n",status);
        if(rwnx_hw->pcidev->chip_id == PRODUCT_ID_AIC8800D80) {
            volatile unsigned int *ack = (volatile unsigned int *)(rwnx_hw->pcidev->pci_bar1_vaddr + AIC8800D80_PCIE_IRQ_ACK_OFFSET);
            ack[0] = status;
        } else
            writel(status, adev->emb_mbox + AIC8800D80X2_PCIE_IRQ_ACK_OFFSET);

        if(status & PCIE_RX_MSG_BIT) {
            struct rwnx_ipc_buf *buf = rwnx_hw->ipc_env->msgbuf[rwnx_hw->ipc_env->msgbuf_idx];
            struct ipc_e2a_msg *msg = buf->addr;
            u8 ret = 0;

             while(1) {
                dma_sync_single_for_cpu(&rwnx_hw->pcidev->pci_dev->dev, buf->dma_addr, sizeof(struct ipc_e2a_msg), DMA_FROM_DEVICE);
                if(!msg || (msg->pattern != IPC_MSGE2A_VALID_PATTERN && msg->pattern != IPC_CFME2A_VALID_PATTERN)) {
                    dma_sync_single_for_device(&rwnx_hw->pcidev->pci_dev->dev, buf->dma_addr, sizeof(struct ipc_e2a_msg), DMA_FROM_DEVICE);
                    break;
                }

                //rwnx_data_dump("rx msg",msg, 16);

                /* Look for pattern which means that this hostbuf has been used for a MSG */
                if (msg->pattern != IPC_MSGE2A_VALID_PATTERN && msg->pattern != IPC_CFME2A_VALID_PATTERN) {
                    ret = -1;
                    //goto msg_no_push;
                    printk("pattern error: 0x%x\n", msg->pattern);
                }


                /* Relay further actions to the msg parser */
                if(msg->pattern == IPC_MSGE2A_VALID_PATTERN)
                    rwnx_rx_handle_msg(rwnx_hw, msg);
                else {
                    uint32_t used_idx = msg->param[1];
                    uint64_t host_id = rwnx_hw->ipc_env->tx_host_id[0][used_idx % PCIE_TXDESC_CNT];
                    struct sk_buff *skb = (struct sk_buff *)(uint64_t)host_id;
                    struct rwnx_txhdr *txhdr = (struct rwnx_txhdr *)skb->data;
                    uint16_t idx = txhdr->sw_hdr->idx;

                    printk("cfm idx=%d\n", idx);
                    if((struct rwnx_sw_txhdr *)rwnx_hw->ipc_env->txcfm[idx] != NULL) {
                        printk("not txc\n");
                        txhdr->sw_hdr->cfmd = 1;
                        aicwf_pcie_host_tx_cfm_handler(rwnx_hw->ipc_env, msg->param, 0);
                    }
                    else
                        aicwf_pcie_host_tx_cfm_handler(rwnx_hw->ipc_env, msg->param, 1);
                }

                /* Reset the msg buffer and re-use it */
                msg->pattern = 0;
                //wmb();

                dma_sync_single_for_device(&rwnx_hw->pcidev->pci_dev->dev, buf->dma_addr, sizeof(struct ipc_e2a_msg), DMA_TO_DEVICE);
                /* Push back the buffer to the LMAC */
                ipc_host_msgbuf_push(rwnx_hw->ipc_env, buf);

                buf = rwnx_hw->ipc_env->msgbuf[rwnx_hw->ipc_env->msgbuf_idx];
                if (!buf)
                    break;
                msg = buf->addr;
            }
        } 

        if((status & PCIE_RX_DATA_BIT) || rxdata_pause)
        {
            //u32_l hostid;
            struct rwnx_ipc_buf *ipc_buf;
            struct sk_buff *skb ;
            struct hw_rxhdr *hw_rxhdr = NULL;

            volatile struct ipc_shared_rx_buf *rxbuf = &rwnx_hw->ipc_env->shared->host_rxbuf[data_cnt];
            rxdata_successive_cnt = 0;
            rxdata_pause = false;

            while(1) {
                //hostid = rxbuf->hostid;
                ipc_buf =&rwnx_hw->rxbufs[data_cnt];

                if(!ipc_buf)
                    break;
                //if(debug_print)
                    //printk("rx %x, %d, %x\n", rxbuf->pattern, data_cnt, ipc_buf->dma_addr);

                if(rxbuf->pattern != PCIE_RXDATA_COMP_PATTERN) {
                    break;
                }
                if(rxdata_successive_cnt >= 10 ){
                    if(rwnx_hw->pcidev->chip_id == PRODUCT_ID_AIC8800D80) {
                        if(*(volatile unsigned int *)(rwnx_hw->pcidev->pci_bar1_vaddr + AIC8800D80_PCIE_IRQ_STATUS_OFFSET) & PCIE_RX_MSG_BIT) {
                            rxdata_pause = true;
                            break;
                        }
                    } else {
                        if(readl(adev->emb_mbox + AIC8800D80X2_PCIE_IRQ_STATUS_OFFSET) & PCIE_RX_MSG_BIT) {
                            rxdata_pause = true;
                            break;
                        }
                    }
                }

                rxbuf->pattern = 0;
                wmb();

                dma_sync_single_for_cpu(&rwnx_hw->pcidev->pci_dev->dev, ipc_buf->dma_addr, 8192, DMA_FROM_DEVICE);
                skb = (struct sk_buff *)ipc_buf->addr;

                //printk("rx data:cnt=%d, skb=%p, dma=%lx, data0,1=%x, %x, idx=%d\n", data_cnt, ipc_buf->addr, ipc_buf->dma_addr, skb->data[0], skb->data[1], hostid-1 );
                //rwnx_data_dump("rx data", skb->data + 60, 64);
                //if(debug_print)
                    //printk("rx %d\n",data_cnt);

                skb_put(skb, ((skb->data[1]<<8) |skb->data[0] )+ 60);

                if (ipc_buf->addr) {
                    dma_unmap_single(rwnx_hw->dev, ipc_buf->dma_addr, ipc_buf->size, DMA_FROM_DEVICE);
                    ipc_buf->addr = NULL;
                }

                #ifdef AICWF_RX_REORDER
                hw_rxhdr = (struct hw_rxhdr *)skb->data;
                if(hw_rxhdr->is_monitor_vif) {
                    printk("rx data:cnt=%d, skb=%p, dma=%lx, len=%x, %x\n", data_cnt, ipc_buf->addr, (long unsigned int)ipc_buf->dma_addr, skb->data[1], skb->data[0] );
                    rwnx_data_dump("data:", skb->data - 128, 128 + 64);
                }
                #endif
                rwnx_rxdataind_aicwf(rwnx_hw, skb, (void *)rwnx_hw->pcidev->rx_priv);
                rwnx_hw->stats.last_rx = jiffies;
                rxdata_successive_cnt++;

                atomic_dec(&rwnx_hw->rxbuf_cnt);
                data_cnt++;
                if(data_cnt == IPC_RXBUF_CNT)
                    data_cnt = 0;

                rwnx_ipc_rxbuf_alloc(rwnx_hw);
                rxbuf = &rwnx_hw->ipc_env->shared->host_rxbuf[data_cnt];
            }
            for(;atomic_read(&rwnx_hw->rxbuf_cnt) < rwnx_hw->ipc_env->rxbuf_nb;){
                if(rwnx_ipc_rxbuf_alloc(rwnx_hw)){
                    printk("rxbuf alloc fail,now rxbuf_cnt = %d \n",atomic_read(&rwnx_hw->rxbuf_cnt));
                    if(atomic_read(&rwnx_hw->rxbuf_cnt) < 254)
                        complete(&rwnx_hw->pcidev->bus_if->busrx_trgg);
                    break;
                }
            }
        }

        if(status & PCIE_TXC_DATA_BIT || txdata_pause)
        {
            uint32_t txcfm_idx = rwnx_hw->ipc_env->txcfm_idx;
            struct rwnx_sw_txhdr *sw_txhdr = (struct rwnx_sw_txhdr *)rwnx_hw->ipc_env->txcfm[txcfm_idx];
            volatile struct txdesc_host *txdesc = &rwnx_hw->ipc_env->shared->txdesc[txcfm_idx];
            volatile uint32_t ready = 0;
            txdata_successive_cnt = 0;
            txdata_pause = false;

            if (sw_txhdr != NULL) {
                ready = txdesc->ready;
                rmb();
            }

            //printk("txc: idx=%d, ready=%x, tt=%d\n", txcfm_idx, txdesc->ready, total);
            while((sw_txhdr != NULL) && (ready == PCIE_TXDATA_COMP_PATTERN)) {
                struct rwnx_ipc_buf *txcfm_buf = &sw_txhdr->ipc_desc;
                struct sk_buff *skb_tmp = sw_txhdr->skb;

                if(txdata_successive_cnt >= 10 ){
                    if(rwnx_hw->pcidev->chip_id == PRODUCT_ID_AIC8800D80) {
                        if(*(volatile unsigned int *)(rwnx_hw->pcidev->pci_bar1_vaddr + AIC8800D80_PCIE_IRQ_STATUS_OFFSET) & PCIE_RX_MSG_BIT) {
                            txdata_pause = true;
                            printk("txpause\n");
                            break;
                        }
                    } else {
                        if(readl(adev->emb_mbox + AIC8800D80X2_PCIE_IRQ_STATUS_OFFSET) & PCIE_RX_MSG_BIT) {
                            txdata_pause = true;
                            break;
                        }
                    }
                }

                //printk("cfm dma=0x%lx, txcfm_idx=%d, sw_txhdr=%p, skb=%p\n", sw_txhdr->ipc_desc.dma_addr, txcfm_idx, sw_txhdr, sw_txhdr->skb);
                //printk("done:%d\n",txcfm_idx);
                //if(debug_print)
                    //printk("1 txc %d, sw_txhdr=%p, skb=%p\n", txcfm_idx, sw_txhdr, sw_txhdr->skb);
                //struct rwnx_amsdu *amsdu = &sw_txhdr->amsdu;

                if(!sw_txhdr->need_cfm || sw_txhdr->cfmd) {
                    //printk("txc\n");
                    #if 0
                    if(sw_txhdr->desc.api.host.packet_cnt > 1) {
                        u8 ii = 0;
                        struct rwnx_amsdu_txhdr *amsdu_txhdr, *tmp;
                        list_for_each_entry_safe(amsdu_txhdr, tmp, &amsdu->hdrs, list) {
                            list_del(&amsdu_txhdr->list);
                            dma_unmap_single(rwnx_hw->dev, sw_txhdr->desc.api.host.packet_addr[1+ii], 
                                                sw_txhdr->desc.api.host.packet_len[1+ii], DMA_TO_DEVICE);
                            if(amsdu_txhdr->skb)
                                consume_skb(amsdu_txhdr->skb);
                            ii++;
                        }
                    }
                    #endif

                    rwnx_ipc_buf_a2e_release(rwnx_hw, txcfm_buf);
                    dma_unmap_single(rwnx_hw->dev, sw_txhdr->ipc_hostdesc.dma_addr, sw_txhdr->ipc_hostdesc.size, DMA_TO_DEVICE);
                    kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
                    skb_pull(skb_tmp, RWNX_TX_HEADROOM);
                    consume_skb(skb_tmp);
                }
                rwnx_hw->stats.last_tx = jiffies;

                rwnx_hw->ipc_env->txcfm[txcfm_idx] = NULL;
                txcfm_idx++;
                if (txcfm_idx == IPC_TXDMA_DESC_CNT)
                    txcfm_idx = 0;

                txdata_successive_cnt++;
                atomic_dec(&rwnx_hw->txdata_cnt);
                rwnx_hw->ipc_env->txcfm_idx = txcfm_idx;

                sw_txhdr = (struct rwnx_sw_txhdr *)rwnx_hw->ipc_env->txcfm[txcfm_idx];
                if (sw_txhdr != NULL) {
                    txdesc = &rwnx_hw->ipc_env->shared->txdesc[txcfm_idx];
                    ready = txdesc->ready;
                    //if(ready != PCIE_TXDATA_COMP_PATTERN)
                    //    printk("rdy =%x, idx=%d\n", ready, txcfm_idx);
                    rmb();
                } else {
                    break;
                }
            }
        }

        if(rwnx_hw->pcidev->chip_id == PRODUCT_ID_AIC8800D80)
            status = *(volatile unsigned int *)(rwnx_hw->pcidev->pci_bar1_vaddr + AIC8800D80_PCIE_IRQ_STATUS_OFFSET);
        else
            status = readl(adev->emb_mbox + AIC8800D80X2_PCIE_IRQ_STATUS_OFFSET);
    }

    //printk("d%d\n",atomic_read(&rwnx_hw->txdata_cnt_push));
    if (atomic_read(&rwnx_hw->txdata_cnt) <128 && rwnx_hw->fc) {
        struct rwnx_vif *rwnx_vif;
        rwnx_hw->fc = 0;
        AICWFDBG(LOGINFO,"fcr\n");
        list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
            if (!rwnx_vif || !rwnx_vif->ndev || !rwnx_vif->up)
                continue;
            netif_tx_wake_all_queues(rwnx_vif->ndev);
        }
    }

    //printk("exit\n");
#endif

    spin_lock_bh(&rwnx_hw->tx_lock);
    if(atomic_read(&rwnx_hw->txdata_cnt) <64)
        rwnx_hwq_process_all(rwnx_hw);
    spin_unlock_bh(&rwnx_hw->tx_lock);
    //if(debug_print)
       // printk("exit\n");

    if(rwnx_hw->is_irq_disable) {
        //printk("en\n");
        rwnx_hw->is_irq_disable = 0;
        enable_irq(rwnx_hw->pcidev->pci_dev->irq);
    }

}

void rwnx_txrestart_task(unsigned long data)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)data;

    #if 1
    //printk("%s\n", __func__);
    spin_lock_bh(&rwnx_hw->tx_lock);
    rwnx_hwq_process_all(rwnx_hw);
    spin_unlock_bh(&rwnx_hw->tx_lock);
    #endif
}
