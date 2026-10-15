/*
	avr_timer_hs.h

	The 10-bit high speed timer of the ATmega16U4/32U4 (Timer/Counter4)

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

#ifndef __AVR_TIMER_HS_H__
#define __AVR_TIMER_HS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"
#include "avr_timer.h"

/*
 * Normal mode only: the counter counts from 0 to TOP (OCR4C) and starts
 * again from 0, setting TOV4; compare units A, B and D set their flags and
 * set, clear or toggle their OC4x pins on a match. The 10-bit registers are
 * accessed through the shared TC4H, as on the chip. The synchronous clock
 * (CK and its prescaler) only.
 * Not done: the PWM modes, dead time, fault protection, enhanced mode, the
 * PLL clock, TLOCK4, and the complementary outputs.
 *
 * Its IRQs are those of the other timers, from AVR_IOCTL_TIMER_GETIRQ(name):
 * TIMER_IRQ_OUT_COMP + 0, 1 and 2 are compare units A, B and D.
 */

enum {
	AVR_TIMER_HS_COMPA = 0,
	AVR_TIMER_HS_COMPB,
	AVR_TIMER_HS_COMPD,

	AVR_TIMER_HS_COMP_COUNT
};

typedef struct avr_timer_hs_comp_t {
	avr_int_vector_t	interrupt;
	avr_io_addr_t		r_ocr;			// low byte; TC4H holds the high bits
	avr_regbit_t		com;			// compare output mode
	avr_regbit_t		com_shadow;		// a copy of it in another register, if any
	avr_regbit_t		foc;			// force output compare strobe
	avr_regbit_t		pwm;			// PWM4x: not supported, warns
	avr_regbit_t		com_pin;		// OC4x

	uint16_t			ocr;			// the 10-bit value
	uint8_t				ocw;			// the waveform output
} avr_timer_hs_comp_t;

typedef struct avr_timer_hs_t {
	avr_io_t		io;
	char			name;

	avr_io_addr_t	r_tcnt;			// low byte
	avr_io_addr_t	r_tch;			// TC4H, the shared high bits
	avr_io_addr_t	r_ocrc;			// TOP, low byte

	avr_regbit_t	cs[4];
	avr_regbit_t	psr;			// prescaler reset strobe
	avr_regbit_t	wgm[2];			// with PWM4x only: not supported
	avr_regbit_t	enhc;			// enhanced mode: not supported
	avr_regbit_t	pll_tm;			// PLL clock: not supported

	avr_timer_hs_comp_t comp[AVR_TIMER_HS_COMP_COUNT];
	avr_int_vector_t overflow;

	// the counter's state
	uint16_t		top;			// OCR4C
	uint16_t		tcnt;			// while stopped
	uint32_t		prescale;		// cycles per count, 0 if stopped
	avr_cycle_count_t base;			// when the counter was (or would have been) 0
	uint16_t		period_top;		// where this period ends: TOP, or MAX when TCNT was set past it
	avr_cycle_count_t blocked;		// a match due then is blocked by a write to TCNT4
	uint8_t			block_next;		// TCNT4 written while stopped: block the first count's match
	uint8_t			warned;
} avr_timer_hs_t;

void avr_timer_hs_init(avr_t * avr, avr_timer_hs_t * p);

#ifdef __cplusplus
};
#endif

#endif /* __AVR_TIMER_HS_H__ */
