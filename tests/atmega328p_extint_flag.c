/*
	atmega328p_extint_flag.c

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
 * INTF0, with INT0's pin (PD2) driven by the firmware itself: a falling
 * edge sets it; writing 0 leaves it, writing 1 clears it, and so does SBI;
 * in low level mode it stays 0; and a 1 written while the pin is high
 * doesn't keep INT0 from firing when the pin goes low; nor does enabling
 * INT0 with the pin already low.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega328p");

static volatile uint8_t fired;

ISR(INT0_vect)
{
	fired++;
	PORTD |= (1 << PD2);		/* the level goes away */
}

static void say(const char *s)
{
	while (*s)
		GPIOR0 = *s++;
}

static void flag(void)
{
	say(EIFR & (1 << INTF0) ? "1" : "0");
}

int main(void)
{
	PORTD = (1 << PD2);
	DDRD = (1 << PD2);

	/* SBI on a bit that is no flag's, in the register that has them */
	__asm__ volatile ("sbi %0, 2" :: "I" (_SFR_IO_ADDR(EIFR)));

	EICRA = (1 << ISC01);		/* falling edge, INT0 off */
	PORTD = 0;
	flag();				/* set */
	EIFR = 0;
	flag();				/* still set */
	EIFR = (1 << INTF0);
	flag();				/* cleared */
	PORTD = (1 << PD2);
	PORTD = 0;
	__asm__ volatile ("sbi %0, %1" :: "I" (_SFR_IO_ADDR(EIFR)), "I" (INTF0));
	flag();				/* cleared by SBI */
	say(" ");

	EICRA = 0;			/* low level */
	PORTD = (1 << PD2);
	PORTD = 0;
	flag();				/* 0 */
	PORTD = (1 << PD2);
	EIFR = (1 << INTF0);
	flag();				/* 0 */
	say(" ");

	EIMSK = (1 << INT0);
	sei();
	PORTD = 0;
	__asm__ volatile ("nop");
	__asm__ volatile ("nop");
	cli();
	say(fired == 1 ? "fired" : "didn't fire");

	EIMSK = 0;
	PORTD = 0;			/* low, INT0 off */
	EIMSK = (1 << INT0);
	sei();
	__asm__ volatile ("nop");
	__asm__ volatile ("nop");
	cli();
	say(fired == 2 ? ", fired" : ", didn't fire");
	sleep_cpu();
}
