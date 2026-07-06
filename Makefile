CROSS_COMPILE := aarch64-linux-gnu-

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
PYTHON ?= python3

BUILD_DIR := build

CFLAGS := -Wall -Wextra -O2 -ffreestanding -nostdlib -nostartfiles -mgeneral-regs-only -Iinclude
LDFLAGS := -T linker.ld -nostdlib

OBJS := \
	$(BUILD_DIR)/block/block_device.o \
	$(BUILD_DIR)/arch/aarch64/boot.o \
	$(BUILD_DIR)/arch/aarch64/cpu.o \
	$(BUILD_DIR)/arch/aarch64/exceptions.o \
	$(BUILD_DIR)/arch/aarch64/exceptions_asm.o \
	$(BUILD_DIR)/arch/aarch64/gic.o \
	$(BUILD_DIR)/arch/aarch64/context_switch.o \
	$(BUILD_DIR)/arch/aarch64/mmu.o \
	$(BUILD_DIR)/drivers/virtqueue.o \
	$(BUILD_DIR)/drivers/ramdisk.o \
	$(BUILD_DIR)/drivers/virtio_blk.o \
	$(BUILD_DIR)/drivers/virtio_mmio.o \
	$(BUILD_DIR)/drivers/virtio_rng.o \
	$(BUILD_DIR)/fs/file.o \
	$(BUILD_DIR)/fs/tinyfs.o \
	$(BUILD_DIR)/fs/vfs.o \
	$(BUILD_DIR)/arch/aarch64/user_enter.o \
	$(BUILD_DIR)/drivers/uart_pl011.o \
	$(BUILD_DIR)/platform/qemu_virt/platform.o \
	$(BUILD_DIR)/kernel/console.o \
	$(BUILD_DIR)/kernel/kernel.o \
	$(BUILD_DIR)/kernel/klog.o \
	$(BUILD_DIR)/kernel/kmalloc.o \
	$(BUILD_DIR)/kernel/initramfs.o \
	$(BUILD_DIR)/kernel/memory_layout.o \
	$(BUILD_DIR)/kernel/panic.o \
	$(BUILD_DIR)/kernel/shell.o \
	$(BUILD_DIR)/kernel/spinlock.o \
	$(BUILD_DIR)/kernel/syscall.o \
	$(BUILD_DIR)/kernel/task.o \
	$(BUILD_DIR)/kernel/test.o \
	$(BUILD_DIR)/kernel/timer.o \
	$(BUILD_DIR)/kernel/user.o \
	$(BUILD_DIR)/mm/pmm.o \
	$(BUILD_DIR)/lib/string.o \
	$(BUILD_DIR)/user/hello_blob.o \
	$(BUILD_DIR)/initramfs_blob.o \
	$(BUILD_DIR)/tinyfs_blob.o

VIRTIO_BLK_IMG := $(BUILD_DIR)/virtio-blk.img

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin

.PHONY: all clean run debug run-exception-test

