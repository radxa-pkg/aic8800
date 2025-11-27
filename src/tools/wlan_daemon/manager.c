#include "manager.h"
#ifdef CONFIG_WIFI_BAND_STEERING
#include "steering.h"
#endif
#include <poll.h>
#include <errno.h>

sigset_t sig;
static s32 sock_fd;
static struct nlmsghdr *nlh = NULL;
static struct iovec iov;
static struct msghdr msgh;
static struct sockaddr_nl s_addr, d_addr;
static struct b_steer_device global_device = {0};

#ifdef CONFIG_SEC_NLCMD
static s32 sock_fd_sec;
static struct nlmsghdr *nlh_sec = NULL;
static struct iovec iov_sec;
static struct msghdr msgh_sec;
static struct sockaddr_nl s_sec_addr, d_sec_addr;
#endif

#if defined(CONFIG_WIFI_BAND_STEERING) && defined(ACTIVE_DUAL_BAND_DETECT)
unsigned char null_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif


#define DYNAMIC_DEBUG_LEVEL    "/tmp/manager_debug"
__inline__ static void dynamic_set_debug_level(void)
{
	int debug_level = 0;

	FILE *fp = fopen(DYNAMIC_DEBUG_LEVEL, "re");
	if (fp != NULL) {
		(void) fscanf(fp, "%d", &debug_level);
		fclose(fp);
		(void) remove(DYNAMIC_DEBUG_LEVEL);

		if (debug_level >= 0 && debug_level != global_dbg_level) {
			printf("[%s,%d] Debug level setting to %d.\n", __FUNCTION__, __LINE__, debug_level);
			global_dbg_level = debug_level;
		}
	}
}


void manager_hostapd_cli_bss_tm_req(
	s8 *intf_name, u8 *sta_mac, u8 *bss_mac, u16 bss_ch,
	u16 bss_info, u8 bss_reg_class, u8 bss_phy_type,
	u8 disassoc_imminent, u32 disassoc_timer)
{
	s8 sys_cmd[256] = {0};
	/* hostapd_cli bss_tm_req parameters */
	u8 valid_int = 100;
	u8 pref = 1;
	u8 preference = 255;
	u8 abridged = 1;

	os_memset(sys_cmd, 0, sizeof(sys_cmd));

	/* send bss tm req */
#if defined(CONFIG_WIFI_BAND_STEERING) && defined(ACTIVE_DUAL_BAND_DETECT)
	if (!os_memcmp(bss_mac, null_mac, 6))
		sprintf(sys_cmd, HOSTAPD_PATH" -p %s -i %s bss_tm_req "MAC_FMT"",
			CTRL_PATH, intf_name, MAC_ARG(sta_mac));
	else
#endif
	sprintf(sys_cmd,
		HOSTAPD_PATH" -p %s -i %s bss_tm_req "MAC_FMT" "BSS_TM_REQ_PARAMS_FMT"",
		CTRL_PATH, intf_name, MAC_ARG(sta_mac), valid_int, pref, disassoc_imminent,
		disassoc_timer, abridged,
		MAC_ARG(bss_mac), bss_info, bss_reg_class, bss_ch, bss_phy_type);

	DBG_MSG_PRINT(MANAGER_STR, "%s.", sys_cmd);
	system(sys_cmd);
	
	return;
}

void manager_hostapd_cli_deauth(s8 *intf_name, u8 *sta_mac)
{
	s8 sys_cmd[128] = {0};

	/* send deauth */
	sprintf(sys_cmd,
		HOSTAPD_PATH" -p %s -i %s deauthenticate "MAC_FMT"",
		CTRL_PATH, intf_name, MAC_ARG(sta_mac));

	DBG_MSG_PRINT(MANAGER_STR, "%s.", sys_cmd);
	system(sys_cmd);

	return;
}

void manager_set_msg(
	struct b_nl_message *msg, u32 *msg_len, void *elm, u32 elm_len)
{
	os_memcpy(msg->content + (*msg_len), elm, elm_len);
	(*msg_len) += elm_len;
}

void manager_send_drv(void *msg, u32 msg_len)
{
	s32 status;

	os_memset(nlh, 0, NLMSG_SPACE(NL_MAX_MSG_SIZE));
	nlh->nlmsg_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);
	nlh->nlmsg_pid = NL_AIC_MANAGER_PID;
	nlh->nlmsg_flags = 0;
	os_memcpy(NLMSG_DATA(nlh), msg, msg_len);

	/* send message */
	status = sendmsg(sock_fd, &msgh, 0);
	if (status < 0)
		DBG_MSG_ERR(MANAGER_STR, "%s send kernel error %u %s.",
			__FUNCTION__, errno, strerror(errno));

	return;
}

