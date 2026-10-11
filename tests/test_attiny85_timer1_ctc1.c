#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	tests_assert_register_receive("attiny85_timer1_ctc1.axf", 100000,
				      "within OCR1C, wraps, no TOV1; PWM: within OCR1C, wraps, TOV1 set", (avr_io_addr_t)0x2f);
	tests_success();
	return 0;
}
