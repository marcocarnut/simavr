/*
	atmega32u4_timer4.c

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
 * Timer/Counter4 in its normal mode. First its registers, written out to
 * GPIOR0: OCR4C's reset value and minimum, 10-bit access through TC4H,
 * the COM4A/COM4B copies in TCCR4C, the strobes reading 0, TOV4 cleared by
 * writing 1. Then scenarios the test watches cycle by cycle: each starts
 * the timer and runs a sled of nops, so that every match or overflow falls
 * on a one-cycle instruction and is seen in the cycle it is due.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega32u4");

#define SLED(n)	asm volatile (".rept " #n "\n\tnop\n\t.endr")

EMPTY_INTERRUPT(TIMER4_COMPA_vect)
EMPTY_INTERRUPT(TIMER4_COMPD_vect)
EMPTY_INTERRUPT(TIMER4_OVF_vect)

/* compare B sets or clears OC4B at its next match from a list of bits, as
 * a soft UART's transmitter does */
static const uint8_t bits[] = { 0, 1, 1, 0, 1, 0, 0, 0, 1, 1 };
static volatile uint8_t bit;

ISR(TIMER4_COMPB_vect)
{
	uint8_t b = bit < sizeof bits - 1 ? bits[++bit] : 1;
	TCCR4A = b ? (3 << COM4B0) : (2 << COM4B0);
}

static void hex(uint8_t v)
{
	static const char h[] = "0123456789abcdef";
	GPIOR0 = h[v >> 4];
	GPIOR0 = h[v & 15];
}

static void say(const char *s)
{
	while (*s)
		GPIOR0 = *s++;
}

static void stop(void)
{
	TCCR4B = 0;
	TCCR4A = 0;
	TCCR4C = 0;
	TIMSK4 = 0;
	TIFR4 = 0xff;
	TC4H = 0;
	TCNT4 = 0;
}

int main(void)
{
	uint8_t v, h;

	/* the registers */
	say("C ");
	hex(OCR4C);
	OCR4C = 1;
	say(" ");
	hex(OCR4C);

	TC4H = 3;
	OCR4B = 0x21;
	TC4H = 0;
	v = OCR4B;
	h = TC4H;
	say(" B ");
	hex(h);
	hex(v);

	TC4H = 1;
	TCNT4L = 0x23;
	TC4H = 0;
	v = TCNT4L;
	h = TC4H;
	say(" T ");
	hex(h);
	hex(v);

	TC4H = 0xff;
	say(" H ");
	hex(TC4H);

	TCCR4A = (1 << COM4A0) | (2 << COM4B0);
	say(" S ");
	hex(TCCR4C);
	TCCR4C = (2 << COM4A0S) | (1 << COM4B0S) | (2 << COM4D0);
	say(" ");
	hex(TCCR4A);

	TCCR4A = (1 << FOC4A) | (1 << FOC4B);
	TCCR4C = (1 << FOC4D);
	say(" F ");
	hex(TCCR4A);
	hex(TCCR4C);

	TC4H = 0;
	OCR4C = 99;
	TCNT4 = 0;
	TCCR4B = 1;
	while (!(TIFR4 & (1 << TOV4)))
		;
	TIFR4 = 1 << TOV4;
	say(" V ");
	hex(TIFR4 & (1 << TOV4));
	say("\n");
	stop();

	DDRB |= 1 << 6;		/* OC4B */
	DDRC |= 1 << 7;		/* OC4A */
	DDRD |= 1 << 7;		/* OC4D */
	sei();

	/* 1: clk/8, TOP 99; B toggles OC4B at 10; flags at A 60, D 40 */
	OCR4C = 99;
	OCR4A = 60;
	OCR4B = 10;
	OCR4D = 40;
	TCCR4A = 1 << COM4B0;
	TIMSK4 = (1 << OCIE4A) | (1 << OCIE4D) | (1 << TOIE4);
	TCCR4B = 4;
	SLED(1800);
	stop();

	/* 2: clk/1, TOP 207, B at 100 sets or clears OC4B from the bits;
	 * forces first clear it and set it */
	OCR4C = 207;
	OCR4B = 100;
	TCCR4A = (2 << COM4B0) | (1 << FOC4B);
	TCCR4A = (3 << COM4B0) | (1 << FOC4B);
	TCCR4A = bits[0] ? (3 << COM4B0) : (2 << COM4B0);
	TIMSK4 = 1 << OCIE4B;
	TCCR4B = 1;
	SLED(2200);
	stop();

	/* 3: TCNT4 written to OCR4D blocks the next count's match: stopped,
	 * then running */
	OCR4C = 99;
	OCR4D = 20;
	TCCR4C = 1 << COM4D0;
	TCNT4 = 20;
	TCCR4B = 4;
	SLED(900);
	TCNT4 = 20;
	SLED(900);
	stop();

	/* 4: 10 bits, clk/1: TOP 0x200, A toggles OC4A at 0x150 */
	TC4H = 2;
	OCR4C = 0x00;
	TC4H = 1;
	OCR4A = 0x50;
	TC4H = 0;
	TCCR4A = 1 << COM4A0;
	TIMSK4 = (1 << OCIE4A) | (1 << TOIE4);
	TCCR4B = 1;
	SLED(1100);
	stop();

	/* 5: TOP lowered below TCNT4: the counter goes on to MAX, then wraps
	 * at the new TOP */
	OCR4C = 0xff;
	TIMSK4 = 1 << TOIE4;
	TCCR4B = 1;
	SLED(200);
	OCR4C = 50;
	SLED(1100);
	stop();

	/* 6: PSR4 starts the count again where it is: clk/8, D at 10 */
	OCR4C = 0xff;
	OCR4D = 10;
	TIMSK4 = 1 << OCIE4D;
	TCCR4B = 4;
	SLED(13);
	TCCR4B = 4 | (1 << PSR4);
	SLED(200);
	stop();

	cli();
	for (;;)
		;
}
