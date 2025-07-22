
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
#include "rwnx_irqs.h"
#ifdef CONFIG_USE_BT
#include "aicwf_bt_init.h"
#endif

extern uint8_t scanning;
extern u8 dhcped;
extern int testmode;

#ifdef AICWF_PCIE_SUPPORT

static const struct pci_device_id aic8820_pci_ids[] = {
    {PCI_DEVICE(AIC8800D80_PCI_VENDOR_ID,AIC8800D80_PCI_DEVICE_ID)},
    {PCI_DEVICE(AIC8800D80X2_PCI_VENDOR_ID,AIC8800D80X2_PCI_DEVICE_ID)},
};

#ifdef CONFIG_WS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static struct wakeup_source *pci_ws;
#endif

void rwnx_pm_stay_awake_pc(struct rwnx_hw *rwnx_hw)
{
	RWNX_DBG(RWNX_FN_ENTRY_STR);

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
	RWNX_DBG(RWNX_FN_ENTRY_STR);

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
	RWNX_DBG(RWNX_FN_ENTRY_STR);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	pci_ws = wakeup_source_register("wifisleep");
#endif
}

static void unregister_ws(void)
{
	RWNX_DBG(RWNX_FN_ENTRY_STR);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	wakeup_source_unregister(pci_ws);
#endif
}
#endif

#ifdef CONFIG_TEMP_CONTROL
//int interval = 30;
//module_param(interval, int, 0660);
static int update_state(s8_l value, u8_l current_state)
{
	s8_l thd_1 = g_rwnx_plat->pcidev->tp_thd_1;
	s8_l thd_2 = g_rwnx_plat->pcidev->tp_thd_2;

	if (value > thd_2)
		return 2;
	else if (value > (thd_2 - BUFFERING_V2) && (current_state == 2))
		return 2;
	else if (value > thd_1 && current_state != 2)
		return 1;
	else if (value > (thd_1 - BUFFERING_V1) && current_state == 1)
		return 1;
	else if (current_state == 0)
		return 0;
	else
		return 1;
}

void aicwf_netif_ctrl(struct aic_pci_dev *pcidev, int val)
{
	unsigned long flags;
	struct rwnx_vif *rwnx_vif;

	if (pcidev->net_stop)
		return;

	spin_lock_irqsave(&pcidev->tx_flow_lock, flags);
	list_for_each_entry(rwnx_vif, &pcidev->rwnx_hw->vifs, list) {
		if (!rwnx_vif || !rwnx_vif->ndev || !rwnx_vif->up)
			continue;
		netif_tx_stop_all_queues(rwnx_vif->ndev);//netif_stop_queue(rwnx_vif->ndev);
	}
	spin_unlock_irqrestore(&pcidev->tx_flow_lock, flags);
	pcidev->net_stop = true;
	mod_timer(&pcidev->netif_timer, jiffies + msecs_to_jiffies(val));

	return;
}

void aicwf_temp_ctrl(struct aic_pci_dev *pcidev)
{
	if (pcidev->set_level) {
		if (pcidev->set_level == 1) {
			pcidev->get_level = 1;
			aicwf_netif_ctrl(pcidev, pcidev->interval_t1/*TMR_INTERVAL_1*/);
			//mdelay(1);
		} else if (pcidev->set_level == 2) {
			pcidev->get_level = 2;
			aicwf_netif_ctrl(pcidev, pcidev->interval_t2/*TMR_INTERVAL_2*/);
			//mdelay(2);
		}
		return;
	} else {
		if (pcidev->cur_temp > (pcidev->tp_thd_1 - 8)) {
			//if ((sdiodev->cur_temp > TEMP_THD_1 && sdiodev->cur_temp <= TEMP_THD_2) || (sdiodev->cur_stat == 1)) {
			if (update_state(pcidev->cur_temp, pcidev->cur_stat) == 1) {
				pcidev->get_level = 1;
				pcidev->cur_stat = 1;
				aicwf_netif_ctrl(pcidev, pcidev->interval_t1/*TMR_INTERVAL_1*/);
				//mdelay(1);
				//break;
			//} else if ((sdiodev->cur_temp > TEMP_THD_2) || (sdiodev->cur_stat == 2)) {
			} else if (update_state(pcidev->cur_temp, pcidev->cur_stat) == 2) {
				pcidev->get_level = 2;
				pcidev->cur_stat = 2;
				aicwf_netif_ctrl(pcidev, pcidev->interval_t2/*TMR_INTERVAL_2*/);
				//mdelay(2);
				//break;
			}
			return;
		}

		if (pcidev->cur_stat) {
			AICWFDBG(LOGINFO, "reset cur_stat");
			pcidev->cur_stat = 0;
			pcidev->get_level = 0;
		}

		return;
	}
}

void aicwf_netif_worker(struct work_struct *work)
{
	struct aic_pci_dev *pcidev = container_of(work, struct aic_pci_dev, netif_work);
	unsigned long flags;
	struct rwnx_vif *rwnx_vif;
	spin_lock_irqsave(&pcidev->tx_flow_lock, flags);
	list_for_each_entry(rwnx_vif, &pcidev->rwnx_hw->vifs, list) {
		if (!rwnx_vif || !rwnx_vif->ndev || !rwnx_vif->up)
			continue;
		netif_tx_wake_all_queues(rwnx_vif->ndev);//netif_wake_queue(rwnx_vif->ndev);
	}
	spin_unlock_irqrestore(&pcidev->tx_flow_lock, flags);
	pcidev->net_stop = false;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static void aicwf_netif_timer(ulong data)
#else
static void aicwf_netif_timer(struct timer_list *t)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
		struct aic_pci_dev *pcidev = (struct aic_pci_dev *) data;
#else
		struct aic_pci_dev *pcidev = from_timer(pcidev, t, netif_timer);
#endif

	if (!work_pending(&pcidev->netif_work))
		schedule_work(&pcidev->netif_work);

	return;
}

