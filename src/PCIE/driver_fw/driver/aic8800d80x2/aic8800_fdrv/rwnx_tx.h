/**
 ******************************************************************************
 *
 * @file rwnx_tx.h
 *
 * Copyright (C) RivieraWaves 2012-2019
 *
 ******************************************************************************
 */
#ifndef _RWNX_TX_H_
#define _RWNX_TX_H_

#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/netdevice.h>
#include "lmac_types.h"
#include "ipc_shared.h"
#include "rwnx_txq.h"
#include "hal_desc.h"
#include "rwnx_utils.h"
#include "aicwf_tcp_ack.h"

#define RWNX_HWQ_BK                     0
#define RWNX_HWQ_BE                     1
#define RWNX_HWQ_VI                     2
#define RWNX_HWQ_VO                     3
#define RWNX_HWQ_BCMC                   4
#define RWNX_HWQ_NB                     NX_TXQ_CNT
#define RWNX_HWQ_ALL_ACS (RWNX_HWQ_BK | RWNX_HWQ_BE | RWNX_HWQ_VI | RWNX_HWQ_VO)
#define RWNX_HWQ_ALL_ACS_BIT (BIT(RWNX_HWQ_BK) | BIT(RWNX_HWQ_BE) |    \
							  BIT(RWNX_HWQ_VI) | BIT(RWNX_HWQ_VO))

#define RWNX_TX_LIFETIME_MS             500
#define RWNX_TX_MAX_RATES               NX_TX_MAX_RATES

#define RWNX_SWTXHDR_ALIGN_SZ           4
#define RWNX_SWTXHDR_ALIGN_MSK (RWNX_SWTXHDR_ALIGN_SZ - 1)
#define RWNX_SWTXHDR_ALIGN_PADS(x) \
					((RWNX_SWTXHDR_ALIGN_SZ - ((x) & RWNX_SWTXHDR_ALIGN_MSK)) \
					 & RWNX_SWTXHDR_ALIGN_MSK)
#if RWNX_SWTXHDR_ALIGN_SZ & RWNX_SWTXHDR_ALIGN_MSK
#error bad RWNX_SWTXHDR_ALIGN_SZ
#endif

#define AMSDU_PADDING(x) ((4 - ((x) & 0x3)) & 0x3)

#define TXU_CNTRL_RETRY        BIT(0)
#define TXU_CNTRL_MORE_DATA    BIT(2)
#define TXU_CNTRL_MGMT         BIT(3)
#define TXU_CNTRL_MGMT_NO_CCK  BIT(4)
#define TXU_CNTRL_AMSDU        BIT(6)
#define TXU_CNTRL_MGMT_ROBUST  BIT(7)
#define TXU_CNTRL_USE_4ADDR    BIT(8)
#define TXU_CNTRL_EOSP         BIT(9)
#define TXU_CNTRL_MESH_FWD     BIT(10)
#define TXU_CNTRL_TDLS         BIT(11)
#define TXU_CNTRL_REUSE_SN     BIT(15)

extern const int rwnx_tid2hwq[IEEE80211_NUM_TIDS];

/**
 * struct rwnx_amsdu_txhdr - Structure added in skb headroom (instead of
 * rwnx_txhdr) for amsdu subframe buffer (except for the first subframe
 * that has a normal rwnx_txhdr)
 *
 * @list     List of other amsdu subframe (rwnx_sw_txhdr.amsdu.hdrs)
 * @ipc_data: IPC buffer for the A-MSDU subframe
 * @skb      skb
 * @pad      padding added before this subframe
 *           (only use when amsdu must be dismantled)
 * @msdu_len Size, in bytes, of the MSDU (without padding nor amsdu header)
 */
struct rwnx_amsdu_txhdr {
	struct list_head list;
	struct rwnx_ipc_buf ipc_data;
	struct sk_buff *skb;
	u16 pad;
	u16 msdu_len;
};

/**
 * struct rwnx_amsdu - Structure to manage creation of an A-MSDU, updated
 * only In the first subframe of an A-MSDU
 *
 * @hdrs List of subframe of rwnx_amsdu_txhdr
 * @len  Current size for this A-MDSU (doesn't take padding into account)
 *       0 means that no amsdu is in progress
 * @nb   Number of subframe in the amsdu
 * @pad  Padding to add before adding a new subframe
 */
struct rwnx_amsdu {
	struct list_head hdrs;
	u16 len;
	u8 nb;
	u8 pad;
};

/**
 * struct rwnx_sw_txhdr - Software part of tx header
 *
 * @rwnx_sta: sta to which this buffer is addressed
 * @rwnx_vif: vif that send the buffer
 * @txq: pointer to TXQ used to send the buffer
 * @hw_queue: Index of the HWQ used to push the buffer.
 *           May be different than txq->hwq->id on confirmation.
 * @frame_len: Size of the frame (doesn't not include mac header)
 *            (Only used to update stat, can't we use skb->len instead ?)
 * @amsdu: Description of amsdu whose first subframe is this buffer
 *        (amsdu.nb = 0 means this buffer is not part of amsdu)
 * @skb: skb received from transmission
 * @ipc_data: IPC buffer for the frame
 * @ipc_desc: IPC buffer for the TX descriptor
 * @jiffies: Jiffies when this buffer has been pushed to the driver
 * @desc: TX descriptor downloaded by firmware
 */
struct rwnx_sw_txhdr {
	struct rwnx_sta *rwnx_sta;
	struct rwnx_vif *rwnx_vif;
	struct rwnx_txq *txq;
	u8 hw_queue;
	u16 frame_len;
	u16 headroom;
#ifdef CONFIG_RWNX_AMSDUS_TX
	struct rwnx_amsdu amsdu;
#endif
#ifndef AICWF_PCIE_SUPPORT
	u32 need_cfm;
#else
	u16 need_cfm;
	u16 cfmd;
	u16 idx;
#endif
	
