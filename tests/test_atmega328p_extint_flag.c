#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	tests_assert_register_receive("atmega328p_extint_flag.axf", 100000,
				      "1100 00 fired, fired", (avr_io_addr_t)0x3e);
	tests_success();
	return 0;
}
