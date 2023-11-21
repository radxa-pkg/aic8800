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
#include <errno.h>

#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/attr.h>
#include <netlink/handlers.h>
#include <netlink/msg.h>

#include <dirent.h>
#include <net/if.h>

#include <sys/types.h>
#include <unistd.h>

#include "sync.h"

#define LOG_TAG  "WifiHAL"

#include <log/log.h>

#include "wifi_hal.h"
#include "common.h"
#include "cpp_bindings.h"
#include "rtt.h"
/*
 BUGBUG: normally, libnl allocates ports for all connections it makes; but
 being a static library, it doesn't really know how many other netlink connections
 are made by the same process, if connections come from different shared libraries.
 These port assignments exist to solve that problem - temporarily. We need to fix
 libnl to try and allocate ports across the entire process.
 */

#define WIFI_HAL_CMD_SOCK_PORT       644
#define WIFI_HAL_EVENT_SOCK_PORT     645

/*
 * Defines for wifi_wait_for_driver_ready()
 * Specify durations between polls and max wait time
 */
#define POLL_DRIVER_DURATION_US (100000)
#define POLL_DRIVER_MAX_TIME_MS (10000)
#define EVENT_BUF_SIZE 2048

static void internal_event_handler(wifi_handle handle, int events);
static int internal_no_seq_check(nl_msg *msg, void *arg);
static int internal_valid_message_handler(nl_msg *msg, void *arg);
static int wifi_get_multicast_id(wifi_handle handle, const char *name, const char *group);
static int wifi_add_membership(wifi_handle handle, const char *group);
static wifi_error wifi_init_interfaces(wifi_handle handle);
static wifi_error wifi_start_rssi_monitoring(wifi_request_id id, wifi_interface_handle
                        iface, s8 max_rssi, s8 min_rssi, wifi_rssi_event_handler eh);
static wifi_error wifi_stop_rssi_monitoring(wifi_request_id id, wifi_interface_handle iface);
static wifi_error wifi_set_packet_filter(wifi_interface_handle handle,
                            const u8 *program, u32 len);
static wifi_error wifi_get_packet_filter_capabilities(wifi_interface_handle handle,
                u32 *version, u32 *max_len);
static wifi_error wifi_configure_nd_offload(wifi_interface_handle iface, u8 enable);
static wifi_error wifi_virtual_interface_create(wifi_handle handle, const char *ifname, wifi_interface_type iface_type);
static wifi_error wifi_virtual_interface_delete(wifi_handle handle, const char* ifname);
static wifi_error wifi_get_usable_channels(wifi_handle handle, u32 band_mask, u32 iface_mode_mask,
                u32 filter_mask, u32 max_size, u32* size, wifi_usable_channel* channels);

typedef enum wifi_attr {
    ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET,
    ANDR_WIFI_ATTRIBUTE_FEATURE_SET,
    ANDR_WIFI_ATTRIBUTE_PNO_RANDOM_MAC_OUI,
    ANDR_WIFI_ATTRIBUTE_NODFS_SET,
    ANDR_WIFI_ATTRIBUTE_COUNTRY,
    ANDR_WIFI_ATTRIBUTE_ND_OFFLOAD_VALUE
    // Add more attribute here
} wifi_attr_t;

enum wifi_rssi_monitor_attr {
    RSSI_MONITOR_ATTRIBUTE_MAX_RSSI,
    RSSI_MONITOR_ATTRIBUTE_MIN_RSSI,
    RSSI_MONITOR_ATTRIBUTE_START,
};

enum wifi_apf_attr {
    APF_ATTRIBUTE_VERSION,
    APF_ATTRIBUTE_MAX_LEN,
    APF_ATTRIBUTE_PROGRAM,
    APF_ATTRIBUTE_PROGRAM_LEN
};

enum apf_request_type {
    GET_APF_CAPABILITIES,
    SET_APF_PROGRAM
};

enum wifi_usable_channel_attributes {
    USABLECHAN_ATTRIBUTE_INVALID    = 0,
    USABLECHAN_ATTRIBUTE_BAND       = 1,
    USABLECHAN_ATTRIBUTE_IFACE      = 2,
    USABLECHAN_ATTRIBUTE_FILTER     = 3,
    USABLECHAN_ATTRIBUTE_MAX_SIZE   = 4,
    USABLECHAN_ATTRIBUTE_SIZE       = 5,
    USABLECHAN_ATTRIBUTE_CHANNELS   = 6,
    USABLECHAN_ATTRIBUTE_MAX
};


/* Initialize/Cleanup */

void wifi_socket_set_local_port(struct nl_sock *sock, uint32_t port)
{
    uint32_t pid = getpid() & 0x3FFFFF;
    nl_socket_set_local_port(sock, pid + (port << 22));
}

static nl_sock * wifi_create_nl_socket(int port)
{
    // ALOGI("Creating socket");
    struct nl_sock *sock = nl_socket_alloc();
    if (sock == NULL) {
        ALOGE("Could not create handle");
        return NULL;
    }

    wifi_socket_set_local_port(sock, port);

    struct sockaddr *addr = NULL;
    // ALOGI("sizeof(sockaddr) = %d, sizeof(sockaddr_nl) = %d", sizeof(*addr), sizeof(*addr_nl));

    // ALOGI("Connecting socket");
    if (nl_connect(sock, NETLINK_GENERIC)) {
        ALOGE("Could not connect handle");
        nl_socket_free(sock);
        return NULL;
    }

    // ALOGI("Making socket nonblocking");
    /*
    if (nl_socket_set_nonblocking(sock)) {
        ALOGE("Could make socket non-blocking");
        nl_socket_free(sock);
        return NULL;
    }
    */

    return sock;
}

