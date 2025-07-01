/******************************************************************************
 *
 *  Copyright (C) 2021-2021 amlogic Corporation
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

#define LOG_TAG "multi_bt"
#include <cutils/properties.h>
#include <cutils/android_filesystem_config.h>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <asm/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <dirent.h>

#include "multibt_hal.h"

//c++
#include <iostream>
#include <string>

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#define VND_PORT_NAME_MAXLEN    256
#define LOOP_TIMES              1

/******************************************************************************
**  Local type definitions
******************************************************************************/

/* vendor serial control block */
typedef struct
{
    int fd;                     /* fd to Bluetooth device */
    struct termios termios;     /* serial terminal of BT port */
    char port_name[VND_PORT_NAME_MAXLEN];
} vnd_userial_cb_t;

struct device_info {
    unsigned short device_id;
	char device_name[20];
    char vendor_lib_name[64];
	char module_name[20];
	unsigned short chip_id;
	bool power_type;
};

struct uart_device_info {
	unsigned short vendor_id;
	char device_name[20];
	char vendor_lib_name[64];
	bool power_type;
};

/******************************************************************************
**  Static variables
******************************************************************************/

static vnd_userial_cb_t vnd_userial;
static int rfkill_id = -1;
static char *rfkill_state_path = NULL;
static int VDBG = 1;

static const tUSERIAL_CFG userial_H5_cfg =
{
    (USERIAL_DATABITS_8 | USERIAL_PARITY_EVEN | USERIAL_STOPBITS_1),
    USERIAL_BAUD_115200,
};
static const tUSERIAL_CFG userial_H4_cfg =
{
    (USERIAL_DATABITS_8 | USERIAL_PARITY_NONE | USERIAL_STOPBITS_1),
    USERIAL_BAUD_115200,
};
static const char *p_pdt_name[] = {
	NULL
};
/******************************************************************************
**  init variables
******************************************************************************/
static uint8_t vendor_info[] =     {0x01,0x01,0x10,0x00};
static uint8_t vendor_reset[] =    {0x01,0x03,0x0c,0x00};
static uint8_t vendor_sync[] =     {0xc0,0x00,0x2f,0x00,0xd0,0x01,0x7e,0xc0}; //{0x01, 0x7E}
//static uint8_t vendor_sync_rsp[] = {0xc0,0x00,0x2f,0x00,0xd0,0x02,0x7d,0xc0}; //{0x02, 0x7D}

static const char MULTIBT_VENDOR_PROP_NAME[] = "persist.vendor.libbt_vendor";
static const char MULTIBT_MODULE_PROP_NAME[] = "persist.vendor.bt_module";
static const char MULTIBT_NAME_PROP_NAME[] = "persist.vendor.bt_name";
static const char MULTIBT_DEBUG_PROP_NAME[] = "persist.vendor.bt_debug";

static std::string devid_subdevid[] = {"1", "2", "3", "4", "5"};
static std::string pciid_subdevid[] = {"0", "1", "2", "3", "4"};
static std::string dev_typeid[] = {"0000", "0001","8800"};

