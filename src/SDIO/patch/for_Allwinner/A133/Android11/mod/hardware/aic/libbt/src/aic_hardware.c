/******************************************************************************
 *
 *  Copyright (C) 2019-2027 AIC Corporation
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
 *  Filename:      hardware.c
 *
 *  Description:   Contains controller-specific functions, like
 *                      firmware patch download
 *                      low power mode operations
 *
 ******************************************************************************/

#define LOG_TAG "bt_hwcfg"

#include <utils/Log.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor_aicbt.h"
#include "esco_parameters.h"
#include "userial.h"
#include "userial_vendor.h"
#include "upio.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef BTHW_DBG
#define BTHW_DBG FALSE
#endif

#if (BTHW_DBG == TRUE)
#define BTHWDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTHWDBG(param, ...) {}
#endif

#define FW_PATCHFILE_EXTENSION      ".hcd"
#define FW_PATCHFILE_EXTENSION_LEN  4
#define FW_PATCHFILE_PATH_MAXLEN    248 /* Local_Name length of return of
                                           HCI_Read_Local_Name */

#define HCI_CMD_MAX_LEN             258

#define HCI_RESET                               0x0C03
#define HCI_VSC_WRITE_UART_CLOCK_SETTING        0xFC45
#define HCI_VSC_UPDATE_BAUDRATE                 0xFC18
#define HCI_READ_LOCAL_NAME                     0x0C14
#define HCI_VSC_DOWNLOAD_MINIDRV                0xFC2E
#define HCI_VSC_WRITE_BD_ADDR                   0xFC70//0xFC01
#define HCI_VSC_WRITE_SLEEP_MODE                0xFC27
#define HCI_VSC_WRITE_SCO_PCM_INT_PARAM         0xFC1C
#define HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM     0xFC1E
#define HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM    0xFC6D
#define HCI_VSC_ENABLE_WBS                      0xFC7E
#define HCI_VSC_LAUNCH_RAM                      0xFC4E
#define HCI_READ_LOCAL_BDADDR                   0x1009

#define HCI_VSC_WR_RF_MDM_REGS_CMD    0xFC53
#define HCI_VSC_SET_RF_MODE_CMD            0xFC48
#define HCI_VSC_RF_CALIB_REQ_CMD           0xFC4B
#define HCI_VSC_WR_AON_PARAM_CMD        0xFC4D

#define HCI_VSC_UPDATE_CONFIG_INFO_CMD           0xFC72
#define HCI_VSC_SET_LP_LEVEL_CMD                        0xFC50
#define HCI_VSC_SET_PWR_CTRL_SLAVE_CMD           0xFC51
#define HCI_VSC_SET_CPU_POWER_OFF_CMD            0xFC52
#define HCI_VSC_SET_SLEEP_EN_CMD                        0xFC47

#define HCI_VSC_WR_RF_MDM_REGS_SIZE    252
#define HCI_VSC_SET_RF_MODE_SIZE            01
#define HCI_VSC_RF_CALIB_REQ_SIZE           132
#define HCI_VSC_WR_AON_PARAM_SIZE        104

#define HCI_VSC_UPDATE_CONFIG_INFO_SIZE           36
#define HCI_VSC_SET_LP_LEVEL_SIZE                        1
#define HCI_VSC_SET_PWR_CTRL_SLAVE_SIZE           1
#define HCI_VSC_SET_CPU_POWER_OFF_SIZE            1
#define HCI_VSC_SET_SLEEP_EN_SIZE                        8


#define HCI_EVT_CMD_CMPL_STATUS_RET_BYTE        5
#define HCI_EVT_CMD_CMPL_LOCAL_NAME_STRING      6
#define HCI_EVT_CMD_CMPL_LOCAL_BDADDR_ARRAY     6
#define HCI_EVT_CMD_CMPL_OPCODE                 3
#define LPM_CMD_PARAM_SIZE                      12
#define UPDATE_BAUDRATE_CMD_PARAM_SIZE          6
#define HCI_CMD_PREAMBLE_SIZE                   3
#define HCD_REC_PAYLOAD_LEN_BYTE                2
#define BD_ADDR_LEN                             6
#define LOCAL_NAME_BUFFER_LEN                   32
#define LOCAL_BDADDR_PATH_BUFFER_LEN            256

#define STREAM_TO_UINT16(u16, p) {u16 = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); (p) += 2;}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT32_TO_STREAM(p, u32) {*(p)++ = (uint8_t)(u32); *(p)++ = (uint8_t)((u32) >> 8); *(p)++ = (uint8_t)((u32) >> 16); *(p)++ = (uint8_t)((u32) >> 24);}

#define SCO_INTERFACE_PCM  0
#define SCO_INTERFACE_I2S  1

/* one byte is for enable/disable
      next 2 bytes are for codec type */
#define SCO_CODEC_PARAM_SIZE                    3

/******************************************************************************
**  Local type definitions
******************************************************************************/

/* Hardware Configuration State */
enum {
    HW_CFG_START = 1,
    HW_CFG_SET_UART_CLOCK,
    HW_CFG_SET_UART_BAUD_1,
    HW_CFG_READ_LOCAL_NAME,
    HW_CFG_DL_MINIDRIVER,
    HW_CFG_DL_FW_PATCH,
    HW_CFG_SET_UART_BAUD_2,
    HW_CFG_SET_BD_ADDR
#if (USE_CONTROLLER_BDADDR == TRUE)
    , HW_CFG_READ_BD_ADDR
#endif
    ,HW_CFG_WR_RF_MDM_REGS,
    HW_CFG_WR_RF_MDM_REGS_END,
    HW_CFG_SET_RF_MODE,
    HW_CFG_RF_CALIB_REQ,
    HW_CFG_UPDATE_CONFIG_INFO,
    HW_CFG_WR_AON_PARAM,
    HW_CFG_SET_LP_LEVEL,
    HW_CFG_SET_PWR_CTRL_SLAVE,
    HW_CFG_SET_CPU_POWR_OFF_EN
};

