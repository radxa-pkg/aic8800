/**
 ******************************************************************************
 *
 * @file rwnx_platform.c
 *
 * Copyright (C) RivieraWaves 2012-2019
 *
 ******************************************************************************
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/delay.h>

#include "rwnx_platform.h"
#include "reg_access.h"
#include "hal_desc.h"
#include "rwnx_main.h"
#include "rwnx_pci.h"
#ifndef CONFIG_RWNX_FHOST
#include "ipc_host.h"
#endif /* !CONFIG_RWNX_FHOST */
#include "rwnx_msg_tx.h"

#ifdef AICWF_SDIO_SUPPORT
#include "aicwf_sdio.h"
#endif

#ifdef AICWF_USB_SUPPORT
#include "aicwf_usb.h"
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

struct rwnx_plat *g_rwnx_plat;

#ifdef CONFIG_RWNX_TL4
/**
 * rwnx_plat_tl4_fw_upload() - Load the requested FW into embedded side.
 *
 * @rwnx_plat: pointer to platform structure
 * @fw_addr: Virtual address where the fw must be loaded
 * @filename: Name of the fw.
 *
 * Load a fw, stored as a hex file, into the specified address
 */
static int rwnx_plat_tl4_fw_upload(struct rwnx_plat *rwnx_plat, u8 *fw_addr,
								   char *filename)
{
	struct device *dev = rwnx_platform_get_dev(rwnx_plat);
	const struct firmware *fw;
	int err = 0;
	u32 *dst;
	u8 const *file_data;
	char typ0, typ1;
	u32 addr0, addr1;
	u32 dat0, dat1;
	int remain;

	err = request_firmware(&fw, filename, dev);
	if (err) {
		return err;
	}
	file_data = fw->data;
	remain = fw->size;

	/* Copy the file on the Embedded side */
	dev_dbg(dev, "\n### Now copy %s firmware, @ = %p\n", filename, fw_addr);

	/* Walk through all the lines of the configuration file */
	while (remain >= 16) {
		u32 data, offset;

		if (sscanf(file_data, "%c:%08X %04X", &typ0, &addr0, &dat0) != 3)
			break;
		if ((addr0 & 0x01) != 0) {
			addr0 = addr0 - 1;
			dat0 = 0;
		} else {
			file_data += 16;
			remain -= 16;
		}
		if ((remain < 16) ||
			(sscanf(file_data, "%c:%08X %04X", &typ1, &addr1, &dat1) != 3) ||
			(typ1 != typ0) || (addr1 != (addr0 + 1))) {
			typ1 = typ0;
			addr1 = addr0 + 1;
			dat1 = 0;
		} else {
			file_data += 16;
			remain -= 16;
		}

		if (typ0 == 'C') {
			offset = 0x00200000;
			if ((addr1 % 4) == 3)
				offset += 2*(addr1 - 3);
			else
				offset += 2*(addr1 + 1);

			data = dat1 | (dat0 << 16);
		} else {
			offset = 2*(addr1 - 1);
			data = dat0 | (dat1 << 16);
		}
		dst = (u32 *)(fw_addr + offset);
		*dst = data;
	}

	release_firmware(fw);

	return err;
}
#endif

#if 0
/**
 * rwnx_plat_bin_fw_upload() - Load the requested binary FW into embedded side.
 *
 * @rwnx_plat: pointer to platform structure
 * @fw_addr: Virtual address where the fw must be loaded
 * @filename: Name of the fw.
 *
 * Load a fw, stored as a binary file, into the specified address
 */
static int rwnx_plat_bin_fw_upload(struct rwnx_plat *rwnx_plat, u8 *fw_addr,
							   char *filename)
{
	const struct firmware *fw;
	struct device *dev = rwnx_platform_get_dev(rwnx_plat);
	int err = 0;
	unsigned int i, size;
	u32 *src, *dst;

	err = request_firmware(&fw, filename, dev);
	if (err) {
		return err;
	}

	/* Copy the file on the Embedded side */
	dev_dbg(dev, "\n### Now copy %s firmware, @ = %p\n", filename, fw_addr);

	src = (u32 *)fw->data;
	dst = (u32 *)fw_addr;
	size = (unsigned int)fw->size;

	/* check potential platform bug on multiple stores vs memcpy */
	for (i = 0; i < size; i += 4) {
		*dst++ = *src++;
	}

	release_firmware(fw);

	return err;
}
#endif

#ifndef CONFIG_ROM_PATCH_EN
#if 0//!defined(CONFIG_NANOPI_M4) && !defined(CONFIG_PLATFORM_ALLWINNER)
/**
 * rwnx_plat_bin_fw_upload_2() - Load the requested binary FW into embedded side.
 *
 * @rwnx_hw: Main driver data
 * @fw_addr: Address where the fw must be loaded
 * @filename: Name of the fw.
 *
 * Load a fw, stored as a binary file, into the specified address
 */
