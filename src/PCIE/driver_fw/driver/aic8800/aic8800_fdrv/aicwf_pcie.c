
#include <linux/jiffies.h>
//#include <linux/timekeeping.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include "aicwf_pcie.h"
#include "pcie_host.h"
#include "aicwf_txrxif.h"
#include "rwnx_defs.h"
#include "rwnx_platform.h"
#include "aic_bsp_export.h"
#include "lmac_msg.h"
#include "rwnx_msg_tx.h"

#define AIC8820_PCI_VENDOR_ID	0xa69c//0xadec//0x8820
#define AIC8820_PCI_DEVICE_ID	0x8d80//0xadec
extern uint8_t scanning;
extern u8 dhcped;

#ifdef AICWF_PCIE_SUPPORT

static const struct pci_device_id aic8820_pci_ids[] = {
    {PCI_DEVICE(AIC8820_PCI_VENDOR_ID,AIC8820_PCI_DEVICE_ID)},
};

#ifdef CONFIG_WS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static struct wakeup_source *pci_ws;
#endif

void rwnx_pm_stay_awake_pc(struct rwnx_hw *rwnx_hw)
{
	printk("%s\n", __func__);

	//pm_stay_awake(&(rwnx_hw->pcidev->pci_dev->dev));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	spin_lock_bh(&rwnx_hw->pcidev->ws_lock);
	if(pci_ws != NULL){
		__pm_stay_awake(pci_ws);
	}
	spin_unlock_bh(&rwnx_hw->pcidev->ws_lock);
#endif
}

void rwnx_pm_relax_pc(struct rwnx_hw *rwnx_hw)
{
	printk("%s\n", __func__);

	//pm_relax(&(rwnx_hw->pcidev->pci_dev->dev));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	spin_lock_bh(&rwnx_hw->pcidev->ws_lock);
	if(pci_ws != NULL){
		__pm_relax(pci_ws);
	}
	spin_unlock_bh(&rwnx_hw->pcidev->ws_lock);
#endif
}

static void register_ws(void)
{
	printk("%s\n", __func__);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	pci_ws = wakeup_source_register("wifisleep");
#endif
}

static void unregister_ws(void)
{
	printk("%s\n", __func__);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	wakeup_source_unregister(pci_ws);
#endif
}
#endif

irqreturn_t aicwf_pcie_irq_hdlr(int irq, void *dev_id)
{
    struct aic_pci_dev *pciedev = (struct aic_pci_dev *) dev_id;

    //printk("%s:%lx\n", __func__);
    if(pciedev->rwnx_hw) {
        disable_irq_nosync(irq);
        pciedev->rwnx_hw->is_irq_disable = 1;
        tasklet_schedule(&pciedev->rwnx_hw->task);
    }

    return IRQ_HANDLED;
}

extern u8 data_cnt;
extern int rwnx_plat_bin_fw_upload_2(struct rwnx_hw *rwnx_hw, u32 fw_addr, char *filename);

