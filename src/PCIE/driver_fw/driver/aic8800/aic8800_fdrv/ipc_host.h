/**
 ******************************************************************************
 *
 * @file ipc_host.h
 *
 * @brief IPC module.
 *
 * Copyright (C) RivieraWaves 2011-2021
 *
 ******************************************************************************
 */
#ifndef _IPC_HOST_H_
#define _IPC_HOST_H_

/*
 * INCLUDE FILES
 ******************************************************************************
 */
#include "ipc_shared.h"
#include "ipc_compat.h"

/*
 * ENUMERATION
 ******************************************************************************
 */

enum ipc_host_desc_status
{
    /// Descriptor is IDLE
    IPC_HOST_DESC_IDLE      = 0,
    /// Data can be forwarded
    IPC_HOST_DESC_FORWARD,
    /// Data has to be kept in UMAC memory
    IPC_HOST_DESC_KEEP,
    /// Delete stored packet
    IPC_HOST_DESC_DELETE,
    /// Update Frame Length status
    IPC_HOST_DESC_LEN_UPDATE,
};

/**
 ******************************************************************************
 * @brief This structure is used to initialize the MAC SW
 *
 * The WLAN device driver provides functions call-back with this structure
 ******************************************************************************
 */
struct ipc_host_cb_tag
{
    /// WLAN driver call-back function: send_data_cfm
    int (*send_data_cfm)(void *pthis, void *host_id,u8 free);

    /// WLAN driver call-back function: recv_data_ind
    uint8_t (*recv_data_ind)(void *pthis, void *host_id);

    /// WLAN driver call-back function: recv_radar_ind
    uint8_t (*recv_radar_ind)(void *pthis, void *host_id);

    /// WLAN driver call-back function: recv_unsup_rx_vec_ind
    uint8_t (*recv_unsup_rx_vec_ind)(void *pthis, void *host_id);

    /// WLAN driver call-back function: recv_msg_ind
    uint8_t (*recv_msg_ind)(void *pthis, void *host_id);

    /// WLAN driver call-back function: recv_msgack_ind
    uint8_t (*recv_msgack_ind)(void *pthis, void *host_id);

    /// WLAN driver call-back function: recv_dbg_ind
    uint8_t (*recv_dbg_ind)(void *pthis, void *host_id);

};

/// Struct used to associate a local pointer with a shared 32bits value
struct ipc_hostid
{
    struct list_head list;
    void *hostptr;   ///< local pointer
    uint32_t hostid; ///< associated value shared over IPC
};

#define PCIE_TXQUEUE_CNT     NX_TXQ_CNT
#define PCIE_TXDESC_CNT      NX_TXDESC_CNT

/// Definition of the IPC Host environment structure.
struct ipc_host_env_tag
{
    /// Structure containing the callback pointers
    struct ipc_host_cb_tag cb;

    /// Pointer to the shared environment
    struct ipc_shared_env_tag *shared;

    /// Fields for Data Rx handling
    #ifdef CONFIG_RWNX_FULLMAC
    /// Array of RX descriptor pushed to firmware
    struct rwnx_ipc_buf *rxdesc[IPC_RXDESC_CNT];
    // Index of the next RX descriptor to be filled by the firmware
    uint8_t rxdesc_idx;
    /// Number of RX Descriptors
    uint8_t rxdesc_nb;
    #endif // CONFIG_RWNX_FULLMAC

    // Index of the next RX data buffer to write (and read for softmac)
    uint8_t rxbuf_idx;
    // Number of RX data buffers
    uint32_t rxbuf_nb;
    // Size, in bytes, of a Rx Data buffer
    uint32_t rxbuf_sz;

    /// Fields for Radar events handling
    // Array of Radar data buffers pushed to firmware
    struct rwnx_ipc_buf *radar[IPC_RADARBUF_CNT];
    // Index of the next radr buffer to read
    uint8_t radar_idx;

    /// Fields for Unsupported frame handling
    // Array of Buffer pushed to firmware to upload unsupported frames
    struct rwnx_ipc_buf *unsuprxvec[IPC_UNSUPRXVECBUF_CNT];
    // Index used for ipc_host_unsuprxvecbuf_array to point to current buffer
    uint8_t unsuprxvec_idx;
    // Store the size of unsupported rx vector buffers
    uint32_t unsuprxvec_sz;

