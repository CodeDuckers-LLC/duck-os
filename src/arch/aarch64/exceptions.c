#include "arch/aarch64/exceptions.h"
#include "arch/aarch64/gic.h"
#include "arch/aarch64/sysreg.h"
#include "drivers/virtio.h"
#include "kernel/klog.h"
#include "kernel/panic.h"
#include "kernel/syscall.h"
#include "kernel/task.h"
#include "kernel/timer.h"

extern char exception_vector_table[];

static const char *exception_vector_name(unsigned long vector_id)
{
    static const char *names[] = {
        "sync current el sp0",
        "irq current el sp0",
        "fiq current el sp0",
        "serror current el sp0",
        "sync current el spx",
        "irq current el spx",
        "fiq current el spx",
        "serror current el spx",
        "sync lower el aarch64",
        "irq lower el aarch64",
        "fiq lower el aarch64",
        "serror lower el aarch64",
        "sync lower el aarch32",
        "irq lower el aarch32",
        "fiq lower el aarch32",
        "serror lower el aarch32",
    };

    if (vector_id < (sizeof(names) / sizeof(names[0])))
    {
        return names[vector_id];
    }

    return "unknown";
}

static const char *exception_kind_name(unsigned long vector_id)
{
    switch (vector_id & 0x3UL)
    {
    case 0:
        return "sync";
    case 1:
        return "irq";
    case 2:
        return "fiq";
    case 3:
        return "serror";
    default:
        return "unknown";
    }
}

static const char *exception_class_name(unsigned long ec)
{
    switch (ec)
    {
    case 0x00:
        return "unknown reason";
    case 0x01:
        return "trapped wfi or wfe";
    case 0x03:
        return "trapped mcr or mrc cp15";
    case 0x04:
        return "trapped mcrr or mrrc cp15";
    case 0x05:
        return "trapped mcr or mrc cp14";
    case 0x06:
        return "trapped ldc or stc";
    case 0x07:
        return "trapped sve or asimd";
    case 0x0e:
        return "illegal execution state";
    case 0x11:
        return "svc aarch32";
    case 0x15:
        return "svc aarch64";
    case 0x18:
        return "trapped msr mrs or system instruction";
    case 0x20:
        return "instruction abort lower el";
    case 0x21:
        return "instruction abort current el";
    case 0x22:
        return "pc alignment fault";
    case 0x24:
        return "data abort lower el";
    case 0x25:
        return "data abort current el";
    case 0x26:
        return "sp alignment fault";
    case 0x2c:
        return "floating point exception aarch64";
    case 0x2f:
        return "serror interrupt";
    case 0x30:
        return "breakpoint lower el";
    case 0x31:
        return "breakpoint current el";
    case 0x32:
        return "software step lower el";
    case 0x33:
        return "software step current el";
    case 0x34:
        return "watchpoint lower el";
    case 0x35:
        return "watchpoint current el";
    case 0x3c:
        return "brk aarch64";
    default:
        return "reserved or unhandled class";
    }
}

static const char *exception_abort_fsc_name(unsigned long fsc)
{
    switch (fsc)
    {
    case 0x00:
        return "address size fault level 0";
    case 0x01:
        return "address size fault level 1";
    case 0x02:
        return "address size fault level 2";
    case 0x03:
        return "address size fault level 3";
    case 0x04:
        return "translation fault level 0";
    case 0x05:
        return "translation fault level 1";
    case 0x06:
        return "translation fault level 2";
    case 0x07:
        return "translation fault level 3";
    case 0x09:
        return "access flag fault level 1";
    case 0x0a:
        return "access flag fault level 2";
    case 0x0b:
        return "access flag fault level 3";
    case 0x0d:
        return "permission fault level 1";
    case 0x0e:
        return "permission fault level 2";
    case 0x0f:
        return "permission fault level 3";
    case 0x10:
        return "synchronous external abort";
    case 0x11:
        return "tag check fault";
    case 0x14:
        return "translation table external abort level 0";
    case 0x15:
        return "translation table external abort level 1";
    case 0x16:
        return "translation table external abort level 2";
    case 0x17:
        return "translation table external abort level 3";
    case 0x18:
        return "parity or ecc error";
    case 0x21:
        return "alignment fault";
    case 0x30:
        return "tlb conflict abort";
    default:
        return "unknown fault status";
    }
}

static void exception_print_abort_details(unsigned long ec, unsigned long iss)
{
    unsigned long fsc;

    fsc = iss & 0x3f;
    kprintf("[ERROR] abort status: %s\n", exception_abort_fsc_name(fsc));
    kprintf("[ERROR] abort fsc: 0x%x\n", (unsigned int)fsc);

    if (ec == 0x24 || ec == 0x25)
    {
        kprintf("[ERROR] data abort access: %s\n",
                (iss & (1UL << 6)) != 0 ? "write" : "read");
        kprintf("[ERROR] data abort s1ptw: %u\n",
                (unsigned int)((iss >> 7) & 0x1));
        kprintf("[ERROR] data abort fnv: %u\n",
                (unsigned int)((iss >> 10) & 0x1));
    }
}