	struct sk_buff *skb;

    struct rwnx_ipc_buf ipc_data;
    struct rwnx_ipc_buf ipc_desc;
    struct rwnx_ipc_buf ipc_hostdesc;
    unsigned long jiffies;
    struct txdesc_host desc;
    u8 raw_frame;
    u8 fixed_rate;
    u16 rate_config;
};

/**
 * struct rwnx_txhdr - Structure to control transmission of packet
 * (Added in skb headroom)
 *
 * @sw_hdr: Information from driver
 * @cache_guard:
 * @hw_hdr: Information for/from hardware
 */
struct rwnx_txhdr {
	struct rwnx_sw_txhdr *sw_hdr;
#ifdef CONFIG_CACHE_GUARD
	char cache_guard[L1_CACHE_BYTES];
#endif
	struct rwnx_hw_txhdr hw_hdr;
};

/**
 * RWNX_TX_HEADROOM - Headroom to use to store struct rwnx_txhdr
 */
#define RWNX_TX_HEADROOM sizeof(struct rwnx_txhdr)

/**
 * RWNX_TX_AMSDU_HEADROOM - Maximum headroom need for an A-MSDU sub frame
 * Need to store struct rwnx_amsdu_txhdr, A-MSDU header (14)
 * optional padding (4) and LLC/SNAP header (8)
 */
#define RWNX_TX_AMSDU_HEADROOM (sizeof(struct rwnx_amsdu_txhdr) + 14 + 4 + 8)

/**
 * RWNX_TX_MAX_HEADROOM - Maximum size needed in skb headroom to prepare a buffer
 * for transmission
 */
#define RWNX_TX_MAX_HEADROOM max(RWNX_TX_HEADROOM, RWNX_TX_AMSDU_HEADROOM)

/**
 * RWNX_TX_DMA_MAP_LEN - Length, in bytes, to map for DMA transfer
 * To be called with skb BEFORE reserving headroom to store struct rwnx_txhdr.
 */
#define RWNX_TX_DMA_MAP_LEN(skb) (skb->len - sizeof(struct ethhdr))

/**
 * SKB buffer format before it is pushed to MACSW
 *
 * For DATA frame
 *                    |--------------------|
 *                    | headroom           |
 *    skb->data ----> |--------------------|
 *                    | struct rwnx_txhdr  |
 *                    | * rwnx_sw_txhdr *  |
 *                    | * [L1 guard]       |
 *                    |--------------------|
 *                    | 802.3 Header       |
 *               +--> |--------------------| <---- desc.host.packet_addr[0]
 *     memory    :    | Data               |
 *     mapped    :    |                    |
 *     for DMA   :    |                    |
 *               :    |                    |
 *               +--> |--------------------|
 *                    | tailroom           |
 *                    |--------------------|
 *
 *
 * For MGMT frame (skb is created by the driver so buffer is always aligned
 *                 with no headroom/tailroom)
 *
 *    skb->data ----> |--------------------|
 *                    | struct rwnx_txhdr  |
 *                    | * rwnx_sw_txhdr *  |
 *                    | * [L1 guard]       |
 *                    |                    |
 *               +--> |--------------------| <---- desc.host.packet_addr[0]
 *     memory    :    | 802.11 HDR         |
 *     mapped    :    |--------------------|
 *     for DMA   :    | Data               |
 *               :    |                    |
 *               +--> |--------------------|
 *
 */


u16 rwnx_select_txq(struct rwnx_vif *rwnx_vif, struct sk_buff *skb);
netdev_tx_t rwnx_start_xmit(struct sk_buff *skb, struct net_device *dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
int rwnx_start_mgmt_xmit(struct rwnx_vif *vif, struct rwnx_sta *sta,
						 struct cfg80211_mgmt_tx_params *params, bool offchan,
						 u64 *cookie);
#else
int rwnx_start_mgmt_xmit(struct rwnx_vif *vif, struct rwnx_sta *sta,
						 struct ieee80211_channel *channel, bool offchan,
						 unsigned int wait, const u8 *buf, size_t len,
					#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
						 bool no_cck,
					#endif
					#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
						 bool dont_wait_for_ack,
					#endif
						 u64 *cookie);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */
int rwnx_txdatacfm(void *pthis, void *host_id,u8 free);

struct rwnx_hw;
struct rwnx_sta;
void rwnx_set_traffic_status(struct rwnx_hw *rwnx_hw,
							 struct rwnx_sta *sta,
							 bool available,
							 u8 ps_id);
void rwnx_ps_bh_enable(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta,
					   bool enable);
void rwnx_ps_bh_traffic_req(struct rwnx_hw *rwnx_hw, struct rwnx_sta *sta,
							u16 pkt_req, u8 ps_id);

void rwnx_switch_vif_sta_txq(struct rwnx_sta *sta, struct rwnx_vif *old_vif,
							 struct rwnx_vif *new_vif);

int rwnx_dbgfs_print_sta(char *buf, size_t size, struct rwnx_sta *sta,
						 struct rwnx_hw *rwnx_hw);
void rwnx_txq_credit_update(struct rwnx_hw *rwnx_hw, int sta_idx, u8 tid,
							s8 update);
void rwnx_tx_push(struct rwnx_hw *rwnx_hw, struct rwnx_txhdr *txhdr, int flags);
int intf_tx(struct rwnx_hw *priv,struct msg_buf *msg);

#endif /* _RWNX_TX_H_ */
