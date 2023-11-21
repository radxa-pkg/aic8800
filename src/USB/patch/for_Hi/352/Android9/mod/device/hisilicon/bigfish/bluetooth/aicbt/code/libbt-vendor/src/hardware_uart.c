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

#define LOG_TAG "bt_hwcfg_uart"
#define AICBT_RELEASE_NAME "20200318_BT_ANDROID_10.0"

#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor_aic.h"
#include "userial.h"
#include "userial_vendor.h"
#include "upio.h"
#include <unistd.h>
#include <endian.h>
#include <byteswap.h>
#include <unistd.h>

#include "bt_vendor_lib.h"
#include "hardware.h"

/******************************************************************************
**  Constants &  Macros
******************************************************************************/

extern uint8_t vnd_local_bd_addr[BD_ADDR_LEN];
extern bool aicbt_auto_restart;
extern bt_hw_cfg_cb_t hw_cfg_cb;
void hw_config_cback(void *p_evt_buf);
extern int getmacaddr(unsigned char * addr);
extern uint8_t aic_get_fw_project_id(uint8_t *p_buf);
extern struct aic_epatch_entry *aic_get_patch_entry(bt_hw_cfg_cb_t *cfg_cb);
extern int aic_get_bt_firmware(uint8_t** fw_buf, char* fw_short_name);
extern volatile int h5_init_datatrans_flag;

#define EXTRA_CONFIG_FILE "/vendor/etc/bluetooth/aic_btconfig.txt"
static struct aic_bt_vendor_config_entry *extra_extry;
static struct aic_bt_vendor_config_entry *extra_entry_inx = NULL;


/******************************************************************************
**  Static variables
******************************************************************************/
//static bt_hw_cfg_cb_t hw_cfg_cb;

//#define BT_CHIP_PROBE_SIMULATION
#ifdef BT_CHIP_PROBE_SIMULATION
static bt_hw_cfg_cb_t hw_cfg_test;
struct bt_chip_char{
    uint16_t    lmp_subversion;
    uint8_t     hci_version;
    uint8_t     hci_revision;
    uint8_t     chip_type;
};
struct bt_chip_char bt_chip_chars[] = \
{
        {0x8723, 0x4, 0xb, CHIPTYPE_NONE},      //8703as
        {0x8723, 0x6, 0xb, CHIPTYPE_NONE},      //8723bs
        {0x8703, 0x4, 0xd, 0x7},                //8703bs
        {0x8703, 0x6, 0xb, 0x3},                //8723cs-cg
        {0x8703, 0x6, 0xb, 0x4},                //8723cs-vf
        {0x8703, 0x6, 0xb, 0x5},                //8723cs-xx
        {0x8723, 0x6, 0xd, CHIPTYPE_NONE},      //8723ds
};
#endif

typedef struct {
    uint16_t    lmp_subversion;
    uint32_t     hci_version_mask;
    uint32_t     hci_revision_mask;
    uint32_t     chip_type_mask;
    uint32_t     project_id_mask;
    char        *patch_name;
    char        *config_name;
    uint16_t     mac_offset;
    uint32_t    max_patch_size;
} patch_info;

static patch_info patch_table[] = {
/*    lmp_subv                      hci_version_mask                    hci_revision_mask                chip_type_mask              project_id_mask                 fw name                                 config name                         mac offset                         max_patch_size  */
    {0,0,0,0,0, "none_fw", "none_config",0,0}
};

/*
static bt_lpm_param_t lpm_param =
{
    LPM_SLEEP_MODE,
    LPM_IDLE_THRESHOLD,
    LPM_HC_IDLE_THRESHOLD,
    LPM_BT_WAKE_POLARITY,
    LPM_HOST_WAKE_POLARITY,
    LPM_ALLOW_HOST_SLEEP_DURING_SCO,
    LPM_COMBINE_SLEEP_MODE_AND_LPM,
    LPM_ENABLE_UART_TXD_TRI_STATE,*/
    //0,  /* not applicable */
   // 0,  /* not applicable */
   // 0,  /* not applicable */
    /*LPM_PULSED_HOST_WAKE
};*/
//signature: aicsemi 
static const uint8_t AIC_EPATCH_SIGNATURE[7]={0x61,0x69,0x63,0x73,0x65,0x6d,0x69};
//Extension Section IGNATURE:0x77FD0451
static const uint8_t EXTENSION_SECTION_SIGNATURE[4]={0x51,0x04,0xFD,0x77};

static int check_match_state(bt_hw_cfg_cb_t *cfg,uint32_t mask)
{
    patch_info  *patch_entry;
    int res = 0;

    for(patch_entry = patch_table; patch_entry->lmp_subversion != LMP_SUBVERSION_NONE; patch_entry++) {
        if(patch_entry->lmp_subversion != cfg->lmp_subversion)
            continue;
        if((patch_entry->hci_version_mask!=HCI_VERSION_MASK_ALL)&&((patch_entry->hci_version_mask&(1<<cfg->hci_version))==0))
             continue;
        if((patch_entry->hci_revision_mask!=HCI_REVISION_MASK_ALL)&&((patch_entry->hci_revision_mask&(1<<cfg->hci_revision))==0))
             continue;
        if((mask&PATCH_OPTIONAL_MATCH_FLAG_CHIPTYPE)&&(patch_entry->chip_type_mask != CHIP_TYPE_MASK_ALL)&&((patch_entry->chip_type_mask&(1<<cfg->chip_type))==0))
             continue;
        res++;
    }
    ALOGI( "check_match_state return %d(cfg->lmp_subversion:0x%x cfg->hci_vesion:0x%x cfg->hci_revision:0x%x cfg->chip_type:0x%x mask:%08x)\n",
            res, cfg->lmp_subversion, cfg->hci_version, cfg->hci_revision, cfg->chip_type, mask);
    return res;
}

static patch_info* get_patch_entry(bt_hw_cfg_cb_t *cfg)
{
    patch_info  *patch_entry;

    ALOGI("get_patch_entry(lmp_subversion:0x%x hci_vesion:0x%x cfg->hci_revision:0x%x chip_type:0x%x)\n",
            cfg->lmp_subversion, cfg->hci_version, cfg->hci_revision, cfg->chip_type);
    for(patch_entry = patch_table; patch_entry->lmp_subversion != LMP_SUBVERSION_NONE; patch_entry++) {
        if(patch_entry->lmp_subversion != cfg->lmp_subversion)
            continue;
        if((patch_entry->hci_version_mask != HCI_VERSION_MASK_ALL)&&((patch_entry->hci_version_mask&(1<<cfg->hci_version)) == 0))
             continue;
        if((patch_entry->hci_revision_mask != HCI_REVISION_MASK_ALL)&&((patch_entry->hci_revision_mask&(1<<cfg->hci_revision)) == 0))
             continue;
        if((patch_entry->chip_type_mask != CHIP_TYPE_MASK_ALL)&&((patch_entry->chip_type_mask&(1<<cfg->chip_type)) == 0))
             continue;
        break;
    }
    ALOGI("get_patch_entry return(patch_name:%s config_name:%s mac_offset:0x%x)\n",
            patch_entry->patch_name, patch_entry->config_name, patch_entry->mac_offset);
    return patch_entry;
}

