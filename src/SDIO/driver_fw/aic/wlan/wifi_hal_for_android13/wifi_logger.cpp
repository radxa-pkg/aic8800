/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Portions copyright (C) 2017 Broadcom Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/filter.h>
#include <linux/errqueue.h>

#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink-private/object-api.h>
#include <netlink-private/types.h>

#include "nl80211_copy.h"
#include "sync.h"

#define LOG_TAG  "WifiHAL"

#include <log/log.h>

#include "wifi_hal.h"
#include "common.h"
#include "cpp_bindings.h"

typedef enum {
    LOGGER_START_LOGGING = ANDROID_NL80211_SUBCMD_DEBUG_RANGE_START,
    LOGGER_TRIGGER_MEM_DUMP,
    LOGGER_GET_MEM_DUMP,
    LOGGER_GET_VER,
    LOGGER_GET_RING_STATUS,
    LOGGER_GET_RING_DATA,
    LOGGER_GET_FEATURE,
    LOGGER_RESET_LOGGING,
    LOGGER_TRIGGER_DRIVER_MEM_DUMP,
    LOGGER_GET_DRIVER_MEM_DUMP,
    LOGGER_START_PKT_FATE_MONITORING,
    LOGGER_GET_TX_PKT_FATES,
    LOGGER_GET_RX_PKT_FATES,
    LOGGER_GET_WAKE_REASON_STATS,
    LOGGER_DEBUG_GET_DUMP,
    LOGGER_FILE_DUMP_DONE_IND,
    LOGGER_SET_HAL_START,
    LOGGER_HAL_STOP,
    LOGGER_SET_HAL_PID
} DEBUG_SUB_COMMAND;

typedef enum {
    LOGGER_ATTRIBUTE_INVALID = 0,
    LOGGER_ATTRIBUTE_DRIVER_VER,
    LOGGER_ATTRIBUTE_FW_VER,
    LOGGER_ATTRIBUTE_RING_ID,
    LOGGER_ATTRIBUTE_RING_NAME,
    LOGGER_ATTRIBUTE_RING_FLAGS,
    LOGGER_ATTRIBUTE_LOG_LEVEL,
    LOGGER_ATTRIBUTE_LOG_TIME_INTVAL,
    LOGGER_ATTRIBUTE_LOG_MIN_DATA_SIZE,
    LOGGER_ATTRIBUTE_FW_DUMP_LEN,
    LOGGER_ATTRIBUTE_FW_DUMP_DATA,
    // LOGGER_ATTRIBUTE_FW_ERR_CODE,
    LOGGER_ATTRIBUTE_RING_DATA,
    LOGGER_ATTRIBUTE_RING_STATUS,
    LOGGER_ATTRIBUTE_RING_NUM,
    LOGGER_ATTRIBUTE_DRIVER_DUMP_LEN,
    LOGGER_ATTRIBUTE_DRIVER_DUMP_DATA,
    LOGGER_ATTRIBUTE_PKT_FATE_NUM,
    LOGGER_ATTRIBUTE_PKT_FATE_DATA,
    LOGGER_ATTRIBUTE_AFTER_LAST,
    LOGGER_ATTRIBUTE_MAX = LOGGER_ATTRIBUTE_AFTER_LAST - 1,
} LOGGER_ATTRIBUTE;

typedef enum {
    DEBUG_OFF = 0,
    DEBUG_NORMAL,
    DEBUG_VERBOSE,
    DEBUG_VERY,
    DEBUG_VERY_VERY,
} LOGGER_LEVEL;

typedef enum {
    GET_FW_VER,
    GET_DRV_VER,
    GET_RING_DATA,
    GET_RING_STATUS,
    GET_FEATURE,
    START_RING_LOG,
} GetCmdType;

typedef enum {
    PACKET_MONITOR_START,
    TX_PACKET_FATE,
    RX_PACKET_FATE,
} PktFateReqType;

enum wake_stat_attributes {
    WAKE_STAT_ATTRIBUTE_TOTAL_CMD_EVENT,
    WAKE_STAT_ATTRIBUTE_CMD_EVENT_WAKE,
    WAKE_STAT_ATTRIBUTE_CMD_EVENT_COUNT,
    WAKE_STAT_ATTRIBUTE_CMD_COUNT_USED,
    WAKE_STAT_ATTRIBUTE_TOTAL_DRIVER_FW,
    WAKE_STAT_ATTRIBUTE_DRIVER_FW_WAKE,
    WAKE_STAT_ATTRIBUTE_DRIVER_FW_COUNT,
    WAKE_STAT_ATTRIBUTE_DRIVER_FW_COUNT_USED,
    WAKE_STAT_ATTRIBUTE_TOTAL_RX_DATA_WAKE,
    WAKE_STAT_ATTRIBUTE_RX_UNICAST_COUNT,
    WAKE_STAT_ATTRIBUTE_RX_MULTICAST_COUNT,
    WAKE_STAT_ATTRIBUTE_RX_BROADCAST_COUNT,
    WAKE_STAT_ATTRIBUTE_RX_ICMP_PKT,
    WAKE_STAT_ATTRIBUTE_RX_ICMP6_PKT,
    WAKE_STAT_ATTRIBUTE_RX_ICMP6_RA,
    WAKE_STAT_ATTRIBUTE_RX_ICMP6_NA,
    WAKE_STAT_ATTRIBUTE_RX_ICMP6_NS,
    WAKE_STAT_ATTRIBUTE_IPV4_RX_MULTICAST_ADD_CNT,
    WAKE_STAT_ATTRIBUTE_IPV6_RX_MULTICAST_ADD_CNT,
    WAKE_STAT_ATTRIBUTE_OTHER__RX_MULTICAST_ADD_CNT,
    WAKE_STAT_ATTRIBUTE_RX_MULTICAST_PKT_INFO
};