static int aicwf_sw_resume(void)
{
	struct rwnx_hw *rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
	//struct rwnx_vif *rwnx_vif;
	struct rwnx_ipc_buf *buf;
	struct rwnx_ipc_buf *ipc_buf;
	int i;
	struct ipc_e2a_msg *msg;

	rwnx_hw->ipc_env->msgbuf_idx = 0; // reset msgrx idx
	for (i = 0; i < IPC_MSGE2A_BUF_CNT; i++) {
		buf = rwnx_hw->ipc_env->msgbuf[i];
		if (!buf) {
			printk("msg error!!!\n");
			break;
		}
		msg = buf->addr;
		msg->pattern = 0;
		ipc_host_msgbuf_push(rwnx_hw->ipc_env, buf);
	}

#if 0
	for (i = 0; i < IPC_TXDMA_DESC_CNT; i++) {
		struct rwnx_sw_txhdr *sw_txhdr = (struct rwnx_sw_txhdr *)rwnx_hw->ipc_env->txcfm[i];
		struct rwnx_ipc_buf *txcfm_buf;
		if (sw_txhdr != NULL) {
			txcfm_buf = &sw_txhdr->ipc_desc;
			struct sk_buff *skb_tmp = sw_txhdr->skb;

			rwnx_ipc_buf_a2e_release(rwnx_hw, txcfm_buf);
			dma_unmap_single(rwnx_hw->dev, sw_txhdr->ipc_hostdesc.dma_addr, sw_txhdr->ipc_hostdesc.size, DMA_TO_DEVICE);
			kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
			skb_pull(skb_tmp, RWNX_TX_HEADROOM);
			consume_skb(skb_tmp);
			rwnx_hw->ipc_env->txcfm[i] = NULL;
			atomic_dec(&rwnx_hw->txdata_cnt);
		}
	}
#endif
	rwnx_hw->ipc_env->txdmadesc_idx = 0; //reset tx idx
	rwnx_hw->ipc_env->txcfm_idx = 0; //reset txcfm idx

	for (i = 0; i < IPC_RXBUF_CNT; i++) {
		ipc_buf = &rwnx_hw->rxbufs[i];
		rwnx_hw->ipc_env->shared->host_rxbuf[i].hostid = RWNX_RXBUFF_HOSTID_GET(ipc_buf);
		rwnx_hw->ipc_env->shared->host_rxbuf[i].dma_addr = ipc_buf->dma_addr;
		rwnx_hw->ipc_env->shared->host_rxbuf[i].pattern = 0x0;
	}

	data_cnt = 0; // reset rx idx
	rwnx_hw->rxbuf_idx = 0;
	rwnx_hw->ipc_env->rxbuf_idx = 0;

	return 0;
}
static int aicwf_resume_access(void)
{
	int ret = 0;
	struct mm_add_if_cfm add_if_cfm;
	struct mm_set_stack_start_cfm set_start_cfm;
	struct aicbsp_feature_t feature;
	struct rwnx_hw *rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
	struct rwnx_vif *rwnx_vif;
	struct rwnx_vif *rwnx_vif_param = NULL;

#ifdef CONFIG_USB_BT
    struct aicbt_patch_table *head = NULL;
    struct aicbt_patch_info_t patch_info = {
        .info_len          = 0,
        .adid_addrinf      = 0,
        .addr_adid         = 0,
        .patch_addrinf     = 0,
        .addr_patch        = 0,
        .reset_addr        = 0,
        .reset_val         = 0,
        .adid_flag_addr    = 0,
        .adid_flag         = 0,
    };

    head = aicbt_patch_table_alloc(rwnx_hw, FW_PATCH_TABLE_NAME_8800D80_U02);
    if (head == NULL){
        printk("aicbt_patch_table_alloc fail\n");
        return -1;
    }

    patch_info.addr_adid = FW_RAM_ADID_BASE_ADDR_8800D80_U02;
    patch_info.addr_patch = FW_RAM_PATCH_BASE_ADDR_8800D80_U02;

    aicbt_patch_info_unpack(&patch_info, head);
    if(patch_info.info_len == 0) {
        printk("%s, aicbt_patch_info_unpack fail\n", __func__);
        return -1;
    }

    printk("addr_adid 0x%x, addr_patch 0x%x\n", patch_info.addr_adid, patch_info.addr_patch);

	if(rwnx_plat_bin_fw_upload_2(rwnx_hw, patch_info.addr_adid, FW_ADID_BASE_NAME_8800D80_U02)) {
		printk("%s load patch fail 1\n", __func__);
		return -1;
	}
	if(rwnx_plat_bin_fw_upload_2(rwnx_hw, patch_info.addr_patch, FW_PATCH_BASE_NAME_8800D80_U02)) {
		printk("%s load patch fail 2\n", __func__);
		return -1;
	}
	if (aicbt_patch_table_load(rwnx_hw, head)) {
        return -1;
    }
	mdelay(15);
#endif

	ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, RAM_FMAC_FW_ADDR, RWNX_PCIE_FW_NAME);
	if (ret) {
		printk("resume fw load fail\n");
		return ret;
	}

	patch_config(rwnx_hw);
	pcie_reset_firmware(rwnx_hw, RAM_FMAC_FW_ADDR);

	aicwf_sw_resume();

	struct rwnx_cmd *cur, *nxt;
	spin_lock_bh(&rwnx_hw->cmd_mgr->lock);
	list_for_each_entry_safe(cur, nxt, &rwnx_hw->cmd_mgr->cmds, list) {
		printk("resume_cmd_id: %d\n", cur->id);
		list_del(&cur->list);
		rwnx_hw->cmd_mgr->queue_sz--;
		if (!(cur->flags & RWNX_CMD_FLAG_NONBLOCK))
			complete(&cur->complete);
	}
	if(rwnx_hw->pcidev->cmd_mgr.state == RWNX_CMD_MGR_STATE_CRASHED) {
		rwnx_hw->pcidev->cmd_mgr.state = RWNX_CMD_MGR_STATE_INITED;
		printk("cmd state recovery\n");
	}
	spin_unlock_bh(&rwnx_hw->cmd_mgr->lock);

	list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
		if (rwnx_vif->up) {
			printk("find vif_up\n");
			rwnx_vif_param = rwnx_vif;
			spin_lock_bh(&rwnx_hw->cb_lock);
			rwnx_vif->vif_index = 0;
			rwnx_hw->vif_table[0] = rwnx_vif;
			spin_unlock_bh(&rwnx_hw->cb_lock);
		}
	}

