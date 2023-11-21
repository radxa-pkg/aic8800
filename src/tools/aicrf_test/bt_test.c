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

//tool role
#define SERVER 0
#define CLIENT 1
#define CLIENT_DIRECT 2
#define WLAN_CLIENT 3

//interface
#define UART    0
#define USB     1
#define WLAN    2

//uart param
#define BAUDRATE 1500000
#define DATABITS 8
#define PARTY 'N'
#define STOP '1'
#define SOFTWARE_HANDSHAKE 0
#define HARDWARE_HANDSHAKE 0

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

#define ARG_IS(_str) (!strcmp(argv[command_counter], _str))

int server_or_client = SERVER;
int interface = USB;
int bt_dev_fd = 0;

char s_baudrate[8];

char* s_hci_command[256];
uint8_t r_hci_command[256];
int r_hci_command_len;
char w_ifname[64];
char *w_command[WLAN_CMD_MAX_CNT];
char hci_event[256];

char dev_path[] = "/dev/aicbt_dev";

char hci_reset[] = { 0x01, 0x03, 0x0c, 0x00 };


typedef struct android_wifi_priv_cmd {
    char *buf;
    int used_len;
    int total_len;
} android_wifi_priv_cmd;

enum{
    maxLth_DH1=27,
    maxLth_DH3=138,
    maxLth_DH5=339,
    maxLth_2DH1=54,
    maxLth_2DH3=367,
    maxLth_2DH5=679,
    maxLth_3DH1=83,
    maxLth_3DH3=552,
    maxLth_3DH5=1021,
};

enum{
    txstop,
    rxstop,
    
    txDH1,
    txDH3,
    txDH5,
    tx2DH1,
    tx2DH3,
    tx2DH5,
    tx3DH1,
    tx3DH3,
    tx3DH5,
    rxDH1,
    rxDH3,
    rxDH5,
    rx2DH1,
    rx2DH3,
    rx2DH5,
    rx3DH1,
    rx3DH3,
    rx3DH5,

    scpc_start,
    scpc_stop,
};

struct bt_test_args_t{
    int type;
    int pattern;
    int hopping;
    int channel;
    int is_edr;
    int whitening;
    int tx_pwr;
    int len;
    uint8_t addr[6];
}bt_test_args={
    .whitening = 0x01,
    .addr = { 0x7E, 0x96, 0xC6, 0x6B, 0x1C, 0x0A },
};

