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
 *  Filename:      bt_hardware.h
 *
 *  Description:   A wrapper header file of bt_vendor_lib.h
 *
 *                 Contains definitions specific for interfacing with AIC
 *                 Bluetooth chipsets
 *
 ******************************************************************************/

#ifndef BT_HARDWARE_H
#define BT_HARDWARE_H

#include "bt_vendor_lib.h"
#include "vnd_buildcfg.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/
#define AICBT_CHIP_8800             8800
#define AICBT_CHIP_8818             8818
#define AICBT_CHIP_8820             8820

#ifndef AICBT_CHIP_NB
#define AICBT_CHIP_NB                 AICBT_CHIP_8818
#endif

#if (AICBT_CHIP_NB != AICBT_CHIP_8800 && AICBT_CHIP_NB != AICBT_CHIP_8818 && AICBT_CHIP_NB != AICBT_CHIP_8820)
#ERROR CHIP NUMBER;
#endif

#define FW_PATCHFILE_EXTENSION                  ".hcd"
#define FW_PATCHFILE_EXTENSION_LEN              4
#define FW_PATCHFILE_PATH_MAXLEN                248 /* Local_Name length of return of
                                                       HCI_Read_Local_Name */

#define HCI_CMD_MAX_LEN                         258

#define HCI_CMD_PARAM_SIZE_MIN            0

#define HCI_RESET                               0x0C03
#define HCI_VSC_WRITE_UART_CLOCK_SETTING        0xFC45
#define HCI_VSC_UPDATE_BAUDRATE                 0xFC18
#define HCI_READ_LOCAL_NAME                     0x0C14
#define HCI_VSC_DOWNLOAD_MINIDRV                0xFC2E
#define HCI_VSC_WRITE_BD_ADDR                   0xFC70
#define HCI_VSC_WRITE_SLEEP_MODE                0xFC27
#define HCI_VSC_WRITE_SCO_PCM_INT_PARAM         0xFC1C
#define HCI_VSC_WRITE_PCM_DATA_FORMAT_PARAM     0xFC1E
#define HCI_VSC_WRITE_I2SPCM_INTERFACE_PARAM    0xFC6D
#define HCI_VSC_ENABLE_WBS                      0xFC7E
#define HCI_VSC_LAUNCH_RAM                      0xFC4E
#define HCI_READ_LOCAL_BDADDR                   0x1009

#define HCI_VSC_WR_RF_MDM_REGS_CMD              0xFC53
#define HCI_VSC_SET_RF_MODE_CMD                 0xFC48
#define HCI_VSC_RF_CALIB_REQ_CMD                0xFC4B
#define HCI_VSC_WR_AON_PARAM_CMD                0xFC4D

#define HCI_VSC_UPDATE_CONFIG_INFO_CMD          0xFC72
#define HCI_VSC_SET_LP_LEVEL_CMD                0xFC50
#define HCI_VSC_SET_PWR_CTRL_SLAVE_CMD          0xFC51
#define HCI_VSC_SET_CPU_POWER_OFF_CMD           0xFC52
#define HCI_VSC_SET_SLEEP_EN_CMD                0xFC47
#define HCI_BLE_ADV_FILTER                      0xFD57 // APCF command
#define HCI_VSC_FW_SOFT_CLOSE_CMD          0xFC77
#define HCI_VSC_FW_STATUS_GET_CMD          0xFC78
#define HCI_VSC_UPDATE_PT_SIZE         249
#define HCI_VSC_SET_UART_BAUD_CMD          0xFC49
#define HCI_VSC_SET_UART_BAUD_SIZE          0x04
#define HCI_VSC_SET_UART_FC_CMD             0xFC4C
#define HCI_VSC_SET_UART_FC_SIZE             0x01

#define HCI_VSC_MEM_WR_CMD             0xFC02
#define HCI_VSC_MEM_WR_SIZE            240
#define HCI_VSC_UPDATE_PT_CMD          0xFC75
#define HCI_VSC_UPDATE_PT_SIZE         249

#define HCI_VSC_WR_RF_MDM_REGS_SIZE             252
#define HCI_VSC_SET_RF_MODE_SIZE                01
#define HCI_VSC_RF_CALIB_REQ_SIZE               132
#define HCI_VSC_WR_AON_PARAM_SIZE               104

#define HCI_VSC_UPDATE_CONFIG_INFO_SIZE         36
#define HCI_VSC_SET_LP_LEVEL_SIZE               1
#define HCI_VSC_SET_PWR_CTRL_SLAVE_SIZE         1
#define HCI_VSC_SET_CPU_POWER_OFF_SIZE          1
#define HCI_VSC_SET_SLEEP_EN_SIZE               8
#define HCI_VSC_CLR_B4_RESET_SIZE               3

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

#define STREAM_TO_UINT16(u16, p)                {u16 = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); (p) += 2;}
#define UINT8_TO_STREAM(p, u8)                  {*(p)++ = (uint8_t)(u8);}
#define UINT16_TO_STREAM(p, u16)                {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT32_TO_STREAM(p, u32)                {*(p)++ = (uint8_t)(u32); *(p)++ = (uint8_t)((u32) >> 8); *(p)++ = (uint8_t)((u32) >> 16); *(p)++ = (uint8_t)((u32) >> 24);}