///////////////////////////////////////////////////////////////////////////////
class DebugCommand : public WifiCommand
{
    char *mBuff;
    int *mBuffSize;
    u32 *mNumRings;
    wifi_ring_buffer_status *mStatus;
    unsigned int *mSupport;
    u32 mVerboseLevel;
    u32 mFlags;
    u32 mMaxIntervalSec;
    u32 mMinDataSize;
    char *mRingName;
    GetCmdType mType;

public:

    // constructor for get version
    DebugCommand(wifi_interface_handle iface, char *buffer, int *buffer_size,
            GetCmdType cmdType)
        : WifiCommand("DebugCommand", iface, 0), mBuff(buffer), mBuffSize(buffer_size), mType
        (cmdType)
    {
        memset(mBuff, 0, *mBuffSize);
    }

    // constructor for ring data
    DebugCommand(wifi_interface_handle iface, char *ring_name, GetCmdType cmdType)
        : WifiCommand("DebugCommand", iface, 0), mRingName(ring_name), mType(cmdType)
    { }

    // constructor for ring status
    DebugCommand(wifi_interface_handle iface, u32 *num_rings,
            wifi_ring_buffer_status *status, GetCmdType cmdType)
        : WifiCommand("DebugCommand", iface, 0), mNumRings(num_rings), mStatus(status), mType(cmdType)
    {
        memset(mStatus, 0, sizeof(wifi_ring_buffer_status) * (*mNumRings));
    }

    // constructor for feature set
    DebugCommand(wifi_interface_handle iface, unsigned int *support, GetCmdType cmdType)
        : WifiCommand("DebugCommand", iface, 0), mSupport(support), mType(cmdType)
    { }

    // constructor for ring params
    DebugCommand(wifi_interface_handle iface, u32 verbose_level, u32 flags,
            u32 max_interval_sec, u32 min_data_size, char *ring_name, GetCmdType cmdType)
        : WifiCommand("DebugCommand", iface, 0), mVerboseLevel(verbose_level), mFlags(flags),
        mMaxIntervalSec(max_interval_sec), mMinDataSize(min_data_size),
        mRingName(ring_name), mType(cmdType)
    { }

