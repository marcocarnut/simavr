#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	tests_assert_register_receive("attiny85_spm_ctpb.axf", 100000,
				      "the page is the second fill throughout", (avr_io_addr_t)0x2f);
	tests_success();
	return 0;
}
