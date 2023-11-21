#include <linux/version.h>
#include "aicbluetooth_cmds.h"
#include "aicwf_usb.h"
#include "md5.h"



//Parser state
#define INIT 0
#define CMD 1
#define PRINT 2
#define GET_VALUE 3

typedef struct
{
    uint8_t enable;
    uint8_t dsss;
    uint8_t ofdmlowrate_2g4;
    uint8_t ofdmhighrate_2g4;
    uint8_t ofdm1024qam_2g4;
    uint8_t ofdmlowrate_5g;
    uint8_t ofdmhighrate_5g;
    uint8_t ofdm1024qam_5g;
} txpwr_idx_conf_t;


txpwr_idx_conf_t nvram_txpwr_idx = {
	.enable = 1,
	.dsss = 9,
	.ofdmlowrate_2g4 = 8,
	.ofdmhighrate_2g4 = 8,
	.ofdm1024qam_2g4 = 8,
	.ofdmlowrate_5g = 11,
	.ofdmhighrate_5g = 10,
	.ofdm1024qam_5g = 9
};

struct aicbt_patch_table {
	char     *name;
	uint32_t type;
	uint32_t *data;
	uint32_t len;
	struct aicbt_patch_table *next;
};

#define AICBT_PT_TAG          "AICBT_PT_TAG"
#define AICBT_PT_TRAP         0x01
#define AICBT_PT_B4           0x02
#define AICBT_PT_BTMODE       0x03
#define AICBT_PT_PWRON        0x04
#define AICBT_PT_AF           0x05

#define AICBSP_MODE_BT_ONLY_SW      0 // bt only mode, with switch
#define AICBSP_MODE_BT_WIFI_COMBO   1 // combo mode
#define AICBSP_MODE_BT_ONLY         2 // bt only mode
#define AICBSP_MODE_BTONLY_TEST     3 // bt only test mode
#define AICBSP_MODE_COMBO_TEST      4 // combo test mode

#define AICBSP_MODE_BT_HCI_MODE_NULL              0
#define AICBSP_MODE_BT_HCI_MODE_MB                1
#define AICBSP_MODE_BT_HCI_MODE_UART              2

uint32_t aicbsp_mode_index = AICBSP_MODE_BT_ONLY;
uint32_t aicbsp_bthcimode_index = AICBSP_MODE_BT_HCI_MODE_MB;

#define FW_PATH_MAX 200
#if defined(CONFIG_PLATFORM_UBUNTU)
static const char* aic_default_fw_path = "/lib/firmware/aic8800";
#else
static const char* aic_default_fw_path = "/vendor/etc/firmware";
#endif
char aic_fw_path[FW_PATH_MAX];
module_param_string(aic_fw_path, aic_fw_path, FW_PATH_MAX, 0660);


int aic_bt_platform_init(struct aic_usb_dev *usbdev)
{
    rwnx_cmd_mgr_init(&usbdev->cmd_mgr);
    usbdev->cmd_mgr.usbdev = (void *)usbdev;
    return 0;

}

void aic_bt_platform_deinit(struct aic_usb_dev *usbdev)
{

}

#define MD5(x) x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15]
#define MD5PINRT "file md5:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n"

