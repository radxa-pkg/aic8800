/******************************************************************************
 *
 *  Copyright (C) 2019-2029 AIC Corporation
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
 *  This file contains sync message over UIPC
 *
 ******************************************************************************/

#ifndef UIPC_MSG_H
#define UIPC_MSG_H

#include "bt_types.h"

/****************************************************************************/
/*                            UIPC version number: 1.0                      */
/****************************************************************************/
#define UIPC_VERSION_MAJOR  0x0001
#define UIPC_VERSION_MINOR  0x0000


/********************************

    UIPC Management Messages

********************************/

/* tUIPC_STATUS codes*/
enum
{
    UIPC_STATUS_SUCCESS,
    UIPC_STATUS_FAIL
};
typedef uint8_t tUIPC_STATUS;

/* op_code */
#define UIPC_OPEN_REQ                   0x00
#define UIPC_OPEN_RSP                   0x01
#define UIPC_CLOSE_REQ                  0x02
#define UIPC_CLOSE_RSP                  0x03

#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)     /* set alignment to 1 byte boundary to allow for offset mappings */

/* Structure of UIPC_OPEN_REQ message */
typedef struct
{
    uint8_t               opcode;         /* UIPC_OPEN_REQ */
} tUIPC_OPEN_REQ;
#define UIPC_OPEN_REQ_MSGLEN        (1)

/* Structure of UIPC_OPEN_RSP message */
typedef struct
{
    uint8_t               opcode;         /* UIPC_OPEN_RESP */
    tUIPC_STATUS        status;         /* UIPC_STATUS */
    uint16_t              version_major;  /* UIPC_VERSION_MAJOR */
    uint16_t              version_minor;  /* UIPC_VERSION_MINOR */
    uint8_t               num_streams;    /* Number of simultaneous streams supported by the light stack */
} tUIPC_OPEN_RSP;
#define UIPC_OPEN_RSP_MSGLEN        (7)

/* Structure of UIPC_CLOSE_REQ message */
typedef struct t_uipc_close_req
{
    uint8_t               opcode;         /* UIPC_CLOSE_REQ */
} tUIPC_CLOSE_REQ;
#define UIPC_CLOSE_REQ_MSGLEN       (1)

/* Structure of UIPC_CLOSE_RSP message, only for BTC, full stack may ignore it */
typedef struct t_uipc_close_rsp
{
    uint8_t               opcode;         /* UIPC_CLOSE_RSP */
} tUIPC_CLOSE_RSP;
#define UIPC_CLOSE_RSP_MSGLEN       (1)

/* UIPC management message structures */
typedef union
{
    uint8_t               opcode;
    tUIPC_OPEN_REQ      open_req;
    tUIPC_OPEN_RSP      open_resp;
    tUIPC_CLOSE_REQ     close_req;
} tUIPC_MSG;

#define UIPC_MGMT_MSG_MAXLEN    (sizeof(tUIPC_MSG))

#define IPC_LOG_MSG_LEN  100
typedef struct t_uipc_log_msg
{
    uint32_t              trace_set_mask;
    uint8_t               msg[IPC_LOG_MSG_LEN];
} tUIPC_LOG_MSG;
#define UIPC_LOG_MSGLEN       (IPC_LOG_MSG_LEN + 4)

/********************************

    H5 Sync Message

********************************/

/* op_code */
#define SLIP_SYNC_TO_LITE_REQ        0
#define SLIP_SYNC_TO_LITE_RESP       1
#define SLIP_SYNC_TO_FULL_REQ        2
#define SLIP_SYNC_TO_FULL_RESP       3
#define SLIP_SYNC_NOTIFY             4

/* status */
#define SLIP_SYNC_SUCCESS            0
#define SLIP_SYNC_FAILURE            1

typedef struct
{
    uint8_t       op_code;
    uint8_t       status;
    uint16_t      acl_pkt_size;
    uint8_t       state;
    uint8_t       lp_state;           /* Low Power state */
    uint8_t       next_seqno;         /* next send seq */
    uint8_t       ack;                /* next ack seq, expected seq from peer */
    uint8_t       sent_ack;           /* last sent ack */
    uint8_t       sliding_window_size;/* window size */
    bool          oof_flow_control;   /* Out of Frame SW Flow Control */
    bool         data_integrity_type;/* Level of Data Integrity Check */
    uint8_t       rx_state;           /* rx state for incoming packet processing */
} tSLIP_SYNC_INFO;

/********************************

    L2CAP Sync Message

********************************/