#ifdef USE_5G
	ret  = rwnx_send_resume_restore(rwnx_hw, 1, 0, CO_BIT(5), 0, rwnx_vif_param);
#else
	ret  = rwnx_send_resume_restore(rwnx_hw, 1, feature.hwinfo < 0,feature.hwinfo, 0, rwnx_vif_param);
#endif
	if (ret) {
		printk("%s restore fail\n", __func__);
		return ret;
	}

	mdelay(200);
	rwnx_hw->pci_suspending = 0;

	return ret;
}

static int aicwf_disconnect_inform(struct rwnx_hw *rwnx_hw, struct rwnx_vif *rwnx_vif) {
	struct net_device *dev;
#ifdef AICWF_RX_REORDER
	struct reord_ctrl_info *reord_info, *tmp;
	u8 *macaddr;
	struct aicwf_rx_priv *rx_priv;
#endif

	RWNX_DBG(RWNX_FN_ENTRY_STR);
	dhcped = 0;

	if(!rwnx_vif)
		return 0;
	dev = rwnx_vif->ndev;

	#ifdef CONFIG_BR_SUPPORT
		struct rwnx_vif *vif = netdev_priv(dev);
			 /* clear bridge database */
			 nat25_db_cleanup(rwnx_vif);
	#endif /* CONFIG_BR_SUPPORT */

	if (rwnx_vif->wdev.iftype == NL80211_IFTYPE_P2P_CLIENT)
		rwnx_hw->is_p2p_connected = 0;
	/* if vif is not up, rwnx_close has already been called */
	if (rwnx_vif->up) {
		cfg80211_disconnected(dev, 1, NULL, 0, true, GFP_ATOMIC);
		netif_tx_stop_all_queues(dev);
		netif_carrier_off(dev);
	}

#ifdef CONFIG_RWNX_BFMER
	/* Disable Beamformer if supported */
	rwnx_bfmer_report_del(rwnx_hw, rwnx_vif->sta.ap);
#endif //(CONFIG_RWNX_BFMER)

#ifdef AICWF_RX_REORDER
	rx_priv = rwnx_hw->pcidev->rx_priv;

	if ((rwnx_vif->wdev.iftype == NL80211_IFTYPE_STATION) || (rwnx_vif->wdev.iftype == NL80211_IFTYPE_P2P_CLIENT)) {
		macaddr = rwnx_vif->ndev->dev_addr;
		printk("deinit:macaddr:%x,%x,%x,%x,%x,%x\r\n", macaddr[0], macaddr[1], macaddr[2], \
							   macaddr[3], macaddr[4], macaddr[5]);

		spin_lock_bh(&rx_priv->stas_reord_lock);
		list_for_each_entry_safe(reord_info, tmp, &rx_priv->stas_reord_list, list) {
			macaddr = rwnx_vif->ndev->dev_addr;
			printk("reord_mac:%x,%x,%x,%x,%x,%x\r\n", reord_info->mac_addr[0], reord_info->mac_addr[1], reord_info->mac_addr[2], \
								   reord_info->mac_addr[3], reord_info->mac_addr[4], reord_info->mac_addr[5]);
			if (!memcmp(reord_info->mac_addr, macaddr, 6)) {
				reord_deinit_sta(rx_priv, reord_info);
				break;
			}
		}
		spin_unlock_bh(&rx_priv->stas_reord_lock);
	} else if ((rwnx_vif->wdev.iftype == NL80211_IFTYPE_AP) || (rwnx_vif->wdev.iftype == NL80211_IFTYPE_P2P_GO)) {
		BUG();//should be not here: del_sta function
	}
#endif

	rwnx_txq_sta_deinit(rwnx_hw, rwnx_vif->sta.ap);
	rwnx_txq_tdls_vif_deinit(rwnx_vif);
	#if 0
	rwnx_dbgfs_unregister_rc_stat(rwnx_hw, rwnx_vif->sta.ap);
	#endif
	rwnx_vif->sta.ap->valid = false;
	rwnx_vif->sta.ap = NULL;
	rwnx_external_auth_disable(rwnx_vif);
	rwnx_chanctx_unlink(rwnx_vif);

	//msleep(200);
	atomic_set(&rwnx_vif->drv_conn_state, (int)RWNX_DRV_STATUS_DISCONNECTED);
	return 0;
}

