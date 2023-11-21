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
#include <utils/String8.h>

#include "wifi_hal.h"
#include "common.h"
#include "cpp_bindings.h"

using namespace android;
#define RTT_RESULT_SIZE (sizeof(wifi_rtt_result));
typedef enum {

    RTT_SUBCMD_SET_CONFIG = ANDROID_NL80211_SUBCMD_RTT_RANGE_START,
    RTT_SUBCMD_CANCEL_CONFIG,
    RTT_SUBCMD_GETCAPABILITY,
    RTT_SUBCMD_GETAVAILCHANNEL,
    RTT_SUBCMD_SET_RESPONDER,
    RTT_SUBCMD_CANCEL_RESPONDER,
} RTT_SUB_COMMAND;

typedef enum {
    RTT_ATTRIBUTE_TARGET_CNT = 0,
    RTT_ATTRIBUTE_TARGET_INFO,
    RTT_ATTRIBUTE_TARGET_MAC,
    RTT_ATTRIBUTE_TARGET_TYPE,
    RTT_ATTRIBUTE_TARGET_PEER,
    RTT_ATTRIBUTE_TARGET_CHAN,
    RTT_ATTRIBUTE_TARGET_PERIOD,
    RTT_ATTRIBUTE_TARGET_NUM_BURST,
    RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST,
    RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM,
    RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR,
    RTT_ATTRIBUTE_TARGET_LCI,
    RTT_ATTRIBUTE_TARGET_LCR,
    RTT_ATTRIBUTE_TARGET_BURST_DURATION,
    RTT_ATTRIBUTE_TARGET_PREAMBLE,
    RTT_ATTRIBUTE_TARGET_BW,
    RTT_ATTRIBUTE_RESULTS_COMPLETE = 30,
    RTT_ATTRIBUTE_RESULTS_PER_TARGET,
    RTT_ATTRIBUTE_RESULT_CNT,
    RTT_ATTRIBUTE_RESULT
} RTT_ATTRIBUTE;
typedef struct strmap_entry {
    int			id;
    String8		text;
} strmap_entry_t;
struct dot11_rm_ie {
    u8 id;
    u8 len;
    u8 token;
    u8 mode;
    u8 type;
} __attribute__ ((packed));
typedef struct dot11_rm_ie dot11_rm_ie_t;
#define DOT11_HDR_LEN 2
#define DOT11_RM_IE_LEN       5
#define DOT11_MNG_MEASURE_REQUEST_ID		38	/* 11H MeasurementRequest */
#define DOT11_MEASURE_TYPE_LCI		8   /* d11 measurement LCI type */
#define DOT11_MEASURE_TYPE_CIVICLOC	11  /* d11 measurement location civic */

static const strmap_entry_t err_info[] = {
    {RTT_STATUS_SUCCESS, String8("Success")},
    {RTT_STATUS_FAILURE, String8("Failure")},
    {RTT_STATUS_FAIL_NO_RSP, String8("No reponse")},
    {RTT_STATUS_FAIL_INVALID_TS, String8("Invalid Timestamp")},
    {RTT_STATUS_FAIL_PROTOCOL, String8("Protocol error")},
    {RTT_STATUS_FAIL_REJECTED, String8("Rejected")},
    {RTT_STATUS_FAIL_NOT_SCHEDULED_YET, String8("not scheduled")},
    {RTT_STATUS_FAIL_SCHEDULE,  String8("schedule failed")},
    {RTT_STATUS_FAIL_TM_TIMEOUT, String8("timeout")},
    {RTT_STATUS_FAIL_AP_ON_DIFF_CHANNEL, String8("AP is on difference channel")},
    {RTT_STATUS_FAIL_NO_CAPABILITY, String8("no capability")},
    {RTT_STATUS_FAIL_BUSY_TRY_LATER, String8("busy and try later")},
    {RTT_STATUS_ABORTED, String8("aborted")}
};

    static const char*
get_err_info(int status)
{
    int i;
    const strmap_entry_t *p_entry;
    int num_entries = sizeof(err_info)/ sizeof(err_info[0]);
    /* scan thru the table till end */
    p_entry = err_info;
    for (i = 0; i < (int) num_entries; i++)
    {
        if (p_entry->id == status)
            return p_entry->text;
        p_entry++;		/* next entry */
    }
    return "unknown error";			/* not found */
}

