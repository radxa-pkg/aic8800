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

#include <fcntl.h>

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <cutils/properties.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "hidl_return_util.h"
#include "hidl_struct_util.h"
#include "wifi_chip.h"
#include "wifi_status_util.h"

namespace {
using android::base::unique_fd;
using android::hardware::hidl_string;
using android::hardware::hidl_vec;
using android::hardware::wifi::V1_0::ChipModeId;
using android::hardware::wifi::V1_0::IfaceType;
using android::hardware::wifi::V1_0::IWifiChip;
using android::sp;

constexpr ChipModeId kInvalidModeId = UINT32_MAX;
// These mode ID's should be unique (even across combo versions). Refer to
// handleChipConfiguration() for it's usage.
// Mode ID's for V1
constexpr ChipModeId kV1StaChipModeId = 0;
constexpr ChipModeId kV1ApChipModeId = 1;
// Mode ID for V2
constexpr ChipModeId kV2ChipModeId = 2;

constexpr char kCpioMagic[] = "070701";
constexpr size_t kMaxBufferSizeBytes = 1024 * 1024;
constexpr uint32_t kMaxRingBufferFileAgeSeconds = 60 * 60;
constexpr uint32_t kMaxRingBufferFileNum = 20;
constexpr char kTombstoneFolderPath[] = "/data/vendor/tombstones/wifi/";

template <typename Iface>
void invalidateAndClear(std::vector<sp<Iface>>& ifaces, sp<Iface> iface) {
    iface->invalidate();
    ifaces.erase(std::remove(ifaces.begin(), ifaces.end(), iface),
                 ifaces.end());
}

template <typename Iface>
void invalidateAndClearAll(std::vector<sp<Iface>>& ifaces) {
    for (const auto& iface : ifaces) {
        iface->invalidate();
    }
    ifaces.clear();
}

template <typename Iface>
std::vector<hidl_string> getNames(std::vector<sp<Iface>>& ifaces) {
    std::vector<hidl_string> names;
    for (const auto& iface : ifaces) {
        names.emplace_back(iface->getName());
    }
    return names;
}

template <typename Iface>
sp<Iface> findUsingName(std::vector<sp<Iface>>& ifaces,
                        const std::string& name) {
    std::vector<hidl_string> names;
    for (const auto& iface : ifaces) {
        if (name == iface->getName()) {
            return iface;
        }
    }
    return nullptr;
}

std::string getWlan0IfaceName() {
    std::array<char, PROPERTY_VALUE_MAX> buffer;
    property_get("wifi.interface", buffer.data(), "wlan0");
    return buffer.data();
}

std::string getWlan1IfaceName() {
    std::array<char, PROPERTY_VALUE_MAX> buffer;
    property_get("wifi.concurrent.interface", buffer.data(), "wlan1");
    return buffer.data();
}

std::string getP2pIfaceName() {
    std::array<char, PROPERTY_VALUE_MAX> buffer;
    property_get("wifi.direct.interface", buffer.data(), "p2p0");

    return buffer.data();
}

// delete files that meet either conditions:
// 1. older than a predefined time in the wifi tombstone dir.
// 2. Files in excess to a predefined amount, starting from the oldest ones
bool removeOldFilesInternal() {
    time_t now = time(0);
    const time_t delete_files_before = now - kMaxRingBufferFileAgeSeconds;
    DIR* dir_dump = opendir(kTombstoneFolderPath);
    if (!dir_dump) {
        LOG(ERROR) << "Failed to open directory: " << strerror(errno);
        return false;
    }
    unique_fd dir_auto_closer(dirfd(dir_dump));
    struct dirent* dp;
    bool success = true;
    std::list<std::pair<const time_t, std::string>> valid_files;
    while ((dp = readdir(dir_dump))) {
        if (dp->d_type != DT_REG) {
            continue;
        }
        std::string cur_file_name(dp->d_name);
        struct stat cur_file_stat;
        std::string cur_file_path = kTombstoneFolderPath + cur_file_name;
        if (stat(cur_file_path.c_str(), &cur_file_stat) == -1) {
            LOG(ERROR) << "Failed to get file stat for " << cur_file_path
                       << ": " << strerror(errno);
            success = false;
            continue;
        }
        const time_t cur_file_time = cur_file_stat.st_mtime;
        valid_files.push_back(
            std::pair<const time_t, std::string>(cur_file_time, cur_file_path));
    }
    valid_files.sort();  // sort the list of files by last modified time from
                         // small to big.
    uint32_t cur_file_count = valid_files.size();
    for (auto cur_file : valid_files) {
        if (cur_file_count > kMaxRingBufferFileNum ||
            cur_file.first < delete_files_before) {
            if (unlink(cur_file.second.c_str()) != 0) {
                LOG(ERROR) << "Error deleting file " << strerror(errno);
                success = false;
            }
            cur_file_count--;
        } else {
            break;
        }
    }
    return success;
}

// Helper function for |cpioArchiveFilesInDir|
bool cpioWriteHeader(int out_fd, struct stat& st, const char* file_name,
                     size_t file_name_len) {
    std::array<char, 32 * 1024> read_buf;
    ssize_t llen =
        sprintf(read_buf.data(),
                "%s%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X",
                kCpioMagic, static_cast<int>(st.st_ino), st.st_mode, st.st_uid,
                st.st_gid, static_cast<int>(st.st_nlink),
                static_cast<int>(st.st_mtime), static_cast<int>(st.st_size),
                major(st.st_dev), minor(st.st_dev), major(st.st_rdev),
                minor(st.st_rdev), static_cast<uint32_t>(file_name_len), 0);
    if (write(out_fd, read_buf.data(), llen) == -1) {
        LOG(ERROR) << "Error writing cpio header to file " << file_name << " "
                   << strerror(errno);
        return false;
    }
    if (write(out_fd, file_name, file_name_len) == -1) {
        LOG(ERROR) << "Error writing filename to file " << file_name << " "
                   << strerror(errno);
        return false;
    }

    // NUL Pad header up to 4 multiple bytes.
    llen = (llen + file_name_len) % 4;
    if (llen != 0) {
        const uint32_t zero = 0;
        if (write(out_fd, &zero, 4 - llen) == -1) {
            LOG(ERROR) << "Error padding 0s to file " << file_name << " "
                       << strerror(errno);
            return false;
        }
    }
    return true;
}

// Helper function for |cpioArchiveFilesInDir|
size_t cpioWriteFileContent(int fd_read, int out_fd, struct stat& st) {
    // writing content of file
    std::array<char, 32 * 1024> read_buf;
    ssize_t llen = st.st_size;
    size_t n_error = 0;
    while (llen > 0) {
        ssize_t bytes_read = read(fd_read, read_buf.data(), read_buf.size());
        if (bytes_read == -1) {
            LOG(ERROR) << "Error reading file " << strerror(errno);
            return ++n_error;
        }
        llen -= bytes_read;
        if (write(out_fd, read_buf.data(), bytes_read) == -1) {
            LOG(ERROR) << "Error writing data to file " << strerror(errno);
            return ++n_error;
        }
        if (bytes_read == 0) {  // this should never happen, but just in case
                                // to unstuck from while loop
            LOG(ERROR) << "Unexpected read result for " << strerror(errno);
            n_error++;
            break;
        }
    }
    llen = st.st_size % 4;
    if (llen != 0) {
        const uint32_t zero = 0;
        if (write(out_fd, &zero, 4 - llen) == -1) {
            LOG(ERROR) << "Error padding 0s to file " << strerror(errno);
            return ++n_error;
        }
    }
    return n_error;
}

// Helper function for |cpioArchiveFilesInDir|
bool cpioWriteFileTrailer(int out_fd) {
    std::array<char, 4096> read_buf;
    read_buf.fill(0);
    if (write(out_fd, read_buf.data(),
              sprintf(read_buf.data(), "070701%040X%056X%08XTRAILER!!!", 1,
                      0x0b, 0) +
                  4) == -1) {
        LOG(ERROR) << "Error writing trailing bytes " << strerror(errno);
        return false;
    }
    return true;
}

// Archives all files in |input_dir| and writes result into |out_fd|
// Logic obtained from //external/toybox/toys/posix/cpio.c "Output cpio archive"
// portion
size_t cpioArchiveFilesInDir(int out_fd, const char* input_dir) {
    struct dirent* dp;
    size_t n_error = 0;
    DIR* dir_dump = opendir(input_dir);
    if (!dir_dump) {
        LOG(ERROR) << "Failed to open directory: " << strerror(errno);
        return ++n_error;
    }
    unique_fd dir_auto_closer(dirfd(dir_dump));
    while ((dp = readdir(dir_dump))) {
        if (dp->d_type != DT_REG) {
            continue;
        }
        std::string cur_file_name(dp->d_name);
        // string.size() does not include the null terminator. The cpio FreeBSD
        // file header expects the null character to be included in the length.
        const size_t file_name_len = cur_file_name.size() + 1;
        struct stat st;
        const std::string cur_file_path = kTombstoneFolderPath + cur_file_name;
        if (stat(cur_file_path.c_str(), &st) == -1) {
            LOG(ERROR) << "Failed to get file stat for " << cur_file_path
                       << ": " << strerror(errno);
            n_error++;
            continue;
        }
        const int fd_read = open(cur_file_path.c_str(), O_RDONLY);
        if (fd_read == -1) {
            LOG(ERROR) << "Failed to open file " << cur_file_path << " "
                       << strerror(errno);
            n_error++;
            continue;
        }
        unique_fd file_auto_closer(fd_read);
        if (!cpioWriteHeader(out_fd, st, cur_file_name.c_str(),
                             file_name_len)) {
            return ++n_error;
        }
        size_t write_error = cpioWriteFileContent(fd_read, out_fd, st);
        if (write_error) {
            return n_error + write_error;
        }
    }
    if (!cpioWriteFileTrailer(out_fd)) {
        return ++n_error;
    }
    return n_error;
}

// Helper function to create a non-const char*.
std::vector<char> makeCharVec(const std::string& str) {
    std::vector<char> vec(str.size() + 1);
    vec.assign(str.begin(), str.end());
    vec.push_back('\0');
    return vec;
}

}  // namespace

