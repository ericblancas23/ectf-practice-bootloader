#include "stm32l4xx.h"

int main(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 10)) | (1u << 10); /* PA5 = output */

    for (;;) {
        GPIOA->ODR ^= (1u << 5);                  /* toggle LD2 */
        for (volatile uint32_t i = 0; i < 500000; i++) { }
    }
}