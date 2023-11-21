/******************************************************************************
 *
 *  Copyright (C) 2019-2027 AIC Corporation
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
 *  Filename:      aic_patch_interface.c
 *
 *  Description:   Contains controller-specific functions, like
 *                      firmware patch download
 *
 ******************************************************************************/

#define LOG_TAG "bt_fwpatchcfg"

#include <utils/Log.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "bt_hci_bdroid.h"
#include "bt_vendor_aicbt.h"
#include "aic_hardware.h"
#include "aic_patch_interface.h"

//need to comment esco_parameters.h for below android9.0
#include "esco_parameters.h"
#include "userial.h"
#include "userial_vendor.h"
#include "upio.h"
#include <sys/inotify.h>
#include <pthread.h>

/******************************************************************************
**  Constants & Macros
******************************************************************************/

struct aicbt_firmware {
    size_t size;
    uint8_t *data;
};

struct aicbt_patch_table {
    char     *name;
    uint32_t type;
    uint32_t *pt_rawdata;
    //uint32_t *pt_addr;
    //uint32_t *pt_data;
    uint32_t len;
    struct aicbt_patch_table *next;
};

static struct aicbt_patch_info_tag aicbt_patch_info = {
    .bt_pfpath             = AICBT_PFILE_PATH_DFT,
    .bt_patch               = AICBT_PATCH_BASIC_NAME,
    .bt_table               = AICBT_PTABLE_BASIC_NAME,
    .patch_base             = AICBT_BT_PATCHBASE_DFT,
    .bt_mode                 = AICBT_BTMODE_DFT,
    .vendor_info            = AICBT_VNDINFO_DFT,
    .efuse_valid            = AICBT_EFUSE_VALID_DFT,
    .bt_trans_mode          = AICBT_BTTRANS_MODE_DFT,
    .uart_baud              = AICBT_UART_BAUD_DFT,
    .uart_flowctrl          = AICBT_UART_FC_DFT,
    .lpm_enable             = AICBT_LPM_ENABLE_DFT,
    .tx_pwr                 = AICBT_TX_PWR_DFT,
    .chip_ver               = 0,
};

static const char patch_name_base[] = ".bin";
static const char patch_name_u02[] = "_u02.bin";
static const char patch_name_u03[] = "_u03.bin";
static const char patch_name_u04[] = "_u04.bin";
static const char patch_name_u05[] = "_u05.bin";
static const char patch_name_u06[] = "_u06.bin";

static struct aicbt_firmware *fw_patch              = NULL;
static struct aicbt_patch_table *fw_patch_table     = NULL;
uint32_t fw_patch_position                          = 0;
uint32_t fw_current_pt_type                         = AICBT_PT_NULL;

struct aicbt_patch_info_tag * aicbt_patch_info_get(void)
{
    return (struct aicbt_patch_info_tag *)&aicbt_patch_info;
}

void aicbt_pt_data_copy(uint32_t *dst, uint32_t *src, uint8_t dpos, uint8_t d_derta, uint8_t spos, uint8_t s_derta,uint8_t len)
{
    for(uint8_t i = 0; i < len; i++) {
        *(dst + dpos + i) = *(src + spos + i);
        dpos += d_derta;
        spos += s_derta;
    }
}

void str2hex(uint8_t*Dest, uint8_t*Src, int Len)
{
    char h1,h2;
    uint8_t s1,s2;
    int i;

    for (i=0; i<Len; i++)
    {
        h1 = Src[2*i];
        h2 = Src[2*i+1];

        s1 = toupper(h1) - 0x30;
        if (s1 > 9) 
            s1 -= 7;

        s2 = toupper(h2) - 0x30;
        if (s2 > 9) 
            s2 -= 7;

        Dest[i] = s1*16 + s2;
    }
}