static int rwnx_plat_bin_fw_upload_2(struct rwnx_hw *rwnx_hw, u32 fw_addr,
							   char *filename)
{
	const struct firmware *fw;
	struct device *dev = rwnx_platform_get_dev(rwnx_hw->plat);
	int err = 0;
	unsigned int i, size;
	u32 *src, *dst;

	err = request_firmware(&fw, filename, dev);
	if (err) {
		printk("request_firmware fail: %d\r\n", err);
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
			err = rwnx_send_dbg_mem_block_write_req(rwnx_hw, fw_addr + i, 1024, dst + i / 4);
			if (err) {
				printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
				break;
			}
		}
	}
	if (!err && (i < size)) {
		//printk("wr blk 1: %p -> %x\r\n", dst + i / 4, fw_addr + i);
		err = rwnx_send_dbg_mem_block_write_req(rwnx_hw, fw_addr + i, size - i, dst + i / 4);
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
#endif
#endif /* !CONFIG_ROM_PATCH_EN */

#if 0
#ifndef CONFIG_RWNX_TL4
#define IHEX_REC_DATA           0
#define IHEX_REC_EOF            1
#define IHEX_REC_EXT_SEG_ADD    2
#define IHEX_REC_START_SEG_ADD  3
#define IHEX_REC_EXT_LIN_ADD    4
#define IHEX_REC_START_LIN_ADD  5

/**
 * rwnx_plat_ihex_fw_upload() - Load the requested intel hex 8 FW into embedded side.
 *
 * @rwnx_plat: pointer to platform structure
 * @fw_addr: Virtual address where the fw must be loaded
 * @filename: Name of the fw.
 *
 * Load a fw, stored as a ihex file, into the specified address.
 */
static int rwnx_plat_ihex_fw_upload(struct rwnx_plat *rwnx_plat, u8 *fw_addr,
									char *filename)
{
	const struct firmware *fw;
	struct device *dev = rwnx_platform_get_dev(rwnx_plat);
	u8 const *src, *end;
	u32 *dst;
	u16 haddr, segaddr, addr;
	u32 hwaddr;
	u8 load_fw, byte_count, checksum, csum, rec_type;
	int err, rec_idx;
	char hex_buff[9];

	err = request_firmware(&fw, filename, dev);
	if (err) {
		return err;
	}

	/* Copy the file on the Embedded side */
	dev_dbg(dev, "\n### Now copy %s firmware, @ = %p\n", filename, fw_addr);

	src = fw->data;
	end = src + (unsigned int)fw->size;
	haddr = 0;
	segaddr = 0;
	load_fw = 1;
	err = -EINVAL;
	rec_idx = 0;
	hwaddr = 0;

#define IHEX_READ8(_val, _cs) {                  \
		hex_buff[2] = 0;                         \
		strncpy(hex_buff, src, 2);               \
		if (kstrtou8(hex_buff, 16, &_val))       \
			goto end;                            \
		src += 2;                                \
		if (_cs)                                 \
			csum += _val;                        \
	}

#define IHEX_READ16(_val) {                        \
		hex_buff[4] = 0;                           \
		strncpy(hex_buff, src, 4);                 \
		if (kstrtou16(hex_buff, 16, &_val))        \
			goto end;                              \
		src += 4;                                  \
		csum += (_val & 0xff) + (_val >> 8);       \
	}

#define IHEX_READ32(_val) {                              \
		hex_buff[8] = 0;                                 \
		strncpy(hex_buff, src, 8);                       \
		if (kstrtouint(hex_buff, 16, &_val))             \
			goto end;                                    \
		src += 8;                                        \
		csum += (_val & 0xff) + ((_val >> 8) & 0xff) +   \
			((_val >> 16) & 0xff) + (_val >> 24);        \
	}

#define IHEX_READ32_PAD(_val, _nb) {                    \
		memset(hex_buff, '0', 8);                       \
		hex_buff[8] = 0;                                \
		strncpy(hex_buff, src, (2 * _nb));              \
		if (kstrtouint(hex_buff, 16, &_val))            \
			goto end;                                   \
		src += (2 * _nb);                               \
		csum += (_val & 0xff) + ((_val >> 8) & 0xff) +  \
			((_val >> 16) & 0xff) + (_val >> 24);       \
}

	/* loop until end of file is read*/
	while (load_fw) {
		rec_idx++;
		csum = 0;

		/* Find next colon start code */
		while (*src != ':') {
			src++;
			if ((src + 3) >= end) /* 3 = : + rec_len */
				goto end;
		}
		src++;

		/* Read record len */
		IHEX_READ8(byte_count, 1);
		if ((src + (byte_count * 2) + 8) >= end) /* 8 = rec_addr + rec_type + chksum */
			goto end;

		/* Read record addr */
		IHEX_READ16(addr);

		/* Read record type */
		IHEX_READ8(rec_type, 1);

		switch (rec_type) {
		case IHEX_REC_DATA:
		{
			/* Update destination address */
			dst = (u32 *) (fw_addr + hwaddr + addr);

			while (byte_count) {
				u32 val;
				if (byte_count >= 4) {
					IHEX_READ32(val);
					byte_count -= 4;
				} else {
					IHEX_READ32_PAD(val, byte_count);
					byte_count = 0;
				}
				*dst++ = __swab32(val);
			}
			break;
		}
		case IHEX_REC_EOF:
		{
			load_fw = 0;
			err = 0;
			break;
		}
		case IHEX_REC_EXT_SEG_ADD: /* Extended Segment Address */
		{
			IHEX_READ16(segaddr);
			hwaddr = (haddr << 16) + (segaddr << 4);
			break;
		}
		case IHEX_REC_EXT_LIN_ADD: /* Extended Linear Address */
		{
			IHEX_READ16(haddr);
			hwaddr = (haddr << 16) + (segaddr << 4);
			break;
		}
		case IHEX_REC_START_LIN_ADD: /* Start Linear Address */
		{
			u32 val;
			IHEX_READ32(val); /* need to read for checksum */
			break;
		}
		case IHEX_REC_START_SEG_ADD:
		default:
		{
			dev_err(dev, "ihex: record type %d not supported\n", rec_type);
			load_fw = 0;
		}
		}

		/* Read and compare checksum */
		IHEX_READ8(checksum, 0);
		if (checksum != (u8)(~csum + 1))
			goto end;
	}

#undef IHEX_READ8
#undef IHEX_READ16
#undef IHEX_READ32
#undef IHEX_READ32_PAD

end:
	release_firmware(fw);

	if (err)
		dev_err(dev, "%s: Invalid ihex record around line %d\n", filename, rec_idx);

	return err;
}
#endif /* CONFIG_RWNX_TL4 */

#ifndef CONFIG_RWNX_SDM
/**
 * rwnx_plat_get_rf() - Retrun the RF used in the platform
 *
 * @rwnx_plat: pointer to platform structure
 */
static u32 rwnx_plat_get_rf(struct rwnx_plat *rwnx_plat)
{
	u32 ver;
	ver = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, MDM_HDMCONFIG_ADDR);

	ver = __MDM_PHYCFG_FROM_VERS(ver);
	WARN(((ver != MDM_PHY_CONFIG_TRIDENT) &&
		  (ver != MDM_PHY_CONFIG_ELMA) &&
		  (ver != MDM_PHY_CONFIG_KARST)),
		 "bad phy version 0x%08x\n", ver);

	return ver;
}