static int aic_load_firmware(u32 ** fw_buf, const char *name, struct device *device)
{
    void *buffer=NULL;
    char *path=NULL;
    struct file *fp=NULL;
    int size = 0, len=0, i=0;
    ssize_t rdlen=0;
    u32 *src=NULL, *dst = NULL;
	MD5_CTX md5;
	unsigned char decrypt[16];

    /* get the firmware path */
    path = __getname();
    if (!path){
            *fw_buf=NULL;
            return -1;
    }

    if (strlen(aic_fw_path) > 0) {
		printk("%s: use customer define fw_path\n", __func__);
		len = snprintf(path, FW_PATH_MAX, "%s/%s", aic_fw_path, name);
    } else {
		len = snprintf(path, FW_PATH_MAX, "%s/%s",aic_default_fw_path, name);
    }

    if (len >= FW_PATH_MAX) {
    	printk("%s: %s file's path too long\n", __func__, name);
        *fw_buf=NULL;
        __putname(path);
        return -1;
    }

    printk("%s :firmware path = %s  \n", __func__ ,path);


    /* open the firmware file */
    fp=filp_open(path, O_RDONLY, 0);
    if(IS_ERR(fp) || (!fp)){
            printk("%s: %s file failed to open\n", __func__, name);
            if(IS_ERR(fp))
		printk("is_Err\n");
	if((!fp))
		printk("null\n");
	*fw_buf=NULL;
            __putname(path);
            fp=NULL;
            return -1;
    }

    size = i_size_read(file_inode(fp));
    if(size<=0){
            printk("%s: %s file size invalid %d\n", __func__, name, size);
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            return -1;
}

    /* start to read from firmware file */
    buffer = vmalloc(size);
    memset(buffer, 0, size);
    if(!buffer){
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            return -1;
    }


    #if LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 16)
    rdlen = kernel_read(fp, buffer, size, &fp->f_pos);
    #else
    rdlen = kernel_read(fp, fp->f_pos, buffer, size);
    #endif

    if(size != rdlen){
            printk("%s: %s file rdlen invalid %d %d\n", __func__, name, (int)rdlen, size);
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            vfree(buffer);
            buffer=NULL;
            return -1;
    }
    if(rdlen > 0){
            fp->f_pos += rdlen;
            printk("f_pos=%d\n", (int)fp->f_pos);
    }


   /*start to transform the data format*/
    src = (u32*)buffer;
    printk("malloc dst\n");
    dst = (u32*)vmalloc(size);
    memset(dst, 0, size);

    if(!dst){
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            vfree(buffer);
            buffer=NULL;
            return -1;
    }

    for(i=0;i<(size/4);i++){
            dst[i] = src[i];
    }

    __putname(path);
    filp_close(fp,NULL);
    fp=NULL;
    vfree(buffer);
    buffer=NULL;
    *fw_buf = dst;

	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *)dst, size);
	MD5Final(&md5, decrypt);

	printk(MD5PINRT, MD5(decrypt));


    return size;

}

int rwnx_plat_bin_fw_upload_android(struct aic_usb_dev *usbdev, u32 fw_addr,
                               char *filename)
{
    struct device *dev = usbdev->dev;
    unsigned int i=0;
    int size;
    u32 *dst=NULL;
    int err=0;

    /* load aic firmware */
    size = aic_load_firmware(&dst, filename, dev);
    if(size<=0){
            printk("wrong size of firmware file\n");
            vfree(dst);
            dst = NULL;
            return -1;
    }

    /* Copy the file on the Embedded side */
    printk("\n### Upload %s firmware, @ = %x  size=%d\n", filename, fw_addr, size);

    if (size > 1024) {// > 1KB data
        for (i = 0; i < (size - 1024); i += 1024) {//each time write 1KB
            err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, 1024, dst + i / 4);
                if (err) {
                printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
                break;
            }
        }
    }

    if (!err && (i < size)) {// <1KB data
        err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, size - i, dst + i / 4);
        if (err) {
            printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
        }
    }

    if (dst) {
        vfree(dst);
        dst = NULL;
    }

    printk("fw download complete\n");

    return err;
}


uint8_t rwnx_atoi(char *value){
	int len = 0;
	int i = 0;
	int result = 0;

	len = strlen(value);

	for(i = 0;i < len ;i++){
		result = result * 10;
		if(value[i] >= 48 && value[i] <= 57){
			result += value[i] - 48;
		}else{
			result = 0;
			break;
		}
	}

	return result;
}