#define HEX_DATA_FLAG        0x21
int aicbt_conf_hex_data_convert(uint32_t *dstdata, char *p_conf_value)
{
    uint8_t hex_val[5] = {0};///1 byte hex flag: 0x, 4 byte hex data
    uint8_t len = strlen(p_conf_value);
    if(len > (2 + 8))/// flag + data > (2 + 8), err case
    {
        ALOGE("aicbt: bt patch info err  len:%x ,%s", len, p_conf_value);
        return -1;
    }
    str2hex(&hex_val[0], (uint8_t *)p_conf_value, 1);///check hex flag(0x)
    ///if hex data
    if(hex_val[0] == HEX_DATA_FLAG)
    {
        len -= 2;
        char conf_val[8] ="00000000" ;
        memcpy((uint8_t *)(conf_val + (8 - len)), (uint8_t *)(p_conf_value + 2), len);
        str2hex(&hex_val[1], (uint8_t *)conf_val, 4);///check hex flag(0x)
        *dstdata = (hex_val[1] << 24) | (hex_val[2] << 16) |( hex_val[3] << 8) |( hex_val[4] << 0);
    }
    else
        *dstdata= atoi(p_conf_value);

    return 0;
}
int aicbt_request_firmware(struct aicbt_firmware** fw, const char* fw_path, const char* fw_name)
{
    char fw_file[FW_PATH_MAX] = {0};
    struct stat st;
    int fd = -1;
    size_t fwsize = 0;
    size_t buf_size = 0;
    /* malloc aicbt_firmware*/
    *fw = malloc(sizeof(struct aicbt_firmware));
    struct aicbt_firmware* fw_tmp = *fw;
    /*link fw path and name*/
    strcpy(fw_file, fw_path);
    strcat(fw_file, fw_name);

    ALOGI("aicbt_request_firmware: %s", fw_file);

    if (stat(fw_file, &st) < 0)
    {
        ALOGE("Can't access firmware, errno:%d", errno);
        return -1;
    }

    fwsize = st.st_size;
    buf_size = fwsize;
    fw_tmp->size = fwsize;
    if ((fd = open(fw_file, O_RDONLY)) < 0)
    {
        ALOGE("Can't open firmware, errno:%d", errno);
        return -1;
    }

    if (!(fw_tmp->data = malloc(buf_size)))
    {
        ALOGE("Can't alloc memory for fw&config, errno:%d", errno);
        if (fd >= 0)
        close(fd);
        return -1;
    }

    if (read(fd, fw_tmp->data, fwsize) < (ssize_t) fwsize)
    {
        free(fw_tmp->data);
        fw_tmp->data = NULL;
        if (fd >= 0)
        close(fd);
        return -1;
    }

    if (fd >= 0)
        close(fd);

    ALOGI("aicbt_request_firmware OK");
    return buf_size;
}

void aicbt_release_firmware(struct aicbt_firmware* fw)
{
    if(fw){
        free(fw->data);
        free(fw);
        fw = NULL;
    }
}

int aicbt_fw_patch_table_free(struct aicbt_patch_table **head)
{
    struct aicbt_patch_table *p = *head, *n = NULL;
    while (p) {
        n = p->next;
        free(p->name);
        free(p->pt_rawdata);
//        free(p->pt_addr);
//        free(p->pt_data);
        free(p);
        p = n;
    }
        *head = NULL;
    return 0;
}

struct aicbt_patch_table *aicbt_fw_patch_table_alloc(const char* fw_path, const char *filename)
{
    uint8_t *rawdata = NULL, *p;
    int size;
    struct aicbt_patch_table *head = NULL, *new_t = NULL, *cur = NULL;

    struct aicbt_firmware *fw = NULL;
    int ret = aicbt_request_firmware(&fw, fw_path, filename);

    if (ret < 0) {
        ALOGE("Request %s fail\n", filename);
        goto err;
    }

    rawdata = (uint8_t *)fw->data;
    size = fw->size;

    if (size <= 0) {
        ALOGE("wrong size of firmware file\n");
        goto err;
    }

    p = rawdata;
    if (memcmp(p, AICBT_PT_TAG, sizeof(AICBT_PT_TAG) < 16 ? sizeof(AICBT_PT_TAG) : 16)) {
        ALOGE("TAG err\n");
        goto err;
    }
    p += 16;

    while (p - rawdata < size) {
        new_t = (struct aicbt_patch_table *)malloc(sizeof(struct aicbt_patch_table));
        memset(new_t, 0, sizeof(struct aicbt_patch_table));
        if (head == NULL) {
            head = new_t;
            cur  = new_t;
        } else {
            cur->next = new_t;
            cur = cur->next;
        }

        cur->name = (char *)malloc(sizeof(char) * 16);
        memcpy(cur->name, p, 16);
        p += 16;

        cur->type = *(uint32_t *)p;
        p += 4;

        cur->len = *(uint32_t *)p;
        p += 4;
		
		ALOGE("TAG:%s cur->type %x, len %d\n", cur->name, cur->type, cur->len);

        cur->pt_rawdata = (uint32_t *)malloc(sizeof(uint8_t) * cur->len * 8);
        memcpy(cur->pt_rawdata, p, cur->len * 8);

#if 0
        cur->pt_addr = (uint32_t *)malloc(sizeof(uint8_t) * cur->len * 4);
        aicbt_pt_data_copy(cur->pt_addr, (uint32_t *)p, 0, 0, 0, 1, cur->len);

        cur->pt_data = (uint32_t *)malloc(sizeof(uint8_t) * cur->len * 4);
        aicbt_pt_data_copy(cur->pt_data, (uint32_t *)p, 0, 1, 0, 1, cur->len); 
#endif

        p += cur->len * 8;
    }

