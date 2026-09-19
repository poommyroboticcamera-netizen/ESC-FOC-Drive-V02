/*
    Copyright 2026

    This file is part of the VESC firmware hardware port for V.Phase2_v02.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/

#ifndef HW_BLDC_LITE_CORE_H_
#define HW_BLDC_LITE_CORE_H_

#if !defined(BLDC_LITE_24)
#error "Include hw_bldc_lite_24.h"
#endif

#define HW_MAJOR                        1
#define HW_MINOR                        0

// The PCB has three low-side shunts. They are not phase shunts.
#define HW_HAS_3_SHUNTS
#ifndef BLDC_LITE_FOC_3SHUNT
#define HW_BLDC_UW_SHUNTS_ONLY
#endif

// U13-U15 have IN+ at AGND and IN- at the MOSFET side of each low-side
// shunt. Sink current therefore drives the INA181 output below its 1.65 V
// reference. The generic BLDC six-step current selector already negates the
// active low-side channel, so the ADC samples must not be inverted here.
#ifdef BLDC_LITE_FOC_3SHUNT
// The amplifiers are wired opposite to the usual VESC low-side convention.
// FOC requires signed phase currents, so invert all three calibration factors.
#define CURRENT_CAL1                    -1.0
#define CURRENT_CAL2                    -1.0
#define CURRENT_CAL3                    -1.0
#define CURRENT_SENSE_POLARITY_TEXT     "INVERTED in firmware; U/V/W enabled"
#else
#define CURRENT_SENSE_POLARITY_TEXT     "NORMAL; populated current channels U/W, V ignored"
#endif

// Provisional only. Verify EG2132/MOSFET waveforms before motor testing.
#define HW_DEAD_TIME_NSEC               1000.0

// Force all six gate-driver inputs low as early as VESC main() allows.
void hw_bldc_lite_early_init(void);
#define HW_EARLY_INIT()                 hw_bldc_lite_early_init()

// Block every generic motor command until this boot has passed bldc_arm.
bool hw_bldc_lite_input_allowed(void);
#define HW_INPUT_ALLOWED()              hw_bldc_lite_input_allowed()

// The PCB has no gate-enable or driver-fault connection.
#define ENABLE_GATE()
#define DISABLE_GATE()
#define DCCAL_ON()
#define DCCAL_OFF()
#define IS_DRV_FAULT()                  0

// No status LEDs are connected in the schematic.
#define LED_GREEN_ON()
#define LED_GREEN_OFF()
#define LED_RED_ON()
#define LED_RED_OFF()

/*
 * ADC vector (three ADCs operating together)
 *
 * 0:  ADC1 IN10 / PC0   CURRENT_U
 * 1:  ADC2 IN11 / PC1   CURRENT_V
 * 2:  ADC3 IN12 / PC2   CURRENT_W
 * 3:  ADC1 IN0  / PA0   PHASE_U divider
 * 4:  ADC2 IN1  / PA1   PHASE_V divider
 * 5:  ADC3 IN2  / PA2   PHASE_W divider
 * 6:  ADC1 IN5  / PA5   unused
 * 7:  ADC2 IN6  / PA6   unused
 * 8:  ADC3 IN3  / PA3   unused
 * 9:  ADC1 IN5  / PA5   unused duplicate
 * 10: ADC2 IN6  / PA6   unused duplicate
 * 11: ADC3 IN3  / PA3   unused duplicate
 * 12: ADC1 VREFINT
 * 13: ADC2 IN0  / PA0   phase duplicate
 * 14: ADC3 IN2  / PA2   phase duplicate
 */
#define HW_ADC_INJ_CHANNELS             3
#define HW_ADC_NBR_CONV                 5
#define HW_ADC_CHANNELS                 (HW_ADC_NBR_CONV * 3)
#ifdef BLDC_LITE_FOC_3SHUNT
#include "motor/foc_adc_noise.h"
#define GET_CURRENT1() ((float)foc_adc_median3(ADC_Value[0], ADC_Value[3], ADC_Value[6]))
#define GET_CURRENT2() ((float)foc_adc_median3(ADC_Value[1], ADC_Value[4], ADC_Value[7]))
#define GET_CURRENT3() ((float)foc_adc_median3(ADC_Value[2], ADC_Value[5], ADC_Value[8]))
float hw_bldc_lite_foc_raw_peak(const volatile float *offsets);
#define HW_FOC_RAW_CURRENT_PEAK(offsets) hw_bldc_lite_foc_raw_peak(offsets)
#endif

// This board is first brought up with the ADC at 21 MHz rather than the VESC
// default 42 MHz. The slower clock stays inside the STM32F405 ADC rating and
// gives the INA181 outputs more acquisition time.
#define HW_ADC_PRESCALER                ADC_Prescaler_Div4