/* h/w config control block */
typedef struct
{
    uint8_t state;                          /* Hardware configuration state */
    int     fw_fd;                          /* FW patch file fd */
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

/* Firmware re-launch settlement time */
typedef struct {
    const char *chipset_name;
    const uint32_t delay_time;
} fw_settlement_entry_t;

/* AMPAK FW auto detection table */
typedef struct {
    char *chip_id;
    char *updated_chip_id;
} fw_auto_detection_entry_t;


struct hci_wr_rf_mdm_regs_cmd
{
    uint16_t offset;
    uint8_t rcvd;
    uint8_t len;
    uint8_t data[248];
};

typedef enum
{
    AIC_RF_MODE_NULL                =0x00,
    AIC_RF_MODE_BT_ONLY,
    AIC_RF_MODE_BT_COMBO,
    AIC_RF_MODE_BTWIFI_COMBO,
    AIC_RF_MODE_MAX,
}aicbt_rf_mode;

struct hci_set_rf_mode_cmd
{
    uint8_t rf_mode;
};

struct buf_tag
{
    uint8_t length;
    uint8_t data[128];
};

struct hci_rf_calib_req_cmd
{
    uint8_t calib_type;
    uint16_t offset;
    struct buf_tag buff;
};

#define AICBT_CONFIG_ID_VX_SET              0x01
#define AICBT_CONFIG_ID_PTA_EN              0x0B

struct hci_vs_update_config_info_cmd
{
    uint16_t config_id;
    uint16_t config_len;
    uint8_t config_data[32];
};
enum vs_update_config_info_state
{
    VS_UPDATE_CONFIG_INFO_STATE_IDLE,
    VS_UPDATE_CONFIG_INFO_STATE_PTA_EN,


    VS_UPDATE_CONFIG_INFO_STATE_END,
};
uint32_t aicbt_up_config_info_state = VS_UPDATE_CONFIG_INFO_STATE_IDLE;

struct aicbt_pta_config
{
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

const struct aicbt_pta_config pta_config =
{
    ///pta enable
    .pta_en = 1,
    ///pta sw enable
    .pta_sw_en = 1,
    ///pta hw enable
    .pta_hw_en = 0,
    ///pta method now using, 1:hw; 0:sw
    .pta_method = 0,
    ///pta bt grant duration
    .pta_bt_du = 0x135,
    ///pta wf grant duration
    .pta_wf_du = 0x0BC,
    ///pta bt grant duration sco
    .pta_bt_du_sco = 0x4E,
    ///pta wf grant duration sco
    .pta_wf_du_sco = 0x27,
    ///pta bt grant duration esco
    .pta_bt_du_esco = 0X9C,
    ///pta wf grant duration esco
    .pta_wf_du_esco = 0x6D,
    ///pta bt grant duration for page
    .pta_bt_page_du = 3000,
    ///pta acl cps value
    .pta_acl_cps_value = 0x1450,
    ///pta sco cps value
    .pta_sco_cps_value = 0x0c50,
};


uint32_t rf_mdm_table_index = 0;

const uint32_t rf_mdm_regs_table_bt_only[][2] =
{
    {0x40580104, 0x000923fb},
    {0x4062201c, 0x0008d000},
    {0x40622028, 0x48912020},
    {0x40622014, 0x00018983},
    {0x40622054, 0x00008f34},
    {0x40620748, 0x021a01a0},
    {0x40620728, 0x00010020},
    {0x40620738, 0x04800fd4},
    {0x4062073c, 0x00c80064},
    {0x4062202c, 0x000cb220},
    {0x4062200c, 0xe9ad2b45},
    {0x40622030, 0x143c30d2},
    {0x40622034, 0x00001602},
    {0x40620754, 0x214220fd},
    {0x40620758, 0x0007f01e},
    {0x4062071c, 0x00000a33},
    {0x40622018, 0x00124124},
    {0x4062000c, 0x04040000},
    {0x40620090, 0x00069082},
    {0x40621034, 0x02003080},
    {0x40621014, 0x0445117a},
    {0x40622024, 0x00001100},
    {0x40622004, 0x0001a9c0},
    {0x4060048c, 0x00500834},
    {0x40600110, 0x027e0058},
    {0x40600880, 0x00500834},
    {0x40600884, 0x00500834},
    {0x40600888, 0x00500834},
    {0x4060088c, 0x00000834},
    {0x4062050c, 0x20202013},
    {0x406205a0, 0x181c0000},
    {0x406205a4, 0x36363636},
    {0x406205f0, 0x0000ff00},
    {0x40620508, 0x54553132},
    {0x40620530, 0x140f0b00},
    {0x406205b0, 0x00005355},
    {0x4062051c, 0x964b5766},
};

const uint32_t rf_mdm_regs_table_bt_combo[][2] =
{
    {0x40580104, 0x000923fb},
    {0x4034402c, 0x5e201884},
    {0x40344030, 0x1a2e5108},
    {0x40344020, 0x00000977},
    {0x40344024, 0x002ec594},
    {0x40344028, 0x00009402},
    {0x4060048c, 0x00500834},
    {0x40600110, 0x027e0058},
    {0x40600880, 0x00500834},
    {0x40600884, 0x00500834},
    {0x40600888, 0x00500834},
    {0x4060088c, 0x00000834},
    {0x4062050c, 0x20202013},
    {0x40620508, 0x54552022},
    {0x406205a0, 0x1c171a03},
    {0x406205a4, 0x36363636},
    {0x406205f0, 0x0000ff00},
    {0x40620530, 0x0c15120f},
    {0x406205b0, 0x00005355},
    {0x4062051c, 0x964b5766},
};

aicbt_rf_mode bt_rf_mode = AIC_RF_MODE_BT_ONLY;///AIC_RF_MODE_BT_COMBO;///AIC_RF_MODE_BT_ONLY;
bool  bt_rf_need_config = false;
//bt_rf_need_calib, may be set by driver to indicate whether need to do rf_calib,default true
bool  bt_rf_need_calib = true;
struct hci_rf_calib_req_cmd rf_calib_req_bt_only = {AIC_RF_MODE_BT_ONLY, 0x0000, {0x08, {0x13,0x42,0x26,0x00,0x0f,0x30,0x02,0x00}}};
struct hci_rf_calib_req_cmd rf_calib_req_bt_combo = {AIC_RF_MODE_BT_COMBO, 0x0000, {0x04, {0x03,0x42,0x26,0x00}}};

aicbt_rf_mode hw_get_bt_rf_mode(void);
void hw_set_bt_rf_mode(aicbt_rf_mode mode);
bool hw_wr_rf_mdm_regs(HC_BT_HDR *p_buf);
bool hw_set_rf_mode(HC_BT_HDR *p_buf);
bool hw_rf_calib_req(HC_BT_HDR *p_buf);
bool hw_aic_bt_pta_en(HC_BT_HDR *p_buf);

enum
{
    BT_LP_LEVEL_ACTIVE                 = 0x00,//BT CORE active, CPUSYS active, VCORE active
    BT_LP_LEVEL_CLOCK_GATE1       = 0x01,//BT CORE clock gate, CPUSYS active, VCORE active
    BT_LP_LEVEL_CLOCK_GATE2       = 0x02,//BT CORE clock gate, CPUSYS clock gate, VCORE active
    BT_LP_LEVEL_CLOCK_GATE3       = 0x03,//BT CORE clock gate, CPUSYS clock gate, VCORE clock gate
    BT_LP_LEVEL_POWER_OFF1        = 0x04,//BT CORE power off, CPUSYS active, VCORE active
    BT_LP_LEVEL_POWER_OFF2        = 0x05,//BT CORE power off, CPUSYS clock gate, VCORE active
    BT_LP_LEVEL_POWER_OFF3        = 0x06,//BT CORE power off, CPUSYS power off, VCORE active
    BT_LP_LEVEL_HIBERNATE           = 0x07,//BT CORE power off, CPUSYS power off, VCORE active
    BT_LP_LEVEL_MAX                      = 0x08,//
};
enum
{
    AICBT_SLEEP_STATE_IDLE                 = 0,
    AICBT_SLEEP_STATE_CONFIG_ING,
    AICBT_SLEEP_STATE_CONFIG_DONE,
};

struct aicbt_hci_set_sleep_en_cmd
{
    ///sleep enable
    uint8_t sleep_en;
    ///external warkeup enable
    uint8_t ext_wakeup_en;
    ///reserved
    uint8_t rsvd[6];

};
typedef struct
{
    /// Em save start address
    uint32_t em_save_start_addr;
    /// Em save end address
    uint32_t em_save_end_addr;
    /// Minimum time that allow power off(in hs)
    int32_t aon_min_power_off_duration;
    /// Maximum aon params
    uint16_t aon_max_nb_params;
    /// RF config const time on cpus side (in hus)
    int16_t aon_rf_config_time_cpus;
    /// RF config const time on aon side (in hus)
    int16_t aon_rf_config_time_aon;
    /// Maximun active acl link supported by aon
    uint16_t aon_max_nb_active_acl;
    /// Maximun ble activity supported by aon
    uint16_t aon_ble_activity_max;
    /// Maximum bt rxdesc field supported by aon
    uint16_t aon_max_bt_rxdesc_field;
    /// Maximum bt rxdesc field supported by aon
    uint16_t aon_max_ble_rxdesc_field;
    /// Maximum regs supported by aon
    uint16_t aon_max_nb_regs;
    /// Maximum length of ke_env supported by aon
    uint16_t aon_max_ke_env_len;
    /// Maximum elements of sch_arb_env supported by aon
    uint16_t aon_max_nb_sc_arb_elt;
    /// Maximum elements of sch_plan_env supported by aon
    uint16_t aon_max_nb_sch_plan_elt;
    /// Maximum elements of sch_alarm_elt_env supported by aon
    uint16_t aon_max_nb_sch_alarm_elt;
    /// Minimum advertising interval in slots(625 us) supported by aon
    uint32_t aon_min_ble_adv_intv;
    /// Minimum connection inverval in 2-sltos(1.25ms) supported by aon
    uint32_t aon_min_ble_con_intv;
    /// Extra sleep duration for cpus(in hs), may be negative
    int32_t aon_extra_sleep_duration_cpus;
    /// Extra sleep duration for aon(in hs), may be negative
    int32_t aon_extra_sleep_duration_aon;
    /// Minimum time that allow host to power off(in us)
    int32_t aon_min_power_off_duration_cpup;
    /// aon debug level for cpus
    uint32_t aon_debug_level;
    /// aon debug level for aon
    uint32_t aon_debug_level_aon;
    /// Power on delay of bt core on when cpus_sys alive on cpus side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly1;
    /// Power on delay of bt core on when cpus_sys clk gate on cpus side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly2;
    /// Power on delay of bt core on when cpus_sys power off on cpus side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly3;
    /// Power on delay of bt core on on aon side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly_aon;
    /// Time to cancle sch arbiter elements in advance when switching to cpus(in hus)
    uint16_t aon_sch_arb_cancel_in_advance_time;
    /// Duration of sleep and wake-up algorithm (depends on CPU speed) expressed in half us on cpus side
    /// should also contain deep_sleep_on rising edge to fimecnt halt (max 4 lp cycles) and finecnt resume to dm_slp_irq(0.5 lp cycles)
    uint16_t aon_sleep_algo_dur_cpus;
    /// Duration of sleep and wake-up algorithm (depends on CPU speed) expressed in half us on aon side
    /// should also contain deep_sleep_on rising edge to fimecnt halt (max 4 lp cycles) and finecnt resume to dm_slp_irq(0.5 lp cycles)
    uint16_t aon_sleep_algo_dur_aon;
    /// Threshold that treat fractional part of restore time (in hus) as 1 hs on cpus side
    uint16_t aon_restore_time_ceil_cpus;
    /// Threshold that treat fractional part of restore time (in hus) as 1 hs on aon side
    uint16_t aon_restore_time_ceil_aon;
    /// Minmum time that allow deep sleep on cpus side (in hs)
    uint16_t aon_min_sleep_duration_cpus;
    /// Minmum time that allow deep sleep on aon side (in hs)
    uint16_t aon_min_sleep_duration_aon;
    /// Difference of resore time an save time on cpus side (in hus)
    int16_t aon_restore_save_time_diff_cpus;
    /// Difference of resore time an save time on aon side (in hus)
    int16_t aon_restore_save_time_diff_aon;
    /// Difference of restore time on aon side and save time on cpus side (in hus)
    int16_t aon_restore_save_time_diff_cpus_aon;
    /// Minimum time that allow clock gate (in hs)
    int32_t aon_min_clock_gate_duration;
    /// Minimum time that allow host to clock gate (in hus)
    int32_t aon_min_clock_gate_duration_cpup;
    // Maximum rf & md regs supported by aon
    uint16_t aon_max_nb_rf_mdm_regs;
}bt_drv_wr_aon_param;
#define AON_BT_PWR_DLY1                     (1 + 5 + 1)
#define AON_BT_PWR_DLY2                     (10 + 48 + 5 + 1)
#define AON_BT_PWR_DLY3                     (10 + 48 + 8 + 5 + 1)
#define AON_BT_PWR_DLY_AON               (10 + 48 + 8 + 5)
const bt_drv_wr_aon_param wr_aon_param =
{
    0x18D700, 0x18F700, 64, 40, 400, 400, 3, 2,
    3, 2, 40, 512, 20, 21, 20, 32,
    8, 0, 0, 20000, 0x103, 0x20067302, AON_BT_PWR_DLY1, AON_BT_PWR_DLY2,
    AON_BT_PWR_DLY3, AON_BT_PWR_DLY_AON, 32, 360, 420, 100, 100, 8,
    24, 40, 140, 0, 64, 20000, 50
};
uint8_t aicbt_lp_level = BT_LP_LEVEL_CLOCK_GATE2;
uint8_t  aicbt_sleep_state = false;
bool hw_aic_bt_set_lp_level(HC_BT_HDR *p_buf);
bool hw_aic_bt_wr_aon_params(HC_BT_HDR *p_buf);
bool hw_aic_bt_set_pwr_ctrl_slave(HC_BT_HDR *p_buf);
bool hw_aic_bt_set_cpu_poweroff_en(HC_BT_HDR *p_buf);

bool hw_aic_bt_set_sleep_en(HC_BT_HDR *p_buf);

/******************************************************************************
**  Externs
******************************************************************************/

void hw_config_cback(void *p_evt_buf);
extern uint8_t vnd_local_bd_addr[BD_ADDR_LEN];


/******************************************************************************
**  Static variables
******************************************************************************/

static char fw_patchfile_path[256] = FW_PATCHFILE_LOCATION;
static char fw_patchfile_name[128] = { 0 };
#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
static int fw_patch_settlement_delay = -1;
#endif

static int wbs_sample_rate = SCO_WBS_SAMPLE_RATE;
static bt_hw_cfg_cb_t hw_cfg_cb;

static bt_lpm_param_t lpm_param =
{
    LPM_SLEEP_MODE,
    LPM_IDLE_THRESHOLD,
    LPM_HC_IDLE_THRESHOLD,
    LPM_BT_WAKE_POLARITY,
    LPM_HOST_WAKE_POLARITY,
    LPM_ALLOW_HOST_SLEEP_DURING_SCO,
    LPM_COMBINE_SLEEP_MODE_AND_LPM,
    LPM_ENABLE_UART_TXD_TRI_STATE,
    0,  /* not applicable */
    0,  /* not applicable */
    0,  /* not applicable */
    LPM_PULSED_HOST_WAKE
};

/* need to update the bt_sco_i2spcm_param as well
   bt_sco_i2spcm_param will be used for WBS setting
   update the bt_sco_param and bt_sco_i2spcm_param */
static uint8_t bt_sco_param[SCO_PCM_PARAM_SIZE] =
{
    SCO_PCM_ROUTING,
    SCO_PCM_IF_CLOCK_RATE,
    SCO_PCM_IF_FRAME_TYPE,
    SCO_PCM_IF_SYNC_MODE,
    SCO_PCM_IF_CLOCK_MODE
};

static uint8_t bt_pcm_data_fmt_param[PCM_DATA_FORMAT_PARAM_SIZE] =
{
    PCM_DATA_FMT_SHIFT_MODE,
    PCM_DATA_FMT_FILL_BITS,
    PCM_DATA_FMT_FILL_METHOD,
    PCM_DATA_FMT_FILL_NUM,
    PCM_DATA_FMT_JUSTIFY_MODE
};

static uint8_t bt_sco_i2spcm_param[SCO_I2SPCM_PARAM_SIZE] =
{
    SCO_I2SPCM_IF_MODE,
    SCO_I2SPCM_IF_ROLE,
    SCO_I2SPCM_IF_SAMPLE_RATE,
    SCO_I2SPCM_IF_CLOCK_RATE
};

/*
 * The look-up table of recommended firmware settlement delay (milliseconds) on
 * known chipsets.
 */
static const fw_settlement_entry_t fw_settlement_table[] = {
    {"AIC8800", 200},
    {(const char *) NULL, 100}  // Giving the generic fw settlement delay setting.
};


/*
 * NOTICE:
 *     If the platform plans to run I2S interface bus over I2S/PCM port of the
 *     BT Controller with the Host AP, explicitly set "SCO_USE_I2S_INTERFACE = TRUE"
 *     in the correspodning include/vnd_<target>.txt file.
 *     Otherwise, leave SCO_USE_I2S_INTERFACE undefined in the vnd_<target>.txt file.
 *     And, PCM interface will be set as the default bus format running over I2S/PCM
 *     port.
 */
#if (defined(SCO_USE_I2S_INTERFACE) && SCO_USE_I2S_INTERFACE == TRUE)
static uint8_t sco_bus_interface = SCO_INTERFACE_I2S;
#else
static uint8_t sco_bus_interface = SCO_INTERFACE_PCM;
#endif

#define INVALID_SCO_CLOCK_RATE  0xFF
static uint8_t sco_bus_clock_rate = INVALID_SCO_CLOCK_RATE;
static uint8_t sco_bus_wbs_clock_rate = INVALID_SCO_CLOCK_RATE;

#define FW_TABLE_VERSION "v1.1 20161117"
static const fw_auto_detection_entry_t fw_auto_detection_table[] = {
    {"8800","AIC8800"},     //8800
    {NULL, NULL}
};

/******************************************************************************
**  Static functions
******************************************************************************/
static void hw_sco_i2spcm_config(uint16_t codec);
static void hw_sco_i2spcm_config_from_command(void *p_mem, uint16_t codec);

/******************************************************************************
**  Controller Initialization Static Functions
******************************************************************************/

/*******************************************************************************
**
** Function        look_up_fw_settlement_delay
**
** Description     If FW_PATCH_SETTLEMENT_DELAY_MS has not been explicitly
**                 re-defined in the platform specific build-time configuration
**                 file, we will search into the look-up table for a
**                 recommended firmware settlement delay value.
**
**                 Although the settlement time might be also related to board
**                 configurations such as the crystal clocking speed.
**
** Returns         Firmware settlement delay
**
*******************************************************************************/
uint32_t look_up_fw_settlement_delay (void)
{
    uint32_t ret_value;
    fw_settlement_entry_t *p_entry;

    if (FW_PATCH_SETTLEMENT_DELAY_MS > 0)
    {
        ret_value = FW_PATCH_SETTLEMENT_DELAY_MS;
    }
#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
    else if (fw_patch_settlement_delay >= 0)
    {
        ret_value = fw_patch_settlement_delay;
    }
#endif
    else
    {
        p_entry = (fw_settlement_entry_t *)fw_settlement_table;

        while (p_entry->chipset_name != NULL)
        {
            if (strstr(hw_cfg_cb.local_chip_name, p_entry->chipset_name)!=NULL)
            {
                break;
            }

            p_entry++;
        }

        ret_value = p_entry->delay_time;
    }

    BTHWDBG( "Settlement delay -- %d ms", ret_value);

    return (ret_value);
}

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

/*******************************************************************************
**
** Function        line_speed_to_userial_baud
**
** Description     helper function converts line speed number into USERIAL baud
**                 rate symbol
**
** Returns         unit8_t (USERIAL baud symbol)
**
*******************************************************************************/
uint8_t line_speed_to_userial_baud(uint32_t line_speed)
{
    uint8_t baud;

    if (line_speed == 4000000)
        baud = USERIAL_BAUD_4M;
    else if (line_speed == 3500000)
        baud = USERIAL_BAUD_3_5M;
    else if (line_speed == 3000000)
        baud = USERIAL_BAUD_3M;
    else if (line_speed == 2500000)
        baud = USERIAL_BAUD_2_5M;
    else if (line_speed == 2000000)
        baud = USERIAL_BAUD_2M;
    else if (line_speed == 1500000)
        baud = USERIAL_BAUD_1_5M;
    else if (line_speed == 1000000)
        baud = USERIAL_BAUD_1M;
    else if (line_speed == 921600)
        baud = USERIAL_BAUD_921600;
    else if (line_speed == 460800)
        baud = USERIAL_BAUD_460800;
    else if (line_speed == 230400)
        baud = USERIAL_BAUD_230400;
    else if (line_speed == 115200)
        baud = USERIAL_BAUD_115200;
    else if (line_speed == 57600)
        baud = USERIAL_BAUD_57600;
    else if (line_speed == 19200)
        baud = USERIAL_BAUD_19200;
    else if (line_speed == 9600)
        baud = USERIAL_BAUD_9600;
    else if (line_speed == 1200)
        baud = USERIAL_BAUD_1200;
    else if (line_speed == 600)
        baud = USERIAL_BAUD_600;
    else
    {
        ALOGE( "userial vendor: unsupported baud speed %d", line_speed);
        baud = USERIAL_BAUD_115200;
    }

    return baud;
}


/*******************************************************************************
**
** Function         hw_strncmp
**
** Description      Used to compare two strings in caseless
**
** Returns          0: match, otherwise: not match
**
*******************************************************************************/
static int hw_strncmp (const char *p_str1, const char *p_str2, const int len)
{
    int i;

    if (!p_str1 || !p_str2)
        return (1);

    for (i = 0; i < len; i++)
    {
        if (toupper(p_str1[i]) != toupper(p_str2[i]))
            return (i+1);
    }

    return 0;
}

/*******************************************************************************
**
** Function         hw_config_findpatch
**
** Description      Search for a proper firmware patch file
**                  The selected firmware patch file name with full path
**                  will be stored in the input string parameter, i.e.
**                  p_chip_id_str, when returns.
**
** Returns          TRUE when found the target patch file, otherwise FALSE
**
*******************************************************************************/
static uint8_t hw_config_findpatch(char *p_chip_id_str)
{
    DIR *dirp;
    struct dirent *dp;
    int filenamelen;
    uint8_t retval = FALSE;
    fw_auto_detection_entry_t *p_entry;

    BTHWDBG("Target name = [%s]", p_chip_id_str);

    if (strlen(fw_patchfile_name)> 0)
    {
        /* If specific filepath and filename have been given in run-time
         * configuration /etc/bluetooth/bt_vendor.conf file, we will use them
         * to concatenate the filename to open rather than searching a file
         * matching to chipset name in the fw_patchfile_path folder.
         */
        sprintf(p_chip_id_str, "%s", fw_patchfile_path);
        if (fw_patchfile_path[strlen(fw_patchfile_path)- 1] != '/')
        {
            strcat(p_chip_id_str, "/");
        }
        strcat(p_chip_id_str, fw_patchfile_name);

        ALOGI("FW patchfile: %s", p_chip_id_str);
        return TRUE;
    }
    BTHWDBG("###AMPAK FW Auto detection patch version = [%s]###", FW_TABLE_VERSION);
    p_entry = (fw_auto_detection_entry_t *)fw_auto_detection_table;
    while (p_entry->chip_id != NULL)
    {
        if (strstr(p_chip_id_str, p_entry->chip_id)!=NULL)
        {
            strcpy(p_chip_id_str,p_entry->updated_chip_id);
            break;
        }
        p_entry++;
    }
    BTHWDBG("Updated Target name = [%s]", p_chip_id_str);

    if ((dirp = opendir(fw_patchfile_path)) != NULL)
    {
        /* Fetch next filename in patchfile directory */
        while ((dp = readdir(dirp)) != NULL)
        {
            /* Check if filename starts with chip-id name */
            if ((hw_strncmp(dp->d_name, p_chip_id_str, strlen(p_chip_id_str)) \
                ) == 0)
            {
                /* Check if it has .hcd extenstion */
                filenamelen = strlen(dp->d_name);
                if ((filenamelen >= FW_PATCHFILE_EXTENSION_LEN) &&
                    ((hw_strncmp(
                          &dp->d_name[filenamelen-FW_PATCHFILE_EXTENSION_LEN], \
                          FW_PATCHFILE_EXTENSION, \
                          FW_PATCHFILE_EXTENSION_LEN) \
                     ) == 0))
                {
                    ALOGI("Found patchfile: %s/%s", \
                        fw_patchfile_path, dp->d_name);

                    /* Make sure length does not exceed maximum */
                    if ((filenamelen + strlen(fw_patchfile_path)) > \
                         FW_PATCHFILE_PATH_MAXLEN)
                    {
                        ALOGE("Invalid patchfile name (too long)");
                    }
                    else
                    {
                        memset(p_chip_id_str, 0, FW_PATCHFILE_PATH_MAXLEN);
                        /* Found patchfile. Store location and name */
                        strcpy(p_chip_id_str, fw_patchfile_path);
                        if (fw_patchfile_path[ \
                            strlen(fw_patchfile_path)- 1 \
                            ] != '/')
                        {
                            strcat(p_chip_id_str, "/");
                        }
                        strcat(p_chip_id_str, dp->d_name);
                        retval = TRUE;
                    }
                    break;
                }
            }
        }

        closedir(dirp);

        if (retval == FALSE)
        {
            /* Try again chip name without revision info */

            int len = strlen(p_chip_id_str);
            char *p = p_chip_id_str + len - 1;

            /* Scan backward and look for the first alphabet
               which is not M or m
            */
            while (len > 3) // AIC****
            {
                if ((isdigit(*p)==0) && (*p != 'M') && (*p != 'm'))
                    break;

                p--;
                len--;
            }

            if (len > 3)
            {
                *p = 0;
                retval = hw_config_findpatch(p_chip_id_str);
            }
        }
    }
    else
    {
        ALOGE("Could not open %s", fw_patchfile_path);
    }

    return (retval);
}

/*******************************************************************************
**
** Function         hw_config_set_bdaddr
**
** Description      Program controller's Bluetooth Device Address
**
** Returns          TRUE, if valid address is sent
**                  FALSE, otherwise
**
*******************************************************************************/
static uint8_t hw_config_set_bdaddr(HC_BT_HDR *p_buf)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    ALOGI("Setting local bd addr to %02X:%02X:%02X:%02X:%02X:%02X",
        vnd_local_bd_addr[0], vnd_local_bd_addr[1], vnd_local_bd_addr[2],
        vnd_local_bd_addr[3], vnd_local_bd_addr[4], vnd_local_bd_addr[5]);

    UINT16_TO_STREAM(p, HCI_VSC_WRITE_BD_ADDR);
    *p++ = BD_ADDR_LEN; /* parameter length */
    *p++ = vnd_local_bd_addr[5];
    *p++ = vnd_local_bd_addr[4];
    *p++ = vnd_local_bd_addr[3];
    *p++ = vnd_local_bd_addr[2];
    *p++ = vnd_local_bd_addr[1];
    *p = vnd_local_bd_addr[0];

    p_buf->len = HCI_CMD_PREAMBLE_SIZE + BD_ADDR_LEN;
    hw_cfg_cb.state = HW_CFG_SET_BD_ADDR;

    retval = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_BD_ADDR, p_buf, \
                                 hw_config_cback);

    return (retval);
}

