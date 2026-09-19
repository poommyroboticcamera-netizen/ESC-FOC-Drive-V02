#ifndef REGISTERS_H
#define REGISTERS_H
#include <stdint.h>
#include <stddef.h>
#ifdef PORTING_HOST_TEST
volatile uint32_t *test_register(uintptr_t address);
#define REG32(address) (*test_register((uintptr_t)(address)))
#define MEMORY_BARRIER() ((void)0)
#else
#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define MEMORY_BARRIER() __asm volatile ("dsb" ::: "memory")
#endif
#define RCC_BASE       0x40023800u
#define RCC_CR         REG32(RCC_BASE + 0x00u)
#define RCC_CFGR       REG32(RCC_BASE + 0x08u)
#define RCC_AHB1ENR    REG32(RCC_BASE + 0x30u)
#define RCC_APB2ENR    REG32(RCC_BASE + 0x44u)
#define RCC_CSR        REG32(RCC_BASE + 0x74u)
#define GPIOA_BASE     0x40020000u
#define GPIOB_BASE     0x40020400u
#define GPIOC_BASE     0x40020800u
#define GPIO_MODER(p)  REG32((p) + 0x00u)
#define GPIO_OTYPER(p) REG32((p) + 0x04u)
#define GPIO_PUPDR(p)  REG32((p) + 0x0cu)
#define GPIO_IDR(p)    REG32((p) + 0x10u)
#define GPIO_BSRR(p)   REG32((p) + 0x18u)
#define TIM1_BASE      0x40010000u
#define TIM1_CR1       REG32(TIM1_BASE + 0x00u)
#define TIM1_CCER      REG32(TIM1_BASE + 0x20u)
#define TIM1_BDTR      REG32(TIM1_BASE + 0x44u)
#define SYST_CSR       REG32(0xe000e010u)
#define SYST_RVR       REG32(0xe000e014u)
#define SYST_CVR       REG32(0xe000e018u)
#define SCB_VTOR       REG32(0xe000ed08u)
#endif