#define SCO_INTERFACE_PCM                       0
#define SCO_INTERFACE_I2S                       1

/* one byte is for enable/disable
      next 2 bytes are for codec type */
#define SCO_CODEC_PARAM_SIZE                    3

#define AICBT_CONFIG_ID_VX_SET                  0x01
#define AICBT_CONFIG_ID_PTA_EN                  0x0B

#define AON_BT_PWR_DLY1                         (1 + 5 + 1)
#define AON_BT_PWR_DLY2                         (10 + 48 + 5 + 1)
#define AON_BT_PWR_DLY3                         (10 + 48 + 8 + 5 + 1)
#define AON_BT_PWR_DLY_AON                      (10 + 48 + 8 + 5)

#define INVALID_SCO_CLOCK_RATE                  0xFF
#define FW_TABLE_VERSION                        "v1.1 20161117"

#define CO_BIT(pos)                               (1 << pos)

#define CO_32(p)                                (p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24))

#define SLP_CONFIG_MASK                   CO_BIT(0)
#define RF_CALIB_MASK                       CO_BIT(8)
#define MDM_CONFIG_MASK                 CO_BIT(16)

enum AICBT_PATCH_STATE_TAG{
 AICBT_PATCH_STATE_IDLE     = 0,
 AICBT_PATCH_STATE_PATCH    = 1,
 AICBT_PATCH_STATE_PT       = 2,
 AICBT_PATCH_STATE_END      = 3,
 AICBT_PATCH_STATE_MAX,
};

enum HW_CHIP_CONF_STATE {
 HW_CCS_IDLE             = 0,
 HW_CCS_SET_UART_FC,
 HW_CCS_SW_UART_BAUD,
 HW_CCS_FW_STA_GET,
 HW_CCS_VER_GET,
 HW_CCS_PATCH_LOAD,
 HW_CCS_SUCCESS,
 HW_CCS_MAX,
};

enum hw_cfg_cb_conf_type
{
    HW_CFG_CB_CONF_CHIPNB = 0,
    HW_CFG_CB_CONF_UFC,
    HW_CFG_CB_CONF_UBSW,
    HW_CFG_CB_CONF_UBINIT,
    HW_CFG_CB_CONF_UBUSED,
    HW_CFG_CB_CONF_MDMRFSLP,
    HW_CFG_CB_CONF_FWRET,
    HW_CFG_CB_CONF_MAX,
};

/* Hardware Configuration State */
enum {
    HW_CFG_CHIP_CONFIG = 0,
    HW_CFG_START = 1,
    HW_CFG_SET_UART_CLOCK,
    HW_CFG_SET_UART_BAUD_1,
    HW_CFG_READ_LOCAL_NAME,
    HW_CFG_DL_MINIDRIVER,
    HW_CFG_DL_FW_PATCH,
    HW_CFG_SET_UART_BAUD_2,
    HW_CFG_SET_BD_ADDR,
#if (USE_CONTROLLER_BDADDR == TRUE)
    HW_CFG_READ_BD_ADDR,
#endif
    HW_CFG_WR_RF_MDM_REGS,
    HW_CFG_WR_RF_MDM_REGS_END,
    HW_CFG_SET_RF_MODE,
    HW_CFG_RF_CALIB_REQ,
    HW_CFG_WR_AON_PARAM,
    HW_CFG_SET_LP_LEVEL,
    HW_CFG_SET_PWR_CTRL_SLAVE,
    HW_CFG_SET_CPU_POWR_OFF_EN,
    HW_CFG_UPDATE_CONFIG_INFO,
    HW_CFG_SET_FW_RET_PARAM,
    HW_CFG_MAX
};

/* h/w config control block */
typedef struct {
    uint8_t state;                          /* Hardware configuration state */
    uint8_t ccs;                          /* chip config state : ccs */
    uint8_t pls;                          /* patch and patch table load stat */
    uint32_t chip_nb;                   /* chip nb, like 8818 , 8820 ... */
    uint32_t ufc;                          /* uart flow ctr used */
    uint32_t ubsw;                      /* uart baud need to switch */
    uint32_t ubaud_init;                      /* uart baud inited */
    uint32_t ubaud_used;                      /* uart baud used */
    uint32_t mdmrfslp;                 /* MdmConfig | RfCalib | SleepConfig mask */
    uint32_t fwret;                 /* firmware retention for pwr plf */
    int     fw_fd;                          /* FW patch file fd */
    uint8_t f_set_baud_2;                   /* Baud rate switch state */
    char    local_chip_name[LOCAL_NAME_BUFFER_LEN];
} bt_hw_cfg_cb_t;


/******************************************************************************
**  Extern variables and functions
******************************************************************************/
int hw_cfg_cb_conf_set(char *p_conf_name, char *p_conf_value, int param);
void hw_aicbt_chip_config(void);
void hw_aicbt_patch_config_cback(void *p_mem);
bool hw_aicbt_patch_load( uint32_t mem_addr, uint8_t *data_ptr, uint8_t data_len, uint8_t type);
//bool hw_aicbt_patch_table_load( uint8_t patch_num, uint32_t *patch_addr, uint32_t *patch_data);
bool hw_aicbt_patch_table_load( uint8_t patch_num, uint32_t *patch_data);

#endif /* BT_HARDWARE_H */

