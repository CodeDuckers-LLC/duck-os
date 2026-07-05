.section .text

.global arch_context_switch
.type arch_context_switch, %function

arch_context_switch:
    // Save callee-saved registers for current task.
    sub sp, sp, #96
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]

    // Publish current stack pointer, then load next task stack pointer.
    mov x9, sp
    str x9, [x0]
    ldr x9, [x1]
    mov sp, x9

    // Restore callee-saved registers for next task and return into it.
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #96
    ret
