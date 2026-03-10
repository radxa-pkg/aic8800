#ifndef _AIC8800D80N_COMPAT_H_
#define _AIC8800D80N_COMPAT_H_

#include "aicsdio.h"


void system_config_8800d80n(struct aic_sdio_dev *rwnx_hw);

int aicwf_plat_patch_load_8800d80n(struct aic_sdio_dev *rwnx_hw);
int aicwf_plat_patch_table_load_8800d80n(struct aic_sdio_dev *rwnx_hw);

void aicwf_patch_config_8800d80n(struct aic_sdio_dev *rwnx_hw);

int aicwf_plat_rftest_load_8800d80n(struct aic_sdio_dev *rwnx_hw);
int aicwf_plat_rftest_exec_8800d80n(struct aic_sdio_dev *rwnx_hw);

int aicwf_plat_cinit_exec_8800d80n(struct aic_sdio_dev *rwnx_hw);
int aicwf_plat_calib_exec_8800d80n(struct aic_sdio_dev *rwnx_hw);

#endif

