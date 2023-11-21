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
#define AICBT_RELEASE_NAME "20200318_BT_ANDROID_10.0"
#include <utils/Log.h>
#include "bt_vendor_aic.h"
#include "upio.h"
#include "userial_vendor.h"


/******************************************************************************
**  Externs
******************************************************************************/
extern unsigned int aicbt_h5logfilter;
extern unsigned int h5_log_enable;
extern bool aic_btsnoop_dump;
extern bool aic_btsnoop_net_dump;
extern bool aic_btsnoop_save_log;
extern char aic_btsnoop_path[];
extern uint8_t coex_log_enable;
extern void hw_config_start(char transtype);
extern void hw_usb_config_start(char transtype,uint32_t val);

#if (HW_END_WITH_HCI_RESET == TRUE)
void hw_epilog_process(void);
#endif

/******************************************************************************
**  Variables
******************************************************************************/
bt_vendor_callbacks_t *bt_vendor_cbacks = NULL;
uint8_t vnd_local_bd_addr[6]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
bool aicbt_auto_restart = false;

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
static char aicbt_device_node[DEVICE_NODE_MAX_LEN] = {0};

static const tUSERIAL_CFG userial_H5_cfg =
{
    (USERIAL_DATABITS_8 | USERIAL_PARITY_EVEN | USERIAL_STOPBITS_1),
    USERIAL_BAUD_115200,
    USERIAL_HW_FLOW_CTRL_OFF
};
static const tUSERIAL_CFG userial_H4_cfg =
{
    (USERIAL_DATABITS_8 | USERIAL_PARITY_NONE | USERIAL_STOPBITS_1),
    USERIAL_BAUD_115200,
    USERIAL_HW_FLOW_CTRL_OFF
};


/******************************************************************************
**  Functions
******************************************************************************/
static int Check_Key_Value(char* path,char* key,int value){
    FILE *fp;
    char newpath[100];
    char string_get[6];
    int value_int = 0;
    memset(newpath,0,100);
    sprintf(newpath,"%s/%s",path,key);
    if((fp = fopen(newpath, "r")) != NULL){
        memset(string_get,0,6);
        if(fgets(string_get, 5, fp) != NULL)
            if(DEBUG_SCAN_USB)
                ALOGE("string_get %s =%s\n",key,string_get);
        fclose(fp);
        value_int = strtol(string_get,NULL,16);
        if(value_int == value)
            return 1;
    }
    return 0;
}