namespace android {
namespace hardware {
namespace wifi {
namespace V1_2 {
namespace implementation {
using hidl_return_util::validateAndCall;
using hidl_return_util::validateAndCallWithLock;

WifiChip::WifiChip(
    ChipId chip_id, const std::weak_ptr<legacy_hal::WifiLegacyHal> legacy_hal,
    const std::weak_ptr<mode_controller::WifiModeController> mode_controller,
    const std::weak_ptr<feature_flags::WifiFeatureFlags> feature_flags)
    : chip_id_(chip_id),
      legacy_hal_(legacy_hal),
      mode_controller_(mode_controller),
      feature_flags_(feature_flags),
      is_valid_(true),
      current_mode_id_(kInvalidModeId),
      debug_ring_buffer_cb_registered_(false) {
    populateModes();
}

void WifiChip::invalidate() {
    if (!writeRingbufferFilesInternal()) {
        LOG(ERROR) << "Error writing files to flash";
    }
    invalidateAndRemoveAllIfaces();
    legacy_hal_.reset();
    event_cb_handler_.invalidate();
    is_valid_ = false;
}

bool WifiChip::isValid() { return is_valid_; }

std::set<sp<IWifiChipEventCallback>> WifiChip::getEventCallbacks() {
    return event_cb_handler_.getCallbacks();
}

Return<void> WifiChip::getId(getId_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getIdInternal, hidl_status_cb);
}

Return<void> WifiChip::registerEventCallback(
    const sp<V1_0::IWifiChipEventCallback>& event_callback,
    registerEventCallback_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::registerEventCallbackInternal,
                           hidl_status_cb, event_callback);
}

Return<void> WifiChip::getCapabilities(getCapabilities_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getCapabilitiesInternal, hidl_status_cb);
}

Return<void> WifiChip::getAvailableModes(getAvailableModes_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getAvailableModesInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::configureChip(ChipModeId mode_id,
                                     configureChip_cb hidl_status_cb) {
    return validateAndCallWithLock(
        this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
        &WifiChip::configureChipInternal, hidl_status_cb, mode_id);
}

Return<void> WifiChip::getMode(getMode_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getModeInternal, hidl_status_cb);
}

Return<void> WifiChip::requestChipDebugInfo(
    requestChipDebugInfo_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::requestChipDebugInfoInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::requestDriverDebugDump(
    requestDriverDebugDump_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::requestDriverDebugDumpInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::requestFirmwareDebugDump(
    requestFirmwareDebugDump_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::requestFirmwareDebugDumpInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::createApIface(createApIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::createApIfaceInternal, hidl_status_cb);
}

Return<void> WifiChip::getApIfaceNames(getApIfaceNames_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getApIfaceNamesInternal, hidl_status_cb);
}

Return<void> WifiChip::getApIface(const hidl_string& ifname,
                                  getApIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getApIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::removeApIface(const hidl_string& ifname,
                                     removeApIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::removeApIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::createNanIface(createNanIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::createNanIfaceInternal, hidl_status_cb);
}

Return<void> WifiChip::getNanIfaceNames(getNanIfaceNames_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getNanIfaceNamesInternal, hidl_status_cb);
}

Return<void> WifiChip::getNanIface(const hidl_string& ifname,
                                   getNanIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getNanIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::removeNanIface(const hidl_string& ifname,
                                      removeNanIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::removeNanIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::createP2pIface(createP2pIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::createP2pIfaceInternal, hidl_status_cb);
}

Return<void> WifiChip::getP2pIfaceNames(getP2pIfaceNames_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getP2pIfaceNamesInternal, hidl_status_cb);
}

Return<void> WifiChip::getP2pIface(const hidl_string& ifname,
                                   getP2pIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getP2pIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::removeP2pIface(const hidl_string& ifname,
                                      removeP2pIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::removeP2pIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::createStaIface(createStaIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::createStaIfaceInternal, hidl_status_cb);
}

Return<void> WifiChip::getStaIfaceNames(getStaIfaceNames_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getStaIfaceNamesInternal, hidl_status_cb);
}

Return<void> WifiChip::getStaIface(const hidl_string& ifname,
                                   getStaIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getStaIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::removeStaIface(const hidl_string& ifname,
                                      removeStaIface_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::removeStaIfaceInternal, hidl_status_cb,
                           ifname);
}

Return<void> WifiChip::createRttController(
    const sp<IWifiIface>& bound_iface, createRttController_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::createRttControllerInternal,
                           hidl_status_cb, bound_iface);
}

Return<void> WifiChip::getDebugRingBuffersStatus(
    getDebugRingBuffersStatus_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getDebugRingBuffersStatusInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::startLoggingToDebugRingBuffer(
    const hidl_string& ring_name, WifiDebugRingBufferVerboseLevel verbose_level,
    uint32_t max_interval_in_sec, uint32_t min_data_size_in_bytes,
    startLoggingToDebugRingBuffer_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::startLoggingToDebugRingBufferInternal,
                           hidl_status_cb, ring_name, verbose_level,
                           max_interval_in_sec, min_data_size_in_bytes);
}

Return<void> WifiChip::forceDumpToDebugRingBuffer(
    const hidl_string& ring_name,
    forceDumpToDebugRingBuffer_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::forceDumpToDebugRingBufferInternal,
                           hidl_status_cb, ring_name);
}

Return<void> WifiChip::stopLoggingToDebugRingBuffer(
    stopLoggingToDebugRingBuffer_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::stopLoggingToDebugRingBufferInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::getDebugHostWakeReasonStats(
    getDebugHostWakeReasonStats_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::getDebugHostWakeReasonStatsInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::enableDebugErrorAlerts(
    bool enable, enableDebugErrorAlerts_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::enableDebugErrorAlertsInternal,
                           hidl_status_cb, enable);
}

Return<void> WifiChip::selectTxPowerScenario(
    V1_1::IWifiChip::TxPowerScenario scenario, selectTxPowerScenario_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::selectTxPowerScenarioInternal,
                           hidl_status_cb, scenario);
}

Return<void> WifiChip::resetTxPowerScenario(
    resetTxPowerScenario_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::resetTxPowerScenarioInternal,
                           hidl_status_cb);
}

