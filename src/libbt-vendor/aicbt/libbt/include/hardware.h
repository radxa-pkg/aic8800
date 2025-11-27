/******************************************************************************
 *
 *  Copyright (C) 2019-2021 Aicsemi Corporation
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

#ifndef HARDWARE_H
#define HARDWARE_H

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(d)                 (d)
#define cpu_to_le32(d)                 (d)
#define le16_to_cpu(d)                 (d)
#define le32_to_cpu(d)                 (d)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le16(d)                 bswap_16(d)
#define cpu_to_le32(d)                 bswap_32(d)
#define le16_to_cpu(d)                 bswap_16(d)
#define le32_to_cpu(d)                 bswap_32(d)
#else
#error "Unknown byte order"
#endif

#define HCI_CMD_MAX_LEN                258

#define HCI_VERSION_MASK_10            (1<<0)     //Bluetooth Core Spec 1.0b
#define HCI_VERSION_MASK_11            (1<<1)     //Bluetooth Core Spec 1.1
#define HCI_VERSION_MASK_12            (1<<2)     //Bluetooth Core Spec 1.2
#define HCI_VERSION_MASK_20            (1<<3)     //Bluetooth Core Spec 2.0+EDR
#define HCI_VERSION_MASK_21            (1<<4)     //Bluetooth Core Spec 2.1+EDR
#define HCI_VERSION_MASK_30            (1<<5)     //Bluetooth Core Spec 3.0+HS
#define HCI_VERSION_MASK_40            (1<<6)     //Bluetooth Core Spec 4.0
#define HCI_VERSION_MASK_41            (1<<7)     //Bluetooth Core Spec 4.1
#define HCI_VERSION_MASK_42            (1<<8)     //Bluetooth Core Spec 4.2
#define HCI_VERSION_MASK_ALL           (0xFFFFFFFF)

#define HCI_EVT_CMD_CMPL_OPCODE_OFFSET (3)     //opcode's offset in COMMAND Completed Event
#define HCI_EVT_CMD_CMPL_STATUS_OFFSET (5)     //status's offset in COMMAND Completed Event

#define HCI_CMD_PREAMBLE_SIZE          (3)
#define HCI_CMD_READ_CHIP_TYPE_SIZE    (5)

#define H5_SYNC_REQ_SIZE               (2)
#define H5_SYNC_RESP_SIZE              (2)
#define H5_CONF_REQ_SIZE               (3)
#define H5_CONF_RESP_SIZE              (2)

#define AICBT_CONFIG_ID_VX_SET         0x01
#define AICBT_CONFIG_ID_PTA_EN         0x0B
#define AICBT_CONFIG_ID_PCM_PARAM      0x0D
#define AICBT_CONFIG_ID_MULTI_ADV      0x20
#define AICBT_CONFIG_ID_FW_LOG         0x31

#define AICBT_CONFIG_ID_FW_LOG_SIZE    0x01

/******************************************************************************
**  Local type definitions
******************************************************************************/

/* Hardware Configuration State */
enum {
    HW_CFG_H5_INIT = 1,
    HW_CFG_READ_LOCAL_VER,// 2
    HW_CFG_READ_ECO_VER,   //eco version // 3
    HW_CFG_READ_CHIP_TYPE,// 4
    HW_CFG_START,// 5
    HW_CFG_SET_UART_BAUD_HOST,//change FW baudrate // 6
    HW_CFG_SET_UART_BAUD_CONTROLLER,//change Host baudrate// 7
    HW_CFG_SET_UART_HW_FLOW_CONTROL,// 8
    HW_CFG_RESET_CHANNEL_CONTROLLER,// 9
    HW_RESET_CONTROLLER,// 10
    HARDWARE_INIT_COMPLETE,// 11
    HW_CFG_DL_FW_PATCH,// 12
    HW_CFG_SET_BD_ADDR,// 13
//#if (USE_CONTROLLER_BDADDR == TRUE)
    HW_CFG_READ_BD_ADDR, // 14
//#endif
    HW_CFG_WR_RF_MDM_REGS,// 15
    HW_CFG_WR_RF_MDM_REGS_END,// 16
    HW_CFG_SET_RF_MODE,// 17
    HW_CFG_RF_CALIB_REQ,// 18
    HW_CFG_SET_FW_RET_PARAM,// 19
    HW_CFG_UPDATE_CONFIG_INFO,// 20
    HW_CFG_WR_AON_PARAM,// 21
    HW_CFG_SET_LP_LEVEL,// 22
    HW_CFG_SET_PWR_CTRL_SLAVE,// 23
    HW_CFG_SET_CPU_POWR_OFF_EN,// 24
    HW_CFG_READ_LOCAL_FEATURES,// 25
    HW_CFG_SET_PCM_PARAM,// 26
    HW_CFG_SET_MULTI_ADV_EN,// 27
    HW_CFG_SET_FW_LOG_ENABLE,//28
};

/* h/w config control block */
typedef struct {
    uint8_t state;                          /* Hardware configuration state */
    uint8_t f_set_baud_2;                   /* Baud rate switch state */
    char    local_chip_name[LOCAL_NAME_BUFFER_LEN];
} bt_hw_cfg_cb_t;

