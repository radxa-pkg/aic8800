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

#include "hardware_legacy/wifi.h"

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
#include <private/android_filesystem_config.h>

extern "C" int init_module(void *, unsigned long, const char *);
extern "C" int delete_module(const char *, unsigned int);
#define WIFI_MODULE_PATH		"/vendor/lib/modules/"
#define RTL8188EU_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8188eu.ko"
#define RTL8723BU_DRIVER_MODULE_PATH 	 WIFI_MODULE_PATH"8723bu.ko"
#define RTL8723BS_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8723bs.ko"
#define RTL8723BS_VQ0_DRIVER_MODULE_PATH WIFI_MODULE_PATH"8723bs-vq0.ko"
#define RTL8723CS_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8723cs.ko"
#define RTL8723DS_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8723ds.ko"
#define RTL8188FU_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8188fu.ko"
#define RTL8822BU_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8822bu.ko"
#define RTL8822BS_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8822bs.ko"
#define RTL8189ES_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8189es.ko"
#define RTL8189FS_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8189fs.ko"
#define RTL8192DU_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8192du.ko"
#define RTL8812AU_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8812au.ko"
#define RTL8822BE_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"8822be.ko"
#define RTL8821CS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8821cs.ko"
#define RTL8822CU_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8822cu.ko"
#define RTL8822CS_DRIVER_MODULE_PATH     WIFI_MODULE_PATH"8822cs.ko"
#define SSV6051_DRIVER_MODULE_PATH  	 WIFI_MODULE_PATH"ssv6051.ko"
#define ESP8089_DRIVER_MODULE_PATH  	 WIFI_MODULE_PATH"esp8089.ko"
#define BCM_DRIVER_MODULE_PATH      	 WIFI_MODULE_PATH"bcmdhd.ko"
#define MLAN_DRIVER_MODULE_PATH      	 WIFI_MODULE_PATH"mlan.ko"
#define MVL_DRIVER_MODULE_PATH      	 WIFI_MODULE_PATH"sd8xxx.ko"
#define RK912_DRIVER_MODULE_PATH         WIFI_MODULE_PATH"rk912.ko"
#define SPRDWL_DRIVER_MODULE_PATH	 WIFI_MODULE_PATH"sprdwl_ng.ko"
#define DRIVER_MODULE_PATH_UNKNOW   	 ""

#define RTL8188EU_DRIVER_MODULE_NAME	 "8188eu"
#define RTL8723BU_DRIVER_MODULE_NAME	 "8723bu"
#define RTL8723BS_DRIVER_MODULE_NAME	 "8723bs"
#define RTL8723BS_VQ0_DRIVER_MODULE_NAME "8723bs-vq0"
#define RTL8723CS_DRIVER_MODULE_NAME	 "8723cs"
#define RTL8723DS_DRIVER_MODULE_NAME	 "8723ds"
#define RTL8188FU_DRIVER_MODULE_NAME	 "8188fu"
#define RTL8822BU_DRIVER_MODULE_NAME	 "8822bu"
#define RTL8822BS_DRIVER_MODULE_NAME	 "8822bs"
#define RTL8189ES_DRIVER_MODULE_NAME	 "8189es"
#define RTL8189FS_DRIVER_MODULE_NAME	 "8189fs"
#define RTL8192DU_DRIVER_MODULE_NAME	 "8192du"
#define RTL8812AU_DRIVER_MODULE_NAME	 "8812au"
#define RTL8822BE_DRIVER_MODULE_NAME	 "8822be"
#define RTL8821CS_DRIVER_MODULE_NAME	 "8821cs"
#define RTL8822CU_DRIVER_MODULE_NAME     "8822cu"
#define RTL8822CS_DRIVER_MODULE_NAME     "8822cs"
#define SSV6051_DRIVER_MODULE_NAME  	 "ssv6051"
#define ESP8089_DRIVER_MODULE_NAME  	 "esp8089"
#define BCM_DRIVER_MODULE_NAME      	 "bcmdhd"
#define MVL_DRIVER_MODULE_NAME      	 "sd8xxx"
#define RK912_DRIVER_MODULE_NAME         "rk912"
#define SPRDWL_DRIVER_MODULE_NAME	 "sprdwl"
#define DRIVER_MODULE_NAME_UNKNOW   	 ""

#ifndef WIFI_DRIVER_FW_PATH_STA
#define WIFI_DRIVER_FW_PATH_STA NULL
#endif
#ifndef WIFI_DRIVER_FW_PATH_AP
#define WIFI_DRIVER_FW_PATH_AP NULL
#endif
#ifndef WIFI_DRIVER_FW_PATH_P2P
#define WIFI_DRIVER_FW_PATH_P2P NULL
#endif

