#include <assert.h>
#include <stdio.h>
#include "../motor/foc_adc_noise.h"
int main(void) {
    assert(foc_adc_median3(4095, 2048, 2049) == 2049);
    assert(foc_adc_median3(2048, 0, 2049) == 2048);
    assert(foc_adc_median3(2048, 2049, 4095) == 2049);
    // Persistent real current is retained; both polarities survive filtering.
    assert(foc_adc_median3(2000, 2000, 2048) == 2000);
    assert(foc_adc_median3(2096, 2048, 2096) == 2096);
    assert(foc_adc_median3(0, 0, 4095) == 0);
    assert(foc_adc_median3(4095, 0, 4095) == 4095);
    puts("PASS: FOC scan median rejects one spike and preserves sustained signed current");
    return 0;
}
