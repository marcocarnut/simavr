/*
	avr_timer_hs.c

	The 10-bit high speed timer of the ATmega16U4/32U4 (Timer/Counter4),
	in its normal mode. See avr_timer_hs.h for what it does and doesn't do.

	The counter is kept as the cycle it was 0 at (base) and the cycles per
	count (prescale), as avr_timer does. As there, a match sets its flag and
	drives its pin one count after TCNT4 reaches OCR4x, and TOV4 is set as
	TCNT4 wraps to 0; the datasheet's synchronisation delays of a cycle or so
	are not modelled.

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "avr_timer_hs.h"
#include "avr_ioport.h"

#define HS_MAX		0x3ff	// the counter is 10 bits
#define HS_TOP_MIN	3		// a smaller OCR4C reads and works as 3

static uint16_t
hs_tcnt(
		avr_timer_hs_t * p,
		avr_cycle_count_t now)
{
	if (!p->prescale)
		return p->tcnt;
	return ((now - p->base) / p->prescale) % (p->period_top + 1);
}

static avr_cycle_count_t
hs_comp_when(
		avr_timer_hs_t * p,
		int ci)
{
	return p->base + (avr_cycle_count_t)p->prescale * (p->comp[ci].ocr + 1);
}

static avr_cycle_count_t
hs_tov_when(
		avr_timer_hs_t * p)
{
	return p->base + (avr_cycle_count_t)p->prescale * (p->period_top + 1);
}

// the first event of this period after cycle 'after': a match, or the wrap
static avr_cycle_count_t
hs_next_event(
		avr_timer_hs_t * p,
		avr_cycle_count_t after)
{
	avr_cycle_count_t next = hs_tov_when(p);
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++) {
		if (!p->comp[ci].r_ocr || p->comp[ci].ocr > p->period_top)
			continue;
		avr_cycle_count_t when = hs_comp_when(p, ci);
		if (when > after && when < next)
			next = when;
	}
	return next;
}

static void
hs_waveform(
		avr_timer_hs_t * p,
		int ci)
{
	avr_t * avr = p->io.avr;
	avr_timer_hs_comp_t * c = &p->comp[ci];
	uint8_t ocw = c->ocw;

	switch (avr_regbit_get(avr, c->com)) {
		case avr_timer_com_normal:
			return;
		case avr_timer_com_toggle:
			ocw = !ocw;
			break;
		case avr_timer_com_clear:
			ocw = 0;
			break;
		case avr_timer_com_set:
			ocw = 1;
			break;
	}
	c->ocw = ocw;
	avr_raise_irq(p->io.irq + TIMER_IRQ_OUT_COMP + ci,
			ocw | (c->com_pin.reg ? AVR_IOPORT_OUTPUT : 0));
}

static avr_cycle_count_t
hs_event(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)param;

	if (!p->prescale)
		return 0;
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++) {
		if (!p->comp[ci].r_ocr || p->comp[ci].ocr > p->period_top ||
				hs_comp_when(p, ci) != when || when == p->blocked)
			continue;
		avr_raise_interrupt(avr, &p->comp[ci].interrupt);
		hs_waveform(p, ci);
	}
	if (hs_tov_when(p) == when) {
		avr_raise_interrupt(avr, &p->overflow);
		p->base = when;
		p->period_top = p->top;
	}
	return hs_next_event(p, when);
}

// the counter, the prescaler, TOP or an OCR4x changed: schedule again
static void
hs_schedule(
		avr_timer_hs_t * p)
{
	avr_t * avr = p->io.avr;

	avr_cycle_timer_cancel(avr, hs_event, p);
	if (!p->prescale)
		return;
	// events up to this cycle are past
	avr_cycle_timer_register(avr,
			hs_next_event(p, avr->cycle) - avr->cycle, hs_event, p);
}

// start the period the counter is in again, from value tcnt now
static void
hs_set_tcnt(
		avr_timer_hs_t * p,
		uint16_t tcnt)
{
	avr_t * avr = p->io.avr;

	p->tcnt = tcnt;
	p->period_top = tcnt > p->top ? HS_MAX : p->top;
	p->base = avr->cycle - (avr_cycle_count_t)p->prescale * tcnt;
}

static void
hs_warn(
		avr_timer_hs_t * p)
{
	avr_t * avr = p->io.avr;
	int pwm = 0;
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++)
		pwm |= avr_regbit_get(avr, p->comp[ci].pwm);
	if (p->warned)
		return;
	if (pwm || avr_regbit_get(avr, p->enhc) ||
			(p->prescale && avr_regbit_get(avr, p->pll_tm))) {
		AVR_LOG(avr, LOG_WARNING,
				"TIMER%c: only the normal mode on the system clock is done "
				"(PWM, enhanced mode, the PLL clock are not)\n", p->name);
		p->warned = 1;
	}
}

static void
hs_write_control(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)param;
	uint8_t old_cs = avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs));
	uint8_t foc = 0, psr = 0;

	// the force and prescaler reset strobes act, and read as 0
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++) {
		avr_regbit_t f = p->comp[ci].foc;
		if (f.reg == addr && (v & (1 << f.bit))) {
			foc |= 1 << ci;
			v &= ~(1 << f.bit);
		}
	}
	if (p->psr.reg == addr && (v & (1 << p->psr.bit))) {
		psr = 1;
		v &= ~(1 << p->psr.bit);
	}
	avr_core_watch_write(avr, addr, v);

	// COM4A and COM4B are in TCCR4A and, as a copy, in TCCR4C
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++) {
		avr_timer_hs_comp_t * c = &p->comp[ci];
		if (!c->com_shadow.reg)
			continue;
		if (c->com.reg == addr)
			avr_regbit_setto(avr, c->com_shadow, avr_regbit_get(avr, c->com));
		else if (c->com_shadow.reg == addr)
			avr_regbit_setto(avr, c->com, avr_regbit_get(avr, c->com_shadow));
	}

	uint8_t cs = avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs));
	if (cs != old_cs || psr) {
		uint16_t tcnt = hs_tcnt(p, avr->cycle);
		p->prescale = cs ? 1 << (cs - 1) : 0;
		if (p->prescale) {
			hs_set_tcnt(p, tcnt);
			if (p->block_next) {
				p->blocked = p->base + (avr_cycle_count_t)p->prescale * (tcnt + 1);
				p->block_next = 0;
			}
		} else
			p->tcnt = tcnt;
		hs_schedule(p);
	}
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++)
		if (foc & (1 << ci))
			hs_waveform(p, ci);
	hs_warn(p);
}

static void
hs_write_tch(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_core_watch_write(avr, addr, v & (HS_MAX >> 8));
}

static void
hs_write_tcnt(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)param;
	uint16_t tcnt = (avr->data[p->r_tch] << 8) | v;

	avr_core_watch_write(avr, addr, v);
	hs_set_tcnt(p, tcnt);
	// a match in the next count is blocked
	if (p->prescale)
		p->blocked = p->base + (avr_cycle_count_t)p->prescale * (tcnt + 1);
	else
		p->block_next = 1;
	hs_schedule(p);
}

static uint8_t
hs_read_tcnt(
		struct avr_t * avr,
		avr_io_addr_t addr,
		void * param)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)param;
	uint16_t tcnt = hs_tcnt(p, avr->cycle);

	avr->data[p->r_tcnt] = tcnt;
	avr->data[p->r_tch] = tcnt >> 8;
	return avr_core_watch_read(avr, addr);
}

static void
hs_write_ocr(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)param;
	uint16_t ocr = (avr->data[p->r_tch] << 8) | v;

	// in the normal mode the OCR4x are not double buffered
	if (addr == p->r_ocrc) {
		if (ocr < HS_TOP_MIN)
			ocr = HS_TOP_MIN;
		uint16_t tcnt = hs_tcnt(p, avr->cycle);
		p->top = ocr;
		if (p->prescale) {
			// past the new TOP, the counter goes on to MAX
			avr_cycle_count_t base = p->base;
			hs_set_tcnt(p, tcnt);
			p->base = base;
		}
	}
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++)
		if (p->comp[ci].r_ocr == addr)
			p->comp[ci].ocr = ocr;
	avr_core_watch_write(avr, addr, ocr);
	hs_schedule(p);
}

static uint8_t
hs_read_ocr(
		struct avr_t * avr,
		avr_io_addr_t addr,
		void * param)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)param;
	uint16_t ocr = p->top;

	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++)
		if (p->comp[ci].r_ocr == addr)
			ocr = p->comp[ci].ocr;
	avr->data[p->r_tch] = ocr >> 8;
	return avr_core_watch_read(avr, addr);
}

// TIFR4: writing 1 clears a flag
static void
hs_write_flags(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)param;

	if (avr_regbit_from_value(avr, p->overflow.raised, v))
		avr_clear_interrupt(avr, &p->overflow);
	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++)
		if (avr_regbit_from_value(avr, p->comp[ci].interrupt.raised, v))
			avr_clear_interrupt(avr, &p->comp[ci].interrupt);
}

static void
hs_reset(
		avr_io_t * port)
{
	avr_timer_hs_t * p = (avr_timer_hs_t *)port;
	avr_t * avr = p->io.avr;

	avr_cycle_timer_cancel(avr, hs_event, p);
	p->top = HS_MAX >> 2;	// 0xff
	p->tcnt = 0;
	p->prescale = 0;
	p->base = 0;
	p->period_top = p->top;
	p->blocked = 0;
	p->block_next = 0;
	p->warned = 0;
	avr->data[p->r_ocrc] = p->top;

	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++) {
		p->comp[ci].ocr = 0;
		p->comp[ci].ocw = 0;

		avr_ioport_getirq_t req = {
			.bit = p->comp[ci].com_pin
		};
		if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req) > 0)
			avr_connect_irq(&port->irq[TIMER_IRQ_OUT_COMP + ci], req.irq[0]);
	}
}

static const char * irq_names[TIMER_IRQ_COUNT] = {
	[TIMER_IRQ_OUT_PWM0] = "8>pwm0",
	[TIMER_IRQ_OUT_PWM1] = "8>pwm1",
	[TIMER_IRQ_OUT_PWM2] = "8>pwm2",
	[TIMER_IRQ_IN_ICP] = "<icp",
	[TIMER_IRQ_OUT_COMP + AVR_TIMER_HS_COMPA] = ">compa",
	[TIMER_IRQ_OUT_COMP + AVR_TIMER_HS_COMPB] = ">compb",
	[TIMER_IRQ_OUT_COMP + AVR_TIMER_HS_COMPD] = ">compd",
};

static avr_io_t _io = {
	.kind = "timer",
	.irq_names = irq_names,
	.reset = hs_reset,
};

static void
hs_register_control(
		avr_timer_hs_t * p,
		avr_regbit_t rb)
{
	// the same handler twice on a register is fine
	if (rb.reg)
		avr_register_io_write(p->io.avr, rb.reg, hs_write_control, p);
}

void
avr_timer_hs_init(
		avr_t * avr,
		avr_timer_hs_t * p)
{
	p->io = _io;

	avr_register_io(avr, &p->io);
	avr_io_setirqs(&p->io, AVR_IOCTL_TIMER_GETIRQ(p->name), TIMER_IRQ_COUNT, NULL);

	avr_register_vector(avr, &p->overflow);
	avr_register_io_write(avr, p->overflow.raised.reg, hs_write_flags, p);

	for (int i = 0; i < ARRAY_SIZE(p->cs); i++)
		hs_register_control(p, p->cs[i]);
	hs_register_control(p, p->psr);
	for (int i = 0; i < ARRAY_SIZE(p->wgm); i++)
		hs_register_control(p, p->wgm[i]);
	hs_register_control(p, p->enhc);

	for (int ci = 0; ci < AVR_TIMER_HS_COMP_COUNT; ci++) {
		avr_timer_hs_comp_t * c = &p->comp[ci];

		avr_register_vector(avr, &c->interrupt);
		hs_register_control(p, c->com);
		hs_register_control(p, c->com_shadow);
		hs_register_control(p, c->foc);
		hs_register_control(p, c->pwm);
		if (c->r_ocr) {
			avr_register_io_write(avr, c->r_ocr, hs_write_ocr, p);
			avr_register_io_read(avr, c->r_ocr, hs_read_ocr, p);
		}
	}
	avr_register_io_write(avr, p->r_ocrc, hs_write_ocr, p);
	avr_register_io_read(avr, p->r_ocrc, hs_read_ocr, p);
	avr_register_io_write(avr, p->r_tch, hs_write_tch, p);
	avr_register_io_write(avr, p->r_tcnt, hs_write_tcnt, p);
	avr_register_io_read(avr, p->r_tcnt, hs_read_tcnt, p);
}
