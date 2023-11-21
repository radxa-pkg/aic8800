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

#include <android-base/logging.h>
#include <cutils/misc.h>
#include <cutils/properties.h>
#include <sys/syscall.h>

extern "C" int init_module(void *, unsigned long, const char *);
extern "C" int delete_module(const char *, unsigned int);
#define finit_module(fd, opts, flags) syscall(SYS_finit_module, fd, opts, flags)
#ifdef MULTI_WIFI_SUPPORT
#include <dlfcn.h>
typedef int (*WIFI_LOAD_DRIVER) ();
typedef int (*WIFI_UNLOAD_DRIVER) ();
typedef int (*WIFI_CHANGE_FW_PATH) (const char *fwpath);
typedef const char * (*WIFI_GET_FW_PATH) (int fw_type);
typedef const char * (*WIFI_GET_VENDOR_NAME) ();

void* pHandle = NULL;
void* init_multi_wifi_handle() {
    if (NULL == pHandle) {
        pHandle = dlopen("libwifi-hal-common-ext.so", RTLD_NOW);
        if (NULL == pHandle) {
            PLOG(ERROR) << "Unable to get multi wifi so";
            return NULL;
        }
    }
    return pHandle;
}

void release_multi_wifi_handle() {
    if (NULL != pHandle) {
        dlclose(pHandle);
        pHandle = NULL;
    }
}

int wifi_load_driver_ext() {
    void* handle = init_multi_wifi_handle();
    if (NULL != handle) {
        WIFI_LOAD_DRIVER pfunc = (WIFI_LOAD_DRIVER)dlsym(handle, "_Z20wifi_load_driver_extv");
        if (NULL == pfunc) {
            LOG(ERROR) << "Unable to get multi wifi wifi_load_driver_ext function";
            return -1;
        }
        return pfunc();
    }

    return -1;
}

int wifi_unload_driver_ext() {
    void* handle = init_multi_wifi_handle();
    if (NULL != handle) {
        WIFI_UNLOAD_DRIVER pfunc = (WIFI_UNLOAD_DRIVER)dlsym(handle, "_Z22wifi_unload_driver_extv");
        if (NULL == pfunc) {
            LOG(ERROR) << "Unable to get multi wifi wifi_unload_driver_ext function";
            return -1;
        }

        int ret = pfunc();
        release_multi_wifi_handle();
        return ret;
    }

    return -1;
}

const char *wifi_get_fw_path_ext(int fw_type) {
    void* handle = init_multi_wifi_handle();
    if (NULL != handle) {
        WIFI_GET_FW_PATH pfunc = (WIFI_GET_FW_PATH)dlsym(handle, "_Z20wifi_get_fw_path_exti");
        if (NULL == pfunc) {
            LOG(ERROR) << "Unable to get multi wifi wifi_get_fw_path_ext function";
            return NULL;
        }
        return pfunc(fw_type);
    }

    return NULL;
}

int wifi_change_fw_path_ext(const char *fwpath) {
    void* handle = init_multi_wifi_handle();
    if (NULL != handle) {
        WIFI_CHANGE_FW_PATH pfunc = (WIFI_CHANGE_FW_PATH)dlsym(handle, "_Z23wifi_change_fw_path_extPKc");
        if (NULL == pfunc) {
            LOG(ERROR) << "Unable to get multi wifi wifi_get_fw_path_ext function";
            return -1;
        }
        return pfunc(fwpath);
    }

    return -1;
}

const char *get_wifi_vendor_name() {
    void* handle = init_multi_wifi_handle();
    if (NULL != handle) {
        WIFI_GET_VENDOR_NAME pfunc = (WIFI_GET_VENDOR_NAME)dlsym(handle, "_Z20get_wifi_vendor_namev");
        if (NULL == pfunc) {
            LOG(ERROR) << "Unable to get multi wifi get_wifi_vendor_name function";
            return NULL;
        }
        return pfunc();
    }

    return NULL;
}
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