/* op_code */
#define L2C_SYNC_TO_LITE_REQ        0
#define L2C_SYNC_TO_LITE_RESP       1
#define L2C_REMOVE_TO_LITE_REQ      2
#define L2C_REMOVE_TO_LITE_RESP     3
#define L2C_FLUSH_TO_FULL_IND       4

/* status */
#define L2C_SYNC_SUCCESS            0
#define L2C_SYNC_FAILURE            1

typedef struct t_l2c_stream_info
{
    uint16_t  local_cid;          /* Local CID                        */
    uint16_t  remote_cid;         /* Remote CID                       */
    uint16_t  out_mtu;            /* Max MTU we will send             */
    uint16_t  handle;             /* The handle used with LM          */
    uint16_t  link_xmit_quota;    /* Num outstanding pkts allowed     */
    bool      is_flushable;       /* TRUE if flushable channel        */
} tL2C_STREAM_INFO;

typedef struct t_l2c_sync_to_lite_req
{
    uint8_t   op_code;                       /* L2C_SYNC_TO_LITE_REQ */
    uint16_t  light_xmit_quota;              /* Total quota for light stack    */
    uint16_t  acl_data_size;                 /* Max ACL data size across HCI transport    */
    uint16_t  non_flushable_pbf;             /* L2CAP_PKT_START_NON_FLUSHABLE if controller supports */
                                           /* Otherwise, L2CAP_PKT_START */
    uint8_t   multi_av_data_cong_start;      /* Multi-AV queue size to start congestion */
    uint8_t   multi_av_data_cong_end;        /* Multi-AV queue size to end congestion */
    uint8_t   multi_av_data_cong_discard;    /* Multi-AV queue size to discard */
    uint8_t   num_stream;
    tL2C_STREAM_INFO stream;
} tL2C_SYNC_TO_LITE_REQ;

typedef struct t_l2c_sync_to_lite_resp_stream
{
    uint16_t  lcid;
    uint8_t   status;
} tL2C_SYNC_TO_LITE_RESP_STREAM;


typedef struct t_l2c_sync_to_lite_resp
{
    uint8_t   op_code;                       /* L2C_SYNC_TO_LITE_RESP */
    uint16_t  light_xmit_unacked;            /* unacked packet more than quota in light stack    */
    uint8_t   num_stream;
    tL2C_SYNC_TO_LITE_RESP_STREAM stream;
} tL2C_SYNC_TO_LITE_RESP;

typedef struct t_l2c_remove_to_lite_req
{
    uint8_t   op_code;                       /* L2C_REMOVE_TO_LITE_REQ */
    uint16_t  light_xmit_quota;              /* Total quota for light stack    */
    uint8_t   num_stream;
    uint16_t  lcid;
} tL2C_REMOVE_TO_LITE_REQ;

typedef tL2C_SYNC_TO_LITE_RESP  tL2C_REMOVE_TO_LITE_RESP;
typedef tL2C_REMOVE_TO_LITE_REQ tL2C_FLUSH_TO_FULL_IND;

typedef union t_l2c_sync_msg
{
    uint8_t                       op_code;
    tL2C_SYNC_TO_LITE_REQ       sync_req;
    tL2C_SYNC_TO_LITE_RESP      sync_resp;
    tL2C_REMOVE_TO_LITE_REQ     remove_req;
    tL2C_REMOVE_TO_LITE_RESP    remove_resp;
    tL2C_FLUSH_TO_FULL_IND      flush_ind;
} tL2C_SYNC_MSG;

/********************************

    AVDTP Sync Message

********************************/

/* op_code */
#define AVDT_SYNC_TO_LITE_REQ        0
#define AVDT_SYNC_TO_LITE_RESP       1
#define AVDT_RESYNC_TO_LITE_REQ      2
#define AVDT_RESYNC_TO_LITE_RESP     3
#define AVDT_SYNC_TO_FULL_REQ        4
#define AVDT_SYNC_TO_FULL_RESP       5
#define AVDT_REMOVE_TO_LITE_REQ      6
#define AVDT_REMOVE_TO_LITE_RESP     7
#define AVDT_SYNC_TO_BTC_LITE_REQ    8
#define AVDT_SYNC_TO_BTC_LITE_RESP   9

/* status */
#define AVDT_SYNC_SUCCESS            0
#define AVDT_SYNC_FAILURE            1

typedef struct
{
    uint16_t  lcid;
    uint32_t  ssrc;
} tAVDT_SYNC_TO_BTC_LITE_REQ_STREAM;

