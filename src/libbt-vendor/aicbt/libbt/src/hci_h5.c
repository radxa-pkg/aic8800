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
*	    hci_h5.c
*
*	Abstract:
*	    Contain HCI transport send/receive functions for UART H5 Interface.
*
*	Notes:
*	      This is designed for UART H5 HCI Interface in Android 8.0
*
******************************************************************************/
#define LOG_TAG "bt_h5_int"
#include <utils/Log.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/wait.h>

#include "hci_h5_int.h"
#include "bt_skbuff.h"
#include "bt_list.h"
#include "bt_hci_bdroid.h"
#include "userial.h"


/******************************************************************************
**  Constants & Macros
******************************************************************************/
#define H5_TRACE_DATA_ENABLE                   0//if you want to see data tx and rx, set H5_TRACE_DATA_ENABLE 1
#define H5_LOG_VERBOSE                         0

unsigned int h5_log_enable = 1;

#ifndef H5_LOG_BUF_SIZE
#define H5_LOG_BUF_SIZE                        1024
#endif
#define H5_LOG_MAX_SIZE                        (H5_LOG_BUF_SIZE - 12)


#ifndef H5_LOG_BUF_SIZE
#define H5_LOG_BUF_SIZE                        1024
#endif
#define H5_LOG_MAX_SIZE                        (H5_LOG_BUF_SIZE - 12)


#define DATA_RETRANS_COUNT                     40  //40*100 = 4000ms(4s)
#define BT_INIT_DATA_RETRANS_COUNT             200  //200*20 = 4000ms(4s)
//#define SYNC_RETRANS_COUNT                   14  //14*250 = 3500ms(3.5s)
#define SYNC_RETRANS_COUNT                     350  //350*10 = 3500ms(3.5s)
//#define CONF_RETRANS_COUNT                   14
#define CONF_RETRANS_COUNT                     350



#define DATA_RETRANS_TIMEOUT_VALUE             100 //ms
#define BT_INIT_DATA_RETRANS_TIMEOUT_VALUE     20 //ms
//#define SYNC_RETRANS_TIMEOUT_VALUE           250
#define SYNC_RETRANS_TIMEOUT_VALUE             10

//#define CONF_RETRANS_TIMEOUT_VALUE           250
#define CONF_RETRANS_TIMEOUT_VALUE             20
//#define WAIT_CT_BAUDRATE_READY_TIMEOUT_VALUE 250
#define WAIT_CT_BAUDRATE_READY_TIMEOUT_VALUE   5
#define H5_HW_INIT_READY_TIMEOUT_VALUE         4000//4



/* Maximum numbers of allowed internal
** outstanding command packets at any time
*/
#define INT_CMD_PKT_MAX_COUNT      8
#define INT_CMD_PKT_IDX_MASK       0x07


//HCI Event codes
#define HCI_CONNECTION_COMP_EVT    0x03
#define HCI_DISCONNECTION_COMP_EVT 0x05
#define HCI_COMMAND_COMPLETE_EVT   0x0E
#define HCI_COMMAND_STATUS_EVT     0x0F
#define HCI_NUM_OF_CMP_PKTS_EVT    0x13
#define HCI_BLE_EVT                0x3E


#define PATCH_DATA_FIELD_MAX_SIZE  252
#define READ_DATA_SIZE             16

// HCI data types //
#define H5_RELIABLE_PKT            0x01
#define H5_UNRELIABLE_PKT          0x00

#define H5_ACK_PKT                 0x00
#define HCI_COMMAND_PKT            0x01
#define HCI_ACLDATA_PKT            0x02
#define HCI_SCODATA_PKT            0x03
#define HCI_EVENT_PKT              0x04
#define H5_VDRSPEC_PKT             0x0E
#define H5_LINK_CTL_PKT            0x0F

#define H5_HDR_SEQ(hdr)            ((hdr)[0] & 0x07)
#define H5_HDR_ACK(hdr)            (((hdr)[0] >> 3) & 0x07)
#define H5_HDR_CRC(hdr)            (((hdr)[0] >> 6) & 0x01)
#define H5_HDR_RELIABLE(hdr)       (((hdr)[0] >> 7) & 0x01)
#define H5_HDR_PKT_TYPE(hdr)       ((hdr)[1] & 0x0f)
#define H5_HDR_LEN(hdr)            ((((hdr)[1] >> 4) & 0xff) + ((hdr)[2] << 4))
#define H5_HDR_SIZE                4

#define H5_CFG_SLID_WIN(cfg)       ((cfg) & 0x07)
#define H5_CFG_OOF_CNTRL(cfg)      (((cfg) >> 3) & 0x01)
#define H5_CFG_DIC_TYPE(cfg)       (((cfg) >> 4) & 0x01)
#define H5_CFG_VER_NUM(cfg)        (((cfg) >> 5) & 0x07)
#define H5_CFG_SIZE                1

#define H5_EVENT_RX                0x0001
#define H5_EVENT_EXIT              0x0200


/******************************************************************************
**  Local type definitions
******************************************************************************/

/* Callback function for the returned event of internal issued command */
typedef void (*tTIMER_HANDLE_CBACK)(union sigval sigval_value);

typedef struct {
    uint16_t opcode;        /* OPCODE of outstanding internal commands */
    tINT_CMD_CBACK cback;   /* Callback function when return of internal
                             * command is received */
} tINT_CMD_Q;

typedef AIC_BUFFER sk_buff;

typedef enum H5_RX_STATE {
    H5_W4_PKT_DELIMITER,
    H5_W4_PKT_START,
    H5_W4_HDR,
    H5_W4_DATA,
    H5_W4_CRC
} tH5_RX_STATE;

typedef enum H5_RX_ESC_STATE {
    H5_ESCSTATE_NOESC,
    H5_ESCSTATE_ESC
} tH5_RX_ESC_STATE;

typedef enum H5_LINK_STATE {
    H5_UNINITIALIZED,
    H5_INITIALIZED,
    H5_ACTIVE
} tH5_LINK_STATE;

/* Control block for HCISU_H5 */
typedef struct HCI_H5_CB {
    HC_BT_HDR   *p_rcv_msg;          /* Buffer to hold current rx HCI message */
    uint32_t    int_cmd_rsp_pending;        /* Num of internal cmds pending for ack */
    uint8_t     int_cmd_rd_idx;         /* Read index of int_cmd_opcode queue */
    uint8_t     int_cmd_wrt_idx;        /* Write index of int_cmd_opcode queue */
    tINT_CMD_Q  int_cmd[INT_CMD_PKT_MAX_COUNT]; /* FIFO queue */

    tINT_CMD_CBACK cback_h5sync;   /* Callback function when h5 sync*/

    uint8_t     sliding_window_size;
    uint8_t     oof_flow_control;
    uint8_t     dic_type;


    RTB_QUEUE_HEAD *unack;      // Unack'ed packets queue
    RTB_QUEUE_HEAD *rel;        // Reliable packets queue

    RTB_QUEUE_HEAD *unrel;      // Unreliable packets queue
    RTB_QUEUE_HEAD *recv_data;      // Unreliable packets queue


    uint8_t     rxseq_txack;        // rxseq == txack. // expected rx SeqNumber
    uint8_t     rxack;             // Last packet sent by us that the peer ack'ed //

    uint8_t     use_crc;
    uint8_t     is_txack_req;      // txack required? Do we need to send ack's to the peer? //

    // Reliable packet sequence number - used to assign seq to each rel pkt. */
    uint8_t     msgq_txseq;         //next pkt seq

    uint16_t    message_crc;
    uint32_t    rx_count;       //expected pkts to recv

    tH5_RX_STATE        rx_state;
    tH5_RX_ESC_STATE    rx_esc_state;
    tH5_LINK_STATE      link_estab_state;

    sk_buff     *rx_skb;
    sk_buff     *data_skb;
    sk_buff     *internal_skb;

    timer_t     timer_data_retrans;
    timer_t     timer_sync_retrans;
    timer_t     timer_conf_retrans;
    timer_t     timer_wait_ct_baudrate_ready;
    timer_t     timer_h5_hw_init_ready;

    uint32_t    data_retrans_count;
    uint32_t    sync_retrans_count;
    uint32_t    conf_retrans_count;

    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    pthread_t       thread_data_retrans;

    pthread_mutex_t data_mutex;
    pthread_cond_t  data_cond;
    pthread_t       thread_data_ready_cb;

    uint8_t     cleanuping;
} tHCI_H5_CB;

struct patch_struct {
    int nTxIndex;   // current sending pkt number
    int nTotal;     // total pkt number
    int nRxIndex;   // ack index from board
    int nNeedRetry; // if no response from board
};

/******************************************************************************
**  Variables
******************************************************************************/
volatile int h5_init_datatrans_flag;
/* Num of allowed outstanding HCI CMD packets */
volatile int num_hci_cmd_pkts = 1;
extern unsigned int aicbt_h5logfilter;

/******************************************************************************
**  Static variables
******************************************************************************/
static volatile uint8_t h5_retransfer_running = 0;
static volatile uint16_t h5_ready_events = 0;
static volatile uint8_t h5_data_ready_running = 0;
static tHCI_H5_CB aic_h5;
static pthread_mutex_t h5_wakeup_mutex = PTHREAD_MUTEX_INITIALIZER;

/******************************************************************************
**  Static function
******************************************************************************/
static timer_t OsAllocateTimer(tTIMER_HANDLE_CBACK timer_callback);
static int OsStartTimer(timer_t timerid, int msec, int mode);
static int OsStopTimer(timer_t timerid);
static uint16_t h5_wake_up();
static hci_h5_callbacks_t *h5_int_hal_callbacks;

/******************************************************************************
**  Externs
******************************************************************************/
extern void aic_btsnoop_net_open(void);
extern void aic_btsnoop_net_close(void);
extern void aic_btsnoop_net_write(serial_data_type_t type, uint8_t *data, bool is_received);
extern void userial_recv_rawdata_hook(unsigned char *buffer, unsigned int total_length);

//timer API for retransfer
int h5_alloc_data_retrans_timer();
int h5_free_data_retrans_timer();
int h5_stop_data_retrans_timer();
int h5_start_data_retrans_timer();

int h5_alloc_sync_retrans_timer();
int h5_free_sync_retrans_timer();
int h5_stop_sync_retrans_timer();
int h5_start_sync_retrans_timer();

int h5_alloc_conf_retrans_timer();
int h5_free_conf_retrans_timer();
int h5_stop_conf_retrans_timer();
int h5_start_conf_retrans_timer();

