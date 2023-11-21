/**
 * rwnx_utils.c
 *
 * IPC utility function definitions
 *
 * Copyright (C) RivieraWaves 2012-2019
 */
#include "rwnx_utils.h"
#include "rwnx_defs.h"
#include "rwnx_rx.h"
#include "rwnx_tx.h"
#include "rwnx_msg_rx.h"
#include "rwnx_debugfs.h"
#include "rwnx_prof.h"
#include "ipc_host.h"

#ifdef CONFIG_RWNX_FULLMAC
#define FW_STR  "fmac"
#endif

/**
 * rwnx_ipc_elem_pool_allocs() - Allocate and push to fw a pool of buffer.
 *
 * @rwnx_hw: Main driver structure
 * @pool: Pool to allocate
 * @nb: Size of the pool to allocate
 * @elem_size: SIze of one pool element
 * @pool_name: Name of the pool
 * @push: Function to push one pool element to fw
 *
 * This function will allocate an array to store the list of element addresses,
 * a dma pool and @nb element in the dma pool.
 * Each element is set with '0' and then push to fw using the @push function.
 * It assumes that pointer inside @ipc parameter are set to NULL at start.
 *
 * Return: 0 on success and <0 upon error. If error is returned any allocated
 * memory is NOT freed and rwnx_ipc_elem_pool_deallocs() must be called.
 */
static int rwnx_ipc_elem_pool_allocs(struct rwnx_hw *rwnx_hw,
                                     struct rwnx_ipc_elem_pool *pool,
                                     int nb, size_t elem_size, char *pool_name,
                                     int (*push)(struct ipc_host_env_tag *,
                                                 void *, uint32_t))
{
    struct rwnx_ipc_elem *buf;
    int i;

    pool->nb = 0;

    /* allocate buf array */
    pool->buf = kmalloc(nb * sizeof(struct rwnx_ipc_elem), GFP_KERNEL);
    if (!pool->buf) {
        dev_err(rwnx_hw->dev, "Allocation of buffer array for %s failed\n",
                pool_name);
        return -ENOMEM;
    }

    /* allocate dma pool */
    pool->pool = dma_pool_create(pool_name, rwnx_hw->dev, elem_size,
                                 cache_line_size(), 0);
    if (!pool->pool) {
        dev_err(rwnx_hw->dev, "Allocation of dma pool %s failed\n",
                pool_name);
        return -ENOMEM;
    }

    for (i = 0, buf = pool->buf; i < nb; buf++, i++) {

        /* allocate an elem */
        buf->addr = dma_pool_alloc(pool->pool, GFP_KERNEL, &buf->dma_addr);
        if (!buf->addr) {
            dev_err(rwnx_hw->dev, "Allocation of block %d/%d in %s failed\n",
                    (i + 1), nb, pool_name);
            return -ENOMEM;
        }
        pool->nb++;

        /* reset the element */
        memset(buf->addr, 0, elem_size);

        /* push it to FW */
        push(rwnx_hw->ipc_env, buf, (uint32_t)buf->dma_addr);
    }

    return 0;
}

/**
 * rwnx_ipc_elem_pool_deallocs() - Free all memory allocated for a pool
 *
 * @pool: Pool to free
 *
 * Must be call once after rwnx_ipc_elem_pool_allocs(), even if it returned
 * an error
 */
static void rwnx_ipc_elem_pool_deallocs(struct rwnx_ipc_elem_pool *pool)
{
    struct rwnx_ipc_elem *buf;
    int i;

    for (i = 0, buf = pool->buf; i < pool->nb ; buf++, i++) {
        dma_pool_free(pool->pool, buf->addr, buf->dma_addr);
    }
    pool->nb = 0;

    if (pool->pool)
        dma_pool_destroy(pool->pool);
    pool->pool = NULL;

    if (pool->buf)
        kfree(pool->buf);
    pool->buf = NULL;
}

/**
 * rwnx_ipc_elem_var_allocs - Alloc a single ipc buffer and push it to fw
 *
 * @rwnx_hw: Main driver structure
 * @elem: Element to allocate
 * @elem_size: Size of the element to allcoate
 * @dir: DMA direction
 * @buf: If not NULL, used this buffer instead of allocating a new one. It must
 * be @elem_size long and be allocated by kmalloc as kfree will be called.
 * @init: Pointer to initial data to write in buffer before DMA sync. Needed
 * only if direction is DMA_TO_DEVICE. If set it is assume that its size is
 * @elem_size.
 * @push: Function to push the element to fw. May be set to NULL.
 *
 * It allocates a buffer (or use the one provided with @buf), initializes it if
 * @init is set, map buffer for DMA transfer, initializes @elem and push buffer
 * to FW if @push is seet.
 *
 * Return: 0 on success and <0 upon error. If error is returned any allocated
 * memory has been freed (including @buf if set).
 */
int rwnx_ipc_elem_var_allocs(struct rwnx_hw *rwnx_hw,
                             struct rwnx_ipc_elem_var *elem, size_t elem_size,
                             enum dma_data_direction dir,
                             void *buf, const void *init,
                             void (*push)(struct ipc_host_env_tag *, uint32_t))
{
    if (buf) {
        elem->addr = buf;
    } else {
        elem->addr = kmalloc(elem_size, GFP_KERNEL);
        if (!elem->addr) {
            dev_err(rwnx_hw->dev, "Allocation of ipc buffer failed\n");
            return -ENOMEM;
        }
    }
    elem->size = elem_size;

    if ((dir == DMA_TO_DEVICE) && init) {
        memcpy(elem->addr, init, elem_size);
    }

    elem->dma_addr = dma_map_single(rwnx_hw->dev, elem->addr, elem_size, dir);
    if (dma_mapping_error(rwnx_hw->dev, elem->dma_addr)) {
        dev_err(rwnx_hw->dev, "DMA mapping failed\n");
        kfree(elem->addr);
        elem->addr = NULL;
        return -EIO;
    }

    if (push)
        push(rwnx_hw->ipc_env, elem->dma_addr);
    return 0;
}