static int aicwf_pcie_init(struct aic_pci_dev *pciedev)
{
	struct pci_dev *pci_dev = pciedev->pci_dev;
	u16 pci_cmd;
	int ret = -ENODEV;
	u8 linkctrl;

	printk("%s\n", __func__);
	/* Hotplug fixups */
	pci_read_config_word(pci_dev, PCI_COMMAND, &pci_cmd);
	pci_cmd |= PCI_COMMAND_PARITY | PCI_COMMAND_SERR;
	pci_write_config_word(pci_dev, PCI_COMMAND, pci_cmd);
	pci_write_config_byte(pci_dev, PCI_CACHE_LINE_SIZE, L1_CACHE_BYTES>>2);

	if(ret = pci_enable_device(pci_dev)) {
		dev_err(&(pci_dev->dev), "pci_enable_device failed\n");
		goto out;
	}

	pci_set_master(pci_dev);

	if(ret = pci_request_regions(pci_dev, KBUILD_MODNAME)) {
		dev_err(&(pci_dev->dev), "pci_request_regions failed\n");
		goto out_request;
	}
	
	if(pci_enable_msi(pci_dev)) {
		dev_err(&(pci_dev->dev), "pci_enable_msi failed\n");
		goto out_msi;
	}

	if( !(pciedev->pci_bar0_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 0))) {
		dev_err(&(pci_dev->dev), "pci_ioremap_bar0 failed\n");
		ret = -ENODEV;
		goto out_bar0;
	}

	if( !(pciedev->pci_bar1_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 1))) {
		dev_err(&(pci_dev->dev), "pci_ioremap_bar1 failed\n");
		ret = -ENODEV;
		goto out_bar1;
	}

	if( !(pciedev->pci_bar2_vaddr = (u8 *)pci_ioremap_bar(pci_dev, 2))) {
		dev_err(&(pci_dev->dev), "pci_ioremap_bar2 failed\n");
		ret = -ENODEV;
		goto out_bar2;
	}

    ret = request_irq(pci_dev->irq, aicwf_pcie_irq_hdlr,  IRQF_SHARED, "aicwf_pci", pciedev);
    if(ret) {
        printk("request irq fail:%d\n", ret);
        goto out_irq;
    }

	//# by G: msg waitlock at L1
	pci_read_config_byte(pci_dev, pci_dev->pcie_cap + PCI_EXP_LNKCTL, &linkctrl);
	if(linkctrl & 0x02){
		linkctrl = linkctrl & ~0x02;
		pci_write_config_byte(pci_dev, pci_dev->pcie_cap + PCI_EXP_LNKCTL, linkctrl);
	}

    #if 0
    unsigned long base = pci_resource_start(pci_dev, 0);
    unsigned long len = pci_resource_len(pci_dev, 0);
    unsigned long flags = pci_resource_flags(pci_dev, 0);
    printk("bar0: base: 0x%lx, len=%ld, flags=0x%lx, vaddr=%p\n", base, len, flags, pciedev->pci_bar0_vaddr);
    base = pci_resource_start(pci_dev, 2);
    len = pci_resource_len(pci_dev, 2);
    flags = pci_resource_flags(pci_dev, 2);
    printk("bar2: base: 0x%lx, len=%ld, flags=0x%lx, vaddr=%p\n", base, len, flags, pciedev->pci_bar0_vaddr);
    #endif
    printk("%s success\n", __func__);

    goto out;