int h5_alloc_wait_controller_baudrate_ready_timer();
int h5_free_wait_controller_baudrate_ready_timer();
int h5_stop_wait_controller_baudrate_ready_timer();
int h5_start_wait_controller_baudrate_ready_timer();

int h5_alloc_hw_init_ready_timer();
int h5_free_hw_init_ready_timer();
int h5_stop_hw_init_ready_timer();
int h5_start_hw_init_ready_timer();

int h5_enqueue(IN sk_buff *skb);

// bite reverse in bytes
// 00000001 -> 10000000
// 00000100 -> 00100000
const uint8_t byte_rev_table[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

#ifndef H5_LOG_BUF_SIZE
#define H5_LOG_BUF_SIZE  1024
#endif
#define H5_LOG_MAX_SIZE  (H5_LOG_BUF_SIZE - 12)

#define LOGI0(t,s) __android_log_write(ANDROID_LOG_INFO, t, s)

static void H5LogMsg(const char *fmt_str, ...)
{
    static char buffer[H5_LOG_BUF_SIZE];
    if(h5_log_enable == 1) {
        va_list ap;
        va_start(ap, fmt_str);
        vsnprintf(&buffer[0], H5_LOG_MAX_SIZE, fmt_str, ap);
        va_end(ap);

        LOGI0("H5: ", buffer);
     } else {
        return;
     }
}

static void aicbt_h5_send_hw_error()
{
    unsigned char p_buf[100];
    const char *str = "host stack: h5 send error\n";
    int length = strlen(str) + 1 + 4;
    p_buf[0] = HCIT_TYPE_EVENT;//event
    p_buf[1] = HCI_VSE_SUBCODE_DEBUG_INFO_SUB_EVT;//firmwre event log
    p_buf[2] = strlen(str) + 2;//len
    p_buf[3] = 0x01;// host log opcode
    strcpy((char *)&p_buf[4], str);
    userial_recv_rawdata_hook(p_buf,length);

    length = 4;
    p_buf[0] = HCIT_TYPE_EVENT;//event
    p_buf[1] = HCI_HARDWARE_ERROR_EVT;//hardware error
    p_buf[2] = 0x01;//len
    p_buf[3] = H5_HWERR_CODE_AIC;//h5 error code
    userial_recv_rawdata_hook(p_buf,length);
}

// reverse bit
static __inline uint8_t bit_rev8(uint8_t byte)
{
    return byte_rev_table[byte];
}

// reverse bit
static __inline uint16_t bit_rev16(uint16_t x)
{
    return (bit_rev8(x & 0xff) << 8) | bit_rev8(x >> 8);
}

static const uint16_t crc_table[] =
{
    0x0000, 0x1081, 0x2102, 0x3183,
    0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f
};

// Initialise the crc calculator
#define H5_CRC_INIT(x) x = 0xffff


/*******************************************************************************
**
** Function        ms_delay
**
** Description     sleep unconditionally for timeout milliseconds
**
** Returns         None
**
*******************************************************************************/
void ms_delay (uint32_t timeout)
{
    struct timespec delay;
    int err;

    if (timeout == 0)
        return;

    delay.tv_sec = timeout / 1000;
    delay.tv_nsec = 1000 * 1000 * (timeout%1000);

    /* [u]sleep can't be used because it uses SIGALRM */
    do {
        err = nanosleep(&delay, &delay);
    } while (err < 0 && errno ==EINTR);
}

/***********************************************
//
//skb related functions
//
//
//
***********************************************/
uint8_t *skb_get_data(IN sk_buff *skb)
{
    return skb->Data;
}

uint32_t skb_get_data_length(IN sk_buff *skb)
{
    return skb->Length;
}

sk_buff * skb_alloc(IN unsigned int len)
{
    sk_buff * skb = (sk_buff * )RtbAllocate(len, 0);
    return skb;
}

void skb_free(IN OUT sk_buff **skb)
{
    RtbFree(*skb);
    *skb = NULL;
    return;
}

static  void skb_unlink(sk_buff *skb, struct _RTB_QUEUE_HEAD * list)
{
    RtbRemoveNode(list, skb);
}

// increase the date length in sk_buffer by len,
// and return the increased header pointer
uint8_t *skb_put(OUT sk_buff* skb, IN uint32_t len)
{
    AIC_BUFFER * rtb = (AIC_BUFFER * )skb;

    return RtbAddTail(rtb, len);
}

// change skb->len to len
// !!! len should less than skb->len
void skb_trim( sk_buff *skb, unsigned int len)
{
    AIC_BUFFER * rtb = (AIC_BUFFER * )skb;
    uint32_t skb_len = skb_get_data_length(skb);

    RtbRemoveTail(rtb, (skb_len - len));
    return;
}

uint8_t skb_get_pkt_type( sk_buff *skb)
{
    return BT_CONTEXT(skb)->PacketType;
}

void skb_set_pkt_type( sk_buff *skb, uint8_t pkt_type)
{
    BT_CONTEXT(skb)->PacketType = pkt_type;
}

// decrease the data length in sk_buffer by len,
// and move the content forward to the header.
// the data in header will be removed.
void skb_pull(OUT  sk_buff * skb, IN uint32_t len)
{
    AIC_BUFFER * rtb = (AIC_BUFFER * )skb;
    RtbRemoveHead(rtb, len);
    return;
}

sk_buff * skb_alloc_and_init(IN uint8_t PktType, IN uint8_t * Data, IN uint32_t  DataLen)
{
    sk_buff * skb = skb_alloc(DataLen);
    if (NULL == skb)
        return NULL;

    memcpy(skb_put(skb, DataLen), Data, DataLen);
    skb_set_pkt_type(skb, PktType);

    return skb;
}

static void skb_queue_head(IN RTB_QUEUE_HEAD * skb_head, IN AIC_BUFFER * skb)
{
    RtbQueueHead(skb_head, skb);
}

static void skb_queue_tail(IN RTB_QUEUE_HEAD * skb_head, IN AIC_BUFFER * skb)
{
    RtbQueueTail(skb_head, skb);
}

static AIC_BUFFER* skb_dequeue_head(IN RTB_QUEUE_HEAD * skb_head)
{
    return RtbDequeueHead(skb_head);
}

static AIC_BUFFER* skb_dequeue_tail(IN RTB_QUEUE_HEAD * skb_head)
{
    return RtbDequeueTail(skb_head);
}

static uint32_t skb_queue_get_length(IN RTB_QUEUE_HEAD * skb_head)
{
    return RtbGetQueueLen(skb_head);
}


/**
* Add "d" into crc scope, caculate the new crc value
*
* @param crc crc data
* @param d one byte data
*/
static void h5_crc_update(uint16_t *crc, uint8_t d)
{
    uint16_t reg = *crc;

    reg = (reg >> 4) ^ crc_table[(reg ^ d) & 0x000f];
    reg = (reg >> 4) ^ crc_table[(reg ^ (d >> 4)) & 0x000f];

    *crc = reg;
}

struct __una_u16 { uint16_t x; };
/*static __inline uint16_t __get_unaligned_cpu16(const void *p)
{
    const struct __una_u16 *ptr = (const struct __una_u16 *)p;
    return ptr->x;
}*/

/*
static __inline uint16_t get_unaligned_be16(const void *p)
{
    return __get_unaligned_cpu16((const uint8_t *)p);
}*/
/**
* Get crc data.
*
* @param h5 aic h5 struct
* @return crc data
*/
static uint16_t h5_get_crc(tHCI_H5_CB *h5)
{
   uint16_t crc = 0;
   uint8_t * data = skb_get_data(h5->rx_skb) + skb_get_data_length(h5->rx_skb) - 2;
   crc = data[1] + (data[0] << 8);
   return crc;
}

/**
* Just add 0xc0 at the end of skb,
* we can also use this to add 0xc0 at start while there is no data in skb
*
* @param skb socket buffer
*/
static void h5_slip_msgdelim(sk_buff *skb)
{
    const char pkt_delim = 0xc0;
    memcpy(skb_put(skb, 1), &pkt_delim, 1);
}

/**
* Slip ecode one byte in h5 proto, as follows:
* 0xc0 -> 0xdb, 0xdc
* 0xdb -> 0xdb, 0xdd
* 0x11 -> 0xdb, 0xde
* 0x13 -> 0xdb, 0xdf
* others will not change
*
* @param skb socket buffer
* @c pure data in the one byte
*/
static void h5_slip_one_byte(sk_buff *skb, uint8_t unencode_form)
{
    const signed char esc_c0[2] = { 0xdb, 0xdc };
    const signed char esc_db[2] = { 0xdb, 0xdd };
    const signed char esc_11[2] = { 0xdb, 0xde };
    const signed char esc_13[2] = { 0xdb, 0xdf };

    switch (unencode_form) {
        case 0xc0:
            memcpy(skb_put(skb, 2), &esc_c0, 2);
            break;
        case 0xdb:
            memcpy(skb_put(skb, 2), &esc_db, 2);
            break;

        case 0x11:
            if(aic_h5.oof_flow_control)
                memcpy(skb_put(skb, 2), &esc_11, 2);
            else
                memcpy(skb_put(skb, 1), &unencode_form, 1);
            break;

        case 0x13:
            if(aic_h5.oof_flow_control)
                memcpy(skb_put(skb, 2), &esc_13, 2);
            else
                memcpy(skb_put(skb, 1), &unencode_form, 1);
            break;

        default:
            memcpy(skb_put(skb, 1), &unencode_form, 1);
    }
}

/**
* Decode one byte in h5 proto, as follows:
* 0xdb, 0xdc -> 0xc0
* 0xdb, 0xdd -> 0xdb
* 0xdb, 0xde -> 0x11
* 0xdb, 0xdf -> 0x13
* others will not change
*
* @param h5 aic h5 struct
* @byte pure data in the one byte
*/
static void h5_unslip_one_byte(tHCI_H5_CB *h5, unsigned char byte)
{
    const uint8_t c0 = 0xc0, db = 0xdb;
    const uint8_t oof1 = 0x11, oof2 = 0x13;
    uint8_t *hdr = (uint8_t *)skb_get_data(h5->rx_skb);

    if (H5_ESCSTATE_NOESC == h5->rx_esc_state) {
        if (0xdb == byte) {
            h5->rx_esc_state = H5_ESCSTATE_ESC;
        } else {
            memcpy(skb_put(h5->rx_skb, 1), &byte, 1);
            //Check Pkt Header's CRC enable bit
            if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                h5_crc_update(&h5->message_crc, byte);
            h5->rx_count--;
        }
    } else if(H5_ESCSTATE_ESC == h5->rx_esc_state) {
        switch (byte) {
            case 0xdc:
                memcpy(skb_put(h5->rx_skb, 1), &c0, 1);
                if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                    h5_crc_update(&h5-> message_crc, 0xc0);
                h5->rx_esc_state = H5_ESCSTATE_NOESC;
                h5->rx_count--;
                break;
            case 0xdd:
                memcpy(skb_put(h5->rx_skb, 1), &db, 1);
                 if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                    h5_crc_update(&h5-> message_crc, 0xdb);
                h5->rx_esc_state = H5_ESCSTATE_NOESC;
                h5->rx_count--;
                break;
            case 0xde:
                memcpy(skb_put(h5->rx_skb, 1), &oof1, 1);
                if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                    h5_crc_update(&h5-> message_crc, oof1);
                h5->rx_esc_state = H5_ESCSTATE_NOESC;
                h5->rx_count--;
                break;
            case 0xdf:
                memcpy(skb_put(h5->rx_skb, 1), &oof2, 1);
                if (H5_HDR_CRC(hdr) && h5->rx_state != H5_W4_CRC)
                    h5_crc_update(&h5-> message_crc, oof2);
                h5->rx_esc_state = H5_ESCSTATE_NOESC;
                h5->rx_count--;
                break;
            default:
                ALOGE("Error: Invalid byte %02x after esc byte", byte);
                skb_free(&h5->rx_skb);
                h5->rx_skb = NULL;
                h5->rx_state = H5_W4_PKT_DELIMITER;
                h5->rx_count = 0;
                break;
        }
    }
}
/**
* Prepare h5 packet, packet format as follow:
*  | LSB 4 octets  | 0 ~4095| 2 MSB
*  |packet header | payload | data integrity check |
*
* pakcket header fromat is show below:
*  | LSB 3 bits         | 3 bits             | 1 bits                       | 1 bits          |
*  | 4 bits     | 12 bits        | 8 bits MSB
*  |sequence number | acknowledgement number | data integrity check present | reliable packet |
*  |packet type | payload length | header checksum
*
* @param h5 aic h5 struct
* @param data pure data
* @param len the length of data
* @param pkt_type packet type
* @return socket buff after prepare in h5 proto
*/
static sk_buff *h5_prepare_pkt(tHCI_H5_CB *h5, uint8_t *data, signed long len, signed long pkt_type)
{
    sk_buff *nskb;
    uint8_t hdr[4];
    uint16_t H5_CRC_INIT(h5_txmsg_crc);
    int rel, i;
    //H5LogMsg("HCI h5_prepare_pkt");

    switch (pkt_type) {
        case HCI_ACLDATA_PKT:
        case HCI_COMMAND_PKT:
        case HCI_EVENT_PKT:
            rel = H5_RELIABLE_PKT;  // reliable
            break;
        case H5_ACK_PKT:
        case H5_VDRSPEC_PKT:
        case H5_LINK_CTL_PKT:
        case HCI_SCODATA_PKT:
            rel = H5_UNRELIABLE_PKT;// unreliable
            break;
        default:
            ALOGE("Unknown packet type");
            return NULL;
    }

    // Max len of packet: (original len +4(h5 hdr) +2(crc))*2
    //   (because bytes 0xc0 and 0xdb are escaped, worst case is
    //   when the packet is all made of 0xc0 and 0xdb :) )
    //   + 2 (0xc0 delimiters at start and end).

    nskb = skb_alloc((len + 6) * 2 + 2);
    if (!nskb) {
        H5LogMsg("nskb is NULL");
        return NULL;
    }

    //Add SLIP start byte: 0xc0
    h5_slip_msgdelim(nskb);
    // set AckNumber in SlipHeader
    hdr[0] = h5->rxseq_txack << 3;
    h5->is_txack_req = 0;

    H5LogMsg("We request packet no(%u) to card", h5->rxseq_txack);
    H5LogMsg("Sending packet with seqno %u and wait %u", h5->msgq_txseq, h5->rxseq_txack);
    if (H5_RELIABLE_PKT == rel) {
        // set reliable pkt bit and SeqNumber
        hdr[0] |= 0x80 + h5->msgq_txseq;
        //H5LogMsg("Sending packet with seqno(%u)", h5->msgq_txseq);
        ++(h5->msgq_txseq);
        h5->msgq_txseq = (h5->msgq_txseq) & 0x07;
    }

    // set DicPresent bit
    if (h5->use_crc)
        hdr[0] |= 0x40;

    // set packet type and payload length
    hdr[1] = ((len << 4) & 0xff) | pkt_type;
    hdr[2] = (uint8_t)(len >> 4);
    // set checksum
    hdr[3] = ~(hdr[0] + hdr[1] + hdr[2]);

    // Put h5 header */
    for (i = 0; i < 4; i++) {
        h5_slip_one_byte(nskb, hdr[i]);
        if (h5->use_crc)
            h5_crc_update(&h5_txmsg_crc, hdr[i]);
    }

    // Put payload */
    for (i = 0; i < len; i++) {
        h5_slip_one_byte(nskb, data[i]);
        if (h5->use_crc)
            h5_crc_update(&h5_txmsg_crc, data[i]);
    }

    // Put CRC */
    if (h5->use_crc) {
        h5_txmsg_crc = bit_rev16(h5_txmsg_crc);
        h5_slip_one_byte(nskb, (uint8_t) ((h5_txmsg_crc >> 8) & 0x00ff));
        h5_slip_one_byte(nskb, (uint8_t) (h5_txmsg_crc & 0x00ff));
    }

    // Add SLIP end byte: 0xc0
    h5_slip_msgdelim(nskb);
    return nskb;
}
/**
* Removed controller acked packet from Host's unacked lists
*
* @param h5 aic h5 struct
*/
static void h5_remove_acked_pkt(tHCI_H5_CB *h5)
{
    RT_LIST_HEAD* Head = NULL;
    RT_LIST_ENTRY* Iter = NULL, *Temp = NULL;
    AIC_BUFFER *skb = NULL;

    int pkts_to_be_removed = 0;
    int seqno = 0;
    int i = 0;

    pthread_mutex_lock(&h5_wakeup_mutex);

    seqno = h5->msgq_txseq;
    pkts_to_be_removed = RtbGetQueueLen(h5->unack);

    while (pkts_to_be_removed) {
        if (h5->rxack == seqno)
            break;

        pkts_to_be_removed--;
        seqno = (seqno - 1) & 0x07;
    }

    if (h5->rxack != seqno) {
        H5LogMsg("Peer acked invalid packet");
    }


    // remove ack'ed packet from bcsp->unack queue
    i = 0;//  number of pkts has been removed from un_ack queue.
    Head = (RT_LIST_HEAD *)(h5->unack);
    LIST_FOR_EACH_SAFELY(Iter, Temp, Head) {
        skb = LIST_ENTRY(Iter, sk_buff, List);
        if (i >= pkts_to_be_removed)
            break;

        skb_unlink(skb, h5->unack);
        skb_free(&skb);
        i++;
    }

    if (0 == skb_queue_get_length(h5->unack)) {
        h5_stop_data_retrans_timer();
        aic_h5.data_retrans_count = 0;
    }

    if (i != pkts_to_be_removed) {
        H5LogMsg("Removed only (%u) out of (%u) pkts", i, pkts_to_be_removed);
    }

    pthread_mutex_unlock(&h5_wakeup_mutex);

}

/**
* Aicsemi send pure ack, send a packet only with an ack
*
* @param fd uart file descriptor
*
*/
/*
static void hci_h5_send_pure_ack(void)
{
    //uint16_t bytes_sent = 0;
    sk_buff * skb = NULL;
    uint8_t ack_data = 0;

    skb = skb_alloc_and_init(H5_ACK_PKT, &ack_data, 0);
    if(!skb) {
        ALOGE("skb_alloc_and_init fail!");
        return;
    }

    H5LogMsg("H5: --->>>send pure ack");
    h5_enqueue(skb);
    h5_wake_up();

#if 0
    sk_buff *nskb = h5_prepare_pkt(&aic_h5, NULL, 0, H5_ACK_PKT);
    if (nskb == NULL) {
        ALOGE("h5_prepare_pkt allocate memory fail");
        return;
    }
    H5LogMsg("H5: --->>>send pure ack");
    uint8_t * data = skb_get_data(nskb);

#if H5_TRACE_DATA_ENABLE
    {
        uint32_t iTemp = 0;
        uint32_t iTempTotal = 16;

        H5LogMsg("H5 TX: length(%d)", skb_get_data_length(nskb));
        if (iTempTotal > skb_get_data_length(nskb)) {
            iTempTotal = skb_get_data_length(nskb);
        }

        for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
            H5LogMsg("0x%x", data[iTemp]);
        }
    }
#endif

    bytes_sent = h5_int_hal_callbacks->h5_int_transmit_data_cb(DATA_TYPE_H5, data, skb_get_data_length(nskb));
    H5LogMsg("bytes_sent(%d)", bytes_sent);

    skb_free(&nskb);
#endif
    return;

}*/

static void hci_h5_send_sync_req(void)
{
    //uint16_t bytes_sent = 0;
    unsigned char    h5sync[2]     = {0x01, 0x7E};
    sk_buff * skb = NULL;

    skb = skb_alloc_and_init(H5_LINK_CTL_PKT, h5sync, sizeof(h5sync));
    if(!skb) {
        ALOGE("skb_alloc_and_init fail!");
        return;
    }
    H5LogMsg("H5: --->>>send sync req");

    h5_enqueue(skb);
    h5_wake_up();
#if 0
    sk_buff *nskb = h5_prepare_pkt(&aic_h5, h5sync, sizeof(h5sync), H5_LINK_CTL_PKT);
    if (nskb == NULL) {
        ALOGE("h5_prepare_pkt allocate memory fail");
        return;
    }
    H5LogMsg("H5: --->>>send sync req");
    uint8_t * data = skb_get_data(nskb);

#if H5_TRACE_DATA_ENABLE
    {
        uint32_t iTemp = 0;
        uint32_t iTempTotal = 16;
        H5LogMsg("H5 TX: length(%d)", skb_get_data_length(nskb));
        if (iTempTotal > skb_get_data_length(nskb)) {
            iTempTotal = skb_get_data_length(nskb);
        }
        for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
            H5LogMsg("0x%x", data[iTemp]);
        }
    }
#endif

    bytes_sent = h5_int_hal_callbacks->h5_int_transmit_data_cb(DATA_TYPE_H5, data, skb_get_data_length(nskb));
    H5LogMsg("bytes_sent(%d)", bytes_sent);

    skb_free(&nskb);
#endif
    return;
}