typedef struct _baudrate_ex
{
    uint32_t aic_speed;
    uint32_t uart_speed;
}baudrate_ex;

baudrate_ex baudrates[] =
{
    {0x00006004, 921600},
    {0x05F75004, 921600},
    {0x00004003, 1500000},
    {0x052A8002, 1500000},
    {0x04928002, 1500000},
    {0x00005002, 2000000},
    {0x00008001, 3000000},
    {0x04928001, 3000000},
    {0x06B58001, 3000000},
    {0x00007001, 3500000},
    {0x052A6001, 3500000},
    {0x00005001, 4000000},
    {0x0000701d, 115200},
    {0x0252C014, 115200}
};

/**
* Change aic Bluetooth speed to uart speed. It is matching in the struct baudrates:
*
* @code
* baudrate_ex baudrates[] =
* {
*   {0x7001, 3500000},
*   {0x6004, 921600},
*   {0x4003, 1500000},
*   {0x5001, 4000000},
*   {0x5002, 2000000},
*   {0x8001, 3000000},
*   {0x701d, 115200}
* };
* @endcode
*
* If there is no match in baudrates, uart speed will be set as #115200.
*
* @param aic_speed aic Bluetooth speed
* @param uart_speed uart speed
*
*/
static void aic_speed_to_uart_speed(uint32_t aic_speed, uint32_t* uart_speed)
{
    *uart_speed = 115200;

    uint8_t i;
    for (i=0; i< sizeof(baudrates)/sizeof(baudrate_ex); i++)
    {
        if (baudrates[i].aic_speed == aic_speed)
        {
            *uart_speed = baudrates[i].uart_speed;
            return;
        }
    }
    return;
}

/**
* Change uart speed to aic Bluetooth speed. It is matching in the struct baudrates:
*
* @code
* baudrate_ex baudrates[] =
* {
*   {0x7001, 3500000},
*   {0x6004, 921600},
*   {0x4003, 1500000},
*   {0x5001, 4000000},
*   {0x5002, 2000000},
*   {0x8001, 3000000},
*   {0x701d, 115200}
* };
* @endcode
*
* If there is no match in baudrates, aic Bluetooth speed will be set as #0x701D.
*
* @param uart_speed uart speed
* @param aic_speed aic Bluetooth speed
*
*//*
static inline void uart_speed_to_aic_speed(uint32_t uart_speed, uint32_t* aic_speed)
{
    *aic_speed = 0x701D;

    unsigned int i;
    for (i=0; i< sizeof(baudrates)/sizeof(baudrate_ex); i++)
    {
      if (baudrates[i].uart_speed == uart_speed)
      {
          *aic_speed = baudrates[i].aic_speed;
          return;
      }
    }
    return;
}
*/

/*******************************************************************************
**
** Function         hw_config_set_bdaddr
**
** Description      Program controller's Bluetooth Device Address
**
** Returns          TRUE, if valid address is sent
**                  FALSE, otherwise
**
*******************************************************************************/
static uint8_t hw_config_set_controller_baudrate(HC_BT_HDR *p_buf, uint32_t baudrate)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    UINT16_TO_STREAM(p, HCI_VSC_UPDATE_BAUDRATE);
    *p++ = 4; /* parameter length */
    UINT32_TO_STREAM(p, baudrate);

    p_buf->len = HCI_CMD_PREAMBLE_SIZE + 4;

    retval = bt_vendor_cbacks->xmit_cb(HCI_VSC_UPDATE_BAUDRATE, p_buf, \
                                 hw_config_cback);

    return (retval);
}

/*******************************************************************************
**
** Function        line_speed_to_userial_baud
**
** Description     helper function converts line speed number into USERIAL baud
**                 rate symbol
**
** Returns         unit8_t (USERIAL baud symbol)
**
*******************************************************************************/
static uint8_t line_speed_to_userial_baud(uint32_t line_speed)
{
    uint8_t baud;

    if (line_speed == 4000000)
        baud = USERIAL_BAUD_4M;
    else if (line_speed == 3000000)
        baud = USERIAL_BAUD_3M;
    else if (line_speed == 2000000)
        baud = USERIAL_BAUD_2M;
    else if (line_speed == 1500000)
        baud = USERIAL_BAUD_1_5M;
    else if (line_speed == 1000000)
        baud = USERIAL_BAUD_1M;
    else if (line_speed == 921600)
        baud = USERIAL_BAUD_921600;
    else if (line_speed == 460800)
        baud = USERIAL_BAUD_460800;
    else if (line_speed == 230400)
        baud = USERIAL_BAUD_230400;
    else if (line_speed == 115200)
        baud = USERIAL_BAUD_115200;
    else if (line_speed == 57600)
        baud = USERIAL_BAUD_57600;
    else if (line_speed == 19200)
        baud = USERIAL_BAUD_19200;
    else if (line_speed == 9600)
        baud = USERIAL_BAUD_9600;
    else if (line_speed == 1200)
        baud = USERIAL_BAUD_1200;
    else if (line_speed == 600)
        baud = USERIAL_BAUD_600;
    else
    {
        ALOGE( "userial vendor: unsupported baud speed %d", line_speed);
        baud = USERIAL_BAUD_115200;
    }

    return baud;
}



/**
* Change uart speed to aic Bluetooth speed. It is matching in the struct baudrates:
*
* @code
* baudrate_ex baudrates[] =
* {
*   {0x7001, 3500000},
*   {0x6004, 921600},
*   {0x4003, 1500000},
*   {0x5001, 4000000},
*   {0x5002, 2000000},
*   {0x8001, 3000000},
*   {0x701d, 115200}
* };
* @endcode
*
* If there is no match in baudrates, aic Bluetooth speed will be set as #0x701D.
*
* @param uart_speed uart speed
* @param aic_speed aic Bluetooth speed
*
*//*
static inline void uart_speed_to_aic_speed(uint32_t uart_speed, uint32_t* aic_speed)
{
    *aic_speed = 0x701D;

    unsigned int i;
    for (i=0; i< sizeof(baudrates)/sizeof(baudrate_ex); i++)
    {
      if (baudrates[i].uart_speed == uart_speed)
      {
          *aic_speed = baudrates[i].aic_speed;
          return;
      }
    }
    return;
}
*/

