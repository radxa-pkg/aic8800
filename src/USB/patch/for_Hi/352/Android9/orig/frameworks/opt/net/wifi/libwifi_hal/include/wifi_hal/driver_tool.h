/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ANDROID_WIFI_SYSTEM_DRIVER_TOOL_H
#define ANDROID_WIFI_SYSTEM_DRIVER_TOOL_H

namespace android {
namespace wifi_hal {

// Utilities for interacting with the driver.
class DriverTool {
 public:
  static const int kFirmwareModeSta;
  static const int kFirmwareModeAp;
  static const int kFirmwareModeP2p;

  // Change the owner of the firmware reload path to wifi:wifi if
  // firmware reload is supported.
  static bool TakeOwnershipOfFirmwareReload();

  DriverTool() = default;
  virtual ~DriverTool() = default;

  // These methods allow manipulation of the WiFi driver.
  // They all return true on success, and false otherwise.
  virtual bool LoadDriver();
  virtual bool UnloadDriver();
  virtual bool IsDriverLoaded();

  // Check if we need to invoke |ChangeFirmwareMode| to configure
  // the firmware for the provided mode.
  // |mode| is one of the kFirmwareMode* constants defined above.
  // Returns true if needed, and false otherwise.
  virtual bool IsFirmwareModeChangeNeeded(int mode);

  // Change the firmware mode.
  // |mode| is one of the kFirmwareMode* constants defined above.
  // Returns true on success, and false otherwise.
  virtual bool ChangeFirmwareMode(int mode);

};  // class DriverTool

}  // namespace wifi_hal
}  // namespace android

#endif  // ANDROID_WIFI_SYSTEM_DRIVER_TOOL_H