typedef struct
{
    uint8_t   opcode;                     /* AVDT_SYNC_TO_BTC_LITE_REQ */
    uint8_t   num_stream;
    tAVDT_SYNC_TO_BTC_LITE_REQ_STREAM  stream;
} tAVDT_SYNC_TO_BTC_LITE_REQ;

typedef struct
{
    uint8_t   opcode;                     /* AVDT_SYNC_TO_BTC_LITE_RESP */
    uint8_t   status;
} tAVDT_SYNC_TO_BTC_LITE_RESP;

typedef struct t_avdt_scb_sync_info
{
    uint8_t   handle;         /* SCB handle */
    BD_ADDR peer_addr;      /* BD address of peer */
    uint16_t  local_cid;      /* Local CID                        */
    uint16_t  peer_mtu;       /* L2CAP mtu of the peer device */
    uint8_t   mux_tsid_media; /* TSID for media transport session */
    uint16_t  media_seq;      /* media packet sequence number */
} tAVDT_SCB_SYNC_INFO;

typedef struct t_avdt_sync_info
{
    uint8_t   op_code;
    uint8_t   status;

    tAVDT_SCB_SYNC_INFO scb_info;

} tAVDT_SYNC_INFO;

typedef union t_avdt_sync_msg
{
    uint8_t                       op_code;
    tAVDT_SYNC_INFO             sync_info;
    tAVDT_SYNC_TO_BTC_LITE_REQ  btc_sync_req;
    tAVDT_SYNC_TO_BTC_LITE_RESP btc_sync_resp;
} tAVDT_SYNC_MSG;

/********************************

    BTA AV Sync Message

********************************/

/* op_code for MM light stack */
#define BTA_AV_SYNC_TO_LITE_REQ             0
#define BTA_AV_SYNC_TO_LITE_RESP            1
#define BTA_AV_STR_START_TO_LITE_REQ        2
#define BTA_AV_STR_START_TO_LITE_RESP       3
#define BTA_AV_STR_STOP_TO_LITE_REQ         4
#define BTA_AV_STR_STOP_TO_LITE_RESP        5
#define BTA_AV_STR_CLEANUP_TO_LITE_REQ      6
#define BTA_AV_STR_CLEANUP_TO_LITE_RESP     7
#define BTA_AV_STR_SUSPEND_TO_LITE_REQ      8
#define BTA_AV_STR_SUSPEND_TO_LITE_RESP     9
#define BTA_AV_SYNC_ERROR_RESP              10

/* op_code for BTC light stack */
#define A2DP_START_REQ                      11
#define A2DP_START_RESP                     12
#define A2DP_STOP_REQ                       13
#define A2DP_STOP_RESP                      14
#define A2DP_CLEANUP_REQ                    15
#define A2DP_CLEANUP_RESP                   16
#define A2DP_SUSPEND_REQ                    17
#define A2DP_SUSPEND_RESP                   18

#define A2DP_JITTER_DONE_IND                41  /* For BTSNK */

#define AUDIO_CODEC_CONFIG_REQ              19
#define AUDIO_CODEC_CONFIG_RESP             20
#define AUDIO_CODEC_SET_BITRATE_REQ         21
#define AUDIO_CODEC_FLUSH_REQ               22
#define AUDIO_ROUTE_CONFIG_REQ              23
#define AUDIO_ROUTE_CONFIG_RESP             24
#define AUDIO_MIX_CONFIG_REQ                25
#define AUDIO_MIX_CONFIG_RESP               26
#define AUDIO_BURST_FRAMES_IND              27
#define AUDIO_BURST_END_IND                 28
#define AUDIO_EQ_MODE_CONFIG_REQ            29
#define AUDIO_SCALE_CONFIG_REQ              30

/* For TIVO, only applicable for I2S -> DAC */
#define AUDIO_SUB_ROUTE_REQ                 51
#define AUDIO_SUB_ROUTE_RESP                52

typedef struct
{
    uint8_t   opcode;     /* A2DP_START_REQ */
    uint16_t  lcid;
    uint16_t  curr_mtu;
}tA2DP_START_REQ;

typedef struct
{
    uint8_t   opcode;     /* A2DP_STOP_REQ */
    uint16_t  lcid;
}tA2DP_STOP_REQ;

typedef struct
{
    uint8_t   opcode;     /* A2DP_SUSPEND_REQ */
    uint16_t  lcid;
}tA2DP_SUSPEND_REQ;

typedef struct
{
    uint8_t   opcode;     /* A2DP_CLEANUP_REQ */
    uint16_t  lcid;
    uint16_t  curr_mtu;
} tA2DP_CLEANUP_REQ;