Return<void> WifiChip::registerEventCallback_1_2(
    const sp<IWifiChipEventCallback>& event_callback,
    registerEventCallback_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
                           &WifiChip::registerEventCallbackInternal_1_2,
                           hidl_status_cb, event_callback);
}

Return<void> WifiChip::selectTxPowerScenario_1_2(
        TxPowerScenario scenario, selectTxPowerScenario_cb hidl_status_cb) {
    return validateAndCall(this, WifiStatusCode::ERROR_WIFI_CHIP_INVALID,
            &WifiChip::selectTxPowerScenarioInternal_1_2, hidl_status_cb, scenario);
}

Return<void> WifiChip::debug(const hidl_handle& handle,
                             const hidl_vec<hidl_string>&) {
    if (handle != nullptr && handle->numFds >= 1) {
        int fd = handle->data[0];
        if (!writeRingbufferFilesInternal()) {
            LOG(ERROR) << "Error writing files to flash";
        }
        uint32_t n_error = cpioArchiveFilesInDir(fd, kTombstoneFolderPath);
        if (n_error != 0) {
            LOG(ERROR) << n_error << " errors occured in cpio function";
        }
        fsync(fd);
    } else {
        LOG(ERROR) << "File handle error";
    }
    return Void();
}

void WifiChip::invalidateAndRemoveAllIfaces() {
    invalidateAndClearAll(ap_ifaces_);
    invalidateAndClearAll(nan_ifaces_);
    invalidateAndClearAll(p2p_ifaces_);
    invalidateAndClearAll(sta_ifaces_);
    // Since all the ifaces are invalid now, all RTT controller objects
    // using those ifaces also need to be invalidated.
    for (const auto& rtt : rtt_controllers_) {
        rtt->invalidate();
    }
    rtt_controllers_.clear();
}

std::pair<WifiStatus, ChipId> WifiChip::getIdInternal() {
    return {createWifiStatus(WifiStatusCode::SUCCESS), chip_id_};
}

WifiStatus WifiChip::registerEventCallbackInternal(
    const sp<V1_0::IWifiChipEventCallback>& /* event_callback */) {
    // Deprecated support for this callback.
    return createWifiStatus(WifiStatusCode::ERROR_NOT_SUPPORTED);
}

std::pair<WifiStatus, uint32_t> WifiChip::getCapabilitiesInternal() {
    legacy_hal::wifi_error legacy_status;
    uint32_t legacy_feature_set;
    uint32_t legacy_logger_feature_set;
    std::tie(legacy_status, legacy_feature_set) =
        legacy_hal_.lock()->getSupportedFeatureSet(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        return {createWifiStatusFromLegacyError(legacy_status), 0};
    }
    std::tie(legacy_status, legacy_logger_feature_set) =
        legacy_hal_.lock()->getLoggerSupportedFeatureSet(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        // some devices don't support querying logger feature set
        legacy_logger_feature_set = 0;
    }
    uint32_t hidl_caps;
    if (!hidl_struct_util::convertLegacyFeaturesToHidlChipCapabilities(
            legacy_feature_set, legacy_logger_feature_set, &hidl_caps)) {
        return {createWifiStatus(WifiStatusCode::ERROR_UNKNOWN), 0};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), hidl_caps};
}