/**
 * rwnx_plat_stop_agcfsm() - Stop a AGC state machine
 *
 * @rwnx_plat: pointer to platform structure
 * @agg_reg: Address of the agccntl register (within RWNX_ADDR_SYSTEM)
 * @agcctl: Updated with value of the agccntl rgister before stop
 * @memclk: Updated with value of the clock register before stop
 * @agc_ver: Version of the AGC load procedure
 * @clkctrladdr: Indicates which AGC clock register should be accessed
 */
static void rwnx_plat_stop_agcfsm(struct rwnx_plat *rwnx_plat, int agc_reg,
								  u32 *agcctl, u32 *memclk, u8 agc_ver,
								  u32 clkctrladdr)
{
	/* First read agcctnl and clock registers */
	*memclk = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, clkctrladdr);

	/* Stop state machine : xxAGCCNTL0[AGCFSMRESET]=1 */
	*agcctl = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, agc_reg);
	RWNX_REG_WRITE((*agcctl) | BIT(12), rwnx_plat, RWNX_ADDR_SYSTEM, agc_reg);

	/* Force clock */
	if (agc_ver > 0) {
		/* CLKGATEFCTRL0[AGCCLKFORCE]=1 */
		RWNX_REG_WRITE((*memclk) | BIT(29), rwnx_plat, RWNX_ADDR_SYSTEM,
					   clkctrladdr);
	} else {
		/* MEMCLKCTRL0[AGCMEMCLKCTRL]=0 */
		RWNX_REG_WRITE((*memclk) & ~BIT(3), rwnx_plat, RWNX_ADDR_SYSTEM,
					   clkctrladdr);
	}
}


/**
 * rwnx_plat_start_agcfsm() - Restart a AGC state machine
 *
 * @rwnx_plat: pointer to platform structure
 * @agg_reg: Address of the agccntl register (within RWNX_ADDR_SYSTEM)
 * @agcctl: value of the agccntl register to restore
 * @memclk: value of the clock register to restore
 * @agc_ver: Version of the AGC load procedure
 * @clkctrladdr: Indicates which AGC clock register should be accessed
 */
static void rwnx_plat_start_agcfsm(struct rwnx_plat *rwnx_plat, int agc_reg,
								   u32 agcctl, u32 memclk, u8 agc_ver,
								   u32 clkctrladdr)
{

	/* Release clock */
	if (agc_ver > 0)
		/* CLKGATEFCTRL0[AGCCLKFORCE]=0 */
		RWNX_REG_WRITE(memclk & ~BIT(29), rwnx_plat, RWNX_ADDR_SYSTEM,
					   clkctrladdr);
	else
		/* MEMCLKCTRL0[AGCMEMCLKCTRL]=1 */
		RWNX_REG_WRITE(memclk | BIT(3), rwnx_plat, RWNX_ADDR_SYSTEM,
					   clkctrladdr);

	/* Restart state machine: xxAGCCNTL0[AGCFSMRESET]=0 */
	RWNX_REG_WRITE(agcctl & ~BIT(12), rwnx_plat, RWNX_ADDR_SYSTEM, agc_reg);
}
#endif

/**
 * rwnx_plat_fcu_load() - Load FCU (Fith Chain Unit) ucode
 *
 * @rwnx_hw: main driver data
 *
 * c.f Modem UM (AGC/CCA initialization)
 */
static int rwnx_plat_fcu_load(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;
#ifndef CONFIG_RWNX_SDM
	struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
	u32 agcctl, memclk;

#ifndef CONFIG_RWNX_FHOST
	/* By default, we consider that there is only one RF in the system */
	rwnx_hw->phy.cnt = 1;
#endif // CONFIG_RWNX_FHOST

	if (rwnx_plat_get_rf(rwnx_plat) != MDM_PHY_CONFIG_ELMA)
		/* No FCU for PHYs other than Elma */
		return 0;

	agcctl = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, RIU_RWNXAGCCNTL_ADDR);
	if (!__RIU_FCU_PRESENT(agcctl))
		/* No FCU present in this version */
		return 0;

#ifndef CONFIG_RWNX_FHOST
	/* FCU is present */
	if (rwnx_hw->band_5g_support) {
		rwnx_hw->phy.cnt = 2;
		rwnx_hw->phy.sec_chan.band = NL80211_BAND_5GHZ;
		rwnx_hw->phy.sec_chan.type = PHY_CHNL_BW_20;
		rwnx_hw->phy.sec_chan.prim20_freq = 5500;
		rwnx_hw->phy.sec_chan.center_freq1 = 5500;
		rwnx_hw->phy.sec_chan.center_freq2 = 0;
	}
#endif // CONFIG_RWNX_FHOST

	rwnx_plat_stop_agcfsm(rwnx_plat, FCU_RWNXFCAGCCNTL_ADDR, &agcctl, &memclk, 0,
						  MDM_MEMCLKCTRL0_ADDR);

	ret = rwnx_plat_bin_fw_upload(rwnx_plat,
							  RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, PHY_FCU_UCODE_ADDR),
							  RWNX_FCU_FW_NAME);

	rwnx_plat_start_agcfsm(rwnx_plat, FCU_RWNXFCAGCCNTL_ADDR, agcctl, memclk, 0,
						   MDM_MEMCLKCTRL0_ADDR);
#endif

	return ret;
}

/**
 * rwnx_is_new_agc_load() - Return is new agc clock register should be used
 *
 * @rwnx_plat: platform data
 * @rf: rf in used
 *
 * c.f Modem UM (AGC/CCA initialization)
 */
#ifndef CONFIG_RWNX_SDM
static u8 rwnx_get_agc_load_version(struct rwnx_plat *rwnx_plat, u32 rf, u32 *clkctrladdr)
{
	u8 agc_load_ver = 0;
	u32 agc_ver;
	u32 regval;

	/* Trident and Elma PHY use old method */
	if (rf !=  MDM_PHY_CONFIG_KARST) {
		*clkctrladdr = MDM_MEMCLKCTRL0_ADDR;
		return 0;
	}

	/* Get the FPGA signature */
	regval = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_SIGNATURE_ADDR);

	if (__FPGA_TYPE(regval) == 0xC0CA)
		*clkctrladdr = CRM_CLKGATEFCTRL0_ADDR;
	else
		*clkctrladdr = MDM_CLKGATEFCTRL0_ADDR;

	/* Read RIU version register */
	agc_ver = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, RIU_RWNXVERSION_ADDR);
	agc_load_ver = __RIU_AGCLOAD_FROM_VERS(agc_ver);

	return agc_load_ver;
}
#endif /* CONFIG_RWNX_SDM */

