#include "platform/platform.h"

#define QEMU_VIRT_UART0_BASE 0x09000000UL
#define QEMU_VIRT_VIRTIO_MMIO_BASE 0x0a000000UL
#define QEMU_VIRT_VIRTIO_MMIO_STRIDE 0x00000200UL
#define QEMU_VIRT_VIRTIO_MMIO_COUNT 32U
#define QEMU_VIRT_VIRTIO_MMIO_IRQ_BASE 48U
#define QEMU_VIRT_RAM_BASE 0x40000000UL
#define QEMU_VIRT_RAM_SIZE (128UL * 1024UL * 1024UL)

const char *platform_name(void)
{
    return "qemu-virt";
}

unsigned long platform_get_uart0_base(void)
{
    return QEMU_VIRT_UART0_BASE;
}

unsigned long platform_get_ram_base(void)
{
    return QEMU_VIRT_RAM_BASE;
}

unsigned long platform_get_ram_size(void)
{
    return QEMU_VIRT_RAM_SIZE;
}

unsigned long platform_get_virtio_mmio_base(void)
{
    return QEMU_VIRT_VIRTIO_MMIO_BASE;
}

unsigned long platform_get_virtio_mmio_stride(void)
{
    return QEMU_VIRT_VIRTIO_MMIO_STRIDE;
}

unsigned int platform_get_virtio_mmio_count(void)
{
    return QEMU_VIRT_VIRTIO_MMIO_COUNT;
}

unsigned int platform_get_virtio_mmio_irq(unsigned int slot)
{
    return QEMU_VIRT_VIRTIO_MMIO_IRQ_BASE + slot;
}
