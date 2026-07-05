ENTRY(_start)

SECTIONS
{
    /* virt RAM beings at 0x40000000, this places the kernel slightly above that */
    . = 0x40080000;
    __kernel_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /* creating a 16kb stack */
    . = ALIGN(16);
    . += 0x4000;
    __stack_top = .;
    __kernel_end = .;
}
