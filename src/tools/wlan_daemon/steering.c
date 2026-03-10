#include "manager.h"
#include "steering.h"

#ifdef CONFIG_WIFI_BAND_STEERING

static struct b_steer_context B_CTX = {0};

static void send_b_steer_cmd_msg(u8 band, u8 ssid)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {0};

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	intf.band = band;
	intf.ssid = ssid;
	manager_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* finish message */
	msg.type = AIC_NL_B_STEER_CMD_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len = msg.len + 8;
	manager_send_daemon((void *)&msg, msg_len, NL_AIC_MANAGER_PID);

	return;
}

static void send_b_steer_block_add_msg(u8 *mac, u8 band, u8 ssid)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {0};
	struct b_elm_sta_info sta_info = {0};

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	intf.band = band;
	intf.ssid = ssid;
	manager_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_STA_INFO_ID;
	hdr.len = ELM_STA_INFO_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_STA_INFO_ID */
	os_memcpy(sta_info.mac, mac, 6);
	manager_set_msg(&msg, &msg_len, (void *)&sta_info, ELM_STA_INFO_LEN);

	/* finish message */
	msg.type = AIC_NL_B_STEER_BLOCK_ADD_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;

	DBG_MSG_PRINT(B_STEER_STR, "send_drv type: %d", msg.type);
	manager_send_drv((void *)&msg, msg_len);
#ifdef CONFIG_SEC_NLCMD
	manager_send_sec_drv((void *)&msg, msg_len);
#endif
	return;
}

static void send_b_steer_block_del_msg(u8 *mac, u8 band, u8 ssid)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {0};
	struct b_elm_sta_info sta_info = {0};

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	intf.band = band;
	intf.ssid = ssid;
	manager_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_STA_INFO_ID;
	hdr.len = ELM_STA_INFO_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_STA_INFO_ID */
	os_memcpy(sta_info.mac, mac, 6);
	manager_set_msg(&msg, &msg_len, (void *)&sta_info, ELM_STA_INFO_LEN);

	/* finish message */
	msg.type = AIC_NL_B_STEER_BLOCK_DEL_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;

	DBG_MSG_PRINT(B_STEER_STR, "send_drv type: %d", msg.type);
	manager_send_drv((void *)&msg, msg_len);
#ifdef CONFIG_SEC_NLCMD
	manager_send_sec_drv((void *)&msg, msg_len);
#endif
	return;
}

static void send_b_steer_roam_msg(
	u8 *sta_mac, u8 band, u8 ssid, u8 *bss_mac, u16 bss_ch,
	u16 bss_info, u8 bss_reg_class, u8 bss_phy_type,
	u8 method, s8 *intf_name)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {0};
	struct b_elm_roam_info roam_info = {0};

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	intf.band = band;
	intf.ssid = ssid;
	manager_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_ROAM_INFO_ID;
	hdr.len = ELM_ROAM_INFO_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_ROAM_INFO_ID */
	os_memcpy(roam_info.sta_mac, sta_mac, 6);
	os_memcpy(roam_info.bss_mac, bss_mac, 6);
	roam_info.bss_ch = bss_ch;
	roam_info.method = method;
	manager_set_msg(&msg, &msg_len, (void *)&roam_info, ELM_ROAM_INFO_LEN);

	/* finish message */
	msg.type = AIC_NL_B_STEER_ROAM_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;

	DBG_MSG_PRINT(B_STEER_STR, "%s, send_drv type: %d", __func__, msg.type);
	manager_send_drv((void *)&msg, msg_len);
#ifdef CONFIG_SEC_NLCMD
	manager_send_sec_drv((void *)&msg, msg_len);
#endif
	DBG_MSG_INFO(B_STEER_STR, "AIC_NL_B_STEER_ROAM_TYPE (hostapd_cli)!");

	if (method == 0)
		manager_hostapd_cli_bss_tm_req(
			intf_name, sta_mac, bss_mac, bss_ch,
			bss_info, bss_reg_class, bss_phy_type,
			B_CTX.bss_tm_req_disassoc_imminent, B_CTX.bss_tm_req_disassoc_timer);
	else
		manager_hostapd_cli_deauth(intf_name, sta_mac);

	return;
}


