#include "fs/logfs.h"

#include "kernel/klog.h"
#include "lib/string.h"

#define LOGFS_RECORD_MAGIC 0x4345524cU
#define LOGFS_RECORD_TYPE_CREATE 1U
#define LOGFS_RECORD_TYPE_APPEND 2U
#define LOGFS_IO_BUFFER_SIZE 512U
#define LOGFS_ALIGN 8U

typedef struct logfs_superblock
{
    unsigned int magic;
    unsigned int version;
    unsigned int block_size;
    unsigned int block_count;
    unsigned int log_offset;
} logfs_superblock_t;

typedef struct logfs_record_header
{
    unsigned int magic;
    unsigned int type;
    unsigned int file_id;
    unsigned int payload_size;
    unsigned int file_size_before;
    unsigned int file_size_after;
    unsigned int checksum;
    unsigned int name_length;
} logfs_record_header_t;

typedef struct logfs_segment
{
    unsigned int file_id;
    unsigned int file_offset;
    unsigned int data_size;
    unsigned long data_offset;
} logfs_segment_t;

typedef struct logfs_file_state
{
    logfs_file_info_t info;
    unsigned int file_id;
    unsigned int segment_count;
    int in_use;
} logfs_file_state_t;

static block_device_t *logfs_block_device;
static logfs_superblock_t logfs_superblock;
static logfs_file_state_t logfs_files[LOGFS_MAX_FILES];
static char logfs_file_names[LOGFS_MAX_FILES][LOGFS_NAME_MAX];
static logfs_segment_t logfs_segments[LOGFS_MAX_SEGMENTS];
static unsigned int logfs_segment_count;
static unsigned int logfs_next_file_id;
static unsigned long logfs_log_end;
static int logfs_ready;
static unsigned char logfs_block_buffer[LOGFS_IO_BUFFER_SIZE];
static unsigned char logfs_record_buffer[LOGFS_IO_BUFFER_SIZE];

static unsigned int logfs_align_up(unsigned int value)
{
    return (value + (LOGFS_ALIGN - 1U)) & ~(LOGFS_ALIGN - 1U);
}

static unsigned int logfs_checksum_bytes(unsigned int seed, const unsigned char *data, unsigned int size)
{
    unsigned int index;

    for (index = 0; index < size; index++)
    {
        seed = (seed * 33U) + data[index];
    }

    return seed;
}

static int logfs_name_valid(const char *name)
{
    unsigned int length;

    if (name == 0 || name[0] == '\0')
    {
        return 0;
    }

    length = strlen(name);
    if (length == 0 || length >= LOGFS_NAME_MAX)
    {
        return 0;
    }

    while (*name != '\0')
    {
        if (*name == '/')
        {
            return 0;
        }

        name++;
    }

    return 1;
}

static int logfs_read_bytes(unsigned long offset, void *buffer, unsigned int size)
{
    unsigned char *dest;
    unsigned int block_size;
    unsigned int copied;

    if (logfs_block_device == 0 || buffer == 0)
    {
        return -1;
    }

    block_size = logfs_block_device->block_size;
    if (block_size > sizeof(logfs_block_buffer))
    {
        return -1;
    }

    dest = (unsigned char *)buffer;
    copied = 0;
    while (copied < size)
    {
        unsigned long block_index;
        unsigned int block_offset;
        unsigned int chunk_size;

        block_index = (offset + copied) / block_size;
        block_offset = (unsigned int)((offset + copied) % block_size);
        chunk_size = block_size - block_offset;
        if (chunk_size > (size - copied))
        {
            chunk_size = size - copied;
        }

        if (logfs_block_device->read_blocks(logfs_block_device, block_index, 1, logfs_block_buffer) != 0)
        {
            return -1;
        }

        memcpy(dest + copied, logfs_block_buffer + block_offset, chunk_size);
        copied += chunk_size;
    }

    return 0;
}