/*initialize function pointer table with Broadcom HHAL API*/
wifi_error init_wifi_vendor_hal_func_table(wifi_hal_fn *fn)
{
    if (fn == NULL) {
        return WIFI_ERROR_UNKNOWN;
    }
    fn->wifi_initialize = wifi_initialize;
    fn->wifi_wait_for_driver_ready = wifi_wait_for_driver_ready;
    fn->wifi_cleanup = wifi_cleanup;
    fn->wifi_event_loop = wifi_event_loop;
    fn->wifi_get_supported_feature_set = wifi_get_supported_feature_set;
    fn->wifi_get_concurrency_matrix = wifi_get_concurrency_matrix;
    fn->wifi_set_scanning_mac_oui =  wifi_set_scanning_mac_oui;
    fn->wifi_get_ifaces = wifi_get_ifaces;
    fn->wifi_get_iface_name = wifi_get_iface_name;
    fn->wifi_start_gscan = wifi_start_gscan;
    fn->wifi_stop_gscan = wifi_stop_gscan;
    fn->wifi_get_cached_gscan_results = wifi_get_cached_gscan_results;
    fn->wifi_set_bssid_hotlist = wifi_set_bssid_hotlist;
    fn->wifi_reset_bssid_hotlist = wifi_reset_bssid_hotlist;
    fn->wifi_set_significant_change_handler = wifi_set_significant_change_handler;
    fn->wifi_reset_significant_change_handler = wifi_reset_significant_change_handler;
    fn->wifi_get_gscan_capabilities = wifi_get_gscan_capabilities;
    fn->wifi_get_link_stats = wifi_get_link_stats;
    fn->wifi_set_link_stats = wifi_set_link_stats;
    fn->wifi_clear_link_stats = wifi_clear_link_stats;
    fn->wifi_get_valid_channels = wifi_get_valid_channels;
    fn->wifi_rtt_range_request = wifi_rtt_range_request;
    fn->wifi_rtt_range_cancel = wifi_rtt_range_cancel;
    fn->wifi_get_rtt_capabilities = wifi_get_rtt_capabilities;
    fn->wifi_rtt_get_responder_info = wifi_rtt_get_responder_info;
    fn->wifi_enable_responder = wifi_enable_responder;
    fn->wifi_disable_responder = wifi_disable_responder;
    fn->wifi_set_nodfs_flag = wifi_set_nodfs_flag;
    fn->wifi_start_logging = wifi_start_logging;
    fn->wifi_set_epno_list = wifi_set_epno_list;
    fn->wifi_reset_epno_list = wifi_reset_epno_list;
    fn->wifi_set_country_code = wifi_set_country_code;
    fn->wifi_get_firmware_memory_dump = wifi_get_firmware_memory_dump;
    fn->wifi_set_log_handler = wifi_set_log_handler;
    fn->wifi_reset_log_handler = wifi_reset_log_handler;
    fn->wifi_set_alert_handler = wifi_set_alert_handler;
    fn->wifi_reset_alert_handler = wifi_reset_alert_handler;
    fn->wifi_get_firmware_version = wifi_get_firmware_version;
    fn->wifi_get_ring_buffers_status = wifi_get_ring_buffers_status;
    fn->wifi_get_logger_supported_feature_set = wifi_get_logger_supported_feature_set;
    fn->wifi_get_ring_data = wifi_get_ring_data;
    fn->wifi_get_driver_version = wifi_get_driver_version;
    fn->wifi_start_rssi_monitoring = wifi_start_rssi_monitoring;
    fn->wifi_stop_rssi_monitoring = wifi_stop_rssi_monitoring;
    fn->wifi_configure_nd_offload = wifi_configure_nd_offload;
    fn->wifi_start_sending_offloaded_packet = wifi_start_sending_offloaded_packet;
    fn->wifi_stop_sending_offloaded_packet = wifi_stop_sending_offloaded_packet;
    fn->wifi_start_pkt_fate_monitoring = wifi_start_pkt_fate_monitoring;
    fn->wifi_get_tx_pkt_fates = wifi_get_tx_pkt_fates;
    fn->wifi_get_rx_pkt_fates = wifi_get_rx_pkt_fates;
    fn->wifi_get_packet_filter_capabilities = wifi_get_packet_filter_capabilities;
    fn->wifi_get_wake_reason_stats = wifi_get_wake_reason_stats;
    fn->wifi_set_packet_filter = wifi_set_packet_filter;
    fn->wifi_virtual_interface_create = wifi_virtual_interface_create;
    fn->wifi_virtual_interface_delete = wifi_virtual_interface_delete;
    fn->wifi_get_usable_channels = wifi_get_usable_channels;
    return WIFI_SUCCESS;
}

wifi_error wifi_initialize(wifi_handle *handle)
{
    srand(getpid());

    ALOGI("Initializing wifi");
    hal_info *info = (hal_info *)malloc(sizeof(hal_info));
    if (info == NULL) {
        ALOGE("Could not allocate hal_info");
        return WIFI_ERROR_UNKNOWN;
    }

    memset(info, 0, sizeof(*info));

    ALOGI("Creating socket");
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, info->cleanup_socks) == -1) {
        ALOGE("Could not create cleanup sockets");
        free(info);
        return WIFI_ERROR_UNKNOWN;
    }

    struct nl_sock *cmd_sock = wifi_create_nl_socket(WIFI_HAL_CMD_SOCK_PORT);
    if (cmd_sock == NULL) {
        ALOGE("Could not create handle");
        free(info);
        return WIFI_ERROR_UNKNOWN;
    }

    struct nl_sock *event_sock = wifi_create_nl_socket(WIFI_HAL_EVENT_SOCK_PORT);
    if (event_sock == NULL) {
        ALOGE("Could not create handle");
        nl_socket_free(cmd_sock);
        free(info);
        return WIFI_ERROR_UNKNOWN;
    }

    struct nl_cb *cb = nl_socket_get_cb(event_sock);
    if (cb == NULL) {
        ALOGE("Could not create handle");
        nl_socket_free(cmd_sock);
        nl_socket_free(event_sock);
        free(info);
        return WIFI_ERROR_UNKNOWN;
    }

    // ALOGI("cb->refcnt = %d", cb->cb_refcnt);
    nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, internal_no_seq_check, info);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, internal_valid_message_handler, info);
    nl_cb_put(cb);

    info->cmd_sock = cmd_sock;
    info->event_sock = event_sock;
    info->clean_up = false;
    info->in_event_loop = false;

    info->event_cb = (cb_info *)malloc(sizeof(cb_info) * DEFAULT_EVENT_CB_SIZE);
    info->alloc_event_cb = DEFAULT_EVENT_CB_SIZE;
    info->num_event_cb = 0;

    info->cmd = (cmd_info *)malloc(sizeof(cmd_info) * DEFAULT_CMD_SIZE);
    info->alloc_cmd = DEFAULT_CMD_SIZE;
    info->num_cmd = 0;

    info->nl80211_family_id = genl_ctrl_resolve(cmd_sock, "nl80211");
    if (info->nl80211_family_id < 0) {
        ALOGE("Could not resolve nl80211 familty id");
        nl_socket_free(cmd_sock);
        nl_socket_free(event_sock);
        free(info);
        return WIFI_ERROR_UNKNOWN;
    }

    pthread_mutex_init(&info->cb_lock, NULL);

    *handle = (wifi_handle) info;

    if (wifi_init_interfaces(*handle) != WIFI_SUCCESS) {
        ALOGE("No wifi interface found");
        nl_socket_free(cmd_sock);
        nl_socket_free(event_sock);
        pthread_mutex_destroy(&info->cb_lock);
        free(info);
        return WIFI_ERROR_NOT_AVAILABLE;
    }

    if ((wifi_add_membership(*handle, "scan") < 0) ||
        (wifi_add_membership(*handle, "mlme")  < 0) ||
        (wifi_add_membership(*handle, "regulatory") < 0) ||
        (wifi_add_membership(*handle, "vendor") < 0)) {
        ALOGE("Add membership failed");
        nl_socket_free(cmd_sock);
        nl_socket_free(event_sock);
        pthread_mutex_destroy(&info->cb_lock);
        free(info);
        return WIFI_ERROR_NOT_AVAILABLE;
    }

    // ALOGI("Found %d interfaces", info->num_interfaces);

    ALOGI("Initialized Wifi HAL Successfully; vendor cmd = %d", NL80211_CMD_VENDOR);
    return WIFI_SUCCESS;
}

