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
 *  Filename:      bt_vendor_aic.h
 *
 *  Description:   A wrapper header file of bt_vendor_lib.h
 *
 *                 Contains definitions specific for interfacing with Aicsemi
 *                 Bluetooth chipsets
 *
 ******************************************************************************/

#ifndef BT_VENDOR_AIC_H
#define BT_VENDOR_AIC_H

#include "bt_vendor_lib.h"
#include "aic_btsnoop_net.h"
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <ctype.h>
#include <cutils/properties.h>
#include "aic_common.h"

#define AIC_VERSION "5.1.1"
/******************************************************************************
**  Constants & Macros
******************************************************************************/
#define AICBT_TRANS_H4          0x20
#define AICBT_TRANS_H5          0x10
#define AICBT_TRANS_UART        0x01
#define AICBT_TRANS_USB         0x02

#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   (!FALSE)
#endif

#ifndef BTVND_DBG
#define BTVND_DBG TRUE
#endif

#if (BTVND_DBG == TRUE)
#define BTVNDDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define BTVNDDBG(param, ...) {}
#endif

#define DOWN_FW_CFG             _IOW('E', 176, int)
#define SET_ISO_CFG             _IOW('E', 177, int)
#define RESET_CONTROLLER        _IOW('E', 178, int)
#define DWFW_CMPLT              _IOW('E', 179, int)

#define GET_USB_INFO            _IOR('E', 180, int)


/* Device port name where Bluetooth controller attached */
#ifndef BLUETOOTH_UART_DEVICE_PORT
#define BLUETOOTH_UART_DEVICE_PORT      "/dev/ttyS1"    /* maguro */
#endif

/* Location of firmware patch files */
#ifndef FW_PATCHFILE_LOCATION
#define FW_PATCHFILE_LOCATION "/vendor/etc/firmware/"  /* maguro */
#endif

#ifndef UART_TARGET_BAUD_RATE
#define UART_TARGET_BAUD_RATE           3000000
#endif



/* The Bluetooth Device Aaddress source switch:
 *
 * -FALSE- (default value)
 *  Get the factory BDADDR from device's file system. Normally the BDADDR is
 *  stored in the location pointed by the PROPERTY_BT_BDADDR_PATH (defined in
 *  btif_common.h file) property.
 *
 * -TRUE-
 *  If the Bluetooth Controller has equipped with a non-volatile memory (such
 *  as BCM4330's OTP memory), the factory BDADDR can be stored in there and
 *  retrieved by the stack while enabling BT.
 *  !!! WARNING !!! Make sure that the OTP feature has been enabled in the
 *  firmware patchram (.hcd) file.
 */
#ifndef USE_CONTROLLER_BDADDR
#define USE_CONTROLLER_BDADDR   TRUE
#endif

/* sleep mode

    0: disable
    1: UART with Host wake/BT wake out of band signals
*/
#ifndef LPM_SLEEP_MODE
#define LPM_SLEEP_MODE                  1
#endif

/* Host Stack Idle Threshold in 300ms or 25ms

  In sleep mode 1, this is the number of firmware loops executed with no
    activity before the Host wake line is deasserted. Activity includes HCI
    traffic excluding certain sleep mode commands and the presence of SCO
    connections if the "Allow Host Sleep During SCO" flag is not set to 1.
    Each count of this parameter is roughly equivalent to 300ms or 25ms.
*/
#ifndef LPM_IDLE_THRESHOLD
#define LPM_IDLE_THRESHOLD              1
#endif

/* Host Controller Idle Threshold in 300ms or 25ms

    This is the number of firmware loops executed with no activity before the
    HC is considered idle. Depending on the mode, HC may then attempt to sleep.
    Activity includes HCI traffic excluding certain sleep mode commands and
    the presence of ACL/SCO connections.
*/
#ifndef LPM_HC_IDLE_THRESHOLD
#define LPM_HC_IDLE_THRESHOLD           1
#endif

/* BT_WAKE Polarity - 0=Active Low, 1= Active High */
#ifndef LPM_BT_WAKE_POLARITY
#define LPM_BT_WAKE_POLARITY            1    /* maguro */
#endif

/* HOST_WAKE Polarity - 0=Active Low, 1= Active High */
#ifndef LPM_HOST_WAKE_POLARITY
#define LPM_HOST_WAKE_POLARITY          1    /* maguro */
#endif

/* LPM_ALLOW_HOST_SLEEP_DURING_SCO

    When this flag is set to 0, the host is not allowed to sleep while
    an SCO is active. In sleep mode 1, the device will keep the host
    wake line asserted while an SCO is active.
    When this flag is set to 1, the host can sleep while an SCO is active.
    This flag should only be set to 1 if SCO traffic is directed to the PCM
    interface.
*/
#ifndef LPM_ALLOW_HOST_SLEEP_DURING_SCO
#define LPM_ALLOW_HOST_SLEEP_DURING_SCO 1
#endif

/* LPM_COMBINE_SLEEP_MODE_AND_LPM

    In Mode 0, always set byte 7 to 0. In sleep mode 1, device always
    requires permission to sleep between scans / periodic inquiries regardless
    of the setting of this byte. In sleep mode 1, if byte is set, device must
    have "permission" to sleep during the low power modes of sniff, hold, and
    park. If byte is not set, device can sleep without permission during these
    modes. Permission to sleep in Mode 1 is obtained if the BT_WAKE signal is
    not asserted.
*/
#ifndef LPM_COMBINE_SLEEP_MODE_AND_LPM
#define LPM_COMBINE_SLEEP_MODE_AND_LPM  1
#endif

/* LPM_ENABLE_UART_TXD_TRI_STATE

    When set to 0, the device will not tristate its UART TX line before going
    to sleep.
    When set to 1, the device will tristate its UART TX line before going to
    sleep.
*/
#ifndef LPM_ENABLE_UART_TXD_TRI_STATE
#define LPM_ENABLE_UART_TXD_TRI_STATE   0
#endif

/* LPM_PULSED_HOST_WAKE
*/
#ifndef LPM_PULSED_HOST_WAKE
#define LPM_PULSED_HOST_WAKE            0
#endif

/* LPM_IDLE_TIMEOUT_MULTIPLE

    The multiple factor of host stack idle threshold in 300ms/25ms
*/
#ifndef LPM_IDLE_TIMEOUT_MULTIPLE
#define LPM_IDLE_TIMEOUT_MULTIPLE       10
#endif

/* BT_WAKE_VIA_USERIAL_IOCTL

    Use userial ioctl function to control BT_WAKE signal
*/
#ifndef BT_WAKE_VIA_USERIAL_IOCTL
#define BT_WAKE_VIA_USERIAL_IOCTL       FALSE
#endif

/* BT_WAKE_VIA_PROC

    LPM & BT_WAKE control through PROC nodes
*/
#ifndef BT_WAKE_VIA_PROC
#define BT_WAKE_VIA_PROC       FALSE
#endif

/* HW_END_WITH_HCI_RESET

    Sample code implementation of sending a HCI_RESET command during the epilog
    process. It calls back to the callers after command complete of HCI_RESET
    is received.
*/
#ifndef HW_END_WITH_HCI_RESET
#define HW_END_WITH_HCI_RESET    FALSE
#endif

/******************************************************************************
**  Extern variables and functions
******************************************************************************/

extern bt_vendor_callbacks_t *bt_vendor_cbacks;

#endif /* BT_VENDOR_AIC_H */