typedef struct
{
    uint8_t   opcode;     /* A2DP_START_RESP, A2DP_STOP_RESP, A2DP_CLEANUP_RESP, A2DP_SUSPEND_RESP */
    uint16_t  lcid;
}tA2DP_GENERIC_RESP;

#define AUDIO_CODEC_NONE            0x0000
#define AUDIO_CODEC_SBC_ENC         0x0001
#define AUDIO_CODEC_SBC_DEC         0x0002
#define AUDIO_CODEC_MP3_ENC         0x0004
#define AUDIO_CODEC_MP3_DEC         0x0008
#define AUDIO_CODEC_AAC_ENC         0x0010
#define AUDIO_CODEC_AAC_DEC         0x0020
#define AUDIO_CODEC_AAC_PLUS_ENC    0x0040
#define AUDIO_CODEC_AAC_PLUS_DEC    0x0080
#define AUDIO_CODEC_MP2_ENC         0x0100
#define AUDIO_CODEC_MP2_DEC         0x0200
#define AUDIO_CODEC_MP2_5_ENC       0x0400
#define AUDIO_CODEC_MP2_5_DEC       0x0800

typedef uint16_t tAUDIO_CODEC_TYPE;

/* SBC CODEC Parameters */

#define CODEC_INFO_SBC_SF_16K       0x00
#define CODEC_INFO_SBC_SF_32K       0x01
#define CODEC_INFO_SBC_SF_44K       0x02
#define CODEC_INFO_SBC_SF_48K       0x03

#define CODEC_INFO_SBC_BLOCK_4      0x00
#define CODEC_INFO_SBC_BLOCK_8      0x01
#define CODEC_INFO_SBC_BLOCK_12     0x02
#define CODEC_INFO_SBC_BLOCK_16     0x03

#define CODEC_INFO_SBC_CH_MONO      0x00
#define CODEC_INFO_SBC_CH_DUAL      0x01
#define CODEC_INFO_SBC_CH_STEREO    0x02
#define CODEC_INFO_SBC_CH_JS        0x03

#define CODEC_INFO_SBC_ALLOC_LOUDNESS   0x00
#define CODEC_INFO_SBC_ALLOC_SNR        0x01

#define CODEC_INFO_SBC_SUBBAND_4    0x00
#define CODEC_INFO_SBC_SUBBAND_8    0x01

/* MPEG audio version ID */
#define CODEC_INFO_MP25_ID              0x00
#define CODEC_INFO_RESERVE              0x01
#define CODEC_INFO_MP2_ID               0x02
#define CODEC_INFO_MP3_ID               0x03

#define CODEC_INFO_MP3_PROTECTION_ON    0x00
#define CODEC_INFO_MP3_PROTECTION_OFF   0x01

#define CODEC_INFO_MP3_BR_IDX_FREE      0x00
#define CODEC_INFO_MP3_BR_IDX_32K       0x01
#define CODEC_INFO_MP3_BR_IDX_40K       0x02
#define CODEC_INFO_MP3_BR_IDX_48K       0x03
#define CODEC_INFO_MP3_BR_IDX_56K       0x04
#define CODEC_INFO_MP3_BR_IDX_64K       0x05
#define CODEC_INFO_MP3_BR_IDX_80K       0x06
#define CODEC_INFO_MP3_BR_IDX_96K       0x07
#define CODEC_INFO_MP3_BR_IDX_112K      0x08
#define CODEC_INFO_MP3_BR_IDX_128K      0x09
#define CODEC_INFO_MP3_BR_IDX_160K      0x0A
#define CODEC_INFO_MP3_BR_IDX_192K      0x0B
#define CODEC_INFO_MP3_BR_IDX_224K      0x0C
#define CODEC_INFO_MP3_BR_IDX_256K      0x0D
#define CODEC_INFO_MP3_BR_IDX_320K      0x0E

#define CODEC_INFO_MP3_SF_44K           0x00
#define CODEC_INFO_MP3_SF_48K           0x01
#define CODEC_INFO_MP3_SF_32K           0x02

#define CODEC_INFO_MP3_MODE_STEREO      0x00
#define CODEC_INFO_MP3_MODE_JS          0x01
#define CODEC_INFO_MP3_MODE_DUAL        0x02
#define CODEC_INFO_MP3_MODE_SINGLE      0x03

/* layer 3, type of joint stereo coding method (intensity and ms) */
#define CODEC_INFO_MP3_MODE_EXT_OFF_OFF 0x00
#define CODEC_INFO_MP3_MODE_EXT_ON_OFF  0x01
#define CODEC_INFO_MP3_MODE_EXT_OFF_ON  0x02
#define CODEC_INFO_MP3_MODE_EXT_ON_ON   0x03


