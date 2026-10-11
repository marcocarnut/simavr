/*
 * The resets of attiny85_watchdog_rereset, timed: the first after the
 * firmware's 100 ms and its 15 ms watchdog; the next two 16 ms apart, the
 * watchdog left as the reset left it. Timed here, across the resets, since
 * simavr starts avr->cycle again from 0 at each.
 */
#include <stdio.h>
#include "tests.h"
#include "sim_io.h"

static int reset_seen;

static void on_reset(avr_io_t *io)
{
	reset_seen = 1;
}

int main(int argc, char **argv) {
	tests_init(argc, argv);
	avr_t *avr = tests_init_avr("attiny85_watchdog_rereset.axf");
	static avr_io_t io = { .kind = "test", .reset = on_reset };
	avr_register_io(avr, &io);

	double at[4];
	int n = 0, state = cpu_Running;
	avr_cycle_count_t base = 0;
	while (n < 4 && base + avr->cycle < 8000000ULL * 5 &&
			state != cpu_Done && state != cpu_Crashed) {
		avr_cycle_count_t before = avr->cycle;
		state = avr_run(avr);
		if (reset_seen) {
			reset_seen = 0;
			base += before;
			at[n++] = base * 1000.0 / avr->frequency;
		}
	}
	if (n != 3)
		fail("%d watchdog resets, not 3", n);
	if (at[0] < 110 || at[0] > 125)
		fail("the first reset at %.1f ms, not after 100 + 16", at[0]);
	for (int i = 1; i < n; i++)
		if (at[i] - at[i - 1] < 15 || at[i] - at[i - 1] > 18)
			fail("reset %d came %.1f ms after the one before, not 16", i + 1,
			     at[i] - at[i - 1]);
	tests_success();
	return 0;
}
