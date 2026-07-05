#include "platform/platform.h"

#define QEMU_VIRT_UART0_BASE 0x09000000UL
#define QEMU_VIRT_RAM_BASE 0x40000000UL
#define QEMU_VIRT_RAM_SIZE (128UL * 1024UL * 1024UL)

const char *platform_name(void)
{
    return "qemu-virt";
}

unsigned long platform_get_uart0_base(void)
{
    return QEMU_VIRT_UART0_BASE;
}

unsigned long platform_get_ram_base(void)
{
    return QEMU_VIRT_RAM_BASE;
}

unsigned long platform_get_ram_size(void)
{
    return QEMU_VIRT_RAM_SIZE;
}