std::pair<WifiStatus, std::vector<IWifiChip::ChipMode>>
WifiChip::getAvailableModesInternal() {
    return {createWifiStatus(WifiStatusCode::SUCCESS), modes_};
}

WifiStatus WifiChip::configureChipInternal(
    /* NONNULL */ std::unique_lock<std::recursive_mutex>* lock,
    ChipModeId mode_id) {
    if (!isValidModeId(mode_id)) {
        return createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS);
    }
    if (mode_id == current_mode_id_) {
        LOG(DEBUG) << "Already in the specified mode " << mode_id;
        return createWifiStatus(WifiStatusCode::SUCCESS);
    }
    WifiStatus status = handleChipConfiguration(lock, mode_id);
    if (status.code != WifiStatusCode::SUCCESS) {
        for (const auto& callback : event_cb_handler_.getCallbacks()) {
            if (!callback->onChipReconfigureFailure(status).isOk()) {
                LOG(ERROR)
                    << "Failed to invoke onChipReconfigureFailure callback";
            }
        }
        return status;
    }
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onChipReconfigured(mode_id).isOk()) {
            LOG(ERROR) << "Failed to invoke onChipReconfigured callback";
        }
    }
    current_mode_id_ = mode_id;
    LOG(INFO) << "Configured chip in mode " << mode_id;
    return status;
}

std::pair<WifiStatus, uint32_t> WifiChip::getModeInternal() {
    if (!isValidModeId(current_mode_id_)) {
        return {createWifiStatus(WifiStatusCode::ERROR_NOT_AVAILABLE),
                current_mode_id_};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), current_mode_id_};
}

std::pair<WifiStatus, IWifiChip::ChipDebugInfo>
WifiChip::requestChipDebugInfoInternal() {
    IWifiChip::ChipDebugInfo result;
    legacy_hal::wifi_error legacy_status;
    std::string driver_desc;
    std::tie(legacy_status, driver_desc) =
        legacy_hal_.lock()->getDriverVersion(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        LOG(ERROR) << "Failed to get driver version: "
                   << legacyErrorToString(legacy_status);
        WifiStatus status = createWifiStatusFromLegacyError(
            legacy_status, "failed to get driver version");
        return {status, result};
    }
    result.driverDescription = driver_desc.c_str();

    std::string firmware_desc;
    std::tie(legacy_status, firmware_desc) =
        legacy_hal_.lock()->getFirmwareVersion(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        LOG(ERROR) << "Failed to get firmware version: "
                   << legacyErrorToString(legacy_status);
        WifiStatus status = createWifiStatusFromLegacyError(
            legacy_status, "failed to get firmware version");
        return {status, result};
    }
    result.firmwareDescription = firmware_desc.c_str();

    return {createWifiStatus(WifiStatusCode::SUCCESS), result};
}

std::pair<WifiStatus, std::vector<uint8_t>>
WifiChip::requestDriverDebugDumpInternal() {
    legacy_hal::wifi_error legacy_status;
    std::vector<uint8_t> driver_dump;
    std::tie(legacy_status, driver_dump) =
        legacy_hal_.lock()->requestDriverMemoryDump(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        LOG(ERROR) << "Failed to get driver debug dump: "
                   << legacyErrorToString(legacy_status);
        return {createWifiStatusFromLegacyError(legacy_status),
                std::vector<uint8_t>()};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), driver_dump};
}

std::pair<WifiStatus, std::vector<uint8_t>>
WifiChip::requestFirmwareDebugDumpInternal() {
    legacy_hal::wifi_error legacy_status;
    std::vector<uint8_t> firmware_dump;
    std::tie(legacy_status, firmware_dump) =
        legacy_hal_.lock()->requestFirmwareMemoryDump(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        LOG(ERROR) << "Failed to get firmware debug dump: "
                   << legacyErrorToString(legacy_status);
        return {createWifiStatusFromLegacyError(legacy_status), {}};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), firmware_dump};
}

