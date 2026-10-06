#include <stdint.h>

#define RCC_AHB1ENR   (*(volatile uint32_t *)0x40023830UL)
#define GPIOA_MODER   (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_ODR     (*(volatile uint32_t *)0x40020014UL)

int main(void)
{
    /* Enable GPIOA clock */
    RCC_AHB1ENR |= (1U << 0);

    /* Configure PA5 as output */
    GPIOA_MODER &= ~(3U << (5 * 2));
    GPIOA_MODER |=  (1U << (5 * 2));

    while (1)
    {
        GPIOA_ODR ^= (1U << 5);

        for (volatile uint32_t i = 0; i < 1000000; i++)
        {
        }
    }
}