#if (USE_CONTROLLER_BDADDR == TRUE)
/*******************************************************************************
**
** Function         hw_config_read_bdaddr
**
** Description      Read controller's Bluetooth Device Address
**
** Returns          TRUE, if valid address is sent
**                  FALSE, otherwise
**
*******************************************************************************/
static uint8_t hw_config_read_bdaddr(HC_BT_HDR *p_buf)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    UINT16_TO_STREAM(p, HCI_READ_LOCAL_BDADDR);
    *p = 0; /* parameter length */

    p_buf->len = HCI_CMD_PREAMBLE_SIZE;
    hw_cfg_cb.state = HW_CFG_READ_BD_ADDR;

    retval = bt_vendor_cbacks->xmit_cb(HCI_READ_LOCAL_BDADDR, p_buf, \
                                 hw_config_cback);

    return (retval);
}
#endif // (USE_CONTROLLER_BDADDR == TRUE)

/*******************************************************************************
**
** Function         hw_config_cback
**
** Description      Callback function for controller configuration
**
** Returns          None
**
*******************************************************************************/
void hw_config_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    char        *p_name, *p_tmp;
    uint8_t     *p, status;
    uint16_t    opcode;
    HC_BT_HDR  *p_buf=NULL;
    uint8_t     is_proceeding = FALSE;
    int         i;
    int         delay=100;
