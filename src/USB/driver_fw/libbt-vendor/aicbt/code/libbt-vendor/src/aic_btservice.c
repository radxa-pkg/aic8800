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

/******************************************************************************
 *
 *  Filename:      aic_btservice.c
 *
 *  Description:   start unix socketc
 *
 ******************************************************************************/

#define LOG_TAG "bt_service"
#define AICBT_RELEASE_NAME "20200318_BT_ANDROID_10.0"

#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <termios.h>
#include <sys/syscall.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include "bt_hci_bdroid.h"
#include "bt_vendor_aic.h"
#include "userial.h"
#include "userial_vendor.h"
#include "aic_btservice.h"
#include "upio.h"
#include <unistd.h>
#include <sys/eventfd.h>
#include <semaphore.h>
#include <endian.h>
#include <byteswap.h>
#include <sys/un.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "bt_vendor_lib.h"


//HCI VENDOR Command opcode
#define HCI_VSC_READ_REGISTER           0xFFFF

#define AICBTSERVICE_SOCKETPATH "@/data/misc/bluedroid/aicbt_service.sock"
#define MAX_CONNECTION_NUMBER 10

#define AIC_HCICMD          0x01
#define AIC_CLOSESOCRET     0x02
#define AIC_INNER           0x03
#define AIC_STRING          0x04
#define OTHER               0xff

#define Aic_Service_Data_SIZE   259
#define Aic_Service_Send_Data_SIZE   259

#define HCICMD_REPLY_TIMEOUT_VALUE  8000 //ms
#define HCI_CMD_PREAMBLE_SIZE   3

typedef void (*tTIMER_HANDLE_CBACK)(union sigval sigval_value);



typedef struct Aic_Btservice_Info
{
    int socketfd;
    int sig_fd[2];
    pthread_t       cmdreadythd;
    pthread_t       epollthd;
    int             current_client_sock;
    int             epoll_fd;
    int             autopair_fd;
    sem_t           cmdqueue_sem;
    sem_t           cmdsend_sem;
    timer_t         timer_hcicmd_reply;
    RT_LIST_HEAD    cmdqueue_list;
    pthread_mutex_t cmdqueue_mutex;
    RT_LIST_HEAD    socket_node_list;
    volatile uint8_t cmdqueue_thread_running;
    volatile uint8_t epoll_thread_running;
    void            (*current_complete_cback)(void *);
    uint16_t        opcode;
}Aic_Btservice_Info;

typedef struct Aic_Service_Data
{
    uint16_t        opcode;
    uint8_t         parameter_len;
    uint8_t         *parameter;
    void            (*complete_cback)(void *);
}Aic_Service_Data;

typedef struct Aic_Queue_Data
{
    RT_LIST_ENTRY   list;
    int             client_sock;
    uint16_t        opcode;
    uint8_t         parameter_len;
    uint8_t         *parameter;
    void            (*complete_cback)(void *);
}Aicqueuedata;

typedef struct Aic_socket_node
{
    RT_LIST_ENTRY   list;
    int             client_fd;
}Aicqueuenode;

extern void aic_vendor_cmd_to_fw(uint16_t opcode, uint8_t parameter_len, uint8_t* parameter, tINT_CMD_CBACK p_cback);
static Aic_Btservice_Info *aic_btservice = NULL;
static void Aic_Service_Send_Hwerror_Event();
//extern void userial_recv_rawdata_hook(unsigned char *, unsigned int);
static timer_t OsAllocateTimer(tTIMER_HANDLE_CBACK timer_callback)
{
    struct sigevent sigev;
    timer_t timerid;

    memset(&sigev, 0, sizeof(struct sigevent));
    // Create the POSIX timer to generate signo
    sigev.sigev_notify = SIGEV_THREAD;
    //sigev.sigev_notify_thread_id = syscall(__NR_gettid);
    sigev.sigev_notify_function = timer_callback;
    sigev.sigev_value.sival_ptr = aic_btservice;

    ALOGD("OsAllocateTimer bt_service sigev.sigev_notify_thread_id = syscall(__NR_gettid)!");
    //Create the Timer using timer_create signal

    if (timer_create(CLOCK_REALTIME, &sigev, &timerid) == 0)
    {
        return timerid;
    }
    else
    {
        ALOGE("timer_create error!");
        return (timer_t)-1;
    }
}

