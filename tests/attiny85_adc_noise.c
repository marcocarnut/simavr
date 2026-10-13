/*
	attiny85_adc_noise.c

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

/*
 * The ADC as firmware gathers entropy from it: ADC0-ADC0 at 20x gain,
 * bipolar, Vcc as the reference -- the amplifier's offset and noise, and
 * nothing else. 32 readings, written out in hex as signed bytes.
 */
#include <avr/io.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

#define N	32

int main(void)
{
	static const char digit[] = "0123456789abcdef";
	static int8_t sample[N];

	ADMUX = 0x09;			/* ADC0-ADC0, 20x; REFS = Vcc */
	ADCSRB = (1 << BIN);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	for (uint8_t i = 0; i < N; i++) {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC))
			;
		int16_t v = ADC;
		v = (v & 0x200) ? v - 0x400 : v;	/* 10-bit two's complement */
		sample[i] = v > 127 ? 127 : v < -128 ? -128 : v;
	}
	for (uint8_t i = 0; i < N; i++) {
		USIDR = digit[(uint8_t)sample[i] >> 4];
		USIDR = digit[sample[i] & 15];
	}
	sleep_cpu();
}
