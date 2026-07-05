#ifndef ARCH_AARCH64_GIC_H
#define ARCH_AARCH64_GIC_H

#define GIC_IRQ_TIMER_PHYSICAL_PPI 30U
#define GIC_IRQ_SPURIOUS 1023U

void gic_init(void);
void gic_enable_irq(unsigned int irq_id);
unsigned int gic_acknowledge_irq(void);
void gic_end_irq(unsigned int irq_id);

#endif
