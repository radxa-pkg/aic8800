/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "hardware_legacy/wifi.h"
#include "libwpa_client/wpa_ctrl.h"

#define LOG_TAG "RkWifiCtrl"
#include "cutils/log.h"
#include "cutils/memory.h"
#include "cutils/misc.h"
#include "cutils/properties.h"
#include "private/android_filesystem_config.h"
#ifdef HAVE_LIBC_SYSTEM_PROPERTIES
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#endif

#define WIFI_CHIP_TYPE_PATH	"/sys/class/rkwifi/chip"
#define WIFI_POWER_INF          "/sys/class/rkwifi/power"
#define WIFI_DRIVER_INF         "/sys/class/rkwifi/driver"

int check_wifi_chip_type(void);
int rk_wifi_power_ctrl(int on);
int rk_wifi_load_driver(int enable);
int check_wireless_ready(void);
int get_kernel_version(void);


int check_wifi_chip_type(void)
{
    int wififd;
    char buf[64];
    int wifi_chip_type = RTL8188EU;

    wififd = open(WIFI_CHIP_TYPE_PATH, O_RDONLY);
    if( wififd < 0 ){
        ALOGD("Can't open %s, errno = %d", WIFI_CHIP_TYPE_PATH, errno);
        goto done;
    }

    memset(buf, 0, 64);

    if( 0 == read(wififd, buf, 10) ){
        ALOGD("read %s failed", WIFI_CHIP_TYPE_PATH);
        close(wififd);
        goto done;
    }
    close(wififd);

    if(0 == strncmp(buf, "RTL8188CU", strlen("RTL8188CU")) )
    {
        wifi_chip_type = RTL8188CU;
        ALOGD("Read wifi chip type OK ! wifi_chip_type = RTL8188CU");
    }
    if(0 == strncmp(buf, "RTL8188EU", strlen("RTL8188EU")) )
    {
        wifi_chip_type = RTL8188EU;
        ALOGD("Read wifi chip type OK ! wifi_chip_type = RTL8188EU");
    }
    else if (0 == strncmp(buf, "BCM4330", strlen("BCM4330")) )
    {
        wifi_chip_type = BCM4330;
        ALOGD("Read wifi chip type OK ! wifi_chip_type = BCM4330");
    }
    else if (0 == strncmp(buf, "RK901", strlen("RK901")) )
    {
        wifi_chip_type = RK901;
        ALOGD("Read wifi chip type OK ! wifi_chip_type = RK901");
    }
    else if (0 == strncmp(buf, "RK903", strlen("RK903")) )
    {
        wifi_chip_type = RK903;
        ALOGD("Read wifi chip type OK ! wifi_chip_type = RK903");
    }
    else if (0 == strncmp(buf,"ESP8089",strlen("ESP8089")))
    {
        wifi_chip_type = ESP8089;
	ALOGD("Read wifi chip type OK ! wifi_chip_type = ESP8089");
    }
    else if (0 == strncmp(buf,"AIC8800",strlen("AIC8800")))
    {
        wifi_chip_type = AIC8800;
        ALOGD("Read wifi chip type OK ! wifi_chip_type = AIC8800");
    }
done:
    return wifi_chip_type;
}

int rk_wifi_power_ctrl(int on)
{
    int sz, fd = -1;
    int ret = -1;
    char buffer = '0';

    ALOGE("rk_wifi_power_ctrl:(%d)", on);
    switch(on)
    {
        case 0:
            buffer = '0';
            break;

        case 1:
            buffer = '1';
            break;
    }

    fd = open(WIFI_POWER_INF, O_WRONLY);
    if (fd < 0)
    {
        ALOGE("rk_wifi_power_ctrl: open(%s) for write failed: %s (%d)",
            WIFI_POWER_INF, strerror(errno), errno);
        return ret;
    }

    sz = write(fd, &buffer, 1);

    if (sz < 0) {
        ALOGE("rk_wifi_power_ctrl: write(%s) failed: %s (%d)",
            &buffer, strerror(errno),errno);
    }
    else {
        ret = 0;
        usleep(1000*1000);
    }

    if (fd >= 0)
        close(fd);

    return ret;
}

/* enable = 0 or 1 */
/* 0 - rmmod driver; 1 - insmod driver. */
int rk_wifi_load_driver(int enable)
{
    int sz, fd = -1;
    int ret = -1;
    char buffer = '0';

    ALOGE("rk_wifi_load_driver:(%s)", enable? "insmod":"rmmod");
    switch(enable)
    {
        case 0:
            buffer = '0';
            break;

        case 1:
            buffer = '1';
            break;
    }

    fd = open(WIFI_DRIVER_INF, O_WRONLY);
    if (fd < 0)
    {
        ALOGE("rk_wifi_load_driver: open(%s) for write failed: %s (%d)",
            WIFI_DRIVER_INF, strerror(errno), errno);
        return ret;
    }

    sz = write(fd, &buffer, 1);

    if (sz < 0) {
        ALOGE("rk_wifi_load_driver: write(%s) failed: %s (%d)",
            &buffer, strerror(errno),errno);
    }
    else {
        ret = 0;
        usleep(1000*1000);
    }

    if (fd >= 0)
        close(fd);

    return ret;
}

/* 0 - not ready; 1 - ready. */
int check_wireless_ready(void)
{
    char line[1024], *ptr = NULL;
    FILE *fp = NULL;

    fp = fopen("/proc/net/wireless", "r");
    if (fp == NULL) {
        ALOGE("Couldn't open /proc/net/wireless\n");
        return 0;
    }

    while(fgets(line, 1024, fp)) {
        if ((strstr(line, "wlan0:") != NULL) || (strstr(line, "p2p0:") != NULL)) {
            ALOGD("Wifi driver is ready for now...");
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);

    ALOGE("Wifi driver is not ready.\n");
    return 0;
}

int get_kernel_version(void)
{
    int fd, version = 0;
    char buf[64];

    fd = open("/proc/version", O_RDONLY);
    if (fd < 0) {
        ALOGD("Can't open '/proc/version', errno = %d", errno);
        goto fderror;
    }
    memset(buf, 0, 64);
    if( 0 == read(fd, buf, 64) ){
        ALOGD("read '/proc/version' failed");
        close(fd);
        goto fderror;
    }
    close(fd);
    if (strstr(buf, "Linux version 3.10") != NULL) {
        version = KERNEL_VERSION_3_10;
        ALOGD("Kernel version is 3.10.");
    } else {
        version = KERNEL_VERSION_3_0_36;
        ALOGD("Kernel version is 3.0.36.");
    }

    return version;

fderror:
    return -1;
}
