#include "board.h"
#include "board_pins.h"
#include "registers.h"

static void force_low(uintptr_t port, uint32_t mask) {
    /* Load the LOW output latch before changing the pin mux to GPIO. */
    GPIO_BSRR(port) = mask << 16;
    GPIO_OTYPER(port) &= ~mask;
    uint32_t mode = GPIO_MODER(port);
    uint32_t pull = GPIO_PUPDR(port);
    for (unsigned pin = 0; pin < 16; ++pin) {
        if (mask & (1u << pin)) {
            mode = (mode & ~(3u << (pin * 2))) | (1u << (pin * 2));
            pull &= ~(3u << (pin * 2));
        }
    }
    GPIO_PUPDR(port) = pull;
    GPIO_MODER(port) = mode;
    MEMORY_BARRIER();
}

void board_gate_safe_early(void) {
    RCC_AHB1ENR |= 7u;
    (void)RCC_AHB1ENR;
    RCC_APB2ENR |= 1u;
    (void)RCC_APB2ENR;
    TIM1_BDTR &= ~((1u << 15) | (1u << 14)); /* MOE and AOE off */
    TIM1_CCER = 0u;
    TIM1_CR1 &= ~1u;
    force_low(GPIOA_BASE, BOARD_GATE_A_MASK);
    force_low(GPIOB_BASE, BOARD_GATE_B_MASK);
}

void board_init_inputs(void) {
    GPIO_MODER(GPIOC_BASE) &= ~((3u << 8) | (3u << 10));
    GPIO_PUPDR(GPIOC_BASE) =
        (GPIO_PUPDR(GPIOC_BASE) & ~((3u << 8) | (3u << 10))) |
        (1u << 8) | (1u << 10);
}

bool board_clock_init_hsi(void) {
    RCC_CR |= 1u;
    unsigned remaining = 1000000u;
    while (!(RCC_CR & 2u) && --remaining) { }
    if (!remaining) return false;
    /* HSI, AHB/APB undivided. HSE/PLL performance target comes later. */
    RCC_CFGR &= ~3u; /* Select HSI before changing bus prescalers. */
    remaining = 1000000u;
    while ((RCC_CFGR & 0xcu) && --remaining) { }
    if (!remaining) return false;
    RCC_CFGR = 0u;
    return true;
}

uint32_t board_gate_levels(void) {
    return ((GPIO_IDR(GPIOA_BASE) >> 8) & 7u) |
           (((GPIO_IDR(GPIOB_BASE) >> 13) & 7u) << 3);
}
