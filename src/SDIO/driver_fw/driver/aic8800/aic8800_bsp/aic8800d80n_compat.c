#include "aic8800d80n_compat.h"
#include "aic_bsp_export.h"

#define RWNX_MAC_FW_RF_BASE_NAME_8800D80N   "lmacfw_rf_8800d80n.bin"
#define RWNX_MAC_FW_CINIT_NAME_8800D80N_U02 "fmacfw_cinit_8800d80n_u02.bin"
#define RWNX_MAC_FW_CALIB_NAME_8800D80N_U02 "fmacfw_calib_8800d80n_u02.bin"
#define RWNX_MAC_PATCH_NAME_8800D80N_U02    "fmacfw_patch_8800d80n_u02.bin"
#define RWNX_MAC_PATCHTBL_NAME_8800D80N_U02 "fmacfw_patch_tbl_8800d80n_u02.bin"

#define RAM_LMAC_FW_RF_ADDR_8800D80N        0x00132C00
#define ROM_FMAC_CINIT_ADDR_8800D80N_U02    0x00133000
#define ROM_FMAC_CALIB_ADDR_8800D80N_U02    0x00138000
#define ROM_FMAC_PATCH_ADDR_8800D80N_U02    0x00188000

#define CHIP_INFO_FLAG_CINIT_BEGIN          (0x01U << 12)
#define CHIP_INFO_FLAG_CINIT_DONE           (0x01U << 13)

#define PATCH_VAR_FLAG_CALIB_BEGIN          (0x01U << 0)
#define PATCH_VAR_FLAG_CALIB_DONE           (0x01U << 1)

typedef u32 (*array2_tbl_t)[2];
typedef u32 (*array3_tbl_t)[3];

u32 syscfg_tbl_masked_8800d80n[][3] = {
    // {Address, mask, value}
    {0x00000000, 0x00000000, 0x00000000}, // last one
};

u32 patch_tbl_wifisetting_8800d80n[][2] =
{
    //{0x00b8, 0x00009d08 | (0x01U << 13)}, // debug_mask, bit13: CALIB_BIT
    {0x170, 0x50000001}, //gpio_wakeup_en
};

//adap test
u32 adaptivity_patch_tbl_8800d80n[][2] = {
};

u32 patch_tbl_rf_func_8800d80n[][2] =
{
};

extern int testmode;
extern u8 chip_sub_id;
extern u8 chip_mcu_id;

