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
#include <unistd.h>
#include <dirent.h>

#include <android-base/logging.h>
#include <cutils/misc.h>
#include <cutils/properties.h>
#include <sys/syscall.h>

extern "C" int init_module(void *, unsigned long, const char *);
extern "C" int delete_module(const char *, unsigned int);

#ifndef WIFI_DRIVER_MODULE_PATH
#define WIFI_DRIVER_MODULE_PATH         ""
#endif

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

#ifndef WIFI_DRIVER_MODULE_NAME
#define WIFI_DRIVER_MODULE_NAME          ""
#endif

#define MODULES_NAME_MAX 64

static const char DRIVER_PROP_NAME[] = "wlan.driver.status";
#ifdef WIFI_DRIVER_MODULE_PATH
//static const char DRIVER_MODULE_NAME[] = WIFI_DRIVER_MODULE_NAME;
//static const char DRIVER_MODULE_TAG[] = WIFI_DRIVER_MODULE_NAME " ";
//static const char DRIVER_MODULE_PATH[] = WIFI_DRIVER_MODULE_PATH;
//static const char DRIVER_MODULE_ARG[] = WIFI_DRIVER_MODULE_ARG;
static const char MODULE_FILE[] = "/proc/modules";
#endif

static const char USB_DIR[] = "/sys/bus/usb/devices";
static const char PCI_DIR[] = "/sys/bus/pci/devices";
static int device_id = WIFI_INVALID_DEVICE;

#ifdef BOARD_WLAN_DEVICE_64BIT
#define MODULE_PATH "/vendor/lib64/modules/"
#else
#define MODULE_PATH "/vendor/lib/modules/"
#endif

/* Product ID of supported WiFi devices */
static wifi_device_s devices[] = {
    {WIFI_REALTEK_RTL8822BE,     "10ec:b822"},
    {WIFI_REALTEK_RTL8822BU,     "0bda:b82c"},
    {WIFI_REALTEK_RTL8723DU,     "0bda:d723"},
	{WIFI_REALTEK_RTL8822CU,     "0bda:c82c"}, /*c82e*/
    {WIFI_ATBM_ATBM6022,         "007a:8888"},
    {WIFI_REALTEK_RTL8188FTV,    "0bda:f179"},
    {WIFI_MEDIATEK_MT7601U,      "148f:7601"},
    {WIFI_MEDIATEK_MT7668U,      "0e8d:7668"},
};

#define DRIVER_MODULE_RTL8822BE    1,  \
{ \
    {"88x2be",MODULE_PATH"88x2be.ko","ifname=wlan0 if2name=p2p0","88x2be "} \
}

#define DRIVER_MODULE_RTL8822BU    1,  \
{ \
    {"88x2bu",MODULE_PATH"88x2bu.ko","ifname=wlan0 if2name=p2p0","88x2bu "} \
}

#define DRIVER_MODULE_RTL8723DU    1,  \
{ \
     {"rtl8723du",MODULE_PATH"rtl8723du.ko","ifname=wlan0 if2name=p2p0","rtl8723du"} \
}

#define DRIVER_MODULE_RTL8822CU    1,  \
{ \
     {"rtl8822cu",MODULE_PATH"rtl8822cu.ko","ifname=wlan0 if2name=p2p0","rtl8822cu"} \
}


#define DRIVER_MODULE_ATBM6022    1,  \
{ \
     {"atbm6022",MODULE_PATH"atbm6022.ko","","atbm6022"} \
}

#define DRIVER_MODULE_RTL8188FTV    1,  \
{ \
    {"rtl8188ftv",MODULE_PATH"rtl8188ftv.ko","ifname=wlan0 if2name=p2p0","rtl8188ftv"} \
}

#define DRIVER_MODULE_MT7601U 2, \
{ \
    {"mtprealloc", MODULE_PATH"/mtprealloc.ko", "", "mtprealloc "}, \
    {"mt7601Usta", MODULE_PATH"/mt7601Usta.ko", "", "mt7601Usta "}, \
}

#define DRIVER_MODULE_MT7668U 2, \
{ \
    {"mt7668u_prealloc", MODULE_PATH"/mt7668u_prealloc.ko", "", "mt7668u_prealloc "}, \
    {"mt7668u", MODULE_PATH"/mt7668u.ko", "", "mt7668u "}, \
}

static wifi_modules_s mode_drivers[] = {
    {DRIVER_MODULE_RTL8822BE},        // RealTek RTL8822BE
    {DRIVER_MODULE_RTL8822BU},        // RealTek RTL8822BU
    {DRIVER_MODULE_RTL8723DU},        // RealTek RTL8723DU
    {DRIVER_MODULE_RTL8822CU},        // RealTek RTL8822CUU
    {DRIVER_MODULE_ATBM6022},         // ATBM6022
    {DRIVER_MODULE_RTL8188FTV},       // RealTek RTL8188FTV
    {DRIVER_MODULE_MT7601U},          // Mediatek MTK7601U
    {DRIVER_MODULE_MT7668U},          // Mediatek MTK7668U
};


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
  char driver_status[PROPERTY_VALUE_MAX];
#ifdef WIFI_DRIVER_MODULE_PATH
  FILE *proc;
  int found = 0;
  char line[MODULES_NAME_MAX];
#endif

  if (WIFI_INVALID_DEVICE == device_id) return 0;

  if (!property_get(DRIVER_PROP_NAME, driver_status, NULL) ||
      strcmp(driver_status, "ok") != 0) {
    return 0; /* driver not loaded */
  }
