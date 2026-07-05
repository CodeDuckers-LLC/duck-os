# Roadmap

This roadmap describes likely next steps for current codebase. It is not promise of implemented features.

## Current

Working today:

- Freestanding AArch64 build
- Linker script with fixed load address
- Assembly entry point
- Simple C kernel entry
- PL011 UART output on QEMU `virt`
- Minimal freestanding string routines
- Minimal `kprintf` formatting over UART
- Minimal EL1 exception vector table and default trap reporting
- Trap-frame capture for exceptions with basic ESR decoding
- Basic QEMU `virt` GICv2 bring-up
- EL1 physical timer interrupt demo with periodic IRQ logging

## Near Term

Reasonable next milestones:

1. Clear `.bss` during early boot instead of relying on loader behavior.
1. Pass basic boot information from assembly entry into C.
2. Split hardware-specific code from generic kernel code.
3. Improve linker layout and segment permissions.
4. Add automated host-side log checks for boot and exception-test output.

## Platform Bring-Up

After early cleanup:

1. Expand interrupt handling beyond timer-only path.
2. Add device tree parsing for timer/GIC discovery instead of fixed addresses.
3. Add cleaner idle loop and IRQ-safe console/logging policy.

## Core Kernel

After platform basics:

1. Physical memory map handling.
2. Page table setup and MMU enable.
3. Simple page allocator.
4. Kernel heap allocator.

## Longer Term

Possible later work:

1. Cooperative or preemptive scheduling.
2. Device drivers beyond early UART.
3. Block device and filesystem support.
4. User/kernel isolation and user space.

## Non-Goals For Now

Not present in current tree:

- Raspberry Pi board support
- SMP support
- Networking
- POSIX compatibility