static int logfs_write_bytes(unsigned long offset, const void *buffer, unsigned int size)
{
    const unsigned char *src;
    unsigned int block_size;
    unsigned int copied;

    if (logfs_block_device == 0 || logfs_block_device->write_blocks == 0 || buffer == 0)
    {
        return -1;
    }

    block_size = logfs_block_device->block_size;
    if (block_size > sizeof(logfs_block_buffer))
    {
        return -1;
    }

    src = (const unsigned char *)buffer;
    copied = 0;
    while (copied < size)
    {
        unsigned long block_index;
        unsigned int block_offset;
        unsigned int chunk_size;

        block_index = (offset + copied) / block_size;
        block_offset = (unsigned int)((offset + copied) % block_size);
        chunk_size = block_size - block_offset;
        if (chunk_size > (size - copied))
        {
            chunk_size = size - copied;
        }

        if (block_offset == 0U && chunk_size == block_size)
        {
            if (logfs_block_device->write_blocks(logfs_block_device, block_index, 1, src + copied) != 0)
            {
                return -1;
            }
        }
        else
        {
            if (logfs_block_device->read_blocks(logfs_block_device, block_index, 1, logfs_block_buffer) != 0)
            {
                return -1;
            }

            memcpy(logfs_block_buffer + block_offset, src + copied, chunk_size);
            if (logfs_block_device->write_blocks(logfs_block_device, block_index, 1, logfs_block_buffer) != 0)
            {
                return -1;
            }
        }

        copied += chunk_size;
    }

    return 0;
}

static logfs_file_state_t *logfs_find_file_state(const char *name)
{
    unsigned int index;

    for (index = 0; index < LOGFS_MAX_FILES; index++)
    {
        if (logfs_files[index].in_use && strcmp(logfs_files[index].info.name, name) == 0)
        {
            return &logfs_files[index];
        }
    }

    return 0;
}

static logfs_file_state_t *logfs_find_file_id(unsigned int file_id)
{
    unsigned int index;

    for (index = 0; index < LOGFS_MAX_FILES; index++)
    {
        if (logfs_files[index].in_use && logfs_files[index].file_id == file_id)
        {
            return &logfs_files[index];
        }
    }

    return 0;
}

static logfs_file_state_t *logfs_alloc_file_state(void)
{
    unsigned int index;

    for (index = 0; index < LOGFS_MAX_FILES; index++)
    {
        if (!logfs_files[index].in_use)
        {
            memset(&logfs_files[index], 0, sizeof(logfs_files[index]));
            logfs_files[index].in_use = 1;
            return &logfs_files[index];
        }
    }

    return 0;
}

static int logfs_add_segment(unsigned int file_id,
                             unsigned int file_offset,
                             unsigned int data_size,
                             unsigned long data_offset)
{
    if (logfs_segment_count >= LOGFS_MAX_SEGMENTS)
    {
        return -1;
    }

    logfs_segments[logfs_segment_count].file_id = file_id;
    logfs_segments[logfs_segment_count].file_offset = file_offset;
    logfs_segments[logfs_segment_count].data_size = data_size;
    logfs_segments[logfs_segment_count].data_offset = data_offset;
    logfs_segment_count++;
    return 0;
}

static int logfs_replay_create(const logfs_record_header_t *header, const unsigned char *payload)
{
    logfs_file_state_t *file;
    unsigned int name_length;

    name_length = header->name_length;
    if (name_length == 0U || name_length >= LOGFS_NAME_MAX)
    {
        return -1;
    }

    if (header->payload_size != (name_length + 1U) || payload[name_length] != '\0')
    {
        return -1;
    }

    if (!logfs_name_valid((const char *)payload))
    {
        return -1;
    }

    if (logfs_find_file_state((const char *)payload) != 0 || logfs_find_file_id(header->file_id) != 0)
    {
        return -1;
    }

    file = logfs_alloc_file_state();
    if (file == 0)
    {
        return -1;
    }

    memcpy(logfs_file_names[file - logfs_files], payload, name_length + 1U);
    file->file_id = header->file_id;
    file->info.name = logfs_file_names[file - logfs_files];
    file->info.size = 0;

    if (header->file_id >= logfs_next_file_id)
    {
        logfs_next_file_id = header->file_id + 1U;
    }

    return 0;
}