out_irq:
    iounmap(pciedev->pci_bar2_vaddr);
out_bar2:
    iounmap(pciedev->pci_bar1_vaddr);
out_bar1:
    iounmap(pciedev->pci_bar0_vaddr);
out_bar0:
    pci_disable_msi(pci_dev);
out_msi:
    pci_release_regions(pci_dev);
out_request:
    pci_disable_device(pci_dev);

out:
    return ret;
}

static void aicwf_pcie_txmsg_db(struct aic_pci_dev *pciedev)
{
	volatile unsigned int *dst_mail = (volatile unsigned int *)(pciedev->pci_bar2_vaddr + 0x800ec);
	dst_mail[0] = 0x1;
}

static void aicwf_pcie_txdata_db(struct aic_pci_dev *pciedev)
{
	volatile unsigned int *dst_mail = (volatile unsigned int *)(pciedev->pci_bar2_vaddr + 0x800ec);
	dst_mail[0] = 0x2;
}

static int aicwf_pcie_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id)
{
	int ret = -ENODEV;

	printk("%s\n", __func__);

	struct aicwf_bus *bus_if = kzalloc(sizeof(struct aicwf_bus), GFP_KERNEL);
	if (!bus_if) {
	 printk("alloc bus fail\n");
	 return -ENOMEM;
	}

	struct aic_pci_dev *pciedev = kzalloc(sizeof(struct aic_pci_dev), GFP_KERNEL);
	if (!pciedev) {
	 printk("alloc pciedev fail\n");
	 kfree(bus_if);
	 return -ENOMEM;
	}

	pciedev->bus_if = bus_if;
	bus_if->bus_priv.pci = pciedev;
	dev_set_drvdata(&pci_dev->dev, bus_if);
	pciedev->pci_dev = pci_dev;

#ifdef CONFIG_WS
	register_ws();
#endif

	ret = aicwf_pcie_init(pciedev);

	if(ret) {
		printk("%s: pci init fail\n", __func__);
		return ret;
	}

	aicwf_pcie_bus_init(pciedev);

	ret = aicwf_pcie_platform_init(pciedev);

	if(!ret)
		aicwf_hostif_ready();

	return ret;
}

static void aicwf_pcie_remove(struct pci_dev *pci_dev)
{
    struct aicwf_bus *bus_if = dev_get_drvdata(&pci_dev->dev);
	struct aic_pci_dev *pci = bus_if->bus_priv.pci;

	printk("%s\n", __func__);

    bus_if->state = BUS_DOWN_ST;
    rwnx_cmd_mgr_deinit(&bus_if->bus_priv.pci->cmd_mgr);

#ifdef CONFIG_WS
	unregister_ws();
#endif
	free_irq(pci_dev->irq, pci);
	pci_disable_device(pci_dev);
	iounmap(pci->pci_bar0_vaddr);
	iounmap(pci->pci_bar1_vaddr);
	iounmap(pci->pci_bar2_vaddr);
	pci_release_regions(pci_dev);
	pci_clear_master(pci_dev);
	pci_disable_msi(pci_dev);

#ifdef AICWF_PCIE_SUPPORT
	if (pci->bus_if->busrx_thread) {
		complete_all(&pci->bus_if->busrx_trgg);
		kthread_stop(pci->bus_if->busrx_thread);
		pci->bus_if->busrx_thread = NULL;
	}
#endif

	kfree(bus_if);
	kfree(pci);
}