class GetRttCapabilitiesCommand : public WifiCommand
{
    wifi_rtt_capabilities *mCapabilities;
public:
    GetRttCapabilitiesCommand(wifi_interface_handle iface, wifi_rtt_capabilities *capabitlites)
        : WifiCommand("GetRttCapabilitiesCommand", iface, 0), mCapabilities(capabitlites)
    {
        memset(mCapabilities, 0, sizeof(*mCapabilities));
    }

    virtual int create() {
        ALOGD("Creating message to get scan capablities; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_GETCAPABILITY);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {

        ALOGD("In GetRttCapabilitiesCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        void *data = reply.get_vendor_data();
        int len = reply.get_vendor_data_len();

        ALOGD("Id = %0x, subcmd = %d, len = %d, expected len = %d", id, subcmd, len,
                sizeof(*mCapabilities));

        memcpy(mCapabilities, data, min(len, (int) sizeof(*mCapabilities)));

        return NL_OK;
    }
};


class GetRttResponderInfoCommand : public WifiCommand
{
    wifi_rtt_responder* mResponderInfo;
public:
    GetRttResponderInfoCommand(wifi_interface_handle iface, wifi_rtt_responder *responderInfo)
        : WifiCommand("GetRttResponderInfoCommand", iface, 0), mResponderInfo(responderInfo)
    {
        memset(mResponderInfo, 0 , sizeof(*mResponderInfo));

    }

    virtual int create() {
        ALOGD("Creating message to get responder info ; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_GETAVAILCHANNEL);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {

        ALOGD("In GetRttResponderInfoCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        void *data = reply.get_vendor_data();
        int len = reply.get_vendor_data_len();

        ALOGD("Id = %0x, subcmd = %d, len = %d, expected len = %d", id, subcmd, len,
                sizeof(*mResponderInfo));

        memcpy(mResponderInfo, data, min(len, (int) sizeof(*mResponderInfo)));

        return NL_OK;
    }
};


class EnableResponderCommand : public WifiCommand
{
    wifi_channel_info  mChannelInfo;
    wifi_rtt_responder* mResponderInfo;
    unsigned m_max_duration_sec;
public:
    EnableResponderCommand(wifi_interface_handle iface, int id, wifi_channel_info channel_hint,
            unsigned max_duration_seconds, wifi_rtt_responder *responderInfo)
            : WifiCommand("EnableResponderCommand", iface, 0), mChannelInfo(channel_hint),
            m_max_duration_sec(max_duration_seconds), mResponderInfo(responderInfo)
    {
        memset(mResponderInfo, 0, sizeof(*mResponderInfo));
    }

    virtual int create() {
        ALOGD("Creating message to set responder ; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_SET_RESPONDER);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {

        ALOGD("In EnableResponderCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        void *data = reply.get_vendor_data();
        int len = reply.get_vendor_data_len();

        ALOGD("Id = %0x, subcmd = %d, len = %d, expected len = %d", id, subcmd, len,
                sizeof(*mResponderInfo));

        memcpy(mResponderInfo, data, min(len, (int) sizeof(*mResponderInfo)));

        return NL_OK;
    }
};


class CancelResponderCommand : public WifiCommand
{

public:
    CancelResponderCommand(wifi_interface_handle iface, int id)
        : WifiCommand("CancelResponderCommand", iface, 0)/*, mChannelInfo(channel)*/
    {

    }

    virtual int create() {
        ALOGD("Creating message to cancel responder ; iface = %d", mIfaceInfo->id);

        int ret = mMsg.create(GOOGLE_OUI, RTT_SUBCMD_CANCEL_RESPONDER);
        if (ret < 0) {
            return ret;
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {
        /* Nothing to do on response! */
        return NL_SKIP;
    }

};


class RttCommand : public WifiCommand
{
    unsigned numRttParams;
    int mCompleted;
    int currentIdx;
    int totalCnt;
    static const int MAX_RESULTS = 1024;
    wifi_rtt_result *rttResults[MAX_RESULTS];
    wifi_rtt_config *rttParams;
    wifi_rtt_event_handler rttHandler;
public:
    RttCommand(wifi_interface_handle iface, int id, unsigned num_rtt_config,
            wifi_rtt_config rtt_config[], wifi_rtt_event_handler handler)
        : WifiCommand("RttCommand", iface, id), numRttParams(num_rtt_config), rttParams(rtt_config),
        rttHandler(handler)
    {
        memset(rttResults, 0, sizeof(rttResults));
        currentIdx = 0;
        mCompleted = 0;
        totalCnt = 0;
    }

    RttCommand(wifi_interface_handle iface, int id)
        : WifiCommand("RttCommand", iface, id)
    {
        currentIdx = 0;
        mCompleted = 0;
        totalCnt = 0;
        numRttParams = 0;
    }

    int createSetupRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, RTT_SUBCMD_SET_CONFIG);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        result = request.put_u8(RTT_ATTRIBUTE_TARGET_CNT, numRttParams);
        if (result < 0) {
            return result;
        }
        nlattr *rtt_config = request.attr_start(RTT_ATTRIBUTE_TARGET_INFO);
        for (unsigned i = 0; i < numRttParams; i++) {
            nlattr *attr2 = request.attr_start(i);
            if (attr2 == NULL) {
                return WIFI_ERROR_OUT_OF_MEMORY;
            }

            result = request.put_addr(RTT_ATTRIBUTE_TARGET_MAC, rttParams[i].addr);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_TYPE, rttParams[i].type);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_PEER, rttParams[i].peer);
            if (result < 0) {
                return result;
            }

            result = request.put(RTT_ATTRIBUTE_TARGET_CHAN, &rttParams[i].channel,
                    sizeof(wifi_channel_info));
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_BURST, rttParams[i].num_burst);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST,
                    rttParams[i].num_frames_per_burst);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM,
                    rttParams[i].num_retries_per_rtt_frame);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR,
                    rttParams[i].num_retries_per_ftmr);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_PERIOD,
                    rttParams[i].burst_period);
            if (result < 0) {
                return result;
            }

            result = request.put_u32(RTT_ATTRIBUTE_TARGET_BURST_DURATION,
                    rttParams[i].burst_duration);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_LCI,
                    rttParams[i].LCI_request);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_LCR,
                    rttParams[i].LCR_request);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_BW,
                    rttParams[i].bw);
            if (result < 0) {
                return result;
            }

            result = request.put_u8(RTT_ATTRIBUTE_TARGET_PREAMBLE,
                    rttParams[i].preamble);
            if (result < 0) {
                return result;
            }
            request.attr_end(attr2);
        }

        request.attr_end(rtt_config);
        request.attr_end(data);
        return WIFI_SUCCESS;
    }

    int createTeardownRequest(WifiRequest& request, unsigned num_devices, mac_addr addr[]) {
        int result = request.create(GOOGLE_OUI, RTT_SUBCMD_CANCEL_CONFIG);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        request.put_u8(RTT_ATTRIBUTE_TARGET_CNT, num_devices);
        for(unsigned i = 0; i < num_devices; i++) {
            result = request.put_addr(RTT_ATTRIBUTE_TARGET_MAC, addr[i]);
            if (result < 0) {
                return result;
            }
        }
        request.attr_end(data);
        return result;
    }
    int start() {
        ALOGD("Setting RTT configuration");
        WifiRequest request(familyId(), ifaceId());
        int result = createSetupRequest(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create setup request; result = %d", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to configure RTT setup; result = %d", result);
            return result;
        }

        registerVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
        ALOGI("Successfully started RTT operation");
        return result;
    }

    virtual int cancel() {
        ALOGD("Stopping RTT");

        WifiRequest request(familyId(), ifaceId());
        int result = createTeardownRequest(request, 0, NULL);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create stop request; result = %d", result);
        } else {
            result = requestResponse(request);
            if (result != WIFI_SUCCESS) {
                ALOGE("failed to stop scan; result = %d", result);
            }
        }

        unregisterVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
        return WIFI_SUCCESS;
    }

    int cancel_specific(unsigned num_devices, mac_addr addr[]) {
        ALOGE("Stopping RTT");

        WifiRequest request(familyId(), ifaceId());
        int result = createTeardownRequest(request, num_devices, addr);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create stop request; result = %d", result);
        } else {
            result = requestResponse(request);
            if (result != WIFI_SUCCESS) {
                ALOGE("failed to stop RTT; result = %d", result);
            }
        }

        unregisterVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
        return WIFI_SUCCESS;
    }

    virtual int handleResponse(WifiEvent& reply) {
        /* Nothing to do on response! */
        return NL_SKIP;
    }

    virtual int handleEvent(WifiEvent& event) {
        ALOGI("Got an RTT event");
        nlattr *vendor_data = event.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = event.get_vendor_data_len();
        if (vendor_data == NULL || len == 0) {
            ALOGI("No rtt results found");
        }
        for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
            if (it.get_type() == RTT_ATTRIBUTE_RESULTS_COMPLETE) {
                mCompleted = it.get_u32();
                ALOGI("retrieved completed flag : %d\n", mCompleted);
            } else if (it.get_type() == RTT_ATTRIBUTE_RESULTS_PER_TARGET) {
                int result_cnt = 0;
                mac_addr bssid;
                for (nl_iterator it2(it.get()); it2.has_next(); it2.next()) {
                    if (it2.get_type() == RTT_ATTRIBUTE_TARGET_MAC) {
                        memcpy(bssid, it2.get_data(), sizeof(mac_addr));
                        ALOGI("retrived target mac : %02x:%02x:%02x:%02x:%02x:%02x\n",
                                bssid[0],
                                bssid[1],
                                bssid[2],
                                bssid[3],
                                bssid[4],
                                bssid[5]);
                    } else if (it2.get_type() == RTT_ATTRIBUTE_RESULT_CNT) {
                        result_cnt = it2.get_u32();
                        ALOGI("retrieved result_cnt : %d\n", result_cnt);
                    } else if (it2.get_type() == RTT_ATTRIBUTE_RESULT) {
                        int result_len = it2.get_len();
                        rttResults[currentIdx] =  (wifi_rtt_result *)malloc(it2.get_len());
                        wifi_rtt_result *rtt_result = rttResults[currentIdx];
                        if (rtt_result == NULL) {
                            mCompleted = 1;
                            ALOGE("failed to allocate the wifi_rtt_result\n");
                            break;
                        }
                        memcpy(rtt_result, it2.get_data(), it2.get_len());
                        result_len -= sizeof(wifi_rtt_result);
                        if (result_len > 0) {
                            result_len -= sizeof(wifi_rtt_result);
                            dot11_rm_ie_t *ele_1;
                            dot11_rm_ie_t *ele_2;
                            /* The result has LCI or LCR element */
                            ele_1 = (dot11_rm_ie_t *)(rtt_result + 1);
                            if (ele_1->id == DOT11_MNG_MEASURE_REQUEST_ID) {
                                if (ele_1->type == DOT11_MEASURE_TYPE_LCI) {
                                    rtt_result->LCI = (wifi_information_element *)ele_1;
                                    result_len -= (ele_1->len + DOT11_HDR_LEN);
                                    /* get a next rm ie */
                                    if (result_len > 0) {
                                        ele_2 = (dot11_rm_ie_t *)((char *)ele_1 + (ele_1->len + DOT11_HDR_LEN));
                                        if ((ele_2->id == DOT11_MNG_MEASURE_REQUEST_ID) &&
                                                (ele_2->type == DOT11_MEASURE_TYPE_CIVICLOC)) {
                                            rtt_result->LCR = (wifi_information_element *)ele_2;
                                        }
                                    }
                                } else if (ele_1->type == DOT11_MEASURE_TYPE_CIVICLOC){
                                    rtt_result->LCR = (wifi_information_element *)ele_1;
                                    result_len -= (ele_1->len + DOT11_HDR_LEN);
                                    /* get a next rm ie */
                                    if (result_len > 0) {
                                        ele_2 = (dot11_rm_ie_t *)((char *)ele_1 + (ele_1->len + DOT11_HDR_LEN));
                                        if ((ele_2->id == DOT11_MNG_MEASURE_REQUEST_ID) &&
                                                (ele_2->type == DOT11_MEASURE_TYPE_LCI)) {
                                            rtt_result->LCI = (wifi_information_element *)ele_2;
                                        }
                                    }
                                }
                            }
                        }
                        totalCnt++;
                        ALOGI("retrived rtt_result : \n\tburst_num :%d, measurement_number : %d, success_number : %d\n"
                                "\tnumber_per_burst_peer : %d, status : %s, retry_after_duration : %d s\n"
                                "\trssi : %d dbm, rx_rate : %d Kbps, rtt : %llu ns, rtt_sd : %llu\n"
                                "\tdistance : %d, burst_duration : %d ms, negotiated_burst_num : %d\n",
                                rtt_result->burst_num, rtt_result->measurement_number,
                                rtt_result->success_number, rtt_result->number_per_burst_peer,
                                get_err_info(rtt_result->status), rtt_result->retry_after_duration,
                                rtt_result->rssi, rtt_result->rx_rate.bitrate * 100,
                                rtt_result->rtt/10, rtt_result->rtt_sd, rtt_result->distance_mm / 10,
                                rtt_result->burst_duration, rtt_result->negotiated_burst_num);
                        currentIdx++;
                    }
                }
            }

        }
        if (mCompleted) {
            unregisterVendorHandler(GOOGLE_OUI, RTT_EVENT_COMPLETE);
            (*rttHandler.on_rtt_results)(id(), totalCnt, rttResults);
            for (int i = 0; i < currentIdx; i++) {
                free(rttResults[i]);
                rttResults[i] = NULL;
            }
            totalCnt = currentIdx = 0;
            WifiCommand *cmd = wifi_unregister_cmd(wifiHandle(), id());
            if (cmd)
                cmd->releaseRef();
        }
        return NL_SKIP;
    }
};


