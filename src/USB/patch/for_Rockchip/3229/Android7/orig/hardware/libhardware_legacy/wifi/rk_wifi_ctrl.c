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
#include <dirent.h>

#include "hardware_legacy/wifi.h"
#include "libwpa_client/wpa_ctrl.h"

#define LOG_TAG "WifiHW"
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
#define WIFI_PRELOAD_INF         "/sys/class/rkwifi/preload"

int check_wifi_chip_type(void);
int check_wifi_chip_type_string(char *type);
int rk_wifi_power_ctrl(int on);
int rk_wifi_load_driver(int enable);
int check_wireless_ready(void);
int get_kernel_version(void);

static int identify_sucess = -1;
static char recoginze_wifi_chip[64];
static const char USB_DIR[] = "/sys/bus/usb/devices";
static const char SDIO_DIR[]= "/sys/bus/sdio/devices";
static const char PCIE_DIR[]= "/sys/bus/pci/devices";

static const char PREFIX_SDIO[] = "SDIO_ID=";
static const char PREFIX_PCIE[] = "PCI_ID=";
static const char PREFIX_USB[] = "PRODUCT=";

static int invalid_wifi_device_id = -1;

typedef struct _wifi_devices
{
  char wifi_name[64];
  char wifi_vid_pid[64];
} wifi_device;

static wifi_device supported_wifi_devices[] = {
	{"RTL8188EU",	"0bda:8179"},
	{"RTL8188EU",	"0bda:0179"},
	{"RTL8723BU",	"0bda:b720"},
	{"RTL8723BS",	"024c:b723"},
	{"RTL8822BS",	"024c:b822"},
	{"RTL8723CS",	"024c:b703"},
	{"RTL8723DS",	"024c:d723"},
	{"RTL8188FU",	"0bda:f179"},
	{"RTL8822BU",	"0bda:b82c"},
	{"RTL8189ES",	"024c:8179"},
	{"RTL8189FS",	"024c:f179"},
	{"RTL8192DU",	"0bda:8194"},
	{"RTL8812AU",	"0bda:8812"},
	{"RTL8821CU",	"0bda:c811"},
	{"SSV6051",	"3030:3030"},
	{"ESP8089",	"6666:1111"},
	{"AP6354",	"02d0:4354"},
	{"AP6330",	"02d0:4330"},
	{"AP6356S",	"02d0:4356"},
	{"AP6335",	"02d0:4335"},
	{"AP6255",      "02d0:a9bf"},
	{"RTL8822BE",	"10ec:b822"},

};

int get_wifi_device_id(const char *bus_dir, const char *prefix)
{
	int idnum;
	int i = 0;
	int ret = invalid_wifi_device_id;
	DIR *dir;
	struct dirent *next;
	FILE *fp = NULL;
	idnum = sizeof(supported_wifi_devices) / sizeof(supported_wifi_devices[0]);
	dir = opendir(bus_dir);
	if (!dir) {
		ALOGE("open dir failed: %s", strerror(errno));
		return invalid_wifi_device_id;
	}

	while ((next = readdir(dir)) != NULL) {
		char line[256];
		char uevent_file[256] = {0};
		sprintf(uevent_file, "%s/%s/uevent", bus_dir, next->d_name);
		ALOGD("uevent path:%s", uevent_file);
		fp = fopen(uevent_file, "r");
		if (NULL == fp) {
			continue;
		}

		while (fgets(line, sizeof(line), fp)) {
			char *pos = NULL;
			int product_vid = 0;
			int product_did = 0;
			int producd_bcddev = 0;
			char temp[10] = {0};
			pos = strstr(line, prefix);
			ALOGV("line: %s, prefix: %s.\n", line, prefix);
			if (pos != NULL) {
				if (strncmp(bus_dir, USB_DIR, sizeof(USB_DIR)) == 0)
					sscanf(pos + 8, "%x/%x/%x", &product_vid, &product_did, &producd_bcddev);
				else if (strncmp(bus_dir, SDIO_DIR, sizeof(SDIO_DIR)) == 0)
					sscanf(pos + 8, "%x:%x", &product_vid, &product_did);
				else if (strncmp(bus_dir, PCIE_DIR, sizeof(PCIE_DIR)) == 0)
					sscanf(pos + 7, "%x:%x", &product_vid, &product_did);
				else
					return invalid_wifi_device_id;

				sprintf(temp, "%04x:%04x", product_vid, product_did);
				ALOGD("pid:vid : %s", temp);
				for (i = 0; i < idnum; i++) {
					if (0 == strncmp(temp, supported_wifi_devices[i].wifi_vid_pid, 9)) {
						ALOGD("found device pid:vid : %s", temp);
						strcpy(recoginze_wifi_chip, supported_wifi_devices[i].wifi_name);
						identify_sucess = 1 ;
						ret = 0;
						fclose(fp);
						goto ready;
					}
				}
			}
		}
		fclose(fp);
	}

	ret = invalid_wifi_device_id;
ready:
	closedir(dir);
	ALOGD("wifi detectd return ret:%d", ret);
	return ret;
}

int check_wifi_preload(void)
{
    int wififd, ret = 0;

    wififd = open(WIFI_PRELOAD_INF, O_RDONLY);
    if( wififd < 0 ) {
        ALOGD("%s: Wifi driver is not preload when bootup, load when open wifi.\n", __func__);
        return 0;
    }
    close(wififd);
    ALOGD("%s: Wifi driver is preload when bootup.\n", __func__);
    return 1;
}

int check_wifi_chip_type_string(char *type)
{
	if (identify_sucess == -1) {
		if (get_wifi_device_id(SDIO_DIR, PREFIX_SDIO) == 0)
			ALOGD("SDIO WIFI identify sucess");
		else if (get_wifi_device_id(USB_DIR, PREFIX_USB) == 0)
			ALOGD("USB WIFI identify sucess");
		else if (get_wifi_device_id(PCIE_DIR, PREFIX_PCIE) == 0)
			ALOGD("PCIE WIFI identify sucess");
		else {
			ALOGD("maybe there is no usb wifi or sdio or pcie wifi,set default wifi module Brocom APXXX");
			strcpy(recoginze_wifi_chip, "APXXX");
			identify_sucess = 1 ;
		}
	}

	strcpy(type, recoginze_wifi_chip);
	ALOGD("%s: %s", __func__, type);
	return 0;
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

	if (get_kernel_version() == KERNEL_VERSION_4_4) {
		fp = fopen("/proc/net/dev", "r");
		if (fp == NULL) {
			ALOGE("Couldn't open /proc/net/dev\n");
			return 0;
		}
	} else if (get_kernel_version() == KERNEL_VERSION_3_10) {
		fp = fopen("/proc/net/wireless", "r");
		if (fp == NULL) {
			ALOGE("Couldn't open /proc/net/wireless\n");
			return 0;
    		}
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
    } else if (strstr(buf, "Linux version 4.4") != NULL) {
	version = KERNEL_VERSION_4_4;
	ALOGD("Kernel version is 4.4.");
    } else {
        version = KERNEL_VERSION_3_0_36;
        ALOGD("Kernel version is 3.0.36.");
    }

    return version;

fderror:
    return -1;
}
