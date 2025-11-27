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
#define AICBT_RELEASE_NAME "2025_0515_BT_ANDROID_4.4-14"

#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor_aic.h"
#include "userial.h"
#include "userial_vendor.h"
#include "upio.h"
#include <unistd.h>
#include <endian.h>
#include <byteswap.h>
#include <unistd.h>

#include "bt_vendor_lib.h"
#include "hardware.h"

#include <sys/inotify.h>
#include <pthread.h>

static bool inotify_pthread_running = false;
static pthread_t inotify_pthread_id = -1;

/******************************************************************************
**  Constants &  Macros
******************************************************************************/
#define CO_32(p)                                (p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24))

/******************************************************************************
**  Externs
******************************************************************************/
extern uint8_t vnd_local_bd_addr[BD_ADDR_LEN];

uint32_t aicbt_up_config_info_state = VS_UPDATE_CONFIG_INFO_STATE_IDLE;
uint32_t rf_mdm_table_index = 0;
aicbt_rf_mode bt_rf_mode = AIC_RF_MODE_BT_ONLY; ///AIC_RF_MODE_BT_COMBO;///AIC_RF_MODE_BT_ONLY;
bool bt_rf_need_config = false;
bool bt_rf_need_calib = true;
uint32_t rf_mdm_regs_offset = 0;

