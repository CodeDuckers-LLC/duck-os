#include "fs/vfs.h"

#include "fs/tinyfs.h"
#include "kernel/klog.h"
#include "lib/string.h"

#define VFS_PATH_MAX TINYFS_NAME_MAX

static vfs_file_info_t vfs_stat_info;
static char vfs_stat_name[VFS_PATH_MAX];
static int vfs_ready;

static int vfs_normalize_path(const char *path, char *buffer, int allow_root)
{
    unsigned int length;

    if (path == 0 || buffer == 0)
    {
        return -1;
    }

    while (*path == '/')
    {
        path++;
    }

    if (*path == '\0')
    {
        if (!allow_root)
        {
            return -1;
        }

        buffer[0] = '\0';
        return 0;
    }

    length = 0;
    while (*path != '\0')
    {
        unsigned int segment_length;
        const char *segment;

        segment = path;
        segment_length = 0;

        while (path[segment_length] != '\0' && path[segment_length] != '/')
        {
            segment_length++;
        }

        if ((segment_length == 1 && segment[0] == '.') ||
            (segment_length == 2 && segment[0] == '.' && segment[1] == '.'))
        {
            return -1;
        }

        if (length != 0)
        {
            if (length + 1 >= VFS_PATH_MAX)
            {
                return -1;
            }

            buffer[length++] = '/';
        }

        if (length + segment_length >= VFS_PATH_MAX)
        {
            return -1;
        }

        memcpy(buffer + length, segment, segment_length);
        length += segment_length;

        path += segment_length;
        while (*path == '/')
        {
            path++;
        }
    }

    buffer[length] = '\0';
    return 0;
}

