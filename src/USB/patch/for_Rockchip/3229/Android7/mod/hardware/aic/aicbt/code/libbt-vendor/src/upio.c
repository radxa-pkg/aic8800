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
 *  Filename:      upio.c
 *
 *  Description:   Contains I/O functions, like
 *                      rfkill control
 *                      BT_WAKE/HOST_WAKE control
 *
 ******************************************************************************/

#define LOG_TAG "bt_upio"

#include <utils/Log.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/properties.h>
#include "bt_vendor_aic.h"
#include "upio.h"
#include "userial_vendor.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef UPIO_DBG
#define UPIO_DBG FALSE
#endif

#if (UPIO_DBG == TRUE)
#define UPIODBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define UPIODBG(param, ...) {}
#endif

/******************************************************************************
**  Local type definitions
******************************************************************************/

#if (BT_WAKE_VIA_PROC == TRUE)

/* proc fs node for enable/disable lpm mode */
#ifndef VENDOR_LPM_PROC_NODE
#define VENDOR_LPM_PROC_NODE "/proc/bluetooth/sleep/lpm"
#endif

/* proc fs node for notifying write request */
#ifndef VENDOR_BTWRITE_PROC_NODE
#define VENDOR_BTWRITE_PROC_NODE "/proc/bluetooth/sleep/btwrite"
#endif

/*
 * Maximum btwrite assertion holding time without consecutive btwrite kicking.
 * This value is correlative(shorter) to the in-activity timeout period set in
 * the bluesleep LPM code. The current value used in bluesleep is 10sec.
 */
#ifndef PROC_BTWRITE_TIMER_TIMEOUT_MS
#define PROC_BTWRITE_TIMER_TIMEOUT_MS   8000
#endif

/* lpm proc control block */
typedef struct
{
    uint8_t btwrite_active;
    uint8_t timer_created;
    timer_t timer_id;
    uint32_t timeout_ms;
} vnd_lpm_proc_cb_t;

static vnd_lpm_proc_cb_t lpm_proc_cb;
#endif

/******************************************************************************
**  Static variables
******************************************************************************/

static uint8_t upio_state[UPIO_MAX_COUNT];
static int rfkill_id = -1;
static int bt_emul_enable = 0;
static char *rfkill_state_path = NULL;

/******************************************************************************
**  Static functions
******************************************************************************/

/* for friendly debugging outpout string */
static char *lpm_mode[] = {
    "UNKNOWN",
    "disabled",
    "enabled"
};

static char *lpm_state[] = {
    "UNKNOWN",
    "de-asserted",
    "asserted"
};

/*****************************************************************************
**   Bluetooth On/Off Static Functions
*****************************************************************************/
static int is_emulator_context(void)
{
    char value[PROPERTY_VALUE_MAX];

    property_get("ro.kernel.qemu", value, "0");
    UPIODBG("is_emulator_context : %s", value);
    if (strcmp(value, "1") == 0) {
        return 1;
    }
    return 0;
}

static int is_rfkill_disabled(void)
{
    char value[PROPERTY_VALUE_MAX];

    property_get("ro.rfkilldisabled", value, "0");
    UPIODBG("is_rfkill_disabled ? [%s]", value);

    if (strcmp(value, "1") == 0) {
        return UPIO_BT_POWER_ON;
    }

    return UPIO_BT_POWER_OFF;
}

static int init_rfkill()
{
    char path[64];
    char buf[16];
    int fd, sz, id;

    if (is_rfkill_disabled())
        return -1;

    for (id = 0; ; id++)
    {
        snprintf(path, sizeof(path), "/sys/class/rfkill/rfkill%d/type", id);
        fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            ALOGE("init_rfkill : open(%s) failed: %s (%d)\n", \
                 path, strerror(errno), errno);
            return -1;
        }

        sz = read(fd, &buf, sizeof(buf));
        close(fd);

        if (sz >= 9 && memcmp(buf, "bluetooth", 9) == 0)
        {
            rfkill_id = id;

            break;
        }
    }

    asprintf(&rfkill_state_path, "/sys/class/rfkill/rfkill%d/state", rfkill_id);
    return 0;
}