static int OsFreeTimer(timer_t timerid)
{
    int ret = 0;
    if(timerid == (timer_t)-1) {
        ALOGE("OsFreeTimer fail timer id error");
        return -1;
    }
    ret = timer_delete(timerid);
    if(ret != 0)
        ALOGE("timer_delete fail with errno(%d)", errno);

    return ret;
}


 static int OsStartTimer(timer_t timerid, int msec, int mode)
 {
    struct itimerspec itval;

    if(timerid == (timer_t)-1) {
        ALOGE("OsStartTimer fail timer id error");
        return -1;
    }
    itval.it_value.tv_sec = msec / 1000;
    itval.it_value.tv_nsec = (long)(msec % 1000) * (1000000L);

    if (mode == 1)
    {
        itval.it_interval.tv_sec    = itval.it_value.tv_sec;
        itval.it_interval.tv_nsec = itval.it_value.tv_nsec;
    }
    else
    {
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = 0;
    }

    //Set the Timer when to expire through timer_settime

    if (timer_settime(timerid, 0, &itval, NULL) != 0)
    {
        ALOGE("time_settime error!");
        return -1;
    }

    return 0;

}

 static int OsStopTimer(timer_t timerid)
 {
    return OsStartTimer(timerid, 0, 0);
 }

static void init_cmdqueue_hash(Aic_Btservice_Info* aic_info)
{
    RT_LIST_HEAD* head = &aic_info->cmdqueue_list;
    ListInitializeHeader(head);
}

static void delete_cmdqueue_from_hash(Aicqueuedata* desc)
{
    if (desc)
    {
        ListDeleteNode(&desc->list);
        free(desc);
        desc = NULL;
    }
}

static void flush_cmdqueue_hash(Aic_Btservice_Info* aic_info)
{
    RT_LIST_HEAD* head = &aic_info->cmdqueue_list;
    RT_LIST_ENTRY* iter = NULL, *temp = NULL;
    Aicqueuedata* desc = NULL;

    pthread_mutex_lock(&aic_info->cmdqueue_mutex);
    LIST_FOR_EACH_SAFELY(iter, temp, head)
    {
        desc = LIST_ENTRY(iter, Aicqueuedata, list);
        delete_cmdqueue_from_hash(desc);
    }
    //ListInitializeHeader(head);
    pthread_mutex_unlock(&aic_info->cmdqueue_mutex);
}

static void hcicmd_reply_timeout_handler(union sigval sigev_value)
{
    Aic_Btservice_Info* btservice;
    btservice = (Aic_Btservice_Info*)sigev_value.sival_ptr;
    ALOGE("%s: opcode 0x%x", __func__, btservice->opcode);
    if(aic_btservice->opcode == 0)
      Aic_Service_Send_Hwerror_Event();
}

static bool hcicmd_alloc_reply_timer()
{
    // Create and set the timer when to expire
    aic_btservice->timer_hcicmd_reply = OsAllocateTimer(hcicmd_reply_timeout_handler);

    if(aic_btservice->timer_hcicmd_reply == (timer_t)-1) {
        ALOGE("%s : alloc reply timer fail!", __func__);
        return false;
    }
    return true;

}

static int hcicmd_free_reply_timer()
{
    if(aic_btservice->timer_hcicmd_reply != (timer_t)-1)
      return OsFreeTimer(aic_btservice->timer_hcicmd_reply);

    aic_btservice->timer_hcicmd_reply = (timer_t)-1;
    return 0;
}


static int hcicmd_start_reply_timer()
{
    return OsStartTimer(aic_btservice->timer_hcicmd_reply, HCICMD_REPLY_TIMEOUT_VALUE, 1);
}

static int hcicmd_stop_reply_timer()
{
    return OsStopTimer(aic_btservice->timer_hcicmd_reply);
}