const uint32_t rf_mdm_regs_table_bt_only[][2] = {
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

const uint32_t rf_mdm_regs_table_bt_combo[][2] = {
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

const struct aicbt_pta_config pta_config = {
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

struct hci_rf_calib_req_cmd rf_calib_req_bt_only = {AIC_RF_MODE_BT_ONLY, 0x0000, {0x08, {0x13,0x42,0x26,0x00,0x0f,0x30,0x02,0x00}}};
struct hci_rf_calib_req_cmd rf_calib_req_bt_combo = {AIC_RF_MODE_BT_COMBO, 0x0000, {0x04, {0x03,0x42,0x26,0x00}}};

bt_hw_cfg_cb_t hw_cfg_cb;

#if (SCO_CFG_INCLUDED == TRUE)

#include "esco_parameters.h"
/* one byte is for enable/disable
      next 2 bytes are for codec type */
#define SCO_CODEC_PARAM_SIZE                    3
#define SCO_INTERFACE_PCM                       0
#define SCO_INTERFACE_I2S                       1
#define INVALID_SCO_CLOCK_RATE                  0xFF

#define HCI_VSC_WRITE_SCO_PCM_INT_PARAM         0xFC1C
#define HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM     0xFC1E
#define HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM    0xFC6D
#define HCI_VSC_ENABLE_WBS                      0xFC7E
#define HCI_BLE_ADV_FILTER                      0xFD57 // APCF command

/* need to update the bt_sco_i2spcm_param as well
   bt_sco_i2spcm_param will be used for WBS setting
   update the bt_sco_param and bt_sco_i2spcm_param */
static uint8_t bt_sco_param[SCO_PCM_PARAM_SIZE] = {
    SCO_PCM_ROUTING,
    SCO_PCM_IF_CLOCK_RATE,
    SCO_PCM_IF_FRAME_TYPE,
    SCO_PCM_IF_SYNC_MODE,
    SCO_PCM_IF_CLOCK_MODE
};

static uint8_t bt_pcm_data_fmt_param[PCM_DATA_FORMAT_PARAM_SIZE] = {
    PCM_DATA_FMT_SHIFT_MODE,
    PCM_DATA_FMT_FILL_BITS,
    PCM_DATA_FMT_FILL_METHOD,
    PCM_DATA_FMT_FILL_NUM,
    PCM_DATA_FMT_JUSTIFY_MODE
};

static uint8_t bt_sco_i2spcm_param[SCO_I2SPCM_PARAM_SIZE] = {
    SCO_I2SPCM_IF_MODE,
    SCO_I2SPCM_IF_ROLE,
    SCO_I2SPCM_IF_SAMPLE_RATE,
    SCO_I2SPCM_IF_CLOCK_RATE
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
static uint8_t sco_bus_clock_rate = INVALID_SCO_CLOCK_RATE;
static uint8_t sco_bus_wbs_clock_rate = INVALID_SCO_CLOCK_RATE;
static int wbs_sample_rate = SCO_WBS_SAMPLE_RATE;

static void hw_sco_i2spcm_config_from_command(void *p_mem, uint16_t codec);
static void hw_sco_i2spcm_config(uint16_t codec);

#endif

//FOR AIC CVSD PCM SETTING START
#define BT_PCM_PARAM_SIZE   8

/* PCM_SETTING
    FALSE : Not set pcm param
    TRUE : set pcm param
*/
#ifndef PCM_SETTING
#define PCM_SETTING FALSE
#endif

/* PCM_ROUTING
    0 : VoHCI
    1 : PCM
*/
#ifndef PCM_ROUTING
#define PCM_ROUTING 0
#endif

/* PCM_ROLE
    0 : mono for master, only support long  <pcm_fsync>
    1 : mono for slave, support long/short <pcm_fsync>
    2 : stereo for master, only support long  <pcm_fsync>
    3 : stereo for slave, support long/short <pcm_fsync>
*/
#ifndef PCM_ROLE
#define PCM_ROLE 0
#endif

/* PCM_OUT_EDGE
    0 : data align with posedge <pcm_clk>
    1 : data align with negedge <pcm_clk>
*/
#ifndef PCM_OUT_EDGE
#define PCM_OUT_EDGE 0
#endif

/* PCM_SYNC_MODE
    0 : first bit align  with <pcm_fsync> bit0 no delay 
    1 : first bit follow with <pcm_fsync> bit1 delay 1 bit
    (master only support 0)
*/
#ifndef PCM_SYNC_MODE
#define PCM_SYNC_MODE 0
#endif

/* PCM_DATA_SHIFT_MODE
    0 : msb first
    1 : lsb first
*/
#ifndef PCM_DATA_SHIFT_MODE
#define PCM_DATA_SHIFT_MODE 0
#endif

/* PCM_CLOCK_MODE
    0 : 128K
    1 : 256K
    2 : 512K
    3 : 1024K
    4 : 2048K
    
pcm_bclk = 8k(SIMPLE_RATE) * 16bits(CVSD data bits) * 2(left and right channels) = 256k

*/
#ifndef PCM_CLOCK_RATE
#define PCM_CLOCK_RATE 1
#endif

/* PCM_SIMPLE_RATE
    0 : 8K
    1 : 16K
    2 : 4K
*/
#ifndef PCM_SIMPLE_RATE
#define PCM_SIMPLE_RATE 0
#endif

/* PCM_IO_SEL
    0x00 : gpioa 0 -3
    0x01 : gpioa 4 -7
    0xFF : undefined
*/
#ifndef PCM_IO_SEL
#define PCM_IO_SEL 0x00
#endif

static uint8_t bt_pcm_param[BT_PCM_PARAM_SIZE] = {
    PCM_ROUTING,
    PCM_ROLE,
    PCM_OUT_EDGE,
    PCM_SYNC_MODE,
    PCM_DATA_SHIFT_MODE,
    PCM_CLOCK_RATE,
    PCM_SIMPLE_RATE,
    PCM_IO_SEL
};
//FOR AIC CVSD PCM SETTING END



void (*hw_config_cback)(void *p_mem);

aicbt_rf_mode hw_get_bt_rf_mode(void)
{
    return bt_rf_mode;
}

void hw_set_bt_rf_mode(aicbt_rf_mode mode)
{
    bt_rf_mode = mode;
}

bool hw_bt_drv_rf_mdm_regs_entry_get(uint32_t *addr, uint32_t *val)
{
    bool ret = false;
    uint32_t table_size = 0;
    uint32_t table_ele_size = 0;

    uint32_t rf_mode = hw_get_bt_rf_mode() ;
    if (rf_mode == AIC_RF_MODE_BT_ONLY) {
        table_size = sizeof(rf_mdm_regs_table_bt_only);
        table_ele_size = sizeof(rf_mdm_regs_table_bt_only[0]);
        *addr = rf_mdm_regs_table_bt_only[rf_mdm_table_index][0];
        *val    = rf_mdm_regs_table_bt_only[rf_mdm_table_index][1];
    }

    if (rf_mode == AIC_RF_MODE_BT_COMBO) {
        table_size = sizeof(rf_mdm_regs_table_bt_combo);
        table_ele_size = sizeof(rf_mdm_regs_table_bt_combo[0]);
        *addr = rf_mdm_regs_table_bt_combo[rf_mdm_table_index][0];
        *val    = rf_mdm_regs_table_bt_combo[rf_mdm_table_index][1];
    }

    if (table_size == 0 || rf_mdm_table_index > (table_size/table_ele_size -1))
        return ret;

    rf_mdm_table_index++;
    ret = true;
    return ret;
}

bool hw_wr_rf_mdm_regs(HC_BT_HDR *p_buf)
{
    ///HC_BT_HDR  *p_buf = NULL;
    uint8_t *p;
    bool ret = FALSE;
    if (p_buf == NULL) {
        if (bt_vendor_cbacks)
            p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                          HCI_CMD_PREAMBLE_SIZE + \
                                                          HCI_VSC_WR_RF_MDM_REGS_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_WR_RF_MDM_REGS_SIZE;
        } else {
            return ret;
        }
    }

    if (p_buf) {
        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_VSC_WR_RF_MDM_REGS_CMD);
        *p++ = (uint8_t)HCI_VSC_WR_RF_MDM_REGS_SIZE; /* parameter length */
        uint32_t addr,val;
        uint8_t i = 0;
        uint8_t len = 0;
        uint8_t *p_data = p + 4;
        for (i = 0; i < 30; i++) {
            if (hw_bt_drv_rf_mdm_regs_entry_get(&addr, &val)) {
                UINT32_TO_STREAM(p_data,addr);
                UINT32_TO_STREAM(p_data,val);
            } else {
                break;
            }

            len = i * 8;
            UINT16_TO_STREAM(p,rf_mdm_regs_offset);
            *p++ = 0;
            *p++ = len;
            if (i == 30) {
                rf_mdm_regs_offset += len;
            } else {
                rf_mdm_regs_offset = 0;
            }

            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_WR_RF_MDM_REGS_SIZE;
            hw_cfg_cb.state = HW_CFG_WR_RF_MDM_REGS;

            ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WR_RF_MDM_REGS_CMD, p_buf, \
                                         hw_config_cback);
            ///all regs has been sent,go to next state
            if (rf_mdm_regs_offset == 0) {
                hw_cfg_cb.state = HW_CFG_WR_RF_MDM_REGS_END;
            }
        }
    }

    return ret;
}

bool hw_set_rf_mode(HC_BT_HDR *p_buf)
{
    ///HC_BT_HDR  *p_buf = NULL;
    uint8_t *p;
    bool ret = FALSE;
    if (p_buf == NULL) {
        if (bt_vendor_cbacks)
            p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                              HCI_CMD_PREAMBLE_SIZE + \
                                                              HCI_VSC_SET_RF_MODE_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;
        } else {
            return ret;
        }
    }

    if (p_buf) {
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
    uint8_t *p;
    bool ret = FALSE;
    if (p_buf == NULL) {
        if (bt_vendor_cbacks)
            p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                          HCI_CMD_PREAMBLE_SIZE + \
                                                          HCI_VSC_RF_CALIB_REQ_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_RF_CALIB_REQ_SIZE;
        } else {
            return ret;
        }
    }

    if (p_buf) {
        ///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        ///p_buf->offset = 0;
        ///p_buf->layer_specific = 0;
        ///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_VSC_RF_CALIB_REQ_CMD);
        *p++ = (uint8_t)HCI_VSC_RF_CALIB_REQ_SIZE; /* parameter length */
        struct hci_rf_calib_req_cmd *rf_calib_req = NULL;

        if (hw_get_bt_rf_mode() ==  AIC_RF_MODE_BT_ONLY) {
            rf_calib_req = (struct hci_rf_calib_req_cmd *)&rf_calib_req_bt_only;
        } else {
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


bool hw_aic_bt_multi_adv_enable(HC_BT_HDR *p_buf)
{
    ///HC_BT_HDR  *p_buf = NULL;
    uint8_t *p;
    bool ret = FALSE;
    if (p_buf == NULL) {
        if (bt_vendor_cbacks)
            p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                          HCI_CMD_PREAMBLE_SIZE + \
                                                          HCI_VSC_UPDATE_CONFIG_INFO_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_UPDATE_CONFIG_INFO_SIZE;
        } else {
            return ret;
        }
    }

    if (p_buf) {
        ///p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        ///p_buf->offset = 0;
        ///p_buf->layer_specific = 0;
        ///p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_SET_RF_MODE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_VSC_UPDATE_CONFIG_INFO_CMD);
        *p++ = (uint8_t)HCI_VSC_UPDATE_CONFIG_INFO_SIZE; /* parameter length */

        UINT16_TO_STREAM(p, AICBT_CONFIG_ID_MULTI_ADV);
        UINT16_TO_STREAM(p, 1);
		*p++ = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_UPDATE_CONFIG_INFO_SIZE;
        //hw_cfg_cb.state = HW_CFG_UPDATE_CONFIG_INFO;
		hw_cfg_cb.state = HW_CFG_SET_MULTI_ADV_EN;
        aicbt_up_config_info_state = VS_UPDATE_CONFIG_INFO_STATE_PTA_EN;
        ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_UPDATE_CONFIG_INFO_CMD, p_buf, \
                                hw_config_cback);
    }

    return ret;
}

bool hw_aic_bt_pta_en(HC_BT_HDR *p_buf)
{
    ///HC_BT_HDR  *p_buf = NULL;
    uint8_t *p;
    bool ret = FALSE;
    if (p_buf == NULL) {
        if (bt_vendor_cbacks)
            p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                          HCI_CMD_PREAMBLE_SIZE + \
                                                          HCI_VSC_UPDATE_CONFIG_INFO_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_UPDATE_CONFIG_INFO_SIZE;
        } else {
            return ret;
        }
    }

    if (p_buf) {
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
        //hw_cfg_cb.state = HW_CFG_UPDATE_CONFIG_INFO;
		hw_cfg_cb.state = HW_CFG_READ_LOCAL_FEATURES;
        aicbt_up_config_info_state = VS_UPDATE_CONFIG_INFO_STATE_PTA_EN;
        ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_UPDATE_CONFIG_INFO_CMD, p_buf, \
                                hw_config_cback);
    }

    return ret;
}

