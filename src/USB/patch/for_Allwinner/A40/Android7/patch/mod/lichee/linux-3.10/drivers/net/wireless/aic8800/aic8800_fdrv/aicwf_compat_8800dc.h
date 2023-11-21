#include <linux/types.h>

#define DPD_RESULT_SIZE_8800DC 1880
int aicwf_patch_table_load(struct rwnx_hw *rwnx_hw, char *filename);
void aicwf_patch_config_8800dc(struct rwnx_hw *rwnx_hw);
int aicwf_set_rf_config_8800dc(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm);
#ifdef CONFIG_DPD
int aicwf_dpd_calib_8800dc(struct rwnx_hw *rwnx_hw, uint32_t *dpd_res);
int aicwf_dpd_result_load_8800dc(struct rwnx_hw *rwnx_hw);
int aicwf_dpd_result_write_8800dc(void *buf, int buf_len);
#endif
int aicwf_plat_patch_load_8800dc(struct rwnx_hw *rwnx_hw);
int	rwnx_plat_userconfig_load_8800dc(struct rwnx_hw *rwnx_hw);
int	rwnx_plat_userconfig_load_8800dw(struct rwnx_hw *rwnx_hw);
void system_config_8800dc(struct rwnx_hw *rwnx_hw);
