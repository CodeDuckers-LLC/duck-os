#include "block/block_device.h"

#include "kernel/console.h"
#include "kernel/klog.h"
#include "lib/string.h"

#define BLOCK_DEVICE_MAX 8

static block_device_t *block_devices[BLOCK_DEVICE_MAX];
static unsigned int block_devices_total;

static void block_print_unsigned(unsigned long value)
{
    char buffer[2 * sizeof(unsigned long)];
    unsigned long length;

    if (value == 0)
    {
        console_putc('0');
        return;
    }

    length = 0;
    while (value != 0)
    {
        buffer[length++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    }

    while (length > 0)
    {
        length--;
        console_putc(buffer[length]);
    }
}

int block_register_device(block_device_t *device)
{
    unsigned int index;

    if (device == 0 ||
        device->name == 0 ||
        device->block_size == 0 ||
        device->block_count == 0 ||
        device->read_blocks == 0)
    {
        return -1;
    }

    for (index = 0; index < block_devices_total; index++)
    {
        if (block_devices[index] == device)
        {
            return -1;
        }
    }

    if (block_devices_total >= BLOCK_DEVICE_MAX)
    {
        return -1;
    }

    block_devices[block_devices_total] = device;
    block_devices_total++;

    kprintf("[INFO] block: registered %s\n", device->name);
    return 0;
}

block_device_t *block_get_device(unsigned int index)
{
    if (index >= block_devices_total)
    {
        return 0;
    }

    return block_devices[index];
}

block_device_t *block_find_device(const char *name)
{
    unsigned int index;

    if (name == 0)
    {
        return 0;
    }

    for (index = 0; index < block_devices_total; index++)
    {
        if (strcmp(block_devices[index]->name, name) == 0)
        {
            return block_devices[index];
        }
    }

    return 0;
}

unsigned int block_device_count(void)
{
    return block_devices_total;
}

void block_list_devices(void)
{
    unsigned int index;

    if (block_devices_total == 0)
    {
        kprintf("no block devices\n");
        return;
    }

    for (index = 0; index < block_devices_total; index++)
    {
        block_device_t *device;

        device = block_devices[index];

        kprintf("%u: %s block_size=%u blocks=",
                index,
                device->name,
                device->block_size);
        block_print_unsigned(device->block_count);
        kprintf(" %s\n", device->write_blocks != 0 ? "rw" : "ro");
    }
}
