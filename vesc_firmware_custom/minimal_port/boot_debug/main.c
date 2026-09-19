#include "board.h"
#include "board_pins.h"
#include "registers.h"
#include "gate_driver.h"
#include "debug_mailbox.h"

void Default_Handler(void) {
    board_gate_safe_early();
    g_debug.state = DEBUG_FAULT;
    g_debug.fault_code = 1u;
    for (;;) { board_gate_safe_early(); }
}

int main(void) {
    gate_driver_init();
    debug_mailbox_init(RCC_CSR);
    if (!board_clock_init_hsi()) {
        g_debug.fault_code = 2u;
        g_debug.state = DEBUG_FAULT;
        for (;;) { gate_driver_disable(); }
    }
    board_init_inputs();
    SCB_VTOR = 0x08000000u;
    SYST_RVR = 15999u; /* nominal HSI 16MHz, 1ms; no SysTick IRQ */
    SYST_CVR = 0u;
    SYST_CSR = 5u;
    g_debug.state = DEBUG_READY; /* debug ready; never MOTOR_READY */
    for (;;) {
        gate_driver_disable();
        debug_mailbox_poll();
        g_debug.gate_levels = board_gate_levels();
        g_debug.buttons = (~GPIO_IDR(GPIOC_BASE) >> 4) & 3u;
        if (g_debug.gate_levels != 0u) {
            g_debug.fault_code = 3u;
            g_debug.state = DEBUG_FAULT;
        }
        if (SYST_CSR & (1u << 16)) {
            ++g_debug.uptime_ms;
            ++g_debug.heartbeat;
        }
    }
}
