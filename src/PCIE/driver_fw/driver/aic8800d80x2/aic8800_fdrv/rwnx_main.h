/**
 ******************************************************************************
 *
 * @file rwnx_main.h
 *
 * Copyright (C) RivieraWaves 2012-2019
 *
 ******************************************************************************
 */

#ifndef _RWNX_MAIN_H_
#define _RWNX_MAIN_H_

#include "rwnx_defs.h"

int rwnx_cfg80211_init(struct rwnx_plat *rwnx_plat, void **platform_data);
void rwnx_cfg80211_deinit(struct rwnx_hw *rwnx_hw);
extern int testmode;
extern u8 chip_sub_id;
extern u8 chip_mcu_id;
extern u8 chip_id;

#define RSSI_GET_INTERVAL                (10 * 1000)   //time interval


#define RSSI_THD_0                       (-20)         //rssi 0 (dBm)
#define RSSI_THD_1                       (-30)         //rssi 1 (dBm)
#define RSSI_THD_2                       (-75)         //rssi 2 (dBm)

#define PWR_LOSS_LVL0                     (-10)        //RSSI > RSSI_THD_0
#define PWR_LOSS_LVL1                     (-5 )        //RSSI_THD_1 < RSSI <RSSI_THD_0
#define PWR_LOSS_LVL2                     (0)          //RSSI_THD_2 < RSSI <RSSI_THD_1
#define PWR_LOSS_LVL3                     (0)//(2)          //RSSI <RSSI_THD_2

#define PWR_DELAY_TIME                   (10 * 1000)   //pwr reduced latency time (ms)

#ifdef CONFIG_DYNAMIC_PERPWR
void rssi_update_txpwrloss(struct rwnx_sta *sta, s8_l rssi);
void aicwf_txpwer_per_sta_worker(struct work_struct *work);
#endif

#ifdef CONFIG_DYNAMIC_PWR
void set_txpwrloss_ctrl(struct rwnx_hw *rwnx_hw, s8 value);
void aicwf_pwrloss_worker(struct work_struct *work);
#endif
void rwnx_update_mesh_power_mode(struct rwnx_vif *vif);
void aicwf_hostif_fail(void);
int rwnx_ic_rf_init(struct rwnx_hw *rwnx_hw);
int rwnx_ic_system_init(struct rwnx_hw *rwnx_hw);
u16 rwnx_select_queue(struct net_device *dev, struct sk_buff *skb,
					  struct net_device *sb_dev);
int rwnx_fill_station_info(struct rwnx_sta *sta, struct rwnx_vif *vif,
								  struct station_info *sinfo, u8 *phymode, u32 *tx_phyrate, u32 *rx_phyrate);
int rwnx_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev,
							 struct bss_parameters *params);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void aicwf_p2p_alive_timeout(ulong data);
#else
void aicwf_p2p_alive_timeout(struct timer_list *t);
#endif
void apm_staloss_work_process(struct work_struct *work);
void apm_probe_sta_work_process(struct work_struct *work);
int rwnx_cfg80211_set_monitor_channel_(struct wiphy *wiphy, struct cfg80211_chan_def *chandef);
int rwnx_cfg80211_probe_client(struct wiphy *wiphy, struct net_device *dev, const u8 *peer, u64 *cookie);
void rwnx_cfg80211_mgmt_frame_register(struct wiphy *wiphy, struct wireless_dev *wdev, u16 frame_type, bool reg);
int rwnx_cfg80211_channel_switch(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_csa_settings *params);
#ifdef CONFIG_PREALLOC_TXQ
struct prealloc_txq{
    int prealloced;
    void *txq;
    size_t size;
};
void *aicwf_prealloc_txq_alloc(size_t size);
void aicwf_prealloc_txq_free(void);
#endif
#endif /* _RWNX_MAIN_H_ */