static void hci_h5_send_sync_resp()
{
    //uint16_t bytes_sent = 0;
    unsigned char h5syncresp[2] = {0x02, 0x7D};
    sk_buff * skb = NULL;

    skb = skb_alloc_and_init(H5_LINK_CTL_PKT, h5syncresp, sizeof(h5syncresp));
    if(!skb) {
        ALOGE("skb_alloc_and_init fail!");
        return;
    }

    H5LogMsg("H5: --->>>send sync resp");
    h5_enqueue(skb);
    h5_wake_up();
#if 0
    sk_buff *nskb = h5_prepare_pkt(&aic_h5, h5syncresp, sizeof(h5syncresp), H5_LINK_CTL_PKT);
    if (nskb == NULL) {
        ALOGE("h5_prepare_pkt allocate memory fail");
        return;
    }
    H5LogMsg("H5: --->>>send sync resp");
    uint8_t * data = skb_get_data(nskb);

#if H5_TRACE_DATA_ENABLE
    {
        uint32_t iTemp = 0;
        uint32_t iTempTotal = 16;
        H5LogMsg("H5 TX: length(%d)", skb_get_data_length(nskb));
        if (iTempTotal > skb_get_data_length(nskb)) {
            iTempTotal = skb_get_data_length(nskb);
        }
        for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
            H5LogMsg("0x%x", data[iTemp]);
        }
    }
