//By Aiden 2020 06/30 15:32
//To compatible other wifi module.

#include "hardware_legacy/wifi.h"
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <android-base/logging.h>
#include <cutils/misc.h>
#include <cutils/properties.h>
#include <asm/ioctl.h>
#include <sys/syscall.h>

//Version
#define version "1.1.0 2022_0913_1904"

//For module switch other type.
#define firmware_p2p "_p2p"
#define firmware_ap "_apsta"

//For module type.
#define UNKNOW 0 
#define SDIO 1
#define USB 2
int module_type = 0;
int module_index = 0;
char* device_id[256];

#define SDIO_DEVICE_INFO_PATH "/sys/bus/sdio/devices/sdio:0001:1/device"
#define USB_DEVICE_INFO_PATH ""
#define WIFI_CHIP_NODE_PATH "/data/vendor/wifi/wid_fp"
#define DRIVER_PROP_NAME "wlan.driver.status"
#define BT_VENDOR_LIB "persist.vendor.libbt_vendor"
#define WIFI_CHIP_VENDOR "vendor.bcm_wifi"

//For wifi power up and sdio card detect.
#define SDIO_POWER_UP	_IO('m', 3)

extern "C" int init_module(void *, unsigned long, const char *);
extern "C" int delete_module(const char *, unsigned int);


typedef struct cihp_name_map_t {
	char vendor[128];
	char driver_name[128];
	char pidvid[128];
	char chipname[128];
	char driver_path[128];
	char insmod_arg[256];
	char firmware_path[128];
	char nvram_path[128];
	char bt_vendor[128];
} cihp_name_map_t;

static int identify_sucess = -1;
static char recoginze_wifi_chip[64];
static const char USB_DIR[] = "/sys/bus/usb/devices";
static const char SDIO_DIR[]= "/sys/bus/sdio/devices";
static const char PCIE_DIR[]= "/sys/bus/pci/devices";
static const char PREFIX_SDIO[] = "SDIO_ID=";
static const char PREFIX_PCIE[] = "PCI_ID=";
static const char PREFIX_USB[] = "PRODUCT=";
static int invalid_wifi_device_id = -1;

//You can add wifi module in list.
const cihp_name_map_t wifi_cihp_name_map[] = {
	{"aic","aic8800_fdrv", "5449:0145", "aic8800_fdrv", "/vendor/lib/modules/aic8800_fdrv.ko", "", "", "","libbt-vendor-aicMulti.so"},
	{"aic","aic8800_fdrv", "5449:0145", "aic8800_fdrv", "/vendor/lib/modules/aic8800_fdrv.ko", "", "", "","libbt-vendor-aicMulti.so"},
	{"aic","aic8800_fdrv", "5449:0145", "aic8800_fdrv", "/vendor/lib/modules/aic8800_fdrv.ko", "", "", "","libbt-vendor-aicMulti.so"}
};

void set_wifi_power(int on){
	int fd;
	
	fd = open("/dev/wifi_power", O_RDWR);
	if(fd != -1){
		if(on == SDIO_POWER_UP){
			if(ioctl(fd, SDIO_POWER_UP) < 0){
				PLOG(ERROR) << "AIDEN Set SDIO Wi-Fi power up error!!!";
				close(fd);
				return;
			}
		}
	}else{
		PLOG(ERROR) << "AIDEN Device open failed !!!";
	}

	close(fd);
	return;
}