/**
 * rwnx_ipc_elem_var_deallocs() - Free memory allocated for a single ipc buffer
 *
 * @rwnx_hw: Main driver structure
 * @elem: Element to free
 */
void rwnx_ipc_elem_var_deallocs(struct rwnx_hw *rwnx_hw,
                                struct rwnx_ipc_elem_var *elem)
{
    if (!elem->addr)
        return;
    dma_unmap_single(rwnx_hw->dev, elem->dma_addr, elem->size, DMA_TO_DEVICE);
    kfree(elem->addr);
    elem->addr = NULL;
}

/**
 * rwnx_ipc_skb_elem_allocs() - Allocate and push a skb buffer for the FW
 *
 * @rwnx_hw: Main driver data
 * @elem: Pointer to the skb elem that will contain the address of the buffer
 */
int rwnx_ipc_skb_elem_allocs(struct rwnx_hw *rwnx_hw,
                                 struct rwnx_ipc_skb_elem *elem, size_t skb_size,
                                 enum dma_data_direction dir,
                                 int (*push)(struct ipc_host_env_tag *,
                                             void *, uint32_t))
{
    elem->skb = dev_alloc_skb(skb_size);
    if (unlikely(!elem->skb)) {
        dev_err(rwnx_hw->dev, "Allocation of ipc skb failed\n");
        return -ENOMEM;
    }

    elem->dma_addr = dma_map_single(rwnx_hw->dev, elem->skb->data, skb_size, dir);
    if (unlikely(dma_mapping_error(rwnx_hw->dev, elem->dma_addr))) {
        dev_err(rwnx_hw->dev, "DMA mapping failed\n");
        dev_kfree_skb(elem->skb);
        elem->skb = NULL;
        return -EIO;
    }

    if (push){
        push(rwnx_hw->ipc_env, elem, elem->dma_addr);
    }
    return 0;
}

/**
 * rwnx_ipc_skb_elem_deallocs() - Free a skb buffer allocated for the FW
 *
 * @rwnx_hw: Main driver data
 * @elem: Pointer to the skb elem that contains the address of the buffer
 * @skb_size: size of the skb buffer data
 * @dir: DMA direction
 */
static void rwnx_ipc_skb_elem_deallocs(struct rwnx_hw *rwnx_hw,
                                       struct rwnx_ipc_skb_elem *elem,
                                       size_t skb_size, enum dma_data_direction dir) {
    if (elem->skb) {
        dma_unmap_single(rwnx_hw->dev, elem->dma_addr, skb_size, dir);
        dev_kfree_skb(elem->skb);
        elem->skb = NULL;
    }
}

/**
 * rwnx_ipc_unsup_rx_vec_elem_allocs() - Allocate and push an unsupported
 *                                       RX vector buffer for the FW
 *
 * @rwnx_hw: Main driver data
 * @elem: Pointer to the skb elem that will contain the address of the buffer
 */
int rwnx_ipc_unsup_rx_vec_elem_allocs(struct rwnx_hw *rwnx_hw,
                                      struct rwnx_ipc_skb_elem *elem)
{
    struct rx_vector_desc *rxdesc;

    if (rwnx_ipc_skb_elem_allocs(rwnx_hw, elem,
            rwnx_hw->ipc_env->unsuprxvec_bufsz, DMA_FROM_DEVICE, NULL))
        return -ENOMEM;

    rxdesc = (struct rx_vector_desc *) elem->skb->data;
    rxdesc->pattern = 0;
    dma_sync_single_for_device(rwnx_hw->dev,
                        elem->dma_addr + offsetof(struct rx_vector_desc, pattern),
                        sizeof(rxdesc->pattern), DMA_BIDIRECTIONAL);

    ipc_host_unsup_rx_vec_buf_push(rwnx_hw->ipc_env, elem, (u32) elem->dma_addr);

    return 0;
}

/**
 * rwnx_ipc_rxbuf_elems_deallocs() - Free all unsupported rx vector buffer
 *                                   allocated for the FW
 *
 * @rwnx_hw: Main driver data
 */
static void rwnx_ipc_unsup_rx_vec_elems_deallocs(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_ipc_skb_elem *elem;
    int i, nb = rwnx_hw->ipc_env->unsuprxvec_bufnb;

    if (!rwnx_hw->e2aunsuprxvec_elems)
        return;

    for (i = 0, elem = rwnx_hw->e2aunsuprxvec_elems; i < nb; i++, elem++) {
        rwnx_ipc_skb_elem_deallocs(rwnx_hw, elem, rwnx_hw->ipc_env->unsuprxvec_bufsz, DMA_FROM_DEVICE);
    }

    kfree(rwnx_hw->e2aunsuprxvec_elems);
    rwnx_hw->e2aunsuprxvec_elems = NULL;
}

