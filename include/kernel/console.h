#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

void console_init(void);
void console_putc(char c);
void console_write(const char *str);
unsigned long console_read_line(char *buffer, unsigned long max_len);

#endif