#if (USE_CONTROLLER_BDADDR == TRUE)
    const uint8_t null_bdaddr[BD_ADDR_LEN] = {0,0,0,0,0,0};
#endif

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    /* Ask a new buffer big enough to hold any HCI commands sent in here */
    if ((status == 0) && bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_MAX_LEN);

    if (p_buf != NULL)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->len = 0;
        p_buf->layer_specific = 0;

        p = (uint8_t *) (p_buf + 1);

        switch (hw_cfg_cb.state)
        {
            case HW_CFG_SET_UART_BAUD_1:
                /* update baud rate of host's UART port */
                ALOGI("bt vendor lib: set UART baud %i", UART_TARGET_BAUD_RATE);
                userial_vendor_set_baud( \
                    line_speed_to_userial_baud(UART_TARGET_BAUD_RATE) \
                );

                /* read local name */
                UINT16_TO_STREAM(p, HCI_READ_LOCAL_NAME);
                *p = 0; /* parameter length */

                p_buf->len = HCI_CMD_PREAMBLE_SIZE;
                hw_cfg_cb.state = HW_CFG_READ_LOCAL_NAME;

                is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_READ_LOCAL_NAME, \
                                                    p_buf, hw_config_cback);
                break;

            case HW_CFG_READ_LOCAL_NAME:
                p_tmp = p_name = (char *) (p_evt_buf + 1) + \
                         HCI_EVT_CMD_CMPL_LOCAL_NAME_STRING;

                for (i=0; (i < LOCAL_NAME_BUFFER_LEN)||(*(p_name+i) != 0); i++)
                    *(p_name+i) = toupper(*(p_name+i));

                if ((p_name = strstr(p_name, "AIC")) != NULL)
                {
                    strncpy(hw_cfg_cb.local_chip_name, p_name, \
                            LOCAL_NAME_BUFFER_LEN-1);
                }
#ifdef USE_BLUETOOTH_AIC8800
                else if ((p_name = strstr(p_tmp, "8800")) != NULL)
                {
                    snprintf(hw_cfg_cb.local_chip_name,
                             LOCAL_NAME_BUFFER_LEN-1, "AIC%s", p_name);
                    strncpy(p_name, hw_cfg_cb.local_chip_name,
                            LOCAL_NAME_BUFFER_LEN-1);
                }
#endif
                else
                {
                    strncpy(hw_cfg_cb.local_chip_name, "UNKNOWN", \
                            LOCAL_NAME_BUFFER_LEN-1);
                    p_name = p_tmp;
                }

                hw_cfg_cb.local_chip_name[LOCAL_NAME_BUFFER_LEN-1] = 0;

                BTHWDBG("Chipset %s", hw_cfg_cb.local_chip_name);

                if ((status = hw_config_findpatch(p_name)) == TRUE)
                {
                    if ((hw_cfg_cb.fw_fd = open(p_name, O_RDONLY)) == -1)
                    {
                        ALOGE("vendor lib preload failed to open [%s]", p_name);
                    }
                    else
                    {
                        /* vsc_download_minidriver */
                        UINT16_TO_STREAM(p, HCI_VSC_DOWNLOAD_MINIDRV);
                        *p = 0; /* parameter length */

                        p_buf->len = HCI_CMD_PREAMBLE_SIZE;
                        hw_cfg_cb.state = HW_CFG_DL_MINIDRIVER;

                        is_proceeding = bt_vendor_cbacks->xmit_cb( \
                                            HCI_VSC_DOWNLOAD_MINIDRV, p_buf, \
                                            hw_config_cback);
                    }
                }
                else
                {
                    ALOGE( \
                    "vendor lib preload failed to locate firmware patch file" \
                    );
                }

                if (is_proceeding == FALSE)
                {
                    is_proceeding = hw_config_set_bdaddr(p_buf);
                }
                break;

            case HW_CFG_DL_MINIDRIVER:
                /* give time for placing firmware in download mode */
                ms_delay(50);
                hw_cfg_cb.state = HW_CFG_DL_FW_PATCH;
                /* fall through intentionally */
            case HW_CFG_DL_FW_PATCH:
                if (hw_cfg_cb.fw_fd >= 0)
                {
                    int ret = read(hw_cfg_cb.fw_fd, p, HCI_CMD_PREAMBLE_SIZE);
                    if (ret > 0)
                    {
                        if ((ret < HCI_CMD_PREAMBLE_SIZE) || \
                            (opcode == HCI_VSC_LAUNCH_RAM))
                        {
                            ALOGW("firmware patch file might be altered!");
                        }
                        else
                        {
                            p_buf->len = ret;
                            ret = read(hw_cfg_cb.fw_fd, \
                                       p+HCI_CMD_PREAMBLE_SIZE,\
                                       *(p+HCD_REC_PAYLOAD_LEN_BYTE));
                            if (ret >= 0)
                            {
                                p_buf->len += ret;
                                STREAM_TO_UINT16(opcode,p);
                                is_proceeding = bt_vendor_cbacks->xmit_cb(opcode, \
                                                        p_buf, hw_config_cback);
                                break;
                            }
                        }
                    }
                    if (ret < 0)
                    {
                        ALOGE("firmware patch file read failed (%s)", strerror(errno));
                    }
                    close(hw_cfg_cb.fw_fd);
                    hw_cfg_cb.fw_fd = -1;
                }
                /* Normally the firmware patch configuration file
                 * sets the new starting baud rate at 115200.
                 * So, we need update host's baud rate accordingly.
                 */
                ALOGI("bt vendor lib: set UART baud 115200");
                userial_vendor_set_baud(USERIAL_BAUD_115200);

                /* Next, we would like to boost baud rate up again
                 * to desired working speed.
                 */
                hw_cfg_cb.f_set_baud_2 = TRUE;

                /* Check if we need to pause a few hundred milliseconds
                 * before sending down any HCI command.
                 */
                delay = look_up_fw_settlement_delay();
                ALOGI("Setting fw settlement delay to %d ", delay);
                ms_delay(delay);

                p_buf->len = HCI_CMD_PREAMBLE_SIZE;
                UINT16_TO_STREAM(p, HCI_RESET);
                *p = 0; /* parameter length */
                hw_cfg_cb.state = HW_CFG_START;
                is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_config_cback);
                break;

            case HW_CFG_START:
		#if 1
		ALOGE("HW_CFG_START 11");
                is_proceeding = hw_config_set_bdaddr(p_buf);
                break;
		#else
                if (UART_TARGET_BAUD_RATE > 3000000)
                {
                    /* set UART clock to 48MHz */
                    UINT16_TO_STREAM(p, HCI_VSC_WRITE_UART_CLOCK_SETTING);
                    *p++ = 1; /* parameter length */
                    *p = 1; /* (1,"UART CLOCK 48 MHz")(2,"UART CLOCK 24 MHz") */

                    p_buf->len = HCI_CMD_PREAMBLE_SIZE + 1;
                    hw_cfg_cb.state = HW_CFG_SET_UART_CLOCK;

                    is_proceeding = bt_vendor_cbacks->xmit_cb( \
                                        HCI_VSC_WRITE_UART_CLOCK_SETTING, \
                                        p_buf, hw_config_cback);
                    break;
                }
		#endif
                /* fall through intentionally */
            case HW_CFG_SET_UART_CLOCK:
                /* set controller's UART baud rate to 3M */
                UINT16_TO_STREAM(p, HCI_VSC_UPDATE_BAUDRATE);
                *p++ = UPDATE_BAUDRATE_CMD_PARAM_SIZE; /* parameter length */
                *p++ = 0; /* encoded baud rate */
                *p++ = 0; /* use encoded form */
                UINT32_TO_STREAM(p, UART_TARGET_BAUD_RATE);

                p_buf->len = HCI_CMD_PREAMBLE_SIZE + \
                             UPDATE_BAUDRATE_CMD_PARAM_SIZE;
                hw_cfg_cb.state = (hw_cfg_cb.f_set_baud_2) ? \
                            HW_CFG_SET_UART_BAUD_2 : HW_CFG_SET_UART_BAUD_1;

                is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_VSC_UPDATE_BAUDRATE, \
                                                    p_buf, hw_config_cback);
                break;

            case HW_CFG_SET_UART_BAUD_2:
                /* update baud rate of host's UART port */
                ALOGI("bt vendor lib: set UART baud %i", UART_TARGET_BAUD_RATE);
                userial_vendor_set_baud( \
                    line_speed_to_userial_baud(UART_TARGET_BAUD_RATE) \
                );

