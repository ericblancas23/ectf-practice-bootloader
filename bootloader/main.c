#include "stm32l4xx.h"
#include "verify_core.h"
#include "pubkey.h"

#define SLOT_A_ADDR 0x08020000u
#define SLOT_B_ADDR 0x08080000u
#define SLOT_SIZE   0x60000u
#define MIN_VERSION 0u          /* replaced by the rollback counter in Part 2 */

static void clock_80mhz(void)
{
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_4WS
               | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS) { }

    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_MSI
                 | (0u  << RCC_PLLCFGR_PLLM_Pos)
                 | (40u << RCC_PLLCFGR_PLLN_Pos)
                 | RCC_PLLCFGR_PLLREN;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
}

static void clock_restore(void)
{
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_MSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_MSI) { }

    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { }
    RCC->PLLCFGR = 0x00001000u;

    FLASH->ACR &= ~FLASH_ACR_LATENCY;
}

static void led_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 10)) | (1u << 10);
}

static void fail_forever(void)
{
    for (;;) {
        GPIOA->ODR ^= (1u << 5);
        for (volatile uint32_t i = 0; i < 800000; i++) { }
    }
}

static void jump_to(uint32_t vector_table)
{
    uint32_t sp = *(volatile uint32_t *)vector_table;
    uint32_t pc = *(volatile uint32_t *)(vector_table + 4);

    if (sp < 0x20000000u || sp > 0x20018000u) return;

    __disable_irq();
    SysTick->CTRL = 0;
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    clock_restore();
    SCB->VTOR = vector_table;
    __DSB(); __ISB();
    __enable_irq();

    __asm volatile ("msr msp, %0\n\tbx %1" :: "r"(sp), "r"(pc));
}

static int slot_valid(uint32_t base, img_info *info)
{
    return img_verify((const uint8_t *)base, SLOT_SIZE, base,
                      MIN_VERSION, PUBKEY, info) == IMG_OK;
}

int main(void)
{
    led_init();
    GPIOA->ODR |= (1u << 5);
    clock_80mhz();

    img_info a, b;
    int a_ok = slot_valid(SLOT_A_ADDR, &a);
    int b_ok = slot_valid(SLOT_B_ADDR, &b);

    uint32_t boot = 0;
    if (a_ok && b_ok) {
        boot = (b.version > a.version) ? SLOT_B_ADDR : SLOT_A_ADDR;  /* tie -> A */
    } else if (a_ok) {
        boot = SLOT_A_ADDR;
    } else if (b_ok) {
        boot = SLOT_B_ADDR;
    }

    if (boot != 0) {
        GPIOA->ODR &= ~(1u << 5);
        jump_to(boot + IMG_HEADER_SIZE);
    }
    fail_forever();
}
