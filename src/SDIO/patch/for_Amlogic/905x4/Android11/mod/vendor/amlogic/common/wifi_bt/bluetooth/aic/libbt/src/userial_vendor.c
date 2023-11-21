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
 *  Filename:      userial_vendor.c
 *
 *  Description:   Contains vendor-specific userial functions
 *
 ******************************************************************************/

#define LOG_TAG "bt_userial_vendor"

#include <utils/Log.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bt_vendor_aicbt.h"
#include "userial.h"
#include "userial_vendor.h"
#include <unistd.h>

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#ifndef VNDUSERIAL_DBG
#define VNDUSERIAL_DBG FALSE
#endif

#if (VNDUSERIAL_DBG == TRUE)
#define VNDUSERIALDBG(param, ...) {ALOGD(param, ## __VA_ARGS__);}
#else
#define VNDUSERIALDBG(param, ...) {}
#endif

#define VND_PORT_NAME_MAXLEN    256

/******************************************************************************
**  Local type definitions
******************************************************************************/

/* vendor serial control block */
typedef struct {
    int fd;                     /* fd to Bluetooth device */
    struct termios termios;     /* serial terminal of BT port */
    char port_name[VND_PORT_NAME_MAXLEN];
} vnd_userial_cb_t;

/******************************************************************************
**  Static variables
******************************************************************************/

static vnd_userial_cb_t vnd_userial;

/*****************************************************************************
**   Helper Functions
*****************************************************************************/

/*******************************************************************************
**
** Function        userial_to_tcio_baud
**
** Description     helper function converts USERIAL baud rates into TCIO
**                  conforming baud rates
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
uint8_t userial_to_tcio_baud(uint8_t cfg_baud, uint32_t *baud)
{
    if (cfg_baud == USERIAL_BAUD_115200)
        *baud = B115200;
    else if (cfg_baud == USERIAL_BAUD_4M)
        *baud = B4000000;
    else if (cfg_baud == USERIAL_BAUD_3_5M)
        *baud = B3500000;
    else if (cfg_baud == USERIAL_BAUD_3M)
        *baud = B3000000;
    else if (cfg_baud == USERIAL_BAUD_2_5M)
        *baud = B2500000;
    else if (cfg_baud == USERIAL_BAUD_2M)
        *baud = B2000000;
    else if (cfg_baud == USERIAL_BAUD_1_5M)
        *baud = B1500000;
    else if (cfg_baud == USERIAL_BAUD_1M)
        *baud = B1000000;
    else if (cfg_baud == USERIAL_BAUD_921600)
        *baud = B921600;
    else if (cfg_baud == USERIAL_BAUD_460800)
        *baud = B460800;
    else if (cfg_baud == USERIAL_BAUD_230400)
        *baud = B230400;
    else if (cfg_baud == USERIAL_BAUD_57600)
        *baud = B57600;
    else if (cfg_baud == USERIAL_BAUD_19200)
        *baud = B19200;
    else if (cfg_baud == USERIAL_BAUD_9600)
        *baud = B9600;
    else if (cfg_baud == USERIAL_BAUD_1200)
        *baud = B1200;
    else if (cfg_baud == USERIAL_BAUD_600)
        *baud = B600;
    else {
        ALOGE( "userial vendor open: unsupported baud idx %i", cfg_baud);
        *baud = B115200;
        return FALSE;
    }

    return TRUE;
}

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
/*******************************************************************************
**
** Function        userial_ioctl_init_bt_wake
**
** Description     helper function to set the open state of the bt_wake if ioctl
**                  is used. it should not hurt in the rfkill case but it might
**                  be better to compile it out.
**
** Returns         none
**
*******************************************************************************/
void userial_ioctl_init_bt_wake(int fd)
{
    uint32_t bt_wake_state;

#if (BT_WAKE_USERIAL_LDISC==TRUE)
    int ldisc = N_AICBT_HCI; /* acibt sleep mode support line discipline */

    /* attempt to load enable discipline driver */
    if (ioctl(vnd_userial.fd, TIOCSETD, &ldisc) < 0) {
        VNDUSERIALDBG("USERIAL_Open():fd %d, TIOCSETD failed: error %d for ldisc: %d",
                      fd, errno, ldisc);
    }
#endif

    /* assert BT_WAKE through ioctl */
    ioctl(fd, USERIAL_IOCTL_BT_WAKE_ASSERT, NULL);
    ioctl(fd, USERIAL_IOCTL_BT_WAKE_GET_ST, &bt_wake_state);
    VNDUSERIALDBG("userial_ioctl_init_bt_wake read back BT_WAKE state=%i", \
               bt_wake_state);
}
#endif // (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)


/*****************************************************************************
**   Userial Vendor API Functions
*****************************************************************************/