#if (USE_CONTROLLER_BDADDR == TRUE)
                if ((is_proceeding = hw_config_read_bdaddr(p_buf)) == TRUE)
                    break;
#else
                if ((is_proceeding = hw_config_set_bdaddr(p_buf)) == TRUE)
                    break;
#endif
                /* fall through intentionally */
            case HW_CFG_SET_BD_ADDR:
                if(bt_rf_need_config == true)
                {
                    is_proceeding = hw_wr_rf_mdm_regs(p_buf);
                    break;
                }
                else
                {
                    if(AIC_RF_MODE_BT_COMBO == hw_get_bt_rf_mode())
                    {

                        is_proceeding = hw_aic_bt_pta_en(p_buf);
                        break;
                    }
                }
                is_proceeding = hw_aic_bt_wr_aon_params(p_buf);
                if(is_proceeding == true)
                    break;
                ALOGI("vendor lib fwcfg completed");
                bt_vendor_cbacks->dealloc(p_buf);
                bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);

                hw_cfg_cb.state = 0;

                if (hw_cfg_cb.fw_fd != -1)
                {
                    close(hw_cfg_cb.fw_fd);
                    hw_cfg_cb.fw_fd = -1;
                }

                is_proceeding = TRUE;

                break;
                case HW_CFG_WR_RF_MDM_REGS:
                #if 1
                    is_proceeding = hw_wr_rf_mdm_regs(p_buf);
                    ALOGE("AICHW_CFG %x\n",HW_CFG_WR_RF_MDM_REGS);
                 #endif
                    break;
                 case HW_CFG_WR_RF_MDM_REGS_END:
                    is_proceeding = hw_set_rf_mode(p_buf);
                    break;
                    case HW_CFG_SET_RF_MODE:
                #if 1
                    if(bt_rf_need_calib == true)
                    {
                        ALOGE("AICHW_CFG %x\n",HW_CFG_SET_RF_MODE);
                        is_proceeding = hw_rf_calib_req(p_buf);
                        break;
                    }
                 #endif
                    ///no break if no need to do rf calib
                    case HW_CFG_RF_CALIB_REQ:
                    ALOGE("AICHW_CFG %x\n",HW_CFG_RF_CALIB_REQ);

                    if(AIC_RF_MODE_BT_COMBO == hw_get_bt_rf_mode())
                    {

                        is_proceeding = hw_aic_bt_pta_en(p_buf);
                        break;
                    }
                    case HW_CFG_UPDATE_CONFIG_INFO:
                        is_proceeding = hw_aic_bt_wr_aon_params(p_buf);
                        if(is_proceeding == true)
                            break;
                    case HW_CFG_WR_AON_PARAM:
                        is_proceeding = hw_aic_bt_set_lp_level(p_buf);
                        if(is_proceeding == true)
                            break;
                    case HW_CFG_SET_LP_LEVEL:
                        aicbt_sleep_state = AICBT_SLEEP_STATE_CONFIG_ING;
                        is_proceeding = hw_aic_bt_set_pwr_ctrl_slave(p_buf);
                        if(is_proceeding == true)
                            break;
                    case HW_CFG_SET_PWR_CTRL_SLAVE:
                        is_proceeding = hw_aic_bt_set_cpu_poweroff_en(p_buf);
                        if(is_proceeding == true)
                            break;
                    case HW_CFG_SET_CPU_POWR_OFF_EN:
                        aicbt_sleep_state = AICBT_SLEEP_STATE_CONFIG_DONE;
                    ALOGI("vendor lib fwcfg completed");
                    bt_vendor_cbacks->dealloc(p_buf);
                    bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);

                    hw_cfg_cb.state = 0;

                    if (hw_cfg_cb.fw_fd != -1)
                    {
                        close(hw_cfg_cb.fw_fd);
                        hw_cfg_cb.fw_fd = -1;
                    }

                    is_proceeding = TRUE;
                    break;
#if (USE_CONTROLLER_BDADDR == TRUE)
            case HW_CFG_READ_BD_ADDR:
                p_tmp = (char *) (p_evt_buf + 1) + \
                         HCI_EVT_CMD_CMPL_LOCAL_BDADDR_ARRAY;

                if (memcmp(p_tmp, null_bdaddr, BD_ADDR_LEN) == 0)
                {
                    // Controller does not have a valid OTP BDADDR!
                    // Set the BTIF initial BDADDR instead.
                    if ((is_proceeding = hw_config_set_bdaddr(p_buf)) == TRUE)
                        break;
                }
                else
                {
                    ALOGI("Controller OTP bdaddr %02X:%02X:%02X:%02X:%02X:%02X",
                        *(p_tmp+5), *(p_tmp+4), *(p_tmp+3),
                        *(p_tmp+2), *(p_tmp+1), *p_tmp);
                }

                ALOGI("vendor lib fwcfg completed");
                bt_vendor_cbacks->dealloc(p_buf);
                bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);

                hw_cfg_cb.state = 0;

                if (hw_cfg_cb.fw_fd != -1)
                {
                    close(hw_cfg_cb.fw_fd);
                    hw_cfg_cb.fw_fd = -1;
                }

                is_proceeding = TRUE;
                break;
#endif // (USE_CONTROLLER_BDADDR == TRUE)
        } // switch(hw_cfg_cb.state)
    } // if (p_buf != NULL)

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if (is_proceeding == FALSE)
    {
        ALOGE("vendor lib fwcfg aborted!!!");
        if (bt_vendor_cbacks)
        {
            if (p_buf != NULL)
                bt_vendor_cbacks->dealloc(p_buf);

            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
        }

        if (hw_cfg_cb.fw_fd != -1)
        {
            close(hw_cfg_cb.fw_fd);
            hw_cfg_cb.fw_fd = -1;
        }

        hw_cfg_cb.state = 0;
    }
}

/******************************************************************************
**   LPM Static Functions
******************************************************************************/

/*******************************************************************************
**
** Function         hw_lpm_ctrl_cback
**
** Description      Callback function for lpm enable/disable rquest
**
** Returns          None
**
*******************************************************************************/
void hw_lpm_ctrl_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    bt_vendor_op_result_t status = BT_VND_OP_RESULT_FAIL;

    if (*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE) == 0)
    {
        status = BT_VND_OP_RESULT_SUCCESS;
    }
    ALOGE("hw_lpm_ctrl_cback:%x,%x \n",status,*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE));

    if (bt_vendor_cbacks)
    {
        bt_vendor_cbacks->lpm_cb(status);
        bt_vendor_cbacks->dealloc(p_evt_buf);
    }
}


#if (SCO_CFG_INCLUDED == TRUE)
/*****************************************************************************
**   SCO Configuration Static Functions
*****************************************************************************/