void aicwf_temp_ctrl_worker(struct work_struct *work)
{
	struct rwnx_hw *rwnx_hw;
	struct mm_set_vendor_swconfig_cfm cfm;
	struct aic_pci_dev *pcidev = container_of(work, struct aic_pci_dev, tp_ctrl_work);
	rwnx_hw = pcidev->rwnx_hw;
	//AICWFDBG(LOGINFO, "%s\n", __func__);

	if (pcidev->bus_if->state == BUS_DOWN_ST) {
		AICWFDBG(LOGERROR, "%s bus down\n", __func__);
		return;
	}

	spin_lock_bh(&pcidev->tm_lock);
	if (!pcidev->tm_start) {
		spin_unlock_bh(&pcidev->tm_lock);
		AICWFDBG(LOGERROR, "tp_timer should stop_1\n");
		return;
	}
	spin_unlock_bh(&pcidev->tm_lock);


	rwnx_hw->started_jiffies = jiffies;

	rwnx_send_get_temp_req(rwnx_hw, &cfm);
	pcidev->cur_temp = cfm.temp_comp_get_cfm.degree;

	spin_lock_bh(&pcidev->tm_lock);
	if (pcidev->tm_start) {
		mod_timer(&pcidev->tp_ctrl_timer, jiffies + msecs_to_jiffies(TEMP_GET_INTERVAL));
	} else
		AICWFDBG(LOGERROR, "tp_timer should stop_2\n");
	spin_unlock_bh(&pcidev->tm_lock);


	return;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static void aicwf_temp_ctrl_timer(ulong data)
#else
static void aicwf_temp_ctrl_timer(struct timer_list *t)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	struct aic_pci_dev *pcidev = (struct aic_pci_dev *) data;
#else
	struct aic_pci_dev *pcidev = from_timer(pcidev, t, tp_ctrl_timer);
#endif

	if (!work_pending(&pcidev->tp_ctrl_work))
		schedule_work(&pcidev->tp_ctrl_work);

	return;
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
        #ifndef CONFIG_HSTMSI_SUPPORT
        writel(0x10, pciedev->emb_tpci + 0x0ec);
	    #endif
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
			AICWFDBG(LOGERROR, "msg error!!!\n");
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
#ifdef CONFIG_CACHE_GUARD
			kmem_cache_free(rwnx_hw->sw_txhdr_cache, sw_txhdr);
#else
			kfree(sw_txhdr);
#endif
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
	//struct mm_add_if_cfm add_if_cfm;
	//struct mm_set_stack_start_cfm set_start_cfm;
	//struct aicbsp_feature_t feature;
	struct rwnx_hw *rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
	struct rwnx_vif *rwnx_vif;
	struct rwnx_vif *rwnx_vif_param = NULL;
	struct rwnx_cmd *cur = NULL;
	struct rwnx_cmd *nxt = NULL;
    u32 fw_addr = testmode? RAM_LMAC_RF_FW_ADDR : RAM_FMAC_FW_ADDR;

#ifdef CONFIG_USE_BT
    aicwf_bt_init(rwnx_hw);
	mdelay(15);
#endif

	ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, fw_addr, RWNX_PCIE_FW_NAME);
	if (ret) {
		AICWFDBG(LOGERROR, "resume fw load fail\n");
		return ret;
	}

	patch_config(rwnx_hw);
	pcie_reset_firmware(rwnx_hw, fw_addr);

	aicwf_sw_resume();

