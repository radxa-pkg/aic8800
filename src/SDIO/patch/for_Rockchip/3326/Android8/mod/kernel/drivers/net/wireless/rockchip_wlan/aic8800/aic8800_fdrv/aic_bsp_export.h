#ifndef __AIC_BSP_EXPORT_H
#define __AIC_BSP_EXPORT_H

enum aicbsp_subsys {
	AIC_BLUETOOTH,
	AIC_WIFI,
};

enum aicbsp_pwr_state {
	AIC_PWR_OFF,
	AIC_PWR_ON,
};

struct aicbsp_feature_t {
	int      hwinfo;
	uint32_t sdio_clock;
	uint8_t  sdio_phase;
    int fwlog_en;
};

enum skb_buff_id {
	AIC_RESV_MEM_TXDATA,
};

int aicbsp_set_subsys(int, int);
int aicbsp_get_feature(struct aicbsp_feature_t *feature, char *fw_path);
bool aicbsp_get_load_fw_in_fdrv(void);
struct sk_buff *aicbsp_resv_mem_alloc_skb(unsigned int length, uint32_t id);
void aicbsp_resv_mem_kfree_skb(struct sk_buff *skb, uint32_t id);

#endif
