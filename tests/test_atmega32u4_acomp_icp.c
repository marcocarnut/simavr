/*
 * Timer 1's input capture from the analog comparator (ACIC), ADC7 on its -
 * input against the bandgap. ADC7 going up past 1.1 V takes ACO down: with
 * ICES1 = 0 that is the capture's edge, with ICES1 = 1 it's ADC7 coming
 * back down. The capture comes the cycle after the step, as the comparator
 * is looked at again. While ACIC is on, the ICP pin (PD4) is not the
 * capture's input, and doesn't capture; with ACIC off it does again, and
 * the comparator doesn't.
 */
#include <stddef.h>
#include "tests.h"
#include "avr_acomp.h"
#include "avr_ioport.h"

#define TCCR1B	0x81
#define ICR1L	0x86
#define ICR1H	0x87
#define GPIOR1	0x4a
#define GPIOR2	0x4b
#define ACSR	0x50
#define ICES1	0x40

static avr_cycle_count_t start;

static void tccr1b(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	if (v & 7)
		start = avr->cycle;
}

static void run_to(avr_t *avr, avr_cycle_count_t cycle)
{
	while (avr->cycle < cycle)
		avr_run(avr);
}

static avr_t *init(uint8_t tccr1b_bits)
{
	avr_t *avr = tests_init_avr("atmega32u4_acomp_icp.axf");
	avr->data[GPIOR1] = tccr1b_bits;
	avr_register_io_write(avr, TCCR1B, tccr1b, NULL);
	run_to(avr, 3000);
	if (avr->data[GPIOR2])
		fail("a capture as the comparator is set up");
	return avr;
}

/* raise irq to v; how many captures follow, and when the last was, from
 * the step */
static int step(avr_t *avr, avr_irq_t *irq, uint32_t v, long *when)
{
	uint8_t n = avr->data[GPIOR2];
	avr_cycle_count_t c = avr->cycle;
	avr_raise_irq(irq, v);
	run_to(avr, c + 300);
	*when = (long)(avr->data[ICR1L] | avr->data[ICR1H] << 8) - (long)(c - start);
	return (uint8_t)(avr->data[GPIOR2] - n);
}

int main(int argc, char **argv)
{
	tests_init(argc, argv);
	long when;

	for (int ices = 0; ices <= 1; ices++) {
		avr_t *avr = init(ices ? ICES1 : 0);
		avr_irq_t *adc7 = avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC7);
		for (int i = 0; i < 2; i++) {
			int n = step(avr, adc7, 2000, &when);
			if (n != !ices || (!ices && when != 1))
				fail("ICES1 %d, ADC7 up: %d captures, at %+ld", ices, n, when);
			if (avr->data[ACSR] & (1 << 5))
				fail("ADC7 2 V over the bandgap: ACO 1");
			n = step(avr, adc7, 500, &when);
			if (n != ices || (ices && when != 1))
				fail("ICES1 %d, ADC7 down: %d captures, at %+ld", ices, n, when);
		}
		avr_terminate(avr);
	}

	avr_t *avr = init(0);
	avr_irq_t *adc7 = avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC7);
	avr_irq_t *pd4 = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 4);
	for (int i = 0; i < 3; i++) {
		if (step(avr, pd4, 0, &when) || step(avr, pd4, 1, &when))
			fail("ACIC on: PD4 captured");
		if (step(avr, adc7, i & 1 ? 500 : 2000, &when) != !(i & 1))
			fail("ACIC on: a PD4 pulse lost the comparator's edge");
	}
	/* ADC7 is up: ACO is 0 and PD4 1 as ACIC goes off */
	avr->data[GPIOR1] |= 1;
	run_to(avr, avr->cycle + 2100);
	if (step(avr, adc7, 500, &when) || step(avr, adc7, 2000, &when))
		fail("ACIC off: the comparator captured");
	if (step(avr, pd4, 0, &when) != 1 || when != 0)
		fail("ACIC off: PD4 down didn't capture at once (%+ld)", when);
	avr_terminate(avr);

	tests_success();
	return 0;
}