wifi_error wifi_wait_for_driver_ready(void)
{
    // This function will wait to make sure basic client netdev is created
    // Function times out after 10 seconds
    int count = (POLL_DRIVER_MAX_TIME_MS * 1000) / POLL_DRIVER_DURATION_US;
    FILE *fd;

    do {
        if (((fd = fopen("/sys/class/net/wlan0", "r")) != NULL) || ((fd = fopen("/sys/class/net/wlan1", "r")) != NULL)) {
            fclose(fd);
            return WIFI_SUCCESS;
        }
        usleep(POLL_DRIVER_DURATION_US);
    } while(--count > 0);

    ALOGE("Timed out waiting on Driver ready ... ");
    return WIFI_ERROR_TIMED_OUT;
}

static int wifi_add_membership(wifi_handle handle, const char *group)
{
    hal_info *info = getHalInfo(handle);

    int id = wifi_get_multicast_id(handle, "nl80211", group);
    if (id < 0) {
        ALOGE("Could not find group %s", group);
        return id;
    }

    int ret = nl_socket_add_membership(info->event_sock, id);
    if (ret < 0) {
        ALOGE("Could not add membership to group %s", group);
    }

    // ALOGI("Successfully added membership for group %s", group);
    return ret;
}

static void internal_cleaned_up_handler(wifi_handle handle)
{
    hal_info *info = getHalInfo(handle);
    wifi_cleaned_up_handler cleaned_up_handler = info->cleaned_up_handler;

    if (info->cmd_sock != 0) {
        close(info->cleanup_socks[0]);
        close(info->cleanup_socks[1]);
        nl_socket_free(info->cmd_sock);
        nl_socket_free(info->event_sock);
        info->cmd_sock = NULL;
        info->event_sock = NULL;
    }

    (*cleaned_up_handler)(handle);
    pthread_mutex_destroy(&info->cb_lock);
    free(info);

    ALOGI("Internal cleanup completed");
}

void wifi_cleanup(wifi_handle handle, wifi_cleaned_up_handler handler)
{
    hal_info *info = getHalInfo(handle);
    char buf[64];

    info->cleaned_up_handler = handler;
    if (TEMP_FAILURE_RETRY(write(info->cleanup_socks[0], "Exit", 4)) < 1) {
        // As a fallback set the cleanup flag to TRUE
        ALOGE("could not write to the cleanup socket");
    } else {
        // Listen to the response
        // Hopefully we dont get errors or get hung up
        // Not much can be done in that case, but assume that
        // it has rx'ed the Exit message to exit the thread.
        // As a fallback set the cleanup flag to TRUE
        memset(buf, 0, sizeof(buf));
        ssize_t result = TEMP_FAILURE_RETRY(read(info->cleanup_socks[0], buf, sizeof(buf)));
        ALOGE("%s: Read after POLL returned %zd, error no = %d (%s)", __FUNCTION__,
               result, errno, strerror(errno));
        if (strncmp(buf, "Done", 4) == 0) {
            ALOGE("Event processing terminated");
        } else {
            ALOGD("Rx'ed %s", buf);
        }
    }
    info->clean_up = true;
    pthread_mutex_lock(&info->cb_lock);

    int bad_commands = 0;

    for (int i = 0; i < info->num_event_cb; i++) {
        cb_info *cbi = &(info->event_cb[i]);
        WifiCommand *cmd = (WifiCommand *)cbi->cb_arg;
        ALOGI("Command left in event_cb %p:%s", cmd, (cmd ? cmd->getType(): ""));
    }

    while (info->num_cmd > bad_commands) {
        int num_cmd = info->num_cmd;
        cmd_info *cmdi = &(info->cmd[bad_commands]);
        WifiCommand *cmd = cmdi->cmd;
        if (cmd != NULL) {
            ALOGI("Cancelling command %p:%s", cmd, cmd->getType());
            pthread_mutex_unlock(&info->cb_lock);
            cmd->cancel();
            pthread_mutex_lock(&info->cb_lock);
            if (num_cmd == info->num_cmd) {
                ALOGI("Cancelling command %p:%s did not work", cmd, (cmd ? cmd->getType(): ""));
                bad_commands++;
            }
            /* release reference added when command is saved */
            cmd->releaseRef();
        }
    }

    for (int i = 0; i < info->num_event_cb; i++) {
        cb_info *cbi = &(info->event_cb[i]);
        WifiCommand *cmd = (WifiCommand *)cbi->cb_arg;
        ALOGE("Leaked command %p", cmd);
    }
    pthread_mutex_unlock(&info->cb_lock);
    internal_cleaned_up_handler(handle);
}

static int internal_pollin_handler(wifi_handle handle)
{
    hal_info *info = getHalInfo(handle);
    struct nl_cb *cb = nl_socket_get_cb(info->event_sock);
    int res = nl_recvmsgs(info->event_sock, cb);
    // ALOGD("nl_recvmsgs returned %d", res);
    nl_cb_put(cb);
    return res;
}

/* Run event handler */
void wifi_event_loop(wifi_handle handle)
{
    hal_info *info = getHalInfo(handle);
    if (info->in_event_loop) {
        return;
    } else {
        info->in_event_loop = true;
    }

    pollfd pfd[2];
    memset(&pfd[0], 0, sizeof(pollfd) * 2);

    pfd[0].fd = nl_socket_get_fd(info->event_sock);
    pfd[0].events = POLLIN;
    pfd[1].fd = info->cleanup_socks[1];
    pfd[1].events = POLLIN;

    char buf[2048];
    /* TODO: Add support for timeouts */

    do {
        int timeout = -1;                   /* Infinite timeout */
        pfd[0].revents = 0;
        pfd[1].revents = 0;
        // ALOGI("Polling socket");
        int result = TEMP_FAILURE_RETRY(poll(pfd, 2, timeout));
        if (result < 0) {
            // ALOGE("Error polling socket");
        } else if (pfd[0].revents & POLLERR) {
            ALOGE("POLL Error; error no = %d (%s)", errno, strerror(errno));
            ssize_t result2 = TEMP_FAILURE_RETRY(read(pfd[0].fd, buf, sizeof(buf)));
            ALOGE("Read after POLL returned %zd, error no = %d (%s)", result2,
                  errno, strerror(errno));
        } else if (pfd[0].revents & POLLHUP) {
            ALOGE("Remote side hung up");
            break;
        } else if (pfd[0].revents & POLLIN) {
            // ALOGI("Found some events!!!");
            internal_pollin_handler(handle);
        } else if (pfd[1].revents & POLLIN) {
            memset(buf, 0, sizeof(buf));
            ssize_t result2 = TEMP_FAILURE_RETRY(read(pfd[1].fd, buf, sizeof(buf)));
            ALOGE("%s: Read after POLL returned %zd, error no = %d (%s)", __FUNCTION__,
                   result2, errno, strerror(errno));
            if (strncmp(buf, "Exit", 4) == 0) {
                ALOGD("Got a signal to exit!!!");
                if (TEMP_FAILURE_RETRY(write(pfd[1].fd, "Done", 4)) < 1) {
                    ALOGE("could not write to the cleanup socket");
                }
                break;
            } else {
                ALOGD("Rx'ed %s on the cleanup socket\n", buf);
            }
        } else {
            ALOGE("Unknown event - %0x, %0x", pfd[0].revents, pfd[1].revents);
        }
    } while (!info->clean_up);
    ALOGI("Exit %s", __FUNCTION__);
}

///////////////////////////////////////////////////////////////////////////////////////

