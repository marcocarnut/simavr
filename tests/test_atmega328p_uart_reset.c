/*
 * UCSR0B after a reset. By default simavr sets TXEN0 there, so that
 * firmware can print without enabling the UART. With
 * AVR_UART_FLAG_TXEN_RESET_OFF it is 0, as on the chip: set after the
 * reset that avr_init does, before the firmware writes UCSR0B, the flag
 * takes back the reset's TXEN0; later resets leave it off; and it doesn't
 * touch a TXEN0 that the firmware set.
 */
#include "tests.h"
#include "avr_uart.h"

#define GPIOR0	0x3e
#define GPIOR1	0x4a
#define GPIOR2	0x4b
#define UCSR0B	0xc1
#define TXEN0	0x08

static void run(avr_t *avr)
{
	avr_cycle_count_t end = avr->cycle + 10000;
	while (!avr->data[GPIOR2] && avr->cycle < end)
		avr_run(avr);
	if (!avr->data[GPIOR2])
		fail("the firmware didn't finish");
}

static void set_flags(avr_t *avr, uint32_t set)
{
	uint32_t f;
	avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &f);
	f |= set;
	avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &f);
}

int main(int argc, char **argv)
{
	tests_init(argc, argv);

	/* the default: TXEN0 on */
	avr_t *avr = tests_init_avr("atmega328p_uart_reset.axf");
	run(avr);
	if (avr->data[GPIOR0] != TXEN0)
		fail("by default, UCSR0B after reset %02x, not %02x",
			avr->data[GPIOR0], TXEN0);
	avr_terminate(avr);

	/* the flag, set after avr_init's reset */
	avr = tests_init_avr("atmega328p_uart_reset.axf");
	set_flags(avr, AVR_UART_FLAG_TXEN_RESET_OFF);
	run(avr);
	if (avr->data[GPIOR0] != 0)
		fail("TXEN_RESET_OFF: UCSR0B after reset %02x, not 00",
			avr->data[GPIOR0]);
	if (avr->data[GPIOR1] != TXEN0)
		fail("TXEN_RESET_OFF: UCSR0B %02x after the firmware set TXEN0",
			avr->data[GPIOR1]);

	/* setting it again doesn't touch the firmware's TXEN0 */
	set_flags(avr, AVR_UART_FLAG_TXEN_RESET_OFF);
	if (avr->data[UCSR0B] != TXEN0)
		fail("TXEN_RESET_OFF set again: the firmware's TXEN0 cleared");

	/* a later reset */
	avr_reset(avr);
	if (avr->data[GPIOR2])
		fail("GPIOR2 survived the reset");
	run(avr);
	if (avr->data[GPIOR0] != 0)
		fail("TXEN_RESET_OFF: UCSR0B after a second reset %02x, not 00",
			avr->data[GPIOR0]);
	avr_terminate(avr);

	tests_success();
	return 0;
}
