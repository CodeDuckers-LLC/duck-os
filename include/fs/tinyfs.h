#ifndef FS_TINYFS_H
#define FS_TINYFS_H

#include "block/block_device.h"

#define TINYFS_MAGIC 0x53465954U
#define TINYFS_VERSION 1U
#define TINYFS_NAME_MAX 64U
#define TINYFS_MAX_FILES 16U

typedef struct tinyfs_superblock
{
    unsigned int magic;
    unsigned int version;
    unsigned int file_count;
    unsigned int file_table_offset;
    unsigned int data_offset;
} tinyfs_superblock_t;

typedef struct tinyfs_file
{
    char name[TINYFS_NAME_MAX];
    unsigned int offset;
    unsigned int size;
    unsigned int flags;
} tinyfs_file_t;

int tinyfs_mount(block_device_t *device);
void tinyfs_list(void);
const tinyfs_file_t *tinyfs_find(const char *path);
const tinyfs_file_t *tinyfs_get_file(unsigned int index);
int tinyfs_read_file(const char *path, void *buffer, unsigned long max_size);
int tinyfs_read_file_part(const char *path, unsigned int offset, void *buffer, unsigned int size);

int tinyfs_is_mounted(void);
unsigned int tinyfs_file_count(void);
const tinyfs_superblock_t *tinyfs_superblock(void);
block_device_t *tinyfs_device(void);

#endif
