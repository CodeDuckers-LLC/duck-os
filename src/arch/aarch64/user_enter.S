.section .text

.global arch_enter_user
.global arch_user_resume_el1
.type arch_enter_user, %function

arch_enter_user:
    stp x19, x20, [sp, #-96]!
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]

    msr SP_EL0, x1
    msr ELR_EL1, x0
    msr SPSR_EL1, xzr
    isb
    eret

arch_user_resume_el1:
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #96
    ret