static int aicwf_pcie_suspend(struct pci_dev *pdev, pm_message_t state)
{
	printk("%s\n", __func__);
	int ret = 0, i = 0;
	//g_rwnx_plat->pcidev->rwnx_hw->pci_suspending = 1;
	struct rwnx_hw *rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
	struct rwnx_vif *rwnx_vif;

	list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
		if (rwnx_vif->up) {
			while ((int)atomic_read(&rwnx_vif->drv_conn_state) == (int)RWNX_DRV_STATUS_DISCONNECTING) {
				printk("suspend waiting disc\n");
				msleep(100);
				i += 1;
				if (i >= 20) {
					aicwf_disconnect_inform(rwnx_hw, rwnx_vif);
					break;
				}
			}
		}
	}

	g_rwnx_plat->pcidev->rwnx_hw->pci_suspending = 1;

	spin_lock_bh(&rwnx_hw->cb_lock);
	if (rwnx_hw->scan_request) {// && rwnx_hw->scan_request->wdev == &rwnx_vif->wdev) {
		printk("suspend scan_done\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		struct cfg80211_scan_info info =
		{
			.aborted = true,
		};
		cfg80211_scan_done (rwnx_hw->scan_request, &info);
#else
		cfg80211_scan_done (rwnx_hw->scan_request, true);
#endif
		rwnx_hw->scan_request = NULL;
		scanning = 0;
	}
	spin_unlock_bh(&rwnx_hw->cb_lock);

	ret = pci_save_state(pdev);
	if (ret) {
		printk("failed on pci_save_state %d\n", ret);
		return ret;
	}

	pci_disable_device(pdev);
	ret = pci_set_power_state(pdev, pci_choose_state(pdev, state));
	if (ret)
		printk("failed on pci_set_power_state %d\n", ret);

	return ret;
}

static int aicwf_pcie_resume(struct pci_dev *pdev)
{
	struct rwnx_hw *rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
	bool fw_started;
	int ret = 0;

	printk("%s enter: %d\n", __func__, atomic_read(&rwnx_hw->txdata_cnt));

	ret = pci_set_power_state(pdev, PCI_D0);
	if (ret) {
		printk("failed on pci_set_power_state %d\n", ret);
		return ret;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		printk("failed on pci_enable_device %d\n", ret);
		return ret;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 37))
	pci_restore_state(pdev);
#else
	ret = pci_restore_state(pdev);
	if (ret) {
		printk("failed on pci_restore_state %d\n", ret);
		return ret;
	}
#endif

	fw_started = *(volatile u32 *) (g_rwnx_plat->pcidev->pci_bar0_vaddr + 0x120000) == 0x1a0000;

	if(!fw_started) {
		ret = aicwf_resume_access();
		if (ret) {
			printk("resume access fail %d\n", ret);
			return ret;
		}
	} else {
		printk("resume skip reload\n");

		g_rwnx_plat->pcidev->rwnx_hw->pci_suspending = 0;

		return ret;
	}
	printk("%s end\n", __func__);

	return ret;
}

static struct pci_driver aicwf_pci_driver = {
	.name 	   = KBUILD_MODNAME,
	.id_table  = aic8820_pci_ids,
	.probe 	   = aicwf_pcie_probe,
	.remove    = aicwf_pcie_remove,
	.suspend   = aicwf_pcie_suspend,
	.resume    = aicwf_pcie_resume,
};

int aicwf_pcie_register_drv(void)
{
	return pci_register_driver(&aicwf_pci_driver);
}

void aicwf_pcie_unregister_drv(void)
{
	if (g_rwnx_plat && g_rwnx_plat->enabled)
        rwnx_platform_deinit(g_rwnx_plat->pcidev->rwnx_hw);

	pci_unregister_driver(&aicwf_pci_driver);
}

