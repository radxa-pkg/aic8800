/**
 * rwnx_ipc_utils.h
 *
 * IPC utility function declarations
 *
 * Copyright (C) RivieraWaves 2012-2021
 */
#ifndef _RWNX_IPC_UTILS_H_
#define _RWNX_IPC_UTILS_H_

#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/skbuff.h>
#include "aicwf_debug.h"
#include "lmac_msg.h"
#if 0
#ifdef CONFIG_RWNX_DBG
/*  #define RWNX_DBG(format, arg...) pr_warn(format, ## arg) */
#define RWNX_DBG printk
#else
#define RWNX_DBG(a...) do {} while (0)
#endif

#define RWNX_FN_ENTRY_STR ">>> %s()\n", __func__
#endif

#ifdef ANDROID_PLATFORM
#define HIGH_KERNEL_VERSION KERNEL_VERSION(5, 15, 41)
#else
#define HIGH_KERNEL_VERSION KERNEL_VERSION(6, 0, 0)
#endif

enum rwnx_dev_flag {
    RWNX_DEV_RESTARTING,
    RWNX_DEV_STACK_RESTARTING,
    RWNX_DEV_STARTED,
    RWNX_DEV_ADDING_STA,
};

struct rwnx_hw;
struct rwnx_sta;
struct rwnx_sw_txhdr;

/**
 * struct rwnx_ipc_buf - Generic IPC buffer
 * An IPC buffer is a buffer allocated in host memory and "DMA mapped" to be
 * accessible by the firmware.
 *
 * @addr: Host address of the buffer. If NULL other field are invalid
 * @dma_addr: DMA address of the buffer.
 * @size: Size, in bytes, of the buffer
 */
struct rwnx_ipc_buf
{
    void *addr;
    dma_addr_t dma_addr;
    size_t size;
};

/**
 * struct rwnx_ipc_buf_pool - Generic pool of IPC buffers
 *
 * @nb: Number of buffers currently allocated in the pool
 * @buffers: Array of buffers (size of array is @nb)
 * @pool: DMA pool in which buffers have been allocated
 */
struct rwnx_ipc_buf_pool {
    int nb;
    struct rwnx_ipc_buf *buffers;
    struct dma_pool *pool;
};

/**
 * struct rwnx_ipc_dbgdump - IPC buffer for debug dump
 *
 * @mutex: Mutex to protect access to debug dump
 * @buf: IPC buffer
 */
struct rwnx_ipc_dbgdump {
    struct mutex mutex;
    struct rwnx_ipc_buf buf;
};

static const u32 rwnx_tx_pattern = 0xCAFEFADE;

/*
 * Maximum Length of Radiotap header vendor specific data(in bytes)
 */
#define RADIOTAP_HDR_VEND_MAX_LEN   16

/*
 * Maximum Radiotap Header Length without vendor specific data (in bytes)
 */
#define RADIOTAP_HDR_MAX_LEN        80

/*
 * Unsupported HT Frame data length (in bytes)
 */
#define UNSUP_RX_VEC_DATA_LEN       2


/**
 * IPC environment control
 */
int rwnx_ipc_init(struct rwnx_hw *rwnx_hw, u8 *shared_ram);
void rwnx_ipc_deinit(struct rwnx_hw *rwnx_hw);
void rwnx_ipc_start(struct rwnx_hw *rwnx_hw);
void rwnx_ipc_stop(struct rwnx_hw *rwnx_hw);
void rwnx_ipc_msg_push(struct rwnx_hw *rwnx_hw, void *msg_buf, uint16_t len);

/**
 * IPC buffer management
 */
int rwnx_ipc_buf_alloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                       size_t buf_size, enum dma_data_direction dir, const void *init);
/**
 * rwnx_ipc_buf_e2a_alloc() - Allocate an Embedded To Application Input IPC buffer
 *
 * @rwnx_hw: Main driver data
 * @buf: IPC buffer structure to store I{PC buffer information
 * @buf_size: Size of the Buffer to allocate
 * @return: 0 on success and != 0 otherwise
 */
static inline int rwnx_ipc_buf_e2a_alloc(struct rwnx_hw *rwnx_hw,
                                         struct rwnx_ipc_buf *buf,
                                         size_t buf_size)
{
    return rwnx_ipc_buf_alloc(rwnx_hw, buf, buf_size, DMA_FROM_DEVICE, NULL);
}

