/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BDROID_BUILDCFG_H
#define _BDROID_BUILDCFG_H

#define BTM_DEF_LOCAL_NAME   "Aicsemi Bluetooth"

// SERVICE_CLASS:0x1A (Bit17 -Networking,Bit19 - Capturing,Bit20 -Object Transfer)
// MAJOR CLASS: COMPUTER
// MINOR CLASS: TABLET
#define BTA_DM_COD {0x1A, 0x01, 0x1C}

#define BTA_GATT_DEBUG FALSE

#define PORT_RX_BUF_LOW_WM  (10)
#define PORT_RX_BUF_HIGH_WM  (40)
#define PORT_RX_BUF_CRITICAL_WM  (45)
#define PORT_CREDIT_RX_MAX   (48)

#define HCI_MAX_SIMUL_CMDS (1)
#define BTM_BLE_SCAN_SLOW_INT_1 (144)
#define BTM_BLE_SCAN_SLOW_WIN_1 (16)
#define BTM_MAX_VSE_CALLBACKS  (6)

#define BTM_BLE_CONN_INT_MIN_DEF     0x06
#define BTM_BLE_CONN_INT_MAX_DEF     0x0C
#define BTM_BLE_CONN_TIMEOUT_DEF     200

#define BTIF_HF_SERVICES (BTA_HFP_SERVICE_MASK)
#define BTIF_HF_SERVICE_NAMES  { BTIF_HSAG_SERVICE_NAME, NULL }

#define BTA_DISABLE_DELAY 1000 /* in milliseconds */
#define BTA_HOST_INTERLEAVE_SEARCH FALSE

/*heartbeat log define*/
#define BTPOLL_DBG FALSE
/*hci log define*/
#define BTHC_DBG FALSE
/*avdtp log define*/
//#define AVDT_DEBUG TRUE
/*BT log verbose*/
#define BT_TRACE_VERBOSE TRUE
/* BT trace messages*/
#define BT_USE_TRACES  TRUE
/*A2DP SINK ENABLE*/
#define BTA_AV_SINK_INCLUDED TRUE
#define BLE_LOCAL_PRIVACY_ENABLED FALSE
#define USE_AUDIO_TRACK TRUE
/*BT lib vendor log*/
//#define BTVND_DBG TRUE
/*page timeout */
#define BTA_DM_PAGE_TIMEOUT 8192
#define BTM_LOCAL_IO_CAPS_BLE   BTM_IO_CAP_KBDISP
#define BT_HCI_DEVICE_NODE_MAX_LEN 512

#define KERNEL_MISSING_CLOCK_BOOTTIME_ALARM TRUE
#endif
