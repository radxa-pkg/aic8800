/*
 * Copyright 2016, The Android Open Source Project
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

#include "hardware_legacy/rk_wifi.h"

#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>

#include <android-base/logging.h>
#include <cutils/misc.h>
#include <cutils/properties.h>
//#include <private/android_filesystem_config.h>

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
	{"RTL8821CS",	"024c:c821"},
        {"RTL8822CU",   "0bda:c82c"},
	{"RTL8822CS",   "024c:c822"},
	{"SSV6051",	"3030:3030"},
	{"ESP8089",	"6666:1111"},
	{"AP6354",	"02d0:4354"},
	{"AP6330",	"02d0:4330"},
	{"AP6356S",	"02d0:4356"},
	{"AP6335",	"02d0:4335"},
	{"AP6255",      "02d0:a9bf"},
	{"RTL8822BE",	"10ec:b822"},
	{"MVL88W8977",	"02df:9145"},
	{"SPRDWL",	"0000:0000"},
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
		PLOG(ERROR) << "open dir failed:" << strerror(errno);
		return invalid_wifi_device_id;
	}

	while ((next = readdir(dir)) != NULL) {
		char line[256];
		char uevent_file[256] = {0};
		sprintf(uevent_file, "%s/%s/uevent", bus_dir, next->d_name);
		PLOG(DEBUG) << "uevent path:" << uevent_file;
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
			PLOG(DEBUG) << "line:" << line << ", prefix:" << prefix << ".";
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
				PLOG(DEBUG) << "pid:vid :" << temp;
				for (i = 0; i < idnum; i++) {
					if (0 == strncmp(temp, supported_wifi_devices[i].wifi_vid_pid, 9)) {
						PLOG(ERROR) << "found device pid:vid :" << temp;
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
	PLOG(DEBUG) << "wifi detectd return ret:" << ret;
	return ret;
}
int check_wifi_chip_type_string(char *type)
{
	if (identify_sucess == -1) {
		if (get_wifi_device_id(SDIO_DIR, PREFIX_SDIO) == 0)
			PLOG(DEBUG) << "SDIO WIFI identify sucess";
		else if (get_wifi_device_id(USB_DIR, PREFIX_USB) == 0)
			PLOG(DEBUG) << "USB WIFI identify sucess";
		else if (get_wifi_device_id(PCIE_DIR, PREFIX_PCIE) == 0)
			PLOG(DEBUG) << "PCIE WIFI identify sucess";
		else {
			PLOG(DEBUG) << "maybe there is no usb wifi or sdio or pcie wifi,set default wifi module Brocom APXXX";
			strcpy(recoginze_wifi_chip, "APXXX");
			identify_sucess = 1 ;
		}
	}

	strcpy(type, recoginze_wifi_chip);
	PLOG(ERROR) << "check_wifi_chip_type_string : " << type;
	return 0;
}
