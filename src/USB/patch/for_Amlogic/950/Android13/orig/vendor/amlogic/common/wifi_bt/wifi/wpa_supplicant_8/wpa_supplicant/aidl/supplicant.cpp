/*
 * WPA Supplicant - Supplicant Aidl interface
 * Copyright (c) 2021, Google Inc. All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "aidl_manager.h"
#include "aidl_return_util.h"
#include "misc_utils.h"
#include "supplicant.h"
#include "p2p_iface.h"

#include <android-base/file.h>
#include <fcntl.h>
#include <sys/stat.h>

#if defined(ANDROID)
#include <cutils/properties.h>
#if defined(__BIONIC_FORTIFY)
#include <sys/system_properties.h>
#endif
#endif

namespace {

// Pre-populated interface params for interfaces controlled by wpa_supplicant.
// Note: This may differ for other OEM's. So, modify this accordingly.
constexpr char kIfaceDriverName[] = "nl80211";
constexpr char kStaIfaceConfPath[] =
	"/data/vendor/wifi/wpa/wpa_supplicant.conf";
static const char* kStaIfaceConfOverlayPaths[] = {
    "/apex/com.android.wifi.hal/etc/wifi/wpa_supplicant_overlay.conf",
    "/vendor/etc/wifi/wpa_supplicant_overlay.conf",
};
constexpr char kP2pIfaceConfPath[] =
	"/data/vendor/wifi/wpa/p2p_supplicant.conf";
static const char* kP2pIfaceConfOverlayPaths[] = {
    "/apex/com.android.wifi.hal/etc/wifi/p2p_supplicant_overlay.conf",
    "/vendor/etc/wifi/p2p_supplicant_overlay.conf",
};
// Migrate conf files for existing devices.
static const char* kTemplateConfPaths[] = {
    "/apex/com.android.wifi.hal/etc/wifi/wpa_supplicant.conf",
    "/vendor/etc/wifi/wpa_supplicant.conf",
    "/system/etc/wifi/wpa_supplicant.conf",
};
constexpr char kOldStaIfaceConfPath[] = "/data/misc/wifi/wpa_supplicant.conf";
constexpr char kOldP2pIfaceConfPath[] = "/data/misc/wifi/p2p_supplicant.conf";
constexpr mode_t kConfigFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

const char* resolvePath(const char* paths[], size_t size)
{
	for (int i = 0; i < size; ++i) {
		if (access(paths[i], R_OK) == 0) {
			return paths[i];
		}
	}
	return nullptr;
}

int copyFile(
	const std::string& src_file_path, const std::string& dest_file_path)
{
	std::string file_contents;
	if (!android::base::ReadFileToString(src_file_path, &file_contents)) {
		wpa_printf(
			MSG_ERROR, "Failed to read from %s. Errno: %s",
			src_file_path.c_str(), strerror(errno));
		return -1;
	}
	if (!android::base::WriteStringToFile(
		file_contents, dest_file_path, kConfigFileMode, getuid(),
		getgid())) {
		wpa_printf(
			MSG_ERROR, "Failed to write to %s. Errno: %s",
			dest_file_path.c_str(), strerror(errno));
		return -1;
	}
	return 0;
}

/**
 * Copy |src_file_path| to |dest_file_path| if it exists.
 *
 * Returns 1 if |src_file_path| does not exist or not accessible,
 * Returns -1 if the copy fails.
 * Returns 0 if the copy succeeds.
 */
int copyFileIfItExists(
	const std::string& src_file_path, const std::string& dest_file_path)
{
	int ret = access(src_file_path.c_str(), R_OK);
	// Sepolicy denial (2018+ device) will return EACCESS instead of ENOENT.
	if ((ret != 0) && ((errno == ENOENT) || (errno == EACCES))) {
		return 1;
	}
	ret = copyFile(src_file_path, dest_file_path);
	if (ret != 0) {
		wpa_printf(
			MSG_ERROR, "Failed copying %s to %s.",
			src_file_path.c_str(), dest_file_path.c_str());
		return -1;
	}
	return 0;
}

/**
 * Ensure that the specified config file pointed by |config_file_path| exists.
 * a) If the |config_file_path| exists with the correct permissions, return.
 * b) If the |config_file_path| does not exist, but |old_config_file_path|
 * exists, copy over the contents of the |old_config_file_path| to
 * |config_file_path|.
 * c) If the |config_file_path| & |old_config_file_path|
 * does not exists, copy over the contents of |template_config_file_path|.
 */