/**
* rwnx_ipc_unsup_rx_vec_elems_allocs() - Allocate and push all unsupported RX
*                                        vector buffer for the FW
*
* @rwnx_hw: Main driver data
*/
static int rwnx_ipc_unsup_rx_vec_elems_allocs(struct rwnx_hw *rwnx_hw)
{
   struct rwnx_ipc_skb_elem *elem;
   int i, nb = rwnx_hw->ipc_env->unsuprxvec_bufnb;

   rwnx_hw->e2aunsuprxvec_elems = kzalloc(nb * sizeof(struct rwnx_ipc_skb_elem),
                                   GFP_KERNEL);
   if (!rwnx_hw->e2aunsuprxvec_elems) {
       dev_err(rwnx_hw->dev, "Failed to allocate unsuprxvec_elems\n");
       return -ENOMEM;
   }

   for (i = 0, elem = rwnx_hw->e2aunsuprxvec_elems; i < nb; i++, elem++)
   {
       if (rwnx_ipc_unsup_rx_vec_elem_allocs(rwnx_hw, elem)) {
           dev_err(rwnx_hw->dev, "Failed to allocate unsuprxvec buf %d/%d\n",
                   i + 1, nb);
           return -ENOMEM;
       }
   }

   return 0;
}

#ifdef CONFIG_RWNX_FULLMAC

/**
 * rwnx_ipc_rxdesc_elem_repush() - Repush a rxdesc to FW
 *
 * @rwnx_hw: Main driver data
 * @elem: Rx desc to repush
 *
 * Once rx buffer has been received, the rxdesc used by FW to upload this
 * buffer can be re-used for another rx buffer.
 */
void rwnx_ipc_rxdesc_elem_repush(struct rwnx_hw *rwnx_hw,
                                 struct rwnx_ipc_elem *elem)
{
    struct rxdesc_tag *rxdesc = elem->addr;
    rxdesc->status = 0;
    dma_sync_single_for_device(rwnx_hw->dev, elem->dma_addr,
                               sizeof(struct rxdesc_tag), DMA_BIDIRECTIONAL);
    ipc_host_rxdesc_push(rwnx_hw->ipc_env, elem, (u32)elem->dma_addr);
}

/**
 * rwnx_ipc_rxbuf_elem_allocs() - Allocate and push a RX buffer for the FW
 *
 * @rwnx_hw: Main driver data
 */
int rwnx_ipc_rxbuf_elem_allocs(struct rwnx_hw *rwnx_hw)
{
    struct sk_buff *skb;
    struct hw_rxhdr *hw_rxhdr;
    dma_addr_t dma_addr;
    int size = rwnx_hw->ipc_env->rx_bufsz;
    int nb, idx;

    skb = dev_alloc_skb(size);
    if (unlikely(!skb)) {
        dev_err(rwnx_hw->dev, "Failed to allocate rx buffer\n");
        return -ENOMEM;
    }

    dma_addr = dma_map_single(rwnx_hw->dev, skb->data, size, DMA_FROM_DEVICE);

    if (unlikely(dma_mapping_error(rwnx_hw->dev, dma_addr))) {
        dev_err(rwnx_hw->dev, "Failed to map rx buffer\n");
        goto err_skb;
    }

    hw_rxhdr = (struct hw_rxhdr *)skb->data;
    hw_rxhdr->pattern = 0;
    dma_sync_single_for_device(rwnx_hw->dev,
                               dma_addr + offsetof(struct hw_rxhdr, pattern),
                               sizeof(hw_rxhdr->pattern), DMA_BIDIRECTIONAL);

    /* Find first free slot */
    nb = 0;
    idx = rwnx_hw->rxbuf_elems.idx;
    while (rwnx_hw->rxbuf_elems.skb[idx] && nb < RWNX_RXBUFF_MAX) {
        idx = ( idx + 1 ) % RWNX_RXBUFF_MAX;
        nb++;
    }

    if (WARN((nb == RWNX_RXBUFF_MAX), "No more free space for rxbuff")) {
        goto err_dma;
    }

    rwnx_hw->rxbuf_elems.skb[idx] = skb;

    /* Save info in skb control buffer  */
    RWNX_RXBUFF_DMA_ADDR_SET(skb, dma_addr);
    RWNX_RXBUFF_PATTERN_SET(skb, rwnx_rxbuff_pattern);
    RWNX_RXBUFF_IDX_SET(skb, idx);

    /* Push buffer to FW */
    ipc_host_rxbuf_push(rwnx_hw->ipc_env, RWNX_RXBUFF_IDX_TO_HOSTID(idx),
                        dma_addr);

    /* Save idx so that on next push the free slot will be found quicker */
    rwnx_hw->rxbuf_elems.idx = ( idx + 1 ) % RWNX_RXBUFF_MAX;

    return 0;

  err_dma:
    dma_unmap_single(rwnx_hw->dev, dma_addr, size, DMA_FROM_DEVICE);
  err_skb:
    dev_kfree_skb(skb);
    return -ENOMEM;
}

/**
 * rwnx_ipc_rxbuf_elem_repush() - Repush a rxbuf to FW
 *
 * @rwnx_hw: Main driver data
 * @skb: Skb to repush
 *
 * In case a skb is not forwarded to upper layer it can be re-used.
 * It is assumed that @skb has been verified before calling this function and
 * that it is a valid rx buffer
 * (i.e. skb == rwnx_hw->rxbuf_elems.skb[RWNX_RXBUFF_IDX_GET(skb)])
 */