#ifndef WIFI_DRIVER_MODULE_ARG
#define WIFI_DRIVER_MODULE_ARG ""
#endif

static const char DRIVER_PROP_NAME[] = "wlan.driver.status";


#ifndef WIFI_DRIVER_MODULE_PATH
#define WIFI_DRIVER_MODULE_PATH		"/vendor/lib/modules/none"
#endif
#ifndef WIFI_DRIVER_MODULE_NAME
#define WIFI_DRIVER_MODULE_NAME    "wlan"
#endif
#ifdef WIFI_DRIVER_MODULE_PATH
//static const char DRIVER_MODULE_NAME[] = WIFI_DRIVER_MODULE_NAME;
static const char DRIVER_MODULE_TAG[] = WIFI_DRIVER_MODULE_NAME " ";
//static const char DRIVER_MODULE_PATH[] = WIFI_DRIVER_MODULE_PATH;
//static const char DRIVER_MODULE_ARG[] = WIFI_DRIVER_MODULE_ARG;
static const char MODULE_FILE[] = "/proc/modules";
#endif

enum {
    KERNEL_VERSION_3_0_8 = 1,
    KERNEL_VERSION_3_0_36,
    KERNEL_VERSION_3_10,
    KERNEL_VERSION_4_4,
    KERNEL_VERSION_4_19,
};

typedef struct _wifi_ko_file_name
{
	char wifi_name[64];
	char wifi_driver_name[64];
	char wifi_module_path[128];
	char wifi_module_arg[128];

}wifi_ko_file_name;

#define UNKKOWN_DRIVER_MODULE_ARG ""
#define SSV6051_DRIVER_MODULE_ARG "stacfgpath=/vendor/etc/firmware/ssv6051-wifi.cfg"
#define MVL88W8977_DRIVER_MODULE_ARG "drv_mode=1 fw_name=mrvl/sd8977_wlan_v2.bin cal_data_cfg=none cfg80211_wext=0xf"

