/******************************************************************************
 *
 *  Copyright (C) 2019-2021 Aicsemi Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
*
*	Module Name:
*		aic_parse.c
*
*	Abstract:
*		Contains wifi-bt coex functions implemented by bluedroid stack
*
*	Notes:
*		  This is designed for wifi-bt Coex in Android 6.0.
*
******************************************************************************/
#define LOG_TAG "aic_parse"
#define AICBT_RELEASE_NAME "AIC_BT_ANDROID_10.0"

#include <utils/Log.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <signal.h>
#include <poll.h>

#include "bt_list.h"
#include "bt_hci_bdroid.h"
#include "aic_parse.h"
#include <sys/syscall.h>

#define AIC_COEX_VERSION "3.0"

//#define AIC_ROLE_SWITCH_RETRY

#ifdef AIC_ROLE_SWITCH_RETRY
#ifndef MAX_LINKS
#define MAX_LINKS          7
#endif

#ifndef BD_ADDR_LEN
#define BD_ADDR_LEN     6
typedef uint8_t BD_ADDR[BD_ADDR_LEN];
#endif

typedef enum {
  ROLE_SWITCH_COMMAND_NONE,
  ROLE_SWITCH_COMMAND_PENDING,
  ROLE_SWITCH_COMMAND_SUCCESS,
  ROLE_SWITCH_COMMAND_DISALLOW
} role_switch_state;

/******************************************************************************
**  role switch monitor structions
******************************************************************************/
typedef struct
{
    bool             in_use;                 /* TRUE when in use, FALSE when not */
    role_switch_state          state;
    BD_ADDR             remote_bd_addr;             /* The BD address of the remote     */
    bool             isMaster;                 /* is_Master  */
    unsigned short              handle;              /* Link handle                      */
    timer_t         timer_hci_role_switch_cmd;                 /* CB Timer Entry */
    unsigned short               count;              /* role swith event(slave) count                    */
    time_t                  time;
    double   diff_s;             /*time diff between two successive role switch (slave)event */
}role_monitor_cb;
BD_ADDR EMPTY_ADDR = {0,0,0,0,0,0};
role_monitor_cb  role_monitor_pool[MAX_LINKS];   /* Role Switch Control Block pool  */
#define          TIME_LIMIT_FOR_ROLE_SWITCH  (60*5)   /*5 minutes*/
#define          UNKOWN_HANDLE              (0XFF)
#define          HCI_CMD_VNDR_ROLESWITCH       0xFCAD

typedef void (*tTIMER_HANDLE_ROLE_SWITCH)(union sigval sigval_value);
static void aic_start_role_switch_schedule(role_monitor_cb  * p);
#endif


char invite_req[] = "INVITE_REQ";
char invite_rsp[] = "INVITE_RSP";
char attend_req[] = "ATTEND_REQ";
char attend_ack[] = "ATTEND_ACK";
char wifi_leave[] = "WIFI_LEAVE";
char leave_ack[] =  "LEAVE_ACK";
char bt_leave[] =   "BT_LEAVE";

#define CONNECT_PORT        30001
#define CONNECT_PORT_WIFI   30000
//#define NETLINK_USER        31
#define MAX_PAYLOAD         255 /* maximum payload size*/

//L2CAP TYPE
#define L2CAP_CONNECTION_REQ        0x02
#define L2CAP_CONNECTION_RSP        0x03
#define L2CAP_DISCONNECTION_REQ     0x06
#define L2CAP_DISCONNECTION_RSP     0x07

#define TIMER_A2DP_PACKET_COUNT     (SIGRTMAX -5)
#define TIMER_PAN_PACKET_COUNT      (SIGRTMAX -6)
#define TIMER_HOGP_PACKET_COUNT     (SIGRTMAX -7)
#define TIMER_POLLING               (SIGRTMAX -8)

#define PAN_PACKET_COUNT                5
#define PACKET_COUNT_TIOMEOUT_VALUE     1000//ms

//vendor cmd to fw
#define HCI_VENDOR_ENABLE_PROFILE_REPORT_COMMAND        (0x0018 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_VENDOR_SET_PROFILE_REPORT_COMMAND           (0x0019 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_VENDOR_MAILBOX_CMD                          (0x008F | HCI_GRP_VENDOR_SPECIFIC)

#define HCI_VENDOR_ADD_BITPOOL_FW                       (0x0051 | HCI_GRP_VENDOR_SPECIFIC)

//subcmd to fw for HCI_VENDOR_MAILBOX_CMD
#define HCI_VENDOR_SUB_CMD_WIFI_CHANNEL_AND_BANDWIDTH_CMD   0x11
#define HCI_VENDOR_SUB_CMD_WIFI_FORCE_TX_POWER_CMD          0x17
#define HCI_VENDOR_SUB_CMD_BT_ENABLE_IGNORE_WLAN_ACT_CMD    0x1B
#define HCI_VENDOR_SUB_CMD_BT_REPORT_CONN_SCO_INQ_INFO      0x23
#define HCI_VENDOR_SUB_CMD_BT_AUTO_REPORT_STATUS_INFO       0x27
#define HCI_VENDOR_SUB_CMD_BT_AUTO_REPORT_ENABLE            0x28
#define HCI_VENDOR_SUB_CMD_BT_SET_TXRETRY_REPORT_PARAM      0x29
#define HCI_VENDOR_SUB_CMD_BT_SET_PTATABLE                  0x2A
#define HCI_VENDOR_SUB_CMD_SET_BT_PSD_MODE                  0x31
#define HCI_VENDOR_SUB_CMD_SET_BT_LNA_CONSTRAINT            0x32
#define HCI_VENDOR_SUB_CMD_GET_AFH_MAP_L                    0x40
#define HCI_VENDOR_SUB_CMD_GET_AFH_MAP_M                    0x41
#define HCI_VENDOR_SUB_CMD_GET_AFH_MAP_H                    0x42
#define HCI_VENDOR_SUB_CMD_RD_REG_REQ                       0x43
#define HCI_VENDOR_SUB_CMD_WR_REG_REQ                       0x44

//sub event from fw
#define HCI_VENDOR_PTA_REPORT_EVENT         0x24
#define    HCI_VENDOR_PTA_AUTO_REPORT_EVENT    0x25

//vendor cmd to wifi driver
#define HCI_OP_HCI_EXTENSION_VERSION_NOTIFY (0x0100 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_OP_BT_OPERATION_NOTIFY          (0x0102 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_OP_HCI_BT_INFO_NOTIFY           (0x0106 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_OP_HCI_BT_COEX_NOTIFY           (0x0107 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_OP_HCI_BT_PATCH_VER_NOTIFY      (0x0108 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_OP_HCI_BT_AFH_MAP_NOTIFY        (0x0109 | HCI_GRP_VENDOR_SPECIFIC)
#define HCI_OP_HCI_BT_REGISTER_VALUE_NOTIFY (0x010a | HCI_GRP_VENDOR_SPECIFIC)

//bt operation to notify for HCI_OP_BT_OPERATION_NOTIFY
#define BT_OPCODE_NONE                  0
#define BT_OPCODE_INQUIRY_START         1
#define BT_OPCODE_INQUIRY_END           2
#define BT_OPCODE_PAGE_START            3
#define BT_OPCODE_PAGE_SUCCESS_END      4
#define BT_OPCODE_PAGE_UNSUCCESS_END    5
#define BT_OPCODE_PAIR_START            6
#define BT_OPCODE_PAIR_END              7
#define BT_OPCODE_ENABLE_BT             8
#define BT_OPCODE_DISABLE_BT            9

//bt info reason to wifi for HCI_OP_HCI_BT_INFO_NOTIFY
#define HOST_RESPONSE                   0 //Host response when receive the BT Info Control Event
#define POLLING_RESPONSE                1 //The BT Info response for polling by BT firmware.
#define AUTO_REPORT                     2 //BT auto report by BT firmware.
#define STACK_REPORT_WHILE_DEVICE_D2    3 //Stack report when BT firmware is under power save state(ex:D2)

// vendor event from wifi
#define AIC_HS_EXTENSION_EVENT_WIFI_SCAN            0x01
#define AIC_HS_EXTENSION_EVENT_RADIO_STATUS_NOTIFY  0x02
#define AIC_HS_EXTENSION_EVENT_HCI_BT_INFO_CONTROL  0x03
#define AIC_HS_EXTENSION_EVENT_HCI_BT_COEX_CONTROL  0x04

//op code from wifi for AIC_HS_EXTENSION_EVENT_HCI_BT_COEX_CONTROL
#define BT_PATCH_VERSION_QUERY      0x00
#define IGNORE_WLAN_ACTIVE_CONTROL  0x01
#define LNA_CONSTRAIN_CONTROL       0x02
#define BT_POWER_DECREASE_CONTROL   0x03
#define BT_PSD_MODE_CONTROL         0x04
#define WIFI_BW_CHNL_NOTIFY         0x05
#define QUERY_BT_AFH_MAP            0x06
#define BT_REGISTER_ACCESS          0x07

#define HCI_EXTENSION_VERSION   0x0004

#define HCI_CMD_PREAMBLE_SIZE   3

#define PSM_SDP     0x0001
#define PSM_RFCOMM  0x0003
#define PSM_PAN     0x000F
#define PSM_HID     0x0011
#define PSM_HID_INT 0x0013
#define PSM_AVCTP   0x0017
#define PSM_AVDTP   0x0019
#define PSM_FTP     0x1001
#define PSM_BIP     0x1003
#define PSM_OPP     0x1015
//--Add more if needed--//

enum {
    profile_sco = 0,
    profile_hid = 1,
    profile_a2dp = 2,
    profile_pan = 3,
    profile_hid_interval = 4,
    profile_hogp = 5,
    profile_voice = 6,
    profile_sink = 7,
    profile_max = 8
};

typedef struct AIC_COEX_INFO {
    RT_LIST_ENTRY   list;
    HC_BT_HDR  *    p_buf;
    uint16_t        opcode;
    tINT_CMD_CBACK  p_cback;
}tAIC_COEX_INFO;

//profile info data
typedef struct AIC_PROF_INFO {
    RT_LIST_ENTRY   list;
    uint16_t        handle;
    uint16_t        psm;
    uint16_t        dcid;
    uint16_t        scid;
    uint8_t         profile_index;
}tAIC_PROF_INFO;

//profile info for each connection
typedef struct AIC_CONN_PROF {
    RT_LIST_ENTRY list;
    uint16_t handle;
    uint8_t type;                   //0:l2cap, 1:sco/esco, 2:le
    uint8_t profile_bitmap;         //0:SCO, 1:HID, 2:A2DP, 3:FTP/PAN/OPP, 4: HID_interval, 5:HOGP, 6:VOICE
    int8_t  profile_refcount[8];    //0:SCO, 1:HID, 2:A2DP, 3:FTP/PAN/OPP, 4:TBD, 5:HOGP, 6:VOICE
}tAIC_CONN_PROF;

//profile info for all
typedef struct AIC_PROF {
    RT_LIST_HEAD    conn_hash;      //hash for connections
    RT_LIST_HEAD    profile_list;   //hash for profile info
    RT_LIST_HEAD    coex_list;
    tINT_CMD_CBACK  current_cback;
    pthread_mutex_t profile_mutex;
    pthread_mutex_t coex_mutex;
    pthread_mutex_t btwifi_mutex;
    pthread_t thread_monitor;
    pthread_t thread_data;
    timer_t  timer_a2dp_packet_count;
    timer_t  timer_pan_packet_count;
    timer_t  timer_hogp_packet_count;
    timer_t  timer_polling;
    //struct sockaddr_nl src_addr;    //for netlink
    struct sockaddr_in server_addr; //server addr for kernel socket
    struct sockaddr_in client_addr; //client addr  for kernel socket
    uint32_t a2dp_packet_count;
    uint32_t pan_packet_count;
    uint32_t hogp_packet_count;
    uint32_t voice_packet_count;
    uint8_t  profile_bitmap;
    uint8_t  profile_status;
    int8_t   profile_refcount[8];
    uint8_t  ispairing;
    uint8_t  isinquirying;
    uint8_t  ispaging;
    uint8_t  wifi_state;
    uint8_t  autoreport;
    uint8_t  polling_enable;
    uint8_t  polling_interval;
    volatile uint8_t coex_recv_thread_running;
    //int32_t   nlsocket;
    int32_t  btcoex_chr;
    int32_t  udpsocket;
    uint8_t  piconet_id;
    uint8_t  mode;
    uint8_t  afh_map[10];
    uint16_t hci_reversion;
    uint16_t lmp_subversion;
    uint8_t  wifi_on;
    uint8_t  bt_on;
    //uint8_t  le_profile_index;
}tAIC_PROF;

typedef struct HCI_RETURN_PARAMETER_MAILBOX_REGISTER {
    uint8_t  type;
    uint32_t offset;
    uint32_t value;
}tHCI_RETURN_PARAMETER_MAILBOX_REGISTER;

typedef struct HCI_EVENT_BT_INFO_CONTROL {
    uint8_t     polling_enable;
    uint8_t     polling_time;
    uint8_t     autoreport_enable;
}tHCI_EVENT_BT_INFO_CONTROL;

tAIC_PROF aic_prof;
volatile int poweroff_allowed = 0;
uint8_t coex_log_enable = 0;
static volatile bool coex_cmd_send = false;

#define BIT(_I)                         (uint16_t)(1<<(_I))
#define is_profile_connected(profile)   ((aic_prof.profile_bitmap & BIT(profile)) >0)
#define is_profile_busy(profile)        ((aic_prof.profile_status & BIT(profile)) >0)

static void timeout_handler(int signo, siginfo_t * info, void *context);
static void notify_func(union sigval sig);

static int coex_msg_send(char *tx_msg, int msg_size);
static int coex_msg_recv(uint8_t *recv_msg, uint8_t *msg_size);

#ifndef AIC_PARSE_LOG_BUF_SIZE
#define AIC_PARSE_LOG_BUF_SIZE  1024
#endif
#define AIC_PARSE_LOG_MAX_SIZE  (AIC_PARSE_LOG_BUF_SIZE - 12)

#define LOGI0(t,s) __android_log_write(ANDROID_LOG_INFO, t, s)
static void AicLogMsg(const char *fmt_str, ...)
{
    static char buffer[AIC_PARSE_LOG_BUF_SIZE];
    if(coex_log_enable)
    {
        va_list ap;
        va_start(ap, fmt_str);
        vsnprintf(&buffer[0], AIC_PARSE_LOG_MAX_SIZE, fmt_str, ap);
        va_end(ap);

        LOGI0("aic_parse: ", buffer);
    }
    else
    {
        return;
    }
}

static const char sample_freqs[4][8] = {
        "16", "32", "44.1", "48"
};

