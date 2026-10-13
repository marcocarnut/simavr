/*
 * A unipolar differential conversion out of range reads the nearest end of
 * 0..0x3ff: 0 below, 0x3ff above.
 */
#include "tests.h"
#include "avr_adc.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	avr_t *avr = tests_init_avr("attiny85_adc_unipolar.axf");
	avr->vcc = 5000;
	avr_irq_t *adc = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, 0);
	avr_raise_irq(adc + ADC_IRQ_ADC2, 1500);
	avr_raise_irq(adc + ADC_IRQ_ADC3, 500);
	/* 1000 mV of 5000: 204; 20000 mV: 1023; -1000 mV: 0 */
	tests_assert_register_receive_avr(avr, 100000, "204 1023 0",
					  (avr_io_addr_t)0x2f /* &USIDR */);
	tests_success();
	return 0;
}
