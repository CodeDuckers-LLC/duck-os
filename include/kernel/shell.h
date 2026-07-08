#ifndef KERNEL_SHELL_H
#define KERNEL_SHELL_H

typedef void (*shell_output_callback_t)(const char *text, unsigned long length, void *user_data);

void shell_run(void);
void shell_execute_line(const char *command, shell_output_callback_t output_callback, void *user_data);

#endif
