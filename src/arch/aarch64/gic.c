#include "arch/aarch64/gic.h"

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

#define GICD_CTLR ((volatile unsigned int *)(GICD_BASE + 0x000))
#define GICD_ISENABLER0 ((volatile unsigned int *)(GICD_BASE + 0x100))

#define GICC_CTLR ((volatile unsigned int *)(GICC_BASE + 0x0000))
#define GICC_PMR ((volatile unsigned int *)(GICC_BASE + 0x0004))
#define GICC_IAR ((volatile unsigned int *)(GICC_BASE + 0x000c))
#define GICC_EOIR ((volatile unsigned int *)(GICC_BASE + 0x0010))

void gic_init(void)
{
    *GICD_CTLR = 0;
    *GICC_CTLR = 0;

    *GICC_PMR = 0xffU;

    *GICD_CTLR = 0x1U;
    *GICC_CTLR = 0x1U;
}

void gic_enable_irq(unsigned int irq_id)
{
    *GICD_ISENABLER0 = (1U << irq_id);
}

unsigned int gic_acknowledge_irq(void)
{
    return *GICC_IAR;
}

void gic_end_irq(unsigned int irq_id)
{
    *GICC_EOIR = irq_id;
}