void helper(){
    printf("\t<-s> to be tool service. ex. \"bt_test -s uart 115200 /dev/ttyS0\" or \"bt_test -s usb\" or \"bt_test -s wlan wlan0\"\n");
    //printf("\t<-s> to be tool service. ex. -s uart 115200 noflow and -s usb\n");
    printf("\t<-c> to send hci cmd to interface.\n");
    printf("\t<-w> to send wlan cmd to interface.\n");
    printf("\t<-H> to send hci cmd to interface with human readable options.\n");
    printf("\t     <command> [<args>]\n");
    printf("\t     Tx test commands:\n");
    printf("\t       txDH1 | txDH3 | txDH5 | tx2DH1 | tx2DH3 | tx2DH5 | tx3DH1 | tx3DH3 | tx3DH5\n");
    printf("\t     Rx test commands:\n");
    printf("\t       rxDH1 | rxDH3 | rxDH5 | rx2DH1 | rx2DH3 | rx2DH5 | rx3DH1 | rx3DH3 | rx3DH5\n");
    printf("\t     Args:\n");
    printf("\t       [patt <pattern>]\n");
    printf("\t       [en_hop | dis_hop]\n");
    printf("\t       [chnl <decimal channel>]\n");
    printf("\t       [en_wht | dis_wht]\n");
    printf("\t       [len <decimal packet length>]\n");
    printf("\t       [addr <hex MAC address>]\n");
    printf("\t     Single carrier test command:\n");
    printf("\t       scpc (start | stop)\n");
    printf("\t     Args:\n");
    printf("\t       [chnl <decimal channel>]\n");
    printf("\t       [tx_pwr <decimal tx power level>]\n");
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
            }else if(!strcmp(argv[command_counter], "wlan")){
                interface = WLAN;
                if(command_number != 4){
                    printf("error command \r\n");
                    return ERR;
                }
                command_counter++;
                memcpy(w_ifname, argv[command_counter], strlen(argv[command_counter]));
                printf("use WLAN %s\r\n", w_ifname);
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
        } else if (!strcmp(argv[command_counter], "-w")) {
            int w_command_counter = 0;
            server_or_client = WLAN_CLIENT;
            if (command_number >= WLAN_CMD_MAX_CNT) {
                printf("too much args\r\n");
                return ERR;
            }
            command_counter++;
            while (command_counter < command_number) {
                w_command[w_command_counter++] = argv[command_counter];
                command_counter++;
            }
            return OK;
        }else if (ARG_IS("-m")){
            // reserved for a variety of chip models
        } else if (ARG_IS("-H")){
            server_or_client = CLIENT_DIRECT;
            command_counter++;
            hci_command_counter = 0;
            while(command_counter < command_number){
                if (ARG_IS("txstop")){
                    bt_test_args.type = txstop;
                    command_counter++;
                    break;
                } else if (ARG_IS("txDH1")){
                    bt_test_args.type = txDH1;
                    bt_test_args.len = maxLth_DH1;
                } else if (ARG_IS("txDH3")){
                    bt_test_args.type = txDH3;
                    bt_test_args.len = maxLth_DH3;
                } else if (ARG_IS("txDH5")){
                    bt_test_args.type = txDH5;
                    bt_test_args.len = maxLth_DH5;
                } else if (ARG_IS("tx2DH1")){
                    bt_test_args.type = tx2DH1;
                    bt_test_args.len = maxLth_2DH1;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("tx2DH3")){
                    bt_test_args.type = tx2DH3;
                    bt_test_args.len = maxLth_2DH3;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("tx2DH5")){
                    bt_test_args.type = tx2DH5;
                    bt_test_args.len = maxLth_2DH5;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("tx3DH1")){
                    bt_test_args.type = tx3DH1;
                    bt_test_args.len = maxLth_3DH1;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("tx3DH3")){
                    bt_test_args.type = tx3DH3;
                    bt_test_args.len = maxLth_3DH3;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("tx3DH5")){
                    bt_test_args.type = tx3DH5;
                    bt_test_args.len = maxLth_3DH5;
                    bt_test_args.is_edr = TRUE;
                }
                else
                if (ARG_IS("rxstop")){
                    bt_test_args.type = rxstop;
                    command_counter++;
                    break;
                } else if (ARG_IS("rxDH1")){
                    bt_test_args.type = rxDH1;
                    bt_test_args.len = maxLth_DH1;
                } else if (ARG_IS("rxDH3")){
                    bt_test_args.type = rxDH3;
                    bt_test_args.len = maxLth_DH3;
                } else if (ARG_IS("rxDH5")){
                    bt_test_args.type = rxDH5;
                    bt_test_args.len = maxLth_DH5;
                } else if (ARG_IS("rx2DH1")){
                    bt_test_args.type = rx2DH1;
                    bt_test_args.len = maxLth_2DH1;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("rx2DH3")){
                    bt_test_args.type = rx2DH3;
                    bt_test_args.len = maxLth_2DH3;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("rx2DH5")){
                    bt_test_args.type = rx2DH5;
                    bt_test_args.len = maxLth_2DH5;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("rx3DH1")){
                    bt_test_args.type = rx3DH1;
                    bt_test_args.len = maxLth_3DH1;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("rx3DH3")){
                    bt_test_args.type = rx3DH3;
                    bt_test_args.len = maxLth_3DH3;
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("rx3DH5")){
                    bt_test_args.type = rx3DH5;
                    bt_test_args.len = maxLth_3DH5;
                    bt_test_args.is_edr = TRUE;
                }
                else
                if (ARG_IS("scpc")){
                    if (command_counter+1>=command_number)
                    {
                        return ERR;
                    }
                    command_counter++;

                    if (ARG_IS("start")){
                        bt_test_args.type  = scpc_start;
                    } else if (ARG_IS("stop")){
                        bt_test_args.type = scpc_stop;
                    }
                }
                else
                if (ARG_IS("patt")){
                    if (command_counter+1>=command_number)
                    {
                        return ERR;
                    }
                    command_counter++;
                    if (ARG_IS("PRBS9") || ARG_IS("00")){
                        bt_test_args.pattern = 0x00;
                    } else if (ARG_IS("11110000") || ARG_IS("01")){
                        bt_test_args.pattern = 0x01;
                    } else if (ARG_IS("10101010") || ARG_IS("02")){
                        bt_test_args.pattern = 0x02;
                    } else if (ARG_IS("PRBS15") || ARG_IS("03")){
                        bt_test_args.pattern = 0x03;
                    } else if (ARG_IS("11111111") || ARG_IS("04")){
                        bt_test_args.pattern = 0x04;
                    } else if (ARG_IS("00000000") || ARG_IS("05")){
                        bt_test_args.pattern = 0x05;
                    } else if (ARG_IS("00001111") || ARG_IS("06")){
                        bt_test_args.pattern = 0x06;
                    } else if (ARG_IS("01010101") || ARG_IS("07")){
                        bt_test_args.pattern = 0x07;
                    } 
                }
                else
                if (ARG_IS("en_hop")){
                    bt_test_args.hopping = TRUE;
                } else if (ARG_IS("dis_hop")){
                    bt_test_args.hopping = FALSE;
                }
                else
                if (ARG_IS("chnl")){
                    if (command_counter+1>=command_number)
                    {
                        return ERR;
                    }
                    command_counter++;

                    char** discard = NULL;
                    
                    bt_test_args.channel = strtol(argv[command_counter], discard,10);
                }
                else
                if (ARG_IS("en_edr")){
                    bt_test_args.is_edr = TRUE;
                } else if (ARG_IS("dis_edr")){
                    bt_test_args.is_edr = FALSE;
                }
                else
                if (ARG_IS("en_wht")){
                    bt_test_args.whitening = 0x00;
                } else if (ARG_IS("dis_wht")){
                    bt_test_args.whitening = 0x01;
                }
                else
                if (ARG_IS("len")){
                    if (command_counter+1>=command_number)
                    {
                        return ERR;
                    }
                    command_counter++;

                    char** discard = NULL;
                    int len = strtol(argv[command_counter], discard, 10);

                    if (!len || len > bt_test_args.len){
                        return ERR;
                    }
                    bt_test_args.len = len;
                }
                else
                if (ARG_IS("addr")){
                    if (command_counter+1>=command_number)
                    {
                        return ERR;
                    }
                    command_counter++;

                    char** discard = NULL;
                    int head = 0;

                    while (head<strlen(argv[command_counter])){
                        char temp[2];

                        strncpy(temp, argv[command_counter]+head, 2);
                        bt_test_args.addr[head/2] = strtol(temp, discard, 16);
                        head+=2;
                    }
                }
                else
                if (ARG_IS("tx_pwr")){
                    if (command_counter+1>=command_number)
                    {
                        return ERR;
                    }
                    command_counter++;

                    char** discard = NULL;
                    
                    bt_test_args.tx_pwr = strtol(argv[command_counter], discard,10);
                }
                command_counter++;
            }
            if (command_counter==command_number){
                return OK;
            } else{
                return ERR;
            }
        }
        else{
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

void send_raw_hci_command(){
    if (bt_test_args.type==txstop){
        r_hci_command[r_hci_command_len++] = 0x01;
        r_hci_command[r_hci_command_len++] = 0x0C;
        r_hci_command[r_hci_command_len++] = 0x18;
        r_hci_command[r_hci_command_len++] = 0x01;
        r_hci_command[r_hci_command_len++] = 0x00;
    } else if (bt_test_args.type == rxstop){
        r_hci_command[r_hci_command_len++] = 0x01;
        r_hci_command[r_hci_command_len++] = 0x0C;
        r_hci_command[r_hci_command_len++] = 0x18;
        r_hci_command[r_hci_command_len++] = 0x01;
        r_hci_command[r_hci_command_len++] = 0x01;
    } else if (bt_test_args.type>=txDH1 && bt_test_args.type<=rx3DH5){
        if (bt_test_args.type>=txDH1 && bt_test_args.type<=tx3DH5){
            r_hci_command[r_hci_command_len++] = 0x01;
            r_hci_command[r_hci_command_len++] = 0x06;
            r_hci_command[r_hci_command_len++] = 0x18;
            r_hci_command[r_hci_command_len++] = 0x0E;
        } else {
            r_hci_command[r_hci_command_len++] = 0x01;
            r_hci_command[r_hci_command_len++] = 0x0B;
            r_hci_command[r_hci_command_len++] = 0x18;
            r_hci_command[r_hci_command_len++] = 0x0D;
        } 
        switch (bt_test_args.type){
            case txDH1:
            case rxDH1:
                r_hci_command[r_hci_command_len++] = 0x04;
                break;
            case txDH3:
            case rxDH3:
                r_hci_command[r_hci_command_len++] = 0x0B;
                break;
            case txDH5:
            case rxDH5:
                r_hci_command[r_hci_command_len++] = 0x0F;
                break;
            case tx2DH1:
            case rx2DH1:
                r_hci_command[r_hci_command_len++] = 0x04;
                break;
            case tx2DH3:
            case rx2DH3:
                r_hci_command[r_hci_command_len++] = 0x0A;
                break;
            case tx2DH5:
            case rx2DH5:
                r_hci_command[r_hci_command_len++] = 0x0E;
                break;
            case tx3DH1:
            case rx3DH1:
                r_hci_command[r_hci_command_len++] = 0x08;
                break;
            case tx3DH3:
            case rx3DH3:
                r_hci_command[r_hci_command_len++] = 0x0B;
                break;
            case tx3DH5:
            case rx3DH5:
                r_hci_command[r_hci_command_len++] = 0x0F;
                break;
            default:
                break;
        }
        r_hci_command[r_hci_command_len++] = bt_test_args.pattern;
        if (bt_test_args.type>=txDH1 && bt_test_args.type<=tx3DH5){
            r_hci_command[r_hci_command_len++] = bt_test_args.hopping;
        }
        r_hci_command[r_hci_command_len++] = bt_test_args.channel;
        r_hci_command[r_hci_command_len++] = bt_test_args.is_edr;
        if (bt_test_args.type>=txDH1 && bt_test_args.type<=tx3DH5){
            r_hci_command[r_hci_command_len++] = bt_test_args.whitening;
            r_hci_command[r_hci_command_len++] = (bt_test_args.len & 0xFF);
            r_hci_command[r_hci_command_len++] = ((bt_test_args.len >> 8) & 0xFF);
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[0];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[1];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[2];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[3];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[4];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[5];
        } else {
            r_hci_command[r_hci_command_len++] = (bt_test_args.len & 0xFF);
            r_hci_command[r_hci_command_len++] = ((bt_test_args.len >> 8) & 0xFF);
            r_hci_command[r_hci_command_len++] = bt_test_args.whitening;
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[0];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[1];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[2];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[3];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[4];
            r_hci_command[r_hci_command_len++] = bt_test_args.addr[5];
        }
    } else if (bt_test_args.type == scpc_start){
	uint8_t i = 0;
        r_hci_command[r_hci_command_len++] = 0x01;
        r_hci_command[r_hci_command_len++] = 0xC6;
        r_hci_command[r_hci_command_len++] = 0xFC;
        r_hci_command[r_hci_command_len++] = 0x0E;
        r_hci_command[r_hci_command_len++] = 0x01;
        r_hci_command[r_hci_command_len++] = bt_test_args.channel;
        r_hci_command[r_hci_command_len++] = bt_test_args.tx_pwr;
        for (i=0;i<11;i++){
            r_hci_command[r_hci_command_len++] = 0x00;
        }
    } else if (bt_test_args.type == scpc_stop){
	uint8_t i = 0;
        r_hci_command[r_hci_command_len++] = 0x01;
        r_hci_command[r_hci_command_len++] = 0xC6;
        r_hci_command[r_hci_command_len++] = 0xFC;
        r_hci_command[r_hci_command_len++] = 0x0E;
        r_hci_command[r_hci_command_len++] = 0x00;
        r_hci_command[r_hci_command_len++] = bt_test_args.channel;
        r_hci_command[r_hci_command_len++] = bt_test_args.tx_pwr;
        for (i=0;i<11;i++){
            r_hci_command[r_hci_command_len++] = 0x00;
        }
    }
    send_to_server(r_hci_command, r_hci_command_len);
}

void send_wlan_command(void)
{
    int i = 0;
    int command_len = 0;
    unsigned char buffer[SOCKER_BUFFER_SIZE] = {0,};
    while ((w_command[i] != NULL) && (i < WLAN_CMD_MAX_CNT)) {
        strcat((char*)&buffer[0], w_command[i]);
        strcat((char*)&buffer[0], " ");
        i++;
    }
    command_len = strlen((char*)&buffer[0]);
    printf("command_len:%d \r\n", command_len);
    if (command_len > 1) {
        //command_len--; // rm last space
        //buffer[command_len] = '\0';
        send_to_server(buffer, command_len);
    }
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


int hci_recv(char* data, int len){
    int i = 0;
    unsigned char tmp_byte[64];// = 0x00;

    memset(data, 0, len);

    len = read(bt_dev_fd, data, len);
    if (len < 0) {
        printf("read error \n");
        return FALSE;
    }
    printf("EVENT(%d): \r\n", len);
    for(i = 0; i < len; i++){
        tmp_byte[i] = (unsigned char)data[i];
        printf("%02X ", tmp_byte[i]);
    }
    printf("\r\n");

    if(tmp_byte[1] == 0x0D) {
        uint32_t rx_pkt_len = tmp_byte[4] | (tmp_byte[5] << 8) | (tmp_byte[6] << 16) | (tmp_byte[7] << 24);
        uint32_t total_rx_pkts = tmp_byte[8] | (tmp_byte[9] << 8) | (tmp_byte[10] << 16) | (tmp_byte[11] << 24);
        uint32_t rx_ok_pkts = tmp_byte[12] | (tmp_byte[13] << 8) | (tmp_byte[14] << 16) | (tmp_byte[15] << 24);
        uint32_t rx_err_pkts = tmp_byte[16] | (tmp_byte[17] << 8) | (tmp_byte[18] << 16) | (tmp_byte[19] << 24);
        uint32_t rx_err_bits = tmp_byte[20] | (tmp_byte[21] << 8) | (tmp_byte[22] << 16) | (tmp_byte[23] << 24);
        float per = (float)  100 * rx_err_pkts / total_rx_pkts;
        float ber = (float)  10000 * rx_err_bits / (total_rx_pkts * rx_pkt_len * 8) ;
        printf("rx result: pkt_len=%d, total=%d, ok=%d, err=%d, err_bits=%d, per=%.2f%%, ber=%.4f%%%%%%\n",
            rx_pkt_len, total_rx_pkts, rx_ok_pkts, rx_err_pkts, rx_err_bits, per, ber);
    }
    return len;
}

int wlan_send(char *data, int len)
{
    int sock;
    struct ifreq ifr;
    int ret = 0;
    int i = 0;
    char wl_cmd[64];
    struct android_wifi_priv_cmd priv_cmd;
    char is_param_err = 0;
    int buf_len = 0;
    int argC = 0;

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("bad sock!\n");
        return ERR;
    }

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, w_ifname);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
        printf("%s Could not read interface %s flags: %s",__func__, w_ifname, strerror(errno));
        return ERR;
    }

    if (!(ifr.ifr_flags & IFF_UP)) {
        printf("%s is not up!\n",w_ifname);
        return ERR;
    }

    printf("ifr.ifr_name = %s\n", ifr.ifr_name);
    memset(&priv_cmd, 0, sizeof(priv_cmd));
    memset(wl_cmd, 0, sizeof(wl_cmd));
    // extract wlan command & argC
    for (i = 0; i < len; i++) {
        if (data[i] != ' ') {
            if (argC == 0) {
                wl_cmd[i] = data[i];
            }
        } else {
            argC++;
        }
    }
    printf("wl_cmd=%s, argC=%d\r\n", wl_cmd, argC);

    priv_cmd.buf = data;
    priv_cmd.used_len = len;
    priv_cmd.total_len = SOCKER_BUFFER_SIZE;
    ifr.ifr_data = (void*)&priv_cmd;

    if ((strcasecmp(wl_cmd, "BT_RESET") == 0)) {
        char bt_reset_hci_cmd[32] = "01 03 0c 00";
        if (argC == 1) {
            buf_len = priv_cmd.used_len;
            memcpy(&priv_cmd.buf[buf_len], &bt_reset_hci_cmd[0], strlen(bt_reset_hci_cmd));
            buf_len += strlen(bt_reset_hci_cmd);
            priv_cmd.used_len = buf_len;
        } else {
            is_param_err = 1;
        }
    } else if ((strcasecmp(wl_cmd, "BT_TXDH") == 0)) {
        char bt_txdh_hci_cmd[255] = "01 06 18 0e ";
        if (argC == 15) {
            buf_len = priv_cmd.used_len;
            int arg_len = strlen("BT_TXDH");
            int txdh_cmd_len = strlen(bt_txdh_hci_cmd);
            memcpy(&bt_txdh_hci_cmd[txdh_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            memcpy(&priv_cmd.buf[arg_len+1], &bt_txdh_hci_cmd[0], strlen(bt_txdh_hci_cmd));
            buf_len += strlen(bt_txdh_hci_cmd);
            priv_cmd.used_len = buf_len;
        } else {
            is_param_err = 1;
        }
    } else if ((strcasecmp(wl_cmd, "BT_RXDH") == 0)) {
        if (argC == 14) {
            char bt_rxdh_hci_cmd[255] = "01 0b 18 0d ";
            buf_len = priv_cmd.used_len;
            int arg_len = strlen("BT_RXDH");
            int rxdh_cmd_len = strlen(bt_rxdh_hci_cmd);
            memcpy(&bt_rxdh_hci_cmd[rxdh_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            memcpy(&priv_cmd.buf[arg_len+1], &bt_rxdh_hci_cmd[0], strlen(bt_rxdh_hci_cmd));
            buf_len += strlen(bt_rxdh_hci_cmd);
            priv_cmd.used_len = buf_len;
        } else {
            is_param_err = 1;
        }
    }  else if ((strcasecmp(wl_cmd, "BT_STOP") == 0)) {
        char bt_stop_hci_cmd[255] = "01 0C 18 01 ";
        if (argC == 2) {
            buf_len = priv_cmd.used_len;
            int arg_len = strlen("BT_STOP");
            int stop_cmd_len = strlen(bt_stop_hci_cmd);
            memcpy(&bt_stop_hci_cmd[stop_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            memcpy(&priv_cmd.buf[arg_len+1], &bt_stop_hci_cmd[0], strlen(bt_stop_hci_cmd));
            buf_len += strlen(bt_stop_hci_cmd);
            priv_cmd.used_len = buf_len;
    } else if ((strcasecmp(wl_cmd, "BT_DATA") == 0)) {
            //char bt_raw_data_cmd[255];
            int arg_len = strlen(wl_cmd);
            buf_len = priv_cmd.used_len;
            memcpy(&priv_cmd.buf[arg_len+1], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            priv_cmd.used_len = buf_len - arg_len - 1;
    } else {
            is_param_err = 1;
        }
    } else {
        is_param_err = 0;
    }
    if (is_param_err) {
        printf("param error!!!\n");
        return ERR;
    }
    if ((ret = ioctl(sock, TXRX_PARA, &ifr)) < 0) {
        printf("%s: error ioctl[TX_PARA] ret= %d\n", __func__, ret);
        return ret;
    }
    memcpy(&priv_cmd, ifr.ifr_data, sizeof(struct android_wifi_priv_cmd));
    if (strcasecmp(wl_cmd, "GET_BT_RX_RESULT") == 0) {
        printf("done: get bt rx total=%d, ok=%d, err=%d\n", *(unsigned int *)priv_cmd.buf,
            *(unsigned int *)&priv_cmd.buf[4],
            *(unsigned int *)&priv_cmd.buf[8]);
    } else if (strcasecmp(wl_cmd, "BT_DATA") == 0) {
        unsigned char *buff = (unsigned char *)&priv_cmd.buf[1];
        int  len = priv_cmd.buf[0];
	int idx = 0;
        printf("done: %d\n", len);
        for (idx = 0; idx < len; idx++) {
            printf("%02x ", buff[idx]);
        }
        printf("\n");
     } else {
        printf("done\n");
    }
    return OK;
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
        } else {
            wlan_send(buffer, recv_len);
        }
    }
}


void* hci_recv_thread( void *arg ){
    printf("hci recv thread ready %p\r\n", arg);
    while(1){
        hci_recv(hci_event, sizeof(hci_event));
//        exit(0);
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
        ret = pthread_create( &hci_recv_th, NULL, hci_recv_thread, NULL);
        printf("bt device test ok \r\n");
    } else if (interface == WLAN) {
        char buf[SOCKER_BUFFER_SIZE] = {"BT_RESET "};
        wlan_send(buf, strlen(buf));
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
    } else if (server_or_client == CLIENT_DIRECT) {
        send_raw_hci_command();
    } else {
        send_wlan_command();
    }
    return 0;
}
