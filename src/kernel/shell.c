#include "block/block_device.h"
#include "drivers/virtio.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "kernel/console.h"
#include "kernel/klog.h"
#include "kernel/kmalloc.h"
#include "kernel/memory_layout.h"
#include "kernel/panic.h"
#include "kernel/shell.h"
#include "kernel/timer.h"
#include "kernel/user.h"
#include "drivers/virtio_rng.h"
#include "lib/string.h"
#include "mm/pmm.h"

#define SHELL_LINE_MAX 64
#define SHELL_FILE_BUFFER_MAX 128
#define SHELL_BLOCK_DUMP_BYTES 512
#define KERNEL_VERSION "duck-os"

static const char *shell_read_token(const char *text, char *buffer, unsigned long buffer_size);
static int shell_parse_unsigned_long(const char *text, unsigned long *value_out);
static void shell_dump_hex_line(unsigned long offset, const unsigned char *buffer, unsigned int count);

static void shell_print_help(void)
{
    kprintf("help\n");
    kprintf("version\n");
    kprintf("ls\n");
    kprintf("cat <file>\n");
    kprintf("run <file>\n");
    kprintf("runelf <file>\n");
    kprintf("mem\n");
    kprintf("uptime\n");
    kprintf("ticks\n");
    kprintf("blk\n");
    kprintf("blkread <device> <block>\n");
    kprintf("fsinfo\n");
    kprintf("virtio\n");
    kprintf("rng\n");
    kprintf("panic\n");
}

static void shell_print_version(void)
{
    kprintf("%s\n", KERNEL_VERSION);
}

static void shell_print_mem(void)
{
    unsigned long ram_start;
    unsigned long ram_end;
    unsigned long kernel_start;
    unsigned long kernel_end;
    unsigned long heap_used;
    unsigned long heap_free;

    ram_start = memory_layout_ram_start();
    ram_end = memory_layout_ram_end();
    kernel_start = memory_layout_kernel_start();
    kernel_end = memory_layout_kernel_end();
    heap_used = kmalloc_used();
    heap_free = ram_end - kernel_end - heap_used;

    kprintf("RAM start: %p\n", (void *)ram_start);
    kprintf("RAM end: %p\n", (void *)ram_end);
    kprintf("kernel start: %p\n", (void *)kernel_start);
    kprintf("kernel end: %p\n", (void *)kernel_end);
    kprintf("heap used: %u bytes\n", (unsigned int)heap_used);
    kprintf("heap free: %u bytes\n", (unsigned int)heap_free);
    kprintf("pmm total pages: %u\n", (unsigned int)pmm_total_pages());
    kprintf("pmm used pages: %u\n", (unsigned int)pmm_used_pages());
    kprintf("pmm free pages: %u\n", (unsigned int)pmm_free_pages());
}

static void shell_print_uptime(void)
{
    unsigned long uptime_ms;
    unsigned long seconds;
    unsigned long milliseconds;

    uptime_ms = timer_uptime_ms();
    seconds = uptime_ms / 1000UL;
    milliseconds = uptime_ms % 1000UL;

    kprintf("uptime: %u.", (unsigned int)seconds);
    if (milliseconds < 100UL)
    {
        console_putc('0');
    }
    if (milliseconds < 10UL)
    {
        console_putc('0');
    }
    kprintf("%u s\n", (unsigned int)milliseconds);
}

static void shell_print_ticks(void)
{
    kprintf("ticks: %u\n", (unsigned int)timer_irq_count());
}

static void shell_print_block_devices(void)
{
    block_list_devices();
}

static void shell_print_fsinfo(void)
{
    block_device_t *device;

    if (!vfs_is_mounted())
    {
        kprintf("vfs not mounted\n");
        return;
    }

    device = vfs_root_device();

    kprintf("fs: %s\n", vfs_root_fs_name());
    kprintf("device: %s\n", device->name);
    kprintf("files: %u\n", vfs_root_file_count());
    kprintf("mount: /\n");
}

