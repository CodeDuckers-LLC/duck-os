# QEMU Notes

Project currently targets QEMU `virt` machine in AArch64 mode.

## Run Command

`make run` starts:

```sh
qemu-system-aarch64 \
  -M virt \
  -cpu cortex-a53 \
  -nographic \
  -serial mon:stdio \
  -kernel build/kernel.elf
```

Meaning:

- `-M virt`: generic virtual ARM machine
- `-cpu cortex-a53`: current CPU model
- `-nographic`: no graphical window
- `-serial mon:stdio`: UART and QEMU monitor share terminal
- `-kernel build/kernel.elf`: load kernel image directly

## Expected Output

When boot succeeds, terminal shows:

```text
[INFO] Custom Pi OS kernel booted
[INFO] Target: QEMU virt AArch64
[INFO] UART is working
[INFO] exceptions: VBAR_EL1 installed
[INFO] AArch64 system registers:
  CurrentEL: 0x4 (EL1)
  SPSel: 0x1
  DAIF: 0x3c0
  MPIDR_EL1: 0x...
[INFO] boot bss clear: PASS
[INFO] exceptions vbar install: PASS
[INFO] string memset: PASS
[INFO] string memcpy: PASS
[INFO] string memcmp: PASS
[INFO] string strlen: PASS
[INFO] string strcmp: PASS
[INFO] string strlcpy: PASS
[INFO] kprintf smoke tests:
  decimal: -42 17 3000000000
  hex: 1a2b3c4d
  pointer: 0x...
  string: duck
  char: Q
  percent/unknown: % %q
```

After that, kernel waits forever.

## Exception Test

`make run-exception-test` rebuilds kernel with `EXCEPTION_SELF_TEST=1` and triggers `brk #0x42` after normal boot tests.

Expected tail of output:

```text
[INFO] exception self test: triggering brk
[ERROR] exception: sync current el spx
[ERROR] ESR class: brk aarch64
[ERROR] BRK immediate: 0x42
[PANIC] Unhandled exception
```

## Debug Mode

`make debug` adds:

- `-S`: do not start CPU until debugger continues it
- `-s`: expose GDB stub on `:1234`

Basic flow:

```sh
make debug
aarch64-linux-gnu-gdb build/kernel.elf
```

Then in GDB:

```gdb
target remote :1234
break _start
continue
```

## Scope

These notes only cover current QEMU `virt` target. They do not describe real hardware boot flow or board-specific behavior.
