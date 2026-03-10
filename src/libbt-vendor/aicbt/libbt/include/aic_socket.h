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
 *  Filename:      userial_vendor.h
 *
 *  Description:   Contains vendor-specific definitions used in serial port
 *                 controls
 *
 ******************************************************************************/

#ifndef AIC_SOCKET_H
#define AIC_SOCKET_H

#include "bt_vendor_aic.h"
#include "userial.h"
#include <sys/poll.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/un.h>


#ifdef CONFIG_SCO_OVER_HCI
#define SCO_CTRL_PATH "/data/misc/bluedroid/.sco_ctrl"
#define SCO_DATA_PATH "/data/misc/bluedroid/.sco_data"

typedef enum {
    SCO_CTRL_CMD_NONE,
    SCO_CTRL_CMD_CHECK_READY,
    SCO_CTRL_CMD_OUT_START,
    SCO_CTRL_CMD_IN_START,
    SCO_CTRL_CMD_OUT_STOP,
    SCO_CTRL_CMD_IN_STOP,
    SCO_CTRL_CMD_SUSPEND,
    SCO_CTRL_GET_AUDIO_CONFIG,
    SCO_CTRL_CMD_OFFLOAD_START,
    SCO_CTRL_CMD_CLOSE,
} tSCO_CTRL_CMD;

#define SCO_SAMPLE_RATE_8K   1
#define SCO_SAMPLE_RATE_16K   2

#endif

#define MAX(a,b) ((a)>(b)?(a):(b))

/******************************************************************************
**  Constants & Macros
******************************************************************************/

uint32_t Skt_Read(int fd, uint8_t *p_buf, uint32_t len, bool* condition);
int Skt_Read_noblock(int fd, uint8_t *p_buf, uint32_t len);
bool Skt_Send(int fd, uint8_t *p_buf, uint16_t msglen);
int Skt_Send_noblock(int fd, uint8_t *p_buf, uint16_t msglen);
#endif
