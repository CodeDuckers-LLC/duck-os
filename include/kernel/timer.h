#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

void timer_init(void);
unsigned long timer_frequency(void);
unsigned long timer_now_ticks(void);
void timer_busy_wait_ms(unsigned long ms);
void timer_start_periodic_ms(unsigned long period_ms);
unsigned long timer_irq_count(void);
unsigned long timer_uptime_ms(void);
void timer_handle_irq(void);

#endif
