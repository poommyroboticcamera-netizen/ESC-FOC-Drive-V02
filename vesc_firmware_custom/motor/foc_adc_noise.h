#ifndef FOC_ADC_NOISE_H
#define FOC_ADC_NOISE_H
#include <stdint.h>
// Reject at most one disturbed conversion in a three-conversion PWM scan.
// Return ADC counts; offset subtraction and signed scaling happen afterwards.
static inline uint16_t foc_adc_median3(uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    return a > b ? a : b;
}
#endif