int rwnx_plat_bin_fw_upload_2_with_version(struct aic_sdio_dev *rwnx_hw, u32 fw_addr,
                               char *filename, char *version_str, int version_size)
{
    int err = 0;
    unsigned int i = 0, size;
    u32 *dst = NULL;
    struct device *dev = rwnx_hw->dev;

    /* Copy the file on the Embedded side */
    AICWFDBG(LOGINFO, "### Upload %s firmware, @ = %x\n", filename, fw_addr);

    size = rwnx_load_firmware(&dst, filename, dev);
    if (!dst) {
        AICWFDBG(LOGERROR, "No such file or directory\n");
        return -1;
    }
    if (size <= 0) {
        AICWFDBG(LOGERROR, "wrong size of firmware file\n");
        dst = NULL;
        err = -1;
        return -1;
    }
    AICWFDBG(LOGINFO, "size=%d, dst[0]=%x\n", size, dst[0]);
    // get version if exist
    if (version_str) {
        char *bin_str = (char *)&dst[4];
        int char_idx = 0;
        for (char_idx = 0; char_idx < version_size; char_idx++) {
            version_str[char_idx] = bin_str[char_idx];
            if (bin_str[char_idx] == '\0') {
                //break;
            }
        }
        if (char_idx == version_size) {
            version_str[version_size - 1] = '\0';
        }
        AICWFDBG(LOGINFO, "version_str=%s\n", version_str);
    }
    // upload
    if (size > 512) {
        for (; i < (size - 512); i += 512) {
            //printk("wr blk 0: %p -> %x\r\n", dst + i / 4, fw_addr + i);
            err = rwnx_send_dbg_mem_block_write_req(rwnx_hw, fw_addr + i, 512, dst + i / 4);
            if (err) {
                AICWFDBG(LOGERROR, "bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
                break;
            }
        }
    }
    if (!err && (i < size)) {
        //printk("wr blk 1: %p -> %x\r\n", dst + i / 4, fw_addr + i);
        err = rwnx_send_dbg_mem_block_write_req(rwnx_hw, fw_addr + i, size - i, dst + i / 4);
        if (err) {
            AICWFDBG(LOGERROR, "bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
        }
    }

    if (dst) {
#ifndef CONFIG_FIRMWARE_ARRAY
        vfree(dst);
#endif
        dst = NULL;
    }

    return err;
}

void system_config_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    int syscfg_num;
    u8 chip_id = 0;
    array3_tbl_t p_syscfg_msk_tbl;
    int ret, cnt;
    const u32 mem_addr = 0x40500000;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;

    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", mem_addr, ret);
        return;
    }
    chip_id = (u8)(rd_mem_addr_cfm.memdata >> 16);
    //printk("%x=%x\n", rd_mem_addr_cfm.memaddr, rd_mem_addr_cfm.memdata);
    if (((rd_mem_addr_cfm.memdata >> 25) & 0x01UL) == 0x00UL) {
        chip_mcu_id = 1;
    }

    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, 0x00000020, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "[0x00000020] rd fail: %d\n", ret);
        return;
    }
    chip_sub_id = (u8)(rd_mem_addr_cfm.memdata);
    //printk("%x=%x\n", rd_mem_addr_cfm.memaddr, rd_mem_addr_cfm.memdata);
    AICWFDBG(LOGINFO, "chip_id=%x, chip_sub_id=%x\n", chip_id, chip_sub_id);

    syscfg_num = sizeof(syscfg_tbl_masked_8800d80n) / sizeof(u32) / 3;
    p_syscfg_msk_tbl = syscfg_tbl_masked_8800d80n;

    for (cnt = 0; cnt < syscfg_num; cnt++) {
        if (p_syscfg_msk_tbl[cnt][0] == 0x00000000) {
            break;
        }

        ret = rwnx_send_dbg_mem_mask_write_req(rwnx_hw,
            p_syscfg_msk_tbl[cnt][0], p_syscfg_msk_tbl[cnt][1], p_syscfg_msk_tbl[cnt][2]);
        if (ret) {
            AICWFDBG(LOGERROR, "%x mask write fail: %d\n", p_syscfg_msk_tbl[cnt][0], ret);
            return;
        }
    }
}

int aicwf_plat_patch_load_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    int ret = 0;
    char patch_ver_str[128];
    //wifi patch
    ret = rwnx_plat_bin_fw_upload_2_with_version(rwnx_hw,
        ROM_FMAC_PATCH_ADDR_8800D80N_U02, RWNX_MAC_PATCH_NAME_8800D80N_U02, patch_ver_str, sizeof(patch_ver_str));
    if (ret) {
        AICWFDBG(LOGERROR, "load patch bin fail: %d\n", ret);
        return ret;
    }
    AICWFDBG(LOGINFO, "PatchVer: %s", patch_ver_str);
    return ret;
}

int aicwf_plat_patch_table_load_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    int err = 0;
    unsigned int i, size;
    u32 *dst = NULL;
    char *filename = RWNX_MAC_PATCHTBL_NAME_8800D80N_U02;
    struct device *dev = rwnx_hw->dev;

    /* Copy the file on the Embedded side */
    AICWFDBG(LOGINFO, "### Upload %s \n", filename);

    size = rwnx_load_firmware(&dst, filename, dev);
    if (!dst) {
       AICWFDBG(LOGERROR, "No such file or directory\n");
       return -1;
    }
    if (size <= 0) {
        AICWFDBG(LOGERROR, "wrong size of firmware file\n");
        dst = NULL;
        err = -1;
    }
    AICWFDBG(LOGINFO, "tbl size = %d \n", size);

    if (!err) {
        for (i = 0; i < (size / 4); i += 2) {
            if ((dst[i] == 0x0) || (dst[i] == 0xFFFFFFFF)) {
                break; // end of tbl
            }
            AICWFDBG(LOGERROR, "patch_tbl:  %x  %x\n", dst[i], dst[i+1]);
            err = rwnx_send_dbg_mem_write_req(rwnx_hw, dst[i], dst[i+1]);
        }
        if (err) {
            AICWFDBG(LOGERROR, "tbl bin upload fail: %x, err:%d\r\n", dst[i], err);
        }
    }

    if (dst) {
#ifndef CONFIG_FIRMWARE_ARRAY
        vfree(dst);
#endif
        dst = NULL;
    }

   return err;

}