    int createRingRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, LOGGER_START_LOGGING);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to create start ring logger request; result = %d", result);
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);

        result = request.put_u32(LOGGER_ATTRIBUTE_LOG_LEVEL, mVerboseLevel);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put log level; result = %d", result);
            return result;
        }
        result = request.put_u32(LOGGER_ATTRIBUTE_RING_FLAGS, mFlags);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put ring flags; result = %d", result);
            return result;
        }
        result = request.put_u32(LOGGER_ATTRIBUTE_LOG_TIME_INTVAL, mMaxIntervalSec);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put log time interval; result = %d", result);
            return result;
        }
        result = request.put_u32(LOGGER_ATTRIBUTE_LOG_MIN_DATA_SIZE, mMinDataSize);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put min data size; result = %d", result);
            return result;
        }
        result = request.put_string(LOGGER_ATTRIBUTE_RING_NAME, mRingName);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put ringbuffer name; result = %d", result);
            return result;
        }
        request.attr_end(data);

        return WIFI_SUCCESS;
    }

    int createRequest(WifiRequest &request) {
        int result;

        switch (mType) {
            case GET_FW_VER:
            {
                result = request.create(GOOGLE_OUI, LOGGER_GET_VER);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to create get fw version request; result = %d", result);
                    return result;
                }

                nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);

                // Driver expecting only attribute type, passing mbuff as data with
                // length 0 to avoid undefined state
                result = request.put(LOGGER_ATTRIBUTE_FW_VER, mBuff, 0);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to put get fw version request; result = %d", result);
                    return result;
                }
                request.attr_end(data);
                break;
            }

            case GET_DRV_VER:
            {
                result = request.create(GOOGLE_OUI, LOGGER_GET_VER);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to create get drv version request; result = %d", result);
                    return result;
                }

                nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);

                // Driver expecting only attribute type, passing mbuff as data with
                // length 0 to avoid undefined state
                result = request.put(LOGGER_ATTRIBUTE_DRIVER_VER, mBuff, 0);

                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to put get drv version request; result = %d", result);
                    return result;
                }
                request.attr_end(data);
                break;
            }

            case GET_RING_DATA:
            {
                result = request.create(GOOGLE_OUI, LOGGER_GET_RING_DATA);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to create get ring data request; result = %d", result);
                    return result;
                }

                nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
                result = request.put_string(LOGGER_ATTRIBUTE_RING_NAME, mRingName);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to put ring data request; result = %d", result);
                    return result;
                }
                request.attr_end(data);
                break;
            }

            case GET_RING_STATUS:
            {
                result = request.create(GOOGLE_OUI, LOGGER_GET_RING_STATUS);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to create get ring status request; result = %d", result);
                    return result;
                }
                break;
            }

            case GET_FEATURE:
            {
                result = request.create(GOOGLE_OUI, LOGGER_GET_FEATURE);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to create get feature request; result = %d", result);
                    return result;
                }
                break;
            }

            case START_RING_LOG:
                result = createRingRequest(request);
                break;

            default:
                ALOGE("Unknown Debug command");
                result = WIFI_ERROR_UNKNOWN;
        }
        return result;
    }

    int start() {
        // ALOGD("Start debug command");
        WifiRequest request(familyId(), ifaceId());
        int result = createRequest(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to create debug request; result = %d", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to register debug response; result = %d", result);
        }
        return result;
    }

    virtual int handleResponse(WifiEvent& reply) {
        ALOGD("In DebugCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        switch (mType) {
            case GET_DRV_VER:
            case GET_FW_VER:
            {
                const struct nlattr *data = (const struct nlattr *)reply.get_vendor_data();
                int len = reply.get_vendor_data_len();

                const struct nlattr *iter;
                int rem, type, datalen;

                ALOGD("len = %d, expected len = %d", len, *mBuffSize);

                nla_for_each_attr(iter, data, len, rem) {
                    type = nla_type(iter);
                    switch (type) {
                        case LOGGER_ATTRIBUTE_DRIVER_VER:
                        case LOGGER_ATTRIBUTE_FW_VER:
                            datalen = nla_len(iter);
                            if (datalen > *mBuffSize)
                                return NL_SKIP;
                            memcpy(mBuff, nla_data(iter), datalen);
                            ALOGD("WLAN %s version: %s", (type == LOGGER_ATTRIBUTE_DRIVER_VER) ? "Driver" : "Firmware", mBuff);
                            *mBuffSize = datalen;
                            break;
                        default:
                            return NL_SKIP;
                    }
                }
                break;
            }

            case START_RING_LOG:
            case GET_RING_DATA:
                break;

            case GET_RING_STATUS:
            {
                nlattr *vendor_data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);
                int len = reply.get_vendor_data_len();
                wifi_ring_buffer_status *status(mStatus);

                if (vendor_data == NULL || len == 0) {
                    ALOGE("No Debug data found");
                    return NL_SKIP;
                }

                nl_iterator it(vendor_data);
                if (it.get_type() == LOGGER_ATTRIBUTE_RING_NUM) {
                    unsigned int num_rings = it.get_u32();
                    if (*mNumRings < num_rings) {
                        ALOGE("Not enough status buffers provided, available: %d required: %d",
                                *mNumRings, num_rings);
                    } else {
                        *mNumRings = num_rings;
                    }
                } else {
                    ALOGE("Unknown attribute: %d expecting %d",
                            it.get_type(), LOGGER_ATTRIBUTE_RING_NUM);
                    return NL_SKIP;
                }

                it.next();
                for (unsigned int i = 0; it.has_next() && i < *mNumRings; it.next()) {
                    if (it.get_type() == LOGGER_ATTRIBUTE_RING_STATUS) {
                        memcpy(status, it.get_data(), sizeof(wifi_ring_buffer_status));
                        i++;
                        status++;
                    } else {
                        ALOGW("Ignoring invalid attribute type = %d, size = %d",
                                it.get_type(), it.get_len());
                    }
                }
                break;
            }

            case GET_FEATURE:
            {
                nlattr *data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);

                if (!mSupport) {
                    ALOGE("Buffers pointers not set");
                    return NL_SKIP;
                }

                nl_iterator it(data);
                /* do not care it.get_type() */
                memcpy(mSupport, it.get_data(), min(it.get_len(), (int) sizeof(*mSupport)));
                ALOGD("logger feature set: 0x%08X", *(int *)mSupport);
                break;
            }

            default:
                ALOGW("Unknown Debug command");
        }
        return NL_OK;
    }

    virtual int handleEvent(WifiEvent& event) {
        /* NO events! */
        return NL_SKIP;
    }
};

/* API to collect a firmware version string */
wifi_error wifi_get_firmware_version(wifi_interface_handle iface, char *buffer,
        int buffer_size)
{
    if (buffer && (buffer_size > 0)) {
        DebugCommand *cmd = new DebugCommand(iface, buffer, &buffer_size, GET_FW_VER);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        wifi_error result = (wifi_error)cmd->start();
        cmd->releaseRef();
        return result;
    } else {
        ALOGE("FW version buffer NULL");
        return  WIFI_ERROR_INVALID_ARGS;
    }
}

/* API to collect a driver version string */
wifi_error wifi_get_driver_version(wifi_interface_handle iface, char *buffer, int buffer_size)
{
    if (buffer && (buffer_size > 0)) {
        DebugCommand *cmd = new DebugCommand(iface, buffer, &buffer_size, GET_DRV_VER);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        wifi_error result = (wifi_error)cmd->start();
        cmd->releaseRef();
        return result;
    } else {
        ALOGE("Driver version buffer NULL");
        return  WIFI_ERROR_INVALID_ARGS;
    }
}