all: $(KERNEL_ELF) $(KERNEL_BIN) $(VIRTIO_BLK_IMG)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/arch/aarch64/boot.o: src/arch/aarch64/boot.S | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/block/block_device.o: src/block/block_device.c include/block/block_device.h include/kernel/console.h include/kernel/klog.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/aarch64/cpu.o: src/arch/aarch64/cpu.c include/arch/aarch64/cpu.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/aarch64/exceptions.o: src/arch/aarch64/exceptions.c include/arch/aarch64/exceptions.h include/arch/aarch64/sysreg.h include/kernel/klog.h include/kernel/panic.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/aarch64/exceptions_asm.o: src/arch/aarch64/exceptions.S include/arch/aarch64/exceptions.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/aarch64/context_switch.o: src/arch/aarch64/context_switch.S | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/aarch64/gic.o: src/arch/aarch64/gic.c include/arch/aarch64/gic.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/aarch64/mmu.o: src/arch/aarch64/mmu.c include/arch/aarch64/gic.h include/arch/aarch64/mmu.h include/arch/aarch64/sysreg.h include/kernel/klog.h include/lib/string.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/virtqueue.o: src/drivers/virtqueue.c include/drivers/virtqueue.h include/lib/string.h include/mm/pmm.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/ramdisk.o: src/drivers/ramdisk.c include/block/block_device.h include/drivers/ramdisk.h include/kernel/klog.h include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/virtio_blk.o: src/drivers/virtio_blk.c include/arch/aarch64/mmu.h include/block/block_device.h include/drivers/virtio.h include/drivers/virtio_blk.h include/kernel/klog.h include/kernel/spinlock.h include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/virtio_mmio.o: src/drivers/virtio_mmio.c include/arch/aarch64/gic.h include/drivers/virtio.h include/drivers/virtio_mmio.h include/kernel/klog.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/virtio_rng.o: src/drivers/virtio_rng.c include/arch/aarch64/cpu.h include/drivers/virtio.h include/drivers/virtio_rng.h include/kernel/klog.h include/kernel/spinlock.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fs/file.o: src/fs/file.c include/fs/file.h include/fs/vfs.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fs/tinyfs.o: src/fs/tinyfs.c include/block/block_device.h include/drivers/ramdisk.h include/fs/tinyfs.h include/kernel/console.h include/kernel/klog.h include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fs/vfs.o: src/fs/vfs.c include/block/block_device.h include/fs/tinyfs.h include/fs/vfs.h include/kernel/klog.h include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/aarch64/user_enter.o: src/arch/aarch64/user_enter.S | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/uart_pl011.o: src/drivers/uart_pl011.c include/drivers/uart.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/platform/qemu_virt/platform.o: src/platform/qemu_virt/platform.c include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/console.o: src/kernel/console.c include/drivers/uart.h include/kernel/console.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/kernel.o: src/kernel/kernel.c include/arch/aarch64/cpu.h include/arch/aarch64/exceptions.h include/arch/aarch64/gic.h include/arch/aarch64/mmu.h include/arch/aarch64/sysreg.h include/drivers/ramdisk.h include/drivers/virtio.h include/drivers/virtio_blk.h include/drivers/virtio_rng.h include/fs/tinyfs.h include/fs/vfs.h include/kernel/console.h include/kernel/klog.h include/kernel/panic.h include/kernel/task.h include/kernel/test.h include/kernel/timer.h include/mm/pmm.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/klog.o: src/kernel/klog.c include/kernel/console.h include/kernel/klog.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/initramfs.o: src/kernel/initramfs.c include/kernel/initramfs.h include/kernel/klog.h include/kernel/panic.h include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/kmalloc.o: src/kernel/kmalloc.c include/kernel/kmalloc.h include/kernel/memory_layout.h include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/memory_layout.o: src/kernel/memory_layout.c include/kernel/klog.h include/kernel/memory_layout.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/panic.o: src/kernel/panic.c include/arch/aarch64/cpu.h include/kernel/console.h include/kernel/panic.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/shell.o: src/kernel/shell.c include/block/block_device.h include/drivers/virtio.h include/drivers/virtio_rng.h include/fs/file.h include/fs/vfs.h include/kernel/console.h include/kernel/klog.h include/kernel/kmalloc.h include/kernel/memory_layout.h include/kernel/panic.h include/kernel/shell.h include/kernel/timer.h include/lib/string.h include/mm/pmm.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/spinlock.o: src/kernel/spinlock.c include/arch/aarch64/sysreg.h include/kernel/spinlock.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/syscall.o: src/kernel/syscall.c include/arch/aarch64/exceptions.h include/kernel/console.h include/kernel/syscall.h include/kernel/task.h include/kernel/timer.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/task.o: src/kernel/task.c include/arch/aarch64/sysreg.h include/kernel/klog.h include/kernel/kmalloc.h include/kernel/panic.h include/kernel/task.h include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/test.o: src/kernel/test.c include/arch/aarch64/cpu.h include/arch/aarch64/exceptions.h include/arch/aarch64/gic.h include/arch/aarch64/mmu.h include/arch/aarch64/sysreg.h include/block/block_device.h include/drivers/ramdisk.h include/drivers/virtio.h include/drivers/virtio_blk.h include/drivers/virtio_rng.h include/fs/file.h include/fs/tinyfs.h include/fs/vfs.h include/kernel/klog.h include/kernel/kmalloc.h include/kernel/memory_layout.h include/kernel/panic.h include/kernel/task.h include/kernel/test.h include/kernel/timer.h include/lib/string.h include/mm/pmm.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/timer.o: src/kernel/timer.c include/arch/aarch64/sysreg.h include/kernel/klog.h include/kernel/timer.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/user.o: src/kernel/user.c include/arch/aarch64/mmu.h include/arch/aarch64/sysreg.h include/fs/file.h include/kernel/klog.h include/kernel/panic.h include/kernel/user.h include/lib/string.h include/mm/pmm.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mm/pmm.o: src/mm/pmm.c include/kernel/klog.h include/kernel/memory_layout.h include/kernel/panic.h include/lib/string.h include/mm/pmm.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib/string.o: src/lib/string.c include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user/hello.o: src/user/hello.S | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user/hello.elf: $(BUILD_DIR)/user/hello.o src/user/user.ld
	$(LD) -T src/user/user.ld -nostdlib -n $(BUILD_DIR)/user/hello.o -o $@

