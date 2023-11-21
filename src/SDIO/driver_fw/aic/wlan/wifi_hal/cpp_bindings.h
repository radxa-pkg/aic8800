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

#include "wifi_hal.h"
#include "common.h"
#include "sync.h"

class WifiEvent
{
    /* TODO: remove this when nl headers are updated */
    static const unsigned NL80211_ATTR_MAX_INTERNAL = 256;
private:
    struct nl_msg *mMsg;
    struct genlmsghdr *mHeader;
    struct nlattr *mAttributes[NL80211_ATTR_MAX_INTERNAL + 1];

public:
    WifiEvent(nl_msg *msg) {
        mMsg = msg;
        mHeader = NULL;
        memset(mAttributes, 0, sizeof(mAttributes));
    }
    ~WifiEvent() {
        /* don't destroy mMsg; it doesn't belong to us */
    }

    void log();

    int parse();

    genlmsghdr *header() {
        return mHeader;
    }

    int get_cmd() {
        return mHeader->cmd;
    }

    int get_vendor_id() {
        return get_u32(NL80211_ATTR_VENDOR_ID);
    }

    int get_vendor_subcmd() {
        return get_u32(NL80211_ATTR_VENDOR_SUBCMD);
    }

    void *get_vendor_data() {
        return get_data(NL80211_ATTR_VENDOR_DATA);
    }

    int get_vendor_data_len() {
        return get_len(NL80211_ATTR_VENDOR_DATA);
    }

    const char *get_cmdString();

    nlattr ** attributes() {
        return mAttributes;
    }

    nlattr *get_attribute(int attribute) {
        return mAttributes[attribute];
    }

    uint8_t get_u8(int attribute) {
        return mAttributes[attribute] ? nla_get_u8(mAttributes[attribute]) : 0;
    }

    uint16_t get_u16(int attribute) {
        return mAttributes[attribute] ? nla_get_u16(mAttributes[attribute]) : 0;
    }

    uint32_t get_u32(int attribute) {
        return mAttributes[attribute] ? nla_get_u32(mAttributes[attribute]) : 0;
    }

    uint64_t get_u64(int attribute) {
        return mAttributes[attribute] ? nla_get_u64(mAttributes[attribute]) : 0;
    }

    int get_len(int attribute) {
        return mAttributes[attribute] ? nla_len(mAttributes[attribute]) : 0;
    }

    void *get_data(int attribute) {
        return mAttributes[attribute] ? nla_data(mAttributes[attribute]) : NULL;
    }

private:
    WifiEvent(const WifiEvent&);        // hide copy constructor to prevent copies
};

class nl_iterator {
    struct nlattr *pos;
    int rem;
public:
    nl_iterator(struct nlattr *attr) {
        pos = (struct nlattr *)nla_data(attr);
        rem = nla_len(attr);
    }
    bool has_next() {
        return nla_ok(pos, rem);
    }
    void next() {
        pos = (struct nlattr *)nla_next(pos, &(rem));
    }
    struct nlattr *get() {
        return pos;
    }
    uint16_t get_type() {
        return pos->nla_type;
    }
    uint8_t get_u8() {
        return nla_get_u8(pos);
    }
    uint16_t get_u16() {
        return nla_get_u16(pos);
    }
    uint32_t get_u32() {
        return nla_get_u32(pos);
    }
    uint64_t get_u64() {
        return nla_get_u64(pos);
    }
    void* get_data() {
        return nla_data(pos);
    }
    int get_len() {
        return nla_len(pos);
    }
private:
    nl_iterator(const nl_iterator&);    // hide copy constructor to prevent copies
};

class WifiRequest
{
private:
    int mFamily;
    int mIface;
    struct nl_msg *mMsg;

public:
    WifiRequest(int family) {
        mMsg = NULL;
        mFamily = family;
        mIface = -1;
    }

    WifiRequest(int family, int iface) {
        mMsg = NULL;
        mFamily = family;
        mIface = iface;
    }

    ~WifiRequest() {
        destroy();
    }

    void destroy() {
        if (mMsg) {
            nlmsg_free(mMsg);
            mMsg = NULL;
        }
    }

    nl_msg *getMessage() {
        return mMsg;
    }

    /* Command assembly helpers */
    int create(int family, uint8_t cmd, int flags, int hdrlen);
    int create(uint8_t cmd) {
        return create(mFamily, cmd, 0, 0);
    }

    int create(uint32_t id, int subcmd);

    int put(int attribute, void *ptr, unsigned len) {
        return nla_put(mMsg, attribute, len, ptr);
    }
    int put_u8(int attribute, uint8_t value) {
        return nla_put(mMsg, attribute, sizeof(value), &value);
    }
    int put_u16(int attribute, uint16_t value) {
        return nla_put(mMsg, attribute, sizeof(value), &value);
    }
    int put_u32(int attribute, uint32_t value) {
        return nla_put(mMsg, attribute, sizeof(value), &value);
    }
    int put_u64(int attribute, uint64_t value) {
        return nla_put(mMsg, attribute, sizeof(value), &value);
    }
    int put_string(int attribute, const char *value) {
        return nla_put(mMsg, attribute, strlen(value) + 1, value);
    }
    int put_addr(int attribute, mac_addr value) {
        return nla_put(mMsg, attribute, sizeof(mac_addr), value);
    }

