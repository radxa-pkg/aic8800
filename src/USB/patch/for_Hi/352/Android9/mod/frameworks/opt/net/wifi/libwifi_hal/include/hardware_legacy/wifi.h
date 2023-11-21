/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef HARDWARE_LEGACY_WIFI_H
#define HARDWARE_LEGACY_WIFI_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#define DRIVER_MODULE_LEN_MAX 256

/* ID of supported WiFi devices */
typedef enum {
    WIFI_REALTEK_RTL8822BE,
    WIFI_REALTEK_RTL8822BU,
    WIFI_REALTEK_RTL8723DU,
	WIFI_REALTEK_RTL8822CU,
    WIFI_ATBM_ATBM6022,
    WIFI_REALTEK_RTL8188FTV,
    WIFI_MEDIATEK_MT7601U,
    WIFI_MEDIATEK_MT7668U,
    WIFI_AIC_AIC8800D,
    WIFI_INVALID_DEVICE = ~0x0,
} wifi_id_e;

/* Product ID struct of WiFi device */
typedef struct {
    wifi_id_e id;        // ID of WiFi device
    char product_id[10];    // Project ID
} wifi_device_s;

/* Driver module struct*/
typedef struct {
    char module_name[DRIVER_MODULE_LEN_MAX];     //modules'name displayed when 'lsmod'
    char module_path[DRIVER_MODULE_LEN_MAX];    // path of module file
    char module_arg[DRIVER_MODULE_LEN_MAX];        // parameters when load module
    char module_tag[DRIVER_MODULE_LEN_MAX];        // modules'tag used when rmmod
} driver_module_s;

/* Driver modules struct of WiFi device */
typedef struct {
    int module_num;            //module number of the driver
    driver_module_s modules[4];    // modules of the driver
} wifi_modules_s;


/**
 * get the WiFi device ID.
 *
 * @return device ID on success, -1 on failure.
 */
int wifi_get_device_id();

/**
 * Load the Wi-Fi driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_load_driver();

/**
 * Unload the Wi-Fi driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_unload_driver();

/**
 * Check if the Wi-Fi driver is loaded.
 * Check if the Wi-Fi driver is loaded.

 * @return 0 on success, < 0 on failure.
 */
int is_wifi_driver_loaded();

/**
 * Return the path to requested firmware
 */
#define WIFI_GET_FW_PATH_STA  0
#define WIFI_GET_FW_PATH_AP 1
#define WIFI_GET_FW_PATH_P2P  2
const char *wifi_get_fw_path(int fw_type);

/**
 * Change the path to firmware for the wlan driver
 */
int wifi_change_fw_path(const char *fwpath);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* HARDWARE_LEGACY_WIFI_H */
