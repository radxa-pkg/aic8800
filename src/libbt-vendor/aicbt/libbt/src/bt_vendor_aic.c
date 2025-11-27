/******************************************************************************
 *
 *  Copyright (C) 2019-2021 Aicsemi Corporation
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
 *  Filename:      bt_vendor_aic.c
 *
 *  Description:   Aicsemi vendor specific library implementation
 *
 ******************************************************************************/

#undef NDEBUG
#define LOG_TAG "libbt_vendor"
#define AICBT_RELEASE_NAME "2025_0515_BT_ANDROID_4.4-14"
#include <utils/Log.h>
#include "bt_vendor_aic.h"
#include "hardware.h"
#include "upio.h"
#include "userial_vendor.h"

/******************************************************************************
**  Externs
******************************************************************************/
int inotify_pthread_init(void);
int inotify_pthread_deinit(void);
extern unsigned int aicbt_h5logfilter;
extern unsigned int h5_log_enable;
extern bool aic_btsnoop_dump;
extern bool aic_btsnoop_net_dump;
extern bool aic_btsnoop_save_log;
extern char aic_btsnoop_path[];
extern uint8_t coex_log_enable;
extern void hw_uart_config_start(char transtype);
extern void hw_usb_config_start(char transtype,uint32_t val);

extern void hw_uart_config_cback(void *p_mem);
extern void hw_usb_config_cback(void *p_mem);
extern void (*hw_config_cback)(void *p_mem);

#if (HW_END_WITH_HCI_RESET == TRUE)
void hw_epilog_process(void);
#endif

/******************************************************************************
**  Variables
******************************************************************************/
bt_vendor_callbacks_t *bt_vendor_cbacks = NULL;
uint8_t vnd_local_bd_addr[6]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
bool aicbt_auto_restart = false;
int aicbt_dbg_level = AICBT_DBG_LEVEL;
/******************************************************************************
**  Local type definitions
******************************************************************************/
#define DEVICE_NODE_MAX_LEN     512
#define AICBT_CONF_FILE         "/vendor/etc/bluetooth/aicbt.conf"
#define USB_DEVICE_DIR          "/sys/bus/usb/devices"
#define DEBUG_SCAN_USB          FALSE

/******************************************************************************
**  Static Variables
******************************************************************************/
//transfer_type(4 bit) | transfer_interface(4 bit)
char aicbt_transtype = 0;
static char aicbt_device_node[DEVICE_NODE_MAX_LEN] = {0}; // final config
static char aicbt_device_read[DEVICE_NODE_MAX_LEN] = {0}; // read from config

static const tUSERIAL_CFG userial_H5_cfg = {
    (USERIAL_DATABITS_8 | USERIAL_PARITY_EVEN | USERIAL_STOPBITS_1),
    USERIAL_BAUD_115200,
    USERIAL_HW_FLOW_CTRL_OFF
};

static const tUSERIAL_CFG userial_H4_cfg = {
    (USERIAL_DATABITS_8 | USERIAL_PARITY_NONE | USERIAL_STOPBITS_1),
    USERIAL_BAUD_1_5M,
    USERIAL_HW_FLOW_CTRL_ON,
};

/******************************************************************************
**  Functions
******************************************************************************/
static int Check_Key_Value(char* path, char* key, int value)
{
    FILE *fp;
    char newpath[100];
    char string_get[6];
    int value_int = 0;
    memset(newpath, 0, 100);
    sprintf(newpath, "%s/%s", path, key);
    if ((fp = fopen(newpath, "r")) != NULL) {
        memset(string_get, 0, 6);
        if (fgets(string_get, 5, fp) != NULL)
            if (DEBUG_SCAN_USB)
                ALOGE("string_get %s =%s\n", key, string_get);
        fclose(fp);
        value_int = strtol(string_get, NULL, 16);
        if (value_int == value)
            return 1;
    }
    return 0;
}

