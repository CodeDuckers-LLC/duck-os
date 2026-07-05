CROSS_COMPILE := aarch64-linux-gnu-

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

BUILD_DIR := build

CFLAGS := -Wall -Wextra -O2 -ffreestanding -nostdlib -nostartfiles -mgeneral-regs-only -Iinclude
LDFLAGS := -T linker.ld -nostdlib

OBJS := \
	$(BUILD_DIR)/arch/aarch64/boot.o \
	$(BUILD_DIR)/arch/aarch64/cpu.o \
	$(BUILD_DIR)/arch/aarch64/exceptions.o \
	$(BUILD_DIR)/arch/aarch64/exceptions_asm.o \
	$(BUILD_DIR)/arch/aarch64/gic.o \
	$(BUILD_DIR)/drivers/uart_pl011.o \
	$(BUILD_DIR)/platform/qemu_virt/platform.o \
	$(BUILD_DIR)/kernel/console.o \
	$(BUILD_DIR)/kernel/kernel.o \
	$(BUILD_DIR)/kernel/klog.o \
	$(BUILD_DIR)/kernel/kmalloc.o \
	$(BUILD_DIR)/kernel/memory_layout.o \
	$(BUILD_DIR)/kernel/panic.o \
	$(BUILD_DIR)/kernel/shell.o \
	$(BUILD_DIR)/kernel/test.o \
	$(BUILD_DIR)/kernel/timer.o \
	$(BUILD_DIR)/mm/pmm.o \
	$(BUILD_DIR)/lib/string.o

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin

.PHONY: all clean run debug run-exception-test

all: $(KERNEL_ELF) $(KERNEL_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/arch/aarch64/boot.o: src/arch/aarch64/boot.S | $(BUILD_DIR)
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

$(BUILD_DIR)/arch/aarch64/gic.o: src/arch/aarch64/gic.c include/arch/aarch64/gic.h | $(BUILD_DIR)
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

$(BUILD_DIR)/kernel/kernel.o: src/kernel/kernel.c include/arch/aarch64/cpu.h include/arch/aarch64/exceptions.h include/arch/aarch64/gic.h include/arch/aarch64/sysreg.h include/kernel/console.h include/kernel/klog.h include/kernel/test.h include/kernel/timer.h include/mm/pmm.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/klog.o: src/kernel/klog.c include/kernel/console.h include/kernel/klog.h | $(BUILD_DIR)
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

$(BUILD_DIR)/kernel/shell.o: src/kernel/shell.c include/kernel/console.h include/kernel/klog.h include/kernel/kmalloc.h include/kernel/memory_layout.h include/kernel/panic.h include/kernel/shell.h include/kernel/timer.h include/lib/string.h include/mm/pmm.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/test.o: src/kernel/test.c include/arch/aarch64/cpu.h include/arch/aarch64/exceptions.h include/arch/aarch64/gic.h include/arch/aarch64/sysreg.h include/kernel/klog.h include/kernel/kmalloc.h include/kernel/memory_layout.h include/kernel/panic.h include/kernel/test.h include/kernel/timer.h include/lib/string.h include/mm/pmm.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/timer.o: src/kernel/timer.c include/arch/aarch64/sysreg.h include/kernel/klog.h include/kernel/timer.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mm/pmm.o: src/mm/pmm.c include/kernel/klog.h include/kernel/memory_layout.h include/kernel/panic.h include/lib/string.h include/mm/pmm.h include/platform/platform.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib/string.o: src/lib/string.c include/lib/string.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

run: all
	qemu-system-aarch64 \
		-M virt \
		-cpu cortex-a53 \
		-nographic \
		-serial mon:stdio \
		-kernel $(KERNEL_ELF)

run-exception-test:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -DEXCEPTION_SELF_TEST=1" all
	qemu-system-aarch64 \
		-M virt \
		-cpu cortex-a53 \
		-nographic \
		-serial mon:stdio \
		-kernel $(KERNEL_ELF)

debug: all
	qemu-system-aarch64 \
		-M virt \
		-cpu cortex-a53 \
		-nographic \
		-serial mon:stdio \
		-S \
		-s \
		-kernel $(KERNEL_ELF)

clean:
	rm -rf $(BUILD_DIR)
