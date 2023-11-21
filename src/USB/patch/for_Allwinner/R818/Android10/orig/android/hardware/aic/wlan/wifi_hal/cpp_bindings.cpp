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
#include <netlink/handlers.h>

#include <ctype.h>

#include "wifi_hal.h"
#include "common.h"
#include "cpp_bindings.h"

void appendFmt(char *buf, int &offset, const char *fmt, ...)
{
    va_list params;
    va_start(params, fmt);
    offset += vsprintf(buf + offset, fmt, params);
    va_end(params);
}

#define C2S(x)  case x: return #x;

static const char *cmdToString(int cmd)
{
	switch (cmd) {
	C2S(NL80211_CMD_UNSPEC)
	C2S(NL80211_CMD_GET_WIPHY)
	C2S(NL80211_CMD_SET_WIPHY)
	C2S(NL80211_CMD_NEW_WIPHY)
	C2S(NL80211_CMD_DEL_WIPHY)
	C2S(NL80211_CMD_GET_INTERFACE)
	C2S(NL80211_CMD_SET_INTERFACE)
	C2S(NL80211_CMD_NEW_INTERFACE)
	C2S(NL80211_CMD_DEL_INTERFACE)
	C2S(NL80211_CMD_GET_KEY)
	C2S(NL80211_CMD_SET_KEY)
	C2S(NL80211_CMD_NEW_KEY)
	C2S(NL80211_CMD_DEL_KEY)
	C2S(NL80211_CMD_GET_BEACON)
	C2S(NL80211_CMD_SET_BEACON)
	C2S(NL80211_CMD_START_AP)
	C2S(NL80211_CMD_STOP_AP)
	C2S(NL80211_CMD_GET_STATION)
	C2S(NL80211_CMD_SET_STATION)
	C2S(NL80211_CMD_NEW_STATION)
	C2S(NL80211_CMD_DEL_STATION)
	C2S(NL80211_CMD_GET_MPATH)
	C2S(NL80211_CMD_SET_MPATH)
	C2S(NL80211_CMD_NEW_MPATH)
	C2S(NL80211_CMD_DEL_MPATH)
	C2S(NL80211_CMD_SET_BSS)
	C2S(NL80211_CMD_SET_REG)
	C2S(NL80211_CMD_REQ_SET_REG)
	C2S(NL80211_CMD_GET_MESH_CONFIG)
	C2S(NL80211_CMD_SET_MESH_CONFIG)
	C2S(NL80211_CMD_SET_MGMT_EXTRA_IE)
	C2S(NL80211_CMD_GET_REG)
	C2S(NL80211_CMD_GET_SCAN)
	C2S(NL80211_CMD_TRIGGER_SCAN)
	C2S(NL80211_CMD_NEW_SCAN_RESULTS)
	C2S(NL80211_CMD_SCAN_ABORTED)
	C2S(NL80211_CMD_REG_CHANGE)
	C2S(NL80211_CMD_AUTHENTICATE)
	C2S(NL80211_CMD_ASSOCIATE)
	C2S(NL80211_CMD_DEAUTHENTICATE)
	C2S(NL80211_CMD_DISASSOCIATE)
	C2S(NL80211_CMD_MICHAEL_MIC_FAILURE)
	C2S(NL80211_CMD_REG_BEACON_HINT)
	C2S(NL80211_CMD_JOIN_IBSS)
	C2S(NL80211_CMD_LEAVE_IBSS)
	C2S(NL80211_CMD_TESTMODE)
	C2S(NL80211_CMD_CONNECT)
	C2S(NL80211_CMD_ROAM)
	C2S(NL80211_CMD_DISCONNECT)
	C2S(NL80211_CMD_SET_WIPHY_NETNS)
	C2S(NL80211_CMD_GET_SURVEY)
	C2S(NL80211_CMD_NEW_SURVEY_RESULTS)
	C2S(NL80211_CMD_SET_PMKSA)
	C2S(NL80211_CMD_DEL_PMKSA)
	C2S(NL80211_CMD_FLUSH_PMKSA)
	C2S(NL80211_CMD_REMAIN_ON_CHANNEL)
	C2S(NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL)
	C2S(NL80211_CMD_SET_TX_BITRATE_MASK)
	C2S(NL80211_CMD_REGISTER_FRAME)
	C2S(NL80211_CMD_FRAME)
	C2S(NL80211_CMD_FRAME_TX_STATUS)
	C2S(NL80211_CMD_SET_POWER_SAVE)
	C2S(NL80211_CMD_GET_POWER_SAVE)
	C2S(NL80211_CMD_SET_CQM)
	C2S(NL80211_CMD_NOTIFY_CQM)
	C2S(NL80211_CMD_SET_CHANNEL)
	C2S(NL80211_CMD_SET_WDS_PEER)
	C2S(NL80211_CMD_FRAME_WAIT_CANCEL)
	C2S(NL80211_CMD_JOIN_MESH)
	C2S(NL80211_CMD_LEAVE_MESH)
	C2S(NL80211_CMD_UNPROT_DEAUTHENTICATE)
	C2S(NL80211_CMD_UNPROT_DISASSOCIATE)
	C2S(NL80211_CMD_NEW_PEER_CANDIDATE)
	C2S(NL80211_CMD_GET_WOWLAN)
	C2S(NL80211_CMD_SET_WOWLAN)
	C2S(NL80211_CMD_START_SCHED_SCAN)
	C2S(NL80211_CMD_STOP_SCHED_SCAN)
	C2S(NL80211_CMD_SCHED_SCAN_RESULTS)
	C2S(NL80211_CMD_SCHED_SCAN_STOPPED)
	C2S(NL80211_CMD_SET_REKEY_OFFLOAD)
	C2S(NL80211_CMD_PMKSA_CANDIDATE)
	C2S(NL80211_CMD_TDLS_OPER)
	C2S(NL80211_CMD_TDLS_MGMT)
	C2S(NL80211_CMD_UNEXPECTED_FRAME)
	C2S(NL80211_CMD_PROBE_CLIENT)
	C2S(NL80211_CMD_REGISTER_BEACONS)
	C2S(NL80211_CMD_UNEXPECTED_4ADDR_FRAME)
	C2S(NL80211_CMD_SET_NOACK_MAP)
	C2S(NL80211_CMD_CH_SWITCH_NOTIFY)
	C2S(NL80211_CMD_START_P2P_DEVICE)
	C2S(NL80211_CMD_STOP_P2P_DEVICE)
	C2S(NL80211_CMD_CONN_FAILED)
	C2S(NL80211_CMD_SET_MCAST_RATE)
	C2S(NL80211_CMD_SET_MAC_ACL)
	C2S(NL80211_CMD_RADAR_DETECT)
	C2S(NL80211_CMD_GET_PROTOCOL_FEATURES)
	C2S(NL80211_CMD_UPDATE_FT_IES)
	C2S(NL80211_CMD_FT_EVENT)
	C2S(NL80211_CMD_CRIT_PROTOCOL_START)
	C2S(NL80211_CMD_CRIT_PROTOCOL_STOP)
	C2S(NL80211_CMD_GET_COALESCE)
	C2S(NL80211_CMD_SET_COALESCE)
	C2S(NL80211_CMD_CHANNEL_SWITCH)
	C2S(NL80211_CMD_VENDOR)
	C2S(NL80211_CMD_SET_QOS_MAP)
	default:
		return "NL80211_CMD_UNKNOWN";
	}
}

