#ifndef BLDC_OVERCURRENT_H
#define BLDC_OVERCURRENT_H

typedef enum {
    BLDC_OC_NONE = 0,
    BLDC_OC_SUSTAINED,
    BLDC_OC_EMERGENCY
} bldc_oc_result_t;

typedef struct {
    unsigned int normal_count;
    unsigned int emergency_count;
} bldc_oc_state_t;

static inline bldc_oc_result_t bldc_overcurrent_update_separate(
        bldc_oc_state_t *state, float magnitude, float raw_magnitude,
        float normal_limit, unsigned int normal_samples,
        float emergency_limit, unsigned int emergency_samples) {
    if (raw_magnitude > emergency_limit) {
        state->emergency_count++;
    } else {
        state->emergency_count = 0;
    }
    if (magnitude > normal_limit) {
        state->normal_count++;
    } else {
        state->normal_count = 0;
    }

    if (state->emergency_count >= emergency_samples) {
        state->normal_count = 0;
        state->emergency_count = 0;
        return BLDC_OC_EMERGENCY;
    }
    if (state->normal_count >= normal_samples) {
        state->normal_count = 0;
        state->emergency_count = 0;
        return BLDC_OC_SUSTAINED;
    }
    return BLDC_OC_NONE;
}

static inline bldc_oc_result_t bldc_overcurrent_update(
        bldc_oc_state_t *state, float magnitude,
        float normal_limit, unsigned int normal_samples,
        float emergency_limit, unsigned int emergency_samples) {
    return bldc_overcurrent_update_separate(state, magnitude, magnitude,
            normal_limit, normal_samples, emergency_limit, emergency_samples);
}

#endif
