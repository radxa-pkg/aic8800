/*
 * Driver interaction with extended Linux Wireless Extensions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 */
#ifndef DRIVER_CMD_WEXT_H
#define DRIVER_CMD_WEXT_H

#define WEXT_NUMBER_SCAN_CHANNELS_FCC	11
#define WEXT_NUMBER_SCAN_CHANNELS_ETSI	13
#define WEXT_NUMBER_SCAN_CHANNELS_MKK1	14

#define WPA_DRIVER_WEXT_WAIT_US		400000
#define WEXT_CSCAN_BUF_LEN		360
#define WEXT_CSCAN_HEADER		"CSCAN S\x01\x00\x00S\x00"
#define WEXT_CSCAN_HEADER_SIZE		12
#define WEXT_CSCAN_SSID_SECTION		'S'
#define WEXT_CSCAN_CHANNEL_SECTION	'C'
#define WEXT_CSCAN_NPROBE_SECTION	'N'
#define WEXT_CSCAN_ACTV_DWELL_SECTION	'A'
#define WEXT_CSCAN_PASV_DWELL_SECTION	'P'
#define WEXT_CSCAN_HOME_DWELL_SECTION	'H'
#define WEXT_CSCAN_TYPE_SECTION		'T'
#define WEXT_CSCAN_TYPE_DEFAULT		0
#define WEXT_CSCAN_TYPE_PASSIVE		1
#define WEXT_CSCAN_PASV_DWELL_TIME	130
#define WEXT_CSCAN_PASV_DWELL_TIME_DEF	250
#define WEXT_CSCAN_PASV_DWELL_TIME_MAX	3000
#define WEXT_CSCAN_HOME_DWELL_TIME	130

#endif /* DRIVER_CMD_WEXT_H */
