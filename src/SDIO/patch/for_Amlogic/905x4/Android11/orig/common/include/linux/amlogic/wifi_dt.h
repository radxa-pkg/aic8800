/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _wifi_dt_h_
#define _wifi_dt_h_

void sdio_reinit(void);
char *get_wifi_inf(void);

void extern_wifi_set_enable(int is_on);
int wifi_irq_num(void);
void set_usb_bt_power(int is_power);

#ifdef CONFIG_PCI
/*amlogic 4.9 kernel support pci interface wifi*/
void pci_lock_rescan_remove(void);
struct pci_bus *pci_find_next_bus(const struct pci_bus *from);
unsigned int pci_rescan_bus(struct pci_bus *bus);
void pci_unlock_rescan_remove(void);
struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
			       struct pci_dev *from);
void pci_stop_and_remove_bus_device_locked(struct pci_dev *dev);
#endif

#endif /* _wifi_dt_h_ */
