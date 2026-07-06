#ifndef BLOCK_BLOCK_DEVICE_H
#define BLOCK_BLOCK_DEVICE_H

struct block_device;

typedef int (*block_device_read_blocks_fn)(struct block_device *device,
                                           unsigned long start_block,
                                           unsigned long block_count,
                                           void *buffer);

typedef int (*block_device_write_blocks_fn)(struct block_device *device,
                                            unsigned long start_block,
                                            unsigned long block_count,
                                            const void *buffer);

typedef struct block_device
{
    const char *name;
    unsigned int block_size;
    unsigned long block_count;
    void *driver_data;
    block_device_read_blocks_fn read_blocks;
    block_device_write_blocks_fn write_blocks;
} block_device_t;

int block_register_device(block_device_t *device);
block_device_t *block_get_device(unsigned int index);
block_device_t *block_find_device(const char *name);
unsigned int block_device_count(void);
void block_list_devices(void);

#endif