static struct b_steer_ignore_entry *b_non_prefer_ignore_entry_lookup(
	u8 *mac, u8 ssid)
{
	u8 i;
	struct b_steer_ignore_entry *ent = NULL;

	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		ent = &(B_CTX.np_band_ignore_list[ssid][i]);
		if (ent->used && !os_memcmp(mac, ent->mac, 6))
			return ent;
	}

	return NULL;
}

static void b_non_prefer_ignore_entry_add(u8 *mac, u8 ssid)
{
	u8 i;
	struct b_steer_ignore_entry *ent = NULL;

	ent = b_non_prefer_ignore_entry_lookup(mac, ssid);

	/* already exist */
	if (ent)
		goto func_return;

	/* find an empty entry */
	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		if (!B_CTX.np_band_ignore_list[ssid][i].used) {
			ent = &(B_CTX.np_band_ignore_list[ssid][i]);
			break;
		}
	}

	/* add the entry */
	if (ent) {
		ent->used = 1;
		os_memcpy(ent->mac, mac, 6);
		ent->entry_exp = B_STEER_ENTRY_IGNORE_PERIOD_COUNT;
	}

func_return:
	return;
}

static struct b_steer_non_prefer_band_entry *b_non_prefer_entry_lookup(
	u8 *mac, u8 ssid)
{
	u8 i;
	struct b_steer_non_prefer_band_entry *ent = NULL;

	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		ent = &(B_CTX.np_band_list[ssid][i]);
		if (ent->used && !os_memcmp(mac, ent->mac, 6))
			return ent;
	}

	return NULL;
}

static void b_non_prefer_entry_add(u8 *mac, u8 ssid)
{
	u8 i;
	struct b_steer_non_prefer_band_entry *ent = NULL;

	DBG_MSG_INFO(B_STEER_STR, "%s "MAC_FMT" ssid: %d", __func__, MAC_ARG(mac), ssid);

	/* ignore */
	if (b_non_prefer_ignore_entry_lookup(mac, ssid))
		goto func_return;

	ent = b_non_prefer_entry_lookup(mac, ssid);
	/* already exist */
	if (ent) {
		DBG_MSG_INFO(B_STEER_STR, "%s already exist", __func__);
		send_b_steer_block_add_msg(mac, B_CTX.non_prefer_band, ssid);
		goto func_return;
	}

	/* find an empty entry */
	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		if (!B_CTX.np_band_list[ssid][i].used) {
			ent = &(B_CTX.np_band_list[ssid][i]);
			break;
		}
	}

	/* add the entry */
	if (ent) {
		ent->used = 1;
		os_memcpy(ent->mac, mac, 6);
		ent->entry_exp_phase1 = B_CTX.entry_exp_phase1;
		ent->assoc_rty_lmt = B_CTX.assoc_rty_lmt;
		ent->entry_exp_phase2 = 0;
		send_b_steer_block_add_msg(mac, B_CTX.non_prefer_band, ssid);
	}

func_return:
	return;
}

static void b_non_prefer_entry_del(u8 *mac, u8 ssid)
{
	struct b_steer_non_prefer_band_entry *ent = NULL;

	DBG_MSG_INFO(B_STEER_STR, "%s "MAC_FMT" ssid: %d", __func__, MAC_ARG(mac), ssid);

	ent = b_non_prefer_entry_lookup(mac, ssid);
	if (ent) {
		ent->used = 0;
		send_b_steer_block_del_msg(mac, B_CTX.non_prefer_band, ssid);
	}

	return;
}

static void b_non_prefer_expire(u8 ssid)
{
	u8 i;
	struct b_steer_ignore_entry *ent1 = NULL;
	struct b_steer_non_prefer_band_entry *ent2 = NULL;

	/* non prefer band ignore entry expire */
	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		ent1 = &(B_CTX.np_band_ignore_list[ssid][i]);
		if (!ent1->used)
			continue;

		if (ent1->entry_exp) {
			ent1->entry_exp--;
			if (ent1->entry_exp == 0)
				ent1->used = 0;
		}
	}

	/* non prefer band entry expire */
	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		ent2 = &(B_CTX.np_band_list[ssid][i]);
		if (!ent2->used)
			continue;
		
		if (ent2->entry_exp_phase2) {
			ent2->entry_exp_phase2--;
			if (ent2->entry_exp_phase2 == 0) {
				ent2->used = 0;
				b_non_prefer_ignore_entry_add(ent2->mac, ssid);
				send_b_steer_block_del_msg(
					ent2->mac, B_CTX.non_prefer_band, ssid);
			}
		}
		else if (ent2->entry_exp_phase1) {
			ent2->entry_exp_phase1--;
			if (ent2->entry_exp_phase1 == 0)
				ent2->entry_exp_phase2 = B_CTX.entry_exp_phase2;
		}
	}

	return;
}