/**
 * rwnx_plat_agc_load() - Load AGC ucode
 *
 * @rwnx_plat: platform data
 * c.f Modem UM (AGC/CCA initialization)
 */
static int rwnx_plat_agc_load(struct rwnx_plat *rwnx_plat)
{
	int ret = 0;
#ifndef CONFIG_RWNX_SDM
	u32 agc = 0, agcctl, memclk;
	u32 clkctrladdr;
	u32 rf = rwnx_plat_get_rf(rwnx_plat);
	u8 agc_ver;

	switch (rf) {
	case MDM_PHY_CONFIG_TRIDENT:
		agc = AGC_RWNXAGCCNTL_ADDR;
		break;
	case MDM_PHY_CONFIG_ELMA:
	case MDM_PHY_CONFIG_KARST:
		agc = RIU_RWNXAGCCNTL_ADDR;
		break;
	default:
		return -1;
	}

	agc_ver = rwnx_get_agc_load_version(rwnx_plat, rf, &clkctrladdr);

	rwnx_plat_stop_agcfsm(rwnx_plat, agc, &agcctl, &memclk, agc_ver, clkctrladdr);

	ret = rwnx_plat_bin_fw_upload(rwnx_plat,
							  RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, PHY_AGC_UCODE_ADDR),
							  RWNX_AGC_FW_NAME);

	if (!ret && (agc_ver == 1)) {
		/* Run BIST to ensure that the AGC RAM was correctly loaded */
		RWNX_REG_WRITE(BIT(28), rwnx_plat, RWNX_ADDR_SYSTEM,
					   RIU_RWNXDYNAMICCONFIG_ADDR);
		while (RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, RIU_RWNXDYNAMICCONFIG_ADDR) & BIT(28)) {
			;
		}

		if (!(RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,
							RIU_AGCMEMBISTSTAT_ADDR) & BIT(0))) {
			dev_err(rwnx_platform_get_dev(rwnx_plat),
					"AGC RAM not loaded correctly 0x%08x\n",
					RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,
								  RIU_AGCMEMSIGNATURESTAT_ADDR));
			ret = -EIO;
		}
	}

	rwnx_plat_start_agcfsm(rwnx_plat, agc, agcctl, memclk, agc_ver, clkctrladdr);

#endif
	return ret;
}

/**
 * rwnx_ldpc_load() - Load LDPC RAM
 *
 * @rwnx_hw: Main driver data
 * c.f Modem UM (LDPC initialization)
 */
static int rwnx_ldpc_load(struct rwnx_hw *rwnx_hw)
{
#ifndef CONFIG_RWNX_SDM
	struct rwnx_plat *rwnx_plat = rwnx_hw->plat;
	u32 rf = rwnx_plat_get_rf(rwnx_plat);
	u32 phy_feat = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, MDM_HDMCONFIG_ADDR);

	if ((rf !=  MDM_PHY_CONFIG_KARST) ||
		(phy_feat & (MDM_LDPCDEC_BIT | MDM_LDPCENC_BIT)) !=
		(MDM_LDPCDEC_BIT | MDM_LDPCENC_BIT)) {
		goto disable_ldpc;
	}

	if (rwnx_plat_bin_fw_upload(rwnx_plat,
							RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, PHY_LDPC_RAM_ADDR),
							RWNX_LDPC_RAM_NAME)) {
		goto disable_ldpc;
	}

	return 0;

disable_ldpc:
	rwnx_hw->mod_params->ldpc_on = false;

#endif /* CONFIG_RWNX_SDM */
	return 0;
}

/**
 * rwnx_plat_lmac_load() - Load FW code
 *
 * @rwnx_plat: platform data
 */
static int rwnx_plat_lmac_load(struct rwnx_plat *rwnx_plat)
{
	int ret;

	#ifdef CONFIG_RWNX_TL4
	ret = rwnx_plat_tl4_fw_upload(rwnx_plat,
								  RWNX_ADDR(rwnx_plat, RWNX_ADDR_CPU, RAM_LMAC_FW_ADDR),
								  RWNX_MAC_FW_NAME);
	#else
	ret = rwnx_plat_ihex_fw_upload(rwnx_plat,
								   RWNX_ADDR(rwnx_plat, RWNX_ADDR_CPU, RAM_LMAC_FW_ADDR),
								   RWNX_MAC_FW_NAME);
	if (ret == -ENOENT) {
		ret = rwnx_plat_bin_fw_upload(rwnx_plat,
									  RWNX_ADDR(rwnx_plat, RWNX_ADDR_CPU, RAM_LMAC_FW_ADDR),
									  RWNX_MAC_FW_NAME2);
	}
	#endif

	return ret;
}
#endif

#ifndef CONFIG_ROM_PATCH_EN

typedef struct {
	txpwr_idx_conf_t txpwr_idx;
	txpwr_ofst_conf_t txpwr_ofst;
} nvram_info_t;

nvram_info_t nvram_info = {
	.txpwr_idx = {
		.enable           = 1,
		.dsss             = 9,
		.ofdmlowrate_2g4  = 8,
		.ofdmhighrate_2g4 = 8,
		.ofdm1024qam_2g4  = 8,
		.ofdmlowrate_5g   = 11,
		.ofdmhighrate_5g  = 10,
		.ofdm1024qam_5g   = 9
	},
	.txpwr_ofst = {
		.enable       = 1,
		.chan_1_4     = 0,
		.chan_5_9     = 0,
		.chan_10_13   = 0,
		.chan_36_64   = 0,
		.chan_100_120 = 0,
		.chan_122_140 = 0,
		.chan_142_165 = 0,
	},
};

void get_userconfig_txpwr_idx(txpwr_idx_conf_t *txpwr_idx)
{
	memcpy(txpwr_idx, &(nvram_info.txpwr_idx), sizeof(txpwr_idx_conf_t));
}

void get_userconfig_txpwr_ofst(txpwr_ofst_conf_t *txpwr_ofst)
{
	memcpy(txpwr_ofst, &(nvram_info.txpwr_ofst), sizeof(txpwr_ofst_conf_t));
}

