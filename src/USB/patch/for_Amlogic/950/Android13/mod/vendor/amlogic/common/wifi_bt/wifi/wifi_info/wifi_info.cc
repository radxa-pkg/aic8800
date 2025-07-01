/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <wifi_info.h>
#include <string.h>

/* @note
 * For some modules. need some basic drivers.
 * The value is a string
 * Driver paths and parameters are distinguished by # signs
 * can have multiple drivers.The more basic the driver is, the more advanced it is
**/
char uwe5621_base[] = "/vendor/lib/modules/uwe5621_bsp_sdio.ko#";
char mtk7663u_base[] = "/vendor/lib/modules/wlan_mt7663_usb_prealloc.ko#";
char mtk760_base[] = "/vendor/lib/modules/mtprealloc.ko#";
char ssv6051_base[] = "/vendor/lib/modules/ssv6051.ko#stacfgpath=/vendor/etc/wifi/ssv6051/ssv6051-wifi.cfg#/vendor/lib/modules/ssv6x5x.ko#tu_stacfgpath=/vendor/etc/wifi/ssv6x5x/ssv6x5x-wifi.cfg#";
char qca6391_base[] = "/vendor/lib/modules/wlan_cnss_core_pcie_6391.ko# #/vendor/lib/modules/wlan_resident_6391.ko#";
char rtl8852be_base[] = "/vendor/lib/modules/rtl8852be_rtkm.ko#";
char aml_w1_base[] = "/vendor/lib/modules/aml_sdio.ko#";
char aml_w1u_base[] = "/vendor/lib/modules/aml_com.ko#hif_type=USB#";
char aml_w1u_s_base[] = "/vendor/lib/modules/aml_com.ko#hif_type=SDIO#";
char qca206x_base[] ="/vendor/lib/modules/wlan_cnss_core_pcie_206x.ko# #/vendor/lib/modules/wlan_resident_206x.ko#";
char nxp8987_base[] ="/vendor/lib/modules/mlan_sd8987.ko#";
char nxp8997_base[] ="/vendor/lib/modules/mlan_sd8997.ko#";
char nxpiw620_base[] ="/vendor/lib/modules/mlan_iw620.ko#";
char aml_w2p_base[] = "/vendor/lib/modules/w2_comm.ko#bus_type=pci#";
char aml_w2s_base[] = "/vendor/lib/modules/w2_comm.ko#bus_type=sdio#";
char aml_w2u_base[] = "/vendor/lib/modules/w2_comm.ko#bus_type=usb#";
char aic_base[] = "/vendor/lib/modules/aic_load_fw.ko#";
char *no_base = NULL;

/*
 * @note Interpretation of information arrays.
 * for example {"0000","0000","sprdwl_ng","/vendor/lib/modules/sprdwl_ng.ko","",uwe5621_base,"uwe5621ds",0x0,""},\
 * @param chip_id 0000 sdio id
 * @param pcie_id 0000 pcie id
 * @param wifi_module_name sprdwl_ng wifi ko name
 * @param wifi_module_path /vendor/lib/modules/sprdwl_ng.ko wifi ko`s path
 * @param wifi_module_arg parameters needed to load KO,if not this is NULL
 * @param wifi_base uwe5621_base Some drivers require more than two drivers, the base ko is in this,There can be more than one base driver.
 * @param wifi_name uwe5621ds wifi module name
 * @param wifi_pid 0x0 usb pid
 * @param wifi_path wifi fw path,is only for bcm wifi
**/