static void b_non_prefer_on_assocreq(u8 *mac, u8 ssid)
{
	struct b_steer_non_prefer_band_entry *ent = NULL;

	ent = b_non_prefer_entry_lookup(mac, ssid);
	if (ent) {
		if (ent->assoc_rty_lmt) {
			ent->assoc_rty_lmt--;
			if (ent->assoc_rty_lmt == 0)
				ent->entry_exp_phase2 = B_CTX.entry_exp_phase2;
		}
	}

	return;
}

static void b_non_prefer_roam_detect(struct b_steer_priv *priv)
{
	u8  i;
	u32 sta_tp;
	struct b_steer_priv *grp_priv = priv->grp_priv;
	struct b_steer_sta *sta = NULL;

	for (i = 0; i < MAX_STA_NUM; i++) {
		sta = &(priv->sta_list[i]);
		if (!sta->used || !sta->is_dual_band)
			continue;

		sta_tp = sta->tx_tp + sta->rx_tp;

		if ((B_CTX.roam_sta_tp_th && sta_tp > B_CTX.roam_sta_tp_th)
			|| (B_CTX.roam_ch_clm_th && grp_priv->ch_clm > B_CTX.roam_ch_clm_th)) {
			sta->b_steer_roam_detect = 0;
			DBG_MSG_DEBUG(B_STEER_STR, "non_prefer_roam_detect case_1");
		}
		else if (sta->rssi > B_CTX.prefer_band_rssi_high) {
			sta->b_steer_roam_detect++;
			DBG_MSG_DEBUG(B_STEER_STR, "non_prefer_roam_detect case_2");
		}
		else if (sta->b_steer_roam_detect > 1) {
			sta->b_steer_roam_detect -= 2;
			DBG_MSG_DEBUG(B_STEER_STR, "non_prefer_roam_detect case_3");
		}
		else {
			sta->b_steer_roam_detect = 0;
			DBG_MSG_DEBUG(B_STEER_STR, "non_prefer_roam_detect case_4");
		}

		if (sta->b_steer_roam_detect > B_CTX.roam_detect_th) {
			sta->b_steer_roam_detect = B_CTX.roam_detect_th;
			DBG_MSG_DEBUG(B_STEER_STR, "non_prefer_roam_detect case_5");
		}

		DBG_MSG_INFO(B_STEER_STR, "mac="MAC_FMT" %s non-prefer b_steer_roam_detect=%u rssi=%d",
					MAC_ARG(sta->mac), priv->name, sta->b_steer_roam_detect, sta->rssi);
	}	

	return;
}

static struct b_steer_prefer_band_entry *b_prefer_band_entry_lookup(
	u8 *mac, u8 ssid)
{
	u8 i;
	struct b_steer_prefer_band_entry *ent = NULL;

	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		ent = &(B_CTX.p_band_list[ssid][i]);
		if (ent->used && !os_memcmp(mac, ent->mac, 6))
			return ent;
	}

	return NULL;
}

static void b_prefer_band_entry_add(u8 *mac, s8 rssi, u8 ssid)
{
	u8  i;
	u32 tmp_i = 0;
	u32 tmp_aging = 0;
	struct b_steer_prefer_band_entry *ent = NULL;

	ent = b_prefer_band_entry_lookup(mac, ssid);
	/* already exist */
	if (ent) {
		ent->rssi = rssi;
		ent->aging = 0;
		goto func_return;
	}

	/* find an empty entry */
	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		if (!B_CTX.p_band_list[ssid][i].used) {
			ent = &(B_CTX.p_band_list[ssid][i]);
			break;
		}
	}

	/* no empty, find the oldest one */
	if (ent == NULL) {
		for (i = 0; i < B_STEER_MAX_NUM; i++) {
			if (tmp_aging < B_CTX.p_band_list[ssid][i].aging) {
				tmp_aging = B_CTX.p_band_list[ssid][i].aging;
				tmp_i = i;
			}
		}
		ent = &(B_CTX.p_band_list[ssid][tmp_i]);
	}

	/* add the entry */
	ent->used = 1;
	os_memcpy(ent->mac, mac, 6);
	ent->rssi = rssi;
	ent->aging = 0;