void init_fw_log(void);

uint8_t hw_set_fw_log(HC_BT_HDR *p_buf)
{
    uint8_t     *p;
    uint8_t     ret = TRUE;
    /* Start from sending HCI_VSC_PCM_PARAM_SET */

    if (p_buf == NULL) {
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE + \
                                                       HCI_VSC_FW_LOG_ENABLE_SET_SIZE);
    }

    memset(p_buf, 0, BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + HCI_VSC_FW_LOG_ENABLE_SET_SIZE);

    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_FW_LOG_ENABLE_SET_SIZE;
        p = (uint8_t *)(p_buf + 1);

        UINT16_TO_STREAM(p, HCI_VSC_FW_LOG_ENABLE_SET);
        *p++ = HCI_VSC_FW_LOG_ENABLE_SET_SIZE;
        
        /*config id*/
        UINT16_TO_STREAM(p, AICBT_CONFIG_ID_FW_LOG);
        /*config len*/
        UINT16_TO_STREAM(p, AICBT_CONFIG_ID_FW_LOG_SIZE);
        
#if (FW_LOG_ENABLE == TRUE)
        AICBTDBG(LOGINFO, "%s fw log enable \r\n", __func__);
        *p++ = 0x01;
        init_fw_log();
#else
        AICBTDBG(LOGINFO, "%s fw log disable \r\n", __func__);
        *p++ = 0x00;
