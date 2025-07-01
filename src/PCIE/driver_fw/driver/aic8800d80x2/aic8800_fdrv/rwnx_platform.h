/**
 ******************************************************************************
 *
 * @file rwnx_platorm.h
 *
 * Copyright (C) RivieraWaves 2012-2019
 *
 ******************************************************************************
 */

#ifndef _RWNX_PLATFORM_H_
#define _RWNX_PLATFORM_H_

#include <linux/pci.h>
#include "lmac_msg.h"

#define RWNX_CONFIG_FW_NAME             "rwnx_settings.ini"
#define RWNX_PHY_CONFIG_TRD_NAME        "rwnx_trident.ini"
#define RWNX_PHY_CONFIG_KARST_NAME      "rwnx_karst.ini"
#define RWNX_AGC_FW_NAME                "agcram.bin"
#define RWNX_LDPC_RAM_NAME              "ldpcram.bin"
#ifdef CONFIG_RWNX_FULLMAC
#define RWNX_MAC_FW_BASE_NAME           "fmacfw"
#elif defined CONFIG_RWNX_FHOST
#define RWNX_MAC_FW_BASE_NAME           "fhostfw"
#endif /* CONFIG_RWNX_FULLMAC */

#ifdef CONFIG_RWNX_TL4
#define RWNX_MAC_FW_NAME RWNX_MAC_FW_BASE_NAME".hex"
#else
#define RWNX_MAC_FW_NAME  RWNX_MAC_FW_BASE_NAME".ihex"
#define RWNX_MAC_FW_NAME2 RWNX_MAC_FW_BASE_NAME".bin"
#endif

#define RWNX_FCU_FW_NAME                "fcuram.bin"

#define RAM_FMAC_FW_ADDR                   0x00120000
#define FW_RAM_ADID_BASE_ADDR_8800D80_U02  0x00201940
#define FW_RAM_PATCH_BASE_ADDR_8800D80_U02 0x0020B43c

#define RWNX_PCIE_FW_NAME                   "fmacfw_8800D80_pcie.bin"
#define RWNX_PCIE_FW_BT_NAME                "fmacfwbt_8800D80_pcie.bin"
#define RWNX_PCIE_RF_FW_NAME                "lmacfw_rf_pcie.bin"
#define FW_PATCH_BASE_NAME_8800D80_U02      "fw_patch_8800d80_u02.bin"
#define FW_ADID_BASE_NAME_8800D80_U02       "fw_adid_8800d80_u02.bin"
#define FW_PATCH_TABLE_NAME_8800D80_U02     "fw_patch_table_8800d80_u02.bin"

#define RWNX_8800D80X2_PCIE_FW_NAME                   "fmacfw_8800D80X2_pcie.bin"
#define RWNX_8800D80X2_PCIE_FW_BT_NAME                "fmacfwbt_8800D80X2_pcie.bin"
#define RWNX_8800D80X2_PCIE_RF_FW_NAME                "lmacfw_rf_8800D80X2_pcie.bin"
#define FW_PATCH_BASE_NAME_8800D80X2_U03              "fw_patch_8800d80x2_u03.bin"
#define FW_ADID_BASE_NAME_8800D80X2_U03               "fw_adid_8800d80x2_u03.bin"
#define FW_PATCH_TABLE_NAME_8800D80X2_U03             "fw_patch_table_8800d80x2_u03.bin"

/**
 * Type of memory to access (cf rwnx_plat.get_address)
 *
 * @RWNX_ADDR_CPU To access memory of the embedded CPU
 * @RWNX_ADDR_SYSTEM To access memory/registers of one subsystem of the
 * embedded system
 *
 */
enum rwnx_platform_addr {
	RWNX_ADDR_CPU,
	RWNX_ADDR_SYSTEM,
	RWNX_ADDR_MAX,
};

struct rwnx_hw;

/**
 * struct rwnx_plat - Operation pointers for RWNX PCI platform
 *
 * @pci_dev: pointer to pci dev
 * @enabled: Set if embedded platform has been enabled (i.e. fw loaded and
 *          ipc started)
 * @enable: Configure communication with the fw (i.e. configure the transfers
 *         enable and register interrupt)
 * @disable: Stop communication with the fw
 * @deinit: Free all ressources allocated for the embedded platform
 * @get_address: Return the virtual address to access the requested address on
 *              the platform.
 * @ack_irq: Acknowledge the irq at link level.
 * @get_config_reg: Return the list (size + pointer) of registers to restore in
 * order to reload the platform while keeping the current configuration.
 *
 * @priv Private data for the link driver
 */