const char *attributeToString(int attribute)
{
    switch (attribute) {
	C2S(NL80211_ATTR_UNSPEC)

	C2S(NL80211_ATTR_WIPHY)
	C2S(NL80211_ATTR_WIPHY_NAME)

	C2S(NL80211_ATTR_IFINDEX)
	C2S(NL80211_ATTR_IFNAME)
	C2S(NL80211_ATTR_IFTYPE)

	C2S(NL80211_ATTR_MAC)

	C2S(NL80211_ATTR_KEY_DATA)
	C2S(NL80211_ATTR_KEY_IDX)
	C2S(NL80211_ATTR_KEY_CIPHER)
	C2S(NL80211_ATTR_KEY_SEQ)
	C2S(NL80211_ATTR_KEY_DEFAULT)

	C2S(NL80211_ATTR_BEACON_INTERVAL)
	C2S(NL80211_ATTR_DTIM_PERIOD)
	C2S(NL80211_ATTR_BEACON_HEAD)
	C2S(NL80211_ATTR_BEACON_TAIL)

	C2S(NL80211_ATTR_STA_AID)
	C2S(NL80211_ATTR_STA_FLAGS)
	C2S(NL80211_ATTR_STA_LISTEN_INTERVAL)
	C2S(NL80211_ATTR_STA_SUPPORTED_RATES)
	C2S(NL80211_ATTR_STA_VLAN)
	C2S(NL80211_ATTR_STA_INFO)

	C2S(NL80211_ATTR_WIPHY_BANDS)

	C2S(NL80211_ATTR_MNTR_FLAGS)

	C2S(NL80211_ATTR_MESH_ID)
	C2S(NL80211_ATTR_STA_PLINK_ACTION)
	C2S(NL80211_ATTR_MPATH_NEXT_HOP)
	C2S(NL80211_ATTR_MPATH_INFO)

	C2S(NL80211_ATTR_BSS_CTS_PROT)
	C2S(NL80211_ATTR_BSS_SHORT_PREAMBLE)
	C2S(NL80211_ATTR_BSS_SHORT_SLOT_TIME)

	C2S(NL80211_ATTR_HT_CAPABILITY)

	C2S(NL80211_ATTR_SUPPORTED_IFTYPES)

	C2S(NL80211_ATTR_REG_ALPHA2)
	C2S(NL80211_ATTR_REG_RULES)

	C2S(NL80211_ATTR_MESH_CONFIG)

	C2S(NL80211_ATTR_BSS_BASIC_RATES)

	C2S(NL80211_ATTR_WIPHY_TXQ_PARAMS)
	C2S(NL80211_ATTR_WIPHY_FREQ)
	C2S(NL80211_ATTR_WIPHY_CHANNEL_TYPE)

	C2S(NL80211_ATTR_KEY_DEFAULT_MGMT)

	C2S(NL80211_ATTR_MGMT_SUBTYPE)
	C2S(NL80211_ATTR_IE)

	C2S(NL80211_ATTR_MAX_NUM_SCAN_SSIDS)

	C2S(NL80211_ATTR_SCAN_FREQUENCIES)
	C2S(NL80211_ATTR_SCAN_SSIDS)
	C2S(NL80211_ATTR_GENERATION) /* replaces old SCAN_GENERATION */
	C2S(NL80211_ATTR_BSS)

	C2S(NL80211_ATTR_REG_INITIATOR)
	C2S(NL80211_ATTR_REG_TYPE)

	C2S(NL80211_ATTR_SUPPORTED_COMMANDS)

	C2S(NL80211_ATTR_FRAME)
	C2S(NL80211_ATTR_SSID)
	C2S(NL80211_ATTR_AUTH_TYPE)
	C2S(NL80211_ATTR_REASON_CODE)

	C2S(NL80211_ATTR_KEY_TYPE)

	C2S(NL80211_ATTR_MAX_SCAN_IE_LEN)
	C2S(NL80211_ATTR_CIPHER_SUITES)

	C2S(NL80211_ATTR_FREQ_BEFORE)
	C2S(NL80211_ATTR_FREQ_AFTER)

	C2S(NL80211_ATTR_FREQ_FIXED)


	C2S(NL80211_ATTR_WIPHY_RETRY_SHORT)
	C2S(NL80211_ATTR_WIPHY_RETRY_LONG)
	C2S(NL80211_ATTR_WIPHY_FRAG_THRESHOLD)
	C2S(NL80211_ATTR_WIPHY_RTS_THRESHOLD)

	C2S(NL80211_ATTR_TIMED_OUT)

	C2S(NL80211_ATTR_USE_MFP)

	C2S(NL80211_ATTR_STA_FLAGS2)

	C2S(NL80211_ATTR_CONTROL_PORT)

	C2S(NL80211_ATTR_TESTDATA)

	C2S(NL80211_ATTR_PRIVACY)

	C2S(NL80211_ATTR_DISCONNECTED_BY_AP)
	C2S(NL80211_ATTR_STATUS_CODE)

	C2S(NL80211_ATTR_CIPHER_SUITES_PAIRWISE)
	C2S(NL80211_ATTR_CIPHER_SUITE_GROUP)
	C2S(NL80211_ATTR_WPA_VERSIONS)
	C2S(NL80211_ATTR_AKM_SUITES)

	C2S(NL80211_ATTR_REQ_IE)
	C2S(NL80211_ATTR_RESP_IE)

	C2S(NL80211_ATTR_PREV_BSSID)

	C2S(NL80211_ATTR_KEY)
	C2S(NL80211_ATTR_KEYS)

	C2S(NL80211_ATTR_PID)

	C2S(NL80211_ATTR_4ADDR)

	C2S(NL80211_ATTR_SURVEY_INFO)

	C2S(NL80211_ATTR_PMKID)
	C2S(NL80211_ATTR_MAX_NUM_PMKIDS)

	C2S(NL80211_ATTR_DURATION)

	C2S(NL80211_ATTR_COOKIE)

	C2S(NL80211_ATTR_WIPHY_COVERAGE_CLASS)

	C2S(NL80211_ATTR_TX_RATES)

	C2S(NL80211_ATTR_FRAME_MATCH)

	C2S(NL80211_ATTR_ACK)

	C2S(NL80211_ATTR_PS_STATE)

	C2S(NL80211_ATTR_CQM)

	C2S(NL80211_ATTR_LOCAL_STATE_CHANGE)

	C2S(NL80211_ATTR_AP_ISOLATE)

	C2S(NL80211_ATTR_WIPHY_TX_POWER_SETTING)
	C2S(NL80211_ATTR_WIPHY_TX_POWER_LEVEL)

	C2S(NL80211_ATTR_TX_FRAME_TYPES)
	C2S(NL80211_ATTR_RX_FRAME_TYPES)
	C2S(NL80211_ATTR_FRAME_TYPE)

	C2S(NL80211_ATTR_CONTROL_PORT_ETHERTYPE)
	C2S(NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT)

	C2S(NL80211_ATTR_SUPPORT_IBSS_RSN)

	C2S(NL80211_ATTR_WIPHY_ANTENNA_TX)
	C2S(NL80211_ATTR_WIPHY_ANTENNA_RX)

	C2S(NL80211_ATTR_MCAST_RATE)

	C2S(NL80211_ATTR_OFFCHANNEL_TX_OK)

	C2S(NL80211_ATTR_BSS_HT_OPMODE)

	C2S(NL80211_ATTR_KEY_DEFAULT_TYPES)

	C2S(NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION)

	C2S(NL80211_ATTR_MESH_SETUP)

	C2S(NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX)
	C2S(NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX)

	C2S(NL80211_ATTR_SUPPORT_MESH_AUTH)
	C2S(NL80211_ATTR_STA_PLINK_STATE)

	C2S(NL80211_ATTR_WOWLAN_TRIGGERS)
	C2S(NL80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED)

	C2S(NL80211_ATTR_SCHED_SCAN_INTERVAL)

	C2S(NL80211_ATTR_INTERFACE_COMBINATIONS)
	C2S(NL80211_ATTR_SOFTWARE_IFTYPES)

	C2S(NL80211_ATTR_REKEY_DATA)

	C2S(NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS)
	C2S(NL80211_ATTR_MAX_SCHED_SCAN_IE_LEN)

	C2S(NL80211_ATTR_SCAN_SUPP_RATES)

	C2S(NL80211_ATTR_HIDDEN_SSID)

	C2S(NL80211_ATTR_IE_PROBE_RESP)
	C2S(NL80211_ATTR_IE_ASSOC_RESP)

	C2S(NL80211_ATTR_STA_WME)
	C2S(NL80211_ATTR_SUPPORT_AP_UAPSD)

	C2S(NL80211_ATTR_ROAM_SUPPORT)

	C2S(NL80211_ATTR_SCHED_SCAN_MATCH)
	C2S(NL80211_ATTR_MAX_MATCH_SETS)

	C2S(NL80211_ATTR_PMKSA_CANDIDATE)

	C2S(NL80211_ATTR_TX_NO_CCK_RATE)

	C2S(NL80211_ATTR_TDLS_ACTION)
	C2S(NL80211_ATTR_TDLS_DIALOG_TOKEN)
	C2S(NL80211_ATTR_TDLS_OPERATION)
	C2S(NL80211_ATTR_TDLS_SUPPORT)
	C2S(NL80211_ATTR_TDLS_EXTERNAL_SETUP)

	C2S(NL80211_ATTR_DEVICE_AP_SME)

	C2S(NL80211_ATTR_DONT_WAIT_FOR_ACK)

	C2S(NL80211_ATTR_FEATURE_FLAGS)

	C2S(NL80211_ATTR_PROBE_RESP_OFFLOAD)

	C2S(NL80211_ATTR_PROBE_RESP)

	C2S(NL80211_ATTR_DFS_REGION)

	C2S(NL80211_ATTR_DISABLE_HT)
	C2S(NL80211_ATTR_HT_CAPABILITY_MASK)

	C2S(NL80211_ATTR_NOACK_MAP)

	C2S(NL80211_ATTR_INACTIVITY_TIMEOUT)

	C2S(NL80211_ATTR_RX_SIGNAL_DBM)

	C2S(NL80211_ATTR_BG_SCAN_PERIOD)

	C2S(NL80211_ATTR_WDEV)

	C2S(NL80211_ATTR_USER_REG_HINT_TYPE)

	C2S(NL80211_ATTR_CONN_FAILED_REASON)

	C2S(NL80211_ATTR_SAE_DATA)

	C2S(NL80211_ATTR_VHT_CAPABILITY)

	C2S(NL80211_ATTR_SCAN_FLAGS)

	C2S(NL80211_ATTR_CHANNEL_WIDTH)
	C2S(NL80211_ATTR_CENTER_FREQ1)
	C2S(NL80211_ATTR_CENTER_FREQ2)

	C2S(NL80211_ATTR_P2P_CTWINDOW)
	C2S(NL80211_ATTR_P2P_OPPPS)

	C2S(NL80211_ATTR_LOCAL_MESH_POWER_MODE)

	C2S(NL80211_ATTR_ACL_POLICY)

	C2S(NL80211_ATTR_MAC_ADDRS)

	C2S(NL80211_ATTR_MAC_ACL_MAX)

	C2S(NL80211_ATTR_RADAR_EVENT)

	C2S(NL80211_ATTR_EXT_CAPA)
	C2S(NL80211_ATTR_EXT_CAPA_MASK)

	C2S(NL80211_ATTR_STA_CAPABILITY)
	C2S(NL80211_ATTR_STA_EXT_CAPABILITY)

	C2S(NL80211_ATTR_PROTOCOL_FEATURES)
	C2S(NL80211_ATTR_SPLIT_WIPHY_DUMP)

	C2S(NL80211_ATTR_DISABLE_VHT)
	C2S(NL80211_ATTR_VHT_CAPABILITY_MASK)

	C2S(NL80211_ATTR_MDID)
	C2S(NL80211_ATTR_IE_RIC)

	C2S(NL80211_ATTR_CRIT_PROT_ID)
	C2S(NL80211_ATTR_MAX_CRIT_PROT_DURATION)

	C2S(NL80211_ATTR_PEER_AID)

	C2S(NL80211_ATTR_COALESCE_RULE)

	C2S(NL80211_ATTR_CH_SWITCH_COUNT)
	C2S(NL80211_ATTR_CH_SWITCH_BLOCK_TX)
	C2S(NL80211_ATTR_CSA_IES)
	C2S(NL80211_ATTR_CSA_C_OFF_BEACON)
	C2S(NL80211_ATTR_CSA_C_OFF_PRESP)

	C2S(NL80211_ATTR_RXMGMT_FLAGS)

	C2S(NL80211_ATTR_STA_SUPPORTED_CHANNELS)

	C2S(NL80211_ATTR_STA_SUPPORTED_OPER_CLASSES)

	C2S(NL80211_ATTR_HANDLE_DFS)

	C2S(NL80211_ATTR_SUPPORT_5_MHZ)
	C2S(NL80211_ATTR_SUPPORT_10_MHZ)

	C2S(NL80211_ATTR_OPMODE_NOTIF)

	C2S(NL80211_ATTR_VENDOR_ID)
	C2S(NL80211_ATTR_VENDOR_SUBCMD)
	C2S(NL80211_ATTR_VENDOR_DATA)
	C2S(NL80211_ATTR_VENDOR_EVENTS)

	C2S(NL80211_ATTR_QOS_MAP)
    default:
        return "NL80211_ATTR_UNKNOWN";
    }
}