int bt_wake_up_host_mode_set(uint8_t mode)
{
    char path[64];
    char buffer = '0';
    int sz;
    int fd = -1;
    int ret = -1;
    ALOGE("bt_wake_up_host_mode_set");

    snprintf(path, sizeof(path), "/proc/bluetooth/sleep/lpm");
    ALOGE("bt_wake_up_host_mode_set path:%s", path);

    fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        ALOGE("bt_wake_up_host_mode_set fd:%d = open(path, O_RDWR): open(%s) failed: %s (%d)\n", \
            fd, path, strerror(errno), errno);
        return -1;
    }

    ALOGE("bt_wake_up_host_mode_set fd:%d = open(path, O_RDWR): open(%s) success\n", \
           fd, path);

    if(mode == 1)
    {
       buffer = '1';
    } else {
       buffer = '0';
    }

    ALOGE("bt_wake_up_host_mode_set buffer:%d", buffer);
    sz = write(fd, &buffer, 1);

    if (sz < 0) {
        ALOGE("bt_wake_up_host_mode_set : write(%s) failed: %s (%d)",
            rfkill_state_path, strerror(errno),errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}

/*****************************************************************************
**   LPM Static Functions
*****************************************************************************/

#if (BT_WAKE_VIA_PROC == TRUE)
/*******************************************************************************
**
** Function        proc_btwrite_timeout
**
** Description     Timeout thread of proc/.../btwrite assertion holding timer
**
** Returns         None
**
*******************************************************************************/
static void proc_btwrite_timeout(union sigval arg)
{
    UPIODBG("..%s..", __FUNCTION__);
    lpm_proc_cb.btwrite_active = FALSE;
}
#endif

/*****************************************************************************
**   UPIO Interface Functions
*****************************************************************************/

/*******************************************************************************
**
** Function        upio_init
**
** Description     Initialization
**
** Returns         None
**
*******************************************************************************/
void upio_init(void)
{
    memset(upio_state, UPIO_UNKNOWN, UPIO_MAX_COUNT);
#if (BT_WAKE_VIA_PROC == TRUE)
    memset(&lpm_proc_cb, 0, sizeof(vnd_lpm_proc_cb_t));
#endif
}

/*******************************************************************************
**
** Function        upio_cleanup
**
** Description     Clean up
**
** Returns         None
**
*******************************************************************************/
void upio_cleanup(void)
{
#if (BT_WAKE_VIA_PROC == TRUE)
    if (lpm_proc_cb.timer_created == TRUE)
        timer_delete(lpm_proc_cb.timer_id);

    lpm_proc_cb.timer_created = FALSE;
#endif
}

/*******************************************************************************
**
** Function        upio_set_bluetooth_power
**
** Description     Interact with low layer driver to set Bluetooth power
**                 on/off.
**
** Returns         0  : SUCCESS or Not-Applicable
**                 <0 : ERROR
**
*******************************************************************************/
int upio_set_bluetooth_power(int on)
{
    int sz;
    int fd = -1;
    int ret = -1;
    char buffer = '0';

    switch(on)
    {
        case UPIO_BT_POWER_OFF:
            buffer = '0';
            break;

        case UPIO_BT_POWER_ON:
            buffer = '1';
            break;
    }

    if (is_emulator_context())
    {
        /* if new value is same as current, return -1 */
        if (bt_emul_enable == on)
            return ret;

        UPIODBG("set_bluetooth_power [emul] %d", on);

        bt_emul_enable = on;
        return 0;
    }

    /* check if we have rfkill interface */
    if (is_rfkill_disabled())
        return 0;

    if (rfkill_id == -1)
    {
        if (init_rfkill())
            return ret;
    }

    fd = open(rfkill_state_path, O_WRONLY);

    if (fd < 0)
    {
        ALOGE("set_bluetooth_power : open(%s) for write failed: %s (%d)",
            rfkill_state_path, strerror(errno), errno);
        return ret;
    }

    sz = write(fd, &buffer, 1);

    if (sz < 0) {
        ALOGE("set_bluetooth_power : write(%s) failed: %s (%d)",
            rfkill_state_path, strerror(errno),errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}


/*******************************************************************************
**
** Function        upio_set
**
** Description     Set i/o based on polarity
**
** Returns         None
**
*******************************************************************************/
void upio_set(uint8_t pio, uint8_t action, uint8_t polarity)
{
    //int rc;
    AIC_UNUSED(polarity);
#if (BT_WAKE_VIA_PROC == TRUE)
    int fd = -1;
    char buffer;
#endif

    switch (pio)
    {
        case UPIO_LPM_MODE:
            if (upio_state[UPIO_LPM_MODE] == action)
            {
                AIC_UNUSED(lpm_mode[action]);
                UPIODBG("LPM is %s already", lpm_mode[action]);
                return;
            }

            upio_state[UPIO_LPM_MODE] = action;

#if (BT_WAKE_VIA_PROC == TRUE)
            fd = open(VENDOR_LPM_PROC_NODE, O_WRONLY);

            if (fd < 0)
            {
                ALOGE("upio_set : open(%s) for write failed: %s (%d)",
                        VENDOR_LPM_PROC_NODE, strerror(errno), errno);
                return;
            }

            if (action == UPIO_ASSERT)
            {
                buffer = '1';
            }
            else
            {
                buffer = '0';

                // delete btwrite assertion holding timer
                if (lpm_proc_cb.timer_created == TRUE)
                {
                    timer_delete(lpm_proc_cb.timer_id);
                    lpm_proc_cb.timer_created = FALSE;
                }
            }

            if (write(fd, &buffer, 1) < 0)
            {
                ALOGE("upio_set : write(%s) failed: %s (%d)",
                        VENDOR_LPM_PROC_NODE, strerror(errno),errno);
            }
            else
            {
                if (action == UPIO_ASSERT)
                {
                    // create btwrite assertion holding timer
                    if (lpm_proc_cb.timer_created == FALSE)
                    {
                        int status;
                        struct sigevent se;

                        se.sigev_notify = SIGEV_THREAD;
                        se.sigev_value.sival_ptr = &lpm_proc_cb.timer_id;
                        se.sigev_notify_function = proc_btwrite_timeout;
                        se.sigev_notify_attributes = NULL;

                        status = timer_create(CLOCK_MONOTONIC, &se,
                                                &lpm_proc_cb.timer_id);

                        if (status == 0)
                            lpm_proc_cb.timer_created = TRUE;
                    }
                }
            }

            if (fd >= 0)
                close(fd);
#endif
            break;

        case UPIO_BT_WAKE:
            if (upio_state[UPIO_BT_WAKE] == action)
            {
                AIC_UNUSED(lpm_state[action]);
                UPIODBG("BT_WAKE is %s already", lpm_state[action]);

#if (BT_WAKE_VIA_PROC == TRUE)
                if (lpm_proc_cb.btwrite_active == TRUE)
                    /*
                     * The proc btwrite node could have not been updated for
                     * certain time already due to heavy downstream path flow.
                     * In this case, we want to explicity touch proc btwrite
                     * node to keep the bt_wake assertion in the LPM kernel
                     * driver. The current kernel bluesleep LPM code starts
                     * a 10sec internal in-activity timeout timer before it
                     * attempts to deassert BT_WAKE line.
                     */
#endif
                return;
            }

            upio_state[UPIO_BT_WAKE] = action;

#if (BT_WAKE_VIA_USERIAL_IOCTL == TRUE)

            userial_vendor_ioctl( ( (action==UPIO_ASSERT) ? \
                      USERIAL_OP_ASSERT_BT_WAKE : USERIAL_OP_DEASSERT_BT_WAKE),\
                      NULL);

#elif (BT_WAKE_VIA_PROC == TRUE)

            /*
             *  Kick proc btwrite node only at UPIO_ASSERT
             */
            if (action == UPIO_DEASSERT)
                return;

            fd = open(VENDOR_BTWRITE_PROC_NODE, O_WRONLY);

            if (fd < 0)
            {
                ALOGE("upio_set : open(%s) for write failed: %s (%d)",
                        VENDOR_BTWRITE_PROC_NODE, strerror(errno), errno);
                return;
            }

            buffer = '1';

            if (write(fd, &buffer, 1) < 0)
            {
                ALOGE("upio_set : write(%s) failed: %s (%d)",
                        VENDOR_BTWRITE_PROC_NODE, strerror(errno),errno);
            }
            else
            {
                lpm_proc_cb.btwrite_active = TRUE;

                if (lpm_proc_cb.timer_created == TRUE)
                {
                    struct itimerspec ts;

                    ts.it_value.tv_sec = PROC_BTWRITE_TIMER_TIMEOUT_MS/1000;
                    ts.it_value.tv_nsec = 1000*(PROC_BTWRITE_TIMER_TIMEOUT_MS%1000);
                    ts.it_interval.tv_sec = 0;
                    ts.it_interval.tv_nsec = 0;

                    timer_settime(lpm_proc_cb.timer_id, 0, &ts, 0);
                }
            }

            UPIODBG("proc btwrite assertion");

            if (fd >= 0)
                close(fd);
#endif

            break;

        case UPIO_HOST_WAKE:
            UPIODBG("upio_set: UPIO_HOST_WAKE");
            break;
    }
}
