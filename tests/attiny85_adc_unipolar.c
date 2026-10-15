/*
	attiny85_adc_unipolar.c

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
 * Differential, unipolar (BIN clear): the result is from 0 to 0x3ff, and a
 * voltage outside that range reads the nearest end -- a negative one 0, one
 * over full scale 0x3ff. The harness puts 1500 mV on ADC2 and 500 mV on
 * ADC3, Vcc 5000 mV as the reference: ADC2-ADC3 at 1x, at 20x (over full
 * scale), and at 1x with IPR (the polarity reversed: -1000 mV).
 */
#include <avr/io.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

static void say_n(uint16_t n)
{
	char b[5];
	int8_t i = 0;
	do
		b[i++] = '0' + n % 10;
	while ((n /= 10));
	while (i)
		USIDR = b[--i];
}

static uint16_t convert(uint8_t mux, uint8_t srb)
{
	ADMUX = mux;			/* REFS = Vcc */
	ADCSRB = srb;
	ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	while (ADCSRA & (1 << ADSC))
		;
	return ADC;
}

int main(void)
{
	say_n(convert(0x06, 0));		/* ADC2-ADC3, 1x */
	USIDR = ' ';
	say_n(convert(0x07, 0));		/* ADC2-ADC3, 20x */
	USIDR = ' ';
	say_n(convert(0x06, 1 << IPR));		/* ADC3-ADC2, 1x */
	sleep_cpu();
}