/******************************************************************************
**  usb/mmc struct config
******************************************************************************/
static const struct device_info bluetooth_dongle[] = {
	{0xC820, "rtl8821cu", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0xC811, "rtl8821cu", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0xD723, "rtl8723du", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0xB82C, "rtl8822bu", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0xB720, "rtl8723bu", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0x0823, "rtl8821au", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0x0821, "rtl8821au", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0x885c, "rtl8852au", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0x885a, "rtl8852au", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0xB733, "rtl8733bu", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0xC82C, "rtl88x2cu", "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, true},
	{0xB761, "rtl8761u",  "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, false},
	{0x8771, "rtl8771u",  "libbt-vendor_rtlMulti.so", "rtk_btusb", 0x0000, false},
	{0x2045, "ap62x8",    "libbt-vendor_bcmMulti.so", "btusb",     0x0000, false},
	{0xBD27, "ap62x8",    "libbt-vendor_bcmMulti.so", "btusb",     0x0000, false},
	{0x0BDC, "ap62x8",    "libbt-vendor_bcmMulti.so", "btusb",     0x0000, false},
	{0x0000, "ap6398s",   "libbt-vendor_bcmMulti.so", "NULL",      0x4359, false},
	{0x9378, "qca9379",   "libbt-vendor_qcaMulti.so", "bt_usb_qcom", 0x0000, true},
	{0x7A85, "qca9379",   "libbt-vendor_qcaMulti.so", "bt_usb_qcom", 0x0000, true},
	{0x7668, "mtk7668u",  "libbt-vendor_mtkMulti.so", "btmtk_usb", 0x0000, true},
	{0x0000, "mtk7920e",  "libbt-vendor_792Multi.so", "btmtkuart", 0x7961, false},
	{0x7961, "mtk7920u",  "libbt-vendor_792Multi.so", "btmtk_usb_unify", 0x0000, false},
	{0x0000, "qca6391",   "libbt-vendor_qtiMulti.so", "NULL"     , 0x1101, false},
	{0x0000, "qca206x",   "libbt-vendor_qtiMulti.so", "NULL"     , 0x1103, false},
	{0x0000, "nxp8987",   "libbt-vendor_nxpMulti.so", "NULL"     , 0x9149, false},
	{0x0000, "nxp8997",   "libbt-vendor_nxpMulti.so", "NULL"     , 0x9141, false},
	{0x0000, "nxpiw620",  "libbt-vendor_nxpMulti.so", "NULL"     , 0x2b56, false},
	{0x0000, "mtk7668s",  "libbt-vendor_mtkMulti.so", "btmtksdio", 0x7608, true},
	{0x0000, "mtk7661s",  "libbt-vendor_mtkMulti.so", "btmtksdio", 0x7603, true},
	{0x0000, "uwe5621ds", "libbt-vendor_uweMulti.so", "sprdbt_tty", 0x0000, true},
	{0x0000, "aml_w1",       "libbt-vendor_amlMulti.so", "NULL"  , 0x8888, true},
	{0x0000, "aml_w1u_s",    "libbt-vendor_amlMulti.so", "NULL"  , 0x500, false},
	{0x4C55, "aml_w1u",      "libbt-vendor_amlMulti.so", "NULL"  , 0x0000, false},
	{0x0000, "aml_w2_p",     "libbt-vendor_amlMulti.so", "NULL"  , 0x602, false},
	{0x0000, "aml_w2_p",     "libbt-vendor_amlMulti.so", "NULL"  , 0x642, false},
	{0x0000, "aml_w2_s",     "libbt-vendor_amlMulti.so", "NULL"  , 0x600, false},
	{0x0000, "aml_w2_s",     "libbt-vendor_amlMulti.so", "NULL"  , 0x640, false},
	{0x601, "aml_w2_u",     "libbt-vendor_amlMulti.so", "NULL"  , 0x0000, false},
	{0x641, "aml_w2_u",     "libbt-vendor_amlMulti.so", "NULL"  , 0x0000, false}
};

/******************************************************************************
**  uart struct config
******************************************************************************/
static const struct uart_device_info uart_dongle[] = {
	{0x0F00, "bcm_bt",  "libbt-vendor_bcmMulti.so", false},
	{0x1D00, "qca_bt",  "libbt-vendor_qcaMulti.so", false},
	{0x5D00, "rtl_bt",  "libbt-vendor_rtlMulti.so", false},
	{0x4600, "mtk_bt",  "libbt-vendor_mtkMulti.so", false},
	{0XFFFF, "aml_bt",  "libbt-vendor_amlMulti.so", false},
	{0XEC01, "uwe_bt",  "libbt-vendor_uweMulti.so", false},
};

static std::string get_usb_path(std::string devid, std::string subdevid)
{
    std::string path = std::string("/sys/bus/usb/devices/") +
                        std::string(devid) +
                        std::string("-") +
                        std::string(subdevid) +
                        std::string("/") +
                        std::string("idProduct");
    return path;
}

static std::string get_dev_path(std::string dev_type, std::string dev_id)
{
    std::string path = std::string("/sys/bus/mmc/devices/") +
                        std::string(dev_type) +
                        std::string(":") +
                        std::string(dev_id) +
                        std::string("/") +
                        std::string(dev_type) +
                        std::string(":") +
                        std::string(dev_id) +
                        std::string(":1/device");
    return path;
}
static std::string get_pci_path(std::string pciid)
{
    std::string path = std::string("/sys/bus/pci/devices/") +
                        std::string("0000:0") +
                        std::string(pciid) +
                        std::string(":00.0/device");
    return path;
}

static int get_config(void)
{
	char str[100];

	memset(str, 0, sizeof(str));
	property_get(MULTIBT_DEBUG_PROP_NAME, str, "is_null");
	if (!strncmp(str, "1", 1)) {
		VDBG = 1;
	}
	PR_INFO("get property : %s", str);
	return 0;
}

static void write_power_type(char * str)
{
	int ret;
	int fd;
	fd = open(BT_POWER_TYPE, O_WRONLY);
	if (fd < 0)
	{
		ALOGE("open(%s) failed: %s (%d)\n", \
			BT_POWER_TYPE, strerror(errno), errno);
	}

	ret = write(fd, str, 1);
	if (ret < 0) {
		ALOGE( "Failed to write bt power evt");
	}
	close(fd);
}

static char* get_power_type(void)
{
	char module_name[16];
	int size = 0;
	int i;

	memset(module_name, 0, sizeof(module_name));
	btvendor_hal.get_module_name(module_name);
	if (!strncmp(module_name, "NULL", 4)) {
		PR_INFO("don't find module name");
		return NULL;
	}

	size = sizeof(uart_dongle) / sizeof(uart_device_info);
	for (i = 0; i < size; i++) {
		if(strstr(module_name, uart_dongle[i].device_name)) {
			PR_INFO("find name: %s", uart_dongle[i].device_name);
			if (!uart_dongle[i].power_type) {
				return (char*)"1";
			}
			else {
				return (char*)"2";
			}
		}
	}

	size = sizeof(bluetooth_dongle) / sizeof(device_info);
	for (i = 0; i < size; i++) {
		if (strstr(module_name, bluetooth_dongle[i].device_name)) {
			PR_INFO("find name: %s", bluetooth_dongle[i].device_name);
			if (!bluetooth_dongle[i].power_type) {
				return (char*)"1";
			}
			else {
				return (char*)"2";
			}
		}
	}
	return NULL;
}

static int set_power_type(void)
{
	char *str;

	str = get_power_type();
	if (!str)
		return 0;

	write_power_type(str);
	return 0;
}

static void get_product_device(void)
{
	char pdt_name[100];
	int i;

	memset(pdt_name, 0, sizeof(pdt_name));
	property_get("ro.product.device", pdt_name, "NULL");
	if (!strncmp(pdt_name, "NULL", sizeof("NULL")-1))
		return;

	for (i = 0; p_pdt_name[i] != NULL; i++) {
		if (!strcmp(p_pdt_name[i], pdt_name)) {
			PR_INFO("product.device : %s", pdt_name);
			write_power_type((char*)"1");
			break;
		}
	}
}

#if 0
static int set_module_name(const char * str)
{
	int fd;
	std::string node_name;
	node_name = std::string(str);
	if((fd = open(NODE_PATH, O_CREAT|O_RDWR, S_IRWXU|S_IROTH)) < 0) {
		PR_ERR("open NODE_PATH error: %s(%d)", strerror(errno), errno);
		goto error;
	}

	if (write(fd, node_name.c_str(),node_name.length()) != node_name.length()) {
		PR_ERR("write node_name error");
		close(fd);
		goto error;
	}

	if (write(fd, "\n", 1) != 1) {
		PR_ERR("write \n error");
		close(fd);
		goto error;
	}

	close(fd);
	return 1;
error:
	return 0;
}
#endif

static int set_module_name(const char * str)
{
	if (str == NULL)
		goto error;

	property_set(MULTIBT_NAME_PROP_NAME, str);
	return 1;
error:
	return 0;
}

static int get_module_name(char * str)
{
	if (str == NULL)
		goto error;

	property_get(MULTIBT_NAME_PROP_NAME, str, "NULL");
	return 1;
error:
	return 0;
}

static unsigned short get_dev_info(std::string path)
{
    char info[16];
    unsigned short val;
    int fp = open(path.c_str(), O_RDONLY);
    if (fp < 0) {
        PR_ERR("Open file (%s) failed !!! %s(%d)", path.c_str(), strerror(errno), errno);
        return 0xFF;
    }
    memset(info, 0, sizeof(info));
    if(read(fp, info, sizeof(info)) < 0) {
		PR_ERR(" %s read failed",__func__);
		close(fp);
		return 0xFF;
	}
    close(fp);
    val = std::strtol(info, nullptr, 16);
    return val;
}

static int matching_usb_device(std::string path)
{
	int cnt, device_id;
	int dongle_size;

	if ((device_id = get_dev_info(path)) == 0xFF) {
		return 1;
	}

	dongle_size = sizeof(bluetooth_dongle)/sizeof(struct device_info);
	for (cnt = 0; cnt < dongle_size; cnt++) {
		if (bluetooth_dongle[cnt].device_id == device_id && bluetooth_dongle[cnt].device_id > 0) {
			if (strncmp(bluetooth_dongle[cnt].module_name, "NULL", sizeof("NULL")-1)) {
				property_set(MULTIBT_MODULE_PROP_NAME, bluetooth_dongle[cnt].module_name);
			}
			property_set(MULTIBT_VENDOR_PROP_NAME, bluetooth_dongle[cnt].vendor_lib_name);
			set_module_name(bluetooth_dongle[cnt].device_name);
			return 0;
		}
	}
	return 1;
}

static int distinguish_vendorusb_module(void)
{
	std::string usb_path;
	for (auto devid : devid_subdevid) {
		for(auto subdevid : devid_subdevid) {
			usb_path = get_usb_path(devid, subdevid);
			if (!matching_usb_device(usb_path)) {
				PR_INFO("matching usb device success");
				return 1;
			}
		}
	}
	return 0;
}

static int set_wifi_power(int on)
{
    int fd = open("/dev/wifi_power", O_RDWR);
    if (fd < 0) {
        PR_ERR("/dev/wifi_power open fail : %s(%d)", strerror(errno), errno);
        return -1;
    }

    if (on == SDIO_POWER_UP) {
        if (ioctl (fd, SDIO_POWER_UP) < 0) {
            PR_ERR("set sdio Wi-Fi power up error!!!");
            close(fd);
            return -1;
       }
    } else if(on== SDIO_POWER_DOWN) {
        if (ioctl (fd, SDIO_POWER_DOWN) < 0) {
            PR_ERR("set sdio Wi-Fi power down error!!!");
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

static int get_dev_type(char *dev_type)
{
    int fd = open("/dev/wifi_power", O_RDWR);
    if (fd < 0) {
       PR_ERR("/dev/wifi_power open fail : %s(%d)", strerror(errno), errno);
       return -1;
    }

    if (ioctl (fd, SDIO_GET_DEV_TYPE, dev_type) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int clr_bten_bit(int type)
{
	int fd = open("/dev/wifi_power", O_RDWR);
    if (fd < 0) {
       PR_ERR("/dev/wifi_power open fail : %s(%d)", strerror(errno), errno);
       return -1;
    }

	switch(type) {
	case CLR_BT_POWER_BIT:
		if (ioctl(fd, type) < 0)
			PR_ERR("%s fail", __func__);
		break;
	default:
		PR_INFO("pls input correct parameters");
		break;
	}
	return 0;
}

static int enum_mmc_type(std::string path)
{
	int cnt;
	int chip_id;
	int dongle_size;

	if ((chip_id = get_dev_info(path)) == 0xFF) {
		return 1;
	}

	dongle_size = sizeof(bluetooth_dongle)/sizeof(struct device_info);
	for (cnt = 0; cnt < dongle_size; cnt++) {
		if (bluetooth_dongle[cnt].chip_id == chip_id && bluetooth_dongle[cnt].device_id <= 0) {
			if (strncmp(bluetooth_dongle[cnt].module_name, "NULL", sizeof("NULL")-1)) {
				property_set(MULTIBT_MODULE_PROP_NAME, bluetooth_dongle[cnt].module_name);
			}
			property_set(MULTIBT_VENDOR_PROP_NAME, bluetooth_dongle[cnt].vendor_lib_name);
			set_module_name(bluetooth_dongle[cnt].device_name);
			return 0;
		}
	}
	return 1;
}

static int enum_uart_type(uint16_t vendor_id)
{
	int cnt;
	int dongle_size = sizeof(uart_dongle)/sizeof(struct uart_device_info);

	for (cnt = 0; cnt < dongle_size; cnt++) {
		if (uart_dongle[cnt].vendor_id == vendor_id) {
			property_set(MULTIBT_VENDOR_PROP_NAME, uart_dongle[cnt].vendor_lib_name);
			set_module_name(uart_dongle[cnt].device_name);
			return 1;
		}
	}
	return 0;
}

static int distinguish_vendorpci_module(void)
{
	std::string pci_path;
	for (auto pciid : pciid_subdevid) {
		pci_path = get_pci_path(pciid);
		if (!enum_mmc_type(pci_path)) {
			PR_INFO("matching pci device success");
			return 1;
		}
	}
	return 0;
}

static int distinguish_vendormmc_module(void)
{
	int cnt = LOOP_TIMES;
	std::string dev_path;
	char dev_type[10] ={"\0"};

	if (get_dev_type(dev_type)) {
		PR_ERR("get dev type error: %s", dev_type);
		return 0;
	}
	PR_INFO("get dev : %s", dev_type);

	while(cnt) {
		for (auto dev_id : dev_typeid) {
			dev_path = get_dev_path(dev_type, dev_id);
			if (!enum_mmc_type(dev_path)) {
				PR_INFO("matching mmc success");
				return 1;
			}
		}
		cnt--;
	}
	return 0;
}

/*****************************************************************************
**   Helper Functions
*****************************************************************************/
static int init_rfkill()
{
    char path[64];
    char buf[16];
    int fd, sz, id;
    sz = -1;//initial

    for (id = 0; ; id++)
    {
        snprintf(path, sizeof(path), "/sys/class/rfkill/rfkill%d/type", id);
        fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            PR_ERR("init_rfkill : open(%s) failed: %s (%d)\n", \
                 path, strerror(errno), errno);
            return -1;
        }

        sz = read(fd, &buf, sizeof(buf));
        close(fd);
        if (sz >= 9 && memcmp(buf, "bluetooth", 9) == 0)
        {
            PR_INFO("break");
            rfkill_id = id;
			break;
        }
        else if (sz == -1)
        {
            PR_ERR("init_rfkill : read(%s) failed: %s (%d)\n", \
                 path, strerror(errno), errno);
            return -1;
        }
    }

    asprintf(&rfkill_state_path, "/sys/class/rfkill/rfkill%d/state", rfkill_id);
    return 0;
}


/*******************************************************************************
**
** Function        upio_set_bluetooth_power
**
** Description     Interact with low layer driver to set Bluetooth power
**                 on/off.
**
** Returns         0  : SUCCESS or Not-Applicable
**                 <0 : ERROR
**
*******************************************************************************/
static int upio_set_bluetooth_power(int on)
{
    int sz;
    int fd = -1;
    int ret = -1;
    char buffer = '0';

    switch(on)
    {
        case UPIO_BT_POWER_OFF:
            buffer = '0';
            break;

        case UPIO_BT_POWER_ON:
            buffer = '1';
            break;
    }

    if (rfkill_id == -1)
    {
        if (init_rfkill())
            return ret;
    }

    fd = open(rfkill_state_path, O_WRONLY);

    if (fd < 0)
    {
        ALOGE("set_bluetooth_power : open(%s) for write failed: %s (%d)",
            rfkill_state_path, strerror(errno), errno);
        return ret;
    }

    sz = write(fd, &buffer, 1);

    if (sz < 0) {
        ALOGE("set_bluetooth_power : write(%s) failed: %s (%d)",
            rfkill_state_path, strerror(errno),errno);
    }
    else
        ret = 0;

    if (fd >= 0)
        close(fd);

    return ret;
}

/*******************************************************************************
**
** Function        userial_to_tcio_baud
**
** Description     helper function converts USERIAL baud rates into TCIO
**                  conforming baud rates
**
** Returns         TRUE/FALSE
**
*******************************************************************************/
static uint8_t userial_to_tcio_baud(uint8_t cfg_baud, uint32_t *baud)
{
    if (cfg_baud == USERIAL_BAUD_115200)
        *baud = B115200;
    else if (cfg_baud == USERIAL_BAUD_4M)
        *baud = B4000000;
    else if (cfg_baud == USERIAL_BAUD_3M)
        *baud = B3000000;
    else if (cfg_baud == USERIAL_BAUD_2M)
        *baud = B2000000;
    else if (cfg_baud == USERIAL_BAUD_1M)
        *baud = B1000000;
    else if (cfg_baud == USERIAL_BAUD_921600)
        *baud = B921600;
    else if (cfg_baud == USERIAL_BAUD_460800)
        *baud = B460800;
    else if (cfg_baud == USERIAL_BAUD_230400)
        *baud = B230400;
    else if (cfg_baud == USERIAL_BAUD_57600)
        *baud = B57600;
    else if (cfg_baud == USERIAL_BAUD_19200)
        *baud = B19200;
    else if (cfg_baud == USERIAL_BAUD_9600)
        *baud = B9600;
    else if (cfg_baud == USERIAL_BAUD_1200)
        *baud = B1200;
    else if (cfg_baud == USERIAL_BAUD_600)
        *baud = B600;
    else
    {
        PR_ERR( "userial vendor open: unsupported baud idx %i", cfg_baud);
        *baud = B115200;
        return FALSE;
    }

    return TRUE;
}

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
/*******************************************************************************
**
** Function        userial_ioctl_init_bt_wake
**
** Description     helper function to set the open state of the bt_wake if ioctl
**                  is used. it should not hurt in the rfkill case but it might
**                  be better to compile it out.
**
** Returns         none
**
*******************************************************************************/
static void userial_ioctl_init_bt_wake(int fd)
{
    uint32_t bt_wake_state;

#if (BT_WAKE_USERIAL_LDISC==TRUE)
    int ldisc = N_BRCM_HCI; /* brcm sleep mode support line discipline */

    /* attempt to load enable discipline driver */
    if (ioctl(vnd_userial.fd, TIOCSETD, &ldisc) < 0)
    {
        PR_INFO("USERIAL_Open():fd %d, TIOCSETD failed: error %d for ldisc: %d",
                      fd, errno, ldisc);
    }
#endif



    /* assert BT_WAKE through ioctl */
    ioctl(fd, USERIAL_IOCTL_BT_WAKE_ASSERT, NULL);
    ioctl(fd, USERIAL_IOCTL_BT_WAKE_GET_ST, &bt_wake_state);
    PR_INFO("userial_ioctl_init_bt_wake read back BT_WAKE state=%i", \
               bt_wake_state);
}
#endif // (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)


/*****************************************************************************
**   Userial Vendor API Functions
*****************************************************************************/

/*******************************************************************************
**
** Function        userial_vendor_init
**
** Description     Initialize userial vendor-specific control block
**
** Returns         None
**
*******************************************************************************/
static void userial_vendor_init(void)
{
    vnd_userial.fd = -1;
    snprintf(vnd_userial.port_name, VND_PORT_NAME_MAXLEN, "%s", \
            BLUETOOTH_UART_DEVICE_PORT);
}

/*******************************************************************************
**
** Function        userial_vendor_open
**
** Description     Open the serial port with the given configuration
**
** Returns         device fd
**
*******************************************************************************/
static int userial_vendor_open(tUSERIAL_CFG *p_cfg)
{
    uint32_t baud;
    uint8_t data_bits;
    uint16_t parity;
    uint8_t stop_bits;

    vnd_userial.fd = -1;

    if (!userial_to_tcio_baud(p_cfg->baud, &baud))
    {
        return -1;
    }

    if(p_cfg->fmt & USERIAL_DATABITS_8)
        data_bits = CS8;
    else if(p_cfg->fmt & USERIAL_DATABITS_7)
        data_bits = CS7;
    else if(p_cfg->fmt & USERIAL_DATABITS_6)
        data_bits = CS6;
    else if(p_cfg->fmt & USERIAL_DATABITS_5)
        data_bits = CS5;
    else
    {
        PR_ERR("userial vendor open: unsupported data bits");
        return -1;
    }

    if(p_cfg->fmt & USERIAL_PARITY_NONE)
        parity = 0;
    else if(p_cfg->fmt & USERIAL_PARITY_EVEN)
        parity = PARENB;
    else if(p_cfg->fmt & USERIAL_PARITY_ODD)
        parity = (PARENB | PARODD);
    else
    {
        PR_ERR("userial vendor open: unsupported parity bit mode");
        return -1;
    }

    if(p_cfg->fmt & USERIAL_STOPBITS_1)
        stop_bits = 0;
    else if(p_cfg->fmt & USERIAL_STOPBITS_2)
        stop_bits = CSTOPB;
    else
    {
        PR_ERR("userial vendor open: unsupported stop bits");
        return -1;
    }

    PR_INFO("userial vendor open: opening %s", vnd_userial.port_name);

    if ((vnd_userial.fd = open(vnd_userial.port_name, O_RDWR)) < 0)
    {
        PR_ERR("userial vendor open: unable to open %s", vnd_userial.port_name);
        return -1;
    }

    PR_INFO("userial vendor open success!!");

    tcflush(vnd_userial.fd, TCIOFLUSH);

    tcgetattr(vnd_userial.fd, &vnd_userial.termios);
    cfmakeraw(&vnd_userial.termios);

	/* Set UART Control Modes */
	vnd_userial.termios.c_cflag |= CLOCAL;
	vnd_userial.termios.c_cflag |= (CRTSCTS | stop_bits| parity);


    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);

    /* set input/output baudrate */
    cfsetospeed(&vnd_userial.termios, baud);
    cfsetispeed(&vnd_userial.termios, baud);
    tcsetattr(vnd_userial.fd, TCSANOW, &vnd_userial.termios);

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
    userial_ioctl_init_bt_wake(vnd_userial.fd);
#endif
	tcflush(vnd_userial.fd, TCIOFLUSH);


    PR_INFO("device fd = %d open", vnd_userial.fd);

    return vnd_userial.fd;
}

/*******************************************************************************
**
** Function        do_write
**
** Description     write
**
** Returns         len
**
*******************************************************************************/
static int do_write(int fd, unsigned char *buf,int len)
{
    int ret = 0;
    int write_offset = 0;
    int write_len = len;
    do {
        ret = write(fd,buf+write_offset,write_len);
        if (ret < 0)
        {
            PR_ERR("%s, write failed ret = %d err = %s",__func__,ret,strerror(errno));
            return -1;
        } else if (ret == 0) {
            PR_ERR("%s, write failed with ret 0 err = %s",__func__,strerror(errno));
            return 0;
        } else {
            if (ret < write_len) {
                PR_INFO("%s, Write pending,do write ret = %d err = %s",__func__,ret,
                       strerror(errno));
                write_len = write_len - ret;
                write_offset = ret;
            } else {
                PR_INFO("Write successful");
                break;
            }
        }
    } while(1);
    return len;
}

/*******************************************************************************
**
** Function        check vendor event
**
** Description     check info
**
** Returns         success return 1
**
*******************************************************************************/
static int check_event(unsigned char * rsp, int size, unsigned char *cmd)
{

#if 0
	int i = 0;
	for(i = 0; i< size; i++)
		PR_INFO("%02x", rsp[i]);
#endif
	if (!(size >= 7))
		return 0;

	if (rsp[4] != cmd[1] || rsp[5] != cmd[2] || rsp[6] != 0X00)
	{
		return 0;
	}
	return 1;
}

/*******************************************************************************
**
** Function        read_vendor_event
**
** Description     read_vendor_event
**
** Returns         str
**
*******************************************************************************/
static int read_vendor_event(int fd, unsigned char* buf, int size)
{
	int remain, r;
	int count = 0;

	if (size <= 0)
		return -1;

    struct pollfd pfd;
    int poll_ret;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLHUP;

    poll_ret = poll(&pfd, 1, 100);

    if (poll_ret <= 0) {
        PR_ERR("%s: receive hci event timeout! ret=%d", __func__, poll_ret);
        return -1;
    }
    PR_INFO("%s: poll ret=%d", __func__, poll_ret);

	while (1) {
		r = read(fd, buf, 1);
		if (r <= 0)
			return -1;
		if (buf[0] == 0x04)
			break;
	}
	count++;

	while (count < 3) {
		r = read(fd, buf + count, 3 - count);
		if (r <= 0)
			return -1;
		count += r;
	}

	if (buf[2] < (size - 3))
		remain = buf[2];
	else
		remain = size - 3;

	while ((count - 3) < remain) {
		r = read(fd, buf + count, remain - (count - 3));
		if (r <= 0)
			return -1;
		count += r;
	}

	return count;
}

/*******************************************************************************
**
** Function        h5_read_vendor_event
**
** Description     h5_read_vendor_event
**
** Returns         count
**
*******************************************************************************/
static int h5_read_vendor_event(int fd, unsigned char* buf, int size)
{
	int r;
	int count = 0;

	if (size <= 0)
		return -1;

    struct pollfd pfd;
    int poll_ret;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLHUP;

    poll_ret = poll(&pfd, 1, 100);

    if (poll_ret <= 0) {
        PR_ERR("%s: receive hci event timeout! ret=%d", __func__, poll_ret);
        return -1;
    }
    PR_INFO("%s: poll ret=%d", __func__, poll_ret);

	while (1) {
		r = read(fd, buf, 1);
		if (r <= 0)
			return -1;
		if (buf[0] == 0xc0)
			break;
	}
	count++;

	while (count < 2) {
		r = read(fd, buf + count, 2 - count);
		if (r <= 0)
			return -1;
		count += r;
	}

	return count;
}

static int h5_send_vendor_cmd(int fd, unsigned char* cmd, int size)
{
	if (do_write(fd, cmd, size) != size)
	{
		PR_ERR("cmd send is error");
		goto error;
	}
	return 0;
error:
	return 1;
}

/*******************************************************************************
**
** Function        hci_vendor_reset
**
** Description     hci_reset
**
** Returns         int
**
*******************************************************************************/
static int start_vendor_cmd(int fd, unsigned char* cmd, int size)
{
	int rsp_size = 0;

	unsigned char rsp[HCI_MAX_EVENT_SIZE];

	memset(rsp, 0x0, HCI_MAX_EVENT_SIZE);
	if (do_write(fd, cmd, size) != size)
	{
		PR_ERR("cmd send is error");
		goto error;
	}

	rsp_size = read_vendor_event(fd, rsp, HCI_MAX_EVENT_SIZE);

	if (rsp_size < 0)
	{
		PR_ERR("%s error",__func__);
		goto error;
	}

	if (!check_event(rsp, rsp_size, cmd))
	{
		PR_ERR("%s rsp event is error", __func__);
		goto error;
	}
	return 0;
error:
	return 1;

}

/*******************************************************************************
**
** Function        get_vendor_info
**
** Description     get_vendor_info
**
** Returns         str
**
*******************************************************************************/
static unsigned char * get_vendor_info(int fd, unsigned char * cmd, int size, unsigned char * event, int *event_size)
{
	int rsp_size = 0;

	unsigned char rsp[HCI_MAX_EVENT_SIZE];
	PR_ERR("%s",__func__);

	memset(rsp, 0x0, HCI_MAX_EVENT_SIZE);
	if (do_write(fd, cmd, size) != size)
	{
		PR_ERR("cmd send is error");
		goto error;
	}

	rsp_size = read_vendor_event(fd, rsp, HCI_MAX_EVENT_SIZE);

	if (rsp_size < 0)
	{
		PR_ERR("get_vendor_info error");
		goto error;
	}

	if (!check_event(rsp, rsp_size, cmd))
	{
		PR_ERR("rsp event is error");
		goto error;
	}

	*event_size = rsp_size;
	memcpy(event, rsp, rsp_size);

	return event;
error:
	return NULL;
}

/*******************************************************************************
**
** Function        matching_vendor_lib
**
** Description     matching_vendor_lib
**
** Returns         0(success) or ~0
**
*******************************************************************************/
static int matching_vendor_lib(unsigned char * buf, int size)
{
	int ret = -1;
	uint16_t vendor_id = 0;

	if (size >= 13)
	{
		vendor_id = (((uint16_t)buf[11])<< 8) | ((uint16_t)buf[12]);
	}
	else
	{
		if(buf[0] == 0xc0 && buf[1] == 0x00)
		{
			vendor_id = BT_VENDOR_ID_REALTECK;
		}
	}

	if (vendor_id == BT_VENDOR_ID_QUALCOMM)
	{
		if (btvendor_hal.pci_module())
		{
			return 0;
		}
		else
		{
			if(enum_uart_type(vendor_id)) {
				return 0;
			}
			else {
				PR_INFO("need add qca module struct");
				goto error;
			}
		}
	}
	else if (enum_uart_type(vendor_id))
	{
		return 0;
	}
	else
	{
		PR_INFO("vendor don't matching");
		goto error;
	}

	return 0;
error:
	return ret;
}

/*******************************************************************************
**
** Function        userial_vendor_close
**
** Description     Conduct vendor-specific close work
**
** Returns         None
**
*******************************************************************************/
static void userial_vendor_close(void)
{
    int result;

    if (vnd_userial.fd == -1)
        return;

#if (BT_WAKE_VIA_USERIAL_IOCTL==TRUE)
    /* de-assert bt_wake BEFORE closing port */
    ioctl(vnd_userial.fd, USERIAL_IOCTL_BT_WAKE_DEASSERT, NULL);
#endif

    PR_INFO("device fd = %d close", vnd_userial.fd);
    // flush Tx before close to make sure no chars in buffer
    tcflush(vnd_userial.fd, TCIOFLUSH);
    if ((result = close(vnd_userial.fd)) < 0)
        PR_ERR( "close(fd:%d) FAILED result:%d", vnd_userial.fd, result);

    vnd_userial.fd = -1;
}

static int init_bt_status(void)
{
	char *str;

	str = get_power_type();
	if (!str)
		return 0;

	if (!strncmp(str, "1", 1)) {
		PR_INFO("BT is powered on separately");
		clr_bten_bit(CLR_BT_POWER_BIT);
	}
	return 0;
}


/*******************************************************************************
**
** Function        bluetooth_distinguish_module
**
** Description     get vendor manufacturer id
**
** Returns         str
**
*******************************************************************************/
static int distinguish_vendoruart_module(void)
{
	int fd;
	unsigned char event[HCI_MAX_EVENT_SIZE];
	int event_size = 0;
	memset(event, 0x0, HCI_MAX_EVENT_SIZE);

	PR_INFO("start H4 init");

	btvendor_hal.userial_init();

	fd = btvendor_hal.userial_open((tUSERIAL_CFG *) &userial_H4_cfg);
	if (fd < 0)
	{
		return 0;
	}

	if(start_vendor_cmd(fd,(unsigned char *)vendor_reset, sizeof(vendor_reset)))
	{
		btvendor_hal.userial_close();
		goto H5;
	}

	if(get_vendor_info(fd, (unsigned char *)vendor_info, sizeof(vendor_info),  event, &event_size) == NULL)
	{
		btvendor_hal.userial_close();
		goto H5;
	}
	if (matching_vendor_lib(event, event_size))
	{
		PR_ERR("matching vendor is fail");
		btvendor_hal.userial_close();
		return 0;
	}
	btvendor_hal.userial_close();
	PR_INFO("H4 matching success");
	return 1;
H5:
	PR_INFO("start H5 init");
	event_size = 0;
	memset(event, 0x0, HCI_MAX_EVENT_SIZE);
#if 0
	upio_set_bluetooth_power(UPIO_BT_POWER_OFF);
	usleep(20000);
	upio_set_bluetooth_power(UPIO_BT_POWER_ON);
	usleep(20000);
#endif
	btvendor_hal.userial_init();
	fd = btvendor_hal.userial_open((tUSERIAL_CFG *) &userial_H5_cfg);
	if (fd < 0)
	{
		return 0;
	}
	if(h5_send_vendor_cmd(fd, (unsigned char *)vendor_sync, sizeof(vendor_sync)))
	{
		btvendor_hal.userial_close();
		return 0;
	}

	event_size = h5_read_vendor_event(fd, event, HCI_MAX_EVENT_SIZE);
	if (event_size < 0)
	{
		PR_ERR("h5_read_vendor_event error ");
		btvendor_hal.userial_close();
		return 0;
	}

	if (matching_vendor_lib(event, event_size))
	{
		PR_ERR("matching vendor is fail");
		btvendor_hal.userial_close();
		return 0;
	}
	btvendor_hal.userial_close();
	PR_INFO("H5 matching success");
	return 1;
}

static int bluetooth_distinguish_module(void)
{
	int cnt = 2;
	/*pcie dou't need go power when uart dou't rsp cmd*/
	if (btvendor_hal.pci_module()) {
		return 1;
	}

	while(cnt) {
		if (cnt == 1) {
			get_product_device();
			PR_INFO("set_wifi_power down");
			set_wifi_power(SDIO_POWER_DOWN);
			PR_INFO("set_wifi_power up");
			set_wifi_power(SDIO_POWER_UP);
		}
		PR_INFO("write_power_type 1");
		write_power_type((char*)"1");
		PR_INFO("upio_set_bluetooth_power on");
		upio_set_bluetooth_power(UPIO_BT_POWER_ON);

		if (btvendor_hal.usb_module()) {
			init_bt_status();
			return 1;
		}
		if (btvendor_hal.mmc_module()) {
			init_bt_status();
			return 1;
		}
		if (btvendor_hal.uart_module()) {
			init_bt_status();
			return 1;
		}
		cnt--;
	}
	return 0;

}

const struct vendor_action btvendor_hal {
	set_power_type,
	get_config,
	userial_vendor_init,
	userial_vendor_open,
	userial_vendor_close,
	get_module_name,
	distinguish_vendoruart_module,
	distinguish_vendorusb_module,
	distinguish_vendormmc_module,
	distinguish_vendorpci_module,
	bluetooth_distinguish_module
};