static int logfs_replay_append(const logfs_record_header_t *header, unsigned long payload_offset)
{
    logfs_file_state_t *file;

    file = logfs_find_file_id(header->file_id);
    if (file == 0 || header->payload_size == 0U)
    {
        return -1;
    }

    if (file->info.size != header->file_size_before ||
        header->file_size_after != (header->file_size_before + header->payload_size))
    {
        return -1;
    }

    if (logfs_add_segment(header->file_id, file->info.size, header->payload_size, payload_offset) != 0)
    {
        return -1;
    }

    file->info.size = header->file_size_after;
    file->segment_count++;
    return 0;
}

static int logfs_replay_record(unsigned long offset)
{
    logfs_record_header_t header;
    unsigned int record_size;
    unsigned int checksum;

    if (logfs_read_bytes(offset, &header, sizeof(header)) != 0)
    {
        return -1;
    }

    if (header.magic == 0U)
    {
        return 1;
    }

    if (header.magic != LOGFS_RECORD_MAGIC)
    {
        return -1;
    }

    record_size = logfs_align_up((unsigned int)sizeof(header) + header.payload_size);
    if (record_size < sizeof(header) ||
        (offset + record_size) > ((unsigned long)logfs_superblock.block_count * logfs_superblock.block_size) ||
        header.payload_size > (sizeof(logfs_record_buffer) - sizeof(header)))
    {
        return -1;
    }

    if (logfs_read_bytes(offset + sizeof(header), logfs_record_buffer, header.payload_size) != 0)
    {
        return -1;
    }

    checksum = header.checksum;
    header.checksum = 0U;
    header.name_length = header.name_length;
    if (logfs_checksum_bytes(logfs_checksum_bytes(0U, (const unsigned char *)&header, sizeof(header)),
                             logfs_record_buffer,
                             header.payload_size) != checksum)
    {
        return -1;
    }

    if (header.type == LOGFS_RECORD_TYPE_CREATE)
    {
        if (logfs_replay_create(&header, logfs_record_buffer) != 0)
        {
            return -1;
        }
    }
    else if (header.type == LOGFS_RECORD_TYPE_APPEND)
    {
        if (logfs_replay_append(&header, offset + sizeof(header)) != 0)
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    logfs_log_end = offset + record_size;
    return 0;
}

static int logfs_append_record(unsigned int type,
                               unsigned int file_id,
                               unsigned int file_size_before,
                               unsigned int file_size_after,
                               unsigned int name_length,
                               const void *payload,
                               unsigned int payload_size,
                               unsigned long *record_offset_out)
{
    logfs_record_header_t header;
    unsigned int record_size;
    unsigned int padding_size;

    if (payload_size > (sizeof(logfs_record_buffer) - sizeof(header)))
    {
        return -1;
    }

    header.magic = LOGFS_RECORD_MAGIC;
    header.type = type;
    header.file_id = file_id;
    header.payload_size = payload_size;
    header.file_size_before = file_size_before;
    header.file_size_after = file_size_after;
    header.checksum = 0U;
    header.name_length = name_length;

    record_size = logfs_align_up((unsigned int)sizeof(header) + payload_size);
    padding_size = record_size - ((unsigned int)sizeof(header) + payload_size);

    memcpy(logfs_record_buffer, &header, sizeof(header));
    if (payload_size != 0U)
    {
        memcpy(logfs_record_buffer + sizeof(header), payload, payload_size);
    }
    if (padding_size != 0U)
    {
        memset(logfs_record_buffer + sizeof(header) + payload_size, 0, padding_size);
    }

    header.checksum = logfs_checksum_bytes(logfs_checksum_bytes(0U,
                                                                (const unsigned char *)&header,
                                                                sizeof(header)),
                                           (const unsigned char *)payload,
                                           payload_size);
    memcpy(logfs_record_buffer, &header, sizeof(header));

    if ((logfs_log_end + record_size) > ((unsigned long)logfs_superblock.block_count * logfs_superblock.block_size))
    {
        return -1;
    }

    if (record_offset_out != 0)
    {
        *record_offset_out = logfs_log_end;
    }

    if (logfs_write_bytes(logfs_log_end, logfs_record_buffer, record_size) != 0)
    {
        return -1;
    }

    logfs_log_end += record_size;
    return 0;
}

int logfs_mount(block_device_t *device)
{
    unsigned long offset;

    logfs_block_device = 0;
    logfs_ready = 0;
    memset(&logfs_superblock, 0, sizeof(logfs_superblock));
    memset(logfs_files, 0, sizeof(logfs_files));
    memset(logfs_file_names, 0, sizeof(logfs_file_names));
    memset(logfs_segments, 0, sizeof(logfs_segments));
    logfs_segment_count = 0;
    logfs_next_file_id = 1U;
    logfs_log_end = 0U;

    if (device == 0 || device->read_blocks == 0 || device->write_blocks == 0 || device->block_size > sizeof(logfs_block_buffer))
    {
        return -1;
    }

    logfs_block_device = device;
    if (logfs_read_bytes(0, &logfs_superblock, sizeof(logfs_superblock)) != 0)
    {
        logfs_block_device = 0;
        return -1;
    }

    if (logfs_superblock.magic != LOGFS_MAGIC ||
        logfs_superblock.version != LOGFS_VERSION ||
        logfs_superblock.block_size != device->block_size ||
        logfs_superblock.block_count > device->block_count ||
        logfs_superblock.log_offset < device->block_size)
    {
        logfs_block_device = 0;
        return -1;
    }

    offset = logfs_superblock.log_offset;
    logfs_log_end = offset;
    while ((offset + sizeof(logfs_record_header_t)) <= ((unsigned long)logfs_superblock.block_count * logfs_superblock.block_size))
    {
        int replay_status;

        replay_status = logfs_replay_record(offset);
        if (replay_status > 0)
        {
            break;
        }

        if (replay_status < 0)
        {
            kprintf("[INFO] logfs: stopped replay at offset %u\n", (unsigned int)offset);
            break;
        }

        offset = logfs_log_end;
    }

    logfs_ready = 1;
    kprintf("[INFO] logfs: mounted %s files=%u head=%u\n",
            device->name,
            logfs_file_count(),
            (unsigned int)logfs_log_end);
    return 0;
}

int logfs_is_mounted(void)
{
    return logfs_ready;
}

block_device_t *logfs_device(void)
{
    if (!logfs_ready)
    {
        return 0;
    }

    return logfs_block_device;
}

unsigned int logfs_file_count(void)
{
    unsigned int index;
    unsigned int count;

    count = 0;
    for (index = 0; index < LOGFS_MAX_FILES; index++)
    {
        if (logfs_files[index].in_use)
        {
            count++;
        }
    }

    return count;
}

const logfs_file_info_t *logfs_get_file(unsigned int index)
{
    unsigned int current;
    unsigned int scan;

    if (!logfs_ready)
    {
        return 0;
    }

    current = 0;
    for (scan = 0; scan < LOGFS_MAX_FILES; scan++)
    {
        if (!logfs_files[scan].in_use)
        {
            continue;
        }

        if (current == index)
        {
            return &logfs_files[scan].info;
        }

        current++;
    }

    return 0;
}

const logfs_file_info_t *logfs_stat(const char *name)
{
    logfs_file_state_t *file;

    if (!logfs_ready || name == 0)
    {
        return 0;
    }

    file = logfs_find_file_state(name);
    if (file == 0)
    {
        return 0;
    }

    return &file->info;
}

int logfs_create(const char *name)
{
    logfs_file_state_t *file;
    unsigned int name_length;

    if (!logfs_ready || !logfs_name_valid(name) || logfs_find_file_state(name) != 0)
    {
        return -1;
    }

    file = logfs_alloc_file_state();
    if (file == 0)
    {
        return -1;
    }

    name_length = strlen(name);
    file->file_id = logfs_next_file_id++;
    memcpy(logfs_file_names[file - logfs_files], name, name_length + 1U);
    file->info.name = logfs_file_names[file - logfs_files];
    file->info.size = 0;

    if (logfs_append_record(LOGFS_RECORD_TYPE_CREATE,
                            file->file_id,
                            0U,
                            0U,
                            name_length,
                            name,
                            name_length + 1U,
                            0) != 0)
    {
        memset(file, 0, sizeof(*file));
        memset(logfs_file_names[file - logfs_files], 0, LOGFS_NAME_MAX);
        return -1;
    }

    return 0;
}

int logfs_append(const char *name, const void *data, unsigned int size)
{
    logfs_file_state_t *file;
    unsigned long record_offset;

    if (!logfs_ready || data == 0 || size == 0U)
    {
        return -1;
    }

    file = logfs_find_file_state(name);
    if (file == 0)
    {
        return -1;
    }

    if (logfs_append_record(LOGFS_RECORD_TYPE_APPEND,
                            file->file_id,
                            file->info.size,
                            file->info.size + size,
                            0U,
                            data,
                            size,
                            &record_offset) != 0)
    {
        return -1;
    }

    if (logfs_add_segment(file->file_id,
                          file->info.size,
                          size,
                          record_offset + sizeof(logfs_record_header_t)) != 0)
    {
        return -1;
    }

    file->segment_count++;
    file->info.size += size;
    return 0;
}

int logfs_read_file(const char *name, void *buffer, unsigned int max_size)
{
    const logfs_file_info_t *file;

    file = logfs_stat(name);
    if (file == 0 || buffer == 0 || file->size > max_size)
    {
        return -1;
    }

    return logfs_read_file_part(name, 0U, buffer, file->size);
}

int logfs_read_file_part(const char *name, unsigned int offset, void *buffer, unsigned int size)
{
    logfs_file_state_t *file;
    unsigned char *dest;
    unsigned int segment_index;
    unsigned int copied;
    unsigned int read_end;

    if (!logfs_ready || buffer == 0)
    {
        return -1;
    }

    file = logfs_find_file_state(name);
    if (file == 0 || offset > file->info.size)
    {
        return -1;
    }

    if (size > (file->info.size - offset))
    {
        size = file->info.size - offset;
    }

    if (size == 0U)
    {
        return 0;
    }

    dest = (unsigned char *)buffer;
    copied = 0;
    read_end = offset + size;
    for (segment_index = 0; segment_index < logfs_segment_count && copied < size; segment_index++)
    {
        logfs_segment_t *segment;
        unsigned int segment_start;
        unsigned int segment_end;
        unsigned int overlap_start;
        unsigned int overlap_end;
        unsigned int read_start;
        unsigned int read_size;

        segment = &logfs_segments[segment_index];
        if (segment->file_id != file->file_id)
        {
            continue;
        }

        segment_start = segment->file_offset;
        segment_end = segment->file_offset + segment->data_size;
        if (offset >= segment_end || read_end <= segment_start)
        {
            continue;
        }

        overlap_start = offset > segment_start ? offset : segment_start;
        overlap_end = read_end < segment_end ? read_end : segment_end;
        read_start = overlap_start - segment_start;
        read_size = overlap_end - overlap_start;

        if (logfs_read_bytes(segment->data_offset + read_start, dest + copied, read_size) != 0)
        {
            return -1;
        }

        copied += read_size;
    }

    return (int)copied;
}