static void line_process(char *buf, unsigned short *offset, int *t)
{
    char *head = buf;
    char *ptr = buf;
    char *argv[32];
    int argc = 0;
    unsigned char len = 0;
    int i = 0;
    static int alt_size = 0;

    if(buf[0] == '\0' || buf[0] == '#' || buf[0] == '[')
        return;
    if(alt_size > MAX_ALT_CONFIG_SIZE-4)
    {
        ALOGW("Extra Config file is too large");
        return;
    }
    if(extra_entry_inx == NULL)
        extra_entry_inx = extra_extry;
    ALOGI("line_process:%s", buf);
    while((ptr = strsep(&head, ", \t")) != NULL)
    {
        if(!ptr[0])
            continue;
        argv[argc++] = ptr;
        if(argc >= 32) {
            ALOGW("Config item is too long");
            break;
        }
    }

    if(argc <4) {
        ALOGE("Invalid Config item, ignore");
        return;
    }

    offset[(*t)] = (unsigned short)((strtoul(argv[0], NULL, 16)) | (strtoul(argv[1], NULL, 16) << 8));
    ALOGI("Extra Config offset %04x", offset[(*t)]);
    extra_entry_inx->offset = offset[(*t)];
    (*t)++;
    len = (unsigned char)strtoul(argv[2], NULL, 16);
    if(len != (unsigned char)(argc - 3)) {
        ALOGE("Extra Config item len %d is not match, we assume the actual len is %d", len, (argc-3));
        len = argc -3;
    }
    extra_entry_inx->entry_len = len;

    alt_size += len + sizeof(struct aic_bt_vendor_config_entry);
    if(alt_size > MAX_ALT_CONFIG_SIZE)
    {
        ALOGW("Extra Config file is too large");
        extra_entry_inx->offset = 0;
        extra_entry_inx->entry_len = 0;
        alt_size -= (len + sizeof(struct aic_bt_vendor_config_entry));
        return;
    }
    for(i = 0; i < len; i++)
    {
        extra_entry_inx->entry_data[i] = (uint8_t)strtoul(argv[3+i], NULL, 16);
        ALOGI("data[%d]:%02x", i, extra_entry_inx->entry_data[i]);
    }
    extra_entry_inx = (struct aic_bt_vendor_config_entry *)((uint8_t *)extra_entry_inx + len + sizeof(struct aic_bt_vendor_config_entry));
}

static void parse_extra_config(const char *path, patch_info *patch_entry, unsigned short *offset, int *t)
{
    int fd, ret;
    unsigned char buf[1024];

    fd = open(path, O_RDONLY);
    if(fd == -1) {
        ALOGI("Couldn't open extra config %s, err:%s", path, strerror(errno));
        return;
    }

    ret = read(fd, buf, sizeof(buf));
    if(ret == -1) {
        ALOGE("Couldn't read %s, err:%s", path, strerror(errno));
        close(fd);
        return;
    }
    else if(ret == 0) {
        ALOGE("%s is empty", path);
        close(fd);
        return;
    }

    if(ret > 1022) {
        ALOGE("Extra config file is too big");
        close(fd);
        return;
    }
    buf[ret++] = '\n';
    buf[ret++] = '\0';
    close(fd);
    char *head = (void *)buf;
    char *ptr = (void *)buf;
    ptr = strsep(&head, "\n\r");
    if(strncmp(ptr, patch_entry->config_name, strlen(ptr)))
    {
        ALOGW("Extra config file not set for %s, ignore", patch_entry->config_name);
        return;
    }
    while((ptr = strsep(&head, "\n\r")) != NULL)
    {
        if(!ptr[0])
            continue;
        line_process(ptr, offset, t);
    }
}

static inline int getAltSettings(patch_info *patch_entry, unsigned short *offset)//(patch_info *patch_entry, unsigned short *offset, int max_group_cnt)
{
    int n = 0;
    if(patch_entry)
        offset[n++] = patch_entry->mac_offset;
    else
      return n;
/*
//sample code, add special settings

    offset[n++] = 0x15B;
*/
    if(extra_extry)
        parse_extra_config(EXTRA_CONFIG_FILE, patch_entry, offset, &n);

    return n;
}
static inline int getAltSettingVal(patch_info *patch_entry, unsigned short offset, unsigned char * val)
{
    int res = 0;
    int i = 0;
    struct aic_bt_vendor_config_entry *ptr = extra_extry;

    while(ptr->offset)
    {
        if(ptr->offset == offset)
        {
            if(offset != patch_entry->mac_offset)
            {
                memcpy(val, ptr->entry_data, ptr->entry_len);
                res = ptr->entry_len;
                ALOGI("Get Extra offset:%04x, val:", offset);
                for(i = 0; i < ptr->entry_len; i++)
                    ALOGI("%02x", ptr->entry_data[i]);
            }
            break;
        }
        ptr = (struct aic_bt_vendor_config_entry *)((uint8_t *)ptr + ptr->entry_len + sizeof(struct aic_bt_vendor_config_entry));
    }
/*
    switch(offset)
    {

//sample code, add special settings
        case 0x15B:
            val[0] = 0x0B;
            val[1] = 0x0B;
            val[2] = 0x0B;
            val[3] = 0x0B;
            res = 4;
            break;

        default:
            res = 0;
            break;
    }
*/
    if((patch_entry)&&(offset == patch_entry->mac_offset)&&(res == 0))
    {
        if(getmacaddr(val) == 0){
            ALOGI("MAC: %02x:%02x:%02x:%02x:%02x:%02x", val[5], val[4], val[3], val[2], val[1], val[0]);
            res = 6;
        }
    }
    return res;
}