//	struct rwnx_cmd *cur, *nxt;
	spin_lock_bh(&rwnx_hw->cmd_mgr->lock);
	list_for_each_entry_safe(cur, nxt, &rwnx_hw->cmd_mgr->cmds, list) {
		AICWFDBG(LOGINFO, "resume_cmd_id: %d\n", cur->id);
		list_del(&cur->list);
		rwnx_hw->cmd_mgr->queue_sz--;
		if (!(cur->flags & RWNX_CMD_FLAG_NONBLOCK))
			complete(&cur->complete);
	}
	if(rwnx_hw->pcidev->cmd_mgr.state == RWNX_CMD_MGR_STATE_CRASHED) {
		rwnx_hw->pcidev->cmd_mgr.state = RWNX_CMD_MGR_STATE_INITED;
		AICWFDBG(LOGINFO, "cmd state recovery\n");
	}
	spin_unlock_bh(&rwnx_hw->cmd_mgr->lock);

	list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
		if (rwnx_vif->up) {
			AICWFDBG(LOGINFO, "find vif_up\n");
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
		AICWFDBG(LOGERROR, "%s restore fail\n", __func__);
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
	const u8 *macaddr;
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
		AICWFDBG(LOGINFO, "deinit:macaddr:%x,%x,%x,%x,%x,%x\r\n", macaddr[0], macaddr[1], macaddr[2], \
							   macaddr[3], macaddr[4], macaddr[5]);

		spin_lock_bh(&rx_priv->stas_reord_lock);
		list_for_each_entry_safe(reord_info, tmp, &rx_priv->stas_reord_list, list) {
			macaddr = rwnx_vif->ndev->dev_addr;
			AICWFDBG(LOGINFO, "reord_mac:%x,%x,%x,%x,%x,%x\r\n", reord_info->mac_addr[0], reord_info->mac_addr[1], reord_info->mac_addr[2], \
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
	struct aic_pci_dev *adev = pciedev;
	u16 pci_cmd;
	int ret = -ENODEV;
	u8 linkctrl, k;
	u32 link_state;

	RWNX_DBG(RWNX_FN_ENTRY_STR);
	/* Hotplug fixups */
	pci_read_config_word(pci_dev, PCI_COMMAND, &pci_cmd);
	pci_cmd |= PCI_COMMAND_PARITY | PCI_COMMAND_SERR;
	pci_write_config_word(pci_dev, PCI_COMMAND, pci_cmd);
	pci_write_config_byte(pci_dev, PCI_CACHE_LINE_SIZE, L1_CACHE_BYTES>>2);

	if((ret = pci_enable_device(pci_dev))) {
		dev_err(&(pci_dev->dev), "pci_enable_device failed\n");
		goto out;
	}

	pci_set_master(pci_dev);

	if((ret = pci_request_regions(pci_dev, KBUILD_MODNAME))) {
		dev_err(&(pci_dev->dev), "pci_request_regions failed\n");
		goto out_request;
	}

    #ifdef CONFIG_HSTMSI_SUPPORT
	if(pci_enable_msi(pci_dev)) {
		dev_err(&(pci_dev->dev), "pci_enable_msi failed\n");
		goto out_msi;
	}
    #endif

	switch (pciedev->chip_id) {
		case PRODUCT_ID_AIC8800D80:
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
			break;
		case PRODUCT_ID_AIC8800D80X2:
			adev->pdev = pci_dev;
		#if 0
			adev->bar0 = pci_resource_start(adev->pdev, 0);
			adev->len0 = pci_resource_len  (adev->pdev, 0);
			adev->map0 = ioremap(adev->bar0, adev->len0);
		#else
			pci_read_config_dword (adev->pdev, 0x80, &link_state);
			LOG_INFO("Link ctrl state %x, gen%d\n", link_state, (link_state>>16)&0xF);
            for(k=0; k<10; k++)
            {
                pci_read_config_dword (adev->pdev, 0x10, &(adev->bar0));
                if(adev->bar0 != 0) break;
                LOG_INFO("WARN: bar0 == 0\n");
                aicwf_pcie_print_st(adev, 0);
            }
            adev->len0 = pci_resource_len(adev->pdev, 0);
            if( !(adev->map0 = (u8 *)pci_ioremap_bar(adev->pdev, 0))) {
                   dev_err(&(pci_dev->dev), "pci_ioremap_bar0 failed\n");
                   ret = -ENODEV;
                   goto out_bar0;
            }
        #endif

			pciedev->pci_bar0_vaddr = adev->map0;
			if(adev->map0 == NULL)
			{
				dev_err(&(pci_dev->dev), "pci_ioremap_bar0 failed\n");
				ret = -ENODEV;
				goto out_bar0;
			}
			LOG_INFO("bar0: %x, len = %x, map = %lx", adev->bar0, adev->len0, (unsigned long)adev->map0);
                        LOG_INFO("start %llu end %llu flags %lu len %llu \n",pci_resource_start(adev->pdev, 0),
                                                                                                       pci_resource_end(adev->pdev, 0),
                                                                                                       pci_resource_flags(adev->pdev, 0),
                                                                                                       pci_resource_len(adev->pdev, 0));


			break;
		default:
			LOG_ERROR("chip id not correct\n");
			break;
	}

    ret = request_irq(pci_dev->irq, aicwf_pcie_irq_hdlr,  IRQF_SHARED, "aicwf_pci", pciedev);
    if(ret) {
        LOG_ERROR("request irq fail:%d\n", ret);
        goto out_irq;
    }

	if((pciedev->chip_id == PRODUCT_ID_AIC8800D80) || (pciedev->chip_id == PRODUCT_ID_AIC8800D80X2)) {
		//# by G: msg waitlock at L1
		pci_read_config_byte(pci_dev, pci_dev->pcie_cap + PCI_EXP_LNKCTL, &linkctrl);
		if(linkctrl & 0x02){
			linkctrl = linkctrl & ~0x02;
			pci_write_config_byte(pci_dev, pci_dev->pcie_cap + PCI_EXP_LNKCTL, linkctrl);
		}
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
    LOG_INFO("%s success\n", __func__);

    goto out;
out_irq:
	if(pciedev->chip_id == PRODUCT_ID_AIC8800D80)
        iounmap(pciedev->pci_bar2_vaddr);
out_bar2:
	if(pciedev->chip_id == PRODUCT_ID_AIC8800D80)
        iounmap(pciedev->pci_bar1_vaddr);
out_bar1:
	if(pciedev->chip_id == PRODUCT_ID_AIC8800D80)
        iounmap(pciedev->pci_bar0_vaddr);
out_bar0:
    pci_disable_msi(pci_dev);
#ifdef CONFIG_HSTMSI_SUPPORT
out_msi:
    pci_release_regions(pci_dev);
#endif
out_request:
    pci_disable_device(pci_dev);

out:
    return ret;
}

static void aicwf_pcie_txmsg_db(struct aic_pci_dev *pciedev)
{
	if(pciedev->chip_id == PRODUCT_ID_AIC8800D80) {
        volatile unsigned int *dst_mail = (volatile unsigned int *)(pciedev->pci_bar2_vaddr + 0x800ec);
        dst_mail[0] = 0x1;
	} else
	    writel(1, pciedev->emb_tpci + 0x0ec);
}

static void aicwf_pcie_txdata_db(struct aic_pci_dev *pciedev)
{
	if(pciedev->chip_id == PRODUCT_ID_AIC8800D80) {
        volatile unsigned int *dst_mail = (volatile unsigned int *)(pciedev->pci_bar2_vaddr + 0x800ec);
        dst_mail[0] = 0x2;
	} else
        writel(2, pciedev->emb_tpci + 0x0ec);
}

void aicwf_pcie_vif_down_db(struct aic_pci_dev *pciedev)
{
	if(pciedev->chip_id == PRODUCT_ID_AIC8800D80) {
        volatile unsigned int *dst_mail = (volatile unsigned int *)(pciedev->pci_bar2_vaddr + 0x800ec);
        dst_mail[0] = 0x2;
	} else
        writel(8, pciedev->emb_tpci + 0x0ec);
}

static int aicwf_pcie_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id)
{
	int ret = -ENODEV;
	struct aicwf_bus *bus_if = NULL;
	struct aic_pci_dev *pciedev = NULL;

	RWNX_DBG(RWNX_FN_ENTRY_STR);

	bus_if = kzalloc(sizeof(struct aicwf_bus), GFP_KERNEL);
	if (!bus_if) {
	 LOG_ERROR("alloc bus fail\n");
	 ret = -ENOMEM;
	 goto out;
	}

	pciedev = kzalloc(sizeof(struct aic_pci_dev), GFP_KERNEL);
	if (!pciedev) {
	 LOG_ERROR("alloc pciedev fail\n");
	 kfree(bus_if);
	 ret = -ENOMEM;
	 goto out;
	}

	if(pci_id->device == AIC8800D80_PCI_DEVICE_ID)
		pciedev->chip_id = PRODUCT_ID_AIC8800D80;
	if(pci_id->device == AIC8800D80X2_PCI_DEVICE_ID)
		pciedev->chip_id = PRODUCT_ID_AIC8800D80X2;
	pciedev->bus_if = bus_if;
	bus_if->bus_priv.pci = pciedev;
	dev_set_drvdata(&pci_dev->dev, bus_if);
	pciedev->pci_dev = pci_dev;

#ifdef CONFIG_WS
	register_ws();
#endif

	ret = aicwf_pcie_init(pciedev);
	if(ret) {
		LOG_ERROR("%s: pci init fail\n", __func__);
		goto out;
	}

    if(pciedev->chip_id == PRODUCT_ID_AIC8800D80X2) {
        ret = aicwf_pcie_setst(pciedev);
	    if(ret) {
		    LOG_ERROR("%s: pci set&tst fail\n", __func__);
		    goto out;
	    }
    }
	ret = aicwf_pcie_bus_init(pciedev);
	if(ret) {
		LOG_ERROR("%s: pci bus init fail\n", __func__);
		goto free;
	}

	ret = aicwf_pcie_platform_init(pciedev);
	if(ret) {
		LOG_ERROR("%s: pci plat init fail\n", __func__);
		goto free;
	}
	aicwf_hostif_ready();
	goto out;

free:
#ifdef CONFIG_WS
	unregister_ws();
#endif
	free_irq(pci_dev->irq, pciedev);
	pci_disable_device(pci_dev);
	iounmap(pciedev->pci_bar0_vaddr);
	if(pciedev->chip_id == PRODUCT_ID_AIC8800D80) {
		iounmap(pciedev->pci_bar1_vaddr);
		iounmap(pciedev->pci_bar2_vaddr);
	}
	pci_release_regions(pci_dev);
	pci_clear_master(pci_dev);
	pci_disable_msi(pci_dev);

	if(pciedev->rx_priv)
		aicwf_rx_deinit(pciedev->rx_priv);
	kfree(bus_if);
	if(g_rwnx_plat)
		kfree(g_rwnx_plat);
	kfree(pciedev);
out:
	return ret;
}

static void aicwf_pcie_remove(struct pci_dev *pci_dev)
{
    struct aicwf_bus *bus_if = dev_get_drvdata(&pci_dev->dev);
	struct aic_pci_dev *pci = bus_if->bus_priv.pci;

	LOG_INFO("%s:g_rwnx_plat=%p, %d\n", __func__, g_rwnx_plat, g_rwnx_plat->enabled);

    if (g_rwnx_plat && g_rwnx_plat->enabled) {
        rwnx_platform_deinit(g_rwnx_plat->pcidev->rwnx_hw);
    }

    bus_if->state = BUS_DOWN_ST;

	if (bus_if->cmd_buf) {
		kfree(bus_if->cmd_buf);
		bus_if->cmd_buf = NULL;
	}

    #ifdef CONFIG_LOWPOWER
    struct ipc_shared_env_tag *shared = (struct ipc_shared_env_tag *)(pci->emb_shrm);
    *(volatile uint32_t *)&shared->fw_init_done = 0;
    if(testmode == 1)
        writel(0, pci->emb_sctl + 0x10);
    writel(4, pci->emb_tpci + 0x0ec); //generate an empty int
    #endif

    rwnx_cmd_mgr_deinit(&bus_if->bus_priv.pci->cmd_mgr);

#ifdef CONFIG_WS
	unregister_ws();
#endif
	free_irq(pci_dev->irq, pci);
	pci_disable_device(pci_dev);
	iounmap(pci->pci_bar0_vaddr);
	if(pci->chip_id == PRODUCT_ID_AIC8800D80) {
		iounmap(pci->pci_bar1_vaddr);
		iounmap(pci->pci_bar2_vaddr);
	}
	pci_release_regions(pci_dev);
	pci_clear_master(pci_dev);
	pci_disable_msi(pci_dev);

	if(pci->rx_priv)
		aicwf_rx_deinit(pci->rx_priv);

	kfree(bus_if);
    if(g_rwnx_plat)
        kfree(g_rwnx_plat);
    kfree(pci);
}

static int aicwf_pcie_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int ret = 0, i = 0;
	//g_rwnx_plat->pcidev->rwnx_hw->pci_suspending = 1;
	struct rwnx_hw *rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
	struct rwnx_vif *rwnx_vif;

	RWNX_DBG(RWNX_FN_ENTRY_STR);
	list_for_each_entry(rwnx_vif, &rwnx_hw->vifs, list) {
		if (rwnx_vif->up) {
			while ((int)atomic_read(&rwnx_vif->drv_conn_state) == (int)RWNX_DRV_STATUS_DISCONNECTING) {
				LOG_ERROR("suspend waiting disc\n");
				msleep(100);
				i += 1;
				if (i >= 20) {
					aicwf_disconnect_inform(rwnx_hw, rwnx_vif);
					break;
				}
			}
		}
	}

#ifdef CONFIG_TEMP_CONTROL
	del_timer_sync(&rwnx_hw->pcidev->tp_ctrl_timer);
	cancel_work_sync(&rwnx_hw->pcidev->tp_ctrl_work);

	mod_timer(&rwnx_hw->pcidev->tp_ctrl_timer, jiffies + msecs_to_jiffies(TEMP_GET_INTERVAL));

	del_timer_sync(&rwnx_hw->pcidev->netif_timer);
	cancel_work_sync(&rwnx_hw->pcidev->netif_work);
#endif

	g_rwnx_plat->pcidev->rwnx_hw->pci_suspending = 1;

	spin_lock_bh(&rwnx_hw->cb_lock);
	if (rwnx_hw->scan_request) {// && rwnx_hw->scan_request->wdev == &rwnx_vif->wdev) {
//		printk("suspend scan_done\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		struct cfg80211_scan_info info =
		{
			.aborted = true,
		};
		cfg80211_scan_done (rwnx_hw->scan_request, &info);
#else
		cfg80211_scan_done (rwnx_hw->scan_request, true);
#endif
		LOG_INFO("suspend scan_done\n");
		rwnx_hw->scan_request = NULL;
		scanning = 0;
	}
	spin_unlock_bh(&rwnx_hw->cb_lock);

	ret = pci_save_state(pdev);
	if (ret) {
		LOG_ERROR("failed on pci_save_state %d\n", ret);
		return ret;
	}

	pci_disable_device(pdev);
	ret = pci_set_power_state(pdev, pci_choose_state(pdev, state));
	if (ret)
		LOG_ERROR("failed on pci_set_power_state %d\n", ret);

	return ret;
}

static int aicwf_pcie_resume(struct pci_dev *pdev)
{
	struct rwnx_hw *rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
	bool fw_started;
	int ret = 0;

	LOG_INFO("%s enter: %d\n", __func__, atomic_read(&rwnx_hw->txdata_cnt));

	ret = pci_set_power_state(pdev, PCI_D0);
	if (ret) {
		LOG_ERROR("failed on pci_set_power_state %d\n", ret);
		return ret;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		LOG_ERROR("failed on pci_enable_device %d\n", ret);
		return ret;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 37))
	pci_restore_state(pdev);
#else
	ret = pci_restore_state(pdev);
	if (ret) {
		LOG_ERROR("failed on pci_restore_state %d\n", ret);
		return ret;
	}
#endif
	
	if(g_rwnx_plat->pcidev->chip_id == PRODUCT_ID_AIC8800D80)
	{
		fw_started = *(volatile u32 *) (g_rwnx_plat->pcidev->pci_bar0_vaddr + 0x120000) == 0x1a2000;
	}
	else
	{
		aicwf_pcie_cfg(g_rwnx_plat->pcidev);
		fw_started = 1;
	}

	if(!fw_started) {
		ret = aicwf_resume_access();
		if (ret) {
			LOG_ERROR("resume access fail %d\n", ret);
			return ret;
		}
	} else {
		LOG_INFO("resume skip reload\n");

		g_rwnx_plat->pcidev->rwnx_hw->pci_suspending = 0;

#ifdef CONFIG_TEMP_CONTROL
		mod_timer(&g_rwnx_plat->pcidev->tp_ctrl_timer, jiffies + msecs_to_jiffies(TEMP_GET_INTERVAL));
#endif

		return ret;
	}
	LOG_INFO("%s end\n", __func__);

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
	if (g_rwnx_plat && g_rwnx_plat->enabled && g_rwnx_plat->pcidev){
#ifdef CONFIG_TEMP_CONTROL
		spin_lock_bh(&g_rwnx_plat->pcidev->tm_lock);
		g_rwnx_plat->pcidev->tm_start = 0;
		if (timer_pending(&g_rwnx_plat->pcidev->tp_ctrl_timer)) {
			LOG_INFO("%s del tp_ctrl_timer\n", __func__);
			del_timer_sync(&g_rwnx_plat->pcidev->tp_ctrl_timer);
		}
		spin_unlock_bh(&g_rwnx_plat->pcidev->tm_lock);
		cancel_work_sync(&g_rwnx_plat->pcidev->tp_ctrl_work);

		if (timer_pending(&g_rwnx_plat->pcidev->netif_timer)) {
			LOG_INFO("%s del netif_timer\n", __func__);
			del_timer_sync(&g_rwnx_plat->pcidev->netif_timer);
		}
		cancel_work_sync(&g_rwnx_plat->pcidev->netif_work);
#endif
        rwnx_platform_deinit(g_rwnx_plat->pcidev->rwnx_hw);
	}
	pci_unregister_driver(&aicwf_pci_driver);
}