static void Aic_Client_Cmd_Cback(void *p_mem)
{
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_mem;
    unsigned char *sendbuf = NULL;
    ssize_t ret = -1;

    if(p_evt_buf != NULL)
    {
        sendbuf = (uint8_t *)(p_evt_buf + 1) + p_evt_buf->offset;
        if(aic_btservice->current_client_sock != -1)
        {
            if(p_evt_buf->event != HCIT_TYPE_EVENT)
              return;
            uint8_t type = HCIT_TYPE_EVENT;
            AIC_NO_INTR(ret = send(aic_btservice->current_client_sock,&type, 1, MSG_NOSIGNAL));
            if(ret < 0) {
              ALOGE("%s send type errno: %s", __func__, strerror(errno));
              return;
            }

            AIC_NO_INTR(ret = send(aic_btservice->current_client_sock,sendbuf,p_evt_buf->len, MSG_NOSIGNAL));
            if(ret < 0)
              ALOGE("%s errno: %s", __func__, strerror(errno));
        }
        else
        {
            ALOGE("%s current_client_sock is not exist!", __func__);
        }
    }
}

void Aic_Service_Vendorcmd_Hook(Aic_Service_Data *AicData, int client_sock)
{
    Aicqueuedata* aicqueue_data = NULL;
    pthread_mutex_lock(&aic_btservice->cmdqueue_mutex);
    if(!aic_btservice || (aic_btservice->cmdqueue_thread_running == 0)){
        ALOGE("aicbt service is null or cmdqueue stop");
        pthread_mutex_unlock(&aic_btservice->cmdqueue_mutex);
        return;
    }

    aicqueue_data = (Aicqueuedata *)malloc(sizeof(Aicqueuedata));
    if (NULL == aicqueue_data)
    {
        ALOGE("aicqueue_data: allocate error");
        if(AicData->parameter_len > 0) {
            free(AicData->parameter);
        }
        pthread_mutex_unlock(&aic_btservice->cmdqueue_mutex);
        return;
    }

    aicqueue_data->opcode = AicData->opcode;
    aicqueue_data->parameter = AicData->parameter;
    aicqueue_data->parameter_len = AicData->parameter_len;
    aicqueue_data->client_sock = client_sock;
    aicqueue_data->complete_cback = AicData->complete_cback;

    ListAddToTail(&(aicqueue_data->list), &(aic_btservice->cmdqueue_list));
    pthread_mutex_unlock(&aic_btservice->cmdqueue_mutex);
    sem_post(&aic_btservice->cmdqueue_sem);
}

static void Aic_Service_Cmd_Event_Cback(void *p_mem)
{
    hcicmd_stop_reply_timer();
    if(p_mem != NULL)
    {
        if(aic_btservice->current_complete_cback != NULL)
        {
            (*aic_btservice->current_complete_cback)(p_mem);
        }
        else
        {
            ALOGE("%s current_complete_cback is not exist!", __func__);
        }
        aic_btservice->current_complete_cback = NULL;
        aic_btservice->opcode = 0;
        sem_post(&aic_btservice->cmdsend_sem);
    }
}

static void Aic_Service_Send_Hwerror_Event()
{
    unsigned char p_buf[100];
    int length;
    p_buf[0] = HCIT_TYPE_EVENT;//event
    p_buf[1] = HCI_VSE_SUBCODE_DEBUG_INFO_SUB_EVT;//firmwre event log
    p_buf[3] = 0x01;// host log opcode
    length = sprintf((char *)&p_buf[4], "aic service error\n");
    p_buf[2] = length + 2;//len
    length = length + 1 + 4;
    userial_recv_rawdata_hook(p_buf,length);

    length = 4;
    p_buf[0] = HCIT_TYPE_EVENT;//event
    p_buf[1] = HCI_HARDWARE_ERROR_EVT;//hardware error
    p_buf[2] = 0x01;//len
    p_buf[3] = AICSERVICE_HWERR_CODE_AIC;//aicbtservice error code
    userial_recv_rawdata_hook(p_buf,length);

}

