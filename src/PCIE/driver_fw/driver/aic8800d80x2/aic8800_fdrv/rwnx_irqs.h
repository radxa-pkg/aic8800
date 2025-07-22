/**
 ******************************************************************************
 *
 * @file rwnx_irqs.h
 *
 * Copyright (C) RivieraWaves 2012-2019
 *
 ******************************************************************************
 */
#ifndef _RWNX_IRQS_H_
#define _RWNX_IRQS_H_

#include <linux/interrupt.h>

/* IRQ handler to be registered by platform driver */
irqreturn_t rwnx_irq_hdlr(int irq, void *dev_id);

void rwnx_task(unsigned long data);
void rwnx_txrestart_task(unsigned long data);
#ifdef CONFIG_RX_TASKLET
void rwnx_task_rx_process(unsigned long data);
#endif
#endif /* _RWNX_IRQS_H_ */
