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
 *  Filename:      bt_vendor_aicbt.c
 *
 *  Description:   AIC vendor specific library implementation
 *
 ******************************************************************************/

#define LOG_TAG "bt_vendor"

#include <utils/Log.h>
#include <string.h>
#include "bt_vendor_aicbt.h"
#include "upio.h"
#include "userial_vendor.h"

#ifndef BTVND_DBG
#define BTVND_DBG FALSE
#endif

#if (BTVND_DBG == TRUE)
#define BTVNDDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTVNDDBG(param, ...) {}
#endif

/******************************************************************************
**  Externs
******************************************************************************/

void hw_config_start(void);
uint8_t hw_lpm_enable(uint8_t turn_on);
uint32_t hw_lpm_get_idle_timeout(void);
void hw_lpm_set_wake_state(uint8_t wake_assert);
#if (SCO_CFG_INCLUDED == TRUE)
void hw_sco_config(void);
#endif
void vnd_load_conf(const char *p_path);
#if (HW_END_WITH_HCI_RESET == TRUE)
void hw_epilog_process(void);
#endif

#if (AICBT_A2DP_OFFLOAD == TRUE)
void aicbt_vnd_a2dp_init(bt_vendor_callbacks_t *callback);
int aicbt_vnd_a2dp_execute(bt_vendor_opcode_t, void *ev_data);
#endif

/******************************************************************************
**  Variables
******************************************************************************/

bt_vendor_callbacks_t *bt_vendor_cbacks = NULL;
uint8_t vnd_local_bd_addr[6]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/******************************************************************************
**  Local type definitions
******************************************************************************/

/******************************************************************************
**  Static Variables
******************************************************************************/

static const tUSERIAL_CFG userial_init_cfg =
{
    (USERIAL_DATABITS_8 | USERIAL_PARITY_NONE | USERIAL_STOPBITS_1),
    USERIAL_BAUD_1_5M
};

/******************************************************************************
**  Functions
******************************************************************************/

/*****************************************************************************
**
**   BLUETOOTH VENDOR INTERFACE LIBRARY FUNCTIONS
**
*****************************************************************************/

static int init(const bt_vendor_callbacks_t* p_cb, unsigned char *local_bdaddr)
{
    ALOGI("init");

    if (p_cb == NULL)
    {
        ALOGE("init failed with no user callbacks!");
        return -1;
    }

#if (VENDOR_LIB_RUNTIME_TUNING_ENABLED == TRUE)
    ALOGW("*****************************************************************");
    ALOGW("*****************************************************************");
    ALOGW("** Warning - BT Vendor Lib is loaded in debug tuning mode!");
    ALOGW("**");
    ALOGW("** If this is not intentional, rebuild libbt-vendor.so ");
    ALOGW("** with VENDOR_LIB_RUNTIME_TUNING_ENABLED=FALSE and ");
    ALOGW("** check if any run-time tuning parameters needed to be");
    ALOGW("** carried to the build-time configuration accordingly.");
    ALOGW("*****************************************************************");
    ALOGW("*****************************************************************");
#endif

    userial_vendor_init();
    upio_init();

    vnd_load_conf(VENDOR_LIB_CONF_FILE);

    /* store reference to user callbacks */
    bt_vendor_cbacks = (bt_vendor_callbacks_t *) p_cb;

    /* This is handed over from the stack */
    memcpy(vnd_local_bd_addr, local_bdaddr, 6);

#if (AICBT_A2DP_OFFLOAD == TRUE)
    aicbt_vnd_a2dp_init(bt_vendor_cbacks);
#endif

    return 0;
}


/** Requested operations */
static int op(bt_vendor_opcode_t opcode, void *param)
{
    int retval = 0;

    BTVNDDBG("op for %d", opcode);

    switch(opcode)
    {
        case BT_VND_OP_POWER_CTRL:
            {
                int *state = (int *) param;
                upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
                if (*state == BT_VND_PWR_ON)
                {
                    ALOGW("NOTE: BT_VND_PWR_ON now forces power-off first");
                    upio_set_bluetooth_power(UPIO_BT_POWER_ON);
                } else {
                    /* Make sure wakelock is released */
                    hw_lpm_set_wake_state(false);
                }
            }
            break;

        case BT_VND_OP_FW_CFG:
            {
                hw_config_start();
            }
            break;

        case BT_VND_OP_SCO_CFG:
            {
#if (SCO_CFG_INCLUDED == TRUE)
                hw_sco_config();
#else
                retval = -1;
#endif
            }
            break;

        case BT_VND_OP_USERIAL_OPEN:
            {
                int (*fd_array)[] = (int (*)[]) param;
                int fd, idx;
                fd = userial_vendor_open((tUSERIAL_CFG *) &userial_init_cfg);
                if (fd != -1)
                {
                    for (idx=0; idx < CH_MAX; idx++)
                        (*fd_array)[idx] = fd;

                    retval = 1;
                }
                /* retval contains numbers of open fd of HCI channels */
            }
            break;

        case BT_VND_OP_USERIAL_CLOSE:
            {
                userial_vendor_close();
            }
            break;

        case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
            {
                uint32_t *timeout_ms = (uint32_t *) param;
                *timeout_ms = hw_lpm_get_idle_timeout();
            }
            break;

        case BT_VND_OP_LPM_SET_MODE:
            {
                uint8_t *mode = (uint8_t *) param;
                extern uint8_t hw_lpm_set_sleep_enable(uint8_t turn_on);
                retval = hw_lpm_set_sleep_enable(*mode);//hw_lpm_enable(*mode);
            }
            break;

        case BT_VND_OP_LPM_WAKE_SET_STATE:
            {
                uint8_t *state = (uint8_t *) param;
                uint8_t wake_assert = (*state == BT_VND_LPM_WAKE_ASSERT) ? \
                                        TRUE : FALSE;

                hw_lpm_set_wake_state(wake_assert);
            }
            break;

         case BT_VND_OP_SET_AUDIO_STATE:
            {
                retval = hw_set_audio_state((bt_vendor_op_audio_state_t *)param);
            }
            break;

        case BT_VND_OP_EPILOG:
            {
#if (HW_END_WITH_HCI_RESET == FALSE)
                if (bt_vendor_cbacks)
                {
                    bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
                }
#else
                hw_epilog_process();
#endif
            }
            break;
#if (AICBT_A2DP_OFFLOAD == TRUE)
        case BT_VND_OP_A2DP_OFFLOAD_START:
        case BT_VND_OP_A2DP_OFFLOAD_STOP:
            retval = aicbt_vnd_a2dp_execute(opcode, param);
            break;
#endif
    }

    return retval;
}

/** Closes the interface */
static void cleanup( void )
{
    BTVNDDBG("cleanup");

    upio_cleanup();

    bt_vendor_cbacks = NULL;
}

// Entry point of DLib
const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    init,
    op,
    cleanup
};
