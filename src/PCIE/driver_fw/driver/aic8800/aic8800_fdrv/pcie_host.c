
#include "aicwf_pcie.h"
#include "rwnx_platform.h"
#include "pcie_host.h"
#include "ipc_shared.h"
#include "rwnx_defs.h"

void aicwf_pcie_host_init(struct ipc_host_env_tag *env, void *cb,  struct ipc_shared_env_tag *shared_env_ptr, void *pthis)
{
	unsigned int i;
	unsigned int size;
	unsigned int *dst;
	struct ipc_hostid *tx_hostid;

	dst = (unsigned int *)shared_env_ptr;
	size = (unsigned int)sizeof(struct ipc_shared_env_tag);
	for (i=0; i < size; i+=4)
	{
		*(volatile u32 *)dst++ = 0;
	}

	// Reset the IPC Host environment
	memset(env, 0, sizeof(struct ipc_host_env_tag));

	// Initialize the shared environment pointer
	env->shared = shared_env_ptr;

	// Save the callbacks in our own environment
	//env->cb = *cb;

	// Save the pointer to the register base
	env->pthis = pthis;

	// Initialize buffers numbers and buffers sizes needed for DMA Receptions
	env->rxbuf_nb = IPC_RXBUF_CNT;
#ifdef CONFIG_RWNX_FULLMAC
	env->rxdesc_nb = IPC_RXDESC_CNT;
#endif //(CONFIG_RWNX_FULLMAC)
	env->unsuprxvec_sz = max(sizeof(struct rx_vector_desc), (size_t) RADIOTAP_HDR_MAX_LEN) +
		RADIOTAP_HDR_VEND_MAX_LEN +  UNSUP_RX_VEC_DATA_LEN;

	// Initialize the pointer to the TX DMA descriptor arrays
	//env->txdmadesc = shared_env_ptr->txdmadesc;

	INIT_LIST_HEAD(&env->tx_hostid_available);
	INIT_LIST_HEAD(&env->tx_hostid_pushed);
	tx_hostid = env->tx_hostid;
	for (i = 0; i < ARRAY_SIZE(env->tx_hostid); i++, tx_hostid++) {
		tx_hostid->hostid = i + 1; // +1 so that 0 is not a valid value
		list_add_tail(&tx_hostid->list, &env->tx_hostid_available);
	}

}


int aicwf_pcie_platform_init(struct aic_pci_dev *pcidev)
{
    struct rwnx_plat *rwnx_plat = NULL;
    void *drvdata;
    int ret = -ENODEV;

    rwnx_plat = kzalloc(sizeof(struct rwnx_plat), GFP_KERNEL);

    if (!rwnx_plat)
        return -ENOMEM;

	rwnx_plat->pcidev = pcidev;

    ret = rwnx_platform_init(rwnx_plat, &drvdata);
	if(ret) {
		printk("%s fail\n", __func__);
	}
#if 0
    pci_set_drvdata(pci_dev, drvdata);

    if (ret)
        rwnx_plat->deinit(rwnx_plat);
#endif
    return ret;
}



int pcie_host_msg_push(struct ipc_host_env_tag *env, void *msg_buf, uint16_t len)
{
    int i;
    volatile uint32_t *src, *dst;

    //ASSERT_ERR(!env->msga2e_hostid);
    ASSERT_ERR(round_up(len, 4) <= sizeof(env->shared->msg_a2e_buf.msg));

    // Copy the message into the IPC MSG buffer
    //src = (uint32_t*)((struct rwnx_cmd *)msg_buf)->a2e_msg;
    src = (volatile uint32_t *)msg_buf;
    dst = (volatile uint32_t*)&(env->shared->msg_a2e_buf.msg);
    
    // Copy the message in the IPC queue
    for (i=0; i<len; i+=4)
    {
        *dst++ = *src++;
    }

    //env->msga2e_hostid = msg_buf;

    return (0);
}

extern u8 debug_print;

void pcie_txdesc_push(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
                          struct sk_buff *skb, int hw_queue)
{
    struct txdesc_host *txdesc_host = &sw_txhdr->desc;
    struct rwnx_ipc_buf *ipc_hostdesc_buf = &sw_txhdr->ipc_hostdesc;
	uint32_t dma_idx = rwnx_hw->ipc_env->txdmadesc_idx;
	//int i;
    unsigned long flags;