func_return:
	return;
}

static void b_prefer_band_entry_del(u8 *mac, u8 ssid)
{
	struct b_steer_prefer_band_entry *ent = NULL;

	ent = b_prefer_band_entry_lookup(mac, ssid);
	if (ent)
		ent->used = 0;

	return;
}

static void b_prefer_band_expire(u8 ssid)
{
	u8 i;
	struct b_steer_prefer_band_entry *ent = NULL;

	for (i = 0; i < B_STEER_MAX_NUM; i++) {
		ent = &(B_CTX.p_band_list[ssid][i]);
		if (!ent->used)
			continue;

		ent->aging++;
		if (ent->aging > B_STEER_PREFER_B_TIMEOUT_COUNT)
			ent->used = 0;
	}

	return;
}

static void b_prefer_band_roam_detect(struct b_steer_priv *priv)
{
	u8 i;
	u32 sta_tp;
	struct b_steer_sta *sta = NULL;

	for (i = 0; i < MAX_STA_NUM; i++) {
		sta = &(priv->sta_list[i]);
		if (!sta->used || !sta->is_dual_band)
			continue;

		sta_tp = sta->tx_tp + sta->rx_tp;

		if (B_CTX.roam_sta_tp_th && sta_tp > B_CTX.roam_sta_tp_th) {
			sta->b_steer_roam_detect = 0;
			DBG_MSG_DEBUG(B_STEER_STR, "roam_detect case_1");
		}
		else if (sta->rssi < B_CTX.prefer_band_rssi_low) {
			sta->b_steer_roam_detect++;
			DBG_MSG_DEBUG(B_STEER_STR, "roam_detect case_2");
		}
		else if (sta->b_steer_roam_detect > 1) {
			sta->b_steer_roam_detect -= 2;
			DBG_MSG_DEBUG(B_STEER_STR, "roam_detect case_3");
		}
		else {
			sta->b_steer_roam_detect = 0;
			DBG_MSG_DEBUG(B_STEER_STR, "roam_detect case_4");
		}

		if (sta->b_steer_roam_detect > B_CTX.roam_detect_th) {
			sta->b_steer_roam_detect = B_CTX.roam_detect_th;
			DBG_MSG_DEBUG(B_STEER_STR, "roam_detect case_5");
		}

		DBG_MSG_INFO(B_STEER_STR, "mac="MAC_FMT" %s prefer b_steer_roam_detect=%u rssi=%d",
					MAC_ARG(sta->mac), priv->name, sta->b_steer_roam_detect, sta->rssi);
	}

	return;
}

static void b_steering_on_probereq(u8 *mac, s8 rssi, u8 band, u8 ssid)
{
	u8 grp_ssid = B_CTX.prefer_band_grp_ssid[ssid];

	
	DBG_MSG_DEBUG(B_STEER_STR, "%s "MAC_FMT" rssi: %d, band: %d, ssid: %d",
		__func__, MAC_ARG(mac), rssi, band, ssid);

	/* prefer band */
	if (band == B_CTX.prefer_band) {
		b_prefer_band_entry_add(mac, rssi, ssid);

		if (rssi < B_CTX.prefer_band_rssi_low)
			b_non_prefer_entry_del(mac, grp_ssid);
		else if (rssi > B_CTX.prefer_band_rssi_high)
			b_non_prefer_entry_add(mac, grp_ssid);
	}

	return;
}

static void b_steering_on_assocreq(u8 *mac, s8 rssi, u8 band, u8 ssid)
{
	u8 grp_ssid = B_CTX.prefer_band_grp_ssid[ssid];

	DBG_MSG_INFO(B_STEER_STR, "%s "MAC_FMT" rssi: %d, band: %d, ssid: %d",
		__func__, MAC_ARG(mac), rssi, band, ssid);

	/* prefer band */
	if (band == B_CTX.prefer_band) {
		b_prefer_band_entry_add(mac, rssi, ssid);

		if (rssi < B_CTX.prefer_band_rssi_low)
			b_non_prefer_entry_del(mac, grp_ssid);
		else if (rssi > B_CTX.prefer_band_rssi_high)
			b_non_prefer_entry_add(mac, grp_ssid);
	}
	/* non prefer band */
	else if (band == B_CTX.non_prefer_band) {
		b_non_prefer_on_assocreq(mac, ssid);
	}

	return;
}