static int Scan_Usb_Devices_For_AIC(char* path)
{
    char newpath[100];
    char subpath[100];
    DIR *pdir;
    DIR *newpdir;
    struct dirent *ptr;
    struct dirent *newptr;
    struct stat filestat;
    struct stat subfilestat;
    if (stat(path, &filestat) != 0) {
        ALOGE("The file or path(%s) can not be get stat!\n", newpath);
        return -1;
    }
    if ((filestat.st_mode & S_IFDIR) != S_IFDIR) {
        ALOGE("(%s) is not be a path!\n", path);
        return -1;
    }
    pdir = opendir(path);
    /*enter sub direc*/
    while ((ptr = readdir(pdir)) != NULL) {
        if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
            continue;
        memset(newpath, 0, 100);
        sprintf(newpath, "%s/%s", path, ptr->d_name);
        if (DEBUG_SCAN_USB)
            ALOGE("The file or path(%s)\n", newpath);
        if (stat(newpath, &filestat) != 0) {
            ALOGE("The file or path(%s) can not be get stat!\n", newpath);
            continue;
        }
        /* Check if it is path. */
        if ((filestat.st_mode & S_IFDIR) == S_IFDIR) {
            if (!Check_Key_Value(newpath, "idVendor", 0xa69c))
                continue;
            newpdir = opendir(newpath);
            /*read sub directory*/
            while ((newptr = readdir(newpdir)) != NULL) {
                if (strcmp(newptr->d_name, ".") == 0 || strcmp(newptr->d_name, "..") == 0)
                    continue;
                memset(subpath, 0, 100);
                sprintf(subpath, "%s/%s", newpath,newptr->d_name);
                if (DEBUG_SCAN_USB)
                    ALOGE("The file or path(%s)\n", subpath);
                if (stat(subpath, &subfilestat) != 0) {
                    ALOGE("The file or path(%s) can not be get stat!\n", newpath);
                    continue;
                }
                 /* Check if it is path. */
                if ((subfilestat.st_mode & S_IFDIR) == S_IFDIR) {
                    if (Check_Key_Value(subpath, "bInterfaceClass", 0xe0) && \
                        Check_Key_Value(subpath, "bInterfaceSubClass", 0x01) && \
                        Check_Key_Value(subpath, "bInterfaceProtocol", 0x01)){
                        closedir(newpdir);
                        closedir(pdir);
                        return 1;
                    }
                }
            }
            closedir(newpdir);
        }
    }
    closedir(pdir);
    return 0;
}

static char *aic_trim(char *str)
{
    while (isspace(*str))
        ++str;

    if (!*str)
        return str;

    char *end_str = str + strlen(str) - 1;
    while (end_str > str && isspace(*end_str))
        --end_str;

    end_str[1] = '\0';
    return str;
}

