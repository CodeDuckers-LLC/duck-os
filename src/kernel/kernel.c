#include "arch/aarch64/exceptions.h"
#include "arch/aarch64/gic.h"
#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysreg.h"
#include "arch/aarch64/cpu.h"
#include "drivers/ramdisk.h"
#include "drivers/virtio_blk.h"
#include "drivers/virtio.h"
#include "drivers/virtio_rng.h"
#include "fs/tinyfs.h"
#include "fs/vfs.h"
#include "kernel/console.h"
#include "kernel/initramfs.h"
#include "kernel/klog.h"
#include "kernel/panic.h"
#include "kernel/shell.h"
#include "kernel/test.h"
#include "kernel/task.h"
#include "kernel/timer.h"
#include "platform/platform.h"
#include "mm/pmm.h"

static void print_hex32(unsigned long value)
{
    int shift;

    console_write("0x");

    for (shift = 28; shift >= 0; shift -= 4)
    {
        unsigned long digit;

        digit = (value >> shift) & 0xfUL;
        if (digit < 10)
        {
            console_putc((char)('0' + digit));
        }
        else
        {
            console_putc((char)('a' + (digit - 10)));
        }
    }
}

static int kernel_load_tinyfs_image(block_device_t *device)
{
    unsigned long image_size;
    unsigned long block_count;

    extern unsigned char _binary_build_tinyfs_img_start[];
    extern unsigned char _binary_build_tinyfs_img_end[];

    if (device == 0 || device->write_blocks == 0)
    {
        return -1;
    }

    image_size = (unsigned long)(_binary_build_tinyfs_img_end - _binary_build_tinyfs_img_start);
    if (image_size == 0 || (image_size % device->block_size) != 0)
    {
        return -1;
    }

    block_count = image_size / device->block_size;
    if (block_count > device->block_count)
    {
        return -1;
    }

    kprintf("[INFO] embedded tinyfs image size: %u bytes\n", (unsigned int)image_size);
    return device->write_blocks(device, 0, block_count, _binary_build_tinyfs_img_start);
}

void kernel_main(void)
{
    unsigned long ram_start;
    unsigned long ram_end;
    block_device_t *ramdisk;

    console_init();
    sysreg_set_daif_irq();
    ram_start = platform_get_ram_base();
    ram_end = ram_start + platform_get_ram_size();
    klog_info("Custom Pi OS kernel booted");
    kprintf("platform: %s\n", platform_name());
    console_write("uart:     ");
    print_hex32(platform_get_uart0_base());
    console_putc('\n');
    console_write("ram:      ");
    print_hex32(ram_start);
    console_write(" - ");
    print_hex32(ram_end);
    console_putc('\n');
    klog_info("UART is working");
    exceptions_init();
    timer_init();
    mmu_init();
    pmm_init();
    virtio_init();
    virtio_rng_init();
    ramdisk_init();
    virtio_blk_init();
    ramdisk = ramdisk_device();
    if (kernel_load_tinyfs_image(ramdisk) != 0)
    {
        panic("tinyfs image load failed");
    }
    if (tinyfs_mount(ramdisk) != 0)
    {
        panic("tinyfs mount failed");
    }
    klog_info("TinyFS mounted");
    if (vfs_mount_root() != 0)
    {
        panic("vfs root mount failed");
    }
    initramfs_init();
    task_init();
    test_run_all();

    gic_init();
    gic_enable_irq(GIC_IRQ_TIMER_PHYSICAL_PPI);
    virtio_enable_irqs();
    timer_start_periodic_ms(10);

    kprintf("timer interrupts active: %s, period 10 ms\n", gic_version_name());
    sysreg_clear_daif_irq();
    klog_info("IRQ unmasked");
    task_run_preemptive_demo();

    shell_run();
}
