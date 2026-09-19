#include "debug_mailbox.h"
#include "gate_driver.h"
#include "registers.h"

#ifndef PORTING_HOST_TEST
__attribute__((section(".board_mailbox"), used))
#endif
volatile debug_mailbox_t g_debug;

void debug_mailbox_init(uint32_t reset_cause) {
    g_debug.magic = DEBUG_MAGIC;
    g_debug.abi = DEBUG_ABI;
    g_debug.state = DEBUG_BOOTING;
    g_debug.reset_cause = reset_cause;
    g_debug.blockers = BLOCK_NO_VBUS | BLOCK_NO_FET_TEMP |
                       BLOCK_NO_HW_OC | BLOCK_BRINGUP_UNVERIFIED;
}

void debug_mailbox_poll(void) {
    const uint32_t seq = g_debug.request_seq;
    if (seq == g_debug.response_seq) return;
    MEMORY_BARRIER();
    const uint32_t command = g_debug.command;
    const uint32_t argument = g_debug.argument;
    uint32_t response = REPLY_BAD_COMMAND;
    /* No command may energize the board, including malformed requests. */
    gate_driver_disable();
    if (argument == 0u) {
        switch (command) {
        case CMD_PING:
        case CMD_STOP:
            response = REPLY_OK;
            break;
        case CMD_ARM:
            (void)gate_driver_enable();
            ++g_debug.arm_rejections;
            response = REPLY_BLOCKED;
            break;
        case CMD_CLEAR_FAULT:
            (void)gate_driver_clear_fault();
            response = REPLY_BLOCKED;
            break;
        default:
            break;
        }
    }
    g_debug.response = response;
    MEMORY_BARRIER();
    g_debug.response_seq = seq;
}