std::pair<WifiStatus, sp<IWifiApIface>> WifiChip::createApIfaceInternal() {
    if (!canCurrentModeSupportIfaceOfType(IfaceType::AP)) {
        return {createWifiStatus(WifiStatusCode::ERROR_NOT_AVAILABLE), {}};
    }
    std::string ifname = allocateApOrStaIfaceName();
    sp<WifiApIface> iface = new WifiApIface(ifname, legacy_hal_);
    ap_ifaces_.push_back(iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceAdded(IfaceType::AP, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceAdded callback";
        }
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

std::pair<WifiStatus, std::vector<hidl_string>>
WifiChip::getApIfaceNamesInternal() {
    if (ap_ifaces_.empty()) {
        return {createWifiStatus(WifiStatusCode::SUCCESS), {}};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), getNames(ap_ifaces_)};
}

std::pair<WifiStatus, sp<IWifiApIface>> WifiChip::getApIfaceInternal(
    const std::string& ifname) {
    const auto iface = findUsingName(ap_ifaces_, ifname);
    if (!iface.get()) {
        return {createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS), nullptr};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

WifiStatus WifiChip::removeApIfaceInternal(const std::string& ifname) {
    const auto iface = findUsingName(ap_ifaces_, ifname);
    if (!iface.get()) {
        return createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS);
    }
    invalidateAndClear(ap_ifaces_, iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceRemoved(IfaceType::AP, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceRemoved callback";
        }
    }
    return createWifiStatus(WifiStatusCode::SUCCESS);
}

std::pair<WifiStatus, sp<IWifiNanIface>> WifiChip::createNanIfaceInternal() {
    if (!canCurrentModeSupportIfaceOfType(IfaceType::NAN)) {
        return {createWifiStatus(WifiStatusCode::ERROR_NOT_AVAILABLE), {}};
    }
    // These are still assumed to be based on wlan0.
    std::string ifname = getWlan0IfaceName();
    sp<WifiNanIface> iface = new WifiNanIface(ifname, legacy_hal_);
    nan_ifaces_.push_back(iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceAdded(IfaceType::NAN, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceAdded callback";
        }
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

std::pair<WifiStatus, std::vector<hidl_string>>
WifiChip::getNanIfaceNamesInternal() {
    if (nan_ifaces_.empty()) {
        return {createWifiStatus(WifiStatusCode::SUCCESS), {}};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), getNames(nan_ifaces_)};
}

std::pair<WifiStatus, sp<IWifiNanIface>> WifiChip::getNanIfaceInternal(
    const std::string& ifname) {
    const auto iface = findUsingName(nan_ifaces_, ifname);
    if (!iface.get()) {
        return {createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS), nullptr};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

WifiStatus WifiChip::removeNanIfaceInternal(const std::string& ifname) {
    const auto iface = findUsingName(nan_ifaces_, ifname);
    if (!iface.get()) {
        return createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS);
    }
    invalidateAndClear(nan_ifaces_, iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceRemoved(IfaceType::NAN, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceAdded callback";
        }
    }
    return createWifiStatus(WifiStatusCode::SUCCESS);
}

std::pair<WifiStatus, sp<IWifiP2pIface>> WifiChip::createP2pIfaceInternal() {
    if (!canCurrentModeSupportIfaceOfType(IfaceType::P2P)) {
        return {createWifiStatus(WifiStatusCode::ERROR_NOT_AVAILABLE), {}};
    }
    std::string ifname = getP2pIfaceName();
    sp<WifiP2pIface> iface = new WifiP2pIface(ifname, legacy_hal_);
    p2p_ifaces_.push_back(iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceAdded(IfaceType::P2P, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceAdded callback";
        }
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

std::pair<WifiStatus, std::vector<hidl_string>>
WifiChip::getP2pIfaceNamesInternal() {
    if (p2p_ifaces_.empty()) {
        return {createWifiStatus(WifiStatusCode::SUCCESS), {}};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), getNames(p2p_ifaces_)};
}

std::pair<WifiStatus, sp<IWifiP2pIface>> WifiChip::getP2pIfaceInternal(
    const std::string& ifname) {
    const auto iface = findUsingName(p2p_ifaces_, ifname);
    if (!iface.get()) {
        return {createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS), nullptr};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

WifiStatus WifiChip::removeP2pIfaceInternal(const std::string& ifname) {
    const auto iface = findUsingName(p2p_ifaces_, ifname);
    if (!iface.get()) {
        return createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS);
    }
    invalidateAndClear(p2p_ifaces_, iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceRemoved(IfaceType::P2P, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceRemoved callback";
        }
    }
    return createWifiStatus(WifiStatusCode::SUCCESS);
}

std::pair<WifiStatus, sp<IWifiStaIface>> WifiChip::createStaIfaceInternal() {
    if (!canCurrentModeSupportIfaceOfType(IfaceType::STA)) {
        return {createWifiStatus(WifiStatusCode::ERROR_NOT_AVAILABLE), {}};
    }
    std::string ifname = allocateApOrStaIfaceName();
    sp<WifiStaIface> iface = new WifiStaIface(ifname, legacy_hal_);
    sta_ifaces_.push_back(iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceAdded(IfaceType::STA, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceAdded callback";
        }
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

std::pair<WifiStatus, std::vector<hidl_string>>
WifiChip::getStaIfaceNamesInternal() {
    if (sta_ifaces_.empty()) {
        return {createWifiStatus(WifiStatusCode::SUCCESS), {}};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), getNames(sta_ifaces_)};
}

std::pair<WifiStatus, sp<IWifiStaIface>> WifiChip::getStaIfaceInternal(
    const std::string& ifname) {
    const auto iface = findUsingName(sta_ifaces_, ifname);
    if (!iface.get()) {
        return {createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS), nullptr};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), iface};
}

WifiStatus WifiChip::removeStaIfaceInternal(const std::string& ifname) {
    const auto iface = findUsingName(sta_ifaces_, ifname);
    if (!iface.get()) {
        return createWifiStatus(WifiStatusCode::ERROR_INVALID_ARGS);
    }
    invalidateAndClear(sta_ifaces_, iface);
    for (const auto& callback : event_cb_handler_.getCallbacks()) {
        if (!callback->onIfaceRemoved(IfaceType::STA, ifname).isOk()) {
            LOG(ERROR) << "Failed to invoke onIfaceRemoved callback";
        }
    }
    return createWifiStatus(WifiStatusCode::SUCCESS);
}

std::pair<WifiStatus, sp<IWifiRttController>>
WifiChip::createRttControllerInternal(const sp<IWifiIface>& bound_iface) {
    sp<WifiRttController> rtt =
        new WifiRttController(getWlan0IfaceName(), bound_iface, legacy_hal_);
    rtt_controllers_.emplace_back(rtt);
    return {createWifiStatus(WifiStatusCode::SUCCESS), rtt};
}

std::pair<WifiStatus, std::vector<WifiDebugRingBufferStatus>>
WifiChip::getDebugRingBuffersStatusInternal() {
    legacy_hal::wifi_error legacy_status;
    std::vector<legacy_hal::wifi_ring_buffer_status>
        legacy_ring_buffer_status_vec;
    std::tie(legacy_status, legacy_ring_buffer_status_vec) =
        legacy_hal_.lock()->getRingBuffersStatus(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        return {createWifiStatusFromLegacyError(legacy_status), {}};
    }
    std::vector<WifiDebugRingBufferStatus> hidl_ring_buffer_status_vec;
    if (!hidl_struct_util::convertLegacyVectorOfDebugRingBufferStatusToHidl(
            legacy_ring_buffer_status_vec, &hidl_ring_buffer_status_vec)) {
        return {createWifiStatus(WifiStatusCode::ERROR_UNKNOWN), {}};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS),
            hidl_ring_buffer_status_vec};
}

WifiStatus WifiChip::startLoggingToDebugRingBufferInternal(
    const hidl_string& ring_name, WifiDebugRingBufferVerboseLevel verbose_level,
    uint32_t max_interval_in_sec, uint32_t min_data_size_in_bytes) {
    WifiStatus status = registerDebugRingBufferCallback();
    if (status.code != WifiStatusCode::SUCCESS) {
        return status;
    }
    legacy_hal::wifi_error legacy_status =
        legacy_hal_.lock()->startRingBufferLogging(
            getWlan0IfaceName(), ring_name,
            static_cast<
                std::underlying_type<WifiDebugRingBufferVerboseLevel>::type>(
                verbose_level),
            max_interval_in_sec, min_data_size_in_bytes);
    ringbuffer_map_.insert(std::pair<std::string, Ringbuffer>(
        ring_name, Ringbuffer(kMaxBufferSizeBytes)));
    return createWifiStatusFromLegacyError(legacy_status);
}

WifiStatus WifiChip::forceDumpToDebugRingBufferInternal(
    const hidl_string& ring_name) {
    WifiStatus status = registerDebugRingBufferCallback();
    if (status.code != WifiStatusCode::SUCCESS) {
        return status;
    }
    legacy_hal::wifi_error legacy_status =
        legacy_hal_.lock()->getRingBufferData(getWlan0IfaceName(), ring_name);

    return createWifiStatusFromLegacyError(legacy_status);
}

WifiStatus WifiChip::stopLoggingToDebugRingBufferInternal() {
    legacy_hal::wifi_error legacy_status =
        legacy_hal_.lock()->deregisterRingBufferCallbackHandler(
            getWlan0IfaceName());
    return createWifiStatusFromLegacyError(legacy_status);
}

std::pair<WifiStatus, WifiDebugHostWakeReasonStats>
WifiChip::getDebugHostWakeReasonStatsInternal() {
    legacy_hal::wifi_error legacy_status;
    legacy_hal::WakeReasonStats legacy_stats;
    std::tie(legacy_status, legacy_stats) =
        legacy_hal_.lock()->getWakeReasonStats(getWlan0IfaceName());
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        return {createWifiStatusFromLegacyError(legacy_status), {}};
    }
    WifiDebugHostWakeReasonStats hidl_stats;
    if (!hidl_struct_util::convertLegacyWakeReasonStatsToHidl(legacy_stats,
                                                              &hidl_stats)) {
        return {createWifiStatus(WifiStatusCode::ERROR_UNKNOWN), {}};
    }
    return {createWifiStatus(WifiStatusCode::SUCCESS), hidl_stats};
}