    struct nlattr * attr_start(int attribute) {
        return nla_nest_start(mMsg, attribute);
    }
    void attr_end(struct nlattr *attr) {
        nla_nest_end(mMsg, attr);
    }

    int set_iface_id(int ifindex) {
        return put_u32(NL80211_ATTR_IFINDEX, ifindex);
    }
private:
    WifiRequest(const WifiRequest&);        // hide copy constructor to prevent copies

};

class WifiCommand
{
protected:
    const char *mType;
    hal_info *mInfo;
    WifiRequest mMsg;
    Condition mCondition;
    wifi_request_id mId;
    interface_info *mIfaceInfo;
    int mRefs;
public:
    WifiCommand(const char *type, wifi_handle handle, wifi_request_id id)
            : mType(type), mMsg(getHalInfo(handle)->nl80211_family_id), mId(id), mRefs(1)
    {
        mIfaceInfo = NULL;
        mInfo = getHalInfo(handle);
        // ALOGD("WifiCommand %p created, mInfo = %p, mIfaceInfo = %p", this, mInfo, mIfaceInfo);
    }

    WifiCommand(const char *type, wifi_interface_handle iface, wifi_request_id id)
            : mType(type), mMsg(getHalInfo(iface)->nl80211_family_id, getIfaceInfo(iface)->id),
            mId(id), mRefs(1)
    {
        mIfaceInfo = getIfaceInfo(iface);
        mInfo = getHalInfo(iface);
        // ALOGD("WifiCommand %p created, mInfo = %p, mIfaceInfo = %p", this, mInfo, mIfaceInfo);
    }

    virtual ~WifiCommand() {
        // ALOGD("WifiCommand %p destroyed", this);
    }

    wifi_request_id id() {
        return mId;
    }

    const char *getType() {
        return mType;
    }

    virtual void addRef() {
        int refs = __sync_add_and_fetch(&mRefs, 1);
        // ALOGD("addRef: WifiCommand %p has %d references", this, refs);
    }

    virtual void releaseRef() {
        int refs = __sync_sub_and_fetch(&mRefs, 1);
        if (refs == 0) {
            delete this;
        } else {
            // ALOGD("releaseRef: WifiCommand %p has %d references", this, refs);
        }
    }

    virtual int create() {
        /* by default there is no way to cancel */
        ALOGD("WifiCommand %p can't be created", this);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    virtual int cancel() {
        /* by default there is no way to cancel */
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    int requestResponse();
    int requestEvent(int cmd);
    int requestVendorEvent(uint32_t id, int subcmd);
    int requestResponse(WifiRequest& request);

protected:
    wifi_handle wifiHandle() {
        return getWifiHandle(mInfo);
    }

    wifi_interface_handle ifaceHandle() {
        return getIfaceHandle(mIfaceInfo);
    }

    int familyId() {
        return mInfo->nl80211_family_id;
    }

    int ifaceId() {
        return mIfaceInfo->id;
    }

    /* Override this method to parse reply and dig out data; save it in the object */
    virtual int handleResponse(WifiEvent& reply) {
        ALOGI("skipping a response");
        return NL_SKIP;
    }

    /* Override this method to parse event and dig out data; save it in the object */
    virtual int handleEvent(WifiEvent& event) {
        ALOGI("skipping an event");
        return NL_SKIP;
    }

    int registerHandler(int cmd) {
        return wifi_register_handler(wifiHandle(), cmd, &event_handler, this);
    }

    void unregisterHandler(int cmd) {
        wifi_unregister_handler(wifiHandle(), cmd);
    }

    int registerVendorHandler(uint32_t id, int subcmd) {
        return wifi_register_vendor_handler(wifiHandle(), id, subcmd, &event_handler, this);
    }

    void unregisterVendorHandler(uint32_t id, int subcmd) {
        wifi_unregister_vendor_handler(wifiHandle(), id, subcmd);
    }

private:
    WifiCommand(const WifiCommand& );           // hide copy constructor to prevent copies

    /* Event handling */
    static int response_handler(struct nl_msg *msg, void *arg);

    static int event_handler(struct nl_msg *msg, void *arg);

    /* Other event handlers */
    static int valid_handler(struct nl_msg *msg, void *arg);

    static int ack_handler(struct nl_msg *msg, void *arg);

    static int finish_handler(struct nl_msg *msg, void *arg);

    static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg);
};

/* nl message processing macros (required to pass C++ type checks) */

#define for_each_attr(pos, nla, rem) \
    for (pos = (nlattr *)nla_data(nla), rem = nla_len(nla); \
        nla_ok(pos, rem); \
        pos = (nlattr *)nla_next(pos, &(rem)))

