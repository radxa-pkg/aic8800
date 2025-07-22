#include "aicwf_pcie.h"
#include "rwnx_platform.h"
#include "aicwf_debug.h"
#include "rwnx_defs.h"
#include "linux/jiffies.h"
#include "aicwf_bt_init.h"

extern int testmode;

struct bt_patch_file_name bt_patch_name[] = {
    [PRODUCT_ID_AIC8800D80-PRODUCT_ID_AIC8800D80] = {
        .fw_adid           = FW_ADID_BASE_NAME_8800D80_U02,
        .fw_patch          = FW_PATCH_BASE_NAME_8800D80_U02,
        .fw_patch_table    = FW_PATCH_TABLE_NAME_8800D80_U02,
	.bt_ext_patch      = FW_EXT_PATCH_BASE_NAME_8800D80_U02,
    },
    [PRODUCT_ID_AIC8800D80X2-PRODUCT_ID_AIC8800D80] = {
        .fw_adid           = FW_ADID_BASE_NAME_8800D80X2_U05,
        .fw_patch          = FW_PATCH_BASE_NAME_8800D80X2_U05,
        .fw_patch_table    = FW_PATCH_TABLE_NAME_8800D80X2_U05,
	.bt_ext_patch      = FW_EXT_PATCH_BASE_NAME_8800D80X2_U05,
    },
};

int aicbt_ext_patch_data_load(struct rwnx_hw *rwnx_hw, struct aicbt_patch_info_t *patch_info)
{
	int ret = 0;
	uint32_t ext_patch_nb = patch_info->ext_patch_nb;
	char ext_patch_file_name[50];
	int index = 0;
	uint32_t id = 0;
	uint32_t addr = 0;
	u8  rem = 1;
	uint32_t mem_w_add = 0;
	uint32_t mem_w_data = 0;

	if (ext_patch_nb > 0){
		if (rwnx_hw->pcidev->chip_id == PRODUCT_ID_AIC8800DC) {
			AICWFDBG(LOGDEBUG, "[0x40506004]: 0x04318000\n");
			mem_w_add = 0x40506004;
			mem_w_data = 0x04318000;
			AICWFDBG(LOGDEBUG, "%s addr:0x%x data:0x%x \n", __func__, mem_w_add, mem_w_data);
			ret = aicwf_pcie_tran(rwnx_hw->pcidev, (void*)mem_w_add, &mem_w_data, 4, AIC_TRAN_DRV2EMB, rem);            
			//ret = rwnx_send_dbg_mem_write_req(rwnx_hw, 0x40506004, 0x04318000);
			AICWFDBG(LOGDEBUG, "[0x40506004]: 0x04338000\n");
			mem_w_add = 0x40506004;
			mem_w_data = 0x04338000;
			ret = aicwf_pcie_tran(rwnx_hw->pcidev, (void*)mem_w_add, &mem_w_data, 4, AIC_TRAN_DRV2EMB, rem); 
			//ret = rwnx_send_dbg_mem_write_req(rwnx_hw, 0x40506004, 0x04338000);
		}
		if (rwnx_hw->pcidev->chip_id == PRODUCT_ID_AIC8800D80X2) {
                        AICWFDBG(LOGDEBUG, "[0x40480000]: 0x00040220\n");
                        mem_w_add = 0x40580000;
                        mem_w_data = 0x00040220;
                        AICWFDBG(LOGDEBUG, "%s addr:0x%x data:0x%x \n", __func__, mem_w_add, mem_w_data);
                        ret = aicwf_pcie_tran(rwnx_hw->pcidev, (void*)mem_w_add, &mem_w_data, 4, AIC_TRAN_DRV2EMB, rem);
                }
		for (index = 0; index < patch_info->ext_patch_nb; index++){
			id = *(patch_info->ext_patch_param + (index * 2));
			addr = *(patch_info->ext_patch_param + (index * 2) + 1); 
			memset(ext_patch_file_name, 0, sizeof(ext_patch_file_name));
			sprintf(ext_patch_file_name,"%s%d.bin",
					bt_patch_name[rwnx_hw->pcidev->chip_id - PRODUCT_ID_AIC8800D80].bt_ext_patch,
					id);
			AICWFDBG(LOGDEBUG, "%s ext_patch_file_name:%s ext_patch_id:%x ext_patch_addr:%x \r\n",
					__func__,ext_patch_file_name, id, addr);

			if (rwnx_plat_bin_fw_upload_2(rwnx_hw, addr, ext_patch_file_name)) {
				ret = -1;
				break;
			}
		}
	}
	return ret;
}