static void aic_update_altsettings(patch_info *patch_entry, unsigned char* config_buf_ptr, size_t *config_len_ptr)
{
    unsigned short offset[256], data_len;
    unsigned char val[256];

    struct aic_bt_vendor_config* config = (struct aic_bt_vendor_config*) config_buf_ptr;
    struct aic_bt_vendor_config_entry* entry = config->entry;
    size_t config_len = *config_len_ptr;
    unsigned int  i = 0;
    int count = 0,temp = 0, j;

    if((extra_extry = (struct aic_bt_vendor_config_entry *)malloc(MAX_ALT_CONFIG_SIZE)) == NULL)
    {
        ALOGE("malloc buffer for extra_extry failed");
    }
    else
        memset(extra_extry, 0, MAX_ALT_CONFIG_SIZE);

    ALOGI("ORG Config len=%08zx:\n", config_len);
    for(i=0;i<=config_len;i+=0x10)
    {
        ALOGI("%08x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, \
            config_buf_ptr[i], config_buf_ptr[i+1], config_buf_ptr[i+2], config_buf_ptr[i+3], config_buf_ptr[i+4], config_buf_ptr[i+5], config_buf_ptr[i+6], config_buf_ptr[i+7], \
            config_buf_ptr[i+8], config_buf_ptr[i+9], config_buf_ptr[i+10], config_buf_ptr[i+11], config_buf_ptr[i+12], config_buf_ptr[i+13], config_buf_ptr[i+14], config_buf_ptr[i+15]);
    }

    memset(offset, 0, sizeof(offset));
    memset(val, 0, sizeof(val));
    data_len = le16_to_cpu(config->data_len);

    count = getAltSettings(patch_entry, offset);//getAltSettings(patch_entry, offset, sizeof(offset)/sizeof(unsigned short));
    if(count <= 0){
        ALOGI("aic_update_altsettings: No AltSettings");
        return;
    }else{
        ALOGI("aic_update_altsettings: %d AltSettings", count);
    }

    if (data_len != config_len - sizeof(struct aic_bt_vendor_config))
    {
        ALOGE("aic_update_altsettings: config len(%x) is not right(%lx)", data_len, (unsigned long)(config_len-sizeof(struct aic_bt_vendor_config)));
        return;
    }

    for (i=0; i<data_len;)
    {
        for(j = 0; j < count;j++)
        {
            if(le16_to_cpu(entry->offset) == offset[j])
            {
                if(offset[j] == patch_entry->mac_offset)
                    offset[j] = 0;
                else
                {
                    struct aic_bt_vendor_config_entry *t = extra_extry;
                    while(t->offset) {
                        if(t->offset == le16_to_cpu(entry->offset))
                        {
                            if(t->entry_len == entry->entry_len)
                                offset[j] = 0;
                            break;
                        }
                        t = (struct aic_bt_vendor_config_entry *)((uint8_t *)t + t->entry_len + sizeof(struct aic_bt_vendor_config_entry));
                    }
                }
            }
        }
        if(getAltSettingVal(patch_entry, le16_to_cpu(entry->offset), val) == entry->entry_len){
            ALOGI("aic_update_altsettings: replace %04x[%02x]", le16_to_cpu(entry->offset), entry->entry_len);
            memcpy(entry->entry_data, val, entry->entry_len);
        }
        temp = entry->entry_len + sizeof(struct aic_bt_vendor_config_entry);
        i += temp;
        entry = (struct aic_bt_vendor_config_entry*)((uint8_t*)entry + temp);
    }
    for(j = 0; j < count;j++){
        if(offset[j] == 0)
            continue;
        entry->entry_len = getAltSettingVal(patch_entry, offset[j], val);
        if(entry->entry_len <= 0)
            continue;
        entry->offset = cpu_to_le16(offset[j]);
        memcpy(entry->entry_data, val, entry->entry_len);
        ALOGI("aic_update_altsettings: add %04x[%02x]", le16_to_cpu(entry->offset), entry->entry_len);
        temp = entry->entry_len + sizeof(struct aic_bt_vendor_config_entry);
        i += temp;
        entry = (struct aic_bt_vendor_config_entry*)((uint8_t*)entry + temp);
    }
    config->data_len = cpu_to_le16(i);
    *config_len_ptr = i+sizeof(struct aic_bt_vendor_config);

    if(extra_extry)
    {
        free(extra_extry);
        extra_extry = NULL;
        extra_entry_inx = NULL;
    }

    ALOGI("NEW Config len=%08zx:\n", *config_len_ptr);
    for(i=0;i<=(*config_len_ptr);i+=0x10)
    {
        ALOGI("%08x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", i, \
            config_buf_ptr[i], config_buf_ptr[i+1], config_buf_ptr[i+2], config_buf_ptr[i+3], config_buf_ptr[i+4], config_buf_ptr[i+5], config_buf_ptr[i+6], config_buf_ptr[i+7], \
            config_buf_ptr[i+8], config_buf_ptr[i+9], config_buf_ptr[i+10], config_buf_ptr[i+11], config_buf_ptr[i+12], config_buf_ptr[i+13], config_buf_ptr[i+14], config_buf_ptr[i+15]);
    }
    return;
}


static uint32_t aic_parse_config_file(unsigned char** config_buf, size_t* filelen, uint8_t bt_addr[6], uint16_t mac_offset)
{
    struct aic_bt_vendor_config* config = (struct aic_bt_vendor_config*) *config_buf;
    uint16_t config_len = le16_to_cpu(config->data_len), temp = 0;
    struct aic_bt_vendor_config_entry* entry = config->entry;
    unsigned int i = 0;
    uint32_t baudrate = 0;
    uint8_t  heartbeat_buf = 0;
    //uint32_t config_has_bdaddr = 0;
    uint8_t *p;

    ALOGD("bt_addr = %x", bt_addr[0]);
    if (le32_to_cpu(config->signature) != AIC_VENDOR_CONFIG_MAGIC)
    {
        ALOGE("config signature magic number(0x%x) is not set to AIC_VENDOR_CONFIG_MAGIC", config->signature);
        return 0;
    }

    if (config_len != *filelen - sizeof(struct aic_bt_vendor_config))
    {
        ALOGE("config len(0x%x) is not right(0x%lx)", config_len, (unsigned long)(*filelen-sizeof(struct aic_bt_vendor_config)));
        return 0;
    }

    hw_cfg_cb.heartbeat = 0;
    for (i=0; i<config_len;)
    {
        switch(le16_to_cpu(entry->offset))
        {
            case 0xc:
            {
                p = (uint8_t *)entry->entry_data;
                STREAM_TO_UINT32(baudrate, p);
                if (entry->entry_len >= 12)
                {
                    hw_cfg_cb.hw_flow_cntrl |= 0x80; /* bit7 set hw flow control */
                    if (entry->entry_data[12] & 0x04) /* offset 0x18, bit2 */
                        hw_cfg_cb.hw_flow_cntrl |= 1; /* bit0 enable hw flow control */
                }

                ALOGI("config baud rate to :0x%08x, hwflowcontrol:0x%x, 0x%x", baudrate, entry->entry_data[12], hw_cfg_cb.hw_flow_cntrl);
                break;
            }
            case 0x017a:
            {
                if(mac_offset == CONFIG_MAC_OFFSET_GEN_1_2)
                {
                    p = (uint8_t *)entry->entry_data;
                    STREAM_TO_UINT8(heartbeat_buf, p);
                    if((heartbeat_buf & 0x02) && (heartbeat_buf & 0x10))
                        hw_cfg_cb.heartbeat = 1;
                    else
                        hw_cfg_cb.heartbeat = 0;

                    ALOGI("config 0x017a heartbeat = %d",hw_cfg_cb.heartbeat);
                }
                break;
            }
            case 0x01be:
            {
                if(mac_offset == CONFIG_MAC_OFFSET_GEN_3PLUS || mac_offset == CONFIG_MAC_OFFSET_GEN_4PLUS)
                {
                    p = (uint8_t *)entry->entry_data;
                    STREAM_TO_UINT8(heartbeat_buf, p);
                    if((heartbeat_buf & 0x02) && (heartbeat_buf & 0x10))
                        hw_cfg_cb.heartbeat = 1;
                    else
                        hw_cfg_cb.heartbeat = 0;

                    ALOGI("config 0x01be heartbeat = %d",hw_cfg_cb.heartbeat);
                }
                break;
            }
            default:
                ALOGI("config offset(0x%x),length(0x%x)", entry->offset, entry->entry_len);
                break;
        }
        temp = entry->entry_len + sizeof(struct aic_bt_vendor_config_entry);
        i += temp;
        entry = (struct aic_bt_vendor_config_entry*)((uint8_t*)entry + temp);
    }

    return baudrate;
}

static void aic_get_bt_final_patch(bt_hw_cfg_cb_t* cfg_cb)
{
    uint8_t proj_id = 0;
    struct aic_epatch_entry* entry = NULL;
    struct aic_epatch *patch = (struct aic_epatch *)cfg_cb->fw_buf;
    //int iBtCalLen = 0;

    if (memcmp(cfg_cb->fw_buf, AIC_EPATCH_SIGNATURE, 7))
    {
        ALOGE("check signature error");
        cfg_cb->dl_fw_flag = 0;
        goto free_buf;
    }

    /* check the extension section signature */
    if (memcmp(cfg_cb->fw_buf + cfg_cb->fw_len - 4, EXTENSION_SECTION_SIGNATURE, 4))
    {
        ALOGE("check extension section signature error");
        cfg_cb->dl_fw_flag = 0;
        goto free_buf;
    }

    proj_id = aic_get_fw_project_id(cfg_cb->fw_buf + cfg_cb->fw_len - 5);

    if((hw_cfg_cb.project_id_mask != PROJECT_ID_MASK_ALL)&& ((hw_cfg_cb.project_id_mask&(1<<proj_id)) ==0))
    {
        ALOGE("hw_cfg_cb.project_id_mask is 0x%08x, fw project_id is %d, does not match!!!",
                        hw_cfg_cb.project_id_mask, proj_id);
        cfg_cb->dl_fw_flag = 0;
        goto free_buf;
    }

    entry = aic_get_patch_entry(cfg_cb);
    if (entry)
    {
        cfg_cb->total_len = entry->patch_length + cfg_cb->config_len;
    }
    else
    {
        cfg_cb->dl_fw_flag = 0;
        goto free_buf;
    }

    ALOGI("total_len = 0x%x", cfg_cb->total_len);

    if (!(cfg_cb->total_buf = malloc(cfg_cb->total_len)))
    {
        ALOGE("Can't alloc memory for multi fw&config, errno:%d", errno);
        cfg_cb->dl_fw_flag = 0;
        goto free_buf;
    }
    else
    {
        memcpy(cfg_cb->total_buf, cfg_cb->fw_buf + entry->patch_offset, entry->patch_length);
        memcpy(cfg_cb->total_buf + entry->patch_length - 4, &patch->fw_version, 4);
        memcpy(&entry->svn_version, cfg_cb->total_buf + entry->patch_length - 8, 4);
        memcpy(&entry->coex_version, cfg_cb->total_buf + entry->patch_length - 12, 4);
        ALOGI("BTCOEX:20%06d-%04x svn_version:%d lmp_subversion:0x%x hci_version:0x%x hci_revision:0x%x chip_type:%d Cut:%d libbt-vendor_uart version:%s\n",
        ((entry->coex_version >> 16) & 0x7ff) + ((entry->coex_version >> 27) * 10000),
        (entry->coex_version & 0xffff), entry->svn_version, cfg_cb->lmp_subversion, cfg_cb->hci_version, cfg_cb->hci_revision, cfg_cb->chip_type, cfg_cb->eversion+1, AIC_VERSION);
    }

    if (cfg_cb->config_len)
    {
        memcpy(cfg_cb->total_buf+entry->patch_length, cfg_cb->config_buf, cfg_cb->config_len);
    }

    cfg_cb->dl_fw_flag = 1;
    ALOGI("Fw:%s exists, config file:%s exists", (cfg_cb->fw_len>0)?"":"not", (cfg_cb->config_len>0)?"":"not");

free_buf:
    if (cfg_cb->fw_len > 0)
    {
        free(cfg_cb->fw_buf);
        cfg_cb->fw_len = 0;
    }

    if (cfg_cb->config_len > 0)
    {
        free(cfg_cb->config_buf);
        cfg_cb->config_len = 0;
    }

    if(entry)
    {
        free(entry);
    }
}

static uint32_t aic_get_bt_config(unsigned char** config_buf,
        uint32_t* config_baud_rate, char * config_file_short_name, uint16_t mac_offset)
{
    char bt_config_file_name[PATH_MAX] = {0};
    struct stat st;
    size_t filelen;
    int fd;
    //FILE* file = NULL;

    sprintf(bt_config_file_name, BT_CONFIG_DIRECTORY, config_file_short_name);
    ALOGI("BT config file: %s", bt_config_file_name);

    if (stat(bt_config_file_name, &st) < 0)
    {
        ALOGE("can't access bt config file:%s, errno:%d\n", bt_config_file_name, errno);
        return 0;
    }

    filelen = st.st_size;
    if(filelen > MAX_ORG_CONFIG_SIZE)
    {
        ALOGE("bt config file is too large(>0x%04x)", MAX_ORG_CONFIG_SIZE);
        return 0;
    }

    if ((fd = open(bt_config_file_name, O_RDONLY)) < 0)
    {
        ALOGE("Can't open bt config file");
        return 0;
    }

    if ((*config_buf = malloc(MAX_ORG_CONFIG_SIZE+MAX_ALT_CONFIG_SIZE)) == NULL)
    {
        ALOGE("malloc buffer for config file fail(0x%zx)\n", filelen);
        close(fd);
        return 0;
    }

    if (read(fd, *config_buf, filelen) < (ssize_t)filelen)
    {
        ALOGE("Can't load bt config file");
        free(*config_buf);
        close(fd);
        return 0;
    }

    *config_baud_rate = aic_parse_config_file(config_buf, &filelen, vnd_local_bd_addr, mac_offset);
    ALOGI("Get config baud rate from config file:0x%x", *config_baud_rate);

    close(fd);
    return filelen;
}


static int hci_download_patch_h4(HC_BT_HDR *p_buf, int index, uint8_t *data, int len)
{
    uint8_t retval = FALSE;
    uint8_t *p = (uint8_t *) (p_buf + 1);

    UINT16_TO_STREAM(p, HCI_VSC_DOWNLOAD_FW_PATCH);
    *p++ = 1 + len;  /* parameter length */
    *p++ = index;
    memcpy(p, data, len);
    p_buf->len = HCI_CMD_PREAMBLE_SIZE + 1+len;

    hw_cfg_cb.state = HW_CFG_DL_FW_PATCH;

    retval = bt_vendor_cbacks->xmit_cb(HCI_VSC_DOWNLOAD_FW_PATCH, p_buf, hw_config_cback);
    return retval;
}

/*******************************************************************************
**
** Function         hw_config_cback
**
** Description      Callback function for controller configuration
**
** Returns          None
**
*******************************************************************************/
void hw_config_cback(void *p_mem)
{
    HC_BT_HDR   *p_evt_buf = NULL;
    uint8_t     *p = NULL, *pp=NULL;
    uint8_t     status = 0;
    uint16_t    opcode = 0;
    HC_BT_HDR   *p_buf = NULL;
    uint8_t     is_proceeding = FALSE;
    int         i = 0;
    uint8_t     iIndexRx = 0;
    patch_info* paic_patch_file_info = NULL;
    uint32_t    host_baudrate = 0;

#if (USE_CONTROLLER_BDADDR == TRUE)
    //const uint8_t null_bdaddr[BD_ADDR_LEN] = {0,0,0,0,0,0};
#endif

    if(p_mem != NULL)
    {
        p_evt_buf = (HC_BT_HDR *) p_mem;
        status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_OFFSET);
        p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE_OFFSET;
        STREAM_TO_UINT16(opcode,p);
    }

    if(opcode == HCI_VSC_H5_INIT) {
        if(status != 0) {
          ALOGE("%s, status = %d", __func__, status);
          if ((bt_vendor_cbacks) && (p_evt_buf != NULL))
              bt_vendor_cbacks->dealloc(p_evt_buf);
          if(aicbt_auto_restart) {
              if(bt_vendor_cbacks)
                  bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
              kill(getpid(), SIGKILL);
          }
          return;
        }
    }

    /* Ask a new buffer big enough to hold any HCI commands sent in here */
    /*a cut fc6d status==1*/
    if (((status == 0) ||(opcode == HCI_VSC_READ_ROM_VERSION)) && bt_vendor_cbacks)
        p_buf = (HC_BT_HDR *)bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + HCI_CMD_MAX_LEN);

    if (p_buf != NULL)
    {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->len = 0;
        p_buf->layer_specific = 0;

        BTVNDDBG("hw_cfg_cb.state = %i", hw_cfg_cb.state);
        switch (hw_cfg_cb.state)
        {
            case HW_CFG_H5_INIT:
            {
                p = (uint8_t *)(p_buf + 1);
                UINT16_TO_STREAM(p, HCI_READ_LMP_VERSION);
                *p++ = 0;
                p_buf->len = HCI_CMD_PREAMBLE_SIZE;

                hw_cfg_cb.state = HW_CFG_READ_LOCAL_VER;
                is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_READ_LMP_VERSION, p_buf, hw_config_cback);
                break;
            }
            case HW_CFG_READ_LOCAL_VER:
            {
                if (status == 0 && p_evt_buf)
                {
                    p = ((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OP1001_HCI_VERSION_OFFSET);
                    STREAM_TO_UINT16(hw_cfg_cb.hci_version, p);
                    p = ((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OP1001_HCI_REVISION_OFFSET);
                    STREAM_TO_UINT16(hw_cfg_cb.hci_revision, p);
                    p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OP1001_LMP_SUBVERSION_OFFSET;
                    STREAM_TO_UINT16(hw_cfg_cb.lmp_subversion, p);
                    BTVNDDBG("lmp_subversion = 0x%x hw_cfg_cb.hci_version = 0x%x hw_cfg_cb.hci_revision = 0x%x", hw_cfg_cb.lmp_subversion, hw_cfg_cb.hci_version, hw_cfg_cb.hci_revision);
                    {
                        hw_cfg_cb.state = HW_CFG_READ_ECO_VER;
                        p = (uint8_t *) (p_buf + 1);
                        UINT16_TO_STREAM(p, HCI_VSC_READ_ROM_VERSION);
                        *p++ = 0;
                        p_buf->len = HCI_CMD_PREAMBLE_SIZE;
                        is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_VSC_READ_ROM_VERSION, p_buf, hw_config_cback);
                    }
                }
                break;
            }
            case HW_CFG_READ_ECO_VER:
            {
                if(status == 0 && p_evt_buf)
                {
                    hw_cfg_cb.eversion = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPFC6D_EVERSION_OFFSET);
                    BTVNDDBG("hw_config_cback chip_id of the IC:%d", hw_cfg_cb.eversion+1);
                }
                else if(1 == status)
                {
                    hw_cfg_cb.eversion = 0;
                }
                else
                {
                    is_proceeding = FALSE;
                    break;
                }

                if(check_match_state(&hw_cfg_cb, 0) > 1)    // check if have multiple matched patch_entry by lmp_subversion,hci_version, hci_revision
                {
                    hw_cfg_cb.state = HW_CFG_READ_CHIP_TYPE;
                     p = (uint8_t *) (p_buf + 1);
                    UINT16_TO_STREAM(p, HCI_VSC_READ_CHIP_TYPE);
                    *p++ = 5;
                    if(hw_cfg_cb.lmp_subversion == 0x8761){
                        UINT8_TO_STREAM(p, 0x20);
                        UINT32_TO_STREAM(p, 0xB000A0A4);
                    }else{
                        UINT8_TO_STREAM(p, 0x00);
                        UINT32_TO_STREAM(p, 0xB000A094);
                    }
                    p_buf->len = HCI_CMD_PREAMBLE_SIZE + HCI_CMD_READ_CHIP_TYPE_SIZE;

                    pp = (uint8_t *) (p_buf + 1);
                    for (i = 0; i < p_buf->len; i++)
                        BTVNDDBG("get chip type command data[%d]= 0x%x", i, *(pp+i));

                    is_proceeding = bt_vendor_cbacks->xmit_cb(HCI_VSC_READ_CHIP_TYPE, p_buf, hw_config_cback);
                    break;
                }
                else
                {
                    hw_cfg_cb.state = HW_CFG_START;
                    goto CFG_START;
                }
            }
            case HW_CFG_READ_CHIP_TYPE:
            {
                if(!p_evt_buf) {
                    ALOGE("%s, buffer is null", __func__);
                    is_proceeding = FALSE;
                    break;
                }
                BTVNDDBG("READ_CHIP_TYPE status = %d, length = %d", status, p_evt_buf->len);
                p = (uint8_t *)(p_evt_buf + 1) ;
                for (i = 0; i < p_evt_buf->len; i++)
                    BTVNDDBG("READ_CHIP_TYPE event data[%d]= 0x%x", i, *(p+i));
                if(status == 0)
                {
                    if(hw_cfg_cb.lmp_subversion == 0x8761)
                        hw_cfg_cb.chip_type = ((*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPFC61_8761_CHIPTYPE_OFFSET))&0x0F);
                    else
                        hw_cfg_cb.chip_type = ((*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPFC61_CHIPTYPE_OFFSET))&0x0F);
                    BTVNDDBG("READ_CHIP_TYPE hw_cfg_cb.lmp_subversion = 0x%x", hw_cfg_cb.lmp_subversion);
                    BTVNDDBG("READ_CHIP_TYPE hw_cfg_cb.hci_version = 0x%x", hw_cfg_cb.hci_version);
                    BTVNDDBG("READ_CHIP_TYPE hw_cfg_cb.hci_revision = 0x%x", hw_cfg_cb.hci_revision);
                    BTVNDDBG("READ_CHIP_TYPE hw_cfg_cb.chip_type = 0x%x", hw_cfg_cb.chip_type);
                }
                else
                {
                    is_proceeding = FALSE;
                    break;
                }
                if(check_match_state(&hw_cfg_cb, PATCH_OPTIONAL_MATCH_FLAG_CHIPTYPE) > 1)  // check if have multiple matched patch_entry by lmp_subversion,hci_version, hci_revision and chiptype
                {
                    BTVNDDBG("check_match_state(lmp_subversion:0x%04x, hci_version:%d, hci_revision:%d chip_type:%d): Multi Matched Patch\n", hw_cfg_cb.lmp_subversion, hw_cfg_cb.hci_version, hw_cfg_cb.hci_revision, hw_cfg_cb.chip_type);
                    is_proceeding = FALSE;
                    break;
                }
                hw_cfg_cb.state = HW_CFG_START;
            }