int ensureConfigFileExists(
	const std::string& config_file_path,
	const std::string& old_config_file_path)
{
	int ret = access(config_file_path.c_str(), R_OK | W_OK);
	if (ret == 0) {
		return 0;
	}
	if (errno == EACCES) {
		ret = chmod(config_file_path.c_str(), kConfigFileMode);
		if (ret == 0) {
			return 0;
		} else {
			wpa_printf(
				MSG_ERROR, "Cannot set RW to %s. Errno: %s",
				config_file_path.c_str(), strerror(errno));
			return -1;
		}
	} else if (errno != ENOENT) {
		wpa_printf(
			MSG_ERROR, "Cannot access %s. Errno: %s",
			config_file_path.c_str(), strerror(errno));
		return -1;
	}
	ret = copyFileIfItExists(old_config_file_path, config_file_path);
	if (ret == 0) {
		wpa_printf(
			MSG_INFO, "Migrated conf file from %s to %s",
			old_config_file_path.c_str(), config_file_path.c_str());
		unlink(old_config_file_path.c_str());
		return 0;
	} else if (ret == -1) {
		unlink(config_file_path.c_str());
		return -1;
	}
	const char* path =
	    resolvePath(kTemplateConfPaths,
	    sizeof(kTemplateConfPaths)/sizeof(kTemplateConfPaths[0]));
	if (path != nullptr) {
		ret = copyFileIfItExists(path, config_file_path);
		if (ret == 0) {
			wpa_printf(
			    MSG_INFO, "Copied template conf file from %s to %s",
			    path, config_file_path.c_str());
			return 0;
		} else if (ret == -1) {
			unlink(config_file_path.c_str());
			return -1;
		}
	}
	// Did not create the conf file.
	return -1;
}
}  // namespace

namespace aidl {
namespace android {
namespace hardware {
namespace wifi {
namespace supplicant {
using aidl_return_util::validateAndCall;
using misc_utils::createStatus;
using misc_utils::createStatusWithMsg;

Supplicant::Supplicant(struct wpa_global* global) : wpa_global_(global) {}
bool Supplicant::isValid()
{
	// This top level object cannot be invalidated.
	return true;
}

::ndk::ScopedAStatus Supplicant::addP2pInterface(
	const std::string& in_name,
	std::shared_ptr<ISupplicantP2pIface>* _aidl_return)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::addP2pInterfaceInternal, _aidl_return, in_name);
}

::ndk::ScopedAStatus Supplicant::addStaInterface(
	const std::string& in_name,
	std::shared_ptr<ISupplicantStaIface>* _aidl_return)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::addStaInterfaceInternal, _aidl_return, in_name);
}

::ndk::ScopedAStatus Supplicant::removeInterface(
	const IfaceInfo& in_ifaceInfo)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::removeInterfaceInternal, in_ifaceInfo);
}

::ndk::ScopedAStatus Supplicant::getP2pInterface(
	const std::string& in_name,
	std::shared_ptr<ISupplicantP2pIface>* _aidl_return)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::getP2pInterfaceInternal, _aidl_return, in_name);
}

::ndk::ScopedAStatus Supplicant::getStaInterface(
	const std::string& in_name,
	std::shared_ptr<ISupplicantStaIface>* _aidl_return)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::getStaInterfaceInternal, _aidl_return, in_name);
}

::ndk::ScopedAStatus Supplicant::listInterfaces(
	std::vector<IfaceInfo>* _aidl_return)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::listInterfacesInternal, _aidl_return);
}

::ndk::ScopedAStatus Supplicant::registerCallback(
	const std::shared_ptr<ISupplicantCallback>& in_callback)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::registerCallbackInternal, in_callback);
}

::ndk::ScopedAStatus Supplicant::setDebugParams(
	DebugLevel in_level, bool in_showTimestamp,
	bool in_showKeys)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::setDebugParamsInternal, in_level,
		in_showTimestamp, in_showKeys);
}

::ndk::ScopedAStatus Supplicant::setConcurrencyPriority(
	IfaceType in_type)
{
	return validateAndCall(
		this, SupplicantStatusCode::FAILURE_IFACE_INVALID,
		&Supplicant::setConcurrencyPriorityInternal, in_type);
}

