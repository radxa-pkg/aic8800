#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "stdint.h"

//tool role
#define SERVER 0
#define CLIENT 1
#define WLAN_CLIENT 2

//interface
#define UART    0
#define USB     1

//uart param
#define BAUDRATE 1500000
#define DATABITS 8
#define PARTY 'N'
#define STOP '1'
#define SOFTWARE_HANDSHAKE 0
#define HARDWARE_HANDSHAKE 1

//socket param
#define BT_HCI_TOOL_PORT 5001
#define SOCKER_BUFFER_SIZE 1024

#define OK 0
#define ERR -1
#define FALSE 0
#define TRUE 1

#define DOWN_FW_CFG             _IOW('E', 176, int)
#define GET_USB_INFO            _IOR('E', 180, int)

#define WLAN_CMD_MAX_CNT        256
#define TXRX_PARA								SIOCDEVPRIVATE+1

int server_or_client = SERVER;
int interface = USB;
int bt_dev_fd = 0;

char s_baudrate[8];

char* s_hci_command[256];
char w_ifname[64];
char *w_command[WLAN_CMD_MAX_CNT];
char hci_event[256];

char dev_path[] = "/dev/aicbt_dev";

char hci_reset[] = { 0x01, 0x03, 0x0c, 0x00 };
char hci_set_evt_mask[]={0x01, 0x01, 0x0c, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0x00, 0x20};
char hci_set_le_evt_mask[]={0x01, 0x01, 0x20, 0x08, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00};
char hci_set_vendor_cmd[]={0x01,0x01,0xfc,0x06,0x00,0x00,0x50,0x40,0x20,0x04};
char hci_set_scan_param[]={0x01,0x41,0x20,0x08,0x00,0x00,0x01,0x01,0xa0,0x00,0x30,0x00};
char hci_set_scan_enable[]={0x01,0x42,0x20,0x06,0x01,0x00,0x00,0x00,0x00,0x00};
char hci_set_scan_disable[]={0x01,0x42,0x20,0x06,0x00,0x00,0x00,0x00,0x00,0x00};
char hci_set_vendor_cmd_fc72[]={0x01,0x72,0xfc,0x24,0x20,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

typedef struct android_wifi_priv_cmd {
    char *buf;
    int used_len;
    int total_len;
} android_wifi_priv_cmd;

struct bd_addr{
    uint8_t addr[6];
};

///Advertising report structure
/*@TRACE*/
struct adv_report
{
    ///Event type:
    /// - ADV_CONN_UNDIR: Connectable Undirected advertising
    /// - ADV_CONN_DIR: Connectable directed advertising
    /// - ADV_DISC_UNDIR: Discoverable undirected advertising
    /// - ADV_NONCONN_UNDIR: Non-connectable undirected advertising
    uint8_t        evt_type;
    ///Advertising address type: public/random
    uint8_t        adv_addr_type;
    ///Advertising address value
    struct bd_addr adv_addr;
    ///Data length in advertising packet
    uint8_t        data_len;
    ///Data of advertising packet
    uint8_t        data[31];
    ///RSSI value for advertising packet (in dBm, between -127 and +20 dBm)
    int8_t         rssi;
};


///Exteneded Advertising report structure
/*@TRACE*/
struct ext_adv_report
{
    ///Event type
    uint16_t       evt_type;
    ///Advertising address type: public/random
    uint8_t        adv_addr_type;
    ///Advertising address value
    struct bd_addr adv_addr;
    ///Primary PHY
    uint8_t        phy;
    ///Secondary PHY
    uint8_t        phy2;
    ///Advertising SID
    uint8_t        adv_sid;
    ///Tx Power
    uint8_t        tx_power;
    ///RSSI value for advertising packet (in dBm, between -127 and +20 dBm)
    int8_t         rssi;
    ///Periodic Advertising interval (Time=N*1.25ms)
    uint16_t       interval;
    ///Direct address type
    uint8_t        dir_addr_type;
    ///Direct address value
    struct bd_addr dir_addr;
    ///Data length in advertising packet
    uint8_t        data_len;
    ///Data of advertising packet
    uint8_t        data[229];
};

void helper(){
    printf("\t<-s> to be tool service. ex. \"bt_test -s uart 115200 /dev/ttyS0\" or \"bt_test -s usb\" \n");
    //printf("\t<-s> to be tool service. ex. -s uart 115200 noflow and -s usb\n");
    printf("\t<-c> to send hci cmd to interface.\n");
    printf("\t<-w> to send wlan cmd to interface.\n");
}

int parse_cmd_line(int argc, char **argv){
    int command_number = argc;
    int command_counter = 1;
    int hci_command_counter = 0;
    //int error_command = 0;

    while(command_counter < command_number){
        if(!strcmp(argv[command_counter], "-s")){
            if(!(command_number >= 3)){
                printf("error command \r\n");
                return ERR;
            }
            printf("bt_hci_tool server setting\r\n");
            server_or_client = SERVER;
            command_counter++;
            if(!strcmp(argv[command_counter], "usb")){
                printf("use USB\r\n");
                interface = USB;
                return OK;
            }else if(!strcmp(argv[command_counter], "uart")){
                interface = UART;
                if(command_number != 5){
                    printf("error command \r\n");
                    return ERR;
                }
                command_counter++;
                memcpy(s_baudrate, argv[command_counter], strlen(argv[command_counter]));
                command_counter++;
                memset(dev_path, 0, strlen(dev_path));
                memcpy(dev_path, argv[command_counter], strlen(argv[command_counter]));
                printf("use UART %d %s\r\n", atoi(s_baudrate), dev_path);
                return OK;
            }else{
                printf("interface set error\r\n");
                return ERR;
            }
        }else if(!strcmp(argv[command_counter], "-c")){
            server_or_client = CLIENT;
            command_counter++;
            hci_command_counter = 0;
            while(command_counter < command_number){
                s_hci_command[hci_command_counter++] = argv[command_counter];
                command_counter++;
            }
            return OK;
        }else{
            if (strcmp(argv[1], "-h") != 0) {
                printf("unknow command %s \r\n", argv[command_counter]);
            }
            helper();
            return ERR;
        }

        command_counter++;
    }

    helper();
    return ERR;

}

void uart_set_options(int baudrate, int databits,
    char parity, char stop, int softwareHandshake, int hardwareHandshake)
{
    struct termios newtio;
    if (tcgetattr(bt_dev_fd, &newtio)!=0)
    {
    printf("tcgetattr() 3 failed \n");
    }

    speed_t _baud=0;
    printf("set baudrate:%d\n", baudrate);

    switch (baudrate)
    {
    #ifdef B0
    case      0: _baud=B0;     break;
    #endif
    #ifdef B50
    case     50: _baud=B50;    break;
    #endif
    #ifdef B75
    case     75: _baud=B75;    break;
    #endif
    #ifdef B110
    case    110: _baud=B110;   break;
    #endif
    #ifdef B134
    case    134: _baud=B134;   break;
    #endif
    #ifdef B150
    case    150: _baud=B150;   break;
    #endif
    #ifdef B200
    case    200: _baud=B200;   break;
    #endif
    #ifdef B300
    case    300: _baud=B300;   break;
    #endif
    #ifdef B600
    case    600: _baud=B600;   break;
    #endif
    #ifdef B1200
    case   1200: _baud=B1200;  break;
    #endif
    #ifdef B1800
    case   1800: _baud=B1800;  break;
    #endif
    #ifdef B2400
    case   2400: _baud=B2400;  break;
    #endif
    #ifdef B4800
    case   4800: _baud=B4800;  break;
    #endif
    #ifdef B7200
    case   7200: _baud=B7200;  break;
    #endif
    #ifdef B9600
    case   9600: _baud=B9600;  break;
    #endif
    #ifdef B14400
    case  14400: _baud=B14400; break;
    #endif
    #ifdef B19200
    case  19200: _baud=B19200; break;
    #endif
    #ifdef B28800
    case  28800: _baud=B28800; break;
    #endif
    #ifdef B38400
    case  38400: _baud=B38400; break;
    #endif
    #ifdef B57600
    case  57600: _baud=B57600; break;
    #endif
    #ifdef B76800
    case  76800: _baud=B76800; break;
    #endif
    #ifdef B115200
    case 115200: _baud=B115200; break;
    #endif
    #ifdef B128000
    case 128000: _baud=B128000; break;
    #endif
    #ifdef B230400
    case 230400: _baud=B230400; break;
    #endif
    #ifdef B460800
    case 460800: _baud=B460800; break;
    #endif
    #ifdef B576000
    case 576000: _baud=B576000; break;
    #endif
    #ifdef B921600
    case 921600: _baud=B921600; break;
    #endif
    #ifdef B1500000
    case 1500000: _baud=B1500000; break;
    #endif
    #ifdef B2000000
    case 2000000: _baud=B2000000; break;
    #endif

    default:
        break;
    }
    cfsetospeed(&newtio, (speed_t)_baud);
    cfsetispeed(&newtio, (speed_t)_baud);

    /* We generate mark and space parity ourself. */
    if (databits == 7 && (parity=='M' || parity == 'S'))
    {
        databits = 8;
    }
    switch (databits)
    {
    case 5:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS5;
        break;
    case 6:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS6;
        break;
    case 7:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS7;
        break;
    case 8:
    default:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS8;
        break;
    }
    newtio.c_cflag |= CLOCAL | CREAD;

    //parity
    newtio.c_cflag &= ~(PARENB | PARODD);
    if (parity == 'E')
    {
        newtio.c_cflag |= PARENB;
    }
    else if (parity== 'O')
    {
        newtio.c_cflag |= (PARENB | PARODD);
    }

    //hardware handshake
    newtio.c_cflag &= ~CRTSCTS;

    //stopbits
    if (stop=='2')
    {
        newtio.c_cflag |= CSTOPB;
    }
    else
    {
        newtio.c_cflag &= ~CSTOPB;
    }

    newtio.c_iflag=IGNBRK;

    //software handshake
    if (softwareHandshake)
    {
        newtio.c_iflag |= IXON | IXOFF;
    }
    else
    {
        newtio.c_iflag &= ~(IXON|IXOFF|IXANY);
    }

    newtio.c_lflag=0;
    newtio.c_oflag=0;

    //   newtio.c_cc[VTIME]=1;
    //   newtio.c_cc[VMIN]=60;

    if (tcsetattr(bt_dev_fd, TCSANOW, &newtio)!=0)
    {
        printf("tcsetattr() 1 failed \n");
    }

    if (tcgetattr(bt_dev_fd, &newtio)!=0)
    {
        printf("tcsetattr() 4 failed \n");
    }

    //hardware handshake
    if (hardwareHandshake)
    {
        newtio.c_cflag |= CRTSCTS;
    }
    else
    {
        newtio.c_cflag &= ~CRTSCTS;
    }

    if (tcsetattr(bt_dev_fd, TCSANOW, &newtio)!=0)
    {
        printf("tcsetattr() 2 failed \n");
    }
}


unsigned char atoh(char* s_data){
    unsigned char ret_value;
    char temp_value = 0;

    //h
    if(s_data[0] >= 48 && s_data[0] <= 57){
        temp_value = s_data[0] - 48;
    }else if(s_data[0] >= 65 && s_data[0] <= 70){
        temp_value = (s_data[0] - 65) + 10;
    }else if(s_data[0] >= 97 && s_data[0] <= 102){
        temp_value = (s_data[0] - 97) + 10;
    }

    ret_value = temp_value;
    ret_value = ret_value << 4;
    //printf("atoh1: ret_value:%x\r\n", ret_value);
    //l
    if(s_data[1] >= 48 && s_data[1] <= 57){
        temp_value = s_data[1] - 48;
    }else if(s_data[1] >= 65 && s_data[1] <= 70){
        temp_value = (s_data[1] - 65) + 10;
    }else if(s_data[1] >= 97 && s_data[1] <= 102){
        temp_value = (s_data[1] - 97) + 10;
    }

    ret_value |= temp_value;
    //printf("atoh2: ret_value:%x\r\n", ret_value);

    return ret_value;

}

int send_to_server(unsigned char* buffer, int len){
    int client_socket;
    struct sockaddr_in serverAddr;
    //unsigned long long package_totel_counter = 0;

    if((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return ERR;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(BT_HCI_TOOL_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(client_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
        return ERR;
    }

    send(client_socket, buffer, len, 0);
    return OK;
}

void send_hci_command(){
    int i = 0;
    int command_len = 0;
    unsigned char buffer[SOCKER_BUFFER_SIZE];

//    printf("send hci command: \r\n");

    for(i = 0;i < 256; i++){
        if(s_hci_command[i] == NULL){
            command_len = i;
            break;
        }

        buffer[i] = atoh(s_hci_command[i]);
//        printf("%02X ", buffer[i]);
    }

    send_to_server(buffer, i);

//    printf("\r\n");
}


int hci_send(char* data, int len){
    int i = 0;
    unsigned char tmp_data[len];

    printf("COMMAND(%d): \r\n", len);
    for(i = 0; i < len; i++){
        tmp_data[i] = (unsigned char)data[i];
        printf("%02X ", tmp_data[i]);
    }
    printf("\r\n");

    len = write(bt_dev_fd, tmp_data, len);
    if (len < 0) {
        printf("write data error \n");
        return FALSE;
    }
    return TRUE;
}

int hci_recv_nop(char* data, int len){
    int i = 0;
    unsigned char tmp_byte = 0x00;

    memset(data, 0, len);

    len = read(bt_dev_fd, data, len);
    if (len < 0) {
        printf("read error \n");
        return FALSE;
    }
    //printf("EVENT(%d): \r\n", len);
    for(i = 0; i < len; i++){
        tmp_byte = (unsigned char)data[i];
        //printf("%02X ", tmp_byte);
    }
    //printf("\r\n");

    return len;
}

int hci_recv(char* data, int len){
    int i = 0;
    unsigned char tmp_byte = 0x00;

    memset(data, 0, len);

    len = read(bt_dev_fd, data, len);
    if (len < 0) {
        printf("read error \n");
        return FALSE;
    }
    printf("EVENT(%d): \r\n", len);
    for(i = 0; i < len; i++){
        tmp_byte = (unsigned char)data[i];
        printf("%02X ", tmp_byte);
    }
    printf("\r\n");

    return len;
}


int dev_open(){
    int n = 0;

    bt_dev_fd = open(dev_path, O_RDWR | O_NDELAY);
    if(bt_dev_fd < 0) {
        printf("dev_path %s fail to open \r\n", dev_path);
        return ERR;
    }
    if(interface == UART){
        tcflush(bt_dev_fd, TCIOFLUSH);
        n = fcntl(bt_dev_fd, F_GETFL, 0);
        fcntl(bt_dev_fd, F_SETFL, n & ~O_NDELAY);
        uart_set_options(/*BAUDRATE*/atoi(s_baudrate), DATABITS, PARTY, STOP, SOFTWARE_HANDSHAKE, HARDWARE_HANDSHAKE);
    }else{
        ioctl(bt_dev_fd, DOWN_FW_CFG);
        ioctl(bt_dev_fd, GET_USB_INFO);
    }
    printf("bt dev open successful\r\n");
    return OK;
}

void dev_close(){
    close(bt_dev_fd);
}


int socket_init(){
    int client;
    int server_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_in clientAddr;
    int addr_len = sizeof(clientAddr);
    char buffer[SOCKER_BUFFER_SIZE];
    int recv_len = 0;

    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return ERR;
    }

    //bzero(&server_addr, sizeof(server_addr));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BT_HCI_TOOL_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //setsockopt(server_socket ,SOL_SOCKET ,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(BOOL));

    if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        printf("binding error\r\n");
        return ERR;
    }

    if(listen(server_socket, 5) < 0){
        printf("listen error");
        return ERR;
    }

    while(1){
        client = accept(server_socket, (struct sockaddr*)&clientAddr, (socklen_t*)&addr_len);
        if(client == ERR){
            return 0;
        }
        recv_len = recv(client, buffer, SOCKER_BUFFER_SIZE, 0);
        if ((interface == USB) || (interface == UART)) {
            hci_send(buffer, recv_len);
        } 
    }
}


void* hci_recv_thread( void *arg ){
    printf("hci recv thread ready %p\r\n", arg);
    while(1){
        unsigned char len;
        hci_recv_nop(hci_event, sizeof(hci_event));
        if(hci_event[0]==0x04&&hci_event[1]==0x3e){
            len = hci_event[2];
            switch(hci_event[3]){
                case 0x02://Subevent_Code: HCI_LE_Advertising_Report
                    {
                        uint8_t num_of_report;
                        struct adv_report report;
                        num_of_report = hci_event[4];
                        report.evt_type = hci_event[5];
                        report.adv_addr_type = hci_event[7];
                        report.adv_addr.addr[0] = hci_event[7];
                        report.adv_addr.addr[1] = hci_event[8];
                        report.adv_addr.addr[2] = hci_event[9];
                        report.adv_addr.addr[3] = hci_event[10];
                        report.adv_addr.addr[4] = hci_event[11];
                        report.adv_addr.addr[5] = hci_event[12];
                        report.data_len = hci_event[13];
                        memcpy(report.data,&hci_event[14],report.data_len);
                        report.rssi = hci_event[14+report.data_len];
                        printf("HCI_LE_Advertising_Report\r\n");
                    }
                    break;
                case 0x0d://Subevent_Code: HCI_LE_Extended_Advertising_Report
                    {
                        uint8_t num_of_report;
                        struct ext_adv_report report;
                        num_of_report = hci_event[4];
                        report.evt_type = (uint16_t)hci_event[5]|(uint16_t)hci_event[6]<<8;
                        report.adv_addr_type = hci_event[7];
                        report.adv_addr.addr[0] = hci_event[8];
                        report.adv_addr.addr[1] = hci_event[9];
                        report.adv_addr.addr[2] = hci_event[10];
                        report.adv_addr.addr[3] = hci_event[11];
                        report.adv_addr.addr[4] = hci_event[12];
                        report.adv_addr.addr[5] = hci_event[13];
                        report.phy = hci_event[14];
                        report.phy2 = hci_event[15];
                        report.adv_sid = hci_event[16];
                        report.tx_power = hci_event[17];
                        report.rssi = hci_event[18];
                        report.interval = (uint16_t)hci_event[19]|(uint16_t)hci_event[20]<<8;
                        report.dir_addr_type = hci_event[21];
                        report.dir_addr.addr[0] = hci_event[22];
                        report.dir_addr.addr[1] = hci_event[23];
                        report.dir_addr.addr[2] = hci_event[24];
                        report.dir_addr.addr[3] = hci_event[25];
                        report.dir_addr.addr[4] = hci_event[26];
                        report.dir_addr.addr[5] = hci_event[27];
                        report.data_len = hci_event[28];
                        memcpy(report.data,&hci_event[29],report.data_len);
                        printf("HCI_LE_Extended_Advertising_Report\r\n");
                    }
                    break;
            }
        }

    }
}

void start_server(){
    int ret = ERR;
    if ((interface == USB) || (interface == UART)) {
        pthread_t hci_recv_th;
        //init bt communication
        ret = dev_open();
        if(ret == ERR){
            printf("dev_open fail \r\n");
            return;
        }
        //test to send reset
        printf("to test hci reset for bt device\r\n");
        hci_send(hci_reset, sizeof(hci_reset));
        hci_recv(hci_event, sizeof(hci_event));
        hci_send(hci_set_evt_mask, sizeof(hci_set_evt_mask));
        hci_recv(hci_event, sizeof(hci_event));
        hci_send(hci_set_le_evt_mask, sizeof(hci_set_le_evt_mask));
        hci_recv(hci_event, sizeof(hci_event));
        hci_send(hci_set_vendor_cmd, sizeof(hci_set_vendor_cmd));
        hci_recv(hci_event, sizeof(hci_event));
        hci_send(hci_set_vendor_cmd_fc72, sizeof(hci_set_vendor_cmd_fc72));
        hci_recv(hci_event, sizeof(hci_event));
        hci_send(hci_set_scan_param, sizeof(hci_set_scan_param));
        hci_recv(hci_event, sizeof(hci_event));
        hci_send(hci_set_scan_enable, sizeof(hci_set_scan_enable));
        hci_recv(hci_event, sizeof(hci_event));
        ret = pthread_create( &hci_recv_th, NULL, hci_recv_thread, NULL);
        printf("bt device test ok \r\n");
    } else{
        printf("bt device init error \r\n");
    }

        //init localhost socket
        ret = socket_init();
        if (!ret) {
            printf("socket init done\r\n");
        }
}

int main(int argc, char *argv[]){
    int ret = parse_cmd_line(argc, argv);
    if (ret) {
        return ret;
    }
    if(server_or_client == SERVER){
        start_server();
    } else if (server_or_client == CLIENT) {
        send_hci_command();
    } else {
        printf("example stack role error\r\n");
    }
    return 0;
}
