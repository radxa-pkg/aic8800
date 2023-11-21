/******************************************************************************
 *
 *  Copyright 2002-2012 Broadcom Corporation
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
 *  This file contains the definition of the btm control block.
 *
 ******************************************************************************/

#include <memory>
#include <string>
#include "bt_target.h"
#include "bt_types.h"
#include "main/shim/dumpsys.h"
#include "stack/btm/btm_int_types.h"
#include "stack/include/btm_client_interface.h"
#include "stack_config.h"

/* Global BTM control block structure
*/
tBTM_CB btm_cb;

/*******************************************************************************
 *
 * Function         btm_init
 *
 * Description      This function is called at BTM startup to allocate the
 *                  control block (if using dynamic memory), and initializes the
 *                  tracing level.  It then initializes the various components
 *                  of btm.
 *
 * Returns          void
 *
 ******************************************************************************/
void btm_init(void) {
  btm_cb.Init(stack_config_get_interface()->get_pts_secure_only_mode()
                  ? BTM_SEC_MODE_SC
                  : BTM_SEC_MODE_SP);
}

/** This function is called to free dynamic memory and system resource allocated by btm_init */
void btm_free(void) {
  btm_cb.Free();
}

constexpr size_t kMaxLogHistoryTagLength = 6;
constexpr size_t kMaxLogHistoryMsgLength = 25;

static void btm_log_history(const std::string& tag, const char* addr,
                            const std::string& msg, const std::string& extra) {
  btm_cb.history_->Push(
      "%-6s %-25s: %s %s", tag.substr(0, kMaxLogHistoryTagLength).c_str(),
      msg.substr(0, kMaxLogHistoryMsgLength).c_str(), addr, extra.c_str());
}

void BTM_LogHistory(const std::string& tag, const RawAddress& bd_addr,
                    const std::string& msg, const std::string& extra) {
  btm_log_history(tag, PRIVATE_ADDRESS(bd_addr), msg, extra);
}

void BTM_LogHistory(const std::string& tag, const RawAddress& bd_addr,
                    const std::string& msg) {
  BTM_LogHistory(tag, bd_addr, msg, std::string());
}

void BTM_LogHistory(const std::string& tag, const tBLE_BD_ADDR& ble_bd_addr,
                    const std::string& msg, const std::string& extra) {
  btm_log_history(tag, PRIVATE_ADDRESS(ble_bd_addr), msg, extra);
}

void BTM_LogHistory(const std::string& tag, const tBLE_BD_ADDR& ble_bd_addr,
                    const std::string& msg) {
  BTM_LogHistory(tag, ble_bd_addr, msg, std::string());
}