void rwnx_data_dump(char* tag, void* data, unsigned long len){
        unsigned long i = 0;
        uint8_t* data_ = (uint8_t* )data;

        AICWFDBG(LOGINFO, "%s %s len:(%lu)\r\n", __func__, tag, len);

        for (i = 0; i < len; i += 16){
        AICWFDBG(LOGDEBUG, "%02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
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
	struct rwnx_hw *rwnx_hw = NULL;

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

			AICWFDBG(LOGERROR, "%s trigger \n",__func__);
			//printk("trigger rwnx_hw = %px \n",rwnx_hw);
			if(g_rwnx_plat->pcidev && g_rwnx_plat->pcidev->rwnx_hw) {
				rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
			} else {
				AICWFDBG(LOGERROR, "%s NULL rwnx_hw \n",__func__);
				break;
			}
			if(!rwnx_hw || !rwnx_hw->ipc_env){
				AICWFDBG(LOGERROR, "%s NULL ipc_env \n",__func__);
				break;
			}
			for(;atomic_read(&rwnx_hw->rxbuf_cnt) < rwnx_hw->ipc_env->rxbuf_nb;){
                if (bus_if->state == BUS_DOWN_ST){
                    break;
                }
				if(rwnx_ipc_rxbuf_alloc(rwnx_hw)){
					AICWFDBG(LOGERROR, "pcie_rxbuf_rep_thread rxbuf alloc fail,now rxbuf_cnt = %d \n",atomic_read(&rwnx_hw->rxbuf_cnt));
					msleep(10);
					//break;
				}
			}
			AICWFDBG(LOGTRACE, "%s out \n",__func__);
		}
	}
	return 0;
}