// Each injected ADC converts the same INA181 output three times per PWM
// trigger. Use the median so that one conversion disturbed by ADC settling or
// a narrow switching spike cannot become the BLDC current-control sample.
// The three raw JDR values remain available to the bldc_injected_status
// terminal diagnostic.
static inline uint16_t hw_bldc_lite_median3_u16(
        uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) {
        if (b > c) {
            return b;
        }
        return a > c ? c : a;
    }
    if (a > c) {
        return a;
    }
    return b > c ? c : b;
}

static inline uint16_t hw_bldc_lite_injected_median3(ADC_TypeDef *adc) {
    const uint16_t a = ADC_GetInjectedConversionValue(
            adc, ADC_InjectedChannel_1);
    const uint16_t b = ADC_GetInjectedConversionValue(
            adc, ADC_InjectedChannel_2);
    const uint16_t c = ADC_GetInjectedConversionValue(
            adc, ADC_InjectedChannel_3);
    return hw_bldc_lite_median3_u16(a, b, c);
}

#define HW_GET_INJ_CURR1()              ((float)hw_bldc_lite_injected_median3(ADC1))
#define HW_GET_INJ_CURR2()              ((float)hw_bldc_lite_injected_median3(ADC2))
#define HW_GET_INJ_CURR3()              ((float)hw_bldc_lite_injected_median3(ADC3))

#define ADC_IND_CURR1                   0
#define ADC_IND_CURR2                   1
#define ADC_IND_CURR3                   2
#ifdef BLDC_LITE_FOC_3SHUNT
// FOC: ranks 1-3 repeat U/V/W; rank 4 reads phase voltage; rank 5 VREF.
#define ADC_IND_SENS1                   9
#define ADC_IND_SENS2                   10
#define ADC_IND_SENS3                   11
#else
#define ADC_IND_SENS1                   3
#define ADC_IND_SENS2                   4
#define ADC_IND_SENS3                   5
#endif
#define ADC_IND_EXT                     6
#define ADC_IND_EXT2                    7
#define ADC_IND_TEMP_MOS                8
#define ADC_IND_TEMP_MOTOR              11
#define ADC_IND_VIN_SENS                3
#define ADC_IND_VREFINT                 12

#ifndef V_REG
#define V_REG                           3.3
#endif

// R91-R93 = 56k and R94-R96 = 2.2k. VESC uses VIN_R1/VIN_R2 for
// both phase-voltage telemetry and FOC voltage reconstruction.
#define VIN_R1                          56000.0
#define VIN_R2                          2200.0
#define BLDC_PHASE_DIVIDER_RATIO        ((VIN_R1 + VIN_R2) / VIN_R2)
#define ADC_VOLTS_PH_FACTOR             1.0

// This board has no physical VSUPPLY ADC. Do not let sensorless BLDC use the
// PHASE_U sample at ADC index 3 as a bus reading because it changes with the
// commutation state. Convert the fixed 24 V supply to the equivalent divider
// ADC count expected by the generic flux-integrator equations.
#define GET_BLDC_VBUS_ADC()             ((float)(BLDC_NOMINAL_VBUS / \
                                            ((V_REG / 4096.0) * BLDC_PHASE_DIVIDER_RATIO)))

// U13-U15 are confirmed INA181A1: fixed gain 20 V/V.
#define CURRENT_AMP_PART                "INA181A1"
#ifndef CURRENT_AMP_GAIN
#define CURRENT_AMP_GAIN                20.0
#endif
#ifndef CURRENT_SHUNT_RES
#define CURRENT_SHUNT_RES               0.001
#endif

#define ADC_VOLTS(ch)                   ((float)ADC_Value[ch] / 4096.0 * V_REG)

// There is no temperature sensor on the PCB. A fixed value prevents a floating
// ADC channel from generating a false reading; it provides no thermal safety.
#define NTC_RES(adc_val)                (10000.0)
#define NTC_RES_MOTOR(adc_val)          (10000.0)
#define NTC_TEMP(adc_ind)               (25.0)
#define NTC_TEMP_MOTOR(beta)            (25.0)

float hw_bldc_lite_get_input_voltage(void);
#define GET_INPUT_VOLTAGE()             hw_bldc_lite_get_input_voltage()

// PPM input at H4 is PB6 / TIM4_CH1.
#define HW_USE_SERVO_TIM4
#define HW_ICU_TIMER                    TIM4
#define HW_ICU_TIM_CLK_EN()             RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE)
#define HW_ICU_DEV                      ICUD4
#define HW_ICU_CHANNEL                  ICU_CHANNEL_1
#define HW_ICU_GPIO_AF                  GPIO_AF_TIM4
#define HW_ICU_GPIO                     GPIOB
#define HW_ICU_PIN                      6