/* API to collect driver records */
wifi_error wifi_get_ring_data(wifi_interface_handle iface, char *ring_name)
{
    DebugCommand *cmd = new DebugCommand(iface, ring_name, GET_RING_DATA);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = (wifi_error)cmd->start();
    cmd->releaseRef();
    return result;
}

/* API to get the status of all ring buffers supported by driver */
wifi_error wifi_get_ring_buffers_status(wifi_interface_handle iface,
        u32 *num_rings, wifi_ring_buffer_status *status)
{
    if (status && num_rings) {
        DebugCommand *cmd = new DebugCommand(iface, num_rings, status, GET_RING_STATUS);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        wifi_error result = (wifi_error)cmd->start();
        cmd->releaseRef();
        return result;
    } else {
        ALOGE("Ring status buffer NULL");
        return  WIFI_ERROR_INVALID_ARGS;
    }
}

/* API to get supportable feature */
wifi_error wifi_get_logger_supported_feature_set(wifi_interface_handle iface,
        unsigned int *support)
{
    if (support) {
        DebugCommand *cmd = new DebugCommand(iface, support, GET_FEATURE);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        wifi_error result = (wifi_error)cmd->start();
        cmd->releaseRef();
        return result;
    } else {
        ALOGE("Get support buffer NULL");
        return  WIFI_ERROR_INVALID_ARGS;
    }
}

wifi_error wifi_start_logging(wifi_interface_handle iface, u32 verbose_level,
        u32 flags, u32 max_interval_sec, u32 min_data_size, char *ring_name)
{
    if (ring_name) {
        DebugCommand *cmd = new DebugCommand(iface, verbose_level, flags, max_interval_sec,
                    min_data_size, ring_name, START_RING_LOG);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        wifi_error result = (wifi_error)cmd->start();
        cmd->releaseRef();
        return result;
    } else {
        ALOGE("Ring name NULL");
        return  WIFI_ERROR_INVALID_ARGS;
    }
}


///////////////////////////////////////////////////////////////////////////////
class SetLogHandler : public WifiCommand
{
    wifi_ring_buffer_data_handler mHandler;

public:
    SetLogHandler(wifi_interface_handle iface, int id, wifi_ring_buffer_data_handler handler)
        : WifiCommand("SetLogHandler", iface, id), mHandler(handler)
    { }

    int start() {
        ALOGV("Register loghandler");
        registerVendorHandler(GOOGLE_OUI, GOOGLE_DEBUG_RING_EVENT);
        return WIFI_SUCCESS;
    }

    virtual int cancel() {
        /* Send a command to driver to stop generating logging events */
        ALOGV("Clear loghandler");

        /* unregister event handler */
        unregisterVendorHandler(GOOGLE_OUI, GOOGLE_DEBUG_RING_EVENT);

        WifiRequest request(familyId(), ifaceId());
        int result = request.create(GOOGLE_OUI, LOGGER_RESET_LOGGING);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create reset request; result = %d", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to request reset; result = %d", result);
            return result;
        }

        ALOGD("Success to clear loghandler");
        return WIFI_SUCCESS;
    }

    virtual int handleEvent(WifiEvent& event) {
        char *buffer = NULL;
        int buffer_size = 0;

        // ALOGD("In SetLogHandler::handleEvent");
        nlattr *vendor_data = event.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = event.get_vendor_data_len();
        int event_id = event.get_vendor_subcmd();
        // ALOGI("Got Logger event: %d", event_id);

        if (vendor_data == NULL || len == 0) {
            ALOGE("No Debug data found");
            return NL_SKIP;
        }

        if(event_id == GOOGLE_DEBUG_RING_EVENT) {
            wifi_ring_buffer_status status;
            memset(&status, 0, sizeof(status));

            for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
                if (it.get_type() == LOGGER_ATTRIBUTE_RING_STATUS) {
                    memcpy(&status, it.get_data(), sizeof(status));
                } else if (it.get_type() == LOGGER_ATTRIBUTE_RING_DATA) {
                    buffer_size = it.get_len();
                    buffer = (char *)it.get_data();
                } else {
                    ALOGW("Ignoring invalid attribute type = %d, size = %d",
                            it.get_type(), it.get_len());
                }
            }

            // ALOGI("Retrieved Debug data");
            if (mHandler.on_ring_buffer_data) {
                (*mHandler.on_ring_buffer_data)((char *)status.name, buffer, buffer_size,
                        &status);
            }
        } else {
            ALOGE("Unknown Event");
            return NL_SKIP;
        }
        return NL_OK;
    }
};

wifi_error wifi_set_log_handler(wifi_request_id id, wifi_interface_handle iface,
        wifi_ring_buffer_data_handler handler)
{
    wifi_handle handle = getWifiHandle(iface);
    ALOGV("Loghandler start, handle = %p", handle);

    SetLogHandler *cmd = new SetLogHandler(iface, id, handler);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = wifi_register_cmd(handle, id, cmd);
    if (result != WIFI_SUCCESS) {
        cmd->releaseRef();
        return result;
    }
    result = (wifi_error)cmd->start();
    if (result != WIFI_SUCCESS) {
        wifi_unregister_cmd(handle, id);
        cmd->releaseRef();
        return result;
    }
    return result;
}

