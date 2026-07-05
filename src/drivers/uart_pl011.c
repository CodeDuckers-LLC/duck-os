#include "drivers/uart.h"
#include "platform/platform.h"

static volatile unsigned int *uart_dr(void)
{
    return (volatile unsigned int *)(platform_get_uart0_base() + 0x00);
}

static volatile unsigned int *uart_fr(void)
{
    return (volatile unsigned int *)(platform_get_uart0_base() + 0x18);
}

#define UART_FR_TXFF (1 << 5)
#define UART_FR_RXFE (1 << 4)

void uart_putc(char c)
{
    while (*uart_fr() & UART_FR_TXFF) {
    }

    *uart_dr() = (unsigned int)c;
}

void uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');
        }

        uart_putc(*str);
        str++;
    }
}

int uart_can_read(void)
{
    return (*uart_fr() & UART_FR_RXFE) == 0;
}

char uart_getc(void)
{
    while (!uart_can_read()) {
    }

    return (char)(*uart_dr() & 0xffU);
}