void get_userconfig_param(txpwr_idx_conf_t *txpwr_idx){
	txpwr_idx->enable = nvram_txpwr_idx.enable;
	txpwr_idx->dsss = nvram_txpwr_idx.dsss;
	txpwr_idx->ofdmlowrate_2g4 = nvram_txpwr_idx.ofdmlowrate_2g4;
	txpwr_idx->ofdmhighrate_2g4 = nvram_txpwr_idx.ofdmhighrate_2g4;
	txpwr_idx->ofdm1024qam_2g4 = nvram_txpwr_idx.ofdm1024qam_2g4;
	txpwr_idx->ofdmlowrate_5g = nvram_txpwr_idx.ofdmlowrate_5g;
	txpwr_idx->ofdmhighrate_5g = nvram_txpwr_idx.ofdmhighrate_5g;
	txpwr_idx->ofdm1024qam_5g = nvram_txpwr_idx.ofdm1024qam_5g;

	printk("%s:enable:%d\r\n", __func__, txpwr_idx->enable);
	printk("%s:dsss:%d\r\n", __func__, txpwr_idx->dsss);
	printk("%s:ofdmlowrate_2g4:%d\r\n", __func__, txpwr_idx->ofdmlowrate_2g4);
	printk("%s:ofdmhighrate_2g4:%d\r\n", __func__, txpwr_idx->ofdmhighrate_2g4);
	printk("%s:ofdm1024qam_2g4:%d\r\n", __func__, txpwr_idx->ofdm1024qam_2g4);
	printk("%s:ofdmlowrate_5g:%d\r\n", __func__, txpwr_idx->ofdmlowrate_5g);
	printk("%s:ofdmhighrate_5g:%d\r\n", __func__, txpwr_idx->ofdmhighrate_5g);
	printk("%s:ofdm1024qam_5g:%d\r\n", __func__, txpwr_idx->ofdm1024qam_5g);

}

EXPORT_SYMBOL(get_userconfig_param);


void rwnx_plat_nvram_set_value(char *command, char *value){
	//TODO send command
	printk("%s:command=%s value=%s", __func__, command, value);
	if(!strcmp(command, "enable")){
		nvram_txpwr_idx.enable = rwnx_atoi(value);
	}else if(!strcmp(command, "dsss")){
		nvram_txpwr_idx.dsss = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdmlowrate_2g4")){
		nvram_txpwr_idx.ofdmlowrate_2g4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdmhighrate_2g4")){
		nvram_txpwr_idx.ofdmhighrate_2g4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm1024qam_2g4")){
		nvram_txpwr_idx.ofdm1024qam_2g4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdmlowrate_5g")){
		nvram_txpwr_idx.ofdmlowrate_5g = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdmhighrate_5g")){
		nvram_txpwr_idx.ofdmhighrate_5g = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm1024qam_5g")){
		nvram_txpwr_idx.ofdm1024qam_5g = rwnx_atoi(value);
	}
}

void rwnx_plat_userconfig_parsing(char *buffer, int size){
    int i = 0;
	int parse_state = 0;
	char command[30];
	char value[100];
	int char_counter = 0;

	memset(command, 0, 30);
	memset(value, 0, 100);

    for(i = 0; i < size; i++){

		//Send command or print nvram log when char is \r or \n
		if(buffer[i] == 0x0a || buffer[i] == 0x0d){
			if(command[0] != 0 && value[0] != 0){
				if(parse_state == PRINT){
					printk("%s:%s\r\n", __func__, value);
				}else if(parse_state == GET_VALUE){
					rwnx_plat_nvram_set_value(command, value);
				}
			}
			//Reset command value and char_counter
			memset(command, 0, 30);
			memset(value, 0, 100);
			char_counter = 0;
			parse_state = INIT;
			continue;
		}

		//Switch parser state
		if(parse_state == INIT){
			if(buffer[i] == '#'){
				parse_state = PRINT;
				continue;
			}else if(buffer[i] == 0x0a || buffer[i] == 0x0d){
				parse_state = INIT;
				continue;
			}else{
				parse_state = CMD;
			}
		}

		//Fill data to command and value
		if(parse_state == PRINT){
			command[0] = 0x01;
			value[char_counter] = buffer[i];
			char_counter++;
		}else if(parse_state == CMD){
			if(command[0] != 0 && buffer[i] == '='){
				parse_state = GET_VALUE;
				char_counter = 0;
				continue;
			}
			command[char_counter] = buffer[i];
			char_counter++;
		}else if(parse_state == GET_VALUE){
			value[char_counter] = buffer[i];
			char_counter++;
		}
	}


}

