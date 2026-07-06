# QEMU Notes

Project currently targets QEMU `virt` machine in AArch64 mode.

## Run Command

`make run` starts:

```sh
qemu-system-aarch64 \
  -M virt,gic-version=2 \
  -cpu cortex-a53 \
  -global virtio-mmio.force-legacy=false \
  -drive if=none,file=build/virtio-blk.img,format=raw,readonly=on,id=vdisk0 \
  -nographic \
  -serial mon:stdio \
  -device virtio-rng-device,bus=virtio-mmio-bus.0 \
  -device virtio-blk-device,drive=vdisk0,bus=virtio-mmio-bus.1 \
  -kernel build/kernel.elf
```

Meaning:

- `-M virt,gic-version=2`: generic virtual ARM machine with GICv2
- `-cpu cortex-a53`: current CPU model
- `-global virtio-mmio.force-legacy=false`: expose modern VirtIO MMIO transport
- `-drive if=none,file=build/virtio-blk.img,format=raw,readonly=on,id=vdisk0`: create raw read-only disk backend
- `-nographic`: no graphical window
- `-serial mon:stdio`: UART and QEMU monitor share terminal
- `-device virtio-rng-device,bus=virtio-mmio-bus.0`: attach one real VirtIO MMIO device
- `-device virtio-blk-device,drive=vdisk0,bus=virtio-mmio-bus.1`: attach one read-only-test VirtIO block device
- `-kernel build/kernel.elf`: load kernel image directly

## VirtIO MMIO

Kernel scans QEMU `virt` MMIO window at boot and prints discovered VirtIO devices:

```text
[INFO] virtio: slot 0 irq 48 device 4 vendor 0x554d4551
[INFO] virtio: slot 1 irq 49 device 2 vendor 0x554d4551
[INFO] virtio: detected 2 device(s)
```

After boot:

- `virtio` dumps discovered devices
- `blk` lists registered block devices
- `blkread vda 0` dumps block 0 from test disk image

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
