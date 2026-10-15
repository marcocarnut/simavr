/*
	atmega32u4_acomp_icp.c

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
 * Timer 1's input capture from the analog comparator: the bandgap on its +
 * input, ADC7 through the multiplexer on its - input (ACBG, ACIC, ACME,
 * ADEN off, MUX 7). Timer 1 counts at clk/1; GPIOR1, set by the test
 * before the firmware runs, gives TCCR1B's ICNC1 and ICES1. Each capture
 * counts in GPIOR2. Then a sled of nops, so the test can step the input at
 * the cycle it chooses; when the test sets GPIOR1's bit 0, ACIC goes off,
 * and the ICP pin (PD4) is the capture's input again.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega32u4");

ISR(TIMER1_CAPT_vect)
{
	GPIOR2++;
}

int main(void)
{
	ADCSRA = 0;
	ADCSRB = 1 << ACME;
	ADMUX = 7;
	ACSR = (1 << ACBG) | (1 << ACIC);
	TCCR1A = 0;
	TCCR1B = (GPIOR1 & ((1 << ICNC1) | (1 << ICES1))) | (1 << CS10);
	TIFR1 = 1 << ICF1;
	TIMSK1 = 1 << ICIE1;
	sei();
	for (;;) {
		asm volatile (".rept 1000\n\tnop\n\t.endr");
		if (GPIOR1 & 1)
			ACSR = 1 << ACBG;
	}
}
