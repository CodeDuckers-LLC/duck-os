.section .text.boot

.global _start
.extern kernel_main
.extern __bss_end
.extern __bss_start
.extern __stack_top

# - Defines the kernel entry point: _start
# - Loads the stack address into x0
# - Moves x0 into the stack pointer
# - Clears the .bss range exported by the linker script
# - Calls kernel_main()
# - If kernel_main() returns, the CPU waits forever

_start:
    ldr x0, =__stack_top
    mov sp, x0

    ldr x0, =__bss_start
    ldr x1, =__bss_end
    mov x2, xzr

clear_bss:
    cmp x0, x1
    b.hs bss_cleared
    str x2, [x0], #8
    b clear_bss

bss_cleared:
    bl kernel_main

hang:
    wfe
    b hang