int rwnx_plat_userconfig_upload_android(char *filename){
    int size;
    u32 *dst=NULL;

	printk("userconfig file path:%s \r\n", filename);

    /* load aic firmware */
    size = aic_load_firmware(&dst, filename, NULL);
    if(size <= 0){
            printk("wrong size of firmware file\n");
            vfree(dst);
            dst = NULL;
            return 0;
    }

	rwnx_plat_userconfig_parsing((char *)dst, size);

	if (dst) {
        vfree(dst);
        dst = NULL;
    }

	return 0;
}



int aicbt_patch_table_free(struct aicbt_patch_table **head)
{
	struct aicbt_patch_table *p = *head, *n = NULL;
	while (p) {
		n = p->next;
		vfree(p->name);
		vfree(p->data);
		vfree(p);
		p = n;
	}
	*head = NULL;
	return 0;
}
int aicbt_patch_table_load(struct aic_usb_dev *usbdev, struct aicbt_patch_table *_head)
{
	struct aicbt_patch_table *head, *p;
	int ret = 0, i;
	uint32_t *data = NULL;

	head = _head;
	for (p = head; p != NULL; p = p->next) {
		data = p->data;
		if(AICBT_PT_BTMODE == p->type){
			*(data + 1) = aicbsp_mode_index;
			*(data + 3) = aicbsp_bthcimode_index;
		}
		for (i = 0; i < p->len; i++) {
			ret = rwnx_send_dbg_mem_write_req(usbdev, *data, *(data + 1));
			if (ret != 0)
				return ret;
			data += 2;
		}
		if (p->type == AICBT_PT_PWRON)
			udelay(500);
	}
	aicbt_patch_table_free(&head);
	return 0;
}


int rwnx_plat_bin_fw_patch_table_upload_android(struct aic_usb_dev *usbdev, char *filename){
    struct device *dev = usbdev->dev;
	struct aicbt_patch_table *head = NULL;
	struct aicbt_patch_table *new = NULL;
	struct aicbt_patch_table *cur = NULL;
   	 int size;
	int ret = 0;
   	uint8_t *rawdata=NULL;
	uint8_t *p = NULL;

    /* load aic firmware */
    size = aic_load_firmware((u32 **)&rawdata, filename, dev);
	if (size <= 0) {
		printk("wrong size of firmware file\n");
		ret = -1;
		goto err;
	}

	p = rawdata;

	if (memcmp(p, AICBT_PT_TAG, sizeof(AICBT_PT_TAG) < 16 ? sizeof(AICBT_PT_TAG) : 16)) {
		printk("TAG err\n");
		ret = -1;
		goto err;
	}
	p += 16;

	while (p - rawdata < size) {
		//printk("size = %d  p - rawdata = %d \r\n", size, p - rawdata);
		new = (struct aicbt_patch_table *)vmalloc(sizeof(struct aicbt_patch_table));
		memset(new, 0, sizeof(struct aicbt_patch_table));
		if (head == NULL) {
			head = new;
			cur  = new;
		} else {
			cur->next = new;
			cur = cur->next;
		}

		cur->name = (char *)vmalloc(sizeof(char) * 16);
		memset(cur->name, 0, sizeof(char) * 16);
		memcpy(cur->name, p, 16);
		p += 16;

		cur->type = *(uint32_t *)p;
		p += 4;

		cur->len = *(uint32_t *)p;
		p += 4;

		if((cur->type )  >= 1000 ) {//Temp Workaround
			cur->len = 0;
		}else{
			cur->data = (uint32_t *)vmalloc(sizeof(uint8_t) * cur->len * 8);
			memset(cur->data, 0, sizeof(uint8_t) * cur->len * 8);
			memcpy(cur->data, p, cur->len * 8);
			p += cur->len * 8;
		}
	}

	vfree(rawdata);
	aicbt_patch_table_load(usbdev, head);
	return ret;
err:
	//aicbt_patch_table_free(&head);

	if (rawdata){
		vfree(rawdata);
	}
	return ret;
}