#endif

        hw_cfg_cb.state = HW_CFG_SET_FW_LOG_ENABLE;
        
        bt_vendor_cbacks->xmit_cb(HCI_VSC_FW_LOG_ENABLE_SET, p_buf, hw_config_cback);

    }else{
        ret = FALSE;
    }

    return ret;

    
}


uint8_t hw_config_set_pcm_param(HC_BT_HDR *p_buf){
    uint8_t     *p;
    uint8_t     ret = TRUE;
    /* Start from sending HCI_VSC_PCM_PARAM_SET */

    if (p_buf == NULL) {
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE + \
                                                       HCI_VSC_PCM_PARAM_SET_SIZE);
    }
    
    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_VSC_PCM_PARAM_SET_SIZE;
        p = (uint8_t *)(p_buf + 1);

        UINT16_TO_STREAM(p, HCI_VSC_PCM_PARAM_SET);
        *p++ = HCI_VSC_PCM_PARAM_SET_SIZE;
        /*config id*/
        UINT16_TO_STREAM(p, AICBT_CONFIG_ID_PCM_PARAM);
        /*config len*/
        UINT16_TO_STREAM(p, BT_PCM_PARAM_SIZE);
        memcpy(p, &bt_pcm_param, BT_PCM_PARAM_SIZE);

        //UINT16_TO_STREAM(p, AICBT_CONFIG_ID_PTA_EN);
        //UINT16_TO_STREAM(p, sizeof(struct aicbt_pta_config));
        AICBTDBG(LOGINFO, "PCM data setting {0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x}",
                bt_pcm_param[0], bt_pcm_param[1],
                bt_pcm_param[2], bt_pcm_param[3],
                bt_pcm_param[4], bt_pcm_param[5],
                bt_pcm_param[6], bt_pcm_param[7]);
        
        hw_cfg_cb.state = HW_CFG_SET_PCM_PARAM;
        
        bt_vendor_cbacks->xmit_cb(HCI_VSC_PCM_PARAM_SET, p_buf, hw_config_cback);

    }else{
        ret = FALSE;
    }

    return ret;

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
uint8_t hw_config_set_bdaddr(HC_BT_HDR *p_buf)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    AICBTDBG(LOGINFO, "Setting local bd addr to %02X:%02X:%02X:%02X:%02X:%02X",
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
uint8_t hw_config_read_bdaddr(HC_BT_HDR *p_buf)
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

uint8_t hw_read_local_features(HC_BT_HDR *p_buf)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    UINT16_TO_STREAM(p, HCI_READ_LOCAL_FEATURES);
    *p = 0; /* parameter length */


    p_buf->len = HCI_CMD_PREAMBLE_SIZE;
    hw_cfg_cb.state = HW_CFG_READ_LOCAL_FEATURES;

    retval = bt_vendor_cbacks->xmit_cb(HCI_READ_LOCAL_FEATURES, p_buf, \
                                 hw_config_cback);
    return (retval);
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

    if (*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE) == 0) {
        status = BT_VND_OP_RESULT_SUCCESS;
    }

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if (status == BT_VND_OP_RESULT_SUCCESS) {
        if ((opcode == HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM) &&
            (SCO_INTERFACE_PCM == sco_bus_interface)) {
            uint8_t ret = FALSE;

            /* Ask a new buffer to hold WRITE_SCO_PCM_INT_PARAM command */
            if (bt_vendor_cbacks)
                p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
                        BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + SCO_PCM_PARAM_SIZE);
            if (p_buf) {
                p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
                p_buf->offset = 0;
                p_buf->layer_specific = 0;
                p_buf->len = HCI_CMD_PREAMBLE_SIZE + SCO_PCM_PARAM_SIZE;
                p = (uint8_t *)(p_buf + 1);

                /* do we need this VSC for I2S??? */
                UINT16_TO_STREAM(p, HCI_VSC_WRITE_SCO_PCM_INT_PARAM);
                *p++ = SCO_PCM_PARAM_SIZE;
                memcpy(p, &bt_sco_param, SCO_PCM_PARAM_SIZE);
                AICBTDBG(LOGINFO, "SCO PCM configure {0x%x, 0x%x, 0x%x, 0x%x, 0x%x}",
                        bt_sco_param[0], bt_sco_param[1], bt_sco_param[2], bt_sco_param[3],
                        bt_sco_param[4]);
                if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_SCO_PCM_INT_PARAM, p_buf,
                        hw_sco_i2spcm_cfg_cback)) == FALSE) {
                    bt_vendor_cbacks->dealloc(p_buf);
                } else {
                    return;
                }
            }
            status = BT_VND_OP_RESULT_FAIL;
        } else if ((opcode == HCI_VSC_WRITE_SCO_PCM_INT_PARAM) &&
                 (SCO_INTERFACE_PCM == sco_bus_interface)) {
            uint8_t ret = FALSE;

            /* Ask a new buffer to hold WRITE_PCM_DATA_FORMAT_PARAM command */
            if (bt_vendor_cbacks)
                p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
                        BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + PCM_DATA_FORMAT_PARAM_SIZE);
            if (p_buf) {
                p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
                p_buf->offset = 0;
                p_buf->layer_specific = 0;
                p_buf->len = HCI_CMD_PREAMBLE_SIZE + PCM_DATA_FORMAT_PARAM_SIZE;

                p = (uint8_t *)(p_buf + 1);
                UINT16_TO_STREAM(p, HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM);
                *p++ = PCM_DATA_FORMAT_PARAM_SIZE;
                memcpy(p, &bt_pcm_data_fmt_param, PCM_DATA_FORMAT_PARAM_SIZE);

                AICBTDBG(LOGINFO, "SCO PCM data format {0x%x, 0x%x, 0x%x, 0x%x, 0x%x}",
                        bt_pcm_data_fmt_param[0], bt_pcm_data_fmt_param[1],
                        bt_pcm_data_fmt_param[2], bt_pcm_data_fmt_param[3],
                        bt_pcm_data_fmt_param[4]);

                if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM,
                        p_buf, hw_sco_i2spcm_cfg_cback)) == FALSE) {
                    bt_vendor_cbacks->dealloc(p_buf);
                } else {
                    return;
                }
            }
            status = BT_VND_OP_RESULT_FAIL;
        }
    }

    AICBTDBG(LOGINFO, "sco I2S/PCM config result %d [0-Success, 1-Fail]", status);
    if (bt_vendor_cbacks) {
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
    AICBTDBG(LOGINFO, "SCO I2S interface change the sample rate to 16K");
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
    AICBTDBG(LOGINFO, "SCO I2S interface change the sample rate to 8K");
    hw_sco_i2spcm_config_from_command(p_mem, SCO_CODEC_CVSD);
}

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
    if (SCO_INTERFACE_I2S == sco_bus_interface) {
        /* 'Enable' I2S mode */
        bt_sco_i2spcm_param[0] = 1;

        /* set nbs clock rate as the value in SCO_I2SPCM_IF_CLOCK_RATE field */
        sco_bus_clock_rate = bt_sco_i2spcm_param[3];
    } else {
        /* 'Disable' I2S mode */
        bt_sco_i2spcm_param[0] = 0;

        /* set nbs clock rate as the value in SCO_PCM_IF_CLOCK_RATE field */
        sco_bus_clock_rate = bt_sco_param[1];

        /* sync up clock mode setting */
        bt_sco_i2spcm_param[1] = bt_sco_param[4];
    }

    if (sco_bus_wbs_clock_rate == INVALID_SCO_CLOCK_RATE) {
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

    if (bt_vendor_cbacks) {
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

    if (p_buf) {
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
        } else if (codec == SCO_CODEC_MSBC) {
            bt_sco_i2spcm_param[2] = wbs_sample_rate; /* SCO_I2SPCM_IF_SAMPLE_RATE 16K */
            bt_sco_i2spcm_param[3] = bt_sco_param[1] = sco_bus_wbs_clock_rate;
        } else {
            bt_sco_i2spcm_param[2] = 0; /* SCO_I2SPCM_IF_SAMPLE_RATE  8k */
            bt_sco_i2spcm_param[3] = bt_sco_param[1] = sco_bus_clock_rate;
            AICBTDBG(LOGINFO, "wrong codec is use in hw_sco_i2spcm_config, goes default NBS");
        }
        memcpy(p, &bt_sco_i2spcm_param, SCO_I2SPCM_PARAM_SIZE);
        cmd_u16 = HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM;
        AICBTDBG(LOGINFO, "I2SPCM config {0x%x, 0x%x, 0x%x, 0x%x}",
                bt_sco_i2spcm_param[0], bt_sco_i2spcm_param[1],
                bt_sco_i2spcm_param[2], bt_sco_i2spcm_param[3]);

        if ((ret = bt_vendor_cbacks->xmit_cb(cmd_u16, p_buf, hw_sco_i2spcm_cfg_cback)) == FALSE) {
            bt_vendor_cbacks->dealloc(p_buf);
        } else {
            return;
        }
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

    AICBTDBG(LOGINFO, "hw_set_SCO_codec 0x%x", codec);

    if (bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(
                BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + SCO_CODEC_PARAM_SIZE);

    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p = (uint8_t *)(p_buf + 1);

        UINT16_TO_STREAM(p, HCI_VSC_ENABLE_WBS);

        if (codec == SCO_CODEC_MSBC) {
            /* Enable mSBC */
            *p++ = SCO_CODEC_PARAM_SIZE; /* set the parameter size */
            UINT8_TO_STREAM(p,1); /* enable */
            UINT16_TO_STREAM(p, codec);

            /* set the totall size of this packet */
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + SCO_CODEC_PARAM_SIZE;

            p_set_SCO_codec_cback = hw_set_MSBC_codec_cback;
        } else {
            /* Disable mSBC */
            *p++ = (SCO_CODEC_PARAM_SIZE - 2); /* set the parameter size */
            UINT8_TO_STREAM(p,0); /* disable */

            /* set the totall size of this packet */
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + SCO_CODEC_PARAM_SIZE - 2;

            p_set_SCO_codec_cback = hw_set_CVSD_codec_cback;
            if ((codec != SCO_CODEC_CVSD) && (codec != SCO_CODEC_NONE))
            {
                AICBTDBG(LOGINFO, "SCO codec setting is wrong: codec: 0x%x", codec);
            }
        }

        if ((ret = bt_vendor_cbacks->xmit_cb(HCI_VSC_ENABLE_WBS, p_buf, p_set_SCO_codec_cback))\
              == FALSE) {
            bt_vendor_cbacks->dealloc(p_buf);
            ret_val = -1;
        }
    } else {
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
#endif // SCO_CFG_INCLUDED

uint8_t get_heartbeat_from_hardware()
{
    return 0;
}

/******************************************************************************
**   LPM Static Functions
******************************************************************************/


#if (HW_END_WITH_HCI_RESET == TRUE)
/******************************************************************************
*
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

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_OFFSET);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE_OFFSET;
    STREAM_TO_UINT16(opcode,p);

    AICBTDBG(LOGINFO, "%s Opcode:0x%04X Status: %d", __FUNCTION__, opcode, status);

    if (bt_vendor_cbacks)
    {
        /* Must free the RX event buffer */
        bt_vendor_cbacks->dealloc(p_evt_buf);

        /* Once epilog process is done, must call epilog_cb callback
           to notify caller */
        bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
    }
}

/******************************************************************************
*
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

    AICBTDBG(LOGINFO, "hw_epilog_process");

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
            AICBTDBG(LOGERROR, "vendor lib epilog process aborted [no buffer]");
            bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}
#endif // (HW_END_WITH_HCI_RESET == TRUE)

void hw_bt_assert_notify(void *p_mem)
{
    uint8_t *p_assert_data = (uint8_t *)p_mem;
    p_assert_data += 3;///hci hdr include evt len subevt
    int assert_param0 = (int)CO_32(p_assert_data);
    p_assert_data += 4;
    int assert_param1 = (int)CO_32(p_assert_data);
    p_assert_data += 4;
    uint32_t assert_lr = CO_32(p_assert_data);
    AICBTDBG(LOGINFO, "bt_assert_evt_notify:P0:0x%08x;P1:0x%08x;LR:0x%08x", assert_param0, assert_param1, assert_lr);
}

#define AICBT_FW_RET_FLAG                0xAC88
#define AICBT_FW_RET_CMD_SUBCODE         0x06

struct aicbt_fw_ret_param_tag
{
    uint16_t ret_flag;
    uint16_t ret_type;
    uint16_t ret_len;
    uint8_t ret_datalen;
    uint8_t ret_data[16];
};

struct aicbt_fw_ret_param_tag ret_params =
{
 .ret_flag  = AICBT_FW_RET_FLAG,
 .ret_type  = 0x00FF,//manufacture data
 .ret_len   = 0x0009,
 .ret_datalen = 0x08,
 .ret_data = {0x00,0x11,0x22,0x33,0xaa,0xbb,0xcc,0xdd}
};

bool hw_aicbt_fw_retention_params_config(HC_BT_HDR *p_buf)
{
    uint8_t   *p;
    bool ret = false;
    int ret_para_len = 2 + 2 + 2 + 1 + ret_params.ret_len;
    AICBTDBG(LOGINFO, "%s", __func__);
        if (bt_vendor_cbacks) {
            p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + ret_para_len);
        }
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE;
            p = (uint8_t *) (p_buf + 1);
            UINT16_TO_STREAM(p, HCI_BLE_ADV_FILTER);
            *p++ = ret_para_len; /* parameter length */
            *p++ = AICBT_FW_RET_CMD_SUBCODE; /* APCF subcmd*/
            UINT16_TO_STREAM(p, ret_params.ret_flag);
            UINT16_TO_STREAM(p, ret_params.ret_type);
            UINT16_TO_STREAM(p, ret_params.ret_len);
            *p++ = ret_params.ret_datalen;
            memcpy(p, (uint8_t *)&ret_params.ret_data[0], ret_params.ret_datalen);
            p  += ret_params.ret_datalen; /* APCF enable for aic ble remote controller */
            p_buf->len = HCI_CMD_PREAMBLE_SIZE + ret_para_len;
            hw_cfg_cb.state = HW_CFG_SET_FW_RET_PARAM;
            bt_vendor_cbacks->xmit_cb(HCI_BLE_ADV_FILTER, p_buf, hw_config_cback);
            ret = true;
        } else {
            AICBTDBG(LOGERROR, "hw_aicbt_fw_retention_params_config aborted [no buffer]");
        }
        return ret;
}