#define MATCH_NODE(type, node, cfg_key) {cfg_key, offsetof(type, node)}

struct parse_match_t {
	char keyname[64];
	int  offset;
};

static const char *parse_key_prefix[] = {
	[0x01] = "module0_",
	[0x21] = "module1_",
};

static const struct parse_match_t parse_match_tab[] = {
	MATCH_NODE(nvram_info_t, txpwr_idx.enable,           "enable"),
	MATCH_NODE(nvram_info_t, txpwr_idx.dsss,             "dsss"),
	MATCH_NODE(nvram_info_t, txpwr_idx.ofdmlowrate_2g4,  "ofdmlowrate_2g4"),
	MATCH_NODE(nvram_info_t, txpwr_idx.ofdmhighrate_2g4, "ofdmhighrate_2g4"),
	MATCH_NODE(nvram_info_t, txpwr_idx.ofdm1024qam_2g4,  "ofdm1024qam_2g4"),
	MATCH_NODE(nvram_info_t, txpwr_idx.ofdmlowrate_5g,   "ofdmlowrate_5g"),
	MATCH_NODE(nvram_info_t, txpwr_idx.ofdmhighrate_5g,  "ofdmhighrate_5g"),
	MATCH_NODE(nvram_info_t, txpwr_idx.ofdm1024qam_5g,   "ofdm1024qam_5g"),

	MATCH_NODE(nvram_info_t, txpwr_ofst.enable,          "ofst_enable"),
	MATCH_NODE(nvram_info_t, txpwr_ofst.chan_1_4,        "ofst_chan_1_4"),
	MATCH_NODE(nvram_info_t, txpwr_ofst.chan_5_9,        "ofst_chan_5_9"),
	MATCH_NODE(nvram_info_t, txpwr_ofst.chan_10_13,      "ofst_chan_10_13"),
	MATCH_NODE(nvram_info_t, txpwr_ofst.chan_36_64,      "ofst_chan_36_64"),
	MATCH_NODE(nvram_info_t, txpwr_ofst.chan_100_120,    "ofst_chan_100_120"),
	MATCH_NODE(nvram_info_t, txpwr_ofst.chan_122_140,    "ofst_chan_122_140"),
	MATCH_NODE(nvram_info_t, txpwr_ofst.chan_142_165,    "ofst_chan_142_165"),
};

static int parse_key_val(const char *str, const char *key, char *val)
{
	const char *p = NULL;
	const char *dst = NULL;
	int keysize = 0;
	int bufsize = 0;

	if (str == NULL || key == NULL || val == NULL)
		return -1;

	keysize = strlen(key);
	bufsize = strlen(str);
	if (bufsize <= keysize)
		return -1;

	p = str;
	while (*p != 0 && *p == ' ')
		p++;

	if (*p == '#')
		return -1;

	if (str + bufsize - p <= keysize)
		return -1;

	if (strncmp(p, key, keysize) != 0)
		return -1;

	p += keysize;

	while (*p != 0 && *p == ' ')
		p++;

	if (*p != '=')
		return -1;

	p++;
	while (*p != 0 && *p == ' ')
		p++;

	if (*p == '"')
		p++;

	dst = p;
	while (*p != 0)
		p++;

	p--;
	while (*p == ' ')
		p--;

	if (*p == '"')
		p--;

	while (*p == '\r' || *p == '\n')
		p--;

	p++;
	strncpy(val, dst, p -dst);
	val[p - dst] = 0;
	return 0;
}

void rwnx_plat_userconfig_parsing(struct rwnx_hw *rwnx_hw, char *buffer)
{
	char conf[100], keyname[64];
	char *line;
	int  i = 0, err;
	long val;

	if (rwnx_hw->vendor_info > (sizeof(parse_key_prefix) / sizeof(parse_key_prefix[0]) - 1)) {
		pr_err("Unsuppor vendor info config\n");
		return;
	}

	while (1) {
		line = buffer;
		while (*buffer != '\r' && *buffer != '\n' && *buffer != 0)
			buffer++;

		while (*buffer == '\r' || *buffer == '\n')
			*buffer++ = 0;

		// store value to data struct
		for (i = 0; i < sizeof(parse_match_tab) / sizeof(parse_match_tab[0]); i++) {
			sprintf(&keyname[0], "%s%s", parse_key_prefix[rwnx_hw->vendor_info], parse_match_tab[i].keyname);
			if (parse_key_val(line, keyname, conf) == 0) {
				err = kstrtol(conf, 0, &val);
				*(unsigned long *)((unsigned long)&nvram_info + parse_match_tab[i].offset) = val;
				printk("%s, %s = %ld\n",  __func__, parse_match_tab[i].keyname, val);
			}
		}

		if (*buffer == 0)
			break;
	}
}

#define AIC_FW_PATH              CONFIG_AIC_FW_PATH
#define FW_PATH_MAX              200
#define FW_USERCONFIG_NAME       "aic_userconfig.txt"

static int aic_load_firmware(u32 **fw_buf, const char *name, struct device *device)
{
	void *buffer = NULL;
	char *path = NULL;
	struct file *fp = NULL;
	int size = 0, len = 0, i = 0;
	ssize_t rdlen = 0;
	u32 *src = NULL, *dst = NULL;

	/* get the firmware path */
	path = __getname();
	if (!path) {
		*fw_buf = NULL;
		return -1;
	}

	len = snprintf(path, FW_PATH_MAX, "%s/%s", AIC_FW_PATH, name);

	if (len >= FW_PATH_MAX) {
		printk("%s: %s file's path too long\n", __func__, name);
		*fw_buf = NULL;
		__putname(path);
		return -1;
	}

	/* open the firmware file */
	fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp) || (!fp)) {
		printk("%s: %s file failed to open\n", __func__, name);
		if (IS_ERR(fp))
			printk("is_Err\n");
		if (!fp)
			printk("null\n");

		*fw_buf = NULL;
		__putname(path);
		fp = NULL;
		return -1;
	}

	size = i_size_read(file_inode(fp));
	if (size <= 0) {
		printk("%s: %s file size invalid %d\n", __func__, name, size);
		*fw_buf = NULL;
		__putname(path);
		filp_close(fp, NULL);
		fp = NULL;
		return -1;
	}

	/* start to read from firmware file */
	buffer = kzalloc(size, GFP_KERNEL);
	if (!buffer) {
		*fw_buf = NULL;
		__putname(path);
		filp_close(fp, NULL);
		fp = NULL;
		return -1;
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 16)
	rdlen = kernel_read(fp, buffer, size, &fp->f_pos);
