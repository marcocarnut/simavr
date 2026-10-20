/*
 * AVR_IOCTL_FLASH_SPM_HALT: a page erase or write halts the CPU, and the
 * timers go on. Off, the erase takes no time. At 4500 us (the datasheet's
 * tWD_FLASH) and at 1000 us, the CPU comes back that long after the SPM;
 * the timers overflowed meanwhile (at 1000 us, Timer0 only), and their
 * interrupts are taken then,
 * in vector order (Timer1's first, though Timer0 overflowed first), one
 * instruction of main between them as after any RETI; the page is
 * written.
 */
#include <stddef.h>
#include "tests.h"
#include "avr_flash.h"
#include "sim_time.h"

#define GPIOR0	0x31
#define PAGE	0x1000

static struct { char c; avr_cycle_count_t when; } ev[64];
static int n;

static void gpior0(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	if (n < 64) {
		ev[n].c = v;
		ev[n++].when = avr->cycle;
	}
}

static int find(char c, int from)
{
	for (int i = from; i < n; i++)
		if (ev[i].c == c)
			return i;
	fail("no '%c' after event %d", c, from);
	return -1;
}

static void run(uint32_t usec, int set)
{
	avr_t *avr = tests_init_avr("attiny85_spm_halt.axf");
	if (set && avr_ioctl(avr, AVR_IOCTL_FLASH_SPM_HALT, &usec))
		fail("AVR_IOCTL_FLASH_SPM_HALT not taken");
	n = 0;
	avr_register_io_write(avr, GPIOR0, gpior0, NULL);
	avr_cycle_count_t end = avr_usec_to_cycles(avr, 20000);
	while (avr->cycle < end && n < 64) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
	}

	avr_cycle_count_t halt = avr_usec_to_cycles(avr, set ? usec : 0);
	int s = find('s', 0), e = find('e', s), w = find('w', e);
	long took = ev[e].when - ev[s].when;
	if (!halt) {
		if (e != s + 1 || took > 20)
			fail("no halt: the erase took %ld cycles, %d events", took, e - s - 1);
	} else {
		/* Timer1's interrupt, then (one instruction of main after a
		 * RETI) 'e', then Timer0's */
		if (halt > 16384) {
			if (e != s + 2 || ev[s + 1].c != '1' || ev[s + 3].c != '0')
				fail("%u us: after the erase %c %c %c, not 1 e 0", usec,
					ev[s + 1].c, ev[s + 2].c, ev[s + 3].c);
		} else {
			/* Timer1 hasn't overflowed yet */
			if (e != s + 2 || ev[s + 1].c != '0')
				fail("%u us: after the erase %c %c, not 0 e", usec,
					ev[s + 1].c, ev[s + 2].c);
		}
		long back = ev[s + 1].when - ev[s].when;
		if (back < (long)halt || back > (long)halt + 30)
			fail("%u us: the CPU came back after %ld cycles, not %lu",
				usec, back, (unsigned long)halt);
		/* the write, with interrupts off: only the timers', from before
		 * it and from during it, at the sei */
		long wrote = ev[w].when - ev[e].when;
		for (int i = e + 1; i < w; i++)
			if (ev[i].c != '0' && ev[i].c != '1')
				fail("%u us: '%c' around the write", usec, ev[i].c);
		if (w - e - 1 > 3 || wrote < (long)halt || wrote > (long)halt + 100)
			fail("%u us: the write took %ld cycles, %d events", usec, wrote, w - e - 1);
	}
	for (int i = 0; i < 64; i += 2)
		if (avr->flash[PAGE + i] != 0x34 || avr->flash[PAGE + i + 1] != 0x12)
			fail("the page at %04x: %02x%02x", PAGE + i,
				avr->flash[PAGE + i + 1], avr->flash[PAGE + i]);
	avr_terminate(avr);
}

int main(int argc, char **argv)
{
	tests_init(argc, argv);
	run(0, 0);
	run(AVR_FLASH_SPM_HALT_USEC, 1);
	run(1000, 1);
	tests_success();
	return 0;
}