// Unconnected UART/I2C defaults. USB and CAN are the intended interfaces.
#define HW_UART_DEV                     SD3
#define HW_UART_GPIO_AF                 GPIO_AF_USART3
#define HW_UART_TX_PORT                 GPIOB
#define HW_UART_TX_PIN                  10
#define HW_UART_RX_PORT                 GPIOB
#define HW_UART_RX_PIN                  11

#define HW_I2C_DEV                      I2CD2
#define HW_I2C_GPIO_AF                  GPIO_AF_I2C2
#define HW_I2C_SCL_PORT                 GPIOB
#define HW_I2C_SCL_PIN                  10
#define HW_I2C_SDA_PORT                 GPIOB
#define HW_I2C_SDA_PIN                  11

// Unconnected sensor pins. Sensorless FOC is the supported mode.
#define HW_HALL_ENC_GPIO1               GPIOC
#define HW_HALL_ENC_PIN1                6
#define HW_HALL_ENC_GPIO2               GPIOC
#define HW_HALL_ENC_PIN2                7
#define HW_HALL_ENC_GPIO3               GPIOC
#define HW_HALL_ENC_PIN3                8
#define HW_ENC_TIM                      TIM3
#define HW_ENC_TIM_AF                   GPIO_AF_TIM3
#define HW_ENC_TIM_CLK_EN()             RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE)
#define HW_ENC_EXTI_PORTSRC             EXTI_PortSourceGPIOC
#define HW_ENC_EXTI_PINSRC              EXTI_PinSource8
#define HW_ENC_EXTI_LINE                EXTI_Line8
#define HW_ENC_TIM_ISR_CH               TIM3_IRQn
#define HW_ENC_TIM_ISR_VEC              TIM3_IRQHandler

// Unconnected SPI defaults required by optional VESC applications.
#define HW_SPI_DEV                      SPID1
#define HW_SPI_GPIO_AF                  GPIO_AF_SPI1
#define HW_SPI_PORT_NSS                 GPIOA
#define HW_SPI_PIN_NSS                  4
#define HW_SPI_PORT_SCK                 GPIOA
#define HW_SPI_PIN_SCK                  5
#define HW_SPI_PORT_MOSI                GPIOA
#define HW_SPI_PIN_MOSI                 7
#define HW_SPI_PORT_MISO                GPIOA
#define HW_SPI_PIN_MISO                 6

#define ADC_V_L1                        ADC_Value[ADC_IND_SENS1]
#define ADC_V_L2                        ADC_Value[ADC_IND_SENS2]
#define ADC_V_L3                        ADC_Value[ADC_IND_SENS3]

// With no DC-bus divider, use the measured phase-neutral average for BLDC
// zero-crossing. FOC uses the voltage estimator exposed above.
#define ADC_V_ZERO                      ((ADC_V_L1 + ADC_V_L2 + ADC_V_L3) / 3)

#define READ_HALL1()                    palReadPad(HW_HALL_ENC_GPIO1, HW_HALL_ENC_PIN1)
#define READ_HALL2()                    palReadPad(HW_HALL_ENC_GPIO2, HW_HALL_ENC_PIN2)
#define READ_HALL3()                    palReadPad(HW_HALL_ENC_GPIO3, HW_HALL_ENC_PIN3)