static void hw_shutdown_cback(void *p_mem)
{
    HC_BT_HDR   *p_evt_buf = (HC_BT_HDR *) p_mem;
    uint8_t     *p, status;
    uint16_t    opcode;

    status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode, p);

    AICBTDBG(LOGINFO, "%s Opcode: 0x%04X Status: 0x%02X", __FUNCTION__, opcode, status);

    if (bt_vendor_cbacks) {
        /* Must free the RX event buffer */
        bt_vendor_cbacks->dealloc(p_evt_buf);
    }
}

static void hw_shutdown_process(void)
{
    HC_BT_HDR *p_buf = NULL;
    uint8_t   *p;
    int apcf_enable_para_len = 2;

    AICBTDBG(LOGINFO, "%s", __func__);

    /* Sending a HCI COMMAND */
    if (bt_vendor_cbacks) {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE + apcf_enable_para_len);
    }

    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        UINT16_TO_STREAM(p, HCI_BLE_ADV_FILTER);
        *p++ = apcf_enable_para_len; /* parameter length */
        *p++ = 0x00;                 /* APCF subcmd of enable/disable */
        *p   = 0x06;                 /* APCF enable for aic ble remote controller */

        p_buf->len = HCI_CMD_PREAMBLE_SIZE + apcf_enable_para_len;

        /* Send command via HC's xmit_cb API */
        bt_vendor_cbacks->xmit_cb(HCI_BLE_ADV_FILTER, p_buf, hw_shutdown_cback);
    } else {
        AICBTDBG(LOGERROR, "vendor lib hw shutdown process aborted [no buffer]");
    }
}