static int internal_no_seq_check(struct nl_msg *msg, void *arg)
{
    return NL_OK;
}

static int internal_valid_message_handler(nl_msg *msg, void *arg)
{
    // ALOGI("got an event");

    wifi_handle handle = (wifi_handle)arg;
    hal_info *info = getHalInfo(handle);

    WifiEvent event(msg);
    int res = event.parse();
    if (res < 0) {
        ALOGE("Failed to parse event: %d", res);
        return NL_SKIP;
    }

    int cmd = event.get_cmd();
    uint32_t vendor_id = 0;
    int subcmd = 0;

    if (cmd == NL80211_CMD_VENDOR) {
        vendor_id = event.get_u32(NL80211_ATTR_VENDOR_ID);
        subcmd = event.get_u32(NL80211_ATTR_VENDOR_SUBCMD);
        ALOGV("event received %s, vendor_id = 0x%0x, subcmd = 0x%0x",
                event.get_cmdString(), vendor_id, subcmd);
    } else {
        // ALOGV("event received %s", event.get_cmdString());
    }

    // ALOGV("event received %s, vendor_id = 0x%0x", event.get_cmdString(), vendor_id);
    // event.log();

    bool dispatched = false;

    pthread_mutex_lock(&info->cb_lock);

    for (int i = 0; i < info->num_event_cb; i++) {
        if (cmd == info->event_cb[i].nl_cmd) {
            if (cmd == NL80211_CMD_VENDOR
                && ((vendor_id != info->event_cb[i].vendor_id)
                || (subcmd != info->event_cb[i].vendor_subcmd)))
            {
                /* event for a different vendor, ignore it */
                continue;
            }

            cb_info *cbi = &(info->event_cb[i]);
            nl_recvmsg_msg_cb_t cb_func = cbi->cb_func;
            void *cb_arg = cbi->cb_arg;
            WifiCommand *cmd = (WifiCommand *)cbi->cb_arg;
            if (cmd != NULL) {
                cmd->addRef();
            }
            pthread_mutex_unlock(&info->cb_lock);
            if (cb_func)
                (*cb_func)(msg, cb_arg);
            if (cmd != NULL) {
                cmd->releaseRef();
            }

            return NL_OK;
        }
    }

    pthread_mutex_unlock(&info->cb_lock);
    return NL_OK;
}

///////////////////////////////////////////////////////////////////////////////////////

class GetMulticastIdCommand : public WifiCommand
{
private:
    const char *mName;
    const char *mGroup;
    int   mId;
public:
    GetMulticastIdCommand(wifi_handle handle, const char *name, const char *group)
        : WifiCommand("GetMulticastIdCommand", handle, 0)
    {
        mName = name;
        mGroup = group;
        mId = -1;
    }

    int getId() {
        return mId;
    }

    virtual int create() {
        int nlctrlFamily = genl_ctrl_resolve(mInfo->cmd_sock, "nlctrl");
        // ALOGI("ctrl family = %d", nlctrlFamily);
        int ret = mMsg.create(nlctrlFamily, CTRL_CMD_GETFAMILY, 0, 0);
        if (ret < 0) {
            return ret;
        }
        ret = mMsg.put_string(CTRL_ATTR_FAMILY_NAME, mName);
        return ret;
    }

    virtual int handleResponse(WifiEvent& reply) {

        // ALOGI("handling reponse in %s", __func__);

        struct nlattr **tb = reply.attributes();
        struct genlmsghdr *gnlh = reply.header();
        struct nlattr *mcgrp = NULL;
        int i;

        if (!tb[CTRL_ATTR_MCAST_GROUPS]) {
            ALOGI("No multicast groups found");
            return NL_SKIP;
        } else {
            // ALOGI("Multicast groups attr size = %d", nla_len(tb[CTRL_ATTR_MCAST_GROUPS]));
        }

        for_each_attr(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], i) {

            // ALOGI("Processing group");
            struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];
            nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX, (nlattr *)nla_data(mcgrp),
                nla_len(mcgrp), NULL);
            if (!tb2[CTRL_ATTR_MCAST_GRP_NAME] || !tb2[CTRL_ATTR_MCAST_GRP_ID]) {
                continue;
            }

            char *grpName = (char *)nla_data(tb2[CTRL_ATTR_MCAST_GRP_NAME]);
            int grpNameLen = nla_len(tb2[CTRL_ATTR_MCAST_GRP_NAME]);

            // ALOGI("Found group name %s", grpName);

            if (strncmp(grpName, mGroup, grpNameLen) != 0)
                continue;

            mId = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
            break;
        }

        return NL_SKIP;
    }

};

class SetPnoMacAddrOuiCommand : public WifiCommand {

private:
    byte *mOui;
    feature_set *fset;
    feature_set *feature_matrix;
    int *fm_size;
    int set_size_max;
public:
    SetPnoMacAddrOuiCommand(wifi_interface_handle handle, oui scan_oui)
        : WifiCommand("SetPnoMacAddrOuiCommand", handle, 0)
    {
        mOui = scan_oui;
    }

    int createRequest(WifiRequest& request, int subcmd, byte *scan_oui) {
        int result = request.create(GOOGLE_OUI, subcmd);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        result = request.put(ANDR_WIFI_ATTRIBUTE_PNO_RANDOM_MAC_OUI, scan_oui, DOT11_OUI_LEN);
        if (result < 0) {
            return result;
        }

        request.attr_end(data);
        return WIFI_SUCCESS;

    }

    int start() {
        ALOGD("Sending mac address OUI");
        WifiRequest request(familyId(), ifaceId());
        int result = createRequest(request, WIFI_SUBCMD_SET_PNO_RANDOM_MAC_OUI, mOui);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create request; result = %d", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to set scanning mac OUI; result = %d", result);
        }

        return result;
    }
protected:
    virtual int handleResponse(WifiEvent& reply) {
         ALOGD("Request complete!");
        /* Nothing to do on response! */
        return NL_SKIP;
    }
};

class SetNodfsCommand : public WifiCommand {

private:
    u32 mNoDfs;
public:
    SetNodfsCommand(wifi_interface_handle handle, u32 nodfs)
        : WifiCommand("SetNodfsCommand", handle, 0) {
        mNoDfs = nodfs;
    }
    virtual int create() {
        int ret;

        ret = mMsg.create(GOOGLE_OUI, WIFI_SUBCMD_NODFS_SET);
        if (ret < 0) {
            ALOGE("Can't create message to send to driver - %d", ret);
            return ret;
        }

        nlattr *data = mMsg.attr_start(NL80211_ATTR_VENDOR_DATA);
        ret = mMsg.put_u32(ANDR_WIFI_ATTRIBUTE_NODFS_SET, mNoDfs);
        if (ret < 0) {
             return ret;
        }

        mMsg.attr_end(data);
        return WIFI_SUCCESS;
    }
};