/* low power mode parameters */
typedef struct
{
    uint8_t sleep_mode;                     /* 0(disable),1(UART),9(H5) */
    uint8_t host_stack_idle_threshold;      /* Unit scale 300ms/25ms */
    uint8_t host_controller_idle_threshold; /* Unit scale 300ms/25ms */
    uint8_t bt_wake_polarity;               /* 0=Active Low, 1= Active High */
    uint8_t host_wake_polarity;             /* 0=Active Low, 1= Active High */
    uint8_t allow_host_sleep_during_sco;
    uint8_t combine_sleep_mode_and_lpm;
    uint8_t enable_uart_txd_tri_state;      /* UART_TXD Tri-State */
    uint8_t sleep_guard_time;               /* sleep guard time in 12.5ms */
    uint8_t wakeup_guard_time;              /* wakeup guard time in 12.5ms */
    uint8_t txd_config;                     /* TXD is high in sleep state */
    uint8_t pulsed_host_wake;               /* pulsed host wake if mode = 1 */
} bt_lpm_param_t;

struct aicbt_pta_config {
    ///pta enable
    uint8_t pta_en;
    ///pta sw enable
    uint8_t pta_sw_en;
    ///pta hw enable
    uint8_t pta_hw_en;
    ///pta method now using, 1:hw; 0:sw
    uint8_t pta_method;
    ///pta bt grant duration
    uint16_t pta_bt_du;
    ///pta wf grant duration
    uint16_t pta_wf_du;
    ///pta bt grant duration sco
    uint16_t pta_bt_du_sco;
    ///pta wf grant duration sco
    uint16_t pta_wf_du_sco;
    ///pta bt grant duration esco
    uint16_t pta_bt_du_esco;
    ///pta wf grant duration esco
    uint16_t pta_wf_du_esco;
    ///pta bt grant duration for page
    uint16_t pta_bt_page_du;
    ///pta acl cps value
    uint16_t pta_acl_cps_value;
    ///pta sco cps value
    uint16_t pta_sco_cps_value;
};

struct hci_wr_rf_mdm_regs_cmd {
    uint16_t offset;
    uint8_t rcvd;
    uint8_t len;
    uint8_t data[248];
};

typedef enum {
    AIC_RF_MODE_NULL   = 0x00,
    AIC_RF_MODE_BT_ONLY,
    AIC_RF_MODE_BT_COMBO,
    AIC_RF_MODE_BTWIFI_COMBO,
    AIC_RF_MODE_MAX,
} aicbt_rf_mode;

struct hci_set_rf_mode_cmd {
    uint8_t rf_mode;
};

struct buf_tag {
    uint8_t length;
    uint8_t data[128];
};

struct hci_rf_calib_req_cmd {
    uint8_t calib_type;
    uint16_t offset;
    struct buf_tag buff;
};

struct hci_vs_update_config_info_cmd {
    uint16_t config_id;
    uint16_t config_len;
    uint8_t config_data[32];
};

enum vs_update_config_info_state {
    VS_UPDATE_CONFIG_INFO_STATE_IDLE,
    VS_UPDATE_CONFIG_INFO_STATE_PTA_EN,
    VS_UPDATE_CONFIG_INFO_STATE_END,
};

extern uint32_t aicbt_up_config_info_state;
extern uint32_t rf_mdm_table_index;
extern aicbt_rf_mode bt_rf_mode;
extern bool bt_rf_need_config;
extern bool bt_rf_need_calib;
extern uint32_t rf_mdm_regs_offset;
extern const uint32_t rf_mdm_regs_table_bt_only[37][2];
extern const uint32_t rf_mdm_regs_table_bt_combo[20][2];
extern const struct aicbt_pta_config pta_config;
extern struct hci_rf_calib_req_cmd rf_calib_req_bt_only;
extern struct hci_rf_calib_req_cmd rf_calib_req_bt_combo;
extern bt_hw_cfg_cb_t hw_cfg_cb;
extern uint8_t vnd_local_bd_addr[BD_ADDR_LEN];

aicbt_rf_mode hw_get_bt_rf_mode(void);
bool hw_wr_rf_mdm_regs(HC_BT_HDR *p_buf);
uint8_t hw_config_set_bdaddr(HC_BT_HDR *p_buf);
uint8_t hw_config_set_pcm_param(HC_BT_HDR *p_buf);
uint8_t hw_config_read_bdaddr(HC_BT_HDR *p_buf);
uint8_t hw_set_fw_log(HC_BT_HDR *p_buf);
uint8_t hw_read_local_features(HC_BT_HDR *p_buf);
bool hw_aic_bt_pta_en(HC_BT_HDR *p_buf);
bool hw_set_rf_mode(HC_BT_HDR *p_buf);
bool hw_rf_calib_req(HC_BT_HDR *p_buf);
void hw_sco_config(void);
int hw_set_audio_state(bt_vendor_op_audio_state_t *p_state);
void hw_bt_assert_notify(void *p_mem);

// uart only
void hw_lpm_set_wake_state(uint8_t wake_assert);
uint32_t hw_lpm_get_idle_timeout(void);

#endif