static const uint8_t sbc_blocks[4] = { 4, 8, 12, 16 };

static const char chan_modes[4][16] = {
        "MONO", "DUAL_CHANNEL", "STEREO", "JOINT_STEREO"
};

static const char alloc_methods[2][12] = {
        "LOUDNESS", "SNR"
};

static const uint8_t subbands[2] = { 4, 8 };

void print_sbc_header(struct sbc_frame_hdr *hdr)
{
    AicLogMsg("syncword: %02x", hdr->syncword);
    AicLogMsg("freq %skHz", sample_freqs[hdr->sampling_frequency]);
    AicLogMsg("blocks %u", sbc_blocks[hdr->blocks]);
    AicLogMsg("channel mode %s", chan_modes[hdr->channel_mode]);
    AicLogMsg("allocation method %s", alloc_methods[hdr->allocation_method]);
    AicLogMsg("subbands %u", subbands[hdr->subbands]);
}

static timer_t OsAllocateTimer(int signo)
{
    struct sigevent sigev;
    timer_t timerid = (timer_t)-1;

    // Create the POSIX timer to generate signo
    //sigev.sigev_notify = SIGEV_THREAD_ID;
    //sigev.sigev_notify_thread_id = syscall(__NR_gettid);
    //sigev.sigev_signo = signo;
    //sigev.sigev_value.sival_ptr = &timerid;

    memset(&sigev, 0, sizeof(sigev));
    sigev.sigev_notify = SIGEV_THREAD;
    sigev.sigev_notify_function = notify_func;
    sigev.sigev_value.sival_int = signo;

    //ALOGE("OsAllocateTimer aic_parse sigev.sigev_notify_thread_id = syscall(__NR_gettid)!");

    //Create the Timer using timer_create signal
    if (timer_create(CLOCK_REALTIME, &sigev, &timerid) == 0)
    {
        return timerid;
    }
    else
    {
        ALOGE("timer_create error!");
        return (timer_t)-1;
    }
}

static int OsFreeTimer(timer_t timerid)
{
    int ret = 0;
    ret = timer_delete(timerid);
    if(ret != 0)
        ALOGE("timer_delete fail with errno(%d)", errno);

    return ret;
}

static int OsStartTimer(timer_t timerid, int msec, int mode)
{
    struct itimerspec itval;

    itval.it_value.tv_sec = msec / 1000;
    itval.it_value.tv_nsec = (long)(msec % 1000) * (1000000L);

    if (mode == 1)
    {
        itval.it_interval.tv_sec  = itval.it_value.tv_sec;
        itval.it_interval.tv_nsec = itval.it_value.tv_nsec;
    }
    else
    {
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = 0;
    }

    //Set the Timer when to expire through timer_settime
    if (timer_settime(timerid, 0, &itval, NULL) != 0)
    {
        ALOGE("time_settime error!");
        return -1;
    }

    return 0;
}

static int OsStopTimer(timer_t timerid)
{
    return OsStartTimer(timerid, 0, 0);
}

int alloc_polling_timer()
{
/*
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;

    //register the Signal Handler
    sigact.sa_sigaction = timeout_handler;

    // Set up sigaction to catch signal first timer
    if (sigaction(TIMER_POLLING, &sigact, NULL) == -1)
    {
        ALOGE("alloc_polling_timer, sigaction failed");
        return -1;
    }
*/
    // Create and set the timer when to expire
    aic_prof.timer_polling= OsAllocateTimer(TIMER_POLLING);
    AicLogMsg("alloc polling timer");

    return 0;
}

int free_polling_timer()
{
    return OsFreeTimer(aic_prof.timer_polling);
}

int stop_polling_timer()
{
    AicLogMsg("stop polling timer");
    return OsStopTimer(aic_prof.timer_polling);
}

int start_polling_timer(int value)
{
    AicLogMsg("start polling timer");
    return OsStartTimer(aic_prof.timer_polling, value, 1);
}

int alloc_hogp_packet_count_timer()
{
/*
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;

    //register the Signal Handler
    sigact.sa_sigaction = timeout_handler;

    // Set up sigaction to catch signal first timer
    if (sigaction(TIMER_HOGP_PACKET_COUNT, &sigact, NULL) == -1)
    {
        ALOGE("alloc_hogp_packet_count_timer, sigaction failed");
        return -1;
    }
*/
    // Create and set the timer when to expire
    aic_prof.timer_hogp_packet_count= OsAllocateTimer(TIMER_HOGP_PACKET_COUNT);
    AicLogMsg("alloc hogp packet");

    return 0;
}

int free_hogp_packet_count_timer()
{
    return OsFreeTimer(aic_prof.timer_hogp_packet_count);
}

int stop_hogp_packet_count_timer()
{
    AicLogMsg("stop hogp packet");
    return OsStopTimer(aic_prof.timer_hogp_packet_count);
}

int start_hogp_packet_count_timer()
{
    AicLogMsg("start hogp packet");
    return OsStartTimer(aic_prof.timer_hogp_packet_count, PACKET_COUNT_TIOMEOUT_VALUE, 1);
}

int alloc_a2dp_packet_count_timer()
{
/*
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;

    //register the Signal Handler
    sigact.sa_sigaction = timeout_handler;

    // Set up sigaction to catch signal first timer
    if (sigaction(TIMER_A2DP_PACKET_COUNT, &sigact, NULL) == -1)
    {
        ALOGE("alloc_a2dp_packet_count_timer, sigaction failed");
        return -1;
    }
*/
    // Create and set the timer when to expire
    aic_prof.timer_a2dp_packet_count= OsAllocateTimer(TIMER_A2DP_PACKET_COUNT);
    AicLogMsg("alloc a2dp packet");

    return 0;
}

int free_a2dp_packet_count_timer()
{
    return OsFreeTimer(aic_prof.timer_a2dp_packet_count);
}

int stop_a2dp_packet_count_timer()
{
    AicLogMsg("stop a2dp packet");
    return OsStopTimer(aic_prof.timer_a2dp_packet_count);
}

int start_a2dp_packet_count_timer()
{
    AicLogMsg("start a2dp packet");
    return OsStartTimer(aic_prof.timer_a2dp_packet_count, PACKET_COUNT_TIOMEOUT_VALUE, 1);
}

int alloc_pan_packet_count_timer()
{
/*
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;

    //register the Signal Handler
    sigact.sa_sigaction = timeout_handler;

    // Set up sigaction to catch signal first timer
    if (sigaction(TIMER_PAN_PACKET_COUNT, &sigact, NULL) == -1)
    {
        ALOGE("alloc_pan_packet_count_timer, sigaction failed");
        return -1;
    }
*/
    // Create and set the timer when to expire
    aic_prof.timer_pan_packet_count= OsAllocateTimer(TIMER_PAN_PACKET_COUNT);

    AicLogMsg("alloc pan packet");
    return 0;
}

int free_pan_packet_count_timer()
{
    return OsFreeTimer(aic_prof.timer_pan_packet_count);
}

int stop_pan_packet_count_timer()
{
    AicLogMsg("stop pan packet");
    return OsStopTimer(aic_prof.timer_pan_packet_count);
}

int start_pan_packet_count_timer()
{
    AicLogMsg("start pan packet");
    return OsStartTimer(aic_prof.timer_pan_packet_count, PACKET_COUNT_TIOMEOUT_VALUE, 1);
}

static int8_t psm_to_profile_index(uint16_t psm)
{
    switch (psm) {
        case PSM_AVCTP:
        case PSM_SDP:
            return -1;   //ignore

        case PSM_HID:
        case PSM_HID_INT:
            return profile_hid;

        case PSM_AVDTP:
            return profile_a2dp;

        case PSM_PAN:
        case PSM_OPP:
        case PSM_FTP:
        case PSM_BIP:
        case PSM_RFCOMM:
            return profile_pan;

        default:
            return profile_pan;
    }
}

tAIC_CONN_PROF* find_connection_by_handle(tAIC_PROF* h5, uint16_t handle)
{
    RT_LIST_HEAD* head = &h5->conn_hash;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_CONN_PROF* desc = NULL;

    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, tAIC_CONN_PROF, list);
        if ((handle & 0xEFF) == desc->handle )  //only last 12 bit are meanful for hci handle
        {
            return desc;
        }
    }
    return NULL;
}

tAIC_CONN_PROF* allocate_connection_by_handle(uint16_t handle)
{
    tAIC_CONN_PROF * phci_conn = NULL;
    phci_conn = malloc(sizeof(tAIC_CONN_PROF));
    if(phci_conn)
        phci_conn->handle = handle;

    return phci_conn;
}


void init_connection_hash(tAIC_PROF* h5)
{
    RT_LIST_HEAD* head = &h5->conn_hash;
    ListInitializeHeader(head);
}

void add_connection_to_hash(tAIC_PROF* h5, tAIC_CONN_PROF* desc)
{
    RT_LIST_HEAD* head = &h5->conn_hash;
    ListAddToTail(&desc->list, head);
}

void delete_connection_from_hash(tAIC_CONN_PROF* desc)
{
    if (desc)
    {
        ListDeleteNode(&desc->list);
        free(desc);
    }
}

void flush_connection_hash(tAIC_PROF* h5)
{
    RT_LIST_HEAD* head = &h5->conn_hash;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_CONN_PROF* desc = NULL;

    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, tAIC_CONN_PROF, list);
        if (desc)
        {
            ListDeleteNode(&desc->list);
            free(desc);
        }
    }
    //ListInitializeHeader(head);
}

void init_profile_hash(tAIC_PROF* h5)
{
    RT_LIST_HEAD* head = &h5->profile_list;
    ListInitializeHeader(head);
}

uint8_t list_allocate_add(uint16_t handle, uint16_t psm, int8_t profile_index, uint16_t dcid, uint16_t scid)
{
    tAIC_PROF_INFO* pprof_info = NULL;

    if(profile_index < 0)
    {
        ALOGE("PSM(0x%x) do not need parse", psm);
        return FALSE;
    }

    pprof_info = malloc(sizeof(tAIC_PROF_INFO));
    if (NULL == pprof_info)
    {
        ALOGE("list_allocate_add: allocate error");
        return FALSE;
    }

    pprof_info->handle = handle;
    pprof_info->psm = psm;
    pprof_info->scid = scid;
    pprof_info->dcid = dcid;
    pprof_info->profile_index = profile_index;

    ListAddToTail(&(pprof_info->list), &(aic_prof.profile_list));

    return TRUE;
}

void delete_profile_from_hash(tAIC_PROF_INFO* desc)
{
    //AicLogMsg("delete profile for handle: %x, psm:%x, dcid:%x, scid:%x", desc->handle, desc->psm, desc->dcid, desc->scid);
    if (desc)
    {
        ListDeleteNode(&desc->list);
        free(desc);
        desc = NULL;
    }
}

void flush_profile_hash(tAIC_PROF* h5)
{
    RT_LIST_HEAD* head = &h5->profile_list;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_PROF_INFO* desc = NULL;

    pthread_mutex_lock(&aic_prof.profile_mutex);
    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, tAIC_PROF_INFO, list);
        delete_profile_from_hash(desc);
    }
    //ListInitializeHeader(head);
    pthread_mutex_unlock(&aic_prof.profile_mutex);
}

tAIC_PROF_INFO* find_profile_by_handle_scid(tAIC_PROF* h5, uint16_t handle, uint16_t scid)
{
    RT_LIST_HEAD* head = &h5->profile_list;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_PROF_INFO* desc = NULL;

    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, tAIC_PROF_INFO, list);
        if (((handle & 0xFFF) == desc->handle ) && (scid == desc->scid))
        {
            return desc;
        }
    }
    return NULL;
}

tAIC_PROF_INFO* find_profile_by_handle_dcid(tAIC_PROF* h5, uint16_t handle, uint16_t dcid)
{
    RT_LIST_HEAD* head = &h5->profile_list;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_PROF_INFO* desc = NULL;

    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, tAIC_PROF_INFO, list);
        if (((handle & 0xFFF) == desc->handle ) && (dcid == desc->dcid))
        {
            return desc;
        }
    }
    return NULL;
}

tAIC_PROF_INFO* find_profile_by_handle_dcid_scid(tAIC_PROF* h5, uint16_t handle, uint16_t dcid, uint16_t scid)
{
    RT_LIST_HEAD* head = &h5->profile_list;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_PROF_INFO* desc = NULL;

    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, tAIC_PROF_INFO, list);
        if (((handle & 0xFFF) == desc->handle ) && (dcid == desc->dcid) && (scid == desc->scid))
        {
            return desc;
        }
    }
    return NULL;
}

void init_coex_hash(tAIC_PROF* h5)
{
    RT_LIST_HEAD* head = &h5->coex_list;
    ListInitializeHeader(head);
}

void delete_coex_from_hash(tAIC_COEX_INFO* desc)
{
    if (desc)
    {
        ListDeleteNode(&desc->list);
        free(desc);
        desc = NULL;
    }
}

void flush_coex_hash(tAIC_PROF* h5)
{
    RT_LIST_HEAD* head = &h5->coex_list;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_COEX_INFO* desc = NULL;

    pthread_mutex_lock(&aic_prof.coex_mutex);
    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, tAIC_COEX_INFO, list);
        delete_coex_from_hash(desc);
    }
    //ListInitializeHeader(head);
    pthread_mutex_unlock(&aic_prof.coex_mutex);
}

static void aic_cmd_complete_cback(void *p_mem)
{
    pthread_mutex_lock(&aic_prof.coex_mutex);
    RT_LIST_ENTRY* iter = ListGetTop(&(aic_prof.coex_list));
    tAIC_COEX_INFO* desc = NULL;
    if (iter) {
        desc = LIST_ENTRY(iter, tAIC_COEX_INFO, list);
        if (desc)
        {
            ListDeleteNode(&desc->list);
        }
    }
    else {
        coex_cmd_send = false;
    }
    pthread_mutex_unlock(&aic_prof.coex_mutex);

    if(aic_prof.current_cback) {
        aic_prof.current_cback(p_mem);
        aic_prof.current_cback = NULL;
    }

    if(p_mem)
        bt_vendor_cbacks->dealloc(p_mem);

    if(desc) {
        pthread_mutex_lock(&aic_prof.coex_mutex);
        if(aic_prof.bt_on) {
          ALOGE("%s, transmit_command Opcode:%x",__func__, desc->opcode);
          aic_prof.current_cback = desc->p_cback;
          bt_vendor_cbacks->xmit_cb(desc->opcode, desc->p_buf, aic_cmd_complete_cback);
        }
        pthread_mutex_unlock(&aic_prof.coex_mutex);
    }

    free(desc);
    return;
}