#ifdef CONFIG_RX_SKBLIST
#ifdef CONFIG_RX_TASKLET
void rwnx_task_rx_process(unsigned long data)
{
    struct rwnx_hw *rwnx_hw = (struct rwnx_hw *)data;
    struct aicwf_rx_priv *rx_priv = rwnx_hw->pcidev->rx_priv;
    struct sk_buff *skb, *next_skb, *first, *pick;
    struct sk_buff *skb_new;
    struct hw_rxhdr *hw_rxhdr;
    int i, mpdu_len, offset;

    while (1)
    {
        spin_lock_bh(&rx_priv->rxqlock);
        if (aicwf_is_framequeue_empty(&rx_priv->rxq)) {
            spin_unlock_bh(&rx_priv->rxqlock);
            break;
        }

        pick = skb_peek(&rx_priv->rxq.queuelist[0]);
        hw_rxhdr = (struct hw_rxhdr *)pick->data;
        if(hw_rxhdr->hwvect.reserved > (rx_priv->rxq.qcnt)) {
            //printk("rx wait:%d,%d\n", hw_rxhdr->hwvect.reserved,rx_priv->rxq.qcnt);
            atomic_set(&rwnx_hw->pcidev->rx_priv->rx_wait, 1);
            spin_unlock_bh(&rx_priv->rxqlock);
            break;
        }

        if(atomic_read(&rwnx_hw->pcidev->rx_priv->rx_wait) == 1)
            atomic_set(&rwnx_hw->pcidev->rx_priv->rx_wait, 0);

        skb = aicwf_frame_dequeue(&rx_priv->rxq);
        //printk("deq:%p,%p\n", skb, pick);
        atomic_dec(&rx_priv->rx_cnt);
        spin_unlock_bh(&rx_priv->rxqlock);
        if (skb == NULL) {
            txrx_err("skb_error\r\n");
            break;
        }

        mpdu_len = ((skb->data[1]<<8) |skb->data[0]) + 60;
        //skb_put(skb, mpdu_len);
        //hw_rxhdr = (struct hw_rxhdr *)skb->data;
        if(hw_rxhdr->flags_is_amsdu) {
             //printk("rx amsdu:%x,%x,%d\n",skb->data[1], skb->data[0], hw_rxhdr->hwvect.reserved);

            if(hw_rxhdr->hwvect.reserved <= 1)
                AICWFDBG(LOGINFO, "amsdu but cnt=%x,%x,%d\n",skb->data[1], skb->data[0], hw_rxhdr->hwvect.reserved);

            if(hw_rxhdr->hwvect.reserved > 10)
                AICWFDBG(LOGERROR, "warning: amsdu cnt=%d\n", hw_rxhdr->hwvect.reserved);
            first = skb;
            skb_put(skb, 2048);
            //dev_kfree_skb(skb);
            for(i=0; i<hw_rxhdr->hwvect.reserved-1;i++) {
                spin_lock_bh(&rx_priv->rxqlock);
                while (aicwf_is_framequeue_empty(&rx_priv->rxq)) {
                    //printk("no next skb, wait\n");
                    spin_unlock_bh(&rx_priv->rxqlock);
                    msleep(10);
                    spin_lock_bh(&rx_priv->rxqlock);
                }
                next_skb = aicwf_frame_dequeue(&rx_priv->rxq);
                if(next_skb == NULL) {
                    AICWFDBG(LOGERROR, "should not happen!!!!!!!!!!!\n");
                    spin_unlock_bh(&rx_priv->rxqlock);
                    break;
                }
                //printk("deq1:%p\n", next_skb);
                skb_put(next_skb, 2048);
                atomic_dec(&rx_priv->rx_cnt);
                next_skb->next = NULL;
                skb->next = next_skb;
                skb = next_skb;
                //dev_kfree_skb(next_skb);
                spin_unlock_bh(&rx_priv->rxqlock);
            }
            skb = first;
        }else {
            if(mpdu_len > 2048) {
                AICWFDBG(LOGERROR, "rx len > 2048: %d,%d\n", mpdu_len, hw_rxhdr->hwvect.reserved);
                //rwnx_data_dump(">2048: ", skb->data, 96);
                offset = 0;
                //if fw not discart since no hostbuf, cnt should > 1
                if(hw_rxhdr->hwvect.reserved > 1) {
                    skb_new = dev_alloc_skb(mpdu_len);
                    if(skb_new)
                        skb_put(skb_new, mpdu_len);

                    for(i=0; i<hw_rxhdr->hwvect.reserved-1;i++) {
                        int copy_len = mpdu_len > 2048? 2048 : mpdu_len;
                        if(skb_new)
                            memcpy(skb_new->data + offset, skb->data, copy_len);
                        mpdu_len -= copy_len;
                        offset += copy_len;
                        dev_kfree_skb(skb);
                        spin_lock_bh(&rx_priv->rxqlock);
                        while (aicwf_is_framequeue_empty(&rx_priv->rxq)) {
                            //printk("no next skb, wait\n");
                            spin_unlock_bh(&rx_priv->rxqlock);
                            msleep(10);
                            spin_lock_bh(&rx_priv->rxqlock);
                        }
                        skb = aicwf_frame_dequeue(&rx_priv->rxq);
                        if(skb == NULL) {
                            AICWFDBG(LOGERROR, "should not happen!!!!!!!!!!!\n");
                            spin_unlock_bh(&rx_priv->rxqlock);
                            break;
                        }
                        //printk("deq2:%p\n", skb);
                        atomic_dec(&rx_priv->rx_cnt);
                        //dev_kfree_skb(next_skb);
                        spin_unlock_bh(&rx_priv->rxqlock);
                    }
                    skb = skb_new;
                }else {
                    skb_put(skb, 2048);
                }
            }else {
                skb_put(skb, mpdu_len);
            }
        }

        //printk("rxdataind:%p,%p,%p\n", rwnx_hw, skb, rx_priv);
        if(skb)
            rwnx_rxdataind_aicwf(rwnx_hw, skb, (void *)rx_priv);
    }
}
#else
int pcie_rxbuf_process_thread(void *data)
{
	struct aicwf_rx_priv *rx_priv = (struct aicwf_rx_priv *)data;
	struct aicwf_bus *bus_if = rx_priv->pciedev->bus_if;
	struct rwnx_hw *rwnx_hw = NULL;
    struct sk_buff *skb, *next_skb, *first,*pick;
    struct sk_buff *skb_new;
    struct hw_rxhdr *hw_rxhdr;
    int i, mpdu_len, offset;

	while (1) {
		if (kthread_should_stop()) {
			AICWFDBG(LOGERROR, "pcie busrx thread stop\n");
			break;
		}
		if (!wait_for_completion_interruptible(&bus_if->rx_trgg)) {
			if (bus_if->state == BUS_DOWN_ST)
				continue;
            rwnx_hw = g_rwnx_plat->pcidev->rwnx_hw;
            while (1) {
                spin_lock_bh(&rx_priv->rxqlock);
                if (aicwf_is_framequeue_empty(&rx_priv->rxq)) {
                    spin_unlock_bh(&rx_priv->rxqlock);
                    break;
                }

				pick = skb_peek(&rx_priv->rxq.queuelist[0]);
				hw_rxhdr = (struct hw_rxhdr *)pick->data;
				if(hw_rxhdr->hwvect.reserved > (rx_priv->rxq.qcnt)) {
				    //printk("rx wait:%d,%d\n", hw_rxhdr->hwvect.reserved,rx_priv->rxq.qcnt);
				    atomic_set(&rwnx_hw->pcidev->rx_priv->rx_wait, 1);
				    spin_unlock_bh(&rx_priv->rxqlock);
				    break;
				}

				if(atomic_read(&rwnx_hw->pcidev->rx_priv->rx_wait) == 1)
					atomic_set(&rwnx_hw->pcidev->rx_priv->rx_wait, 0);

                skb = aicwf_frame_dequeue(&rx_priv->rxq);
                //printk("deq:%p,%p\n", skb, pick);
                atomic_dec(&rx_priv->rx_cnt);
                spin_unlock_bh(&rx_priv->rxqlock);
                if (skb == NULL) {
                    txrx_err("skb_error\r\n");
                    break;
                }

                mpdu_len = ((skb->data[1]<<8) |skb->data[0]) + 60;
                //skb_put(skb, mpdu_len);
                //hw_rxhdr = (struct hw_rxhdr *)skb->data;
                if(hw_rxhdr->flags_is_amsdu) {
                     //printk("rx amsdu:%x,%x,%d\n",skb->data[1], skb->data[0], hw_rxhdr->hwvect.reserved);

                    if(hw_rxhdr->hwvect.reserved <= 1)
                        AICWFDBG(LOGINFO, "amsdu but cnt=%x,%x,%d\n",skb->data[1], skb->data[0], hw_rxhdr->hwvect.reserved);

                    if(hw_rxhdr->hwvect.reserved > 10)
                        AICWFDBG(LOGERROR, "warning: amsdu cnt=%d\n", hw_rxhdr->hwvect.reserved);
                    first = skb;
                    skb_put(skb, 2048);
                    //dev_kfree_skb(skb);
                    for(i=0; i<hw_rxhdr->hwvect.reserved-1;i++) {
                        spin_lock_bh(&rx_priv->rxqlock);
                        while (aicwf_is_framequeue_empty(&rx_priv->rxq)) {
                            //printk("no next skb, wait\n");
                            spin_unlock_bh(&rx_priv->rxqlock);
                            msleep(10);
                            spin_lock_bh(&rx_priv->rxqlock);
                        }
                        next_skb = aicwf_frame_dequeue(&rx_priv->rxq);
                        if(next_skb == NULL) {
                            AICWFDBG(LOGERROR, "should not happen!!!!!!!!!!!\n");
                            spin_unlock_bh(&rx_priv->rxqlock);
                            break;
                        }
                        //printk("deq1:%p\n", next_skb);
                        skb_put(next_skb, 2048);
                        atomic_dec(&rx_priv->rx_cnt);
                        next_skb->next = NULL;
                        skb->next = next_skb;
                        skb = next_skb;
                        //dev_kfree_skb(next_skb);
                        spin_unlock_bh(&rx_priv->rxqlock);
                    }
                    skb = first;
                }else {
                    if(mpdu_len > 2048) {
                        AICWFDBG(LOGERROR, "rx len > 2048: %d,%d\n", mpdu_len, hw_rxhdr->hwvect.reserved);
                        //rwnx_data_dump(">2048: ", skb->data, 96);
                        offset = 0;
                        //if fw not discart since no hostbuf, cnt should > 1
                        if(hw_rxhdr->hwvect.reserved > 1) {
                            skb_new = dev_alloc_skb(mpdu_len);
                            if(skb_new)
                                skb_put(skb_new, mpdu_len);

                            for(i=0; i<hw_rxhdr->hwvect.reserved-1;i++) {
                                int copy_len = mpdu_len > 2048? 2048 : mpdu_len;
                                if(skb_new)
                                    memcpy(skb_new->data + offset, skb->data, copy_len);
                                mpdu_len -= copy_len;
                                offset += copy_len;
                                dev_kfree_skb(skb);
                                spin_lock_bh(&rx_priv->rxqlock);
                                while (aicwf_is_framequeue_empty(&rx_priv->rxq)) {
                                    //printk("no next skb, wait\n");
                                    spin_unlock_bh(&rx_priv->rxqlock);
                                    msleep(10);
                                    spin_lock_bh(&rx_priv->rxqlock);
                                }
                                skb = aicwf_frame_dequeue(&rx_priv->rxq);
                                if(skb == NULL) {
                                    AICWFDBG(LOGERROR, "should not happen!!!!!!!!!!!\n");
                                    spin_unlock_bh(&rx_priv->rxqlock);
                                    break;
                                }
                                //printk("deq2:%p\n", skb);
                                atomic_dec(&rx_priv->rx_cnt);
                                //dev_kfree_skb(next_skb);
                                spin_unlock_bh(&rx_priv->rxqlock);
                            }
                            skb = skb_new;
                        }else {
                            skb_put(skb, 2048);
                        }
                    }else {
                        skb_put(skb, mpdu_len);
                    }
                }

                //printk("rxdataind:%p,%p,%p\n", rwnx_hw, skb, rx_priv);
                if(skb)
                    rwnx_rxdataind_aicwf(rwnx_hw, skb, (void *)rx_priv);
            }

			//printk("%s\n",__func__);
		}
	}

	return 0;
}
#endif
#endif

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

