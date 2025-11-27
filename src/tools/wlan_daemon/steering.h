#ifndef _STEERING_H_
#define _STEERING_H_

#include "common.h"

#define B_STEER_MAX_NUM        64//32


#define B_STEER_OUTPUT_FILE                 "/var/run/b_steering.log"
#define B_STEER_STR                         "[STEERING]"
#define B_STEER_PREFER_B_TIMEOUT_COUNT      150
#define B_STEER_ENTRY_IGNORE_PERIOD_COUNT   15
#define B_STEER_ROAM_STA_COUNT              1


#ifdef ACTIVE_DUAL_BAND_DETECT
#define BSS_TM_REQ_RETRY_LIMIT        (5)
#define BSS_TM_REQ_INTERVAL           (30)
#define BSS_TM_REQ_TP_LIMIT           (100) //100 kbits unuse
#endif

#define PREFER_BAND_RSSI_H            (-30)
#define PREFER_BAND_RSSI_L            (-60)


struct b_steer_ignore_entry {
	u8  used;
	u8  mac[6];
	u32 entry_exp;
	struct b_steer_wifi_list list;
};

/* allow */
struct b_steer_prefer_band_entry {
	u8  used;
	u8  mac[6];
	s8  rssi;
	u32 aging;
	struct b_steer_wifi_list list;
};

/* block */
struct b_steer_non_prefer_band_entry {
	u8  used;
	u8  mac[6];
	u32 entry_exp_phase1;
	u32 assoc_rty_lmt;
	u32 entry_exp_phase2;
	struct b_steer_wifi_list list;
};

struct b_steer_context {
	u8  prefer_band;
	u8  non_prefer_band;
	/* bss_tm_req parameters */
	u8  bss_tm_req_th;
	u8  bss_tm_req_disassoc_imminent;
	u32 bss_tm_req_disassoc_timer;

	/* band roaming parameters */
	u8  roam_detect_th;
	// unuse
	u32 roam_sta_tp_th;
	/* CLM: non prefer band --> prefer band */
	//unuse
	u8  roam_ch_clm_th;

	/* rssi parameters */
	s8 prefer_band_rssi_high;
	s8 prefer_band_rssi_low;

	/* non prefer band parameters */
	u32 entry_exp_phase1;
	u32 assoc_rty_lmt;
	u32 entry_exp_phase2;

	/* index is ths ssid number of the band, */
	/* another band group ssid */
	u8 prefer_band_grp_ssid[SSID_NUM];
	u8 non_prefer_band_grp_ssid[SSID_NUM];

	/* data structure */
	struct b_steer_prefer_band_entry
		p_band_list[SSID_NUM][B_STEER_MAX_NUM];
	struct b_steer_non_prefer_band_entry
		np_band_list[SSID_NUM][B_STEER_MAX_NUM];
	struct b_steer_ignore_entry
		np_band_ignore_list[SSID_NUM][B_STEER_MAX_NUM];

	struct b_steer_wifi_list *p_band_hash_tbl[SSID_NUM][HASH_TBL_SIZE];
	struct b_steer_wifi_list *np_band_hash_tbl[SSID_NUM][HASH_TBL_SIZE];
	struct b_steer_wifi_list *np_band_ignore_hash_tbl[SSID_NUM][HASH_TBL_SIZE];
};



void b_steering_parse_arg(u8 *argn, s32 argc, char *argv[]);
void b_steering_mng_on_frame_rpt(
	struct b_steer_priv *priv, u16 frame_type, u8 *mac, s8 rssi);
void b_steering_on_time_tick(struct b_steer_priv *priv);
void b_steering_roam_detect(struct b_steer_priv *priv);
void b_steering_roam_start_upcoming(struct b_steer_priv *priv);
void b_steering_on_cmd(struct b_steer_priv *priv);
void b_steering_on_config_update(s8 *config);
void b_steering_init(struct b_steer_device *device);
void b_steering_deinit(void);

#endif