#ifdef CONFIG_SEC_NLCMD
void manager_send_sec_drv(void *msg, u32 msg_len)
{
	s32 status;

	os_memset(nlh_sec, 0, NLMSG_SPACE(NL_MAX_MSG_SIZE));
	nlh_sec->nlmsg_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);
	nlh_sec->nlmsg_pid = NL_AIC_MANAGER_PID;
	nlh_sec->nlmsg_flags = 0;
	os_memcpy(NLMSG_DATA(nlh_sec), msg, msg_len);

	/* send message */
	status = sendmsg(sock_fd_sec, &msgh_sec, 0);
	if (status < 0)
		DBG_MSG_ERR(MANAGER_STR, "%s send kernel error %u %s.",
			__FUNCTION__, errno, strerror(errno));

	return;
}
#endif

void manager_send_daemon(void *msg, u32 msg_len, pid_t pid)
{
	s32 status;

	/* create netlink */
	sock_fd = socket(AF_NETLINK, SOCK_RAW, NL_AIC_PROTOCOL);
	if (sock_fd == -1) {
		DBG_MSG_ERR(MANAGER_STR, "create socket error.");
		return;
	}

	/* source address */
	os_memset(&s_addr, 0, sizeof(s_addr));
	s_addr.nl_family = AF_NETLINK;
	s_addr.nl_pid = getpid();
	s_addr.nl_groups = 0;

	/* destination address */
	os_memset(&d_addr, 0, sizeof(d_addr));
	d_addr.nl_family = AF_NETLINK;
	d_addr.nl_pid = pid;
	d_addr.nl_groups = 0;

	/* bind socket */
	if (bind(sock_fd, (struct sockaddr *)&s_addr, sizeof(s_addr)) != 0) {
		DBG_MSG_ERR(MANAGER_STR, "bind socket error.");
		close(sock_fd);
		return;
	}

	/* allocate netlink header */
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(NL_MAX_MSG_SIZE));
	if (!nlh) {
		DBG_MSG_ERR(MANAGER_STR, "malloc nlmsghdr error.");
		close(sock_fd);
		return;
	}
	os_memset(nlh, 0, NLMSG_SPACE(NL_MAX_MSG_SIZE));
	nlh->nlmsg_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);
	nlh->nlmsg_pid = NL_AIC_MANAGER_PID;
	nlh->nlmsg_flags = 0;
	os_memcpy(NLMSG_DATA(nlh), msg, msg_len);

	/* iov structure */
	iov.iov_base = (void *)nlh;
	iov.iov_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);

	/* msghdr */
	os_memset(&msgh, 0, sizeof(msgh));
	msgh.msg_name = (void *)&d_addr;
	msgh.msg_namelen = sizeof(d_addr);
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;

	/* send message */
	status = sendmsg(sock_fd, &msgh, 0);
	if (status < 0)
		DBG_MSG_ERR(MANAGER_STR, "send error.");

	close(sock_fd);

	return;
}


static void send_daemon_on_msg_manager(void)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};

	/* finish message */
	msg.type = AIC_NL_DAEMON_ON_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len = msg.len + 8;

	DBG_MSG_PRINT(MANAGER_STR, "%s, type: %d", __func__, msg.type);
	manager_send_drv((void *)&msg, msg_len);
#ifdef CONFIG_SEC_NLCMD
	manager_send_sec_drv((void *)&msg, msg_len);
#endif
	return;
}

static void send_daemon_off_msg_manager(void)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};

	/* finish message */
	msg.type = AIC_NL_DAEMON_OFF_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len = msg.len + 8;

	DBG_MSG_PRINT(MANAGER_STR, "%s, type: %d", __func__, msg.type);
	manager_send_drv((void *)&msg, msg_len);
#ifdef CONFIG_SEC_NLCMD
	manager_send_sec_drv((void *)&msg, msg_len);
#endif

	exit(0);
}


static void send_del_sta_msg_manager(struct b_steer_device *device, u8 band, u8 *mac)
{
	u8 i;
	struct b_steer_priv *priv = NULL;

	DBG_MSG_INFO(MANAGER_STR, "%s band:%d "MAC_FMT, __func__, band, MAC_ARG(mac));
	for (i = 0; i < SSID_NUM; i++) {
		priv = &(device->priv[band][i]);

		if (priv->active)
			manager_hostapd_cli_deauth(priv->name, mac);
	}

	return;
}

static void send_priv_info_cmd_msg_manager(u8 band, u8 ssid)
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
	msg.type = AIC_NL_PRIV_INFO_CMD_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len = msg.len + 8;
	manager_send_daemon((void *)&msg, msg_len, NL_AIC_MANAGER_PID);

	return;
}

