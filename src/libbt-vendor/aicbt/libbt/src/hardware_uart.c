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
#define AICBT_RELEASE_NAME "2025_0515_BT_ANDROID_4.4-14"

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
**  Constants & Macros
******************************************************************************/

#define HCI_CMD_MAX_LEN                         258

#define HCI_VSC_WR_AON_PARAM_SIZE               104
#define HCI_VSC_SET_LP_LEVEL_SIZE               1
#define HCI_VSC_SET_PWR_CTRL_SLAVE_SIZE         1
#define HCI_VSC_SET_CPU_POWER_OFF_SIZE          1
#define HCI_VSC_SET_SLEEP_EN_SIZE               8
#define HCI_VSC_CLR_B4_RESET_SIZE               3

#define HCI_EVT_CMD_CMPL_STATUS_RET_BYTE        5
#define HCI_EVT_CMD_CMPL_LOCAL_BDADDR_ARRAY     6
#define HCI_EVT_CMD_CMPL_OPCODE                 3
#define LPM_CMD_PARAM_SIZE                      12

#define AON_BT_PWR_DLY1                         (1 + 5 + 1)
#define AON_BT_PWR_DLY2                         (10 + 48 + 5 + 1)
#define AON_BT_PWR_DLY3                         (10 + 48 + 8 + 5 + 1)
#define AON_BT_PWR_DLY_AON                      (10 + 48 + 8 + 5)

/******************************************************************************
**  Local type definitions
******************************************************************************/

enum {
    BT_LP_LEVEL_ACTIVE            = 0x00,//BT CORE active, CPUSYS active, VCORE active
    BT_LP_LEVEL_CLOCK_GATE1       = 0x01,//BT CORE clock gate, CPUSYS active, VCORE active
    BT_LP_LEVEL_CLOCK_GATE2       = 0x02,//BT CORE clock gate, CPUSYS clock gate, VCORE active
    BT_LP_LEVEL_CLOCK_GATE3       = 0x03,//BT CORE clock gate, CPUSYS clock gate, VCORE clock gate
    BT_LP_LEVEL_POWER_OFF1        = 0x04,//BT CORE power off, CPUSYS active, VCORE active
    BT_LP_LEVEL_POWER_OFF2        = 0x05,//BT CORE power off, CPUSYS clock gate, VCORE active
    BT_LP_LEVEL_POWER_OFF3        = 0x06,//BT CORE power off, CPUSYS power off, VCORE active
    BT_LP_LEVEL_HIBERNATE         = 0x07,//BT CORE power off, CPUSYS power off, VCORE active
    BT_LP_LEVEL_MAX               = 0x08,//
};

enum {
    AICBT_SLEEP_STATE_IDLE        = 0,
    AICBT_SLEEP_STATE_CONFIG_ING,
    AICBT_SLEEP_STATE_CONFIG_DONE,
};

struct aicbt_hci_set_sleep_en_cmd {
    ///sleep enable
    uint8_t sleep_en;
    ///external warkeup enable
    uint8_t ext_wakeup_en;
    ///reserved
    uint8_t rsvd[6];
};