#define CODEC_INFO_MP2_PROTECTION_ON    0x00
#define CODEC_INFO_MP2_PROTECTION_OFF   0x01

#define CODEC_INFO_MP2_BR_IDX_FREE      0x00
#define CODEC_INFO_MP2_BR_IDX_8K        0x01
#define CODEC_INFO_MP2_BR_IDX_16K       0x02
#define CODEC_INFO_MP2_BR_IDX_24K       0x03
#define CODEC_INFO_MP2_BR_IDX_32K       0x04
#define CODEC_INFO_MP2_BR_IDX_40K       0x05
#define CODEC_INFO_MP2_BR_IDX_48K       0x06
#define CODEC_INFO_MP2_BR_IDX_56K       0x07
#define CODEC_INFO_MP2_BR_IDX_64K       0x08
#define CODEC_INFO_MP2_BR_IDX_80K       0x09
#define CODEC_INFO_MP2_BR_IDX_96K       0x0A
#define CODEC_INFO_MP2_BR_IDX_112K      0x0B
#define CODEC_INFO_MP2_BR_IDX_128K      0x0C
#define CODEC_INFO_MP2_BR_IDX_144K      0x0D
#define CODEC_INFO_MP2_BR_IDX_160K      0x0E

#define CODEC_INFO_MP2_SF_22K           0x00
#define CODEC_INFO_MP2_SF_24K           0x01
#define CODEC_INFO_MP2_SF_16K           0x02

#define CODEC_INFO_MP2_MODE_STEREO      0x00
#define CODEC_INFO_MP2_MODE_JS          0x01
#define CODEC_INFO_MP2_MODE_DUAL        0x02
#define CODEC_INFO_MP2_MODE_SINGLE      0x03

/* layer 3, type of joint stereo coding method (intensity and ms) */
#define CODEC_INFO_MP2_MODE_EXT_OFF_OFF 0x00
#define CODEC_INFO_MP2_MODE_EXT_ON_OFF  0x01
#define CODEC_INFO_MP2_MODE_EXT_OFF_ON  0x02
#define CODEC_INFO_MP2_MODE_EXT_ON_ON   0x03

#define CODEC_INFO_MP2_SAMPLE_PER_FRAME     576

/* mpeg 2.5 layer 3 decoder */

#define CODEC_INFO_MP25_PROTECTION_ON   0x00
#define CODEC_INFO_MP25_PROTECTION_OFF  0x01

#define CODEC_INFO_MP25_BR_IDX_FREE     0x00
#define CODEC_INFO_MP25_BR_IDX_8K       0x01
#define CODEC_INFO_MP25_BR_IDX_16K      0x02
#define CODEC_INFO_MP25_BR_IDX_24K      0x03
#define CODEC_INFO_MP25_BR_IDX_32K      0x04
#define CODEC_INFO_MP25_BR_IDX_40K      0x05
#define CODEC_INFO_MP25_BR_IDX_48K      0x06
#define CODEC_INFO_MP25_BR_IDX_56K      0x07
#define CODEC_INFO_MP25_BR_IDX_64K      0x08
#define CODEC_INFO_MP25_BR_IDX_80K      0x09
#define CODEC_INFO_MP25_BR_IDX_96K      0x0A
#define CODEC_INFO_MP25_BR_IDX_112K     0x0B
#define CODEC_INFO_MP25_BR_IDX_128K     0x0C
#define CODEC_INFO_MP25_BR_IDX_144K     0x0D
#define CODEC_INFO_MP25_BR_IDX_160K     0x0E

#define CODEC_INFO_MP25_SF_11K          0x00
#define CODEC_INFO_MP25_SF_12K          0x01
#define CODEC_INFO_MP25_SF_8K           0x02

#define CODEC_INFO_MP25_MODE_STEREO     0x00
#define CODEC_INFO_MP25_MODE_JS         0x01
#define CODEC_INFO_MP25_MODE_DUAL       0x02
#define CODEC_INFO_MP25_MODE_SINGLE     0x03

/* layer 3, type of joint stereo coding method (intensity and ms) */
#define CODEC_INFO_MP25_MODE_EXT_OFF_OFF 0x00
#define CODEC_INFO_MP25_MODE_EXT_ON_OFF  0x01
#define CODEC_INFO_MP25_MODE_EXT_OFF_ON  0x02
#define CODEC_INFO_MP25_MODE_EXT_ON_ON   0x03

#define CODEC_INFO_MP25_SAMPLE_PER_FRAME    576