class SetCountryCodeCommand : public WifiCommand {
private:
    const char *mCountryCode;
public:
    SetCountryCodeCommand(wifi_interface_handle handle, const char *country_code)
        : WifiCommand("SetCountryCodeCommand", handle, 0) {
        mCountryCode = country_code;
        }
    virtual int create() {
        int ret;

        ret = mMsg.create(GOOGLE_OUI, WIFI_SUBCMD_SET_COUNTRY_CODE);
        if (ret < 0) {
             ALOGE("Can't create message to send to driver - %d", ret);
             return ret;
        }

        nlattr *data = mMsg.attr_start(NL80211_ATTR_VENDOR_DATA);
        ret = mMsg.put_string(ANDR_WIFI_ATTRIBUTE_COUNTRY, mCountryCode);
        if (ret < 0) {
            return ret;
        }

        mMsg.attr_end(data);
        return WIFI_SUCCESS;

    }
};

class SetRSSIMonitorCommand : public WifiCommand {
private:
    s8 mMax_rssi;
    s8 mMin_rssi;
    wifi_rssi_event_handler mHandler;
public:
    SetRSSIMonitorCommand(wifi_request_id id, wifi_interface_handle handle,
                s8 max_rssi, s8 min_rssi, wifi_rssi_event_handler eh)
        : WifiCommand("SetRSSIMonitorCommand", handle, id), mMax_rssi(max_rssi), mMin_rssi
        (min_rssi), mHandler(eh)
        {
        }
   int createRequest(WifiRequest& request, int enable) {
        int result = request.create(GOOGLE_OUI, WIFI_SUBCMD_SET_RSSI_MONITOR);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        result = request.put_u32(RSSI_MONITOR_ATTRIBUTE_MAX_RSSI, (enable ? mMax_rssi: 0));
        if (result < 0) {
            return result;
        }
        ALOGD("create request");
        result = request.put_u32(RSSI_MONITOR_ATTRIBUTE_MIN_RSSI, (enable? mMin_rssi: 0));
        if (result < 0) {
            return result;
        }
        result = request.put_u32(RSSI_MONITOR_ATTRIBUTE_START, enable);
        if (result < 0) {
            return result;
        }
        request.attr_end(data);
        return result;
    }

    int start() {
        WifiRequest request(familyId(), ifaceId());
        int result = createRequest(request, 1);
        if (result < 0) {
            return result;
        }
        result = requestResponse(request);
        if (result < 0) {
            ALOGI("Failed to set RSSI Monitor, result = %d", result);
            return result;
        }
        ALOGI("Successfully set RSSI monitoring");
        registerVendorHandler(GOOGLE_OUI, GOOGLE_RSSI_MONITOR_EVENT);


        if (result < 0) {
            unregisterVendorHandler(GOOGLE_OUI, GOOGLE_RSSI_MONITOR_EVENT);
            return result;
        }
        ALOGI("Done!");
        return result;
    }

    virtual int cancel() {

        WifiRequest request(familyId(), ifaceId());
        int result = createRequest(request, 0);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create request; result = %d", result);
        } else {
            result = requestResponse(request);
            if (result != WIFI_SUCCESS) {
                ALOGE("failed to stop RSSI monitoring = %d", result);
            }
        }
        unregisterVendorHandler(GOOGLE_OUI, GOOGLE_RSSI_MONITOR_EVENT);
        return WIFI_SUCCESS;
    }

    virtual int handleResponse(WifiEvent& reply) {
        /* Nothing to do on response! */
        return NL_SKIP;
    }

   virtual int handleEvent(WifiEvent& event) {
        ALOGI("Got a RSSI monitor event");

        nlattr *vendor_data = event.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = event.get_vendor_data_len();

        if (vendor_data == NULL || len == 0) {
            ALOGI("RSSI monitor: No data");
            return NL_SKIP;
        }
        /* driver<->HAL event structure */
        #define RSSI_MONITOR_EVT_VERSION   1
        typedef struct {
            u8 version;
            s8 cur_rssi;
            mac_addr BSSID;
        } rssi_monitor_evt;

        rssi_monitor_evt *data = (rssi_monitor_evt *)event.get_vendor_data();

        if (data->version != RSSI_MONITOR_EVT_VERSION) {
            ALOGI("Event version mismatch %d, expected %d", data->version, RSSI_MONITOR_EVT_VERSION);
            return NL_SKIP;
        }

        if (*mHandler.on_rssi_threshold_breached) {
            (*mHandler.on_rssi_threshold_breached)(id(), data->BSSID, data->cur_rssi);
        } else {
            ALOGW("No RSSI monitor handler registered");
        }

        return NL_SKIP;
    }

};

class AndroidPktFilterCommand : public WifiCommand {
    private:
        const u8* mProgram;
        u32 mProgramLen;
        u32* mVersion;
        u32* mMaxLen;
        int mReqType;
    public:
        AndroidPktFilterCommand(wifi_interface_handle handle,
                u32* version, u32* max_len)
            : WifiCommand("AndroidPktFilterCommand", handle, 0),
                    mVersion(version), mMaxLen(max_len),
                    mReqType(GET_APF_CAPABILITIES)
        {
        }

        AndroidPktFilterCommand(wifi_interface_handle handle,
                const u8* program, u32 len)
            : WifiCommand("AndroidPktFilterCommand", handle, 0),
                    mProgram(program), mProgramLen(len),
                    mReqType(SET_APF_PROGRAM)
        {
        }

    int createRequest(WifiRequest& request) {
        if (mReqType == SET_APF_PROGRAM) {
            ALOGI("\n%s: APF set program request\n", __FUNCTION__);
            return createSetPktFilterRequest(request);
        } else if (mReqType == GET_APF_CAPABILITIES) {
            ALOGI("\n%s: APF get capabilities request\n", __FUNCTION__);
            return createGetPktFilterCapabilitesRequest(request);
        } else {
            ALOGE("\n%s Unknown APF request\n", __FUNCTION__);
            return WIFI_ERROR_NOT_SUPPORTED;
        }
        return WIFI_SUCCESS;
    }

    int createSetPktFilterRequest(WifiRequest& request) {
        u8 *program = new u8[mProgramLen];
        NULL_CHECK_RETURN(program, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        int result = request.create(GOOGLE_OUI, APF_SUBCMD_SET_FILTER);
        if (result < 0) {
            delete[] program;
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        result = request.put_u32(APF_ATTRIBUTE_PROGRAM_LEN, mProgramLen);
        if (result < 0) {
            delete[] program;
            return result;
        }
        memcpy(program, mProgram, mProgramLen);
        result = request.put(APF_ATTRIBUTE_PROGRAM, program, mProgramLen);
        if (result < 0) {
            delete[] program;
            return result;
        }
        request.attr_end(data);
        delete[] program;
        return result;
    }

    int createGetPktFilterCapabilitesRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, APF_SUBCMD_GET_CAPABILITIES);
        if (result < 0) {
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);
        request.attr_end(data);
        return result;
    }

    int start() {
        WifiRequest request(familyId(), ifaceId());
        int result = createRequest(request);
        if (result < 0) {
            return result;
        }
        result = requestResponse(request);
        if (result < 0) {
            ALOGI("Request Response failed for APF, result = %d", result);
            return result;
        }
        ALOGI("Done!");
        return result;
    }

    int cancel() {
        return WIFI_SUCCESS;
    }

