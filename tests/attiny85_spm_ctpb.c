/*
	attiny85_spm_ctpb.c

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
 * CTPB clears the temporary page buffer, as Micronucleus does before each
 * page: a fill before it is forgotten, and the SPM that clears fills
 * nothing itself, whatever Z and r1:r0 hold.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

/* No UART in the tiny85: the test reads what is written to USIDR. */
static void say(const char *s)
{
	while (*s)
		USIDR = *s++;
}

static void clear_page_buffer(uint16_t z, uint16_t r01)
{
	asm volatile(
		"movw r0, %1\n\t"
		"out %0, %2\n\t"
		"spm\n\t"
		"clr r1\n\t"
		: : "I" (_SFR_IO_ADDR(SPMCSR)), "r" (r01),
		    "r" ((uint8_t)((1 << CTPB) | (1 << SPMEN))), "z" (z)
		: "r0");
}

int main(void)
{
	static const uint16_t page = 0x1000;

	boot_page_fill(page, 0x1111);			/* to be forgotten */
	clear_page_buffer(page + SPM_PAGESIZE - 2, 0x2222);
	for (int i = 0; i < SPM_PAGESIZE; i += 2)
		boot_page_fill(page + i, 0x3333);
	boot_page_erase(page);
	boot_spm_busy_wait();
	boot_page_write(page);
	boot_spm_busy_wait();

	int bad = -1;
	for (int i = 0; i < SPM_PAGESIZE && bad < 0; i += 2)
		if (pgm_read_word(page + i) != 0x3333)
			bad = i;
	if (bad < 0)
		say("the page is the second fill throughout");
	else if (pgm_read_word(page + bad) == 0x1111)
		say("the fill before CTPB stayed");
	else if (pgm_read_word(page + bad) == 0x2222)
		say("CTPB filled a word");
	else
		say("something else is in the page");
	cli();
	sleep_cpu();
}
