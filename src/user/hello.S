.section .text
.global _start

_start:
    adr x0, message
    mov x1, #22
    mov x8, #1
    svc #1

    mov x0, #0
    mov x8, #3
    svc #1

1:
    b 1b

message:
    .ascii "[USER] hello from EL0\n"