static void send_config_update_cmd_msg_manager(u8 *config)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_buffer buffer = {0};

	/* element header */
	hdr.id = AIC_ELM_BUFFER_ID;
	hdr.len = ELM_BUFFER_LEN;
	manager_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_BUFFER_ID */
	os_memcpy(buffer.buf, config, 255);
	manager_set_msg(&msg, &msg_len, (void *)&buffer, ELM_BUFFER_LEN);

	/* finish message */
	msg.type = AIC_NL_CONFIG_UPDATE_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len = msg.len + 8;
	manager_send_daemon((void *)&msg, msg_len, NL_AIC_MANAGER_PID);

	return;
}

static s8 config_fname_parse_arg_manager(
	u8 *argn, s32 argc, char *argv[], struct b_steer_device *device)
{
	if (*argn + 1 >= argc) {
		DBG_MSG_WARN(MANAGER_STR, "wrong argument.");
		return -1;
	}

	os_strcpy(device->config_fname, argv[*argn + 1]);

	DBG_MSG_PRINT(MANAGER_STR, "config: %s.", device->config_fname);

	*argn = *argn + 1;

	return 0;
}

static void priv_info_parse_arg_manager(u8 *argn, s32 argc, char *argv[])
{
	u8 band = 255;
	u8 ssid = 255;

	if (*argn + 2 >= argc) {
		DBG_MSG_WARN(MANAGER_STR, "wrong argument.");
		return;
	}

	if (!os_strcmp(argv[*argn + 1], "2g"))
		band = BAND_ON_24G;
	else if (!os_strcmp(argv[*argn + 1], "5g"))
		band = BAND_ON_5G;

	ssid = argv[*argn + 2][0] - '0';

	send_priv_info_cmd_msg_manager(band, ssid);

	*argn = *argn + 2;

	return;
}

static void config_update_parse_arg_manager(u8 *argn, s32 argc, char *argv[])
{
	u8 config[255] = {0};

	if (*argn + 1 >= argc) {
		DBG_MSG_WARN(MANAGER_STR, "wrong argument.");
		return;
	}

	if (strlen(argv[*argn + 1]) < sizeof(config))
		strncpy(config, argv[*argn + 1], strlen(argv[*argn + 1]));
	else
		strncpy(config, argv[*argn + 1], sizeof(config));

	send_config_update_cmd_msg_manager(config);

	*argn = *argn + 1;

	return;
}

static s32 parse_argument_manager(
	int argc, char *argv[], struct b_steer_device *device)
{
	u8 argn = 1;

	while (argn < argc) {
		if (!os_strcmp(argv[argn], "-config_fname")) {
			if (config_fname_parse_arg_manager(&argn, argc, argv, device) < 0)
				return -1;
		}
		else if (!os_strcmp(argv[argn], "-priv_info")) {
			priv_info_parse_arg_manager(&argn, argc, argv);
			return -1;
		}
		else if (!os_strcmp(argv[argn], "-config_update")) {
			config_update_parse_arg_manager(&argn, argc, argv);
			return -1;
		}
#ifdef CONFIG_WIFI_BAND_STEERING
		else if (!os_strcmp(argv[argn], "-band_steering")) {
			b_steering_parse_arg(&argn, argc, argv);
			return -1;
		}
#endif
		else {
			DBG_MSG_WARN(MANAGER_STR, "invalid argument (%s).",
				argv[argn]);
			return -1;
		}
		argn++;
	}

	return 0;
}

static struct b_steer_sta *mng_sta_info_lookup(
	struct b_steer_priv *priv, u8 *mac)
{
	u64 offset;
	u32 hash_idx;
	struct b_steer_wifi_list *list;
	struct b_steer_sta *sta = NULL;

	offset = (u64)(&((struct b_steer_sta *)0)->list);
	hash_idx = b_wifi_mac_hash(mac);
	list = priv->sta_hash_tbl[hash_idx];

	while (list) {
		sta = (struct b_steer_sta *)((u64)list - offset);
		if (!os_memcmp(sta->mac, mac, 6))
			return sta;

		list = list->next;
	}

	return NULL;
}

static struct b_steer_frame *mng_frame_info_lookup(
	struct b_steer_priv *priv, u8 *mac)
{
	u64 offset;
	u32 hash_idx;
	struct b_steer_wifi_list *list;
	struct b_steer_frame *frame = NULL;

	offset = (u64)(&((struct b_steer_frame *)0)->list);
	hash_idx = b_wifi_mac_hash(mac);
	list = priv->frame_hash_tbl[hash_idx];