/*******************************************************************************
**
** Function         hw_sco_i2spcm_cfg_cback
**
** Description      Callback function for SCO I2S/PCM configuration rquest
**
** Returns          None
**
*******************************************************************************/
static void hw_sco_i2spcm_cfg_cback(void *p_mem)
{
    HC_BT_HDR   *p_evt_buf = (HC_BT_HDR *)p_mem;
    uint8_t     *p;
    uint16_t    opcode;
    HC_BT_HDR   *p_buf = NULL;
    bt_vendor_op_result_t status = BT_VND_OP_RESULT_FAIL;

    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    if (*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE) == 0)
    {
        status = BT_VND_OP_RESULT_SUCCESS;
    }

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if (status == BT_VND_OP_RESULT_SUCCESS)
    {
        if ((opcode == HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM) &&
            (SCO_INTERFACE_PCM == sco_bus_interface))
        {
            uint8_t ret = FALSE;

            /* Ask a new buffer to hold WRITE_SCO_PCM_INT_PARAM command */
            if (bt_vendor_cbacks)
                p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
                        BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + SCO_PCM_PARAM_SIZE);
            if (p_buf)
            {
                p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
                p_buf->offset = 0;
                p_buf->layer_specific = 0;
                p_buf->len = HCI_CMD_PREAMBLE_SIZE + SCO_PCM_PARAM_SIZE;
                p = (uint8_t *)(p_buf + 1);

                /* do we need this VSC for I2S??? */
                UINT16_TO_STREAM(p, HCI_VSC_WRITE_SCO_PCM_INT_PARAM);
                *p++ = SCO_PCM_PARAM_SIZE;
                memcpy(p, &bt_sco_param, SCO_PCM_PARAM_SIZE);
                ALOGI("SCO PCM configure {0x%x, 0x%x, 0x%x, 0x%x, 0x%x}",
                        bt_sco_param[0], bt_sco_param[1], bt_sco_param[2], bt_sco_param[3],
                        bt_sco_param[4]);
                if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_SCO_PCM_INT_PARAM, p_buf,
                        hw_sco_i2spcm_cfg_cback)) == FALSE)
                {
                    bt_vendor_cbacks->dealloc(p_buf);
                }
                else
                    return;
            }
            status = BT_VND_OP_RESULT_FAIL;
        }
        else if ((opcode == HCI_VSC_WRITE_SCO_PCM_INT_PARAM) &&
                 (SCO_INTERFACE_PCM == sco_bus_interface))
        {
            uint8_t ret = FALSE;

            /* Ask a new buffer to hold WRITE_PCM_DATA_FORMAT_PARAM command */
            if (bt_vendor_cbacks)
                p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
                        BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + PCM_DATA_FORMAT_PARAM_SIZE);
            if (p_buf)
            {
                p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
                p_buf->offset = 0;
                p_buf->layer_specific = 0;
                p_buf->len = HCI_CMD_PREAMBLE_SIZE + PCM_DATA_FORMAT_PARAM_SIZE;

                p = (uint8_t *)(p_buf + 1);
                UINT16_TO_STREAM(p, HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM);
                *p++ = PCM_DATA_FORMAT_PARAM_SIZE;
                memcpy(p, &bt_pcm_data_fmt_param, PCM_DATA_FORMAT_PARAM_SIZE);

                ALOGI("SCO PCM data format {0x%x, 0x%x, 0x%x, 0x%x, 0x%x}",
                        bt_pcm_data_fmt_param[0], bt_pcm_data_fmt_param[1],
                        bt_pcm_data_fmt_param[2], bt_pcm_data_fmt_param[3],
                        bt_pcm_data_fmt_param[4]);

                if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM,
                        p_buf, hw_sco_i2spcm_cfg_cback)) == FALSE)
                {
                    bt_vendor_cbacks->dealloc(p_buf);
                }
                else
                    return;
            }
            status = BT_VND_OP_RESULT_FAIL;
        }
    }

    ALOGI("sco I2S/PCM config result %d [0-Success, 1-Fail]", status);
    if (bt_vendor_cbacks)
    {
        bt_vendor_cbacks->audio_state_cb(status);
    }
}

/*******************************************************************************
**
** Function         hw_set_MSBC_codec_cback
**
** Description      Callback function for setting WBS codec
**
** Returns          None
**
*******************************************************************************/
static void hw_set_MSBC_codec_cback(void *p_mem)
{
    /* whenever update the codec enable/disable, need to update I2SPCM */
    ALOGI("SCO I2S interface change the sample rate to 16K");
    hw_sco_i2spcm_config_from_command(p_mem, SCO_CODEC_MSBC);
}

/*******************************************************************************
**
** Function         hw_set_CVSD_codec_cback
**
** Description      Callback function for setting NBS codec
**
** Returns          None
**
*******************************************************************************/
static void hw_set_CVSD_codec_cback(void *p_mem)
{
    /* whenever update the codec enable/disable, need to update I2SPCM */
    ALOGI("SCO I2S interface change the sample rate to 8K");
    hw_sco_i2spcm_config_from_command(p_mem, SCO_CODEC_CVSD);
}

#endif // SCO_CFG_INCLUDED

/*****************************************************************************
**   Hardware Configuration Interface Functions
*****************************************************************************/


/*******************************************************************************
**
** Function        hw_config_start
**
** Description     Kick off controller initialization process
**
** Returns         None
**
*******************************************************************************/
void hw_config_start(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    hw_cfg_cb.state = 0;
    hw_cfg_cb.fw_fd = -1;
    hw_cfg_cb.f_set_baud_2 = FALSE;

    /* Start from sending HCI_RESET */

    if (bt_vendor_cbacks)
    {
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
    }

    ALOGE("hw_config_start\n");
    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_RESET);
        *p = 0; /* parameter length */

        hw_cfg_cb.state = HW_CFG_START;
        ///reset slee state
        aicbt_sleep_state = AICBT_SLEEP_STATE_IDLE;
        bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_config_cback);
    }
    else
    {
        if (bt_vendor_cbacks)
        {
            ALOGE("vendor lib fw conf aborted [no buffer]");
            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}

/*******************************************************************************
**
** Function        hw_lpm_enable
**
** Description     Enalbe/Disable LPM
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
uint8_t hw_lpm_enable(uint8_t turn_on)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;
    uint8_t     ret = FALSE;

    if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE + \
                                                       LPM_CMD_PARAM_SIZE);

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + LPM_CMD_PARAM_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_VSC_WRITE_SLEEP_MODE);
        *p++ = LPM_CMD_PARAM_SIZE; /* parameter length */

        if (turn_on)
        {
            memcpy(p, &lpm_param, LPM_CMD_PARAM_SIZE);
            upio_set(UPIO_LPM_MODE, UPIO_ASSERT, 0);
        }
        else
        {
            memset(p, 0, LPM_CMD_PARAM_SIZE);
            upio_set(UPIO_LPM_MODE, UPIO_DEASSERT, 0);
        }

        if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_SLEEP_MODE, p_buf, \
                                        hw_lpm_ctrl_cback)) == FALSE)
        {
            bt_vendor_cbacks->dealloc(p_buf);
        }
    }

    if ((ret == FALSE) && bt_vendor_cbacks)
        bt_vendor_cbacks->lpm_cb(BT_VND_OP_RESULT_FAIL);

    return ret;
}

bool hw_lm_direct_return(uint8_t turn_on)
{

    if (bt_vendor_cbacks)
    {
        bt_vendor_cbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
    }
    return true;
}

uint8_t hw_lpm_set_sleep_enable(uint8_t turn_on)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;
    uint8_t     ret = FALSE;
    ///hw_lm_direct_return(turn_on);
    ///return true;
    if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE + \
                                                       HCI_VSC_SET_SLEEP_EN_SIZE);
    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_SLEEP_EN_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_VSC_SET_SLEEP_EN_CMD);
        *p++ = (uint8_t)HCI_VSC_SET_SLEEP_EN_SIZE; /* parameter length */
        ALOGE("hw_lpm_set_sleep_enable:%x \n",turn_on);
        if (turn_on)
        {
            upio_set(UPIO_LPM_MODE, UPIO_ASSERT, 0);
            UINT8_TO_STREAM(p, 1);
            UINT8_TO_STREAM(p, 0);
        }
        else
        {
            UINT8_TO_STREAM(p, 0);
            UINT8_TO_STREAM(p, 0);
            upio_set(UPIO_LPM_MODE, UPIO_DEASSERT, 0);
        }

        p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_SLEEP_EN_SIZE;
        if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_SET_SLEEP_EN_CMD, p_buf, \
                                  hw_lpm_ctrl_cback)) == FALSE)
        {
            bt_vendor_cbacks->dealloc(p_buf);
        }

    }
    if ((ret == FALSE) && bt_vendor_cbacks)
        bt_vendor_cbacks->lpm_cb(BT_VND_OP_RESULT_FAIL);

    return ret;
}

/*******************************************************************************
**
** Function        hw_lpm_get_idle_timeout
**
** Description     Calculate idle time based on host stack idle threshold
**
** Returns         idle timeout value
**
*******************************************************************************/
uint32_t hw_lpm_get_idle_timeout(void)
{
    uint32_t timeout_ms;

    /* set idle time to be LPM_IDLE_TIMEOUT_MULTIPLE times of
     * host stack idle threshold (in 300ms/25ms)
     */
    timeout_ms = (uint32_t)lpm_param.host_stack_idle_threshold \
                            * LPM_IDLE_TIMEOUT_MULTIPLE;
    if (strstr(hw_cfg_cb.local_chip_name, "AIC8800") != NULL)
        timeout_ms *= 25; // 12.5 or 25 ?
    else if (strstr(hw_cfg_cb.local_chip_name, "AIC8800") != NULL)
        timeout_ms *= 50;
    else
        timeout_ms *= 300;

    return timeout_ms;
}

/*******************************************************************************
**
** Function        hw_lpm_set_wake_state
**
** Description     Assert/Deassert BT_WAKE
**
** Returns         None
**
*******************************************************************************/
void hw_lpm_set_wake_state(uint8_t wake_assert)
{
    uint8_t state = (wake_assert) ? UPIO_ASSERT : UPIO_DEASSERT;
    ALOGE("set_wake_stat %x\n",wake_assert);
    upio_set(UPIO_BT_WAKE, state, lpm_param.bt_wake_polarity);
}

#if (SCO_CFG_INCLUDED == TRUE)
/*******************************************************************************
**
** Function         hw_sco_config
**
** Description      Configure SCO related hardware settings
**
** Returns          None
**
*******************************************************************************/
void hw_sco_config(void)
{
    if (SCO_INTERFACE_I2S == sco_bus_interface)
    {
        /* 'Enable' I2S mode */
        bt_sco_i2spcm_param[0] = 1;

        /* set nbs clock rate as the value in SCO_I2SPCM_IF_CLOCK_RATE field */
        sco_bus_clock_rate = bt_sco_i2spcm_param[3];
    }
    else
    {
        /* 'Disable' I2S mode */
        bt_sco_i2spcm_param[0] = 0;

        /* set nbs clock rate as the value in SCO_PCM_IF_CLOCK_RATE field */
        sco_bus_clock_rate = bt_sco_param[1];

        /* sync up clock mode setting */
        bt_sco_i2spcm_param[1] = bt_sco_param[4];
    }

    if (sco_bus_wbs_clock_rate == INVALID_SCO_CLOCK_RATE)
    {
        /* set default wbs clock rate */
        sco_bus_wbs_clock_rate = SCO_I2SPCM_IF_CLOCK_RATE4WBS;

        if (sco_bus_wbs_clock_rate < sco_bus_clock_rate)
            sco_bus_wbs_clock_rate = sco_bus_clock_rate;
    }

    /*
     *  To support I2S/PCM port multiplexing signals for sharing Bluetooth audio
     *  and FM on the same PCM pins, we defer Bluetooth audio (SCO/eSCO)
     *  configuration till SCO/eSCO is being established;
     *  i.e. in hw_set_audio_state() call.
     *  When configured as I2S only, Bluetooth audio configuration is executed
     *  immediately with SCO_CODEC_CVSD by default.
     */

    if (SCO_INTERFACE_I2S == sco_bus_interface) {
        hw_sco_i2spcm_config(SCO_CODEC_CVSD);
    }

    if (bt_vendor_cbacks)
    {
        bt_vendor_cbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
    }
}