int get_wifi_device_id(const char *bus_dir, const char *prefix){
	int idnum;
	int i = 0;
	int ret = invalid_wifi_device_id;
	DIR *dir;
	struct dirent *next;
	FILE *fp = NULL;
	int fd = 0;
	idnum = sizeof(wifi_cihp_name_map) / sizeof(wifi_cihp_name_map[0]);

	dir = opendir(bus_dir);
	if (!dir) {
			PLOG(ERROR) << "open dir failed:" << strerror(errno);
			return invalid_wifi_device_id;
	}

	while ((next = readdir(dir)) != NULL) {
		char line[256];
		char uevent_file[256] = {0};
		sprintf(uevent_file, "%s/%s/uevent", bus_dir, next->d_name);
		PLOG(DEBUG) << "uevent path:" << uevent_file;
		fp = fopen(uevent_file, "r");
		if (NULL == fp) {
			continue;
 		}
		

		while (fgets(line, sizeof(line), fp)) {
   			char *pos = NULL;
			int product_vid = 0;
			int product_did = 0;
			int producd_bcddev = 0;
			char temp[10] = {0};
			pos = strstr(line, prefix);
			//PLOG(DEBUG) << "line:" << line << ", prefix:" << prefix << ".";
			if (pos != NULL) {
				if (strncmp(bus_dir, USB_DIR, sizeof(USB_DIR)) == 0){
					sscanf(pos + 8, "%x/%x/%x", &product_vid, &product_did, &producd_bcddev);
				}
				else if (strncmp(bus_dir, SDIO_DIR, sizeof(SDIO_DIR)) == 0){
					sscanf(pos + 8, "%x:%x", &product_vid, &product_did);
				}
				else if (strncmp(bus_dir, PCIE_DIR, sizeof(PCIE_DIR)) == 0){
					sscanf(pos + 7, "%x:%x", &product_vid, &product_did);
				}
				else{
					return invalid_wifi_device_id;
				}

				sprintf(temp, "%04x:%04x", product_vid, product_did);
				PLOG(ERROR) << "AIDEN CHECK pid:vid :" << temp;
				for (i = 0; i < idnum; i++) {
					if (0 == strncmp(temp, wifi_cihp_name_map[i].pidvid, 9)) {
 						PLOG(ERROR) << "found device pid:vid :" << temp;
						module_index = i;
						fd = open(WIFI_CHIP_NODE_PATH, O_RDWR);
						write (fd, (void*)wifi_cihp_name_map[i].chipname, sizeof(wifi_cihp_name_map[i].chipname));
						property_set("persist.vendor.libbt_vendor", wifi_cihp_name_map[i].bt_vendor);
						property_set("vendor.bcm_wifi", wifi_cihp_name_map[i].vendor);
						close(fd);
						strcpy(recoginze_wifi_chip, wifi_cihp_name_map[i].driver_name);
 						identify_sucess = 1 ;
						ret = 0;
						fclose(fp);
						goto ready;
					}
				}
			}
		}
		fclose(fp);
        }

        ret = invalid_wifi_device_id;
ready:
        closedir(dir);
        PLOG(ERROR) << "wifi detectd return ret:" << ret;
        return ret;

	
}

void module_search(){

	if (get_wifi_device_id(USB_DIR, PREFIX_USB) == 0){
			module_type = USB;
			PLOG(ERROR) << "SDIO WIFI identify sucess";
	}else if (get_wifi_device_id(SDIO_DIR, PREFIX_SDIO) == 0){
			module_type = SDIO;
			PLOG(ERROR) << "USB WIFI identify sucess";
	}else if (get_wifi_device_id(PCIE_DIR, PREFIX_PCIE) == 0){
			PLOG(ERROR) << "PCIE WIFI identify sucess";
	}else {
			PLOG(DEBUG) << "maybe there is no usb wifi or sdio or pcie wifi,set default wifi module APXXXX";
			strcpy(recoginze_wifi_chip, "APXXXX");
			identify_sucess = 1 ;
	}
}

