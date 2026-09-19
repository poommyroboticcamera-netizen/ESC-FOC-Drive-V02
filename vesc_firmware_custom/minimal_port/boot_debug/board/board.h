#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include <stdbool.h>
/* Called before initialized data exists; uses no globals or C runtime. */
void board_gate_safe_early(void);
void board_init_inputs(void);
bool board_clock_init_hsi(void);
uint32_t board_gate_levels(void);
#endif
