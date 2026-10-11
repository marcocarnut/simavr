/*
 * The watchdog's oscillator (AVR_IOCTL_WATCHDOG_SET_OSC). Exact by default:
 * every watchdog interrupt lands at the same timer 0 count. With jitter the
 * counts differ, the same seed giving the same ones and another seed others;
 * an offset moves the period.
 */
#include <stdio.h>
#include <string.h>
#include "tests.h"
#include "avr_watchdog.h"
#include "sim_interrupts.h"

#define WDT_VECTOR	12

static char out[32];
static int nout;
static avr_cycle_count_t first;

static void usidr(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	if (nout < (int)sizeof out - 1)
		out[nout++] = v;
	out[nout] = 0;
}

static void pending(struct avr_irq_t *irq, uint32_t value, void *param)
{
	avr_t *avr = param;
	if (value && !first)
		first = avr->cycle;
}

/* Run with the oscillator so (NULL: as it comes); the samples into s. */
static void run(const avr_watchdog_osc_t *osc, char *s)
{
	avr_t *avr = tests_init_avr("attiny85_watchdog_osc.axf");
	nout = 0;
	out[0] = 0;
	first = 0;
	avr_register_io_write(avr, 0x2f, usidr, NULL);
	avr_irq_register_notify(avr_get_interrupt_irq(avr, WDT_VECTOR) + AVR_INT_IRQ_PENDING,
				pending, avr);
	if (osc)
		avr_ioctl(avr, AVR_IOCTL_WATCHDOG_SET_OSC, (void *)osc);
	int state = cpu_Running;
	while (avr->cycle < 8000000 && state != cpu_Done && state != cpu_Crashed)
		state = avr_run(avr);
	strcpy(s, out);
	avr_terminate(avr);
}

/* The widest gap between two of the samples, round the 256 counts. */
static int spread(const char *s)
{
	int v[8], m = 0;
	for (int i = 0; i < 8; i++)
		sscanf(s + 2 * i, "%2x", &v[i]);
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++) {
			int d = (v[i] - v[j]) & 0xFF;
			if (d > 128)
				d = 256 - d;
			if (d > m)
				m = d;
		}
	return m;
}

int main(int argc, char **argv) {
	char exact[32], a[32], a2[32], b[32], off[32];
	tests_init(argc, argv);

	run(NULL, exact);
	avr_cycle_count_t nominal = first;
	static const avr_watchdog_osc_t sa = { 0, 2000, 1 }, sb = { 0, 2000, 2 },
		fast = { 50000, 0, 0 };
	run(&sa, a);
	run(&sa, a2);
	run(&sb, b);
	run(&fast, off);
	avr_cycle_count_t faster = first;

	if (strlen(exact) != 16 || strlen(a) != 16)
		fail("expected 8 samples, got \"%s\", \"%s\"", exact, a);
	/* exact, they differ by the instruction the interrupt came in: a count */
	if (spread(exact) > 2)
		fail("exact oscillator: the samples spread by %d (%s)", spread(exact), exact);
	if (spread(a) < 16)
		fail("with jitter: the samples spread by only %d (%s)", spread(a), a);
	if (strcmp(a, a2))
		fail("same seed, other samples: %s, %s", a, a2);
	if (!strcmp(a, b))
		fail("another seed, the same samples: %s", a);
	/* 16 ms at 8 MHz is 128000 cycles, 5% faster 121905: 6095 sooner,
	   counted from wherever the firmware turned the watchdog on */
	long sooner = (long)nominal - (long)faster;
	if (sooner < 6085 || sooner > 6105)
		fail("5%% fast: the first interrupt %ld cycles sooner, not 6095", sooner);
	printf("exact %s, seed 1 %s, seed 2 %s; 5%% fast: %ld cycles sooner\n",
	       exact, a, b, sooner);
	tests_success();
	return 0;
}