    int handleResponse(WifiEvent& reply) {
        ALOGD("In SetAPFCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        int id = reply.get_vendor_id();
        int subcmd = reply.get_vendor_subcmd();

        nlattr *vendor_data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = reply.get_vendor_data_len();

        ALOGD("Id = %0x, subcmd = %d, len = %d", id, subcmd, len);
        if (vendor_data == NULL || len == 0) {
            ALOGE("no vendor data in SetAPFCommand response; ignoring it");
            return NL_SKIP;
        }
        if( mReqType == SET_APF_PROGRAM) {
            ALOGD("Response recieved for set packet filter command\n");
        } else if (mReqType == GET_APF_CAPABILITIES) {
            *mVersion = 0;
            *mMaxLen = 0;
            ALOGD("Response recieved for get packet filter capabilities command\n");
            for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
                if (it.get_type() == APF_ATTRIBUTE_VERSION) {
                    *mVersion = it.get_u32();
                    ALOGI("APF version is %d\n", *mVersion);
                } else if (it.get_type() == APF_ATTRIBUTE_MAX_LEN) {
                    *mMaxLen = it.get_u32();
                    ALOGI("APF max len is %d\n", *mMaxLen);
                } else {
                    ALOGE("Ignoring invalid attribute type = %d, size = %d",
                            it.get_type(), it.get_len());
                }
            }
        }
        return NL_OK;
    }

    int handleEvent(WifiEvent& event) {
        /* No Event to recieve for APF commands */
        return NL_SKIP;
    }
};

class SetNdoffloadCommand : public WifiCommand {

private:
    u8 mEnable;
public:
    SetNdoffloadCommand(wifi_interface_handle handle, u8 enable)
        : WifiCommand("SetNdoffloadCommand", handle, 0) {
        mEnable = enable;
    }
    virtual int create() {
        int ret;

        ret = mMsg.create(GOOGLE_OUI, WIFI_SUBCMD_CONFIG_ND_OFFLOAD);
        if (ret < 0) {
            ALOGE("Can't create message to send to driver - %d", ret);
            return ret;
        }

        nlattr *data = mMsg.attr_start(NL80211_ATTR_VENDOR_DATA);
        ret = mMsg.put_u8(ANDR_WIFI_ATTRIBUTE_ND_OFFLOAD_VALUE, mEnable);
        if (ret < 0) {
             return ret;
        }

        mMsg.attr_end(data);
        return WIFI_SUCCESS;
    }
};

class GetFeatureSetCommand : public WifiCommand {

private:
    int feature_type;
    feature_set *fset;
    feature_set *feature_matrix;
    int *fm_size;
    int set_size_max;
public:
    GetFeatureSetCommand(wifi_interface_handle handle, int feature, feature_set *set,
         feature_set set_matrix[], int *size, int max_size)
        : WifiCommand("GetFeatureSetCommand", handle, 0)
    {
        feature_type = feature;
        fset = set;
        feature_matrix = set_matrix;
        fm_size = size;
        set_size_max = max_size;
    }

    virtual int create() {
        int ret;

        if(feature_type == ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET) {
            ret = mMsg.create(GOOGLE_OUI, WIFI_SUBCMD_GET_FEATURE_SET);
        } else if (feature_type == ANDR_WIFI_ATTRIBUTE_FEATURE_SET) {
            ret = mMsg.create(GOOGLE_OUI, WIFI_SUBCMD_GET_FEATURE_SET_MATRIX);
        } else {
            ALOGE("Unknown feature type %d", feature_type);
            return -1;
        }

        if (ret < 0) {
            ALOGE("Can't create message to send to driver - %d", ret);
        }

        return ret;
    }

protected:
    virtual int handleResponse(WifiEvent& reply) {

        ALOGV("In GetFeatureSetCommand::handleResponse");

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
            ALOGE("no vendor data in GetFeatureSetCommand response; ignoring it");
            return NL_SKIP;
        }
        if (feature_type == ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET) {
            if (!fset) {
                ALOGE("Buffers pointers not set");
                return NL_SKIP;
            }

            nl_iterator it(vendor_data);
            if (it.get_type() == ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET) {
                memcpy(fset, it.get_data(), min(it.get_len(), (int) sizeof(*fset)));
            } else {
                ALOGE("Unknown attribute: %d expecting %d",
                        it.get_type(), ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET);
                return NL_SKIP;
            }
            ALOGD("wifi feature set: 0x%08X", *(int *)fset);
        } else {
            int num_features_set = 0;
            int i = 0;

            if(!feature_matrix || !fm_size) {
                ALOGE("Buffers pointers not set");
                return NL_SKIP;
            }

            for (nl_iterator it(vendor_data); it.has_next(); it.next()) {
                if (it.get_type() == ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET) {
                    num_features_set = it.get_u32();
                    ALOGV("Got feature list with %d concurrent sets", num_features_set);
                    if(set_size_max && (num_features_set > set_size_max))
                        num_features_set = set_size_max;
                    *fm_size = num_features_set;
                } else if ((it.get_type() == ANDR_WIFI_ATTRIBUTE_FEATURE_SET) &&
                             i < num_features_set) {
                    feature_matrix[i] = it.get_u32();
                    i++;
                } else {
                    ALOGW("Ignoring invalid attribute type = %d, size = %d",
                            it.get_type(), it.get_len());
                }
            }

        }
        return NL_OK;
    }

};

class WiFiInterfaceCommand: public WifiCommand {
private:
    const char *mIfaceName;
    wifi_interface_type mIfaceType;
    bool mIfaceAdd;
public:
    WiFiInterfaceCommand(wifi_handle handle, const char *ifname, wifi_interface_type iftype)
        : WifiCommand("WiFiInterfaceAddCommand", handle, 0)
    {
        mIfaceName = ifname;
        mIfaceType = iftype;
        mIfaceAdd  = true;
    }

    WiFiInterfaceCommand(wifi_handle handle, const char *ifname)
        : WifiCommand("WiFiInterfaceDelCommand", handle, 0)
    {
        mIfaceName = ifname;
        mIfaceAdd  = false;
    }

    virtual int create() {
        int ret;
        if (mIfaceAdd == true) {
            u32 ifaceid = if_nametoindex("wlan0");
            if (!ifaceid) {
                ALOGE("%s: wlan0 not present", __func__);
                return WIFI_ERROR_UNKNOWN;
            }

            ret = mMsg.create(NL80211_CMD_NEW_INTERFACE);
            if (ret != WIFI_SUCCESS) {
                ALOGE("failed to create request; result = %d", ret);
                return ret;
            }

            nl80211_iftype type;
            switch(mIfaceType) {
            case WIFI_INTERFACE_TYPE_STA:    /* IfaceType:STA */
                type = NL80211_IFTYPE_STATION;
                break;
            case WIFI_INTERFACE_TYPE_AP:    /* IfaceType:AP */
                type = NL80211_IFTYPE_AP;
                break;
            case WIFI_INTERFACE_TYPE_P2P:    /* IfaceType:P2P */
                type = NL80211_IFTYPE_P2P_DEVICE;
                break;
            case WIFI_INTERFACE_TYPE_NAN:    /* IfaceType:NAN */
                type = NL80211_IFTYPE_NAN;
                break;
            default:
                ALOGE("%s: Wrong interface type %u", __func__, mIfaceType);
                return WIFI_ERROR_UNKNOWN;
            }

            mMsg.put_u32(NL80211_ATTR_IFINDEX, ifaceid);
            mMsg.put_string(NL80211_ATTR_IFNAME, mIfaceName);
            mMsg.put_u32(NL80211_ATTR_IFTYPE, type);
        } else {
            u32 ifaceid = if_nametoindex(mIfaceName);
            if (!ifaceid) {
                ALOGE("%s: %s not present", __func__, mIfaceName);
                return WIFI_ERROR_UNKNOWN;
            }

            ret = mMsg.create(NL80211_CMD_DEL_INTERFACE);
            if (ret != WIFI_SUCCESS) {
                ALOGE("failed to create request; result = %d", ret);
                return ret;
            }

            mMsg.put_u32(NL80211_ATTR_IFINDEX, ifaceid);
        }
        return WIFI_SUCCESS;
    }
};

