#include "stm32l4xx.h"
#include "verify_core.h"
#include "flash.h"
#include "pubkey.h"

#define SLOT_A_ADDR      0x08020000u
#define SLOT_B_ADDR      0x08080000u
#define SLOT_SIZE        0x60000u

#define ROLLBACK_ADDR    0x08010000u
#define ROLLBACK_SIZE    0x800u
#define ROLLBACK_ENTRIES (ROLLBACK_SIZE / 8u)

/* ---------------- clocks ---------------- */

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

/* ---------------- LED ---------------- */

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

/* ---------------- rollback floor ---------------- */

static uint32_t floor_read(void)
{
    const volatile uint32_t *log = (const volatile uint32_t *)ROLLBACK_ADDR;
    uint32_t floor = 0;

    for (uint32_t i = 0; i < ROLLBACK_ENTRIES; i++) {
        uint32_t v  = log[2 * i];
        uint32_t nv = log[2 * i + 1];
        if (v == 0xFFFFFFFFu && nv == 0xFFFFFFFFu) break;   /* end of log */
        if (v == ~nv && v > floor) floor = v;               /* skip corrupt entries */
    }
    return floor;
}

static int floor_raise(uint32_t version)
{
    const volatile uint32_t *log = (const volatile uint32_t *)ROLLBACK_ADDR;
    uint32_t i;

    for (i = 0; i < ROLLBACK_ENTRIES; i++) {
        if (log[2 * i] == 0xFFFFFFFFu && log[2 * i + 1] == 0xFFFFFFFFu) break;
    }

    flash_unlock();
    int r = 0;
    if (i == ROLLBACK_ENTRIES) {          /* log full: start over (weak spot, see Phase 9) */
        r = flash_erase_page(ROLLBACK_ADDR);
        i = 0;
    }
    if (r == 0) r = flash_program_u64(ROLLBACK_ADDR + 8u * i, version, ~version);
    flash_lock();

    if (r == 0 && floor_read() < version) r = -1;   /* read back to confirm */
    return r;
}

/* ---------------- boot ---------------- */

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

static int slot_valid(uint32_t base, uint32_t min_version, img_info *info)
{
    return img_verify((const uint8_t *)base, SLOT_SIZE, base,
                      min_version, PUBKEY, info) == IMG_OK;
}

int main(void)
{
    led_init();
    GPIOA->ODR |= (1u << 5);
    clock_80mhz();

    uint32_t floor = floor_read();

    img_info a, b;
    int a_ok = slot_valid(SLOT_A_ADDR, floor, &a);
    int b_ok = slot_valid(SLOT_B_ADDR, floor, &b);

    uint32_t boot = 0, version = 0;
    if (a_ok && (!b_ok || a.version >= b.version)) {
        boot = SLOT_A_ADDR; version = a.version;
    } else if (b_ok) {
        boot = SLOT_B_ADDR; version = b.version;
    }

    if (boot != 0) {
        if (version > floor) {
            (void)floor_raise(version);   /* image is already verified; boot even if this fails */
        }
        GPIOA->ODR &= ~(1u << 5);
        jump_to(boot + IMG_HEADER_SIZE);
    }
    fail_forever();
}
