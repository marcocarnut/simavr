/*
	attiny85_watchdog_rereset.c

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
 * After a watchdog reset the watchdog stays on, at its shortest timeout:
 * firmware that leaves it alone is reset again 16 ms later. Runs 100 ms,
 * then lets the watchdog reset it; then leaves it alone twice; then stops.
 * The resets are counted in EEPROM.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

int main(void)
{
	if (MCUSR & (1 << WDRF)) {
		uint8_t n = eeprom_read_byte((uint8_t *)0) + 1;	/* 0xFF + 1: the first */
		eeprom_write_byte((uint8_t *)0, n);
		if (n >= 2) {
			MCUSR = 0;
			wdt_disable();
			cli();
			sleep_cpu();
		}
	} else {
		_delay_ms(100);
		wdt_enable(WDTO_15MS);
	}
	sei();
	for (;;)
		;
}