static void hw_sco_i2spcm_config_from_command(void *p_mem, uint16_t codec) {
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *)p_mem;
    bool command_success = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE) == 0;

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if (command_success)
        hw_sco_i2spcm_config(codec);
    else if (bt_vendor_cbacks)
        bt_vendor_cbacks->audio_state_cb(BT_VND_OP_RESULT_FAIL);
}


/*******************************************************************************
**
** Function         hw_sco_i2spcm_config
**
** Description      Configure SCO over I2S or PCM
**
** Returns          None
**
*******************************************************************************/
static void hw_sco_i2spcm_config(uint16_t codec)
{
    HC_BT_HDR *p_buf = NULL;
    uint8_t *p, ret;
    uint16_t cmd_u16 = HCI_CMD_PREAMBLE_SIZE + SCO_I2SPCM_PARAM_SIZE;

    if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + cmd_u16);

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = cmd_u16;

        p = (uint8_t *)(p_buf + 1);

        UINT16_TO_STREAM(p, HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM);
        *p++ = SCO_I2SPCM_PARAM_SIZE;
        if (codec == SCO_CODEC_CVSD)
        {
            bt_sco_i2spcm_param[2] = 0; /* SCO_I2SPCM_IF_SAMPLE_RATE  8k */
            bt_sco_i2spcm_param[3] = bt_sco_param[1] = sco_bus_clock_rate;
        }
        else if (codec == SCO_CODEC_MSBC)
        {
            bt_sco_i2spcm_param[2] = wbs_sample_rate; /* SCO_I2SPCM_IF_SAMPLE_RATE 16K */
            bt_sco_i2spcm_param[3] = bt_sco_param[1] = sco_bus_wbs_clock_rate;
        }
        else
        {
            bt_sco_i2spcm_param[2] = 0; /* SCO_I2SPCM_IF_SAMPLE_RATE  8k */
            bt_sco_i2spcm_param[3] = bt_sco_param[1] = sco_bus_clock_rate;
            ALOGE("wrong codec is use in hw_sco_i2spcm_config, goes default NBS");
        }
        memcpy(p, &bt_sco_i2spcm_param, SCO_I2SPCM_PARAM_SIZE);
        cmd_u16 = HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM;
        ALOGI("I2SPCM config {0x%x, 0x%x, 0x%x, 0x%x}",
                bt_sco_i2spcm_param[0], bt_sco_i2spcm_param[1],
                bt_sco_i2spcm_param[2], bt_sco_i2spcm_param[3]);

        if ((ret = bt_vendor_cbacks->xmit_cb(cmd_u16, p_buf, hw_sco_i2spcm_cfg_cback)) == FALSE)
        {
            bt_vendor_cbacks->dealloc(p_buf);
        }
        else
            return;
    }

    bt_vendor_cbacks->audio_state_cb(BT_VND_OP_RESULT_FAIL);
}

/*******************************************************************************
**
** Function         hw_set_SCO_codec
**
** Description      This functgion sends command to the controller to setup
**                              WBS/NBS codec for the upcoming eSCO connection.
**
** Returns          -1 : Failed to send VSC
**                   0 : Success
**
*******************************************************************************/
static int hw_set_SCO_codec(uint16_t codec)
{
    HC_BT_HDR   *p_buf = NULL;
    uint8_t     *p;
    uint8_t     ret;
    int         ret_val = 0;
    tINT_CMD_CBACK p_set_SCO_codec_cback;

    BTHWDBG( "hw_set_SCO_codec 0x%x", codec);

    if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
                BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + SCO_CODEC_PARAM_SIZE);

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p = (uint8_t *)(p_buf + 1);

        UINT16_TO_STREAM(p, HCI_VSC_ENABLE_WBS);

        if (codec == SCO_CODEC_MSBC)
        {
            /* Enable mSBC */
            *p++ = SCO_CODEC_PARAM_SIZE; /* set the parameter size */
            UINT8_TO_STREAM(p,1); /* enable */
            UINT16_TO_STREAM(p, codec);

            /* set the totall size of this packet */
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + SCO_CODEC_PARAM_SIZE;

            p_set_SCO_codec_cback = hw_set_MSBC_codec_cback;
        }
        else
        {
            /* Disable mSBC */
            *p++ = (SCO_CODEC_PARAM_SIZE - 2); /* set the parameter size */
            UINT8_TO_STREAM(p,0); /* disable */

            /* set the totall size of this packet */
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + SCO_CODEC_PARAM_SIZE - 2;

            p_set_SCO_codec_cback = hw_set_CVSD_codec_cback;
            if ((codec != SCO_CODEC_CVSD) && (codec != SCO_CODEC_NONE))
            {
                ALOGW("SCO codec setting is wrong: codec: 0x%x", codec);
            }
        }

        if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_ENABLE_WBS, p_buf, p_set_SCO_codec_cback))\
              == FALSE)
        {
            bt_vendor_cbacks->dealloc(p_buf);
            ret_val = -1;
        }
    }
    else
    {
        ret_val = -1;
    }

    return ret_val;
}

/*******************************************************************************
**
** Function         hw_set_audio_state
**
** Description      This function configures audio base on provided audio state
**
** Paramters        pointer to audio state structure
**
** Returns          0: ok, -1: error
**
*******************************************************************************/
int hw_set_audio_state(bt_vendor_op_audio_state_t *p_state)
{
    int ret_val = -1;

    if (!bt_vendor_cbacks)
        return ret_val;

    ret_val = hw_set_SCO_codec(p_state->peer_codec);
    return ret_val;
}

#else  // SCO_CFG_INCLUDED
int hw_set_audio_state(bt_vendor_op_audio_state_t *p_state)
{
    return -256;
}
#endif
/*******************************************************************************
**
** Function        hw_set_patch_file_path
**
** Description     Set the location of firmware patch file
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_file_path(char *p_conf_name, char *p_conf_value, int param)
{

    strcpy(fw_patchfile_path, p_conf_value);

    return 0;
}

/*******************************************************************************
**
** Function        hw_set_patch_file_name
**
** Description     Give the specific firmware patch filename
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_file_name(char *p_conf_name, char *p_conf_value, int param)
{

    strcpy(fw_patchfile_name, p_conf_value);

    return 0;
}

#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
/*******************************************************************************
**
** Function        hw_set_patch_settlement_delay
**
** Description     Give the specific firmware patch settlement time in milliseconds
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int hw_set_patch_settlement_delay(char *p_conf_name, char *p_conf_value, int param)
{
    fw_patch_settlement_delay = atoi(p_conf_value);

    return 0;
}
#endif  //VENDOR_LIB_RUNTIME_TUNING_ENABLED

/*****************************************************************************
**   Sample Codes Section
*****************************************************************************/

#if (HW_END_WITH_HCI_RESET == TRUE)
/*******************************************************************************
**
** Function         hw_epilog_cback
**
** Description      Callback function for Command Complete Events from HCI
**                  commands sent in epilog process.
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_cback(void *p_mem)
{
    HC_BT_HDR   *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t     *p, status;
    uint16_t    opcode;

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode,p);

    BTHWDBG("%s Opcode:0x%04X Status: %d", __FUNCTION__, opcode, status);

    if (bt_vendor_cbacks)
    {
        /* Must free the RX event buffer */
        bt_vendor_cbacks->dealloc(p_evt_buf);

        /* Once epilog process is done, must call epilog_cb callback
           to notify caller */
        bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
    }
}

/*******************************************************************************
**
** Function         hw_epilog_process
**
** Description      Sample implementation of epilog process
**
** Returns          None
**
*******************************************************************************/
void hw_epilog_process(void)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    BTHWDBG("hw_epilog_process");

    /* Sending a HCI_RESET */
    if (bt_vendor_cbacks)
    {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
    }

    if (p_buf)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_RESET);
        *p = 0; /* parameter length */

        /* Send command via HC's xmit_cb API */
        bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_epilog_cback);
    }
    else
    {
        if (bt_vendor_cbacks)
        {
            ALOGE("vendor lib epilog process aborted [no buffer]");
            bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}
#endif // (HW_END_WITH_HCI_RESET == TRUE)

bool hw_bt_drv_rf_mdm_regs_entry_get(uint32_t *addr, uint32_t *val)
{
    bool ret = false;
    uint32_t table_size = 0;
    uint32_t table_ele_size = 0;

    uint32_t rf_mode = hw_get_bt_rf_mode() ;
    if(rf_mode == AIC_RF_MODE_BT_ONLY)
    {
        table_size = sizeof(rf_mdm_regs_table_bt_only);
        table_ele_size = sizeof(rf_mdm_regs_table_bt_only[0]);
        *addr = rf_mdm_regs_table_bt_only[rf_mdm_table_index][0];
        *val    = rf_mdm_regs_table_bt_only[rf_mdm_table_index][1];
    }
    if(rf_mode == AIC_RF_MODE_BT_COMBO)
    {
        table_size = sizeof(rf_mdm_regs_table_bt_combo);
        table_ele_size = sizeof(rf_mdm_regs_table_bt_combo[0]);
        *addr = rf_mdm_regs_table_bt_combo[rf_mdm_table_index][0];
        *val    = rf_mdm_regs_table_bt_combo[rf_mdm_table_index][1];
    }

    if(table_size == 0 || rf_mdm_table_index > (table_size/table_ele_size -1))
    {
        return ret;
    }
    rf_mdm_table_index++;
    ret = true;
    return ret;
}
uint32_t rf_mdm_regs_offset = 0;
bool hw_wr_rf_mdm_regs(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_WR_RF_MDM_REGS_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_WR_RF_MDM_REGS_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
		///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
		///p_buf->offset = 0;
		///p_buf->layer_specific = 0;
		///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_WR_RF_MDM_REGS_SIZE;

		p = (uint8_t *) (p_buf + 1);
		UINT16_TO_STREAM(p, HCI_VSC_WR_RF_MDM_REGS_CMD);
		*p++ = (uint8_t)HCI_VSC_WR_RF_MDM_REGS_SIZE; /* parameter length */
             uint32_t addr,val;
             uint8_t i = 0;
             uint8_t len = 0;
             uint8_t 	*p_data = p+4;
             for(i = 0;i < 30;i++)
             {
                if(hw_bt_drv_rf_mdm_regs_entry_get(&addr,&val))
                {
                    UINT32_TO_STREAM(p_data,addr);
                    UINT32_TO_STREAM(p_data,val);
                }
                else
                {
                    break;
                }
             }

             len = i*8;///;
             UINT16_TO_STREAM(p,rf_mdm_regs_offset);
             *p++ = 0;
             *p++ = len;
             if(i == 30)
             {
                rf_mdm_regs_offset += len;
             }
             else
             {
                 rf_mdm_regs_offset = 0;
             }

            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_WR_RF_MDM_REGS_SIZE;
            hw_cfg_cb.state = HW_CFG_WR_RF_MDM_REGS;

            ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WR_RF_MDM_REGS_CMD, p_buf, \
                                         hw_config_cback);
            ///all regs has been sent,go to next state
            if(rf_mdm_regs_offset == 0)
            {
                hw_cfg_cb.state = HW_CFG_WR_RF_MDM_REGS_END;
            }
	}

	return ret;
}