WifiStatus WifiChip::enableDebugErrorAlertsInternal(bool enable) {
    legacy_hal::wifi_error legacy_status;
    if (enable) {
        android::wp<WifiChip> weak_ptr_this(this);
        const auto& on_alert_callback = [weak_ptr_this](
                                            int32_t error_code,
                                            std::vector<uint8_t> debug_data) {
            const auto shared_ptr_this = weak_ptr_this.promote();
            if (!shared_ptr_this.get() || !shared_ptr_this->isValid()) {
                LOG(ERROR) << "Callback invoked on an invalid object";
                return;
            }
            for (const auto& callback : shared_ptr_this->getEventCallbacks()) {
                if (!callback->onDebugErrorAlert(error_code, debug_data)
                         .isOk()) {
                    LOG(ERROR) << "Failed to invoke onDebugErrorAlert callback";
                }
            }
        };
        legacy_status = legacy_hal_.lock()->registerErrorAlertCallbackHandler(
            getWlan0IfaceName(), on_alert_callback);
    } else {
        legacy_status = legacy_hal_.lock()->deregisterErrorAlertCallbackHandler(
            getWlan0IfaceName());
    }
    return createWifiStatusFromLegacyError(legacy_status);
}

WifiStatus WifiChip::selectTxPowerScenarioInternal(
        V1_1::IWifiChip::TxPowerScenario scenario) {
    auto legacy_status = legacy_hal_.lock()->selectTxPowerScenario(
        getWlan0IfaceName(),
        hidl_struct_util::convertHidlTxPowerScenarioToLegacy(scenario));
    return createWifiStatusFromLegacyError(legacy_status);
}

WifiStatus WifiChip::resetTxPowerScenarioInternal() {
    auto legacy_status =
        legacy_hal_.lock()->resetTxPowerScenario(getWlan0IfaceName());
    return createWifiStatusFromLegacyError(legacy_status);
}

WifiStatus WifiChip::registerEventCallbackInternal_1_2(
    const sp<IWifiChipEventCallback>& event_callback) {
    if (!event_cb_handler_.addCallback(event_callback)) {
        return createWifiStatus(WifiStatusCode::ERROR_UNKNOWN);
    }
    return createWifiStatus(WifiStatusCode::SUCCESS);
}

WifiStatus WifiChip::selectTxPowerScenarioInternal_1_2(TxPowerScenario scenario) {
    auto legacy_status = legacy_hal_.lock()->selectTxPowerScenario(
        getWlan0IfaceName(),
        hidl_struct_util::convertHidlTxPowerScenarioToLegacy_1_2(scenario));
    return createWifiStatusFromLegacyError(legacy_status);
}

WifiStatus WifiChip::handleChipConfiguration(
    /* NONNULL */ std::unique_lock<std::recursive_mutex>* lock,
    ChipModeId mode_id) {
    // If the chip is already configured in a different mode, stop
    // the legacy HAL and then start it after firmware mode change.
    if (isValidModeId(current_mode_id_)) {
        LOG(INFO) << "Reconfiguring chip from mode " << current_mode_id_
                  << " to mode " << mode_id;
        invalidateAndRemoveAllIfaces();
        legacy_hal::wifi_error legacy_status =
            legacy_hal_.lock()->stop(lock, []() {});
        if (legacy_status != legacy_hal::WIFI_SUCCESS) {
            LOG(ERROR) << "Failed to stop legacy HAL: "
                       << legacyErrorToString(legacy_status);
            return createWifiStatusFromLegacyError(legacy_status);
        }
    }
    // Firmware mode change not needed for V2 devices.
    bool success = true;
    if (mode_id == kV1StaChipModeId) {
        success = mode_controller_.lock()->changeFirmwareMode(IfaceType::STA);
    } else if (mode_id == kV1ApChipModeId) {
        success = mode_controller_.lock()->changeFirmwareMode(IfaceType::AP);
    }
    if (!success) {
        return createWifiStatus(WifiStatusCode::ERROR_UNKNOWN);
    }
    legacy_hal::wifi_error legacy_status = legacy_hal_.lock()->start();
    if (legacy_status != legacy_hal::WIFI_SUCCESS) {
        LOG(ERROR) << "Failed to start legacy HAL: "
                   << legacyErrorToString(legacy_status);
        return createWifiStatusFromLegacyError(legacy_status);
    }
    // Every time the HAL is restarted, we need to register the
    // radio mode change callback.
    WifiStatus status = registerRadioModeChangeCallback();
    if (status.code != WifiStatusCode::SUCCESS) {
        // This probably is not a critical failure?
        LOG(ERROR) << "Failed to register radio mode change callback";
    }
    return createWifiStatus(WifiStatusCode::SUCCESS);
}