static int insmod(const char *filename, const char *args) {
    int ret;
    int fd;

  	fd = TEMP_FAILURE_RETRY(open(filename, O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
  	if (fd < 0) {
    	PLOG(ERROR) << "Failed to open " << filename;
    	return -1;
  	}
  	ret = syscall(__NR_finit_module, fd, args, 0);

  	close(fd);

  	if (ret < 0) {
    	PLOG(ERROR) << "finit_module return: " << ret;
  	}

  	return ret;
}

static int rmmod(const char *modname) {
  	int ret = -1;
  	int maxtry = 10;

  	while (maxtry-- > 0) {
    	ret = delete_module(modname, O_NONBLOCK | O_EXCL);
    	if (ret < 0 && errno == EAGAIN)
      		usleep(500000);
    	else
      	break;
  	}

  	if (ret != 0)
    	PLOG(ERROR) << "Unable to unload driver module '" << modname << "'";
  	return ret;
}

int ext_wifi_load_driver(){
	int ret = 0;

	
	PLOG(ERROR) << "AIDEN Version: " << version;
        
        property_set(DRIVER_PROP_NAME, "unloaded");
	if(module_type != UNKNOW){
		PLOG(ERROR) << "AIDEN aiden_wifi_load_driver module " << wifi_cihp_name_map[module_index].chipname;
		ret = insmod(wifi_cihp_name_map[module_index].driver_path, wifi_cihp_name_map[module_index].insmod_arg);	
	}else{
		PLOG(ERROR) << "AIDEN aiden_wifi_load_driver module_type UNKNOW";
		return -1;
	}
	PLOG(DEBUG) << "AIDEN aiden_wifi_load_driver ret " << ret;
	
	return ret;
}

int ext_wifi_load_driver_ext(void){
	
	PLOG(ERROR) << "AIDEN aiden_wifi_load_driver_ext start";
	set_wifi_power(SDIO_POWER_UP);
	module_search();
	return ext_wifi_load_driver();
}

int ext_wifi_unload_driver_ext(void){
	
	PLOG(ERROR) << "AIDEN aiden_wifi_unload_driver_ext start";
	if(module_type == UNKNOW){
		return -1;
	}
	return rmmod(wifi_cihp_name_map[module_index].driver_name);
}

const char* ext_get_wifi_vendor_name(void){
	
	PLOG(ERROR) << "AIDEN aiden_get_wifi_vendor_name start";
	if(module_type == UNKNOW){
		return "ERR";
	}
	return wifi_cihp_name_map[module_index].chipname;
}

const char* ext_wifi_get_fw_path_ext(int fw_type){
	char *fw_path;
	
	PLOG(ERROR) << "AIDEN aiden_wifi_get_fw_path_ext start";
	fw_path = (char*)malloc(sizeof(char)*256);
	memset(fw_path, 0, 256);
	if(module_type == UNKNOW){
		free(fw_path);
		return NULL;
	}

	if (0 != strncmp(wifi_cihp_name_map[module_index].vendor, "bcm",3)){
		free(fw_path);
                return NULL;
	}

	switch (fw_type){
		case WIFI_GET_FW_PATH_STA:
      		return (const char*)wifi_cihp_name_map[module_index].firmware_path;
    	case WIFI_GET_FW_PATH_AP:
			strcat(fw_path, wifi_cihp_name_map[module_index].firmware_path);
			strcat(fw_path, "_apsta");
      		return (const char*)fw_path;
    	case WIFI_GET_FW_PATH_P2P:
			strcat(fw_path, wifi_cihp_name_map[module_index].firmware_path);
			strcat(fw_path, "_p2p");
      		return (const char*)fw_path;
	}
	
	return "ERR";
}

int ext_wifi_change_fw_path(const char *fwpath) {
  int len;
  int fd;
  int ret = 0;

  if(module_type == UNKNOW){
	return -1;
  }


  PLOG(ERROR) << "AIDEN aiden_wifi_change_fw_path " << fwpath;

  if(strlen(fwpath) == 0){
	  return 0;
  }

  if (!fwpath) return ret;
  fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_FW_PATH_PARAM, O_WRONLY));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open wlan fw path param";
    return -1;
  }
  len = strlen(fwpath) + 1;
  if (TEMP_FAILURE_RETRY(write(fd, fwpath, len)) != len) {
    PLOG(ERROR) << "Failed to write wlan fw path param";
    ret = -1;
  }
  close(fd);
  return ret;
}