static void b_steering_on_auth(u8 *mac, s8 rssi, u8 band, u8 ssid)
{
	u8 grp_ssid = B_CTX.prefer_band_grp_ssid[ssid];

	DBG_MSG_INFO(B_STEER_STR, "%s "MAC_FMT" rssi: %d, band: %d, ssid: %d",
		__func__, MAC_ARG(mac), rssi, band, ssid);

	/* prefer band */
	if (band == B_CTX.prefer_band) {
		b_prefer_band_entry_add(mac, rssi, ssid);

		if (rssi < B_CTX.prefer_band_rssi_low)
			b_non_prefer_entry_del(mac, grp_ssid);
		else if (rssi > B_CTX.prefer_band_rssi_high)
			b_non_prefer_entry_add(mac, grp_ssid);
	}

	return;
}

static void b_steering_roam_start(struct b_steer_priv *priv)
{
	u8 i;
	u8 method;
	u8 roam_num = 0;
	struct b_steer_priv *grp_priv = priv->grp_priv;
	struct b_steer_sta *sta = NULL;

	for (i = 0; i < MAX_STA_NUM; i++) {
		sta = &(priv->sta_list[i]);
		if (!sta->used)
			continue;

		if (sta->b_steer_roam_detect >= B_CTX.roam_detect_th) {
			method = (sta->b_steer_roam_cnt < B_CTX.bss_tm_req_th) ? 0 : 1;

			send_b_steer_roam_msg(
				sta->mac, priv->band, priv->ssid,
				grp_priv->mac, grp_priv->ch,
				grp_priv->self_nb_rpt.bss_info,
				grp_priv->self_nb_rpt.reg_class,
				grp_priv->self_nb_rpt.phy_type,
				method, priv->name);

			sta->b_steer_roam_cnt++;
			sta->b_steer_roam_detect = B_CTX.roam_detect_th / 2;

			roam_num++;
			if (roam_num == B_STEER_ROAM_STA_COUNT)
				break;
		}
	}

	return;
}

static void b_steering_config_fill(const s8 *buf, s8 *pos)
{
	/* TBD: should check the data type and data size */

	if (!os_strcmp(buf, "prefer_band")) {
		if (!os_strcmp(pos, "5G")) {
			B_CTX.prefer_band = BAND_ON_5G;
			B_CTX.non_prefer_band = BAND_ON_24G;
		}
		else if (!os_strcmp(pos, "24G")) {
			B_CTX.prefer_band = BAND_ON_24G;
			B_CTX.non_prefer_band = BAND_ON_5G;
		}
		else {
			DBG_MSG_WARN(B_STEER_STR, "invalid config: %s.", pos);
		}
	}
	else if (!os_strcmp(buf, "bss_tm_req_th")) {
		B_CTX.bss_tm_req_th = atoi(pos);
	}
	else if (!os_strcmp(buf, "bss_tm_req_disassoc_imminent")) {
		B_CTX.bss_tm_req_disassoc_imminent = atoi(pos);
	}
	else if (!os_strcmp(buf, "bss_tm_req_disassoc_timer")) {
		B_CTX.bss_tm_req_disassoc_timer = atoi(pos);
	}
	else if (!os_strcmp(buf, "roam_sta_tp_th")) {
		B_CTX.roam_sta_tp_th = atoi(pos);
	}
	else if (!os_strcmp(buf, "roam_detect_th")) {
		B_CTX.roam_detect_th = atoi(pos);
	}
	else if (!os_strcmp(buf, "roam_ch_clm_th")) {
		B_CTX.roam_ch_clm_th = atoi(pos);
	}
	else if (!os_strcmp(buf, "prefer_band_rssi_high")) {
		B_CTX.prefer_band_rssi_high = atoi(pos);
	}
	else if (!os_strcmp(buf, "prefer_band_rssi_low")) {
		B_CTX.prefer_band_rssi_low = atoi(pos);
	}
	else if (!os_strcmp(buf, "entry_exp_phase1")) {
		B_CTX.entry_exp_phase1 = atoi(pos);
	}
	else if (!os_strcmp(buf, "assoc_rty_lmt")) {
		B_CTX.assoc_rty_lmt = atoi(pos);
	}
	else if (!os_strcmp(buf, "entry_exp_phase2")) {
		B_CTX.entry_exp_phase2 = atoi(pos);
	}
	else if (!os_strcmp(buf, "prefer_band_grp_ssid0")) {
		B_CTX.prefer_band_grp_ssid[0] = atoi(pos);
	}
	else if (!os_strcmp(buf, "non_prefer_band_grp_ssid0")) {
		B_CTX.non_prefer_band_grp_ssid[0] = atoi(pos);
	}
#if (SSID_NUM > 1)
	else if (!os_strcmp(buf, "prefer_band_grp_ssid1")) {
		B_CTX.prefer_band_grp_ssid[1] = atoi(pos);
	}
	else if (!os_strcmp(buf, "non_prefer_band_grp_ssid1")) {
		B_CTX.non_prefer_band_grp_ssid[1] = atoi(pos);
	}
#endif
#if (SSID_NUM > 2) 
	else if (!os_strcmp(buf, "prefer_band_grp_ssid2")) {
		B_CTX.prefer_band_grp_ssid[2] = atoi(pos);
	}
	else if (!os_strcmp(buf, "non_prefer_band_grp_ssid2")) {
		B_CTX.non_prefer_band_grp_ssid[2] = atoi(pos);
	}
#endif
#if (SSID_NUM > 3)
	else if (!os_strcmp(buf, "prefer_band_grp_ssid3")) {
		B_CTX.prefer_band_grp_ssid[3] = atoi(pos);
	}
	else if (!os_strcmp(buf, "non_prefer_band_grp_ssid3")) {
		B_CTX.non_prefer_band_grp_ssid[3] = atoi(pos);
	}
#endif
#if (SSID_NUM > 4)
	else if (!os_strcmp(buf, "prefer_band_grp_ssid4")) {
		B_CTX.prefer_band_grp_ssid[4] = atoi(pos);
	}
	else if (!os_strcmp(buf, "non_prefer_band_grp_ssid4")) {
		B_CTX.non_prefer_band_grp_ssid[4] = atoi(pos);
	}
#endif
	else {
		DBG_MSG_WARN(B_STEER_STR, "unknown config: %s.", buf);
	}

	return;
}

