#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "type.h"

#include "libhwinfo.h"

extern driver_cmd_nl80211_cb bcm_nl80211_cb;
extern driver_cmd_nl80211_cb rtl_nl80211_cb;
extern driver_cmd_nl80211_cb  xr_nl80211_cb;
extern driver_cmd_nl80211_cb ssv_nl80211_cb;
extern driver_cmd_nl80211_cb sprd_nl80211_cb;
extern driver_cmd_nl80211_cb aic_nl80211_cb;

static driver_cmd_nl80211_cb *pcb = NULL;

static int check_initialed(void)
{
	const char *vendor_name = NULL;

	if (pcb != NULL)
		return 0;

	if ((vendor_name = get_wifi_vendor_name()) == NULL)
		return -1;

	wpa_printf(MSG_WARNING, "%s for nl80211, use %s wpa_supplicant_8_lib.", __func__, vendor_name);
	if (strcmp(vendor_name, "broadcom") == 0)
		pcb = &bcm_nl80211_cb;
	else if (strcmp(vendor_name, "realtek") == 0)
		pcb = &rtl_nl80211_cb;
	else if (strcmp(vendor_name, "xradio") == 0)
		pcb = &xr_nl80211_cb;
	else if (strcmp(vendor_name, "ssv") == 0)
		pcb = &ssv_nl80211_cb;
	else if (strcmp(vendor_name, "sprd") == 0)
		pcb = &sprd_nl80211_cb;
	else if (strcmp(vendor_name, "aic") == 0)
                pcb = &aic_nl80211_cb;
	else
		return -1;

	return 0;
}

int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf,
                                  size_t buf_len )
{
	if (check_initialed() == 0)
		return pcb->wpa_driver_nl80211_driver_cmd(priv, cmd, buf, buf_len);

	return 0;
}

int wpa_driver_set_p2p_noa(void *priv, u8 count, int start, int duration)
{
	if (check_initialed() == 0)
		return pcb->wpa_driver_set_p2p_noa(priv, count, start, duration);

	return 0;
}

int wpa_driver_get_p2p_noa(void *priv __unused, u8 *buf __unused, size_t len __unused)
{
	if (check_initialed() == 0)
		return pcb->wpa_driver_get_p2p_noa(priv, buf, len);

	return 0;
}

int wpa_driver_set_p2p_ps(void *priv, int legacy_ps, int opp_ps, int ctwindow)
{
	if (check_initialed() == 0)
		return pcb->wpa_driver_set_p2p_ps(priv, legacy_ps, opp_ps, ctwindow);

	return 0;
}

int wpa_driver_set_ap_wps_p2p_ie(void *priv, const struct wpabuf *beacon,
                                 const struct wpabuf *proberesp,
                                 const struct wpabuf *assocresp)
{
	if (check_initialed() == 0)
		return pcb->wpa_driver_set_ap_wps_p2p_ie(priv, beacon, proberesp, assocresp);

	return 0;
}

