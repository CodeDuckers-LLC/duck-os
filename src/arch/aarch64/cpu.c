#include "arch/aarch64/cpu.h"

void cpu_wait_for_interrupt(void)
{
    asm volatile("wfi");
}

void cpu_wait_forever(void)
{
    while (1)
    {
        cpu_wait_for_interrupt();
    }
}
