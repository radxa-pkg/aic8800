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

struct ble_ad_filter{
	uint8_t len;
	uint8_t ad_type;
	uint8_t data[30];
};

struct bd_addr{
	uint8_t addr[6];
};

struct white_list{
	struct bd_addr wl_bdaddr;
	uint8_t wl_bdaddr_type;
};

struct ble_m2d_flash_t{
	struct ble_ad_filter ad_filter;
	struct white_list wl;
};


#define OK 0
#define ERR -1
#define FALSE 0
#define TRUE 1

#define SOCKER_BUFFER_SIZE 1024
#define SET_APCF		_IOR('E', 181, int)

char dev_path[] = "/dev/aic_btusb_ex_dev";
int bt_dev_fd = 0;
char* s_hci_command[256];

int dev_open(){

    bt_dev_fd = open(dev_path, O_RDWR | O_NDELAY);
    if(bt_dev_fd < 0) {
        printf("dev_path %s fail to open %d \r\n", dev_path, bt_dev_fd);
        return ERR;
    }
    printf("bt dev open successful\r\n");
    return OK;
}

void dev_close(){
    close(bt_dev_fd);
}

void helper(){
    printf("\t<-s> to send hci cmd to interface.\n");
    printf("\t<-c> to clean the pairing device.\n");
    printf("\t \"apcf_test -s <addr_type> <addr0> <addr1> <addr2> <addr3> <addr4> <addr5> <ad_len> <ad_type> <ad_data[0]> ...\n");
	printf("\t \"apcf_test -c \n");
	printf("\t eg. \"apcf_test -s 01 11 22 33 44 55 66 03 ff 00 01\n");
    printf("\t addr_type = 0x01\n");
	printf("\t addr[6] = {0x11,0x22,0x33,0x44,0x55}\n");
	printf("\t ad_len = 0x03 ,ad_type = 0xff ad_data = {0x00,0x01}\n");
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
    printf("atoh1: ret_value:%x\r\n", ret_value);
    //l
    if(s_data[1] >= 48 && s_data[1] <= 57){
        temp_value = s_data[1] - 48;
    }else if(s_data[1] >= 65 && s_data[1] <= 70){
        temp_value = (s_data[1] - 65) + 10;
    }else if(s_data[1] >= 97 && s_data[1] <= 102){
        temp_value = (s_data[1] - 97) + 10;
    }

    ret_value |= temp_value;
    printf("atoh2: ret_value:%x\r\n", ret_value);

    return ret_value;

}

int parse_cmd(int argc, char **argv){
	int command_number = argc;
	int command_counter = 1;
	int hci_command_counter = 0;

	while(command_counter < command_number){
		if(!strcmp(argv[command_counter], "-s")){
			command_counter++;
			hci_command_counter = 0;
			while(command_counter < command_number){
				s_hci_command[hci_command_counter++] = argv[command_counter];
				command_counter++;
				}
			return OK;
		}else if(!strcmp(argv[command_counter], "-c")){
			command_counter++;
			hci_command_counter = 0;
			while(command_counter < command_number){
				s_hci_command[hci_command_counter++] = argv[command_counter];
				command_counter++;
				}
			return OK;
		}
	}
	helper();
	return ERR;
}

void dev_ioctl_w(void){
	int i = 0;
	int command_len = 0;
	unsigned char buffer[SOCKER_BUFFER_SIZE];
	unsigned char send_buffer[SOCKER_BUFFER_SIZE];
	struct ble_m2d_flash_t m2d_flash;

//	  printf("send hci command: \r\n");
	memset(buffer,0,SOCKER_BUFFER_SIZE);
	memset(send_buffer,0,SOCKER_BUFFER_SIZE);

	for(i = 0;i < 256; i++){
		if(s_hci_command[i] == NULL){
			command_len = i;
			break;
		}

		buffer[i] = atoh(s_hci_command[i]);
		printf("%02X ", buffer[i]);
	}

	m2d_flash.wl.wl_bdaddr_type = buffer[0];
	m2d_flash.wl.wl_bdaddr.addr[0] = buffer[1];
	m2d_flash.wl.wl_bdaddr.addr[1] = buffer[2];
	m2d_flash.wl.wl_bdaddr.addr[2] = buffer[3];
	m2d_flash.wl.wl_bdaddr.addr[3] = buffer[4];
	m2d_flash.wl.wl_bdaddr.addr[4] = buffer[5];
	m2d_flash.wl.wl_bdaddr.addr[5] = buffer[6];
	printf(" type 0x%x, addr = 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\r\n",m2d_flash.wl.wl_bdaddr_type,\
		m2d_flash.wl.wl_bdaddr.addr[0],m2d_flash.wl.wl_bdaddr.addr[1],m2d_flash.wl.wl_bdaddr.addr[2],\
		m2d_flash.wl.wl_bdaddr.addr[3],m2d_flash.wl.wl_bdaddr.addr[4],m2d_flash.wl.wl_bdaddr.addr[5]);

	m2d_flash.ad_filter.len = buffer[7];
	if(m2d_flash.ad_filter.len > 1){
		m2d_flash.ad_filter.ad_type = buffer[8];
		memcpy(m2d_flash.ad_filter.data, &buffer[9],(m2d_flash.ad_filter.len-1));
	}

	send_buffer[0] = 4+1+sizeof(struct ble_m2d_flash_t);
	send_buffer[1] = 0x01;
	send_buffer[2] = 0x57;
	send_buffer[3] = 0xfd;
	send_buffer[4] = 1+sizeof(struct ble_m2d_flash_t);
	send_buffer[5] = 0x08;
	memcpy(&send_buffer[6],&m2d_flash,sizeof(struct ble_m2d_flash_t));
	
//	  printf("\r\n");
	ioctl(bt_dev_fd, SET_APCF, send_buffer);
}

int main(int argc, char *argv[]){
	printf("argc:%d \r\n", argc);
	int ret = parse_cmd(argc, argv);
	if (ret) {
		return ret;
	}
    dev_open();
	dev_ioctl_w();
	dev_close();
	return 0;
}
