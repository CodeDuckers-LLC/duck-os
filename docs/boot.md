# Boot Flow

Current boot path is minimal.

## Entry

Linker script sets kernel load address to `0x40080000`, which is inside RAM region used by QEMU `virt`.

Execution starts at `_start` in `src/arch/aarch64/boot.S`.

Current assembly entry does only three things:

1. Load `__stack_top` from linker script.
2. Set `sp`.
3. Clear the linker-defined `.bss` range from `__bss_start` to `__bss_end`.
4. Branch to `kernel_main`.

If `kernel_main` returns, code enters infinite `wfi` loop.

## Stack

Linker script reserves `0x4000` bytes for early stack and exports `__stack_top`.

Current setup:

- Single stack
- No per-CPU state
- No guard page

## Memory Sections

Linker script places these sections in order:

- `.text`
- `.rodata`
- `.data`
- `.bss`

Current boot code explicitly clears `.bss` before entering C so zero-initialized globals are safe to use from `kernel_main` onward.

Linker script also exports:

- `__kernel_start`
- `__kernel_end`

Current kernel treats `__kernel_end` as first free physical address after linked image and early stack reservation.

## Kernel Entry

`kernel_main` currently:

1. Writes boot status messages through the logging layer.
2. Prints platform name, UART base, and RAM range through platform API.
3. Masks IRQs in `DAIF` during early setup.
4. Installs `VBAR_EL1` when running at EL1.
5. Reads and prints generic timer frequency.
6. Builds simple identity-mapped EL1 page tables and enables MMU.
7. Initializes physical memory manager and prints page usage.
8. Runs boot self-tests, including `.bss`, exception install, MMU, memory layout, allocators, and timer polling checks.
9. Optionally triggers deliberate `brk` exception in dedicated exception-test build.
10. Initializes QEMU `virt` GICv2.
11. Programs EL1 physical timer for 1 second periodic interrupts.
12. Unmasks IRQs and enters UART shell loop.

No device tree parsing, advanced virtual memory layout, or broad device interrupt support exists yet.
Current MMU setup is intentionally minimal:

- 4 KiB granule
- TTBR0_EL1 only
- identity maps RAM as normal cacheable memory
- identity maps PL011 UART and required GIC MMIO as device memory
- no high-half kernel mapping
- no userspace mappings

## UART

Early output uses PL011 UART through platform layer. Current QEMU `virt` mapping resolves UART0 to `0x09000000`.

## Logging

Formatted boot output uses `kprintf`, which writes through kernel console layer over PL011 UART.

Currently supported format specifiers are:

- `%s`
- `%c`
- `%d`
- `%u`
- `%x`
- `%p`
- `%%`

Unknown format specifiers are printed literally as `%` followed by the unknown character.

## Exceptions

Kernel now provides minimal ARM64 exception vector table for EL1 with IRQ return path.

- `exceptions_init()` installs `VBAR_EL1` only when `CurrentEL` reports EL1.
- All 16 vector slots branch through common assembly entry that saves trap frame.
- Trap frame captures `x0` through `x30`, exception-time `sp`, `ELR_EL1`, `SPSR_EL1`, `ESR_EL1`, `FAR_EL1`, `DAIF`, and vector slot ID.
- Synchronous exceptions still print decoded trap state and end in `panic`.
- IRQ vectors dispatch through GIC acknowledge path and `eret` back to interrupted code.

## Interrupts

Current interrupt bring-up is minimal and QEMU `virt` specific.

- GICv2 distributor base is hardcoded to `0x08000000`.
- GICv2 CPU interface base is hardcoded to `0x08010000`.
- Kernel enables only EL1 physical timer PPI `30`.
- `DAIF.I` stays masked until vectors, timer, and GIC are ready.
- Timer handler rearms `CNTP_TVAL_EL0` and increments software tick counter.
