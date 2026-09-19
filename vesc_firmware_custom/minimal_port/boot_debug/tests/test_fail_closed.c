#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "registers.h"
#include "board.h"
#include "board_pins.h"
#include "gate_driver.h"
#include "debug_mailbox.h"

static uint32_t rcc[32], ga[16], gb[16], gc[16], tim[32];
volatile uint32_t *test_register(uintptr_t a) {
    if (a >= RCC_BASE && a < RCC_BASE+sizeof(rcc)) return &rcc[(a-RCC_BASE)/4];
    if (a >= GPIOA_BASE && a < GPIOA_BASE+sizeof(ga)) return &ga[(a-GPIOA_BASE)/4];
    if (a >= GPIOB_BASE && a < GPIOB_BASE+sizeof(gb)) return &gb[(a-GPIOB_BASE)/4];
    if (a >= GPIOC_BASE && a < GPIOC_BASE+sizeof(gc)) return &gc[(a-GPIOC_BASE)/4];
    if (a >= TIM1_BASE && a < TIM1_BASE+sizeof(tim)) return &tim[(a-TIM1_BASE)/4];
    assert(!"unexpected MMIO register");
    return 0;
}
static void dangerous_state(void) {
    TIM1_BDTR = 0xc000u;
    TIM1_CCER = 0x555u;
    TIM1_CR1 = 1u;
    GPIO_MODER(GPIOA_BASE) = 0xffffffffu;
    GPIO_MODER(GPIOB_BASE) = 0xffffffffu;
    GPIO_OTYPER(GPIOA_BASE) = 0xffffu;
    GPIO_OTYPER(GPIOB_BASE) = 0xffffu;
}
static void assert_disabled(void) {
    assert((TIM1_BDTR & 0xc000u) == 0u);
    assert(TIM1_CCER == 0u && !(TIM1_CR1 & 1u));
    assert(GPIO_BSRR(GPIOA_BASE) == (BOARD_GATE_A_MASK << 16));
    assert(GPIO_BSRR(GPIOB_BASE) == (BOARD_GATE_B_MASK << 16));
    for (unsigned pin=8; pin<=10; ++pin) assert(((GPIO_MODER(GPIOA_BASE)>>(2*pin))&3u)==1u);
    for (unsigned pin=13; pin<=15; ++pin) assert(((GPIO_MODER(GPIOB_BASE)>>(2*pin))&3u)==1u);
    /* Shutdown must not remux PA13/14 SWD pins. */
    assert(((GPIO_MODER(GPIOA_BASE)>>26)&15u)==15u);
}
int main(void) {
    memset((void *)&g_debug,0,sizeof(g_debug));
    debug_mailbox_init(0x1234u);
    assert(g_debug.reset_cause==0x1234u && g_debug.blockers==15u);
    dangerous_state();
    assert(gate_driver_enable()==GATE_UNSUPPORTED_HW);
    assert_disabled();
    assert(gate_driver_fault()==GATE_FAULT_UNAVAILABLE);
    /* Exercise all opcodes, with both valid and malformed arguments. */
    for (unsigned arg=0;arg<2;++arg) for (unsigned cmd=0;cmd<256;++cmd) {
        dangerous_state();
        g_debug.command=cmd;
        g_debug.argument=arg;
        ++g_debug.request_seq;
        debug_mailbox_poll();
        assert_disabled();
        unsigned expected=REPLY_BAD_COMMAND;
        if (arg==0 && (cmd==CMD_PING || cmd==CMD_STOP)) expected=REPLY_OK;
        if (arg==0 && (cmd==CMD_ARM || cmd==CMD_CLEAR_FAULT)) expected=REPLY_BLOCKED;
        assert(g_debug.response==expected);
        assert(g_debug.response_seq==g_debug.request_seq);
        assert(g_debug.blockers==15u);
    }
    assert(g_debug.arm_rejections==1u);
    const uint32_t seq=g_debug.response_seq;
    debug_mailbox_poll();
    assert(g_debug.response_seq==seq && g_debug.arm_rejections==1u);
    puts("PASS: 512 command/argument cases, fail-closed gate enable, shutdown from active TIM1, SWD preserved");
    return 0;
}