void rwnx_ipc_rxbuf_elem_repush(struct rwnx_hw *rwnx_hw,
                                struct sk_buff *skb)
{
    dma_addr_t dma_addr;
    struct hw_rxhdr *hw_rxhdr = (struct hw_rxhdr *)skb->data;
    int idx;

    /* reset pattern */
    hw_rxhdr->pattern = 0;
    dma_addr = RWNX_RXBUFF_DMA_ADDR_GET(skb);
    dma_sync_single_for_device(rwnx_hw->dev,
                               dma_addr + offsetof(struct hw_rxhdr, pattern),
                               sizeof(hw_rxhdr->pattern), DMA_BIDIRECTIONAL);

    /* re-push buffer to FW */
    idx = RWNX_RXBUFF_IDX_GET(skb);
    ipc_host_rxbuf_push(rwnx_hw->ipc_env, RWNX_RXBUFF_IDX_TO_HOSTID(idx),
                        dma_addr);
}

/**
 * rwnx_ipc_rxbuf_elems_allocs() - Allocate and push all RX buffer for the FW
 *
 * @rwnx_hw: Main driver data
 */
static int rwnx_ipc_rxbuf_elems_allocs(struct rwnx_hw *rwnx_hw)
{
    int i, nb = rwnx_hw->ipc_env->rx_bufnb;

    for (i = 0; i < RWNX_RXBUFF_MAX; i++) {
        rwnx_hw->rxbuf_elems.skb[i] = NULL;
    }
    rwnx_hw->rxbuf_elems.idx = 0;

    for (i = 0; i < nb; i++) {
        if (rwnx_ipc_rxbuf_elem_allocs(rwnx_hw)) {
            dev_err(rwnx_hw->dev, "Failed to allocate rx buf %d/%d\n",
                    i + 1, nb);
            return -ENOMEM;
        }
    }
    return 0;
}

/**
 * rwnx_ipc_rxbuf_elems_deallocs() - Free all RX buffer allocated for the FW
 *
 * @rwnx_hw: Main driver data
 */
static void rwnx_ipc_rxbuf_elems_deallocs(struct rwnx_hw *rwnx_hw)
{
    struct sk_buff *skb;
    int i;

    for (i = 0; i < RWNX_RXBUFF_MAX; i++) {
        if (rwnx_hw->rxbuf_elems.skb[i]) {
            skb = rwnx_hw->rxbuf_elems.skb[i];
            dma_unmap_single(rwnx_hw->dev, RWNX_RXBUFF_DMA_ADDR_GET(skb),
                             rwnx_hw->ipc_env->rx_bufsz, DMA_FROM_DEVICE);
            dev_kfree_skb(skb);
            rwnx_hw->rxbuf_elems.skb[i] = NULL;
        }
    }
}

/**
 * rwnx_ipc_rxbuf_elem_pull() - Extract a skb from local table
 *
 * @rwnx_hw: Main driver data
 * @skb: SKb to extract for table
 *
 * After checking that skb is actually a pointer of local table, extract it
 * from the table.
 * When buffer is removed, DMA mapping is remove which has the effect to
 * synchronize the buffer for the cpu.
 * To be called before passing skb to upper layer.
 */
void rwnx_ipc_rxbuf_elem_pull(struct rwnx_hw *rwnx_hw, struct sk_buff *skb)
{
    unsigned int idx = RWNX_RXBUFF_IDX_GET(skb);

    if (RWNX_RXBUFF_VALID_IDX(idx) && (rwnx_hw->rxbuf_elems.skb[idx] == skb)) {
        dma_addr_t dma_addr = RWNX_RXBUFF_DMA_ADDR_GET(skb);
        rwnx_hw->rxbuf_elems.skb[idx] = NULL;
        dma_unmap_single(rwnx_hw->dev, dma_addr,
                         rwnx_hw->ipc_env->rx_bufsz, DMA_FROM_DEVICE);
    } else {
        WARN(1, "Incorrect rxbuff idx skb=%p table[%u]=%p", skb, idx,
             idx < RWNX_RXBUFF_MAX ? rwnx_hw->rxbuf_elems.skb[idx] : NULL);
    }

    /* Reset the pattern and idx */
    RWNX_RXBUFF_PATTERN_SET(skb, 0);
    RWNX_RXBUFF_IDX_SET(skb, RWNX_RXBUFF_MAX);
}

/**
 * rwnx_ipc_rxbuf_elem_sync() - Sync part of a RX buffer
 *
 * @rwnx_hw: Main driver data
 * @skb: SKb to sync
 * @len: Len to sync
 *
 * After checking that skb is actually a pointer of local table, sync @p len
 * bytes of the buffer for CPU. Buffer is not removed from the table
 */
void rwnx_ipc_rxbuf_elem_sync(struct rwnx_hw *rwnx_hw, struct sk_buff *skb,
                              int len)
{
    unsigned int idx = RWNX_RXBUFF_IDX_GET(skb);

    if (RWNX_RXBUFF_VALID_IDX(idx) && (rwnx_hw->rxbuf_elems.skb[idx] == skb)) {
        dma_addr_t dma_addr = RWNX_RXBUFF_DMA_ADDR_GET(skb);
        dma_sync_single_for_cpu(rwnx_hw->dev, dma_addr, len, DMA_FROM_DEVICE);
    } else {
        WARN(1, "Incorrect rxbuff idx skb=%p table[%u]=%p", skb, idx,
             idx < RWNX_RXBUFF_MAX ? rwnx_hw->rxbuf_elems.skb[idx] : NULL);
    }
}