CFG_START:
            case HW_CFG_START:
            {
#ifdef BT_CHIP_PROBE_SIMULATION
                {
                    int ii;
                    memcpy(&hw_cfg_test, &hw_cfg_cb, sizeof(hw_cfg_test));
                    for(i=0;i<sizeof(bt_chip_chars)/sizeof(bt_chip_chars[0]);i++)
                    {
                        BTVNDDBG("BT_CHIP_PROBE_SIMULATION loop:%d $$$ BEGIN $$$\n", i);
                        hw_cfg_test.lmp_subversion = bt_chip_chars[i].lmp_subversion;
                        hw_cfg_test.hci_version = bt_chip_chars[i].hci_version;
                        hw_cfg_test.hci_revision = bt_chip_chars[i].hci_revision;
                        hw_cfg_test.chip_type = CHIPTYPE_NONE;
                        if(check_match_state(&hw_cfg_test, 0) > 1){
                            BTVNDDBG("check_match_state hw_cfg_test(lmp_subversion:0x%04x, hci_version:%d, hci_revision:%d chip_type:%d): Multi Matched Patch\n", hw_cfg_test.lmp_subversion, hw_cfg_test.hci_version, hw_cfg_test.hci_revision, hw_cfg_test.chip_type);
                            if(bt_chip_chars[i].chip_type != CHIPTYPE_NONE){
                                BTVNDDBG("BT_CHIP_PROBE_SIMULATION loop:%d *** Include ChipType ***\n", i);
                                hw_cfg_test.chip_type = bt_chip_chars[i].chip_type;
                                if(check_match_state(&hw_cfg_test, PATCH_OPTIONAL_MATCH_FLAG_CHIPTYPE) > 1){
                                    BTVNDDBG("check_match_state hw_cfg_test(lmp_subversion:0x%04x, hci_version:%d, hci_revision:%d chip_type:%d): Multi Matched Patch\n", hw_cfg_test.lmp_subversion, hw_cfg_test.hci_version, hw_cfg_test.hci_revision, hw_cfg_test.chip_type);
                                }else{
                                    paic_patch_file_info = get_patch_entry(&hw_cfg_test);
                                }
                            }
                        }else{
                            paic_patch_file_info = get_patch_entry(&hw_cfg_test);
                        }
                        BTVNDDBG("BT_CHIP_PROBE_SIMULATION loop:%d $$$ END $$$\n", i);
                    }
                }
#endif
                //get efuse config file and patch code file
                paic_patch_file_info = get_patch_entry(&hw_cfg_cb);


                if((paic_patch_file_info == NULL) || (paic_patch_file_info->lmp_subversion == 0))
                {
                    ALOGE("get patch entry error");
                    is_proceeding = FALSE;
                    break;
                }
                hw_cfg_cb.max_patch_size = paic_patch_file_info->max_patch_size;
                hw_cfg_cb.config_len = aic_get_bt_config(&hw_cfg_cb.config_buf, &hw_cfg_cb.baudrate, paic_patch_file_info->config_name, paic_patch_file_info->mac_offset);
                if (hw_cfg_cb.config_len == 0)
                {
                    ALOGE("Get Config file fail, just use efuse settings");
                    //hw_cfg_cb.config_len = 0;
                }
                aic_update_altsettings(paic_patch_file_info, hw_cfg_cb.config_buf, &(hw_cfg_cb.config_len));

                hw_cfg_cb.fw_len = aic_get_bt_firmware(&hw_cfg_cb.fw_buf, paic_patch_file_info->patch_name);
                if (hw_cfg_cb.fw_len < 0)
                {
                    ALOGE("Get BT firmware fail");
                    hw_cfg_cb.fw_len = 0;
                }
                else{
                    hw_cfg_cb.project_id_mask = paic_patch_file_info->project_id_mask;
                    aic_get_bt_final_patch(&hw_cfg_cb);
                }
                BTVNDDBG("Check total_len(0x%08x) max_patch_size(0x%08x)", hw_cfg_cb.total_len, hw_cfg_cb.max_patch_size);
                if (hw_cfg_cb.total_len > hw_cfg_cb.max_patch_size)
                {
                    ALOGE("total length of fw&config(0x%08x) larger than max_patch_size(0x%08x)", hw_cfg_cb.total_len, hw_cfg_cb.max_patch_size);
                    is_proceeding = FALSE;
                    break;
                }

                if ((hw_cfg_cb.total_len > 0) && hw_cfg_cb.dl_fw_flag)
                {
                    hw_cfg_cb.patch_frag_cnt = hw_cfg_cb.total_len / PATCH_DATA_FIELD_MAX_SIZE;
                    hw_cfg_cb.patch_frag_tail = hw_cfg_cb.total_len % PATCH_DATA_FIELD_MAX_SIZE;
                    if (hw_cfg_cb.patch_frag_tail)
                        hw_cfg_cb.patch_frag_cnt += 1;
                    else
                        hw_cfg_cb.patch_frag_tail = PATCH_DATA_FIELD_MAX_SIZE;
                    BTVNDDBG("patch fragment count %d, tail len %d", hw_cfg_cb.patch_frag_cnt, hw_cfg_cb.patch_frag_tail);
                }
                else
                {
                    is_proceeding = FALSE;
                    break;
                }

                if ((hw_cfg_cb.baudrate == 0) && ((hw_cfg_cb.hw_flow_cntrl & 0x80) == 0))
                {
                    BTVNDDBG("no baudrate to set and no need to set hw flow control");
                    goto DOWNLOAD_FW;
                }

                if ((hw_cfg_cb.baudrate == 0) && (hw_cfg_cb.hw_flow_cntrl & 0x80))
                {
                    BTVNDDBG("no baudrate to set but set hw flow control is needed");
                    goto SET_HW_FLCNTRL;
                }
            }
            /* fall through intentionally */
            case HW_CFG_SET_UART_BAUD_CONTROLLER:
                BTVNDDBG("bt vendor lib: set CONTROLLER UART baud 0x%x", hw_cfg_cb.baudrate);
                hw_cfg_cb.state = HW_CFG_SET_UART_BAUD_HOST;
                is_proceeding = hw_config_set_controller_baudrate(p_buf, hw_cfg_cb.baudrate);
                break;

            case HW_CFG_SET_UART_BAUD_HOST:
                /* update baud rate of host's UART port */
                BTVNDDBG("bt vendor lib: set HOST UART baud start");
                aic_speed_to_uart_speed(hw_cfg_cb.baudrate, &host_baudrate);
                BTVNDDBG("bt vendor lib: set HOST UART baud %i", host_baudrate);
                userial_vendor_set_baud(line_speed_to_userial_baud(host_baudrate));

                if((hw_cfg_cb.hw_flow_cntrl & 0x80) == 0)
                    goto DOWNLOAD_FW;

