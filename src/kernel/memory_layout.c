#include "kernel/klog.h"
#include "kernel/memory_layout.h"
#include "platform/platform.h"

extern char __kernel_start[];
extern char __kernel_end[];

static unsigned long first_free_phys;

static void memory_layout_init(void)
{
    if (first_free_phys == 0)
    {
        first_free_phys = (unsigned long)__kernel_end;
    }
}

unsigned long memory_layout_ram_start(void)
{
    return platform_get_ram_base();
}

unsigned long memory_layout_ram_end(void)
{
    return platform_get_ram_base() + platform_get_ram_size();
}

unsigned long memory_layout_kernel_start(void)
{
    return (unsigned long)__kernel_start;
}

unsigned long memory_layout_kernel_end(void)
{
    return (unsigned long)__kernel_end;
}

unsigned long memory_layout_first_free_phys(void)
{
    memory_layout_init();
    return first_free_phys;
}

void memory_layout_set_first_free_phys(unsigned long address)
{
    memory_layout_init();
    first_free_phys = address;
}

void memory_layout_print(void)
{
    klog_info("physical memory layout:");
    kprintf("  RAM start: %p\n", (void *)memory_layout_ram_start());
    kprintf("  RAM end: %p\n", (void *)memory_layout_ram_end());
    kprintf("  kernel start: %p\n", (void *)memory_layout_kernel_start());
    kprintf("  kernel end: %p\n", (void *)memory_layout_kernel_end());
    kprintf("  first free physical address: %p\n",
            (void *)memory_layout_first_free_phys());
}