aicbt_rf_mode hw_get_bt_rf_mode(void)
{
    return bt_rf_mode;
}
void hw_set_bt_rf_mode(aicbt_rf_mode mode)
{
    bt_rf_mode = mode;
}

bool hw_set_rf_mode(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_SET_RF_MODE_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
		///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
		///p_buf->offset = 0;
		///p_buf->layer_specific = 0;
		///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;

		p = (uint8_t *) (p_buf + 1);
		UINT16_TO_STREAM(p, HCI_VSC_SET_RF_MODE_CMD);
		*p++ = HCI_VSC_SET_RF_MODE_SIZE; /* parameter length */

             *p =  hw_get_bt_rf_mode();

              p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;
              hw_cfg_cb.state = HW_CFG_SET_RF_MODE;

              ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_SET_RF_MODE_CMD, p_buf, \
                                     hw_config_cback);
	}

	return ret;
}

bool hw_rf_calib_req(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_RF_CALIB_REQ_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_RF_CALIB_REQ_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
            ///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            ///p_buf->offset = 0;
            ///p_buf->layer_specific = 0;
            ///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;

            p = (uint8_t *) (p_buf + 1);
            UINT16_TO_STREAM(p, HCI_VSC_RF_CALIB_REQ_CMD);
            *p++ = (uint8_t)HCI_VSC_RF_CALIB_REQ_SIZE; /* parameter length */
            struct hci_rf_calib_req_cmd *rf_calib_req = NULL;

            if(hw_get_bt_rf_mode() ==  AIC_RF_MODE_BT_ONLY)
            {
                rf_calib_req = (struct hci_rf_calib_req_cmd *)&rf_calib_req_bt_only;
            }
            else
            {
                rf_calib_req = (struct hci_rf_calib_req_cmd *)&rf_calib_req_bt_combo;
            }
            UINT8_TO_STREAM(p, rf_calib_req->calib_type);
            UINT16_TO_STREAM(p, rf_calib_req->offset);
            *p++ = rf_calib_req->buff.length;
            memcpy(p, (void *)&rf_calib_req->buff.data[0], rf_calib_req->buff.length);
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_RF_CALIB_REQ_SIZE;
            hw_cfg_cb.state = HW_CFG_RF_CALIB_REQ;

            ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_RF_CALIB_REQ_CMD, p_buf, \
                                 hw_config_cback);
	}

	return ret;
}

bool hw_aic_bt_pta_en(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_UPDATE_CONFIG_INFO_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_UPDATE_CONFIG_INFO_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
		///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
		///p_buf->offset = 0;
		///p_buf->layer_specific = 0;
		///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;

		p = (uint8_t *) (p_buf + 1);
		UINT16_TO_STREAM(p, HCI_VSC_UPDATE_CONFIG_INFO_CMD);
		*p++ = (uint8_t)HCI_VSC_UPDATE_CONFIG_INFO_SIZE; /* parameter length */

              UINT16_TO_STREAM(p, AICBT_CONFIG_ID_PTA_EN);
              UINT16_TO_STREAM(p, sizeof(struct aicbt_pta_config));
              memcpy(p, (void *)&pta_config, sizeof(struct aicbt_pta_config));
              p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_UPDATE_CONFIG_INFO_SIZE;
              hw_cfg_cb.state = HW_CFG_UPDATE_CONFIG_INFO;
              aicbt_up_config_info_state = VS_UPDATE_CONFIG_INFO_STATE_PTA_EN;
              ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_UPDATE_CONFIG_INFO_CMD, p_buf, \
                                     hw_config_cback);
	}

	return ret;
}


bool hw_aic_bt_set_lp_level(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_SET_LP_LEVEL_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_LP_LEVEL_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
		///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
		///p_buf->offset = 0;
		///p_buf->layer_specific = 0;
		///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;

		p = (uint8_t *) (p_buf + 1);
		UINT16_TO_STREAM(p, HCI_VSC_SET_LP_LEVEL_CMD);
		*p++ = (uint8_t)HCI_VSC_SET_LP_LEVEL_SIZE; /* parameter length */

              UINT8_TO_STREAM(p, aicbt_lp_level);

              p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_LP_LEVEL_SIZE;
              hw_cfg_cb.state = HW_CFG_SET_LP_LEVEL;
              ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_SET_LP_LEVEL_CMD, p_buf, \
                                     hw_config_cback);
	}

	return ret;
}

void hw_set_sleep_en_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    ALOGE("hw_set_sleep_en_cback\n");

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

}


bool hw_aic_bt_set_sleep_en(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;


        if(aicbt_sleep_state == AICBT_SLEEP_STATE_CONFIG_DONE)
        {
           if (p_buf == NULL)
           {
               if (bt_vendor_cbacks)
                   p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                                  HCI_CMD_PREAMBLE_SIZE + \
                                                                  HCI_VSC_SET_SLEEP_EN_SIZE);
               if (p_buf)
               {
                   p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
                   p_buf->offset = 0;
                   p_buf->layer_specific = 0;
                   p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_SLEEP_EN_SIZE;
               }
               else
                   return ret;

           }

           if (p_buf)
           {

               p = (uint8_t *) (p_buf + 1);
               UINT16_TO_STREAM(p, HCI_VSC_SET_SLEEP_EN_CMD);
               *p++ = (uint8_t)HCI_VSC_SET_SLEEP_EN_SIZE; /* parameter length */
               ALOGE("hw_aic_bt_set_sleep_en \n");

               UINT8_TO_STREAM(p, 1);
               UINT8_TO_STREAM(p, 0);

               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_SLEEP_EN_SIZE;
               if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_SET_SLEEP_EN_CMD, p_buf, \
                                         hw_set_sleep_en_cback)) == FALSE)
               {
                   bt_vendor_cbacks->dealloc(p_buf);
               }

           }

        }

	return ret;
}

bool hw_aic_bt_wr_aon_params(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_WR_AON_PARAM_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_WR_AON_PARAM_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
		///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
		///p_buf->offset = 0;
		///p_buf->layer_specific = 0;
		///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;

		p = (uint8_t *) (p_buf + 1);
		UINT16_TO_STREAM(p, HCI_VSC_WR_AON_PARAM_CMD);
		*p++ = (uint8_t)HCI_VSC_WR_AON_PARAM_SIZE; /* parameter length */
             memcpy(p, &wr_aon_param, HCI_VSC_WR_AON_PARAM_SIZE);
             ALOGI("HW_CFG_WR_AON_PARAM");

              p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_WR_AON_PARAM_SIZE;
              hw_cfg_cb.state = HW_CFG_WR_AON_PARAM;
              ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WR_AON_PARAM_CMD, p_buf, \
                                     hw_config_cback);
	}

	return ret;
}

bool hw_aic_bt_set_pwr_ctrl_slave(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_SET_PWR_CTRL_SLAVE_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_PWR_CTRL_SLAVE_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
		p = (uint8_t *) (p_buf + 1);
		UINT16_TO_STREAM(p, HCI_VSC_SET_PWR_CTRL_SLAVE_CMD);
		*p++ = (uint8_t)HCI_VSC_SET_PWR_CTRL_SLAVE_SIZE; /* parameter length */
             *p = 1;

             ALOGI("HW_CFG_SET_PWR_CTRL_SLAVE");
              p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_PWR_CTRL_SLAVE_SIZE;
              hw_cfg_cb.state = HW_CFG_SET_PWR_CTRL_SLAVE;
              ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_SET_PWR_CTRL_SLAVE_CMD, p_buf, \
                                     hw_config_cback);
	}

	return ret;
}

bool hw_aic_bt_set_cpu_poweroff_en(HC_BT_HDR *p_buf)
{
	///HC_BT_HDR  *p_buf = NULL;
	uint8_t 	*p;
	bool 	ret = FALSE;
       if (p_buf == NULL)
       {
           if (bt_vendor_cbacks)
               p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_SET_CPU_POWER_OFF_SIZE);
           if (p_buf)
           {
               p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
               p_buf->offset = 0;
               p_buf->layer_specific = 0;
               p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_CPU_POWER_OFF_SIZE;
           }
           else
                return ret;
       }

	if (p_buf)
	{
		p = (uint8_t *) (p_buf + 1);
		UINT16_TO_STREAM(p, HCI_VSC_SET_CPU_POWER_OFF_CMD);
		*p++ = (uint8_t)HCI_VSC_SET_CPU_POWER_OFF_SIZE; /* parameter length */
             *p = 1;

             ALOGI("HW_CFG_SET_CPU_POWR_OFF_EN");
              p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_CPU_POWER_OFF_SIZE;
              hw_cfg_cb.state = HW_CFG_SET_CPU_POWR_OFF_EN;
              ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_SET_CPU_POWER_OFF_CMD, p_buf, \
                                     hw_config_cback);
	}

	return ret;
}


