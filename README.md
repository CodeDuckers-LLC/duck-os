# duck-os

Small bare-metal ARM64 OS project.

Current state:

- Targets `qemu-system-aarch64` `virt` machine
- Builds freestanding AArch64 kernel from C and assembly
- Boots with custom `_start` entry point
- Writes log messages over PL011 UART
- Supports polling-based PL011 UART input
- Provides tiny UART shell
- Installs minimal EL1 exception vector table when booted at EL1
- Handles periodic EL1 physical timer interrupts through QEMU `virt` GICv2

Project does not yet provide:

- Virtual memory
- Broad interrupt handling beyond generic timer bring-up
- Dynamic memory allocation
- Process or task scheduling
- Filesystems
- User space

## Requirements

Tested on Ubuntu with:

- `git`
- `make`
- `aarch64-linux-gnu-gcc`
- `aarch64-linux-gnu-objcopy`
- `qemu-system-aarch64`

On Debian or Ubuntu:

```sh
sudo apt install build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-system-arm
```

## Develop From Source

Clone the source and enter the project directory:

```sh
git clone <repo-url> duck-os
cd duck-os
```

If the cross toolchain binaries are not already on your `PATH`, export them before building:

```sh
export PATH="/path/to/toolchain/bin:$PATH"
export CROSS_COMPILE=aarch64-linux-gnu-
```

`CROSS_COMPILE` defaults to `aarch64-linux-gnu-`, so the export is only needed if you want to override the tool prefix at build time:

```sh
make CROSS_COMPILE=aarch64-linux-gnu-
```

## Build

```sh
make
```

Build output:

- `build/kernel.elf`
- `build/kernel.bin`

## Run

```sh
make run
```

Current serial output:

```text
[INFO] Custom Pi OS kernel booted
platform: qemu-virt
uart:     0x09000000
ram:      0x40000000 - 0x48000000
[INFO] UART is working
[INFO] exceptions: VBAR_EL1 installed
[INFO] timer: CNTFRQ_EL0=62500000 Hz
[INFO] kernel tests: start
[INFO] AArch64 system registers:
  CurrentEL: 0x4 (EL1)
  SPSel: 0x1
  DAIF: 0x3c0
  MPIDR_EL1: 0x...
[TEST] PASS: .bss clear
[TEST] PASS: exception VBAR install
[TEST] PASS: DAIF IRQ masked during boot
[TEST] PASS: timer IRQ id
[INFO] physical memory layout:
  RAM start: 0x40000000
  RAM end: 0x48000000
  kernel start: 0x40080000
  kernel end: 0x...
  first free physical address: 0x...
[TEST] PASS: RAM base
[TEST] PASS: RAM end
[TEST] PASS: kernel start in RAM
[TEST] PASS: kernel end in RAM
[TEST] PASS: first free physical address
[TEST] PASS: memset
[TEST] PASS: memcpy
[TEST] PASS: memcmp
[TEST] PASS: strlen
[TEST] PASS: strcmp
[TEST] PASS: strlcpy
[TEST] PASS: kmalloc block1
[TEST] PASS: kmalloc block2
[TEST] PASS: kzalloc block3
[TEST] PASS: kmalloc align block1
[TEST] PASS: kmalloc align block2
[TEST] PASS: kmalloc align block3
[TEST] PASS: kmalloc non-overlap block1 block2
[TEST] PASS: kmalloc non-overlap block2 block3
[TEST] PASS: kzalloc zero fill
[TEST] PASS: kmalloc used
[TEST] PASS: timer frequency
timer frequency: 62500000 Hz
waiting 1 second...
done
[TEST] PASS: timer monotonic
[TEST] PASS: timer advances
[TEST] PASS: timer IRQ count before enable
[TEST] INFO: kprintf decimal: -42 17 3000000000
[TEST] INFO: kprintf hex: 1a2b3c4d
[TEST] INFO: kprintf pointer: 0x...
[TEST] INFO: kprintf string: duck
[TEST] INFO: kprintf char: Q
[TEST] INFO: kprintf percent/unknown: % %q
[TEST] PASS: kprintf smoke
[TEST] SUMMARY: 30/30 passed
[INFO] kernel tests: halt
timer interrupt shell: armed for 1000 ms
[INFO] IRQ unmasked
[INFO] shell ready
>help
help
version
mem
uptime
panic
>uptime
uptime: 4.322 s
```

## Exception Test

Trigger deliberate `brk` exception and inspect decoded trap output:

```sh
make run-exception-test
```

Expected end of output includes:

```text
[INFO] exception self test: triggering brk
[ERROR] exception: sync current el spx
[ERROR] ESR class: brk aarch64
[ERROR] BRK immediate: 0x42
[PANIC] Unhandled exception
```

## Debug

Start QEMU and wait for debugger:

```sh
make debug
```

This starts QEMU with:

- GDB stub on TCP port `1234`
- CPU halted at startup

Example attach command:

```sh
aarch64-linux-gnu-gdb build/kernel.elf
```

Then in GDB:

```gdb
target remote :1234
break kernel_main
continue
```

## Docs

- [Boot flow](docs/boot.md)
- [QEMU notes](docs/qemu.md)
- [Roadmap](docs/roadmap.md)
- [Coding style](docs/coding-style.md)