    /// Fields for Data Tx handling
    // Index to the first free TX DMA desc
    uint32_t txdmadesc_idx;
    // Pointer to the TX DMA descriptor array
    volatile struct dma_desc *txdmadesc;

    // Table of element to store tx_hostid
    struct ipc_hostid tx_hostid[IPC_TXCFM_CNT];
    // List of available tx_hostid
    struct list_head tx_hostid_available;
    // List of tx_hostid currently pushed to the firmware
    struct list_head tx_hostid_pushed;

    // Array of Buffer pushed to firmware to upload TX confirmation
    struct rwnx_ipc_buf *txcfm[IPC_TXDMA_DESC_CNT];//[IPC_TXCFM_CNT];
    // Index of next TX confirmation to process
    uint32_t txcfm_idx;

    /// Fields for Emb->App Messages
    // Array of MSG buffer allocated for the firmware
    struct rwnx_ipc_buf *msgbuf[IPC_MSGE2A_BUF_CNT];
    // Index of the next MSG E2A buffers to read
    uint8_t msgbuf_idx;

    /// Fields for App->Emb Messages
    /// E2A ACKs of A2E MSGs
    uint8_t msga2e_cnt;
    void *msga2e_hostid;

    /// Fields for Debug MSGs handling
    // Array of debug buffer allocated for the firmware
    struct rwnx_ipc_buf *dbgbuf[IPC_DBGBUF_CNT];
    // Index of the next Debug message to read
    uint8_t dbgbuf_idx;


    // Index used that points to the first free TX desc
    uint32_t txdesc_free_idx[PCIE_TXQUEUE_CNT];
    // Index used that points to the first used TX desc
    uint32_t txdesc_used_idx[PCIE_TXQUEUE_CNT];
    // Array storing the currently pushed host ids, per IPC queue
    uint64_t tx_host_id[PCIE_TXQUEUE_CNT][PCIE_TXDESC_CNT];

    /// Pointer to the attached object (used in callbacks and register accesses)
    void *pthis;
};

extern const int nx_txdesc_cnt[];
extern const int nx_txuser_cnt[];

/**
 ******************************************************************************
 * @brief Initialize the IPC running on the Application CPU.
 *
 * This function:
 *   - initializes the IPC software environments
 *   - enables the interrupts in the IPC block
 *
 * @param[in]   env   Pointer to the IPC host environment
 *
 * @warning Since this function resets the IPC Shared memory, it must be called
 * before the LMAC FW is launched because LMAC sets some init values in IPC
 * Shared memory at boot.
 *
 ******************************************************************************
 */
void ipc_host_init(struct ipc_host_env_tag *env,
                  struct ipc_host_cb_tag *cb,
                  struct ipc_shared_env_tag *shared_env_ptr,
                  void *pthis);

/**
 ******************************************************************************
 * @brief Check if there are TX frames pending in the TX queues.
 *
 * @param[in]   env   Pointer to the IPC host environment
 *
 * @return true if there are frames pending, false otherwise.
 *
 ******************************************************************************
 */
bool ipc_host_tx_frames_pending(struct ipc_host_env_tag *env);

/**
 ******************************************************************************
 * @brief Get and flush a packet from the IPC queue passed as parameter.
 *
 * @param[in]   env        Pointer to the IPC host environment
 *
 * @return The flushed hostid if there is one, 0 otherwise.
 ******************************************************************************
 */
void *ipc_host_tx_flush(struct ipc_host_env_tag *env);

/**
 ******************************************************************************
 * ipc_host_pattern_push() - Push address on the IPC buffer from where FW can
 * download TX pattern.
 *
 * @env: IPC host environment
 * @buf: IPC buffer that contains the pattern downloaded by fw after each TX
 ******************************************************************************
 */
void ipc_host_pattern_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * ipc_host_rxbuf_push() - Push a pre-allocated buffer for a Rx packet
 *
 * @env: IPC host environment
 * @buf: IPC buffer to use for the RX packet
 *
 * Push a new buffer for the firmware to upload an RX packet.
 ******************************************************************************
 */