/*******************************************************************************
**
** Function        userial_vendor_init
**
** Description     Initialize userial vendor-specific control block
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_init(void)
{
    vnd_userial.fd = -1;
    snprintf(vnd_userial.port_name, VND_PORT_NAME_MAXLEN, "%s", \
            BLUETOOTH_UART_DEVICE_PORT);
}

/*******************************************************************************
**
** Function        userial_vendor_open
**
** Description     Open the serial port with the given configuration
**
** Returns         device fd
**
*******************************************************************************/
int userial_vendor_open(tUSERIAL_CFG *p_cfg)
{

    uint32_t baud;
    uint8_t data_bits;
    uint16_t parity;
    uint8_t stop_bits;

    vnd_userial.fd = -1;

    if (!userial_to_tcio_baud(p_cfg->baud, &baud)) {
        return -1;
    }

    if (p_cfg->fmt & USERIAL_DATABITS_8)
        data_bits = CS8;
    else if (p_cfg->fmt & USERIAL_DATABITS_7)
        data_bits = CS7;
    else if (p_cfg->fmt & USERIAL_DATABITS_6)
        data_bits = CS6;
    else if (p_cfg->fmt & USERIAL_DATABITS_5)
        data_bits = CS5;
    else {
        ALOGE("userial vendor open: unsupported data bits");
        return -1;
    }

    if (p_cfg->fmt & USERIAL_PARITY_NONE)
        parity = 0;
    else if (p_cfg->fmt & USERIAL_PARITY_EVEN)
        parity = PARENB;
    else if (p_cfg->fmt & USERIAL_PARITY_ODD)
        parity = (PARENB | PARODD);
    else {
        ALOGE("userial vendor open: unsupported parity bit mode");
        return -1;
    }

    if (p_cfg->fmt & USERIAL_STOPBITS_1)
        stop_bits = 0;
    else if (p_cfg->fmt & USERIAL_STOPBITS_2)
        stop_bits = CSTOPB;
    else {
        ALOGE("userial vendor open: unsupported stop bits");
        return -1;
    }

    ALOGI("userial vendor open: opening %s", vnd_userial.port_name);

    if ((vnd_userial.fd = open(vnd_userial.port_name, O_RDWR)) == -1) {
        ALOGE("userial vendor open: unable to open %s", vnd_userial.port_name);
        return -1;
    }
	

    tcflush(vnd_userial.fd, TCIOFLUSH);

    tcgetattr(vnd_userial.fd, &vnd_userial.termios);
    cfmakeraw(&vnd_userial.termios);
    vnd_userial.termios.c_cflag |= (CRTSCTS | stop_bits);
    //vnd_userial.termios.c_cflag |= stop_bits;
    //vnd_userial.termios.c_cflag &= ~CRTSCTS;


    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);
    tcflush(vnd_userial.fd, TCIOFLUSH);

    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);
    tcflush(vnd_userial.fd, TCIOFLUSH);
    tcflush(vnd_userial.fd, TCIOFLUSH);

    /* set input/output baudrate */
    cfsetospeed(&vnd_userial.termios, baud);
    cfsetispeed(&vnd_userial.termios, baud);
    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
    userial_ioctl_init_bt_wake(vnd_userial.fd);
#endif

    ALOGI("device fd = %d open", vnd_userial.fd);

    return vnd_userial.fd;

}

/*******************************************************************************
**
** Function        userial_vendor_close
**
** Description     Conduct vendor-specific close work
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_close(void)
{
    int result;

    if (vnd_userial.fd == -1)
        return;

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
    /* de-assert bt_wake BEFORE closing port */
    ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_DEASSERT, NULL);
#endif

    ALOGI("device fd = %d close", vnd_userial.fd);
    // flush Tx before close to make sure no chars in buffer
    tcflush(vnd_userial.fd, TCIOFLUSH);
    if ((result = close(vnd_userial.fd)) < 0)
        ALOGE( "close(fd:%d) FAILED result:%d", vnd_userial.fd, result);

    vnd_userial.fd = -1;
}

/*******************************************************************************
**
** Function        userial_vendor_set_baud
**
** Description     Set new baud rate
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_set_baud(uint8_t userial_baud)
{
    uint32_t tcio_baud;

    userial_to_tcio_baud(userial_baud, &tcio_baud);

    cfsetospeed(&vnd_userial.termios, tcio_baud);
    cfsetispeed(&vnd_userial.termios, tcio_baud);
    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);
}

/*******************************************************************************
**
** Function        userial_vendor_ioctl
**
** Description     ioctl inteface
**
** Returns         None
**
*******************************************************************************/
void userial_vendor_ioctl(userial_vendor_ioctl_op_t op, void *p_data)
{
    switch (op) {
#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
        case USERIAL_OP_ASSERT_BT_WAKE:
            VNDUSERIALDBG("## userial_vendor_ioctl: Asserting BT_Wake ##");
            ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_ASSERT, NULL);
            break;

        case USERIAL_OP_DEASSERT_BT_WAKE:
            VNDUSERIALDBG("## userial_vendor_ioctl: De-asserting BT_Wake ##");
            ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_DEASSERT, NULL);
            break;

        case USERIAL_OP_GET_BT_WAKE_STATE:
            ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_GET_ST, p_data);
            break;
#endif  //  (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)

        default:
            break;
    }
}

/*******************************************************************************
**
** Function        userial_set_port
**
** Description     Configure UART port name
**
** Returns         0 : Success
**                 Otherwise : Fail
**
*******************************************************************************/
int userial_set_port(char *p_conf_name, char *p_conf_value, int param)
{
    strcpy(vnd_userial.port_name, p_conf_value);

    return 0;
}