static const dongle_info dongle_registerd[]={\
    {"0000","0000","sprdwl_ng","/vendor/lib/modules/sprdwl_ng.ko","",uwe5621_base,"uwe5621ds",0x0,""},\
    {"0000","0602","w2p","/vendor/lib/modules/w2.ko","",aml_w2p_base,"aml_w2p",0x0,""},\
    {"0000","0642","w2p","/vendor/lib/modules/w2.ko","",aml_w2p_base,"aml_w2p",0x0,""},\
    {"0600","0000","w2s","/vendor/lib/modules/w2.ko","",aml_w2s_base,"aml_w2s",0x0,""},\
    {"0640","0000","w2s","/vendor/lib/modules/w2.ko","",aml_w2s_base,"aml_w2s",0x0,""},\
    {"0000","0000","w2u","/vendor/lib/modules/w2.ko","",aml_w2u_base,"aml_w2u",0x0601,""},\
    {"0000","0000","w2u","/vendor/lib/modules/w2.ko","",aml_w2u_base,"aml_w2u",0x0641,""},\
    {"0000","0000","aic8800_fdrv","/vendor/lib/modules/aic8800_fdrv.ko","",aic_base,"aic",0x8d80,""},\
    {"0000","0000","aic8800_fdrv","/vendor/lib/modules/aic8800_fdrv.ko","",aic_base,"aic",0x8d81,""},\
    {"a962","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/40181/fw_bcm40181a2.bin nvram_path=../../etc/wifi/40181/nvram.txt",no_base,"bcm6210",0x0,"/vendor/etc/wifi/40181/fw_bcm40181a2"},\
    {"0000","0000","wlan_mt76x8_usb","/vendor/lib/modules/wlan_mt76x8_usb.ko","sta=wlan ap=ap p2p=p2p",no_base,"mtk7668u",0x7668,""},\
    {"4335","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/6335/fw_bcm4339a0_ag.bin nvram_path=../../etc/wifi/6335/nvram.txt",no_base,"bcm6335",0x0,"/vendor/etc/wifi/6335/fw_bcm4339a0_ag"},\
    {"0000","43c5","dhdpci","/vendor/lib/modules/dhdpci.ko","firmware_path=../..etc/wifi/4336/fw_bcm4336_ag.bin nvram_path=../../etc/wifi/4336/nvram.txt",no_base,"bcm4336",0x0,"/vendor/etc/wifi/4336/fw_bcm4336_ag"},\
    {"a94d","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/6234/fw_bcm43341b0_ag.bin nvram_path=../../etc/wifi/6234/nvram.txt",no_base,"bcm6234",0x0,"/vendor/etc/wifi/6234/fw_bcm43341b0_ag"},\
    {"a9bf","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/6255/fw_bcm43455c0_ag.bin nvram_path=../../etc/wifi/6255/nvram.txt",no_base,"bcm6255",0x0,"/vendor/etc/wifi/6255/fw_bcm43455c0_ag"},\
    {"aae7","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/AP6271/fw_bcm43751a1_ag.bin nvram_path=../../etc/wifi/AP6271/nvram_ap6271s.txt",no_base,"bcm6271",0x0,"/vendor/etc/wifi/AP6271/fw_bcm43751a1_ag"},\
    {"a9a6","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/6212/fw_bcm43438a0.bin nvram_path=../../etc/wifi/6212/nvram.txt",no_base,"bcm6212",0x0,"/vendor/etc/wifi/6212/fw_bcm43438a0"},\
    {"4362","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/43751/fw_bcm43751_ag.bin nvram_path=../../etc/wifi/43751/nvram.txt",no_base,"bcm43751",0x0,"/vendor/etc/wifi/43751/fw_bcm43751_ag"},\
    {"4345","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/43458/fw_bcm43455c0_ag.bin nvram_path=../../etc/wifi/43458/nvram_43458.txt",no_base,"bcm43458",0x0,"/vendor/etc/wifi/43458/fw_bcm43455c0_ag"},\
    {"4354","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/4354/fw_bcm4354a1_ag.bin nvram_path=../../etc/wifi/4354/nvram_ap6354.txt",no_base,"bcm6354",0x0,"/vendor/etc/wifi/4354/fw_bcm4354a1_ag"},\
    {"4356","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/4356/fw_bcm4356a2_ag.bin nvram_path=../../etc/wifi/4356/nvram_ap6356.txt",no_base,"bcm6356",0x0,"/vendor/etc/wifi/4356/fw_bcm4356a2_ag"},\
    {"4359","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/4359/fw_bcm4359c0_ag.bin nvram_path=../../etc/wifi/4359/nvram.txt",no_base,"bcm4359",0x0,"/vendor/etc/wifi/4359/fw_bcm4359c0_ag"},\
    {"0000","4415","dhdpci","/vendor/lib/modules/dhdpci.ko","firmware_path=../../etc/wifi/4359/fw_bcm4359c0_ag.bin nvram_path=../../etc/wifi/4359/nvram.txt",no_base,"bcm4359",0x0,"/vendor/etc/wifi/4359/fw_bcm4359c0_ag"},\
    {"aa31","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/4358/fw_bcm4358_ag.bin nvram_path=../../etc/wifi/4358/nvram_4358.txt",no_base,"bcm4358",0x0,"/vendor/etc/wifi/4358/fw_bcm4358_ag"},\
    {"8888","0000","vlsicomm","/vendor/lib/modules/vlsicomm.ko","vmac0=wlan0 vmac1=ap0 conf_path=w1",aml_w1_base,"aml_w1",0x0,""},\
    {"8888","0000","w1u","/vendor/lib/modules/w1u.ko","vmac0=wlan0 vmac1=ap0",aml_w1u_s_base,"aml_w1u_s",0x0,""},\
    {"0000","0000","w1u","/vendor/lib/modules/w1u.ko","vmac0=wlan0 vmac1=ap0",aml_w1u_base,"aml_w1u",0x4c55,""},\
    {"8179","0000","8189es","/vendor/lib/modules/8189es.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8189es",0x0,""},\
    {"b723","0000","8723bs","/vendor/lib/modules/8723bs.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8723bs",0x0,""},\
    {"c723","0000","8723cs","/vendor/lib/modules/8723cs.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8723cs",0x0,""},\
    {"f179","0000","8189fs","/vendor/lib/modules/8189fs.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8189ftv",0x0,""},\
    {"818b","0000","8192es","/vendor/lib/modules/8192es.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8192es",0x0,""},\
    {"0000","0000","8188eu","/vendor/lib/modules/8188eu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8188eu",0x8179,""},\
    {"0000","0000","8188gtvu","/vendor/lib/modules/8188gtvu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8188gtv",0x018c,""},\
    {"0000","0000","atbm603x_comb_wifi_usb","/vendor/lib/modules/atbm603x_comb_wifi_usb.ko","",no_base,"atbm603x",0x8888,""},\
    {"0000","0000","8188eu","/vendor/lib/modules/8188eu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8188eu",0x0179,""},\
    {"0000","0000","mt7601usta","/vendor/lib/modules/mt7601usta.ko","",mtk760_base,"mtk7601",0x7601,""},\
    {"0000","0000","mt7603usta","/vendor/lib/modules/mt7603usta.ko","",mtk760_base,"mtk7603",0x7603,""},\
    {"0000","0000","mt7662u_sta","/vendor/lib/modules/mt7662u_sta.ko","",no_base,"mtk7632",0x76a0,""},\
    {"0000","0000","mt7662u_sta","/vendor/lib/modules/mt7662u_sta.ko","",no_base,"mtk7632",0x76a1,""},\
    {"c821","0000","8821cs","/vendor/lib/modules/8821cs.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8821cs",0x0,""},\
    {"b821","0000","8821cs","/vendor/lib/modules/8821cs.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8821cs",0x0,""},\
    {"0000","0000","8723du","/vendor/lib/modules/8723du.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8723du",0xd723,""},\
    {"d723","0000","8723ds","/vendor/lib/modules/8723ds.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8723ds",0x0,""},\
    {"0000","0000","8821au","/vendor/lib/modules/8821au.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8821au",0x0823,""},\
    {"0000","0000","8821au","/vendor/lib/modules/8821au.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8821au",0x0821,""},\
    {"0000","0000","8821au","/vendor/lib/modules/8821au.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8821au",0x0811,""},\
    {"0000","0000","8812au","/vendor/lib/modules/8812au.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8812au",0x881a,""},\
    {"c822","0000","8822cs","/vendor/lib/modules/8822cs.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8822cs",0x0,""}, \
    {"0000","0000","8188fu","/vendor/lib/modules/8188fu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8188ftv",0xf179,""},\
    {"0000","0000","8192eu","/vendor/lib/modules/8192eu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8192eu",0x818b,""},\
    {"0000","0000","8192fu","/vendor/lib/modules/8192fu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8192fu",0xf192,""},\
    {"b822","0000","8822bs","/vendor/lib/modules/8822bs.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8822bs",0x0,""},\
    {"b852","0000","8852bs","/vendor/lib/modules/8852bs.ko","ifname=wlan0 if2name=p2p0",no_base,"rtl8852bs",0x0,""},\
    {"0000","0000","8733bu","/vendor/lib/modules/8733bu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8733bu",0xb733,""},\
    {"0000","0000","8852au","/vendor/lib/modules/8852au.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8852au",0x885c,""},\
    {"0000","0000","8852au","/vendor/lib/modules/8852au.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8852au",0x885a,""},\
    {"0701","0000","wlan","/vendor/lib/modules/wlan_9377.ko","",no_base,"qca9377",0x0,""},\
    {"050a","0000","wlan","/vendor/lib/modules/wlan_6174.ko","ap_name=ap0 country_code=CN",no_base,"qca6174",0x0,""},\
    {"0801","0000","wlan","/vendor/lib/modules/wlan_9379.ko","",no_base,"qca9379",0x0,""},\
    {"0000","0000","wlan","/vendor/lib/modules/wlan_9379.ko","",no_base,"qca9379",0x9378,""},\
    {"0000","0000","wlan","/vendor/lib/modules/wlan_9379.ko","",no_base,"qca9379",0x7a85,""},\
    {"7608","0000","wlan_mt76x8_sdio","/vendor/lib/modules/wlan_mt76x8_sdio.ko","sta=wlan ap=ap p2p=p2p",no_base,"mtk7668s",0x0,""},\
    {"7603","0000","wlan_mt7663_sdio","/vendor/lib/modules/wlan_mt7663_sdio.ko","",no_base,"mtk7661s",0x0,""},\
    {"037a","0000","wlan_mt7663_sdio","/vendor/lib/modules/wlan_mt7663_sdio.ko","",no_base,"mtk7661s",0x0,""},\
    {"0000","0000","wlan_mt7663_usb","/vendor/lib/modules/wlan_mt7663_usb.ko","",mtk7663u_base,"mtk7663u",0x7663,""},\
    {"0000","7961","wlan_mt7961_pcie","/vendor/lib/modules/wlan_mt7961_pcie.ko","",no_base,"mtk7961_pcie",0x0,""},\
    {"0000","0000","wlan_mt7961_usb","/vendor/lib/modules/wlan_mt7961_usb.ko","",no_base,"mtk7961_usb",0x7961,""},\
    {"0000","0000","bcmdhd","/vendor/lib/modules/bcmdhd.ko","firmware_path=../../etc/wifi/43569/fw_bcm4358u_ag.bin nvram_path=../../etc/wifi/43569/nvram_ap62x8.txt dhd_pwr_ctrl=0",no_base,"bcm43569",0xbd27,"/vendor/etc/wifi/43569/fw_bcm4358u_ag"}, \
    {"0000","0000","bcmdhd","/vendor/lib/modules/bcmdhd.ko","firmware_path=../../etc/wifi/43569/fw_bcm4358u_ag.bin nvram_path=../../etc/wifi/43569/nvram_ap62x8.txt dhd_pwr_ctrl=0",no_base,"bcm43569",0x0bdc,"/vendor/etc/wifi/43569/fw_bcm4358u_ag"}, \
    {"0000","0000","8723bu","/vendor/lib/modules/8723bu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8723bu",0xb720,""}, \
    {"0000","0000","8822bu","/vendor/lib/modules/8822bu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8822bu",0xb82c,""}, \
    {"0000","0000","88x2cu","/vendor/lib/modules/88x2cu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl88x2cu",0xc82c,""}, \
    {"0000","0000","88x2eu","/vendor/lib/modules/88x2eu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl88x2eu",0xa82a,""}, \
    {"0000","0000","8821cu","/vendor/lib/modules/8821cu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8821cu",0xc820,""}, \
    {"0000","0000","8821cu","/vendor/lib/modules/8821cu.ko","ifname=wlan0 if2name=ap0",no_base,"rtl8821cu",0xc811,""}, \
    {"3030","0000","ssv_hwif_ctrl","/vendor/lib/modules/ssv_hwif_ctrl.ko",no_base,ssv6051_base,"ssv6051",0x0,""}, \
    {"0000","449d","dhdpci","/vendor/lib/modules/dhdpci.ko","firmware_path=../../etc/wifi/43752a2/fw_bcm43752a2_pcie_ag.bin nvram_path=../../etc/wifi/43752a2/nvram_ap6275p.txt",no_base,"bcm43752a2p",0x0,"/vendor/etc/wifi/43752a2/fw_bcm43752a2_pcie_ag"},\
    {"0000","4475","dhdpci","/vendor/lib/modules/dhdpci.ko","firmware_path=../../etc/wifi/43752a2/fw_bcm4375b4_pcie_ag.bin nvram_path=../../etc/wifi/43752a2/nvram_ap6275hh3.txt",no_base,"bcm43752a2p",0x0,"/vendor/etc/wifi/43752a2/fw_bcm4375b4_pcie_ag"},\
    {"aae8","0000","dhd","/vendor/lib/modules/dhd.ko","firmware_path=../../etc/wifi/43752a2/fw_bcm43752a2_ag.bin nvram_path=../../etc/wifi/43752a2/nvram_ap6275s.txt",no_base,"bcm43752a2s",0x0,"/vendor/etc/wifi/43752a2/fw_bcm43752a2_ag"},\
    {"0000","1101","wlan","/vendor/lib/modules/wlan_6391.ko","",qca6391_base,"qca6391",0x0,""},\
    {"0000","1103","wlan","/vendor/lib/modules/wlan_206x.ko","",qca206x_base,"qca206x",0x0,""},\
    {"0000","8852","8852ae","/vendor/lib/modules/8852ae.ko","ifname=wlan0 if2name=p2p0",rtl8852be_base,"rtl8852ae",0x0,""},\
    {"0000","b852","8852be","/vendor/lib/modules/8852be.ko","ifname=wlan0 if2name=p2p0",rtl8852be_base,"rtl8852be",0x0,""},\
    {"9149","0000","moal_sd8987","/vendor/lib/modules/moal_sd8987.ko","mod_para=nxp/wifi_mod_para_8987.conf",nxp8987_base,"nxp8987",0x0,""},\
    {"9141","0000","moal_sd8997","/vendor/lib/modules/moal_sd8997.ko","mod_para=nxp/wifi_mod_para_8997.conf",nxp8997_base,"nxp8997",0x0,""},\
    {"0000","2b56","moal_iw620","/vendor/lib/modules/moal_iw620.ko","mod_para=nxp/wifi_mod_para_iw620.conf",nxpiw620_base,"nxpiw620",0x0,""},\
    {"0000","0000","ssv6x5x","/vendor/lib/modules/ssv6x5x.ko","tu_stacfgpath=/vendor/etc/wifi/ssv6x5x/ssv6x5x-wifi.cfg",no_base,"ssv6155",0x6000,""}};

int get_wifi_info (dongle_info *ext_info)
{
    int i;
    for (i = 0; i <(int)(sizeof(dongle_registerd)/sizeof(dongle_info)); i++)
    {
        ext_info[i] = dongle_registerd[i];
    }
    return (int)(sizeof(dongle_registerd)/sizeof(dongle_info));
}