static void shell_print_rng(void)
{
    unsigned char bytes[16];
    unsigned int count;
    unsigned int i;

    if (!virtio_rng_available())
    {
        kprintf("virtio-rng unavailable\n");
        return;
    }

    if (virtio_rng_fill(bytes, sizeof(bytes), &count) != 0)
    {
        kprintf("virtio-rng request failed\n");
        return;
    }

    kprintf("rng:");
    for (i = 0; i < count; i++)
    {
        kprintf(" %x", (unsigned int)bytes[i]);
    }
    kprintf("\n");
}

static void shell_print_virtio(void)
{
    virtio_print_devices();
}

static void shell_block_read(const char *args)
{
    char device_name[16];
    char block_text[16];
    block_device_t *device;
    unsigned long block_number;
    unsigned char buffer[SHELL_BLOCK_DUMP_BYTES];
    unsigned int offset;

    args = shell_read_token(args, device_name, sizeof(device_name));
    args = shell_read_token(args, block_text, sizeof(block_text));
    if (device_name[0] == '\0' || block_text[0] == '\0')
    {
        kprintf("usage: blkread <device> <block>\n");
        return;
    }

    device = block_find_device(device_name);
    if (device == 0)
    {
        kprintf("device not found: %s\n", device_name);
        return;
    }

    if (shell_parse_unsigned_long(block_text, &block_number) != 0)
    {
        kprintf("invalid block: %s\n", block_text);
        return;
    }

    if (device->block_size > sizeof(buffer))
    {
        kprintf("block too large: %u\n", device->block_size);
        return;
    }

    if (device->read_blocks(device, block_number, 1, buffer) != 0)
    {
        kprintf("blkread failed: %s block %u\n", device->name, (unsigned int)block_number);
        return;
    }

    for (offset = 0; offset < device->block_size; offset += 16U)
    {
        unsigned int count;

        count = device->block_size - offset;
        if (count > 16U)
        {
            count = 16U;
        }
        shell_dump_hex_line(offset, buffer + offset, count);
    }
}

static int shell_starts_with(const char *text, const char *prefix)
{
    while (*prefix != '\0')
    {
        if (*text != *prefix)
        {
            return 0;
        }

        text++;
        prefix++;
    }

    return 1;
}

static const char *shell_skip_spaces(const char *text)
{
    while (*text == ' ')
    {
        text++;
    }

    return text;
}

static const char *shell_read_token(const char *text, char *buffer, unsigned long buffer_size)
{
    unsigned long length;

    text = shell_skip_spaces(text);
    if (*text == '\0')
    {
        if (buffer_size > 0U)
        {
            buffer[0] = '\0';
        }
        return text;
    }

    length = 0;
    while (*text != '\0' && *text != ' ')
    {
        if ((length + 1U) < buffer_size)
        {
            buffer[length] = *text;
        }
        length++;
        text++;
    }

    if (buffer_size > 0U)
    {
        if (length >= buffer_size)
        {
            length = buffer_size - 1U;
        }
        buffer[length] = '\0';
    }

    return text;
}

static int shell_parse_unsigned_long(const char *text, unsigned long *value_out)
{
    unsigned long value;

    if (text == 0 || *text == '\0' || value_out == 0)
    {
        return -1;
    }

    value = 0;
    while (*text != '\0')
    {
        if (*text < '0' || *text > '9')
        {
            return -1;
        }

        value = (value * 10UL) + (unsigned long)(*text - '0');
        text++;
    }

    *value_out = value;
    return 0;
}

static void shell_dump_hex_line(unsigned long offset, const unsigned char *buffer, unsigned int count)
{
    unsigned int index;

    kprintf("%x:", (unsigned int)offset);
    for (index = 0; index < count; index++)
    {
        kprintf(" %x", (unsigned int)buffer[index]);
    }
    kprintf("\n");
}