/* API to request RTT measurement */
wifi_error wifi_rtt_range_request(wifi_request_id id, wifi_interface_handle iface,
        unsigned num_rtt_config, wifi_rtt_config rtt_config[], wifi_rtt_event_handler handler)
{
    wifi_handle handle = getWifiHandle(iface);
    RttCommand *cmd = new RttCommand(iface, id, num_rtt_config, rtt_config, handler);
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

/* API to cancel RTT measurements */
wifi_error wifi_rtt_range_cancel(wifi_request_id id,  wifi_interface_handle iface,
        unsigned num_devices, mac_addr addr[])
{
    wifi_handle handle = getWifiHandle(iface);
    RttCommand *cmd = new RttCommand(iface, id);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    cmd->cancel_specific(num_devices, addr);
    cmd->releaseRef();
    return WIFI_SUCCESS;
}

/* API to get RTT capability */
wifi_error wifi_get_rtt_capabilities(wifi_interface_handle iface,
        wifi_rtt_capabilities *capabilities)
{
    GetRttCapabilitiesCommand command(iface, capabilities);
    return (wifi_error) command.requestResponse();
}

/* API to get the responder information */
wifi_error wifi_rtt_get_responder_info(wifi_interface_handle iface,
        wifi_rtt_responder* responderInfo)
{
    GetRttResponderInfoCommand command(iface, responderInfo);
    return (wifi_error) command.requestResponse();

}

/**
 * Enable RTT responder mode.
 * channel_hint - hint of the channel information where RTT responder should be enabled on.
 * max_duration_seconds - timeout of responder mode.
 * wifi_rtt_responder - information for RTT responder e.g. channel used and preamble supported.
 */
wifi_error wifi_enable_responder(wifi_request_id id, wifi_interface_handle iface,
                                wifi_channel_info channel_hint, unsigned max_duration_seconds,
                                wifi_rtt_responder* responderInfo)
{
    EnableResponderCommand command(iface, id, channel_hint, max_duration_seconds, responderInfo);
    return (wifi_error) command.requestResponse();
}

/**
 * Disable RTT responder mode.
 */
wifi_error wifi_disable_responder(wifi_request_id id, wifi_interface_handle iface)
{
    CancelResponderCommand command(iface, id);
    return (wifi_error) command.requestResponse();
}