wifi_ko_file_name module_list[] =
{
	{"RTL8723BU", RTL8723BU_DRIVER_MODULE_NAME, RTL8723BU_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8188EU", RTL8188EU_DRIVER_MODULE_NAME, RTL8188EU_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8192DU", RTL8192DU_DRIVER_MODULE_NAME, RTL8192DU_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8822BU", RTL8822BU_DRIVER_MODULE_NAME, RTL8822BU_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8822BS", RTL8822BS_DRIVER_MODULE_NAME, RTL8822BS_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8188FU", RTL8188FU_DRIVER_MODULE_NAME, RTL8188FU_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8189ES", RTL8189ES_DRIVER_MODULE_NAME, RTL8189ES_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8723BS", RTL8723BS_DRIVER_MODULE_NAME, RTL8723BS_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8723CS", RTL8723CS_DRIVER_MODULE_NAME, RTL8723CS_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8723DS", RTL8723DS_DRIVER_MODULE_NAME, RTL8723DS_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8812AU", RTL8812AU_DRIVER_MODULE_NAME, RTL8812AU_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8189FS", RTL8189FS_DRIVER_MODULE_NAME, RTL8189FS_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8822BE", RTL8822BE_DRIVER_MODULE_NAME, RTL8822BE_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8821CS", RTL8821CS_DRIVER_MODULE_NAME, RTL8821CS_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"RTL8822CU", RTL8822CU_DRIVER_MODULE_NAME, RTL8822CU_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
        {"RTL8822CS", RTL8822CS_DRIVER_MODULE_NAME, RTL8822CS_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"SSV6051",     SSV6051_DRIVER_MODULE_NAME,   SSV6051_DRIVER_MODULE_PATH, SSV6051_DRIVER_MODULE_ARG},
	{"ESP8089",     ESP8089_DRIVER_MODULE_NAME,   ESP8089_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"AP6335",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"AP6330",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"AP6354",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"AP6356S",         BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"AP6255",          BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"APXXX",           BCM_DRIVER_MODULE_NAME,       BCM_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"MVL88W8977",      MVL_DRIVER_MODULE_NAME,       MVL_DRIVER_MODULE_PATH, MVL88W8977_DRIVER_MODULE_ARG},
        {"RK912",         RK912_DRIVER_MODULE_NAME,     RK912_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"SPRDWL",	    SPRDWL_DRIVER_MODULE_NAME, SPRDWL_DRIVER_MODULE_PATH, UNKKOWN_DRIVER_MODULE_ARG},
	{"UNKNOW",       DRIVER_MODULE_NAME_UNKNOW,    DRIVER_MODULE_PATH_UNKNOW, UNKKOWN_DRIVER_MODULE_ARG}

};

static char wifi_type[64] = {0};
extern "C" int check_wifi_chip_type_string(char *type);

int get_kernel_version(void)
{
    int fd, version = 0;
    char buf[64];

    fd = open("/proc/version", O_RDONLY);
    if (fd < 0) {
        PLOG(ERROR) << "Can't open '/proc/version', errno = " << errno;
        goto fderror;
    }
    memset(buf, 0, 64);
    if( 0 == read(fd, buf, 64) ){
        PLOG(ERROR) << "read '/proc/version' failed";
        close(fd);
        goto fderror;
    }
    close(fd);
    if (strstr(buf, "Linux version 3.10") != NULL) {
        version = KERNEL_VERSION_3_10;
        PLOG(ERROR) << "Kernel version is 3.10.";
    } else if (strstr(buf, "Linux version 4.4") != NULL) {
	version = KERNEL_VERSION_4_4;
	PLOG(ERROR) << "Kernel version is 4.4.";
    } else if (strstr(buf, "Linux version 4.19") != NULL) {
	version = KERNEL_VERSION_4_19;
	PLOG(ERROR) << "Kernel version is 4.19.";
    }else {
        version = KERNEL_VERSION_3_0_36;
        PLOG(ERROR) << "Kernel version is 3.0.36.";
    }

    return version;

fderror:
    return -1;
}

/* 0 - not ready; 1 - ready. */
int check_wireless_ready(void)
{
	char line[1024];
	FILE *fp = NULL;

	if (get_kernel_version() == KERNEL_VERSION_4_4 ||
			get_kernel_version() == KERNEL_VERSION_4_19) {
		fp = fopen("/proc/net/dev", "r");
		if (fp == NULL) {
			PLOG(ERROR) << "Couldn't open /proc/net/dev";
			return 0;
		}
	} else {
		fp = fopen("/proc/net/wireless", "r");
		if (fp == NULL) {
			PLOG(ERROR) << "Couldn't open /proc/net/wireless";
			return 0;
		}
	}

	while(fgets(line, 1024, fp)) {
		if ((strstr(line, "wlan0:") != NULL) || (strstr(line, "p2p0:") != NULL)) {
			PLOG(ERROR) << "Wifi driver is ready for now...";
			property_set(DRIVER_PROP_NAME, "ok");
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);

	PLOG(ERROR) << "Wifi driver is not ready.";
	return 0;
}

static int insmod(const char *filename, const char *args) {
  int ret;
  int fd;

  fd = TEMP_FAILURE_RETRY(open(filename, O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open " << filename;
    return -1;
  }

  ret = syscall(__NR_finit_module, fd, args, 0);

  close(fd);
  if (ret < 0) {
    PLOG(ERROR) << "finit_module return: " << ret;
  }

  return ret;
}

static int rmmod(const char *modname) {
  int ret = -1;
  int maxtry = 10;

  while (maxtry-- > 0) {
    ret = delete_module(modname, O_NONBLOCK | O_EXCL);
    if (ret < 0 && errno == EAGAIN)
      usleep(500000);
    else
      break;
  }

  if (ret != 0)
    PLOG(DEBUG) << "Unable to unload driver module '" << modname << "'";
  return ret;
}

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
int wifi_change_driver_state(const char *state) {
  int len;
  int fd;
  int ret = 0;

  if (!state) return -1;
  fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_STATE_CTRL_PARAM, O_WRONLY));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open driver state control param";
    return -1;
  }
  len = strlen(state) + 1;
  if (TEMP_FAILURE_RETRY(write(fd, state, len)) != len) {
    PLOG(ERROR) << "Failed to write driver state control param";
    ret = -1;
  }
  close(fd);
  return ret;
}
#endif

int is_wifi_driver_loaded() {
#ifdef WIFI_DRIVER_MODULE_PATH
  FILE *proc;
  char line[sizeof(DRIVER_MODULE_TAG) + 10];
#endif
#if 0
  if (!property_get(DRIVER_PROP_NAME, driver_status, NULL) ||
      strcmp(driver_status, "ok") != 0) {
    return 0; /* driver not loaded */
  }
#endif
if (check_wireless_ready() == 0) {
  /*
   * If the property says the driver is loaded, check to
   * make sure that the property setting isn't just left
   * over from a previous manual shutdown or a runtime
   * crash.
   */
  if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
    PLOG(WARNING) << "Could not open " << MODULE_FILE;
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
  }
  while ((fgets(line, sizeof(line), proc)) != NULL) {
    if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
      fclose(proc);
      return 1;
    }
  }
  fclose(proc);
  property_set(DRIVER_PROP_NAME, "unloaded");
  return 0;
} else {
  return 1;
}
}

