/*
 * In the USI's two-wire mode, SDA and SCL are open drain: PINB reads the
 * line, low if the chip or something outside pulls it low. Here PB0 and
 * PB2 are outputs at 1, and the test pulls them low from outside, as an
 * I2C master or another slave does. After a start the USI holds SCL low,
 * and PINB reads that too. Writing PORTB again doesn't make the
 * line high; USIDR's MSB at 0 pulls SDA low, once SCL is low; with the
 * USI off, an output reads PORT again.
 */
#include "tests.h"
#include "avr_ioport.h"

#define GPIOR0	0x31
#define GPIOR1	0x32
#define GPIOR2	0x33
#define SDA	0x01
#define SCL	0x04

static avr_t *avr;

static uint8_t pins(void)
{
	avr_cycle_count_t end = avr->cycle + 100;
	while (avr->cycle < end)
		avr_run(avr);
	return avr->data[GPIOR0] & (SDA | SCL);
}

static void ask(uint8_t c)
{
	avr->data[GPIOR1] = c;
	avr_cycle_count_t end = avr->cycle + 1000;
	while (avr->data[GPIOR2] != c && avr->cycle < end)
		avr_run(avr);
	if (avr->data[GPIOR2] != c)
		fail("the firmware didn't do %d", c);
}

static void expect(uint8_t want, const char *what)
{
	uint8_t v = pins();
	if (v != want)
		fail("%s: PINB SDA %d SCL %d, not %d %d", what,
			!!(v & SDA), !!(v & SCL), !!(want & SDA), !!(want & SCL));
}

int main(int argc, char **argv)
{
	tests_init(argc, argv);
	avr = tests_init_avr("attiny85_usi_twowire_pin.axf");
	avr_irq_t *sda = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0);
	avr_irq_t *scl = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 2);

	expect(SDA | SCL, "released");
	avr_raise_irq(scl, 0);
	expect(SDA, "SCL pulled low outside");
	ask(1);
	expect(SDA, "SCL pulled low outside, PORTB written again");
	avr_raise_irq(scl, 1);
	expect(SDA | SCL, "SCL let go");
	avr_raise_irq(sda, 0);
	expect(SCL, "SDA pulled low outside");
	avr_raise_irq(sda, 1);
	expect(SDA | SCL, "SDA let go");

	/* SDA going low with SCL high was a start: the USI holds SCL low
	 * after it falls, until USISIF is cleared */
	avr_raise_irq(sda, 0);
	avr_raise_irq(scl, 0);
	avr_raise_irq(scl, 1);
	expect(0, "SCL held after a start");
	ask(5);
	avr_raise_irq(scl, 1);
	expect(SCL, "SCL let go after USISIF cleared");
	avr_raise_irq(scl, 0);
	avr_raise_irq(sda, 1);
	avr_raise_irq(scl, 1);
	expect(SDA | SCL, "both let go");
	ask(5);
	/* USIDR's MSB gets to SDA while SCL is low (the latch is open then) */
	ask(2);
	expect(SDA | SCL, "USIDR 00, SCL high");
	avr_raise_irq(scl, 0);
	expect(0, "USIDR 00, SCL low");
	avr_raise_irq(scl, 1);
	expect(SCL, "USIDR 00, SCL high again");
	ask(3);
	expect(SCL, "USIDR ff, SCL high");
	avr_raise_irq(scl, 0);
	expect(SDA, "USIDR ff, SCL low");
	avr_raise_irq(scl, 1);
	expect(SDA | SCL, "USIDR ff, SCL high again");

	/* the USI off: outputs push-pull, PINB reads PORTB */
	ask(4);
	avr_raise_irq(scl, 0);
	avr_raise_irq(sda, 0);
	expect(SDA | SCL, "USI off, both pulled low outside");

	avr_terminate(avr);
	tests_success();
	return 0;
}
