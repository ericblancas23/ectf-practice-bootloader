#include "stm32l4xx.h"
#include "uart.h"

void uart_init(void)
{
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
    (void)RCC->APB1ENR1;                       /* let the clock settle */

    /* PA2 = TX, PA3 = RX: alternate function mode, AF7 */
    GPIOA->MODER = (GPIOA->MODER & ~((3u << 4) | (3u << 6))) | (2u << 4) | (2u << 6);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~((0xFu << 8) | (0xFu << 12))) | (7u << 8) | (7u << 12);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3u << 6)) | (1u << 6);   /* pull-up on RX */

    USART2->CR1 = 0;
    USART2->BRR = 80000000u / 115200u;         /* 694 at 80 MHz */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void uart_putc(uint8_t c)
{
    while (!(USART2->ISR & USART_ISR_TXE)) { }
    USART2->TDR = c;
}

void uart_puts(const char *s)
{
    while (*s) uart_putc((uint8_t)*s++);
}

void uart_flush(void)
{
    while (!(USART2->ISR & USART_ISR_TC)) { }
}

int uart_getc(uint8_t *c, uint32_t timeout_loops)
{
    while (timeout_loops--) {
        uint32_t isr = USART2->ISR;
        if (isr & USART_ISR_ORE) USART2->ICR = USART_ICR_ORECF;   /* clear overrun */
        if (isr & USART_ISR_RXNE) {
            *c = (uint8_t)USART2->RDR;
            return 0;
        }
    }
    return -1;
}
