#include "rwnx_main.h"
#include "rwnx_msg_tx.h"
#include "reg_access.h"
#include "aicwf_compat_8800dc.h"

#define RWNX_MAC_RF_PATCH_BASE_NAME_8800DC     "fmacfw_rf_patch_8800dc"
#define RWNX_MAC_RF_PATCH_NAME_8800DC RWNX_MAC_RF_PATCH_BASE_NAME_8800DC".bin"
#define FW_USERCONFIG_NAME_8800DC         "aic_userconfig_8800dc.txt"

int rwnx_plat_bin_fw_upload_2(struct rwnx_hw *rwnx_hw, u32 fw_addr, char *filename);
int rwnx_request_firmware_common(struct rwnx_hw *rwnx_hw,
	u32** buffer, const char *filename);
void rwnx_plat_userconfig_parsing2(char *buffer, int size);

void rwnx_release_firmware_common(u32** buffer);

uint32_t txgain_table[32] =
{
    0xA4B22189,
    0x00007825,
    0xA4B2214B,
    0x00007825,
    0xA4B2214F,
    0x00007825,
    0xA4B221D5,
    0x00007825,
    0xA4B221DC,
    0x00007825,
    0xA4B221E5,
    0x00007825,
    0xAC9221E5,
    0x00006825,
    0xAC9221EF,
    0x00006825,
    0xBC9221EE,
    0x00006825,
    0xBC9221FF,
    0x00006825,
    0xBC9221FF,
    0x00004025,
    0xB792203F,
    0x00004026,
    0xDC92203F,
    0x00004025,
    0xE692203F,
    0x00004025,
    0xFF92203F,
    0x00004035,
    0xFFFE203F,
    0x00004832
};

uint32_t rxgain_table_24g_20m[64] = {
    0x82f282d1,
    0x9591a324,
    0x80808419,
    0x000000f0,
    0x42f282d1,
    0x95923524,
    0x80808419,
    0x000000f0,
    0x22f282d1,
    0x9592c724,
    0x80808419,
    0x000000f0,
    0x02f282d1,
    0x9591a324,
    0x80808419,
    0x000000f0,
    0x06f282d1,
    0x9591a324,
    0x80808419,
    0x000000f0,
    0x0ef29ad1,
    0x9591a324,
    0x80808419,
    0x000000f0,
    0x0ef29ad3,
    0x95923524,
    0x80808419,
    0x000000f0,
    0x0ef29ad7,
    0x9595a324,
    0x80808419,
    0x000000f0,
    0x06f282d2,
    0x95911124,
    0x80808419,
    0x000000f0,
    0x06f282f4,
    0x95911124,
    0x80808419,
    0x000000f0,
    0x06f282e6,
    0x9591a324,
    0x80808419,
    0x000000f0,
    0x06f282e6,
    0x9595a324,
    0x80808419,
    0x000000f0,
    0x06f282e6,
    0x9599a324,
    0x80808419,
    0x000000f0,
    0x06f282e6,
    0x959b5924,
    0x80808419,
    0x000000f0,
    0x06f282e6,
    0x959f5924,
    0x80808419,
    0x000000f0,
    0x0ef29ae6,
    0x959f5924,
    0x80808419,
    0x000000f0
};



uint32_t rxgain_table_24g_40m[64] = {
    0x83428151,
    0x9631a328,
    0x80808419,
    0x000000f0,
    0x43428151,
    0x96323528,
    0x80808419,
    0x000000f0,
    0x23428151,
    0x9632c728,
    0x80808419,
    0x000000f0,
    0x03428151,
    0x9631a328,
    0x80808419,
    0x000000f0,
    0x07429951,
    0x9631a328,
    0x80808419,
    0x000000f0,
    0x0f42d151,
    0x9631a328,
    0x80808419,
    0x000000f0,
    0x0f42d153,
    0x96323528,
    0x80808419,
    0x000000f0,
    0x0f42d157,
    0x9635a328,
    0x80808419,
    0x000000f0,
    0x07429952,
    0x96311128,
    0x80808419,
    0x000000f0,
    0x07429974,
    0x96311128,
    0x80808419,
    0x000000f0,
    0x07429966,
    0x9631a328,
    0x80808419,
    0x000000f0,
    0x07429966,
    0x9635a328,
    0x80808419,
    0x000000f0,
    0x07429966,
    0x9639a328,
    0x80808419,
    0x000000f0,
    0x07429966,
    0x963b5928,
    0x80808419,
    0x000000f0,
    0x07429966,
    0x963f5928,
    0x80808419,
    0x000000f0,
    0x0f42d166,
    0x963f5928,
    0x80808419,
    0x000000f0
};





int aicwf_set_rf_config_8800dc(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm){
	int ret = 0;

	if ((ret = rwnx_send_txpwr_lvl_req(rwnx_hw))) {
		return -1;
	}

	if ((ret = rwnx_send_txpwr_ofst_req(rwnx_hw))) {
		return -1;
	}


	if (testmode == 0) {
		if ((ret = rwnx_send_rf_config_req(rwnx_hw, 0,	1, (u8_l *)txgain_table, 128)))
			return -1;

		if ((ret = rwnx_send_rf_config_req(rwnx_hw, 0,	0, (u8_l *)rxgain_table_24g_20m, 256)))
			return -1;

		if ((ret = rwnx_send_rf_config_req(rwnx_hw, 32,  0, (u8_l *)rxgain_table_24g_40m, 256)))
			return -1;

		if ((ret = rwnx_send_rf_calib_req(rwnx_hw, cfm))) {
			return -1;
		}
	}

	return 0 ;
}

int	rwnx_plat_userconfig_load_8800dc(struct rwnx_hw *rwnx_hw){
    int size;
    u32 *dst=NULL;
    char *filename = FW_USERCONFIG_NAME_8800DC;

    AICWFDBG(LOGINFO, "userconfig file path:%s \r\n", filename);

    /* load file */
    size = rwnx_request_firmware_common(rwnx_hw, &dst, filename);
    if (size <= 0) {
            AICWFDBG(LOGERROR, "wrong size of firmware file\n");
            dst = NULL;
            return 0;
    }

	/* Copy the file on the Embedded side */
    AICWFDBG(LOGINFO, "### Load file done: %s, size=%d\n", filename, size);

	rwnx_plat_userconfig_parsing2((char *)dst, size);

    rwnx_release_firmware_common(&dst);

    AICWFDBG(LOGINFO, "userconfig download complete\n\n");
    return 0;

}


