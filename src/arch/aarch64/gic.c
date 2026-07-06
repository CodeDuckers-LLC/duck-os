#include "arch/aarch64/gic.h"

#define GICD_CTLR ((volatile unsigned int *)(GICD_BASE + 0x000))
#define GICD_ISENABLER(irq_id) ((volatile unsigned int *)(GICD_BASE + 0x100 + (((irq_id) / 32U) * 4U)))
#define GICD_ITARGETSR(irq_id) ((volatile unsigned int *)(GICD_BASE + 0x800 + (((irq_id) / 4U) * 4U)))

#define GICC_CTLR ((volatile unsigned int *)(GICC_BASE + 0x0000))
#define GICC_PMR ((volatile unsigned int *)(GICC_BASE + 0x0004))
#define GICC_IAR ((volatile unsigned int *)(GICC_BASE + 0x000c))
#define GICC_EOIR ((volatile unsigned int *)(GICC_BASE + 0x0010))

const char *gic_version_name(void)
{
    return "GICv2";
}

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
    unsigned int bit_mask;

    bit_mask = (1U << (irq_id % 32U));
    *GICD_ISENABLER(irq_id) = bit_mask;

    if (irq_id >= 32U)
    {
        volatile unsigned int *targets;
        unsigned int shift;
        unsigned int value;

        targets = GICD_ITARGETSR(irq_id);
        shift = (irq_id % 4U) * 8U;
        value = *targets;
        value &= ~(0xffU << shift);
        value |= (0x01U << shift);
        *targets = value;
    }
}

unsigned int gic_acknowledge_irq(void)
{
    return *GICC_IAR;
}

void gic_end_irq(unsigned int irq_id)
{
    *GICC_EOIR = irq_id;
}