    aicbt_release_firmware(fw);
    return head;

err:
    aicbt_fw_patch_table_free(&head);
    aicbt_release_firmware(fw);
    return NULL;
}

struct aicbt_firmware * aicbt_fw_patch_alloc(const char* fw_path, const char *filename)
{
    int err = 0;

    struct aicbt_firmware *fw = NULL;
    int ret = aicbt_request_firmware(&fw, fw_path, filename);

    if (ret < 0) {
        ALOGE("Request %s fail\n", filename);
        goto err;
    }

    if (fw->size <= 0) {
        ALOGE("wrong size of firmware file\n");
        goto err;
    }

    ALOGI("aicbt_fw_patch_alloc ok\n");
    return fw ;
err:
    if(fw) {
        aicbt_release_firmware(fw);
        fw  = NULL;
    }
    return NULL;
}

int aicbt_fw_patch_table_load(void)
{
    bool patch_done = true;
    ALOGI("aicbt_fw_patch_table_load:%x\n",(uint32_t)fw_patch_table);
    if(fw_patch_table){
        struct aicbt_patch_table *fw_pt = fw_patch_table;
        struct aicbt_patch_table *p = NULL;
        uint32_t len = 0;
        //uint32_t tmp_pos = fw_patch_position;
        //uint32_t *pt_addr = NULL;
        //uint32_t *pt_data = NULL;
        if((AICBT_PT_PWRON == fw_current_pt_type)){
            fw_current_pt_type = AICBT_PT_AF;
        }

        for (p = fw_pt; p->type != fw_current_pt_type || p == NULL; p = p->next) ;

        if(p != NULL) {
            len  = p->len;
            //pt_addr = p->pt_addr;
            //pt_data = p->pt_data;
            if (AICBT_PT_VER == p->type) {
                ALOGI("aicbt: bt patch version: %s\n", (char *)p->pt_rawdata);
            } else {
#if 0
                len -= tmp_pos;
                if(len > HCI_PT_MAX_LEN) {
                    len = HCI_PT_MAX_LEN;
                }
                if (AICBT_PT_BTMODE == p->type) {
                aicbt_pt_data_copy(pt_data + tmp_pos, &aicbt_patch_info.bt_mode+ tmp_pos, 0, 0, 0, 0, len);
                }
                ALOGI("aicbt: pt_addr:%08x,pt_data:%08x\n", *(pt_addr  + tmp_pos), *(pt_data + tmp_pos));
#endif
                //if(hw_aicbt_patch_table_load(len, pt_addr + tmp_pos, pt_data + tmp_pos)) {
                ALOGI("set p->name:%s \r\n", p->name);
               	if(hw_aicbt_patch_table_load(len, p->pt_rawdata)){
                    fw_patch_position += len;
                    if(fw_patch_position == p->len) {
                        p = p->next;
                        if(p) {
                            fw_current_pt_type = p->type;
                        } else {
                            ALOGE("aicbt: bt patch tabel err\n");
                            return patch_done;
                        }
                        fw_patch_position = 0;
                    }
                    patch_done = false;
                } else {
                    ALOGE("aicbt: bt patch tabel load err\n");
                }
            }
        }
        if(patch_done) {
			ALOGI("patch table setting done\r\n");
            aicbt_fw_patch_table_free(&fw_patch_table);
            fw_patch_table = NULL;
        }
    }
    return patch_done;
}

bool aicbt_fw_patch_load(void)
{
    bool patch_done = true;

    if(fw_patch){
        struct aicbt_firmware *fw_p = fw_patch;
        uint32_t tmp_pos = fw_patch_position;
        uint32_t  size = fw_p->size;
        uint8_t *dst = fw_p->data;
        uint8_t shift = HCI_PATCH_DATA_MAX_LEN;

		

        if(size == tmp_pos){
            ALOGI("aicbt: bt patch done\n");
            aicbt_release_firmware(fw_patch);
            fw_patch = NULL;
            fw_patch_position = 0;
        } else {
            if(size < (tmp_pos + HCI_PATCH_DATA_MAX_LEN)){
                shift = size - tmp_pos;
            }

            hw_aicbt_patch_load(aicbt_patch_info.patch_base+ tmp_pos, dst + tmp_pos, shift, _32BIT);
            fw_patch_position += shift;
            patch_done = false;
        }
    }
    return patch_done;
}

