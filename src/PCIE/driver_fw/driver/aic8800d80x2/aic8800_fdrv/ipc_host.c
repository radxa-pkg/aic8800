/**
 ******************************************************************************
 *
 * @file ipc_host.c
 *
 * @brief IPC module.
 *
 * Copyright (C) RivieraWaves 2011-2019
 *
 ******************************************************************************
 */

/*
 * INCLUDE FILES
 ******************************************************************************
 */
#include <linux/spinlock.h>
#include "rwnx_defs.h"
#include "rwnx_prof.h"
#include "reg_ipc_app.h"
#include "ipc_host.h"

/*
 * TYPES DEFINITION
 ******************************************************************************
 */

const int nx_txdesc_cnt[] =
{
    NX_TXDESC_CNT0,
    NX_TXDESC_CNT1,
    NX_TXDESC_CNT2,
    NX_TXDESC_CNT3,
    #if NX_TXQ_CNT == 5
    NX_TXDESC_CNT4,
    #endif
};

const int nx_txuser_cnt[] =
{
    CONFIG_USER_MAX,
    CONFIG_USER_MAX,
    CONFIG_USER_MAX,
    CONFIG_USER_MAX,
    #if NX_TXQ_CNT == 5
    1,
    #endif
};


/*
 * FUNCTIONS DEFINITIONS
 ******************************************************************************
 */
/**
 * ipc_host_rxdesc_handler() - Handle the reception of a Rx Descriptor
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_RXDESC is set
 */
static void ipc_host_rxdesc_handler(struct ipc_host_env_tag *env)
{
    // For profiling
    REG_SW_SET_PROFILING(env->pthis, SW_PROF_IRQ_E2A_RXDESC);

    // LMAC has triggered an IT saying that a reception has occurred.
    // Then we first need to check the validity of the current hostbuf, and the validity
    // of the next hostbufs too, because it is likely that several hostbufs have been
    // filled within the time needed for this irq handling
    while (1) {
        #ifdef CONFIG_RWNX_FULLMAC
        // call the external function to indicate that a RX descriptor is received
        if (env->cb.recv_data_ind(env->pthis, env->rxdesc[env->rxdesc_idx]) != 0)
        #else
        // call the external function to indicate that a RX packet is received
        if (env->cb.recv_data_ind(env->pthis, env->rxbuf[env->rxbuf_idx]) != 0)
        #endif //(CONFIG_RWNX_FULLMAC)
            break;

    }

    // For profiling
    REG_SW_CLEAR_PROFILING(env->pthis, SW_PROF_IRQ_E2A_RXDESC);
}

/**
 * ipc_host_radar_handler() - Handle the reception of radar events
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_RADAR is set
 */
static void ipc_host_radar_handler(struct ipc_host_env_tag *env)
{
#ifdef CONFIG_RWNX_RADAR
    spin_lock(&((struct rwnx_hw *)env->pthis)->radar.lock);
    while (env->cb.recv_radar_ind(env->pthis,
                                  env->radar[env->radar_idx]) == 0)
        ;
    spin_unlock(&((struct rwnx_hw *)env->pthis)->radar.lock);
#endif /* CONFIG_RWNX_RADAR */
}

/**
 * ipc_host_unsup_rx_vec_handler() - Handle the reception of unsupported rx vector
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_UNSUP_RX_VEC is set
 */
static void ipc_host_unsup_rx_vec_handler(struct ipc_host_env_tag *env)
{
    while (env->cb.recv_unsup_rx_vec_ind(env->pthis,
                                         env->unsuprxvec[env->unsuprxvec_idx]) == 0)
        ;
}

/**
 * ipc_host_msg_handler() - Handler for firmware message
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_MSG is set
 */
static void ipc_host_msg_handler(struct ipc_host_env_tag *env)
{
    while (env->cb.recv_msg_ind(env->pthis, env->msgbuf[env->msgbuf_idx]) == 0)
        ;
}

/**
 * ipc_host_msgack_handler() - Handle the reception of message acknowledgement
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_MSG_ACK is set
 */
static void ipc_host_msgack_handler(struct ipc_host_env_tag *env)
{
    void *hostid = env->msga2e_hostid;

    ASSERT_ERR(hostid);
    ASSERT_ERR(env->msga2e_cnt == (((struct lmac_msg *)(&env->shared->msg_a2e_buf.msg))->src_id & 0xFF));

    env->msga2e_hostid = NULL;
    env->msga2e_cnt++;
    env->cb.recv_msgack_ind(env->pthis, hostid);
}

/**
 * ipc_host_dbg_handler() - Handle the reception of Debug event
 *
 * @env: pointer to the IPC Host environment
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_DBG is set
 */
