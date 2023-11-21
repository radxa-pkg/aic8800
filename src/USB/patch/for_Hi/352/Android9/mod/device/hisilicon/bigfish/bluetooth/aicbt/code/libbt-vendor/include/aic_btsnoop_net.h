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
 *  Filename:      aic_btsnoop_net.h
 *
 *  Description:   A wrapper header file of bt_vendor_lib.h
 *
 *                 Contains definitions specific for interfacing with Aicsemi 
 *                 Bluetooth chipsets
 *
 ******************************************************************************/

#ifndef AIC_BTSNOOP_NET_H
#define AIC_BTSNOOP_NET_H

#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include "hci_h5_int.h"
#include <utils/Log.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

void aic_btsnoop_open(void);
void aic_btsnoop_close(void);
void aic_btsnoop_capture(const HC_BT_HDR *p_buf, bool is_rcvd);

void aic_btsnoop_net_open();
void aic_btsnoop_net_close();
void aic_btsnoop_net_write(serial_data_type_t type, uint8_t *data, bool is_received);

#endif