#endif

    bytes_sent = h5_int_hal_callbacks->h5_int_transmit_data_cb(DATA_TYPE_H5, data, skb_get_data_length(nskb));
    H5LogMsg("bytes_sent(%d)", bytes_sent);

    skb_free(&nskb);
#endif
    return;
}

static void hci_h5_send_conf_req()
{
    //uint16_t bytes_sent = 0;
    unsigned char h5conf[3] = {0x03, 0xFC, 0x14};
    sk_buff * skb = NULL;

    skb = skb_alloc_and_init(H5_LINK_CTL_PKT, h5conf, sizeof(h5conf));
    if (!skb) {
        ALOGE("skb_alloc_and_init fail!");
        return;
    }

    H5LogMsg("H5: --->>>send conf req");
    h5_enqueue(skb);
    h5_wake_up();

#if 0
    sk_buff *nskb = h5_prepare_pkt(&aic_h5, h5conf, sizeof(h5conf), H5_LINK_CTL_PKT);
    if (nskb == NULL) {
        ALOGE("h5_prepare_pkt allocate memory fail");
        return;
    }
    H5LogMsg("H5: --->>>send conf req");
    uint8_t * data = skb_get_data(nskb);

#if H5_TRACE_DATA_ENABLE
    {
        uint32_t iTemp = 0;
        uint32_t iTempTotal = 16;
        H5LogMsg("H5 TX: length(%d)", skb_get_data_length(nskb));
        if (iTempTotal > skb_get_data_length(nskb)) {
            iTempTotal = skb_get_data_length(nskb);
        }
        for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
            H5LogMsg("0x%x", data[iTemp]);
        }
    }
#endif

    bytes_sent = h5_int_hal_callbacks->h5_int_transmit_data_cb(DATA_TYPE_H5, data, skb_get_data_length(nskb));
    H5LogMsg("bytes_sent(%d)", bytes_sent);

    skb_free(&nskb);
#endif
    return;
}


static void hci_h5_send_conf_resp()
{
    //uint16_t bytes_sent = 0;
    unsigned char h5confresp[2] = {0x04, 0x7B};
    sk_buff * skb = NULL;

    skb = skb_alloc_and_init(H5_LINK_CTL_PKT, h5confresp, sizeof(h5confresp));
    if (!skb) {
        ALOGE("skb_alloc_and_init fail!");
        return;
    }

    H5LogMsg("H5: --->>>send conf resp");
    h5_enqueue(skb);
    h5_wake_up();
#if 0
    sk_buff *nskb = h5_prepare_pkt(&aic_h5, h5confresp, sizeof(h5confresp), H5_LINK_CTL_PKT);
    if (nskb == NULL) {
        ALOGE("h5_prepare_pkt allocate memory fail");
        return;
    }
    H5LogMsg("H5: --->>>send conf resp");
    uint8_t * data = skb_get_data(nskb);

#if H5_TRACE_DATA_ENABLE
    {
        uint32_t iTemp = 0;
        uint32_t iTempTotal = 16;
        H5LogMsg("H5 TX: length(%d)", skb_get_data_length(nskb));
        if (iTempTotal > skb_get_data_length(nskb)) {
            iTempTotal = skb_get_data_length(nskb);
        }
        for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
            H5LogMsg("0x%x", data[iTemp]);
        }
    }
#endif

    bytes_sent = h5_int_hal_callbacks->h5_int_transmit_data_cb(DATA_TYPE_H5, data, skb_get_data_length(nskb));
    H5LogMsg("bytes_sent(%d)", bytes_sent);

    skb_free(&nskb);
#endif
    return;
}

static void aic_notify_hw_h5_init_result(uint8_t status)
{
    H5LogMsg("aic_notify_hw_h5_init_result %d", status);
    uint8_t sync_event[6] = {0x0e, 0x04, 0x03, 0xee, 0xfc, status};
    // we need to make a sync event to bt
    sk_buff     *rx_skb;
    rx_skb = skb_alloc_and_init(HCI_EVENT_PKT, sync_event, sizeof(sync_event));

    if (!rx_skb) {
        ALOGE("%s, rx_skb alloc fail!", __func__);
        return;
    }
    pthread_mutex_lock(&aic_h5.data_mutex);
    skb_queue_tail(aic_h5.recv_data, rx_skb);
    pthread_cond_signal(&aic_h5.data_cond);
    pthread_mutex_unlock(&aic_h5.data_mutex);
}

static uint8_t *acl_pack = NULL;
static uint32_t acl_len=0;
static uint8_t loopbackmode = 0x00;
static timer_t loopacltimer=0;

static void loop_acl_cb()
{
    sk_buff     *rx_skb;
    rx_skb = skb_alloc_and_init(HCI_ACLDATA_PKT, acl_pack, acl_len);

    pthread_mutex_lock(&aic_h5.data_mutex);
    skb_queue_tail(aic_h5.recv_data, rx_skb);
    pthread_cond_signal(&aic_h5.data_cond);
    pthread_mutex_unlock(&aic_h5.data_mutex);
}

static void loopacl_timer_handler()
{
    loop_acl_cb();
}

static void start_loopacktimer()
{
    if (loopacltimer == 0)
        loopacltimer = OsAllocateTimer(loopacl_timer_handler);
    OsStartTimer(loopacltimer, 10, 0);
}

static sk_buff * h5_dequeue()
{
    sk_buff *skb = NULL;
    //   First of all, check for unreliable messages in the queue,
    //   since they have higher priority
    //H5LogMsg("h5_dequeue++");
    if ((skb = (sk_buff*)skb_dequeue_head(aic_h5.unrel)) != NULL) {
        sk_buff *nskb = h5_prepare_pkt(&aic_h5, skb_get_data(skb), skb_get_data_length(skb), skb_get_pkt_type(skb));
        if (nskb) {
            skb_free(&skb);
            return nskb;
        } else {
            skb_queue_head(aic_h5.unrel, skb);
        }
    }
    //   Now, try to send a reliable pkt. We can only send a
    //   reliable packet if the number of packets sent but not yet ack'ed
    //   is < than the winsize

    //    H5LogMsg("RtbGetQueueLen(aic_h5.unack) = (%d), sliding_window_size = (%d)", RtbGetQueueLen(aic_h5.unack), aic_h5.sliding_window_size);

    if (RtbGetQueueLen(aic_h5.unack)< aic_h5.sliding_window_size &&
        (skb = (sk_buff *)skb_dequeue_head(aic_h5.rel)) != NULL) {
        sk_buff *nskb = h5_prepare_pkt(&aic_h5, skb_get_data(skb), skb_get_data_length(skb), skb_get_pkt_type(skb));
        if (nskb) {
            skb_queue_tail(aic_h5.unack, skb);
            h5_start_data_retrans_timer();
            return nskb;
        } else {
            skb_queue_head(aic_h5.rel, skb);
        }
    }
    //   We could not send a reliable packet, either because there are
    //   none or because there are too many unack'ed packets. Did we receive
    //   any packets we have not acknowledged yet
    if (aic_h5.is_txack_req) {
        // if so, craft an empty ACK pkt and send it on BCSP unreliable
        // channel
        sk_buff *nskb = h5_prepare_pkt(&aic_h5, NULL, 0, H5_ACK_PKT);
        return nskb;
    }
    // We have nothing to send
    return NULL;
}

int h5_enqueue(IN sk_buff *skb)
{
    //Pkt length must be less than 4095 bytes
    if (skb_get_data_length(skb) > 0xFFF) {
        ALOGE("skb len > 0xFFF");
        skb_free(&skb);
        return 0;
    }

    switch (skb_get_pkt_type(skb)) {
        case HCI_ACLDATA_PKT:
        case HCI_COMMAND_PKT:
            skb_queue_tail(aic_h5.rel, skb);
            break;

        case H5_LINK_CTL_PKT:
        case H5_ACK_PKT:
        case H5_VDRSPEC_PKT:
        case HCI_SCODATA_PKT:
            skb_queue_tail(aic_h5.unrel, skb);/* 3-wire LinkEstablishment*/
            break;
        default:
            skb_free(&skb);
            break;
    }
    return 0;
}


static uint16_t h5_wake_up()
{
    uint16_t bytes_sent = 0;
    sk_buff *skb = NULL;
    uint8_t * data = NULL;
    uint32_t data_len = 0;

    pthread_mutex_lock(&h5_wakeup_mutex);
    //H5LogMsg("h5_wake_up");
    while (NULL != (skb = h5_dequeue())) {
        data = skb_get_data(skb);
        data_len = skb_get_data_length(skb);
        // we adopt the hci_h5 interface to send data
        bytes_sent = h5_int_hal_callbacks->h5_int_transmit_data_cb(DATA_TYPE_H5, data, data_len);
        // bytes_sent = userial_write(0, data, data_len);

        H5LogMsg("bytes_sent(%d)", bytes_sent);

#if H5_TRACE_DATA_ENABLE
        {
            uint32_t iTemp = 0;
            uint32_t iTempTotal = 16;
            H5LogMsg("H5 TX: length(%d)", data_len);
            if (iTempTotal > data_len) {
                iTempTotal = data_len;
            }
            for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
                H5LogMsg("0x%x", data[iTemp]);
            }
        }
#endif
        skb_free(&skb);
    }

    pthread_mutex_unlock(&h5_wakeup_mutex);
    return bytes_sent;
}