#endif /* ! CONFIG_RWNX_FULLMAC */

/**
 * rwnx_elems_deallocs() - Deallocate IPC storage elements.
 * @rwnx_hw: Main driver data
 *
 * This function deallocates all the elements required for communications with
 * LMAC, such as Rx Data elements, MSGs elements, ...
 * This function should be called in correspondence with the allocation function.
 */
static void rwnx_elems_deallocs(struct rwnx_hw *rwnx_hw)
{
    rwnx_ipc_rxbuf_elems_deallocs(rwnx_hw);
    rwnx_ipc_unsup_rx_vec_elems_deallocs(rwnx_hw);
#ifdef CONFIG_RWNX_FULLMAC
    rwnx_ipc_elem_pool_deallocs(&rwnx_hw->e2arxdesc_pool);
#endif
    rwnx_ipc_elem_pool_deallocs(&rwnx_hw->e2amsgs_pool);
    rwnx_ipc_elem_pool_deallocs(&rwnx_hw->dbgmsgs_pool);
    rwnx_ipc_elem_pool_deallocs(&rwnx_hw->e2aradars_pool);
    rwnx_ipc_elem_var_deallocs(rwnx_hw, &rwnx_hw->pattern_elem);
    rwnx_ipc_elem_var_deallocs(rwnx_hw, &rwnx_hw->dbgdump_elem.buf);
}

/**
 * rwnx_elems_allocs() - Allocate IPC storage elements.
 * @rwnx_hw: Main driver data
 *
 * This function allocates all the elements required for communications with
 * LMAC, such as Rx Data elements, MSGs elements, ...
 * This function should be called in correspondence with the deallocation function.
 */
static int rwnx_elems_allocs(struct rwnx_hw *rwnx_hw)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (dma_set_coherent_mask(rwnx_hw->dev, DMA_BIT_MASK(32)))
        goto err_alloc;

    if (rwnx_ipc_elem_pool_allocs(rwnx_hw, &rwnx_hw->e2amsgs_pool,
                                  rwnx_hw->ipc_env->ipc_e2amsg_bufnb,
                                  rwnx_hw->ipc_env->ipc_e2amsg_bufsz,
                                  "rwnx_ipc_e2amsgs_pool",
                                  ipc_host_msgbuf_push))
        goto err_alloc;

    if (rwnx_ipc_elem_pool_allocs(rwnx_hw, &rwnx_hw->dbgmsgs_pool,
                                  rwnx_hw->ipc_env->ipc_dbg_bufnb,
                                  rwnx_hw->ipc_env->ipc_dbg_bufsz,
                                  "rwnx_ipc_dbgmsgs_pool",
                                  ipc_host_dbgbuf_push))
        goto err_alloc;

    if (rwnx_ipc_elem_pool_allocs(rwnx_hw, &rwnx_hw->e2aradars_pool,
                                  rwnx_hw->ipc_env->radar_bufnb,
                                  rwnx_hw->ipc_env->radar_bufsz,
                                  "rwnx_ipc_e2aradars_pool",
                                  ipc_host_radarbuf_push))
        goto err_alloc;

    if (rwnx_ipc_unsup_rx_vec_elems_allocs(rwnx_hw))
        goto err_alloc;

    if (rwnx_ipc_elem_var_allocs(rwnx_hw, &rwnx_hw->pattern_elem,
                                 sizeof(u32), DMA_TO_DEVICE,
                                 NULL, &rwnx_rxbuff_pattern,
                                 ipc_host_patt_addr_push))
        goto err_alloc;

    if (rwnx_ipc_elem_var_allocs(rwnx_hw, &rwnx_hw->dbgdump_elem.buf,
                                 sizeof(struct dbg_debug_dump_tag),
                                 DMA_FROM_DEVICE, NULL, NULL,
                                 ipc_host_dbginfobuf_push))
        goto err_alloc;

    /*
     * Note that the RX buffers are no longer allocated here as their size depends on the
     * FW configuration, which is not available at that time.
     * They will be allocated when checking the parameter compatibility between the driver
     * and the underlying components (i.e. during the rwnx_handle_dynparams() execution)
     */

#ifdef CONFIG_RWNX_FULLMAC
    if (rwnx_ipc_elem_pool_allocs(rwnx_hw, &rwnx_hw->e2arxdesc_pool,
                                  rwnx_hw->ipc_env->rxdesc_nb,
                                  sizeof(struct rxdesc_tag),
                                  "rwnx_ipc_e2arxdesc_pool",
                                  ipc_host_rxdesc_push))
        goto err_alloc;

#endif /* CONFIG_RWNX_FULLMAC */

    return 0;

err_alloc:
    rwnx_elems_deallocs(rwnx_hw);
    return -ENOMEM;
}

/**
 * rwnx_ipc_msg_push() - Push a msg to IPC queue
 *
 * @rwnx_hw: Main driver data
 * @msg_buf: Pointer to message
 * @len: Size, in bytes, of message
 */
void rwnx_ipc_msg_push(struct rwnx_hw *rwnx_hw, void *msg_buf, uint16_t len)
{
    ipc_host_msg_push(rwnx_hw->ipc_env, msg_buf, len);
}