static void b_steering_config_read(const s8 *fname)
{
	FILE *fp;
	s8 buf[4096];
	s8 *pos;
	
	fp = fopen(fname, "r");
	if (fp == NULL)
		return;

	while (fgets(buf, sizeof(buf), fp)) {
		if (buf[0] == '#')
			continue;

		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		pos = os_strchr(buf, '=');
		if (pos == NULL)
			continue;

		*pos = '\0';
		pos++;
		b_steering_config_fill(buf, pos);
	}

	fclose(fp);

	return;
}


void b_steering_parse_arg(u8 *argn, s32 argc, char *argv[])
{
	u8 band = 255;
	u8 ssid = 255;

	if (*argn + 2 >= argc) {
		DBG_MSG_WARN(B_STEER_STR, "wrong argument.");
		return;
	}

	if (!os_strcmp(argv[*argn + 1], "2g"))
		band = BAND_ON_24G;
	else if (!os_strcmp(argv[*argn + 1], "5g"))
		band = BAND_ON_5G;

	ssid = argv[*argn + 2][0] - '0';

	send_b_steer_cmd_msg(band, ssid);

	*argn = *argn + 2;

	return;
}

void b_steering_mng_on_frame_rpt(
	struct b_steer_priv *priv, u16 frame_type, u8 *mac, s8 rssi)
{
	u8 band = priv->band;
	u8 ssid = priv->ssid;

	if (!priv->band_steering_enable)
		return;

	if (frame_type == WIFI_PROBEREQ)
		b_steering_on_probereq(mac, rssi, band, ssid);
	else if (frame_type == WIFI_ASSOCREQ)
		b_steering_on_assocreq(mac, rssi, band, ssid);
	else if (frame_type == WIFI_AUTH)
		b_steering_on_auth(mac, rssi, band, ssid);

	return;
}

void b_steering_on_time_tick(struct b_steer_priv *priv)
{
	u8 band = priv->band;
	u8 ssid = priv->ssid;

	if (!priv->band_steering_enable)
		return;

	/* prefer band */
	if (band == B_CTX.prefer_band)
		b_prefer_band_expire(ssid);
	/* non prefer band */
	else if (band == B_CTX.non_prefer_band)
		b_non_prefer_expire(ssid);

	return;
}