int aicwf_pcie_bus_init(struct aic_pci_dev *pciedev)
{
	struct aicwf_bus *bus_if = pciedev->bus_if;
	int ret = 0;
	struct aicwf_rx_priv* rx_priv = NULL;
    bus_if->dev = &pciedev->pci_dev->dev;
    bus_if->ops = &aicwf_pcie_bus_ops;
    bus_if->state = BUS_UP_ST;

    //struct aicwf_rx_priv* rx_priv;
    rx_priv = aicwf_rx_init(pciedev);
    if(!rx_priv) {
        txrx_err("rx init failed\n");
        ret = -1;
        //goto out_free_bus;
    }
    pciedev->rx_priv = rx_priv;

#ifdef CONFIG_TEMP_CONTROL
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	init_timer(&pciedev->tp_ctrl_timer);
	pciedev->tp_ctrl_timer.data = (ulong) pciedev;
	pciedev->tp_ctrl_timer.function = aicwf_temp_ctrl_timer;
	init_timer(&pciedev->netif_timer);
	pciedev->netif_timer.data = (ulong) pciedev;
	pciedev->netif_timer.function = aicwf_netif_timer;
#else
	timer_setup(&pciedev->tp_ctrl_timer, aicwf_temp_ctrl_timer, 0);
	timer_setup(&pciedev->netif_timer, aicwf_netif_timer, 0);
#endif
	INIT_WORK(&pciedev->tp_ctrl_work, aicwf_temp_ctrl_worker);
	INIT_WORK(&pciedev->netif_work, aicwf_netif_worker);
	mod_timer(&pciedev->tp_ctrl_timer, jiffies + msecs_to_jiffies(TEMP_GET_INTERVAL));
	spin_lock_init(&pciedev->tm_lock);
	pciedev->net_stop = false;;
	pciedev->on_off = true;
	pciedev->cur_temp = 0;
	pciedev->get_level = 0;
	pciedev->set_level = 0;
	pciedev->interval_t1 = TMR_INTERVAL_1;
	pciedev->interval_t2 = TMR_INTERVAL_2;
	pciedev->cur_stat = 0;
	pciedev->tp_thd_1 = TEMP_THD_1;
	pciedev->tp_thd_2 = TEMP_THD_2;
	pciedev->tm_start = 1;
#endif

	ret = aicwf_bus_init(0, &pciedev->pci_dev->dev);
	if(ret)
		LOG_ERROR("%s fail\n", __func__);
	return ret;
}

#endif