	while (list) {
		frame = (struct b_steer_frame *)((u64)list - offset);
		if (!os_memcmp(frame->sa, mac, 6))
			return frame;

		list = list->next;
	}

	return NULL;
}

static void mng_on_del_sta(
	struct b_steer_priv *priv,
	struct b_elm_sta_info *sta_info)
{
	u64 offset;
	u32 hash_idx;
	u8 *mac = sta_info->mac;
	struct b_steer_wifi_list *list;
	struct b_steer_sta *sta = NULL;

	DBG_MSG_INFO(MANAGER_STR, "%s", __func__);

	sta = mng_sta_info_lookup(priv, mac);

	if (sta == NULL) {
		DBG_MSG_INFO(MANAGER_STR, "sta is null");
		return;
	}

	offset = (u64)(&((struct b_steer_sta *)0)->list);
	hash_idx = b_wifi_mac_hash(mac);
	list = priv->sta_hash_tbl[hash_idx];

	while (list) {
		sta = (struct b_steer_sta *)((u64)list - offset);
		if (!os_memcmp(sta->mac, mac, 6)) {
			DBG_MSG_INFO(MANAGER_STR, "hash_idx:%d, "MAC_FMT,
					hash_idx, MAC_ARG(mac));
			sta->used = 0;
			priv->sta_num--;
			b_wifi_list_unlink(list);
		}

		list = list->next;
	}

	return;
}

static void mng_on_new_sta(
	struct b_steer_device *device,
	struct b_steer_priv *priv,
	struct b_elm_sta_info *sta_info)
{
	u8  i;
	u32 hash_idx;
	u8  *mac = sta_info->mac;
	u8  del_band = (priv->band == BAND_ON_24G) ? BAND_ON_5G : BAND_ON_24G;
	struct b_steer_sta *sta = NULL;

	DBG_MSG_INFO(MANAGER_STR, "new station "MAC_FMT" on %s.",
		MAC_ARG(mac), priv->name);

	/* delete first */
	mng_on_del_sta(priv, sta_info);
#ifdef CONFIG_WIFI_BAND_STEERING
	if (priv->grp_priv && priv->grp_priv->band_steering_enable)
		mng_on_del_sta(priv->grp_priv, sta_info);
#endif

	/* find an empty entry */
	for (i = 0; i < MAX_STA_NUM; i++) {
		if (!priv->sta_list[i].used) {
			sta = &(priv->sta_list[i]);
			break;
		}
	}

	/* add the entry */
	if (sta) {
		DBG_MSG_INFO(MANAGER_STR, "sta add the entry");
		os_memset(sta, 0, sizeof(*sta));
		sta->used = 1;
		os_memcpy(sta->mac, mac, 6);
		sta->rssi = sta_info->rssi;
		sta->link_time = 0;
		//unuse tp
		sta->tx_tp = 0;
		sta->rx_tp = 0;
		sta->is_dual_band = 0;
#ifdef CONFIG_WIFI_BAND_STEERING
		sta->b_steer_roam_cnt = 0;
		sta->b_steer_roam_detect = 0;
#endif
		priv->sta_num++;
		/* hash update */
		hash_idx = b_wifi_mac_hash(mac);
		b_wifi_list_link(&(sta->list), &(priv->sta_hash_tbl[hash_idx]));
	}

	/* deletes the station on another band */
	send_del_sta_msg_manager(device, del_band, mac);	

	return;
}

static void mng_intf_update(
	struct b_steer_priv *priv,
	struct b_elm_intf *intf)
{
	priv->active = 1;
	os_memcpy(priv->mac, intf->mac, 6);
	os_memcpy(priv->name, intf->name, 16);

	return;
}

static void mng_intf_info_update(
	struct b_steer_priv *priv,
	struct b_elm_intf_info *intf_info)
{
	priv->ch = intf_info->ch;
	priv->ch_clm = intf_info->ch_clm;

	priv->self_nb_rpt.bss_info = intf_info->bss_info;
	priv->self_nb_rpt.reg_class = intf_info->reg_class;
	priv->self_nb_rpt.phy_type = intf_info->phy_type;

	return;
}

static void mng_on_intf_rpt(
	struct b_steer_priv *priv,
	struct b_elm_intf *intf,
	struct b_elm_intf_info *intf_info)
{
	mng_intf_update(priv, intf);
	mng_intf_info_update(priv, intf_info);

	return;
}

static void mng_on_sta_rpt(
	struct b_steer_device *device,
	struct b_steer_priv *priv,
	struct b_elm_sta_info *sta_info)
{
	u8 i;
	u8 *mac = sta_info->mac;
	u8 dual_band = (priv->band == BAND_ON_24G) ? BAND_ON_5G : BAND_ON_24G;
	struct b_steer_sta *sta = NULL;
	struct b_steer_frame *frame = NULL;
	struct b_steer_priv *dual_priv = NULL;

