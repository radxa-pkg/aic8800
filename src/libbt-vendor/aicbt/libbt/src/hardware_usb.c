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

#define LOG_TAG "bt_hwcfg_usb"
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
#include "aic_common.h"

void hw_usb_config_cback(void *p_evt_buf);
bool hw_aicbt_fw_retention_params_config(HC_BT_HDR *p_buf);
bool hw_aic_bt_multi_adv_enable(HC_BT_HDR *p_buf);

/******************************************************************************
**  Static variables
******************************************************************************/


/*******************************************************************************
**
** Function         hw_usb_config_cback
**
** Description      Callback function for controller configuration
**
** Returns          None
**
*******************************************************************************/
void hw_usb_config_cback(void *p_mem)
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

	if (p_buf != NULL) {
		p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
		p_buf->offset = 0;
		p_buf->len = 0;
		p_buf->layer_specific = 0;

		p = (uint8_t *) (p_buf + 1);

		AICBTDBG(LOGINFO, "hw_cfg_cb.state = %d", hw_cfg_cb.state);

		switch (hw_cfg_cb.state) {
			case HW_CFG_START:
				AICBTDBG(LOGINFO, "HW_CFG_START");
				if((is_proceeding = hw_set_fw_log(p_buf)) == TRUE)
					break;
            case HW_CFG_SET_FW_LOG_ENABLE:
#if (USE_CONTROLLER_BDADDR == TRUE)
				if((is_proceeding = hw_config_read_bdaddr(p_buf)) == TRUE)
					break;
#else
				if((is_proceeding = hw_config_set_bdaddr(p_buf)) == TRUE)
					break;

				break;
#endif
#if (USE_CONTROLLER_BDADDR == TRUE)
			case HW_CFG_READ_BD_ADDR:
				p_tmp = (char *) (p_evt_buf + 1) + \
					HCI_EVT_CMD_CMPL_LOCAL_BDADDR_ARRAY;

				AICBTDBG(LOGINFO, "Controller OTP bdaddr %02X:%02X:%02X:%02X:%02X:%02X",
						*(p_tmp+5), *(p_tmp+4), *(p_tmp+3),
						*(p_tmp+2), *(p_tmp+1), *p_tmp);
				vnd_local_bd_addr[0] = *(p_tmp+5);
				vnd_local_bd_addr[1] = *(p_tmp+4);
				vnd_local_bd_addr[2] = *(p_tmp+3);
				vnd_local_bd_addr[3] = *(p_tmp+2);
				vnd_local_bd_addr[4] = *(p_tmp+1);
				vnd_local_bd_addr[5] = *(p_tmp);

				if ((is_proceeding = hw_config_set_bdaddr(p_buf)) == TRUE)
					break;

				is_proceeding = TRUE;
				break;
#endif // (USE_CONTROLLER_BDADDR == TRUE)
			case HW_CFG_SET_BD_ADDR:
				hw_read_local_features(p_buf);
				is_proceeding = TRUE;
				break;
			case HW_CFG_READ_LOCAL_FEATURES:
#if (BT_RETENTION == TRUE)
				if ((is_proceeding = hw_aicbt_fw_retention_params_config(p_buf)) == TRUE)
					break;
			case HW_CFG_SET_FW_RET_PARAM:
#endif
#if (PCM_SETTING == TRUE)
				if ((is_proceeding = hw_config_set_pcm_param(p_buf)) == TRUE)
					break;
			case HW_CFG_SET_PCM_PARAM:
#endif
#if (MULTI_ADV_ENABLE == TRUE)
				if ((is_proceeding = hw_aic_bt_multi_adv_enable(p_buf)) == TRUE)
#endif
					break;	
			case HW_CFG_SET_MULTI_ADV_EN:
				AICBTDBG(LOGINFO, "vendor lib fwcfg completed %d", hw_cfg_cb.state);
				bt_vendor_cbacks->dealloc(p_buf);
				bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
				hw_cfg_cb.state = 0;

				is_proceeding = TRUE;

				break;
		}
	}

	/* Free the RX event buffer */
	if (bt_vendor_cbacks)
		bt_vendor_cbacks->dealloc(p_evt_buf);

	if (is_proceeding == FALSE) {
		AICBTDBG(LOGERROR, "vendor lib fwcfg aborted!!!");
		if (bt_vendor_cbacks) {
			if (p_buf != NULL)
				bt_vendor_cbacks->dealloc(p_buf);

			bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
		}
        

		hw_cfg_cb.state = 0;
	}
}


/*******************************************************************************
**
** Function        hw__usb_config_start
**
** Description     Kick off controller initialization process
**
** Returns         None
**
*******************************************************************************/
void hw_usb_config_start(char transtype, uint32_t usb_id)
{
    AIC_UNUSED(transtype);
    uint16_t usb_pid = usb_id & 0x0000ffff;
    uint16_t usb_vid = (usb_id >> 16) & 0x0000ffff;
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    AICBTDBG(LOGINFO, "AICBT_RELEASE_NAME: %s", AICBT_RELEASE_NAME);
    AICBTDBG(LOGINFO, "\nAicsemi libbt-vendor_usb Version %s \n", AIC_VERSION);
    AICBTDBG(LOGINFO, "hw_usb_config_start, transtype = 0x%x, pid = 0x%04x, vid = 0x%04x \n", transtype, usb_pid, usb_vid);

    if (bt_vendor_cbacks) {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE;

            p = (uint8_t *) (p_buf + 1);

            p = (uint8_t *)(p_buf + 1);
            UINT16_TO_STREAM(p, HCI_VENDOR_RESET);
            *p++ = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE;

            hw_cfg_cb.state = HW_CFG_START;
            bt_vendor_cbacks->xmit_cb(HCI_VENDOR_RESET, p_buf, hw_usb_config_cback);
        } else {
            ALOGE("%s buffer alloc fail!", __func__);
        }
    } else {
        ALOGE("%s call back is null", __func__);
    }
}