void rwnx_data_dump(char* tag, void* data, unsigned long len){
        unsigned long i = 0;
        uint8_t* data_ = (uint8_t* )data;

        printk("%s %s len:(%lu)\r\n", __func__, tag, len);

        for (i = 0; i < len; i += 16){
        printk("%02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                data_[0 + i],
                data_[1 + i],
                data_[2 + i],
                data_[3 + i],
                data_[4 + i],
                data_[5 + i],
                data_[6 + i],
                data_[7 + i],
                data_[8 + i],
                data_[9 + i],
                data_[10 + i],
                data_[11 + i],
                data_[12 + i],
                data_[13 + i],
                data_[14 + i],
                data_[15 + i]);
                }

}

int pcie_rxbuf_rep_thread(void *data)
{
	struct aicwf_rx_priv *rx_priv = (struct aicwf_rx_priv *)data;
	struct aicwf_bus *bus_if = rx_priv->pciedev->bus_if;
	struct rwnx_hw *rwnx_hw = rx_priv->pciedev->rwnx_hw;

	//printk("rwnx_hw = %px \n",rwnx_hw);
	while (1) {
		if (kthread_should_stop()) {
			AICWFDBG(LOGERROR, "pcie busrx thread stop\n");
			break;
		}
		if (!wait_for_completion_interruptible(&bus_if->busrx_trgg)) {

			//bus_if = g_rwnx_plat->pcidev->bus_if;
			//rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;

			if (bus_if->state == BUS_DOWN_ST)
				continue;

			printk("%s trigger \n",__func__);
			printk("trigger rwnx_hw = %px \n",rwnx_hw);
			for(;atomic_read(&rwnx_hw->rxbuf_cnt) < rwnx_hw->ipc_env->rxbuf_nb;){
				if(rwnx_ipc_rxbuf_alloc(rwnx_hw)){
					printk("pcie_rxbuf_rep_thread rxbuf alloc fail,now rxbuf_cnt = %d \n",atomic_read(&rwnx_hw->rxbuf_cnt));
					msleep(10);
					//break;
				}
			}
			printk("%s out \n",__func__);
		}
	}

	return 0;
}

static int aicwf_pcie_bus_start(struct device *dev)
{

	return 0;
}

static void aicwf_pcie_bus_stop(struct device *dev)
{

}

static int aicwf_pcie_bus_txdata(struct device *dev, struct sk_buff *skb)
{
	//printk("%s\n", __func__);
    struct aicwf_bus *bus_if = dev_get_drvdata(dev);

	aicwf_pcie_txdata_db(bus_if->bus_priv.pci);

	return 0;
}

static int aicwf_pcie_bus_txmsg(struct device *dev, u8 *msg, uint msglen)
{
    struct aicwf_bus *bus_if = dev_get_drvdata(dev);
    struct rwnx_hw *rwnx_hw = bus_if->bus_priv.pci->rwnx_hw;

	pcie_host_msg_push(rwnx_hw->ipc_env, msg, msglen);
	aicwf_pcie_txmsg_db(bus_if->bus_priv.pci);

	//rwnx_data_dump("msg", msg, msglen);

	return 0;
}

static struct aicwf_bus_ops aicwf_pcie_bus_ops = {
    .start = aicwf_pcie_bus_start,
    .stop = aicwf_pcie_bus_stop,
    .txdata = aicwf_pcie_bus_txdata,
    .txmsg = aicwf_pcie_bus_txmsg,
};

void aicwf_pcie_bus_init(struct aic_pci_dev *pciedev)
{
	struct aicwf_bus *bus_if = pciedev->bus_if;
	int ret;

    bus_if->dev = &pciedev->pci_dev->dev;
    bus_if->ops = &aicwf_pcie_bus_ops;
    bus_if->state = BUS_UP_ST;

    struct aicwf_rx_priv* rx_priv;
    rx_priv = aicwf_rx_init(pciedev);
    if(!rx_priv) {
        txrx_err("rx init failed\n");
        //ret = -1;
        //goto out_free_bus;
    }
    pciedev->rx_priv = rx_priv;
	
	ret = aicwf_bus_init(0, &pciedev->pci_dev->dev);
	if(ret)
		printk("%s fail\n", __func__);
}

#endif