extern int adap_test;
void aicwf_patch_config_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    #if 1 //def CONFIG_ROM_PATCH_EN
    int ret = 0;
    int cnt = 0;

//adap test
    int adap_patch_num = 0;
//adap test

    if (testmode == 0) {
        const u32 cfg_base        = 0x10170;
        struct dbg_mem_read_cfm cfm;
        //int i;
        u32 wifisetting_cfg_addr;
        u32 agc_cfg_addr;
        u32 txgain_cfg_24g_addr, txgain_cfg_5g_addr;
        u32 jump_tbl_addr = 0;

        u32 patch_tbl_wifisetting_num = sizeof(patch_tbl_wifisetting_8800d80n)/sizeof(u32)/2;
        //u32 jump_tbl_size = 0;
        //u32 patch_tbl_func_num = 0;

        //array2_tbl_t jump_tbl_base = NULL;
        //array2_tbl_t patch_tbl_func_base = NULL;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base, &cfm))) {
            AICWFDBG(LOGERROR, "setting base[0x%x] rd fail: %d\n", cfg_base, ret);
        }
        wifisetting_cfg_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 4, &cfm))) {
             AICWFDBG(LOGERROR, "jump_tbl base[0x%x] rd fail: %d\n", cfg_base + 4, ret);
        }
        jump_tbl_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x10, &cfm))) {
            AICWFDBG(LOGERROR, "agc_cfg base[0x%x] rd fail: %d\n", cfg_base + 0xc, ret);
        }
        agc_cfg_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x14, &cfm))) {
            AICWFDBG(LOGERROR, "txgain_cfg_24g base[0x%x] rd fail: %d\n", cfg_base + 0x10, ret);
        }
        txgain_cfg_24g_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x18, &cfm))) {
            AICWFDBG(LOGERROR, "txgain_cfg_5g base[0x%x] rd fail: %d\n", cfg_base + 0x10, ret);
        }
        txgain_cfg_5g_addr = cfm.memdata;

        AICWFDBG(LOGINFO, "wifisetting_cfg_addr=%x, jump_tbl_addr=%x, agc_cfg_addr=%x, txgain_cfg_24g_addr=%x, txgain_cfg_5g_addr=%x\n",
            wifisetting_cfg_addr, jump_tbl_addr, agc_cfg_addr, txgain_cfg_24g_addr, txgain_cfg_5g_addr);

        for (cnt = 0; cnt < patch_tbl_wifisetting_num; cnt++) {
            if ((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + patch_tbl_wifisetting_8800d80n[cnt][0], patch_tbl_wifisetting_8800d80n[cnt][1]))) {
                AICWFDBG(LOGERROR, "wifisetting %x write fail\n", patch_tbl_wifisetting_8800d80n[cnt][0]);
            }
        }

        //adap test
        if (adap_test) {
            adap_patch_num = sizeof(adaptivity_patch_tbl_8800d80n)/sizeof(u32)/2;
            for(cnt = 0; cnt < adap_patch_num; cnt++)
            {
                if((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0], adaptivity_patch_tbl_8800d80n[cnt][1]))) {
                    AICWFDBG(LOGERROR, "%x write fail\n", wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0]);
                }
            }
        }
        //adap test
    }
    else {
        u32 patch_tbl_rf_func_num = sizeof(patch_tbl_rf_func_8800d80n)/sizeof(u32)/2;
        for (cnt = 0; cnt < patch_tbl_rf_func_num; cnt++) {
            if ((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, patch_tbl_rf_func_8800d80n[cnt][0], patch_tbl_rf_func_8800d80n[cnt][1]))) {
                AICWFDBG(LOGERROR, "patch_tbl_rf_func %x write fail\n", patch_tbl_rf_func_8800d80n[cnt][0]);
            }
        }
    }
    #endif
}

int aicwf_plat_rftest_load_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    int ret = 0;
    ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, RAM_LMAC_FW_RF_ADDR_8800D80N, RWNX_MAC_FW_RF_BASE_NAME_8800D80N);
    if (ret) {
        AICWFDBG(LOGINFO, "load rftest bin fail: %d\n", ret);
        return ret;
    }
    return ret;
}