static int Scan_Usb_Devices_For_AIC(char* path){
    char newpath[100];
    char subpath[100];
    DIR * pdir;
    DIR * newpdir;
    struct dirent * ptr;
    struct dirent * newptr;
    struct stat filestat;
    struct stat subfilestat;
    if(stat(path, &filestat) != 0){
        ALOGE("The file or path(%s) can not be get stat!\n", newpath);
        return -1;
    }
    if((filestat.st_mode & S_IFDIR) != S_IFDIR){
        ALOGE("(%s) is not be a path!\n", path);
        return -1;
    }
    pdir =opendir(path);
    /*enter sub direc*/
    while((ptr = readdir(pdir))!=NULL){
        if(strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)
            continue;
        memset(newpath,0,100);
        sprintf(newpath,"%s/%s", path,ptr->d_name);
        if(DEBUG_SCAN_USB)
            ALOGE("The file or path(%s)\n", newpath);
        if(stat(newpath, &filestat) != 0){
            ALOGE("The file or path(%s) can not be get stat!\n", newpath);
            continue;
        }
        /* Check if it is path. */
        if((filestat.st_mode & S_IFDIR) == S_IFDIR){
            if(!Check_Key_Value(newpath,"idVendor",0x0bda))
                continue;
            newpdir =opendir(newpath);
            /*read sub directory*/
            while((newptr = readdir(newpdir))!=NULL){
                if(strcmp(newptr->d_name, ".") == 0 || strcmp(newptr->d_name, "..") == 0)
                    continue;
                memset(subpath,0,100);
                sprintf(subpath,"%s/%s", newpath,newptr->d_name);
                if(DEBUG_SCAN_USB)
                    ALOGE("The file or path(%s)\n", subpath);
                if(stat(subpath, &subfilestat) != 0){
                    ALOGE("The file or path(%s) can not be get stat!\n", newpath);
                    continue;
                }
                 /* Check if it is path. */
                if((subfilestat.st_mode & S_IFDIR) == S_IFDIR){
                    if(Check_Key_Value(subpath,"bInterfaceClass",0xe0) && \
                        Check_Key_Value(subpath,"bInterfaceSubClass",0x01) && \
                        Check_Key_Value(subpath,"bInterfaceProtocol",0x01)){
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
static char *aic_trim(char *str) {
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
static void load_aicbt_stack_conf()
{
    char *split;
    FILE *fp = fopen(AICBT_CONF_FILE, "rt");
    if (!fp) {
      ALOGE("%s unable to open file '%s': %s", __func__, AICBT_CONF_FILE, strerror(errno));
      return;
    }
    int line_num = 0;
    char line[1024];
    //char value[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *line_ptr = aic_trim(line);
        ++line_num;

        // Skip blank and comment lines.
        if (*line_ptr == '\0' || *line_ptr == '#' || *line_ptr == '[')
          continue;

        split = strchr(line_ptr, '=');
        if (!split) {
            ALOGE("%s no key/value separator found on line %d.", __func__, line_num);
            continue;
        }

        *split = '\0';
        char *endptr;
        if(!strcmp(aic_trim(line_ptr), "AicbtLogFilter")) {
            aicbt_h5logfilter = strtol(aic_trim(split+1), &endptr, 0);
        }
        else if(!strcmp(aic_trim(line_ptr), "H5LogOutput")) {
            h5_log_enable = strtol(aic_trim(split+1), &endptr, 0);
        }
        else if(!strcmp(aic_trim(line_ptr), "AicBtsnoopDump")) {
            if(!strcmp(aic_trim(split+1), "true"))
                aic_btsnoop_dump = true;
        }
        else if(!strcmp(aic_trim(line_ptr), "AicBtsnoopNetDump")) {
            if(!strcmp(aic_trim(split+1), "true"))
                aic_btsnoop_net_dump = true;
        }
        else if(!strcmp(aic_trim(line_ptr), "BtSnoopFileName")) {
            sprintf(aic_btsnoop_path, "%s_aic", aic_trim(split+1));
        }
        else if(!strcmp(aic_trim(line_ptr), "BtSnoopSaveLog")) {
            if(!strcmp(aic_trim(split+1), "true"))
                aic_btsnoop_save_log = true;
        }
        else if(!strcmp(aic_trim(line_ptr), "BtCoexLogOutput")) {
            coex_log_enable = strtol(aic_trim(split+1), &endptr, 0);
        }
        else if(!strcmp(aic_trim(line_ptr), "AicBtAutoRestart")) {
            if(!strcmp(aic_trim(split+1), "true"))
                aicbt_auto_restart = true;
        }
    }

    fclose(fp);

}

static void aicbt_stack_conf_cleanup()
{
    aicbt_h5logfilter = 0;
    h5_log_enable = 0;
    aic_btsnoop_dump = false;
    aic_btsnoop_net_dump = false;
}

static void load_aicbt_conf()
{
    char *split;
    memset(aicbt_device_node, 0, sizeof(aicbt_device_node));
    FILE *fp = fopen(AICBT_CONF_FILE, "rt");
    if (!fp) {
      ALOGE("%s unable to open file '%s': %s", __func__, AICBT_CONF_FILE, strerror(errno));
      strcpy(aicbt_device_node,"/dev/aicbt_dev");
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
        ALOGE("%s no key/value separator found on line %d.", __func__, line_num);
        strcpy(aicbt_device_node,"/dev/aicbt_dev");
        fclose(fp);
        return;
      }

      *split = '\0';
      if(!strcmp(aic_trim(line_ptr), "BtDeviceNode")) {
        strcpy(aicbt_device_node, aic_trim(split + 1));
      }
    }

    fclose(fp);

    aicbt_transtype = 0;
    if(aicbt_device_node[0]=='?'){
        /*1.Scan_Usb_Device*/
        if(Scan_Usb_Devices_For_AIC(USB_DEVICE_DIR) == 0x01) {
            strcpy(aicbt_device_node,"/dev/aicbt_dev");
        }
        else{
            int i = 0;
            while(aicbt_device_node[i] != '\0'){
                aicbt_device_node[i] = aicbt_device_node[i+1];
                i++;
            }
        }
    }

    if((split = strchr(aicbt_device_node, ':')) != NULL) {
        *split = '\0';
        if(!strcmp(aic_trim(split + 1), "H5")) {
            aicbt_transtype |= AICBT_TRANS_H5;
        }
        else if(!strcmp(aic_trim(split + 1), "H4")) {
            aicbt_transtype |= AICBT_TRANS_H4;
        }
    }
    else if(strcmp(aicbt_device_node, "/dev/aicbt_dev")) {
        //default use h5
        aicbt_transtype |= AICBT_TRANS_H5;
    }

    if(strcmp(aicbt_device_node, "/dev/aicbt_dev")) {
        aicbt_transtype |= AICBT_TRANS_UART;
    }
    else {
        aicbt_transtype |= AICBT_TRANS_USB;
        aicbt_transtype |= AICBT_TRANS_H4;
    }
}

static void byte_reverse(unsigned char* data, int len)
{
    int i;
    int tmp;

    for(i = 0; i < len/2; i++) {
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
    ALOGI("AICBT_RELEASE_NAME: %s",AICBT_RELEASE_NAME);
    ALOGI("init");

    load_aicbt_conf();
    load_aicbt_stack_conf();
    if (p_cb == NULL)
    {
        ALOGE("init failed with no user callbacks!");
        return -1;
    }

    userial_vendor_init(aicbt_device_node);

    if(aicbt_transtype & AICBT_TRANS_UART) {
        upio_init();
        ALOGE("bt_wake_up_host_mode_set(1)");
        bt_wake_up_host_mode_set(1);
    }

    /* store reference to user callbacks */
    bt_vendor_cbacks = (bt_vendor_callbacks_t *) p_cb;

    /* This is handed over from the stack */
    memcpy(vnd_local_bd_addr, local_bdaddr, 6);
    byte_reverse(vnd_local_bd_addr, 6);

    if(aic_btsnoop_dump)
        aic_btsnoop_open();
    if(aic_btsnoop_net_dump)
        aic_btsnoop_net_open();

    return 0;
}



/** Requested operations */
static int op(bt_vendor_opcode_t opcode, void *param)
{
    int retval = 0;

    ALOGE("aic usb op for %d, aicbt_transtype %d", opcode, aicbt_transtype);

    switch(opcode)
    {
        case BT_VND_OP_POWER_CTRL:
            {
                //if(aicbt_transtype & AICBT_TRANS_UART)
                {
                    int *state = (int *) param;
                    if (*state == BT_VND_PWR_OFF)
                    {
                        upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
                        usleep(200000);
                        BTVNDDBG("set power off and delay 200ms");
                    }
                    else if (*state == BT_VND_PWR_ON)
                    {
                        upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
                        usleep(200000);
                        BTVNDDBG("set power off and delay 200ms");
                        upio_set_bluetooth_power(UPIO_BT_POWER_ON);
                        if(aicbt_transtype & AICBT_TRANS_UART)
                            usleep(200000);
                        else
                            usleep(1000000);
                        BTVNDDBG("set power on and delay 1000ms");
                    }
                }
            }
            break;

        case BT_VND_OP_FW_CFG:
            {
                if(aicbt_transtype & AICBT_TRANS_UART) {
                    hw_config_start(aicbt_transtype);
                }
                else {
                  int usb_info = 0;
                  retval = userial_vendor_usb_ioctl(GET_USB_INFO, &usb_info);
                  if(retval == -1) {
                    ALOGE("get usb info fail");
                    bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
                    return retval;
                  }
                  else
                    hw_usb_config_start(AICBT_TRANS_H4, usb_info);
                }
            }
            break;

        case BT_VND_OP_SCO_CFG:
            {
                retval = -1;
            }
            break;

        case BT_VND_OP_USERIAL_OPEN:
            {
                if((aicbt_transtype & AICBT_TRANS_UART) && (aicbt_transtype & AICBT_TRANS_H5)) {
                    int fd, idx;
                    int (*fd_array)[] = (int (*)[]) param;
                    if(userial_vendor_open((tUSERIAL_CFG *) &userial_H5_cfg) != -1){
                        retval = 1;
                    }

                    fd = userial_socket_open();
                    if (fd != -1)
                    {
                        for (idx=0; idx < CH_MAX; idx++)
                            (*fd_array)[idx] = fd;
                    }
                    else
                        retval = 0;

                /* retval contains numbers of open fd of HCI channels */
                }
                else if((aicbt_transtype & AICBT_TRANS_UART) && (aicbt_transtype & AICBT_TRANS_H4)) {
                    int (*fd_array)[] = (int (*)[]) param;
                    int fd, idx;
                    if(userial_vendor_open((tUSERIAL_CFG *) &userial_H4_cfg) != -1) {
                        retval = 1;
                    }
                    fd = userial_socket_open();
                    if (fd != -1)
                    {
                        for (idx=0; idx < CH_MAX; idx++)
                            (*fd_array)[idx] = fd;
                    }
                    else
                        retval = 0;
                /* retval contains numbers of open fd of HCI channels */
                }
                else {
                    BTVNDDBG("USB op for %d", opcode);
                    int fd, idx = 0;
                    int (*fd_array)[] = (int (*)[]) param;
                    for(idx = 0; idx < 10; idx++) {
                        if(userial_vendor_usb_open() != -1){
                            retval = 1;
                            break;
                        }
                    }
                    fd = userial_socket_open();
                    if (fd != -1)
                    {
                        for (idx = 0; idx < CH_MAX; idx++)
                            (*fd_array)[idx] = fd;
                    }
                    else
                        retval = 0;

                }
            }
            break;

        case BT_VND_OP_USERIAL_CLOSE:
            {
                userial_vendor_close();
            }
            break;

        case BT_VND_OP_GET_LPM_IDLE_TIMEOUT:
            {

            }
            break;

        case BT_VND_OP_LPM_SET_MODE:
            {
                bt_vendor_lpm_mode_t mode = *(bt_vendor_lpm_mode_t *) param;
                //for now if the mode is BT_VND_LPM_DISABLE, we guess the hareware bt
                //interface is closing, we shall not send any cmd to the interface.
                if(mode == BT_VND_LPM_DISABLE) {
                    userial_set_bt_interface_state(0);
                }
            }
            break;

        case BT_VND_OP_LPM_WAKE_SET_STATE:
            {

            }
            break;
        case BT_VND_OP_EPILOG:
            {
                if(aicbt_transtype & AICBT_TRANS_USB) {
                    if (bt_vendor_cbacks)
                    {
                        bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
                    }
                }
                else {
#if (HW_END_WITH_HCI_RESET == FALSE)
                    if (bt_vendor_cbacks)
                    {
                        bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
                    }
#else
                    hw_epilog_process();
#endif
                }
            }
            break;

        default:
            break;
    }

    return retval;
}

/** Closes the interface */
static void cleanup( void )
{
    BTVNDDBG("cleanup");

    if(aicbt_transtype & AICBT_TRANS_UART) {
        upio_cleanup();
        bt_wake_up_host_mode_set(0);
    }
    bt_vendor_cbacks = NULL;

    if(aic_btsnoop_dump)
        aic_btsnoop_close();
    if(aic_btsnoop_net_dump)
        aic_btsnoop_net_close();
    aicbt_stack_conf_cleanup();
}

// Entry point of DLib
const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
    sizeof(bt_vendor_interface_t),
    init,
    op,
    cleanup
};
