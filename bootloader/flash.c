#include "stm32l4xx.h"
#include "flash.h"

#define FLASH_KEY1 0x45670123u
#define FLASH_KEY2 0xCDEF89ABu
#define FLASH_SR_ERRORS (FLASH_SR_OPTVERR | FLASH_SR_RDERR  | FLASH_SR_FASTERR | \
                         FLASH_SR_MISERR  | FLASH_SR_PGSERR | FLASH_SR_SIZERR  | \
                         FLASH_SR_PGAERR  | FLASH_SR_WRPERR | FLASH_SR_PROGERR | \
                         FLASH_SR_OPERR)

static void wait_ready(void)
{
    while (FLASH->SR & FLASH_SR_BSY) { }
}

static void clear_flags(void)
{
    FLASH->SR = FLASH_SR_ERRORS | FLASH_SR_EOP;
}

static int finish(void)
{
    wait_ready();
    uint32_t sr = FLASH->SR;
    clear_flags();
    return (sr & FLASH_SR_ERRORS) ? -1 : 0;
}

static void dcache_reset(void)
{
    FLASH->ACR &= ~FLASH_ACR_DCEN;
    FLASH->ACR |=  FLASH_ACR_DCRST;
    FLASH->ACR &= ~FLASH_ACR_DCRST;
    FLASH->ACR |=  FLASH_ACR_DCEN;
}

void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

int flash_erase_page(uint32_t addr)
{
    if (addr < FLASH_BASE || addr >= FLASH_BASE + 0x100000u) return -1;

    uint32_t offset = addr - FLASH_BASE;
    uint32_t bank2  = (offset >= 0x80000u);
    uint32_t page   = (offset & 0x7FFFFu) / 0x800u;

    wait_ready();
    clear_flags();

    FLASH->CR = (FLASH->CR & ~(FLASH_CR_PNB | FLASH_CR_BKER | FLASH_CR_PG |
                               FLASH_CR_MER1 | FLASH_CR_MER2))
              | FLASH_CR_PER
              | (page << FLASH_CR_PNB_Pos)
              | (bank2 ? FLASH_CR_BKER : 0u);
    FLASH->CR |= FLASH_CR_STRT;

    int r = finish();
    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_PNB | FLASH_CR_BKER);
    dcache_reset();
    return r;
}

int flash_program_u64(uint32_t addr, uint32_t lo, uint32_t hi)
{
    if (addr & 7u) return -1;

    wait_ready();
    clear_flags();

    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint32_t *)addr = lo;
    __ISB();
    *(volatile uint32_t *)(addr + 4u) = hi;

    int r = finish();
    FLASH->CR &= ~FLASH_CR_PG;
    dcache_reset();
    return r;
}
