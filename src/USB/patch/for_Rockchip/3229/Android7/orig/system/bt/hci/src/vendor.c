/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
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

#define LOG_TAG "bt_vendor"

#include "vendor.h"

#include <assert.h>
#include <dlfcn.h>

#include "buffer_allocator.h"
#include "bt_vendor_lib.h"
#include "bta_av_api.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"


static const char *VENDOR_LIBRARY_NAME = "libbt-vendor.so";
static const char *VENDOR_LIBRARY_SYMBOL_NAME = "BLUETOOTH_VENDOR_LIB_INTERFACE";

static const vendor_t interface;
static const allocator_t *buffer_allocator;
static const hci_t *hci;
static vendor_cb callbacks[VENDOR_LAST_OP];

static void *lib_handle;
static bt_vendor_interface_t *lib_interface;
static const bt_vendor_callbacks_t lib_callbacks;

// Interface functions

static bool vendor_open(
    const uint8_t *local_bdaddr,
    const hci_t *hci_interface) {
  assert(lib_handle == NULL);
  hci = hci_interface;

  lib_handle = dlopen(VENDOR_LIBRARY_NAME, RTLD_NOW);
  if (!lib_handle) {
    LOG_ERROR(LOG_TAG, "%s unable to open %s: %s", __func__, VENDOR_LIBRARY_NAME, dlerror());
    goto error;
  }

  LOG_ERROR(LOG_TAG, "%s open %s ok", __func__, VENDOR_LIBRARY_NAME);	

  lib_interface = (bt_vendor_interface_t *)dlsym(lib_handle, VENDOR_LIBRARY_SYMBOL_NAME);
  if (!lib_interface) {
    LOG_ERROR(LOG_TAG, "%s unable to find symbol %s in %s: %s", __func__, VENDOR_LIBRARY_SYMBOL_NAME, VENDOR_LIBRARY_NAME, dlerror());
    goto error;
  }

  LOG_INFO(LOG_TAG, "alloc value %p", lib_callbacks.alloc);

  int status = lib_interface->init(&lib_callbacks, (unsigned char *)local_bdaddr);
  if (status) {
    LOG_ERROR(LOG_TAG, "%s unable to initialize vendor library: %d", __func__, status);
    goto error;
  }

  return true;

error:;
  lib_interface = NULL;
  if (lib_handle)
    dlclose(lib_handle);
  lib_handle = NULL;
  return false;
}

static void vendor_close(void) {
  if (lib_interface)
    lib_interface->cleanup();

  if (lib_handle)
    dlclose(lib_handle);

  lib_interface = NULL;
  lib_handle = NULL;
}

static int send_command(vendor_opcode_t opcode, void *param) {
  assert(lib_interface != NULL);
  return lib_interface->op((bt_vendor_opcode_t)opcode, param);
}

static int send_async_command(vendor_async_opcode_t opcode, void *param) {
  assert(lib_interface != NULL);
  return lib_interface->op((bt_vendor_opcode_t)opcode, param);
}

static void set_callback(vendor_async_opcode_t opcode, vendor_cb callback) {
  callbacks[opcode] = callback;
}

// Internal functions

// Called back from vendor library when the firmware configuration
// completes.
static void firmware_config_cb(bt_vendor_op_result_t result) {
  LOG_INFO(LOG_TAG, "firmware callback");
  vendor_cb callback = callbacks[VENDOR_CONFIGURE_FIRMWARE];
  assert(callback != NULL);
  callback(result == BT_VND_OP_RESULT_SUCCESS);
}

// Called back from vendor library to indicate status of previous
// SCO configuration request. This should only happen during the
// postload process.
static void sco_config_cb(bt_vendor_op_result_t result) {
  LOG_INFO(LOG_TAG, "%s", __func__);
  vendor_cb callback = callbacks[VENDOR_CONFIGURE_SCO];
  assert(callback != NULL);
  callback(result == BT_VND_OP_RESULT_SUCCESS);
}

// Called back from vendor library to indicate status of previous
// LPM enable/disable request.
static void low_power_mode_cb(bt_vendor_op_result_t result) {
  LOG_INFO(LOG_TAG, "%s", __func__);
  vendor_cb callback = callbacks[VENDOR_SET_LPM_MODE];
  assert(callback != NULL);
  callback(result == BT_VND_OP_RESULT_SUCCESS);
}

/******************************************************************************
**
** Function         sco_audiostate_cb
**
** Description      HOST/CONTROLLER VENDOR LIB CALLBACK API - This function is
**                  called when the libbt-vendor completed vendor specific codec
**                  setup request
**
** Returns          None
**
******************************************************************************/
static void sco_audiostate_cb(bt_vendor_op_result_t result)
{
    uint8_t status = (result == BT_VND_OP_RESULT_SUCCESS) ? 0 : 1;

    LOG_INFO(LOG_TAG, "sco_audiostate_cb(status: %d)",status);
}

// Called by vendor library when it needs an HCI buffer.
static void *buffer_alloc_cb(int size) {
  return buffer_allocator->alloc(size);
}

// Called by vendor library when it needs to free a buffer allocated with
// |buffer_alloc_cb|.
static void buffer_free_cb(void *buffer) {
  buffer_allocator->free(buffer);
}

static void transmit_completed_callback(BT_HDR *response, void *context) {
  // Call back to the vendor library if it provided a callback to call.
  if (context) {
    ((tINT_CMD_CBACK)context)(response);
  } else {
    // HCI layer expects us to release the response.
    buffer_free_cb(response);
  }
}

// Called back from vendor library when it wants to send an HCI command.
static uint8_t transmit_cb(UNUSED_ATTR uint16_t opcode, void *buffer, tINT_CMD_CBACK callback) {
  //AIDEN LOG_ERROR(LOG_TAG, "%s", __func__);
  assert(hci != NULL);
  //AIDEN LOG_ERROR(LOG_TAG, "%s hci->transmit_command", __func__);
  hci->transmit_command((BT_HDR *)buffer, transmit_completed_callback, NULL, callback);
  return true;
}

// Called back from vendor library when the epilog procedure has
// completed. It is safe to call vendor_interface->cleanup() after
// this callback has been received.
static void epilog_cb(bt_vendor_op_result_t result) {
  LOG_INFO(LOG_TAG, "%s", __func__);
  vendor_cb callback = callbacks[VENDOR_DO_EPILOG];
  assert(callback != NULL);
  callback(result == BT_VND_OP_RESULT_SUCCESS);
}

// Called back from vendor library when the a2dp offload machine has to report status of
// an a2dp offload command.
static void a2dp_offload_cb(bt_vendor_op_result_t result, bt_vendor_opcode_t op, uint8_t bta_av_handle) {
  tBTA_AV_STATUS status = (result == BT_VND_OP_RESULT_SUCCESS) ? BTA_AV_SUCCESS : BTA_AV_FAIL_RESOURCES;

  if (op == BT_VND_OP_A2DP_OFFLOAD_START) {
      BTA_AvOffloadStartRsp(bta_av_handle, status);
  }
}

static const bt_vendor_callbacks_t lib_callbacks = {
  sizeof(lib_callbacks),
  firmware_config_cb,
  sco_config_cb,
  low_power_mode_cb,
  sco_audiostate_cb,
  buffer_alloc_cb,
  buffer_free_cb,
  transmit_cb,
  epilog_cb,
  a2dp_offload_cb
};

static const vendor_t interface = {
  vendor_open,
  vendor_close,
  send_command,
  send_async_command,
  set_callback,
};

const vendor_t *vendor_get_interface() {
  buffer_allocator = buffer_allocator_get_interface();
  return &interface;
}
