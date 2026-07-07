#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

typedef struct pci_device_info
{
    unsigned int bus;
    unsigned int device;
    unsigned int function;
    unsigned int vendor_id;
    unsigned int device_id;
    unsigned int class_code;
    unsigned int subclass;
    unsigned int prog_if;
    unsigned int header_type;
    unsigned int bar_count;
    unsigned long bars[6];
} pci_device_info_t;

void pci_init(void);
int pci_available(void);
unsigned int pci_device_count(void);
const pci_device_info_t *pci_get_device(unsigned int index);
void pci_print_devices(void);

#endif
