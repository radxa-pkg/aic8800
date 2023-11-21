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
 *  Filename:      bt_vendor_aicbt.h
 *
 *  Description:   A wrapper header file of bt_vendor_lib.h
 *
 *                 Contains definitions specific for interfacing with AIC
 *                 Bluetooth chipsets
 *
 ******************************************************************************/

#ifndef BT_VENDOR_AICBT_H
#define BT_VENDOR_AICBT_H

#include "bt_vendor_lib.h"
#include "vnd_buildcfg.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   (!FALSE)
#endif

#ifndef VENDOR_LIB_RUNTIME_TUNING_ENABLED
#define VENDOR_LIB_RUNTIME_TUNING_ENABLED   FALSE
#endif

/* Run-time configuration file */
#ifndef VENDOR_LIB_CONF_FILE
#define VENDOR_LIB_CONF_FILE "/etc/bluetooth/bt_vendor.conf"
#endif

/* Device port name where Bluetooth controller attached */
#ifndef BLUETOOTH_UART_DEVICE_PORT
#define BLUETOOTH_UART_DEVICE_PORT      "/dev/ttyO1"    /* maguro */
#endif

/* Location of firmware patch files */
#ifndef FW_PATCHFILE_LOCATION
#define FW_PATCHFILE_LOCATION "/vendor/firmware/"  /* maguro */
#endif

#ifndef UART_TARGET_BAUD_RATE
#define UART_TARGET_BAUD_RATE           3000000
#endif

/* The millisecond delay pauses on HCI transport after firmware patches
 * were downloaded. This gives some time for firmware to restart with
 * patches before host attempts to send down any HCI commands.
 *
 * Note: It has been discovered that AIC8800 needs at least 200ms
 * settlement delay in here. Without the delay, a Hardware Error event
 * from AIC8800 had been seen in HCI upstream path right after the
 * host sent the HCI_VSC_SET_BDADDR commad to the controller at higher
 * baud.
 */
#ifndef FW_PATCH_SETTLEMENT_DELAY_MS
#define FW_PATCH_SETTLEMENT_DELAY_MS          0
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
 *  as AIC8800's OTP memory), the factory BDADDR can be stored in there and
 *  retrieved by the stack while enabling BT.
 *  !!! WARNING !!! Make sure that the OTP feature has been enabled in the
 *  firmware patchram (.hcd) file.
 */
#ifndef USE_CONTROLLER_BDADDR
#define USE_CONTROLLER_BDADDR   FALSE
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

/* BT_WAKE_USERIAL_LDISC

    Use line discipline if the BT_WAKE control is in line discipline
*/
#ifndef BT_WAKE_USERIAL_LDISC
#define BT_WAKE_USERIAL_LDISC           FALSE
#endif

/* BT_WAKE_VIA_PROC

    LPM & BT_WAKE control through PROC nodes
*/
#ifndef BT_WAKE_VIA_PROC
#define BT_WAKE_VIA_PROC       FALSE
#endif

#ifndef BT_WAKE_VIA_PROC_NOTIFY_DEASSERT
#define  BT_WAKE_VIA_PROC_NOTIFY_DEASSERT       TRUE
#else
#undef BT_WAKE_VIA_PROC_NOTIFY_DEASSERT
#define BT_WAKE_VIA_PROC_NOTIFY_DEASSERT       TRUE
#endif

/* N_AICBT_HCI

    UART ioctl line discipline
*/
#ifndef N_AICBT_HCI
#define N_AICBT_HCI             25
#endif

/* SCO_CFG_INCLUDED

    Do SCO configuration by default. If the firmware patch had been embedded
    with desired SCO configuration, set this FALSE to bypass configuration
    from host software.
*/
#ifndef SCO_CFG_INCLUDED
#define SCO_CFG_INCLUDED                TRUE
#endif

#ifndef SCO_USE_I2S_INTERFACE
#define SCO_USE_I2S_INTERFACE           FALSE
#endif

#define SCO_I2SPCM_PARAM_SIZE           4

/* SCO_WBS_SAMPLE_RATE
    0 : 8K
    1 : 16K
    2 : 4K
 This macro is used for setting WBS sampling rate for a SCO connection
 If the mobile network supports WBS, we need to use 16KHz as default
 but if the platform doesn't support 16KHz, the sample rate can be
 overriden to 8KHz by setting this to 0.
*/
#ifndef SCO_WBS_SAMPLE_RATE
#define SCO_WBS_SAMPLE_RATE            1
#endif


/* SCO_I2SPCM_IF_MODE - 0=Disable, 1=Enable */
#ifndef SCO_I2SPCM_IF_MODE
#define SCO_I2SPCM_IF_MODE              1
#endif

