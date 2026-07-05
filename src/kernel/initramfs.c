#include "kernel/initramfs.h"
#include "kernel/klog.h"
#include "kernel/panic.h"
#include "lib/string.h"

#define INITRAMFS_MAGIC "IRFS"
#define INITRAMFS_MAX_FILES 16UL

struct initramfs_header
{
    unsigned char magic[4];
    unsigned long file_count;
} __attribute__((packed));

struct initramfs_entry_header
{
    unsigned long name_length;
    unsigned long data_length;
} __attribute__((packed));

static struct initramfs_file initramfs_files[INITRAMFS_MAX_FILES];
static unsigned long initramfs_count;
static int initramfs_ready;

extern unsigned char _binary_build_initramfs_img_start[];
extern unsigned char _binary_build_initramfs_img_end[];

static unsigned long initramfs_align_up(unsigned long value)
{
    return (value + 7UL) & ~7UL;
}

static const unsigned char *initramfs_image_start(void)
{
    return _binary_build_initramfs_img_start;
}

static const unsigned char *initramfs_image_end(void)
{
    return _binary_build_initramfs_img_end;
}

unsigned long initramfs_file_count(void)
{
    return initramfs_count;
}

void initramfs_init(void)
{
    const unsigned char *cursor;
    const unsigned char *image_end;
    const struct initramfs_header *header;
    unsigned long i;

    if (initramfs_ready)
    {
        return;
    }

    cursor = initramfs_image_start();
    image_end = initramfs_image_end();

    if ((unsigned long)(image_end - cursor) < sizeof(*header))
    {
        panic("initramfs image too small");
    }

    header = (const struct initramfs_header *)cursor;
    if (memcmp(header->magic, INITRAMFS_MAGIC, 4) != 0)
    {
        panic("initramfs bad magic");
    }

    if (header->file_count > INITRAMFS_MAX_FILES)
    {
        panic("initramfs too many files");
    }

    initramfs_count = header->file_count;
    cursor += sizeof(*header);

    for (i = 0; i < initramfs_count; i++)
    {
        const struct initramfs_entry_header *entry;
        const unsigned char *name;
        const unsigned char *data;
        unsigned long name_length;
        unsigned long data_length;

        if ((unsigned long)(image_end - cursor) < sizeof(*entry))
        {
            panic("initramfs truncated entry");
        }

        entry = (const struct initramfs_entry_header *)cursor;
        name_length = entry->name_length;
        data_length = entry->data_length;
        cursor += sizeof(*entry);

        if ((unsigned long)(image_end - cursor) < name_length)
        {
            panic("initramfs truncated name");
        }

        name = cursor;
        cursor += initramfs_align_up(name_length);

        if ((unsigned long)(image_end - cursor) < data_length)
        {
            panic("initramfs truncated data");
        }

        data = cursor;
        cursor += initramfs_align_up(data_length);

        initramfs_files[i].name = (const char *)name;
        initramfs_files[i].data = data;
        initramfs_files[i].size = data_length;
    }

    initramfs_ready = 1;
    kprintf("[INFO] initramfs: files=%u\n", (unsigned int)initramfs_count);
}

void initramfs_list(void)
{
    unsigned long i;

    initramfs_init();

    for (i = 0; i < initramfs_count; i++)
    {
        kprintf("%s (%u bytes)\n",
                initramfs_files[i].name,
                (unsigned int)initramfs_files[i].size);
    }
}

const struct initramfs_file *initramfs_find(const char *name)
{
    unsigned long i;

    initramfs_init();

    for (i = 0; i < initramfs_count; i++)
    {
        if (strcmp(initramfs_files[i].name, name) == 0)
        {
            return &initramfs_files[i];
        }
    }

    return 0;
}

const unsigned char *initramfs_read(const struct initramfs_file *file)
{
    if (file == 0)
    {
        return 0;
    }

    return file->data;
}
