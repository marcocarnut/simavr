/*
 * A control write through the USB controller: SETUP, then the data stage's
 * OUT at once, as a host sends it, then the status stage's IN. Endpoint
 * 0's bank holds the SETUP until the firmware clears RXSTPI, so the OUT is
 * NAKed till then; taken at once, it overwrote the SETUP, and the firmware
 * read its data as the request. Then a control read gives the data back.
 */
#include <string.h>
#include "tests.h"
#include "avr_usb.h"

#define GPIOR0	0x3e
#define GPIOR1	0x4a
#define UDCON	0xe0

static avr_t *avr;

static void run(int cycles)
{
	avr_cycle_count_t end = avr->cycle + cycles;
	while (avr->cycle < end)
		avr_run(avr);
}

static int usb(uint32_t ctl, int pipe, uint8_t *buf, uint32_t *sz)
{
	struct avr_io_usb io = { .pipe = pipe, .sz = *sz, .buf = buf };
	int r = avr_ioctl(avr, ctl, &io);
	*sz = io.sz;
	return r;
}

/* the packet, retried while NAKed; how many NAKs */
static int retry(uint32_t ctl, int pipe, uint8_t *buf, uint32_t *sz, const char *what)
{
	uint32_t n = *sz;
	for (int naks = 0; naks < 1000; naks++) {
		*sz = n;
		int r = usb(ctl, pipe, buf, sz);
		if (r == AVR_IOCTL_USB_OK)
			return naks;
		if (r != AVR_IOCTL_USB_NAK)
			fail("%s: %d", what, r);
		run(50);
	}
	fail("%s: NAKed for ever", what);
	return -1;
}

int main(int argc, char **argv)
{
	tests_init(argc, argv);
	avr = tests_init_avr("atmega32u4_usb.axf");
	run(10000);
	if (avr->data[UDCON] & 1)
		fail("the firmware didn't attach");
	uint32_t sz = 0;
	usb(AVR_IOCTL_USB_RESET, 0, NULL, &sz);
	run(2000);

	/* the write: SETUP and its data back to back */
	uint8_t setup[8] = { 0x40, 1, 0, 0, 0, 0, 4, 0 };
	uint8_t data[4] = { 0x11, 0x22, 0x33, 0x44 };
	sz = 8;
	if (usb(AVR_IOCTL_USB_SETUP, 0, setup, &sz))
		fail("SETUP not taken");
	sz = 4;
	int naks = retry(AVR_IOCTL_USB_WRITE, 0, data, &sz, "the data OUT");
	uint8_t b[64];
	sz = 64;
	retry(AVR_IOCTL_USB_READ, 0, b, &sz, "the status IN");
	if (sz)
		fail("the status IN: %u bytes", sz);
	if (avr->data[GPIOR1])
		fail("the firmware STALLed the write (the data OUT %s)",
			naks ? "NAKed first" : "taken at once");
	if (avr->data[GPIOR0] != 1)
		fail("the firmware took %d requests, not 1", avr->data[GPIOR0]);
	if (!naks)
		fail("the data OUT taken while the SETUP was in the bank");

	/* the read, and its status OUT */
	uint8_t get[8] = { 0xC0, 2, 0, 0, 0, 0, 4, 0 };
	sz = 8;
	if (usb(AVR_IOCTL_USB_SETUP, 0, get, &sz))
		fail("SETUP not taken");
	sz = 64;
	retry(AVR_IOCTL_USB_READ, 0, b, &sz, "the data IN");
	if (sz != 4 || memcmp(b, data, 4))
		fail("the data IN: %u bytes, %02x %02x %02x %02x", sz, b[0], b[1], b[2], b[3]);
	sz = 0;
	retry(AVR_IOCTL_USB_WRITE, 0, NULL, &sz, "the status OUT");
	run(2000);
	if (avr->data[GPIOR0] != 2)
		fail("the firmware took %d requests, not 2", avr->data[GPIOR0]);

	avr_terminate(avr);
	tests_success();
	return 0;
}