typedef struct {
    /// Em save start address
    uint32_t em_save_start_addr;
    /// Em save end address
    uint32_t em_save_end_addr;
    /// Minimum time that allow power off(in hs)
    int32_t aon_min_power_off_duration;
    /// Maximum aon params
    uint16_t aon_max_nb_params;
    /// RF config const time on cpus side (in hus)
    int16_t aon_rf_config_time_cpus;
    /// RF config const time on aon side (in hus)
    int16_t aon_rf_config_time_aon;
    /// Maximun active acl link supported by aon
    uint16_t aon_max_nb_active_acl;
    /// Maximun ble activity supported by aon
    uint16_t aon_ble_activity_max;
    /// Maximum bt rxdesc field supported by aon
    uint16_t aon_max_bt_rxdesc_field;
    /// Maximum bt rxdesc field supported by aon
    uint16_t aon_max_ble_rxdesc_field;
    /// Maximum regs supported by aon
    uint16_t aon_max_nb_regs;
    /// Maximum length of ke_env supported by aon
    uint16_t aon_max_ke_env_len;
    /// Maximum elements of sch_arb_env supported by aon
    uint16_t aon_max_nb_sc_arb_elt;
    /// Maximum elements of sch_plan_env supported by aon
    uint16_t aon_max_nb_sch_plan_elt;
    /// Maximum elements of sch_alarm_elt_env supported by aon
    uint16_t aon_max_nb_sch_alarm_elt;
    /// Minimum advertising interval in slots(625 us) supported by aon
    uint32_t aon_min_ble_adv_intv;
    /// Minimum connection inverval in 2-sltos(1.25ms) supported by aon
    uint32_t aon_min_ble_con_intv;
    /// Extra sleep duration for cpus(in hs), may be negative
    int32_t aon_extra_sleep_duration_cpus;
    /// Extra sleep duration for aon(in hs), may be negative
    int32_t aon_extra_sleep_duration_aon;
    /// Minimum time that allow host to power off(in us)
    int32_t aon_min_power_off_duration_cpup;
    /// aon debug level for cpus
    uint32_t aon_debug_level;
    /// aon debug level for aon
    uint32_t aon_debug_level_aon;
    /// Power on delay of bt core on when cpus_sys alive on cpus side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly1;
    /// Power on delay of bt core on when cpus_sys clk gate on cpus side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly2;
    /// Power on delay of bt core on when cpus_sys power off on cpus side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly3;
    /// Power on delay of bt core on on aon side(in lp cycles)
    uint16_t aon_bt_pwr_on_dly_aon;
    /// Time to cancle sch arbiter elements in advance when switching to cpus(in hus)
    uint16_t aon_sch_arb_cancel_in_advance_time;
    /// Duration of sleep and wake-up algorithm (depends on CPU speed) expressed in half us on cpus side
    /// should also contain deep_sleep_on rising edge to fimecnt halt (max 4 lp cycles) and finecnt resume to dm_slp_irq(0.5 lp cycles)
    uint16_t aon_sleep_algo_dur_cpus;
    /// Duration of sleep and wake-up algorithm (depends on CPU speed) expressed in half us on aon side
    /// should also contain deep_sleep_on rising edge to fimecnt halt (max 4 lp cycles) and finecnt resume to dm_slp_irq(0.5 lp cycles)
    uint16_t aon_sleep_algo_dur_aon;
    /// Threshold that treat fractional part of restore time (in hus) as 1 hs on cpus side
    uint16_t aon_restore_time_ceil_cpus;
    /// Threshold that treat fractional part of restore time (in hus) as 1 hs on aon side
    uint16_t aon_restore_time_ceil_aon;
    /// Minmum time that allow deep sleep on cpus side (in hs)
    uint16_t aon_min_sleep_duration_cpus;
    /// Minmum time that allow deep sleep on aon side (in hs)
    uint16_t aon_min_sleep_duration_aon;
    /// Difference of resore time an save time on cpus side (in hus)
    int16_t aon_restore_save_time_diff_cpus;
    /// Difference of resore time an save time on aon side (in hus)
    int16_t aon_restore_save_time_diff_aon;
    /// Difference of restore time on aon side and save time on cpus side (in hus)
    int16_t aon_restore_save_time_diff_cpus_aon;
    /// Minimum time that allow clock gate (in hs)
    int32_t aon_min_clock_gate_duration;
    /// Minimum time that allow host to clock gate (in hus)
    int32_t aon_min_clock_gate_duration_cpup;
    // Maximum rf & md regs supported by aon
    uint16_t aon_max_nb_rf_mdm_regs;
} bt_drv_wr_aon_param;