void aic_vendor_cmd_to_fw(uint16_t opcode, uint8_t parameter_len, uint8_t* parameter, tINT_CMD_CBACK p_cback)
{
    HC_BT_HDR  *p_buf = NULL;

    if(!aic_prof.bt_on)
        return;

    if(bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + parameter_len);

    if(NULL == p_buf)
    {
        ALOGE("aic_vendor_cmd_to_fw: HC_BT_HDR alloc error");
        return;
    }
    memset(p_buf, 0, (BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + parameter_len));
    p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
    p_buf->offset = 0;
    p_buf->len = HCI_CMD_PREAMBLE_SIZE + parameter_len;
    p_buf->layer_specific = 0;

    uint8_t *p = (uint8_t *) (p_buf + 1);
    UINT16_TO_STREAM(p, opcode);
    *p++ = parameter_len;
    AicLogMsg("aic_vendor_cmd_to_fw: Opcode:%x, parameter_len = %d",opcode, parameter_len);

    if(parameter_len > 0)
    {
        memcpy(p, parameter, parameter_len);
    }
    if(bt_vendor_cbacks)
    {
        pthread_mutex_lock(&aic_prof.coex_mutex);
        if(!aic_prof.bt_on) {
          pthread_mutex_unlock(&aic_prof.coex_mutex);
          return;
        }
        if(!coex_cmd_send) {
            coex_cmd_send = true;
            AicLogMsg("begin transmit_command Opcode:%x",opcode);
            pthread_mutex_unlock(&aic_prof.coex_mutex);
            aic_prof.current_cback = p_cback;
            bt_vendor_cbacks->xmit_cb(opcode, p_buf, aic_cmd_complete_cback);
        }
        else {
            tAIC_COEX_INFO* pcoex_info = NULL;
            pcoex_info = malloc(sizeof(tAIC_COEX_INFO));
            if (NULL == pcoex_info)
            {
                ALOGE("aic_vendor_cmd_to_fw: allocate error");
                pthread_mutex_unlock(&aic_prof.coex_mutex);
                return;
            }

            pcoex_info->p_buf = p_buf;
            pcoex_info->opcode = opcode;
            pcoex_info->p_cback = p_cback;

            ListAddToTail(&(pcoex_info->list), &(aic_prof.coex_list));
            pthread_mutex_unlock(&aic_prof.coex_mutex);
        }

    }
    return ;
}


#ifdef AIC_ROLE_SWITCH_RETRY

static timer_t OsAllocateTimer_role_switch(tTIMER_HANDLE_ROLE_SWITCH timer_callback,role_monitor_cb *p_cb)
{
    struct sigevent sigev;
    timer_t timerid;

    memset(&sigev, 0, sizeof(struct sigevent));
    sigev.sigev_notify = SIGEV_THREAD;
    sigev.sigev_notify_function = timer_callback;
    sigev.sigev_value.sival_ptr = p_cb;

    ALOGD("OsAllocateTimer bt_service sigev.sigev_notify_thread_id = syscall(__NR_gettid)!");

    if (timer_create(CLOCK_REALTIME, &sigev, &timerid) == 0)
    {
        return timerid;
    }
    else
    {
        ALOGE("timer_create error!");
        return (timer_t)-1;
    }
}

static int OsFreeTimer_role_switch(timer_t timerid)
{
    int ret = 0;
    if(timerid == (timer_t)-1) {
        ALOGE("OsFreeTimer fail timer id error");
        return -1;
    }
    ret = timer_delete(timerid);
    if(ret != 0)
        ALOGE("timer_delete fail with errno(%d)", errno);

    return ret;
}

 static int OsStartTimer_role_switch(timer_t timerid, int msec, int mode)
 {
    struct itimerspec itval;

    if(timerid == (timer_t)-1) {
        ALOGE("OsStartTimer fail timer id error");
        return -1;
    }
    itval.it_value.tv_sec = msec / 1000;
    itval.it_value.tv_nsec = (long)(msec % 1000) * (1000000L);
    //ALOGE("OsStartTimer_role_switch  = %ld itval.it_value.tv_nsec = %ld libs_liu",itval.it_value.tv_sec,itval.it_value.tv_nsec);
    if (mode == 1)
    {
        itval.it_interval.tv_sec    = itval.it_value.tv_sec;
        itval.it_interval.tv_nsec = itval.it_value.tv_nsec;
    }
    else
    {
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = 0;
    }

    //Set the Timer when to expire through timer_settime
    //ALOGE("OsStartTimer_role_switch  = %ld itval.it_value.tv_nsec = %ld libs_liu end timerid = %ld",itval.it_value.tv_sec,itval.it_value.tv_nsec,(long)timerid);
    if (timer_settime(timerid, 0, &itval, NULL) != 0)
    {
        ALOGE("time_settime error!");
        return -1;
    }
    ALOGI("OsStartTimer_role_switch  = %ld itval.it_value.tv_nsec = %ld",itval.it_value.tv_sec,itval.it_value.tv_nsec);

    return 0;

}

static int OsStopTimer_role_switch(timer_t timerid)
{
   return OsStartTimer_role_switch(timerid, 0, 0);
}

int find_remote_device_by_address(BD_ADDR address){
    int  index = 0;
    role_monitor_cb    *p_cb = &(role_monitor_pool[0]);
    for (index = 0; index < MAX_LINKS; index++,p_cb++){
        if((p_cb->in_use)&&(!memcmp (p_cb->remote_bd_addr, address, BD_ADDR_LEN))){
            return index;
        }
    }
    ALOGE( "find_remote_device_by_address  device not found");
    return -1;
}


int find_pending_role_switch_process(){
    int  index = 0;
    role_monitor_cb    *p_cb = &(role_monitor_pool[0]);
    for (index = 0; index < MAX_LINKS; index++,p_cb++){
        if((p_cb->in_use)&&(p_cb->state == ROLE_SWITCH_COMMAND_PENDING)){
            return index;
        }
    }
    ALOGE( "find_pending_role_switch_process  device not found");
    return -1;
}


int allocate_role_switch_pool_by_handle(uint16_t handle,BD_ADDR remote_address)
{
    int  index = 0;
    role_monitor_cb    *p_cb = &(role_monitor_pool[0]);
    /*check there is no same address exist*/
    if(((index = find_remote_device_by_address(remote_address)) != -1)){
        if(role_monitor_pool[index].handle == UNKOWN_HANDLE){
            ALOGI( "allocate_role_switch_pool_by_handle slot has been exist and is waiting update\n");
            role_monitor_pool[index].handle = handle;
            return index;
        }else{
            ALOGE( "allocate_role_switch_pool_by_handle slot has been exist it ,return \n");
            return -1;
        }
    }
    for (index = 0; index < MAX_LINKS; index++,p_cb++){
        if(!(p_cb->in_use)){
            p_cb->count = 0;
            p_cb->diff_s = 0;
            p_cb->handle = handle;
            p_cb->time = 0;
            p_cb->in_use = TRUE;
            p_cb->timer_hci_role_switch_cmd = (timer_t)-1;
            memcpy(p_cb->remote_bd_addr,remote_address,BD_ADDR_LEN);
            return index;
        }
    }
    ALOGE( "allocate_role_switch_pool_by_handle  no slot found");
    return -1;

}

static void aic_record_connection_info(uint8_t* p){
    uint8_t status = 0;
    uint16_t handle = 0;
    int index = 0;
    BD_ADDR remote_address;
    status = *p++;
    if(status != 0){
        ALOGE("aic_record_connection_info handle = 0x%x status = %d connection failed! ignore !",handle,status);
        return;
    }
    STREAM_TO_UINT16 (handle, p);
    //ALOGE("aic_record_connection_info handle = 0x%x",handle);
    memcpy(remote_address,p,BD_ADDR_LEN);
    //ALOGE("aic_record_connection_info remote_address = %x %x %x %x %x %x libs_liu",remote_address[0],remote_address[1],
     //   remote_address[2],remote_address[3],remote_address[4],remote_address[5]);
    index = allocate_role_switch_pool_by_handle(handle,remote_address);
    if(index <0){
        ALOGE("aic_record_connection_info index = 0x%x",index);
        return;
    }
    ALOGD("aic_record_connection_info index = 0x%x",index);
}

static void aic_connection_info_clear(uint8_t* p){

    uint8_t status = 0;
    uint16_t handle = 0;
    status = *p++;
    STREAM_TO_UINT16(handle, p);
    ALOGE("aic_connection_info_clear handle = 0x%x libs_liu",handle);

    int  index = 0;
    role_monitor_cb    *p_cb = &(role_monitor_pool[0]);
    for (index = 0; index < MAX_LINKS; index++,p_cb++){
        if((p_cb->in_use)&&(p_cb->handle == handle)){
            //ALOGE("aic_connection_info_clear  begin to clear this slot  p_cb->timer_hci_role_switch_cmd = %ld",(long)p_cb->timer_hci_role_switch_cmd);
            p_cb->in_use = FALSE;
            p_cb->state = ROLE_SWITCH_COMMAND_NONE;
            p_cb->handle = 0;
            p_cb->diff_s = 0;
            p_cb->count = 0;
            p_cb->isMaster = FALSE;
            OsStopTimer_role_switch(p_cb->timer_hci_role_switch_cmd);
            OsFreeTimer_role_switch(p_cb->timer_hci_role_switch_cmd);
            p_cb->timer_hci_role_switch_cmd = (timer_t)-1;
            memcpy(p_cb->remote_bd_addr,EMPTY_ADDR,BD_ADDR_LEN);
            return;
        }
    }
    ALOGD( "aic_connection_info_clear  done");
    return ;
}

static void Aic_Role_switch_Event_Cback(void *arg)
{
    if(arg != NULL)
    {
        HC_BT_HDR  *p_buf = NULL;
        p_buf = (HC_BT_HDR *)arg;
        uint8_t *p = p_buf->data;
        ALOGE( " Aic_Role_switch_Event_Cback event_code = %d length = %d",p[0],p[1]);

        /*find out which one inititor this process*/
        int index = find_pending_role_switch_process();
        if(index == -1)
            return;
        role_monitor_cb    *p_cb = &(role_monitor_pool[index]);
        p_cb->state = ROLE_SWITCH_COMMAND_SUCCESS;
        if(p[5] == 0x0c){
            p_cb->state = ROLE_SWITCH_COMMAND_DISALLOW;
            ALOGE( " Aic_Role_switch_Event_Cback  command is disallowed libs_liu");
            p_cb->count  = 1;
            aic_start_role_switch_schedule(p_cb);
        }

    }else{
        ALOGE("%s Aic_Role_switch_Event_Cback arg == NULL, it should not happend", __func__);
    }
}



static void aic_send_role_switch_handler(union sigval sigev_value){
     role_monitor_cb * p_cb = (role_monitor_cb *)sigev_value.sival_ptr;
     if(!p_cb->in_use){
        ALOGE( "aic_send_role_switch_handler  p_cb now is not in use ,return !");
        return;
     }
     p_cb->state = ROLE_SWITCH_COMMAND_PENDING;
    /*begin to send hci command to controller*/
    uint8_t param_len = 7;
    uint8_t param[param_len];
    memcpy(param,p_cb->remote_bd_addr,BD_ADDR_LEN);
    param[param_len-1] = 0;
    aic_vendor_cmd_to_fw(HCI_CMD_VNDR_ROLESWITCH,param_len , param, Aic_Role_switch_Event_Cback);
    /*remember to free the timer*/
    OsStopTimer_role_switch(p_cb->timer_hci_role_switch_cmd);
    OsFreeTimer_role_switch(p_cb->timer_hci_role_switch_cmd);
    p_cb->timer_hci_role_switch_cmd = (timer_t)-1;

}

static void aic_start_role_switch_schedule(role_monitor_cb  * p){
    role_monitor_cb *p_cb = p;
    double time_out;
    if(p_cb == NULL){
        ALOGE("aic_start_role_switch_schedule  p_cb==NULL");
        return;
    }
    if(p_cb->diff_s > TIME_LIMIT_FOR_ROLE_SWITCH){
        ALOGE("aic_start_role_switch_schedule p_cb->diff_s is larger then threshold value");
        p_cb->count = 0;
    }
    time_out = pow((double)2,(double)(p_cb->count))*500;
    if(time_out > TIME_LIMIT_FOR_ROLE_SWITCH*1000){
        ALOGE("aic_start_role_switch_schedule time_out is too large,do not try again");
    }

    p_cb->timer_hci_role_switch_cmd = OsAllocateTimer_role_switch(aic_send_role_switch_handler,p_cb);
    if(p_cb->timer_hci_role_switch_cmd == (timer_t)-1) {
        ALOGE("%s : alloc reply timer fail!", __func__);
        return;
    }
    ALOGE("%s : time_out = %lf", __func__,time_out);
    OsStartTimer_role_switch(p_cb->timer_hci_role_switch_cmd, (int)time_out, 1);
}

static void aic_handle_role_change_evt(uint8_t* p){
    uint8_t status = 0;
    int index = 0;
    uint8_t new_role = 0;
    status = *p++;
    BD_ADDR remote_address;
    ALOGE("aic_handle_role_change_evt  status = %d",status);
    memcpy(remote_address,p,BD_ADDR_LEN);
    ALOGE("aic_handle_role_change_evt remote_address = %x %x %x %x %x %x",remote_address[0],remote_address[1],
        remote_address[2],remote_address[3],remote_address[4],remote_address[5]);
    p += BD_ADDR_LEN;
    new_role = *p;
    if(new_role == 0){
        ALOGE("aic_handle_role_change_evt  now is Mastar ,do nothing");
    }else{
        ALOGE("aic_handle_role_change_evt  now is slave ");
        /*find the slot */
        index = find_remote_device_by_address(remote_address);
        if(index < 0){
            ALOGE("aic_handle_role_change_evt device not found ,maybe role change comming first and alloc one libs_liu");
            index = allocate_role_switch_pool_by_handle(UNKOWN_HANDLE,remote_address);
             if(index <0){
                ALOGE("allocate_role_switch_pool_by_handle failed  index = 0x%x libs_liu",index);
                return;
             }
        }
        /*get time_r*/
        role_monitor_cb  * p_cb = &(role_monitor_pool[index]);
        time_t now = time(NULL);
        p_cb->diff_s = difftime(now,p_cb->time);
        ALOGE("aic_handle_role_change_evt p_cb->diff_s =%lf  libs_liu",p_cb->diff_s);
        p_cb->time = now;
        p_cb->count++;
        p_cb->isMaster = FALSE;
        /*begin to schedule timer*/
        aic_start_role_switch_schedule(p_cb);
    }

}

#endif

