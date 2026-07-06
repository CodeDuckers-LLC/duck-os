#ifndef FS_VFS_H
#define FS_VFS_H

#include "block/block_device.h"

#define VFS_FILE_FLAG_DIRECTORY 0x1U

typedef struct vfs_file_info
{
    const char *name;
    unsigned int size;
    unsigned int flags;
} vfs_file_info_t;

int vfs_mount_root(void);
int vfs_list(const char *path);
const vfs_file_info_t *vfs_stat(const char *path);
int vfs_read_file(const char *path, void *buffer, unsigned long max_size);
int vfs_read_file_part(const char *path, unsigned int offset, void *buffer, unsigned int size);

int vfs_is_mounted(void);
const char *vfs_root_fs_name(void);
block_device_t *vfs_root_device(void);
unsigned int vfs_root_file_count(void);

#endif