static const bt_drv_wr_aon_param wr_aon_param = {
    0x18D700, 0x18F700, 64, 40, 400, 400, 3, 2,
    3, 2, 40, 512, 20, 21, 20, 32,
    8, -2, 0, 20000, 0x0, 0x20067302, AON_BT_PWR_DLY1, AON_BT_PWR_DLY2,
    AON_BT_PWR_DLY3, AON_BT_PWR_DLY_AON, 32, 512, 420, 100, 100, 8,
    24, 40, 140, 0, 64, 20000, 50
};

static uint8_t aicbt_lp_level = BT_LP_LEVEL_CLOCK_GATE2;

/******************************************************************************
**  Static variables
******************************************************************************/
static bt_lpm_param_t lpm_param = {
    LPM_SLEEP_MODE,
    LPM_IDLE_THRESHOLD,
    LPM_HC_IDLE_THRESHOLD,
    LPM_BT_WAKE_POLARITY,
    LPM_HOST_WAKE_POLARITY,
    LPM_ALLOW_HOST_SLEEP_DURING_SCO,
    LPM_COMBINE_SLEEP_MODE_AND_LPM,
    LPM_ENABLE_UART_TXD_TRI_STATE,
    0,  /* not applicable */
    0,  /* not applicable */
    0,  /* not applicable */
    LPM_PULSED_HOST_WAKE
};

static int local_transtype = 0;

bool hw_aicbt_fw_retention_params_config(HC_BT_HDR *p_buf);
static bool hw_aic_bt_set_lp_level(HC_BT_HDR *p_buf);
static bool hw_aic_bt_wr_aon_params(HC_BT_HDR *p_buf);
static bool hw_aic_bt_set_pwr_ctrl_slave(HC_BT_HDR *p_buf);
static bool hw_aic_bt_set_cpu_poweroff_en(HC_BT_HDR *p_buf);
static void hw_config_pre_start(void);
void hw_uart_config_start(char transtype);

void hw_uart_config_cback(void *p_evt_buf);

/******************************************************************************
**  Controller Initialization Static Functions
******************************************************************************/