void aic_notify_profileinfo_to_fw()
{
    RT_LIST_HEAD* head = NULL;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    tAIC_CONN_PROF* hci_conn = NULL;
    uint8_t  handle_number = 0;
    uint32_t buffer_size = 0;
    uint8_t *p_buf = NULL;

    head = &aic_prof.conn_hash;
    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        hci_conn = LIST_ENTRY(iter, tAIC_CONN_PROF, list);
        if (hci_conn && hci_conn->profile_bitmap)
            handle_number++;
    }

    buffer_size = 1 + handle_number*3 + 1;

    p_buf = (uint8_t *) malloc(buffer_size);

    if(NULL == p_buf)
    {
        ALOGE("aic_notify_profileinfo_to_fw: alloc error");
        return;
    }
    uint8_t *p = (uint8_t *)p_buf;

    AicLogMsg("aic_notify_profileinfo_to_fw, BufferSize is %x", buffer_size);
    *p++ = handle_number;
    AicLogMsg("aic_notify_profileinfo_to_fw, NumberOfHandles is %x", handle_number);
    head = &aic_prof.conn_hash;
    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        hci_conn = LIST_ENTRY(iter, tAIC_CONN_PROF, list);
        if (hci_conn && hci_conn->profile_bitmap)
        {
            UINT16_TO_STREAM(p, hci_conn->handle);
            AicLogMsg("aic_notify_profileinfo_to_fw, handle is %x",hci_conn->handle);
            *p++ = hci_conn->profile_bitmap;
            AicLogMsg("aic_notify_profileinfo_to_fw, profile_bitmap is %x",hci_conn->profile_bitmap);
            handle_number --;
        }
        if(0 == handle_number)
            break;
    }

    *p++ = aic_prof.profile_status;
    AicLogMsg("aic_notify_profileinfo_to_fw, profile_status is %x",aic_prof.profile_status);

    aic_vendor_cmd_to_fw(HCI_VENDOR_SET_PROFILE_REPORT_COMMAND, buffer_size, p_buf, NULL);

    free(p_buf);

    return ;
}

void update_profile_state(uint8_t profile_index, uint8_t is_busy)
{
    uint8_t need_update = FALSE;

    if((aic_prof.profile_bitmap & BIT(profile_index)) == 0)
    {
        ALOGE("update_profile_state: ERROR!!! profile(Index: %x) does not exist", profile_index);
        return;
    }

    if(is_busy)
    {
        if((aic_prof.profile_status & BIT(profile_index)) == 0)
        {
            need_update = TRUE;
            aic_prof.profile_status |= BIT(profile_index);
        }
    }
    else
    {
        if((aic_prof.profile_status & BIT(profile_index)) > 0)
        {
            need_update = TRUE;
            aic_prof.profile_status &= ~(BIT(profile_index));
        }
    }

    if(need_update)
    {
        AicLogMsg("update_profile_state, aic_prof.profie_bitmap = %x", aic_prof.profile_bitmap);
        AicLogMsg("update_profile_state, aic_prof.profile_status = %x", aic_prof.profile_status);
        aic_notify_profileinfo_to_fw();
    }
}

void aic_check_setup_timer(int8_t profile_index)
{
    if(profile_index == profile_a2dp) {
        aic_prof.a2dp_packet_count = 0;
        start_a2dp_packet_count_timer();
    }
    if(profile_index == profile_pan) {
        aic_prof.pan_packet_count = 0;
        start_pan_packet_count_timer();
    }
    //hogp & voice share one timer now
    if((profile_index == profile_hogp) || (profile_index == profile_voice)) {
        if((0 == aic_prof.profile_refcount[profile_hogp])
                && (0 == aic_prof.profile_refcount[profile_voice])) {
            aic_prof.hogp_packet_count = 0;
            aic_prof.voice_packet_count = 0;
            start_hogp_packet_count_timer();
        }
    }
}

void aic_check_del_timer(int8_t profile_index)
{
    if(profile_a2dp == profile_index)
    {
        aic_prof.a2dp_packet_count = 0;
        stop_a2dp_packet_count_timer();
    }
    if(profile_pan == profile_index)
    {
        aic_prof.pan_packet_count = 0;
        stop_pan_packet_count_timer();
    }
    if(profile_hogp == profile_index)
    {
        aic_prof.hogp_packet_count = 0;
        if(aic_prof.profile_refcount[profile_voice] == 0)
            stop_hogp_packet_count_timer();
    }
    if(profile_voice == profile_index)
    {
        aic_prof.voice_packet_count = 0;
        if(aic_prof.profile_refcount[profile_hogp] == 0)
            stop_hogp_packet_count_timer();
    }
}
void update_profile_connection(tAIC_CONN_PROF * phci_conn, int8_t profile_index, uint8_t is_add)
{
    uint8_t need_update = FALSE;
    int kk = 0;

    AicLogMsg("update_profile_connection: is_add=%d, psm_index=%d", is_add, profile_index);
    if (profile_index < 0)
        return;

    if(is_add)
    {
        if(aic_prof.profile_refcount[profile_index] == 0)
        {
            need_update = TRUE;
            aic_prof.profile_bitmap |= BIT(profile_index);

            //SCO is always busy
            if(profile_index == profile_sco)
                aic_prof.profile_status |= BIT(profile_index);

            aic_check_setup_timer(profile_index);
        }
        aic_prof.profile_refcount[profile_index]++;

        if(0 == phci_conn->profile_refcount[profile_index])
        {
            need_update = TRUE;
            phci_conn->profile_bitmap |= BIT(profile_index);
        }
        phci_conn->profile_refcount[profile_index]++;
    }
    else
    {
        aic_prof.profile_refcount[profile_index]--;
        AicLogMsg("for test: --, aic_prof.profile_refcount[%x] = %x", profile_index, aic_prof.profile_refcount[profile_index]);
        if(aic_prof.profile_refcount[profile_index] == 0)
        {
            need_update = TRUE;
            aic_prof.profile_bitmap &= ~(BIT(profile_index));

            //If profile does not exist, Status is meaningless
            aic_prof.profile_status &= ~(BIT(profile_index));
            aic_check_del_timer(profile_index);
        }

        phci_conn->profile_refcount[profile_index]--;
        if(0 == phci_conn->profile_refcount[profile_index])
        {
            need_update = TRUE;
            phci_conn->profile_bitmap &= ~(BIT(profile_index));

            //clear profile_hid_interval if need
            if(profile_hid == profile_index)
            {
                if((phci_conn->profile_bitmap &(BIT(profile_hid_interval))))
                {
                    phci_conn->profile_bitmap &= ~(BIT(profile_hid_interval));
                    aic_prof.profile_refcount[profile_hid_interval]--;
                }
            }
        }
    }

    if(need_update)
    {
        AicLogMsg("update_profile_connection: aic_h5.profile_bitmap = %x", aic_prof.profile_bitmap);
        for(kk=0; kk<8; kk++)
            AicLogMsg("update_profile_connection: aic_h5.profile_refcount[%d] = %d", kk, aic_prof.profile_refcount[kk]);
        aic_notify_profileinfo_to_fw();
    }
}

void update_hid_active_state(uint16_t handle, uint16_t interval)
{
    uint8_t need_update = 0;
    AicLogMsg("update_hid_active_state: handle = %x, interval = 0x%x", handle, interval);
    tAIC_CONN_PROF *phci_conn = find_connection_by_handle(&aic_prof, handle);

    if(phci_conn == NULL)
        return;

    if(((phci_conn->profile_bitmap)&(BIT(profile_hid))) == 0)
    {
        AicLogMsg("hid not connected in the handle, nothing to be down");
        return;
    }

    if(interval < 60)
    {
        if((phci_conn->profile_bitmap &(BIT(profile_hid_interval))) == 0)
        {
            need_update = 1;
            phci_conn->profile_bitmap |= BIT(profile_hid_interval);

            aic_prof.profile_refcount[profile_hid_interval]++;
            if(aic_prof.profile_refcount[profile_hid_interval] == 1)
                aic_prof.profile_status |= BIT(profile_hid);
        }
    }
    else
    {
        if((phci_conn->profile_bitmap &(BIT(profile_hid_interval))))
        {
            need_update = 1;
            phci_conn->profile_bitmap &= ~(BIT(profile_hid_interval));

            aic_prof.profile_refcount[profile_hid_interval]--;
            if(aic_prof.profile_refcount[profile_hid_interval] == 0)
                aic_prof.profile_status &= ~(BIT(profile_hid));
        }
    }

    if(need_update)
        aic_notify_profileinfo_to_fw();
}
uint8_t handle_l2cap_con_req(uint16_t handle, uint16_t psm, uint16_t scid, uint8_t direction)
{
    uint8_t status = FALSE;
    tAIC_PROF_INFO* prof_info = NULL;

    int8_t profile_index = psm_to_profile_index(psm);

    if(profile_index < 0) {
        AicLogMsg("PSM(0x%x) do not need parse", psm);
        return status;
    }

    pthread_mutex_lock(&aic_prof.profile_mutex);
    if(direction)//1: out
        prof_info = find_profile_by_handle_scid(&aic_prof, handle, scid);
    else // 0:in
        prof_info = find_profile_by_handle_dcid(&aic_prof, handle, scid);

    if(prof_info)
    {
        AicLogMsg("handle_l2cap_con_req: This profile is already exist!!!");
        pthread_mutex_unlock(&aic_prof.profile_mutex);
        return status;
    }

    if(direction)//1: out
        status = list_allocate_add(handle, psm, profile_index, 0, scid);
    else // 0:in
        status = list_allocate_add(handle, psm, profile_index, scid, 0);

    pthread_mutex_unlock(&aic_prof.profile_mutex);

    if (!status)
        ALOGE("handle_l2cap_con_req: list_allocate_add failed!");

    return status;
}

uint8_t handle_l2cap_con_rsp(uint16_t handle, uint16_t dcid, uint16_t scid, uint8_t direction, uint8_t result)
{
    tAIC_PROF_INFO* prof_info = NULL;

    pthread_mutex_lock(&aic_prof.profile_mutex);
    if(!direction)//0, in
        prof_info = find_profile_by_handle_scid(&aic_prof, handle, scid);
    else //1, out
        prof_info = find_profile_by_handle_dcid(&aic_prof, handle, scid);

    if (!prof_info)
    {
        //AicLogMsg("handle_l2cap_con_rsp: prof_info Not Find!!");
        pthread_mutex_unlock(&aic_prof.profile_mutex);
        return 0;
    }

    if(!result)//success
    {
        AicLogMsg("l2cap connection success, update connection");
        if(!direction)//0, in
            prof_info->dcid = dcid;
        else//1, out
            prof_info->scid = dcid;

        tAIC_CONN_PROF *phci_conn = find_connection_by_handle(&aic_prof, handle);
        if(phci_conn)
            update_profile_connection(phci_conn, prof_info->profile_index, TRUE);
    }

    pthread_mutex_unlock(&aic_prof.profile_mutex);
    return 1;
}

uint8_t handle_l2cap_discon_req(uint16_t handle, uint16_t dcid, uint16_t scid, uint8_t direction)
{
    tAIC_PROF_INFO* prof_info = NULL;
    AicLogMsg("l2cap_discon_req, handle = %x, dcid = %x, scid = %x, direction = %x", handle, dcid, scid, direction);
    pthread_mutex_lock(&aic_prof.profile_mutex);
    if(!direction)//0: in
        prof_info = find_profile_by_handle_dcid_scid(&aic_prof, handle, scid, dcid);
    else //1: out
        prof_info = find_profile_by_handle_dcid_scid(&aic_prof, handle, dcid, scid);

    if (!prof_info)
    {
        //AicLogMsg("handle_l2cap_discon_req: prof_info Not Find!");
        pthread_mutex_unlock(&aic_prof.profile_mutex);
        return 0;
    }

    tAIC_CONN_PROF *phci_conn = find_connection_by_handle(&aic_prof, handle);
    if(NULL == phci_conn)
    {
        pthread_mutex_unlock(&aic_prof.profile_mutex);
        return 0;
    }

    update_profile_connection(phci_conn, prof_info->profile_index, FALSE);
    if (prof_info->profile_index == profile_a2dp && (phci_conn->profile_bitmap & BIT(profile_sink)))
        update_profile_connection(phci_conn, profile_sink, FALSE);
    delete_profile_from_hash(prof_info);
    pthread_mutex_unlock(&aic_prof.profile_mutex);

    return 1;
}

void packets_count(uint16_t handle, uint16_t scid, uint16_t length, uint8_t direction, uint8_t *user_data)
{
    tAIC_PROF_INFO* prof_info = NULL;
    //uint8_t profile_type;

    tAIC_CONN_PROF* hci_conn = find_connection_by_handle(&aic_prof, handle);
    if(NULL == hci_conn)
        return;

    if(0 == hci_conn->type)//l2cap
    {
        if(!direction)    //0: in
            prof_info = find_profile_by_handle_scid(&aic_prof, handle, scid);
        else //1: out
            prof_info = find_profile_by_handle_dcid(&aic_prof, handle, scid);

        if(!prof_info)
        {
            //AicLogMsg("packets_count: prof_info Not Find!");
            //ALOGE("handle=%x, scid=%x, length=%d,",handle, scid, length);
            return ;
        }

        if((prof_info->profile_index == profile_a2dp) && (length > 100))//avdtp media data
        {
            if(!is_profile_busy(profile_a2dp)){
                struct sbc_frame_hdr *sbc_header;
                struct rtp_header *rtph;
                uint8_t bitpool;
                update_profile_state(profile_a2dp, TRUE);
                if (!direction) {
                    update_profile_connection(hci_conn, profile_sink, true);
                    update_profile_state(profile_sink, TRUE);
                }
                rtph = (struct rtp_header *)user_data;
                AicLogMsg("rtp: v %u, cc %u, pt %u", rtph->v, rtph->cc, rtph->pt);
                /* move forward */
                user_data += sizeof(struct rtp_header) + rtph->cc * 4 + 1;
                /* point to the sbc frame header */
                sbc_header = (struct sbc_frame_hdr *)user_data;
                bitpool = sbc_header->bitpool;
                print_sbc_header(sbc_header);
                AicLogMsg("rtp: v %u, cc %u, pt %u", rtph->v, rtph->cc, rtph->pt);
                aic_vendor_cmd_to_fw(HCI_VENDOR_ADD_BITPOOL_FW, 1, &bitpool, NULL);
            }
            aic_prof.a2dp_packet_count++;
        }

        if(prof_info->profile_index == profile_pan)
            aic_prof.pan_packet_count++;
    }
}