static int vfs_name_seen(char seen[][VFS_PATH_MAX], unsigned int seen_count, const char *name, unsigned int length)
{
    unsigned int index;

    for (index = 0; index < seen_count; index++)
    {
        if (seen[index][length] == '\0' && memcmp(seen[index], name, length) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static int vfs_collect_child_name(const char *directory,
                                  const char *file_name,
                                  const char **child_name,
                                  unsigned int *child_length,
                                  int *is_directory)
{
    unsigned int directory_length;
    const char *remainder;
    unsigned int index;

    directory_length = strlen(directory);
    if (directory_length == 0)
    {
        remainder = file_name;
    }
    else
    {
        if (memcmp(file_name, directory, directory_length) != 0 || file_name[directory_length] != '/')
        {
            return 0;
        }

        remainder = file_name + directory_length + 1;
    }

    if (*remainder == '\0')
    {
        return 0;
    }

    *child_name = remainder;
    *child_length = 0;
    *is_directory = 0;

    for (index = 0; remainder[index] != '\0'; index++)
    {
        if (remainder[index] == '/')
        {
            *child_length = index;
            *is_directory = 1;
            return 1;
        }
    }

    *child_length = index;
    return 1;
}

static int vfs_directory_exists(const char *directory)
{
    unsigned int index;

    if (directory[0] == '\0')
    {
        return 1;
    }

    for (index = 0; index < tinyfs_file_count(); index++)
    {
        const tinyfs_file_t *file;
        const char *child_name;
        unsigned int child_length;
        int is_directory;

        file = tinyfs_get_file(index);
        if (file == 0)
        {
            continue;
        }

        if (vfs_collect_child_name(directory,
                                   file->name,
                                   &child_name,
                                   &child_length,
                                   &is_directory))
        {
            return 1;
        }
    }

    return 0;
}

int vfs_mount_root(void)
{
    block_device_t *device;

    if (!tinyfs_is_mounted())
    {
        return -1;
    }

    device = tinyfs_device();
    if (device == 0)
    {
        return -1;
    }

    vfs_ready = 1;
    kprintf("[INFO] vfs: root mounted fs=%s device=%s\n",
            vfs_root_fs_name(),
            device->name);
    return 0;
}

int vfs_list(const char *path)
{
    char normalized[VFS_PATH_MAX];
    char seen[TINYFS_MAX_FILES][VFS_PATH_MAX];
    unsigned int seen_count;
    unsigned int index;

    if (!vfs_ready || vfs_normalize_path(path, normalized, 1) != 0 || !vfs_directory_exists(normalized))
    {
        return -1;
    }

    seen_count = 0;
    for (index = 0; index < tinyfs_file_count(); index++)
    {
        const tinyfs_file_t *file;
        const char *child_name;
        unsigned int child_length;
        int is_directory;

        file = tinyfs_get_file(index);
        if (file == 0)
        {
            continue;
        }

        if (!vfs_collect_child_name(normalized,
                                    file->name,
                                    &child_name,
                                    &child_length,
                                    &is_directory))
        {
            continue;
        }

        if (vfs_name_seen(seen, seen_count, child_name, child_length))
        {
            continue;
        }

        memcpy(seen[seen_count], child_name, child_length);
        seen[seen_count][child_length] = '\0';
        seen_count++;

        kprintf("%s\n", seen[seen_count - 1]);
    }

    return 0;
}

int vfs_list_entries(const char *path, vfs_list_entry_fn_t callback, void *context)
{
    char normalized[VFS_PATH_MAX];
    char seen[TINYFS_MAX_FILES][VFS_PATH_MAX];
    char entry_name[VFS_PATH_MAX];
    vfs_file_info_t info;
    unsigned int seen_count;
    unsigned int index;

    if (!vfs_ready || callback == 0 || vfs_normalize_path(path, normalized, 1) != 0 || !vfs_directory_exists(normalized))
    {
        return -1;
    }

    seen_count = 0;
    for (index = 0; index < tinyfs_file_count(); index++)
    {
        const tinyfs_file_t *file;
        const char *child_name;
        unsigned int child_length;
        int is_directory;

        file = tinyfs_get_file(index);
        if (file == 0)
        {
            continue;
        }

        if (!vfs_collect_child_name(normalized,
                                    file->name,
                                    &child_name,
                                    &child_length,
                                    &is_directory))
        {
            continue;
        }

        if (vfs_name_seen(seen, seen_count, child_name, child_length))
        {
            continue;
        }

        memcpy(seen[seen_count], child_name, child_length);
        seen[seen_count][child_length] = '\0';
        seen_count++;

        memcpy(entry_name, child_name, child_length);
        entry_name[child_length] = '\0';
        info.name = entry_name;
        info.size = is_directory ? 0U : file->size;
        info.flags = is_directory ? VFS_FILE_FLAG_DIRECTORY : file->flags;
        if (callback(&info, context) != 0)
        {
            break;
        }
    }

    return 0;
}

const vfs_file_info_t *vfs_stat(const char *path)
{
    const tinyfs_file_t *file;
    char normalized[VFS_PATH_MAX];
    unsigned int index;

    if (!vfs_ready)
    {
        return 0;
    }

    if (vfs_normalize_path(path, normalized, 1) != 0)
    {
        return 0;
    }

    if (normalized[0] == '\0')
    {
        vfs_stat_info.name = "/";
        vfs_stat_info.size = 0;
        vfs_stat_info.flags = VFS_FILE_FLAG_DIRECTORY;
        return &vfs_stat_info;
    }

    file = tinyfs_find(normalized);
    if (file != 0)
    {
        vfs_stat_info.name = file->name;
        vfs_stat_info.size = file->size;
        vfs_stat_info.flags = file->flags;
        return &vfs_stat_info;
    }

    for (index = 0; index < tinyfs_file_count(); index++)
    {
        const tinyfs_file_t *entry;
        const char *child_name;
        unsigned int child_length;
        int is_directory;

        entry = tinyfs_get_file(index);
        if (entry == 0)
        {
            continue;
        }

        if (!vfs_collect_child_name(normalized,
                                    entry->name,
                                    &child_name,
                                    &child_length,
                                    &is_directory))
        {
            continue;
        }

        memcpy(vfs_stat_name, normalized, strlen(normalized));
        vfs_stat_name[strlen(normalized)] = '\0';
        vfs_stat_info.name = vfs_stat_name;
        vfs_stat_info.size = 0;
        vfs_stat_info.flags = VFS_FILE_FLAG_DIRECTORY;
        return &vfs_stat_info;
    }

    return 0;
}

int vfs_read_file(const char *path, void *buffer, unsigned long max_size)
{
    char normalized[VFS_PATH_MAX];
    const vfs_file_info_t *file;

    if (!vfs_ready)
    {
        return -1;
    }

    if (vfs_normalize_path(path, normalized, 0) != 0)
    {
        return -1;
    }

    file = vfs_stat(normalized);
    if (file == 0 || (file->flags & VFS_FILE_FLAG_DIRECTORY) != 0)
    {
        return -1;
    }

    return tinyfs_read_file(normalized, buffer, max_size);
}

int vfs_read_file_part(const char *path, unsigned int offset, void *buffer, unsigned int size)
{
    char normalized[VFS_PATH_MAX];
    const vfs_file_info_t *file;

    if (!vfs_ready)
    {
        return -1;
    }

    if (vfs_normalize_path(path, normalized, 0) != 0)
    {
        return -1;
    }

    file = vfs_stat(normalized);
    if (file == 0 || (file->flags & VFS_FILE_FLAG_DIRECTORY) != 0)
    {
        return -1;
    }

    return tinyfs_read_file_part(normalized, offset, buffer, size);
}

int vfs_is_mounted(void)
{
    return vfs_ready;
}

const char *vfs_root_fs_name(void)
{
    if (!vfs_ready)
    {
        return 0;
    }

    return "tinyfs";
}

block_device_t *vfs_root_device(void)
{
    if (!vfs_ready)
    {
        return 0;
    }

    return tinyfs_device();
}

unsigned int vfs_root_file_count(void)
{
    if (!vfs_ready)
    {
        return 0;
    }

    return tinyfs_file_count();
}
