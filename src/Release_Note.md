# [Release_Note] aic8800d_linux_sdk_V5.0_2026_0123_5f7be68d

## Driver

### All driver 
- fix build error above kernel 6.12
- set bt lpm_enable=0 when cpmode is rftest
- fix powerlimit txt not ending with a newline panic
- wifi mac no need + 4 (need fw support)

### USB driver
- fix some issues with Bluetooth voice calls
- change REORDER_UPDATE_TIME to 50
- first add D80ND40N support
- fix memory leak
- fix some unused variable warning
-  move D80X2 bt patch to 0x120000, wifi fw to 0x128000 （need fw support）
- [aic_btusb] add testmode module_param for d80n, add x2p pid 0x8da1
- fix compile err above kernel 6.15
-  merge CONFIG_TEMP_CONTROL support
- merge get_txpwr from SDIO drv
- update aicwf_usb_resume, fix error after resume

### SDIO driver
- update load 8800d80x2 sdio bt ext patch
- first add D80ND40N support
- fix memory leak
-  fix assert in connect with xiaomi_ax6000 router relay mode
- support wowlan wake up
- distinguish between hw/sw_vendor_config and hw/sw_vendor_config_x2
- fix build error for kernel 3.10

### PCIE driver
- sync KYLIN support from usb driver
- fix cmd_wq create fail
- fix driver download and load while a fake interrupt comes problem
- add tx_thread CONFIG_TX_THREAD
- add CONFIG_CACHE_GUARD, default y
- update files in DEBIAN, support x86, add modprobe cfg80211
-  fix usbbt resume

### bt_vendor 
- add whitelist function after ble wakeup resume
- fix colse CONFIG MULTI_ADV_ENABLE
- fixed build err
- modify msbc process and MASK MSBC PLC, use AIC PLC
- fw log enable setting in aicbt.conf.

### wifi_hal

+ add set latency mode.

## BT Patch

- **DC-H** modify for pta with adv and le scan, add skp evt for le con for wifi iperf > 100M;
- **DC-H** modify for re_auth err in cts test;
- **DC-H** add le_rem_con_param_req_neg_reply cmd patch, set reason to 0x3b;
- **DC-H** add PROLINE check, ext fem, off as dft;
- **DC-H** make adv priority to 31 only;
- **DC-H** add wifi scan resch and modify for txpwr select err;
- **DC-H** modify for coex evt duration min, which cause assert during sch insert;
- **DC-H** modify for chmap init, which cause assert during le init;
- **DC-H** modify for le evt duration_min max to 1.56ms.

## WiFi Firmware

### D80/D40X2
- fix usb2.0 rx error
- 优化 2.4g dpd校准、5g loft校准

### D80D40 T/H 
- fix bt_clk bug
- mac addr mask
- scanu result bugfix
- 测试模式频偏校准支持设置0
- fix WS8700 router 4xltf 连接问题

### DCDWDL T/H

+ update DL-H vcore to 900mV
+ mac addr mask

## Tools
- **wifi_test** Support DC D80 remain print when use get_mac_addr and get_bt_mac_addr.
- **wifi_test** add cmd get_rssi
- **bt_test** fix send wrong mac_addr
- **bt_test** change pwrlimit to 0x7f

## AIC_Docs
- 