void b_steering_roam_detect(struct b_steer_priv *priv)
{
	u8 band = priv->band;
	u8 ssid = priv->ssid;

	if (!priv->band_steering_enable)
		return;

	/* prefer band */
	if (band == B_CTX.prefer_band)
		b_prefer_band_roam_detect(priv);
	/* non prefer band */
	else if (band == B_CTX.non_prefer_band)
		b_non_prefer_roam_detect(priv);

	return;
}

void b_steering_roam_start_upcoming(struct b_steer_priv *priv)
{
	if (!priv->band_steering_enable)
		return;

	b_steering_roam_start(priv);

	return;
}

void b_steering_on_cmd(struct b_steer_priv *priv)
{
	u8 i;
	u8 band = priv->band;
	u8 ssid = priv->ssid;
	s8 sys_cmd[64] = {0};
	FILE *fp;

	if (band >= BAND_NUM || ssid >= SSID_NUM) {
		DBG_MSG_WARN(B_STEER_STR, "wrong argument.");
		return;
	}

	fp = fopen(B_STEER_OUTPUT_FILE, "w");
	if (fp == NULL) {
		DBG_MSG_WARN(B_STEER_STR, "can't open [%s].", B_STEER_OUTPUT_FILE);
		return;
	}

	fprintf(fp, "[BAND_STEERING] Common Parameters.\n");
	fprintf(fp, "prefer_band: %s\n", BAND_NAME(B_CTX.prefer_band));
	fprintf(fp, "non_prefer_band: %s\n", BAND_NAME(B_CTX.non_prefer_band));
	fprintf(fp, "bss_tm_req_th: %u\n", B_CTX.bss_tm_req_th);
	fprintf(fp, "bss_tm_req_disassoc_imminent: %u\n", B_CTX.bss_tm_req_disassoc_imminent);
	fprintf(fp, "bss_tm_req_disassoc_timer: %u\n", B_CTX.bss_tm_req_disassoc_timer);
	fprintf(fp, "roam_detect_th: %u\n", B_CTX.roam_detect_th);
	fprintf(fp, "roam_sta_tp_th(kbits): %u\n", B_CTX.roam_sta_tp_th);
	fprintf(fp, "roam_ch_clm_th(%%): %u\n", B_CTX.roam_ch_clm_th);
	fprintf(fp, "================================\n");

	/* show prefer band list */
	if (band == B_CTX.prefer_band) {
		struct b_steer_prefer_band_entry *ent;

		fprintf(fp, "[BAND_STEERING] prefer_band.\n");
		fprintf(fp, "band: %s\n", BAND_NAME(band));
		fprintf(fp, "ssid: %u\n", ssid);
		fprintf(fp, "grp_ssid: %u\n", B_CTX.prefer_band_grp_ssid[ssid]);
		fprintf(fp, "prefer_band_rssi_high: %u\n", B_CTX.prefer_band_rssi_high);
		fprintf(fp, "prefer_band_rssi_low: %u\n", B_CTX.prefer_band_rssi_low);
		fprintf(fp, "================================\n");
		for (i = 0; i < B_STEER_MAX_NUM; i++) {
			ent = &(B_CTX.p_band_list[ssid][i]);
			if (ent->used) {
				fprintf(fp, "mac: "MAC_FMT"\n", MAC_ARG(ent->mac));
				fprintf(fp, "rssi: %u\n", ent->rssi);
				fprintf(fp, "aging: %u\n", ent->aging);
				fprintf(fp, "------------------------------\n");
			}
		}
	}
	/* show non prefer band list */
	else if (band == B_CTX.non_prefer_band) {
		struct b_steer_non_prefer_band_entry *ent;

		fprintf(fp, "[BAND_STEERING] non_prefer_band.\n");
		fprintf(fp, "band: %s\n", BAND_NAME(band));
		fprintf(fp, "ssid: %u\n", ssid);
		fprintf(fp, "grp_ssid: %u\n", B_CTX.non_prefer_band_grp_ssid[ssid]);
		fprintf(fp, "entry_exp_phase1: %u\n", B_CTX.entry_exp_phase1);
		fprintf(fp, "assoc_rty_lmt: %u\n", B_CTX.assoc_rty_lmt);
		fprintf(fp, "entry_exp_phase2: %u\n", B_CTX.entry_exp_phase2);
		fprintf(fp, "================================\n");
		for (i = 0; i < B_STEER_MAX_NUM; i++) {
			ent = &(B_CTX.np_band_list[ssid][i]);
			if (ent->used) {
				fprintf(fp, "mac: "MAC_FMT"\n", MAC_ARG(ent->mac));
				fprintf(fp, "entry_exp_phase1: %u\n", ent->entry_exp_phase1);
				fprintf(fp, "assoc_rty_lmt: %u\n", ent->assoc_rty_lmt);
				fprintf(fp, "entry_exp_phase2: %u\n", ent->entry_exp_phase2);
				fprintf(fp, "------------------------------\n");
			}
		}
	}
	else {
		DBG_MSG_WARN(B_STEER_STR, "wrong argument.");
	}

	fclose(fp);

	sprintf(sys_cmd, "cat %s", B_STEER_OUTPUT_FILE);
	system(sys_cmd);

	return;
}