	sta = mng_sta_info_lookup(priv, mac);

	if (sta == NULL) /* maybe the sta_list is already full */
		return;

	//printf("%s, %d,%d,%d,%d\n", __func__, sta_info->rssi, sta_info->link_time, sta_info->tx_tp, sta_info->rx_tp);

	sta->rssi = (sta_info->rssi) ? sta_info->rssi : sta->rssi;
	sta->link_time = sta_info->link_time;
	//unuse tp
	sta->tx_tp = sta_info->tx_tp;
	sta->rx_tp = sta_info->rx_tp;

	/* check if the station is dual band */
	if (sta->is_dual_band != 1) {
		for (i = 0; i < SSID_NUM; i++) {
			dual_priv = &(device->priv[dual_band][i]);
			frame = mng_frame_info_lookup(dual_priv, mac);
			if (frame) {
				sta->is_dual_band = 1;
				break;
			}
		}
	}

#if defined(CONFIG_WIFI_BAND_STEERING) && defined(ACTIVE_DUAL_BAND_DETECT)
	/* if sta mac not found in cached frames, trigger sta to send probe request*/
	if ((sta->is_dual_band != 1)
	&& (sta->b_steer_bss_tm_req_cnt < BSS_TM_REQ_RETRY_LIMIT)
	&& ((sta->link_time % BSS_TM_REQ_INTERVAL) <= 1)
	&& (sta_info->tx_tp < BSS_TM_REQ_TP_LIMIT)
	&& (sta_info->rx_tp < BSS_TM_REQ_TP_LIMIT))
	{
		struct b_steer_priv *grp_priv = priv->grp_priv;
		DBG_MSG_PRINT(MANAGER_STR, "trigger probe_req "MAC_FMT" on %s", MAC_ARG(sta->mac), priv->name);
		manager_hostapd_cli_bss_tm_req(
			priv->name, sta->mac, null_mac,
			0, 0, 0, 0, 0, 0);
		sta->b_steer_bss_tm_req_cnt++;
	}
#endif

	return;
}

static void mng_on_frame_rpt(
	struct b_steer_priv *priv,
	struct b_elm_frame_info *frame_info)
{
	u8  i;
	u32 tmp_i = 0;
	u32 tmp_aging = 0;
	u32 hash_idx;
	u8  *mac = frame_info->sa;
	struct b_steer_frame *frame = NULL;

	frame = mng_frame_info_lookup(priv, mac);

	/* already exist */
	if (frame) {
		frame->aging = 0;
		return;
	}

	/* find an empty entry */
	for (i = 0; i < MAX_STA_NUM; i++) {
		if (!priv->frame_db[i].used) {
			frame = &(priv->frame_db[i]);
			break;
		}
	}

	/* no empty, find the oldest one */
	if (frame == NULL) {
		for (i = 0; i < MAX_STA_NUM; i++) {
			if (tmp_aging < priv->frame_db[i].aging) {
				tmp_aging = priv->frame_db[i].aging;
				tmp_i = i;
			}
		}
		frame = &(priv->frame_db[tmp_i]);
		b_wifi_list_unlink(&(frame->list));
	}

	/* add the entry */
	frame->used = 1;
	frame->frame_type = frame_info->frame_type;
	os_memcpy(frame->sa, mac, 6);
	frame->aging = 0;
	/* hash update */
	hash_idx = b_wifi_mac_hash(mac);
	b_wifi_list_link(&(frame->list), &(priv->frame_hash_tbl[hash_idx]));

	return;
}

static void mng_on_sta_ext_rpt(
	struct b_steer_device *device,
	struct b_steer_priv *priv,
	struct b_elm_sta_info_ext *sta_info_ext)
{
	u8 i;
	u8 *mac = sta_info_ext->mac;
	u8 dual_band = (priv->band == BAND_ON_24G) ? BAND_ON_5G : BAND_ON_24G;
	struct b_steer_sta *sta = NULL;
	struct b_steer_frame *frame = NULL;
	struct b_steer_priv *dual_priv = NULL;

	sta = mng_sta_info_lookup(priv, mac);

	if (sta == NULL)
		return;

	/* check if the station is dual band */
	if (sta_info_ext->supported_band & BAND_CAP_2G &&
			sta_info_ext->supported_band & BAND_CAP_5G) {
		DBG_MSG_INFO(MANAGER_STR, "is_dual_band: "MAC_FMT, MAC_ARG(mac));
		sta->is_dual_band |= 1;
	}