static void shell_list_files(const char *path)
{
    const char *list_path;

    list_path = path;
    if (list_path == 0 || *list_path == '\0')
    {
        list_path = "/";
    }

    if (vfs_list(list_path) != 0)
    {
        kprintf("list failed\n");
    }
}

static void shell_cat_file(const char *name)
{
    file_t *file;
    unsigned char buffer[SHELL_FILE_BUFFER_MAX];
    int read_size;
    int any_read;
    unsigned char last_char;
    unsigned long i;

    if (*name == '\0')
    {
        kprintf("usage: cat <file>\n");
        return;
    }

    file = file_open(name);
    if (file == 0)
    {
        kprintf("file not found: %s\n", name);
        return;
    }

    any_read = 0;
    last_char = '\0';
    while (1)
    {
        read_size = file_read(file, buffer, sizeof(buffer));
        if (read_size < 0)
        {
            kprintf("read failed: %s\n", name);
            file_close(file);
            return;
        }

        if (read_size == 0)
        {
            break;
        }

        for (i = 0; i < (unsigned long)read_size; i++)
        {
            console_putc((char)buffer[i]);
        }

        any_read = 1;
        last_char = buffer[read_size - 1];
    }

    if (!any_read)
    {
        console_putc('\n');
    }
    else if (last_char != '\n')
    {
        console_putc('\n');
    }

    file_close(file);
}

static void shell_run_user_file(const char *name)
{
    if (*name == '\0')
    {
        kprintf("usage: run <file>\n");
        return;
    }

    if (user_run_file(name) != 0)
    {
        kprintf("run failed: %s\n", name);
    }
}

static void shell_run_elf_file(const char *name)
{
    if (*name == '\0')
    {
        kprintf("usage: runelf <file>\n");
        return;
    }

    if (user_run_file(name) != 0)
    {
        kprintf("runelf failed: %s\n", name);
    }
}

static void shell_run_command(const char *line)
{
    if (strcmp(line, "") == 0)
    {
        return;
    }

    if (strcmp(line, "help") == 0)
    {
        shell_print_help();
        return;
    }

    if (strcmp(line, "version") == 0)
    {
        shell_print_version();
        return;
    }

    if (shell_starts_with(line, "ls"))
    {
        shell_list_files(shell_skip_spaces(line + 2));
        return;
    }

    if (strcmp(line, "mem") == 0)
    {
        shell_print_mem();
        return;
    }

    if (strcmp(line, "uptime") == 0)
    {
        shell_print_uptime();
        return;
    }

    if (strcmp(line, "ticks") == 0)
    {
        shell_print_ticks();
        return;
    }

    if (strcmp(line, "blk") == 0)
    {
        shell_print_block_devices();
        return;
    }

    if (shell_starts_with(line, "blkread"))
    {
        shell_block_read(shell_skip_spaces(line + 7));
        return;
    }

    if (strcmp(line, "fsinfo") == 0)
    {
        shell_print_fsinfo();
        return;
    }

    if (strcmp(line, "virtio") == 0)
    {
        shell_print_virtio();
        return;
    }

    if (strcmp(line, "rng") == 0)
    {
        shell_print_rng();
        return;
    }

    if (strcmp(line, "panic") == 0)
    {
        panic("panic command");
    }

    if (shell_starts_with(line, "cat"))
    {
        shell_cat_file(shell_skip_spaces(line + 3));
        return;
    }

    if (shell_starts_with(line, "runelf"))
    {
        shell_run_elf_file(shell_skip_spaces(line + 6));
        return;
    }

    if (shell_starts_with(line, "run"))
    {
        shell_run_user_file(shell_skip_spaces(line + 3));
        return;
    }

    kprintf("unknown command: %s\n", line);
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];

    klog_info("shell ready");

    while (1)
    {
        console_write("duck-os@dev > ");
        console_read_line(line, sizeof(line));
        shell_run_command(line);
    }
}
