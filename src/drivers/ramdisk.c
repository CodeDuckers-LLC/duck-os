#include "drivers/ramdisk.h"

#include "kernel/klog.h"
#include "lib/string.h"

static unsigned char ramdisk_storage[RAMDISK_BLOCK_SIZE * RAMDISK_BLOCK_COUNT];
static block_device_t ramdisk0_device;
static int ramdisk0_initialized;

static int ramdisk_range_valid(unsigned long start_block, unsigned long block_count)
{
    unsigned long end_block;

    if (block_count == 0)
    {
        return 0;
    }

    end_block = start_block + block_count;
    if (end_block < start_block)
    {
        return 0;
    }

    return end_block <= RAMDISK_BLOCK_COUNT;
}

static int ramdisk_read_blocks(struct block_device *device,
                               unsigned long start_block,
                               unsigned long block_count,
                               void *buffer)
{
    unsigned long offset;
    unsigned long size;

    if (device == 0 || buffer == 0 || !ramdisk_range_valid(start_block, block_count))
    {
        return -1;
    }

    offset = start_block * RAMDISK_BLOCK_SIZE;
    size = block_count * RAMDISK_BLOCK_SIZE;
    memcpy(buffer, &ramdisk_storage[offset], size);
    return 0;
}

static int ramdisk_write_blocks(struct block_device *device,
                                unsigned long start_block,
                                unsigned long block_count,
                                const void *buffer)
{
    unsigned long offset;
    unsigned long size;

    if (device == 0 || buffer == 0 || !ramdisk_range_valid(start_block, block_count))
    {
        return -1;
    }

    offset = start_block * RAMDISK_BLOCK_SIZE;
    size = block_count * RAMDISK_BLOCK_SIZE;
    memcpy(&ramdisk_storage[offset], buffer, size);
    return 0;
}

void ramdisk_init(void)
{
    if (ramdisk0_initialized)
    {
        return;
    }

    ramdisk0_device.name = "ram0";
    ramdisk0_device.block_size = RAMDISK_BLOCK_SIZE;
    ramdisk0_device.block_count = RAMDISK_BLOCK_COUNT;
    ramdisk0_device.driver_data = ramdisk_storage;
    ramdisk0_device.read_blocks = ramdisk_read_blocks;
    ramdisk0_device.write_blocks = ramdisk_write_blocks;

    if (block_register_device(&ramdisk0_device) != 0)
    {
        klog_error("ramdisk: register failed");
        return;
    }

    ramdisk0_initialized = 1;
    kprintf("[INFO] ramdisk: %s initialized (%u bytes)\n",
            ramdisk0_device.name,
            (unsigned int)(RAMDISK_BLOCK_SIZE * RAMDISK_BLOCK_COUNT));
}

block_device_t *ramdisk_device(void)
{
    if (!ramdisk0_initialized)
    {
        return 0;
    }

    return &ramdisk0_device;
}