static void exception_print_esr_details(unsigned long esr)
{
    unsigned long ec;
    unsigned long il;
    unsigned long iss;

    ec = (esr >> 26) & 0x3f;
    il = (esr >> 25) & 0x1;
    iss = esr & 0x01ffffff;

    kprintf("[ERROR] ESR class: %s\n", exception_class_name(ec));
    kprintf("[ERROR] ESR EC: 0x%x\n", (unsigned int)ec);
    kprintf("[ERROR] ESR IL: %u\n", (unsigned int)il);
    kprintf("[ERROR] ESR ISS: 0x%x\n", (unsigned int)iss);

    if (ec == 0x20 || ec == 0x21 || ec == 0x24 || ec == 0x25)
    {
        exception_print_abort_details(ec, iss);
    }
    else if (ec == 0x15)
    {
        kprintf("[ERROR] SVC immediate: 0x%x\n", (unsigned int)(iss & 0xffff));
    }
    else if (ec == 0x3c)
    {
        kprintf("[ERROR] BRK immediate: 0x%x\n", (unsigned int)(iss & 0xffff));
    }
}

static int exception_is_irq_vector(unsigned long vector_id)
{
    return vector_id == 1 || vector_id == 5 || vector_id == 9 || vector_id == 13;
}

void exceptions_init(void)
{
    unsigned long current_el;

    current_el = sysreg_read_currentel() >> 2;
    if (current_el != 1)
    {
        kprintf("[INFO] exceptions: skip VBAR_EL1 install at EL%u\n",
                (unsigned int)current_el);
        return;
    }

    sysreg_write_vbar_el1((unsigned long)exception_vector_table);
    klog_info("exceptions: VBAR_EL1 installed");
}

int exceptions_self_test(void)
{
    unsigned long current_el;

    current_el = sysreg_read_currentel() >> 2;
    if (current_el != 1)
    {
        return 1;
    }

    return sysreg_read_vbar_el1() == (unsigned long)exception_vector_table;
}

void exceptions_trigger_test(void)
{
    asm volatile("brk #0x42");
}

static struct exception_trap_frame *exception_handle_irq(struct exception_trap_frame *frame)
{
    unsigned int irq_id;

    irq_id = gic_acknowledge_irq();
    irq_id &= 0x3ffU;

    if (irq_id >= 1020U)
    {
        return frame;
    }

    if (irq_id == GIC_IRQ_TIMER_PHYSICAL_PPI)
    {
        timer_handle_irq();
        gic_end_irq(irq_id);
        if ((timer_irq_count() % task_time_slice_ticks()) == 0)
        {
            return task_schedule_from_exception(frame, 1);
        }
        return frame;
    }

    if (irq_id >= 48U && irq_id < 80U)
    {
        virtio_handle_irq(irq_id);
        gic_end_irq(irq_id);
        return frame;
    }

    kprintf("[ERROR] unexpected IRQ: %u\n", irq_id);
    gic_end_irq(irq_id);
    panic("Unhandled IRQ");
    return frame;
}

static void exception_handle_default(struct exception_trap_frame *frame)
{
    kprintf("[ERROR] exception: %s\n", exception_vector_name(frame->vector_id));
    kprintf("[ERROR] type: %s\n", exception_kind_name(frame->vector_id));
    kprintf("[ERROR] vector: %u\n", (unsigned int)frame->vector_id);
    kprintf("[ERROR] ESR_EL1: 0x%x\n", (unsigned int)frame->esr_el1);
    exception_print_esr_details(frame->esr_el1);
    kprintf("[ERROR] ELR_EL1: %p\n", (void *)frame->elr_el1);
    kprintf("[ERROR] SPSR_EL1: 0x%x\n", (unsigned int)frame->spsr_el1);
    kprintf("[ERROR] FAR_EL1: %p\n", (void *)frame->far_el1);
    kprintf("[ERROR] DAIF: 0x%x\n", (unsigned int)frame->daif);
    kprintf("[ERROR] SP: %p\n", (void *)frame->sp);
    kprintf("[ERROR] X0: %p\n", (void *)frame->x[0]);
    kprintf("[ERROR] X1: %p\n", (void *)frame->x[1]);
    kprintf("[ERROR] X2: %p\n", (void *)frame->x[2]);
    kprintf("[ERROR] X3: %p\n", (void *)frame->x[3]);
    kprintf("[ERROR] X29: %p\n", (void *)frame->x[29]);
    kprintf("[ERROR] X30: %p\n", (void *)frame->x[30]);

    panic("Unhandled exception");
}

struct exception_trap_frame *exception_handle(struct exception_trap_frame *frame)
{
    unsigned long ec;
    unsigned long iss;
    unsigned long svc_imm;

    if (exception_is_irq_vector(frame->vector_id))
    {
        return exception_handle_irq(frame);
    }

    ec = (frame->esr_el1 >> 26) & 0x3f;
    iss = frame->esr_el1 & 0x01ffffff;
    svc_imm = iss & 0xffffUL;

    if (ec == 0x15 && svc_imm == 0)
    {
        return task_schedule_from_exception(frame, 1);
    }
    if (ec == 0x15 && svc_imm == 1)
    {
        return syscall_handle(frame);
    }

    exception_handle_default(frame);
    return frame;
}
