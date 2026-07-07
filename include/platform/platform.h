#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

unsigned long platform_get_uart0_base(void);
unsigned long platform_get_ram_base(void);
unsigned long platform_get_ram_size(void);
unsigned long platform_get_virtio_mmio_base(void);
unsigned long platform_get_virtio_mmio_stride(void);
unsigned int platform_get_virtio_mmio_count(void);
unsigned int platform_get_virtio_mmio_irq(unsigned int slot);
unsigned long platform_get_pci_ecam_base(void);
unsigned long platform_get_pci_ecam_size(void);
unsigned int platform_get_pci_bus_start(void);
unsigned int platform_get_pci_bus_end(void);
const char *platform_name(void);

#endif
