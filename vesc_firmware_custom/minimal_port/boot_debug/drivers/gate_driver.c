#include "gate_driver.h"
#include "board.h"
void gate_driver_init(void) { board_gate_safe_early(); }
void gate_driver_disable(void) { board_gate_safe_early(); }
gate_result_t gate_driver_enable(void) {
    board_gate_safe_early();
    return GATE_UNSUPPORTED_HW;
}
gate_fault_t gate_driver_fault(void) { return GATE_FAULT_UNAVAILABLE; }
gate_result_t gate_driver_clear_fault(void) {
    board_gate_safe_early();
    return GATE_UNSUPPORTED_HW;
}