/**
 * rwnx_ipc_txdesc_push() - Push a txdesc to FW
 *
 * @rwnx_hw: Main driver data
 * @tx_desc: Pointer on &struct txdesc_api to push to FW
 * @hostid: Pointer save in ipc env to retrieve tx buffer upon confirmation.
 * @hw_queue: Hw queue to push txdesc to
 * @user: User position to push the txdesc to. It must be set to 0 if  MU-MIMMO
 * is not used.
 */
void rwnx_ipc_txdesc_push(struct rwnx_hw *rwnx_hw, void *tx_desc,
                          void *hostid, int hw_queue, int user)
{
    volatile struct txdesc_host *txdesc_host;

    txdesc_host = ipc_host_txdesc_get(rwnx_hw->ipc_env, hw_queue, user);
    BUG_ON(!txdesc_host);
#if 0
    /* check potential platform bug on multiple stores */
    memcpy(&txdesc_host->api, tx_desc, sizeof(*desc));
#else
    {
        u32 *src, *dst;
        int i;
        dst = (typeof(dst))&txdesc_host->api;
        src = (typeof(src))tx_desc;
        for (i = 0; i < sizeof(txdesc_host->api) / sizeof(*src); i++)
            *dst++ = *src++;
    }
#endif
    wmb(); /* vs desc */
    ipc_host_txdesc_push(rwnx_hw->ipc_env, hw_queue, user, hostid);
}

/**
 * rwnx_ipc_fw_trace_desc_get() - Return pointer to the start of trace
 * description in IPC environment
 *
 * @rwnx_hw: Main driver data
 */
void *rwnx_ipc_fw_trace_desc_get(struct rwnx_hw *rwnx_hw)
{
    return (void *)&(rwnx_hw->ipc_env->shared->trace_pattern);
}

/**
 * rwnx_ipc_sta_buffer_init - Initialize counter of bufferred data for a given sta
 *
 * @rwnx_hw: Main driver data
 * @sta_idx: Index of the station to initialize
 */
void rwnx_ipc_sta_buffer_init(struct rwnx_hw *rwnx_hw, int sta_idx)
{
#if 0
    int i;
    volatile u32_l *buffered;

    if (sta_idx >= NX_REMOTE_STA_MAX)
        return;

    buffered = rwnx_hw->ipc_env->shared->buffered[sta_idx];

    for (i = 0; i < TID_MAX; i++) {
        *buffered++ = 0;
    }
#endif
}

/**
 * rwnx_ipc_sta_buffer - Update counter of bufferred data for a given sta
 *
 * @rwnx_hw: Main driver data
 * @sta: Managed station
 * @tid: TID on which data has been added or removed
 * @size: Size of data to add (or remove if < 0) to STA buffer.
 */
void rwnx_ipc_sta_buffer(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta, int tid, int size)
{
#if 0
    u32_l *buffered;
#endif
    if (!sta)
        return;

    if ((sta->sta_idx >= NX_REMOTE_STA_MAX) || (tid >= TID_MAX))
        return;

#if 0
    buffered = &rwnx_hw->ipc_env->shared->buffered[sta->sta_idx][tid];

    if (size < 0) {
        size = -size;
        if (*buffered < size)
            *buffered = 0;
        else
            *buffered -= size;
    } else {
        // no test on overflow
        *buffered += size;
    }
#endif
}

/**
 * rwnx_msgind() - IRQ handler callback for %IPC_IRQ_E2A_MSG
 *
 * @pthis: Pointer to main driver data
 * @hostid: Pointer to IPC elem from e2amsgs_pool
 */
static u8 rwnx_msgind(void *pthis, void *hostid)
{
    struct rwnx_hw *rwnx_hw = pthis;
    struct rwnx_ipc_elem *elem = hostid;
    struct ipc_e2a_msg *msg = elem->addr;
    u8 ret = 0;

    REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_MSGIND);

    /* Look for pattern which means that this hostbuf has been used for a MSG */
    if (msg->pattern != IPC_MSGE2A_VALID_PATTERN) {
        ret = -1;
        goto msg_no_push;
    }
    /* Relay further actions to the msg parser */
    rwnx_rx_handle_msg(rwnx_hw, msg);

    /* Reset the msg element and re-use it */
    msg->pattern = 0;
    wmb();

    /* Push back the buffer to the LMAC */
    ipc_host_msgbuf_push(rwnx_hw->ipc_env, elem, (u32)elem->dma_addr);

msg_no_push:
    REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_MSGIND);
    return ret;
}

/**
 * rwnx_msgackind() - IRQ handler callback for %IPC_IRQ_E2A_MSG_ACK
 *
 * @pthis: Pointer to main driver data
 * @hostid: Pointer to command acknoledged
 */
static u8 rwnx_msgackind(void *pthis, void *hostid)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)pthis;
    rwnx_hw->cmd_mgr->llind(rwnx_hw->cmd_mgr, (struct rwnx_cmd *)hostid);
    return -1;
}

/**
 * rwnx_radarind() - IRQ handler callback for %IPC_IRQ_E2A_RADAR
 *
 * @pthis: Pointer to main driver data
 * @hostid: Pointer to IPC elem from e2aradars_pool
 */
