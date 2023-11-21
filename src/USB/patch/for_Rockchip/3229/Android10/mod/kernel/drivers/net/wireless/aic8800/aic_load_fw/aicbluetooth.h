#ifndef _AICBLUETOOTH_H
#define _AICBLUETOOTH_H

int aic_bt_platform_init(struct aic_usb_dev *sdiodev);

void aic_bt_platform_deinit(struct aic_usb_dev *sdiodev);
#if defined(CONFIG_PLATFORM_UBUNTU)
int rwnx_plat_bin_fw_upload_pc(struct aic_usb_dev *usbdev, u32 fw_addr,
                                char *filename);
#else
int rwnx_plat_bin_fw_upload_android(struct aic_usb_dev *sdiodev, u32 fw_addr,
                               char *filename);

int rwnx_plat_nvram_upload_android(struct aic_usb_dev *usbdev, char *filename);

#endif // (CONFIG_PLATFORM_UBUNTU)

#endif
