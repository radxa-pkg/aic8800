/******************************************************************************
 *
 *  Copyright (C) 2019-2021 Aicsemi Corporation.
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
 *  Filename:      userial_vendor.c
 *
 *  Description:   Contains vendor-specific userial functions
 *
 ******************************************************************************/
#undef NDEBUG
#define LOG_TAG "aic_socket"

#include <utils/Log.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include "userial.h"
#include "userial_vendor.h"
#include "aic_socket.h"

/******************************************************************************
**  Constants & Macros
******************************************************************************/

/******************************************************************************
**  Extern functions
******************************************************************************/


/******************************************************************************
**  Local type definitions
******************************************************************************/

/******************************************************************************
**  Static functions
******************************************************************************/

/******************************************************************************
**  functions
******************************************************************************/
uint32_t Skt_Read(int fd, uint8_t *p_buf, uint32_t len, bool* condition)
{
    int n_read = 0;
    struct pollfd pfd;

    if (fd == -1) {
        return 0;
    }

    while (n_read < (int)len) {
        if (condition && !(*condition))
            return n_read;

        pfd.fd = fd;
        pfd.events = POLLIN|POLLHUP|POLLNVAL|POLLRDHUP;

        /* make sure there is data prior to attempting read to avoid blocking
           a read for more than poll timeout */

        int poll_ret;
        AIC_NO_INTR(poll_ret = poll(&pfd, 1, 100));
        if (poll_ret == 0) {
            continue;
        }

        if (poll_ret < 0) {
            ALOGE("%s(): poll() failed: return %d errno %d (%s)",
                           __func__, poll_ret, errno, strerror(errno));
            break;
        }

        if (pfd.revents & (POLLHUP|POLLNVAL|POLLRDHUP))
            return 0;

        ssize_t n;
        AIC_NO_INTR(n = recv(fd, p_buf + n_read, len - n_read, 0));

        if (n == 0) {
            ALOGE("Skt_Read : channel detached remotely");
            return 0;
        }

        if (n < 0) {
            ALOGE("Skt_Read : read failed (%s)", strerror(errno));
            return 0;
        }

        n_read += n;

    }
    return n_read;
}

int Skt_Read_noblock(int fd, uint8_t *p_buf, uint32_t len)
{
    int n_read = 0;
    struct pollfd pfd;

    if (fd == -1) {
        ALOGE("UIPC_Read_noblock closed");
        return 0;
    }

    pfd.fd = fd;
    pfd.events = POLLIN|POLLHUP|POLLRDHUP;

    if (poll(&pfd, 1, 0) == 0)
        return 0;

    if (pfd.revents & (POLLHUP|POLLNVAL|POLLRDHUP) )
        return 0;

    n_read = recv(fd, p_buf, len, MSG_DONTWAIT|MSG_NOSIGNAL);

    return n_read;
}

bool Skt_Send(int fd, uint8_t *p_buf, uint16_t msglen)
{
    ssize_t ret;
    AIC_NO_INTR(ret = write(fd, p_buf, msglen));
    if (ret < 0) {
        ALOGE("failed to write (%s)", strerror(errno));
    }

    return false;
}

int Skt_Send_noblock(int fd, uint8_t *p_buf, uint16_t msglen)
{
    int res = 0;
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = POLLOUT|POLLHUP;
    if (poll(&pfd, 1, 0) == 0)
        return 0;

    if (pfd.revents & (POLLHUP|POLLNVAL)) {
        ALOGE("poll : channel detached remotely");
        return 0;
    }

    res = send(fd, p_buf, msglen, MSG_DONTWAIT);
    if (res < 0)
        ALOGE("failed to write (%s)", strerror(errno));

    return res;
}

/******************************************************************************
**  Static variables
******************************************************************************/

/*****************************************************************************
**   Helper Functions
*****************************************************************************/


