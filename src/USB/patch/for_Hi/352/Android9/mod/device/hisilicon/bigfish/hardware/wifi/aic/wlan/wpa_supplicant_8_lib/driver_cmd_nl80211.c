/*
 * Driver interaction with extended Linux CFG8021
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 */

#include "includes.h"
#include <sys/types.h>
#include <fcntl.h>
#include <net/if.h>

#include "common.h"
#include "linux_ioctl.h"
#include "driver_nl80211.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#ifdef ANDROID
#include "android_drv.h"
#endif

typedef struct android_wifi_priv_cmd {
	char *bufaddr;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;

static int drv_errors = 0;

static void wpa_driver_send_hang_msg(struct wpa_driver_nl80211_data *drv)
{
	drv_errors++;
	if (drv_errors > DRV_NUMBER_SEQUENTIAL_ERRORS) {
		drv_errors = 0;
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "HANGED");
	}
}

static void wpa_driver_notify_country_change(void *ctx, char *cmd)
{
	if ((os_strncasecmp(cmd, "COUNTRY", 7) == 0) ||
	    (os_strncasecmp(cmd, "SETBAND", 7) == 0)) {
		union wpa_event_data event;

		os_memset(&event, 0, sizeof(event));
		event.channel_list_changed.initiator = REGDOM_SET_BY_USER;
		if (os_strncasecmp(cmd, "COUNTRY", 7) == 0) {
			event.channel_list_changed.type = REGDOM_TYPE_COUNTRY;
			if (os_strlen(cmd) > 9) {
				event.channel_list_changed.alpha2[0] = cmd[8];
				event.channel_list_changed.alpha2[1] = cmd[9];
			}
		} else {
			event.channel_list_changed.type = REGDOM_TYPE_UNKNOWN;
		}
		wpa_supplicant_event(ctx, EVENT_CHANNEL_LIST_CHANGED, &event);
	}
}

int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf,
				  size_t buf_len )
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ifreq ifr;
	android_wifi_priv_cmd priv_cmd;
	int ret = 0;

	if (bss->ifindex <= 0 && bss->wdev_id > 0) {
		/* DRIVER CMD received on the DEDICATED P2P Interface which doesn't
		 * have an NETDEVICE associated with it. So we have to re-route the
		 * command to the parent NETDEVICE
		 */
		struct wpa_supplicant *wpa_s = (struct wpa_supplicant *)(drv->ctx);

		wpa_printf(MSG_DEBUG, "Re-routing DRIVER cmd to parent iface");
		if (wpa_s && wpa_s->parent && wpa_s->parent->drv_priv) {
			/* Update the nl80211 pointers corresponding to parent iface */
			bss = wpa_s->parent->drv_priv;
			drv = bss->drv;
			wpa_printf(MSG_DEBUG, "Re-routing command to iface: %s"
					      " cmd (%s)", bss->ifname, cmd);
		}
	}

	if (os_strcasecmp(cmd, "STOP") == 0) {
		linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 0);
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STOPPED");
	} else if (os_strcasecmp(cmd, "START") == 0) {
		linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1);
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "STARTED");
	} else if (os_strcasecmp(cmd, "MACADDR") == 0) {
		u8 macaddr[ETH_ALEN] = {};

		ret = linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname, macaddr);
		if (!ret)
			ret = os_snprintf(buf, buf_len,
					  "Macaddr = " MACSTR "\n", MAC2STR(macaddr));
	} else { /* Use private command */
		os_memcpy(buf, cmd, strlen(cmd) + 1);
		memset(&ifr, 0, sizeof(ifr));
		memset(&priv_cmd, 0, sizeof(priv_cmd));
		os_strlcpy(ifr.ifr_name, bss->ifname, IFNAMSIZ);
		priv_cmd.bufaddr = buf;
		priv_cmd.used_len = buf_len;
		priv_cmd.total_len = buf_len;
		ifr.ifr_data = &priv_cmd;

		if ((ret = ioctl(drv->global->ioctl_sock, SIOCDEVPRIVATE + 1, &ifr)) < 0) {
			wpa_printf(MSG_ERROR, "%s: failed to issue private command: %s", __func__, cmd);
			wpa_driver_send_hang_msg(drv);
		} else {
			drv_errors = 0;
			ret = 0;
			if ((os_strcasecmp(cmd, "LINKSPEED") == 0) ||
			    (os_strcasecmp(cmd, "RSSI") == 0) ||
			    (os_strcasecmp(cmd, "GETBAND") == 0) ||
			    (os_strncasecmp(cmd, "WLS_BATCHING", 12) == 0))
				ret = strlen(buf);
			wpa_driver_notify_country_change(drv->ctx, cmd);
			wpa_printf(MSG_DEBUG, "%s %s len = %d, %zu", __func__, buf, ret, strlen(buf));
		}
	}
	return ret;
}

int wpa_driver_set_p2p_noa(void *priv, u8 count, int start, int duration)
{
	char buf[MAX_DRV_CMD_SIZE];

	memset(buf, 0, sizeof(buf));
	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	snprintf(buf, sizeof(buf), "P2P_SET_NOA %d %d %d", count, start, duration);
	return wpa_driver_nl80211_driver_cmd(priv, buf, buf, strlen(buf)+1);
}

int wpa_driver_get_p2p_noa(void *priv __unused, u8 *buf __unused, size_t len __unused)
{
	/* Return 0 till we handle p2p_presence request completely in the driver */
	return 0;
}

int wpa_driver_set_p2p_ps(void *priv, int legacy_ps, int opp_ps, int ctwindow)
{
	char buf[MAX_DRV_CMD_SIZE];

	memset(buf, 0, sizeof(buf));
	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	snprintf(buf, sizeof(buf), "P2P_SET_PS %d %d %d", legacy_ps, opp_ps, ctwindow);
	return wpa_driver_nl80211_driver_cmd(priv, buf, buf, strlen(buf) + 1);
}

int wpa_driver_set_ap_wps_p2p_ie(void *priv, const struct wpabuf *beacon,
				 const struct wpabuf *proberesp,
				 const struct wpabuf *assocresp)
{
	char *buf;
	const struct wpabuf *ap_wps_p2p_ie = NULL;

	char *_cmd = "SET_AP_WPS_P2P_IE";
	char *pbuf;
	int ret = 0;
	int i, buf_len;
	struct cmd_desc {
		int cmd;
		const struct wpabuf *src;
	} cmd_arr[] = {
		{0x1, beacon},
		{0x2, proberesp},
		{0x4, assocresp},
		{-1, NULL}
	};

	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	for (i = 0; cmd_arr[i].cmd != -1; i++) {
		ap_wps_p2p_ie = cmd_arr[i].src;
		if (ap_wps_p2p_ie) {
			buf_len = strlen(_cmd) + 3 + wpabuf_len(ap_wps_p2p_ie);
			buf = os_zalloc(buf_len);
			if (NULL == buf) {
				wpa_printf(MSG_ERROR, "%s: Out of memory",
					   __func__);
				ret = -1;
				break;
			}
		} else {
			continue;
		}
		pbuf = buf;
		pbuf += snprintf(pbuf, buf_len - wpabuf_len(ap_wps_p2p_ie),
				 "%s %d",_cmd, cmd_arr[i].cmd);
		*pbuf++ = '\0';
		os_memcpy(pbuf, wpabuf_head(ap_wps_p2p_ie), wpabuf_len(ap_wps_p2p_ie));
		ret = wpa_driver_nl80211_driver_cmd(priv, buf, buf, buf_len);
		os_free(buf);
		if (ret < 0)
			break;
	}

	return ret;
}