void WifiEvent::log() {
    parse();

    byte *data = (byte *)genlmsg_attrdata(mHeader, 0);
    int len = genlmsg_attrlen(mHeader, 0);
    ALOGD("cmd = %s, len = %d", get_cmdString(), len);
    ALOGD("vendor_id = %04x, vendor_subcmd = %d", get_vendor_id(), get_vendor_subcmd());

    for (int i = 0; i < len; i += 16) {
        char line[81];
        int linelen = min(16, len - i);
        int offset = 0;
        appendFmt(line, offset, "%02x", data[i]);
        for (int j = 1; j < linelen; j++) {
            appendFmt(line, offset, " %02x", data[i+j]);
        }

        for (int j = linelen; j < 16; j++) {
            appendFmt(line, offset, "   ");
        }

        line[23] = '-';

        appendFmt(line, offset, "  ");

        for (int j = 0; j < linelen; j++) {
            if (isprint(data[i+j])) {
                appendFmt(line, offset, "%c", data[i+j]);
            } else {
                appendFmt(line, offset, "-");
            }
        }

        ALOGD("%s", line);
    }

    for (unsigned i = 0; i < NL80211_ATTR_MAX_INTERNAL; i++) {
        if (mAttributes[i] != NULL) {
            ALOGD("found attribute %s", attributeToString(i));
        }
    }

    ALOGD("-- End of message --");
}

