#ifndef ARCH_AARCH64_GIC_H
#define ARCH_AARCH64_GIC_H

#define GIC_IRQ_TIMER_PHYSICAL_PPI 30U
#define GIC_IRQ_SPURIOUS 1023U
#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

const char *gic_version_name(void);
void gic_init(void);
void gic_enable_irq(unsigned int irq_id);
unsigned int gic_acknowledge_irq(void);
void gic_end_irq(unsigned int irq_id);

#endif
