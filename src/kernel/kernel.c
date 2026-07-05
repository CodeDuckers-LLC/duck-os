#include "arch/aarch64/exceptions.h"
#include "arch/aarch64/gic.h"
#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysreg.h"
#include "arch/aarch64/cpu.h"
#include "kernel/console.h"
#include "kernel/initramfs.h"
#include "kernel/klog.h"
#include "kernel/shell.h"
#include "kernel/test.h"
#include "kernel/task.h"
#include "kernel/timer.h"
#include "platform/platform.h"
#include "mm/pmm.h"

static void print_hex32(unsigned long value)
{
    int shift;

    console_write("0x");

    for (shift = 28; shift >= 0; shift -= 4)
    {
        unsigned long digit;

        digit = (value >> shift) & 0xfUL;
        if (digit < 10)
        {
            console_putc((char)('0' + digit));
        }
        else
        {
            console_putc((char)('a' + (digit - 10)));
        }
    }
}

void kernel_main(void)
{
    unsigned long ram_start;
    unsigned long ram_end;

    console_init();
    sysreg_set_daif_irq();
    ram_start = platform_get_ram_base();
    ram_end = ram_start + platform_get_ram_size();
    klog_info("Custom Pi OS kernel booted");
    kprintf("platform: %s\n", platform_name());
    console_write("uart:     ");
    print_hex32(platform_get_uart0_base());
    console_putc('\n');
    console_write("ram:      ");
    print_hex32(ram_start);
    console_write(" - ");
    print_hex32(ram_end);
    console_putc('\n');
    klog_info("UART is working");
    exceptions_init();
    timer_init();
    mmu_init();
    pmm_init();
    initramfs_init();
    task_init();
    test_run_all();

    gic_init();
    gic_enable_irq(GIC_IRQ_TIMER_PHYSICAL_PPI);
    timer_start_periodic_ms(10);

    kprintf("timer interrupts active: %s, period 10 ms\n", gic_version_name());
    sysreg_clear_daif_irq();
    klog_info("IRQ unmasked");
    task_run_preemptive_demo();

    shell_run();
}