$(BUILD_DIR)/user/hello.bin: $(BUILD_DIR)/user/hello.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/user/hello_blob.o: $(BUILD_DIR)/user/hello.bin
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

$(BUILD_DIR)/initramfs.img: tools/mkinitramfs.py initramfs/hello.txt initramfs/motd.txt | $(BUILD_DIR)
	$(PYTHON) tools/mkinitramfs.py $@ initramfs/hello.txt initramfs/motd.txt

$(BUILD_DIR)/initramfs_blob.o: $(BUILD_DIR)/initramfs.img
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

$(BUILD_DIR)/tinyfs.img: Makefile tools/mktinyfs.py rootfs/hello.txt rootfs/motd.txt rootfs/bin/init rootfs/etc/motd rootfs/home/readme.txt $(BUILD_DIR)/user/hello.bin $(BUILD_DIR)/user/hello.elf | $(BUILD_DIR)
	rm -rf $(BUILD_DIR)/rootfs
	mkdir -p $(BUILD_DIR)/rootfs/bin
	cp -R rootfs/. $(BUILD_DIR)/rootfs/
	cp $(BUILD_DIR)/user/hello.bin $(BUILD_DIR)/rootfs/bin/hello.bin
	cp $(BUILD_DIR)/user/hello.elf $(BUILD_DIR)/rootfs/bin/hello.elf
	$(PYTHON) tools/mktinyfs.py $@ $(BUILD_DIR)/rootfs

$(BUILD_DIR)/tinyfs_blob.o: $(BUILD_DIR)/tinyfs.img
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@

$(VIRTIO_BLK_IMG): tools/mkvirtio_blk.py | $(BUILD_DIR)
	$(PYTHON) tools/mkvirtio_blk.py $@

$(KERNEL_ELF): $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

run: all
	qemu-system-aarch64 \
		-M virt,gic-version=2 \
		-cpu cortex-a53 \
		-global virtio-mmio.force-legacy=false \
		-drive if=none,file=$(VIRTIO_BLK_IMG),format=raw,readonly=on,id=vdisk0 \
		-nographic \
		-serial mon:stdio \
		-device virtio-rng-device,bus=virtio-mmio-bus.0 \
		-device virtio-blk-device,drive=vdisk0,bus=virtio-mmio-bus.1 \
		-kernel $(KERNEL_ELF)

run-exception-test:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DEXCEPTION_SELF_TEST=1" all
	qemu-system-aarch64 \
		-M virt,gic-version=2 \
		-cpu cortex-a53 \
		-global virtio-mmio.force-legacy=false \
		-drive if=none,file=$(VIRTIO_BLK_IMG),format=raw,readonly=on,id=vdisk0 \
		-nographic \
		-serial mon:stdio \
		-device virtio-rng-device,bus=virtio-mmio-bus.0 \
		-device virtio-blk-device,drive=vdisk0,bus=virtio-mmio-bus.1 \
		-kernel $(KERNEL_ELF)

debug: all
	qemu-system-aarch64 \
		-M virt,gic-version=2 \
		-cpu cortex-a53 \
		-global virtio-mmio.force-legacy=false \
		-drive if=none,file=$(VIRTIO_BLK_IMG),format=raw,readonly=on,id=vdisk0 \
		-nographic \
		-serial mon:stdio \
		-device virtio-rng-device,bus=virtio-mmio-bus.0 \
		-device virtio-blk-device,drive=vdisk0,bus=virtio-mmio-bus.1 \
		-S \
		-s \
		-kernel $(KERNEL_ELF)

clean:
	rm -rf $(BUILD_DIR)
