#include <stdarg.h>

#include "kernel/console.h"
#include "kernel/klog.h"

static void kput_unsigned(unsigned long value, unsigned int base)
{
    char buffer[2 * sizeof(unsigned long)];
    unsigned long i;

    if (value == 0) {
        console_putc('0');
        return;
    }

    i = 0;
    while (value != 0) {
        unsigned long digit;

        digit = value % base;
        if (digit < 10) {
            buffer[i++] = (char)('0' + digit);
        } else {
            buffer[i++] = (char)('a' + (digit - 10));
        }
        value /= base;
    }

    while (i > 0) {
        i--;
        console_putc(buffer[i]);
    }
}

static void kput_signed(int value)
{
    unsigned long magnitude;

    if (value < 0) {
        console_putc('-');
        magnitude = (unsigned long)(-(value + 1)) + 1;
    } else {
        magnitude = (unsigned long)value;
    }

    kput_unsigned(magnitude, 10);
}

void kprintf(const char *format, ...)
{
    va_list args;

    va_start(args, format);

    while (*format != '\0') {
        if (*format != '%') {
            console_putc(*format);
            format++;
            continue;
        }

        format++;
        if (*format == '\0') {
            console_putc('%');
            break;
        }

        switch (*format) {
        case 's': {
            const char *str;

            str = va_arg(args, const char *);
            if (str == 0) {
                str = "(null)";
            }

            while (*str != '\0') {
                console_putc(*str);
                str++;
            }
            break;
        }
        case 'c':
            console_putc((char)va_arg(args, int));
            break;
        case 'd':
            kput_signed(va_arg(args, int));
            break;
        case 'u':
            kput_unsigned((unsigned long)va_arg(args, unsigned int), 10);
            break;
        case 'x':
            kput_unsigned((unsigned long)va_arg(args, unsigned int), 16);
            break;
        case 'p':
            console_putc('0');
            console_putc('x');
            kput_unsigned((unsigned long)va_arg(args, void *), 16);
            break;
        case '%':
            console_putc('%');
            break;
        default:
            console_putc('%');
            console_putc(*format);
            break;
        }

        format++;
    }

    va_end(args);
}

void klog_info(const char *message)
{
    kprintf("[INFO] %s\n", message);
}

void klog_error(const char *message)
{
    kprintf("[ERROR] %s\n", message);
}
