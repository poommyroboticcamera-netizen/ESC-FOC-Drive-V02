#ifndef BLDC_CONTROL_NOISE_H
#define BLDC_CONTROL_NOISE_H

#include <math.h>
#include <stdbool.h>

// Control feedback only. Never use this state for an overcurrent decision.
// Backward-Euler RC filter remains bounded for any positive sample period.
static inline float bldc_control_noise_update(float previous, float sample,
        float sample_hz, float tau_seconds) {
    if (!(sample_hz > 0.0f) || !(tau_seconds > 0.0f)) {
        return sample;
    }
    const float alpha = 1.0f / (1.0f + sample_hz * tau_seconds);
    return previous + alpha * (sample - previous);
}

// Six-step selects the conducting low-side phase. Its sign depends on the
// amplifier and shunt orientation, while torque-current regulation needs the
// magnitude. Direction remains controlled by the signed duty/current command.
static inline float bldc_current_control_error(float current_set,
        float current_feedback) {
    return fabsf(current_set) - fabsf(current_feedback);
}

static inline float bldc_current_limit_value(float current_feedback) {
    return fabsf(current_feedback);
}

// Select one of the two populated low-side current channels. When V is the
// sinking phase, use the measured U or W phase in the active two-phase pair.
// Sampling is synchronized to PWM, so the source phase is observable during
// the low-side sampling interval. Sign is intentionally discarded.
static inline float bldc_uw_current_sample(bool forward, int comm_step,
        float current_u, float current_w) {
    bool use_u;
    if (forward) {
        use_u = comm_step == 3 || comm_step == 5 || comm_step == 6;
    } else {
        use_u = comm_step == 2 || comm_step == 5 || comm_step == 6;
    }
    if (comm_step < 1 || comm_step > 6) {
        return fmaxf(fabsf(current_u), fabsf(current_w));
    }
    return fabsf(use_u ? current_u : current_w);
}

// During full brake all low sides can conduct. Reconstruct the unpopulated V
// phase from Kirchhoff's current law and use the largest phase magnitude. This
// is conservative for protection and never consumes the physical V ADC input.
static inline float bldc_uw_reconstructed_peak(float current_u,
        float current_w) {
    const float current_v = -(current_u + current_w);
    return fmaxf(fabsf(current_v),
            fmaxf(fabsf(current_u), fabsf(current_w)));
}

#endif
