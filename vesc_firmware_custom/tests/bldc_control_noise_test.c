#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../motor/bldc_control_noise.h"

int main(void) {
    const float rates[] = {4000.0f, 20000.0f, 40000.0f};
    for (unsigned int r = 0; r < sizeof(rates)/sizeof(rates[0]); r++) {
        float y = 0.0f;
        for (int i = 0; i < 100; i++) {
            float next = bldc_control_noise_update(y, 1.0f, rates[r], 0.00005f);
            assert(next >= y && next <= 1.0f);
            y = next;
        }
        assert(fabsf(y - 1.0f) < 0.00001f);
        for (int i = 0; i < 100; i++) {
            y = bldc_control_noise_update(y, (i & 1) ? 0.2f : -0.2f,
                    rates[r], 0.00005f);
        }
        assert(fabsf(y) < 0.2f);
    }
    assert(bldc_control_noise_update(0.0f, 2.66f, 0.0f, 0.00005f) == 2.66f);
    assert(fabsf(bldc_current_control_error(0.05f, 0.01f) - 0.04f) < 0.00001f);
    assert(bldc_current_control_error(0.05f, -0.83f) < -0.77f);
    assert(bldc_current_control_error(-0.05f, 0.83f) < -0.77f);
    assert(fabsf(bldc_current_limit_value(-1.53f) - 1.53f) < 0.00001f);
    assert(bldc_current_limit_value(-1.53f) > 0.50f);
    assert(fabsf(bldc_uw_current_sample(true, 1, -1.0f, -2.0f) - 2.0f) < 0.00001f);
    assert(fabsf(bldc_uw_current_sample(true, 3, -1.0f, -2.0f) - 1.0f) < 0.00001f);
    assert(fabsf(bldc_uw_current_sample(false, 2, -1.0f, -2.0f) - 1.0f) < 0.00001f);
    assert(fabsf(bldc_uw_current_sample(false, 3, -1.0f, -2.0f) - 2.0f) < 0.00001f);
    assert(fabsf(bldc_uw_reconstructed_peak(1.0f, 2.0f) - 3.0f) < 0.00001f);
    assert(fabsf(bldc_uw_reconstructed_peak(-1.0f, 2.0f) - 2.0f) < 0.00001f);
    puts("PASS: filter, magnitude limits and U/W six-step selection");
    return 0;
}
