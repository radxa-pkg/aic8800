#
# wifi bt config
#

#
# BOARD_CONNECTIVITY_VENDOR:
# for broadcom, realteck wifi, bt
# Broadcom:
#           rk90x,
#           ap6xxx,
#           ap6xxx_gps,      #like ap6476
#
# for mtk wifi, bt
# MediaTek: 
#           combo_mt66xx,
#           mt5931_6622,   
#
# for realtek wifi, bt & rda587x bt
# RealTek:    
#         
#           rtl81xx,         #only wifi
#           rtl8723as,       #like  rtl8723as sdio
#           rtl8723bs,       #like  rtl8723bs sdio
#           rtl8723au,       #like  rtl8723au usb
#           rtl8723bu,       #like  rtl8723bu usb
#           rda587x,         #like  rtl8188+rda587x
#           mt6622,          #like  rtl8188+mt6622 
#
# for Espressif wifi & Beken bt
# Espressif:
#           esp8089_bk3515,
#           esp8089,         #only wifi
#           mt6622,          #like esp8089+mt6622
#           rda587x,         #like esp8089+rda587x
#
# MediaTek_mt7601:
#	    mt7601           #only wifi
#           rda587x,         #like  rtl8188+rda587x
#           mt6622,          #like  rtl8188+mt6622 
#

BOARD_CONNECTIVITY_VENDOR := Broadcom
BOARD_CONNECTIVITY_MODULE := ap6xxx