wifi_error wifi_reset_log_handler(wifi_request_id id, wifi_interface_handle iface)
{
    wifi_handle handle = getWifiHandle(iface);
    ALOGV("Loghandler reset, wifi_request_id = %d, handle = %p", id, handle);

    if (id == -1) {
        wifi_ring_buffer_data_handler handler;
        memset(&handler, 0, sizeof(handler));

        SetLogHandler *cmd = new SetLogHandler(iface, id, handler);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        cmd->cancel();
        cmd->releaseRef();
        return WIFI_SUCCESS;
    }

    return wifi_cancel_cmd(id, iface);
}

///////////////////////////////////////////////////////////////////////////////
class SetAlertHandler : public WifiCommand
{
    wifi_alert_handler mHandler;
    int mBuffSize;
    char *mBuff;
    int mErrCode;

public:
    SetAlertHandler(wifi_interface_handle iface, int id, wifi_alert_handler handler)
        : WifiCommand("SetAlertHandler", iface, id), mHandler(handler), mBuffSize(0), mBuff(NULL),
            mErrCode(0)
    { }

    int start() {
        ALOGV("Start Alerting");
        registerVendorHandler(GOOGLE_OUI, GOOGLE_DEBUG_MEM_DUMP_EVENT);
        return WIFI_SUCCESS;
    }

    virtual int cancel() {
        ALOGV("Clear alerthandler");

        /* unregister alert handler */
        unregisterVendorHandler(GOOGLE_OUI, GOOGLE_DEBUG_MEM_DUMP_EVENT);
        wifi_unregister_cmd(wifiHandle(), id());
        ALOGD("Success to clear alerthandler");
        return WIFI_SUCCESS;
    }

    virtual int handleResponse(WifiEvent& reply) {
        ALOGD("In SetAlertHandler::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        nlattr *vendor_data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = reply.get_vendor_data_len();

        ALOGD("len = %d", len);
        if (vendor_data == NULL || len == 0) {
            ALOGE("no vendor data in memory dump response; ignoring it");
            return NL_SKIP;
        }

        for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
            if (it.get_type() == LOGGER_ATTRIBUTE_FW_DUMP_DATA) {
                ALOGI("Initiating alert callback");
                if (mHandler.on_alert) {
                    (*mHandler.on_alert)(id(), mBuff, mBuffSize, mErrCode);
                }
                if (mBuff) {
                    free(mBuff);
                    mBuff = NULL;
                }
            }
        }
        return NL_OK;
    }

    virtual int handleEvent(WifiEvent& event) {
        wifi_ring_buffer_id ring_id;
        char *buffer = NULL;
        int buffer_size = 0;


        nlattr *vendor_data = event.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = event.get_vendor_data_len();
        int event_id = event.get_vendor_subcmd();
        ALOGI("Got event: %d", event_id);

        if (vendor_data == NULL || len == 0) {
            ALOGE("No Debug data found");
            return NL_SKIP;
        }

        if (event_id == GOOGLE_DEBUG_MEM_DUMP_EVENT) {
            for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
                if (it.get_type() == LOGGER_ATTRIBUTE_FW_DUMP_LEN) {
                    mBuffSize = it.get_u32();
                } else if (it.get_type() == LOGGER_ATTRIBUTE_RING_DATA) {
                    buffer_size = it.get_len();
                    buffer = (char *)it.get_data();
            /*
                } else if (it.get_type() == LOGGER_ATTRIBUTE_FW_ERR_CODE) {
                    mErrCode = it.get_u32();
            */
                } else {
                    ALOGW("Ignoring invalid attribute type = %d, size = %d",
                            it.get_type(), it.get_len());
                }
            }
            if (mBuffSize) {
                ALOGD("dump size: %d meta data size: %d", mBuffSize, buffer_size);
                if (mBuff) free(mBuff);
                mBuff = (char *)malloc(mBuffSize + buffer_size);
                if (!mBuff) {
                    ALOGE("Buffer allocation failed");
                    return NL_SKIP;
                }
                memcpy(mBuff, buffer, buffer_size);

                WifiRequest request(familyId(), ifaceId());
                int result = request.create(GOOGLE_OUI, LOGGER_GET_MEM_DUMP);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to create get memory dump request; result = %d", result);
                    free(mBuff);
                    return NL_SKIP;
                }
                nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
                result = request.put_u32(LOGGER_ATTRIBUTE_FW_DUMP_LEN, mBuffSize);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to put get memory dump request; result = %d", result);
                    return result;
                }

                result = request.put_u64(LOGGER_ATTRIBUTE_FW_DUMP_DATA,
                         (uint64_t)(mBuff+buffer_size));
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to put get memory dump request; result = %d", result);
                    return result;
                }

                request.attr_end(data);
                mBuffSize += buffer_size;

                result = requestResponse(request);

                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to register get momory dump response; result = %d", result);
                }
            } else {
                ALOGE("dump event missing dump length attribute");
                return NL_SKIP;
            }
        }
        return NL_OK;
    }
};

