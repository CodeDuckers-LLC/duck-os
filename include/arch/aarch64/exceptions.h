#ifndef ARCH_AARCH64_EXCEPTIONS_H
#define ARCH_AARCH64_EXCEPTIONS_H

struct exception_trap_frame
{
    unsigned long x[31];
    unsigned long sp;
    unsigned long elr_el1;
    unsigned long spsr_el1;
    unsigned long esr_el1;
    unsigned long far_el1;
    unsigned long daif;
    unsigned long vector_id;
};

void exceptions_init(void);
int exceptions_self_test(void);
void exceptions_trigger_test(void);
void exception_handle(struct exception_trap_frame *frame);

#endif