struct rwnx_plat {
	struct pci_dev *pci_dev;

#ifdef AICWF_SDIO_SUPPORT
	struct aic_sdio_dev *sdiodev;
#endif

#ifdef AICWF_USB_SUPPORT
	struct aic_usb_dev *usbdev;
#endif
#ifdef AICWF_PCIE_SUPPORT
	struct aic_pci_dev *pcidev;
#endif

	bool enabled;

	int (*enable)(struct rwnx_hw *rwnx_hw);
	int (*disable)(struct rwnx_hw *rwnx_hw);
	void (*deinit)(struct rwnx_plat *rwnx_plat);
	u8* (*get_address)(struct rwnx_plat *rwnx_plat, int addr_name,
					   unsigned int offset);
	void (*ack_irq)(struct rwnx_plat *rwnx_plat);
	int (*get_config_reg)(struct rwnx_plat *rwnx_plat, const u32 **list);

	u8 priv[0] __aligned(sizeof(void *));
};

#define RWNX_ADDR(plat, base, offset)           \
	plat->get_address(plat, base, offset)

#define RWNX_REG_READ(plat, base, offset)               \
	readl(plat->get_address(plat, base, offset))

#define RWNX_REG_WRITE(val, plat, base, offset)         \
	writel(val, plat->get_address(plat, base, offset))

extern struct rwnx_plat *g_rwnx_plat;

int rwnx_platform_init(struct rwnx_plat *rwnx_plat, void **platform_data);
void rwnx_platform_deinit(struct rwnx_hw *rwnx_hw);

int rwnx_platform_on(struct rwnx_hw *rwnx_hw, void *config);
void rwnx_platform_off(struct rwnx_hw *rwnx_hw, void **config);

//int rwnx_platform_register_drv(void);
//void rwnx_platform_unregister_drv(void);

void get_userconfig_txpwr_idx(txpwr_idx_conf_t *txpwr_idx);
void get_userconfig_txpwr_ofst(txpwr_ofst_conf_t *txpwr_ofst);
void get_userconfig_xtal_cap(xtal_cap_conf_t *xtal_cap);
s8_l get_txpwr_max(s8_l power);
void set_txpwr_loss_ofst(s8_l value);

void get_userconfig_txpwr_lvl_in_fdrv(txpwr_lvl_conf_t *txpwr_lvl);
void get_userconfig_txpwr_lvl_v2_in_fdrv(txpwr_lvl_conf_v2_t *txpwr_lvl_v2);
void get_userconfig_txpwr_lvl_v3_in_fdrv(txpwr_lvl_conf_v3_t *txpwr_lvl_v3);
void get_userconfig_txpwr_lvl_v4_in_fdrv(txpwr_lvl_conf_v4_t *txpwr_lvl_v4);
void get_userconfig_txpwr_lvl_adj_in_fdrv(txpwr_lvl_adj_conf_t *txpwr_lvl_adj);
void get_userconfig_txpwr_loss(txpwr_loss_conf_t *txpwr_loss);
void get_userconfig_txpwr_ofst_in_fdrv(txpwr_ofst_conf_t *txpwr_ofst);
void get_userconfig_txpwr_ofst2x_in_fdrv(txpwr_ofst2x_conf_t *txpwr_ofst2x);
void get_userconfig_txpwr_ofst2x_v2_in_fdrv(txpwr_ofst2x_conf_v2_t *txpwr_ofst2x_v2);

extern struct device *rwnx_platform_get_dev(struct rwnx_plat *rwnx_plat);

static inline unsigned int rwnx_platform_get_irq(struct rwnx_plat *rwnx_plat)
{
	return rwnx_plat->pci_dev->irq;
}

#ifdef CONFIG_USE_BT
struct aicbt_patch_table {
	char     *name;
	uint32_t type;
	uint32_t *data;
	uint32_t len;
	struct aicbt_patch_table *next;
};

struct aicbt_info_t {
    uint32_t btmode;
    uint32_t btport;
    uint32_t uart_baud;
    uint32_t uart_flowctrl;
    uint32_t lpm_enable;
    uint32_t txpwr_lvl;
};

struct aicbt_patch_info_t {
       uint32_t info_len;
       uint32_t adid_addrinf;
	uint32_t addr_adid;
       uint32_t patch_addrinf;
	uint32_t addr_patch;
	uint32_t reset_addr;
	uint32_t reset_val;
	uint32_t adid_flag_addr;
	uint32_t adid_flag;
};

struct aicbsp_info_t {
    int hwinfo;
    uint32_t cpmode;
};

#define AICBT_PT_TAG          "AICBT_PT_TAG"