/**
 * rwnx_ipc_buf_a2e_alloc() - Allocate an Application to Embedded Output IPC buffer
 *
 * @rwnx_hw: Main driver data
 * @buf: IPC buffer structure to store I{PC buffer information
 * @buf_size: Size of the Buffer to allocate
 * @buf_data: Initialization data for the buffer. Must be at least
 * @buf_size long
 * @return: 0 on success and != 0 otherwise
 */
static inline int rwnx_ipc_buf_a2e_alloc(struct rwnx_hw *rwnx_hw,
                                         struct rwnx_ipc_buf *buf,
                                         size_t buf_size, const void *buf_data)
{
    return rwnx_ipc_buf_alloc(rwnx_hw, buf, buf_size, DMA_TO_DEVICE, buf_data);
}
void rwnx_ipc_buf_dealloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf);
int rwnx_ipc_buf_a2e_init(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                          void *data, size_t buf_size);

void rwnx_ipc_buf_release(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf,
                          enum dma_data_direction dir);

/**
 * rwnx_ipc_buf_e2a_release() - Release DMA mapping for an Application to Embedded IPC buffer
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to release
 *
 * An A2E buffer is realeased when it has been read by the embbeded side. This is
 * used before giving back a buffer to upper layer, or before deleting a buffer
 * when rwnx_ipc_buf_dealloc() cannot be used.
 */
static inline void rwnx_ipc_buf_a2e_release(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf)
{
    rwnx_ipc_buf_release(rwnx_hw, buf, DMA_TO_DEVICE);
}

/**
 * rwnx_ipc_buf_e2a_release() - Release DMA mapping for an Embedded to Application IPC buffer
 *
 * @rwnx_hw: Main driver structure
 * @buf: IPC buffer to release
 *
 * An E2A buffer is released when it has been updated by the embedded and it's ready
 * to be forwarded to upper layer (i.e. out of the driver) or to be deleted and
 * rwnx_ipc_buf_dealloc() cannot be used.
 *
 * Note: This function has the side effect to synchronize the buffer for the host so no need to
 * call rwnx_ipc_buf_e2a_sync().
 */
static inline void rwnx_ipc_buf_e2a_release(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf)
{
    rwnx_ipc_buf_release(rwnx_hw, buf, DMA_FROM_DEVICE);
}

void rwnx_ipc_buf_e2a_sync(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf, size_t len);
void rwnx_ipc_buf_e2a_sync_back(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf, size_t len);

/**
 * IPC rx buffer management
 */
int rwnx_ipc_rxbuf_init(struct rwnx_hw *rwnx_hw, uint32_t rx_bufsz);
int rwnx_ipc_rxbuf_alloc(struct rwnx_hw *rwnx_hw);
void rwnx_ipc_rxbuf_dealloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf);
void rwnx_ipc_rxbuf_repush(struct rwnx_hw *rwnx_hw,
                           struct rwnx_ipc_buf *buf);
#ifdef CONFIG_RWNX_FULLMAC
void rwnx_ipc_rxdesc_repush(struct rwnx_hw *rwnx_hw,
                            struct rwnx_ipc_buf *buf);
struct rwnx_ipc_buf *rwnx_ipc_rxbuf_from_hostid(struct rwnx_hw *rwnx_hw, u32 hostid);
#endif /* CONFIG_RWNX_FULLMAC */

int rwnx_ipc_unsuprxvec_alloc(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf);
void rwnx_ipc_unsuprxvec_repush(struct rwnx_hw *rwnx_hw, struct rwnx_ipc_buf *buf);

/**
 * IPC TX specific functions
 */
#if 0
void rwnx_ipc_txdesc_push(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
                          struct sk_buff *hostid, int hw_queue);
#endif
struct sk_buff *rwnx_ipc_get_skb_from_cfm(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_ipc_buf *buf);
void rwnx_ipc_sta_buffer_init(struct rwnx_hw *rwnx_hw, int sta_idx);
void rwnx_ipc_sta_buffer(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta, int tid, int size);
void rwnx_ipc_tx_drain(struct rwnx_hw *rwnx_hw);
bool rwnx_ipc_tx_pending(struct rwnx_hw *rwnx_hw);

/**
 * FW dump handler / trace
 */
void rwnx_error_ind(struct rwnx_hw *rwnx_hw);
void rwnx_umh_done(struct rwnx_hw *rwnx_hw);
void *rwnx_ipc_fw_trace_desc_get(struct rwnx_hw *rwnx_hw);

#endif /* _RWNX_IPC_UTILS_H_ */