WifiStatus WifiChip::registerDebugRingBufferCallback() {
    if (debug_ring_buffer_cb_registered_) {
        return createWifiStatus(WifiStatusCode::SUCCESS);
    }

    android::wp<WifiChip> weak_ptr_this(this);
    const auto& on_ring_buffer_data_callback =
        [weak_ptr_this](const std::string& name,
                        const std::vector<uint8_t>& data,
                        const legacy_hal::wifi_ring_buffer_status& status) {
            const auto shared_ptr_this = weak_ptr_this.promote();
            if (!shared_ptr_this.get() || !shared_ptr_this->isValid()) {
                LOG(ERROR) << "Callback invoked on an invalid object";
                return;
            }
            WifiDebugRingBufferStatus hidl_status;
            if (!hidl_struct_util::convertLegacyDebugRingBufferStatusToHidl(
                    status, &hidl_status)) {
                LOG(ERROR) << "Error converting ring buffer status";
                return;
            }
            const auto& target = shared_ptr_this->ringbuffer_map_.find(name);
            if (target != shared_ptr_this->ringbuffer_map_.end()) {
                Ringbuffer& cur_buffer = target->second;
                cur_buffer.append(data);
            } else {
                LOG(ERROR) << "Ringname " << name << " not found";
                return;
            }
        };
    legacy_hal::wifi_error legacy_status =
        legacy_hal_.lock()->registerRingBufferCallbackHandler(
            getWlan0IfaceName(), on_ring_buffer_data_callback);

    if (legacy_status == legacy_hal::WIFI_SUCCESS) {
        debug_ring_buffer_cb_registered_ = true;
    }
    return createWifiStatusFromLegacyError(legacy_status);
}

WifiStatus WifiChip::registerRadioModeChangeCallback() {
    android::wp<WifiChip> weak_ptr_this(this);
    const auto& on_radio_mode_change_callback =
        [weak_ptr_this](const std::vector<legacy_hal::WifiMacInfo>& mac_infos) {
            const auto shared_ptr_this = weak_ptr_this.promote();
            if (!shared_ptr_this.get() || !shared_ptr_this->isValid()) {
                LOG(ERROR) << "Callback invoked on an invalid object";
                return;
            }
            std::vector<IWifiChipEventCallback::RadioModeInfo>
                hidl_radio_mode_infos;
            if (!hidl_struct_util::convertLegacyWifiMacInfosToHidl(
                    mac_infos, &hidl_radio_mode_infos)) {
                LOG(ERROR) << "Error converting wifi mac info";
                return;
            }
            for (const auto& callback : shared_ptr_this->getEventCallbacks()) {
                if (!callback->onRadioModeChange(hidl_radio_mode_infos)
                         .isOk()) {
                    LOG(ERROR) << "Failed to invoke onRadioModeChange"
                               << " callback on: " << toString(callback);
                }
            }
        };
    legacy_hal::wifi_error legacy_status =
        legacy_hal_.lock()->registerRadioModeChangeCallbackHandler(
            getWlan0IfaceName(), on_radio_mode_change_callback);
    return createWifiStatusFromLegacyError(legacy_status);
}

void WifiChip::populateModes() {
    // The chip combination supported for current devices is fixed.
    // They can be one of the following based on device features:
    // a) 2 separate modes of operation with 1 interface combination each:
    //    Mode 1 (STA mode): Will support 1 STA and 1 P2P or NAN(optional)
    //                       concurrent iface operations.
    //    Mode 2 (AP mode): Will support 1 AP iface operation.
    //
    // b) 1 mode of operation with 2 interface combinations
    // (conditional on isDualInterfaceSupported()):
    //    Interface Combination 1: Will support 1 STA and 1 P2P or NAN(optional)
    //                             concurrent iface operations.
    //    Interface Combination 2: Will support 1 STA and 1 AP concurrent
    //                             iface operations.
    // If Aware is enabled (conditional on isAwareSupported()), the iface
    // combination will be modified to support either P2P or NAN in place of
    // just P2P.
    if (feature_flags_.lock()->isDualInterfaceSupported()) {
        // V2 Iface combinations for Mode Id = 2.
        const IWifiChip::ChipIfaceCombinationLimit
            chip_iface_combination_limit_1 = {{IfaceType::STA}, 1};
        const IWifiChip::ChipIfaceCombinationLimit
            chip_iface_combination_limit_2 = {{IfaceType::AP}, 1};
        IWifiChip::ChipIfaceCombinationLimit chip_iface_combination_limit_3;
        if (feature_flags_.lock()->isAwareSupported()) {
            chip_iface_combination_limit_3 = {{IfaceType::P2P, IfaceType::NAN},
                                              1};
        } else {
            chip_iface_combination_limit_3 = {{IfaceType::P2P}, 1};
        }
        const IWifiChip::ChipIfaceCombination chip_iface_combination_1 = {
            {chip_iface_combination_limit_1, chip_iface_combination_limit_2}};
        const IWifiChip::ChipIfaceCombination chip_iface_combination_2 = {
            {chip_iface_combination_limit_1, chip_iface_combination_limit_3}};
        if (feature_flags_.lock()->isApDisabled()) {
          const IWifiChip::ChipMode chip_mode = {
              kV2ChipModeId,
              {chip_iface_combination_2}};
          modes_ = {chip_mode};
        } else {
          const IWifiChip::ChipMode chip_mode = {
            kV2ChipModeId,
            {chip_iface_combination_1, chip_iface_combination_2}};
          modes_ = {chip_mode};
        }
    } else {
        // V1 Iface combinations for Mode Id = 0. (STA Mode)
        const IWifiChip::ChipIfaceCombinationLimit
            sta_chip_iface_combination_limit_1 = {{IfaceType::STA}, 1};
        IWifiChip::ChipIfaceCombinationLimit sta_chip_iface_combination_limit_2;
        if (feature_flags_.lock()->isAwareSupported()) {
            sta_chip_iface_combination_limit_2 = {
                {IfaceType::P2P, IfaceType::NAN}, 1};
        } else {
            sta_chip_iface_combination_limit_2 = {{IfaceType::P2P}, 1};
        }
        const IWifiChip::ChipIfaceCombination sta_chip_iface_combination = {
            {sta_chip_iface_combination_limit_1,
             sta_chip_iface_combination_limit_2}};
        const IWifiChip::ChipMode sta_chip_mode = {
            kV1StaChipModeId, {sta_chip_iface_combination}};
        // Iface combinations for Mode Id = 1. (AP Mode)
        const IWifiChip::ChipIfaceCombinationLimit
            ap_chip_iface_combination_limit = {{IfaceType::AP}, 1};
        const IWifiChip::ChipIfaceCombination ap_chip_iface_combination = {
            {ap_chip_iface_combination_limit}};
        const IWifiChip::ChipMode ap_chip_mode = {kV1ApChipModeId,
                                                  {ap_chip_iface_combination}};
        if (feature_flags_.lock()->isApDisabled()) {
          modes_ = {sta_chip_mode};
        } else {
          modes_ = {sta_chip_mode, ap_chip_mode};
        }
    }
}

