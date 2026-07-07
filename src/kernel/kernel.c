#include "block/block_device.h"
#include "arch/aarch64/exceptions.h"
#include "arch/aarch64/gic.h"
#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysreg.h"
#include "arch/aarch64/cpu.h"
#include "drivers/ramdisk.h"
#include "drivers/pci.h"
#include "drivers/virtio_blk.h"
#include "drivers/virtio_gpu.h"
#include "drivers/virtio_input.h"
#include "drivers/virtio.h"
#include "drivers/virtio_rng.h"
#include "fs/logfs.h"
#include "fs/tinyfs.h"
#include "fs/vfs.h"
#include "kernel/console.h"
#include "kernel/initramfs.h"
#include "kernel/input.h"
#include "kernel/klog.h"
#include "kernel/panic.h"
#include "kernel/shell.h"
#include "kernel/test.h"
#include "kernel/task.h"
#include "kernel/timer.h"
#include "lib/string.h"
#include "platform/platform.h"
#include "mm/pmm.h"

#define KERNEL_DEFAULT_HOSTNAME "duck-os"
#define KERNEL_BOOT_CONFIG_MAX 128U

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

static unsigned int kernel_trim_text(char *text)
{
    unsigned int length;

    length = (unsigned int)strlen(text);
    while (length > 0U)
    {
        char ch;

        ch = text[length - 1U];
        if (ch != '\n' && ch != '\r' && ch != ' ' && ch != '\t')
        {
            break;
        }

        text[length - 1U] = '\0';
        length--;
    }

    return length;
}

static int kernel_read_boot_text(const char *path, char *buffer, unsigned int buffer_size)
{
    int read_size;

    if (path == 0 || buffer == 0 || buffer_size < 2U)
    {
        return -1;
    }

    read_size = vfs_read_file(path, buffer, buffer_size - 1U);
    if (read_size <= 0)
    {
        return -1;
    }

    buffer[read_size] = '\0';
    return (int)kernel_trim_text(buffer);
}

static void kernel_print_boot_config(void)
{
    char hostname[KERNEL_BOOT_CONFIG_MAX];
    char motd[KERNEL_BOOT_CONFIG_MAX];
    char theme[KERNEL_BOOT_CONFIG_MAX];
    int hostname_length;
    int motd_length;
    int theme_length;

    strlcpy(hostname, KERNEL_DEFAULT_HOSTNAME, sizeof(hostname));
    hostname_length = kernel_read_boot_text("/etc/hostname", hostname, sizeof(hostname));
    if (hostname_length < 0 || hostname[0] == '\0')
    {
        strlcpy(hostname, KERNEL_DEFAULT_HOSTNAME, sizeof(hostname));
    }

    kprintf("hostname: %s\n", hostname);

    motd_length = kernel_read_boot_text("/etc/motd", motd, sizeof(motd));
    if (motd_length >= 0 && motd[0] != '\0')
    {
        kprintf("motd: %s\n", motd);
    }

    theme_length = kernel_read_boot_text("/etc/theme", theme, sizeof(theme));
    if (theme_length >= 0 && theme[0] != '\0')
    {
        kprintf("[INFO] theme: %s\n", theme);
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

static int kernel_mount_root_tinyfs(block_device_t *ramdisk)
{
    block_device_t *virtio_root;

    virtio_root = block_find_device("vda");
    if (virtio_root != 0)
    {
        if (tinyfs_mount(virtio_root) == 0)
        {
            kprintf("[INFO] rootfs: mounted tinyfs from %s\n", virtio_root->name);
            return 0;
        }

        kprintf("[INFO] rootfs: %s does not contain a valid tinyfs image, falling back to %s\n",
                virtio_root->name,
                ramdisk->name);
    }
    else
    {
        kprintf("[INFO] rootfs: vda unavailable, falling back to %s\n", ramdisk->name);
    }

    if (kernel_load_tinyfs_image(ramdisk) != 0)
    {
        return -1;
    }

    if (tinyfs_mount(ramdisk) != 0)
    {
        return -1;
    }

    kprintf("[INFO] rootfs: mounted tinyfs from %s\n", ramdisk->name);
    return 0;
}

static void kernel_mount_logfs(void)
{
    block_device_t *device;

    device = block_find_device("vda");
    if (device == 0)
    {
        kprintf("[INFO] logfs: vda unavailable\n");
        return;
    }

    if (logfs_mount(device) != 0)
    {
        kprintf("[INFO] logfs: %s not mounted\n", device->name);
        return;
    }
}

void kernel_main(void)
{
    unsigned long ram_start;
    unsigned long ram_end;
    block_device_t *ramdisk;

    console_init();
    input_init();
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
    pci_init();
    virtio_init();
    virtio_rng_init();
    ramdisk_init();
    virtio_blk_init();
    virtio_gpu_init();
    virtio_input_init();
    if (virtio_gpu_available())
    {
        console_attach_graphics((framebuffer_t *)virtio_gpu_framebuffer());
        console_set_output_mode(CONSOLE_SINK_SERIAL | CONSOLE_SINK_GRAPHICS);
    }
    if (virtio_input_available())
    {
        console_set_input_mode(INPUT_SOURCE_SERIAL | INPUT_SOURCE_KEYBOARD);
    }
    ramdisk = ramdisk_device();
    if (ramdisk == 0)
    {
        panic("ramdisk unavailable");
    }
    if (kernel_mount_root_tinyfs(ramdisk) != 0)
    {
        panic("tinyfs root mount failed");
    }
    if (vfs_mount_root() != 0)
    {
        panic("vfs root mount failed");
    }
    kernel_print_boot_config();
    kernel_mount_logfs();
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