static void timeout_handler(int signo, siginfo_t * info, void *context)
{
    AIC_UNUSED(info);
    AIC_UNUSED(context);
    if (signo == TIMER_POLLING)
    {
        AicLogMsg("polling timeout");
        if(aic_prof.polling_enable)
        {
            uint8_t temp_cmd[1];
            temp_cmd[0] = HCI_VENDOR_SUB_CMD_BT_REPORT_CONN_SCO_INQ_INFO;
            aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 1, temp_cmd, NULL);
        }
    }
    else if (signo == TIMER_A2DP_PACKET_COUNT)
    {
        AicLogMsg("count a2dp packet timeout, a2dp_packet_count = %d",aic_prof.a2dp_packet_count);
        if(aic_prof.a2dp_packet_count == 0)
        {
            if(is_profile_busy(profile_a2dp))
            {
                AicLogMsg("timeout_handler: a2dp busy->idle!");
                update_profile_state(profile_a2dp, FALSE);
                if (is_profile_busy(profile_sink)) {
                    AicLogMsg("timeout_handler: sink busy->idle!");
                    update_profile_state(profile_sink, FALSE);
                }
            }
        }
        aic_prof.a2dp_packet_count = 0;
    }
    else if (signo == TIMER_HOGP_PACKET_COUNT)
    {
        AicLogMsg("count hogp packet timeout, hogp_packet_count = %d",aic_prof.hogp_packet_count);
        if(aic_prof.hogp_packet_count == 0)
        {
            if(is_profile_busy(profile_hogp))
            {
                AicLogMsg("timeout_handler: hogp busy->idle!");
                update_profile_state(profile_hogp, FALSE);
            }
        }
        aic_prof.hogp_packet_count = 0;

        AicLogMsg("count hogp packet timeout, voice_packet_count = %d",aic_prof.voice_packet_count);
        if(aic_prof.voice_packet_count == 0)
        {
            if(is_profile_busy(profile_voice))
            {
                AicLogMsg("timeout_handler: voice busy->idle!");
                update_profile_state(profile_voice, FALSE);
            }
        }
        aic_prof.voice_packet_count = 0;
    }
    else if (signo == TIMER_PAN_PACKET_COUNT)
    {
        AicLogMsg("count pan packet timeout, pan_packet_count = %d",aic_prof.pan_packet_count);
        if(aic_prof.pan_packet_count < PAN_PACKET_COUNT)
        {
            if(is_profile_busy(profile_pan))
            {
                AicLogMsg("timeout_handler: pan busy->idle!");
                update_profile_state(profile_pan, FALSE);
            }
        }
        else
        {
            if(!is_profile_busy(profile_pan))
            {
                AicLogMsg("timeout_handler: pan idle->busy!");
                update_profile_state(profile_pan, TRUE);
            }
        }
        aic_prof.pan_packet_count = 0;
    }
    else
    {
        ALOGE("aic_parse_data timer unspported signo(%d)", signo);
    }
}

static void notify_func(union sigval sig)
{
    int signo = sig.sival_int;
    timeout_handler(signo, NULL, NULL);
}

#if 0
int netlink_send(int nlsk, char *buffer)
{
    struct nlmsghdr* nlhdr;
    struct iovec iov;
    struct msghdr msg;
    struct sockaddr_nl nladdr;

    if(nlsk <= 0)
        return -1;

    memset(&msg, 0 ,sizeof(struct msghdr));
    memset(&nladdr, 0 ,sizeof(struct sockaddr_nl));

    nlhdr = (struct nlmsghdr *)malloc(NLMSG_SPACE(strlen(buffer) + 1));
    strcpy(NLMSG_DATA(nlhdr),buffer);

    nlhdr->nlmsg_len = NLMSG_LENGTH(strlen(buffer) + 1);
    nlhdr->nlmsg_pid = getpid(); //sender pid
    nlhdr->nlmsg_flags = NLM_F_REQUEST;
    nlhdr->nlmsg_type = NLMSG_MIN_TYPE;

    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = 0; //send to kernel
    nladdr.nl_groups = 0;

    iov.iov_base = (void *)nlhdr;
    iov.iov_len = nlhdr->nlmsg_len;

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_name = (void *)&(nladdr);
    msg.msg_namelen = sizeof(nladdr);

    return sendmsg(nlsk, &msg, 0);
}
#endif

int udpsocket_send(char *tx_msg, int msg_size)
{
    int n; /* message byte size */

    AicLogMsg("udpsocket_send tx_msg:%s",tx_msg);
    n = sendto(aic_prof.udpsocket, tx_msg, msg_size, 0, (struct sockaddr *) &aic_prof.client_addr, sizeof(aic_prof.client_addr));
    if (n < 0)
    {
        ALOGE("ERROR in sendto");
        return -1;
    }
    return 0;
}

int udpsocket_recv(uint8_t *recv_msg, uint8_t *msg_size)
{
    //struct hostent *hostp;  /* client host info */
    char buf[MAX_PAYLOAD];  /* message buf */
    //char *hostaddrp;        /* dotted decimal host addr string */
    int n;                  /* message byte size */
    struct sockaddr_in recv_addr;
    socklen_t clientlen = sizeof(recv_addr);
    struct pollfd pfd = {
        .events = POLLPRI | POLLIN,
        .revents = 0,
        .fd = aic_prof.udpsocket
    };

    bzero(buf, MAX_PAYLOAD);

    while (poll(&pfd, 1, 1000) <= 0) {
        if (aic_prof.coex_recv_thread_running ==0) {
            AicLogMsg("%s, SIGUSR2 should have caught us before this", __func__);
            return -1;
        }
    }


    n = recvfrom(aic_prof.udpsocket, buf, MAX_PAYLOAD, 0, (struct sockaddr *) &recv_addr, &clientlen);
    if (n < 0) {
        ALOGE("ERROR in recvfrom");
        return -1;
    } else {
        *msg_size = n;
        memcpy(recv_msg,buf,n);
    }
    return 0;
}


int btcoex_chr_send(char *tx_msg, int msg_size)
{
    int n; /* message byte size */

    AicLogMsg("btcoex_chr_send tx_msg:%s",tx_msg);
    AIC_NO_INTR (n = write(aic_prof.btcoex_chr, tx_msg, msg_size));
    if (n < 0)
    {
        ALOGE("ERROR in write");
        return -1;
    }
    return n;
}

int btcoex_chr_recv(uint8_t *recv_msg, uint8_t *msg_size)
{
    char buf[MAX_PAYLOAD];  /* message buf */
    int n = -1;                  /* message byte size */
    struct pollfd pfd = {
        .events = POLLPRI|POLLIN|POLLHUP|POLLERR|POLLRDHUP,
        .revents = 0,
        .fd = aic_prof.btcoex_chr
    };

    bzero(buf, MAX_PAYLOAD);

    while (poll(&pfd, 1, 1000) <= 0) {
        if (aic_prof.coex_recv_thread_running == 0) {
            AicLogMsg("%s, SIGUSR2 should have caught us before this", __func__);
            return -1;
        }
    }

    if (pfd.revents & POLLIN) {
        AIC_NO_INTR(n = read(aic_prof.btcoex_chr, buf, MAX_PAYLOAD));
        if (n < 0) {
            ALOGE("ERROR in recvfrom");
            return -1;
        } else {
            *msg_size = n;
            memcpy(recv_msg, buf, n);
        }
    }
    else {
        ALOGE("aic_btcoex is wrong");
        return -1;
    }
    return 0;
}

void aic_notify_extension_version_to_wifi()
{
    uint8_t para_length = 2;
    char p_buf[para_length + HCI_CMD_PREAMBLE_SIZE];

    if(!aic_prof.wifi_on)
        return;

    char *p = p_buf;
    UINT16_TO_STREAM(p, HCI_OP_HCI_EXTENSION_VERSION_NOTIFY);
    *p++ = para_length;
    UINT16_TO_STREAM(p, HCI_EXTENSION_VERSION);
    AicLogMsg("extension version is 0x%x", HCI_EXTENSION_VERSION);
    if(coex_msg_send(p_buf, para_length + HCI_CMD_PREAMBLE_SIZE) < 0)
        ALOGE("aic_notify_extension_version_to_wifi: udpsocket send error");
}

void aic_notify_btpatch_version_to_wifi()
{
    uint8_t para_length = 4;
    char p_buf[para_length + HCI_CMD_PREAMBLE_SIZE];

    if(!aic_prof.wifi_on)
        return;

    char *p = p_buf;
    UINT16_TO_STREAM(p, HCI_OP_HCI_BT_PATCH_VER_NOTIFY);
    *p++ = para_length;
    UINT16_TO_STREAM(p, aic_prof.hci_reversion);
    UINT16_TO_STREAM(p, aic_prof.lmp_subversion);
    AicLogMsg("btpatch_version, length is 0x%x, hci_reversion is 0x%x, lmp_subversion is %x", para_length, aic_prof.hci_reversion, aic_prof.lmp_subversion);

    if(coex_msg_send(p_buf, para_length + HCI_CMD_PREAMBLE_SIZE) < 0)
        ALOGE("aic_notify_btpatch_version_to_wifi: udpsocket send error");
}

void aic_notify_afhmap_to_wifi()
{
    uint8_t para_length = 13;
    char p_buf[para_length + HCI_CMD_PREAMBLE_SIZE];

    if(!aic_prof.wifi_on)
        return;

    char *p = p_buf;
    UINT16_TO_STREAM(p, HCI_OP_HCI_BT_AFH_MAP_NOTIFY);
    *p++ = para_length;
    *p++ = aic_prof.piconet_id;
    *p++ = aic_prof.mode;
    *p++ = 10;
    memcpy(p, aic_prof.afh_map, 10);

    AicLogMsg("afhmap, piconet_id is 0x%x, map type is 0x%x", aic_prof.piconet_id, aic_prof.mode);
    uint8_t kk = 0;
    for(kk=0; kk < 10; kk++)
        AicLogMsg("afhmap data[%d] is 0x%x", kk, aic_prof.afh_map[kk]);

    if(coex_msg_send(p_buf, para_length + HCI_CMD_PREAMBLE_SIZE) < 0)
        ALOGE("aic_notify_afhmap_to_wifi: udpsocket send error");
}

void aic_notify_btcoex_to_wifi(uint8_t opcode, uint8_t status)
{
    uint8_t para_length = 2;
    char p_buf[para_length + HCI_CMD_PREAMBLE_SIZE];

    if(!aic_prof.wifi_on)
        return;

    char *p = p_buf;
    UINT16_TO_STREAM(p, HCI_OP_HCI_BT_COEX_NOTIFY);
    *p++ = para_length;
    *p++ = opcode;
    if(!status)
        *p++ = 0;
    else
        *p++ = 1;

    AicLogMsg("btcoex, opcode is 0x%x, status is 0x%x", opcode, status);

    if(coex_msg_send(p_buf, para_length + HCI_CMD_PREAMBLE_SIZE) < 0)
        ALOGE("aic_notify_btcoex_to_wifi: udpsocket send error");
}

void aic_notify_btoperation_to_wifi(uint8_t operation, uint8_t append_data_length, uint8_t *append_data)
{
    uint8_t para_length = 3 + append_data_length;
    char p_buf[para_length + HCI_CMD_PREAMBLE_SIZE];

    if(!aic_prof.wifi_on)
        return;

    char *p = p_buf;
    UINT16_TO_STREAM(p, HCI_OP_BT_OPERATION_NOTIFY);
    *p++ = para_length;
    *p++ = operation;
    *p++ = append_data_length;
    if(append_data_length)
        memcpy(p, append_data, append_data_length);

    AicLogMsg("btoperation, opration is 0x%x, append_data_length is 0x%x", operation, append_data_length);
    uint8_t kk = 0;
    if(append_data_length)
    {
        for(kk=0; kk < append_data_length; kk++)
            AicLogMsg("append data is 0x%x", *(append_data+kk));
    }

    if(coex_msg_send(p_buf, para_length + HCI_CMD_PREAMBLE_SIZE) < 0)
        ALOGE("aic_notify_btoperation_to_wifi: udpsocket send error");
}

void aic_notify_info_to_wifi(uint8_t reason, uint8_t length, uint8_t* report_info)
{
    uint8_t para_length = 4 + length;
    char p_buf[para_length + HCI_CMD_PREAMBLE_SIZE];
    int i;

    if(!aic_prof.wifi_on)
        return;

    char *p = p_buf;
    UINT16_TO_STREAM(p, HCI_OP_HCI_BT_INFO_NOTIFY);
    *p++ = para_length;
    *p++ = aic_prof.polling_enable;
    *p++ = aic_prof.polling_interval;
    *p++ = reason;
    *p++ = length;

    if(length)
        memcpy(p, report_info, length);

    AicLogMsg("bt info, length is 0x%x, polling_enable is 0x%x, poiiling_interval is %x",para_length, aic_prof.polling_enable, aic_prof.polling_interval);
    AicLogMsg("bt info, reason is 0x%x, info length is 0x%x", reason, length);
    if(length)
    {
        for(i=0;i<length;i++)
            AicLogMsg("bt info[%d]: %02x", i, report_info[i]);
    }

    if(coex_msg_send(p_buf, para_length + HCI_CMD_PREAMBLE_SIZE) < 0)
        ALOGE("aic_notify_info_to_wifi: udpsocket send error");
}

void aic_notify_regester_to_wifi(uint8_t* reg_value)
{
    uint8_t para_length = 9;
    char p_buf[para_length + HCI_CMD_PREAMBLE_SIZE];

    if(!aic_prof.wifi_on)
        return;

    char *p = p_buf;
    UINT16_TO_STREAM(p, HCI_OP_HCI_BT_REGISTER_VALUE_NOTIFY);
    *p++ = para_length;
    memcpy(p, reg_value, para_length);

    tHCI_RETURN_PARAMETER_MAILBOX_REGISTER *reg = (tHCI_RETURN_PARAMETER_MAILBOX_REGISTER *)reg_value;
    AicLogMsg("bt register, register type is %x", reg->type);
    AicLogMsg("bt register, register offset is %x", reg->offset);
    AicLogMsg("bt register, register value is %x", reg->value);

    if(coex_msg_send(p_buf, para_length + HCI_CMD_PREAMBLE_SIZE) < 0)
        ALOGE("aic_notify_regester_to_wifi: udpsocket send error");
}

static void aic_handle_bt_info_control(uint8_t* p)
{
    tHCI_EVENT_BT_INFO_CONTROL*  info = (tHCI_EVENT_BT_INFO_CONTROL*)p;
    uint8_t temp_cmd[3];

    AicLogMsg("aic_prof.polling_enable is %x",aic_prof.polling_enable);
    AicLogMsg("receive bt info control event from wifi, polling enable is 0x%x, polling time is 0x%x, auto report is 0x%x",
                    info->polling_enable, info->polling_time, info->autoreport_enable);

    if(info->polling_enable && !aic_prof.polling_enable)
        start_polling_timer(info->polling_time * 1000);

    if(!info->polling_enable && aic_prof.polling_enable)
        stop_polling_timer();

    aic_prof.polling_enable = info->polling_enable;
    aic_prof.polling_interval = info->polling_time;
    aic_prof.autoreport = info->autoreport_enable;

    temp_cmd[0] = HCI_VENDOR_SUB_CMD_BT_AUTO_REPORT_ENABLE;
    temp_cmd[1] = 1;
    temp_cmd[2] = info->autoreport_enable;
    aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 3, temp_cmd, NULL);

    aic_notify_info_to_wifi(HOST_RESPONSE, 0, NULL);
}