::ndk::ScopedAStatus Supplicant::getDebugLevel(DebugLevel* _aidl_return)
{
	*_aidl_return = static_cast<DebugLevel>(wpa_debug_level);
	return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Supplicant::isDebugShowTimestampEnabled(bool* _aidl_return)
{
	*_aidl_return = ((wpa_debug_timestamp != 0) ? true : false);
	return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Supplicant::isDebugShowKeysEnabled(bool* _aidl_return)
{
	*_aidl_return = ((wpa_debug_show_keys != 0) ? true : false);
	return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Supplicant::terminate()
{
	wpa_printf(MSG_INFO, "Terminating...");
	wpa_supplicant_terminate_proc(wpa_global_);
	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Supplicant::addP2pDevInterface(struct wpa_interface iface_params)
{
	char primary_ifname[IFNAMSIZ];
	u32 primary_ifname_len =
		strlen(iface_params.ifname) - strlen(P2P_MGMT_DEVICE_PREFIX);

	if(primary_ifname_len > IFNAMSIZ) {
		wpa_printf(MSG_DEBUG, "%s, Invalid primary iface name ", __FUNCTION__);
		return createStatus(SupplicantStatusCode::FAILURE_ARGS_INVALID);
	}

	strncpy(primary_ifname, iface_params.ifname +
		strlen(P2P_MGMT_DEVICE_PREFIX), primary_ifname_len);
	wpa_printf(MSG_DEBUG, "%s, Initialize p2p-dev-wlan0 iface with"
		"primary_iface = %s", __FUNCTION__, primary_ifname);
	struct wpa_supplicant* wpa_s =
		wpa_supplicant_get_iface(wpa_global_, primary_ifname);
	if (!wpa_s) {
		wpa_printf(MSG_DEBUG, "%s,NULL wpa_s for wlan0", __FUNCTION__);
		return createStatus(SupplicantStatusCode::FAILURE_IFACE_UNKNOWN);
	}
	if (wpas_p2p_add_p2pdev_interface(
		wpa_s, wpa_s->global->params.conf_p2p_dev) < 0) {
		wpa_printf(MSG_INFO,
			"Failed to enable P2P Device");
		return createStatus(SupplicantStatusCode::FAILURE_UNKNOWN);
	}
	return ndk::ScopedAStatus::ok();
}

std::pair<std::shared_ptr<ISupplicantP2pIface>, ndk::ScopedAStatus>
Supplicant::addP2pInterfaceInternal(const std::string& name)
{
	std::shared_ptr<ISupplicantP2pIface> iface;
	char wifi_status[PROPERTY_VALUE_MAX] = {'\0'};

	// Check if required |ifname| argument is empty.
	if (name.empty()) {
		return {nullptr, createStatus(SupplicantStatusCode::FAILURE_ARGS_INVALID)};
	}
	// Try to get the wpa_supplicant record for this iface, return
	// the iface object with the appropriate status code if it exists.
	ndk::ScopedAStatus status;
	std::tie(iface, status) = getP2pInterfaceInternal(name);
	if (status.isOk()) {
		wpa_printf(MSG_INFO, "Iface already exists, return existing");
		return {iface, ndk::ScopedAStatus::ok()};
	}

	struct wpa_interface iface_params = {};
	iface_params.driver = kIfaceDriverName;
	if (ensureConfigFileExists(
		kP2pIfaceConfPath, kOldP2pIfaceConfPath) != 0) {
		wpa_printf(
			MSG_ERROR, "Conf file does not exists: %s",
			kP2pIfaceConfPath);
		return {nullptr, createStatusWithMsg(
			SupplicantStatusCode::FAILURE_UNKNOWN, "Conf file does not exist")};
	}
	iface_params.confname = kP2pIfaceConfPath;
	property_get("vendor.wifi_name", wifi_status, NULL);
	const char* path = resolvePath(
		    kP2pIfaceConfOverlayPaths,
		    sizeof(kP2pIfaceConfOverlayPaths)/sizeof(kP2pIfaceConfOverlayPaths[0]));
	if (path != nullptr && os_strncasecmp(wifi_status, "bcm", 3) != 0 && os_strncasecmp(wifi_status, "uwe", 3) != 0) {
		iface_params.confanother = path;
	}

	iface_params.ifname = name.c_str();
	if (strncmp(iface_params.ifname, P2P_MGMT_DEVICE_PREFIX,
		strlen(P2P_MGMT_DEVICE_PREFIX)) == 0) {
		status = addP2pDevInterface(iface_params);
		if (!status.isOk()) {
			return {iface, createStatus(static_cast<SupplicantStatusCode>(
				status.getServiceSpecificError()))};
		}
	} else {
		struct wpa_supplicant* wpa_s =
			wpa_supplicant_add_iface(wpa_global_, &iface_params, NULL);
		if (!wpa_s) {
			return {nullptr, createStatus(SupplicantStatusCode::FAILURE_UNKNOWN)};
		}
		// Request the current scan results from the driver and update
		// the local BSS list wpa_s->bss. This is to avoid a full scan
		// while processing the connect request on newly created interface.
		wpa_supplicant_update_scan_results(wpa_s);
	}
	// The supplicant core creates a corresponding aidl object via
	// AidlManager when |wpa_supplicant_add_iface| is called.
	return getP2pInterfaceInternal(name);
}

std::pair<std::shared_ptr<ISupplicantStaIface>, ndk::ScopedAStatus>
Supplicant::addStaInterfaceInternal(const std::string& name)
{
	std::shared_ptr<ISupplicantStaIface> iface;
	char wifi_status[PROPERTY_VALUE_MAX] = {'\0'};

	// Check if required |ifname| argument is empty.
	if (name.empty()) {
		return {nullptr, createStatus(SupplicantStatusCode::FAILURE_ARGS_INVALID)};
	}
	// Try to get the wpa_supplicant record for this iface, return
	// the iface object with the appropriate status code if it exists.
	ndk::ScopedAStatus status;
	std::tie(iface, status) = getStaInterfaceInternal(name);
	if (status.isOk()) {
		wpa_printf(MSG_INFO, "Iface already exists, return existing");
		return {iface, ndk::ScopedAStatus::ok()};
	}

	struct wpa_interface iface_params = {};
	iface_params.driver = kIfaceDriverName;
	if (ensureConfigFileExists(
		kStaIfaceConfPath, kOldStaIfaceConfPath) != 0) {
		wpa_printf(
			MSG_ERROR, "Conf file does not exists: %s",
			kStaIfaceConfPath);
		return {nullptr, createStatusWithMsg(
			SupplicantStatusCode::FAILURE_UNKNOWN, "Conf file does not exist")};
	}
	iface_params.confname = kStaIfaceConfPath;
	property_get("vendor.wifi_name", wifi_status, NULL);
	const char* path = resolvePath(
		    kStaIfaceConfOverlayPaths,
		    sizeof(kStaIfaceConfOverlayPaths)/sizeof(kStaIfaceConfOverlayPaths[0]));
	if (path != nullptr && os_strncasecmp(wifi_status, "bcm", 3) != 0 && os_strncasecmp(wifi_status, "uwe", 3) != 0) {
		iface_params.confanother = path;
	}

	iface_params.ifname = name.c_str();
	if (strncmp(iface_params.ifname, P2P_MGMT_DEVICE_PREFIX,
		strlen(P2P_MGMT_DEVICE_PREFIX)) == 0) {
		status = addP2pDevInterface(iface_params);
		if (!status.isOk()) {
			return {iface, createStatus(static_cast<SupplicantStatusCode>(
				status.getServiceSpecificError()))};
		}
	} else {
		struct wpa_supplicant* wpa_s =
			wpa_supplicant_add_iface(wpa_global_, &iface_params, NULL);
		if (!wpa_s) {
			return {nullptr, createStatus(SupplicantStatusCode::FAILURE_UNKNOWN)};
		}
		// Request the current scan results from the driver and update
		// the local BSS list wpa_s->bss. This is to avoid a full scan
		// while processing the connect request on newly created interface.
		wpa_supplicant_update_scan_results(wpa_s);
	}
	// The supplicant core creates a corresponding aidl object via
	// AidlManager when |wpa_supplicant_add_iface| is called.
	return getStaInterfaceInternal(name);
}

ndk::ScopedAStatus Supplicant::removeInterfaceInternal(
	const IfaceInfo& iface_info)
{
	struct wpa_supplicant* wpa_s =
		wpa_supplicant_get_iface(wpa_global_, iface_info.name.c_str());
	if (!wpa_s) {
		return createStatus(SupplicantStatusCode::FAILURE_IFACE_UNKNOWN);
	}
	if (wpa_supplicant_remove_iface(wpa_global_, wpa_s, 0)) {
		return createStatus(SupplicantStatusCode::FAILURE_UNKNOWN);
	}
	return ndk::ScopedAStatus::ok();
}

std::pair<std::shared_ptr<ISupplicantP2pIface>, ndk::ScopedAStatus>
Supplicant::getP2pInterfaceInternal(const std::string& name)
{
	struct wpa_supplicant* wpa_s =
		wpa_supplicant_get_iface(wpa_global_, name.c_str());
	if (!wpa_s) {
		return {nullptr, createStatus(SupplicantStatusCode::FAILURE_IFACE_UNKNOWN)};
	}
	AidlManager* aidl_manager = AidlManager::getInstance();
	std::shared_ptr<ISupplicantP2pIface> iface;
	if (!aidl_manager ||
		aidl_manager->getP2pIfaceAidlObjectByIfname(
		wpa_s->ifname, &iface)) {
		return {iface, createStatus(SupplicantStatusCode::FAILURE_UNKNOWN)};
	}
	// Set this flag true here, since there is no AIDL initialize
	// method for the p2p config, and the supplicant interface is
	// not ready when the p2p iface is created.
	wpa_s->conf->persistent_reconnect = true;
	return {iface, ndk::ScopedAStatus::ok()};
}

std::pair<std::shared_ptr<ISupplicantStaIface>, ndk::ScopedAStatus>
Supplicant::getStaInterfaceInternal(const std::string& name)
{
	struct wpa_supplicant* wpa_s =
		wpa_supplicant_get_iface(wpa_global_, name.c_str());
	if (!wpa_s) {
		return {nullptr, createStatus(SupplicantStatusCode::FAILURE_IFACE_UNKNOWN)};
	}
	AidlManager* aidl_manager = AidlManager::getInstance();
	std::shared_ptr<ISupplicantStaIface> iface;
	if (!aidl_manager ||
		aidl_manager->getStaIfaceAidlObjectByIfname(
		wpa_s->ifname, &iface)) {
		return {iface, createStatus(SupplicantStatusCode::FAILURE_UNKNOWN)};
	}
	return {iface, ndk::ScopedAStatus::ok()};
}

std::pair<std::vector<IfaceInfo>, ndk::ScopedAStatus>
Supplicant::listInterfacesInternal()
{
	std::vector<IfaceInfo> ifaces;
	for (struct wpa_supplicant* wpa_s = wpa_global_->ifaces; wpa_s;
		 wpa_s = wpa_s->next) {
		if (wpa_s->global->p2p_init_wpa_s == wpa_s) {
			ifaces.emplace_back(IfaceInfo{
				IfaceType::P2P, wpa_s->ifname});
		} else {
			ifaces.emplace_back(IfaceInfo{
				IfaceType::STA, wpa_s->ifname});
		}
	}
	return {std::move(ifaces), ndk::ScopedAStatus::ok()};
}

ndk::ScopedAStatus Supplicant::registerCallbackInternal(
	const std::shared_ptr<ISupplicantCallback>& callback)
{
	AidlManager* aidl_manager = AidlManager::getInstance();
	if (!aidl_manager ||
		aidl_manager->addSupplicantCallbackAidlObject(callback)) {
		return createStatus(SupplicantStatusCode::FAILURE_UNKNOWN);
	}
	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Supplicant::setDebugParamsInternal(
	DebugLevel level, bool show_timestamp, bool show_keys)
{
	if (wpa_supplicant_set_debug_params(
		wpa_global_, static_cast<uint32_t>(level), show_timestamp,
		show_keys)) {
		return createStatus(SupplicantStatusCode::FAILURE_UNKNOWN);
	}
	return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Supplicant::setConcurrencyPriorityInternal(IfaceType type)
{
	if (type == IfaceType::STA) {
		wpa_global_->conc_pref =
			wpa_global::wpa_conc_pref::WPA_CONC_PREF_STA;
	} else if (type == IfaceType::P2P) {
		wpa_global_->conc_pref =
			wpa_global::wpa_conc_pref::WPA_CONC_PREF_P2P;
	} else {
		return createStatus(SupplicantStatusCode::FAILURE_ARGS_INVALID);
	}
	return ndk::ScopedAStatus::ok();
}
}  // namespace supplicant
}  // namespace wifi
}  // namespace hardware
}  // namespace android
}  // namespace aidl
