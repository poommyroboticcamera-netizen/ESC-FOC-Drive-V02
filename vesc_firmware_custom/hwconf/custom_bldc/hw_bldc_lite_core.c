/*
    Copyright 2026

    This file is part of the VESC firmware hardware port for V.Phase2_v02.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#include "hw.h"

#include "ch.h"
#include "hal.h"
#include "stm32f4xx_conf.h"
#include "terminal.h"
#include "commands.h"
#include "mc_interface.h"
#include "mcpwm.h"
#include "mcpwm_foc.h"
#include "timeout.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static volatile bool i2c_running = false;
static volatile float vbus_estimate = BLDC_NOMINAL_VBUS;
static volatile bool software_armed = false;

static THD_WORKING_AREA(stop_button_thread_wa, 256);
static THD_FUNCTION(stop_button_thread, arg);
static void terminal_bldc_hw_status(int argc, const char **argv);
static void terminal_bldc_injected_status(int argc, const char **argv);
static void terminal_bldc_arm(int argc, const char **argv);
static void terminal_bldc_disarm(int argc, const char **argv);
static void terminal_bldc_current(int argc, const char **argv);
static bool current_adcs_plausible(void);
static void apply_diagnostic_limits(void);

typedef struct {
    uint16_t min[3];
    uint16_t max[3];
    uint32_t sum[3];
    int samples;
} current_adc_stats_t;

typedef struct {
    uint16_t min[3][3];
    uint16_t max[3][3];
    uint32_t sum[3][3];
    uint16_t median_min[3];
    uint16_t median_max[3];
    uint32_t median_sum[3];
    int samples;
} injected_adc_stats_t;

static void sample_current_adcs(current_adc_stats_t *stats, int samples);
static void sample_injected_adcs(injected_adc_stats_t *stats, int samples);

static const I2CConfig i2cfg = {
        OPMODE_I2C,
        100000,
        STD_DUTY_CYCLE
};

bool hw_bldc_lite_input_allowed(void) {
    return software_armed;
}

void hw_bldc_lite_early_init(void) {
    RCC_AHB1PeriphClockCmd(
            RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB,
            ENABLE);

    // Drive HIN and LIN low before the normal 100 ms VESC startup delay.
    palClearPad(GPIOA, 8);
    palClearPad(GPIOA, 9);
    palClearPad(GPIOA, 10);
    palClearPad(GPIOB, 13);
    palClearPad(GPIOB, 14);
    palClearPad(GPIOB, 15);

    palSetPadMode(GPIOA, 8, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, 9, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOA, 10, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, 13, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, 14, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
    palSetPadMode(GPIOB, 15, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
}

void hw_init_gpio(void) {
    RCC_AHB1PeriphClockCmd(
            RCC_AHB1Periph_GPIOA |
            RCC_AHB1Periph_GPIOB |
            RCC_AHB1Periph_GPIOC |
            RCC_AHB1Periph_GPIOD,
            ENABLE);

    hw_bldc_lite_early_init();

    // TIM1 complementary outputs: U/V/W top on PA8/9/10, bottom on PB13/14/15.
    palSetPadMode(GPIOA, 8, PAL_MODE_ALTERNATE(GPIO_AF_TIM1) |
            PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING);
    palSetPadMode(GPIOA, 9, PAL_MODE_ALTERNATE(GPIO_AF_TIM1) |
            PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING);
    palSetPadMode(GPIOA, 10, PAL_MODE_ALTERNATE(GPIO_AF_TIM1) |
            PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING);
    palSetPadMode(GPIOB, 13, PAL_MODE_ALTERNATE(GPIO_AF_TIM1) |
            PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING);
    palSetPadMode(GPIOB, 14, PAL_MODE_ALTERNATE(GPIO_AF_TIM1) |
            PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING);
    palSetPadMode(GPIOB, 15, PAL_MODE_ALTERNATE(GPIO_AF_TIM1) |
            PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUDR_FLOATING);

    // Current and phase-voltage ADC pins.
    palSetPadMode(GPIOA, 0, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOA, 1, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOA, 2, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOA, 3, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOA, 5, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOA, 6, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOC, 0, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOC, 1, PAL_MODE_INPUT_ANALOG);
    palSetPadMode(GPIOC, 2, PAL_MODE_INPUT_ANALOG);

    // SW3/SW4 are active-low local software-stop inputs.
    palSetPadMode(GPIOC, 4, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(GPIOC, 5, PAL_MODE_INPUT_PULLUP);

    // Unconnected sensorless defaults.
    palSetPadMode(HW_HALL_ENC_GPIO1, HW_HALL_ENC_PIN1, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(HW_HALL_ENC_GPIO2, HW_HALL_ENC_PIN2, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(HW_HALL_ENC_GPIO3, HW_HALL_ENC_PIN3, PAL_MODE_INPUT_PULLUP);

#ifdef BLDC_LITE_FOC_3SHUNT
    terminal_register_command_callback(
            "foc_hw_status",
            "Show custom-board ADC and FOC safety status.",
            0,
            terminal_bldc_hw_status);
    terminal_register_command_callback(
            "foc_injected_status",
            "Alias of foc_adc_status; DISARMED only.",
            0,
            terminal_bldc_injected_status);
    terminal_register_command_callback(
            "foc_adc_status",
            "Compare regular DMA current ranks and median; DISARMED only.",
            0,
            terminal_bldc_injected_status);
    terminal_register_command_callback(
            "foc_arm",
            "Arm after checks: foc_arm CONFIRM",
            "CONFIRM",
            terminal_bldc_arm);
    terminal_register_command_callback(
            "foc_disarm",
            "Disarm the custom board immediately.",
            0,
            terminal_bldc_disarm);
    terminal_register_command_callback(
            "foc_current",
            "Run sensorless FOC current; VESC alive packets are required.",
            "[amps]",
            terminal_bldc_current);
#else
    terminal_register_command_callback(
            "bldc_hw_status",
            "Show custom-board ADC and safety status.",
            0,
            terminal_bldc_hw_status);
    terminal_register_command_callback(
            "bldc_injected_status",
            "Compare all three injected samples; DISARMED only.",
            0,
            terminal_bldc_injected_status);
    terminal_register_command_callback(
            "bldc_arm",
            "Arm after checks: bldc_arm CONFIRM",
            "CONFIRM",
            terminal_bldc_arm);
    terminal_register_command_callback(
            "bldc_disarm",
            "Disarm the custom board immediately.",
            0,
            terminal_bldc_disarm);
    terminal_register_command_callback(
            "bldc_current",
            "Run sensorless six-step BLDC current; VESC alive packets are required.",
            "[amps]",
            terminal_bldc_current);
#endif

    chThdCreateStatic(
            stop_button_thread_wa,
            sizeof(stop_button_thread_wa),
            NORMALPRIO + 2,
            stop_button_thread,
            0);
}

void hw_setup_adc_channels(void) {
    const uint8_t t_samp = ADC_SampleTime_56Cycles;

#ifdef BLDC_LITE_FOC_3SHUNT
    // Triple simultaneous conversions: 3 * (56 + 12) / 21 MHz = 9.71 us
    // for current samples, 16.19 us for all 5 ranks. At 25 kHz and 5% max
    // modulation these fit inside the long all-low sampling interval.
    for (int rank = 1; rank <= 3; rank++) {
        ADC_RegularChannelConfig(ADC1, ADC_Channel_10, rank, t_samp);
        ADC_RegularChannelConfig(ADC2, ADC_Channel_11, rank, t_samp);
        ADC_RegularChannelConfig(ADC3, ADC_Channel_12, rank, t_samp);
    }
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 4, t_samp);
    ADC_RegularChannelConfig(ADC2, ADC_Channel_1, 4, t_samp);
    ADC_RegularChannelConfig(ADC3, ADC_Channel_2, 4, t_samp);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 5, t_samp);
    ADC_RegularChannelConfig(ADC2, ADC_Channel_0, 5, t_samp);
    ADC_RegularChannelConfig(ADC3, ADC_Channel_2, 5, t_samp);
#else

    // ADC1 regular channels.
    ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, t_samp);       // PC0 CURRENT_U
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 2, t_samp);        // PA0 PHASE_U
    ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 3, t_samp);        // PA5 unused
    ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 4, t_samp);        // PA5 unused
    ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 5, t_samp);  // VREFINT

    // ADC2 regular channels.
    ADC_RegularChannelConfig(ADC2, ADC_Channel_11, 1, t_samp);       // PC1 CURRENT_V
    ADC_RegularChannelConfig(ADC2, ADC_Channel_1, 2, t_samp);        // PA1 PHASE_V
    ADC_RegularChannelConfig(ADC2, ADC_Channel_6, 3, t_samp);        // PA6 unused
    ADC_RegularChannelConfig(ADC2, ADC_Channel_6, 4, t_samp);        // PA6 unused
    ADC_RegularChannelConfig(ADC2, ADC_Channel_0, 5, t_samp);        // PA0 duplicate

    // ADC3 regular channels.
    ADC_RegularChannelConfig(ADC3, ADC_Channel_12, 1, t_samp);       // PC2 CURRENT_W
    ADC_RegularChannelConfig(ADC3, ADC_Channel_2, 2, t_samp);        // PA2 PHASE_W
    ADC_RegularChannelConfig(ADC3, ADC_Channel_3, 3, t_samp);        // PA3 unused
    ADC_RegularChannelConfig(ADC3, ADC_Channel_3, 4, t_samp);        // PA3 unused
    ADC_RegularChannelConfig(ADC3, ADC_Channel_2, 5, t_samp);        // PA2 duplicate
#endif

    // Injected conversions are synchronized to TIM1 for current control.
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_10, 1, t_samp);
    ADC_InjectedChannelConfig(ADC2, ADC_Channel_11, 1, t_samp);
    ADC_InjectedChannelConfig(ADC3, ADC_Channel_12, 1, t_samp);
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_10, 2, t_samp);
    ADC_InjectedChannelConfig(ADC2, ADC_Channel_11, 2, t_samp);
    ADC_InjectedChannelConfig(ADC3, ADC_Channel_12, 2, t_samp);
    ADC_InjectedChannelConfig(ADC1, ADC_Channel_10, 3, t_samp);
    ADC_InjectedChannelConfig(ADC2, ADC_Channel_11, 3, t_samp);
    ADC_InjectedChannelConfig(ADC3, ADC_Channel_12, 3, t_samp);
}

#ifdef BLDC_LITE_FOC_3SHUNT
float hw_bldc_lite_foc_raw_peak(const volatile float *offsets) {
    const float scale[3] = {FAC_CURRENT1, FAC_CURRENT2, FAC_CURRENT3};
    float peak = 0.0f;
    for (int rank = 0; rank < 3; rank++) {
        for (int phase = 0; phase < 3; phase++) {
            const float magnitude = fabsf(((float)ADC_Value[rank * 3 + phase]
                    - offsets[phase]) * scale[phase]);
            if (magnitude > peak) { peak = magnitude; }
        }
    }
    return peak;
}
#endif

float hw_bldc_lite_get_input_voltage(void) {
    const float adc_to_phase_volts =
            (V_REG / 4096.0) * BLDC_PHASE_DIVIDER_RATIO;
    float phase_peak = (float)ADC_Value[ADC_IND_SENS1];

    if ((float)ADC_Value[ADC_IND_SENS2] > phase_peak) {
        phase_peak = (float)ADC_Value[ADC_IND_SENS2];
    }
    if ((float)ADC_Value[ADC_IND_SENS3] > phase_peak) {
        phase_peak = (float)ADC_Value[ADC_IND_SENS3];
    }
    phase_peak *= adc_to_phase_volts;

    // There is no physical VSUPPLY ADC. Start from the selected nominal bus and
    // only move upward when a phase sample proves that the bus is higher. This
    // is a guard/estimator, not reliable over-voltage protection.
    if (phase_peak > vbus_estimate && phase_peak < 85.0) {
        vbus_estimate += (phase_peak - vbus_estimate) * 0.05;
    } else {
        vbus_estimate += (BLDC_NOMINAL_VBUS - vbus_estimate) * 0.00002;
    }

    if (vbus_estimate < BLDC_NOMINAL_VBUS) {
        vbus_estimate = BLDC_NOMINAL_VBUS;
    }

    return vbus_estimate;
}

void hw_start_i2c(void) {
    i2cAcquireBus(&HW_I2C_DEV);

    if (!i2c_running) {
        palSetPadMode(HW_I2C_SCL_PORT, HW_I2C_SCL_PIN,
                PAL_MODE_ALTERNATE(HW_I2C_GPIO_AF) |
                PAL_STM32_OTYPE_OPENDRAIN |
                PAL_STM32_OSPEED_MID1 |
                PAL_STM32_PUDR_PULLUP);
        palSetPadMode(HW_I2C_SDA_PORT, HW_I2C_SDA_PIN,
                PAL_MODE_ALTERNATE(HW_I2C_GPIO_AF) |
                PAL_STM32_OTYPE_OPENDRAIN |
                PAL_STM32_OSPEED_MID1 |
                PAL_STM32_PUDR_PULLUP);

        i2cStart(&HW_I2C_DEV, &i2cfg);
        i2c_running = true;
    }

    i2cReleaseBus(&HW_I2C_DEV);
}

void hw_stop_i2c(void) {
    i2cAcquireBus(&HW_I2C_DEV);

    if (i2c_running) {
        palSetPadMode(HW_I2C_SCL_PORT, HW_I2C_SCL_PIN, PAL_MODE_INPUT);
        palSetPadMode(HW_I2C_SDA_PORT, HW_I2C_SDA_PIN, PAL_MODE_INPUT);
        i2cStop(&HW_I2C_DEV);
        i2c_running = false;
    }

    i2cReleaseBus(&HW_I2C_DEV);
}

void hw_try_restore_i2c(void) {
    if (!i2c_running) {
        return;
    }

    i2cAcquireBus(&HW_I2C_DEV);
    palSetPadMode(HW_I2C_SCL_PORT, HW_I2C_SCL_PIN,
            PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_PUDR_PULLUP);
    palSetPadMode(HW_I2C_SDA_PORT, HW_I2C_SDA_PIN,
            PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_PUDR_PULLUP);
    palSetPad(HW_I2C_SCL_PORT, HW_I2C_SCL_PIN);
    palSetPad(HW_I2C_SDA_PORT, HW_I2C_SDA_PIN);

    for (int i = 0; i < 16; i++) {
        palClearPad(HW_I2C_SCL_PORT, HW_I2C_SCL_PIN);
        chThdSleep(1);
        palSetPad(HW_I2C_SCL_PORT, HW_I2C_SCL_PIN);
        chThdSleep(1);
    }

    palSetPadMode(HW_I2C_SCL_PORT, HW_I2C_SCL_PIN,
            PAL_MODE_ALTERNATE(HW_I2C_GPIO_AF) |
            PAL_STM32_OTYPE_OPENDRAIN |
            PAL_STM32_OSPEED_MID1 |
            PAL_STM32_PUDR_PULLUP);
    palSetPadMode(HW_I2C_SDA_PORT, HW_I2C_SDA_PIN,
            PAL_MODE_ALTERNATE(HW_I2C_GPIO_AF) |
            PAL_STM32_OTYPE_OPENDRAIN |
            PAL_STM32_OSPEED_MID1 |
            PAL_STM32_PUDR_PULLUP);
    HW_I2C_DEV.state = I2C_STOP;
    i2cStart(&HW_I2C_DEV, &i2cfg);
    i2cReleaseBus(&HW_I2C_DEV);
}

static THD_FUNCTION(stop_button_thread, arg) {
    (void)arg;
    chRegSetThreadName("BLDC local stop");

    // mc_interface_init() runs after hw_init_gpio().
    chThdSleepMilliseconds(1000);
    apply_diagnostic_limits();
    int disarmed_tick = 0;

    for (;;) {
        const bool pressed =
                !palReadPad(GPIOC, 4) ||
                !palReadPad(GPIOC, 5);

        if (pressed) {
            software_armed = false;
        }

        // VESC calibrates the current offsets during motor initialization.
        // Injecting the software DRV fault before that calibration completes
        // makes the calibration wait until its timeout and leaves the three
        // current offsets at their generic defaults. Keep PWM under VESC's
        // normal initialization control until DC calibration is complete;
        // command interfaces are not available until initialization returns.
        if (!mc_interface_dccal_done()) {
            disarmed_tick = 0;
            chThdSleepMilliseconds(1);
            continue;
        }

        // There is no hardware gate-enable pin. Repeatedly issuing a fault is
        // the software interlock that keeps PWM commands stopped until the
        // operator explicitly arms this boot.
        if (!software_armed) {
            if (disarmed_tick == 0) {
                mc_interface_fault_stop(FAULT_CODE_DRV, false, false);
            }
            disarmed_tick++;
            if (disarmed_tick >= 100) {
                disarmed_tick = 0;
            }
        } else {
            disarmed_tick = 0;
        }

        chThdSleepMilliseconds(1);
    }
}

static bool current_adcs_plausible(void) {
    const int adc_mid = 2048;
    // INA181A1 + 1 mOhm gives about 24.8 ADC counts/A at 3.3 V ADC full-scale.
    // A +/-200-count window rejects a missing/saturated channel while leaving
    // margin for reference, ADC and amplifier offset during initial bring-up.
    const int tolerance = 200;
    current_adc_stats_t stats;
    sample_current_adcs(&stats, 32);

    const int populated_channels[3] = {
#ifdef BLDC_LITE_FOC_3SHUNT
            0, 1, 2
#else
            0, 2, 0
#endif
    };
#ifdef BLDC_LITE_FOC_3SHUNT
    const int populated_channel_count = 3;
#else
    const int populated_channel_count = 2;
#endif
    for (int channel = 0; channel < populated_channel_count; channel++) {
        const int i = populated_channels[channel];
        // Check the complete sample window. A noisy or intermittent channel
        // must not arm just because one instantaneous conversion looks valid.
        if (stats.min[i] <= (adc_mid - tolerance) ||
                stats.max[i] >= (adc_mid + tolerance)) {
            return false;
        }
    }

    return true;
}

static void sample_current_adcs(current_adc_stats_t *stats, int samples) {
#ifndef BLDC_LITE_FOC_3SHUNT
    const int adc_indices[3] = {
            ADC_IND_CURR1,
            ADC_IND_CURR2,
            ADC_IND_CURR3
    };
#endif

    stats->samples = samples;
    for (int phase = 0; phase < 3; phase++) {
        stats->min[phase] = 4095;
        stats->max[phase] = 0;
        stats->sum[phase] = 0;
    }

    for (int sample = 0; sample < samples; sample++) {
        for (int phase = 0; phase < 3; phase++) {
#ifdef BLDC_LITE_FOC_3SHUNT
            const uint16_t value = foc_adc_median3(ADC_Value[phase],
                    ADC_Value[phase + 3], ADC_Value[phase + 6]);
#else
            const uint16_t value = ADC_Value[adc_indices[phase]];
#endif
            if (value < stats->min[phase]) {
                stats->min[phase] = value;
            }
            if (value > stats->max[phase]) {
                stats->max[phase] = value;
            }
            stats->sum[phase] += value;
        }
        chThdSleepMilliseconds(1);
    }
}

static void sample_injected_adcs(injected_adc_stats_t *stats, int samples) {
#ifndef BLDC_LITE_FOC_3SHUNT
    ADC_TypeDef *adcs[3] = {ADC1, ADC2, ADC3};
    const uint8_t ranks[3] = {
            ADC_InjectedChannel_1,
            ADC_InjectedChannel_2,
            ADC_InjectedChannel_3
    };
#endif

    stats->samples = samples;
    for (int phase = 0; phase < 3; phase++) {
        stats->median_min[phase] = 4095;
        stats->median_max[phase] = 0;
        stats->median_sum[phase] = 0;
        for (int rank = 0; rank < 3; rank++) {
            stats->min[phase][rank] = 4095;
            stats->max[phase][rank] = 0;
            stats->sum[phase][rank] = 0;
        }
    }

    for (int sample = 0; sample < samples; sample++) {
        for (int phase = 0; phase < 3; phase++) {
            uint16_t values[3];
            for (int rank = 0; rank < 3; rank++) {
#ifdef BLDC_LITE_FOC_3SHUNT
                const uint16_t value = ADC_Value[phase + rank * 3];
#else
                const uint16_t value = ADC_GetInjectedConversionValue(
                        adcs[phase], ranks[rank]);
#endif
                values[rank] = value;
                if (value < stats->min[phase][rank]) {
                    stats->min[phase][rank] = value;
                }
                if (value > stats->max[phase][rank]) {
                    stats->max[phase][rank] = value;
                }
                stats->sum[phase][rank] += value;
            }

            const uint16_t median = hw_bldc_lite_median3_u16(
                    values[0], values[1], values[2]);
            if (median < stats->median_min[phase]) {
                stats->median_min[phase] = median;
            }
            if (median > stats->median_max[phase]) {
                stats->median_max[phase] = median;
            }
            stats->median_sum[phase] += median;
        }
        chThdSleepMilliseconds(1);
    }
}

static void terminal_bldc_arm(int argc, const char **argv) {
    // Custom terminal callbacks receive the command itself in argv[0].
    if (argc != 2 || strcmp(argv[1], "CONFIRM") != 0) {
#ifdef BLDC_LITE_FOC_3SHUNT
        commands_printf("Usage: foc_arm CONFIRM");
#else
        commands_printf("Usage: bldc_arm CONFIRM");
#endif
        return;
    }

    if (!palReadPad(GPIOC, 4) || !palReadPad(GPIOC, 5)) {
        commands_printf("ARM REJECTED: release SW3 and SW4.");
        return;
    }

    if (!mc_interface_dccal_done()) {
        commands_printf("ARM REJECTED: current DC calibration is not complete.");
        return;
    }

    if (!current_adcs_plausible()) {
        commands_printf("ARM REJECTED: current ADCs are not near 2048.");
        commands_printf("Run hardware status to inspect ADC average/min/max.");
        return;
    }

    // Re-apply the diagnostic envelope at every arm so a stored or temporary
    // VESC Tool configuration cannot widen the first-bring-up limits.
    apply_diagnostic_limits();
    software_armed = true;
    commands_printf("ARMED for this boot only. Diagnostic limits are active.");
}

static void terminal_bldc_disarm(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    software_armed = false;
    mc_interface_fault_stop(FAULT_CODE_DRV, false, false);
    commands_printf("DISARMED.");
}

static void terminal_bldc_current(int argc, const char **argv) {
    float current = 0.0;

    if (argc != 2 || sscanf(argv[1], "%f", &current) != 1) {
#ifdef BLDC_LITE_FOC_3SHUNT
        commands_printf("Usage: foc_current [amps]");
#else
        commands_printf("Usage: bldc_current [amps]");
#endif
        return;
    }
    if (!software_armed) {
#ifdef BLDC_LITE_FOC_3SHUNT
        commands_printf("FOC CURRENT REJECTED: run foc_arm CONFIRM first.");
#else
        commands_printf("BLDC CURRENT REJECTED: run bldc_arm CONFIRM first.");
#endif
        return;
    }
#ifdef BLDC_LITE_FOC_3SHUNT
    if (mc_interface_get_configuration()->motor_type != MOTOR_TYPE_FOC) {
        commands_printf("FOC CURRENT REJECTED: controller is not in FOC mode.");
        return;
    }
#else
    if (mc_interface_get_configuration()->motor_type != MOTOR_TYPE_BLDC) {
        commands_printf("BLDC CURRENT REJECTED: controller is not in BLDC mode.");
        return;
    }
#endif
    if (mc_interface_get_fault() != FAULT_CODE_NONE) {
        commands_printf("BLDC CURRENT REJECTED: clear the active fault first.");
        return;
    }
    if (!(current >= MCCONF_CC_MIN_CURRENT &&
            current <= HW_BLDC_TERMINAL_CURRENT_MAX)) {
        commands_printf("BLDC CURRENT REJECTED: use %.2f to %.2f A.",
                (double)MCCONF_CC_MIN_CURRENT,
                (double)HW_BLDC_TERMINAL_CURRENT_MAX);
        return;
    }

    timeout_reset();
    mc_interface_set_current(current);
#ifdef BLDC_LITE_FOC_3SHUNT
    commands_printf("FOC three-shunt current %.3f A started.", (double)current);
    commands_printf("Keep VESC Send Alive enabled; stop with foc_disarm.");
#else
    commands_printf("BLDC six-step current %.3f A started.", (double)current);
    commands_printf("Keep VESC Send Alive enabled; stop with bldc_disarm.");
#endif
}

static void terminal_bldc_hw_status(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    current_adc_stats_t regular_stats;
    const volatile mc_configuration *mcconf = mc_interface_get_configuration();
    sample_current_adcs(&regular_stats, 64);

    commands_printf("Hardware: %s", HW_NAME);
    commands_printf("Current sense: %s, gain %.1f V/V, shunt %.3f mOhm",
            CURRENT_AMP_PART,
            (double)CURRENT_AMP_GAIN,
            (double)(CURRENT_SHUNT_RES * 1000.0));
    commands_printf("Current polarity: %s", CURRENT_SENSE_POLARITY_TEXT);
#ifdef BLDC_LITE_FOC_3SHUNT
    commands_printf("Firmware profile v11: FOC three-shunt U/V/W");
    commands_printf("FOC current filter: regular DMA median of 3 per PWM scan");
    commands_printf("Raw emergency protection: all 9 unfiltered current conversions");
    commands_printf("Current channels used by control/protection: U, V and W");
#else
    commands_printf("Injected current filter: median of 3 repeated samples");
    commands_printf("Noise compensation v9: two-shunt U/W selection and magnitude limits");
    commands_printf("Current channels used by control/protection: U and W (V ignored)");
#endif
    commands_printf("Software interlock: %s",
            software_armed ? "ARMED" : "DISARMED");
    commands_printf("Current DC calibration: %s",
            mc_interface_dccal_done() ? "DONE" : "WAITING");
    if (mcconf->motor_type == MOTOR_TYPE_BLDC && mc_interface_dccal_done()) {
        float zero_u, zero_v, zero_w;
        mcpwm_get_current_offsets(&zero_u, &zero_v, &zero_w);
        commands_printf("BLDC zero U/V/W: %.3f %.3f %.3f ADC counts",
                (double)zero_u, (double)zero_v, (double)zero_w);
    }
#ifdef BLDC_LITE_FOC_3SHUNT
    if (mcconf->motor_type == MOTOR_TYPE_FOC && mc_interface_dccal_done()) {
        commands_printf("FOC zero U/V/W: %.3f %.3f %.3f ADC counts",
                (double)mcconf->foc_offsets_current[0],
                (double)mcconf->foc_offsets_current[1],
                (double)mcconf->foc_offsets_current[2]);
    }
    commands_printf("Motor control: %s",
            mcconf->motor_type == MOTOR_TYPE_FOC ?
                    "FOC sensorless three-shunt" : "INVALID (not FOC)");
#else
    commands_printf("Motor control: %s",
            mcconf->motor_type == MOTOR_TYPE_BLDC ?
                    "BLDC sensorless six-step" : "INVALID (not BLDC)");
#endif
#ifdef BLDC_LITE_FOC_3SHUNT
    commands_printf("FOC PWM/sample: %.0f Hz / V0; median current window 9.71 us",
            (double)mcconf->foc_f_zv);
    commands_printf("FOC PI kp/ki: %.6f / %.3f",
            (double)mcconf->foc_current_kp, (double)mcconf->foc_current_ki);
    commands_printf("FOC motor R/L/flux: %.6f Ohm / %.3f uH / %.6f Wb",
            (double)mcconf->foc_motor_r, (double)(mcconf->foc_motor_l * 1e6f),
            (double)mcconf->foc_motor_flux_linkage);
#else
    commands_printf("BLDC gain/min-ERPM/cycle/BEMF: %.4f / %.0f / %.1f / %.1f",
            (double)mcconf->cc_gain,
            (double)mcconf->sl_min_erpm,
            (double)mcconf->sl_cycle_int_limit,
            (double)mcconf->sl_bemf_coupling_k);
    commands_printf("BLDC startup boost duty: %.4f (disabled for low-current bench)",
            (double)mcconf->cc_startup_boost_duty);
#endif
    commands_printf("Diagnostic limits phase/input/abs/duty: %.2f / %.2f / %.2f A / %.3f",
            (double)mcconf->l_current_max,
            (double)mcconf->l_in_current_max,
            (double)mcconf->l_abs_current_max,
            (double)mcconf->l_max_duty);
    commands_printf("ABS protection: %.2f A for %u samples, raw emergency %.2f A for %u samples",
            (double)mcconf->l_abs_current_max,
            (unsigned int)HW_ABS_OVERCURRENT_DEBOUNCE_SAMPLES,
            (double)HW_ABS_OVERCURRENT_HARD_LIMIT,
            (unsigned int)HW_ABS_OVERCURRENT_HARD_DEBOUNCE_SAMPLES);
    commands_printf("Terminal current range: %.2f to %.2f A",
            (double)MCCONF_CC_MIN_CURRENT,
            (double)HW_BLDC_TERMINAL_CURRENT_MAX);
#ifndef BLDC_LITE_FOC_3SHUNT
    commands_printf("BLDC nominal bus reference: %.1f ADC counts",
            (double)GET_BLDC_VBUS_ADC());
#endif
    commands_printf("Nominal/estimated bus: %.2f / %.2f V",
            (double)BLDC_NOMINAL_VBUS,
            (double)hw_bldc_lite_get_input_voltage());
    commands_printf("Current DMA avg U/V/W (control samples): %u %u %u",
            (unsigned int)(regular_stats.sum[0] / regular_stats.samples),
            (unsigned int)(regular_stats.sum[1] / regular_stats.samples),
            (unsigned int)(regular_stats.sum[2] / regular_stats.samples));
    commands_printf("Current DMA min U/V/W: %u %u %u",
            regular_stats.min[0], regular_stats.min[1], regular_stats.min[2]);
    commands_printf("Current DMA max U/V/W: %u %u %u",
            regular_stats.max[0], regular_stats.max[1], regular_stats.max[2]);
    commands_printf("Phase ADC U/V/W: %u %u %u",
            ADC_Value[ADC_IND_SENS1],
            ADC_Value[ADC_IND_SENS2],
            ADC_Value[ADC_IND_SENS3]);
    commands_printf("SW3/SW4: %s / %s",
            palReadPad(GPIOC, 4) ? "released" : "PRESSED",
            palReadPad(GPIOC, 5) ? "released" : "PRESSED");
    commands_printf("WARNING: no physical VSUPPLY ADC, driver fault, or MOSFET temperature sensor.");
}

static void terminal_bldc_injected_status(int argc, const char **argv) {
    (void)argc;
    (void)argv;

    if (software_armed || mc_interface_get_state() != MC_STATE_OFF) {
        commands_printf("ADC TEST REJECTED: disarm first.");
        return;
    }

    injected_adc_stats_t stats;
    const char *phase_names[3] = {"U", "V", "W"};
    sample_injected_adcs(&stats, 128);

#ifdef BLDC_LITE_FOC_3SHUNT
    commands_printf("FOC regular DMA raw ranks, %d DISARMED samples:", stats.samples);
#else
    commands_printf("Injected ADC raw ranks, %d DISARMED samples:", stats.samples);
#endif
    for (int phase = 0; phase < 3; phase++) {
        commands_printf("%s avg r1/r2/r3: %u %u %u",
                phase_names[phase],
                (unsigned int)(stats.sum[phase][0] / stats.samples),
                (unsigned int)(stats.sum[phase][1] / stats.samples),
                (unsigned int)(stats.sum[phase][2] / stats.samples));
        commands_printf("%s min r1/r2/r3: %u %u %u",
                phase_names[phase],
                stats.min[phase][0], stats.min[phase][1], stats.min[phase][2]);
        commands_printf("%s max r1/r2/r3: %u %u %u",
                phase_names[phase],
                stats.max[phase][0], stats.max[phase][1], stats.max[phase][2]);
        commands_printf("%s median avg/min/max: %u %u %u",
                phase_names[phase],
                (unsigned int)(stats.median_sum[phase] / stats.samples),
                stats.median_min[phase], stats.median_max[phase]);
    }
    commands_printf("Control and normal ABS protection use the three-rank median.");
#ifdef BLDC_LITE_FOC_3SHUNT
    commands_printf("Emergency protection uses all unfiltered current ranks.");
#endif
}

static void apply_diagnostic_limits(void) {
    volatile mc_configuration *mcconf =
            (volatile mc_configuration *)mc_interface_get_configuration();

#ifdef BLDC_LITE_FOC_3SHUNT
    mcconf->motor_type = MOTOR_TYPE_FOC;
    mcconf->foc_sensor_mode = FOC_SENSOR_MODE_SENSORLESS;
    mcconf->foc_control_sample_mode = FOC_CONTROL_SAMPLE_MODE_V0;
    mcconf->foc_current_sample_mode = FOC_CURRENT_SAMPLE_MODE_LONGEST_ZERO;
#else
    mcconf->motor_type = MOTOR_TYPE_BLDC;
#endif
    mcconf->pwm_mode = MCCONF_PWM_MODE;
    mcconf->comm_mode = MCCONF_COMM_MODE;
    mcconf->sensor_mode = MCCONF_SENSOR_MODE;
    mcconf->cc_gain = MCCONF_CC_GAIN;
    mcconf->cc_min_current = MCCONF_CC_MIN_CURRENT;
    mcconf->cc_startup_boost_duty = MCCONF_CC_STARTUP_BOOST_DUTY;
    mcconf->cc_ramp_step_max = MCCONF_CC_RAMP_STEP;
    mcconf->sl_min_erpm = MCCONF_SL_MIN_RPM;
    mcconf->sl_min_erpm_cycle_int_limit = MCCONF_SL_MIN_ERPM_CYCLE_INT_LIMIT;
    mcconf->sl_max_fullbreak_current_dir_change = MCCONF_SL_MAX_FB_CURR_DIR_CHANGE;
    mcconf->sl_cycle_int_limit = MCCONF_SL_CYCLE_INT_LIMIT;
    mcconf->sl_phase_advance_at_br = MCCONF_SL_PHASE_ADVANCE_AT_BR;
    mcconf->sl_cycle_int_rpm_br = MCCONF_SL_CYCLE_INT_BR;
    mcconf->sl_bemf_coupling_k = MCCONF_SL_BEMF_COUPLING_K;
    mcconf->l_current_max = MCCONF_L_CURRENT_MAX;
    mcconf->l_current_min = MCCONF_L_CURRENT_MIN;
    mcconf->l_in_current_max = MCCONF_L_IN_CURRENT_MAX;
    mcconf->l_in_current_min = MCCONF_L_IN_CURRENT_MIN;
    mcconf->l_abs_current_max = MCCONF_L_MAX_ABS_CURRENT;
    mcconf->l_slow_abs_current = MCCONF_L_SLOW_ABS_OVERCURRENT;
    mcconf->l_max_duty = MCCONF_L_MAX_DUTY;
    mcconf->l_duty_start = MCCONF_L_DUTY_START;
    mcconf->l_watt_max = MCCONF_L_WATT_MAX;
    mcconf->l_watt_min = MCCONF_L_WATT_MIN;
}