const char *WifiEvent::get_cmdString() {
    return cmdToString(get_cmd());
}


int WifiEvent::parse() {
    if (mHeader != NULL) {
        return WIFI_SUCCESS;
    }
    mHeader = (genlmsghdr *)nlmsg_data(nlmsg_hdr(mMsg));
    int result = nla_parse(mAttributes, NL80211_ATTR_MAX_INTERNAL, genlmsg_attrdata(mHeader, 0),
          genlmsg_attrlen(mHeader, 0), NULL);

    // ALOGD("event len = %d", nlmsg_hdr(mMsg)->nlmsg_len);
    return result;
}

int WifiRequest::create(int family, uint8_t cmd, int flags, int hdrlen) {

    destroy();

    mMsg = nlmsg_alloc();
    if (mMsg != NULL) {
        genlmsg_put(mMsg, /* pid = */ 0, /* seq = */ 0, family,
                hdrlen, flags, cmd, /* version = */ 0);
        return WIFI_SUCCESS;
    } else {
        return WIFI_ERROR_OUT_OF_MEMORY;
    }
}

int WifiRequest::create(uint32_t id, int subcmd) {
    int res = create(NL80211_CMD_VENDOR);
    if (res < 0) {
        return res;
    }

    res = put_u32(NL80211_ATTR_VENDOR_ID, id);
    if (res < 0) {
        return res;
    }

    res = put_u32(NL80211_ATTR_VENDOR_SUBCMD, subcmd);
    if (res < 0) {
        return res;
    }

    if (mIface != -1) {
        res = set_iface_id(mIface);
    }

    return res;
}


