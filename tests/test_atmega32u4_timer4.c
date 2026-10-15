/*
 * Timer/Counter4 of the ATmega32U4, normal mode. The registers, from what
 * the firmware writes to GPIOR0; then, for each scenario, every OC4x pin
 * edge and every enabled Timer4 interrupt flag, at the cycle it comes,
 * counted from the write to TCCR4B that starts the timer.
 *
 * A match sets its flag and drives its pin one count after TCNT4 reaches
 * OCR4x; TOV4 comes as TCNT4 wraps to 0: with a prescaler of P, from a
 * start at 0, OCR4x = n comes at P * (n + 1) and the overflow at
 * P * (TOP + 1).
 */
#include <stdio.h>
#include <string.h>
#include "tests.h"
#include "avr_ioport.h"
#include "sim_interrupts.h"

#define TCCR4B	0xc1
#define TCNT4L	0xbe
#define OCR4C	0xd1
#define TIMSK4	0x72
#define GPIOR0	0x3e

static char text[256];
static int ntext;

static struct event {
	avr_cycle_count_t cycle;
	char kind;		/* S P X start, prescaler reset, stop; T C TCNT4 and OCR4C
					 * written; a b d pin; A B D O flag */
	int value;
} log_[400];
static int nlog;
static avr_t *avr;

static void add(char kind, int value)
{
	if (nlog < (int)(sizeof log_ / sizeof log_[0]))
		log_[nlog++] = (struct event){ avr->cycle, kind, value };
}

static void gpior0(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	if (ntext < (int)sizeof text - 1)
		text[ntext++] = v;
}

static void tccr4b(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	add(v & 0x40 ? 'P' : v & 0x0f ? 'S' : 'X', v);
}

static void tcnt4(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	add('T', v);
}

static void ocr4c(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	add('C', v);
}

static void pin(struct avr_irq_t *irq, uint32_t value, void *param)
{
	static int last[3];
	int i = (char *)param - "abd";
	value &= 0xff;	/* the timer's raise carries AVR_IOPORT_OUTPUT */
	if ((int)!!value != last[i]) {
		last[i] = !!value;
		add(*(char *)param, !!value);
	}
}

static void flag(struct avr_irq_t *irq, uint32_t value, void *param)
{
	static const uint8_t enable[] = { 6, 5, 7, 2 };	/* OCIE4A, B, D, TOIE4 */
	int i = (char *)param - "ABDO";
	if (value && (avr->data[TIMSK4] & (1 << enable[i])))
		add(*(char *)param, 1);
}

/* scenario n (from 1), as "+cycle kind value" from its start to its stop */
static void scenario(int n, char *out)
{
	int starts = 0, i;
	out[0] = 0;
	for (i = 0; i < nlog; i++)
		if (log_[i].kind == 'S' && ++starts == n + 1)
			break;
	if (i == nlog)
		fail("no scenario %d", n);
	for (int j = i; j < nlog && log_[j].kind != 'X'; j++)
		sprintf(out + strlen(out), "%s%+lld %c%d", out[0] ? ", " : "",
				(long long)(log_[j].cycle - log_[i].cycle), log_[j].kind,
				log_[j].value);
}

static void expect(int n, const char *what, const char *want)
{
	char got[4096];
	scenario(n, got);
	if (strcmp(got, want))
		fail("%s:\n  got  %s\n  want %s", what, got, want);
}

/* the log entry of kind k in scenario n, counted from its start */
static avr_cycle_count_t at(int n, char k)
{
	int starts = 0;
	for (int i = 0; i < nlog; i++) {
		if (log_[i].kind == 'S')
			starts++;
		if (starts == n + 1 && log_[i].kind == k)
			return log_[i].cycle;
	}
	fail("scenario %d: no %c", n, k);
}