static void aicbt_load_stack_conf(void)
{
    char *split;
    FILE *fp = fopen(AICBT_CONF_FILE, "rt");
    if (!fp) {
      AICBTDBG(LOGERROR, "%s unable to open file '%s': %s", __func__, AICBT_CONF_FILE, strerror(errno));
      return;
    }
    int line_num = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *line_ptr = aic_trim(line);
        ++line_num;

        // Skip blank and comment lines.
        if (*line_ptr == '\0' || *line_ptr == '#' || *line_ptr == '[')
          continue;

        split = strchr(line_ptr, '=');
        if (!split) {
            AICBTDBG(LOGERROR, "%s no key/value separator found on line %d.", __func__, line_num);
            continue;
        }

        *split = '\0';
        char *endptr;
        if (!strcmp(aic_trim(line_ptr), "AicbtLogFilter")) {
            aicbt_h5logfilter = strtol(aic_trim(split+1), &endptr, 0);
        } else if (!strcmp(aic_trim(line_ptr), "H5LogOutput")) {
            h5_log_enable = strtol(aic_trim(split+1), &endptr, 0);
        } else if (!strcmp(aic_trim(line_ptr), "AicBtsnoopDump")) {
            if (!strcmp(aic_trim(split+1), "true"))
                aic_btsnoop_dump = true;
        } else if (!strcmp(aic_trim(line_ptr), "AicBtsnoopNetDump")) {
            if (!strcmp(aic_trim(split+1), "true"))
                aic_btsnoop_net_dump = true;
        } else if (!strcmp(aic_trim(line_ptr), "BtSnoopFileName")) {
            time_t current_time = time(NULL);
            struct tm *time_created = localtime(&current_time);
            char config_time_created[sizeof("YYYY-MM-DD-HH-MM-SS")];
            strftime(config_time_created, sizeof("YYYY-MM-DD-HH-MM-SS"), "%Y-%m-%d-%H-%M-%S", time_created);
            sprintf(aic_btsnoop_path, "%s_aic_%s%s", aic_trim(split+1), config_time_created,".cfa");
        } else if (!strcmp(aic_trim(line_ptr), "BtSnoopSaveLog")) {
            if(!strcmp(aic_trim(split+1), "true"))
                aic_btsnoop_save_log = true;
        } else if (!strcmp(aic_trim(line_ptr), "BtCoexLogOutput")) {
            coex_log_enable = strtol(aic_trim(split+1), &endptr, 0);
        } else if (!strcmp(aic_trim(line_ptr), "AicBtAutoRestart")) {
            if (!strcmp(aic_trim(split+1), "true"))
                aicbt_auto_restart = true;
        } else if (!strcmp(aic_trim(line_ptr), "BtDeviceNode")) {
            strcpy(aicbt_device_read, aic_trim(split + 1));
        } else if (!strcmp(aic_trim(line_ptr), "AicBtLogLevel")) {
            aicbt_dbg_level = strtol(aic_trim(split+1), &endptr, 0);
        }
    }
    fclose(fp);
}

static void aicbt_stack_conf_cleanup(void)
{
    aicbt_h5logfilter = 0;
    h5_log_enable = 0;
    aic_btsnoop_dump = false;
    aic_btsnoop_net_dump = false;
}

static void aicbt_load_device_conf(void)
{
    char *split;
    char bt_port[128];
    memset(aicbt_device_node, 0, sizeof(aicbt_device_node));
    if (strlen(aicbt_device_read) == 0) {
        ALOGE("no config find from file '%s'", AICBT_CONF_FILE);
        property_get("persist.vendor.bluetooth_port", bt_port, "/dev/ttyS1");
        sprintf(&aicbt_device_node[0], "?%s", bt_port);
        ALOGD("use default device node config: %s", aicbt_device_node);
    } else {
        strcpy(aicbt_device_node, aicbt_device_read);
    }

    aicbt_transtype = 0;
    if (aicbt_device_node[0]=='?'){
        /*1.Scan_Usb_Device*/
        if(Scan_Usb_Devices_For_AIC(USB_DEVICE_DIR) == 0x01) {
            strcpy(aicbt_device_node,"/dev/aicbt_dev");
        } else {
            int i = 0;
            while(aicbt_device_node[i] != '\0'){
                aicbt_device_node[i] = aicbt_device_node[i+1];
                i++;
            }
        }
    }

    if ((split = strchr(aicbt_device_node, ':')) != NULL) {
        *split = '\0';
#if 0
        if (!strcmp(aic_trim(split + 1), "H5")) {
            aicbt_transtype |= AICBT_TRANS_H5;
        } else 
#endif
        if (!strcmp(aic_trim(split + 1), "H4")) {
            aicbt_transtype |= AICBT_TRANS_H4;
        }
    } else if (strcmp(aicbt_device_node, "/dev/aicbt_dev")) {
        // default use h4 for uart device
        aicbt_transtype |= AICBT_TRANS_H4;
    }

    if (strcmp(aicbt_device_node, "/dev/aicbt_dev")) {
        aicbt_transtype |= AICBT_TRANS_UART;
    } else {
        aicbt_transtype |= AICBT_TRANS_USB;
        aicbt_transtype |= AICBT_TRANS_H4;
    }

    AICBTDBG(LOGINFO, "final device node: %s, device type: 0x%02X", aicbt_device_node, aicbt_transtype);
}