static u8 rwnx_radarind(void *pthis, void *hostid)
{
#ifdef CONFIG_RWNX_RADAR
    struct rwnx_hw *rwnx_hw = pthis;
    struct rwnx_ipc_elem *elem = hostid;
    struct radar_pulse_array_desc *pulses = elem->addr;
    u8 ret = 0;
    int i;

    /* Look for pulse count meaning that this hostbuf contains RADAR pulses */
    if (pulses->cnt == 0) {
        ret = -1;
        goto radar_no_push;
    }

    if (rwnx_radar_detection_is_enable(&rwnx_hw->radar, pulses->idx)) {
        /* Save the received pulses only if radar detection is enabled */
        for (i = 0; i < pulses->cnt; i++) {
            struct rwnx_radar_pulses *p = &rwnx_hw->radar.pulses[pulses->idx];

            p->buffer[p->index] = pulses->pulse[i];
            p->index = (p->index + 1) % RWNX_RADAR_PULSE_MAX;
            if (p->count < RWNX_RADAR_PULSE_MAX)
                p->count++;
        }

        /* Defer pulse processing in separate work */
        if (! work_pending(&rwnx_hw->radar.detection_work))
            schedule_work(&rwnx_hw->radar.detection_work);
    }

    /* Reset the radar element and re-use it */
    pulses->cnt = 0;
    wmb();

    /* Push back the buffer to the LMAC */
    ipc_host_radarbuf_push(rwnx_hw->ipc_env, elem, (u32)elem->dma_addr);

radar_no_push:
    return ret;
#else
    return -1;
#endif
}

/**
 * rwnx_prim_tbtt_ind() - IRQ handler callback for %IPC_IRQ_E2A_TBTT_PRIM
 *
 * @pthis: Pointer to main driver data
 */
static void rwnx_prim_tbtt_ind(void *pthis)
{
#if 0
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)pthis;
    rwnx_tx_bcns(rwnx_hw);
#endif
}

/**
 * rwnx_sec_tbtt_ind() - IRQ handler callback for %IPC_IRQ_E2A_TBTT_SEC
 *
 * @pthis: Pointer to main driver data
 */
static void rwnx_sec_tbtt_ind(void *pthis)
{
}

/**
 * rwnx_dbgind() - IRQ handler callback for %IPC_IRQ_E2A_DBG
 *
 * @pthis: Pointer to main driver data
 * @hostid: Pointer to IPC elem from dbgmsgs_pool
 */
static u8 rwnx_dbgind(void *pthis, void *hostid)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)pthis;
    struct rwnx_ipc_elem *elem = hostid;
    struct ipc_dbg_msg *dbg_msg = elem->addr;
    u8 ret = 0;

    REG_SW_SET_PROFILING(rwnx_hw, SW_PROF_DBGIND);

    /* Look for pattern which means that this hostbuf has been used for a MSG */
    if (dbg_msg->pattern != IPC_DBG_VALID_PATTERN) {
        ret = -1;
        goto dbg_no_push;
    }

    /* Display the string */
    printk("%s %s", (char *)FW_STR, (char *)dbg_msg->string);

    /* Reset the msg element and re-use it */
    dbg_msg->pattern = 0;
    wmb();

    /* Push back the buffer to the LMAC */
    ipc_host_dbgbuf_push(rwnx_hw->ipc_env, elem, (u32)elem->dma_addr);

dbg_no_push:
    REG_SW_CLEAR_PROFILING(rwnx_hw, SW_PROF_DBGIND);

    return ret;
}

/**
 * rwnx_ipc_rxbuf_init() - Allocate and initialize RX buffers.
 *
 * @rwnx_hw: Main driver data
 * @rx_bufsz: Size of the buffer to be allocated
 *
 * This function updates the RX buffer size according to the parameter and allocates the
 * RX buffers
 */
int rwnx_ipc_rxbuf_init(struct rwnx_hw *rwnx_hw, uint32_t rx_bufsz)
{
    rwnx_hw->ipc_env->rx_bufsz = rx_bufsz;
    return(rwnx_ipc_rxbuf_elems_allocs(rwnx_hw));
}

/**
 * rwnx_ipc_init() - Initialize IPC interface.
 *
 * @rwnx_hw: Main driver data
 * @shared_ram: Pointer to shared memory that contains IPC shared struct
 *
 * This function initializes IPC interface by registering callbacks, setting
 * shared memory area and calling IPC Init function.
 * It should be called only once during driver's lifetime.
 */
int rwnx_ipc_init(struct rwnx_hw *rwnx_hw, u8 *shared_ram)
{
    struct ipc_host_cb_tag cb;
    int res;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    /* initialize the API interface */
    cb.recv_data_ind   = rwnx_rxdataind;
    cb.recv_radar_ind  = rwnx_radarind;
    cb.recv_msg_ind    = rwnx_msgind;
    cb.recv_msgack_ind = rwnx_msgackind;
    cb.recv_dbg_ind    = rwnx_dbgind;
    cb.send_data_cfm   = rwnx_txdatacfm;
    cb.prim_tbtt_ind   = rwnx_prim_tbtt_ind;
    cb.sec_tbtt_ind    = rwnx_sec_tbtt_ind;
    cb.recv_unsup_rx_vec_ind = rwnx_unsup_rx_vec_ind;

    /* set the IPC environment */
    rwnx_hw->ipc_env = (struct ipc_host_env_tag *)
                       kzalloc(sizeof(struct ipc_host_env_tag), GFP_KERNEL);

    if (!rwnx_hw->ipc_env)
        return -ENOMEM;

    /* call the initialization of the IPC */
    ipc_host_init(rwnx_hw->ipc_env, &cb,
                  (struct ipc_shared_env_tag *)shared_ram, rwnx_hw);

    rwnx_cmd_mgr_init(rwnx_hw->cmd_mgr);

    res = rwnx_elems_allocs(rwnx_hw);
    if (res) {
        kfree(rwnx_hw->ipc_env);
        rwnx_hw->ipc_env = NULL;
    }

    return res;
}

