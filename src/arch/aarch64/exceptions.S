.section .text

.align 11
.global exception_vector_table
.type exception_vector_table, %function

.extern exception_handle

.macro vector_entry vector_id
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #\vector_id
    b exception_entry_common
    .balign 128
.endm

exception_vector_table:
    vector_entry 0
    vector_entry 1
    vector_entry 2
    vector_entry 3
    vector_entry 4
    vector_entry 5
    vector_entry 6
    vector_entry 7
    vector_entry 8
    vector_entry 9
    vector_entry 10
    vector_entry 11
    vector_entry 12
    vector_entry 13
    vector_entry 14
    vector_entry 15

exception_entry_common:
    sub sp, sp, #0x130

    stp x1, x2, [sp, #0x08]
    stp x3, x4, [sp, #0x18]
    stp x5, x6, [sp, #0x28]
    stp x7, x8, [sp, #0x38]
    stp x9, x10, [sp, #0x48]
    stp x11, x12, [sp, #0x58]
    stp x13, x14, [sp, #0x68]
    stp x15, x16, [sp, #0x78]
    stp x17, x18, [sp, #0x88]
    stp x19, x20, [sp, #0x98]
    stp x21, x22, [sp, #0xa8]
    stp x23, x24, [sp, #0xb8]
    stp x25, x26, [sp, #0xc8]
    stp x27, x28, [sp, #0xd8]
    stp x29, x30, [sp, #0xe8]

    ldr x1, [sp, #0x130]
    str x1, [sp, #0x00]

    add x1, sp, #0x140
    str x1, [sp, #0xf8]

    mrs x1, ELR_EL1
    str x1, [sp, #0x100]

    mrs x1, SPSR_EL1
    str x1, [sp, #0x108]

    mrs x1, ESR_EL1
    str x1, [sp, #0x110]

    mrs x1, FAR_EL1
    str x1, [sp, #0x118]

    mrs x1, DAIF
    str x1, [sp, #0x120]

    str x0, [sp, #0x128]

    mov x0, sp
    bl exception_handle

    ldr x0, [sp, #0x00]
    ldp x1, x2, [sp, #0x08]
    ldp x3, x4, [sp, #0x18]
    ldp x5, x6, [sp, #0x28]
    ldp x7, x8, [sp, #0x38]
    ldp x9, x10, [sp, #0x48]
    ldp x11, x12, [sp, #0x58]
    ldp x13, x14, [sp, #0x68]
    ldp x15, x16, [sp, #0x78]
    ldp x17, x18, [sp, #0x88]
    ldp x19, x20, [sp, #0x98]
    ldp x21, x22, [sp, #0xa8]
    ldp x23, x24, [sp, #0xb8]
    ldp x25, x26, [sp, #0xc8]
    ldp x27, x28, [sp, #0xd8]
    ldp x29, x30, [sp, #0xe8]

    add sp, sp, #0x140
    eret
