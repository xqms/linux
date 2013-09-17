/*
 * (C) Copyright 2009 Intel Corporation
 * Author: Jacob Pan (jacob.jun.pan@intel.com)
 *
 * Shared with ARM platforms, Jamie Iles, Picochip 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Support for the Synopsys DesignWare APB Timers.
 */
#ifndef __DW_APB_TIMER_H__
#define __DW_APB_TIMER_H__

#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>

#define APBTMRS_REG_SIZE       0x14

/* The IP uses two registers for count and values, to provide 64bit accuracy
 * on 32bit platforms. The additional registers move the following registers
 * down by 0x8 byte, as both the count and value registers are duplicated.
 */
#define APBTMR_QUIRK_64BIT_COUNTER	BIT(0)

/* The IP does not provide a end-of-interrupt register to clear pending
 * interrupts, but requires to write a 1 to the interrupt-status register.
 */
#define APBTMR_QUIRK_NO_EOI			BIT(1)

/* The IP uses an inverted interrupt-mask bit. 
 * Instead of activating interrupts clearing a maks bit, it needs an enable
 * bit to be set 1.
 */
#define APBTMR_QUIRK_INVERSE_INTMASK	BIT(2)

/* The IP uses inverted logic for the bit setting periodic mode.
 * Periodic means it times out after the period is over and is set to
 * 1 in the original IP. This IP uses 1 for free running mode.
 */
#define APBTMR_QUIRK_INVERSE_PERIODIC	BIT(3)

struct dw_apb_timer {
	void __iomem				*base;
	unsigned long				freq;
	int					irq;
	int					quirks;
	u8					reg_load_count;
	u8					reg_current_value;
	u8					reg_control;
	u8					reg_eoi;
	u8					reg_int_status;
};

struct dw_apb_clock_event_device {
	struct clock_event_device		ced;
	struct dw_apb_timer			timer;
	struct irqaction			irqaction;
	void					(*eoi)(struct dw_apb_timer *);
};

struct dw_apb_clocksource {
	struct dw_apb_timer			timer;
	struct clocksource			cs;
};

void dw_apb_clockevent_register(struct dw_apb_clock_event_device *dw_ced);
void dw_apb_clockevent_pause(struct dw_apb_clock_event_device *dw_ced);
void dw_apb_clockevent_resume(struct dw_apb_clock_event_device *dw_ced);
void dw_apb_clockevent_stop(struct dw_apb_clock_event_device *dw_ced);

struct dw_apb_clock_event_device *
dw_apb_clockevent_init(int cpu, const char *name, unsigned rating,
		       void __iomem *base, int irq, unsigned long freq,
		       int quirks);
struct dw_apb_clocksource *
dw_apb_clocksource_init(unsigned rating, const char *name, void __iomem *base,
			unsigned long freq, int quirks);
void dw_apb_clocksource_register(struct dw_apb_clocksource *dw_cs);
void dw_apb_clocksource_start(struct dw_apb_clocksource *dw_cs);
cycle_t dw_apb_clocksource_read(struct dw_apb_clocksource *dw_cs);

#endif /* __DW_APB_TIMER_H__ */