static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

int WifiCommand::requestResponse() {
    int err = create();                 /* create the message */
    if (err < 0) {
        return err;
    }

    return requestResponse(mMsg);
}

int WifiCommand::requestResponse(WifiRequest& request) {
    int err = 0;

    struct nl_cb *cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb)
        goto out;

    err = nl_send_auto_complete(mInfo->cmd_sock, request.getMessage());    /* send message */
    if (err < 0)
        goto out;

    err = 1;

    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, response_handler, this);

    while (err > 0) {                   /* wait for reply */
        int res = nl_recvmsgs(mInfo->cmd_sock, cb);
        if (res) {
            ALOGE("nl80211: %s->nl_recvmsgs failed: %d", __func__, res);
        }
    }
out:
    nl_cb_put(cb);
    return err;
}

int WifiCommand::requestEvent(int cmd) {

    ALOGD("requesting event %d", cmd);

    int res = wifi_register_handler(wifiHandle(), cmd, event_handler, this);
    if (res < 0) {
        return res;
    }

    res = create();                                                 /* create the message */
    if (res < 0)
        goto out;

    ALOGD("waiting for response %d", cmd);

    res = nl_send_auto_complete(mInfo->cmd_sock, mMsg.getMessage());    /* send message */
    if (res < 0)
        goto out;

    ALOGD("waiting for event %d", cmd);
    res = mCondition.wait();
    if (res < 0)
        goto out;

out:
    wifi_unregister_handler(wifiHandle(), cmd);
    return res;
}

