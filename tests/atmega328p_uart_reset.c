/*
	atmega328p_uart_reset.c

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
 * UCSR0B as the reset leaves it, in GPIOR0; then with TXEN0 set by the
 * firmware, in GPIOR1. GPIOR2 says it's done.
 */
#include <avr/io.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega328p");

int main(void)
{
	GPIOR0 = UCSR0B;
	UCSR0B = 1 << TXEN0;
	GPIOR1 = UCSR0B;
	GPIOR2 = 1;
	for (;;)
		;
}