/* AAC/AAC+ CODEC Parameters */
#define CODEC_INFO_AAC_SF_IDX_96K   0x0
#define CODEC_INFO_AAC_SF_IDX_88K   0x1
#define CODEC_INFO_AAC_SF_IDX_64K   0x2
#define CODEC_INFO_AAC_SF_IDX_48K   0x3
#define CODEC_INFO_AAC_SF_IDX_44K   0x4
#define CODEC_INFO_AAC_SF_IDX_32K   0x5
#define CODEC_INFO_AAC_SF_IDX_24K   0x6
#define CODEC_INFO_AAC_SF_IDX_22K   0x7
#define CODEC_INFO_AAC_SF_IDX_16K   0x8
#define CODEC_INFO_AAC_SF_IDX_12K   0x9
#define CODEC_INFO_AAC_SF_IDX_11K   0xA
#define CODEC_INFO_AAC_SF_IDX_08K   0xB
#define CODEC_INFO_AAC_SF_IDX_RESERVE   0xC

#define CODEC_INFO_AAC_BR_RATE_48K  288000
#define CODEC_INFO_AAC_BR_RATE_44K  264600
#define CODEC_INFO_AAC_BR_RATE_32K  192000


#define CODEC_INFO_AAC_1_CH           1         /*center front speaker */
#define CODEC_INFO_AAC_2_CH           2         /*left, right front speaker */
#define CODEC_INFO_AAC_3_CH           3         /*center front speaker, left right front speaker */
#define CODEC_INFO_AAC_4_CH           4         /*center/rear front speaker, left/right front speaker */
#define CODEC_INFO_AAC_5_CH           5         /*center, left, right front speaker, left/right surround */
#define CODEC_INFO_AAC_6_CH           6         /*center, left, right front speaker, left/right surround, LFE */
#define CODEC_INFO_AAC_7_CH           7         /*(left, right)center/left,right front speaker, left/right surround, LFE */


typedef struct
{
    uint8_t   sampling_freq;
    uint8_t   channel_mode;
    uint8_t   block_length;
    uint8_t   num_subbands;
    uint8_t   alloc_method;
    uint8_t   bitpool_size;   /* 2 - 250 */
} tCODEC_INFO_SBC;

typedef struct
{
    uint8_t   ch_mode;
    uint8_t   sampling_freq;
    uint8_t   bitrate_index;  /* 0 - 14 */
} tCODEC_INFO_MP3;

typedef struct
{
    uint8_t   ch_mode;
    uint8_t   sampling_freq;
    uint8_t   bitrate_index;  /* 0 - 14 */
} tCODEC_INFO_MP2;


typedef struct
{
    uint8_t   ch_mode;
    uint8_t   sampling_freq;
    uint8_t   bitrate_index;  /* 0 - 14 */
} tCODEC_INFO_MP2_5;

typedef struct
{
    uint16_t  sampling_freq;
    uint8_t   channel_mode;   /* 0x02:mono, 0x01:dual */
    uint32_t  bitrate;        /* 0 - 320K */
    uint32_t  sbr_profile;        /* 1: ON, 0: OFF */
} tCODEC_INFO_AAC;

typedef union
{
    tCODEC_INFO_SBC     sbc;
    tCODEC_INFO_MP3     mp3;
    tCODEC_INFO_MP2     mp2;
    tCODEC_INFO_MP2_5   mp2_5;
    tCODEC_INFO_AAC     aac;
} tCODEC_INFO;

typedef struct
{
    uint8_t               opcode;     /* AUDIO_CODEC_CONFIG_REQ */
    tAUDIO_CODEC_TYPE   codec_type;
    tCODEC_INFO         codec_info;
} tAUDIO_CODEC_CONFIG_REQ;

#define AUDIO_CONFIG_SUCCESS            0x00
#define AUDIO_CONFIG_NOT_SUPPORTED      0x01
#define AUDIO_CONFIG_FAIL_OUT_OF_MEMORY 0x02
#define AUDIO_CONFIG_FAIL_CODEC_USED    0x03
#define AUDIO_CONFIG_FAIL_ROUTE         0x04
typedef uint8_t tAUDIO_CONFIG_STATUS;

typedef struct
{
    uint8_t                   opcode; /* AUDIO_CODEC_CONFIG_RESP */
    tAUDIO_CONFIG_STATUS    status;
} tAUDIO_CODEC_CONFIG_RESP;

