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
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stddef.h>
#include <signal.h>
#include <unistd.h>

#define UNIX_DOMAIN "@/data/misc/bluedroid/aicbt_service.sock"

typedef struct Aic_Socket_Data
{
    unsigned char           type;      //hci,other,inner
    unsigned char           opcodel;
    unsigned char           opcodeh;
    unsigned char           parameter_len;
    unsigned char           parameter[0];
}Aic_Socket_Data;

/*typedef struct
{
    unsigned short          event;
    unsigned short          len;
    unsigned short          offset;
    unsigned short          layer_specific;
    unsigned char           data[];
} HC_BT_HDR;
*/
const char shortOptions[] = "f:r:h";
const struct option longOptions[] = {
    {"fullhcicmd",  required_argument,      NULL,   'f'},
    {"read",        required_argument,      NULL,   'r'},
    {"help",        no_argument,            NULL,   'h'},
    {0, 0, 0, 0}
};

static void usage(void)
{
    fprintf(stderr, "Usage: aiccmd [options]\n\n"
         "Options:\n"
         "-f | --fullhcicmd [opcode,parameter_len,parameter]    send hci cmd\n"
         "-r | --read       [address]                           read register address \n"
         "-h | --help                                           Print this message\n\n");
}

int Aicbt_Sendcmd(int socketfd,char *p)
{
    char *token = NULL;
    int i=0;
    unsigned short OpCode = 0;
    unsigned char ParamLen = 0;
    unsigned char ParamLen_1 = 0;
    int sendlen = 0;
    int params_count=0;
    int ret=0;
    Aic_Socket_Data *p_buf = NULL;

    token = strtok(p,",");
    if (token != NULL) {
        OpCode = strtol(token, NULL, 0);
        //printf("OpCode = %x\n",OpCode);
        params_count++;
    } else {
        //ret = FUNCTION_PARAMETER_ERROR;
        printf("parameter error\n");
        return -1;
    }

    token = strtok(NULL, ",");
    if (token != NULL) {
        ParamLen = strtol(token, NULL, 0);
        //printf("ParamLen = %d\n",ParamLen);
        params_count++;
    } else {
        printf("parameter error\n");
        return -1;
    }

    p_buf=(Aic_Socket_Data *)malloc(sizeof(Aic_Socket_Data) + sizeof(char)*ParamLen);
    p_buf->type = 0x01;
    p_buf->opcodeh = OpCode>>8;
    p_buf->opcodel = OpCode&0xff;
    p_buf->parameter_len = ParamLen;

    ParamLen_1 = ParamLen;
    while (ParamLen_1--) {
        token = strtok(NULL, ",");
        if (token != NULL) {
            p_buf->parameter[i++] = strtol(token, NULL, 0);
            params_count++;
        } else {
            printf("parameter error\n");
            return -1;
        }
    }

    if (params_count != ParamLen + 2) {
        printf("parameter error\n");
        return -1;
    }

    sendlen=sizeof(Aic_Socket_Data)+sizeof(char)*p_buf->parameter_len;
    ret=write(socketfd,p_buf,sendlen);
    if(ret!=sendlen)
        return -1;

    free(p_buf);
    return 0;
}

int Aicbt_Getevent(int sock_fd)
{
    unsigned char type = 0;
    unsigned char event = 0;
    unsigned char len = 0;
    unsigned char *recvbuf = NULL;
    unsigned char tot_len = 0;
    int ret=0;
    int i;

    ret = read(sock_fd, &type, 1);
    if(ret <= 0)
        return -1;

    ret = read(sock_fd, &event, 1);
    if(ret <= 0)
        return -1;

    ret = read(sock_fd, &len, 1);
    if(ret <= 0)
        return -1;

    tot_len = len + 2;
    recvbuf=(unsigned char *)malloc(sizeof(char)*tot_len);
    recvbuf[0] = event;
    recvbuf[1] = len;
    ret = read(sock_fd,recvbuf+2,len);
    if(ret < len)
        return -1;

    printf("Event: ");
    for(i=0;i<tot_len-1;i++)
        printf("0x%02x ",recvbuf[i]);
    printf("0x%02x\n",recvbuf[tot_len-1]);

    free(recvbuf);
    return 0;
}

int socketinit()
{
    int sock_fd;
    struct sockaddr_un un;
    int len;
    memset(&un, 0, sizeof(un));            /* fill socket address structure with our address */
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, UNIX_DOMAIN);
    un.sun_path[0]=0;
    len = offsetof(struct sockaddr_un, sun_path) + strlen(UNIX_DOMAIN);

    sock_fd= socket(AF_UNIX, SOCK_STREAM, 0);
    if(sock_fd<0)
    {
        printf("socket failed %s\n",strerror(errno));
        return -1;
    }
    if(connect(sock_fd,(struct sockaddr *)&un, len)<0)
    {
        printf("connect failed %s\n",strerror(errno));
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

int main(int argc , char* argv[])
{
    int index;
    int c;
    int ret;
    int socketfd;

    socketfd = socketinit();
    if(socketfd<0)
    {
        printf("socketinit failed\n");
        exit(0);
    }

    c = getopt_long(argc, argv, shortOptions, longOptions, &index);

    if(c==-1)
    {
        usage();
    }
    else
    {
        switch(c)
        {
            case 'f':
            {
                printf("Hcicmd %s\n",optarg);
                ret = Aicbt_Sendcmd(socketfd,optarg);
                if(ret>=0)
                {
                    if(Aicbt_Getevent(socketfd)<0)
                        printf("Getevent fail\n");
                }
                break;
            }
            case 'r':
            {
                printf("read register %s\n",optarg);
                //Aicbt_Readreg(socketfd,optarg);
                break;
            }
            case 'h':
            {
                usage();
                break;
            }
        }
    }

    close(socketfd);

}
