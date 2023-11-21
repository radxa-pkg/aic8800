#include <linux/version.h>
#include "aicbluetooth_cmds.h"
#include "aicwf_usb.h"

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



int aic_bt_platform_init(struct aic_usb_dev *usbdev)
{
    rwnx_cmd_mgr_init(&usbdev->cmd_mgr);
    usbdev->cmd_mgr.usbdev = (void *)usbdev;
    return 0;

}

void aic_bt_platform_deinit(struct aic_usb_dev *usbdev)
{

}

static const char* aic_bt_path = "/vendor/etc/firmware";
#define FW_PATH_MAX 200
char aic_fw_path[FW_PATH_MAX];
module_param_string(aic_fw_path, aic_fw_path, FW_PATH_MAX, 0660);

#if defined(CONFIG_PLATFORM_UBUNTU)
int rwnx_plat_bin_fw_upload_pc(struct aic_usb_dev *usbdev, u32 fw_addr,
                               char *filename)
{
    const struct firmware *fw;
    struct device *dev = usbdev->dev;
    int err = 0;
    unsigned int i, size;
    u32 *src, *dst;

    err = request_firmware(&fw, filename, dev);
    if (err) {
        printk("request_firmware fail: %d\r\n",err);
        return err;
    }

    /* Copy the file on the Embedded side */
    printk("\n### Upload %s firmware, @ = %x\n", filename, fw_addr);

    size = (unsigned int)fw->size;
    src = (u32 *)fw->data;
    dst = (u32 *)kzalloc(size, GFP_KERNEL);
    if (dst == NULL) {
        printk(KERN_CRIT "%s: data allocation failed\n", __func__);
        return -2;
    }
    //printk("src:%p,dst:%p,size:%d\r\n",src,dst,size);
    for (i = 0; i < (size / 4); i++) {
        dst[i] = src[i];
    }

    if (size > 1024) {
        for (i = 0; i < (size - 1024); i += 1024) {
            //printk("wr blk 0: %p -> %x\r\n", dst + i / 4, fw_addr + i);
            err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, 1024, dst + i / 4);
            if (err) {
                printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
                break;
            }
        }
    }
    if (!err && (i < size)) {
        //printk("wr blk 1: %p -> %x\r\n", dst + i / 4, fw_addr + i);
        err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, size - i, dst + i / 4);
        if (err) {
            printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
        }
    }
    if (dst) {
        kfree(dst);
        dst = NULL;
    }

    release_firmware(fw);

    return err;
}

#else

//static const char* aic_bt_path = "/vendor/etc/firmware";
//#define FW_PATH_MAX 200

static int aic_load_firmware(u32 ** fw_buf, const char *name, struct device *device)
{
    void *buffer=NULL;
    char *path=NULL;
    struct file *fp=NULL;
    int size = 0, len=0, i=0;
    ssize_t rdlen=0;
    u32 *src=NULL, *dst = NULL;

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
	len = snprintf(path, FW_PATH_MAX, "%s/%s",aic_bt_path, name);
    }

    len = snprintf(path, FW_PATH_MAX, "%s/%s",aic_bt_path, name);
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
    buffer = kzalloc(size, GFP_KERNEL);
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
            kfree(buffer);
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
    dst = (u32*)kzalloc(size,GFP_KERNEL);

    if(!dst){
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            kfree(buffer);
            buffer=NULL;
            return -1;
    }

    for(i=0;i<(size/4);i++){
            dst[i] = src[i];
    }

    __putname(path);
    filp_close(fp,NULL);
    fp=NULL;
    kfree(buffer);
    buffer=NULL;
    *fw_buf = dst;

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
            kfree(dst);
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
        kfree(dst);
        dst = NULL;
    }

    printk("fw download complete\n");

    return err;
}
#endif


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

void get_nvram_param(txpwr_idx_conf_t *txpwr_idx){
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

EXPORT_SYMBOL(get_nvram_param);


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

void rwnx_plat_nvram_parsing(char *buffer){
    int i = 0;
	int parse_state = 0;
	char command[30];
	char value[100];
	int char_counter = 0;

	memset(command, 0, 30);
	memset(value, 0, 100);

    for(i = 0; i < strlen(buffer); i++){

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

int rwnx_plat_nvram_upload_android(struct aic_usb_dev *usbdev, char *filename){
    int err = 0;
	struct file *fp = NULL;
	char *path = NULL;
	int len = 0;
	int size = 0;
	char *buffer = NULL;
	int rdlen = 0;

	printk("nvram setting...\r\n");

    /* get the firmware path */
    path = __getname();
    if (!path){
		    printk("%s: __getname fail \n", __func__);
            return 0;
    }
	
	if(strlen(aic_fw_path) > 0){		
		len = snprintf(path, FW_PATH_MAX, "%s/%s", aic_fw_path, filename);	
	}else{
    	len = snprintf(path, FW_PATH_MAX, "%s/%s", aic_bt_path, filename);
	}

    printk("nvram file path:%s \r\n", path);
	
    /* open the nvram file */
    fp = filp_open(path, O_RDONLY, 0);
	
    if(IS_ERR(fp) || (!fp)){
            printk("%s: %s file failed to open\n", __func__, filename);
            if(IS_ERR(fp)){
				printk("is_Err\n");
            }
	    	if((!fp)){
				printk("null\n");
	    	}
        __putname(path);
        fp = NULL;
        return 0;
    }

    size = i_size_read(file_inode(fp));
    if(size <= 0){
            printk("%s: %s file size invalid %d\n", __func__, filename, size);
            __putname(path);
            filp_close(fp, NULL);
            fp = NULL;
            return 0;
	}
	
    /* start to read from nvram file */
    buffer = (char*)vmalloc(size);
	memset(buffer, 0, size);
	
    if(!buffer){
		    printk("%s: buffer can't allow memory \r\n", __func__);
            __putname(path);
            filp_close(fp, NULL);
            fp = NULL;
            return 0;
    }

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 16)
	rdlen = kernel_read(fp, buffer, size, &fp->f_pos);
#else
	rdlen = kernel_read(fp, fp->f_pos, buffer, size);
#endif 

	if(size != rdlen){
			printk("%s: %s file rdlen invalid %d %d\n", __func__, filename, (int)rdlen, size);
			__putname(path);
			filp_close(fp,NULL);
			fp = NULL;
			vfree(buffer);
			buffer = NULL;
			return 0;
	}
	if(rdlen > 0){
			fp->f_pos += rdlen;
			printk("f_pos=%d\n", (int)fp->f_pos);
	}

    rwnx_plat_nvram_parsing(buffer);

	__putname(path);
	filp_close(fp, NULL);
	vfree(buffer);

	printk("nvram setting complete\n");
    return err;
}



