#ifndef FS_LOGFS_H
#define FS_LOGFS_H

#include "block/block_device.h"

#define LOGFS_MAGIC 0x5346474cU
#define LOGFS_VERSION 1U
#define LOGFS_NAME_MAX 32U
#define LOGFS_MAX_FILES 16U
#define LOGFS_MAX_SEGMENTS 64U

typedef struct logfs_file_info
{
    const char *name;
    unsigned int size;
} logfs_file_info_t;

int logfs_mount(block_device_t *device);
int logfs_is_mounted(void);
block_device_t *logfs_device(void);
unsigned int logfs_file_count(void);
const logfs_file_info_t *logfs_get_file(unsigned int index);
const logfs_file_info_t *logfs_stat(const char *name);
int logfs_create(const char *name);
int logfs_append(const char *name, const void *data, unsigned int size);
int logfs_read_file(const char *name, void *buffer, unsigned int max_size);
int logfs_read_file_part(const char *name, unsigned int offset, void *buffer, unsigned int size);

#endif