static void ipc_host_dbg_handler(struct ipc_host_env_tag *env)
{
    while(env->cb.recv_dbg_ind(env->pthis,
                               env->dbgbuf[env->dbgbuf_idx]) == 0)
        ;
}

/**
 * ipc_host_tx_cfm_handler() - Handle the reception of TX confirmation
 *
 * @env: pointer to the IPC Host environment
 * @queue_idx: index of the hardware on which the confirmation has been received
 * @user_pos: index of the user position
 *
 * Called from general IRQ handler when status %IPC_IRQ_E2A_TXCFM is set.
 * Process confirmations in order until:
 * - There is no more buffer pushed (no need to check confirmation in this case)
 * - The confirmation has not been updated by firmware
 */
static void ipc_host_tx_cfm_handler(struct ipc_host_env_tag *env,
                                    const int queue_idx, const int user_pos)
{
    while (!list_empty(&env->tx_hostid_pushed))
    {
        if (env->cb.send_data_cfm(env->pthis,
                                  env->txcfm[env->txcfm_idx],1))
            break;

        env->txcfm_idx++;
        if (env->txcfm_idx == IPC_TXDMA_DESC_CNT)//IPC_TXCFM_CNT)
            env->txcfm_idx = 0;
    }
}

/**
 ******************************************************************************
 */
bool ipc_host_tx_frames_pending(struct ipc_host_env_tag *env)
{
    return !list_empty(&env->tx_hostid_pushed);
}

/**
 ******************************************************************************
 */
void *ipc_host_tx_flush(struct ipc_host_env_tag *env)
{
    struct ipc_hostid *tx_hostid;
    tx_hostid = list_first_entry_or_null(&env->tx_hostid_pushed,
                                         struct ipc_hostid, list);

    if (!tx_hostid)
        return NULL;

    list_del(&tx_hostid->list);
    list_add_tail(&tx_hostid->list, &env->tx_hostid_available);
    return tx_hostid->hostptr;
}

/**
 ******************************************************************************
 */
void ipc_host_init(struct ipc_host_env_tag *env,
                  struct ipc_host_cb_tag *cb,
                  struct ipc_shared_env_tag *shared_env_ptr,
                  void *pthis)
{
    unsigned int i;
    unsigned int size;
    unsigned int *dst;
    struct ipc_hostid *tx_hostid;

    // Reset the environments
    // Reset the IPC Shared memory
#if 0
    /* check potential platform bug on multiple stores */
    memset(shared_env_ptr, 0, sizeof(struct ipc_shared_env_tag));
#else
    dst = (unsigned int *)shared_env_ptr;
    size = (unsigned int)sizeof(struct ipc_shared_env_tag);
    for (i=0; i < size; i+=4)
    {
        *dst++ = 0;
    }
#endif
    // Reset the IPC Host environment
    memset(env, 0, sizeof(struct ipc_host_env_tag));

    // Initialize the shared environment pointer
    env->shared = shared_env_ptr;

    // Save the callbacks in our own environment
    env->cb = *cb;

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

/**
 ******************************************************************************
 */
void ipc_host_pattern_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    env->shared->pattern_addr = buf->dma_addr;
}

/**
 ******************************************************************************
 */
int ipc_host_rxbuf_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

#ifdef CONFIG_RWNX_FULLMAC
    shared_env->host_rxbuf[env->rxbuf_idx].hostid = RWNX_RXBUFF_HOSTID_GET(buf);
    shared_env->host_rxbuf[env->rxbuf_idx].dma_addr = buf->dma_addr;
	shared_env->host_rxbuf[env->rxbuf_idx].pattern = 0x0;
	//printk("%s: idx=%d, hostid=%d, dma=%lx, skb=%p\n", __func__,env->rxbuf_idx, 
	//		(void *)shared_env->host_rxbuf[env->rxbuf_idx].hostid, shared_env->host_rxbuf[env->rxbuf_idx].dma_addr, buf->addr);
#else
    env->rxbuf[env->rxbuf_idx] = buf;
    shared_env->host_rxbuf[env->rxbuf_idx] = buf->dma_addr;
#endif // CONFIG_RWNX_FULLMAC

    // Signal to the embedded CPU that at least one buffer is available
    //ipc_app2emb_trigger_set(shared_env, IPC_IRQ_A2E_RXBUF_BACK);

    // Increment the array index
    env->rxbuf_idx = (env->rxbuf_idx + 1) % IPC_RXBUF_CNT;

    return 0;
}

#ifdef CONFIG_RWNX_FULLMAC
/**
 ******************************************************************************
 */