/**
 * rwnx_ipc_deinit() - Release IPC interface
 *
 * @rwnx_hw: Main driver data
 */
void rwnx_ipc_deinit(struct rwnx_hw *rwnx_hw)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    rwnx_ipc_tx_drain(rwnx_hw);
    rwnx_cmd_mgr_deinit(rwnx_hw->cmd_mgr);
    rwnx_elems_deallocs(rwnx_hw);
    if (rwnx_hw->ipc_env) {
        kfree(rwnx_hw->ipc_env);
        rwnx_hw->ipc_env = NULL;
    }
}

/**
 * rwnx_ipc_start() - Start IPC interface
 *
 * @rwnx_hw: Main driver data
 */
void rwnx_ipc_start(struct rwnx_hw *rwnx_hw)
{
    ipc_host_enable_irq(rwnx_hw->ipc_env, IPC_IRQ_E2A_ALL);
}

/**
 * rwnx_ipc_stop() - Stop IPC interface
 *
 * @rwnx_hw: Main driver data
 */
void rwnx_ipc_stop(struct rwnx_hw *rwnx_hw)
{
    ipc_host_disable_irq(rwnx_hw->ipc_env, IPC_IRQ_E2A_ALL);
}

/**
 * rwnx_ipc_tx_drain() - Flush IPC TX buffers
 *
 * @rwnx_hw: Main driver data
 *
 * This assumes LMAC is still (tx wise) and there's no TX race until LMAC is up
 * tx wise.
 * This also lets both IPC sides remain in sync before resetting the LMAC,
 * e.g with rwnx_send_reset.
 */
void rwnx_ipc_tx_drain(struct rwnx_hw *rwnx_hw)
{
    int i, j;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    if (!rwnx_hw->ipc_env) {
        printk(KERN_CRIT "%s: bypassing (restart must have failed)\n", __func__);
        return;
    }

    for (i = 0; i < RWNX_HWQ_NB; i++) {
        for (j = 0; j < nx_txuser_cnt[i]; j++) {
            struct sk_buff *skb;
            while ((skb = (struct sk_buff *)ipc_host_tx_flush(rwnx_hw->ipc_env, i, j))) {
                struct rwnx_sw_txhdr *sw_txhdr =
                    ((struct rwnx_txhdr *)skb->data)->sw_hdr;
                dma_unmap_single(rwnx_hw->dev, sw_txhdr->dma_addr,
                                 sw_txhdr->map_len, DMA_TO_DEVICE);
                skb_pull(skb, sw_txhdr->headroom);
#ifdef CONFIG_RWNX_FULLMAC
                dev_kfree_skb_any(skb);
#endif /* CONFIG_RWNX_FULLMAC */
            }
        }
    }
}

/**
 * rwnx_ipc_tx_pending() - Check if TX pframes are pending at FW level
 *
 * @rwnx_hw: Main driver data
 */
bool rwnx_ipc_tx_pending(struct rwnx_hw *rwnx_hw)
{
    return ipc_host_tx_frames_pending(rwnx_hw->ipc_env);
}

/**
 * rwnx_error_ind() - %DBG_ERROR_IND message callback
 *
 * @rwnx_hw: Main driver data
 *
 * This function triggers the UMH script call that will indicate to the user
 * space the error that occurred and stored the debug dump. Once the UMH script
 * is executed, the rwnx_umh_done() function has to be called.
 */
void rwnx_error_ind(struct rwnx_hw *rwnx_hw)
{
    struct rwnx_ipc_elem_var *elem = &rwnx_hw->dbgdump_elem.buf;
    struct dbg_debug_dump_tag *dump = elem->addr;

    dma_sync_single_for_device(rwnx_hw->dev, elem->dma_addr, elem->size,
                               DMA_FROM_DEVICE);
    dev_err(rwnx_hw->dev, "(type %d): dump received\n",
            dump->dbg_info.error_type);
#ifdef CONFIG_RWNX_DEBUGFS
    rwnx_hw->debugfs.trace_prst = true;
#endif
    rwnx_trigger_um_helper(&rwnx_hw->debugfs);
}

/**
 * rwnx_umh_done() - Indicate User Mode helper finished
 *
 * @rwnx_hw: Main driver data
 *
 */
void rwnx_umh_done(struct rwnx_hw *rwnx_hw)
{
    if (!test_bit(RWNX_DEV_STARTED, &rwnx_hw->drv_flags))
        return;

    /* this assumes error_ind won't trigger before ipc_host_dbginfobuf_push
       is called and so does not irq protect (TODO) against error_ind */
#ifdef CONFIG_RWNX_DEBUGFS
    rwnx_hw->debugfs.trace_prst = false;
#endif
    ipc_host_dbginfobuf_push(rwnx_hw->ipc_env, rwnx_hw->dbgdump_elem.buf.dma_addr);
}

int rwnx_init_aic(struct rwnx_hw *rwnx_hw)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);
#ifdef AICWF_SDIO_SUPPORT
	aicwf_sdio_host_init(&(rwnx_hw->sdio_env), NULL, NULL, rwnx_hw);
#else
	aicwf_usb_host_init(&(rwnx_hw->usb_env), NULL, NULL, rwnx_hw);
#endif
    rwnx_cmd_mgr_init(rwnx_hw->cmd_mgr);

    return 0;
}

