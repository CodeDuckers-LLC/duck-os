#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

unsigned long platform_get_uart0_base(void);
unsigned long platform_get_ram_base(void);
unsigned long platform_get_ram_size(void);
const char *platform_name(void);

#endif
