#ifndef _MANAGER_H_
#define _MANAGER_H_

#include "common.h"

#define B_PRIV_INFO_OUTPUT    "/var/run/b_priv_info.txt"
#define MANAGER_STR           "[MANAGER]"

//#define CTRL_PATH "/etc/wifi/hostapd"
#define CTRL_PATH "/var/run/hostapd"

//#define HOSTAPD_PATH "hostapd_cli"
#define HOSTAPD_PATH "sudo ~/work/wpa/5g_pre/hostapd-2.10/hostapd/hostapd_cli"


#define MANAGER_VER    "v1.1"

/* Netlink Protocol Number */
#define NL_AIC_PROTOCOL            28
#define NL_AIC_PROTOCOL_SEC_DRV    29

/* Netlink Protocol ID */
#define NL_AIC_MANAGER_PID         5185

/* Netlink Max Message Size */
#define NL_MAX_MSG_SIZE            768

/* Netlink Message Type List */
#define AIC_NL_DAEMON_ON_TYPE            1
#define AIC_NL_DAEMON_OFF_TYPE           2
#define AIC_NL_DAEMON_ALIVE_TYPE         3
#define AIC_NL_DEL_STA_TYPE              4
#define AIC_NL_NEW_STA_TYPE              5
#define AIC_NL_INTF_RPT_TYPE             6
#define AIC_NL_STA_RPT_TYPE              7
#define AIC_NL_FRAME_RPT_TYPE            8
#define AIC_NL_TIME_TICK_TYPE            9
#define AIC_NL_PRIV_INFO_CMD_TYPE        10
#ifdef CONFIG_WIFI_BAND_STEERING
#define AIC_NL_B_STEER_CMD_TYPE          11
#define AIC_NL_B_STEER_BLOCK_ADD_TYPE    12
#define AIC_NL_B_STEER_BLOCK_DEL_TYPE    13
#define AIC_NL_B_STEER_ROAM_TYPE         14
#endif

#define AIC_NL_GENERAL_CMD_TYPE          100
#define AIC_NL_CUSTOMER_TYPE             101
#define AIC_NL_CONFIG_UPDATE_TYPE        102


/* Element ID */
#define AIC_ELM_INTF_ID            1
#define AIC_ELM_INTF_INFO_ID       2
#define AIC_ELM_FRAME_INFO_ID      3
#define AIC_ELM_STA_INFO_ID        4
#define AIC_ELM_ROAM_INFO_ID       5
#define AIC_ELM_BUFFER_ID          6
#define AIC_ELM_STA_INFO_EXT_ID    7

struct b_nl_message {
	u32 type;
	u32 len;
	u8  content[NL_MAX_MSG_SIZE];
};

struct b_elm_header {
	u8 id;
	u8 len;
};

struct b_elm_intf {
	u8  mac[6];
	u8  root;
	u8  band;
	u8  ssid;
	s8  name[16];
};

struct b_elm_intf_info {
	u16 ch;
	u8  ch_clm;
	u8  ch_noise;
	u32 tx_tp;
	u32 rx_tp;
	u32 assoc_sta_num;
	/* self neighbor report info */
	u32 bss_info;
	u8  reg_class;
	u8  phy_type;
};

struct b_elm_frame_info {
	u16 frame_type;
	u8  sa[6];
	s8  rssi;
};

struct b_elm_sta_info {
	u8  mac[6];
	s8  rssi;
	u32 link_time;
	u32 tx_tp; /* kbits */
	u32 rx_tp; /* kbits */
};

struct b_elm_roam_info {
	u8  sta_mac[6]; /* station mac */
	u8  bss_mac[6]; /* target bss mac */
	u16 bss_ch; /* target bss channel */
	u8  method; /* 0: 11V, 1: Deauth */
};

struct b_elm_buffer {
	u8 buf[255];
};

struct b_elm_sta_info_ext {
	u8 mac[6];
	u8 supported_band;
	u8 empty[119]; /* for future use */
};


/* Element Size List */
#define ELM_HEADER_LEN            (sizeof(struct b_elm_header))
#define ELM_INTF_LEN              (sizeof(struct b_elm_intf))
#define ELM_INTF_INFO_LEN         (sizeof(struct b_elm_intf_info))
#define ELM_FRAME_INFO_LEN        (sizeof(struct b_elm_frame_info))
#define ELM_STA_INFO_LEN          (sizeof(struct b_elm_sta_info))
#define ELM_ROAM_INFO_LEN         (sizeof(struct b_elm_roam_info))
#define ELM_BUFFER_LEN            (sizeof(struct b_elm_buffer))
#define ELM_STA_INFO_EXT_LEN      (sizeof(struct b_elm_sta_info_ext))



void manager_hostapd_cli_bss_tm_req(
	s8 *intf_name, u8 *sta_mac, u8 *bss_mac, u16 bss_ch,
	u16 bss_info, u8 bss_reg_class, u8 bss_phy_type,
	u8 disassoc_imminent, u32 disassoc_timer);
void manager_hostapd_cli_deauth(s8 *intf_name, u8 *sta_mac);
void manager_set_msg(
	struct b_nl_message *msg, u32 *msg_len, void *elm, u32 elm_len);
void manager_send_drv(void *msg, u32 msg_len);
#ifdef CONFIG_SEC_NLCMD
void manager_send_sec_drv(void *msg, u32 msg_len);
#endif
void manager_send_daemon(void *msg, u32 msg_len, pid_t pid);

#endif