/*******************************************************************************
**
** Function         hw_uart_config_cback
**
** Description      Callback function for controller configuration
**
** Returns          None
**
*******************************************************************************/
void hw_uart_config_cback(void *p_mem)
{
	HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
	char        *p_name, *p_tmp;
	uint8_t     *p, status;
	uint16_t    opcode;
	HC_BT_HDR  *p_buf=NULL;
	uint8_t     is_proceeding = FALSE;
	int         i;
	int         delay=100;
#if (USE_CONTROLLER_BDADDR == TRUE)
	const uint8_t null_bdaddr[BD_ADDR_LEN] = {0,0,0,0,0,0};
#endif

	status = *((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
	p = (uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
	STREAM_TO_UINT16(opcode,p);

	/* Ask a new buffer big enough to hold any HCI commands sent in here */
	if ((status == 0) && bt_vendor_cbacks)
		p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
				HCI_CMD_MAX_LEN);

	if (p_buf != NULL) {
		p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
		p_buf->offset = 0;
		p_buf->len = 0;
		p_buf->layer_specific = 0;

		p = (uint8_t *) (p_buf + 1);

		AICBTDBG(LOGINFO, "hw_cfg_cb.state = %d", hw_cfg_cb.state);

		switch (hw_cfg_cb.state) {
			case HW_CFG_START:
				AICBTDBG(LOGINFO, "HW_CFG_START"); 
				if((is_proceeding = hw_set_fw_log(p_buf)) == TRUE)
					break;
            case HW_CFG_SET_FW_LOG_ENABLE:
#if (USE_CONTROLLER_BDADDR == TRUE)
				if((is_proceeding = hw_config_read_bdaddr(p_buf)) == TRUE)
					break;
#else
				if((is_proceeding = hw_config_set_bdaddr(p_buf)) == TRUE)
					break;

				break;
#endif
#if (USE_CONTROLLER_BDADDR == TRUE)
			case HW_CFG_READ_BD_ADDR:
				p_tmp = (char *) (p_evt_buf + 1) + \
					HCI_EVT_CMD_CMPL_LOCAL_BDADDR_ARRAY;

				AICBTDBG(LOGINFO, "Controller OTP bdaddr %02X:%02X:%02X:%02X:%02X:%02X",
						*(p_tmp+5), *(p_tmp+4), *(p_tmp+3),
						*(p_tmp+2), *(p_tmp+1), *p_tmp);
				vnd_local_bd_addr[0] = *(p_tmp+5);
				vnd_local_bd_addr[1] = *(p_tmp+4);
				vnd_local_bd_addr[2] = *(p_tmp+3);
				vnd_local_bd_addr[3] = *(p_tmp+2);
				vnd_local_bd_addr[4] = *(p_tmp+1);
				vnd_local_bd_addr[5] = *(p_tmp);

				if ((is_proceeding = hw_config_set_bdaddr(p_buf)) == TRUE)
					break;

				is_proceeding = TRUE;
				break;
#endif // (USE_CONTROLLER_BDADDR == TRUE)
			case HW_CFG_SET_BD_ADDR:
                hw_read_local_features(p_buf);
                is_proceeding = TRUE;
                break;
			case HW_CFG_READ_LOCAL_FEATURES:
#if (BT_RETENTION == TRUE)
				if ((is_proceeding = hw_aicbt_fw_retention_params_config(p_buf)) == TRUE)
					break;
			case HW_CFG_SET_FW_RET_PARAM:
#endif
#if (PCM_SETTING == TRUE)
				if ((is_proceeding = hw_config_set_pcm_param(p_buf)) == TRUE)
					break;
			case HW_CFG_SET_PCM_PARAM:
#endif
				AICBTDBG(LOGINFO, "vendor lib fwcfg completed %d", hw_cfg_cb.state);
				bt_vendor_cbacks->dealloc(p_buf);
				bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
				hw_cfg_cb.state = 0;

				is_proceeding = TRUE;

				break;

		}
	}

	/* Free the RX event buffer */
	if (bt_vendor_cbacks)
		bt_vendor_cbacks->dealloc(p_evt_buf);

	if (is_proceeding == FALSE) {
		AICBTDBG(LOGERROR, "vendor lib fwcfg aborted!!!");
		if (bt_vendor_cbacks) {
			if (p_buf != NULL)
				bt_vendor_cbacks->dealloc(p_buf);

			bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
		}

		hw_cfg_cb.state = 0;
	}
}

/*****************************************************************************
**   Hardware Configuration Interface Functions
*****************************************************************************/


/*******************************************************************************
**
** Function        hw_uart_config_start
**
** Description     Kick off controller initialization process
**
** Returns         None
**
*******************************************************************************/
void hw_uart_config_start(char transtype)
{
    HC_BT_HDR  *p_buf = NULL;
    uint8_t     *p;
    local_transtype = transtype;

    AICBTDBG(LOGINFO, "AICBT_RELEASE_NAME: %s", AICBT_RELEASE_NAME);
    AICBTDBG(LOGINFO, "Aicsemi libbt-vendor_uart Version %s ",AIC_VERSION);
    AICBTDBG(LOGINFO, "hw_uart_config_start, transtype = 0x%x ", transtype);

    hw_cfg_cb.state = 0;
    hw_cfg_cb.f_set_baud_2 = FALSE;

    /* Start from sending H5 INIT */
    if (bt_vendor_cbacks) {
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR *) bt_vendor_cbacks->alloc(BT_HC_HDR_SIZE + \
                                                       HCI_CMD_PREAMBLE_SIZE);
    } else {
        AICBTDBG(LOGERROR, "%s call back is null", __func__);
        return;
    }

    if (p_buf) {
        p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
        p_buf->offset = 0;
        p_buf->layer_specific = 0;
        p_buf->len = HCI_CMD_PREAMBLE_SIZE;

        p = (uint8_t *) (p_buf + 1);
        if (transtype & AICBT_TRANS_H4) {
            UINT16_TO_STREAM(p, HCI_RESET);
            *p = 0; /* parameter length */
            hw_cfg_cb.state = HW_CFG_START;
            bt_vendor_cbacks->xmit_cb(HCI_RESET, p_buf, hw_uart_config_cback);
        } else {
            UINT16_TO_STREAM(p, HCI_VSC_H5_INIT);
            *p = 0; /* parameter length */
            hw_cfg_cb.state = HW_CFG_H5_INIT;
            bt_vendor_cbacks->xmit_cb(HCI_VSC_H5_INIT, p_buf, hw_uart_config_cback);
        }
    } else {
        AICBTDBG(LOGERROR, "vendor lib fw conf aborted [no buffer]");
        bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_FAIL);
    }
}