static void* cmdready_thread()
{
    //Aicqueuedata* aic_data;

    while(aic_btservice->cmdqueue_thread_running)
    {
        sem_wait(&aic_btservice->cmdqueue_sem);
        sem_wait(&aic_btservice->cmdsend_sem);

        if(aic_btservice->cmdqueue_thread_running != 0)
        {
            pthread_mutex_lock(&aic_btservice->cmdqueue_mutex);
            RT_LIST_ENTRY* iter = ListGetTop(&(aic_btservice->cmdqueue_list));
            Aicqueuedata* desc = NULL;
            if (iter) {
                desc = LIST_ENTRY(iter, Aicqueuedata, list);
                if (desc)
                {
                    ListDeleteNode(&desc->list);
                }
            }

            pthread_mutex_unlock(&aic_btservice->cmdqueue_mutex);

            if(desc) {
                if(desc->opcode == HCI_CMD_VNDR_AUTOPAIR)
                {
                    aic_btservice->autopair_fd = desc->client_sock;
                }

                if(desc->opcode != HCI_CMD_VNDR_HEARTBEAT)
                    ALOGD("%s, transmit_command Opcode:%x",__func__, desc->opcode);
                aic_btservice->current_client_sock = desc->client_sock;
                aic_btservice->current_complete_cback = desc->complete_cback;
                aic_btservice->opcode = desc->opcode;
                hcicmd_start_reply_timer();
                aic_vendor_cmd_to_fw(desc->opcode, desc->parameter_len, desc->parameter, Aic_Service_Cmd_Event_Cback);
                if(desc->parameter_len > 0)
                    free(desc->parameter);
            }
            free(desc);
        }
    }
    pthread_exit(0);
}

static void parseString(int client_sock, char *msg)
{
    ALOGD("%s msg = %s", __func__, msg);
    if(!strcmp(msg, "Service Name")) {
        char buffer[7] = {'R', 'e', 'a', 'l', 't', 'e', 'k'};
        write(client_sock, buffer, 7);
    }
}

static void Getpacket(int client_sock)
{
    unsigned char type=0;
    unsigned char opcodeh=0;
    unsigned char opcodel=0;
    unsigned char parameter_length=0;
    unsigned char *parameter = NULL;
    int recvlen=0;
    Aic_Service_Data *p_buf;

    recvlen = read(client_sock, &type, 1);
    ALOGD("%s recvlen=%d,type=%d",__func__,recvlen, type);
    if(recvlen <= 0)
    {
        if(epoll_ctl(aic_btservice->epoll_fd, EPOLL_CTL_DEL, client_sock, NULL) == -1)
        {
            ALOGE("%s unable to register fd %d to epoll set: %s", __func__, client_sock, strerror(errno));
        }
        close(client_sock);
        if(client_sock == aic_btservice->autopair_fd)
        {
            aic_btservice->autopair_fd = -1;
        }
        return;
    }

    switch (type)
    {
        case AIC_HCICMD:
        {
            recvlen = read(client_sock,&opcodel,1);
            if(recvlen <= 0)
            {
                ALOGE("read opcode low char error");
                break;
            }
            recvlen = read(client_sock,&opcodeh,1);
            if(recvlen <= 0)
            {
                ALOGE("read opcode high char error");
                break;
            }
            recvlen = read(client_sock,&parameter_length,1);
            if(recvlen <= 0)
            {
                ALOGE("read parameter_length char error");
                break;
            }

            if(parameter_length > 0)
            {
                parameter = (unsigned char *)malloc(sizeof(char)*parameter_length);
                if(!parameter) {
                    ALOGE("%s parameter alloc fail!", __func__);
                    return;
                }
                recvlen = read(client_sock, parameter, parameter_length);
                ALOGD("%s parameter_length=%d,recvlen=%d",__func__,parameter_length, recvlen);
                if(recvlen <= 0 || recvlen != parameter_length)
                {
                    ALOGE("read parameter_length char error recvlen=%d,parameter_length=%d\n",recvlen,parameter_length);
                    free(parameter);
                    break;
                }
            }
            p_buf = (Aic_Service_Data *)malloc(sizeof(Aic_Service_Data));
            if (NULL == p_buf)
            {
                ALOGE("p_buf: allocate error");
                if(parameter)
                  free(parameter);
                return;
            }

            p_buf->opcode = ((unsigned short)opcodeh)<<8 | opcodel;
            p_buf->parameter = parameter;
            p_buf->parameter_len = parameter_length;
            p_buf->complete_cback = Aic_Client_Cmd_Cback;
            Aic_Service_Vendorcmd_Hook(p_buf,client_sock);
            free(p_buf);
            break;
        }

        case AIC_CLOSESOCRET:
        {
            close(client_sock);
            //pthread_exit(0);
            break;
        }

        case AIC_INNER:
        {

            break;
        }

        case AIC_STRING:
        {
            recvlen = read(client_sock, &parameter_length, 1);
            if(recvlen <= 0)
            {
                ALOGE("read data error");
                break;
            }
            char* message = (char* )malloc(parameter_length + 1);
            recvlen = read(client_sock, message, parameter_length);
            if(recvlen != parameter_length) {
                ALOGE("%s, read length is not equal to parameter_length", __func__);
                free(message);
                break;
            }
            message[parameter_length] = '\0';
            parseString(client_sock , message);
            free(message);
            break;
        }
        default:
        {
            ALOGE("%s The AicSockData type is not found!", __func__);
            break;
        }
    }

}

