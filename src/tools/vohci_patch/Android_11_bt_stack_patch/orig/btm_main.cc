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

#include <string.h>
#include "bt_target.h"
#include "bt_types.h"
#include "btm_int.h"
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
  /* All fields are cleared; nonzero fields are reinitialized in appropriate
   * function */
  memset(&btm_cb, 0, sizeof(tBTM_CB));
  btm_cb.page_queue = fixed_queue_new(SIZE_MAX);
  btm_cb.sec_pending_q = fixed_queue_new(SIZE_MAX);
  btm_cb.sec_collision_timer = alarm_new("btm.sec_collision_timer");
  btm_cb.pairing_timer = alarm_new("btm.pairing_timer");

#if defined(BTM_INITIAL_TRACE_LEVEL)
  btm_cb.trace_level = BTM_INITIAL_TRACE_LEVEL;
#else
  btm_cb.trace_level = BT_TRACE_LEVEL_NONE; /* No traces */
#endif
  /* Initialize BTM component structures */
  btm_inq_db_init(); /* Inquiry Database and Structures */
  btm_acl_init();    /* ACL Database and Structures */
  /* Security Manager Database and Structures */
  if (stack_config_get_interface()->get_pts_secure_only_mode())
    btm_sec_init(BTM_SEC_MODE_SC);
  else
    btm_sec_init(BTM_SEC_MODE_SP);
  btm_sco_init(); /* SCO Database and Structures (If included) */

  btm_cb.sec_dev_rec = list_new(osi_free);

  btm_dev_init(); /* Device Manager Structures & HCI_Reset */
}

/** This function is called to free dynamic memory and system resource allocated by btm_init */
void btm_free(void) {
  fixed_queue_free(btm_cb.page_queue, NULL);
  btm_cb.page_queue = NULL;

  fixed_queue_free(btm_cb.sec_pending_q, NULL);
  btm_cb.sec_pending_q = NULL;

  list_node_t* end = list_end(btm_cb.sec_dev_rec);
  list_node_t* node = list_begin(btm_cb.sec_dev_rec);
  while (node != end) {
    tBTM_SEC_DEV_REC* p_dev_rec = static_cast<tBTM_SEC_DEV_REC*>(list_node(node));

    // we do list_remove in, must grab next before removing
    node = list_next(node);
    wipe_secrets_and_remove(p_dev_rec);
  }

  list_free(btm_cb.sec_dev_rec);
  btm_cb.sec_dev_rec = NULL;

  alarm_free(btm_cb.sec_collision_timer);
  btm_cb.sec_collision_timer = NULL;

  alarm_free(btm_cb.pairing_timer);
  btm_cb.pairing_timer = NULL;
}