// This image is deliberately locked to sensorless six-step BLDC. Applying the
// force macro before motor initialization prevents a stored FOC configuration
// from starting the wrong control implementation.
#ifdef BLDC_LITE_FOC_3SHUNT
#define HW_FORCE_MOTOR_TYPE             MOTOR_TYPE_FOC
#define MCCONF_DEFAULT_MOTOR_TYPE       MOTOR_TYPE_FOC
#define MCCONF_FOC_SENSOR_MODE          FOC_SENSOR_MODE_SENSORLESS
#define MCCONF_FOC_CONTROL_SAMPLE_MODE  FOC_CONTROL_SAMPLE_MODE_V0
#define MCCONF_FOC_CURRENT_SAMPLE_MODE  FOC_CURRENT_SAMPLE_MODE_LONGEST_ZERO
#define HW_FOC_I_ABS_FILTER_MAGNITUDE
#define HW_FOC_BENCH_SAMPLING
#define MCCONF_FOC_F_ZV                 25000.0
#else
#define HW_FORCE_MOTOR_TYPE             MOTOR_TYPE_BLDC
#define MCCONF_DEFAULT_MOTOR_TYPE       MOTOR_TYPE_BLDC
#endif
#define MCCONF_PWM_MODE                 PWM_MODE_SYNCHRONOUS
#define MCCONF_COMM_MODE                COMM_MODE_INTEGRATE
#define MCCONF_SENSOR_MODE              SENSOR_MODE_SENSORLESS
#define MCCONF_CC_GAIN                  0.0046
#define MCCONF_CC_MIN_CURRENT           0.05
// This 24 V, low-current bench setup cannot tolerate the generic BLDC startup
// boost. At 0.01 it overrides the current loop below about 0.8% duty and can
// create several consecutive over-current pulses before duty backs off.
#define MCCONF_CC_STARTUP_BOOST_DUTY    0.0
#define MCCONF_CC_RAMP_STEP             0.01
#define MCCONF_SL_MIN_RPM               150.0
#define MCCONF_SL_MIN_ERPM_CYCLE_INT_LIMIT 1100.0
#define MCCONF_SL_CYCLE_INT_LIMIT       62.0
#define MCCONF_SL_BEMF_COUPLING_K       600.0
#define MCCONF_SL_PHASE_ADVANCE_AT_BR   0.8
#define MCCONF_SL_CYCLE_INT_BR          80000.0
#define MCCONF_SL_MAX_FB_CURR_DIR_CHANGE 0.10
#define HW_BLDC_TERMINAL_CURRENT_MAX    0.20
// The normal ABS limit uses consecutive samples of the instantaneous current
// magnitude. This rejects isolated ADC/PWM spikes without allowing alternating
// d/q current to cancel in a signed low-pass filter. The magnitude telemetry is
// filtered directly for diagnostics, while a separate raw emergency cutoff is
// always immediate.
#define MCCONF_L_SLOW_ABS_OVERCURRENT   false
#define HW_ABS_OVERCURRENT_DEBOUNCE_SAMPLES 4
#define HW_ABS_OVERCURRENT_HARD_LIMIT   2.2
#define HW_ABS_OVERCURRENT_HARD_DEBOUNCE_SAMPLES 2
#define HW_BLDC_RAW_CURRENT_PROTECTION
// v6: smooth only the current regulator; ABS and current limits stay unfiltered.
#define HW_BLDC_CONTROL_FILTER_TAU_S    0.00005f
#define HW_BLDC_FRACTIONAL_ZERO
#define HW_BLDC_MAGNITUDE_CURRENT_LIMITS
#define HW_DISABLE_FOC_OPENLOOP_ALL
#define MCCONF_L_CURRENT_MAX            0.5
#define MCCONF_L_CURRENT_MIN            -0.1
#define MCCONF_L_IN_CURRENT_MAX         0.25
#define MCCONF_L_IN_CURRENT_MIN         -0.02
#define MCCONF_L_MAX_ABS_CURRENT        1.5
#define MCCONF_L_MIN_VOLTAGE            BLDC_DEFAULT_VIN_MIN
#define MCCONF_L_MAX_VOLTAGE            BLDC_DEFAULT_VIN_MAX
#define MCCONF_L_BATTERY_CUT_START      BLDC_BATTERY_CUT_START
#define MCCONF_L_BATTERY_CUT_END        BLDC_BATTERY_CUT_END

// The estimated voltage is nominal at boot. Setting both regen thresholds at
// or below nominal forces the configured input regen current toward zero.
#define MCCONF_L_BATTERY_REGEN_CUT_START (BLDC_NOMINAL_VBUS - 0.5)
#define MCCONF_L_BATTERY_REGEN_CUT_END  BLDC_NOMINAL_VBUS
#define MCCONF_L_MIN_DUTY               0.005
#define MCCONF_L_MAX_DUTY               0.05
#define MCCONF_L_DUTY_START             0.03
#define MCCONF_L_WATT_MAX               (BLDC_NOMINAL_VBUS * 0.25)
#define MCCONF_L_WATT_MIN               -1.0

// Do not accept autonomous motor commands until the user explicitly configures
// an application through VESC Tool.
#define APPCONF_APP_TO_USE              APP_NONE

// Hard configuration ranges exposed to VESC Tool.
#define HW_LIM_CURRENT                  -0.1, 0.5
#define HW_LIM_CURRENT_IN               -0.02, 0.25
#define HW_LIM_CURRENT_ABS              0.0, 1.5
#define HW_LIM_VIN                      BLDC_HW_VIN_MIN, BLDC_HW_VIN_MAX
#define HW_LIM_ERPM                     -50000.0, 50000.0
#define HW_LIM_DUTY_MIN                 0.0, 0.05
#define HW_LIM_DUTY_MAX                 0.0, 0.05
#define HW_LIM_TEMP_FET                 -40.0, 110.0

#endif /* HW_BLDC_LITE_CORE_H_ */
