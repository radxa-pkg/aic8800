#ifndef _COMMON_H_
#define _COMMON_H_

#include <linux/netlink.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "os.h"

#define BIT(x)          (1 << (x))

enum band_type {
	BAND_ON_24G = 0,
	BAND_ON_5G = 1,
	BAND_ON_60G = 2,
	BAND_ON_6G = 3,
	BAND_MAX,
};

#define BAND_CAP_2G    BIT(BAND_ON_24G)
#define BAND_CAP_5G    BIT(BAND_ON_5G)


enum WIFI_FRAME_TYPE {
	WIFI_MGT_TYPE       = (0),
};

enum WIFI_FRAME_SUBTYPE {
	WIFI_ASSOCREQ       = (0 | WIFI_MGT_TYPE),
	WIFI_PROBEREQ       = (BIT(6) | WIFI_MGT_TYPE),
	WIFI_AUTH           = (BIT(7) | BIT(5) | BIT(4) | WIFI_MGT_TYPE),
};


#ifdef CONFIG_WIFI_BAND_STEERING
#define ACTIVE_DUAL_BAND_DETECT
#endif

typedef unsigned char   u8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef uint64_t        u64;
typedef signed char     s8;
typedef int16_t         s16;
typedef int32_t         s32;

#define BSS_TM_REQ_PARAMS_FMT    \
        "valid_int=%u pref=%u disassoc_imminent=%u disassoc_timer=%u abridged=%u "	\
        "neighbor=%02x:%02x:%02x:%02x:%02x:%02x,%u,%u,%u,%u"

#define MAC_FMT         "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARG(x)      ((u8 *)(x))[0], ((u8 *)(x))[1], \
                        ((u8 *)(x))[2], ((u8 *)(x))[3], \
                        ((u8 *)(x))[4], ((u8 *)(x))[5]

#define BAND_NUM			2
#define SSID_NUM			1
#define MAX_STA_NUM			32
#define HASH_TBL_SIZE		32
#define BAND_NAME(x)		((x == BAND_ON_5G) ? "5G" : "24G")

enum {
	_DBG_MSG_PRINT_ = 0,
	_DBG_MSG_ERR_ = 1,
	_DBG_MSG_WARN_ = 2,
	_DBG_MSG_INFO_ = 3,
	_DBG_MSG_DEBUG_ = 4,
	_MSG_MAX_ = 5,
};

static int global_dbg_level = _DBG_MSG_INFO_;
#define SHOULD_LOG(level) (global_dbg_level >= (level))


struct b_steer_nb_rpt_hdr {
	u8  id;
	u8  len;
	u8  bssid[6];
	u32 bss_info;
	u8  reg_class;
	u8  ch_num;
	u8  phy_type;
	u8  preference;
	u8  enable;
};

struct b_steer_wifi_list {
    struct b_steer_wifi_list *next;
    struct b_steer_wifi_list **pprev;
};

struct b_steer_sta {
	u8  used;
	u8  mac[6];
	s8  rssi;
	u32 link_time;
	u32 tx_tp;
	u32 rx_tp;
	u8  status;
	u8  is_dual_band;

#ifdef CONFIG_WIFI_BAND_STEERING
	u8  b_steer_roam_cnt;
	u8  b_steer_roam_detect;
#ifdef ACTIVE_DUAL_BAND_DETECT
	u8  b_steer_bss_tm_req_cnt;
#endif
#endif

	struct b_steer_wifi_list list;
};

struct b_steer_frame {
	u8  used;
	u16 frame_type;
	u8  sa[6];
	u32 aging;

	struct b_steer_wifi_list list;
};

struct b_steer_priv {
	u8  active;
	u8  mac[6];
	u8  root;
	u8  band;
	u8  ssid;
	s8  name[16];
	u16 ch;
	u8  ch_clm;
	u8  ch_noise;
	u32 tx_tp;
	u32 rx_tp;
	u8  sta_num;
	struct b_steer_sta sta_list[MAX_STA_NUM];
	struct b_steer_wifi_list *sta_hash_tbl[HASH_TBL_SIZE];

	struct b_steer_frame frame_db[MAX_STA_NUM];
	struct b_steer_wifi_list *frame_hash_tbl[HASH_TBL_SIZE];

	struct b_steer_nb_rpt_hdr self_nb_rpt;

#ifdef CONFIG_WIFI_BAND_STEERING
	u8 band_steering_enable;
	struct b_steer_priv *grp_priv;
#endif
};

struct b_steer_device {
	struct b_steer_priv priv[BAND_NUM][SSID_NUM];
	s8 config_fname[64];
};

__inline static u32 b_wifi_mac_hash(const u8 *mac)
{
	u32 x;

	x = mac[0];
	x = (x << 2) ^ mac[1];
	x = (x << 2) ^ mac[2];
	x = (x << 2) ^ mac[3];
	x = (x << 2) ^ mac[4];
	x = (x << 2) ^ mac[5];

	x ^= x >> 8;
	x  = x & (HASH_TBL_SIZE - 1);

	return x;
}

__inline__ static void b_wifi_list_link(
	struct b_steer_wifi_list *link, struct b_steer_wifi_list **head)
{
    if (!link || !head) {
        printf("STEER %s, list is null", __func__);
        return;
    }

    link->next = *head;

    if (link->next != NULL)
        link->next->pprev = &link->next;

    *head = link;

    link->pprev = head;
}

__inline__ static void b_wifi_list_unlink(struct b_steer_wifi_list *link)
{
    if (!link || !link->pprev) {
        printf("STEER %s, list is null", __func__);
        return;
    }

    *(link->pprev) = link->next;

    if (link->next != NULL)
        link->next->pprev = link->pprev;

    link->next = NULL;
    link->pprev = NULL;
}

__inline__ static void DBG_MSG_PRINT(const char *prefix, const char *fmt, ...) 
{
    if (!fmt || !SHOULD_LOG(_DBG_MSG_PRINT_))
        return;

    va_list va;
    va_start(va, fmt);
    printf("%s PRINT: ", prefix);
    vprintf(fmt, va);
    putchar('\n');
    va_end(va);
}

__inline__ static void DBG_MSG_ERR(const char *prefix, const char *fmt, ...) 
{
    if (!fmt || !SHOULD_LOG(_DBG_MSG_ERR_))
        return;

    va_list va;
    va_start(va, fmt);
    printf("%s ERROR: ", prefix);
    vprintf(fmt, va);
    putchar('\n');
    va_end(va);
}

__inline__ static void DBG_MSG_WARN(const char *prefix, const char *fmt, ...) 
{
    if (!fmt || !SHOULD_LOG(_DBG_MSG_WARN_))
        return;

    va_list va;
    va_start(va, fmt);
    printf("%s WARN: ", prefix);
    vprintf(fmt, va);
    putchar('\n');
    va_end(va);
}

__inline__ static void DBG_MSG_INFO(const char *prefix, const char *fmt, ...) 
{
    if (!fmt || !SHOULD_LOG(_DBG_MSG_INFO_))
        return;

    va_list va;
    va_start(va, fmt);
    printf("%s INFO: ", prefix);
    vprintf(fmt, va);
    putchar('\n');
    va_end(va);
}

__inline__ static void DBG_MSG_DEBUG(const char *prefix, const char *fmt, ...) 
{
    if (!fmt || !SHOULD_LOG(_DBG_MSG_DEBUG_))
        return;

    va_list va;
    va_start(va, fmt);
    printf("%s DEBUG: ", prefix);
    vprintf(fmt, va);
    putchar('\n');
    va_end(va);
}


#endif

