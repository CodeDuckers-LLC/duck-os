#include "drivers/uart.h"
#include "kernel/console.h"

static int console_is_backspace(char c)
{
    return c == '\b' || c == 0x7f;
}

void console_init(void)
{
}

void console_putc(char c)
{
    if (c == '\n')
    {
        uart_putc('\r');
    }

    uart_putc(c);
}

void console_write(const char *str)
{
    while (*str != '\0')
    {
        console_putc(*str);
        str++;
    }
}

unsigned long console_read_line(char *buffer, unsigned long max_len)
{
    unsigned long length;

    if (max_len == 0)
    {
        return 0;
    }

    length = 0;

    while (1)
    {
        char c;

        c = uart_getc();

        if (c == '\r' || c == '\n')
        {
            console_putc('\n');
            buffer[length] = '\0';
            return length;
        }

        if (console_is_backspace(c))
        {
            if (length != 0)
            {
                length--;
                console_write("\b \b");
            }
            continue;
        }

        if (length + 1 < max_len)
        {
            buffer[length] = c;
            length++;
            console_putc(c);
        }
    }
}