typedef struct
{
    uint8_t               opcode;     /* AUDIO_CODEC_SET_BITRATE_REQ */
    tAUDIO_CODEC_TYPE   codec_type;
    union
    {
        uint8_t   sbc;
        uint8_t   mp3;
        uint32_t  aac;
    } codec_bitrate;
} tAUDIO_CODEC_SET_BITRATE_REQ;

#define AUDIO_ROUTE_SRC_FMRX        0x00
#define AUDIO_ROUTE_SRC_I2S         0x01
#define AUDIO_ROUTE_SRC_ADC         0x02
#define AUDIO_ROUTE_SRC_HOST        0x03
#define AUDIO_ROUTE_SRC_PTU         0x04
#define AUDIO_ROUTE_SRC_BTSNK       0x05
#define AUDIO_ROUTE_SRC_NONE        0x80
#define MAX_AUDIO_ROUTE_SRC         6
typedef uint8_t tAUDIO_ROUTE_SRC;

#define AUDIO_ROUTE_MIX_NONE        0x00
#define AUDIO_ROUTE_MIX_HOST        0x01
#define AUDIO_ROUTE_MIX_PCM         0x02
#define AUDIO_ROUTE_MIX_CHIRP       0x03
#define AUDIO_ROUTE_MIX_I2S         0x04
#define AUDIO_ROUTE_MIX_ADC         0x05
#define AUDIO_ROUTE_MIX_RESERVED    0x06
#define MAX_AUDIO_ROUTE_MIX         7
typedef uint8_t tAUDIO_ROUTE_MIX;

#define AUDIO_ROUTE_OUT_NONE        0x0000
#define AUDIO_ROUTE_OUT_BTA2DP      0x0001
#define AUDIO_ROUTE_OUT_FMTX        0x0002
#define AUDIO_ROUTE_OUT_BTSCO       0x0004
#define AUDIO_ROUTE_OUT_HOST        0x0008
#define AUDIO_ROUTE_OUT_DAC         0x0010
#define AUDIO_ROUTE_OUT_I2S         0x0020
#define AUDIO_ROUTE_OUT_BTA2DP_DAC  0x0040
#define AUDIO_ROUTE_OUT_BTA2DP_I2S  0x0080
#define AUDIO_ROUTE_OUT_BTSCO_DAC   0x0100
#define AUDIO_ROUTE_OUT_BTSCO_I2S   0x0200
#define AUDIO_ROUTE_OUT_HOST_BTA2DP 0x0400
#define AUDIO_ROUTE_OUT_HOST_BTSCO  0x0800
#define AUDIO_ROUTE_OUT_HOST_DAC    0x1000
#define AUDIO_ROUTE_OUT_HOST_I2S    0x2000
#define AUDIO_ROUTE_OUT_DAC_I2S     0x4000
#define AUDIO_ROUTE_OUT_RESERVED_2  0x8000

#define MAX_AUDIO_SINGLE_ROUTE_OUT  6
#define MAX_AUDIO_MULTI_ROUTE_OUT   16
typedef uint16_t tAUDIO_MULTI_ROUTE_OUT;
typedef uint8_t  tAUDIO_ROUTE_OUT;

#define AUDIO_ROUTE_SF_8K           0x00
#define AUDIO_ROUTE_SF_16K          0x01
#define AUDIO_ROUTE_SF_32K          0x02
#define AUDIO_ROUTE_SF_44_1K        0x03
#define AUDIO_ROUTE_SF_48K          0x04
#define AUDIO_ROUTE_SF_11K          0x05
#define AUDIO_ROUTE_SF_12K          0x06
#define AUDIO_ROUTE_SF_22K          0x07
#define AUDIO_ROUTE_SF_24K          0x08
#define AUDIO_ROUTE_SF_NA           0xFF
typedef uint8_t tAUDIO_ROUTE_SF;

#define AUDIO_ROUTE_EQ_BASS_BOOST   0x00
#define AUDIO_ROUTE_EQ_CLASSIC      0x01
#define AUDIO_ROUTE_EQ_JAZZ         0x02
#define AUDIO_ROUTE_EQ_LIVE         0x03
#define AUDIO_ROUTE_EQ_NORMAL       0x04
#define AUDIO_ROUTE_EQ_ROCK         0x05
#define AUDIO_ROUTE_EQ_BYPASS       0x06

#define AUDIO_ROUTE_DIGITAL_VOLUME_CONTROL  0x07

#define AUDIO_ROUTE_EQ_CONFIG_GAIN  0xFF    /* Custion Gain Config */
typedef uint8_t tAUDIO_ROUTE_EQ;