int WifiCommand::requestVendorEvent(uint32_t id, int subcmd) {

    int res = wifi_register_vendor_handler(wifiHandle(), id, subcmd, event_handler, this);
    if (res < 0) {
        return res;
    }

    res = create();                                                 /* create the message */
    if (res < 0)
        goto out;

    res = nl_send_auto_complete(mInfo->cmd_sock, mMsg.getMessage());    /* send message */
    if (res < 0)
        goto out;

    res = mCondition.wait();
    if (res < 0)
        goto out;

out:
    wifi_unregister_vendor_handler(wifiHandle(), id, subcmd);
    return res;
}

/* Event handlers */
int WifiCommand::response_handler(struct nl_msg *msg, void *arg) {
    // ALOGD("response_handler called");
    WifiCommand *cmd = (WifiCommand *)arg;
    WifiEvent reply(msg);
    int res = reply.parse();
    if (res < 0) {
        ALOGE("Failed to parse reply message = %d", res);
        return NL_SKIP;
    } else {
        // reply.log();
        return cmd->handleResponse(reply);
    }
}

int WifiCommand::event_handler(struct nl_msg *msg, void *arg) {
    WifiCommand *cmd = (WifiCommand *)arg;
    WifiEvent event(msg);
    int res = event.parse();
    if (res < 0) {
        ALOGE("Failed to parse event = %d", res);
        res = NL_SKIP;
    } else {
        res = cmd->handleEvent(event);
    }

    cmd->mCondition.signal();
    return res;
}

/* Other event handlers */
int WifiCommand::valid_handler(struct nl_msg *msg, void *arg) {
    // ALOGD("valid_handler called");
    int *err = (int *)arg;
    *err = 0;
    return NL_SKIP;
}

int WifiCommand::ack_handler(struct nl_msg *msg, void *arg) {
    // ALOGD("ack_handler called");
    int *err = (int *)arg;
    *err = 0;
    return NL_STOP;
}

int WifiCommand::finish_handler(struct nl_msg *msg, void *arg) {
    // ALOGD("finish_handler called");
    int *ret = (int *)arg;
    *ret = 0;
    return NL_SKIP;
}

int WifiCommand::error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) {
    int *ret = (int *)arg;
    *ret = err->error;

    // ALOGD("error_handler received : %d", err->error);
    return NL_SKIP;
}