int aicwf_plat_rftest_exec_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    int ret = 0;
    uint32_t fw_addr, boot_type;
    uint32_t rst_hdlr_addr = RAM_LMAC_FW_RF_ADDR_8800D80N + 0x04;
    uint32_t rst_hdlr_val;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, rst_hdlr_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", rst_hdlr_addr, ret);
        return ret;
    }
    rst_hdlr_val = rd_mem_addr_cfm.memdata;
    if ((rst_hdlr_val & ~0x03FF) == RAM_LMAC_FW_RF_ADDR_8800D80N) {
        AICWFDBG(LOGINFO, "rftest loaded, hdlr=%x\n", rst_hdlr_val);
    }
    /* fw start */
    fw_addr = RAM_LMAC_FW_RF_ADDR_8800D80N;
    boot_type = HOST_START_APP_AUTO;
    AICWFDBG(LOGINFO, "Start app: %08x, %d\n", fw_addr, boot_type);
    ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type, NULL);
    if (ret) {
        AICWFDBG(LOGERROR, "start app fail: %d\n", ret);
        return ret;
    }
    return ret;
}

int aicwf_plat_cinit_exec_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    int ret = 0;
    uint32_t fw_addr, boot_type;
    uint32_t mem_addr = 0x40500184;
    uint32_t mem_val;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", mem_addr, ret);
        return ret;
    }
    mem_val = rd_mem_addr_cfm.memdata;
    if (mem_val & CHIP_INFO_FLAG_CINIT_BEGIN) {
        // wait done
        while (!(mem_val & CHIP_INFO_FLAG_CINIT_DONE)) {
            AICWFDBG(LOGINFO, "cinit rd chipinfo=%x\n", mem_val);
            ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
            if (ret) {
                AICWFDBG(LOGERROR, "%x rd fail: %d\n", mem_addr, ret);
                return ret;
            }
            mem_val = rd_mem_addr_cfm.memdata;
        }
        AICWFDBG(LOGINFO, "cinit executed, chipinfo=%x\n", mem_val);
        return ret;
    }
    ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_CINIT_ADDR_8800D80N_U02, RWNX_MAC_FW_CINIT_NAME_8800D80N_U02);
    if (ret) {
        AICWFDBG(LOGERROR, "load cinit bin fail: %d\n", ret);
        return ret;
    }
    /* fw start */
    fw_addr = ROM_FMAC_CINIT_ADDR_8800D80N_U02 + 0x0009;
    boot_type = HOST_START_APP_FNCALL;
    AICWFDBG(LOGINFO, "Start app: %08x, %d\n", fw_addr, boot_type);
    ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type, NULL);
    if (ret) {
        AICWFDBG(LOGERROR, "start app fail: %d\n", ret);
        return ret;
    }
    return ret;
}

int aicwf_plat_calib_exec_8800d80n(struct aic_sdio_dev *rwnx_hw)
{
    int ret = 0;
    uint32_t fw_addr, boot_type;
    uint32_t patch_var_flags_addr = ROM_FMAC_PATCH_ADDR_8800D80N_U02 + 0x08;
    uint32_t patch_var_flags_val;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, patch_var_flags_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", patch_var_flags_addr, ret);
        return ret;
    }
    patch_var_flags_val = rd_mem_addr_cfm.memdata;
    if (patch_var_flags_val & (PATCH_VAR_FLAG_CALIB_BEGIN | PATCH_VAR_FLAG_CALIB_DONE)) {
        AICWFDBG(LOGINFO, "calib executed, var_flags=%x\n", patch_var_flags_val);
        return ret;
    }
    ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, ROM_FMAC_CALIB_ADDR_8800D80N_U02, RWNX_MAC_FW_CALIB_NAME_8800D80N_U02);
    if (ret) {
        AICWFDBG(LOGERROR, "load calib bin fail: %d\n", ret);
        return ret;
    }
    /* fw start */
    fw_addr = ROM_FMAC_CALIB_ADDR_8800D80N_U02 + 0x0009;
    boot_type = HOST_START_APP_FNCALL;
    AICWFDBG(LOGINFO, "Start app: %08x, %d\n", fw_addr, boot_type);
    ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type, NULL);
    if (ret) {
        AICWFDBG(LOGERROR, "start app fail: %d\n", ret);
        return ret;
    }
    return ret;
}