static void aic_handle_bt_coex_control(uint8_t* p)
{
    uint8_t opcode = *p++;
	uint8_t op_len = 0;
    AicLogMsg("receive bt coex control event from wifi, opration is 0x%x", opcode);
    switch (opcode)
    {
        case BT_PATCH_VERSION_QUERY:
        {
            aic_notify_btpatch_version_to_wifi();
            break;
        }

        case IGNORE_WLAN_ACTIVE_CONTROL:
        {
            uint8_t opcode_len = *p++;
            uint8_t value = *p++;
            uint8_t temp_cmd[3];
			op_len = opcode_len;
            temp_cmd[0] = HCI_VENDOR_SUB_CMD_BT_ENABLE_IGNORE_WLAN_ACT_CMD;
            temp_cmd[1] = 1;
            temp_cmd[2] = value;
            aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 3, temp_cmd, NULL);
            break;
        }

        case LNA_CONSTRAIN_CONTROL:
        {
            uint8_t opcode_len = *p++;
            uint8_t value = *p++;
            uint8_t temp_cmd[3];
			op_len = opcode_len;
            temp_cmd[0] = HCI_VENDOR_SUB_CMD_SET_BT_LNA_CONSTRAINT;
            temp_cmd[1] = 1;
            temp_cmd[2] = value;
            aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 3, temp_cmd, NULL);
            break;
        }

        case BT_POWER_DECREASE_CONTROL:
        {
            uint8_t opcode_len = *p++;
            uint8_t power_decrease = *p++;
            uint8_t temp_cmd[3];
			op_len = opcode_len;
            temp_cmd[0] = HCI_VENDOR_SUB_CMD_WIFI_FORCE_TX_POWER_CMD;
            temp_cmd[1] = 1;
            temp_cmd[2] = power_decrease;
            aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 3, temp_cmd, NULL);
            break;
        }

        case BT_PSD_MODE_CONTROL:
        {
            uint8_t opcode_len = *p++;
            uint8_t psd_mode = *p++;
            uint8_t temp_cmd[3];
			op_len = opcode_len;
            temp_cmd[0] = HCI_VENDOR_SUB_CMD_SET_BT_PSD_MODE;
            temp_cmd[1] = 1;
            temp_cmd[2] = psd_mode;
            aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 3, temp_cmd, NULL);
            break;
        }

        case WIFI_BW_CHNL_NOTIFY:
        {
            uint8_t opcode_len = *p++;
            uint8_t temp_cmd[5];
			op_len = opcode_len;
            temp_cmd[0] = HCI_VENDOR_SUB_CMD_WIFI_CHANNEL_AND_BANDWIDTH_CMD;
            temp_cmd[1] = 3;
            memcpy(temp_cmd+2, p, 3);//wifi_state, wifi_centralchannel, chnnels_btnotuse
            aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 5, temp_cmd, NULL);
            break;
        }

        case QUERY_BT_AFH_MAP:
        {
            uint8_t opcode_len = *p++;
            aic_prof.piconet_id = *p++;
            aic_prof.mode = *p++;
            uint8_t temp_cmd[4];
			op_len = opcode_len;
            temp_cmd[0] = HCI_VENDOR_SUB_CMD_GET_AFH_MAP_L;
            temp_cmd[1] = 2;
            temp_cmd[2] = aic_prof.piconet_id;
            temp_cmd[3] = aic_prof.mode;
            aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 4, temp_cmd, NULL);
            break;
        }

        case BT_REGISTER_ACCESS:
        {
            uint8_t opcode_len = *p++;
            uint8_t access_type = *p++;
			op_len = opcode_len;
            if(access_type == 0) //read
            {
                uint8_t temp_cmd[7];
                temp_cmd[0] = HCI_VENDOR_SUB_CMD_RD_REG_REQ;
                temp_cmd[1] = 5;
                temp_cmd[2] = *p++;
                memcpy(temp_cmd+3, p, 4);
                aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 7, temp_cmd, NULL);
            }
            else //write
            {
                uint8_t temp_cmd[11];
                temp_cmd[0] = HCI_VENDOR_SUB_CMD_RD_REG_REQ;
                temp_cmd[1] = 5;
                temp_cmd[2] = *p++;
                memcpy(temp_cmd+3, p, 8);
                aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 11, temp_cmd, NULL);
            }
            break;
        }

        default:
            break;
    }
}

void aic_handle_event_from_wifi(uint8_t* msg)
{
    uint8_t *p = msg;
    uint8_t event_code = *p++;
    uint8_t total_length = 0;

    AicLogMsg("receive invite rsp from wifi msg : %s", msg);
    if(memcmp(msg, invite_rsp, sizeof(invite_rsp)) == 0)
    {
#if 0
        AicLogMsg("receive invite rsp from wifi, close netlink socket if needed");
        if(aic_prof.nlsocket > 0)
        {
            close(aic_prof.nlsocket);
            AicLogMsg("close netlink socket %d", aic_prof.nlsocket);
        }
#endif
        AicLogMsg("receive invite rsp from wifi, wifi is already on");
        aic_prof.wifi_on = 1;
        aic_notify_extension_version_to_wifi();
    }

    if(memcmp(msg, attend_req, sizeof(attend_req)) == 0)
    {
        AicLogMsg("receive attend req from wifi, wifi turn on");
        aic_prof.wifi_on = 1;
        coex_msg_send(attend_ack, sizeof(attend_ack));
        aic_notify_extension_version_to_wifi();
    }

    if(memcmp(msg, wifi_leave, sizeof(wifi_leave)) == 0)
    {
        AicLogMsg("receive wifi leave from wifi, wifi turn off");
        aic_prof.wifi_on = 0;
        coex_msg_send(leave_ack, sizeof(leave_ack));
        if(aic_prof.polling_enable)
        {
            aic_prof.polling_enable = 0;
            stop_polling_timer();
        }
    }

    if(memcmp(msg, leave_ack, sizeof(leave_ack)) == 0)
    {
        AicLogMsg("receive leave ack from wifi");
    }

    if(event_code == 0xFE)
    {
        total_length = *p++;
        uint8_t extension_event = *p++;
        switch(extension_event)
        {
            case  AIC_HS_EXTENSION_EVENT_WIFI_SCAN:
            {
                uint8_t operation = *p;
                AicLogMsg("receive wifi scan notify evnet from wifi, operation is 0x%x", operation);
                break;
            }

            case  AIC_HS_EXTENSION_EVENT_HCI_BT_INFO_CONTROL:
            {
                aic_handle_bt_info_control(p);
                break;
            }

            case AIC_HS_EXTENSION_EVENT_HCI_BT_COEX_CONTROL:
            {
                aic_handle_bt_coex_control(p);
                break;
            }

            default:
                break;
        }
    }

    if(event_code == 0x0E)
    {
        uint16_t wifi_opcode;
        uint8_t op_status;
        p += 2;//length, number of complete packets
        STREAM_TO_UINT16(wifi_opcode, p);
        op_status = *p;
        AicLogMsg("receive command complete event from wifi, op code is 0x%x, status is 0x%x", wifi_opcode, op_status);
    }
}

static void coex_receive_thread_exit_handler(int sig)
{
    AicLogMsg("USR2, this signal is %d \n", sig);
    usleep(100);
    pthread_exit(0);
}

static void btwifi_coex_receive_thread(void *arg)
{
    AIC_UNUSED(arg);
    uint8_t msg_recv[MAX_PAYLOAD];
    uint8_t recv_length;
    struct sigaction actions;

    memset(&actions, 0, sizeof(actions));
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0;
    actions.sa_handler = coex_receive_thread_exit_handler;

    sigaction(SIGUSR2,&actions,NULL);//int rc = sigaction(SIGUSR2,&actions,NULL);

    AicLogMsg("btwifi_coex_receive_thread started");
    prctl(PR_SET_NAME, (unsigned long)"btwifi_coex_receive_thread", 0, 0, 0);

    while(aic_prof.coex_recv_thread_running)
    {
        memset(msg_recv, 0 , MAX_PAYLOAD);
        if (coex_msg_recv(msg_recv, &recv_length) == 0)
            aic_handle_event_from_wifi(msg_recv);
    }

    AicLogMsg("btwifi_coex_receive_thread exiting");
    pthread_exit(NULL);
}

int create_udpsocket_socket()
{
    int portno = CONNECT_PORT;
    int optval; /* flag value for setsockopt */

    AicLogMsg("create udpsocket port: %d\n", portno);

    pthread_mutex_lock(&aic_prof.btwifi_mutex);

    pthread_attr_t thread_attr_data;
    if (aic_prof.coex_recv_thread_running)
    {
        ALOGE("udp_receive_thread already exit");
        pthread_mutex_unlock(&aic_prof.btwifi_mutex);
        return -1 ;
    }

    aic_prof.coex_recv_thread_running = 1;
    aic_prof.udpsocket = socket(AF_INET, SOCK_DGRAM, 0);
    AicLogMsg("create socket %d", aic_prof.udpsocket);

    if (aic_prof.udpsocket < 0)
    {
        ALOGE("create udpsocket error...%s\n", strerror(errno));
        aic_prof.coex_recv_thread_running = 0;
        AicLogMsg("close socket %d", aic_prof.udpsocket);
        pthread_mutex_unlock(&aic_prof.btwifi_mutex);
        return -1 ;
    }

    bzero((char *) &aic_prof.server_addr, sizeof(aic_prof.server_addr));
    aic_prof.server_addr.sin_family = AF_INET;
    aic_prof.server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    aic_prof.server_addr.sin_port = htons(CONNECT_PORT);

    bzero((char *) &aic_prof.client_addr, sizeof(aic_prof.client_addr));
    aic_prof.client_addr.sin_family = AF_INET;
    aic_prof.client_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    aic_prof.client_addr.sin_port = htons(CONNECT_PORT_WIFI);

    optval = 1;
    int ret = setsockopt(aic_prof.udpsocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
    if(ret == -1){
        ALOGE("%s, setsockopt error: %s", __func__, strerror(errno));
    }

    if (bind(aic_prof.udpsocket, (struct sockaddr *)&aic_prof.server_addr, sizeof(aic_prof.server_addr)) < 0)
    {
        ALOGE("bind udpsocket error...%s\n", strerror(errno));
        aic_prof.coex_recv_thread_running = 0;
        close(aic_prof.udpsocket);
        AicLogMsg("close socket %d", aic_prof.udpsocket);
        pthread_mutex_unlock(&aic_prof.btwifi_mutex);
        return -1 ;
    }

    pthread_attr_init(&thread_attr_data);
    if (pthread_create(&aic_prof.thread_data, &thread_attr_data, (void*)btwifi_coex_receive_thread, NULL) != 0)
    {
        ALOGE("pthread_create failed!");
        pthread_attr_destroy(&thread_attr_data);
        aic_prof.coex_recv_thread_running = 0;
        pthread_mutex_unlock(&aic_prof.btwifi_mutex);
        return -1 ;
    }
    pthread_attr_destroy(&thread_attr_data);
    pthread_mutex_unlock(&aic_prof.btwifi_mutex);
    return 0;
}

int stop_btwifi_coex_receive_thread()
{
    pthread_mutex_lock(&aic_prof.btwifi_mutex);
    int result = 0;

    AicLogMsg("notify wifi bt turn off");
    if(aic_prof.wifi_on)
        coex_msg_send(bt_leave, sizeof(bt_leave));

    if (aic_prof.coex_recv_thread_running)
    {
        AicLogMsg("data thread is running, stop it");

        //add for pthread_cancel
        if ((result = pthread_kill(aic_prof.thread_data, SIGUSR2)) != 0)
        {
            ALOGE("error cancelling data thread");
        }
        aic_prof.coex_recv_thread_running = 0;

        if ((result = pthread_join(aic_prof.thread_data, NULL)) < 0)
        {
            ALOGE( "data thread pthread_join() failed result:%d", result);
        }

        if(aic_prof.udpsocket > 0) {
            AicLogMsg("close socket %d", aic_prof.udpsocket);
            if((result = close(aic_prof.udpsocket)) != 0)
            {
                ALOGE("close socket error!");
            }
        }
        else if(aic_prof.btcoex_chr > 0) {
            AicLogMsg("close char device  %d", aic_prof.btcoex_chr);
            if((result = close(aic_prof.btcoex_chr)) != 0)
            {
                ALOGE("close char device  error!");
            }
        }
    }
    pthread_mutex_unlock(&aic_prof.btwifi_mutex);
    return 0;
}

#if 0
int create_netlink_socket()
{
    AicLogMsg("in creat netlink socket");
    aic_prof.nlsocket = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);

    if (aic_prof.nlsocket < 0)
    {
        ALOGE("create netlink socket error...%s\n", strerror(errno));
        close(aic_prof.nlsocket);
        AicLogMsg("close netlink socket %d", aic_prof.nlsocket);
        return -1 ;
    }
    AicLogMsg("create netlink socket %d", aic_prof.nlsocket);
    memset(&aic_prof.src_addr, 0, sizeof(aic_prof.src_addr));
    aic_prof.src_addr.nl_family = AF_NETLINK;
    aic_prof.src_addr.nl_pid = getpid(); /* self pid */
    aic_prof.src_addr.nl_groups    = 0 ;    /* not in mcast groups */
    int ret = bind(aic_prof.nlsocket, (struct sockaddr *)&aic_prof.src_addr, sizeof(aic_prof.src_addr));
    if(ret < 0)
    {
        ALOGE("bind netlink socket error...%s\n", strerror(errno));
        close(aic_prof.nlsocket);
        AicLogMsg("close netlink socket %d", aic_prof.nlsocket);
        return -1 ;
    }

    return 0;
}
#endif

