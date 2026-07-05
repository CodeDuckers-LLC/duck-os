#include "arch/aarch64/sysreg.h"
#include "kernel/klog.h"
#include "kernel/timer.h"

static unsigned long timer_freq;
static unsigned long timer_period_ticks;
static unsigned long timer_irq_ticks;
static unsigned long timer_boot_ticks;

#define CNTP_CTL_ENABLE (1UL << 0)

void timer_init(void)
{
    if (timer_freq != 0)
    {
        return;
    }

    timer_freq = sysreg_read_cntfrq_el0();
    timer_boot_ticks = sysreg_read_cntpct_el0();
    kprintf("[INFO] timer: CNTFRQ_EL0=%u Hz\n", (unsigned int)timer_freq);
}

unsigned long timer_frequency(void)
{
    timer_init();
    return timer_freq;
}

unsigned long timer_now_ticks(void)
{
    timer_init();
    return sysreg_read_cntpct_el0();
}

void timer_start_periodic_ms(unsigned long period_ms)
{
    unsigned long period_ticks;

    timer_init();

    period_ticks = (timer_freq * period_ms) / 1000UL;
    if (period_ticks == 0)
    {
        period_ticks = 1;
    }

    timer_period_ticks = period_ticks;
    sysreg_write_cntp_tval_el0(period_ticks);
    sysreg_write_cntp_ctl_el0(CNTP_CTL_ENABLE);
}

unsigned long timer_irq_count(void)
{
    return timer_irq_ticks;
}

unsigned long timer_uptime_ms(void)
{
    unsigned long elapsed_ticks;

    timer_init();

    elapsed_ticks = timer_now_ticks() - timer_boot_ticks;
    return (elapsed_ticks * 1000UL) / timer_freq;
}

void timer_handle_irq(void)
{
    timer_irq_ticks++;

    if (timer_period_ticks != 0)
    {
        sysreg_write_cntp_tval_el0(timer_period_ticks);
        sysreg_write_cntp_ctl_el0(CNTP_CTL_ENABLE);
    }
}

void timer_busy_wait_ms(unsigned long ms)
{
    unsigned long start;
    unsigned long wait_ticks;

    if (ms == 0)
    {
        return;
    }

    timer_init();

    wait_ticks = (timer_freq * ms) / 1000UL;
    if (wait_ticks == 0)
    {
        wait_ticks = 1;
    }

    start = timer_now_ticks();
    while ((timer_now_ticks() - start) < wait_ticks)
    {
    }
}
