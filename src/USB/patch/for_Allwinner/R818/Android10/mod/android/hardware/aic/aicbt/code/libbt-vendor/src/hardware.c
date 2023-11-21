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
#define AICBT_RELEASE_NAME "20200318_BT_ANDROID_10.0rk"

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

/******************************************************************************
**  Constants &  Macros
******************************************************************************/

/******************************************************************************
**  Externs
******************************************************************************/

//void hw_config_cback(void *p_evt_buf);
//void hw_usb_config_cback(void *p_evt_buf);

extern uint8_t vnd_local_bd_addr[BD_ADDR_LEN];
extern bool aicbt_auto_restart;

/******************************************************************************
**  Static variables
******************************************************************************/
bt_hw_cfg_cb_t hw_cfg_cb;

/*
static bt_lpm_param_t lpm_param =
{
    LPM_SLEEP_MODE,
    LPM_IDLE_THRESHOLD,
    LPM_HC_IDLE_THRESHOLD,
    LPM_BT_WAKE_POLARITY,
    LPM_HOST_WAKE_POLARITY,
    LPM_ALLOW_HOST_SLEEP_DURING_SCO,
    LPM_COMBINE_SLEEP_MODE_AND_LPM,
    LPM_ENABLE_UART_TXD_TRI_STATE,*/
    //0,  /* not applicable */
   // 0,  /* not applicable */
   // 0,  /* not applicable */
    /*LPM_PULSED_HOST_WAKE
};*/

int getmacaddr(unsigned char * addr)
{
    int i = 0;
    char data[256], *str;
    int addr_fd;

    char property[100] = {0};
    if (property_get("persist.vendor.aicbt.bdaddr_path", property, "none")) {
        if(strcmp(property, "none") == 0) {
            return -1;
        }
        else if(strcmp(property, "default") == 0) {
          memcpy(addr, vnd_local_bd_addr, BD_ADDR_LEN);
          return 0;

        }
        else if ((addr_fd = open(property, O_RDONLY)) != -1)
        {
            memset(data, 0, sizeof(data));
            int ret = read(addr_fd, data, 17);
            if(ret < 17) {
                ALOGE("%s, read length = %d", __func__, ret);
                close(addr_fd);
                return -1;
            }
            for (i = 0,str = data; i < 6; i++) {
                addr[5-i] = (unsigned char)strtoul(str, &str, 16);
                str++;
            }
            close(addr_fd);
            return 0;
        }
    }
    return -1;
}

int aic_get_bt_firmware(uint8_t** fw_buf, char* fw_short_name)
{
    char filename[PATH_MAX] = {0};
    struct stat st;
    int fd = -1;
    size_t fwsize = 0;
    size_t buf_size = 0;

    sprintf(filename, FIRMWARE_DIRECTORY, fw_short_name);
    ALOGI("BT fw file: %s", filename);

    if (stat(filename, &st) < 0)
    {
        ALOGE("Can't access firmware, errno:%d", errno);
        return -1;
    }

    fwsize = st.st_size;
    buf_size = fwsize;

    if ((fd = open(filename, O_RDONLY)) < 0)
    {
        ALOGE("Can't open firmware, errno:%d", errno);
        return -1;
    }

    if (!(*fw_buf = malloc(buf_size)))
    {
        ALOGE("Can't alloc memory for fw&config, errno:%d", errno);
        if (fd >= 0)
        close(fd);
        return -1;
    }

    if (read(fd, *fw_buf, fwsize) < (ssize_t) fwsize)
    {
        free(*fw_buf);
        *fw_buf = NULL;
        if (fd >= 0)
        close(fd);
        return -1;
    }

    if (fd >= 0)
        close(fd);

    ALOGI("Load FW OK");
    return buf_size;
}

uint8_t aic_get_fw_project_id(uint8_t *p_buf)
{
    uint8_t opcode;
    uint8_t len;
    uint8_t data = 0;

    do {
        opcode = *p_buf;
        len = *(p_buf - 1);
        if (opcode == 0x00)
        {
            if (len == 1)
            {
                data = *(p_buf - 2);
                ALOGI("bt_hw_parse_project_id: opcode %d, len %d, data %d", opcode, len, data);
                break;
            }
            else
            {
                ALOGW("bt_hw_parse_project_id: invalid len %d", len);
            }
        }
        p_buf -= len + 2;
    } while (*p_buf != 0xFF);

    return data;
}

uint8_t get_heartbeat_from_hardware()
{
    return hw_cfg_cb.heartbeat;
}

uint16_t getLmp_subversion()
{
    return hw_cfg_cb.lmp_subversion;
}
uint8_t getchip_type()
{
    return hw_cfg_cb.chip_type;
}

struct aic_epatch_entry *aic_get_patch_entry(bt_hw_cfg_cb_t *cfg_cb)
{
    uint16_t i;
    struct aic_epatch *patch;
    struct aic_epatch_entry *entry;
    uint8_t *p;
    uint16_t chip_id;

    patch = (struct aic_epatch *)cfg_cb->fw_buf;
    entry = (struct aic_epatch_entry *)malloc(sizeof(*entry));
    if(!entry)
    {
        ALOGE("aic_get_patch_entry: failed to allocate mem for patch entry");
        return NULL;
    }

    patch->number_of_patch = le16_to_cpu(patch->number_of_patch);

    ALOGI("aic_get_patch_entry: fw_ver 0x%08x, patch_num %d",
                le32_to_cpu(patch->fw_version), patch->number_of_patch);

    for (i = 0; i < patch->number_of_patch; i++)
    {
        p = cfg_cb->fw_buf + 14 + 2*i;
        STREAM_TO_UINT16(chip_id, p);
        if (chip_id == cfg_cb->eversion + 1)
        {
            entry->chip_id = chip_id;
            p = cfg_cb->fw_buf + 14 + 2*patch->number_of_patch + 2*i;
            STREAM_TO_UINT16(entry->patch_length, p);
            p = cfg_cb->fw_buf + 14 + 4*patch->number_of_patch + 4*i;
            STREAM_TO_UINT32(entry->patch_offset, p);
            ALOGI("aic_get_patch_entry: chip_id %d, patch_len 0x%x, patch_offset 0x%x",
                    entry->chip_id, entry->patch_length, entry->patch_offset);
            break;
        }
    }

    if (i == patch->number_of_patch)
    {
        ALOGE("aic_get_patch_entry: failed to get etnry");
        free(entry);
        entry = NULL;
    }

    return entry;
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

    if (*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_OFFSET) == 0)
    {
        status = BT_VND_OP_RESULT_SUCCESS;
    }

    if (bt_vendor_cbacks)
    {
        bt_vendor_cbacks->lpm_cb(status);
        bt_vendor_cbacks->dealloc(p_evt_buf);
    }
}


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

    BTVNDDBG("%s Opcode:0x%04X Status: %d", __FUNCTION__, opcode, status);

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

    BTVNDDBG("hw_epilog_process");

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