int main(int argc, char **argv)
{
	char want[1024];
	tests_init(argc, argv);

	avr = tests_init_avr("atmega32u4_timer4.axf");
	avr_register_io_write(avr, GPIOR0, gpior0, NULL);
	avr_register_io_write(avr, TCCR4B, tccr4b, NULL);
	avr_register_io_write(avr, TCNT4L, tcnt4, NULL);
	avr_register_io_write(avr, OCR4C, ocr4c, NULL);
	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), 7), pin, "abd" + 0);
	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 6), pin, "abd" + 1);
	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 7), pin, "abd" + 2);
	for (int v = 38; v <= 41; v++) {
		avr_irq_t *irq = avr_get_interrupt_irq(avr, v);
		if (!irq)
			fail("no vector %d: Timer4 isn't there", v);
		avr_irq_register_notify(irq + AVR_INT_IRQ_PENDING, flag, "ABDO" + v - 38);
	}

	int state = cpu_Running;
	while (avr->cycle < 100000 && state != cpu_Done && state != cpu_Crashed)
		state = avr_run(avr);
	if (state == cpu_Crashed)
		fail("crashed");
	text[ntext] = 0;

	if (strcmp(text, "C ff 03 B 0321 T 0123 H 03 S 60 90 F 0000 V 00\n"))
		fail("registers: \"%s\"", text);

	/* clk/8: B toggles OC4B at 10; A at 60, D at 40, TOP 99 */
	expect(1, "1: flags and a toggle at clk/8",
		"+0 S4, +88 b1, +328 D1, +488 A1, +800 O1, "
		"+888 b0, +1128 D1, +1288 A1, +1600 O1, +1688 b1");

	/* clk/1, TOP 207, B at 100: each match's level was chosen at the one
	 * before; forces clear and set OC4B first */
	{
		static const int bits[] = { 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1 };
		int level = 1, x = 0;
		char forced[16] = "";
		/* the force, between the stop before and the start */
		for (int i = 0; i < nlog && log_[i].cycle < at(2, 'S'); i++) {
			if (log_[i].kind == 'X')
				x = i;
		}
		for (int i = x; i < nlog && log_[i].kind != 'S'; i++)
			if (log_[i].kind == 'b' && strlen(forced) < sizeof forced - 1)
				forced[strlen(forced)] = '0' + log_[i].value;
		if (strcmp(forced, "01"))
			fail("2: FOC4B didn't clear and set OC4B: \"%s\"", forced);
		avr_cycle_count_t s = at(2, 'S'), stop = at(2, 'X');
		sprintf(want, "+0 S1");
		for (int k = 0; 101 + 208 * k < (int)(stop - s); k++) {
			int t = 101 + 208 * k, b = k < 11 ? bits[k] : 1;
			sprintf(want + strlen(want), ", +%d B1", t);
			if (b != level)
				sprintf(want + strlen(want), ", +%d b%d", t, level = b);
		}
		expect(2, "2: COM4B chosen match by match", want);
	}

	/* TCNT4 = OCR4D = 20 blocks the match at the next count: the first
	 * toggle is a period later, at 800 - 160 + 168 */
	{
		avr_cycle_count_t s = at(3, 'S'), w = at(3, 'T');
		sprintf(want, "+0 S4, +808 d1, +%lld T20, +%lld d0",
			(long long)(w - s), (long long)(w - s + 808));
		expect(3, "3: a TCNT4 write blocks a match", want);
	}

	/* 10 bits: TOP 0x200, A at 0x150 */
	expect(4, "4: 10-bit TOP and OCR4A",
		"+0 S1, +337 A1, +337 a1, +513 O1, +850 A1, +850 a0, +1026 O1");

	/* TOP lowered to 50 at TCNT4 > 50: on to MAX, then 51 counts */
	{
		avr_cycle_count_t s = at(5, 'S'), w = at(5, 'C');
		if (w - s <= 50)
			fail("5: OCR4C written at TCNT4 %lld, not past 50", (long long)(w - s));
		sprintf(want, "+0 S1, +%lld C50", (long long)(w - s));
		for (int t = 1024; t < (int)(at(5, 'X') - s); t += 51)
			sprintf(want + strlen(want), ", +%d O1", t);
		expect(5, "5: TOP lowered under TCNT4", want);
	}

	/* PSR4 at TCNT4 = t: count t starts again there, so the match at 10 is
	 * 8 * (11 - t) later */
	{
		avr_cycle_count_t s = at(6, 'S'), w = at(6, 'P');
		long long t = (w - s) / 8;
		if ((w - s) % 8 == 0)
			fail("6: the prescaler reset falls on a count, it shows nothing");
		sprintf(want, "+0 S4, +%lld P68, +%lld D1", (long long)(w - s),
			(long long)(w - s) + 8 * (11 - t));
		expect(6, "6: PSR4", want);
	}

	tests_success();
	return 0;
}
