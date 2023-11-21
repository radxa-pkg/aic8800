/******************************************************************************
 *
 *  Copyright (C) 2017-2027 AIC Corporation
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
 *  Filename:      aic_patch_interface.h
 *
 *  Description:   A wrapper header file of aic_patch_interface.h
 *
 *                 Contains definitions specific for interfacing with AIC
 *                 Bluetooth chipsets
 *
 ******************************************************************************/

#ifndef BT_AIC_PATCH_IF_H
#define BT_AIC_PATCH_IF_H

#include "bt_vendor_lib.h"
#include "vnd_buildcfg.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/
#define FW_PATH_MAX                         200
#define HCI_PATCH_DATA_MAX_LEN              240 
#define HCI_PT_MAX_LEN                      31

#define AICBT_PFILE_PATH_DFT            "/vendor/etc/firmware/"
#define AICBT_PATCH_BASIC_NAME          "fw_patch"
#define AICBT_PTABLE_BASIC_NAME         "fw_patch_table"
#define AICBT_PT_TAG                    "AICBT_PT_TAG"

enum aicbt_patch_table_type {
    AICBT_PT_NULL = 0x00,
    AICBT_PT_TRAP,
    AICBT_PT_B4,
    AICBT_PT_BTMODE,
    AICBT_PT_PWRON,
    AICBT_PT_AF,
    AICBT_PT_DONE,
    AICBT_PT_VER,
    AICBT_PT_MAX,

};

enum aicbt_btport_type {
    AICBT_BTPORT_NULL,
    AICBT_BTPORT_MB,
    AICBT_BTPORT_UART,
};

/*  btmode
 * used for force bt mode,if not AICBSP_MODE_NULL
 * efuse valid and vendor_info will be invalid, even has beed set valid
*/
enum aicbt_btmode_type {
    AICBT_BTMODE_BT_ONLY_SW = 0x0,    // bt only mode with switch
    AICBT_BTMODE_BT_WIFI_COMBO,       // wifi/bt combo mode
    AICBT_BTMODE_BT_ONLY,             // bt only mode without switch
    AICBT_BTMODE_BT_ONLY_TEST,        // bt only test mode
    AICBT_BTMODE_BT_WIFI_COMBO_TEST,  // wifi/bt combo test mode
    AICBT_MODE_NULL = 0xFF,           // invalid value
};

/*  uart_baud
 * used for config uart baud when btport set to uart,
 * otherwise meaningless
*/
enum aicbt_uart_baud_type {
    AICBT_UART_BAUD_115200     = 115200,
    AICBT_UART_BAUD_921600     = 921600,
    AICBT_UART_BAUD_1_5M       = 1500000,
    AICBT_UART_BAUD_3_25M      = 3250000,
};

enum aicbt_uart_flowctrl_type {
    AICBT_UART_FLOWCTRL_DISABLE = 0x0,    // uart without flow ctrl
    AICBT_UART_FLOWCTRL_ENABLE,           // uart with flow ctrl
};

enum aicbsp_cpmode_type {
    AICBSP_CPMODE_WORK,
    AICBSP_CPMODE_TEST,
    AICBSP_CPMODE_MAX,
};

enum bt_chip_ver {
    CHIP_VER_U02 = 0x03,
    CHIP_VER_U03 = 0x07,
    CHIP_VER_U04 = 0x0f,
};

#define AICBT_BT_PATCHBASE_DFT          0x100000
#define AICBT_BTMODE_DFT                AICBT_MODE_NULL
#define AICBT_VNDINFO_DFT               0
#define AICBT_EFUSE_VALID_DFT           1
#define AICBT_BTTRANS_MODE_DFT          AICBT_BTPORT_UART
#define AICBT_UART_BAUD_DFT             AICBT_UART_BAUD_1_5M
#define AICBT_UART_FC_DFT               AICBT_UART_FLOWCTRL_ENABLE
#define AICBT_LPM_ENABLE_DFT            1
#define AICBT_TX_PWR_DFT                0x6020

struct aicbt_patch_info_tag {
    ///const char *bt_adid;
    char bt_pfpath[32];
    char bt_patch[32];
    char bt_table[32];
    uint32_t patch_base;
    uint32_t bt_mode;
    uint32_t vendor_info;
    uint32_t efuse_valid;
    uint32_t bt_trans_mode;
    uint32_t uart_baud;
    uint32_t uart_flowctrl;
    uint32_t lpm_enable;
    uint32_t tx_pwr;
    uint32_t chip_ver;
};

enum aicbt_patch_proc
{
    AICBT_PATCH_PROC_IDLE,
    AICBT_PATCH_PROC_START,
    AICBT_PATCH_PROC_PATCH,
    AICBT_PATCH_PROC_PT,
    AICBT_PATCH_PROC_END,
    AICBT_PATCH_PROC_MAX,
};

enum aicbt_patch_info_type
{
    AICBT_P_INFO_PFPATH = 0,
    AICBT_P_INFO_PATCH,
    AICBT_P_INFO_PTABLE,
    AICBT_P_INFO_PBASE,
    AICBT_P_INFO_BTMODE,
    AICBT_P_INFO_VNDINFO,
    AICBT_P_INFO_EFUSE,
    AICBT_P_INFO_TRANSMOD,
    AICBT_P_INFO_UARTBAUD,
    AICBT_P_INFO_UARTFC,
    AICBT_P_INFO_LPMEN,
    AICBT_P_INFO_TXPWR,
};

#define _8BIT                                      8
#define _16BIT                                    16
#define _32BIT                                    32

/******************************************************************************
**  Extern variables and functions
******************************************************************************/
struct aicbt_patch_info_tag * aicbt_patch_info_get(void);
void aicbt_pt_data_copy(uint32_t *dst, uint32_t *src, uint8_t dpos, uint8_t d_derta, uint8_t spos, uint8_t s_derta,uint8_t len);
void str2hex(uint8_t*Dest, uint8_t*Src, int Len);
int aicbt_conf_hex_data_convert(uint32_t *dstdata, char *p_conf_value);
int aicbt_fw_patch_table_load(void);
bool aicbt_fw_patch_load(void);
int aicbt_patch_alloc_init(void);
void aicbt_patch_chipver_set(uint32_t chip_ver);
int aicbt_patch_info_config(char *p_conf_name, char *p_conf_value, int param);

#endif /* BT_AIC_PATCH_IF_H */