typedef struct
{
    uint8_t               opcode;     /* AUDIO_ROUTE_CONFIG_REQ */
    tAUDIO_ROUTE_SRC    src;
    tAUDIO_ROUTE_SF     src_sf;
    tAUDIO_ROUTE_OUT    out;
    tAUDIO_ROUTE_SF     out_codec_sf;
    tAUDIO_ROUTE_SF     out_i2s_sf;
    tAUDIO_ROUTE_EQ     eq_mode;
} tAUDIO_ROUTE_CONFIG_REQ;

typedef struct
{
    uint8_t                   opcode; /* AUDIO_ROUTE_CONFIG_RESP */
    tAUDIO_CONFIG_STATUS    status;
} tAUDIO_ROUTE_CONFIG_RESP;

typedef struct
{
    uint16_t  amp[2];                 /* left/right 15 bit amplitude value        */
    uint16_t  tone[2];                /* left/right 12 bit frequency 0 - 4096Hz   */
    uint16_t  mark[2];                /* left/right 16 bit mark time 0 - 65535ms  */
    uint16_t  space[2];               /* left/right 16 bit space time 0 - 65535ms */
} tCHIRP_CONFIG;

typedef struct
{
    uint8_t   pri_l;                  /* Primary Left scale : 0 ~ 255     */
    uint8_t   mix_l;                  /* Mixing Left scale : 0 ~ 255      */
    uint8_t   pri_r;                  /* Primary Right scale : 0 ~ 255    */
    uint8_t   mix_r;                  /* Mixing Right scale : 0 ~ 255     */
} tMIX_SCALE_CONFIG;

/* For custon equalizer gain configuration */
typedef struct
{
    uint32_t  audio_l_g0;         /* IIR biquad filter left ch gain 0 */
    uint32_t  audio_l_g1;         /* IIR biquad filter left ch gain 1 */
    uint32_t  audio_l_g2;         /* IIR biquad filter left ch gain 2 */
    uint32_t  audio_l_g3;         /* IIR biquad filter left ch gain 3 */
    uint32_t  audio_l_g4;         /* IIR biquad filter left ch gain 4 */
    uint32_t  audio_l_gl;         /* IIR biquad filter left ch global gain  */
    uint32_t  audio_r_g0;         /* IIR biquad filter left ch gain 0 */
    uint32_t  audio_r_g1;         /* IIR biquad filter left ch gain 1 */
    uint32_t  audio_r_g2;         /* IIR biquad filter left ch gain 2 */
    uint32_t  audio_r_g3;         /* IIR biquad filter left ch gain 3 */
    uint32_t  audio_r_g4;         /* IIR biquad filter left ch gain 4 */
    uint32_t  audio_r_gl;         /* IIR biquad filter left ch global gain */
} tEQ_GAIN_CONFIG;

typedef struct
{
    uint8_t               opcode;     /* AUDIO_MIX_CONFIG_REQ */
    tAUDIO_ROUTE_MIX    mix_src;
    tAUDIO_ROUTE_SF     mix_src_sf;
    tMIX_SCALE_CONFIG   mix_scale;
    tCHIRP_CONFIG       chirp_config;
} tAUDIO_MIX_CONFIG_REQ;

typedef struct
{
    uint8_t                   opcode; /* AUDIO_MIX_CONFIG_RESP */
    tAUDIO_CONFIG_STATUS    status;
} tAUDIO_MIX_CONFIG_RESP;


typedef struct
{
    uint8_t   opcode;                 /* AUDIO_BURST_FRAMES_IND */
    uint32_t  burst_size;             /* in bytes */
} tAUDIO_BURST_FRAMES_IND;

typedef struct
{
    uint8_t   opcode;                 /* AUDIO_BURST_END_IND */
} tAUDIO_BURST_END_IND;

typedef struct
{
    uint8_t   opcode;                 /* AUDIO_CODEC_FLUSH_REQ */
} tAUDIO_CODEC_FLUSH_REQ;

typedef struct
{
    uint8_t               opcode;     /* AUDIO_EQ_MODE_CONFIG_REQ */
    tAUDIO_ROUTE_EQ     eq_mode;
    tEQ_GAIN_CONFIG     filter_gain;    /* Valid only when eq_mode is 0xFF */
} tAUDIO_EQ_MODE_CONFIG_REQ;

typedef struct
{
    uint8_t               opcode;     /* AUDIO_SCALE_CONFIG_REQ */
    tMIX_SCALE_CONFIG   mix_scale;
} tAUDIO_SCALE_CONFIG_REQ;

#pragma pack(pop)					/* pop saved alignment to stack */

#endif /* UIPC_MSG_H */
