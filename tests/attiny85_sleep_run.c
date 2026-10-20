/*
	attiny85_sleep_run.c

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
 * Sleeps in idle mode, Timer1 overflowing now and then (clk/16384), and
 * wakes on a pin change on PB3, which writes 1 to GPIOR0.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

EMPTY_INTERRUPT(TIMER1_OVF_vect)

ISR(PCINT0_vect)
{
	GPIOR0 = 1;
}

int main(void)
{
	TIMSK = _BV(TOIE1);
	TCCR1 = 15;
	PCMSK = _BV(PCINT3);
	GIMSK = _BV(PCIE);
	set_sleep_mode(SLEEP_MODE_IDLE);
	sei();
	for (;;)
		sleep_mode();
}
