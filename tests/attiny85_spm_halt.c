/*
	attiny85_spm_halt.c

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

/*
 * A page erase and a page write, with Timer0 (clk/8) and Timer1 (clk/64)
 * overflowing meanwhile. What happens goes to GPIOR0: 's' before the
 * erase, 'e' after it, 'w' after the write, '0' and '1' from the timers'
 * overflow interrupts.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

ISR(TIMER0_OVF_vect)
{
	GPIOR0 = '0';
}

ISR(TIMER1_OVF_vect)
{
	GPIOR0 = '1';
}

#define PAGE	0x1000

int main(void)
{
	for (int i = 0; i < SPM_PAGESIZE; i += 2)
		boot_page_fill(PAGE + i, 0x1234);
	TIMSK = _BV(TOIE0) | _BV(TOIE1);
	sei();
	TCCR0B = 2;		/* clk/8: overflows every 2048 cycles */
	TCCR1 = 7;		/* clk/64: every 16384 */
	GPIOR0 = 's';
	boot_page_erase(PAGE);
	GPIOR0 = 'e';
	cli();
	boot_page_write(PAGE);
	sei();
	GPIOR0 = 'w';
	cli();
	for (;;)
		;
}