void b_steering_on_config_update(s8 *config)
{
	b_steering_config_read(config);

	return;
}

void b_steering_init(struct b_steer_device *device)
{
	u8 i;
	u8 grp_ssid;
	struct b_steer_priv *priv;
	struct b_steer_priv *grp_priv;
	DBG_MSG_PRINT(B_STEER_STR, "Band Steering Init Enter");

	/* common parameters */
	B_CTX.prefer_band = BAND_ON_5G;
	B_CTX.non_prefer_band = BAND_ON_24G;
	/* bss_tm_req parameters */
	B_CTX.bss_tm_req_th = 2;
	B_CTX.bss_tm_req_disassoc_imminent = 1;
	B_CTX.bss_tm_req_disassoc_timer = 100;

	/* band roaming parameters */
	B_CTX.roam_detect_th = 10;
	B_CTX.roam_sta_tp_th = 0;
	/* CLM: non prefer band --> prefer band */
	B_CTX.roam_ch_clm_th = 0;

	/* rssi parameters */
	B_CTX.prefer_band_rssi_high = PREFER_BAND_RSSI_H;
	B_CTX.prefer_band_rssi_low = PREFER_BAND_RSSI_L;

	/* non prefer band parameter */
	B_CTX.entry_exp_phase1 = 60;
	B_CTX.assoc_rty_lmt = 5;
	B_CTX.entry_exp_phase2 = 5;

	/* data structure */
	os_memset(B_CTX.p_band_list, 0,	sizeof(B_CTX.p_band_list));
	os_memset(B_CTX.np_band_list, 0, sizeof(B_CTX.np_band_list));
	os_memset(B_CTX.np_band_ignore_list, 0, sizeof(B_CTX.np_band_ignore_list));

	/* group */
	for (i = 0; i < SSID_NUM; i++) {
		B_CTX.prefer_band_grp_ssid[i] = 0xff;
		B_CTX.non_prefer_band_grp_ssid[i] = 0xff;
	}
	B_CTX.prefer_band_grp_ssid[0] = 0;
	B_CTX.non_prefer_band_grp_ssid[0] = 0;

	b_steering_config_read(device->config_fname);

	/* prefer band group non prfer band */
	for (i = 0; i < SSID_NUM; i++) {
		grp_ssid = B_CTX.prefer_band_grp_ssid[i];
		if (grp_ssid >= SSID_NUM)
			continue;

		/* point each others */
		priv = &(device->priv[B_CTX.prefer_band][i]);
		grp_priv = &(device->priv[B_CTX.non_prefer_band][grp_ssid]);
		priv->grp_priv = grp_priv;
		grp_priv->grp_priv = priv;
		DBG_MSG_INFO(B_STEER_STR, "Linking prefer band ssid %d with non-prefer band ssid %d\n", i, grp_ssid);
	}

	/* non prefer band group check */
	for (i = 0; i < SSID_NUM; i++) {
		grp_ssid = B_CTX.non_prefer_band_grp_ssid[i];
		if (grp_ssid >= SSID_NUM)
			continue;

		priv = &(device->priv[B_CTX.non_prefer_band][i]);
		grp_priv = priv->grp_priv;
		if (grp_priv && grp_priv->ssid == grp_ssid) {
			DBG_MSG_PRINT(B_STEER_STR, "Band Steering Enable");
			DBG_MSG_INFO(B_STEER_STR, "non_prefer_band_grp_ssid: %d\n", grp_ssid);
			priv->band_steering_enable = 1;
			grp_priv->band_steering_enable = 1;
		}
	}

	return;
}

void b_steering_deinit(void)
{
	return;
}

#endif

