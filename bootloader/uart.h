#pragma once
#include <stdint.h>

void uart_init(void);
void uart_putc(uint8_t c);
void uart_puts(const char *s);
void uart_flush(void);
int  uart_getc(uint8_t *c, uint32_t timeout_loops);   /* 0 = ok, -1 = timeout */