void aic_btservice_internal_event_intercept(uint8_t *p_full_msg, uint8_t *p_msg)
{
    uint8_t *p = p_msg;
    uint8_t event_code = *p++;
    //uint8_t len = *p++;
    uint8_t  subcode;
    HC_BT_HDR *p_evt_buf = (HC_BT_HDR *) p_full_msg;
    if(event_code == 0xff)
        ALOGD("aic_btservice_internal_event_intercept event_code=0x%x",event_code);
    switch (event_code)
    {
        case HCI_VENDOR_SPECIFIC_EVT:
        {
            STREAM_TO_UINT8(subcode, p);
            switch(subcode)
            {
                case HCI_AICBT_AUTOPAIR_EVT:
                {

                    ALOGD("p_evt_buf_len=%d",p_evt_buf->len);
                    if(aic_btservice->autopair_fd != -1)
                    {
                        write(aic_btservice->autopair_fd, p_evt_buf, p_evt_buf->len+8);
                        uint8_t p_bluedroid_len = p_evt_buf->len+1;
                        uint8_t p_bluedroid[p_bluedroid_len];
                        p_bluedroid[0] = DATA_TYPE_EVENT;
                        memcpy((uint8_t *)(p_bluedroid + 1), p_msg, p_evt_buf->len);
                        p_bluedroid[1] = 0x3e;  //event_code
                        p_bluedroid[3] = 0x02;  //subcode
                        userial_recv_rawdata_hook(p_bluedroid, p_bluedroid_len);
                    }
                }

                default:
                  break;
            }
            break;
        }
        default:
            break;
    }
}


static int aic_socket_accept(socketfd)
{
    struct sockaddr_un un;
    socklen_t len;
    int client_sock = 0;
    len = sizeof(un);
    struct epoll_event event;

    client_sock = accept(socketfd, (struct sockaddr *)&un, &len);
    if(client_sock<0)
    {
        ALOGE("accept failed\n");
        return -1;
    }
    //pthread_create(&connectthread,NULL,(void *)accept_request_thread,&client_sock);

    ALOGD("%s client socket fd: %d", __func__, client_sock);
    event.data.fd = client_sock;
    event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
    //list_add(client_sock);
    if(epoll_ctl(aic_btservice->epoll_fd, EPOLL_CTL_ADD, client_sock, &event)==-1)
    {
        ALOGE("%s unable to register fd %d to epoll set: %s", __func__, client_sock, strerror(errno));
        close(client_sock);
        return -1;
    }
    Aicqueuenode* node = (Aicqueuenode*)malloc(sizeof(Aicqueuenode));
    node->client_fd = client_sock;
    ListAddToTail(&node->list, &aic_btservice->socket_node_list);
    return 0;
}