	return;
}

static void mng_on_priv_info_cmd(struct b_steer_priv *priv)
{
	u8 i;
	u8 band = priv->band;
	u8 ssid = priv->ssid;
	s8 sys_cmd[64] = {0};
	FILE *fp;
	struct b_steer_sta *sta;

	if (band >= BAND_NUM || ssid >= SSID_NUM) {
		DBG_MSG_WARN(MANAGER_STR, "wrong argument.");
		return;
	}

	fp = fopen(B_PRIV_INFO_OUTPUT, "w");
	if (fp == NULL) {
		DBG_MSG_WARN(MANAGER_STR, "can't open [%s].", B_PRIV_INFO_OUTPUT);
		return;
	}

	fprintf(fp, "[MANAGER] priv_info.\n");
	fprintf(fp, "mac: "MAC_FMT"\n", MAC_ARG(priv->mac));
	fprintf(fp, "band: %s\n", BAND_NAME(priv->band));
	fprintf(fp, "ssid: %u\n", priv->ssid);
	fprintf(fp, "name: %s\n", priv->name);
	fprintf(fp, "ch: %u\n", priv->ch);
	fprintf(fp, "ch: %u%%\n", priv->ch_clm);
	fprintf(fp, "sta_num: %u\n", priv->sta_num);
	fprintf(fp, "======== STA LIST ========\n");
	for (i = 0; i < MAX_STA_NUM; i++) {
		sta = &(priv->sta_list[i]);
		if (!sta->used)
			continue;

		fprintf(fp, "mac: "MAC_FMT"\n", MAC_ARG(sta->mac));
		fprintf(fp, "rssi: %u\n", sta->rssi);
		fprintf(fp, "tx_tp: %u\n", sta->tx_tp);
		fprintf(fp, "rx_tp: %u\n", sta->rx_tp);
		fprintf(fp, "is_dual_band: %u\n", sta->is_dual_band);
#ifdef CONFIG_WIFI_BAND_STEERING
		fprintf(fp, "b_steer_roam_cnt: %u\n", sta->b_steer_roam_cnt);
		fprintf(fp, "b_steer_roam_detect: %u\n", sta->b_steer_roam_detect);
#ifdef ACTIVE_DUAL_BAND_DETECT
		fprintf(fp, "b_steer_bss_tm_req_cnt: %u\n", sta->b_steer_bss_tm_req_cnt);
#endif
#endif
		fprintf(fp, "--------------------------\n");
	}

	fclose(fp);

	sprintf(sys_cmd, "cat %s", B_PRIV_INFO_OUTPUT);
	system(sys_cmd);

	return;
}

static s32 mng_daemon_init(struct b_steer_device *device)
{
	u8 i, j;

	sigemptyset(&sig);
	signal(SIGTERM, (void *)&send_daemon_off_msg_manager);

	sock_fd = socket(AF_NETLINK, SOCK_RAW, NL_AIC_PROTOCOL);
	if (sock_fd == -1) {
		DBG_MSG_ERR(MANAGER_STR, "create socket_1 error.");
		return -1;
	}

	os_memset(&s_addr, 0, sizeof(s_addr));
	s_addr.nl_family = AF_NETLINK;
	s_addr.nl_pid = NL_AIC_MANAGER_PID;
	s_addr.nl_groups = 0;

	os_memset(&d_addr, 0, sizeof(d_addr));
	d_addr.nl_family = AF_NETLINK;
	d_addr.nl_pid = 0;
	d_addr.nl_groups = 0;

	if (bind(sock_fd, (struct sockaddr *)&s_addr, sizeof(s_addr)) != 0) {
		DBG_MSG_ERR(MANAGER_STR, "bind socket error.");
		close(sock_fd);
		return -1;
	}

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(NL_MAX_MSG_SIZE));
	if (!nlh) {
		DBG_MSG_ERR(MANAGER_STR, "malloc nlmsghdr error.");
		close(sock_fd);
		return -1;
	}
	os_memset(nlh, 0, NLMSG_SPACE(NL_MAX_MSG_SIZE));
	nlh->nlmsg_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);
	nlh->nlmsg_pid = NL_AIC_MANAGER_PID;
	nlh->nlmsg_flags = 0;

	iov.iov_base = (void *)nlh;
	iov.iov_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);

	os_memset(&msgh, 0, sizeof(msgh));
	msgh.msg_name = (void *)&d_addr;
	msgh.msg_namelen = sizeof(d_addr);
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;

