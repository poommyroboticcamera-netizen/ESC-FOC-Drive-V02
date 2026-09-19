#include <assert.h>
#include <stdio.h>
#include "../motor/bldc_overcurrent.h"

int main(void) {
    bldc_oc_state_t s = {0, 0};
    assert(bldc_overcurrent_update(&s, 2.66f, 1.5f, 4, 2.2f, 2) == BLDC_OC_NONE);
    assert(bldc_overcurrent_update(&s, 0.10f, 1.5f, 4, 2.2f, 2) == BLDC_OC_NONE);
    assert(bldc_overcurrent_update(&s, 2.66f, 1.5f, 4, 2.2f, 2) == BLDC_OC_NONE);
    assert(bldc_overcurrent_update(&s, 2.66f, 1.5f, 4, 2.2f, 2) == BLDC_OC_EMERGENCY);
    assert(bldc_overcurrent_update(&s, 1.80f, 1.5f, 4, 2.2f, 2) == BLDC_OC_NONE);
    assert(bldc_overcurrent_update(&s, 1.80f, 1.5f, 4, 2.2f, 2) == BLDC_OC_NONE);
    assert(bldc_overcurrent_update(&s, 1.80f, 1.5f, 4, 2.2f, 2) == BLDC_OC_NONE);
    assert(bldc_overcurrent_update(&s, 1.80f, 1.5f, 4, 2.2f, 2) == BLDC_OC_SUSTAINED);
    // Emergency must still trip if the median rejects a large raw excursion.
    assert(bldc_overcurrent_update_separate(&s, 0.1f, 3.0f, 1.5f, 4, 2.2f, 2) == BLDC_OC_NONE);
    assert(bldc_overcurrent_update_separate(&s, 0.1f, 3.0f, 1.5f, 4, 2.2f, 2) == BLDC_OC_EMERGENCY);
    puts("PASS: isolated spike rejected; normal and independent raw emergency trip");
    return 0;
}
