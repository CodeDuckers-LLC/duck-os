#include "fs/file.h"

#include "fs/vfs.h"

#define FILE_TABLE_MAX 8

static file_t file_table[FILE_TABLE_MAX];

file_t *file_open(const char *path)
{
    const vfs_file_info_t *info;
    unsigned int index;

    info = vfs_stat(path);
    if (info == 0 || (info->flags & VFS_FILE_FLAG_DIRECTORY) != 0)
    {
        return 0;
    }

    for (index = 0; index < FILE_TABLE_MAX; index++)
    {
        if (!file_table[index].in_use)
        {
            file_table[index].path = info->name;
            file_table[index].size = info->size;
            file_table[index].offset = 0;
            file_table[index].flags = info->flags;
            file_table[index].in_use = 1;
            return &file_table[index];
        }
    }

    return 0;
}

int file_read(file_t *file, void *buffer, unsigned int size)
{
    unsigned int remaining;
    int read_size;

    if (file == 0 || !file->in_use || buffer == 0)
    {
        return -1;
    }

    if (file->offset >= file->size)
    {
        return 0;
    }

    remaining = file->size - file->offset;
    if (size > remaining)
    {
        size = remaining;
    }

    read_size = vfs_read_file_part(file->path, file->offset, buffer, size);
    if (read_size < 0)
    {
        return -1;
    }

    file->offset += (unsigned int)read_size;
    return read_size;
}

int file_seek(file_t *file, unsigned int offset)
{
    if (file == 0 || !file->in_use || offset > file->size)
    {
        return -1;
    }

    file->offset = offset;
    return 0;
}

void file_close(file_t *file)
{
    if (file == 0)
    {
        return;
    }

    file->path = 0;
    file->size = 0;
    file->offset = 0;
    file->flags = 0;
    file->in_use = 0;
}
