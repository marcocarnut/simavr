/*
 * A bulk IN endpoint with nothing to send NAKs the IN. simavr answered a
 * zero-length packet instead, which a host takes as the end of a
 * transfer: a CDC host saw an empty read every time it asked.
 */
#include "tests.h"
#include "avr_usb.h"

#define UDCON	0xe0

static avr_t *avr;

static void run(int cycles)
{
	avr_cycle_count_t end = avr->cycle + cycles;
	while (avr->cycle < end)
		avr_run(avr);
}

int main(int argc, char **argv)
{
	tests_init(argc, argv);
	avr = tests_init_avr("atmega32u4_usb.axf");
	run(10000);
	if (avr->data[UDCON] & 1)
		fail("the firmware didn't attach");
	struct avr_io_usb io = { 0 };
	avr_ioctl(avr, AVR_IOCTL_USB_RESET, &io);
	run(2000);

	uint8_t b[64];
	for (int i = 0; i < 3; i++) {
		io = (struct avr_io_usb){ .pipe = 0x81, .sz = 64, .buf = b };
		int r = avr_ioctl(avr, AVR_IOCTL_USB_READ, &io);
		if (r != AVR_IOCTL_USB_NAK)
			fail("bulk IN, nothing to send: %d, %u bytes, not a NAK", r, io.sz);
		run(1000);
	}
	avr_terminate(avr);
	tests_success();
	return 0;
}