static int wifi_get_multicast_id(wifi_handle handle, const char *name, const char *group)
{
    GetMulticastIdCommand cmd(handle, name, group);
    int res = cmd.requestResponse();
    if (res < 0)
        return res;
    else
        return cmd.getId();
}

/////////////////////////////////////////////////////////////////////////

static bool is_wifi_interface(const char *name)
{
    if (strncmp(name, "wlan", 4) != 0 && strncmp(name, "p2p", 3) != 0) {
        /* not a wifi interface; ignore it */
        return false;
    } else {
        return true;
    }
}

static int get_interface(const char *name, interface_info *info)
{
    strcpy(info->name, name);
    info->id = if_nametoindex(name);
    // ALOGI("found an interface : %s, id = %d", name, info->id);
    return WIFI_SUCCESS;
}

wifi_error wifi_init_interfaces(wifi_handle handle)
{
    hal_info *info = (hal_info *)handle;

    struct dirent *de;

    DIR *d = opendir("/sys/class/net");
    if (d == 0)
        return WIFI_ERROR_UNKNOWN;

    int n = 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.')
            continue;
        if (is_wifi_interface(de->d_name) ) {
            n++;
        }
    }

    closedir(d);

    if (n == 0)
        return WIFI_ERROR_NOT_AVAILABLE;

    d = opendir("/sys/class/net");
    if (d == 0)
        return WIFI_ERROR_UNKNOWN;

    info->interfaces = (interface_info **)malloc(sizeof(interface_info *) * n);

    int i = 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.')
            continue;
        if (is_wifi_interface(de->d_name)) {
            interface_info *ifinfo = (interface_info *)malloc(sizeof(interface_info));
            if (get_interface(de->d_name, ifinfo) != WIFI_SUCCESS) {
                free(ifinfo);
                continue;
            }
            ifinfo->handle = handle;
            info->interfaces[i] = ifinfo;
            i++;
        }
    }

    closedir(d);

    info->num_interfaces = n;
    return WIFI_SUCCESS;
}

wifi_error wifi_get_ifaces(wifi_handle handle, int *num, wifi_interface_handle **interfaces)
{
    hal_info *info = (hal_info *)handle;

    *interfaces = (wifi_interface_handle *)info->interfaces;
    *num = info->num_interfaces;

    return WIFI_SUCCESS;
}

wifi_interface_handle wifi_get_wlan_interface(wifi_handle info, wifi_interface_handle *ifaceHandles, int numIfaceHandles)
{
    char buf[EVENT_BUF_SIZE];
    wifi_interface_handle wlan0Handle;
    wifi_error res = wifi_get_ifaces((wifi_handle)info, &numIfaceHandles, &ifaceHandles);
    if (res < 0) {
        return NULL;
    }
    for (int i = 0; i < numIfaceHandles; i++) {
        if (wifi_get_iface_name(ifaceHandles[i], buf, sizeof(buf)) == WIFI_SUCCESS) {
            if (strncmp(buf, "wlan0", 5) == 0) {
                ALOGI("found interface %s\n", buf);
                wlan0Handle = ifaceHandles[i];
                return wlan0Handle;
            }
        }
    }
    return NULL;
}


wifi_error wifi_get_iface_name(wifi_interface_handle handle, char *name, size_t size)
{
    interface_info *info = (interface_info *)handle;
    strcpy(name, info->name);
    return WIFI_SUCCESS;
}

wifi_error wifi_get_supported_feature_set(wifi_interface_handle handle, feature_set *set)
{
    GetFeatureSetCommand command(handle, ANDR_WIFI_ATTRIBUTE_NUM_FEATURE_SET, set, NULL, NULL, 1);
    return (wifi_error) command.requestResponse();
}

wifi_error wifi_get_concurrency_matrix(wifi_interface_handle handle, int set_size_max,
       feature_set set[], int *set_size)
{
    GetFeatureSetCommand command(handle, ANDR_WIFI_ATTRIBUTE_FEATURE_SET, NULL,
            set, set_size, set_size_max);
    return (wifi_error) command.requestResponse();
}

wifi_error wifi_set_scanning_mac_oui(wifi_interface_handle handle, oui scan_oui)
{
    SetPnoMacAddrOuiCommand command(handle, scan_oui);
    return (wifi_error)command.start();

}

wifi_error wifi_set_nodfs_flag(wifi_interface_handle handle, u32 nodfs)
{
    SetNodfsCommand command(handle, nodfs);
    return (wifi_error) command.requestResponse();
}

wifi_error wifi_set_country_code(wifi_interface_handle handle, const char *country_code)
{
    SetCountryCodeCommand command(handle, country_code);
    return (wifi_error) command.requestResponse();
}

