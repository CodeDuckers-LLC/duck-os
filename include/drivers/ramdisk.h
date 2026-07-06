#ifndef DRIVERS_RAMDISK_H
#define DRIVERS_RAMDISK_H

#include "block/block_device.h"

#define RAMDISK_BLOCK_SIZE 512U
#define RAMDISK_BLOCK_COUNT 128UL

void ramdisk_init(void);
block_device_t *ramdisk_device(void);

#endif