int ipc_host_rxbuf_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * ipc_host_rxdesc_push() - Push a pre-allocated buffer for a Rx Descriptor
 *
 * @env: IPC host environment
 * @buf: IPC buffer to use for the RX descriptor
 *
 * Push a new buffer for the firmware to upload an RX descriptor.
 ******************************************************************************
 */
int ipc_host_rxdesc_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * ipc_host_radar_push() - Push a pre-allocated buffer for a Rx Descriptor
 *
 * @env: IPC host environment
 * @buf: IPC buffer to use for the Radar event
 *
 * This function is called at Init time to initialize all radar event buffers.
 * Then each time embedded send a radar event, this function is used to push
 * back the same buffer once it has been handled.
 ******************************************************************************
 */
int ipc_host_radar_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * ipc_host_unsuprxvec_push() - Push a pre-allocated buffer for the firmware
 * to upload unsupported RX vector descriptor.
 *
 * @env: IPC host environment
 * @buf: IPC buffer to use for the Unsupported RX vector.
 *
 * This function is called at Init time to initialize all unsupported rx vector
 * buffers. Then each time the embedded sends a unsupported rx vector, this
 * function is used to push a new unsupported rx vector buffer.
 ******************************************************************************
 */
int ipc_host_unsuprxvec_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * ipc_host_msgbuf_push() - Push the pre-allocated buffer for IPC MSGs from
 * the firmware
 *
 * @env: IPC host environment
 * @buf: IPC buffer to use for the IPC messages
 *
 * This function is called at Init time to initialize all Emb2App messages
 * buffers. Then each time embedded send a IPC message, this function is used
 * to push back the same buffer once it has been handled.
 ******************************************************************************
 */
int ipc_host_msgbuf_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * ipc_host_dbgbuf_push() - Push the pre-allocated buffer for Debug MSGs from
 * the firmware
 *
 * @env: IPC host environment
 * @buf: IPC buffer to use for the debug messages
 *
 * This function is called at Init time to initialize all debug messages.
 * Then each time embedded send a debug message, this function is used to push
 * back the same buffer once it has been handled.
 ******************************************************************************
 */
int ipc_host_dbgbuf_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * ipc_host_dbginfo_push() - Push the pre-allocated logic analyzer and debug
 * information buffer
 *
 * @env: IPC host environment
 * @buf: IPC buffer to use for the FW dump
 ******************************************************************************
 */
void ipc_host_dbginfo_push(struct ipc_host_env_tag *env, struct rwnx_ipc_buf *buf);

/**
 ******************************************************************************
 * @brief Handle all IPC interrupts on the host side.
 *
 * The following interrupts should be handled:
 * Tx confirmation, Rx buffer requests, Rx packet ready and kernel messages
 *
 * @param[in]   env   Pointer to the IPC host environment
 *
 ******************************************************************************
 */
void ipc_host_irq(struct ipc_host_env_tag *env, uint32_t status);

/**
 ******************************************************************************
 * @brief Send a message to the embedded side
 *
 * @param[in]   env      Pointer to the IPC host environment
 * @param[in]   msg_buf  Pointer to the message buffer
 * @param[in]   msg_len  Length of the message to be transmitted
 *
 * @return      Non-null value on failure
 *
 ******************************************************************************
 */
int ipc_host_msg_push(struct ipc_host_env_tag *env, void *msg_buf, uint16_t len);

/**
 ******************************************************************************
 * @brief Enable IPC interrupts
 *
 * @param[in]   env  Global ipc_host environment pointer
 * @param[in]   value  Bitfield of the interrupts to enable
 *
 * @warning After calling this function, IPC interrupts can be triggered at any
 * time. Potentially, an interrupt could happen even before returning from the
 * function if there is a request pending from the embedded side.
 *
 ******************************************************************************
 */
void ipc_host_enable_irq(struct ipc_host_env_tag *env, uint32_t value);
void ipc_host_disable_irq(struct ipc_host_env_tag *env, uint32_t value);

uint32_t ipc_host_get_status(struct ipc_host_env_tag *env);
uint32_t ipc_host_get_rawstatus(struct ipc_host_env_tag *env);

uint32_t ipc_host_tx_host_ptr_to_id(struct ipc_host_env_tag *env, void *host_ptr);
void *ipc_host_tx_host_id_to_ptr(struct ipc_host_env_tag *env, uint32_t hostid);

#endif // _IPC_HOST_H_