enum aicbt_patch_table_type {
	AICBT_PT_INF  = 0x00,
	AICBT_PT_TRAP = 0x1,
	AICBT_PT_B4,
	AICBT_PT_BTMODE,
	AICBT_PT_PWRON,
	AICBT_PT_AF,
	AICBT_PT_VER,
};


enum aicbt_btport_type {
    AICBT_BTPORT_NULL,
    AICBT_BTPORT_MB,
    AICBT_BTPORT_UART,
};

/*  btmode
 * used for force bt mode,if not AICBSP_MODE_NULL
 * efuse valid and vendor_info will be invalid, even has beed set valid
*/
enum aicbt_btmode_type {
    AICBT_BTMODE_BT_ONLY_SW = 0x0,    // bt only mode with switch
    AICBT_BTMODE_BT_WIFI_COMBO,       // wifi/bt combo mode
    AICBT_BTMODE_BT_ONLY,             // bt only mode without switch
    AICBT_BTMODE_BT_ONLY_TEST,        // bt only test mode
    AICBT_BTMODE_BT_WIFI_COMBO_TEST,  // wifi/bt combo test mode
    AICBT_BTMODE_BT_ONLY_COANT,       // bt only mode with no external switch
    AICBT_MODE_NULL = 0xFF,           // invalid value
};

/*  uart_baud
 * used for config uart baud when btport set to uart,
 * otherwise meaningless
*/
enum aicbt_uart_baud_type {
    AICBT_UART_BAUD_115200     = 115200,
    AICBT_UART_BAUD_921600     = 921600,
    AICBT_UART_BAUD_1_5M       = 1500000,
    AICBT_UART_BAUD_3_25M      = 3250000,
};

enum aicbt_uart_flowctrl_type {
    AICBT_UART_FLOWCTRL_DISABLE = 0x0,    // uart without flow ctrl
    AICBT_UART_FLOWCTRL_ENABLE,           // uart with flow ctrl
};

enum aicbsp_cpmode_type {
    AICBSP_CPMODE_WORK,
    AICBSP_CPMODE_TEST,
};
#define AIC_M2D_OTA_INFO_ADDR       0x88000020
#define AIC_M2D_OTA_DATA_ADDR       0x88000040
#define AIC_M2D_OTA_FLASH_ADDR      0x08004000
#define AIC_M2D_OTA_CODE_START_ADDR 0x08004188
#define AIC_M2D_OTA_VER_ADDR        0x0800418c
///aic bt tx pwr lvl :lsb->msb: first byte, min pwr lvl; second byte, max pwr lvl;
///pwr lvl:20(min), 30 , 40 , 50 , 60(max)
#define AICBT_TXPWR_LVL            0x00006020
#define AICBT_TXPWR_LVL_D80        0x00006F2F

#define AICBSP_MODE_BT_HCI_MODE_NULL              0
#define AICBSP_MODE_BT_HCI_MODE_MB                1
#define AICBSP_MODE_BT_HCI_MODE_UART              2

#define AICBSP_HWINFO_DEFAULT       (-1)
#define AICBSP_CPMODE_DEFAULT       AICBSP_CPMODE_WORK

#define AICBT_BTMODE_DEFAULT        AICBT_BTMODE_BT_ONLY_COANT
#ifdef CONFIG_USB_BT
#define AICBT_BTPORT_DEFAULT        AICBT_BTPORT_MB
#else
#define AICBT_BTPORT_DEFAULT        AICBT_BTPORT_UART
#endif
#define AICBT_UART_BAUD_DEFAULT     AICBT_UART_BAUD_1_5M
#define AICBT_UART_FC_DEFAULT       AICBT_UART_FLOWCTRL_ENABLE
#define AICBT_LPM_ENABLE_DEFAULT    0
#define AICBT_TXPWR_LVL_DEFAULT     AICBT_TXPWR_LVL


struct bt_patch_file_name{
	const char *fw_adid;
	const char *fw_patch;
	const char *fw_patch_table;
};

int aicbt_patch_table_free(struct aicbt_patch_table *head);
struct aicbt_patch_table *aicbt_patch_table_alloc(struct rwnx_hw *rwnx_hw, const char *filename);
int aicbt_patch_table_load(struct rwnx_hw *rwnx_hw, struct aicbt_patch_table *_head);
int aicbt_patch_info_unpack(struct aicbt_patch_info_t *patch_info, struct aicbt_patch_table *head_t);
int rwnx_plat_bin_fw_patch_table_upload_android(struct rwnx_hw *rwnx_hw, char *filename);
int patch_config(struct rwnx_hw *rwnx_hw);
int pcie_reset_firmware(struct rwnx_hw *rwnx_hw, u32 fw_addr);

#endif

#endif /* _RWNX_PLATFORM_H_ */