std::vector<IWifiChip::ChipIfaceCombination>
WifiChip::getCurrentModeIfaceCombinations() {
    if (!isValidModeId(current_mode_id_)) {
        LOG(ERROR) << "Chip not configured in a mode yet";
        return {};
    }
    for (const auto& mode : modes_) {
        if (mode.id == current_mode_id_) {
            return mode.availableCombinations;
        }
    }
    CHECK(0) << "Expected to find iface combinations for current mode!";
    return {};
}

// Returns a map indexed by IfaceType with the number of ifaces currently
// created of the corresponding type.
std::map<IfaceType, size_t> WifiChip::getCurrentIfaceCombination() {
    std::map<IfaceType, size_t> iface_counts;
    iface_counts[IfaceType::AP] = ap_ifaces_.size();
    iface_counts[IfaceType::NAN] = nan_ifaces_.size();
    iface_counts[IfaceType::P2P] = p2p_ifaces_.size();
    iface_counts[IfaceType::STA] = sta_ifaces_.size();
    return iface_counts;
}

// This expands the provided iface combinations to a more parseable
// form. Returns a vector of available combinations possible with the number
// of ifaces of each type in the combination.
// This method is a port of HalDeviceManager.expandIfaceCombos() from framework.
std::vector<std::map<IfaceType, size_t>> WifiChip::expandIfaceCombinations(
    const IWifiChip::ChipIfaceCombination& combination) {
    uint32_t num_expanded_combos = 1;
    for (const auto& limit : combination.limits) {
        for (uint32_t i = 0; i < limit.maxIfaces; i++) {
            num_expanded_combos *= limit.types.size();
        }
    }

    // Allocate the vector of expanded combos and reset all iface counts to 0
    // in each combo.
    std::vector<std::map<IfaceType, size_t>> expanded_combos;
    expanded_combos.resize(num_expanded_combos);
    for (auto& expanded_combo : expanded_combos) {
        for (const auto type :
             {IfaceType::AP, IfaceType::NAN, IfaceType::P2P, IfaceType::STA}) {
            expanded_combo[type] = 0;
        }
    }
    uint32_t span = num_expanded_combos;
    for (const auto& limit : combination.limits) {
        for (uint32_t i = 0; i < limit.maxIfaces; i++) {
            span /= limit.types.size();
            for (uint32_t k = 0; k < num_expanded_combos; ++k) {
                const auto iface_type =
                    limit.types[(k / span) % limit.types.size()];
                expanded_combos[k][iface_type]++;
            }
        }
    }
    return expanded_combos;
}

bool WifiChip::canExpandedIfaceCombinationSupportIfaceOfType(
    const std::map<IfaceType, size_t>& combo, IfaceType requested_type) {
    const auto current_combo = getCurrentIfaceCombination();

    // Check if we have space for 1 more iface of |type| in this combo
    for (const auto type :
         {IfaceType::AP, IfaceType::NAN, IfaceType::P2P, IfaceType::STA}) {
        size_t num_ifaces_needed = current_combo.at(type);
        if (type == requested_type) {
            num_ifaces_needed++;
        }
        size_t num_ifaces_allowed = combo.at(type);
        if (num_ifaces_needed > num_ifaces_allowed) {
            return false;
        }
    }
    return true;
}

// This method does the following:
// a) Enumerate all possible iface combos by expanding the current
//    ChipIfaceCombination.
// b) Check if the requested iface type can be added to the current mode.
bool WifiChip::canCurrentModeSupportIfaceOfType(IfaceType type) {
    if (!isValidModeId(current_mode_id_)) {
        LOG(ERROR) << "Chip not configured in a mode yet";
        return false;
    }
    const auto combinations = getCurrentModeIfaceCombinations();
    for (const auto& combination : combinations) {
        const auto expanded_combos = expandIfaceCombinations(combination);
        for (const auto& expanded_combo : expanded_combos) {
            if (canExpandedIfaceCombinationSupportIfaceOfType(expanded_combo,
                                                              type)) {
                return true;
            }
        }
    }
    return false;
}

bool WifiChip::isValidModeId(ChipModeId mode_id) {
    for (const auto& mode : modes_) {
        if (mode.id == mode_id) {
            return true;
        }
    }
    return false;
}

// Return "wlan0", if "wlan0" is not already in use, else return "wlan1".
// This is based on the assumption that we'll have a max of 2 concurrent
// AP/STA ifaces.
std::string WifiChip::allocateApOrStaIfaceName() {
    auto ap_iface = findUsingName(ap_ifaces_, getWlan0IfaceName());
    auto sta_iface = findUsingName(sta_ifaces_, getWlan0IfaceName());
    if (!ap_iface.get() && !sta_iface.get()) {
        return getWlan0IfaceName();
    }
    ap_iface = findUsingName(ap_ifaces_, getWlan1IfaceName());
    sta_iface = findUsingName(sta_ifaces_, getWlan1IfaceName());
    if (!ap_iface.get() && !sta_iface.get()) {
        return getWlan1IfaceName();
    }
    // This should never happen. We screwed up somewhere if it did.
    CHECK(0) << "wlan0 and wlan1 in use already!";
    return {};
}

bool WifiChip::writeRingbufferFilesInternal() {
    if (!removeOldFilesInternal()) {
        LOG(ERROR) << "Error occurred while deleting old tombstone files";
        return false;
    }
    // write ringbuffers to file
    for (const auto& item : ringbuffer_map_) {
        const Ringbuffer& cur_buffer = item.second;
        if (cur_buffer.getData().empty()) {
            continue;
        }
        const std::string file_path_raw =
            kTombstoneFolderPath + item.first + "XXXXXXXXXX";
        const int dump_fd = mkstemp(makeCharVec(file_path_raw).data());
        if (dump_fd == -1) {
            LOG(ERROR) << "create file failed: " << strerror(errno);
            return false;
        }
        unique_fd file_auto_closer(dump_fd);
        for (const auto& cur_block : cur_buffer.getData()) {
            if (write(dump_fd, cur_block.data(),
                      sizeof(cur_block[0]) * cur_block.size()) == -1) {
                LOG(ERROR) << "Error writing to file " << strerror(errno);
            }
        }
    }
    return true;
}

}  // namespace implementation
}  // namespace V1_2
}  // namespace wifi
}  // namespace hardware
}  // namespace android
