/******************************************************************************
 *
 *  Copyright (C) 2019-2021 Aicsemi Corporation.
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
#define LOG_TAG "aic_btsnoop_net"
#include "aic_btsnoop_net.h"
#include <unistd.h>

#define AIC_NO_INTR(fn)  do {} while ((fn) == -1 && errno == EINTR)

#define DATA_DIRECT_2_ELLISY 1

#define HCI_COMMAND         0x01
#define HCI_ACL_DATA_H2C    0x02
#define HCI_ACL_DATA_C2H    0x82
#define HCI_SCO_DATA_H2C    0x03
#define HCI_SCO_DATA_C2H    0x83
#define HCI_EVENT           0x84

#define HCI_COMMAND_PKT         0x01
#define HCI_ACLDATA_PKT         0x02
#define HCI_SCODATA_PKT         0x03
#define HCI_EVENT_PKT           0x04


unsigned int aicbt_h5logfilter = 0x01;
bool aic_btsnoop_dump = false;
bool aic_btsnoop_net_dump = false;
bool aic_btsnoop_save_log = false;
char aic_btsnoop_path[1024] = {'\0'};
static pthread_mutex_t btsnoop_log_lock;


static void aic_safe_close_(int *fd);
static void *aic_listen_fn_(void *context);

static const char *AIC_LISTEN_THREAD_NAME_ = "aic_btsnoop_net";
static const int AIC_LOCALHOST_ = 0xC0A80AE2;       // 192.168.10.226
static const int AIC_LISTEN_PORT_ = 8872;

static const int AIC_REMOTEHOST_ = 0xC0A80A03;       // 192.168.10.21
static const int AIC_REMOTE_PORT_ = 24352;


static pthread_t aic_listen_thread_;
static bool aic_listen_thread_valid_ = false;
static pthread_mutex_t aic_client_socket_lock_ = PTHREAD_MUTEX_INITIALIZER;
static int aic_listen_socket_ = -1;

// File descriptor for btsnoop file.
static int hci_btsnoop_fd = -1;
// Epoch in microseconds since 01/01/0000.
static const uint64_t BTSNOOP_EPOCH_DELTA = 0x00dcddb30f2f8000ULL;


static uint64_t aic_btsnoop_timestamp(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  // Timestamp is in microseconds.
  uint64_t timestamp = tv.tv_sec * 1000LL * 1000LL;
  timestamp += tv.tv_usec;
  timestamp += BTSNOOP_EPOCH_DELTA;
  return timestamp;
}

void aic_btsnoop_open()
{
    pthread_mutex_init(&btsnoop_log_lock, NULL);
    char last_log_path[PATH_MAX];
    uint64_t timestamp;
    uint32_t usec;

    if (hci_btsnoop_fd != -1) {
      ALOGE("%s btsnoop log file is already open.", __func__);
      return;
    }

    if(aic_btsnoop_save_log) {
        time_t current_time = time(NULL);
        struct tm* time_created = localtime(&current_time);
        char config_time_created[sizeof("YYYY-MM-DD-HH:MM:SS")];
        strftime(config_time_created, sizeof("YYYY-MM-DD-HH:MM:SS"), "%Y-%m-%d-%H:%M:%S",
             time_created);
        timestamp = aic_btsnoop_timestamp() - BTSNOOP_EPOCH_DELTA;
        usec = (uint32_t)(timestamp % 1000000LL);
        snprintf(last_log_path, PATH_MAX, "%s.%s:%dUS", aic_btsnoop_path, config_time_created, usec);
        if (!rename(aic_btsnoop_path, last_log_path) && errno != ENOENT)
            ALOGE("%s unable to rename '%s' to '%s': %s", __func__, aic_btsnoop_path, last_log_path, strerror(errno));
    }
    else {
        snprintf(last_log_path, PATH_MAX, "%s.last", aic_btsnoop_path);
        if (!rename(aic_btsnoop_path, last_log_path) && errno != ENOENT)
            ALOGE("%s unable to rename '%s' to '%s': %s", __func__, aic_btsnoop_path, last_log_path, strerror(errno));
    }

    hci_btsnoop_fd = open(aic_btsnoop_path,
                          O_WRONLY | O_CREAT | O_TRUNC,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

    if (hci_btsnoop_fd == -1) {
      ALOGE("%s unable to open '%s': %s", __func__, aic_btsnoop_path, strerror(errno));
      return;
    }

    write(hci_btsnoop_fd, "btsnoop\0\0\0\0\1\0\0\x3\xea", 16);
}

void aic_btsnoop_close(void) {
    pthread_mutex_destroy(&btsnoop_log_lock);
    if (hci_btsnoop_fd != -1)
        close(hci_btsnoop_fd);
    hci_btsnoop_fd = -1;
}

static void aic_btsnoop_write(const void *data, size_t length) {
    if (hci_btsnoop_fd != -1)
        write(hci_btsnoop_fd, data, length);
}

static void aic_btsnoop_write_packet(serial_data_type_t type, const uint8_t *packet, bool is_received) {
    int length_he = 0;
    int length;
    int flags;
    int drops = 0;
    pthread_mutex_lock(&btsnoop_log_lock);
    switch (type) {
    case HCI_COMMAND_PKT:
        length_he = packet[2] + 4;
        flags = 2;
    break;
    case HCI_ACLDATA_PKT:
        length_he = (packet[3] << 8) + packet[2] + 5;
        flags = is_received;
    break;
    case HCI_SCODATA_PKT:
        length_he = packet[2] + 4;
        flags = is_received;
    break;
    case HCI_EVENT_PKT:
        length_he = packet[1] + 3;
        flags = 3;
    break;
    default:
        break;
    }

    uint64_t timestamp = aic_btsnoop_timestamp();
    uint32_t time_hi = timestamp >> 32;
    uint32_t time_lo = timestamp & 0xFFFFFFFF;

    length = htonl(length_he);
    flags = htonl(flags);
    drops = htonl(drops);
    time_hi = htonl(time_hi);
    time_lo = htonl(time_lo);

    aic_btsnoop_write(&length, 4);
    aic_btsnoop_write(&length, 4);
    aic_btsnoop_write(&flags, 4);
    aic_btsnoop_write(&drops, 4);
    aic_btsnoop_write(&time_hi, 4);
    aic_btsnoop_write(&time_lo, 4);
    aic_btsnoop_write(&type, 1);
    aic_btsnoop_write(packet, length_he - 1);
    pthread_mutex_unlock(&btsnoop_log_lock);
}

void aic_btsnoop_capture(const HC_BT_HDR *p_buf, bool is_rcvd) {
  const uint8_t *p = (const uint8_t *)(p_buf + 1) + p_buf->offset;

  if (hci_btsnoop_fd == -1)
    return;

  switch (p_buf->event & MSG_EVT_MASK) {
    case MSG_HC_TO_STACK_HCI_EVT:
    if((*(p + 3) == 0x94) && (*(p + 4) == 0xfc) && (*(p + 5) == 0x00)&&(aicbt_h5logfilter&1)){}
    else
      aic_btsnoop_write_packet(HCI_EVENT_PKT, p, false);
      break;
    case MSG_HC_TO_STACK_HCI_ACL:
    case MSG_STACK_TO_HC_HCI_ACL:
      aic_btsnoop_write_packet(HCI_ACLDATA_PKT, p, is_rcvd);
      break;
    case MSG_HC_TO_STACK_HCI_SCO:
    case MSG_STACK_TO_HC_HCI_SCO:
      aic_btsnoop_write_packet(HCI_SCODATA_PKT, p, is_rcvd);
      break;
    case MSG_STACK_TO_HC_HCI_CMD:
      if(((aicbt_h5logfilter & 1) == 0) || (*p != 0x94) || (*(p + 1) != 0xfc))
      aic_btsnoop_write_packet(HCI_COMMAND_PKT, p, true);
      break;
  }
}

void aic_btsnoop_net_open() {
    aic_listen_thread_valid_ = (pthread_create(&aic_listen_thread_, NULL, aic_listen_fn_, NULL) == 0);
    if (!aic_listen_thread_valid_) {
        ALOGE("%s pthread_create failed: %s", __func__, strerror(errno));
    } else {
        ALOGD("initialized");
    }
}

void aic_btsnoop_net_close() {
    if (aic_listen_thread_valid_) {
        shutdown(aic_listen_socket_, SHUT_RDWR);
        pthread_join(aic_listen_thread_, NULL);
        aic_listen_thread_valid_ = false;
    }
}

void aic_btsnoop_net_write(serial_data_type_t type, uint8_t *data, bool is_received) {
    if (aic_listen_socket_ == -1) {
        return;
    }
    int length = 0;
    uint8_t *p = data;

    switch (type) {
    case HCI_COMMAND_PKT:
        if(((aicbt_h5logfilter & 1) == 0) || (*p != 0x94) || (*(p + 1) != 0xfc))
            length = data[2] + 3;
        else
            return;
    break;
    case HCI_ACLDATA_PKT:
        length = (data[3] << 8) + data[2] + 4;
    break;
    case HCI_SCODATA_PKT:
        length = data[2] + 3;
    break;
    case HCI_EVENT_PKT:
    if((*(p + 3) == 0x94) && (*(p + 4) == 0xfc) && (*(p + 5) == 0x00)&&(aicbt_h5logfilter&1)){return;}
    else
        length = data[1] + 2;
    break;
    default:
        break;
    }


    uint8_t buffer[4126] = {0};
    //uint8_t test_buffer[] = {0x03, 0x00, 0x00, 0x00, 0x01, 0x01, 0x10, 0x00};
    //uint8_t test_buffer2[] = {0x01, 0x10, 0x00};
    struct sockaddr_in client_addr;
    int i = 0;

#if DATA_DIRECT_2_ELLISY
    uint8_t bit_rate[4] = {0x00, 0x1b, 0x37, 0x4b};
    struct tm *t;
    time_t tt;
    time(&tt);
    t = localtime(&tt);

    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t nano_time = (t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec) * 1000 * 1000LL * 1000 + tv.tv_usec * 1000;
    uint16_t year = (t->tm_year + 1900) & 0xFFFF;
    uint8_t  month = (t->tm_mon+1) & 0xFF;
    uint8_t  day =
    buffer[0] = 0x02;
    buffer[1] = 0x00;
    buffer[2] = 0x01;
    buffer[3] = 0x02;
    //time
    memcpy(&buffer[4], &year, 2);
    buffer[6] = month;
    buffer[7] = day;
    memcpy(&buffer[8], &nano_time, 6);
    //bit rate
    buffer[14] = 0x80;
    memcpy(&buffer[15], bit_rate, 4);
    //type
    buffer[19] = 0x81;
    i = 20;
#else
    memcpy(&buffer[i], &length, sizeof(int));
    i = 4;
#endif
    switch (type) {
        case HCI_COMMAND_PKT:
        buffer[i] = HCI_COMMAND;
        break;

        case HCI_ACLDATA_PKT:
        if(is_received) {
            buffer[i] = HCI_ACL_DATA_C2H;
        }
        else {
            buffer[i] = HCI_ACL_DATA_H2C;
        }
        break;

        case HCI_SCODATA_PKT:
        if(is_received) {
            buffer[i] = HCI_SCO_DATA_C2H;
        }
        else {
            buffer[i] = HCI_SCO_DATA_H2C;
        }
        break;

        case HCI_EVENT_PKT:
        buffer[i] = HCI_EVENT;
        break;

        default:
        buffer[i] = 0;
        break;

    }
#if DATA_DIRECT_2_ELLISY
    //buffer[i] = HCI_COMMAND;
    buffer[21] = 0x82;
    i = 22;
#else
    i = 5;
#endif
    memcpy(&buffer[i], data, length);
    //memcpy(&buffer[i], test_buffer2, 3);
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(AIC_REMOTEHOST_);
    client_addr.sin_port = htons(AIC_REMOTE_PORT_);
    pthread_mutex_lock(&aic_client_socket_lock_);
    int ret;
    AIC_NO_INTR(ret = sendto(aic_listen_socket_, buffer, (length+i), 0,(struct sockaddr*)&client_addr, sizeof(struct sockaddr_in)));
    //sendto(aic_listen_socket_, buffer, 25, 0,(struct sockaddr*)&client_addr, sizeof(struct sockaddr_in));
    pthread_mutex_unlock(&aic_client_socket_lock_);
}

static void *aic_listen_fn_(void *context) {
    AIC_UNUSED(context);
    prctl(PR_SET_NAME, (unsigned long)AIC_LISTEN_THREAD_NAME_, 0, 0, 0);

    aic_listen_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (aic_listen_socket_ == -1) {
        ALOGE("%s socket creation failed: %s", __func__, strerror(errno));
        goto cleanup;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(AIC_LOCALHOST_);
    addr.sin_port = htons(AIC_LISTEN_PORT_);

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(AIC_REMOTEHOST_);
    client_addr.sin_port = htons(AIC_REMOTE_PORT_);

    if (bind(aic_listen_socket_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        ALOGE("%s unable to bind listen socket: %s", __func__, strerror(errno));
        goto cleanup;
    }

    return NULL;
cleanup:
    aic_safe_close_(&aic_listen_socket_);
    return NULL;
}

static void aic_safe_close_(int *fd) {
  assert(fd != NULL);
  if (*fd != -1) {
    close(*fd);
    *fd = -1;
  }
}