#else
	rdlen = kernel_read(fp, fp->f_pos, buffer, size);
#endif

	if (size != rdlen) {
		printk("%s: %s file rdlen invalid %d %d\n", __func__, name, (int)rdlen, size);
		*fw_buf = NULL;
		__putname(path);
		filp_close(fp, NULL);
		fp = NULL;
		kfree(buffer);
		buffer = NULL;
		return -1;
	}

	if (rdlen > 0) {
		fp->f_pos += rdlen;
		printk("f_pos=%d\n", (int)fp->f_pos);
	}

	/*start to transform the data format*/
	src = (u32 *)buffer;
	printk("malloc dst\n");
	dst = (u32 *)kzalloc(size, GFP_KERNEL);

	if (!dst) {
		*fw_buf = NULL;
		__putname(path);
		filp_close(fp, NULL);
		fp = NULL;
		kfree(buffer);
		buffer = NULL;
		return -1;
	}

	for (i = 0; i < size / 4; i++) {
		dst[i] = src[i];
	}

	__putname(path);
	filp_close(fp, NULL);
	fp = NULL;
	kfree(buffer);
	buffer = NULL;
	*fw_buf = dst;

	return size;
}

int rwnx_plat_userconfig_upload_android(struct rwnx_hw *rwnx_hw, char *filename)
{
	int size;
	u32 *dst = NULL;

	printk("userconfig file path:%s \r\n", filename);

	/* load aic firmware */
	size = aic_load_firmware(&dst, filename, NULL);
	if (size <= 0) {
		printk("wrong size of firmware file\n");
		kfree(dst);
		dst = NULL;
		return 0;
	}

	rwnx_plat_userconfig_parsing(rwnx_hw, (char *)dst);

	if (dst) {
		kfree(dst);
		dst = NULL;
	}

	return 0;
}

/**
 * rwnx_plat_fmac_load() - Load FW code
 *
 * @rwnx_hw: Main driver data
 */
static int rwnx_plat_fmac_load(struct rwnx_hw *rwnx_hw)
{
	int ret = 0;

	RWNX_DBG(RWNX_FN_ENTRY_STR);
#if 1 //defined(CONFIG_NANOPI_M4) || defined(CONFIG_PLATFORM_ALLWINNER)
	ret = rwnx_plat_userconfig_upload_android(rwnx_hw, FW_USERCONFIG_NAME);
	//ret = rwnx_plat_bin_fw_upload_android(rwnx_hw, RAM_FMAC_FW_ADDR, RWNX_MAC_FW_NAME2);
#else
	ret = rwnx_plat_bin_fw_upload_2(rwnx_hw,
								  RAM_FMAC_FW_ADDR,
								  RWNX_MAC_FW_NAME2);
#endif
	return ret;
}
#endif /* !CONFIG_ROM_PATCH_EN */

#if 0
/**
 * rwnx_plat_mpif_sel() - Select the MPIF according to the FPGA signature
 *
 * @rwnx_plat: platform data
 */
static void rwnx_plat_mpif_sel(struct rwnx_plat *rwnx_plat)
{
#ifndef CONFIG_RWNX_SDM
	u32 regval;
	u32 type;

	/* Get the FPGA signature */
	regval = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_SIGNATURE_ADDR);
	type = __FPGA_TYPE(regval);

	/* Check if we need to switch to the old MPIF or not */
	if ((type != 0xCAFE) && (type != 0XC0CA) && (regval & 0xF) < 0x3) {
		/* A old FPGA A is used, so configure the FPGA B to use the old MPIF */
		RWNX_REG_WRITE(0x3, rwnx_plat, RWNX_ADDR_SYSTEM, FPGAB_MPIF_SEL_ADDR);
	}
#endif
}
#endif


/**
 * rwnx_platform_reset() - Reset the platform
 *
 * @rwnx_plat: platform data
 */
static int rwnx_platform_reset(struct rwnx_plat *rwnx_plat)
{
	u32 regval;

#if defined(AICWF_USB_SUPPORT) || defined(AICWF_SDIO_SUPPORT)
	return 0;
#endif

	/* the doc states that SOFT implies FPGA_B_RESET
	 * adding FPGA_B_RESET is clearer */
	RWNX_REG_WRITE(SOFT_RESET | FPGA_B_RESET, rwnx_plat,
				   RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);
	msleep(100);

	regval = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);

	if (regval & SOFT_RESET) {
		dev_err(rwnx_platform_get_dev(rwnx_plat), "reset: failed\n");
		return -EIO;
	}

	RWNX_REG_WRITE(regval & ~FPGA_B_RESET, rwnx_plat,
				   RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);
	msleep(100);
	return 0;
}

/**
 * rwmx_platform_save_config() - Save hardware config before reload
 *
 * @rwnx_plat: Pointer to platform data
 *
 * Return configuration registers values.
 */
static void *rwnx_term_save_config(struct rwnx_plat *rwnx_plat)
{
	const u32 *reg_list;
	u32 *reg_value, *res;
	int i, size = 0;

	if (rwnx_plat->get_config_reg) {
		size = rwnx_plat->get_config_reg(rwnx_plat, &reg_list);
	}

	if (size <= 0)
		return NULL;

	res = kmalloc(sizeof(u32) * size, GFP_KERNEL);
	if (!res)
		return NULL;

	reg_value = res;
	for (i = 0; i < size; i++) {
		*reg_value++ = RWNX_REG_READ(rwnx_plat, RWNX_ADDR_SYSTEM,
									 *reg_list++);
	}

	return res;
}

#if 0
/**
 * rwmx_platform_restore_config() - Restore hardware config after reload
 *
 * @rwnx_plat: Pointer to platform data
 * @reg_value: Pointer of value to restore
 * (obtained with rwmx_platform_save_config())
 *
 * Restore configuration registers value.
 */
