#ifndef GATE_DRIVER_H
#define GATE_DRIVER_H
#include <stdint.h>
typedef enum { GATE_OK = 0, GATE_UNSUPPORTED_HW = 1 } gate_result_t;
typedef enum { GATE_FAULT_UNAVAILABLE = 0 } gate_fault_t;
void gate_driver_init(void);
gate_result_t gate_driver_enable(void);
void gate_driver_disable(void);
gate_fault_t gate_driver_fault(void);
gate_result_t gate_driver_clear_fault(void);
#endif
