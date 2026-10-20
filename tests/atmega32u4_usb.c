/*
	atmega32u4_usb.c

	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * A USB device, polled, at the registers: endpoint 0 (64 bytes) takes
 * vendor request 1, OUT, keeping its data, and gives it back on vendor
 * request 2, IN; anything else is STALLed. Endpoint 1 is a bulk IN with
 * nothing to send. GPIOR0 counts the requests taken, GPIOR1 the STALLs.
 */
#include <avr/io.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega32u4");

static uint8_t kept[16], nkept;

static void endpoints(void)
{
	UENUM = 0;
	UECONX = _BV(EPEN);
	UECFG0X = 0;				/* control */
	UECFG1X = _BV(EPSIZE1) | _BV(EPSIZE0) | _BV(ALLOC);	/* 64 */
	UENUM = 1;
	UECONX = _BV(EPEN);
	UECFG0X = _BV(EPTYPE1) | _BV(EPDIR);	/* bulk IN */
	UECFG1X = _BV(EPSIZE1) | _BV(EPSIZE0) | _BV(ALLOC);
	UENUM = 0;
}

static void request(void)
{
	uint8_t s[8];
	for (uint8_t i = 0; i < 8; i++)
		s[i] = UEDATX;
	UEINTX &= ~_BV(RXSTPI);
	uint8_t len = s[6] < sizeof kept ? s[6] : sizeof kept;

	if (s[0] == 0x40 && s[1] == 1) {
		while (!(UEINTX & _BV(RXOUTI)))
			;
		for (nkept = 0; nkept < len; nkept++)
			kept[nkept] = UEDATX;
		UEINTX &= ~_BV(RXOUTI);
		UEINTX &= ~_BV(TXINI);		/* the status stage */
		GPIOR0++;
	} else if (s[0] == 0xC0 && s[1] == 2) {
		while (!(UEINTX & _BV(TXINI)))
			;
		for (uint8_t i = 0; i < nkept && i < len; i++)
			UEDATX = kept[i];
		UEINTX &= ~_BV(TXINI);
		while (!(UEINTX & _BV(RXOUTI)))	/* the status stage */
			;
		UEINTX &= ~_BV(RXOUTI);
		GPIOR0++;
	} else {
		UECONX |= _BV(STALLRQ);
		GPIOR1++;
	}
}

int main(void)
{
	PLLCSR = _BV(PINDIV) | _BV(PLLE);
	while (!(PLLCSR & _BV(PLOCK)))
		;
	USBCON = _BV(USBE) | _BV(OTGPADE);
	UDCON = 0;				/* attach */
	for (;;) {
		if (UDINT & _BV(EORSTI)) {
			UDINT &= ~_BV(EORSTI);
			endpoints();
		}
		UENUM = 0;
		if (UEINTX & _BV(RXSTPI))
			request();
	}
}