static void rwnx_term_restore_config(struct rwnx_plat *rwnx_plat,
									 u32 *reg_value)
{
	const u32 *reg_list;
	int i, size = 0;

	if (!reg_value || !rwnx_plat->get_config_reg)
		return;

	size = rwnx_plat->get_config_reg(rwnx_plat, &reg_list);

	for (i = 0; i < size; i++) {
		RWNX_REG_WRITE(*reg_value++, rwnx_plat, RWNX_ADDR_SYSTEM,
					   *reg_list++);
	}
}
#endif

#ifndef CONFIG_RWNX_FHOST
#if 0
static int rwnx_check_fw_compatibility(struct rwnx_hw *rwnx_hw)
{
	struct ipc_shared_env_tag *shared = rwnx_hw->ipc_env->shared;
	#ifdef CONFIG_RWNX_FULLMAC
	struct wiphy *wiphy = rwnx_hw->wiphy;
	#endif //CONFIG_RWNX_FULLMAC
	#ifdef CONFIG_RWNX_OLD_IPC
	int ipc_shared_version = 10;
	#else //CONFIG_RWNX_OLD_IPC
	int ipc_shared_version = 11;
	#endif //CONFIG_RWNX_OLD_IPC
	int res = 0;

	if (shared->comp_info.ipc_shared_version != ipc_shared_version) {
		wiphy_err(wiphy, "Different versions of IPC shared version between driver and FW (%d != %d)\n ",
				  ipc_shared_version, shared->comp_info.ipc_shared_version);
		res = -1;
	}

	if (shared->comp_info.radarbuf_cnt != IPC_RADARBUF_CNT) {
		wiphy_err(wiphy, "Different number of host buffers available for Radar events handling "\
				  "between driver and FW (%d != %d)\n", IPC_RADARBUF_CNT,
				  shared->comp_info.radarbuf_cnt);
		res = -1;
	}

	if (shared->comp_info.unsuprxvecbuf_cnt != IPC_UNSUPRXVECBUF_CNT) {
		wiphy_err(wiphy, "Different number of host buffers available for unsupported Rx vectors "\
				  "handling between driver and FW (%d != %d)\n", IPC_UNSUPRXVECBUF_CNT,
				  shared->comp_info.unsuprxvecbuf_cnt);
		res = -1;
	}

	#ifdef CONFIG_RWNX_FULLMAC
	if (shared->comp_info.rxdesc_cnt != IPC_RXDESC_CNT) {
		wiphy_err(wiphy, "Different number of shared descriptors available for Data RX handling "\
				  "between driver and FW (%d != %d)\n", IPC_RXDESC_CNT,
				  shared->comp_info.rxdesc_cnt);
		res = -1;
	}
	#endif /* CONFIG_RWNX_FULLMAC */

	if (shared->comp_info.rxbuf_cnt != IPC_RXBUF_CNT) {
		wiphy_err(wiphy, "Different number of host buffers available for Data Rx handling "\
				  "between driver and FW (%d != %d)\n", IPC_RXBUF_CNT,
				  shared->comp_info.rxbuf_cnt);
		res = -1;
	}

	if (shared->comp_info.msge2a_buf_cnt != IPC_MSGE2A_BUF_CNT) {
		wiphy_err(wiphy, "Different number of host buffers available for Emb->App MSGs "\
				  "sending between driver and FW (%d != %d)\n", IPC_MSGE2A_BUF_CNT,
				  shared->comp_info.msge2a_buf_cnt);
		res = -1;
	}

	if (shared->comp_info.dbgbuf_cnt != IPC_DBGBUF_CNT) {
		wiphy_err(wiphy, "Different number of host buffers available for debug messages "\
				  "sending between driver and FW (%d != %d)\n", IPC_DBGBUF_CNT,
				  shared->comp_info.dbgbuf_cnt);
		res = -1;
	}

	if (shared->comp_info.bk_txq != NX_TXDESC_CNT0) {
		wiphy_err(wiphy, "Driver and FW have different sizes of BK TX queue (%d != %d)\n",
				  NX_TXDESC_CNT0, shared->comp_info.bk_txq);
		res = -1;
	}

	if (shared->comp_info.be_txq != NX_TXDESC_CNT1) {
		wiphy_err(wiphy, "Driver and FW have different sizes of BE TX queue (%d != %d)\n",
				  NX_TXDESC_CNT1, shared->comp_info.be_txq);
		res = -1;
	}

	if (shared->comp_info.vi_txq != NX_TXDESC_CNT2) {
		wiphy_err(wiphy, "Driver and FW have different sizes of VI TX queue (%d != %d)\n",
				  NX_TXDESC_CNT2, shared->comp_info.vi_txq);
		res = -1;
	}

	if (shared->comp_info.vo_txq != NX_TXDESC_CNT3) {
		wiphy_err(wiphy, "Driver and FW have different sizes of VO TX queue (%d != %d)\n",
				  NX_TXDESC_CNT3, shared->comp_info.vo_txq);
		res = -1;
	}

	#if NX_TXQ_CNT == 5
	if (shared->comp_info.bcn_txq != NX_TXDESC_CNT4) {
		wiphy_err(wiphy, "Driver and FW have different sizes of BCN TX queue (%d != %d)\n",
				NX_TXDESC_CNT4, shared->comp_info.bcn_txq);
		res = -1;
	}
	#else
	if (shared->comp_info.bcn_txq > 0) {
		wiphy_err(wiphy, "BCMC enabled in firmware but disabled in driver\n");
		res = -1;
	}
	#endif /* NX_TXQ_CNT == 5 */

	if (shared->comp_info.ipc_shared_size != sizeof(ipc_shared_env)) {
		wiphy_err(wiphy, "Different sizes of IPC shared between driver and FW (%zd != %d)\n",
				  sizeof(ipc_shared_env), shared->comp_info.ipc_shared_size);
		res = -1;
	}

	if (shared->comp_info.msg_api != MSG_API_VER) {
		wiphy_warn(wiphy, "WARNING: Different supported message API versions between "\
				   "driver and FW (%d != %d)\n", MSG_API_VER, shared->comp_info.msg_api);
	}

	return res;
}
#endif
#endif /* !CONFIG_RWNX_FHOST */

/**
 * rwnx_platform_on() - Start the platform
 *
 * @rwnx_hw: Main driver data
 * @config: Config to restore (NULL if nothing to restore)
 *
 * It starts the platform :
 * - load fw and ucodes
 * - initialize IPC
 * - boot the fw
 * - enable link communication/IRQ
 *
 * Called by 802.11 part
 */