static void *epoll_thread()
{
    struct epoll_event events[64];
    int nfds=0;
    int i=0;

    while(aic_btservice->epoll_thread_running)
    {
        nfds = epoll_wait(aic_btservice->epoll_fd,events, 32, 500);
        if(aic_btservice->epoll_thread_running != 0)
        {
            if(nfds > 0)
            {
                for(i = 0; i < nfds; i++)
                {
                    if(events[i].data.fd == aic_btservice->sig_fd[1]) {
                        ALOGE("epoll_thread , receive exit signal");
                        continue;
                    }

                    if(events[i].data.fd == aic_btservice->socketfd && events[i].events & EPOLLIN)
                    {
                        if(aic_socket_accept(events[i].data.fd) < 0)
                        {
                            pthread_exit(0);
                        }
                    }
                    else if(events[i].events & EPOLLRDHUP) {
                        if(epoll_ctl(aic_btservice->epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1)
                        {
                            ALOGE("%s unable to register fd %d to epoll set: %s", __func__, events[i].data.fd, strerror(errno));
                        }
                        RT_LIST_HEAD * Head = &(aic_btservice->socket_node_list);
                        RT_LIST_ENTRY* Iter = NULL, *Temp = NULL;
                        Aicqueuenode* desc = NULL;
                        LIST_FOR_EACH_SAFELY(Iter, Temp, Head)
                        {
                            desc = LIST_ENTRY(Iter, Aicqueuenode, list);
                            if(desc && (desc->client_fd == events[i].data.fd)) {
                              ListDeleteNode(&desc->list);
                              free(desc);
                              break;
                            }
                        }
                        close(events[i].data.fd);
                    }
                    else if(events[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR))
                    {
                        Getpacket(events[i].data.fd);
                    }
                }
            }
        }
    }
    pthread_exit(0);
}

static int unix_socket_start(const char *servername)
{
    int len;
    struct sockaddr_un un;
    struct epoll_event event;

    if ((aic_btservice->socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        ALOGE("%s create AF_UNIX socket fail!", __func__);
        aic_btservice->socketfd = -1;
        return -1;
    }

    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, servername);
    un.sun_path[0]=0;
    len = offsetof(struct sockaddr_un, sun_path) + strlen(servername);

    if (bind(aic_btservice->socketfd, (struct sockaddr *)&un, len) < 0)
    {
        ALOGE("%s bind socket fail!", __func__);
        goto fail;
    }

    if (listen(aic_btservice->socketfd, MAX_CONNECTION_NUMBER) < 0)
    {
        ALOGE("%s listen socket fail!", __func__);
        goto fail;
    }
    /*
    if(chmod(AICBTSERVICE_SOCKETPATH,0666) != 0)
    {
        ALOGE("%s chmod failed");
    }
    */
    event.data.fd = aic_btservice->socketfd;
    event.events = EPOLLIN;
    if(epoll_ctl(aic_btservice->epoll_fd, EPOLL_CTL_ADD, aic_btservice->socketfd,&event) == -1)
    {
        ALOGE("%s unable to register fd %d to epoll set: %s", __func__, aic_btservice->socketfd, strerror(errno));
        goto fail;
    }

    event.data.fd = aic_btservice->sig_fd[1];
    event.events = EPOLLIN;
    if(epoll_ctl(aic_btservice->epoll_fd, EPOLL_CTL_ADD, aic_btservice->sig_fd[1], &event) == -1)
    {
        ALOGE("%s unable to register signal fd %d to epoll set: %s", __func__, aic_btservice->sig_fd[1], strerror(errno));
        goto fail;
    }
    return 0;

fail:
    close(aic_btservice->socketfd);
    aic_btservice->socketfd = -1;
    return -1;
}

void AIC_btservice_send_close_signal(void)
{
    unsigned char close_signal = 1;
    ssize_t ret;
    AIC_NO_INTR(ret = write(aic_btservice->sig_fd[0], &close_signal, 1));
}

int AIC_btservice_thread_start()
{
    aic_btservice->epoll_thread_running=1;
    if (pthread_create(&aic_btservice->epollthd, NULL, epoll_thread, NULL)!=0)
    {
        ALOGE("pthread_create epoll_thread: %s", strerror(errno));
        return -1;
    }

    aic_btservice->cmdqueue_thread_running = 1;
    if (pthread_create(&aic_btservice->cmdreadythd, NULL, cmdready_thread, NULL)!=0)
    {
        ALOGE("pthread_create cmdready_thread: %s", strerror(errno));
        return -1;
    }

    return 0;
}

void AIC_btservice_thread_stop()
{
    pthread_mutex_lock(&aic_btservice->cmdqueue_mutex);
    aic_btservice->epoll_thread_running=0;
    aic_btservice->cmdqueue_thread_running=0;
    hcicmd_stop_reply_timer();
    pthread_mutex_unlock(&aic_btservice->cmdqueue_mutex);
    AIC_btservice_send_close_signal();
    sem_post(&aic_btservice->cmdqueue_sem);
    sem_post(&aic_btservice->cmdsend_sem);
    pthread_join(aic_btservice->cmdreadythd, NULL);
    pthread_join(aic_btservice->epollthd, NULL);
    close(aic_btservice->epoll_fd);
    //close socket fd connected before
    RT_LIST_HEAD * Head = &(aic_btservice->socket_node_list);
    RT_LIST_ENTRY* Iter = NULL, *Temp = NULL;
    Aicqueuenode* desc = NULL;
    LIST_FOR_EACH_SAFELY(Iter, Temp, Head)
    {
        desc = LIST_ENTRY(Iter, Aicqueuenode, list);
        if(desc) {
          close(desc->client_fd);
          ListDeleteNode(&desc->list);
          free(desc);
        }
    }
    ALOGD("%s end!", __func__);
}

int AIC_btservice_init()
{
    int ret;
    aic_btservice = (Aic_Btservice_Info *)malloc(sizeof(Aic_Btservice_Info));
    if(aic_btservice) {
        memset(aic_btservice, 0, sizeof(Aic_Btservice_Info));
    }
    else {
        ALOGE("%s, alloc fail", __func__);
        return -1;
    }

    aic_btservice->current_client_sock = -1;
    aic_btservice->current_complete_cback = NULL;
    aic_btservice->autopair_fd = -1;
    if(!hcicmd_alloc_reply_timer()) {
        ALOGE("%s alloc timer fail!", __func__);
        ret = -1;
        goto fail2;
    }

    sem_init(&aic_btservice->cmdqueue_sem, 0, 0);
    sem_init(&aic_btservice->cmdsend_sem, 0, 1);

    pthread_mutex_init(&aic_btservice->cmdqueue_mutex, NULL);
    init_cmdqueue_hash(aic_btservice);
    if(bt_vendor_cbacks == NULL)
    {
        ALOGE("%s bt_vendor_cbacks is NULL!", __func__);
        ret = -2;
        goto fail1;
    }

    if((ret = socketpair(AF_UNIX, SOCK_STREAM, 0, aic_btservice->sig_fd)) < 0) {
        ALOGE("%s, errno : %s", __func__, strerror(errno));
        goto fail1;
    }

    RT_LIST_HEAD* head = &aic_btservice->socket_node_list;
    ListInitializeHeader(head);

    aic_btservice->epoll_fd = epoll_create(64);
    if (aic_btservice->epoll_fd == -1) {
        ALOGE("%s unable to create epoll instance: %s", __func__, strerror(errno));
        ret = -3;
        close(aic_btservice->sig_fd[0]);
        close(aic_btservice->sig_fd[1]);
        goto fail1;
    }

    if(unix_socket_start(AICBTSERVICE_SOCKETPATH) < 0)
    {
        ALOGE("%s unix_socket_start fail!", __func__);
        ret = -4;
        close(aic_btservice->epoll_fd);
        close(aic_btservice->sig_fd[0]);
        close(aic_btservice->sig_fd[1]);
        goto fail1;
    }

    ret = AIC_btservice_thread_start();
    if(ret < 0)
    {
        ALOGE("%s AIC_btservice_thread_start fail!", __func__);
        goto fail0;
    }
    ALOGD("%s init done!", __func__);

    return 0;

fail0:
    close(aic_btservice->epoll_fd);
    close(aic_btservice->sig_fd[0]);
    close(aic_btservice->sig_fd[1]);
    close(aic_btservice->socketfd);
    aic_btservice->socketfd = -1;
fail1:
    sem_destroy(&aic_btservice->cmdqueue_sem);
    sem_destroy(&aic_btservice->cmdsend_sem);
    flush_cmdqueue_hash(aic_btservice);
    hcicmd_free_reply_timer();
    pthread_mutex_destroy(&aic_btservice->cmdqueue_mutex);

fail2:
    free(aic_btservice);
    aic_btservice = NULL;
    return ret;
}

void AIC_btservice_destroyed()
{
    if(!aic_btservice)
        return;
    AIC_btservice_thread_stop();
    close(aic_btservice->socketfd);
    aic_btservice->socketfd = -1;
    close(aic_btservice->sig_fd[0]);
    close(aic_btservice->sig_fd[1]);
    sem_destroy(&aic_btservice->cmdqueue_sem);
    sem_destroy(&aic_btservice->cmdsend_sem);
    flush_cmdqueue_hash(aic_btservice);
    hcicmd_free_reply_timer();
    pthread_mutex_destroy(&aic_btservice->cmdqueue_mutex);
    aic_btservice->autopair_fd = -1;
    aic_btservice->current_client_sock = -1;
    free(aic_btservice);
    aic_btservice = NULL;
    ALOGD("%s destroyed done!", __func__);
}


