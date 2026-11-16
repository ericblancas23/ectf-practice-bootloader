#include "stm32l4xx.h"
#include "layout.h"
#include "uart.h"
#include "flash.h"
#include "verify_core.h"
#include "update.h"
#include "pubkey.h"

#define CHUNK    256u
#define TIMEOUT  40000000u      /* polling loops: a few seconds at 80 MHz */

static uint8_t buf[CHUNK];      /* static, so it isn't on the stack */

__attribute__((noreturn))
static void finish(const char *msg)
{
    flash_lock();
    uart_puts(msg);
    uart_flush();
    NVIC_SystemReset();
}

static int recv_exact(uint8_t *dst, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        if (uart_getc(&dst[i], TIMEOUT) != 0) return -1;
    }
    return 0;
}

static uint32_t le32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

void update_run(uint32_t target, uint32_t floor)
{
    uart_puts(target == SLOT_A_ADDR ? "READY A\n" : "READY B\n");

    /* 1. Length, checked before anything touches flash. */
    uint8_t lenb[4];
    if (recv_exact(lenb, 4) != 0) finish("TIMEOUT\n");
    uint32_t len = le32(lenb);
    if (len < IMG_HEADER_SIZE + IMG_SIG_SIZE || len > SLOT_SIZE) finish("E LEN\n");

    /* 2. Erase first: an interrupted upload leaves the slot invalid, not half-valid. */
    flash_unlock();
    uint32_t pages = (len + PAGE_SIZE - 1u) / PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++) {
        if (flash_erase_page(target + p * PAGE_SIZE) != 0) finish("E ERASE\n");
    }
    uart_putc('K');

    /* 3. Chunks: receive, pad to 8 bytes, program, acknowledge. */
    uint32_t off = 0;
    while (off < len) {
        uint32_t n = len - off;
        if (n > CHUNK) n = CHUNK;
        if (recv_exact(buf, n) != 0) finish("TIMEOUT\n");

        uint32_t padded = (n + 7u) & ~7u;
        for (uint32_t i = n; i < padded; i++) buf[i] = 0xFF;

        for (uint32_t i = 0; i < padded; i += 8) {
            if (flash_program_u64(target + off + i, le32(&buf[i]), le32(&buf[i + 4])) != 0)
                finish("E WRITE\n");
        }
        off += n;
        GPIOA->ODR ^= (1u << 5);              /* LED flickers as chunks arrive */
        uart_putc('K');
    }
    flash_lock();

    /* 4. Verify what is actually in flash, not what we think we wrote. */
    img_info info;
    img_result r = img_verify((const uint8_t *)target, SLOT_SIZE, target,
                              floor, PUBKEY, &info);
    if (r == IMG_OK) finish("OK\n");

    static char fail[] = "FAIL 0\n";
    fail[5] = (char)('0' + (int)r);
    finish(fail);
}
