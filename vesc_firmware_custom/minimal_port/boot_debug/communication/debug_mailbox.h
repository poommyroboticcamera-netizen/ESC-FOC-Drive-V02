#ifndef DEBUG_MAILBOX_H
#define DEBUG_MAILBOX_H
#include <stdint.h>
enum { DEBUG_MAGIC = 0x42444731u, DEBUG_ABI = 1u };
enum { CMD_PING=1, CMD_STOP=2, CMD_ARM=3, CMD_CLEAR_FAULT=4 };
enum { REPLY_NONE=0, REPLY_OK=1, REPLY_BLOCKED=2, REPLY_BAD_COMMAND=3 };
enum { DEBUG_BOOTING=0, DEBUG_READY=1, DEBUG_FAULT=2 };
enum { BLOCK_NO_VBUS=1u, BLOCK_NO_FET_TEMP=2u,
       BLOCK_NO_HW_OC=4u, BLOCK_BRINGUP_UNVERIFIED=8u };
typedef struct {
    uint32_t magic, abi, uptime_ms, state;
    uint32_t reset_cause, blockers, gate_levels, buttons;
    uint32_t heartbeat, fault_code, request_seq, command;
    uint32_t argument, response_seq, response, arm_rejections;
} debug_mailbox_t;
extern volatile debug_mailbox_t g_debug;
void debug_mailbox_init(uint32_t reset_cause);
void debug_mailbox_poll(void);
#endif