void h5_process_ctl_pkts(void)
{
    //process h5 link establish
    //int len;
    uint8_t cfg;

    //tHCI_H5_CB *p_cb = &aic_h5;
    sk_buff *skb = aic_h5.rx_skb;

    unsigned char   h5sync[2]     = {0x01, 0x7E},
                    h5syncresp[2] = {0x02, 0x7D},
                    h5conf[3]     = {0x03, 0xFC, 0x14},
                    h5confresp[2] = {0x04, 0x7B};
                    //h5InitOk[2] = {0xF1, 0xF1};

    //uint8_t *ph5_payload = NULL;
    //ph5_payload = (uint8_t *)(p_cb->p_rcv_msg + 1);

    if (aic_h5.link_estab_state == H5_UNINITIALIZED) {  //sync
        if (!memcmp(skb_get_data(skb), h5sync, 2)) {
            H5LogMsg("H5: <<<---recv sync req");
            hci_h5_send_sync_resp();
        } else if (!memcmp(skb_get_data(skb), h5syncresp, 2)) {
            H5LogMsg("H5: <<<---recv sync resp");
            h5_stop_sync_retrans_timer();
            aic_h5.sync_retrans_count  = 0;
            aic_h5.link_estab_state = H5_INITIALIZED;

            //send config req
            hci_h5_send_conf_req();
            h5_start_conf_retrans_timer();
        }
    } else if(aic_h5.link_estab_state == H5_INITIALIZED) {  //config
        if (!memcmp(skb_get_data(skb), h5sync, 0x2)) {

            H5LogMsg("H5: <<<---recv sync req in H5_INITIALIZED");
            hci_h5_send_sync_resp();
        } else if (!memcmp(skb_get_data(skb), h5conf, 0x2)) {
             H5LogMsg("H5: <<<---recv conf req");
             hci_h5_send_conf_resp();
        } else if (!memcmp(skb_get_data(skb), h5confresp,  0x2)) {
            H5LogMsg("H5: <<<---recv conf resp");
            h5_stop_conf_retrans_timer();
            aic_h5.conf_retrans_count  = 0;

            aic_h5.link_estab_state = H5_ACTIVE;
            //notify hw to download patch
            memcpy(&cfg, skb_get_data(skb)+2, H5_CFG_SIZE);
            aic_h5.sliding_window_size = H5_CFG_SLID_WIN(cfg);
            aic_h5.oof_flow_control = H5_CFG_OOF_CNTRL(cfg);
            aic_h5.dic_type = H5_CFG_DIC_TYPE(cfg);
            H5LogMsg("aic_h5.sliding_window_size(%d), oof_flow_control(%d), dic_type(%d)",
            aic_h5.sliding_window_size, aic_h5.oof_flow_control, aic_h5.dic_type);
            if (aic_h5.dic_type)
                aic_h5.use_crc = 1;

            aic_notify_hw_h5_init_result(0);
        } else {
            H5LogMsg("H5_INITIALIZED receive event, ingnore");
        }
    } else if(aic_h5.link_estab_state == H5_ACTIVE) {
        if (!memcmp(skb_get_data(skb), h5sync, 0x2)) {

            H5LogMsg("H5: <<<---recv sync req in H5_ACTIVE");
            aicbt_h5_send_hw_error();
            //kill(getpid(), SIGKILL);
            hci_h5_send_sync_resp();
            H5LogMsg("H5 : H5_ACTIVE transit to H5_UNINITIALIZED");
            aic_h5.link_estab_state = H5_UNINITIALIZED;
            hci_h5_send_sync_req();
            h5_start_sync_retrans_timer();
        } else if (!memcmp(skb_get_data(skb), h5conf, 0x2)) {
             H5LogMsg("H5: <<<---recv conf req in H5_ACTIVE");
             hci_h5_send_conf_resp();
        } else if (!memcmp(skb_get_data(skb), h5confresp,  0x2)) {
            H5LogMsg("H5: <<<---recv conf resp in H5_ACTIVE, discard");
        } else {
            H5LogMsg("H5_ACTIVE receive unknown link control msg, ingnore");
        }
    }
}

uint8_t isAicInternalCommand(uint16_t opcode)
{
    if(opcode == 0xFC17
        || opcode == 0xFC6D
        || opcode == 0xFC61
        || opcode == 0xFC20) {
        return 1;
    } else {
        return 0;
    }
}

/*****************************************************************************
* Check if it's a hci frame, if it is, complete it with response or parse the cmd complete event
*
* @param skb socket buffer
*
*/
extern uint8_t getchip_type();


static uint8_t hci_recv_frame(sk_buff *skb, uint8_t pkt_type)
{
    uint8_t intercepted = 0;
    //uint32_t i = 0 ;
#if H5_TRACE_DATA_ENABLE
    uint8_t *data = skb_get_data(skb);
#endif
    uint32_t data_len = skb_get_data_length(skb);

    H5LogMsg("UART H5 RX: length = %d", data_len);

#if H5_TRACE_DATA_ENABLE
    {
        uint32_t iTemp = 0;
        uint32_t iTempTotal = 16;
        H5LogMsg("H5 RX: length(%d)", data_len);
        if (iTempTotal > data_len) {
            iTempTotal = data_len;
        }
        for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
            H5LogMsg("0x%x", data[iTemp]);
        }
    }
#endif
    //we only intercept evt packet here
    if (pkt_type == HCI_EVENT_PKT) {
        uint8_t     *p;
        uint8_t     event_code;
        uint16_t    opcode, len;
        p = (uint8_t *)skb_get_data(skb);

        event_code = *p++;
        len = *p++;
        H5LogMsg("hci_recv_frame event_code(0x%x), len = %d", event_code, len);
        if (event_code == HCI_COMMAND_STATUS_EVT && len==0x04
            && p[0]==0x01 && p[1]==0x02 && p[2]==0xff && p[3]==0x3b){
            *( p-2) = HCI_COMMAND_COMPLETE_EVT;
            p[0]=0x02;p[1]=0xff;p[2]=0x3b;p[3]=0x01;
        }// fix HciUnknownCommand test fail

        if (loopbackmode == 0x01){
            if(event_code == HCI_LOOPBACK_COMMAND_EVT
                && p[0]==0x19 && p[1]==0xfc) {
                intercepted = 1;
                skb_free(&skb);
            }
        }//drop 0xfc19 frame when in loopbackmode,fix loopbackmode test fail

        if (event_code == HCI_COMMAND_COMPLETE_EVT) {
            num_hci_cmd_pkts = *p++;
            STREAM_TO_UINT16(opcode, p);

            if (opcode == HCI_VSC_UPDATE_BAUDRATE) {
                intercepted = 1;
                aic_h5.internal_skb = skb;
                H5LogMsg("CommandCompleteEvent for command h5_start_wait_controller_baudrate_ready_timer (0x%04X)", opcode);
                h5_start_wait_controller_baudrate_ready_timer();
            }
        } else if (event_code == 0xff) {
           intercepted = 1;
           skb_free(&skb);
        }
    }

    H5LogMsg("hci_recv_frame intercepted = %d", intercepted);
    return intercepted;
}


/**
* after rx data is parsed, and we got a rx frame saved in h5->rx_skb,
* this routinue is called.
* things todo in this function:
* 1. check if it's a hci frame, if it is, complete it with response or ack
* 2. see the ack number, free acked frame in queue
* 3. reset h5->rx_state, set rx_skb to null.
*
* @param h5 aic h5 struct
*
*/
static uint8_t h5_complete_rx_pkt(tHCI_H5_CB *h5)
{
    int pass_up = 1;
    uint16_t eventtype = 0;
    uint8_t *h5_hdr = NULL;
    //uint8_t complete_pkt = true;
    uint8_t pkt_type = 0;
    //tHCI_H5_CB *p_cb=&aic_h5;
    uint8_t status = 0;

    //H5LogMsg("HCI 3wire h5_complete_rx_pkt");
    h5_hdr = (uint8_t *)skb_get_data(h5->rx_skb);
    H5LogMsg("SeqNumber(%d), AckNumber(%d)", H5_HDR_SEQ(h5_hdr), H5_HDR_ACK(h5_hdr));

#if H5_TRACE_DATA_ENABLE
    {
        uint32_t iTemp = 0;
        uint32_t iTempTotal = 16;
        uint32_t data_len = skb_get_data_length(h5->rx_skb);
        uint8_t *data = skb_get_data(h5->rx_skb);
        H5LogMsg("H5 RX: length(%d)", data_len);

        if (iTempTotal > data_len) {
            iTempTotal = data_len;
        }
        for (iTemp = 0; iTemp < iTempTotal; iTemp++) {
            H5LogMsg("0x%x", data[iTemp]);
        }
    }
#endif

    if (H5_HDR_RELIABLE(h5_hdr)) {
        H5LogMsg("Received reliable seqno %u from card", h5->rxseq_txack);
        pthread_mutex_lock(&h5_wakeup_mutex);
        h5->rxseq_txack = H5_HDR_SEQ(h5_hdr) + 1;
        h5->rxseq_txack %= 8;
        h5->is_txack_req = 1;
        pthread_mutex_unlock(&h5_wakeup_mutex);
        // send down an empty ack if needed.
        h5_wake_up();
    }

    h5->rxack = H5_HDR_ACK(h5_hdr);
    pkt_type = H5_HDR_PKT_TYPE(h5_hdr);
    //H5LogMsg("h5_complete_rx_pkt, pkt_type = %d", pkt_type);
    switch (pkt_type) {
        case HCI_ACLDATA_PKT:
            pass_up = 1;
            eventtype = MSG_HC_TO_STACK_HCI_ACL;
            break;

        case HCI_EVENT_PKT:
            pass_up = 1;
            eventtype = MSG_HC_TO_STACK_HCI_EVT;
            break;

        case HCI_SCODATA_PKT:
            pass_up = 1;
            eventtype = MSG_HC_TO_STACK_HCI_SCO;
            break;
        case HCI_COMMAND_PKT:
            pass_up = 1;
            eventtype = MSG_HC_TO_STACK_HCI_ERR;
            break;

        case H5_LINK_CTL_PKT:
            pass_up = 0;
            break;

        case H5_ACK_PKT:
            pass_up = 0;
            break;

        default:
            ALOGE("Unknown pkt type(%d)", H5_HDR_PKT_TYPE(h5_hdr));
            eventtype = MSG_HC_TO_STACK_HCI_ERR;
            pass_up = 0;
            break;
    }

    // remove h5 header and send packet to hci
    h5_remove_acked_pkt(h5);

    if (H5_HDR_PKT_TYPE(h5_hdr) == H5_LINK_CTL_PKT) {

        skb_pull(h5->rx_skb, H5_HDR_SIZE);
        h5_process_ctl_pkts();
    }

    // decide if we need to pass up.
    if (pass_up) {
        skb_pull(h5->rx_skb, H5_HDR_SIZE);
        skb_set_pkt_type(h5->rx_skb, pkt_type);

        //send command or  acl data it to bluedroid stack
        uint16_t len = 0;
        sk_buff * skb_complete_pkt = h5->rx_skb;

        /* Allocate a buffer for message */

        len = BT_HC_HDR_SIZE + skb_get_data_length(skb_complete_pkt);
        h5->p_rcv_msg = (HC_BT_HDR *) malloc(len);

        if (h5->p_rcv_msg) {
            /* Initialize buffer with received h5 data */
            h5->p_rcv_msg->offset = 0;
            h5->p_rcv_msg->layer_specific = 0;
            h5->p_rcv_msg->event = eventtype;
            h5->p_rcv_msg->len = skb_get_data_length(skb_complete_pkt);
            memcpy((uint8_t *)(h5->p_rcv_msg + 1), skb_get_data(skb_complete_pkt), skb_get_data_length(skb_complete_pkt));
        }

        status = hci_recv_frame(skb_complete_pkt, pkt_type);

        if (h5->p_rcv_msg)
            free(h5->p_rcv_msg);

        if (!status) {
            pthread_mutex_lock(&aic_h5.data_mutex);
            skb_queue_tail(aic_h5.recv_data, h5->rx_skb);
            pthread_cond_signal(&aic_h5.data_cond);
            pthread_mutex_unlock(&aic_h5.data_mutex);
        }
    } else {
        // free ctl packet
        skb_free(&h5->rx_skb);
    }
    h5->rx_state = H5_W4_PKT_DELIMITER;
    aic_h5.rx_skb = NULL;
    return pkt_type;
}


