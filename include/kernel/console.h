#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

void console_init(void);
void console_putc(char c);
void console_write(const char *str);
void console_write_len(const char *buffer, unsigned long length);
void console_putc_unlocked(char c);
void console_write_unlocked(const char *str);
unsigned long console_read_line(char *buffer, unsigned long max_len);

#endif