SET_HW_FLCNTRL:
            case HW_CFG_SET_UART_HW_FLOW_CONTROL:
                BTVNDDBG("Change HW flowcontrol setting");
                if(hw_cfg_cb.hw_flow_cntrl & 0x01)
                {
                    userial_vendor_set_hw_fctrl(1);
                }
                else
                {
                    userial_vendor_set_hw_fctrl(0);
                }
                ms_delay(50);
                hw_cfg_cb.state = HW_CFG_DL_FW_PATCH;
                BTVNDDBG("start donwload fw");

DOWNLOAD_FW:
            case HW_CFG_DL_FW_PATCH:
                BTVNDDBG("bt vendor lib: HW_CFG_DL_FW_PATCH status:%i, opcode:0x%x", status, opcode);

                //recv command complete event for patch code download command
                if(opcode == HCI_VSC_DOWNLOAD_FW_PATCH)
                {
                    iIndexRx = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_OFFSET + 1);
                    BTVNDDBG("bt vendor lib: HW_CFG_DL_FW_PATCH status:%i, iIndexRx:%i", status, iIndexRx);
                    hw_cfg_cb.patch_frag_idx++;

                    if(iIndexRx&0x80)
                    {
                        BTVNDDBG("vendor lib fwcfg completed");
                        free(hw_cfg_cb.total_buf);
                        hw_cfg_cb.total_len = 0;

                        bt_vendor_cbacks->dealloc(p_buf);
                        bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);

                        hw_cfg_cb.state = 0;
                        is_proceeding = TRUE;
                        h5_init_datatrans_flag = 0;
                        break;
                    }
                }

                if (hw_cfg_cb.patch_frag_idx < hw_cfg_cb.patch_frag_cnt)
                {
                    iIndexRx = hw_cfg_cb.patch_frag_idx?((hw_cfg_cb.patch_frag_idx-1)%0x7f+1):0;
                    if (hw_cfg_cb.patch_frag_idx == hw_cfg_cb.patch_frag_cnt - 1)
                    {
                        BTVNDDBG("HW_CFG_DL_FW_PATCH: send last fw fragment");
                        iIndexRx |= 0x80;
                        hw_cfg_cb.patch_frag_len = hw_cfg_cb.patch_frag_tail;
                    }
                    else
                    {
                        iIndexRx &= 0x7F;
                        hw_cfg_cb.patch_frag_len = PATCH_DATA_FIELD_MAX_SIZE;
                    }
                }

                is_proceeding = hci_download_patch_h4(p_buf, iIndexRx,
                                    hw_cfg_cb.total_buf+(hw_cfg_cb.patch_frag_idx*PATCH_DATA_FIELD_MAX_SIZE),
                                    hw_cfg_cb.patch_frag_len);
                break;

            default:
                break;
        } // switch(hw_cfg_cb.state)
    } // if (p_buf != NULL)

    /* Free the RX event buffer */
    if ((bt_vendor_cbacks) && (p_evt_buf != NULL))
        bt_vendor_cbacks->dealloc(p_evt_buf);

    if (is_proceeding == FALSE)
    {
        ALOGE("vendor lib fwcfg aborted!!!");
        if (bt_vendor_cbacks)
        {
            if (p_buf != NULL)
                bt_vendor_cbacks->dealloc(p_buf);

            bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
        }

        if(hw_cfg_cb.config_len)
        {
            free(hw_cfg_cb.config_buf);
            hw_cfg_cb.config_len = 0;
        }

        if(hw_cfg_cb.fw_len)
        {
            free(hw_cfg_cb.fw_buf);
            hw_cfg_cb.fw_len= 0;
        }

        if(hw_cfg_cb.total_len)
        {
            free(hw_cfg_cb.total_buf);
            hw_cfg_cb.total_len = 0;
        }
        hw_cfg_cb.state = 0;
    }
}