#ifndef BT_INIT_DONE_ADDR
#define BT_INIT_DONE_ADDR 0x40500150
#endif
#ifndef BT_INIT_DONE_BIT
#define BT_INIT_DONE_BIT (1<<16)
#endif
#ifndef BT_INIT_TO_NS
#define BT_INIT_TO_NS 80000000
#endif
int aicwf_bt_init(struct rwnx_hw *rwnx_hw){
    //u32 tmp = 0;
    int err, cnt;
    u32 bt_init_st;
    ktime_t time_ini, time_cur;

#ifdef CONFIG_LOAD_BT_CONF
    aicbt_parse_config(rwnx_hw);
#endif

    aicwf_pcie_tran(rwnx_hw->pcidev, (void *)BT_INIT_DONE_ADDR, &bt_init_st, 4, AIC_TRAN_EMB2DRV, 1);
    AICWFDBG(LOGINFO, "bt init %x\n", bt_init_st);
    
    if((bt_init_st & BT_INIT_DONE_BIT) == 0)
    {
        struct aicbt_patch_table *head = NULL;
        struct aicbt_patch_info_t patch_info = {
            .info_len          = 0,
            .adid_addrinf      = 0,
            .addr_adid         = 0,
            .patch_addrinf     = 0,
            .addr_patch        = 0,
            .reset_addr        = 0,
            .reset_val         = 0,
            .adid_flag_addr    = 0,
            .adid_flag         = 0,
        };
        struct bt_patch_file_name *bt_patch_file = NULL;

        AICWFDBG(LOGINFO, "%s chip_id:%d\r\n", __func__, rwnx_hw->pcidev->chip_id);
        bt_patch_file = &bt_patch_name[rwnx_hw->pcidev->chip_id- PRODUCT_ID_AIC8800D80];
        
        head = aicbt_patch_table_alloc(rwnx_hw, bt_patch_file->fw_patch_table);
        if (head == NULL){
            AICWFDBG(LOGERROR, "aicbt_patch_table_alloc fail\n");
            return -1;
        }

        patch_info.addr_adid = FW_RAM_ADID_BASE_ADDR_8800D80_U02;
        patch_info.addr_patch = FW_RAM_PATCH_BASE_ADDR_8800D80_U02;

        aicbt_patch_info_unpack(&patch_info, head);
        if(patch_info.info_len == 0) {
            AICWFDBG(LOGERROR, "%s, aicbt_patch_info_unpack fail\n", __func__);
            return -1;
        }

        AICWFDBG(LOGINFO, "addr_adid 0x%x, addr_patch 0x%x\n", patch_info.addr_adid, patch_info.addr_patch);

    	if(rwnx_plat_bin_fw_upload_2(rwnx_hw, patch_info.addr_adid, (char*)bt_patch_file->fw_adid)) {
    		return -1;
    	}

    	if(rwnx_plat_bin_fw_upload_2(rwnx_hw, patch_info.addr_patch, (char*)bt_patch_file->fw_patch)) {
    		return -1;
    	}

    	if (aicbt_ext_patch_data_load(rwnx_hw, &patch_info)) {
    		return -1;
    	}

    	if (aicbt_patch_table_load(rwnx_hw, head)) {
    		return -1;
    	}

    #if 1
        cnt = 0;
        time_ini = ktime_get_boottime();
        while((bt_init_st & BT_INIT_DONE_BIT) == 0)
        {
            err = aicwf_pcie_tran(rwnx_hw->pcidev, (void *)BT_INIT_DONE_ADDR, &bt_init_st, 4, AIC_TRAN_EMB2DRV, 1);
            cnt++;
            time_cur = ktime_get_boottime();
            if(err || (time_cur - time_ini) > BT_INIT_TO_NS)
            {
                break;
            }
        }
        AICWFDBG(LOGINFO, "bt init done: err = %d, cnt = %d, st = %x, use %lld ns\n", err, cnt, bt_init_st, (long long)(time_cur - time_ini));
    #else
        mdelay(70);
    #endif
    }

    return 0;
}