int ipc_host_rxdesc_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    env->rxdesc[env->rxdesc_idx] = buf;
    shared_env->host_rxdesc[env->rxdesc_idx].dma_addr = buf->dma_addr;

    // Signal to the embedded CPU that at least one descriptor is available
    //ipc_app2emb_trigger_set(shared_env, IPC_IRQ_A2E_RXDESC_BACK);

    env->rxdesc_idx = (env->rxdesc_idx + 1) % IPC_RXDESC_CNT;

    return 0;
}
#endif /* CONFIG_RWNX_FULLMAC */

/**
 ******************************************************************************
 */
int ipc_host_radar_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    // Save Ipc buffer in host env
    env->radar[env->radar_idx] = buf;

    // Copy the DMA address in the ipc shared memory
    shared_env->radarbuf_hostbuf[env->radar_idx] = buf->dma_addr;

    // Increment the array index
    env->radar_idx = (env->radar_idx + 1) % IPC_RADARBUF_CNT;

    return 0;
}

/**
 ******************************************************************************
 */
int ipc_host_unsuprxvec_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env_ptr = env->shared;

    env->unsuprxvec[env->unsuprxvec_idx] = buf;
    shared_env_ptr->unsuprxvecbuf_hostbuf[env->unsuprxvec_idx] = buf->dma_addr;

    env->unsuprxvec_idx = (env->unsuprxvec_idx + 1) % IPC_UNSUPRXVECBUF_CNT;

    return 0;
}

/**
 ******************************************************************************
 */
int ipc_host_msgbuf_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    env->msgbuf[env->msgbuf_idx] = buf;
    shared_env->msg_e2a_hostbuf_addr[env->msgbuf_idx] = buf->dma_addr;

    env->msgbuf_idx = (env->msgbuf_idx + 1) % IPC_MSGE2A_BUF_CNT;

    return 0;
}

/**
 ******************************************************************************
 */
int ipc_host_dbgbuf_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    env->dbgbuf[env->dbgbuf_idx] = buf;
    shared_env->dbg_hostbuf_addr[env->dbgbuf_idx] = buf->dma_addr;

    env->dbgbuf_idx = (env->dbgbuf_idx + 1) % IPC_DBGBUF_CNT;

    return 0;
}

/**
 ******************************************************************************
 */
void ipc_host_dbginfo_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    struct ipc_shared_env_tag *shared_env = env->shared;

    shared_env->la_dbginfo_addr = buf->dma_addr;
}

/**
 ******************************************************************************
 */
#if 0
void ipc_host_txdesc_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf)
{
    uint32_t dma_idx = env->txdmadesc_idx;
    volatile struct dma_desc *dmadesc_pushed;

    dmadesc_pushed = &env->txdmadesc[dma_idx++];

    // Write DMA address to the descriptor
    dmadesc_pushed->src = buf->dma_addr;

    wmb();

    if (dma_idx == IPC_TXDMA_DESC_CNT)
        env->txdmadesc_idx = 0;
    else
        env->txdmadesc_idx = dma_idx;

    // trigger interrupt to firmware
    //ipc_app2emb_trigger_setf(env->shared, IPC_IRQ_A2E_TXDESC);
}
#endif

/**
 * ipc_host_tx_host_ptr_to_id() - Save and convert host pointer to host id
 *
 * @env: pointer to the IPC Host environment
 * @host_ptr: host pointer to save in the ipc_hostid element ()
 * @return: uint32_t value associated to this host buffer.
 *
 * Move a free ipc_hostid from the tx_hostid_available list to the tx_hostid_pushed list.
 * The element is initialized with the host pointer and the associated 32bits value is
 * returned.
 * It is expected that list tx_hostid_available contains at least one element.
 */
uint32_t ipc_host_tx_host_ptr_to_id(struct ipc_host_env_tag *env, void *host_ptr)
{
    struct ipc_hostid *tx_hostid;
    tx_hostid = list_first_entry_or_null(&env->tx_hostid_available,
                                         struct ipc_hostid, list);
    if (!tx_hostid)
        return 0;

    list_del(&tx_hostid->list);
    list_add_tail(&tx_hostid->list, &env->tx_hostid_pushed);
    tx_hostid->hostptr = host_ptr;
    return tx_hostid->hostid;
}

/**
 * ipc_host_tx_host_id_to_ptr() - Retrieve host ptr from host id
 *
 * @env: pointer to the IPC Host environment
 * @hostid: hostid present in the confirmation
 * @return: pointer saved via ipc_host_tx_host_ptr_to_id()
 *
 * Allow to retrieve the host ptr (to the tx buffer) form the host id found in
 * the confirmation.
 * Move back the tx_hostid element from the tx_hostid_pushed list to the
 * tx_hostid_available list.
 */