static void byte_reverse(unsigned char *data, int len)
{
    int i;
    int tmp;

    for (i = 0; i < len/2; i++) {
        tmp = len - i - 1;
        data[i] ^= data[tmp];
        data[tmp] ^= data[i];
        data[i] ^= data[tmp];
    }
}

/*****************************************************************************
**
**   BLUETOOTH VENDOR INTERFACE LIBRARY FUNCTIONS
**
*****************************************************************************/

static int init(const bt_vendor_callbacks_t* p_cb, unsigned char *local_bdaddr)
{
    AICBTDBG(LOGINFO, "AICBT_RELEASE_NAME: %s ", AICBT_RELEASE_NAME);
    AICBTDBG(LOGINFO, "init\r\n");
    
    aicbt_load_stack_conf();
    aicbt_load_device_conf();

    
    if (p_cb == NULL) {
        AICBTDBG(LOGERROR, "init failed with no user callbacks!");
        return -1;
    }

    userial_vendor_init(aicbt_device_node);

    if (aicbt_transtype & AICBT_TRANS_UART) {
        upio_init();
        hw_config_cback = hw_uart_config_cback;
    } else {
        hw_config_cback = hw_usb_config_cback;
    }

    /* store reference to user callbacks */
    bt_vendor_cbacks = (bt_vendor_callbacks_t *) p_cb;

    /* This is handed over from the stack */
    memcpy(vnd_local_bd_addr, local_bdaddr, 6);

    if (aic_btsnoop_dump)
        aic_btsnoop_open();
    if (aic_btsnoop_net_dump)
        aic_btsnoop_net_open();
    inotify_pthread_init();

    return 0;
}


