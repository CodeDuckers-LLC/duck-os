#include "drivers/pci.h"
#include "kernel/klog.h"
#include "platform/platform.h"

#define PCI_MAX_DEVICES 64U
#define PCI_MAX_FUNCTIONS 8U
#define PCI_MAX_BARS 6U
#define PCI_INVALID_VENDOR_ID 0xffffU
#define PCI_HEADER_TYPE_MASK 0x7fU
#define PCI_HEADER_TYPE_BRIDGE 0x01U
#define PCI_HEADER_TYPE_MULTIFUNCTION 0x80U

static pci_device_info_t pci_devices[PCI_MAX_DEVICES];
static unsigned int pci_devices_found;
static int pci_initialized;

static unsigned long pci_config_address(unsigned int bus,
                                        unsigned int device,
                                        unsigned int function,
                                        unsigned int offset)
{
    return platform_get_pci_ecam_base() +
           ((unsigned long)bus << 20) +
           ((unsigned long)device << 15) +
           ((unsigned long)function << 12) +
           (unsigned long)offset;
}

static unsigned int pci_read_config32(unsigned int bus,
                                      unsigned int device,
                                      unsigned int function,
                                      unsigned int offset)
{
    volatile unsigned int *config;

    config = (volatile unsigned int *)pci_config_address(bus, device, function, offset);
    return *config;
}

static unsigned int pci_read_vendor_id(unsigned int bus,
                                       unsigned int device,
                                       unsigned int function)
{
    return pci_read_config32(bus, device, function, 0x00U) & 0xffffU;
}

static const char *pci_class_name(unsigned int class_code, unsigned int subclass)
{
    if (class_code == 0x01U && subclass == 0x06U)
    {
        return "SATA controller";
    }

    if (class_code == 0x02U)
    {
        return "Network controller";
    }

    if (class_code == 0x03U)
    {
        return "Display controller";
    }

    if (class_code == 0x06U && subclass == 0x00U)
    {
        return "Host bridge";
    }

    if (class_code == 0x06U && subclass == 0x04U)
    {
        return "PCI bridge";
    }

    if (class_code == 0x0cU && subclass == 0x03U)
    {
        return "USB controller";
    }

    return "Unknown";
}

static void pci_store_device(unsigned int bus,
                             unsigned int device,
                             unsigned int function)
{
    pci_device_info_t *info;
    unsigned int id;
    unsigned int class_reg;
    unsigned int header_reg;
    unsigned int bar_index;
    unsigned int bar_limit;
    unsigned int config_offset;

    if (pci_devices_found >= PCI_MAX_DEVICES)
    {
        return;
    }

    info = &pci_devices[pci_devices_found];
    id = pci_read_config32(bus, device, function, 0x00U);
    class_reg = pci_read_config32(bus, device, function, 0x08U);
    header_reg = pci_read_config32(bus, device, function, 0x0cU);

    info->bus = bus;
    info->device = device;
    info->function = function;
    info->vendor_id = id & 0xffffU;
    info->device_id = (id >> 16) & 0xffffU;
    info->prog_if = (class_reg >> 8) & 0xffU;
    info->subclass = (class_reg >> 16) & 0xffU;
    info->class_code = (class_reg >> 24) & 0xffU;
    info->header_type = (header_reg >> 16) & 0xffU;

    if ((info->header_type & PCI_HEADER_TYPE_MASK) == PCI_HEADER_TYPE_BRIDGE)
    {
        bar_limit = 2U;
    }
    else
    {
        bar_limit = PCI_MAX_BARS;
    }

    info->bar_count = bar_limit;
    for (bar_index = 0; bar_index < bar_limit; bar_index++)
    {
        unsigned int bar_value;

        config_offset = 0x10U + (bar_index * 4U);
        bar_value = pci_read_config32(bus, device, function, config_offset);
        info->bars[bar_index] = (unsigned long)bar_value;

        if ((bar_value & 0x01U) == 0U && (bar_value & 0x06U) == 0x04U && (bar_index + 1U) < bar_limit)
        {
            unsigned int upper;

            upper = pci_read_config32(bus, device, function, config_offset + 4U);
            info->bars[bar_index] |= ((unsigned long)upper << 32);
            bar_index++;
            info->bars[bar_index] = 0UL;
        }
    }

    pci_devices_found++;
}

void pci_init(void)
{
    unsigned int bus;
    unsigned int device;
    unsigned int function;
    unsigned int bus_start;
    unsigned int bus_end;

    if (pci_initialized)
    {
        return;
    }

    pci_initialized = 1;
    pci_devices_found = 0;

    if (!pci_available())
    {
        kprintf("[INFO] pci: unavailable\n");
        return;
    }

    bus_start = platform_get_pci_bus_start();
    bus_end = platform_get_pci_bus_end();

    for (bus = bus_start; bus <= bus_end; bus++)
    {
        for (device = 0; device < 32U; device++)
        {
            unsigned int header_type;
            unsigned int function_limit;

            if (pci_read_vendor_id(bus, device, 0U) == PCI_INVALID_VENDOR_ID)
            {
                continue;
            }

            header_type = (pci_read_config32(bus, device, 0U, 0x0cU) >> 16) & 0xffU;
            function_limit = (header_type & PCI_HEADER_TYPE_MULTIFUNCTION) != 0U ? PCI_MAX_FUNCTIONS : 1U;

            for (function = 0; function < function_limit; function++)
            {
                if (pci_read_vendor_id(bus, device, function) == PCI_INVALID_VENDOR_ID)
                {
                    continue;
                }

                pci_store_device(bus, device, function);
            }
        }
    }

    kprintf("[INFO] pci: enumerated %u device(s)\n", pci_devices_found);
}

int pci_available(void)
{
    return platform_get_pci_ecam_base() != 0UL &&
           platform_get_pci_ecam_size() != 0UL &&
           platform_get_pci_bus_end() >= platform_get_pci_bus_start();
}

unsigned int pci_device_count(void)
{
    return pci_devices_found;
}

const pci_device_info_t *pci_get_device(unsigned int index)
{
    if (index >= pci_devices_found)
    {
        return 0;
    }

    return &pci_devices[index];
}

void pci_print_devices(void)
{
    unsigned int index;

    if (!pci_available())
    {
        kprintf("pci unavailable\n");
        return;
    }

    if (pci_devices_found == 0U)
    {
        kprintf("pci: no devices\n");
        return;
    }

    for (index = 0; index < pci_devices_found; index++)
    {
        const pci_device_info_t *info;
        unsigned int bar;

        info = &pci_devices[index];
        kprintf("%u:%u.%u vendor=%x device=%x class=%x:%x:%x %s\n",
                info->bus,
                info->device,
                info->function,
                info->vendor_id,
                info->device_id,
                info->class_code,
                info->subclass,
                info->prog_if,
                pci_class_name(info->class_code, info->subclass));

        for (bar = 0; bar < info->bar_count; bar++)
        {
            if (info->bars[bar] == 0UL)
            {
                continue;
            }

            kprintf("  BAR%u: %p\n", bar, (void *)info->bars[bar]);
        }
    }
}