/**
* Parse the receive data in h5 proto.
*
* @param h5 aic h5 struct
* @param data point to data received before parse
* @param count num of data
* @return reserved count
*/
static bool h5_recv(tHCI_H5_CB *h5, uint8_t *data, int count)
{
//   unsigned char *ptr;
    uint8_t *ptr;
    uint8_t * skb_data = NULL;
    uint8_t *hdr = NULL;
    bool complete_packet = false;

    ptr = (uint8_t *)data;
    //H5LogMsg("count %d rx_state %d rx_count %ld", count, h5->rx_state, h5->rx_count);
    while (count) {
        if (h5->rx_count) {
            if (*ptr == 0xc0) {
                ALOGE("short h5 packet");
                skb_free(&h5->rx_skb);
                h5->rx_state = H5_W4_PKT_START;
                h5->rx_count = 0;
            } else
                h5_unslip_one_byte(h5, *ptr);

            ptr++; count--;
            continue;
        }

        //H5LogMsg("h5_recv rx_state=%d", h5->rx_state);
        switch (h5->rx_state) {
        case H5_W4_HDR:
            // check header checksum. see Core Spec V4 "3-wire uart" page 67
            skb_data = skb_get_data(h5->rx_skb);
            hdr = (uint8_t *)skb_data;

            if ((0xff & (uint8_t) ~ (skb_data[0] + skb_data[1] +
                                   skb_data[2])) != skb_data[3]) {
                ALOGE("h5 hdr checksum error!!!");
                skb_free(&h5->rx_skb);
                h5->rx_state = H5_W4_PKT_DELIMITER;
                h5->rx_count = 0;
                continue;
            }

            if (H5_HDR_RELIABLE(hdr)
                && (H5_HDR_SEQ(hdr) != h5->rxseq_txack)) {
                ALOGE("Out-of-order packet arrived, got(%u)expected(%u)",
                   H5_HDR_SEQ(hdr), h5->rxseq_txack);
                h5->is_txack_req = 1;
                h5_wake_up();

                skb_free(&h5->rx_skb);
                h5->rx_state = H5_W4_PKT_DELIMITER;
                h5->rx_count = 0;

                continue;
            }
            h5->rx_state = H5_W4_DATA;
            //payload length: May be 0
            h5->rx_count = H5_HDR_LEN(hdr);
            continue;
        case H5_W4_DATA:
            hdr = (uint8_t *)skb_get_data(h5->rx_skb);
            if (H5_HDR_CRC(hdr)) { // pkt with crc /
                h5->rx_state = H5_W4_CRC;
                h5->rx_count = 2;
            } else {
                h5_complete_rx_pkt(h5); //Send ACK
                complete_packet = true;
                H5LogMsg("--------> H5_W4_DATA ACK\n");
            }
            continue;

        case H5_W4_CRC:
            if (bit_rev16(h5->message_crc) != h5_get_crc(h5)) {
                ALOGE("Checksum failed, computed(%04x)received(%04x)",
                    bit_rev16(h5->message_crc), h5_get_crc(h5));
                skb_free(&h5->rx_skb);
                h5->rx_state = H5_W4_PKT_DELIMITER;
                h5->rx_count = 0;
                continue;
            }
            skb_trim(h5->rx_skb, skb_get_data_length(h5->rx_skb) - 2);
            h5_complete_rx_pkt(h5);
            complete_packet = true;
            continue;

        case H5_W4_PKT_DELIMITER:
            switch (*ptr) {
                case 0xc0:
                    h5->rx_state = H5_W4_PKT_START;
                    break;
                default:
                    break;
            }
            ptr++;
            count--;
            break;

        case H5_W4_PKT_START:
            switch (*ptr) {
            case 0xc0:
                ptr++; count--;
                break;
            default:
                h5->rx_state = H5_W4_HDR;
                h5->rx_count = 4;
                h5->rx_esc_state = H5_ESCSTATE_NOESC;
                H5_CRC_INIT(h5->message_crc);

                // Do not increment ptr or decrement count
                // Allocate packet. Max len of a H5 pkt=
                // 0xFFF (payload) +4 (header) +2 (crc)
                h5->rx_skb = skb_alloc(0x1005);
                if (!h5->rx_skb) {
                    h5->rx_state = H5_W4_PKT_DELIMITER;
                    h5->rx_count = 0;
                    return false;
                }
                break;
            }
            break;
        }
    }
    return complete_packet;
}

/******************************************************************************
**  Static functions
******************************************************************************/
static void data_ready_cb_thread(void *arg)
{
    AIC_UNUSED(arg);
    sk_buff *skb;
    uint8_t pkt_type = 0;
    unsigned int total_length = 0;

    H5LogMsg("data_ready_cb_thread started");

    prctl(PR_SET_NAME, (unsigned long)"data_ready_cb_thread", 0, 0, 0);

    while (h5_data_ready_running) {
        pthread_mutex_lock(&aic_h5.data_mutex);
        while (h5_data_ready_running && (skb_queue_get_length(aic_h5.recv_data) == 0)) {
            pthread_cond_wait(&aic_h5.data_cond, &aic_h5.data_mutex);
        }
        pthread_mutex_unlock(&aic_h5.data_mutex);

        if (h5_data_ready_running && (skb = skb_dequeue_head(aic_h5.recv_data)) != NULL) {
            aic_h5.data_skb = skb;
        } else
            continue;

        pkt_type = skb_get_pkt_type(aic_h5.data_skb);
        total_length = skb_get_data_length(aic_h5.data_skb);
        h5_int_hal_callbacks->h5_data_ready_cb(pkt_type, total_length);
    }

    H5LogMsg("data_ready_cb_thread exiting");
    pthread_exit(NULL);
}

static void data_retransfer_thread(void *arg)
{
    AIC_UNUSED(arg);
    uint16_t events;
    uint16_t data_retrans_counts = DATA_RETRANS_COUNT;

    H5LogMsg("data_retransfer_thread started");

    prctl(PR_SET_NAME, (unsigned long)"data_retransfer_thread", 0, 0, 0);

    while (h5_retransfer_running) {
        pthread_mutex_lock(&aic_h5.mutex);
        while (h5_ready_events == 0) {
            pthread_cond_wait(&aic_h5.cond, &aic_h5.mutex);
        }
        events = h5_ready_events;
        h5_ready_events = 0;
        pthread_mutex_unlock(&aic_h5.mutex);

        if (events & H5_EVENT_RX) {
            sk_buff *skb;
            ALOGE("retransmitting (%u) pkts, retransfer count(%d)", skb_queue_get_length(aic_h5.unack), aic_h5.data_retrans_count);
            if (h5_init_datatrans_flag == 0)
                data_retrans_counts = DATA_RETRANS_COUNT;
            else
                data_retrans_counts = BT_INIT_DATA_RETRANS_COUNT;

            if (aic_h5.data_retrans_count < data_retrans_counts) {
                pthread_mutex_lock(&h5_wakeup_mutex);
                while ((skb = skb_dequeue_tail(aic_h5.unack)) != NULL) {
#if H5_TRACE_DATA_ENABLE
                    uint32_t data_len = skb_get_data_length(skb);
                    uint8_t* pdata = skb_get_data(skb);
                    if (data_len>16)
                        data_len=16;

                    for (i = 0 ; i < data_len; i++)
                        ALOGE("0x%02X", pdata[i]);
#endif
                    aic_h5.msgq_txseq = (aic_h5.msgq_txseq - 1) & 0x07;
                    skb_queue_head(aic_h5.rel, skb);

                }
                pthread_mutex_unlock(&h5_wakeup_mutex);
                aic_h5.data_retrans_count++;
                h5_wake_up();

            } else {
                //do not put packet to rel queue, and do not send
                //Kill bluetooth
                aicbt_h5_send_hw_error();
                //kill(getpid(), SIGKILL);
            }
        } else {
            if (events & H5_EVENT_EXIT)
                break;
        }
    }

    H5LogMsg("data_retransfer_thread exiting");
    pthread_exit(NULL);

}