static void *inotify_pthread_handle(void *args)
{
    int errTimes = 0;
    char *file = (char *)args;
    int fd = -1;
    int wd = -1;
    struct inotify_event *event;
    int length;
    int nread;
    char buf[BUFSIZ];
    int i = 0;

    fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        AICBTDBG(LOGERROR, "inotify_init failed, Error no. %d: %s", errno, strerror(errno));
        goto INOTIFY_FAIL;
    }

    buf[sizeof(buf) - 1] = 0;

    int rc;
    fd_set fds;
    struct timeval tv;
    int timeout_usec = 500000;

    while (inotify_pthread_running) {
        wd = inotify_add_watch(fd, file, IN_CREATE);
        if (wd < 0) {
            AICBTDBG(LOGERROR, "inotify_add_watch %s failed, Error no.%d: %s\n", file, errno, strerror(errno));
            if (errTimes++ < 3)
                continue;
            else
                goto INOTIFY_FAIL;
        }

        rc = -1;
        while (inotify_pthread_running && rc <= 0) {
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = timeout_usec;
            rc = select(fd+1, &fds, NULL, NULL, &tv);
        }

        while (inotify_pthread_running && rc > 0 && (length = read(fd, buf, sizeof(buf) - 1)) > 0) {
            nread = 0;
            while (nread < length) {
                event  = (struct inotify_event *)&buf[nread];
                nread += sizeof(struct inotify_event) + event->len;
                if (event->mask & IN_CREATE) {
                    if ((event->wd == wd) && (strcmp(event->name, "shutdown") == 0)) {
                        hw_shutdown_process();
                    }
                }
            }
        }
    }
    close(fd);
    return NULL;

INOTIFY_FAIL:
    return (void *)(-1);
}

int inotify_pthread_init(void)
{
    const char *args = "/dev";
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
    inotify_pthread_running = true;
    if (pthread_create(&inotify_pthread_id, &thread_attr, inotify_pthread_handle, (void *)args) != 0) {
        AICBTDBG(LOGERROR, "pthread_create : %s", strerror(errno));
        inotify_pthread_id = -1;
        return -1;
    }
    AICBTDBG(LOGINFO, "%s success", __func__);
    return 0;
}

int inotify_pthread_deinit(void)
{
    inotify_pthread_running = false;
    if (inotify_pthread_id != -1) {
        pthread_join(inotify_pthread_id, NULL);
    }
    AICBTDBG(LOGINFO, "%s success", __func__);
    return 0;
}