/*****************************************************************************
**   Hardware Configuration Interface Functions
*****************************************************************************/


/*******************************************************************************
**
** Function        hw_config_start
**
** Description     Kick off controller initialization process
**
** Returns         None
**
*******************************************************************************/
void hw_config_start(char transtype)
{
    memset(&hw_cfg_cb, 0, sizeof(bt_hw_cfg_cb_t));
    hw_cfg_cb.dl_fw_flag = 1;
    hw_cfg_cb.chip_type = CHIPTYPE_NONE;
    BTVNDDBG("AICBT_RELEASE_NAME: %s",AICBT_RELEASE_NAME);
    BTVNDDBG("\nAicsemi libbt-vendor_uart Version %s \n",AIC_VERSION);
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;

    BTVNDDBG("hw_config_start, transtype = 0x%x \n", transtype);
    /* Start from sending H5 INIT */
    if (bt_vendor_cbacks)
    {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
        if(p_buf)
        {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE;

            p = (uint8_t *) (p_buf + 1);

            if(transtype & AICBT_TRANS_H4) {
                p = (uint8_t *)(p_buf + 1);
                UINT16_TO_STREAM(p, HCI_READ_LMP_VERSION);
                *p++ = 0;
                p_buf->len = HCI_CMD_PREAMBLE_SIZE;

                hw_cfg_cb.state = HW_CFG_READ_LOCAL_VER;
                bt_vendor_cbacks->xmit_cb(HCI_READ_LMP_VERSION, p_buf, hw_config_cback);
            }
            else {
                UINT16_TO_STREAM(p, HCI_VSC_H5_INIT);
                *p = 0; /* parameter length */
                hw_cfg_cb.state = HW_CFG_H5_INIT;

                bt_vendor_cbacks->xmit_cb(HCI_VSC_H5_INIT, p_buf, hw_config_cback);
            }
        }
        else {
            ALOGE("%s buffer alloc fail!", __func__);
        }
    }
    else
        ALOGE("%s call back is null", __func__);
}

