/*
	atmega88_timer1_small_ocr.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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
 * Timer 1 in normal mode: a compare match at OCR1A 0 to 3, set late in the
 * period before. The overflow is serviced at the end of whatever
 * instruction runs when TCNT1 wraps, a few cycles late; a match due in
 * those cycles must still come in this period, not 65536 cycles later.
 * The main loop runs instructions of 1 to 4 cycles so that the lateness
 * varies.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega88");

#define PERIODS	200

static volatile uint8_t periods, matches, next, seen, done;

ISR(TIMER1_OVF_vect)
{
	periods++;
}

ISR(TIMER1_COMPA_vect)
{
	if (OCR1A < 16) {
		matches++;
		OCR1A = 60000;
	} else {
		/* at 60000 in period n: the matches of periods 1 to n */
		if (periods == PERIODS) {
			seen = matches;
			done = 1;
		}
		next = (next + 1) & 3;		/* 0, 1, 2, 3 in turn */
		OCR1A = next;
	}
}

static void say(const char *s)
{
	for (; *s; s++) {
		while (!(UCSR0A & (1 << UDRE0)))
			;
		UDR0 = *s;
	}
}

static void say_n(uint8_t n)
{
	char b[4] = { '0' + n / 100, '0' + n / 10 % 10, '0' + n % 10, 0 };
	say(b);
}

int main(void)
{
	UCSR0B = 1 << TXEN0;
	OCR1A = 60000;
	TIMSK1 = (1 << OCIE1A) | (1 << TOIE1);
	TCCR1B = 1 << CS10;
	sei();
	while (!done)
		asm volatile(
			"rcall 1f\n\t"
			"rjmp 2f\n"
		"1:\tret\n"
		"2:\tlpm r0, Z\n\t"
			"lds r0, 0x100\n\t"
			"nop" ::: "r0");
	cli();
	say("small OCR1A: ");
	say_n(seen);
	say(" matches in ");
	say_n(PERIODS);
	say(" periods\n");
	sleep_cpu();
}