/******************************************************************************
**   LPM Static Functions
******************************************************************************/

/*******************************************************************************
**
** Function         hw_lpm_ctrl_cback
**
** Description      Callback function for lpm enable/disable rquest
**
** Returns          None
**
*******************************************************************************/
static void hw_lpm_ctrl_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    bt_vendor_op_result_t status = BT_VND_OP_RESULT_FAIL;

    if (*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE) == 0) {
        status = BT_VND_OP_RESULT_SUCCESS;
    }
    ALOGE("hw_lpm_ctrl_cback:%x,%x \n",status,*((uint8_t *)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE));

    if (bt_vendor_cbacks) {
        bt_vendor_cbacks->lpm_cb(status);
        bt_vendor_cbacks->dealloc(p_evt_buf);
    }
}

bool hw_lm_direct_return(uint8_t turn_on)
{

    if (bt_vendor_cbacks) {
        bt_vendor_cbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
    }
    return true;
}

/*******************************************************************************
**
** Function        hw_lpm_get_idle_timeout
**
** Description     Calculate idle time based on host stack idle threshold
**
** Returns         idle timeout value
**
*******************************************************************************/
uint32_t hw_lpm_get_idle_timeout(void)
{
    uint32_t timeout_ms;

    /* set idle time to be LPM_IDLE_TIMEOUT_MULTIPLE times of
     * host stack idle threshold (in 300ms/25ms)
     */
    timeout_ms = (uint32_t)lpm_param.host_stack_idle_threshold \
                            * LPM_IDLE_TIMEOUT_MULTIPLE;
    if (strstr(hw_cfg_cb.local_chip_name, "AIC8800") != NULL)
        timeout_ms *= 25; // 12.5 or 25 ?
    else if (strstr(hw_cfg_cb.local_chip_name, "AIC8800") != NULL)
        timeout_ms *= 50;
    else
        timeout_ms *= 300;

    return timeout_ms;
}

/*******************************************************************************
**
** Function        hw_lpm_set_wake_state
**
** Description     Assert/Deassert BT_WAKE
**
** Returns         None
**
*******************************************************************************/
void hw_lpm_set_wake_state(uint8_t wake_assert)
{
    uint8_t state = (wake_assert) ? UPIO_ASSERT : UPIO_DEASSERT;
    if (hw_cfg_cb.state != 0) {
        upio_set(UPIO_LPM_MODE, UPIO_ASSERT, 0);
        wake_assert = UPIO_ASSERT;
    }
    AICBTDBG(LOGINFO, "set_wake_stat %x\n",wake_assert);
    upio_set(UPIO_BT_WAKE, state, lpm_param.bt_wake_polarity);
}

void hw_set_sleep_en_cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    AICBTDBG(LOGINFO, "hw_set_sleep_en_cback\n");

    /* Free the RX event buffer */
    if (bt_vendor_cbacks)
        bt_vendor_cbacks->dealloc(p_evt_buf);

}