void *ipc_host_tx_host_id_to_ptr(struct ipc_host_env_tag *env, uint32_t hostid)
{
    struct ipc_hostid *tx_hostid;

    if (unlikely(!hostid || (hostid > ARRAY_SIZE(env->tx_hostid))))
        return NULL;

    tx_hostid = &env->tx_hostid[hostid - 1];

    list_del(&tx_hostid->list);
    list_add_tail(&tx_hostid->list, &env->tx_hostid_available);
    return tx_hostid->hostptr;
}

/**
 ******************************************************************************
 */
void ipc_host_irq(struct ipc_host_env_tag *env, uint32_t status)
{
    // Acknowledge the pending interrupts
    ipc_emb2app_ack_clear(env->shared, status);
    // And re-read the status, just to be sure that the acknowledgment is
    // effective when we start the interrupt handling
    ipc_emb2app_status_get(env->shared);

    // Optimized for only one IRQ at a time
    if (status & IPC_IRQ_E2A_RXDESC)
    {
        // handle the RX descriptor reception
        ipc_host_rxdesc_handler(env);
    }
    if (status & IPC_IRQ_E2A_MSG_ACK)
    {
        ipc_host_msgack_handler(env);
    }
    if (status & IPC_IRQ_E2A_MSG)
    {
        ipc_host_msg_handler(env);
    }
    if (status & IPC_IRQ_E2A_TXCFM)
    {
        int i;

        spin_lock(&((struct rwnx_hw *)env->pthis)->tx_lock);
        // handle the TX confirmation reception
        for (i = 0; i < IPC_TXQUEUE_CNT; i++)
        {
            int j = 0;
#ifdef CONFIG_RWNX_MUMIMO_TX
            for (; j < nx_txuser_cnt[i]; j++)
#endif
            {
                uint32_t q_bit = CO_BIT(j + i * CONFIG_USER_MAX + IPC_IRQ_E2A_TXCFM_POS);
                if (status & q_bit)
                {
                    // handle the confirmation
                    ipc_host_tx_cfm_handler(env, i, j);
                }
            }
        }
        spin_unlock(&((struct rwnx_hw *)env->pthis)->tx_lock);
    }
    if (status & IPC_IRQ_E2A_RADAR)
    {
        // handle the radar event reception
        ipc_host_radar_handler(env);
    }

    if (status & IPC_IRQ_E2A_UNSUP_RX_VEC)
    {
        // handle the unsupported rx vector reception
        ipc_host_unsup_rx_vec_handler(env);
    }

    if (status & IPC_IRQ_E2A_DBG)
    {
        ipc_host_dbg_handler(env);
    }
}

/**
 ******************************************************************************
 */
int ipc_host_msg_push(struct ipc_host_env_tag *env, void *msg_buf, uint16_t len)
{
    int i;
    uint32_t *src, *dst;

    REG_SW_SET_PROFILING(env->pthis, SW_PROF_IPC_MSGPUSH);

    ASSERT_ERR(!env->msga2e_hostid);
    ASSERT_ERR(round_up(len, 4) <= sizeof(env->shared->msg_a2e_buf.msg));

    // Copy the message into the IPC MSG buffer
    src = (uint32_t*)((struct rwnx_cmd *)msg_buf)->a2e_msg;
    dst = (uint32_t*)&(env->shared->msg_a2e_buf.msg);

    // Copy the message in the IPC queue
    for (i=0; i<len; i+=4)
    {
        *dst++ = *src++;
    }

    env->msga2e_hostid = msg_buf;

    // Trigger the irq to send the message to EMB
    //ipc_app2emb_trigger_set(env->shared, IPC_IRQ_A2E_MSG);

    REG_SW_CLEAR_PROFILING(env->pthis, SW_PROF_IPC_MSGPUSH);

    return (0);
}

/**
 ******************************************************************************
 */
void ipc_host_enable_irq(struct ipc_host_env_tag *env, uint32_t value)
{
    // Enable the handled interrupts
    ipc_emb2app_unmask_set(env->shared, value);
}

/**
 ******************************************************************************
 */
void ipc_host_disable_irq(struct ipc_host_env_tag *env, uint32_t value)
{
    // Enable the handled interrupts
    ipc_emb2app_unmask_clear(env->shared, value);
}

/**
 ******************************************************************************
 */
uint32_t ipc_host_get_status(struct ipc_host_env_tag *env)
{
    volatile uint32_t status;

    status = ipc_emb2app_status_get(env->shared);

    return status;
}

/**
 ******************************************************************************
 */
uint32_t ipc_host_get_rawstatus(struct ipc_host_env_tag *env)
{
    volatile uint32_t rawstatus;

    rawstatus = ipc_emb2app_rawstatus_get(env->shared);

    return rawstatus;
}