static const char DRIVER_PROP_NAME[] = "wlan.driver.status";
static bool is_driver_loaded = false;
#ifdef WIFI_DRIVER_MODULE_PATH
static const char DRIVER_MODULE_NAME[] = WIFI_DRIVER_MODULE_NAME;
static const char DRIVER_MODULE_TAG[] = WIFI_DRIVER_MODULE_NAME " ";
static const char DRIVER_MODULE_PATH[] = WIFI_DRIVER_MODULE_PATH;
static const char DRIVER_MODULE_ARG[] = WIFI_DRIVER_MODULE_ARG;
static const char MODULE_FILE[] = "/proc/modules";
#endif

static int insmod(const char *filename, const char *args) {
  void *module;
  unsigned int size;
  int ret;

  module = load_file(filename, &size);
  if (!module) return -1;

  //ret = init_module(module, size, args);
  ret = finit_module(module, args, 0);

  free(module);

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
  char line[sizeof(DRIVER_MODULE_TAG) + 10];
#endif

  if (!property_get(DRIVER_PROP_NAME, driver_status, NULL)) {
    return 0; /* driver not loaded */
  }

  if (!is_driver_loaded) {
    return 0;
  } /* driver not loaded */

#ifdef WIFI_DRIVER_MODULE_PATH
  /*
   * If the property says the driver is loaded, check to
   * make sure that the property setting isn't just left
   * over from a previous manual shutdown or a runtime
   * crash.
   */
  if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
    PLOG(WARNING) << "Could not open " << MODULE_FILE;
    is_driver_loaded = false;
    if (strcmp(driver_status, "unloaded") != 0) {
      property_set(DRIVER_PROP_NAME, "unloaded");
    }
    return 0;
  }
  while ((fgets(line, sizeof(line), proc)) != NULL) {
    if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
      fclose(proc);
      return 1;
    }
  }
  fclose(proc);
  is_driver_loaded = false;
  if (strcmp(driver_status, "unloaded") != 0) {
    property_set(DRIVER_PROP_NAME, "unloaded");
  }
  return 0;
#else
  return 1;
#endif
}

int wifi_load_driver() {
#ifdef WIFI_DRIVER_MODULE_PATH
#ifdef MULTI_WIFI_SUPPORT
  if (wifi_load_driver_ext() != 0) {
    return -1;
  } else {
   return 0;
  }
#endif
  if (is_wifi_driver_loaded()) {
    return 0;
  }

  if (insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG) < 0) return -1;
#endif

#ifdef WIFI_DRIVER_STATE_CTRL_PARAM
  if (is_wifi_driver_loaded()) {
    return 0;
  }

  if (wifi_change_driver_state(WIFI_DRIVER_STATE_ON) < 0) {
#ifdef WIFI_DRIVER_MODULE_PATH
    PLOG(WARNING) << "Driver unloading, err='fail to change driver state'";
    if (rmmod(DRIVER_MODULE_NAME) == 0) {
      PLOG(DEBUG) << "Driver unloaded";
    } else {
      // Set driver prop to "ok", expect HL to restart Wi-Fi.
      PLOG(DEBUG) << "Driver unload failed! set driver prop to 'ok'.";
      property_set(DRIVER_PROP_NAME, "ok");
    }
#endif
    return -1;
  }
#endif
  is_driver_loaded = true;
  return 0;
}

int wifi_unload_driver() {
#ifdef MULTI_WIFI_SUPPORT
    wifi_unload_driver_ext();
    property_set(DRIVER_PROP_NAME, "unloaded");
    return 0;
#endif
  if (!is_wifi_driver_loaded()) {
    return 0;
  }
#ifdef WIFI_DRIVER_MODULE_PATH
  if (rmmod(DRIVER_MODULE_NAME) == 0) {
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
  is_driver_loaded = false;
  property_set(DRIVER_PROP_NAME, "unloaded");
  return 0;
#endif
}

const char *wifi_get_fw_path(int fw_type) {
#ifdef MULTI_WIFI_SUPPORT
    return wifi_get_fw_path_ext(fw_type);
#endif
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
#ifdef MULTI_WIFI_SUPPORT
    return wifi_change_fw_path_ext(fwpath);
#endif

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