int wifi_load_driver() {
#ifdef WIFI_DRIVER_MODULE_PATH
	char* wifi_ko_path = NULL ;
	char* wifi_ko_arg =NULL;
	int i = 0;
	int count = 100;
	if (is_wifi_driver_loaded()) {
		return 0;
	}
	if (wifi_type[0] == 0) {
		check_wifi_chip_type_string(wifi_type);
	}
	for (i=0; i< (int)(sizeof(module_list) / sizeof(module_list[0])); i++) {
		if (!strcmp(wifi_type , module_list[i].wifi_name)) {
			wifi_ko_path = module_list[i].wifi_module_path;
			wifi_ko_arg = module_list[i].wifi_module_arg;
			PLOG(ERROR) << "matched ko file path " << wifi_ko_path;
			break;
		}
	}
	if (wifi_ko_path == NULL) {
		PLOG(ERROR) << "falied to find wifi driver for type=" << wifi_type;
		return -1;
	}

	if (strstr(wifi_ko_path, MVL_DRIVER_MODULE_NAME)) {
		insmod(MLAN_DRIVER_MODULE_PATH, "");
	}

  if (insmod(wifi_ko_path, wifi_ko_arg) < 0) {
	  return -1;
  }
#endif

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    return 0;
  }

  if (wifi_change_driver_state(WIFI_DRIVER_STATE_ON) < 0) return -1;
#endif
  while (count-- > 0) {
	  if (is_wifi_driver_loaded()) {
		property_set(DRIVER_PROP_NAME, "ok");
		return 0;
	  }
	  usleep(200000);
  }
  property_set(DRIVER_PROP_NAME, "timeout");
  return -1;
}

int wifi_unload_driver() {
#if 0
  if (!is_wifi_driver_loaded()) {
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
  }

  usleep(200000); /* allow to finish interface down */
#ifdef WIFI_DRIVER_MODULE_PATH
  char* wifi_ko_name = NULL ;
  int i = 0;
  if (wifi_type[0] == 0) {
	check_wifi_chip_type_string(wifi_type);
  }
  for (i=0; i< (int)(sizeof(module_list) / sizeof(module_list[0])); i++) {
	if (!strcmp(wifi_type , module_list[i].wifi_name)) {
		wifi_ko_name = module_list[i].wifi_driver_name;
		PLOG(ERROR) << "matched ko file name " << wifi_ko_name;
		break;
	}
  }
  if (rmmod(wifi_ko_name) == 0) {
    int count = 20; /* wait at most 10 seconds for completion */
    while (count-- > 0) {
      if (!is_wifi_driver_loaded()) break;
      usleep(500000);
    }
    usleep(500000); /* allow card removal */
    if (count) {
      return 0;
    }
    return -1;
  } else
    return -1;
#else
#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    if (wifi_change_driver_state(WIFI_DRIVER_STATE_OFF) < 0) return -1;
  }
#endif
  property_set(DRIVER_PROP_NAME, "unloaded");
  return 0;
#endif
#endif
  property_set(DRIVER_PROP_NAME, "unloaded");
  return 0;
}

const char *wifi_get_fw_path(int fw_type) {
  switch (fw_type) {
    case WIFI_GET_FW_PATH_STA:
      return WIFI_DRIVER_FW_PATH_STA;
    case WIFI_GET_FW_PATH_AP:
      return WIFI_DRIVER_FW_PATH_AP;
    case WIFI_GET_FW_PATH_P2P:
      return WIFI_DRIVER_FW_PATH_P2P;
  }
  return NULL;
}

int wifi_change_fw_path(const char *fwpath) {
  int len;
  int fd;
  int ret = 0;

  if (wifi_type[0] == 0)
	check_wifi_chip_type_string(wifi_type);
  if (0 != strncmp(wifi_type, "AP", 2)) return ret;
  if (!fwpath) return ret;
  fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_FW_PATH_PARAM, O_WRONLY));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open wlan fw path param";
    return -1;
  }
  len = strlen(fwpath) + 1;
  if (TEMP_FAILURE_RETRY(write(fd, fwpath, len)) != len) {
    PLOG(ERROR) << "Failed to write wlan fw path param";
    ret = -1;
  }
  close(fd);
  return ret;
}