int rwnx_platform_on(struct rwnx_hw *rwnx_hw, void *config)
{
	#ifndef CONFIG_ROM_PATCH_EN
	#ifdef CONFIG_DOWNLOAD_FW
	int ret;
	#endif
	#endif
	struct rwnx_plat *rwnx_plat = rwnx_hw->plat;

	RWNX_DBG(RWNX_FN_ENTRY_STR);

	if (rwnx_plat->enabled)
		return 0;

	#if 0
	if (rwnx_platform_reset(rwnx_plat))
		return -1;

	rwnx_plat_mpif_sel(rwnx_plat);

	ret = rwnx_plat_fcu_load(rwnx_hw);
	if (ret)
		return ret;
	ret = rwnx_plat_agc_load(rwnx_plat);
	if (ret)
		return ret;
	ret = rwnx_ldpc_load(rwnx_hw);
	if (ret)
		return ret;
	ret = rwnx_plat_lmac_load(rwnx_plat);
	if (ret)
		return ret;

	shared_ram = RWNX_ADDR(rwnx_plat, RWNX_ADDR_SYSTEM, SHARED_RAM_START_ADDR);
	ret = rwnx_ipc_init(rwnx_hw, shared_ram);
	if (ret)
		return ret;

	ret = rwnx_plat->enable(rwnx_hw);
	if (ret)
		return ret;
	RWNX_REG_WRITE(BOOTROM_ENABLE, rwnx_plat,
				   RWNX_ADDR_SYSTEM, SYSCTRL_MISC_CNTL_ADDR);

	#ifndef CONFIG_RWNX_FHOST
	{
		ret = rwnx_check_fw_compatibility(rwnx_hw);
		if (ret) {
			rwnx_hw->plat->disable(rwnx_hw);
			tasklet_kill(&rwnx_hw->task);
			rwnx_ipc_deinit(rwnx_hw);
			return ret;
		}
	}
	#endif /* !CONFIG_RWNX_FHOST */

	if (config)
		rwnx_term_restore_config(rwnx_plat, config);

	rwnx_ipc_start(rwnx_hw);
	#else
	#ifndef CONFIG_ROM_PATCH_EN
	#ifdef CONFIG_DOWNLOAD_FW
	ret = rwnx_plat_fmac_load(rwnx_hw);
	if (ret)
		return ret;
	#endif /* !CONFIG_ROM_PATCH_EN */
	#endif
	#endif

	rwnx_plat->enabled = true;

	return 0;
}

/**
 * rwnx_platform_off() - Stop the platform
 *
 * @rwnx_hw: Main driver data
 * @config: Updated with pointer to config, to be able to restore it with
 * rwnx_platform_on(). It's up to the caller to free the config. Set to NULL
 * if configuration is not needed.
 *
 * Called by 802.11 part
 */
void rwnx_platform_off(struct rwnx_hw *rwnx_hw, void **config)
{
#if defined(AICWF_USB_SUPPORT) || defined(AICWF_SDIO_SUPPORT)
	rwnx_hw->plat->enabled = false;
	return ;
#endif

	if (!rwnx_hw->plat->enabled) {
		if (config)
			*config = NULL;
		return;
	}

	if (config)
		*config = rwnx_term_save_config(rwnx_hw->plat);

	rwnx_hw->plat->disable(rwnx_hw);

	tasklet_kill(&rwnx_hw->task);
	rwnx_platform_reset(rwnx_hw->plat);

	rwnx_hw->plat->enabled = false;
}

/**
 * rwnx_platform_init() - Initialize the platform
 *
 * @rwnx_plat: platform data (already updated by platform driver)
 * @platform_data: Pointer to store the main driver data pointer (aka rwnx_hw)
 *                That will be set as driver data for the platform driver
 * Return: 0 on success, < 0 otherwise
 *
 * Called by the platform driver after it has been probed
 */
int rwnx_platform_init(struct rwnx_plat *rwnx_plat, void **platform_data)
{
	RWNX_DBG(RWNX_FN_ENTRY_STR);

	rwnx_plat->enabled = false;
	g_rwnx_plat = rwnx_plat;

#if defined CONFIG_RWNX_FULLMAC
	return rwnx_cfg80211_init(rwnx_plat, platform_data);
#elif defined CONFIG_RWNX_FHOST
	return rwnx_fhost_init(rwnx_plat, platform_data);
#endif
}

/**
 * rwnx_platform_deinit() - Deinitialize the platform
 *
 * @rwnx_hw: main driver data
 *
 * Called by the platform driver after it is removed
 */
void rwnx_platform_deinit(struct rwnx_hw *rwnx_hw)
{
	RWNX_DBG(RWNX_FN_ENTRY_STR);

#if defined CONFIG_RWNX_FULLMAC
	rwnx_cfg80211_deinit(rwnx_hw);
#elif defined CONFIG_RWNX_FHOST
	rwnx_fhost_deinit(rwnx_hw);
#endif
}

/**
 * rwnx_platform_register_drv() - Register all possible platform drivers
 */
int rwnx_platform_register_drv(void)
{
	return rwnx_pci_register_drv();
}


/**
 * rwnx_platform_unregister_drv() - Unegister all platform drivers
 */
void rwnx_platform_unregister_drv(void)
{
	return rwnx_pci_unregister_drv();
}

struct device *rwnx_platform_get_dev(struct rwnx_plat *rwnx_plat)
{
#ifdef AICWF_SDIO_SUPPORT
	return rwnx_plat->sdiodev->dev;
#endif
#ifdef AICWF_USB_SUPPORT
	return rwnx_plat->usbdev->dev;
#endif
	return &(rwnx_plat->pci_dev->dev);
}


#ifndef CONFIG_RWNX_SDM
MODULE_FIRMWARE(RWNX_AGC_FW_NAME);
MODULE_FIRMWARE(RWNX_FCU_FW_NAME);
MODULE_FIRMWARE(RWNX_LDPC_RAM_NAME);
#endif
MODULE_FIRMWARE(RWNX_MAC_FW_NAME);
#ifndef CONFIG_RWNX_TL4
MODULE_FIRMWARE(RWNX_MAC_FW_NAME2);
#endif