wifi_error wifi_set_alert_handler(wifi_request_id id, wifi_interface_handle iface,
        wifi_alert_handler handler)
{
    wifi_handle handle = getWifiHandle(iface);
    ALOGV("Alerthandler start, handle = %p", handle);

    SetAlertHandler *cmd = new SetAlertHandler(iface, id, handler);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = wifi_register_cmd(handle, id, cmd);
    if (result != WIFI_SUCCESS) {
        cmd->releaseRef();
        return result;
    }
    result = (wifi_error)cmd->start();
    if (result != WIFI_SUCCESS) {
        wifi_unregister_cmd(handle, id);
        cmd->releaseRef();
        return result;
    }
    return result;
}

wifi_error wifi_reset_alert_handler(wifi_request_id id, wifi_interface_handle iface)
{
    wifi_handle handle = getWifiHandle(iface);
    ALOGV("Alerthandler reset, wifi_request_id = %d, handle = %p", id, handle);

    if (id == -1) {
        wifi_alert_handler handler;
        memset(&handler, 0, sizeof(handler));

        SetAlertHandler *cmd = new SetAlertHandler(iface, id, handler);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        cmd->cancel();
        cmd->releaseRef();
        return WIFI_SUCCESS;
    }

    return wifi_cancel_cmd(id, iface);
}

///////////////////////////////////////////////////////////////////////////////
class MemoryDumpCommand: public WifiCommand
{
    wifi_firmware_memory_dump_handler mHandler;
    int mBuffSize;
    char *mBuff;

public:
    MemoryDumpCommand(wifi_interface_handle iface, wifi_firmware_memory_dump_handler handler)
        : WifiCommand("MemoryDumpCommand", iface, 0), mHandler(handler), mBuffSize(0), mBuff(NULL)
    { }

    int start() {
        ALOGD("Start memory dump command");
        WifiRequest request(familyId(), ifaceId());

        int result = request.create(GOOGLE_OUI, LOGGER_TRIGGER_MEM_DUMP);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to create trigger fw memory dump request; result = %d", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to register trigger memory dump response; result = %d", result);
        }
        return result;
    }

    virtual int handleResponse(WifiEvent& reply) {
        ALOGD("In MemoryDumpCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        nlattr *vendor_data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = reply.get_vendor_data_len();

        ALOGD("len = %d", len);
        if (vendor_data == NULL || len == 0) {
            ALOGE("no vendor data in memory dump response; ignoring it");
            return NL_SKIP;
        }

        for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
            if (it.get_type() == LOGGER_ATTRIBUTE_FW_DUMP_LEN) {
                mBuffSize = it.get_u32();

                if (mBuff)
                    free(mBuff);
                mBuff = (char *)malloc(mBuffSize);
                if (!mBuff) {
                    ALOGE("Buffer allocation failed");
                    return NL_SKIP;
                }
                WifiRequest request(familyId(), ifaceId());
                int result = request.create(GOOGLE_OUI, LOGGER_GET_MEM_DUMP);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to create get memory dump request; result = %d", result);
                    free(mBuff);
                    return NL_SKIP;
                }

                nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
                result = request.put_u32(LOGGER_ATTRIBUTE_FW_DUMP_LEN, mBuffSize);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to put get memory dump request; result = %d", result);
                    return result;
                }

                result = request.put_u64(LOGGER_ATTRIBUTE_FW_DUMP_DATA, (uint64_t)mBuff);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to put get memory dump request; result = %d", result);
                    return result;
                }
                request.attr_end(data);

                result = requestResponse(request);
                if (result != WIFI_SUCCESS) {
                    ALOGE("Failed to register get momory dump response; result = %d", result);
                }
            } else if (it.get_type() == LOGGER_ATTRIBUTE_FW_DUMP_DATA) {
                ALOGI("Initiating memory dump callback");
                if (mHandler.on_firmware_memory_dump) {
                    (*mHandler.on_firmware_memory_dump)(mBuff, mBuffSize);
                }
                if (mBuff) {
                    free(mBuff);
                    mBuff = NULL;
                }
            } else {
                ALOGW("Ignoring invalid attribute type = %d, size = %d",
                        it.get_type(), it.get_len());
            }
        }
        return NL_OK;
    }

    virtual int handleEvent(WifiEvent& event) {
        /* NO events! */
        return NL_SKIP;
    }
};

/* API to collect a firmware memory dump for a given iface */
wifi_error wifi_get_firmware_memory_dump( wifi_interface_handle iface,
        wifi_firmware_memory_dump_handler handler)
{
    MemoryDumpCommand *cmd = new MemoryDumpCommand(iface, handler);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = (wifi_error)cmd->start();
    cmd->releaseRef();
    return result;
}

class PacketFateCommand: public WifiCommand
{
    void *mReportBufs;
    size_t mNoReqFates;
    size_t *mNoProvidedFates;
    PktFateReqType mReqType;

public:
    PacketFateCommand(wifi_interface_handle handle)
        : WifiCommand("PacketFateCommand", handle, 0), mReqType(PACKET_MONITOR_START)
    { }

