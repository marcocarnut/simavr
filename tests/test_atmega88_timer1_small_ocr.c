#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	tests_assert_uart_receive("atmega88_timer1_small_ocr.axf", 4000000,
				  "small OCR1A: 200 matches in 200 periods\n", '0');
	tests_success();
	return 0;
}
