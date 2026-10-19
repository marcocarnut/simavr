/*
	attiny85_usi_twowire_pin.c

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
 * SDA (PB0) and SCL (PB2) as outputs at 1 in the USI's two-wire mode, as
 * a slave sets them up. PINB goes on to GPIOR0; the test asks for
 * changes through GPIOR1 and sees them done in GPIOR2.
 */
#include <avr/io.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

int main(void)
{
	/* the pins driven high, then low, then the USI on, then released */
	DDRB = _BV(PB0) | _BV(PB2);
	PORTB = _BV(PB0) | _BV(PB2);
	PORTB = 0;
	USIDR = 0xff;
	USICR = _BV(USIWM1) | _BV(USICS1);
	PORTB = _BV(PB0) | _BV(PB2);
	for (;;) {
		uint8_t c = GPIOR1;
		if (c != GPIOR2) {
			switch (c) {
			case 1: PORTB = _BV(PB0) | _BV(PB2); break;	/* again */
			case 2: USIDR = 0x00; break;
			case 3: USIDR = 0xff; break;
			case 4: USICR = 0; break;
			case 5: USISR = _BV(USISIF) | _BV(USIOIF) | _BV(USIPF); break;
			}
			GPIOR2 = c;
		}
		GPIOR0 = PINB;
	}
}