static wifi_error wifi_start_rssi_monitoring(wifi_request_id id, wifi_interface_handle
                        iface, s8 max_rssi, s8 min_rssi, wifi_rssi_event_handler eh)
{
    ALOGD("Start RSSI monitor %d", id);
    wifi_handle handle = getWifiHandle(iface);
    SetRSSIMonitorCommand *cmd = new SetRSSIMonitorCommand(id, iface, max_rssi, min_rssi, eh);
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

static wifi_error wifi_stop_rssi_monitoring(wifi_request_id id, wifi_interface_handle iface)
{
    ALOGD("Stopping RSSI monitor");

    if(id == -1) {
        wifi_rssi_event_handler handler;
        s8 max_rssi = 0, min_rssi = 0;
        wifi_handle handle = getWifiHandle(iface);
        memset(&handler, 0, sizeof(handler));
        SetRSSIMonitorCommand *cmd = new SetRSSIMonitorCommand(id, iface,
                                                    max_rssi, min_rssi, handler);
        NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
        cmd->cancel();
        cmd->releaseRef();
        return WIFI_SUCCESS;
    }
    return wifi_cancel_cmd(id, iface);
}

static wifi_error wifi_get_packet_filter_capabilities(wifi_interface_handle handle,
        u32 *version, u32 *max_len)
{
    ALOGD("Getting APF capabilities, halHandle = %p\n", handle);
    AndroidPktFilterCommand *cmd = new AndroidPktFilterCommand(handle, version, max_len);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = (wifi_error)cmd->start();
    if (result == WIFI_SUCCESS) {
        ALOGD("Getting APF capability, version = %d, max_len = %d\n", *version, *max_len);
    }
    cmd->releaseRef();
    return result;
}

static wifi_error wifi_set_packet_filter(wifi_interface_handle handle,
        const u8 *program, u32 len)
{
    ALOGD("Setting APF program, halHandle = %p\n", handle);
    AndroidPktFilterCommand *cmd = new AndroidPktFilterCommand(handle, program, len);
    NULL_CHECK_RETURN(cmd, "memory allocation failure", WIFI_ERROR_OUT_OF_MEMORY);
    wifi_error result = (wifi_error)cmd->start();
    cmd->releaseRef();
    return result;
}

static wifi_error wifi_configure_nd_offload(wifi_interface_handle handle, u8 enable)
{
    SetNdoffloadCommand command(handle, enable);
    return (wifi_error) command.requestResponse();
}

wifi_error wifi_virtual_interface_create(wifi_handle handle,
                                         const char* ifname,
                                         wifi_interface_type iface_type)
{
    ALOGD("%s, ifname: %s, iftype: %d", __func__, ifname, iface_type);

    if (strncmp(ifname, "wlan0", 5) == 0) {
        ALOGD("%s, skip for main iface %s", __func__, ifname);
        return WIFI_SUCCESS;
    }

    if (if_nametoindex(ifname)) {
        ALOGD("%s, skip for exsist iface %s", __func__, ifname);
        return WIFI_SUCCESS;
    }

    WiFiInterfaceCommand command(handle, ifname, iface_type);
    wifi_error status = (wifi_error) command.requestResponse();

    if (status == WIFI_SUCCESS) {
        hal_info *info = (hal_info *)handle;
        if (info->interfaces)
            free(info->interfaces);
        wifi_error status = wifi_init_interfaces(handle);
    }

    return status;
}

wifi_error wifi_virtual_interface_delete(wifi_handle handle,
                                         const char* ifname)
{
    ALOGD("%s, ifname: %s", __func__, ifname);

    if (strncmp(ifname, "wlan0", 5) == 0) {
        ALOGD("%s, skip for main iface %s", __func__, ifname);
        return WIFI_SUCCESS;
    }

    if (!if_nametoindex(ifname)) {
        ALOGD("%s, skip for none exsist iface %s", __func__, ifname);
        return WIFI_SUCCESS;
    }

    WiFiInterfaceCommand command(handle, ifname);
    wifi_error status = (wifi_error) command.requestResponse();

    if (status == WIFI_SUCCESS) {
        hal_info *info = (hal_info *)handle;
        if (info->interfaces)
            free(info->interfaces);
        wifi_error status = wifi_init_interfaces(handle);
    }

    return status;
}

/////////////////////////////////////////////////////////////////////////////


class UsableChannelCommand : public WifiCommand {

private:
    u32 mBandMask;
    u32 mIfaceModeMask;
    u32 mFilterMask;
    u32 mMaxSize;
    u32* mSize;
    wifi_usable_channel* mChannels;

public:
    UsableChannelCommand(wifi_interface_handle handle, u32 band_mask, u32 iface_mode_mask,
                   u32 filter_mask, u32 max_size, u32* size, wifi_usable_channel* channels)
        : WifiCommand("UsableChannelCommand", handle, 0),
                       mBandMask(band_mask), mIfaceModeMask(iface_mode_mask),
                       mFilterMask(filter_mask), mMaxSize(max_size),
		       mSize(size), mChannels(channels)
    {
    }

    int createRequest(WifiRequest& request) {
        createUsableChannelRequest(request);
        return WIFI_SUCCESS;
    }

    int createUsableChannelRequest(WifiRequest& request) {
        int result = request.create(GOOGLE_OUI, WIFI_SUBCMD_USABLE_CHANNEL);
        if (result < 0) {
            ALOGE("Failed to create UsableChannel request; result = %d", result);
            return result;
        }

        nlattr *data = request.attr_start(NL80211_ATTR_VENDOR_DATA);

        result = request.put_u32(USABLECHAN_ATTRIBUTE_BAND, mBandMask);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put log level; result = %d", result);
            return result;
        }
        result = request.put_u32(USABLECHAN_ATTRIBUTE_IFACE, mIfaceModeMask);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put ring flags; result = %d", result);
            return result;
        }
        result = request.put_u32(USABLECHAN_ATTRIBUTE_FILTER, mFilterMask);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put usablechan filter; result = %d", result);
            return result;
        }
        result = request.put_u32(USABLECHAN_ATTRIBUTE_MAX_SIZE, mMaxSize);
        if (result != WIFI_SUCCESS) {
            ALOGE("Failed to put usablechan max_size; result = %d", result);
            return result;
        }
        request.attr_end(data);

        return WIFI_SUCCESS;
    }

    int start() {
        WifiRequest request(familyId(), ifaceId());
        int result = createRequest(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to create request; result = %d", result);
            return result;
        }

        result = requestResponse(request);
        if (result != WIFI_SUCCESS) {
            ALOGE("failed to set scanning mac OUI; result = %d", result);
        }

        return result;
    }

    virtual int handleResponse(WifiEvent& reply) {
        ALOGD("In DebugCommand::handleResponse");

        if (reply.get_cmd() != NL80211_CMD_VENDOR) {
            ALOGD("Ignoring reply with cmd = %d", reply.get_cmd());
            return NL_SKIP;
        }

        nlattr *vendor_data = reply.get_attribute(NL80211_ATTR_VENDOR_DATA);
        int len = reply.get_vendor_data_len();
        wifi_usable_channel *channels(mChannels);

        if (vendor_data == NULL || len == 0) {
            ALOGE("No Debug data found");
            return NL_SKIP;
        }

        nl_iterator it(vendor_data);
        if (it.get_type() == USABLECHAN_ATTRIBUTE_SIZE) {
            *mSize = it.get_u32();
        } else {
            ALOGE("Unknown attribute: %d expecting %d",
                it.get_type(), USABLECHAN_ATTRIBUTE_SIZE);
            return NL_SKIP;
        }

        it.next();
        if (it.get_type() == USABLECHAN_ATTRIBUTE_CHANNELS) {
            memcpy(channels, it.get_data(), sizeof(wifi_usable_channel) * *mSize);
        } else {
            ALOGE("Unknown attribute: %d expecting %d",
                it.get_type(), USABLECHAN_ATTRIBUTE_SIZE);
            return NL_SKIP;
        }

        return NL_OK;
    }

    virtual int handleEvent(WifiEvent& event) {
        /* NO events! */
        return NL_SKIP;
    }
};

wifi_error wifi_get_usable_channels(wifi_handle handle, u32 band_mask, u32 iface_mode_mask,
                u32 filter_mask, u32 max_size, u32* size, wifi_usable_channel* channels)
{
    int numIfaceHandles = 0;
    wifi_interface_handle *ifaceHandles = NULL;
    wifi_interface_handle wlan0Handle;

    ALOGD("%s, TEST is function was callback", __func__);

    wlan0Handle = wifi_get_wlan_interface((wifi_handle)handle, ifaceHandles, numIfaceHandles);
    UsableChannelCommand command(wlan0Handle, band_mask, iface_mode_mask,
                                    filter_mask, max_size, size, channels);

    wifi_error result = (wifi_error)command.start();

    if (result != WIFI_SUCCESS)
        return WIFI_ERROR_NOT_SUPPORTED;

    return WIFI_SUCCESS;
}


