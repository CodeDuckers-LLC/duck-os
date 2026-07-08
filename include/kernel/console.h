#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include "gfx/framebuffer.h"

#define CONSOLE_SINK_SERIAL 0x1U
#define CONSOLE_SINK_GRAPHICS 0x2U

typedef void (*console_output_capture_fn_t)(char ch, void *user_data);

void console_init(void);
void console_putc(char c);
void console_write(const char *str);
void console_write_len(const char *buffer, unsigned long length);
void console_flush(void);
void console_putc_unlocked(char c);
void console_write_unlocked(const char *str);
unsigned long console_read_line(char *buffer, unsigned long max_len);
void console_attach_graphics(framebuffer_t *fb);
framebuffer_t *console_graphics_framebuffer(void);
void console_set_output_mode(unsigned int mode);
unsigned int console_output_mode(void);
void console_set_input_mode(unsigned int mode);
unsigned int console_input_mode(void);
void console_set_output_capture(console_output_capture_fn_t callback, void *user_data);

#endif
