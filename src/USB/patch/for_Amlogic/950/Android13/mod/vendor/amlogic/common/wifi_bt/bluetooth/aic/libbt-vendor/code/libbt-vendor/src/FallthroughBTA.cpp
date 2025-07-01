/*
 * Copyright (C) 2017 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "bda_generation"
#include "FallthroughBTA.h"
//#include <android-base/logging.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <utils/Log.h>


using namespace android;

// global variable constructed when dlopen the library!
// this can setup the random bda before framework checkit!
FallthroughBTA gFallbackAddressSetup;

namespace android {
void FallthroughBTA::bytes_to_string(const uint8_t* addr, char* addr_str) {
  sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
          addr[3], addr[4], addr[5]);
}

bool FallthroughBTA::string_to_bytes(const char* addr_str, uint8_t* addr) {
  if (addr_str == NULL) return false;
  if (strnlen(addr_str, kStringLength) != kStringLength) return false;
  unsigned char trailing_char = '\0';

  return (sscanf(addr_str, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx%1c",
                 &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5],
                 &trailing_char) == kBytes);
}

FallthroughBTA::FallthroughBTA() {
  char property[PROPERTY_VALUE_MAX] = {0};
  uint8_t local_addr[FallthroughBTA::kBytes];

  // No factory BDADDR found. Look for a previously stored BDA.
  if (property_get(PERSIST_BDADDR_PROPERTY, property, NULL) &&
      string_to_bytes(property, local_addr)) {
        // valid, donot setup the fallback properties!
        return;
  }

  /* Generate new BDA if necessary */

    char bdstr[kStringLength + 1];

    /* No autogen BDA. Generate one now. */
    // init seeds:
    time_t now = time(NULL);
    srand(now);
    local_addr[0] = 0x22;
    local_addr[1] = 0x22;
    local_addr[2] = (uint8_t)rand();
    local_addr[3] = (uint8_t)rand();
    local_addr[4] = (uint8_t)rand();
    local_addr[5] = (uint8_t)rand();

    /* Convert to ascii, and store as a persistent property */
    bytes_to_string(local_addr, bdstr);

    ALOGE("%s: No preset BDA! Generating BDA: %s for prop %s", __func__,
          (char*)bdstr, PERSIST_BDADDR_PROPERTY);
    ALOGE("%s: This is a bug in the platform!  Please fix!", __func__);

    if (property_set(PERSIST_BDADDR_PROPERTY, (char*)bdstr) < 0) {
      ALOGE("%s: Failed to set random BDA in prop %s", __func__,
            PERSIST_BDADDR_PROPERTY);
    }
  }
}