	//printk("txdesc_host=%p, dma_idx=%d, pkt_addr=%x\n", txdesc_host, dma_idx, txdesc_host->api.host.packet_addr[0]);
    volatile uint32_t *src;
    volatile struct txdesc_host *dst;
	src = (volatile uint32_t *)txdesc_host;
	dst = &rwnx_hw->ipc_env->shared->txdesc[dma_idx];

    txdesc_host->ctrl.hwq = hw_queue;
	txdesc_host->pattern = 0xAC123456;
	//txdesc_host->ready = 0xFFFFFFFF;
    txdesc_host->api.host.flags_ext = 0;

        sw_txhdr->desc.ready = 0;
        sw_txhdr->desc.packet_dma_addr = 0;


	//rwnx_data_dump("dhcp2", skb->data, 128);
	
	sw_txhdr->ipc_desc.addr = skb;
	sw_txhdr->ipc_desc.dma_addr = sw_txhdr->ipc_data.dma_addr;//txdesc_host->api.host.packet_addr[0];
	sw_txhdr->ipc_desc.size = sw_txhdr->frame_len;//txdesc_host->api.host.packet_len[0];
	sw_txhdr->idx = dma_idx;


	wmb();

#if 0
    ipc_hostdesc_buf->size = sizeof(struct txdesc_host);
    ipc_hostdesc_buf->dma_addr = dma_map_single(rwnx_hw->dev, &sw_txhdr->desc, sizeof(struct txdesc_host), DMA_TO_DEVICE);
	if (dma_mapping_error(rwnx_hw->dev, ipc_hostdesc_buf->dma_addr)) {
		printk("fail to push\n");
		struct rwnx_ipc_buf *txcfm_buf = &sw_txhdr->ipc_desc;
		struct sk_buff *skb_tmp = sw_txhdr->skb;
		rwnx_ipc_buf_a2e_release(rwnx_hw, txcfm_buf);
		//dma_unmap_single(rwnx_hw->dev, sw_txhdr->ipc_hostdesc.dma_addr, sw_txhdr->ipc_hostdesc.size, DMA_TO_DEVICE);
		kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
		skb_pull(skb_tmp, RWNX_TX_HEADROOM);
		consume_skb(skb_tmp);
		atomic_dec(&rwnx_hw->txdata_cnt);
		return ;
    }
#endif
	//sw_txhdr->desc.ready = ipc_hostdesc_buf->dma_addr;
	//sw_txhdr->desc.packet_dma_addr = sw_txhdr->ipc_desc.dma_addr;
	//sw_txhdr->idx = dma_idx;
         
	dma_sync_single_for_device(rwnx_hw->dev, ipc_hostdesc_buf->dma_addr, sizeof(struct txdesc_host), DMA_TO_DEVICE);

	//printk("dma_idx=%d,dma is 0x%lx,  desc_dma=%x, sw_txhdr=%p,len=%d\n", dma_idx, txdesc_host->api.host.packet_addr[0], ipc_hostdesc_buf->dma_addr, sw_txhdr,txdesc_host->api.host.packet_len[0]);

    dst->api.host.packet_len = sw_txhdr->ipc_desc.size;
	dst->packet_dma_addr = sw_txhdr->ipc_desc.dma_addr;

	wmb();

	dst->ready = ipc_hostdesc_buf->dma_addr;

	wmb();

	rwnx_hw->ipc_env->txcfm[dma_idx] = (struct rwnx_ipc_buf *)sw_txhdr;
	//printk("push:%d, %x, %d , %x ,offset=%lx\n", dma_idx, ipc_hostdesc_buf->dma_addr, sw_txhdr->ipc_desc.size, dst->ready,  (unsigned long)dst - (unsigned long)rwnx_hw->ipc_env->shared);
	//printk("push:%d, sw_txhdr=%p, skb=%p\n", dma_idx, sw_txhdr, sw_txhdr->skb);
    //dst->api.host.packet_addr[0]= sw_txhdr->ipc_desc.dma_addr;
    //dst->api.host.packet_len[0] = sw_txhdr->ipc_desc.size;
    #if 0
    for (i=0; i<sizeof(struct txdesc_host ); i+=4)
    {
        *dst++ = *src++;
    }
    #endif
    
	//if(debug_print)
	//		printk("tx %d, %x\n", dma_idx, ipc_hostdesc_buf->dma_addr);