#ifdef WIFI_DRIVER_MODULE_PATH
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

  for (int i = 0; i < mode_drivers[device_id].module_num; i++) {
    while ((fgets(line, sizeof(line), proc)) != NULL) {
      if (!strncmp(line, mode_drivers[device_id].modules[i].module_tag, \
            strlen(mode_drivers[device_id].modules[i].module_tag))) {
        found++;
        break;
      }
    }
    rewind(proc);
  }

  fclose(proc);

  if (mode_drivers[device_id].module_num == found) {
    property_set(DRIVER_PROP_NAME, "ok");
    return 1;
  } else {
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
  }
#endif
  return 1;
}

int wifi_load_driver() {

  device_id = wifi_get_device_id();
  if (WIFI_INVALID_DEVICE == device_id) {
    PLOG(ERROR) << "Cannot find supported device";
    return -1;
  }

  if (is_wifi_driver_loaded()) return 0;

  for (int i = 0; i < mode_drivers[device_id].module_num; i++) {
    if (insmod(mode_drivers[device_id].modules[i].module_path, \
         mode_drivers[device_id].modules[i].module_arg) < 0)
      return -1;
  }

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    return 0;
  }

  if (wifi_change_driver_state(WIFI_DRIVER_STATE_ON) < 0) return -1;
#endif
  property_set(DRIVER_PROP_NAME, "ok");
  return 0;
}

int wifi_unload_driver() {

  if (!is_wifi_driver_loaded()) return 0;

  usleep(200000); /* allow to finish interface down */
#ifdef WIFI_DRIVER_MODULE_PATH
  for (int i = mode_drivers[device_id].module_num-1; i >= 0; i--) {
    if (rmmod(mode_drivers[device_id].modules[i].module_name) == 0)
      continue;
    else
      return -1;
  }

  int count = 20; /* wait at most 10 seconds for completion */
  while (count-- > 0) {
    if (!is_wifi_driver_loaded()) break;
    usleep(500000);
  }
  usleep(500000); /* allow card removal */
  if (count) {
    device_id = WIFI_INVALID_DEVICE;
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
  }
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

static int wifi_get_pci_device_id(void) {

  DIR *dir;
  int ret = WIFI_INVALID_DEVICE;
  struct dirent *next;
  FILE *fp = NULL;
  int idnum = sizeof(devices) / sizeof(devices[0]);

  dir = opendir(PCI_DIR);
  if (!dir)  return WIFI_INVALID_DEVICE;

  while ((next = readdir(dir)) != NULL) {
    char line[256];
    char uevent_file[256] = {0};

    /* read uevent file, uevent's data like below:
     * MAJOR=189
     * MINOR=4
     * DEVNAME=bus/usb/001/005
     * DEVTYPE=usb_device
     * DRIVER=usb
     * DEVICE=/proc/bus/usb/001/005
     * PRODUCT=bda/8176/200
     * TYPE=0/0/0
     * BUSNUM=001
     * DEVNUM=005
     */
    sprintf(uevent_file, "%s/%s/uevent", PCI_DIR, next->d_name);

    fp = fopen(uevent_file, "r");
    if (NULL == fp)  continue;

    while (fgets(line, sizeof(line), fp)) {
      char *pos = NULL;
      int product_vid, product_did;
      char temp[10] = {0};
      pos = strstr(line, "PCI_ID=");
      if(pos != NULL) {
        if (sscanf(pos + 7, "%x:%x", &product_vid, &product_did)  <= 0)
          continue;
        sprintf(temp, "%04x:%04x", product_vid, product_did);

        for (int i = 0; i < idnum; i++) {
          if (0 == strncmp(temp, devices[i].product_id, 9)) {
            ret = devices[i].id;
            break;
          }
        }
      }
      if (ret != WIFI_INVALID_DEVICE) break;
    }
    fclose(fp);
    if (ret != WIFI_INVALID_DEVICE)  break;
  }

  closedir(dir);
  return ret;
}

static int wifi_get_usb_device_id(void) {

  DIR *dir;
  int ret = WIFI_INVALID_DEVICE;
  struct dirent *next;
  FILE *fp = NULL;
  int idnum = sizeof(devices) / sizeof(devices[0]);

  dir = opendir(USB_DIR);
  if (!dir) return WIFI_INVALID_DEVICE;

  while ((next = readdir(dir)) != NULL) {
    char line[256];
    char uevent_file[256] = {0};
    sprintf(uevent_file, "%s/%s/uevent", USB_DIR, next->d_name);

    fp = fopen(uevent_file, "r");
    if (NULL == fp) continue;

    while (fgets(line, sizeof(line), fp)){
      char *pos = NULL;
      int product_vid, product_did, product_bcdev;
      char temp[10] = {0};
      pos = strstr(line, "PRODUCT=");

      if (NULL != pos) {
        if (sscanf(pos + 8, "%x/%x/%x", &product_vid, &product_did, &product_bcdev) <= 0)
          continue;
        sprintf(temp, "%04x:%04x", product_vid, product_did);
        for (int i = 0; i < idnum; i++) {
          if (0 == strncmp(temp, devices[i].product_id, 9)) {
            ret = devices[i].id;
            break;
          }
        }
      }
      if (ret != WIFI_INVALID_DEVICE) break;
    }
    fclose(fp);
    if (ret != WIFI_INVALID_DEVICE) break;
  }
  closedir(dir);

  return ret;
}

int wifi_get_device_id(void) {
  int ret = WIFI_INVALID_DEVICE;

  ret = wifi_get_pci_device_id();
  if (WIFI_INVALID_DEVICE != ret) return ret;

  ret = wifi_get_usb_device_id();
  if (WIFI_INVALID_DEVICE != ret) return ret;

  return ret;
}