    PacketFateCommand(wifi_interface_handle handle, wifi_tx_report *tx_report_bufs,
            size_t n_requested_fates, size_t *n_provided_fates)
        : WifiCommand("PacketFateCommand", handle, 0), mReportBufs(tx_report_bufs),
                  mNoReqFates(n_requested_fates), mNoProvidedFates(n_provided_fates),
                  mReqType(TX_PACKET_FATE)
    { }

    PacketFateCommand(wifi_interface_handle handle, wifi_rx_report *rx_report_bufs,
            size_t n_requested_fates, size_t *n_provided_fates)
        : WifiCommand("PacketFateCommand", handle, 0), mReportBufs(rx_report_bufs),
                  mNoReqFates(n_requested_fates), mNoProvidedFates(n_provided_fates),
                  mReqType(RX_PACKET_FATE)
    { }

    int createRequest(WifiRequest& request) {
        if (mReqType == TX_PACKET_FATE) {
            ALOGD("%s Get Tx packet fate request\n", __FUNCTION__);
            return createTxPktFateRequest(request);
        } else if (mReqType == RX_PACKET_FATE) {
            ALOGD("%s Get Rx packet fate request\n", __FUNCTION__);
            return createRxPktFateRequest(request);
        } else if (mReqType == PACKET_MONITOR_START) {
            ALOGD("%s Monitor packet fate request\n", __FUNCTION__);
            return createMonitorPktFateRequest(request);
        } else {
            ALOGE("%s Unknown packet fate request\n", __FUNCTION__);
            return WIFI_ERROR_NOT_SUPPORTED;
        }
        return WIFI_SUCCESS;
    }

    int createMonitorPktFateRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, LOGGER_START_PKT_FATE_MONITORING);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        request.attr_end(data);
        return result;
    }

    int createTxPktFateRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, LOGGER_GET_TX_PKT_FATES);
        if (result < 0) {
            return result;
        }

        memset(mReportBufs, 0, (mNoReqFates * sizeof(wifi_tx_report)));
        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
#if 0//AIDEN Temp comment it for pass gms
        result = request.put_u32(LOGGER_ATTRIBUTE_PKT_FATE_NUM, mNoReqFates);
        if (result < 0) {
            return result;
        }
        result = request.put_u64(LOGGER_ATTRIBUTE_PKT_FATE_DATA, (uint64_t)mReportBufs);
        if (result < 0) {
            return result;
        }
#endif
        request.attr_end(data);
        return result;
    }

    int createRxPktFateRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, LOGGER_GET_RX_PKT_FATES);
        if (result < 0) {
            return result;
        }

        memset(mReportBufs, 0, (mNoReqFates * sizeof(wifi_rx_report)));
        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
#if 0//AIDEN Temp comment it for pass gms
        result = request.put_u32(LOGGER_ATTRIBUTE_PKT_FATE_NUM, mNoReqFates);
        if (result < 0) {
            return result;
        }
        result = request.put_u64(LOGGER_ATTRIBUTE_PKT_FATE_DATA, (uint64_t)mReportBufs);
        if (result < 0) {
            return result;
        }
#endif
        request.attr_end(data);
        return result;
    }

    int start() {
        ALOGD("Start get packet fate command\n");
        WifiRequest request(familyId(), ifaceId());

        int result = createRequest(request);
        if (result < 0) {
            ALOGE("Failed to create get pkt fate request; result = %d\n", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to register get pkt fate response; result = %d\n", result);
        }
        return result;
    }

    int handleResponse(WifiEvent& reply) {
        ALOGD("In GetPktFateCommand::handleResponse\n");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGI("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();
        nlattr *vendor_data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = reply.get_vendor_data_len();

        ALOGI("Id = %0x, subcmd = %d, len = %d", id, subcmd, len);

        if (mReqType == TX_PACKET_FATE) {
            ALOGI("Response recieved for get TX pkt fate command\n");
        } else if (mReqType == RX_PACKET_FATE) {
            ALOGI("Response recieved for get RX pkt fate command\n");
        } else if (mReqType == PACKET_MONITOR_START) {
            ALOGI("Response recieved for monitor pkt fate command\n");
            return NL_OK;
        } else {
            ALOGE("Response recieved for unknown pkt fate command\n");
            return NL_SKIP;
        }

        if (vendor_data == NULL || len == 0) {
            ALOGE("no vendor data in GetPktFateCommand response; ignoring it\n");
            return NL_SKIP;
        }

        for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
            if (it.get_type() == LOGGER_ATTRIBUTE_PKT_FATE_NUM) {
                *mNoProvidedFates = it.get_u32();
                ALOGI("No: of pkt fates provided is %d\n", *mNoProvidedFates);
            } else {
                ALOGE("Ignoring invalid attribute type = %d, size = %d\n",
                        it.get_type(), it.get_len());
            }
        }

        return NL_OK;
    }

    int handleEvent(WifiEvent& event) {
        /* NO events to handle here! */
        return NL_SKIP;
    }
};

class GetWakeReasonCountCommand : public WifiCommand {
    WLAN_DRIVER_WAKE_REASON_CNT *mWakeReasonCnt;
    void *mCmdEventWakeCount;
    public:
    GetWakeReasonCountCommand(wifi_interface_handle handle,
        WLAN_DRIVER_WAKE_REASON_CNT *wlanDriverWakeReasonCount) :
        WifiCommand("GetWakeReasonCountCommand", handle, 0),
        mWakeReasonCnt(wlanDriverWakeReasonCount)
    {
        mCmdEventWakeCount = mWakeReasonCnt->cmd_event_wake_cnt;
    }

