/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _WIFI_H
#define _WIFI_H

#if __cplusplus
extern "C" {
#endif

enum WIFI_CHIP_TYPE_LIST{
    RTL8188CU = 0,
    RTL8192CU,
    RTL8188EU,
    BCM4330,
    RK901,
    RK903,
    AP6335,
    AP6234,
    AP6441,
    MT7601,
    RTL8723AS,
    RTL8723AU,
    RTL8723BS,
    RTL8723BU,
    RTL8192DU,
    MT6620,
    ESP8089,
    NUM_MAX,
};

enum {
    KERNEL_VERSION_3_0_8 = 1,
    KERNEL_VERSION_3_0_36,
    KERNEL_VERSION_3_10,
};

int check_wifi_chip_type(void);

int rk_wifi_power_ctrl(int on);

int rk_wifi_load_driver(int enable);

int check_wireless_ready(void);

int get_kernel_version(void);

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
 *
 * @return 0 on success, < 0 on failure.
 */
int is_wifi_driver_loaded();


/**
 * Start supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_start_supplicant(int p2pSupported);

/**
 * Stop supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_stop_supplicant(int p2pSupported);

/**
 * Open a connection to supplicant
 *
 * @return 0 on success, < 0 on failure.
 */


 
#ifdef CONFIG_P2P_AUTO_GO_AS_SOFTAP
/**
 * Start supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_ap_start_supplicant();
/**
 * Stop supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wifi_ap_stop_supplicant();
#endif
 


int wifi_connect_to_supplicant();

/**
 * Close connection to supplicant
 *
 * @return 0 on success, < 0 on failure.
 */
void wifi_close_supplicant_connection();

/**
 * wifi_wait_for_event() performs a blocking call to 
 * get a Wi-Fi event and returns a string representing 
 * a Wi-Fi event when it occurs.
 *
 * @param buf is the buffer that receives the event
 * @param len is the maximum length of the buffer
 *
 * @returns number of bytes in buffer, 0 if no
 * event (for instance, no connection), and less than 0
 * if there is an error.
 */
int wifi_wait_for_event(char *buf, size_t len);

/**
 * wifi_command() issues a command to the Wi-Fi driver.
 *
 * Android extends the standard commands listed at
 * /link http://hostap.epitest.fi/wpa_supplicant/devel/ctrl_iface_page.html
 * to include support for sending commands to the driver:
 *
 * See wifi/java/android/net/wifi/WifiNative.java for the details of
 * driver commands that are supported
 *
 * @param command is the string command (preallocated with 32 bytes)
 * @param commandlen is command buffer length
 * @param reply is a buffer to receive a reply string
 * @param reply_len on entry, this is the maximum length of
 *        the reply buffer. On exit, the number of
 *        bytes in the reply buffer.
 *
 * @return 0 if successful, < 0 if an error.
 */
int wifi_command(const char *command, char *reply, size_t *reply_len);

/**
 * do_dhcp_request() issues a dhcp request and returns the acquired
 * information. 
 * 
 * All IPV4 addresses/mask are in network byte order.
 *
 * @param ipaddr return the assigned IPV4 address
 * @param gateway return the gateway being used
 * @param mask return the IPV4 mask
 * @param dns1 return the IPV4 address of a DNS server
 * @param dns2 return the IPV4 address of a DNS server
 * @param server return the IPV4 address of DHCP server
 * @param lease return the length of lease in seconds.
 *
 * @return 0 if successful, < 0 if error.
 */
int do_dhcp_request(int *ipaddr, int *gateway, int *mask,
                   int *dns1, int *dns2, int *server, int *lease);

/**
 * Return the error string of the last do_dhcp_request().
 */
const char *get_dhcp_error_string();

/**
 * Return the path to requested firmware
 */
#define WIFI_GET_FW_PATH_STA	0
#define WIFI_GET_FW_PATH_AP	1
#define WIFI_GET_FW_PATH_P2P	2
const char *wifi_get_fw_path(int fw_type);

/**
 * Change the path to firmware for the wlan driver
 */
int wifi_change_fw_path(const char *fwpath);

/**
 * Check and create if necessary initial entropy file
 */
#define WIFI_ENTROPY_FILE	"/data/misc/wifi/entropy.bin"
int ensure_entropy_file_exists();

#if __cplusplus
};  // extern "C"
#endif

#endif  // _WIFI_H