void h5_retransfer_signal_event(uint16_t event)
{
    pthread_mutex_lock(&aic_h5.mutex);
    h5_ready_events |= event;
    pthread_cond_signal(&aic_h5.cond);
    pthread_mutex_unlock(&aic_h5.mutex);
}

static int create_data_retransfer_thread()
{
    //struct sched_param param;
    //int policy;

    pthread_attr_t thread_attr;


    if (h5_retransfer_running) {
        ALOGW("create_data_retransfer_thread has been called repeatedly without calling cleanup ?");
    }

    h5_retransfer_running = 1;
    h5_ready_events = 0;

    pthread_attr_init(&thread_attr);
    pthread_mutex_init(&aic_h5.mutex, NULL);
    pthread_cond_init(&aic_h5.cond, NULL);

    if (pthread_create(&aic_h5.thread_data_retrans, &thread_attr, \
               (void*)data_retransfer_thread, NULL) != 0) {
        ALOGE("pthread_create thread_data_retrans failed!");
        h5_retransfer_running = 0;
        return -1 ;
    }
/*
    if (pthread_getschedparam(hc_cb.worker_thread, &policy, &param) == 0) {
        policy = BTHC_LINUX_BASE_POLICY;

#if (BTHC_LINUX_BASE_POLICY!=SCHED_NORMAL)
        param.sched_priority = BTHC_MAIN_THREAD_PRIORITY;
#endif
        result = pthread_setschedparam(hc_cb.worker_thread, policy, &param);
        if (result != 0) {
            ALOGW("create_data_retransfer_thread pthread_setschedparam failed (%s)", \
            strerror(result));
        }
    }
*/
    return 0;

}


static int create_data_ready_cb_thread()
{
    //struct sched_param param;
    //int policy;

    pthread_attr_t thread_attr;


    if (h5_data_ready_running) {
        ALOGW("create_data_ready_cb_thread has been called repeatedly without calling cleanup ?");
    }

    h5_data_ready_running = 1;

    pthread_attr_init(&thread_attr);
    pthread_mutex_init(&aic_h5.data_mutex, NULL);
    pthread_cond_init(&aic_h5.data_cond, NULL);

    if (pthread_create(&aic_h5.thread_data_ready_cb, &thread_attr, \
               (void*)data_ready_cb_thread, NULL) != 0) {
        ALOGE("pthread_create thread_data_ready_cb failed!");
        h5_data_ready_running = 0;
        return -1 ;
    }
    return 0;

}


/*****************************************************************************
**   HCI H5 INTERFACE FUNCTIONS
*****************************************************************************/

/*******************************************************************************
**
** Function        hci_h5_init
**
** Description     Initialize H5 module
**
** Returns         None
**
*******************************************************************************/
void hci_h5_int_init(hci_h5_callbacks_t* h5_callbacks)
{
    H5LogMsg("hci_h5_int_init");

    h5_int_hal_callbacks = h5_callbacks;
    memset(&aic_h5, 0, sizeof(tHCI_H5_CB));


    /* Per HCI spec., always starts with 1 */
    num_hci_cmd_pkts = 1;

    h5_alloc_data_retrans_timer();
    h5_alloc_sync_retrans_timer();
    h5_alloc_conf_retrans_timer();
    h5_alloc_wait_controller_baudrate_ready_timer();
    h5_alloc_hw_init_ready_timer();

    aic_h5.thread_data_retrans = -1;

    aic_h5.recv_data = RtbQueueInit();

    if (create_data_ready_cb_thread() != 0)
        ALOGE("H5 create_data_ready_cb_thread failed");

    if (create_data_retransfer_thread() != 0)
        ALOGE("H5 create_data_retransfer_thread failed");


    aic_h5.unack = RtbQueueInit();
    aic_h5.rel = RtbQueueInit();
    aic_h5.unrel = RtbQueueInit();

    aic_h5.rx_state = H5_W4_PKT_DELIMITER;
    aic_h5.rx_esc_state = H5_ESCSTATE_NOESC;

    h5_init_datatrans_flag = 1;

}

/*******************************************************************************
**
** Function        hci_h5_cleanup
**
** Description     Clean H5 module
**
** Returns         None
**
*******************************************************************************/
void hci_h5_cleanup(void)
{
    H5LogMsg("hci_h5_cleanup");
    //uint8_t try_cnt=10;
    int result;

    aic_h5.cleanuping = 1;


    //btsnoop_cleanup();

    h5_free_data_retrans_timer();
    h5_free_sync_retrans_timer();
    h5_free_conf_retrans_timer();
    h5_free_wait_controller_baudrate_ready_timer();
    h5_free_hw_init_ready_timer();

    if (h5_data_ready_running) {
        h5_data_ready_running = 0;
        pthread_mutex_lock(&aic_h5.data_mutex);
        pthread_cond_signal(&aic_h5.data_cond);
        pthread_mutex_unlock(&aic_h5.data_mutex);
        if ((result = pthread_join(aic_h5.thread_data_ready_cb, NULL)) < 0)
            ALOGE("H5 thread_data_ready_cb pthread_join() FAILED result:%d", result);
    }

    if (h5_retransfer_running) {
        h5_retransfer_running = 0;
        h5_retransfer_signal_event(H5_EVENT_EXIT);
        if ((result = pthread_join(aic_h5.thread_data_retrans, NULL)) < 0)
            ALOGE("H5 pthread_join() FAILED result:%d", result);
    }

    //ms_delay(200);

    pthread_mutex_destroy(&aic_h5.mutex);
    pthread_mutex_destroy(&aic_h5.data_mutex);
    pthread_cond_destroy(&aic_h5.cond);
    pthread_cond_destroy(&aic_h5.data_cond);

    RtbQueueFree(aic_h5.unack);
    RtbQueueFree(aic_h5.rel);
    RtbQueueFree(aic_h5.unrel);

    h5_int_hal_callbacks = NULL;
    aic_h5.internal_skb = NULL;
}

/*******************************************************************************
**
** Function        hci_h5_receive_msg
**
** Description     Construct HCI EVENT/ACL packets and send them to stack once
**                 complete packet has been received.
**
** Returns         Number of read bytes
**
*******************************************************************************/
bool  hci_h5_receive_msg(uint8_t *byte, uint16_t length)
{
    bool status = false;
    //H5LogMsg("hci_h5_receive_msg byte:%d",h5_byte);
    status = h5_recv(&aic_h5, byte, length);
    return status;
}


size_t  hci_h5_int_read_data(uint8_t * data_buffer, size_t max_size)
{
    H5LogMsg("hci_h5_int_read_data need_size = %d", max_size);
    if (!aic_h5.data_skb) {
        ALOGE("hci_h5_int_read_data, there is no data to read for this packet!");
        return -1;
    }
    sk_buff * skb_complete_pkt = aic_h5.data_skb;
    uint8_t *data = skb_get_data(skb_complete_pkt);
    uint32_t data_len = skb_get_data_length(skb_complete_pkt);

    H5LogMsg("hci_h5_int_read_data length = %d, need_size = %d", data_len, max_size);
    if (data_len <= max_size) {
        memcpy(data_buffer, data, data_len);
        skb_free(&aic_h5.data_skb);
        aic_h5.data_skb = NULL;
        return data_len;
    } else {
        memcpy(data_buffer, data, max_size);
        skb_pull(aic_h5.data_skb, max_size);
        return max_size;
    }
}


/*******************************************************************************
**
** Function        hci_h5_send_cmd
**
** Description     get cmd data from hal and send cmd
**
**
** Returns          bytes send
**
*******************************************************************************/
uint16_t hci_h5_send_cmd(serial_data_type_t type, uint8_t *data, uint16_t length)
{
    sk_buff * skb = NULL;
    uint16_t bytes_to_send, opcode;

    if (type != DATA_TYPE_COMMAND) {
        ALOGE("%s Receive wrong type type : %d", __func__, type);
        return -1;
    }

    skb = skb_alloc_and_init(type, data, length);
    if (!skb) {
        ALOGE("send cmd skb_alloc_and_init fail!");
        return -1;
    }

    h5_enqueue(skb);

    num_hci_cmd_pkts--;

    /* If this is an internal Cmd packet, the layer_specific field would
         * have stored with the opcode of HCI command.
         * Retrieve the opcode from the Cmd packet.
         */
    STREAM_TO_UINT16(opcode, data);
    H5LogMsg("HCI Command opcode(0x%04X)", opcode);
    if (opcode == 0x0c03) {
        H5LogMsg("RX HCI RESET Command, stop hw init timer");
        h5_stop_hw_init_ready_timer();
    }
    if (opcode == HCI_WRITE_LOOPBACK_MODE)
        loopbackmode = 0x01;

    bytes_to_send = h5_wake_up();
    return length;
}

/*******************************************************************************
**
** Function        hci_h5_send_acl_data
**
** Description     get cmd data from hal and send cmd
**
**
** Returns          bytes send
**
*******************************************************************************/
uint16_t hci_h5_send_acl_data(serial_data_type_t type, uint8_t *data, uint16_t length)
{
    uint16_t bytes_to_send;//, lay_spec;
    sk_buff * skb = NULL;
    if (loopbackmode == 1) {
        acl_len = length;
        if (!acl_pack)
            acl_pack = malloc(2050);
        if (acl_len > 2050)
            acl_len = 2050;
        memcpy(acl_pack,data,acl_len);
        start_loopacktimer();
        return length;
    }

    skb = skb_alloc_and_init(type, data, length);
    if (!skb) {
        ALOGE("hci_h5_send_acl_data, alloc skb buffer fail!");
        return -1;
    }

    h5_enqueue(skb);

    bytes_to_send = h5_wake_up();
    return length;
}

/*******************************************************************************
**
** Function        hci_h5_send_sco_data
**
** Description     get sco data from hal and send sco data
**
**
** Returns          bytes send
**
*******************************************************************************/
uint16_t hci_h5_send_sco_data(serial_data_type_t type, uint8_t *data, uint16_t length)
{
    sk_buff * skb = NULL;
    uint16_t bytes_to_send;//, lay_spec;

    skb = skb_alloc_and_init(type, data, length);
    if (!skb) {
        ALOGE("send sco data skb_alloc_and_init fail!");
        return -1;
    }

    h5_enqueue(skb);

    bytes_to_send = h5_wake_up();
    return length;
}


