#ifndef ARCH_AARCH64_SYSREG_H
#define ARCH_AARCH64_SYSREG_H

static inline unsigned long sysreg_read_currentel(void)
{
    unsigned long value;

    asm volatile("mrs %0, CurrentEL" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_spsel(void)
{
    unsigned long value;

    asm volatile("mrs %0, SPSel" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_daif(void)
{
    unsigned long value;

    asm volatile("mrs %0, DAIF" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_mpidr_el1(void)
{
    unsigned long value;

    asm volatile("mrs %0, MPIDR_EL1" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_esr_el1(void)
{
    unsigned long value;

    asm volatile("mrs %0, ESR_EL1" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_elr_el1(void)
{
    unsigned long value;

    asm volatile("mrs %0, ELR_EL1" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_spsr_el1(void)
{
    unsigned long value;

    asm volatile("mrs %0, SPSR_EL1" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_far_el1(void)
{
    unsigned long value;

    asm volatile("mrs %0, FAR_EL1" : "=r"(value));
    return value;
}

static inline void sysreg_write_vbar_el1(unsigned long value)
{
    asm volatile("msr VBAR_EL1, %0" : : "r"(value));
    asm volatile("isb");
}

static inline unsigned long sysreg_read_vbar_el1(void)
{
    unsigned long value;

    asm volatile("mrs %0, VBAR_EL1" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_cntfrq_el0(void)
{
    unsigned long value;

    asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_cntpct_el0(void)
{
    unsigned long value;

    asm volatile("isb");
    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(value));
    return value;
}

static inline unsigned long sysreg_read_cntp_ctl_el0(void)
{
    unsigned long value;

    asm volatile("mrs %0, CNTP_CTL_EL0" : "=r"(value));
    return value;
}

static inline void sysreg_write_cntp_tval_el0(unsigned long value)
{
    asm volatile("msr CNTP_TVAL_EL0, %0" : : "r"(value));
    asm volatile("isb");
}

static inline void sysreg_write_cntp_ctl_el0(unsigned long value)
{
    asm volatile("msr CNTP_CTL_EL0, %0" : : "r"(value));
    asm volatile("isb");
}

static inline void sysreg_set_daif_irq(void)
{
    asm volatile("msr DAIFSet, #2");
    asm volatile("isb");
}

static inline void sysreg_clear_daif_irq(void)
{
    asm volatile("msr DAIFClr, #2");
    asm volatile("isb");
}

#endif
