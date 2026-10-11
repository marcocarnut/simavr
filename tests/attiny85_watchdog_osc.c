/*
	attiny85_watchdog_osc.c

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
 * The watchdog timed against the CPU, as firmware gathers entropy: timer 0
 * counts CPU cycles, and each watchdog interrupt takes its count. Eight of
 * them, written out in hex.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

static volatile uint8_t sample[8], n;

ISR(WDT_vect)
{
	if (n < 8)
		sample[n++] = TCNT0;
}

int main(void)
{
	static const char digit[] = "0123456789abcdef";

	TCCR0B = (1 << CS00);
	WDTCR = (1 << WDIE);		/* interrupt mode, 16 ms */
	sei();
	while (n < 8)
		;
	cli();
	WDTCR = 0;
	for (uint8_t i = 0; i < 8; i++) {
		USIDR = digit[sample[i] >> 4];
		USIDR = digit[sample[i] & 15];
	}
	sleep_cpu();
}