#ifdef CONFIG_SEC_NLCMD
	sock_fd_sec = socket(AF_NETLINK, SOCK_RAW, NL_AIC_PROTOCOL_SEC_DRV);
	if (sock_fd_sec == -1) {
		DBG_MSG_ERR(MANAGER_STR, "create socket_2 error.");
		return -1;
	}

	os_memset(&s_sec_addr, 0, sizeof(s_sec_addr));
	s_sec_addr.nl_family = AF_NETLINK;
	s_sec_addr.nl_pid = NL_AIC_MANAGER_PID;
	s_sec_addr.nl_groups = 0;

	os_memset(&d_sec_addr, 0, sizeof(d_sec_addr));
	d_sec_addr.nl_family = AF_NETLINK;
	d_sec_addr.nl_pid = 0;
	d_sec_addr.nl_groups = 0;

	if (bind(sock_fd_sec, (struct sockaddr *)&s_sec_addr, sizeof(s_sec_addr)) != 0) {
		DBG_MSG_ERR(MANAGER_STR, "bind socket error.");
		close(sock_fd_sec);
		return -1;
	}

	nlh_sec = (struct nlmsghdr *)malloc(NLMSG_SPACE(NL_MAX_MSG_SIZE));
	if (!nlh_sec) {
		DBG_MSG_ERR(MANAGER_STR, "malloc nlmsghdr error.");
		close(sock_fd_sec);
		return -1;
	}
	os_memset(nlh_sec, 0, NLMSG_SPACE(NL_MAX_MSG_SIZE));
	nlh_sec->nlmsg_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);
	nlh_sec->nlmsg_pid = NL_AIC_MANAGER_PID;
	nlh_sec->nlmsg_flags = 0;

	iov_sec.iov_base = (void *)nlh_sec;
	iov_sec.iov_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);

	os_memset(&msgh_sec, 0, sizeof(msgh_sec));
	msgh_sec.msg_name = (void *)&d_sec_addr;
	msgh_sec.msg_namelen = sizeof(d_sec_addr);
	msgh_sec.msg_iov = &iov_sec;
	msgh_sec.msg_iovlen = 1;
#endif

	/* data structure init */
	for (i = 0; i < BAND_NUM; i++) {
		for (j = 0; j < SSID_NUM; j++) {
			device->priv[i][j].band = i;
			device->priv[i][j].ssid = j;
		}
	}

#ifdef CONFIG_WIFI_BAND_STEERING
	b_steering_init(device);
#endif

	return 0;
}

static void mng_process_recv_message(struct b_nl_message *msg, struct b_steer_device *device)
{
	struct b_steer_priv *priv = NULL;
	struct b_elm_header *hdr = NULL;
	struct b_elm_frame_info *frame_info = NULL;
	struct b_elm_intf *intf = NULL;
	struct b_elm_intf_info *intf_info = NULL;
	struct b_elm_sta_info *sta_info = NULL;
	struct b_elm_buffer *buffer = NULL;
	struct b_elm_sta_info_ext *sta_info_ext = NULL;

	u32 offset = 0;

	if (msg->type != AIC_NL_CONFIG_UPDATE_TYPE) {
		/* check the 1st element: AIC_ELM_INTF_ID */
		if (msg->len < ELM_HEADER_LEN + ELM_INTF_LEN)
			return;

		hdr = (struct b_elm_header *)(msg->content + offset);
		offset += ELM_HEADER_LEN;

		if (hdr->id != AIC_ELM_INTF_ID)
			return;

		intf = (struct b_elm_intf *)(msg->content + offset);
		offset += hdr->len;
		//printf("recv msg, type: %d, intf: band %d, ssid: %d\n", msg->type, intf->band, intf->ssid);
		priv = &(device->priv[intf->band][intf->ssid]);
	}

	//printf("%s, type: %d\n", __func__, msg->type);

	/* message type */
	switch(msg->type) {
	case AIC_NL_DEL_STA_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			case AIC_ELM_STA_INFO_ID:
				sta_info =
					(struct b_elm_sta_info *)(msg->content + offset);
				break;
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		if (sta_info)
			mng_on_del_sta(priv, sta_info);

		break;
	case AIC_NL_NEW_STA_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			case AIC_ELM_STA_INFO_ID:
				sta_info =
					(struct b_elm_sta_info *)(msg->content + offset);
				break;
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		if (sta_info)
			mng_on_new_sta(device, priv, sta_info);

		break;
	case AIC_NL_INTF_RPT_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			case AIC_ELM_INTF_INFO_ID:
				intf_info =
					(struct b_elm_intf_info *)(msg->content + offset);
				break;
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		if (intf_info)
			mng_on_intf_rpt(priv, intf, intf_info);
		break;
	case AIC_NL_STA_RPT_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			case AIC_ELM_STA_INFO_ID:
				sta_info =
					(struct b_elm_sta_info *)(msg->content + offset);
				break;
			case AIC_ELM_STA_INFO_EXT_ID:
				sta_info_ext =
					(struct b_elm_sta_info_ext *)(msg->content + offset);
				break;
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		if (sta_info)
			mng_on_sta_rpt(device, priv, sta_info);
		if (sta_info_ext)
			mng_on_sta_ext_rpt(device, priv, sta_info_ext);
		break;
	case AIC_NL_FRAME_RPT_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			case AIC_ELM_FRAME_INFO_ID:
				frame_info =
					(struct b_elm_frame_info *)(msg->content + offset);
				break;
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		if (frame_info) {
			mng_on_frame_rpt(priv, frame_info);
#ifdef CONFIG_WIFI_BAND_STEERING
			b_steering_mng_on_frame_rpt(priv,
				frame_info->frame_type,
				frame_info->sa,
				frame_info->rssi);
#endif
		}
		break;
	case AIC_NL_TIME_TICK_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
#ifdef CONFIG_WIFI_BAND_STEERING
		if (intf) {
			b_steering_on_time_tick(priv);
			b_steering_roam_detect(priv);
			b_steering_roam_start_upcoming(priv);
		}
#endif
		break;
	case AIC_NL_PRIV_INFO_CMD_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		mng_on_priv_info_cmd(priv);
		break;
#ifdef CONFIG_WIFI_BAND_STEERING
	case AIC_NL_B_STEER_CMD_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		b_steering_on_cmd(priv);
		break;
#endif		
	case AIC_NL_CONFIG_UPDATE_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element id: should handle wrong length */
			switch(hdr->id) {
			case AIC_ELM_BUFFER_ID:
				buffer =
					(struct b_elm_buffer *)(msg->content + offset);
				break;
			default:
				DBG_MSG_WARN(MANAGER_STR, "message(%u): unknown element id.",
					msg->type);
				break;
			}
			offset += hdr->len;
		}
		if (buffer) {
			DBG_MSG_PRINT(MANAGER_STR, "CONFIG_UPDATE: source = %s.", buffer->buf);
#ifdef CONFIG_WIFI_BAND_STEERING
			b_steering_on_config_update(buffer->buf);
#endif
		}
		break;
	default:
		DBG_MSG_WARN(MANAGER_STR, "unknown message type.");
		break;
	}

}