    int createRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, LOGGER_GET_WAKE_REASON_STATS);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);

        request.attr_end(data);
        return WIFI_SUCCESS;
    }

    int start() {
        ALOGD("Start get wake stats command\n");
        WifiRequest request(familyId(), ifaceId());

        int result = createRequest(request);
        if (result < 0) {
            ALOGE("Failed to create request result = %d\n", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to register wake stats  response; result = %d\n", result);
        }
        return result;
    }

    protected:
    int handleResponse(WifiEvent& reply) {
        ALOGE("In GetWakeReasonCountCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        nlattr *vendor_data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = reply.get_vendor_data_len();

        ALOGV("Id = %0x, subcmd = %d, len = %d", id, subcmd, len);
        if (vendor_data == NULL || len == 0) {
            ALOGE("no vendor data in GetGetWakeReasonCountCommand response; ignoring it");
            return NL_SKIP;
        }

        for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
            switch (it.get_type()) {
                case WAKE_STAT_ATTRIBUTE_TOTAL_DRIVER_FW:
                    mWakeReasonCnt->total_driver_fw_local_wake =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_TOTAL_CMD_EVENT:
                    mWakeReasonCnt->total_cmd_event_wake =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_CMD_COUNT_USED:
                    mWakeReasonCnt->cmd_event_wake_cnt_used =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_CMD_EVENT_WAKE:
                    memcpy(mCmdEventWakeCount, it.get_data(),
                            (mWakeReasonCnt->cmd_event_wake_cnt_used * sizeof(int)));
                    break;
                case WAKE_STAT_ATTRIBUTE_TOTAL_RX_DATA_WAKE:
                    mWakeReasonCnt->total_rx_data_wake =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_UNICAST_COUNT:
                    mWakeReasonCnt->rx_wake_details.rx_unicast_cnt =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_MULTICAST_COUNT:
                    mWakeReasonCnt->rx_wake_details.rx_multicast_cnt =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_BROADCAST_COUNT:
                    mWakeReasonCnt->rx_wake_details.rx_broadcast_cnt =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_ICMP_PKT:
                    mWakeReasonCnt->rx_wake_pkt_classification_info.icmp_pkt =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_ICMP6_PKT:
                    mWakeReasonCnt->rx_wake_pkt_classification_info.icmp6_pkt =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_ICMP6_RA:
                    mWakeReasonCnt->rx_wake_pkt_classification_info.icmp6_ra =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_ICMP6_NA:
                    mWakeReasonCnt->rx_wake_pkt_classification_info.icmp6_na =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_RX_ICMP6_NS:
                    mWakeReasonCnt->rx_wake_pkt_classification_info.icmp6_ns =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_IPV4_RX_MULTICAST_ADD_CNT:
                    mWakeReasonCnt->rx_multicast_wake_pkt_info.ipv4_rx_multicast_addr_cnt =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_IPV6_RX_MULTICAST_ADD_CNT:
                    mWakeReasonCnt->rx_multicast_wake_pkt_info.ipv6_rx_multicast_addr_cnt =
                        it.get_u32();
                    break;
                case WAKE_STAT_ATTRIBUTE_OTHER__RX_MULTICAST_ADD_CNT:
                    mWakeReasonCnt->rx_multicast_wake_pkt_info.other_rx_multicast_addr_cnt =
                        it.get_u32();
                    break;
                default:
                    break;
            }

        }
        return NL_OK;
    }
};

wifi_error wifi_start_pkt_fate_monitoring(wifi_interface_handle handle)
{
    PacketFateCommand *cmd = new PacketFateCommand(handle);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = (wifi_error)cmd->start();
    cmd->releaseRef();
    return result;
}

wifi_error wifi_get_tx_pkt_fates(wifi_interface_handle handle,
        wifi_tx_report *tx_report_bufs, size_t n_requested_fates,
        size_t *n_provided_fates)
{
    PacketFateCommand *cmd = new PacketFateCommand(handle, tx_report_bufs,
                n_requested_fates, n_provided_fates);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = (wifi_error)cmd->start();
    cmd->releaseRef();
    return result;
}

wifi_error wifi_get_rx_pkt_fates(wifi_interface_handle handle,
        wifi_rx_report *rx_report_bufs, size_t n_requested_fates,
        size_t *n_provided_fates)
{
    PacketFateCommand *cmd = new PacketFateCommand(handle, rx_report_bufs,
                n_requested_fates, n_provided_fates);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = (wifi_error)cmd->start();
    cmd->releaseRef();
    return result;
}

wifi_error wifi_get_wake_reason_stats(wifi_interface_handle handle,
        WLAN_DRIVER_WAKE_REASON_CNT *wifi_wake_reason_cnt)
{
    GetWakeReasonCountCommand *cmd =
        new GetWakeReasonCountCommand(handle, wifi_wake_reason_cnt);
    wifi_error result = (wifi_error)cmd->start();
    cmd->releaseRef();
    return result;
}