int aicbt_patch_alloc_init(void)
{
    /* alloc for fw patch*/
    if(fw_patch == NULL){
        fw_patch = aicbt_fw_patch_alloc(aicbt_patch_info.bt_pfpath, aicbt_patch_info.bt_patch);
        if(fw_patch == NULL){
            ALOGE("aicbt: bt patch alloc err\n");
            return -1;
        }
        fw_patch_position = 0;
    }

    /* alloc for fw patch table*/
    if(fw_patch_table == NULL){
        fw_patch_table = aicbt_fw_patch_table_alloc(aicbt_patch_info.bt_pfpath, aicbt_patch_info.bt_table);
        if(fw_patch_table == NULL){
            ALOGE("aicbt: bt patch table err\n");
            return -1;
        }
        fw_current_pt_type = AICBT_PT_TRAP;
    }

    return 0;
}

void aicbt_patch_filename_config(uint32_t chip_ver)
{
   char *post_name = (char *)&patch_name_base;
   switch(chip_ver){
       case CHIP_VER_U02:
        post_name = (char *)&patch_name_u02;
       break;
       case CHIP_VER_U03:
        post_name = (char *)&patch_name_u03;
       break;
       case CHIP_VER_U04:
        post_name = (char *)&patch_name_u04;
       break;
       default :
       break;
   }
   strcat(aicbt_patch_info.bt_patch, post_name);
   strcat(aicbt_patch_info.bt_table, post_name);
   ALOGI("aicbt : patch file name : %s , %s", (char *)aicbt_patch_info.bt_patch, (char *)aicbt_patch_info.bt_table);
}

void aicbt_patch_chipver_set(uint32_t chip_ver)
{
    aicbt_patch_info.chip_ver = chip_ver;
    ALOGI("aicbt : chip_ver: %x ", chip_ver);
    aicbt_patch_filename_config(chip_ver);
}

int aicbt_patch_info_config(char *p_conf_name, char *p_conf_value, int param)
{
    struct aicbt_patch_info_tag *info_t = (struct aicbt_patch_info_tag *)&aicbt_patch_info;
    uint32_t *info_param = NULL;
    char *str_ptr = NULL;
    bool str_type = false;
    switch(param) {
        case AICBT_P_INFO_PFPATH:{
            str_type = true;
            str_ptr = (char *)&info_t->bt_pfpath[0];
        } break;
        case AICBT_P_INFO_PATCH :{
            str_type = true;
            str_ptr = (char *)&info_t->bt_patch[0];
        } break;
        case AICBT_P_INFO_PTABLE :{
            str_type = true;
            str_ptr = (char *)&info_t->bt_table[0];
        } break;
        case AICBT_P_INFO_PBASE :{
            info_param = &info_t->patch_base;
        } break;
        case AICBT_P_INFO_BTMODE :{
            info_param = &info_t->bt_mode;
        } break;
        case AICBT_P_INFO_VNDINFO :{
            info_param = &info_t->vendor_info;
        } break;
        case AICBT_P_INFO_EFUSE :{
            info_param = &info_t->efuse_valid;
        } break;
        case AICBT_P_INFO_TRANSMOD :{
            info_param = &info_t->bt_trans_mode;
        } break;
        case AICBT_P_INFO_UARTBAUD :{
            info_param = &info_t->uart_baud;
        } break;
        case AICBT_P_INFO_UARTFC :{
            info_param = &info_t->uart_flowctrl;
        } break;
        case AICBT_P_INFO_LPMEN :{
            info_param = &info_t->lpm_enable;
        } break;
        case AICBT_P_INFO_TXPWR :{
            info_param = &info_t->tx_pwr;
        } break;
        default :
        ALOGI("aicbt: unknow bt patch info type :%s, %x", p_conf_name, param);
        return -1;
        break;
    }

    if(str_type == true){
        strcpy(str_ptr, p_conf_value);
    } else {
        if(aicbt_conf_hex_data_convert(info_param, p_conf_value)){
            ALOGI("aicbt: cfg_cb_conf hex convert err :%s , %s", p_conf_name, p_conf_value);
            return -1;
        }
    }

    ALOGI("aicbt: bt patch info :%s,  %s",p_conf_name, p_conf_value);
    return 0;
}

