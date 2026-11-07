#include "stm32l4xx.h"
#include "verify_core.h"
#include "pubkey.h"

#define SLOT_A_ADDR 0x08020000u
#define SLOT_SIZE   0x60000u

static void led_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 10)) | (1u << 10);
}

static void fail_forever(void)
{
    for (;;) {                                   /* rapid blink = rejected */
        GPIOA->ODR ^= (1u << 5);
        for (volatile uint32_t i = 0; i < 40000; i++) { }
    }
}

static void jump_to(uint32_t vector_table)
{
    uint32_t sp = *(volatile uint32_t *)vector_table;
    uint32_t pc = *(volatile uint32_t *)(vector_table + 4);

    if (sp < 0x20000000u || sp > 0x20018000u) return;   /* sanity check */

    __disable_irq();
    SysTick->CTRL = 0;
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->VTOR = vector_table;
    __DSB(); __ISB();
    __enable_irq();

    __asm volatile ("msr msp, %0\n\tbx %1" :: "r"(sp), "r"(pc));
}

int main(void)
{
    led_init();
    GPIOA->ODR |= (1u << 5);                     /* solid LED while verifying */

    img_info info;
    if (img_verify((const uint8_t *)SLOT_A_ADDR, SLOT_SIZE, SLOT_A_ADDR,
                   0, PUBKEY, &info) == IMG_OK) {
        GPIOA->ODR &= ~(1u << 5);
        jump_to(SLOT_A_ADDR + IMG_HEADER_SIZE);
    }
    fail_forever();
}
