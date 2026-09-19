#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/* PDF 2026-09-04 + Gerber V02 2026-09-07. No VESC source copied. */
#define BOARD_GATE_A_MASK ((1u << 8) | (1u << 9) | (1u << 10))
#define BOARD_GATE_B_MASK ((1u << 13) | (1u << 14) | (1u << 15))
#define BOARD_BUTTON_MASK ((1u << 4) | (1u << 5))
#define HW_HAS_PHASE_FILTER 0
#define HW_HAS_DIRECT_VBUS 0
#define HW_HAS_FET_TEMPERATURE 0
#define HW_HAS_DRIVER_FAULT_INPUT 0
#define HW_HAS_GATE_ENABLE_PIN 0
#define HW_HAS_TIMER_BREAK_ROUTE 0
#define BOARD_SHUNT_OHMS 0.001f
#define BOARD_CURRENT_GAIN 20.0f

/* This milestone never accepts a power-stage enable build option. */
#if defined(ENABLE_MOTOR) || defined(ENABLE_PWM)
#error "boot_debug cannot enable a motor: complete hardware and bring-up review first"
#endif
#endif