    //atomic_inc(&rwnx_hw->txdata_cnt);
	dma_idx++;
    if (dma_idx == IPC_TXDMA_DESC_CNT)
        rwnx_hw->ipc_env->txdmadesc_idx = 0;
    else
        rwnx_hw->ipc_env->txdmadesc_idx = dma_idx;

	atomic_dec(&rwnx_hw->txdata_cnt_push);
	//printk("p%d,%d\n",atomic_read(&rwnx_hw->txdata_cnt), atomic_read(&rwnx_hw->txdata_cnt_push));

	#if 0
    struct rwnx_ipc_buf *ipc_desc = &sw_txhdr->ipc_desc;

    txdesc_host->ctrl.hwq = hw_queue;
    txdesc_host->api.host.hostid = ipc_host_tx_host_ptr_to_id(rwnx_hw->ipc_env, skb);
    txdesc_host->ready = 0xFFFFFFFF;
    if (!txdesc_host->api.host.hostid) {
        dev_err(rwnx_hw->dev, "No more tx_hostid available \n");
        return;
    }

    if (rwnx_ipc_buf_a2e_init(rwnx_hw, ipc_desc, txdesc_host, sizeof(*txdesc_host)))
        return ;

    ipc_host_txdesc_push(rwnx_hw->ipc_env, ipc_desc);
	#endif
}

void aicwf_pcie_host_txdesc_push(struct ipc_host_env_tag *env, const int queue_idx, const uint64_t host_id)
{
    //printk("push, %d, %d, 0x%llx \r\n", queue_idx, env->txdesc_free_idx[queue_idx], host_id);
    // Save the host id in the environment
    env->tx_host_id[queue_idx][env->txdesc_free_idx[queue_idx] % PCIE_TXDESC_CNT] = host_id;

    // Increment the index
    env->txdesc_free_idx[queue_idx]++;
    if(env->txdesc_free_idx[queue_idx]==0x40000000)
        env->txdesc_free_idx[queue_idx] = 0;
}

void aicwf_pcie_host_tx_cfm_handler(struct ipc_host_env_tag *env, u32 *data, u8 free)
{
    u32 queue_idx  = 0;// data[0];
    //struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)env->pthis;
    struct sk_buff *skb = NULL;
    struct rwnx_txhdr *txhdr;

    // TX confirmation descriptors have been received
   // REG_SW_SET_PROFILING(env->pthis, SW_PROF_IRQ_E2A_TXCFM);
    //while (1)
    {
        // Get the used index and increase it. We do the increase before knowing if the
        // current buffer is confirmed because the callback function may call the
        // ipc_host_txdesc_get() in case flow control was enabled and the index has to be
        // already at the good value to ensure that the test of FIFO full is correct
        //uint32_t used_idx = env->txdesc_used_idx[queue_idx]++;
	    uint32_t used_idx = data[1];
        uint64_t host_id = env->tx_host_id[queue_idx][used_idx % PCIE_TXDESC_CNT];

        // Reset the host id in the array
        env->tx_host_id[queue_idx][used_idx % PCIE_TXDESC_CNT] = 0;
		printk("pcie cfm: data：%x, used_idx=%d, skb=%p\n", data[0], used_idx, (struct sk_buff *)(uint64_t)host_id);
		#if 1
        // call the external function to indicate that a TX packet is freed
        if (host_id == 0)
        {
            // No more confirmations, so put back the used index at its initial value
            env->txdesc_used_idx[queue_idx] = used_idx;
            printk("pcie ERROR:No more confirmations\r\n");
            //break;
        }
        // set the cfm status
        skb = (struct sk_buff *)(uint64_t)host_id;
        txhdr = (struct rwnx_txhdr *)skb->data;
        txhdr->hw_hdr.cfm.status = (union rwnx_hw_txstatus)data[0];
        printk("pcie host_tx_cfm_handler:used_idx=%d, 0x%p, status=%x\r\n",used_idx, skb, txhdr->hw_hdr.cfm.status.value);
        //if (env->cb.send_data_cfm(env->pthis, host_id) != 0)
        if (rwnx_txdatacfm(env->pthis, (void *)host_id, free) != 0)
        {
            // No more confirmations, so put back the used index at its initial value
            env->txdesc_used_idx[queue_idx] = used_idx;
            env->tx_host_id[queue_idx][used_idx % PCIE_TXDESC_CNT] = host_id;
            // and exit the loop
            printk("pcie ERROR:rwnx_txdatacfm,\r\n");
          //  break;
        }
		#endif
    }
}

