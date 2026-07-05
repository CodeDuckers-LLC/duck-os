#include "arch/aarch64/cpu.h"
#include "kernel/console.h"
#include "kernel/panic.h"

void panic(const char *message)
{
    console_write("[PANIC] ");
    console_write(message);
    console_putc('\n');

    cpu_wait_forever();
}
