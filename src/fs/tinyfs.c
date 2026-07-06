#include "fs/tinyfs.h"

#include "kernel/klog.h"
#include "lib/string.h"

#define TINYFS_IO_BUFFER_SIZE 512U

static block_device_t *tinyfs_block_device;
static tinyfs_superblock_t tinyfs_header;
static tinyfs_file_t tinyfs_files[TINYFS_MAX_FILES];
static unsigned char tinyfs_block_buffer[TINYFS_IO_BUFFER_SIZE];
static int tinyfs_ready;

static int tinyfs_read_bytes(unsigned int offset, void *buffer, unsigned long size)
{
    unsigned char *dest;
    unsigned long copied;
    unsigned int block_size;

    if (tinyfs_block_device == 0 || buffer == 0)
    {
        return -1;
    }

    dest = (unsigned char *)buffer;
    copied = 0;
    block_size = tinyfs_block_device->block_size;

    while (copied < size)
    {
        unsigned long block_index;
        unsigned long block_offset;
        unsigned long chunk_size;

        block_index = (offset + copied) / block_size;
        block_offset = (offset + copied) % block_size;
        chunk_size = block_size - block_offset;
        if (chunk_size > (size - copied))
        {
            chunk_size = size - copied;
        }

        if (tinyfs_block_device->read_blocks(tinyfs_block_device,
                                             block_index,
                                             1,
                                             tinyfs_block_buffer) != 0)
        {
            return -1;
        }

        memcpy(dest + copied, tinyfs_block_buffer + block_offset, chunk_size);
        copied += chunk_size;
    }

    return 0;
}

int tinyfs_mount(block_device_t *device)
{
    unsigned int entry_offset;
    unsigned int index;

    tinyfs_block_device = 0;
    tinyfs_ready = 0;

    if (device == 0 || device->read_blocks == 0 || device->block_size > sizeof(tinyfs_block_buffer))
    {
        return -1;
    }

    tinyfs_block_device = device;

    if (tinyfs_read_bytes(0, &tinyfs_header, sizeof(tinyfs_header)) != 0)
    {
        tinyfs_block_device = 0;
        return -1;
    }

    if (tinyfs_header.magic != TINYFS_MAGIC ||
        tinyfs_header.version != TINYFS_VERSION ||
        tinyfs_header.file_count > TINYFS_MAX_FILES)
    {
        tinyfs_block_device = 0;
        return -1;
    }

    entry_offset = tinyfs_header.file_table_offset;
    for (index = 0; index < tinyfs_header.file_count; index++)
    {
        if (tinyfs_read_bytes(entry_offset, &tinyfs_files[index], sizeof(tinyfs_files[index])) != 0)
        {
            tinyfs_block_device = 0;
            return -1;
        }

        tinyfs_files[index].name[TINYFS_NAME_MAX - 1] = '\0';
        entry_offset += sizeof(tinyfs_files[index]);
    }

    tinyfs_ready = 1;
    kprintf("[INFO] tinyfs: mounted %s files=%u\n",
            device->name,
            tinyfs_header.file_count);
    return 0;
}

void tinyfs_list(void)
{
    unsigned int index;

    if (!tinyfs_ready)
    {
        kprintf("tinyfs not mounted\n");
        return;
    }

    for (index = 0; index < tinyfs_header.file_count; index++)
    {
        kprintf("%s\n", tinyfs_files[index].name);
    }
}

const tinyfs_file_t *tinyfs_find(const char *path)
{
    unsigned int index;

    if (!tinyfs_ready || path == 0)
    {
        return 0;
    }

    for (index = 0; index < tinyfs_header.file_count; index++)
    {
        if (strcmp(tinyfs_files[index].name, path) == 0)
        {
            return &tinyfs_files[index];
        }
    }

    return 0;
}

const tinyfs_file_t *tinyfs_get_file(unsigned int index)
{
    if (!tinyfs_ready || index >= tinyfs_header.file_count)
    {
        return 0;
    }

    return &tinyfs_files[index];
}

int tinyfs_read_file(const char *path, void *buffer, unsigned long max_size)
{
    const tinyfs_file_t *file;

    file = tinyfs_find(path);
    if (file == 0 || buffer == 0 || file->size > max_size)
    {
        return -1;
    }

    if (tinyfs_read_bytes(file->offset, buffer, file->size) != 0)
    {
        return -1;
    }

    return (int)file->size;
}

int tinyfs_read_file_part(const char *path, unsigned int offset, void *buffer, unsigned int size)
{
    const tinyfs_file_t *file;
    unsigned int available;

    file = tinyfs_find(path);
    if (file == 0 || buffer == 0 || offset > file->size)
    {
        return -1;
    }

    available = file->size - offset;
    if (size > available)
    {
        size = available;
    }

    if (size == 0)
    {
        return 0;
    }

    if (tinyfs_read_bytes(file->offset + offset, buffer, size) != 0)
    {
        return -1;
    }

    return (int)size;
}

int tinyfs_is_mounted(void)
{
    return tinyfs_ready;
}

unsigned int tinyfs_file_count(void)
{
    if (!tinyfs_ready)
    {
        return 0;
    }

    return tinyfs_header.file_count;
}

const tinyfs_superblock_t *tinyfs_superblock(void)
{
    if (!tinyfs_ready)
    {
        return 0;
    }

    return &tinyfs_header;
}

block_device_t *tinyfs_device(void)
{
    if (!tinyfs_ready)
    {
        return 0;
    }

    return tinyfs_block_device;
}
