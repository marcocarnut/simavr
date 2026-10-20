/*
 * While the CPU sleeps, avr_run returns as soon as a cycle timer has run,
 * so that whatever drives the pins between avr_run calls does it when it
 * means to. Here the test steps one cycle at a time, with a timer of its
 * own, while the firmware sleeps until Timer1's next overflow, half a
 * second away; then it changes PB3, and the pin change interrupt comes at once.
 * Before, each avr_run asleep ran the timers due, and then slept on to
 * the next one, the overflow: the test's step took that long, and its pin
 * change came that late.
 */
#include <stddef.h>
#include "tests.h"
#include "avr_ioport.h"

#define GPIOR0	0x31

static avr_cycle_count_t isr_at;

static void gpior0(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	if (v && !isr_at)
		isr_at = avr->cycle;
}

static avr_cycle_count_t done(struct avr_t *avr, avr_cycle_count_t when, void *param)
{
	*(int *)param = 0;
	return 0;
}

/* run for one cycle, as an I2C master bit-banging the pins does */
static avr_cycle_count_t step(avr_t *avr)
{
	avr_cycle_count_t c = avr->cycle;
	int looping = 1;
	avr_cycle_timer_register(avr, 1, done, &looping);
	while (looping)
		avr_run(avr);
	return avr->cycle - c;
}

int main(int argc, char **argv)
{
	tests_init(argc, argv);
	avr_t *avr = tests_init_avr("attiny85_sleep_run.axf");
	avr_register_io_write(avr, GPIOR0, gpior0, NULL);

	while (avr->state != cpu_Sleeping && avr->cycle < 10000)
		avr_run(avr);
	if (avr->state != cpu_Sleeping)
		fail("the firmware didn't sleep");

	avr_cycle_count_t most = 0;
	for (int i = 0; i < 1000; i++) {
		avr_cycle_count_t d = step(avr);
		if (d > most)
			most = d;
	}
	if (most > 2)
		fail("asleep, a one cycle step took %lu cycles",
			(unsigned long)most);

	avr_irq_t *pb3 = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 3);
	avr_cycle_count_t raised = avr->cycle;
	avr_raise_irq(pb3, 1);
	while (!isr_at && avr->cycle < raised + 100000)
		step(avr);
	if (!isr_at || isr_at - raised > 40)
		fail("the pin change interrupt came %ld cycles after the change",
			isr_at ? (long)(isr_at - raised) : -1L);

	avr_terminate(avr);
	tests_success();
	return 0;
}