/* SCO_I2SPCM_IF_ROLE - 0=Slave, 1=Master */
#ifndef SCO_I2SPCM_IF_ROLE
#define SCO_I2SPCM_IF_ROLE              1
#endif

/* SCO_I2SPCM_IF_SAMPLE_RATE

    0 : 8K
    1 : 16K
    2 : 4K
*/
#ifndef SCO_I2SPCM_IF_SAMPLE_RATE
#define SCO_I2SPCM_IF_SAMPLE_RATE       0
#endif

/* SCO_I2SPCM_IF_CLOCK_RATE

    0 : 128K
    1 : 256K
    2 : 512K
    3 : 1024K
    4 : 2048K
*/
#ifndef SCO_I2SPCM_IF_CLOCK_RATE
#define SCO_I2SPCM_IF_CLOCK_RATE        1
#endif

/* SCO_I2SPCM_IF_CLOCK_RATE4WBS

    0 : 128K
    1 : 256K
    2 : 512K
    3 : 1024K
    4 : 2048K
*/
#ifndef SCO_I2SPCM_IF_CLOCK_RATE4WBS
#define SCO_I2SPCM_IF_CLOCK_RATE4WBS        2
#endif


#define SCO_PCM_PARAM_SIZE              5

/* SCO_PCM_ROUTING

    0 : PCM
    1 : Transport
    2 : Codec
    3 : I2S
*/
#ifndef SCO_PCM_ROUTING
#define SCO_PCM_ROUTING                 0
#endif

/* SCO_PCM_IF_CLOCK_RATE

    NOTICE: suggested to be consistent with SCO_I2SPCM_IF_CLOCK_RATE

    0 : 128K
    1 : 256K
    2 : 512K
    3 : 1024K
    4 : 2048K
*/
#ifndef SCO_PCM_IF_CLOCK_RATE
#define SCO_PCM_IF_CLOCK_RATE           4
#endif

/* SCO_PCM_IF_FRAME_TYPE - 0=Short, 1=Long */
#ifndef SCO_PCM_IF_FRAME_TYPE
#define SCO_PCM_IF_FRAME_TYPE           0
#endif

/* SCO_PCM_IF_SYNC_MODE

    NOTICE: in most usage cases the value will be the same as
            SCO_PCM_IF_CLOCK_MODE setting

    0 : Slave
    1 : Master
*/
#ifndef SCO_PCM_IF_SYNC_MODE
#define SCO_PCM_IF_SYNC_MODE            0
#endif

/* SCO_PCM_IF_CLOCK_MODE

    NOTICE: suggested to be consistent with SCO_I2SPCM_IF_ROLE

    0 : Slave
    1 : Master
*/
#ifndef SCO_PCM_IF_CLOCK_MODE
#define SCO_PCM_IF_CLOCK_MODE           0
#endif

#define PCM_DATA_FORMAT_PARAM_SIZE      5

/* PCM_DATA_FMT_SHIFT_MODE

    0 : MSB first
    1 : LSB first
*/
#ifndef PCM_DATA_FMT_SHIFT_MODE
#define PCM_DATA_FMT_SHIFT_MODE         0
#endif

/* PCM_DATA_FMT_FILL_BITS

    Specifies the value with which to fill unused bits
    if Fill_Method is set to programmable
*/
#ifndef PCM_DATA_FMT_FILL_BITS
#define PCM_DATA_FMT_FILL_BITS          0
#endif

/* PCM_DATA_FMT_FILL_METHOD

    0 : 0's
    1 : 1's
    2 : Signed
    3 : Programmable
*/
#ifndef PCM_DATA_FMT_FILL_METHOD
#define PCM_DATA_FMT_FILL_METHOD        3
#endif

/* PCM_DATA_FMT_FILL_NUM

    Specifies the number of bits to be filled
*/
#ifndef PCM_DATA_FMT_FILL_NUM
#define PCM_DATA_FMT_FILL_NUM           0
#endif

/* PCM_DATA_FMT_JUSTIFY_MODE

    0 : Left justify (fill data shifted out last)
    1 : Right justify (fill data shifted out first)
*/
#ifndef PCM_DATA_FMT_JUSTIFY_MODE
#define PCM_DATA_FMT_JUSTIFY_MODE       0
#endif

/* HW_END_WITH_HCI_RESET

    Sample code implementation of sending a HCI_RESET command during the epilog
    process. It calls back to the callers after command complete of HCI_RESET
    is received.
*/
#ifndef HW_END_WITH_HCI_RESET
#define HW_END_WITH_HCI_RESET    TRUE
#endif

/******************************************************************************
**  Extern variables and functions
******************************************************************************/

extern bt_vendor_callbacks_t *bt_vendor_cbacks;

extern int hw_set_audio_state(bt_vendor_op_audio_state_t *p_state);

#endif /* BT_VENDOR_AICBT_H */