int open_btcoex_chrdev()
{
    AicLogMsg("open_btcoex_chrdev\n");

    pthread_mutex_lock(&aic_prof.btwifi_mutex);

    pthread_attr_t thread_attr_data;
    if (aic_prof.coex_recv_thread_running)
    {
        ALOGE("udp_receive_thread already exit");
        pthread_mutex_unlock(&aic_prof.btwifi_mutex);
        return -1 ;
    }

    aic_prof.coex_recv_thread_running = 1;
    if ((aic_prof.btcoex_chr = open("/dev/aic_btcoex", O_RDWR)) < 0)
    {
        ALOGE("open aic_btcoex error...%s\n", strerror(errno));
        aic_prof.coex_recv_thread_running = 0;
        pthread_mutex_unlock(&aic_prof.btwifi_mutex);
        return -1 ;
    }

    pthread_attr_init(&thread_attr_data);
    if (pthread_create(&aic_prof.thread_data, &thread_attr_data, (void*)btwifi_coex_receive_thread, NULL) != 0)
    {
        ALOGE("create coexchr_receive_thread failed!");
        pthread_attr_destroy(&thread_attr_data);
        aic_prof.coex_recv_thread_running = 0;
        pthread_mutex_unlock(&aic_prof.btwifi_mutex);
        return -1 ;
    }
    pthread_attr_destroy(&thread_attr_data);
    pthread_mutex_unlock(&aic_prof.btwifi_mutex);
    return 0;
}

void aic_parse_init(void)
{
    ALOGI("AICBT_RELEASE_NAME: %s",AICBT_RELEASE_NAME);
    AicLogMsg("aic_profile_init, version: %s", AIC_COEX_VERSION);

    memset(&aic_prof, 0, sizeof(aic_prof));
    pthread_mutex_init(&aic_prof.profile_mutex, NULL);
    pthread_mutex_init(&aic_prof.coex_mutex, NULL);
    pthread_mutex_init(&aic_prof.btwifi_mutex, NULL);
    alloc_a2dp_packet_count_timer();
    alloc_pan_packet_count_timer();
    alloc_hogp_packet_count_timer();
    alloc_polling_timer();

    init_profile_hash(&aic_prof);
    init_connection_hash(&aic_prof);
    init_coex_hash(&aic_prof);

    if(create_udpsocket_socket() < 0) {
        ALOGE("UDP socket fail, try to use aic_btcoex chrdev");
        open_btcoex_chrdev();
    }
#ifdef AIC_ROLE_SWITCH_RETRY
    memset(role_monitor_pool,0,sizeof(role_monitor_pool));
#endif
}

void aic_parse_cleanup()
{
    AicLogMsg("aic_profile_cleanup");
    free_a2dp_packet_count_timer();
    free_pan_packet_count_timer();
    free_hogp_packet_count_timer();
    free_polling_timer();

    flush_connection_hash(&aic_prof);
    flush_profile_hash(&aic_prof);
    pthread_mutex_destroy(&aic_prof.profile_mutex);
    flush_coex_hash(&aic_prof);
    pthread_mutex_destroy(&aic_prof.coex_mutex);

    stop_btwifi_coex_receive_thread();
    pthread_mutex_destroy(&aic_prof.btwifi_mutex);

    memset(&aic_prof, 0, sizeof(aic_prof));
}

static void aic_handle_vender_mailbox_cmp_evt(uint8_t* p, uint8_t len)
{
    uint8_t status = *p++;
    if(len <= 4)
    {
        AicLogMsg("receive mailbox cmd from fw, total length <= 4");
        return;
    }
    uint8_t subcmd = *p++;
    AicLogMsg("receive mailbox cmd from fw, subcmd is 0x%x, status is 0x%x", subcmd, status);
    switch(subcmd)
    {
        case HCI_VENDOR_SUB_CMD_BT_REPORT_CONN_SCO_INQ_INFO:
            if(status == 0) //success
            {
                if((len-5) != 8)
                    AicLogMsg("aic_handle_vender_mailbox_cmp_evt:HCI_VENDOR_SUB_CMD_BT_REPORT_CONN_SCO_INQ_INFO len=%d", len);
                aic_notify_info_to_wifi(POLLING_RESPONSE, (len-5), (uint8_t*)p);
            }
            break;

        case HCI_VENDOR_SUB_CMD_WIFI_CHANNEL_AND_BANDWIDTH_CMD:
            aic_notify_btcoex_to_wifi(WIFI_BW_CHNL_NOTIFY, status);
            break;

        case HCI_VENDOR_SUB_CMD_WIFI_FORCE_TX_POWER_CMD:
            aic_notify_btcoex_to_wifi(BT_POWER_DECREASE_CONTROL, status);
            break;

        case HCI_VENDOR_SUB_CMD_BT_ENABLE_IGNORE_WLAN_ACT_CMD:
            aic_notify_btcoex_to_wifi(IGNORE_WLAN_ACTIVE_CONTROL, status);
            break;

        case HCI_VENDOR_SUB_CMD_SET_BT_PSD_MODE:
            aic_notify_btcoex_to_wifi(BT_PSD_MODE_CONTROL, status);
            break;

        case HCI_VENDOR_SUB_CMD_SET_BT_LNA_CONSTRAINT:
            aic_notify_btcoex_to_wifi(LNA_CONSTRAIN_CONTROL, status);
            break;

        case HCI_VENDOR_SUB_CMD_BT_AUTO_REPORT_ENABLE:
            break;

        case HCI_VENDOR_SUB_CMD_BT_SET_TXRETRY_REPORT_PARAM:
            break;

        case HCI_VENDOR_SUB_CMD_BT_SET_PTATABLE:
            break;

        case HCI_VENDOR_SUB_CMD_GET_AFH_MAP_L:
        {
            if(status == 0)//success
            {
                memcpy(aic_prof.afh_map, p+4, 4); //cmd_idx, length, piconet_id, mode
                uint8_t temp_cmd[4];
                temp_cmd[0] = HCI_VENDOR_SUB_CMD_GET_AFH_MAP_M;
                temp_cmd[1] = 2;
                temp_cmd[2] = aic_prof.piconet_id;
                temp_cmd[3] = aic_prof.mode;
                aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 4, temp_cmd, NULL);
            }
            else //fail
            {
                memset(aic_prof.afh_map, 0, 10);
                aic_notify_afhmap_to_wifi();
            }
            break;
        }
        case HCI_VENDOR_SUB_CMD_GET_AFH_MAP_M:
        {
            if(status == 0)//success
            {
                memcpy(aic_prof.afh_map+4, p+4, 4);
                uint8_t temp_cmd[4];
                temp_cmd[0] = HCI_VENDOR_SUB_CMD_GET_AFH_MAP_H;
                temp_cmd[1] = 2;
                temp_cmd[2] = aic_prof.piconet_id;
                temp_cmd[3] = aic_prof.mode;
                aic_vendor_cmd_to_fw(HCI_VENDOR_MAILBOX_CMD, 4, temp_cmd, NULL);
            }
            else //fail
            {
                memset(aic_prof.afh_map, 0, 10);
                aic_notify_afhmap_to_wifi();
            }
            break;
        }

        case HCI_VENDOR_SUB_CMD_GET_AFH_MAP_H:
        {
            if(status == 0)
                memcpy(aic_prof.afh_map+8, p+4, 2);
            else
                memset(aic_prof.afh_map, 0, 10);

            aic_notify_afhmap_to_wifi();
            break;
        }

        case HCI_VENDOR_SUB_CMD_RD_REG_REQ:
        {
            if(status == 0)
                aic_notify_regester_to_wifi(p+3);//cmd_idx,length,regist type
            break;
        }

        case HCI_VENDOR_SUB_CMD_WR_REG_REQ:
            aic_notify_btcoex_to_wifi(BT_REGISTER_ACCESS, status);
            break;

        default:
            break;
    }
}

static void aic_handle_cmd_complete_evt(uint8_t*p, uint8_t len)
{
    uint16_t opcode;
    uint8_t status;
    p++;
    STREAM_TO_UINT16(opcode, p);
    switch (opcode)
    {
        case HCI_PERIODIC_INQUIRY_MODE:
        {
            status = *p++;
            if(status && aic_prof.isinquirying)
            {
                aic_prof.isinquirying = 0;
                AicLogMsg("HCI_PERIODIC_INQUIRY_MODE start error, notify wifi inquiry stop");
                aic_notify_btoperation_to_wifi(BT_OPCODE_INQUIRY_END, 0, NULL);
            }
            break;
        }

        case HCI_READ_LOCAL_VERSION_INFO:
        {
            status = *p++;
            if(!status)
            {
                p++;
                STREAM_TO_UINT16(aic_prof.hci_reversion, p);
                p+=3;
                STREAM_TO_UINT16(aic_prof.lmp_subversion, p);
            }
            AicLogMsg("aic_prof.hci_reversion = %x", aic_prof.hci_reversion);
            AicLogMsg("aic_prof.lmp_subversion = %x", aic_prof.lmp_subversion);
            break;
        }

        case HCI_RESET:
        {
            AicLogMsg("bt start ok");
            coex_msg_send(invite_req, sizeof(invite_req));
#if 0
            if(create_netlink_socket() == 0)
            {
                AicLogMsg("wifi is already on when bt turn on");
                aic_prof.wifi_on = 1;
                netlink_send(aic_prof.nlsocket, invite_req);
            }
            else
                AicLogMsg("wifi is off when bt turn on, wait for wifi turning on...");
#endif
            break;
        }

        case 0xfc1b:
            AicLogMsg("received cmd complete event for fc1b");
            poweroff_allowed = 1;
            break;

        case HCI_VENDOR_MAILBOX_CMD:
            aic_handle_vender_mailbox_cmp_evt(p, len);
            break;

        case HCI_VENDOR_ADD_BITPOOL_FW:
            status = *p++;
            AicLogMsg("received cmd complete event for HCI_VENDOR_ADD_BITPOOL_FW status:%d",status);

        default:
            break;
    }
}

static void aic_handle_connection_complete_evt(uint8_t* p)
{
    uint8_t status = 0;
    uint16_t handle = 0;
    status = *p++;
    STREAM_TO_UINT16 (handle, p);
    p +=6;
    uint8_t link_type = *p++;

    if(status == 0)
    {
        if(aic_prof.ispaging)
        {
            aic_prof.ispaging = 0;
            AicLogMsg("notify wifi page success end");
            aic_notify_btoperation_to_wifi(BT_OPCODE_PAGE_SUCCESS_END, 0, NULL);
        }

        tAIC_CONN_PROF* hci_conn = find_connection_by_handle(&aic_prof, handle);
        if(hci_conn == NULL)
        {
            hci_conn = allocate_connection_by_handle(handle);
            if(hci_conn)
            {
                add_connection_to_hash(&aic_prof, hci_conn);
                hci_conn->profile_bitmap = 0;
                memset(hci_conn->profile_refcount, 0, 8);
                if((0 == link_type) ||(2 == link_type))//sco or esco
                {
                    hci_conn->type = 1;
                    update_profile_connection(hci_conn, profile_sco, TRUE);
                }
                else
                    hci_conn->type = 0;
            }
            else
            {
                ALOGE("HciConnAllocate fail");
            }
        }
        else
        {
            AicLogMsg("HCI Connection handle(0x%x) has already exist!", handle);
            hci_conn->profile_bitmap = 0;
            memset(hci_conn->profile_refcount, 0, 8);
            if((0 == link_type)||(2 == link_type))//sco or esco
            {
                hci_conn->type = 1;
                update_profile_connection(hci_conn, profile_sco, TRUE);
            }
            else
                hci_conn->type = 0;
        }
    }
    else if(aic_prof.ispaging)
    {
        aic_prof.ispaging = 0;
        AicLogMsg("notify wifi page unsuccess end");
        aic_notify_btoperation_to_wifi(BT_OPCODE_PAGE_UNSUCCESS_END, 0, NULL);
    }
}

static void aic_handle_disconnect_complete_evt(uint8_t* p)
{
    if(aic_prof.ispairing)//for slave: connection will be disconnected if authentication fail
    {
        aic_prof.ispairing = 0;
        AicLogMsg("notify wifi pair end");
        aic_notify_btoperation_to_wifi(BT_OPCODE_PAIR_END, 0, NULL);
    }

    uint8_t status = 0;
    uint16_t handle = 0;
    uint8_t reason = 0;
    status = *p++;
    STREAM_TO_UINT16(handle, p);
    reason = *p;

    if(status == 0)
    {
        tAIC_CONN_PROF *hci_conn = find_connection_by_handle(&aic_prof, handle);
        if(hci_conn)
        {
            switch(hci_conn->type)
            {
                case 0:
                {
                    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
                    tAIC_PROF_INFO* prof_info = NULL;

                    pthread_mutex_lock(&aic_prof.profile_mutex);
                    LIST_FOR_EACH_SAFELY(iter, temp, &aic_prof.profile_list)
                    {
                        prof_info = LIST_ENTRY(iter, tAIC_PROF_INFO, list);
                        if ((handle == prof_info->handle) && prof_info->scid && prof_info->dcid)
                        {
                            AicLogMsg("find info when hci disconnect, handle:%x, psm:%x, dcid:%x, scid:%x", prof_info->handle, prof_info->psm, prof_info->dcid, prof_info->scid);
                            //If both scid and dcid > 0, L2cap connection is exist.
                            update_profile_connection(hci_conn, prof_info->profile_index, FALSE);
                            delete_profile_from_hash(prof_info);
                        }
                    }
                    pthread_mutex_unlock(&aic_prof.profile_mutex);
                    break;
                }

                case 1:
                    update_profile_connection(hci_conn, profile_sco, FALSE);
                    break;

                case 2:
                {
                    if(hci_conn->profile_bitmap & BIT(profile_hogp))
                        update_profile_connection(hci_conn, profile_hogp, FALSE);

                    if(hci_conn->profile_bitmap & BIT(profile_voice))
                        update_profile_connection(hci_conn, profile_voice, FALSE);

                    update_profile_connection(hci_conn, profile_hid, FALSE);
                    break;
                }

                default:
                    break;
            }
            delete_connection_from_hash(hci_conn);
        }
        else
        {
            ALOGE("HCI Connection handle(0x%x) not found", handle);
        }
    }
}

static void aic_handle_le_connection_complete_evt(uint8_t* p, bool enhanced)
{
    uint16_t handle, interval;
    uint8_t status;
    tAIC_CONN_PROF* hci_conn = NULL;

    status = *p++;
    STREAM_TO_UINT16 (handle, p);
    p += 8; //role, address type, address
    if(enhanced) {
        p += 12;
    }
    STREAM_TO_UINT16 (interval, p);

    if(status == 0) {
        if(aic_prof.ispaging){
            aic_prof.ispaging = 0;
            AicLogMsg("notify wifi page success end");
            aic_notify_btoperation_to_wifi(BT_OPCODE_PAGE_SUCCESS_END, 0, NULL);
        }

        hci_conn = find_connection_by_handle(&aic_prof, handle);
        if(hci_conn == NULL) {
            hci_conn = allocate_connection_by_handle(handle);
            if(hci_conn) {
                add_connection_to_hash(&aic_prof, hci_conn);
                hci_conn->profile_bitmap = 0;
                memset(hci_conn->profile_refcount, 0, 8);
                hci_conn->type = 2;
                update_profile_connection(hci_conn, profile_hid, TRUE); //for coex, le is the same as hid
                update_hid_active_state(handle, interval);
            } else {
                ALOGE("hci connection allocate fail");
            }
        } else {
            AicLogMsg("hci connection handle(0x%x) has already exist!", handle);
            hci_conn->profile_bitmap = 0;
            memset(hci_conn->profile_refcount, 0, 8);
            hci_conn->type = 2;
            update_profile_connection(hci_conn, profile_hid, TRUE);
            update_hid_active_state(handle, interval);
        }
    } else if(aic_prof.ispaging) {
        aic_prof.ispaging = 0;
        AicLogMsg("notify wifi page unsuccess end");
        aic_notify_btoperation_to_wifi(BT_OPCODE_PAGE_UNSUCCESS_END, 0, NULL);
    }
}

