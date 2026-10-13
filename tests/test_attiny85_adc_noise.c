/*
 * The ADC's input noise (AVR_IOCTL_ADC_SET_NOISE). Exact by default: a
 * differential channel with both inputs on one pin reads 0. With an offset
 * alone it reads the offset, times the gain, every time; with noise the
 * readings spread round it, the same seed giving the same ones and another
 * seed others.
 */
#include <stdio.h>
#include <string.h>
#include "tests.h"
#include "avr_adc.h"

#define N	32

static char out[2 * N + 1];
static int nout;

static void usidr(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	if (nout < (int)sizeof out - 1)
		out[nout++] = v;
	out[nout] = 0;
}

/* Run with the noise so (NULL: as it comes); the readings into s, v. */
static void run(const avr_adc_noise_t *noise, char *s, int *v)
{
	avr_t *avr = tests_init_avr("attiny85_adc_noise.axf");
	nout = 0;
	out[0] = 0;
	avr->vcc = 5000;
	avr_register_io_write(avr, 0x2f, usidr, NULL);
	if (noise && avr_ioctl(avr, AVR_IOCTL_ADC_SET_NOISE, (void *)noise))
		fail("no AVR_IOCTL_ADC_SET_NOISE");
	int state = cpu_Running;
	while (avr->cycle < 2000000 && state != cpu_Done && state != cpu_Crashed)
		state = avr_run(avr);
	strcpy(s, out);
	if (strlen(s) != 2 * N)
		fail("expected %d readings, got \"%s\"", N, s);
	for (int i = 0; i < N; i++) {
		unsigned x;
		sscanf(s + 2 * i, "%2x", &x);
		v[i] = (int8_t)x;
	}
	avr_terminate(avr);
}

int main(int argc, char **argv) {
	char exact[2 * N + 1], off[2 * N + 1], a[2 * N + 1], a2[2 * N + 1], b[2 * N + 1];
	int ve[N], vo[N], va[N], vx[N];
	tests_init(argc, argv);

	/* 2.5 mV at the input, 50 mV at 20x: 5.1 LSB of 5000 mV / 511 */
	static const avr_adc_noise_t offset = { 2500, 0, 0 },
		sa = { 2500, 400, 1 }, sb = { 2500, 400, 2 };
	run(NULL, exact, ve);
	run(&offset, off, vo);
	run(&sa, a, va);
	run(&sa, a2, vx);
	run(&sb, b, vx);

	for (int i = 0; i < N; i++) {
		if (ve[i])
			fail("exact: ADC0-ADC0 reads %d, not 0 (%s)", ve[i], exact);
		if (vo[i] != 5)
			fail("offset alone: reads %d, not 5 (%s)", vo[i], off);
	}
	/* 0.4 mV of noise, 8 mV at 20x: a sigma of 0.8 LSB */
	int sum = 0, lo = 127, hi = -128;
	for (int i = 0; i < N; i++) {
		sum += va[i];
		lo = va[i] < lo ? va[i] : lo;
		hi = va[i] > hi ? va[i] : hi;
	}
	if (sum < 4 * N || sum > 6 * N)
		fail("with noise: a mean of %.2f, not about 4.6 (%s)", (double)sum / N, a);
	if (hi - lo < 2 || hi - lo > 8)
		fail("with noise: readings from %d to %d (%s)", lo, hi, a);
	if (strcmp(a, a2))
		fail("same seed, other readings: %s, %s", a, a2);
	if (!strcmp(a, b))
		fail("another seed, the same readings: %s", a);
	printf("exact %s\noffset %s\nseed 1 %s\nseed 2 %s\n", exact, off, a, b);
	tests_success();
	return 0;
}