int main(int argc, char *argv[])
{
	struct b_steer_device *device = &(global_device);
#ifdef CONFIG_SEC_NLCMD
	struct pollfd fdset[2];
#else
	struct pollfd fdset[1];
#endif

	if (parse_argument_manager(argc, argv, device) < 0)
		return 0;

	DBG_MSG_PRINT(MANAGER_STR, "%s.", MANAGER_VER);

	/* init */
	if(mng_daemon_init(device) < 0)
		return -1;

	send_daemon_on_msg_manager();

	sleep(1);

	/* recv message from kernel */
	while (1) {
		s32 status;
		s32 nfds = 0;
		struct b_nl_message *msg = NULL;

		memset((void *)fdset, 0, sizeof(fdset));
		fdset[0].fd     = sock_fd;
		fdset[0].events = POLLIN;

#ifdef CONFIG_SEC_NLCMD
		fdset[1].fd     = sock_fd_sec;
		fdset[1].events = POLLIN;

		nfds = 2;
#else
		nfds = 1;
#endif
		if (0 > poll(fdset, nfds, -1)) {
			DBG_MSG_ERR(MANAGER_STR, "poll fail with errno=%d (%s)", errno, strerror(errno));
			break;
		}

		dynamic_set_debug_level();

		if (fdset[0].revents & POLLIN) {
			os_memset(nlh, 0, NLMSG_SPACE(NL_MAX_MSG_SIZE));
			status = recvmsg(fdset[0].fd, &msgh, 0);
			if(-1 == status) {
				DBG_MSG_ERR(MANAGER_STR, "recv error from first drv");
				continue;
			}
			msg = (struct b_nl_message *)NLMSG_DATA(nlh);
			mng_process_recv_message(msg, device);
		}

#ifdef CONFIG_SEC_NLCMD
		if (fdset[1].revents & POLLIN) {
			os_memset(nlh_sec, 0, NLMSG_SPACE(NL_MAX_MSG_SIZE));
			status = recvmsg(fdset[1].fd, &msgh_sec, 0);
			if(-1 == status) {
				DBG_MSG_ERR(MANAGER_STR, "recv error from second drv");
				continue;
			}
			msg = (struct b_nl_message *)NLMSG_DATA(nlh_sec);
			mng_process_recv_message(msg, device);
		}
#endif
	}

	return 0;
}