/** Requested operations */
static int op(bt_vendor_opcode_t opcode, void *param)
{
    int retval = 0;

    AICBTDBG(LOGINFO, "op for %d", opcode);

    switch (opcode) {
        case BT_VND_OP_POWER_CTRL:// 0
            {
                int *state = (int *) param;
                upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
                if (*state == BT_VND_PWR_ON)
                {
                    AICBTDBG(LOGINFO, "NOTE: BT_VND_PWR_ON now forces power-off first");
                    upio_set_bluetooth_power(UPIO_BT_POWER_ON);
                    hw_lpm_set_wake_state(false);
                    usleep(100000);
                    hw_lpm_set_wake_state(true);
                    usleep(100000);
                } else {
                    /* Make sure wakelock is released */
                    if (aicbt_transtype & AICBT_TRANS_UART)
                        hw_lpm_set_wake_state(false);
                }
            }
            break;

        case BT_VND_OP_FW_CFG:// 1
            {
                if (aicbt_transtype & AICBT_TRANS_UART) {
                    hw_uart_config_start(aicbt_transtype);
                } else {
                    int usb_info = 0;
                    retval = userial_vendor_usb_ioctl(GET_USB_INFO, &usb_info);
                    if (retval == -1) {
                        AICBTDBG(LOGERROR, "get usb info fail");
                        bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
                        return retval;
                    } else {
                        hw_usb_config_start(AICBT_TRANS_H4, usb_info);
                    }
                }
            }
            break;

        case BT_VND_OP_SCO_CFG:// 2
            {
#if (SCO_CFG_INCLUDED == TRUE)
                hw_sco_config();
#else
                retval = -1;
#endif
            }
            break;

        case BT_VND_OP_USERIAL_OPEN:// 3
            {
                if ((aicbt_transtype & AICBT_TRANS_UART) && (aicbt_transtype & AICBT_TRANS_H5)) {
                    int fd, idx;
                    int (*fd_array)[] = (int (*)[]) param;
                    if (userial_vendor_open((tUSERIAL_CFG *) &userial_H5_cfg) != -1) {
                        retval = 1;
                    }

                    fd = userial_socket_open();
                    if (fd != -1) {
                        for (idx = 0; idx < CH_MAX; idx++)
                            (*fd_array)[idx] = fd;
                    } else {
                        retval = 0;
                    }
                /* retval contains numbers of open fd of HCI channels */
                } else if ((aicbt_transtype & AICBT_TRANS_UART) && (aicbt_transtype & AICBT_TRANS_H4)) {
                    int (*fd_array)[] = (int (*)[]) param;
                    int fd, idx;
                    if (userial_vendor_open((tUSERIAL_CFG *) &userial_H4_cfg) != -1) {
                        retval = 1;
                    }
                    fd = userial_socket_open();

		    userial_vendor_set_hw_fctrl(userial_H4_cfg.hw_fctrl);
                    
		    if (fd != -1) {
                        for (idx=0; idx < CH_MAX; idx++)
                            (*fd_array)[idx] = fd;
                    } else {
                        retval = 0;
                    }
                /* retval contains numbers of open fd of HCI channels */
                } else {
                    int fd, idx = 0;
                    int (*fd_array)[] = (int (*)[]) param;
                    for (idx = 0; idx < 10; idx++) {
                        if (userial_vendor_usb_open() != -1) {
                            retval = 1;
                            break;
                        }
                    }
                    fd = userial_socket_open();
                    if (fd != -1) {
                        for (idx = 0; idx < CH_MAX; idx++)
                            (*fd_array)[idx] = fd;
                    } else {
                        retval = 0;
                    }
                }
                /* retval contains numbers of open fd of HCI channels */
            }
            break;

        case BT_VND_OP_USERIAL_CLOSE:// 4
            {
                userial_vendor_close();
            }
            break;

        case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:// 5
            {
                uint32_t *timeout_ms = (uint32_t *) param;
                *timeout_ms = hw_lpm_get_idle_timeout();
            }
            break;

        case BT_VND_OP_LPM_SET_MODE:// 6
            if (aicbt_transtype & AICBT_TRANS_UART) {
                uint8_t *mode = (uint8_t *) param;
            } else {
                bt_vendor_lpm_mode_t mode = *(bt_vendor_lpm_mode_t *) param;
            }
            break;

        case BT_VND_OP_LPM_WAKE_SET_STATE:// 7
            if (aicbt_transtype & AICBT_TRANS_UART) {
                uint8_t *state = (uint8_t *) param;
                uint8_t wake_assert = (*state == BT_VND_LPM_WAKE_ASSERT) ? \
                                        TRUE : FALSE;

                hw_lpm_set_wake_state(wake_assert);
            }
            break;

         case BT_VND_OP_SET_AUDIO_STATE:// 8
            {
                retval = hw_set_audio_state((bt_vendor_op_audio_state_t *)param);
            }
            break;

        case BT_VND_OP_EPILOG:// 9
            if (aicbt_transtype & AICBT_TRANS_USB) {
                if (bt_vendor_cbacks)
                    bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
            } else {
#if (HW_END_WITH_HCI_RESET == FALSE)
                if (bt_vendor_cbacks) {
                    bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
                }
#else
                hw_epilog_process();
#endif
            }
            break;
        case 0x57:///vend assert subevt
            {
                hw_bt_assert_notify(param);
            }
            break;
    }

    return retval;
}
void deinit_fw_log(void);

/** Closes the interface */
static void cleanup( void )
{
    AICBTDBG(LOGINFO, "cleanup");

    inotify_pthread_deinit();
    if (aicbt_transtype & AICBT_TRANS_UART) {
        upio_cleanup();
    }

    bt_vendor_cbacks = NULL;

    if (aic_btsnoop_dump)
        aic_btsnoop_close();
    if (aic_btsnoop_net_dump)
        aic_btsnoop_net_close();
    aicbt_stack_conf_cleanup();
    deinit_fw_log();
}

// Entry point of DLib
const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    init,
    op,
    cleanup
};