static void aic_handle_le_connection_update_complete_evt(uint8_t* p)
{
    uint16_t handle, interval;
    uint8_t status;

    status = *p++;
    STREAM_TO_UINT16 (handle, p);
    STREAM_TO_UINT16 (interval, p);
    update_hid_active_state(handle, interval);
}

static void aic_handle_le_meta_evt(uint8_t* p)
{
    uint8_t sub_event = *p++;
    switch (sub_event) {
    case HCI_BLE_CONN_COMPLETE_EVT:
        aic_handle_le_connection_complete_evt(p, false);
        break;
    case HCI_BLE_ENHANCED_CONN_COMPLETE_EVT:
        aic_handle_le_connection_complete_evt(p, true);
        break;
    case HCI_BLE_LL_CONN_PARAM_UPD_EVT:
        aic_handle_le_connection_update_complete_evt(p);
        break;

    default :
        break;
    }
}

static int coex_msg_send(char *tx_msg, int msg_size)
{
    int ret = -1;
    if(aic_prof.udpsocket > 0) {
        ret = udpsocket_send(tx_msg, msg_size);
    }
    else if(aic_prof.btcoex_chr > 0) {
        ret = btcoex_chr_send(tx_msg, msg_size);
    }
    return ret;

}

static int coex_msg_recv(uint8_t *recv_msg, uint8_t *msg_size)
{
    int ret = -1;
    if(aic_prof.udpsocket > 0) {
        ret = udpsocket_recv(recv_msg, msg_size);
    }
    else if(aic_prof.btcoex_chr > 0) {
        ret = btcoex_chr_recv(recv_msg, msg_size);
    }
    return ret;
}
void aic_parse_internal_event_intercept(uint8_t *p_msg)
{
    //ALOGE("in aic_parse_internal_event_intercept, *p= %x", *p);
    uint8_t *p = p_msg;
    uint8_t event_code = *p++;
    uint8_t len = *p++;
    uint16_t opcode, mode_change_handle, mode_interval, subcode;
    uint8_t status, num_hci_cmd_pkts;

    switch (event_code)
    {
        case HCI_INQUIRY_COMP_EVT:
        {
            if(aic_prof.isinquirying)
            {
                aic_prof.isinquirying = 0;
                AicLogMsg("notify wifi inquiry end");
                aic_notify_btoperation_to_wifi(BT_OPCODE_INQUIRY_END, 0, NULL);
            }
            break;
        }

        case HCI_PIN_CODE_REQUEST_EVT:
        {
            if(!aic_prof.ispairing)
            {
                aic_prof.ispairing = 1;
                AicLogMsg("notify wifi pair start");
                aic_notify_btoperation_to_wifi(BT_OPCODE_PAIR_START, 0, NULL);
            }
            break;
        }

        case HCI_IO_CAPABILITY_REQUEST_EVT:
        {
            if(!aic_prof.ispairing)
            {
                aic_prof.ispairing = 1;
                AicLogMsg("notify wifi pair start");
                aic_notify_btoperation_to_wifi(BT_OPCODE_PAIR_START, 0, NULL);
            }
            break;
        }

        case HCI_AUTHENTICATION_COMP_EVT:
        {
            if(aic_prof.ispairing)
            {
                aic_prof.ispairing = 0;
                AicLogMsg("notify wifi pair end");
                aic_notify_btoperation_to_wifi(BT_OPCODE_PAIR_END, 0, NULL);
            }
            break;
        }

        case HCI_LINK_KEY_NOTIFICATION_EVT:
        {
            if(aic_prof.ispairing)
            {
                aic_prof.ispairing = 0;
                AicLogMsg("notify wifi pair end");
                aic_notify_btoperation_to_wifi(BT_OPCODE_PAIR_END, 0, NULL);
            }
            break;
        }

        case HCI_MODE_CHANGE_EVT:
        {
            status = *p++;
            STREAM_TO_UINT16(mode_change_handle, p);
            p++;
            STREAM_TO_UINT16(mode_interval, p);
            update_hid_active_state(mode_change_handle, mode_interval);
            break;
        }

        case HCI_COMMAND_COMPLETE_EVT:
            aic_handle_cmd_complete_evt(p, len);
            break;

        case HCI_COMMAND_STATUS_EVT:
        {
            status = *p++;
            num_hci_cmd_pkts = *p++;
            STREAM_TO_UINT16(opcode, p);
            if((opcode == HCI_INQUIRY) && (status))
            {
                if(aic_prof.isinquirying)
                {
                    aic_prof.isinquirying = 0;
                    AicLogMsg("inquiry start error, notify wifi inquiry stop");
                    aic_notify_btoperation_to_wifi(BT_OPCODE_INQUIRY_END, 0, NULL);
                }
            }

            if(opcode == HCI_CREATE_CONNECTION)
            {
                if(!status && !aic_prof.ispaging)
                {
                    aic_prof.ispaging = 1;
                    AicLogMsg("notify wifi start page");
                    aic_notify_btoperation_to_wifi(BT_OPCODE_PAGE_START, 0, NULL);
                }
            }
            break;
        }

        case HCI_CONNECTION_COMP_EVT:
        case HCI_ESCO_CONNECTION_COMP_EVT:
            aic_handle_connection_complete_evt(p);
#ifdef AIC_ROLE_SWITCH_RETRY
            /*update role switch pool ,record this info*/
            aic_record_connection_info(p);
#endif
            break;

        case HCI_DISCONNECTION_COMP_EVT:
            aic_handle_disconnect_complete_evt(p);
#ifdef AIC_ROLE_SWITCH_RETRY
            aic_connection_info_clear(p);
#endif
            break;

#ifdef AIC_ROLE_SWITCH_RETRY
        case HCI_ROLE_CHANGE_EVT:
            aic_handle_role_change_evt(p);
            break;
#endif

        case HCI_VENDOR_SPECIFIC_EVT:
        {
            STREAM_TO_UINT16(subcode, p);
            if(subcode == HCI_VENDOR_PTA_AUTO_REPORT_EVENT)
            {
                AicLogMsg("notify wifi driver with autoreport data");
                if((len-2) != 8)
                    AicLogMsg("aic_parse_internal_event_intercept:HCI_VENDOR_SPECIFIC_EVT:HCI_VENDOR_PTA_AUTO_REPORT_EVENT len=%d", len);
                aic_notify_info_to_wifi(AUTO_REPORT, (len-2), (uint8_t *)p);
            }
            break;
        }

        case HCI_BLE_EVENT:
            aic_handle_le_meta_evt(p);
            break;

        default:
            break;
    }
}

void aic_parse_command(uint8_t *pp)
{
    uint8_t *p = pp;
    uint16_t cmd;
    STREAM_TO_UINT16(cmd, p);

    switch (cmd)
    {
        case HCI_INQUIRY:
        case HCI_PERIODIC_INQUIRY_MODE:
        {
            if(!aic_prof.isinquirying)
            {
                aic_prof.isinquirying = 1;
                AicLogMsg("notify wifi inquiry start");
                aic_notify_btoperation_to_wifi(BT_OPCODE_INQUIRY_START, 0, NULL);
            }
            break;
        }

        case HCI_INQUIRY_CANCEL:
        case HCI_EXIT_PERIODIC_INQUIRY_MODE:
        {
            if(aic_prof.isinquirying)
            {
                aic_prof.isinquirying = 0;
                AicLogMsg("notify wifi inquiry stop");
                aic_notify_btoperation_to_wifi(BT_OPCODE_INQUIRY_END, 0, NULL);
            }
            break;
        }

        case HCI_ACCEPT_CONNECTION_REQUEST:
        {
            if(!aic_prof.ispaging)
            {
                aic_prof.ispaging = 1;
                AicLogMsg("notify wifi page start");
                aic_notify_btoperation_to_wifi(BT_OPCODE_PAGE_START, 0, NULL);
            }
            break;
        }

        default:
            break;
    }
}

void aic_parse_l2cap_data(uint8_t *pp, uint8_t direction)
{
    uint16_t handle, total_len, pdu_len, channel_ID, command_len, psm, scid, dcid, result, status;
    uint8_t flag, code, identifier;
    STREAM_TO_UINT16 (handle, pp);
    flag = (handle >> HCI_DATA_EVENT_OFFSET) & HCI_DATA_EVENT_MASK;
    handle = handle & 0x0FFF;
    STREAM_TO_UINT16 (total_len, pp);
    STREAM_TO_UINT16 (pdu_len, pp);
    STREAM_TO_UINT16 (channel_ID, pp);

    if(flag != AIC_START_PACKET_BOUNDARY)
      return;

    if(channel_ID == 0x0001)
    {
        code = (uint8_t)(*pp++);
        switch (code)
        {
            case L2CAP_CONNECTION_REQ:
                identifier = (uint8_t)(*pp++);
                STREAM_TO_UINT16 (command_len, pp);
                STREAM_TO_UINT16 (psm, pp);
                STREAM_TO_UINT16 (scid, pp);
                AicLogMsg("L2CAP_CONNECTION_REQ, handle=%x, PSM=%x, scid=%x", handle, psm, scid);
                handle_l2cap_con_req(handle, psm, scid, direction);
                break;

            case L2CAP_CONNECTION_RSP:
                identifier = (uint8_t)(*pp++);
                STREAM_TO_UINT16 (command_len, pp);
                STREAM_TO_UINT16 (dcid, pp);
                STREAM_TO_UINT16 (scid, pp);
                STREAM_TO_UINT16 (result, pp);
                STREAM_TO_UINT16 (status, pp);
                AicLogMsg("L2CAP_CONNECTION_RESP, handle=%x, dcid=%x, scid=%x, result=%x", handle, dcid, scid, result);
                //if(result == 0)
                    handle_l2cap_con_rsp(handle, dcid, scid, direction, result);
                break;

            case L2CAP_DISCONNECTION_REQ:
                identifier = (uint8_t)(*pp++);
                STREAM_TO_UINT16 (command_len, pp);
                STREAM_TO_UINT16 (dcid, pp);
                STREAM_TO_UINT16 (scid, pp);
                AicLogMsg("L2CAP_DISCONNECTION_REQ, handle=%x, dcid=%x, scid=%x",handle, dcid, scid);
                handle_l2cap_discon_req(handle, dcid, scid, direction);
                break;

            case L2CAP_DISCONNECTION_RSP:
                break;

            default:
                break;
        }
    }
    else
    {
        if((flag != 0x01)&&(is_profile_connected(profile_a2dp) || is_profile_connected(profile_pan)))//Do not count the continuous packets
            packets_count(handle, channel_ID, pdu_len, direction, pp);
    }
}

void aic_add_le_profile(BD_ADDR bdaddr, uint16_t handle, uint8_t profile_map)
{
    AIC_UNUSED(bdaddr);
    AicLogMsg("aic_add_le_profile, handle is %x, profile_map is %x", handle, profile_map);

    tAIC_CONN_PROF* hci_conn = find_connection_by_handle(&aic_prof, handle);
    if(hci_conn)
    {
        if((profile_map & 0x01) || (profile_map & 0x02))//bit0: mouse, bit1:keyboard
            update_profile_connection(hci_conn, profile_hogp, TRUE);

        if(profile_map & 0x04)
            update_profile_connection(hci_conn, profile_voice, TRUE);
    }
    else
    {
        ALOGE("aic_add_le_profile, connection handle(0x%x) not exist!", handle);
    }
}

void aic_delete_le_profile(BD_ADDR bdaddr, uint16_t handle, uint8_t profile_map)
{
    AIC_UNUSED(bdaddr);
    AicLogMsg("aic_delete_le_profile, handle is %x, profile_map is %x", handle, profile_map);

    pthread_mutex_lock(&aic_prof.profile_mutex);
    tAIC_CONN_PROF* hci_conn = find_connection_by_handle(&aic_prof, handle);
    if(hci_conn == NULL)
    {
        ALOGE("aic_delete_le_profile, hci_conn not exist with handle %x", handle);
    }
    else
    {
        if((profile_map & 0x01) || (profile_map & 0x02))//bit0: mouse, bit1:keyboard
            update_profile_connection(hci_conn, profile_hogp, FALSE);

        if(profile_map & 0x04)
            update_profile_connection(hci_conn, profile_voice, FALSE);
    }
    pthread_mutex_unlock(&aic_prof.profile_mutex);
}

void aic_add_le_data_count(uint8_t data_type)
{
    AicLogMsg("aic_add_le_data_count, data_type is %x", data_type);

    if((data_type == 1) || (data_type == 2))//1:keyboard, 2:mouse
    {
        aic_prof.hogp_packet_count++;
        if(!is_profile_busy(profile_hogp))
        {
            AicLogMsg("hogp idle->busy");
            update_profile_state(profile_hogp, TRUE);
        }
    }

    if(data_type == 3)//voice
    {
        aic_prof.voice_packet_count ++;
        if(!is_profile_busy(profile_voice))
        {
            AicLogMsg("voice idle->busy");
            update_profile_state(profile_voice, TRUE);
        }
    }
}

void aic_set_bt_on(uint8_t bt_on) {
    AicLogMsg("bt stack is init");
    pthread_mutex_lock(&aic_prof.coex_mutex);
    aic_prof.bt_on = bt_on;
    pthread_mutex_unlock(&aic_prof.coex_mutex);
    if(!bt_on)
      return;
    uint8_t ttmp[1] = {1};
    aic_vendor_cmd_to_fw(0xfc1b, 1, ttmp, NULL);
}

static aic_parse_manager_t parse_interface = {
  aic_parse_internal_event_intercept,
  aic_parse_l2cap_data,
  aic_parse_init,
  aic_parse_cleanup,
  aic_parse_command,
  aic_add_le_profile,
  aic_delete_le_profile,
  aic_add_le_data_count,
  aic_set_bt_on,
};

aic_parse_manager_t *aic_parse_manager_get_interface() {
  return &parse_interface;
}