/*******************************************************************************
**
** Function        hci_h5_send_sync_cmd
**
** Description     Place the internal commands (issued internally by vendor lib)
**                 in the tx_q.
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
uint8_t hci_h5_send_sync_cmd(uint16_t opcode, uint8_t *p_buf, uint16_t length)
{
    if (p_buf != NULL) {
        H5LogMsg("hci_h5_send_sync_cmd buf is not null");
    }

    if (aic_h5.link_estab_state == H5_UNINITIALIZED) {
        if (opcode == HCI_VSC_H5_INIT) {
            h5_start_hw_init_ready_timer();
            hci_h5_send_sync_req();
            h5_start_sync_retrans_timer();
        }
    } else if(aic_h5.link_estab_state == H5_ACTIVE) {
        H5LogMsg("hci_h5_send_sync_cmd(0x%x), link_estab_state = %d, length = %d", opcode, aic_h5.link_estab_state, length);
        return false;
    }

    return true;
}

/***
    Timer related functions
*/
static timer_t OsAllocateTimer(tTIMER_HANDLE_CBACK timer_callback)
{
    struct sigevent sigev;
    timer_t timerid;

    memset(&sigev, 0, sizeof(struct sigevent));
    // Create the POSIX timer to generate signo
    sigev.sigev_notify = SIGEV_THREAD;
    //sigev.sigev_notify_thread_id = syscall(__NR_gettid);
    sigev.sigev_notify_function = timer_callback;
    sigev.sigev_value.sival_ptr = &timerid;

    ALOGE("OsAllocateTimer aic_parse sigev.sigev_notify_thread_id = syscall(__NR_gettid)!");
    //Create the Timer using timer_create signal

    if (timer_create(CLOCK_REALTIME, &sigev, &timerid) == 0) {
        return timerid;
    } else {
        ALOGE("timer_create error!");
        return (timer_t)-1;
    }
}

int OsFreeTimer(timer_t timerid)
{
    int ret = 0;
    ret = timer_delete(timerid);
    if (ret != 0)
        ALOGE("timer_delete fail with errno(%d)", errno);

    return ret;
}


static int OsStartTimer(timer_t timerid, int msec, int mode)
{
    struct itimerspec itval;

    itval.it_value.tv_sec = msec / 1000;
    itval.it_value.tv_nsec = (long)(msec % 1000) * (1000000L);

    if (mode == 1) {
        itval.it_interval.tv_sec    = itval.it_value.tv_sec;
        itval.it_interval.tv_nsec = itval.it_value.tv_nsec;
    } else {
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = 0;
    }

    //Set the Timer when to expire through timer_settime

    if (timer_settime(timerid, 0, &itval, NULL) != 0) {
        ALOGE("time_settime error!");
        return -1;
    }

    return 0;

}

static int OsStopTimer(timer_t timerid)
{
    return OsStartTimer(timerid, 0, 0);
}

static void h5_retransfer_timeout_handler(union sigval sigev_value)
{
    AIC_UNUSED(sigev_value);
    H5LogMsg("h5_retransfer_timeout_handler");
    if (aic_h5.cleanuping) {
        ALOGE("h5_retransfer_timeout_handler H5 is cleanuping, EXIT here!");
        return;
    }
    h5_retransfer_signal_event(H5_EVENT_RX);
}

static void h5_sync_retrans_timeout_handler(union sigval sigev_value)
{
    AIC_UNUSED(sigev_value);
    H5LogMsg("h5_sync_retrans_timeout_handler");
    if (aic_h5.cleanuping) {
        ALOGE("h5_sync_retrans_timeout_handler H5 is cleanuping, EXIT here!");
        return;
    }

    if (aic_h5.sync_retrans_count < SYNC_RETRANS_COUNT) {
        hci_h5_send_sync_req();
        aic_h5.sync_retrans_count ++;
    } else {
        if (aic_h5.link_estab_state == H5_UNINITIALIZED) {
            aic_notify_hw_h5_init_result(0x03);
        }
        h5_stop_sync_retrans_timer();
    }
}

static void h5_conf_retrans_timeout_handler(union sigval sigev_value)
{
    AIC_UNUSED(sigev_value);
    H5LogMsg("h5_conf_retrans_timeout_handler");
    if (aic_h5.cleanuping) {
        ALOGE("h5_conf_retrans_timeout_handler H5 is cleanuping, EXIT here!");
        return;
    }

    H5LogMsg("Wait H5 Conf Resp timeout, %d times", aic_h5.conf_retrans_count);
    if (aic_h5.conf_retrans_count < CONF_RETRANS_COUNT) {
        hci_h5_send_conf_req();
        aic_h5.conf_retrans_count++;
    } else {
        if (aic_h5.link_estab_state != H5_ACTIVE) {
            aic_notify_hw_h5_init_result(0x03);
        }
        h5_stop_conf_retrans_timer();
    }
}

static void h5_wait_controller_baudrate_ready_timeout_handler(union sigval sigev_value)
{
    AIC_UNUSED(sigev_value);
    H5LogMsg("h5_wait_ct_baundrate_ready_timeout_handler");
    if (aic_h5.cleanuping) {
        ALOGE("h5_wait_controller_baudrate_ready_timeout_handler H5 is cleanuping, EXIT here!");
        if (aic_h5.internal_skb)
            skb_free(&aic_h5.internal_skb);
        return;
    }
    H5LogMsg("No Controller retransfer, baudrate of controller ready");
    pthread_mutex_lock(&aic_h5.data_mutex);
    skb_queue_tail(aic_h5.recv_data, aic_h5.internal_skb);
    pthread_cond_signal(&aic_h5.data_cond);
    pthread_mutex_unlock(&aic_h5.data_mutex);

    aic_h5.internal_skb = NULL;
}

static void h5_hw_init_ready_timeout_handler(union sigval sigev_value)
{
    AIC_UNUSED(sigev_value);
    H5LogMsg("h5_hw_init_ready_timeout_handler");
    if (aic_h5.cleanuping) {
        ALOGE("H5 is cleanuping, EXIT here!");
        return;
    }
    H5LogMsg("TIMER_H5_HW_INIT_READY timeout, kill restart BT");
    //kill(getpid(), SIGKILL);
}

/*
** h5 data retrans timer functions
*/
int h5_alloc_data_retrans_timer()
{
    // Create and set the timer when to expire
    aic_h5.timer_data_retrans = OsAllocateTimer(h5_retransfer_timeout_handler);

    return 0;
}

int h5_free_data_retrans_timer()
{
    return OsFreeTimer(aic_h5.timer_data_retrans);
}

int h5_start_data_retrans_timer()
{
    if (h5_init_datatrans_flag == 0)
        return OsStartTimer(aic_h5.timer_data_retrans, DATA_RETRANS_TIMEOUT_VALUE, 0);
    else
        return OsStartTimer(aic_h5.timer_data_retrans, BT_INIT_DATA_RETRANS_TIMEOUT_VALUE, 0);
}

int h5_stop_data_retrans_timer()
{
    return OsStopTimer(aic_h5.timer_data_retrans);
}

/*
** h5 sync retrans timer functions
*/
int h5_alloc_sync_retrans_timer()
{
    // Create and set the timer when to expire
    aic_h5.timer_sync_retrans = OsAllocateTimer(h5_sync_retrans_timeout_handler);

    return 0;
}

int h5_free_sync_retrans_timer()
{
    return OsFreeTimer(aic_h5.timer_sync_retrans);
}


int h5_start_sync_retrans_timer()
{
    return OsStartTimer(aic_h5.timer_sync_retrans, SYNC_RETRANS_TIMEOUT_VALUE, 1);
}

int h5_stop_sync_retrans_timer()
{
    return OsStopTimer(aic_h5.timer_sync_retrans);
}


/*
** h5 config retrans timer functions
*/
int h5_alloc_conf_retrans_timer()
{
    // Create and set the timer when to expire
    aic_h5.timer_conf_retrans = OsAllocateTimer(h5_conf_retrans_timeout_handler);

    return 0;
}

int h5_free_conf_retrans_timer()
{
    return OsFreeTimer(aic_h5.timer_conf_retrans);
}


int h5_start_conf_retrans_timer()
{
    return OsStartTimer(aic_h5.timer_conf_retrans, CONF_RETRANS_TIMEOUT_VALUE, 1);
}

int h5_stop_conf_retrans_timer()
{
    return OsStopTimer(aic_h5.timer_conf_retrans);
}


/*
** h5 wait controller baudrate ready timer functions
*/
int h5_alloc_wait_controller_baudrate_ready_timer()
{
    // Create and set the timer when to expire
    aic_h5.timer_wait_ct_baudrate_ready = OsAllocateTimer(h5_wait_controller_baudrate_ready_timeout_handler);

    return 0;
}

int h5_free_wait_controller_baudrate_ready_timer()
{
    return OsFreeTimer(aic_h5.timer_wait_ct_baudrate_ready);
}


int h5_start_wait_controller_baudrate_ready_timer()
{
    return OsStartTimer(aic_h5.timer_wait_ct_baudrate_ready, WAIT_CT_BAUDRATE_READY_TIMEOUT_VALUE, 0);
}

int h5_stop_wait_controller_baudrate_ready_timer()
{
    return OsStopTimer(aic_h5.timer_wait_ct_baudrate_ready);
}


/*
** h5 hw init ready timer functions
*/
int h5_alloc_hw_init_ready_timer()
{
    // Create and set the timer when to expire
    aic_h5.timer_h5_hw_init_ready = OsAllocateTimer(h5_hw_init_ready_timeout_handler);
    return 0;
}

int h5_free_hw_init_ready_timer()
{
    return OsFreeTimer(aic_h5.timer_h5_hw_init_ready);
}

int h5_start_hw_init_ready_timer()
{
    return OsStartTimer(aic_h5.timer_h5_hw_init_ready, H5_HW_INIT_READY_TIMEOUT_VALUE, 0);
}

int h5_stop_hw_init_ready_timer()
{
    return OsStopTimer(aic_h5.timer_h5_hw_init_ready);
}


/******************************************************************************
**  HCI H5 Services interface table
******************************************************************************/
const hci_h5_t hci_h5_int_func_table = {
    .h5_int_init        = hci_h5_int_init,
    .h5_int_cleanup     = hci_h5_cleanup,
    .h5_send_cmd        = hci_h5_send_cmd,
    .h5_send_sync_cmd   = hci_h5_send_sync_cmd,
    .h5_send_acl_data   = hci_h5_send_acl_data,
    .h5_send_sco_data   = hci_h5_send_sco_data,
    .h5_recv_msg        = hci_h5_receive_msg,
    .h5_int_read_data   = hci_h5_int_read_data,
};

const hci_h5_t *hci_get_h5_int_interface(void) {
    return &hci_h5_int_func_table;
}


