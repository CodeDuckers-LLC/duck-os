#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

void uart_putc(char c);
void uart_puts(const char *str);
int uart_can_read(void);
char uart_getc(void);

#endif
